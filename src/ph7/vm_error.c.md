# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3288/3687 lines (89.18%)

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
|    33513 |   28 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile,sxu32 nLine)` |
|        5 |   29 | `{` |
|    33518 |   30 | `	pVm->nLastErrType = iErr;` |
|    33518 |   31 | `	pVm->nLastErrLine = nLine;` |
|    33518 |   32 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|    33518 |   33 | `	if( zMsg && nMsg > 0 ){` |
|    33518 |   34 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|    16755 |   35 | `	}` |
|    33518 |   36 | `	SyBlobReset(&pVm->sLastErrFile);` |
|    33518 |   37 | `	if( pFile ){` |
|    33518 |   38 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|    16755 |   39 | `	}` |
|    33518 |   40 | `}` |
|        - |   41 | `/*` |
|        - |   42 | ` * The stream a diagnostic's LOG copy goes to: the registered stderr consumer,` |
|        - |   43 | ` * or -- for embedders that never wired one -- the program-output consumer, so a` |
|        - |   44 | ` * diagnostic is never silently swallowed.` |
|        - |   45 | ` */` |
|      942 |   46 | `static ph7_output_consumer * VmErrConsumer(ph7_vm *pVm)` |
|        4 |   47 | `{` |
|      946 |   48 | `	return pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
|        4 |   49 | `}` |
|        - |   50 | `/*` |
|        - |   51 | `` * php's `display_errors` value table, which is not a boolean one: the directive`` |
|        - |   52 | `` * names a DESTINATION. The words `on`, `yes`, `true` and `stdout` (matched`` |
|        - |   53 | ` * case-insensitively against the WHOLE value, so a stray space defeats them) mean` |
|        - |   54 | `` * the program output stream and `stderr` means the error stream; anything else is`` |
|        - |   55 | ` * read as a base-ten number the way strtol() reads one -- leading blanks skipped,` |
|        - |   56 | ` * an optional sign, trailing garbage ignored -- and then TRUNCATED TO EIGHT BITS.` |
|        - |   57 | ``  * That truncation is observable and is why `256` is off, `257` is stdout, `258` `` |
|        - |   58 | `` * is stderr and `-2` is stdout. A truncated value that is neither 0, 1 nor 2 is`` |
|        - |   59 | ` * stdout.` |
|        - |   60 | ` *` |
|        - |   61 | ` * The engine read the directive with the plain truth test every other boolean` |
|        - |   62 | `` * directive gets. `stderr` and `stdout` are WORDS, so both read false and the`` |
|        - |   63 | `` * display copy was switched OFF by the two spellings that ask for it; `2` read`` |
|        - |   64 | ` * true and put on stdout what php puts on stderr; and the truth test trimmed the` |
|        - |   65 | `` * value, so `ini_set('display_errors','on ')` was on where php reads it as a`` |
|        - |   66 | ` * number and gets off.` |
|        - |   67 | ` */` |
|      176 |   68 | `PH7_PRIVATE int PH7_VmDisplayErrorsMode(const char *zVal,sxu32 nVal)` |
|        4 |   69 | `{` |
|      180 |   70 | `	sxu64 uAcc = 0;` |
|      180 |   71 | `	sxu32 i = 0;` |
|      180 |   72 | `	int bNeg = 0, bSat = 0;` |
|        - |   73 | `	unsigned int uMode;` |
|      180 |   74 | `	if( zVal == 0 \|\| nVal < 1 ){` |
|       16 |   75 | `		return PH7_DISPLAY_ERRORS_OFF;` |
|        - |   76 | `	}` |
|      164 |   77 | `	if( nVal == 2 && SyStrnicmp(zVal,"on",2) == 0 ){ return PH7_DISPLAY_ERRORS_STDOUT; }` |
|      164 |   78 | `	if( nVal == 3 && SyStrnicmp(zVal,"yes",3) == 0 ){ return PH7_DISPLAY_ERRORS_STDOUT; }` |
|      164 |   79 | `	if( nVal == 4 && SyStrnicmp(zVal,"true",4) == 0 ){ return PH7_DISPLAY_ERRORS_STDOUT; }` |
|      164 |   80 | `	if( nVal == 6 && SyStrnicmp(zVal,"stderr",6) == 0 ){ return PH7_DISPLAY_ERRORS_STDERR; }` |
|      146 |   81 | `	if( nVal == 6 && SyStrnicmp(zVal,"stdout",6) == 0 ){ return PH7_DISPLAY_ERRORS_STDOUT; }` |
|        - |   82 | `	/* strtol()'s own lead-in: the blanks it skips, then one optional sign */` |
|      209 |   83 | `	while( i < nVal && (zVal[i] == ' ' \|\| zVal[i] == '\t' \|\| zVal[i] == '\n'` |
|      134 |   84 | `	    \|\| zVal[i] == '\v' \|\| zVal[i] == '\f' \|\| zVal[i] == '\r') ){` |
|        4 |   85 | `		i++;` |
|      ! 0 |   86 | `	}` |
|      138 |   87 | `	if( i < nVal && (zVal[i] == '+' \|\| zVal[i] == '-') ){` |
|       12 |   88 | `		bNeg = zVal[i] == '-';` |
|       12 |   89 | `		i++;` |
|        6 |   90 | `	}` |
|      308 |   91 | `	while( i < nVal && zVal[i] >= '0' && zVal[i] <= '9' ){` |
|        - |   92 | `		/* strtol() saturates instead of wrapping, and the remaining digits cannot` |
|        - |   93 | `		 * move a saturated value: LONG_MAX's low byte is 0xFF and LONG_MIN's 0x00.` |
|        - |   94 | `		 * The two limits are not symmetric, which is exactly the difference` |
|        - |   95 | ``		 * between `-9223372036854775808` (off) and `-9223372036854775809` (off by`` |
|        - |   96 | `		 * saturation, not by truncation) -- and between the latter and` |
|        - |   97 | ``		 * `9223372036854775808`, which saturates the other way and is stdout. */`` |
|      174 |   98 | `		sxu64 uLimit = bNeg ? (sxu64)9223372036854775808ULL : (sxu64)9223372036854775807ULL;` |
|      174 |   99 | `		if( uAcc > uLimit / 10 ){` |
|      ! 0 |  100 | `			bSat = 1;` |
|      ! 0 |  101 | `			break;` |
|        - |  102 | `		}` |
|      174 |  103 | `		uAcc = uAcc * 10 + (sxu64)(zVal[i] - '0');` |
|      174 |  104 | `		if( uAcc > uLimit ){` |
|      ! 0 |  105 | `			bSat = 1;` |
|      ! 0 |  106 | `			break;` |
|        - |  107 | `		}` |
|      174 |  108 | `		i++;` |
|        4 |  109 | `	}` |
|      138 |  110 | `	if( bSat ){` |
|      ! 0 |  111 | `		uMode = bNeg ? 0u : 255u;` |
|      ! 0 |  112 | `	}else{` |
|      138 |  113 | `		uMode = (unsigned int)((bNeg ? (sxu64)(0 - uAcc) : uAcc) & 0xFF);` |
|        - |  114 | `	}` |
|      134 |  115 | `	if( uMode != PH7_DISPLAY_ERRORS_OFF` |
|      119 |  116 | `	 && uMode != PH7_DISPLAY_ERRORS_STDOUT` |
|       76 |  117 | `	 && uMode != PH7_DISPLAY_ERRORS_STDERR ){` |
|       16 |  118 | `		return PH7_DISPLAY_ERRORS_STDOUT;` |
|        - |  119 | `	}` |
|      122 |  120 | `	return (int)uMode;` |
|       92 |  121 | `}` |
|        - |  122 | `/*` |
|        - |  123 | ` * Append the platform newline and hand a finished diagnostic blob to a consumer.` |
|        - |  124 | ` * bTrack counts the bytes toward program output length (only the stdout DISPLAY` |
|        - |  125 | ` * copy is program output; the stderr LOG copy is not, and must not perturb` |
|        - |  126 | ` * headers_sent()/output accounting).` |
|        - |  127 | ` */` |
|     1026 |  128 | `static sxi32 VmWriteDiagnostic(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bTrack)` |
|        4 |  129 | `{` |
|        - |  130 | `	sxi32 rc;` |
|        - |  131 | `	/* Append a new line */` |
|        - |  132 | `#ifdef __WINNT__` |
|        4 |  133 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  134 | `#else` |
|     1026 |  135 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  136 | `#endif` |
|        - |  137 | `	/* Invoke the output consumer callback */` |
|     1030 |  138 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|     1030 |  139 | `	if( bTrack ){` |
|       89 |  140 | `		VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|       43 |  141 | `	}` |
|     1030 |  142 | `	return rc;` |
|        4 |  143 | `}` |
|        - |  144 | `/*` |
|        - |  145 | `` * php's `error_log` ini destination, which used to be a directive this engine`` |
|        - |  146 | ` * stored and NOTHING read. Every LOG copy went to the error stream whatever it` |
|        - |  147 | ` * was set to, so a program that pointed its log at a file -- which is what a` |
|        - |  148 | ` * framework's bootstrap does before it does anything else -- had its` |
|        - |  149 | ` * diagnostics written to the terminal instead and an empty log file to show` |
|        - |  150 | ` * for it. The diagnostic was not lost, but it was not where the program said` |
|        - |  151 | ` * to put it, and nothing on either end said so.` |
|        - |  152 | ` *` |
|        - |  153 | ` * php's routing, which is what this is: with a destination set, the LOG copy is` |
|        - |  154 | `` * APPENDED to that file behind php's own `[d-M-Y H:i:s e] ` timestamp; with it`` |
|        - |  155 | ` * unset, and with a destination that will not open, it goes to the SAPI logger` |
|        - |  156 | ` * -- the error stream -- with no timestamp at all. Answers TRUE when the file` |
|        - |  157 | ` * took the write, so the caller knows the stream copy is not owed.` |
|        - |  158 | ` */` |
|     2234 |  159 | `PH7_PRIVATE int PH7_VmErrorLogToFile(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        5 |  160 | `{` |
|        - |  161 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  162 | `	/* The tiny build has neither the stream layer that opens the destination nor` |
|        - |  163 | `	 * the date breakdown that timestamps it, so every LOG copy stays on the` |
|        - |  164 | `	 * stream -- which is where a build with no file functions wants it anyway. */` |
|        - |  165 | `	SXUNUSED(pVm); SXUNUSED(zMsg); SXUNUSED(nMsg);` |
|        - |  166 | `	return 0;` |
|        - |  167 | `#else` |
|        - |  168 | `	const ph7_io_stream *pStream;` |
|        - |  169 | `	const char *zPath;` |
|        - |  170 | `	SyBlob sLine;` |
|        - |  171 | `	void *pHandle;` |
|     2239 |  172 | `	int nPath, rc = 0;` |
|     2239 |  173 | `	nPath = (int)SyBlobLength(&pVm->sErrLogPath);` |
|     2239 |  174 | `	if( nPath < 1 ){` |
|     2231 |  175 | `		return 0;` |
|        - |  176 | `	}` |
|        - |  177 | `	/* The stream layer opens by C string, and the two doors that write this blob` |
|        - |  178 | `	 * reset it in place -- so an unterminated path inherits the TAIL of whatever` |
|        - |  179 | ``	 * longer one preceded it, and `-d error_log=/no/such/dir/x.log` followed by`` |
|        - |  180 | `	 * ini_set('error_log','/tmp/a.log') opened "/tmp/a.logh/x.log" and quietly` |
|        - |  181 | `	 * fell back to the stream. Both doors terminate; this reads the bytes before` |
|        - |  182 | `	 * the terminator. */` |
|        9 |  183 | `	zPath = (const char *)SyBlobData(&pVm->sErrLogPath);` |
|        9 |  184 | `	pStream = PH7_VmGetStreamDevice(pVm,&zPath,nPath);` |
|        9 |  185 | `	if( pStream == 0 \|\| pStream->xWrite == 0 ){` |
|      ! 0 |  186 | `		return 0;` |
|        - |  187 | `	}` |
|        - |  188 | `	/* No warning on the way in: php's logger is SILENT about a destination it` |
|        - |  189 | `	 * cannot open -- raising one here would be a diagnostic about a diagnostic,` |
|        - |  190 | `	 * and the recursion is not the only reason php does not. */` |
|        9 |  191 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,zPath,` |
|        - |  192 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND,FALSE,0,FALSE,0,0);` |
|        9 |  193 | `	if( pHandle == 0 ){` |
|        3 |  194 | `		return 0;` |
|        - |  195 | `	}` |
|        7 |  196 | `	SyBlobInit(&sLine,&pVm->sAllocator);` |
|        7 |  197 | `	PH7_VmLogTimestamp(pVm,&sLine);` |
|        7 |  198 | `	SyBlobAppend(&sLine,zMsg,nMsg);` |
|        - |  199 | `#ifdef __WINNT__` |
|        1 |  200 | `	SyBlobAppend(&sLine,"\r\n",sizeof("\r\n")-1);` |
|        - |  201 | `#else` |
|        6 |  202 | `	SyBlobAppend(&sLine,"\n",sizeof(char));` |
|        - |  203 | `#endif` |
|        - |  204 | `	/* One write: php builds the whole line and hands it over in one go, which is` |
|        - |  205 | `	 * what keeps two processes appending to one log from interleaving mid-line. */` |
|        6 |  206 | `	if( SyBlobLength(&sLine) > 0` |
|        7 |  207 | `	 && pStream->xWrite(pHandle,SyBlobData(&sLine),(ph7_int64)SyBlobLength(&sLine)) > 0 ){` |
|        7 |  208 | `		rc = 1;` |
|        3 |  209 | `	}` |
|        7 |  210 | `	SyBlobRelease(&sLine);` |
|        7 |  211 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        7 |  212 | `	return rc;` |
|        - |  213 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     1122 |  214 | `}` |
|        - |  215 | `/*` |
|        - |  216 | `` * Hand a finished LOG copy to whichever sink `error_log` names. The file wins`` |
|        - |  217 | ` * when it is set and takes the write; the error stream is what is left.` |
|        - |  218 | ` */` |
|      896 |  219 | `static sxi32 VmWriteErrorLog(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg)` |
|        4 |  220 | `{` |
|      900 |  221 | `	if( PH7_VmErrorLogToFile(pVm,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg)) ){` |
|        3 |  222 | `		return SXRET_OK;` |
|        - |  223 | `	}` |
|      898 |  224 | `	return VmWriteDiagnostic(pVm,pCons,pMsg,0);` |
|      452 |  225 | `}` |
|        - |  226 | `/*` |
|        - |  227 | ` * Emit a fatal report as php's two copies, each behind its own ini gate -- the` |
|        - |  228 | ` * same split VmEmitDiagnostic makes for a warning or a notice, which fatals did` |
|        - |  229 | ` * not have. The caller hands over the LABEL and a header-less BODY (the sentence,` |
|        - |  230 | `` * the location and the `Stack trace:` block):`` |
|        - |  231 | ` *   - LOG copy     -> the error (stderr) stream when log_errors is on:` |
|        - |  232 | ``  *                     `PHP LABEL:  BODY` `` |
|        - |  233 | ` *   - DISPLAY copy -> the stream display_errors NAMES: program output (stdout) with` |
|        - |  234 | ``  *                     a leading blank line, `\nLABEL: BODY`, or -- for the `stderr` `` |
|        - |  235 | `` *                     spelling -- the error stream with no blank line, `LABEL: BODY`.`` |
|        - |  236 | ` * php prints BOTH when both are on, and the two are not the same bytes: the` |
|        - |  237 | `` * display copy has no `PHP ` prefix, ONE space after the colon and a leading blank`` |
|        - |  238 | ` * line. Every fatal here used to be built as the log shape and then written to` |
|        - |  239 | `` * whichever channel was on, so `display_errors=1` printed the stderr wording to`` |
|        - |  240 | `` * stdout and `display_errors=1 log_errors=1` printed one copy where php prints two.`` |
|        - |  241 | ` * Stock CLI php (display_errors off, log_errors on) is the one setting where the` |
|        - |  242 | ` * old routing happened to be right, which is why it stood.` |
|        - |  243 | ` */` |
|      620 |  244 | `static sxi32 VmEmitFatalReport(ph7_vm *pVm,const char *zLabel,const char *zBody,sxu32 nBody)` |
|        4 |  245 | `{` |
|        - |  246 | `	SyBlob sCopy;` |
|      624 |  247 | `	sxi32 rc = SXRET_OK;` |
|      624 |  248 | `	if( pVm->bLogErrors ){` |
|      622 |  249 | `		SyBlobInit(&sCopy,&pVm->sAllocator);` |
|      622 |  250 | `		SyBlobFormat(&sCopy,"PHP %s:  ",zLabel);` |
|      622 |  251 | `		SyBlobAppend(&sCopy,zBody,nBody);` |
|      622 |  252 | `		rc = VmWriteErrorLog(pVm,VmErrConsumer(pVm),&sCopy);` |
|      622 |  253 | `		SyBlobRelease(&sCopy);` |
|      309 |  254 | `	}` |
|      624 |  255 | `	if( pVm->iDisplayErrors != PH7_DISPLAY_ERRORS_OFF ){` |
|        5 |  256 | `		int bErrStream = pVm->iDisplayErrors == PH7_DISPLAY_ERRORS_STDERR;` |
|        - |  257 | `		sxi32 rc2;` |
|        5 |  258 | `		SyBlobInit(&sCopy,&pVm->sAllocator);` |
|        5 |  259 | `		if( !bErrStream ){` |
|        - |  260 | `			/* php's text-mode display copy is prefixed with a blank line */` |
|        3 |  261 | `			SyBlobAppend(&sCopy,"\n",sizeof(char));` |
|        1 |  262 | `		}` |
|        5 |  263 | `		SyBlobFormat(&sCopy,"%s: ",zLabel);` |
|        5 |  264 | `		SyBlobAppend(&sCopy,zBody,nBody);` |
|        8 |  265 | `		rc2 = VmWriteDiagnostic(pVm,` |
|        3 |  266 | `			bErrStream ? VmErrConsumer(pVm) : &pVm->sVmConsumer,&sCopy,!bErrStream);` |
|        5 |  267 | `		SyBlobRelease(&sCopy);` |
|        - |  268 | `		/* keep the first failure rather than letting a later successful write mask it */` |
|        5 |  269 | `		if( rc == SXRET_OK ){` |
|        5 |  270 | `			rc = rc2;` |
|        2 |  271 | `		}` |
|        2 |  272 | `	}` |
|      624 |  273 | `	return rc;` |
|        4 |  274 | `}` |
|        - |  275 | `/*` |
|        - |  276 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  277 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  278 | ` * information.` |
|        - |  279 | ` */` |
|    38157 |  280 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        5 |  281 | `{` |
|        - |  282 | `	/* A handler is only called for the levels it was REGISTERED for. php ANDs` |
|        - |  283 | `	 * set_error_handler()'s $error_levels against the error's own bit and, when` |
|        - |  284 | `	 * it misses, does NOT walk down to an outer handler -- the diagnostic falls` |
|        - |  285 | `	 * straight through to the engine's own reporting, which is what returning` |
|        - |  286 | `	 * TRUE below means. */` |
|    38157 |  287 | `	if( ph7_value_is_callable(&pVm->sErrCB)` |
|    22061 |  288 | `	 && (pVm->iErrCBLevels & (sxi64)PH7_VmErrPhpBit(iErr)) != 0 ){` |
|        - |  289 | `		ph7_value apArg[4];` |
|        - |  290 | `		ph7_value *apArgPtr[4];` |
|        - |  291 | `		ph7_value sResult;` |
|        - |  292 | `		ph7_value sRunning;` |
|        - |  293 | `		SyString sErr;` |
|        - |  294 | `		int bReport;` |
|        - |  295 | `		/* PH7_CTX_NOTICE is the engine's own severity token, 3 — a number php has no` |
|        - |  296 | `		 * E_* constant for. The reporting mask and the "Notice: " label already read` |
|        - |  297 | `		 * it as E_NOTICE; the handler was the one place it leaked, so a userland` |
|        - |  298 | ``		 * `set_error_handler` saw `$errno === 3` where php passes 8 and an`` |
|        - |  299 | ``		 * `if ($errno & E_NOTICE)` test simply never fired. */`` |
|     5947 |  300 | `		if( iErr == PH7_CTX_NOTICE ){` |
|      453 |  301 | `			iErr = 8; /* E_NOTICE */` |
|      224 |  302 | `		}` |
|        - |  303 | `		/* Prepare arguments */` |
|     5947 |  304 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  305 | `			/* use explicit message length to avoid reading past buffer */` |
|     5947 |  306 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|     5947 |  307 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|     5947 |  308 | `		if( pFile ){` |
|     5947 |  309 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|     5947 |  310 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     2960 |  311 | `		}else{` |
|      ! 0 |  312 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  313 | `		}` |
|     5947 |  314 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|     5947 |  315 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  316 | `		/* Set up pointer array */` |
|     5947 |  317 | `		apArgPtr[0] = &apArg[0];` |
|     5947 |  318 | `		apArgPtr[1] = &apArg[1];` |
|     5947 |  319 | `		apArgPtr[2] = &apArg[2];` |
|     5947 |  320 | `		apArgPtr[3] = &apArg[3];` |
|        - |  321 | `		/* php HIDES the handler for the duration of its own call: a diagnostic the` |
|        - |  322 | `		 * handler itself raises reaches the engine's reporting instead of` |
|        - |  323 | `		 * re-entering (PHL recursed until the stack ran out and printed nothing at` |
|        - |  324 | ``		 * all), and `set_error_handler()` called from inside one therefore replaces`` |
|        - |  325 | `		 * an EMPTY entry. What the handler leaves behind decides who is installed` |
|        - |  326 | `		 * when it returns: an untouched slot gets the original back, and anything` |
|        - |  327 | `		 * the handler installed itself STAYS. */` |
|     5947 |  328 | `		PH7_MemObjInit(pVm,&sRunning);` |
|     5947 |  329 | `		PH7_MemObjStore(&pVm->sErrCB,&sRunning);` |
|     5947 |  330 | `		PH7_MemObjRelease(&pVm->sErrCB);` |
|     5947 |  331 | `		MemObjSetType(&pVm->sErrCB,MEMOBJ_NULL);` |
|        - |  332 | `		/* Call the handler */` |
|        - |  333 | `		{` |
|     5947 |  334 | `			sxi32 rcCb = PH7_VmCallUserFunction(pVm,&sRunning,4,apArgPtr,&sResult);` |
|     5947 |  335 | `			if( !ph7_value_is_callable(&pVm->sErrCB) ){` |
|     5945 |  336 | `				PH7_MemObjStore(&sRunning,&pVm->sErrCB);` |
|     2954 |  337 | `			}` |
|     5947 |  338 | `			PH7_MemObjRelease(&sRunning);` |
|     5947 |  339 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
|        - |  340 | `				/* The handler threw (or aborted) instead of returning: php never` |
|        - |  341 | `				 * reports the original diagnostic then — the exception supersedes` |
|        - |  342 | `				 * it (and is routed by the boundary parking / fetch-point router).` |
|        - |  343 | `				 * Reporting it here would print a spurious Warning AFTER the` |
|        - |  344 | `				 * user's catch already ran. */` |
|        5 |  345 | `				PH7_MemObjRelease(&apArg[0]);` |
|        5 |  346 | `				PH7_MemObjRelease(&apArg[1]);` |
|        5 |  347 | `				PH7_MemObjRelease(&apArg[2]);` |
|        5 |  348 | `				PH7_MemObjRelease(&apArg[3]);` |
|        5 |  349 | `				PH7_MemObjRelease(&sResult);` |
|        5 |  350 | `				return FALSE;` |
|        - |  351 | `			}` |
|        - |  352 | `		}` |
|        - |  353 | `		/* Does the engine still report this diagnostic itself?` |
|        - |  354 | `		 *` |
|        - |  355 | ``		 * php's rule is IDENTITY, not truthiness: `zend_user_error_handler` falls`` |
|        - |  356 | `		 * through to the built-in reporter only when the handler returned the` |
|        - |  357 | `		 * BOOLEAN false. EVERY other answer means handled -- including the two` |
|        - |  358 | ``		 * commonest shapes there are, a bare `return;` and a handler with no return`` |
|        - |  359 | `		 * statement at all, both of which arrive here as NULL.` |
|        - |  360 | `		 *` |
|        - |  361 | `		 * This coerced the answer to bool, so seven of php's fourteen answers` |
|        - |  362 | `		 * reported through: null, a bare return, 0, 0.0, "", "0" and []. Every` |
|        - |  363 | ``		 * `set_error_handler(function () {})` in the world -- the idiom a library`` |
|        - |  364 | `		 * uses to SILENCE a call it expects to fail -- printed the diagnostic` |
|        - |  365 | `		 * anyway. It is the single largest divergence in the ecosystem gate: 930 of` |
|        - |  366 | ``		 * twig's 1949 accepted baseline lines are one `unserialize()` inside exactly`` |
|        - |  367 | `		 * that idiom, in symfony/phpunit-bridge.` |
|        - |  368 | `		 *` |
|        - |  369 | `		 * Read before the release, and before any coercion. */` |
|     5943 |  370 | `		bReport = (sResult.iFlags & MEMOBJ_BOOL) != 0 && sResult.x.iVal == 0;` |
|        - |  371 | `		/* Release */` |
|     5943 |  372 | `		PH7_MemObjRelease(&apArg[0]);` |
|     5943 |  373 | `		PH7_MemObjRelease(&apArg[1]);` |
|     5943 |  374 | `		PH7_MemObjRelease(&apArg[2]);` |
|     5943 |  375 | `		PH7_MemObjRelease(&apArg[3]);` |
|     5943 |  376 | `		PH7_MemObjRelease(&sResult);` |
|     5943 |  377 | `		return bReport ? TRUE : FALSE;` |
|        - |  378 | `	}` |
|        - |  379 | `	/* No handler, always call error handler */` |
|    32220 |  380 | `	return TRUE;` |
|    19066 |  381 | `}` |
|        - |  382 | `/*` |
|        - |  383 | ` * php's diagnostic label for a severity/errno (display shape; the caller` |
|        - |  384 | ` * appends ": " after it). The PH7_CTX_* levels map to their php analogs, and` |
|        - |  385 | ` * raw E_* errnos passed through by builtins/deprecation sites map by value.` |
|        - |  386 | ` * PH7_CTX_ERR keeps the engine's native "Error" label: many CTX_ERR` |
|        - |  387 | ` * diagnostics are PHL-specific continue-running notices php never prints, so` |
|        - |  388 | ` * claiming php's "Fatal error" there would mislabel them — the per-diagnostic` |
|        - |  389 | ` * severity reclassification is the remaining audit tail. Note the raw` |
|        - |  390 | ` * errno reaches user handlers untouched (e.g. 8192 for E_DEPRECATED); this` |
|        - |  391 | ` * only picks the DISPLAY label.` |
|        - |  392 | ` */` |
|        - |  393 | `/*` |
|        - |  394 | ` * Map an internal severity onto php's error_reporting bit, then ask whether the current` |
|        - |  395 | ` * error_reporting() level wants it. PH7 only had the bErrReport boolean, so ANY non-zero` |
|        - |  396 | `` * level reported everything and `error_reporting(E_ALL & ~E_DEPRECATED)` still printed`` |
|        - |  397 | ` * every deprecation.` |
|        - |  398 | ` */` |
|    34667 |  399 | `PH7_PRIVATE sxi32 PH7_VmErrPhpBit(sxi32 iErr)` |
|        5 |  400 | `{` |
|    34672 |  401 | `	switch( iErr ){` |
|    16050 |  402 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|    32070 |  403 | `		return 2;` |
|       50 |  404 | `	case 512  /* E_USER_WARNING */:` |
|      103 |  405 | `		return 512;` |
|      287 |  406 | `	case PH7_CTX_NOTICE:             /* 3 */` |
|        - |  407 | `	case 8    /* E_NOTICE */:` |
|      579 |  408 | `		return 8;` |
|       39 |  409 | `	case 1024 /* E_USER_NOTICE */:` |
|       83 |  410 | `		return 1024;` |
|      252 |  411 | `	case 8192 /* E_DEPRECATED */:` |
|      508 |  412 | `		return 8192;` |
|       24 |  413 | `	case 16384 /* E_USER_DEPRECATED */:` |
|       51 |  414 | `		return 16384;` |
|      ! 0 |  415 | `	case 256  /* E_USER_ERROR */:` |
|      ! 0 |  416 | `		return 256;` |
|        - |  417 | `	/* The COMPILER's three, which php reports under bits of their own rather` |
|        - |  418 | `	 * than under E_WARNING/E_ERROR: a diagnostic the compiler raises is` |
|        - |  419 | `	 * E_COMPILE_WARNING or E_COMPILE_ERROR, and the parser's own refusal is` |
|        - |  420 | ``	 * E_PARSE. `error_reporting(E_ALL & ~E_COMPILE_WARNING)` hides the first`` |
|        - |  421 | `	 * and leaves a compile-time E_WARNING standing, which is a distinction` |
|        - |  422 | `	 * only these rows can express. */` |
|      242 |  423 | `	case 4    /* E_PARSE */:` |
|      488 |  424 | `		return 4;` |
|      383 |  425 | `	case 64   /* E_COMPILE_ERROR */:` |
|      770 |  426 | `		return 64;` |
|       12 |  427 | `	case 128  /* E_COMPILE_WARNING */:` |
|       26 |  428 | `		return 128;` |
|       12 |  429 | `	default:` |
|       28 |  430 | `		return 1; /* E_ERROR and everything else fatal-ish */` |
|        - |  431 | `	}` |
|    17321 |  432 | `}` |
|    33505 |  433 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|        5 |  434 | `{` |
|    33510 |  435 | `	if( !pVm->bErrReport ){` |
|     4799 |  436 | `		return 0;` |
|        - |  437 | `	}` |
|    28714 |  438 | `	return (pVm->iErrMask & PH7_VmErrPhpBit(iErr)) != 0;` |
|    16756 |  439 | `}` |
|      406 |  440 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|        4 |  441 | `{` |
|      410 |  442 | `	switch(iErr){` |
|      163 |  443 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|        - |  444 | `	case 512  /* E_USER_WARNING */:` |
|      330 |  445 | `		return "Warning";` |
|       23 |  446 | `	case PH7_CTX_NOTICE:           /* 3 */` |
|        - |  447 | `	case 8    /* E_NOTICE */:` |
|        - |  448 | `	case 1024 /* E_USER_NOTICE */:` |
|       50 |  449 | `		return "Notice";` |
|        5 |  450 | `	case 8192  /* E_DEPRECATED */:` |
|        - |  451 | `	case 16384 /* E_USER_DEPRECATED */:` |
|       12 |  452 | `		return "Deprecated";` |
|      ! 0 |  453 | `	case 256 /* E_USER_ERROR */:` |
|      ! 0 |  454 | `		return "Fatal error";` |
|       12 |  455 | `	default:` |
|       28 |  456 | `		return "Error";` |
|        - |  457 | `	}` |
|      207 |  458 | `}` |
|        - |  459 | `/*` |
|        - |  460 | ` * Append php's display-shape diagnostic header/trailer around a message:` |
|        - |  461 | `` * `LABEL: <func(): >message in FILE on line LINE`. The LINE is a fixed 1`` |
|        - |  462 | `` * pending the runtime-line-tracking gate (correct for `-r` one-liners;`` |
|        - |  463 | ` * cross-engine tests wildcard it with %d) — the SHAPE is php's, so tests can` |
|        - |  464 | ` * be authored cross-engine with --EXPECTF--.` |
|        - |  465 | ` */` |
|      128 |  466 | `static void VmDiagnosticHeader(SyBlob *pWorker,sxi32 iErr)` |
|        4 |  467 | `{` |
|      132 |  468 | `	SyBlobFormat(pWorker,"%s: ",VmDiagnosticLabel(iErr));` |
|      132 |  469 | `}` |
|        - |  470 | `/*` |
|        - |  471 | ` * WHERE a diagnostic happened, as php reports it.` |
|        - |  472 | ` *` |
|        - |  473 | ` * Normally the file being executed and the current line, with php's floor of 1 for a` |
|        - |  474 | ` * line the engine never recorded. But a diagnostic can be raised with no PHP frame` |
|        - |  475 | `` * under it at all -- php tests `EG(current_execute_data) == NULL` and then has nothing`` |
|        - |  476 | `` * to name, so `zend_get_executed_filename()` answers the literal string "Unknown" and`` |
|        - |  477 | ` * the line is 0. The shutdown destructor pass is where PHL reaches that state: the` |
|        - |  478 | ` * refusal of a non-public __destruct is raised BETWEEN bodies, after the program's last` |
|        - |  479 | ` * statement. A diagnostic raised INSIDE a destructor body has a frame again and reports` |
|        - |  480 | ` * its real file and line, which is why this is a flag the raise site sets and not the` |
|        - |  481 | ` * whole phase.` |
|        - |  482 | ` */` |
|    38069 |  483 | `static sxu32 VmDiagnosticWhere(ph7_vm *pVm,SyString **ppFile)` |
|        5 |  484 | `{` |
|        - |  485 | `	static SyString sNoFrame = { "Unknown", sizeof("Unknown")-1 };` |
|    38074 |  486 | `	if( pVm->bNoFrameLoc ){` |
|        8 |  487 | `		*ppFile = &sNoFrame;` |
|        8 |  488 | `		return 0;` |
|        - |  489 | `	}` |
|        - |  490 | `	{` |
|        - |  491 | `		/* A prelude builtin has no frame at all in php -- it is an INTERNAL` |
|        - |  492 | `		 * function there -- so a diagnostic raised under one is reported at the` |
|        - |  493 | `		 * call the program wrote, not inside the chunk (which is one line, so` |
|        - |  494 | ``		 * every one of them said `on line 1`). */`` |
|    38068 |  495 | `		SyString *pCallFile = 0;` |
|    38068 |  496 | `		sxu32 nCallLine = 0;` |
|    38068 |  497 | `		if( PH7_VmPreludeBuiltinFrame(&(*pVm),&pCallFile,&nCallLine) ){` |
|       48 |  498 | `			if( pCallFile ){` |
|       48 |  499 | `				*ppFile = pCallFile;` |
|       22 |  500 | `			}` |
|       48 |  501 | `			return nCallLine ? nCallLine : 1;` |
|        - |  502 | `		}` |
|        - |  503 | `	}` |
|        - |  504 | `	{` |
|        - |  505 | `		/* The file the RUNNING code is in, which is the defining file of the` |
|        - |  506 | `		 * innermost active function -- not the top of the include stack. The two` |
|        - |  507 | `		 * agree only while top-level code is running: once a call reaches a` |
|        - |  508 | `		 * function defined in another unit, the include stack has moved on, so` |
|        - |  509 | `		 * every diagnostic raised inside a library named the ENTRY SCRIPT. In a` |
|        - |  510 | `		 * composer tree that is every warning any vendor package raises, and it is` |
|        - |  511 | `		 * what a framework's error handler logs. The line was already right. */` |
|    38024 |  512 | `		SyString *pUnit = PH7_VmExecutingUnitFile(&(*pVm));` |
|    38024 |  513 | `		if( pUnit && pUnit->nByte > 0 ){` |
|    38024 |  514 | `			*ppFile = pUnit;` |
|    18992 |  515 | `		}` |
|        - |  516 | `	}` |
|    38024 |  517 | `	return pVm->nCurLine ? pVm->nCurLine : 1;` |
|    19022 |  518 | `}` |
|      414 |  519 | `static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)` |
|        4 |  520 | `{` |
|      418 |  521 | `	if( pFile ){` |
|        - |  522 | `		/* nLine arrives already normalized by VmDiagnosticWhere: "no line recorded"` |
|        - |  523 | `		 * is php's 1, and a raise with no frame under it is php's literal 0. */` |
|      418 |  524 | `		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,nLine);` |
|      207 |  525 | `	}` |
|      418 |  526 | `}` |
|        - |  527 | `/*` |
|        - |  528 | ``  * php's LOG-shape diagnostic header: `PHP LABEL:  <func(): >` -- the `PHP ` `` |
|        - |  529 | ` * prefix and TWO spaces after the colon, matching the compile-error path` |
|        - |  530 | ` * (compile.c) and stock CLI's stderr log copy.` |
|        - |  531 | ` */` |
|      278 |  532 | `static void VmDiagnosticLogHeader(SyBlob *pWorker,sxi32 iErr)` |
|        4 |  533 | `{` |
|      282 |  534 | `	SyBlobAppend(pWorker,"PHP ",sizeof("PHP ")-1);` |
|      282 |  535 | `	SyBlobFormat(pWorker,"%s:  ",VmDiagnosticLabel(iErr));` |
|      282 |  536 | `}` |
|        - |  537 | `/*` |
|        - |  538 | `` * Prepend php's `func(): ` qualifier to a diagnostic body.`` |
|        - |  539 | ` *` |
|        - |  540 | ` * php puts the raising function's name in the MESSAGE, not in the printed header,` |
|        - |  541 | ` * so its user error handler ($errstr), its error_get_last()['message'] and its` |
|        - |  542 | ` * printed copy all carry the same text. PHL used to add it in the two header` |
|        - |  543 | ` * builders above, i.e. only to the PRINTED copy -- so a set_error_handler() that` |
|        - |  544 | ` * matched on the function name never fired, and error_get_last() answered a body` |
|        - |  545 | ` * php never produces. Building it into the message here is the single place that` |
|        - |  546 | ` * fixes all three. Builtins that already spell the qualifier into their own text` |
|        - |  547 | `` * (`fopen(%s): Failed to open stream`) pass a NULL name and are untouched.`` |
|        - |  548 | ` */` |
|    36874 |  549 | `static void VmDiagnosticQualify(SyBlob *pOut,SyString *pFuncName)` |
|        5 |  550 | `{` |
|    36879 |  551 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|     2000 |  552 | `		SyBlobAppend(pOut,pFuncName->zString,pFuncName->nByte);` |
|     2000 |  553 | `		SyBlobAppend(pOut,"(): ",sizeof("(): ")-1);` |
|      989 |  554 | `	}` |
|    36879 |  555 | `}` |
|        - |  556 | `/*` |
|        - |  557 | ` * Emit a runtime diagnostic as php's two copies, each behind its own ini gate` |
|        - |  558 | ` * (the caller has already cleared the error_reporting() mask and the '@' gate):` |
|        - |  559 | ` *   - LOG copy     -> the error (stderr) stream when log_errors is on:` |
|        - |  560 | ``  *                     `PHP LABEL:  BODY in FILE on line N` `` |
|        - |  561 | ` *   - DISPLAY copy -> the stream display_errors NAMES: program output (stdout) with` |
|        - |  562 | `` *                     a leading blank line, `\nLABEL: BODY in FILE on line N`, or --`` |
|        - |  563 | `` *                     for the `stderr` spelling -- the error stream with no blank`` |
|        - |  564 | ` *                     line. php writes the stderr form with fprintf(), outside the` |
|        - |  565 | ` *                     output layer, so an ob_start() never captures it and it does` |
|        - |  566 | ` *                     not count toward the program's output length.` |
|        - |  567 | `` * BODY already carries php's `func(): ` qualifier (VmDiagnosticQualify).`` |
|        - |  568 | ` * Stock CLI php (display_errors off, log_errors on) writes only the stderr copy,` |
|        - |  569 | ` * keeping program stdout clean. BODY/location are shared; only the header and the` |
|        - |  570 | ` * display copy's leading blank line differ. sWorker is reused across the two` |
|        - |  571 | ` * copies; BODY must live in a separate buffer (it does at both call sites).` |
|        - |  572 | ` */` |
|      428 |  573 | `static sxi32 VmEmitDiagnostic(ph7_vm *pVm,sxi32 iErr,` |
|        - |  574 | `	const char *zBody,sxu32 nBody,SyString *pFile,sxu32 nLine)` |
|        4 |  575 | `{` |
|      432 |  576 | `	SyBlob *pWorker = &pVm->sWorker;` |
|      432 |  577 | `	sxi32 rc = SXRET_OK;` |
|      432 |  578 | `	if( pVm->bLogErrors ){` |
|      282 |  579 | `		SyBlobReset(pWorker);` |
|      282 |  580 | `		VmDiagnosticLogHeader(pWorker,iErr);` |
|      282 |  581 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      282 |  582 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      282 |  583 | `		rc = VmWriteErrorLog(pVm,VmErrConsumer(pVm),pWorker);` |
|      139 |  584 | `	}` |
|      432 |  585 | `	if( pVm->iDisplayErrors != PH7_DISPLAY_ERRORS_OFF ){` |
|      132 |  586 | `		int bErrStream = pVm->iDisplayErrors == PH7_DISPLAY_ERRORS_STDERR;` |
|        - |  587 | `		sxi32 rc2;` |
|      132 |  588 | `		SyBlobReset(pWorker);` |
|      132 |  589 | `		if( !bErrStream ){` |
|        - |  590 | `			/* php's text-mode display copy is prefixed with a blank line */` |
|       87 |  591 | `			SyBlobAppend(pWorker,"\n",sizeof(char));` |
|       42 |  592 | `		}` |
|      132 |  593 | `		VmDiagnosticHeader(pWorker,iErr);` |
|      132 |  594 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      132 |  595 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      218 |  596 | `		rc2 = VmWriteDiagnostic(pVm,` |
|       86 |  597 | `			bErrStream ? VmErrConsumer(pVm) : &pVm->sVmConsumer,pWorker,!bErrStream);` |
|        - |  598 | `		/* keep the first failure (e.g. a PH7_ABORT from a broken stderr) rather` |
|        - |  599 | `		 * than letting a later successful write mask it */` |
|      132 |  600 | `		if( rc == SXRET_OK ){` |
|      132 |  601 | `			rc = rc2;` |
|       64 |  602 | `		}` |
|       64 |  603 | `	}` |
|      432 |  604 | `	return rc;` |
|        4 |  605 | `}` |
|     1908 |  606 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  607 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  608 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  609 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  610 | `	const char *zMessage /* Null terminated error message */` |
|        - |  611 | `	)` |
|        5 |  612 | `{` |
|        - |  613 | `	SyBlob sMsg;` |
|        - |  614 | `	SyString *pFile;` |
|        - |  615 | `	sxu32 nMsg;` |
|        - |  616 | `	sxu32 nLine;` |
|     1913 |  617 | `	sxi32 rc = SXRET_OK;` |
|        - |  618 | `	char zMemMsg[128];` |
|     1913 |  619 | `	if( VmMemLimitMessage(&(*pVm),zMemMsg,sizeof(zMemMsg)) ){` |
|        - |  620 | `		/* Every engine "out of memory" wording downstream of the ceiling becomes` |
|        - |  621 | `		 * php's one sentence. Severity 256 is what makes the label read "Fatal` |
|        - |  622 | `		 * error": PHL's label table maps E_ERROR(1) to its own "Error", and this` |
|        - |  623 | `		 * diagnostic is one users match against php's output, not against the` |
|        - |  624 | `		 * engine's house style. */` |
|      ! 0 |  625 | `		zMessage = zMemMsg;` |
|      ! 0 |  626 | `		iErr = 256;` |
|      ! 0 |  627 | `		pFuncName = 0;` |
|      ! 0 |  628 | `	}` |
|     1913 |  629 | `	nMsg = (sxu32)SyStrlen(zMessage);` |
|     1913 |  630 | `	if( pVm->nSpeculative > 0 ){` |
|        - |  631 | `		/* Speculative evaluation (PH7_VmEvalConstExpr): the value is being LOOKED at,` |
|        - |  632 | `		 * not produced, so this diagnostic never happened. Count it -- the caller reads` |
|        - |  633 | `		 * the counter as "php's compiler would not have folded this". */` |
|      ! 0 |  634 | `		pVm->nSpecDiag++;` |
|      ! 0 |  635 | `		return SXRET_OK;` |
|        - |  636 | `	}` |
|        - |  637 | `	/* Peek the processed file if available */` |
|     1913 |  638 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     1913 |  639 | `	nLine = VmDiagnosticWhere(&(*pVm),&pFile);` |
|     1913 |  640 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     1913 |  641 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|        - |  642 | `		/* Qualify only when there IS a name: PH7_VmMemoryError() reports an` |
|        - |  643 | `		 * out-of-memory fatal through this path with none, and must not need` |
|        - |  644 | `		 * an allocation to say so. */` |
|      726 |  645 | `		VmDiagnosticQualify(&sMsg,pFuncName);` |
|      726 |  646 | `		SyBlobAppend(&sMsg,zMessage,nMsg);` |
|      726 |  647 | `		zMessage = (const char *)SyBlobData(&sMsg);` |
|      726 |  648 | `		nMsg = SyBlobLength(&sMsg);` |
|      356 |  649 | `	}` |
|        - |  650 | `	/* Check for user error handler. php calls it whatever error_reporting() says` |
|        - |  651 | `	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */` |
|     1913 |  652 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)nMsg, pFile, (sxi32)nLine) ){` |
|      265 |  653 | `		VmRecordLastError(&(*pVm),iErr,zMessage,nMsg,pFile,nLine);` |
|      265 |  654 | `		if( VmErrReportWants(pVm,iErr) && pVm->nErrSuppress == 0 ){` |
|        - |  655 | `			/* error_reporting() masks a severity out of the DISPLAY, and inside` |
|        - |  656 | `			 * '@' php still runs the handler (done just above) but prints` |
|        - |  657 | `			 * nothing itself. */` |
|      154 |  658 | `			rc = VmEmitDiagnostic(pVm,iErr,zMessage,nMsg,pFile,nLine);` |
|       75 |  659 | `		}` |
|      130 |  660 | `	}` |
|     1913 |  661 | `	SyBlobRelease(&sMsg);` |
|     1913 |  662 | `	return rc;` |
|      953 |  663 | `}` |
|        - |  664 | `/*` |
|        - |  665 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  666 | ` *` |
|        - |  667 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  668 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  669 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  670 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  671 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  672 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  673 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  674 | ` * calling it from a VM op.` |
|        - |  675 | ` */` |
|      ! 0 |  676 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  677 | `{` |
|      ! 0 |  678 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  679 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  680 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  681 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  682 | `	return PH7_ABORT;` |
|      ! 0 |  683 | `}` |
|        - |  684 | `/*` |
|        - |  685 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  686 | ` */` |
|      ! 0 |  687 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  688 | `{` |
|      ! 0 |  689 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  690 | `}` |
|        - |  691 | `/*` |
|        - |  692 | ` * php 8.1: an implicit float->int conversion deprecates when it loses` |
|        - |  693 | ` * precision. Used by the integer-only OPERATORS (%, \|, &, ^, <<, >>, ~),` |
|        - |  694 | ` * which truncate their operands. An integral float like 2.0 is silent.` |
|        - |  695 | ` * (Builtin int PARAMETERS need the same treatment — they coerce through each` |
|        - |  696 | ` * function's own ph7_value_to_int call, so they ride the recorded ZPP sweep.)` |
|        - |  697 | ` */` |
|        - |  698 | `/*` |
|        - |  699 | `` * TRUE when reading pVal as an `int` would lose what it holds -- the event php`` |
|        - |  700 | `` * 8.1 only DEPRECATES (`Implicit conversion from float 1.9 / float-string "1.9"`` |
|        - |  701 | `` * to int loses precision`) and the scope policy refuses outright.`` |
|        - |  702 | ` *` |
|        - |  703 | ` * Two kinds of value can lose something, and php treats them as one: a FLOAT,` |
|        - |  704 | ` * and a numeric STRING whose bytes spell a double. Which strings those are is` |
|        - |  705 | ` * not just the ones carrying a '.' or an exponent — an integer-shaped run too` |
|        - |  706 | ` * long for an int64 is a double in php too ("99999999999999999999"), and it` |
|        - |  707 | ` * loses digits exactly the same way. So the question is asked of the NUMBER the` |
|        - |  708 | ` * value converts to, whatever spelling it arrived in.` |
|        - |  709 | ` *` |
|        - |  710 | ` * A value is lossy when that number is not an exact int64: outside the range at` |
|        - |  711 | ` * all (NaN and the infinities included), or carrying a fraction. An integral` |
|        - |  712 | `` * float in range (`4.0 % 3`, `$o->i = 5.0`) loses nothing and is not lossy.`` |
|        - |  713 | ` */` |
|   132771 |  714 | `PH7_PRIVATE int VmValueIsLossyToInt(ph7_value *pVal)` |
|        5 |  715 | `{` |
|        - |  716 | `	ph7_real r;` |
|   132776 |  717 | `	if( pVal == 0 ){` |
|      ! 0 |  718 | `		return FALSE;` |
|        - |  719 | `	}` |
|   132776 |  720 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|       37 |  721 | `		r = pVal->rVal;` |
|   132781 |  722 | `	}else if( (pVal->iFlags & MEMOBJ_STRING) && pVal->pVm ){` |
|        - |  723 | `		/* Asked of a COPY: the operand is still needed intact when the answer is` |
|        - |  724 | `		 * no, and a numeric conversion would replace it. */` |
|        - |  725 | `		ph7_value sProbe;` |
|        - |  726 | `		SyString sStr;` |
|        - |  727 | `		int bReal;` |
|   101183 |  728 | `		const char *z = (const char *)SyBlobData(&pVal->sBlob);` |
|   101183 |  729 | `		sxu32 n = SyBlobLength(&pVal->sBlob), i;` |
|        - |  730 | `		/* A string with no '.', no exponent and fewer bytes than the shortest` |
|        - |  731 | `` 		 * out-of-range integer cannot spell a double, so the ordinary `$s % 2` `` |
|        - |  732 | `		 * answers without building anything. Conservative on purpose: it may` |
|        - |  733 | `		 * still probe a string that turns out to be an int, never the reverse. */` |
|   101183 |  734 | `		if( n < 19 ){` |
|   303027 |  735 | `			for( i = 0 ; i < n ; ++i ){` |
|   201901 |  736 | `				if( z[i] == '.' \|\| z[i] == 'e' \|\| z[i] == 'E' ){` |
|       25 |  737 | `					break;` |
|        - |  738 | `				}` |
|   100930 |  739 | `			}` |
|   101177 |  740 | `			if( i >= n ){` |
|   101135 |  741 | `				return FALSE;` |
|        - |  742 | `			}` |
|       23 |  743 | `		}` |
|       54 |  744 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       54 |  745 | `		PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);` |
|       54 |  746 | `		PH7_MemObjToNumeric(&sProbe);` |
|       54 |  747 | `		bReal = (sProbe.iFlags & MEMOBJ_REAL) != 0;` |
|       54 |  748 | `		r = sProbe.rVal;` |
|       54 |  749 | `		PH7_MemObjRelease(&sProbe);` |
|       54 |  750 | `		if( !bReal ){` |
|        9 |  751 | `			return FALSE;` |
|        - |  752 | `		}` |
|       24 |  753 | `	}else{` |
|    31564 |  754 | `		return FALSE;` |
|        - |  755 | `	}` |
|        - |  756 | `	/* The bounds are tested in DOUBLE space, BEFORE the cast: (sxi64)r is` |
|        - |  757 | `	 * undefined outside them, and a test of an undefined cast's result is one an` |
|        - |  758 | `	 * optimiser is entitled to delete -- which is exactly how the printf family's` |
|        - |  759 | `	 * PHP_INT_MIN guard disappeared. NaN fails both comparisons and either` |
|        - |  760 | `	 * infinity fails one, so all three are lossy without a libm predicate. */` |
|        - |  761 | `	/* The cast is a no-op wherever ph7_real is the double this screen is written` |
|        - |  762 | `	 * for. Under PH7_OMIT_FLOATING_POINT ph7_real is sxi64, and handing an` |
|        - |  763 | `	 * integer to a double parameter is a narrowing MSVC reports as C4244 --` |
|        - |  764 | `	 * which /WX makes a build error, so the tiny build is where it bites. */` |
|       81 |  765 | `	if( !PH7_RealFitsInt64((double)r) ){` |
|       26 |  766 | `		return TRUE;` |
|        - |  767 | `	}` |
|       57 |  768 | `	return r != (ph7_real)(sxi64)r;` |
|    66372 |  769 | `}` |
|        - |  770 | ``/* php only DEPRECATES a lossy float(-string) -> int operand (`5 % 2.7`,`` |
|        - |  771 | `` * `3 \| 1.5`, `"1.9" % 2`); PHL targets php's non-deprecated surface and rejects`` |
|        - |  772 | `` * it with a TypeError. An INTEGRAL float (`4.0 % 3`) loses nothing and is`` |
|        - |  773 | ` * accepted. Returns SXRET_OK to continue, or the throw status for the caller to` |
|        - |  774 | ` * route via PH7_DISPATCH_ENFORCE_RC. */` |
|    32127 |  775 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)` |
|        5 |  776 | `{` |
|    32132 |  777 | `	if( !VmValueIsLossyToInt(pVal) ){` |
|    32106 |  778 | `		return SXRET_OK;` |
|        - |  779 | `	}` |
|       27 |  780 | `	return VmThrowFixedError(pVm,"TypeError",` |
|       26 |  781 | `		(pVal->iFlags & MEMOBJ_REAL)` |
|        - |  782 | `			? "Implicit conversion from float to int loses precision"` |
|        - |  783 | `			: "Implicit conversion from float-string to int loses precision");` |
|    16050 |  784 | `}` |
|        - |  785 | `/*` |
|        - |  786 | ` * Single source of truth for the PHP call-depth cap policy.` |
|        - |  787 | ` * Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows` |
|        - |  788 | ` * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,` |
|        - |  789 | ` * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the` |
|        - |  790 | ` * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.` |
|        - |  791 | ` */` |
|  2404017 |  792 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|        5 |  793 | `{` |
|        - |  794 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|        - |  795 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
|  2404022 |  796 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|        5 |  797 | `}` |
|        - |  798 | `/*` |
|        - |  799 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|        - |  800 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|        - |  801 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|        - |  802 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|        - |  803 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|        - |  804 | ` * keep native re-entries off a finite C stack).` |
|        - |  805 | ` */` |
|  4544215 |  806 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|        5 |  807 | `{` |
|  4544220 |  808 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
|        5 |  809 | `}` |
|        - |  810 | `/*` |
|        - |  811 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|        - |  812 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|        - |  813 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|        - |  814 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|        - |  815 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|        - |  816 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|        - |  817 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|        - |  818 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|        - |  819 | ` * register_shutdown_function() callbacks. Raised by OP_CALL only (the sole` |
|        - |  820 | ` * site testing the PHP call-depth cap); native nesting has its own fatal` |
|        - |  821 | ` * (VmNativeNestingFatal).` |
|        - |  822 | ` *` |
|        - |  823 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|        - |  824 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|        - |  825 | ` * re-enter and loop.` |
|        - |  826 | ` */` |
|        2 |  827 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|        1 |  828 | `{` |
|        3 |  829 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  830 | `		return PH7_ABORT;` |
|        - |  831 | `	}` |
|        3 |  832 | `	pVm->iExitStatus = 255;` |
|        3 |  833 | `	pVm->bHaltRequested = 1;` |
|        3 |  834 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|        3 |  835 | `	return PH7_ABORT;` |
|        2 |  836 | `}` |
|        - |  837 | `/*` |
|        - |  838 | ` * Sibling of VmRecursionFatal for the NATIVE nesting bound (see the` |
|        - |  839 | ` * VmByteCodeExec wrapper): same clean-halt semantics, its own message so the` |
|        - |  840 | ` * two limits are distinguishable. Non-catchable for the same at-depth reason.` |
|        - |  841 | ` */` |
|        4 |  842 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm)` |
|        2 |  843 | `{` |
|        6 |  844 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  845 | `		return PH7_ABORT;` |
|        - |  846 | `	}` |
|        6 |  847 | `	pVm->iExitStatus = 255;` |
|        6 |  848 | `	pVm->bHaltRequested = 1;` |
|        6 |  849 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|        6 |  850 | `	return PH7_ABORT;` |
|        4 |  851 | `}` |
|        - |  852 | `/*` |
|        - |  853 | `` * ext/pcntl's `Error installing signal handler for %d`, and it is the same`` |
|        - |  854 | ` * clean-halt fatal as the two above rather than a catchable Error -- because` |
|        - |  855 | ` * php's is not catchable either: a disposition sigaction() refuses (SIGKILL,` |
|        - |  856 | ` * SIGSTOP) is an E_ERROR under php, not the ValueError every other pcntl_signal()` |
|        - |  857 | ` * refusal is. Raised only from PH7_builtin_pcntl_signal.` |
|        - |  858 | ` */` |
|      ! 0 |  859 | `PH7_PRIVATE sxi32 PH7_VmSignalInstallFatal(ph7_vm *pVm,int signo)` |
|      ! 0 |  860 | `{` |
|      ! 0 |  861 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  862 | `		return PH7_ABORT;` |
|        - |  863 | `	}` |
|      ! 0 |  864 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  865 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  866 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Error installing signal handler for %d",signo);` |
|      ! 0 |  867 | `	return PH7_ABORT;` |
|      ! 0 |  868 | `}` |
|        - |  869 | `/*` |
|        - |  870 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  871 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  872 | ` * information.` |
|        - |  873 | ` */` |
|        - |  874 | `/*` |
|        - |  875 | ` * Did memory_limit just stop this script, and if so what does php call it?` |
|        - |  876 | ` *` |
|        - |  877 | ` * The allocation that crossed the ceiling returned NULL into whichever site asked` |
|        - |  878 | ` * for it, and that site then complains in its OWN words -- "PH7 is running out of` |
|        - |  879 | ` * memory while loading variable", and a dozen others, each naming the operation` |
|        - |  880 | ` * that happened to be unlucky. php has ONE sentence for this, and it is the one` |
|        - |  881 | ` * every framework's OOM triage greps for, so the first diagnostic raised after the` |
|        - |  882 | ` * ceiling is hit becomes that sentence whatever the site meant to say.` |
|        - |  883 | ` *` |
|        - |  884 | ` * Both raise funnels consult this (the va_list one and the plain-message one): which` |
|        - |  885 | ` * of them a given allocation failure happens to reach is an accident of the site, and` |
|        - |  886 | ` * the user-visible answer must not be.` |
|        - |  887 | ` *` |
|        - |  888 | ` * Answers 0 and writes nothing when no ceiling was hit. Clears the flag, so a script` |
|        - |  889 | ` * that somehow survives its own allocation failure is not told twice -- and so the` |
|        - |  890 | ` * PH7_VmThrowError this returns into does not see it again and recurse.` |
|        - |  891 | ` */` |
|    38061 |  892 | `static int VmMemLimitMessage(ph7_vm *pVm,char *zBuf,sxu32 nBuf)` |
|        5 |  893 | `{` |
|    38066 |  894 | `	if( pVm->sAllocator.nMemTried == 0 ){` |
|    38066 |  895 | `		return 0;` |
|        - |  896 | `	}` |
|      ! 0 |  897 | `	SyBufferFormat(zBuf,nBuf,` |
|        - |  898 | `		"Allowed memory size of %u bytes exhausted (tried to allocate %u bytes)",` |
|      ! 0 |  899 | `		pVm->sAllocator.nMemLimitHit,pVm->sAllocator.nMemTried);` |
|      ! 0 |  900 | `	pVm->sAllocator.nMemTried = 0;` |
|      ! 0 |  901 | `	return 1;` |
|    19018 |  902 | `}` |
|    36153 |  903 | `static sxi32 VmThrowErrorAp(` |
|        - |  904 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  905 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  906 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  907 | `	const char *zFormat, /* Format message */` |
|        - |  908 | `	va_list ap           /* Variable list of arguments */` |
|        - |  909 | `	)` |
|        5 |  910 | `{` |
|        - |  911 | `	SyBlob sMsg;` |
|        - |  912 | `	SyString *pFile;` |
|        - |  913 | `	sxu32 nLine;` |
|    36158 |  914 | `	sxi32 rc = SXRET_OK;` |
|        - |  915 | `	char zMemMsg[128];` |
|    36158 |  916 | `	if( pVm->nSpeculative > 0 ){` |
|        - |  917 | `		/* See PH7_VmThrowError: nothing a speculative evaluation raises is observable. */` |
|      ! 0 |  918 | `		pVm->nSpecDiag++;` |
|      ! 0 |  919 | `		return SXRET_OK;` |
|        - |  920 | `	}` |
|    36158 |  921 | `	if( VmMemLimitMessage(&(*pVm),zMemMsg,sizeof(zMemMsg)) ){` |
|      ! 0 |  922 | `		return PH7_VmThrowError(&(*pVm),0,256,zMemMsg);` |
|        - |  923 | `	}` |
|        - |  924 | `	/* Peek the processed file if available */` |
|    36158 |  925 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    36158 |  926 | `	nLine = VmDiagnosticWhere(&(*pVm),&pFile);` |
|        - |  927 | ``	/* Format the raw message behind php's `func(): ` qualifier */`` |
|    36158 |  928 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|    36158 |  929 | `	VmDiagnosticQualify(&sMsg,pFuncName);` |
|    36158 |  930 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  931 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|        - |  932 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|        - |  933 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|        - |  934 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|    36158 |  935 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)nLine) ){` |
|        - |  936 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|        - |  937 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|    31904 |  938 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile,nLine);` |
|    31904 |  939 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|    31626 |  940 | `			SyBlobRelease(&sMsg);` |
|    31626 |  941 | `			return SXRET_OK;` |
|        - |  942 | `		}` |
|      421 |  943 | `		rc = VmEmitDiagnostic(pVm,iErr,(const char *)SyBlobData(&sMsg),` |
|      139 |  944 | `			SyBlobLength(&sMsg),pFile,nLine);` |
|      139 |  945 | `	}` |
|     4537 |  946 | `	SyBlobRelease(&sMsg);` |
|     4537 |  947 | `	return rc;` |
|    18070 |  948 | `}` |
|        - |  949 | `/*` |
|        - |  950 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|        - |  951 | ` * scope), or NULL when executing outside any class context.` |
|        - |  952 | ` */` |
|    15084 |  953 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|        5 |  954 | `{` |
|    15089 |  955 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      211 |  956 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      211 |  957 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        - |  958 | `	}` |
|    14883 |  959 | `	return 0;` |
|     7481 |  960 | `}` |
|        - |  961 | `/*` |
|        - |  962 | ` * May the engine run an exception class's __construct for a throw it is raising` |
|        - |  963 | ` * itself? Yes, until the nesting gets absurd. The constructor CALL can throw in` |
|        - |  964 | ` * turn (a message-formatting error, or — the case that forced this — a` |
|        - |  965 | ` * constructor whose own class is not method-mounted yet, which OP_CALL reports` |
|        - |  966 | ` * as an undefined function and therefore as another engine throw). Each such` |
|        - |  967 | ` * throw would construct another exception and recurse until the native-nesting` |
|        - |  968 | ` * cap halted the VM with no diagnostic. A small cap keeps legitimate nesting` |
|        - |  969 | ` * (an engine throw from inside a user exception's constructor) working and` |
|        - |  970 | ` * stops the self-feeding case at four levels: the innermost exception is simply` |
|        - |  971 | ` * left with an empty message. On TRUE the caller must decrement nExcCtorDepth` |
|        - |  972 | ` * after the call.` |
|        - |  973 | ` */` |
|        - |  974 | `#define VM_EXC_CTOR_MAX_DEPTH 4` |
|   351201 |  975 | `static int VmExcCtorEnter(ph7_vm *pVm)` |
|        5 |  976 | `{` |
|   351206 |  977 | `	if( pVm->nExcCtorDepth >= VM_EXC_CTOR_MAX_DEPTH ){` |
|      ! 0 |  978 | `		return 0;` |
|        - |  979 | `	}` |
|   351206 |  980 | `	pVm->nExcCtorDepth++;` |
|   351206 |  981 | `	return 1;` |
|   175604 |  982 | `}` |
|        - |  983 | `/*` |
|        - |  984 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|        - |  985 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|        - |  986 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|        - |  987 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|        - |  988 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|        - |  989 | ` */` |
|   206387 |  990 | `PH7_PRIVATE sxi32 VmThrowBuiltinErrorCode(ph7_vm *pVm,const char *zClass,sxu32 nClass,` |
|        - |  991 | `	SyBlob *pMsg,sxi32 iCode)` |
|        5 |  992 | `{` |
|        - |  993 | `	ph7_class *pErrClass;` |
|        - |  994 | `	ph7_class_instance *pThis;` |
|        - |  995 | `	ph7_class_method *pCons;` |
|        - |  996 | `	VmFrame *pFrame;` |
|        - |  997 | `	sxi32 rc;` |
|   206392 |  998 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|   206392 |  999 | `	if( pErrClass == 0 ){` |
|      ! 0 | 1000 | `		SyBlobRelease(pMsg);` |
|      ! 0 | 1001 | `		return PH7_ABORT;` |
|        - | 1002 | `	}` |
|   206392 | 1003 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|   206392 | 1004 | `	if( pThis == 0 ){` |
|      ! 0 | 1005 | `		SyBlobRelease(pMsg);` |
|      ! 0 | 1006 | `		return PH7_ABORT;` |
|        - | 1007 | `	}` |
|   206392 | 1008 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|   206392 | 1009 | `	if( pCons && VmExcCtorEnter(&(*pVm)) ){` |
|        - | 1010 | `		ph7_value sArg,sCode;` |
|        - | 1011 | `		ph7_value *apArg[2];` |
|        - | 1012 | `		SyString sMsgStr;` |
|   206392 | 1013 | `		int nArg = 1;` |
|   206392 | 1014 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|   206392 | 1015 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|   206392 | 1016 | `		apArg[0] = &sArg;` |
|   206392 | 1017 | `		if( iCode != 0 ){` |
|        - | 1018 | `			/* php's $code, second constructor argument -- the DOMException codes a` |
|        - | 1019 | `			 * program compares against (DOM_NOT_FOUND_ERR & co) travel this way. */` |
|       45 | 1020 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|       45 | 1021 | `			apArg[1] = &sCode;` |
|       45 | 1022 | `			nArg = 2;` |
|       22 | 1023 | `		}` |
|   206392 | 1024 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|   206392 | 1025 | `		if( iCode != 0 ){` |
|       45 | 1026 | `			PH7_MemObjRelease(&sCode);` |
|       22 | 1027 | `		}` |
|   206392 | 1028 | `		PH7_MemObjRelease(&sArg);` |
|   206392 | 1029 | `		pVm->nExcCtorDepth--;` |
|   103192 | 1030 | `	}` |
|   206392 | 1031 | `	SyBlobRelease(pMsg);` |
|   206392 | 1032 | `	pFrame = pVm->pFrame;` |
|   206392 | 1033 | `	if( pFrame ){` |
|   206392 | 1034 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   206392 | 1035 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|   103192 | 1036 | `	}` |
|   206392 | 1037 | `	rc = VmThrowException(&(*pVm),pThis);` |
|   206392 | 1038 | `	PH7_ClassInstanceUnref(pThis);` |
|   206392 | 1039 | `	if( rc == SXERR_ABORT ){` |
|       38 | 1040 | `		return PH7_ABORT;` |
|        - | 1041 | `	}` |
|   206358 | 1042 | `	return PH7_EXCEPTION;` |
|   103197 | 1043 | `}` |
|        - | 1044 | `/* The same with php's default $code of 0, which is what all but the DOM refusals` |
|        - | 1045 | ` * carry. */` |
|   205852 | 1046 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|        5 | 1047 | `{` |
|   205857 | 1048 | `	return VmThrowBuiltinErrorCode(pVm,zClass,nClass,pMsg,0);` |
|        5 | 1049 | `}` |
|        - | 1050 | `/*` |
|        - | 1051 | ` * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.` |
|        - | 1052 | ` * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that` |
|        - | 1053 | ` * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT` |
|        - | 1054 | ` * when the class is unavailable / the engine is aborting) so the caller routes the` |
|        - | 1055 | ` * result through its normal goto Exception / goto Abort.` |
|        - | 1056 | ` */` |
|      320 | 1057 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)` |
|        4 | 1058 | `{` |
|      324 | 1059 | `	return VmThrowFixedErrorCode(pVm,zClass,0,zMsg);` |
|        4 | 1060 | `}` |
|        - | 1061 | `/* The same, carrying php's $code: what a native class's property handler refused` |
|        - | 1062 | ` * with (PH7_NativePropCtx::iThrowCode) is raised through here. */` |
|      535 | 1063 | `PH7_PRIVATE sxi32 VmThrowFixedErrorCode(ph7_vm *pVm,const char *zClass,sxi32 iCode,` |
|        - | 1064 | `	const char *zMsg)` |
|        5 | 1065 | `{` |
|        - | 1066 | `	SyBlob sMsg;` |
|      540 | 1067 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|      540 | 1068 | `	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));` |
|      540 | 1069 | `	return VmThrowBuiltinErrorCode(pVm, zClass, SyStrlen(zClass), &sMsg, iCode);` |
|        5 | 1070 | `}` |
|        - | 1071 | `/*` |
|        - | 1072 | ` * Enum case singletons (PHP 8.1).` |
|        - | 1073 | ` *` |
|        - | 1074 | `` * Each `case` of an enum is a class constant (PH7_CLASS_ATTR_ENUMCASE) whose`` |
|        - | 1075 | ` * slot holds THE singleton instance of the enum class for that case; strict` |
|        - | 1076 | `` * `===` between two accesses is then the ordinary instance-pointer identity.`` |
|        - | 1077 | ` * Materialization is lazy and all-at-once on first access, matching php: the` |
|        - | 1078 | ` * backing-value type check and the duplicate-value check only fire when a` |
|        - | 1079 | ` * case (or cases()/from()/tryFrom()) is first touched.` |
|        - | 1080 | ` */` |
|        - | 1081 | `/* Write [pSrcVal] into the instance property [zProp] of [pObj], clearing the` |
|        - | 1082 | ` * readonly write-once latch so later user writes raise php's "Cannot modify` |
|        - | 1083 | ` * readonly property" through the normal store path. */` |
|        - | 1084 |  |
|        - | 1085 | ``/* Return the backing value (the `value` property) of an already-materialized`` |
|        - | 1086 | ` * enum case, or 0 when unavailable (pure enum / not yet materialized). */` |
|      430 | 1087 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase)` |
|        2 | 1088 | `{` |
|      432 | 1089 | `	ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pCase->nIdx);` |
|        - | 1090 | `	ph7_class_instance *pObj;` |
|        - | 1091 | `	SyHashEntry *pEntry;` |
|      432 | 1092 | `	if( pSlot == 0 \|\| (pSlot->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       70 | 1093 | `		return 0;` |
|        - | 1094 | `	}` |
|      364 | 1095 | `	pObj = (ph7_class_instance *)pSlot->x.pOther;` |
|      364 | 1096 | `	pEntry = SyHashGet(&pObj->hAttr,"value",sizeof("value")-1);` |
|      364 | 1097 | `	if( pEntry == 0 ){` |
|      ! 0 | 1098 | `		return 0;` |
|        - | 1099 | `	}` |
|      364 | 1100 | `	return (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,((VmClassAttr *)pEntry->pUserData)->nIdx);` |
|      217 | 1101 | `}` |
|        - | 1102 | `/*` |
|        - | 1103 | ` * Raise the pending self-referencing-constant Error recorded by an inner` |
|        - | 1104 | ` * initializer evaluation (pConstCycleAttr). Called only at nConstEvalDepth 0 —` |
|        - | 1105 | ` * a throw inside an initializer mini-exec cannot be routed to a user catch` |
|        - | 1106 | ` * (pre-existing engine restriction), so the outermost, opcode-level evaluation` |
|        - | 1107 | ` * raises it. Returns the throw status to park/route.` |
|        - | 1108 | ` */` |
|        2 | 1109 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm)` |
|        1 | 1110 | `{` |
|        - | 1111 | `	SyBlob sMsg;` |
|        3 | 1112 | `	ph7_class_attr *pAttr = pVm->pConstCycleAttr;` |
|        3 | 1113 | `	ph7_class *pOwner = pVm->pConstCycleClass;` |
|        3 | 1114 | `	pVm->pConstCycleAttr = 0;` |
|        3 | 1115 | `	pVm->pConstCycleClass = 0;` |
|        3 | 1116 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 1117 | `	SyBlobFormat(&sMsg,"Cannot declare self-referencing constant %z::%z",` |
|        1 | 1118 | `		pOwner ? &pOwner->sDisp : &pAttr->sName,&pAttr->sName);` |
|        3 | 1119 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1120 | `}` |
|        - | 1121 | `/*` |
|        - | 1122 | ` * Materialize ONE case singleton of an enum class. php 8.1 semantics: cases` |
|        - | 1123 | ` * materialize lazily and individually on first access — the backing-value` |
|        - | 1124 | ` * type check fires per case, and the duplicate-value check compares only` |
|        - | 1125 | ` * against cases that have already materialized (a broken sibling case does` |
|        - | 1126 | ` * not poison a valid one). Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT` |
|        - | 1127 | ` * of a thrown catchable error — TypeError (backing type mismatch) or Error` |
|        - | 1128 | ` * (duplicate value / self-reference) — which the caller routes` |
|        - | 1129 | ` * (VmBoundaryPark at an opcode site, direct return from a builtin thunk).` |
|        - | 1130 | ` */` |
|      571 | 1131 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)` |
|        5 | 1132 | `{` |
|        - | 1133 | `	ph7_class_attr **apCase;` |
|        - | 1134 | `	ph7_class_instance *pObj;` |
|        - | 1135 | `	ph7_value *pSlot;` |
|        - | 1136 | `	ph7_value sBacking,sPropVal;` |
|        - | 1137 | `	sxu32 i;` |
|      576 | 1138 | `	if( pCase->nIdx != SXU32_HIGH ){` |
|      401 | 1139 | `		return SXRET_OK;` |
|        - | 1140 | `	}` |
|      176 | 1141 | `	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - | 1142 | ``		/* `case A = self::A->value` — record the cycle; the outermost`` |
|        - | 1143 | `		 * evaluation raises it (see VmConstCycleThrow). */` |
|      ! 0 | 1144 | `		if( pVm->pConstCycleAttr == 0 ){` |
|      ! 0 | 1145 | `			pVm->pConstCycleAttr = pCase;` |
|      ! 0 | 1146 | `			pVm->pConstCycleClass = pClass;` |
|      ! 0 | 1147 | `		}` |
|      ! 0 | 1148 | `		return SXRET_OK;` |
|        - | 1149 | `	}` |
|      176 | 1150 | `	PH7_MemObjInit(pVm,&sBacking);` |
|      176 | 1151 | `	if( pClass->nEnumBacking != 0 ){` |
|      108 | 1152 | `		if( pCase->pNativeValue ){` |
|        - | 1153 | `			/* A NATIVE enum states its backing value as a literal: there is no` |
|        - | 1154 | `			 * compiler to have emitted the byte-code branch below, and a literal` |
|        - | 1155 | `			 * is what that byte-code would have produced anyway. */` |
|        5 | 1156 | `			PH7_NativeLiteralValue(&(*pVm),pCase->pNativeValue,&sBacking);` |
|      106 | 1157 | `		}else if( SySetUsed(&pCase->aByteCode) > 0 ){` |
|        - | 1158 | ``			/* pConstEvalClass: `case A = self::OFF + 1` resolves self::, and the`` |
|        - | 1159 | `			 * frame marker keeps it reachable when the first access happens` |
|        - | 1160 | `			 * inside another class's method (VmLocalExec pushes no frame, so` |
|        - | 1161 | `			 * that method's frame is still the current one). */` |
|      104 | 1162 | `			ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      104 | 1163 | `			void *pSaveFrame = pVm->pConstEvalFrame;` |
|        - | 1164 | `			sxi32 rcExec;` |
|      104 | 1165 | `			pVm->pConstEvalClass = pClass;` |
|      104 | 1166 | `			pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|      104 | 1167 | `			pCase->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      104 | 1168 | `			pVm->nConstEvalDepth++;` |
|      104 | 1169 | `			rcExec = VmLocalExec(&(*pVm),&pCase->aByteCode,&sBacking,FALSE);` |
|      104 | 1170 | `			pVm->nConstEvalDepth--;` |
|      104 | 1171 | `			pCase->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      104 | 1172 | `			pVm->pConstEvalClass = pSaveCtx;` |
|      104 | 1173 | `			pVm->pConstEvalFrame = pSaveFrame;` |
|      104 | 1174 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 1175 | `				/* The backing expression raised: abandon materialization and` |
|        - | 1176 | `				 * hand the status to the caller to park/route. */` |
|        3 | 1177 | `				PH7_MemObjRelease(&sBacking);` |
|        3 | 1178 | `				return rcExec;` |
|        - | 1179 | `			}` |
|      101 | 1180 | `			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|      ! 0 | 1181 | `				PH7_MemObjRelease(&sBacking);` |
|      ! 0 | 1182 | `				return VmConstCycleThrow(&(*pVm));` |
|        - | 1183 | `			}` |
|       49 | 1184 | `		}` |
|      105 | 1185 | `		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){` |
|        - | 1186 | `			/* php: TypeError, checked lazily at first case access */` |
|        - | 1187 | `			SyBlob sMsg;` |
|        3 | 1188 | `			const char *zGiven = ph7_type_name(&sBacking);` |
|        3 | 1189 | `			PH7_MemObjRelease(&sBacking);` |
|        3 | 1190 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        2 | 1191 | `			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",` |
|        2 | 1192 | `				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");` |
|        3 | 1193 | `			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        - | 1194 | `		}` |
|      103 | 1195 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        - | 1196 | `			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL\|MEMOBJ_INT,` |
|        - | 1197 | `			 * the typed-constant leniency) to a genuine int. */` |
|       30 | 1198 | `			PH7_MemObjToInteger(&sBacking);` |
|       16 | 1199 | `		}else{` |
|       75 | 1200 | `			PH7_MemObjToString(&sBacking);` |
|        - | 1201 | `		}` |
|        - | 1202 | `		/* php: two cases sharing one backing value are an Error — compared` |
|        - | 1203 | `		 * against already-materialized cases only (php registers values as` |
|        - | 1204 | `		 * each case evaluates). */` |
|      103 | 1205 | `		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      333 | 1206 | `		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){` |
|        - | 1207 | `			ph7_value *pPrev;` |
|      235 | 1208 | `			int bDup = 0;` |
|      235 | 1209 | `			if( apCase[i] == pCase ){` |
|      101 | 1210 | `				continue;` |
|        - | 1211 | `			}` |
|      136 | 1212 | `			pPrev = VmEnumCaseBackingValue(&(*pVm),apCase[i]);` |
|      136 | 1213 | `			if( pPrev ){` |
|       68 | 1214 | `				if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       16 | 1215 | `					bDup = (pPrev->x.iVal == sBacking.x.iVal);` |
|        9 | 1216 | `				}else{` |
|       66 | 1217 | `					bDup = SyBlobLength(&pPrev->sBlob) == SyBlobLength(&sBacking.sBlob)` |
|       52 | 1218 | `						&& SyMemcmp(SyBlobData(&pPrev->sBlob),SyBlobData(&sBacking.sBlob),` |
|       24 | 1219 | `							SyBlobLength(&sBacking.sBlob)) == 0;` |
|        - | 1220 | `				}` |
|       33 | 1221 | `			}` |
|      136 | 1222 | `			if( bDup ){` |
|        - | 1223 | `				/* php prints the two cases in DECLARATION order regardless of` |
|        - | 1224 | `				 * which one is being evaluated. */` |
|        3 | 1225 | `				ph7_class_attr *pFirst = apCase[i], *pSecond = pCase;` |
|        - | 1226 | `				SyBlob sMsg;` |
|        - | 1227 | `				sxu32 j;` |
|        5 | 1228 | `				for( j = 0 ; j < SySetUsed(&pClass->aEnumCases) ; j++ ){` |
|        5 | 1229 | `					if( apCase[j] == pCase ){ break; }` |
|        2 | 1230 | `				}` |
|        3 | 1231 | `				if( j < i ){` |
|      ! 0 | 1232 | `					pFirst = pCase;` |
|      ! 0 | 1233 | `					pSecond = apCase[i];` |
|      ! 0 | 1234 | `				}` |
|        3 | 1235 | `				PH7_MemObjRelease(&sBacking);` |
|        3 | 1236 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 1237 | `				SyBlobFormat(&sMsg,"Duplicate value in enum %z for cases %z and %z",` |
|        1 | 1238 | `					&pClass->sDisp,&pFirst->sName,&pSecond->sName);` |
|        3 | 1239 | `				return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 1240 | `			}` |
|       68 | 1241 | `		}` |
|       49 | 1242 | `	}` |
|        - | 1243 | `	/* Create the singleton and fill its readonly props */` |
|      169 | 1244 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|      169 | 1245 | `	if( pObj == 0 ){` |
|      ! 0 | 1246 | `		PH7_MemObjRelease(&sBacking);` |
|      ! 0 | 1247 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1248 | `			"Cannot create enum case %z::%z due to a memory failure",` |
|      ! 0 | 1249 | `			&pClass->sDisp,&pCase->sName);` |
|      ! 0 | 1250 | `		return PH7_ABORT;` |
|        - | 1251 | `	}` |
|      169 | 1252 | `	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);` |
|      169 | 1253 | `	PH7_NativeSetProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);` |
|      169 | 1254 | `	PH7_MemObjRelease(&sPropVal);` |
|      169 | 1255 | `	if( pClass->nEnumBacking != 0 ){` |
|      101 | 1256 | `		PH7_NativeSetProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);` |
|       49 | 1257 | `	}` |
|      169 | 1258 | `	PH7_MemObjRelease(&sBacking);` |
|        - | 1259 | `	/* Park the singleton in the case's constant slot. The slot takes over` |
|        - | 1260 | `	 * the instance's initial iRef=1 (synthesized-object invariant). */` |
|      169 | 1261 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      169 | 1262 | `	if( pSlot == 0 ){` |
|      ! 0 | 1263 | `		PH7_ClassInstanceUnref(pObj);` |
|      ! 0 | 1264 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1265 | `			"Cannot reserve a memory object for enum case %z::%z",` |
|      ! 0 | 1266 | `			&pClass->sDisp,&pCase->sName);` |
|      ! 0 | 1267 | `		return PH7_ABORT;` |
|        - | 1268 | `	}` |
|      169 | 1269 | `	pSlot->x.pOther = pObj;` |
|      169 | 1270 | `	MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|      169 | 1271 | `	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      169 | 1272 | `	pCase->nIdx = pSlot->nIdx;` |
|      169 | 1273 | `	return SXRET_OK;` |
|      288 | 1274 | `}` |
|        - | 1275 | `/*` |
|        - | 1276 | ` * Materialize EVERY case singleton of [pClass], in declaration order — the` |
|        - | 1277 | ` * cases()/from()/tryFrom() entry point (php equally evaluates all cases` |
|        - | 1278 | ` * there, so a broken case surfaces its error at the same point).` |
|        - | 1279 | ` */` |
|      269 | 1280 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1281 | `{` |
|        - | 1282 | `	ph7_class_attr **apCase;` |
|        - | 1283 | `	sxu32 n;` |
|      274 | 1284 | `	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1285 | `		return SXRET_OK;` |
|        - | 1286 | `	}` |
|      274 | 1287 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      823 | 1288 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|      560 | 1289 | `		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);` |
|      560 | 1290 | `		if( rc != SXRET_OK ){` |
|        8 | 1291 | `			return rc;` |
|        - | 1292 | `		}` |
|      276 | 1293 | `	}` |
|      267 | 1294 | `	return SXRET_OK;` |
|      139 | 1295 | `}` |
|        - | 1296 | `/*` |
|        - | 1297 | ` * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,` |
|        - | 1298 | ` * or 0 when the name does not name an enum.` |
|        - | 1299 | ` */` |
|      220 | 1300 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)` |
|        3 | 1301 | `{` |
|        - | 1302 | `	ph7_class *pClass;` |
|      223 | 1303 | `	if( (pName->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pName->sBlob) < 1 ){` |
|      ! 0 | 1304 | `		return 0;` |
|        - | 1305 | `	}` |
|      332 | 1306 | `	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),` |
|      109 | 1307 | `		SyBlobLength(&pName->sBlob),FALSE,0);` |
|      227 | 1308 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|        6 | 1309 | `		pClass = pClass->pNextName;` |
|        2 | 1310 | `	}` |
|      223 | 1311 | `	return pClass;` |
|      112 | 1312 | `}` |
|        - | 1313 | `/*` |
|        - | 1314 | ` * The line a LAZY class initializer about to run should report a throw at: the` |
|        - | 1315 | ` * line of the access that triggered it. Answers 0 — meaning "keep the` |
|        - | 1316 | ` * initializer's own line" — when the access site is INTERNAL code (a prelude` |
|        - | 1317 | ` * chunk, e.g. ReflectionClass::getConstants() calling the materializer): its` |
|        - | 1318 | ` * line numbers belong to an embedded source that PH7_VmStampThrowableSite will` |
|        - | 1319 | ` * not name, so reporting one would pair a foreign line with the user's file.` |
|        - | 1320 | ` */` |
|      544 | 1321 | `static sxu32 VmLazyInitLineHere(ph7_vm *pVm)` |
|        5 | 1322 | `{` |
|      549 | 1323 | `	VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      544 | 1324 | `	if( pInner && pInner->pUserData` |
|      334 | 1325 | `	 && ((ph7_vm_func *)pInner->pUserData)->sFile.nByte == 0 ){` |
|      ! 0 | 1326 | `		return 0;` |
|        - | 1327 | `	}` |
|      549 | 1328 | `	return pVm->nCurLine;` |
|      277 | 1329 | `}` |
|        - | 1330 | `/*` |
|        - | 1331 | ` * Hand a reserved memory-object slot back to the free list. Takes the INDEX,` |
|        - | 1332 | ` * not the pointer -- which used to be load-bearing, because a nested evaluation` |
|        - | 1333 | ` * (an initializer, or the constructor of the very TypeError being raised) grew` |
|        - | 1334 | ` * and REALLOC'd the pool and left a pointer taken before it dangling. P1's fixed` |
|        - | 1335 | ` * segments retired that; an index is still the right currency for a free, since` |
|        - | 1336 | ` * the free list is keyed by one. The slot's contents are released first:` |
|        - | 1337 | ` * PH7_ReserveMemObj re-inits a recycled slot without releasing it, so a string` |
|        - | 1338 | ` * blob / array / object left in there would be orphaned once per evaluation,` |
|        - | 1339 | ` * which for a constant that re-evaluates on every access grows without bound.` |
|        - | 1340 | ` */` |
|       36 | 1341 | `static void VmRecycleMemObj(ph7_vm *pVm,sxu32 nIdx)` |
|        3 | 1342 | `{` |
|       39 | 1343 | `	ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|       39 | 1344 | `	if( pObj == 0 ){` |
|      ! 0 | 1345 | `		return;` |
|        - | 1346 | `	}` |
|       39 | 1347 | `	PH7_MemObjRelease(pObj);` |
|       39 | 1348 | `	VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       21 | 1349 | `}` |
|        - | 1350 | `/*` |
|        - | 1351 | ` * Evaluate a class constant's initializer on demand.` |
|        - | 1352 | ` *` |
|        - | 1353 | ` * Constant slots are normally filled eagerly at class mount, but a constant` |
|        - | 1354 | ` * whose initializer references ANOTHER not-yet-mounted constant (same class —` |
|        - | 1355 | `` * `const B = self::A + 1` — or a class mounted later in hash order) reaches`` |
|        - | 1356 | ` * OP_MEMBER with nIdx still unset; before this helper the load silently` |
|        - | 1357 | ` * produced NULL (a mount-order-dependent silent wrong answer, surfaced by the` |
|        - | 1358 | ` * enum work, 13 Jul 2026). Evaluates the initializer now — with` |
|        - | 1359 | ` * pConstEvalClass set so self::/parent:: resolve — memoizes the slot, and` |
|        - | 1360 | ` * leaves the mount loop's later visit to skip it (nIdx already set).` |
|        - | 1361 | ` * A re-entrant evaluation of the SAME constant is php's catchable` |
|        - | 1362 | ` * "Cannot declare self-referencing constant" Error.` |
|        - | 1363 | ` */` |
|     1248 | 1364 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 1365 | `{` |
|        - | 1366 | `	ph7_value *pMemObj;` |
|     1248 | 1367 | `	if( pAttr->nIdx != SXU32_HIGH` |
|     1248 | 1368 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|     1253 | 1369 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){` |
|      ! 0 | 1370 | `		return SXRET_OK;` |
|        - | 1371 | `	}` |
|     1253 | 1372 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - | 1373 | `		/* Cycle: record it for the OUTERMOST evaluation to raise` |
|        - | 1374 | `		 * (VmConstCycleThrow) — a throw at this inner level would be lost` |
|        - | 1375 | `		 * inside the initializer mini-exec. Loads NULL benignly here. */` |
|        3 | 1376 | `		if( pVm->pConstCycleAttr == 0 ){` |
|        3 | 1377 | `			pVm->pConstCycleAttr = pAttr;` |
|        3 | 1378 | `			pVm->pConstCycleClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        1 | 1379 | `		}` |
|        3 | 1380 | `		return SXRET_OK;` |
|        - | 1381 | `	}` |
|     1251 | 1382 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1251 | 1383 | `	if( pMemObj == 0 ){` |
|      ! 0 | 1384 | `		return SXERR_MEM;` |
|        - | 1385 | `	}` |
|     1251 | 1386 | `	if( pAttr->pNativeValue ){` |
|        - | 1387 | `		/* A NATIVE class's constant carries a literal instead of byte-code. The` |
|        - | 1388 | `		 * mount loop materializes those, but it stopped visiting untyped constants` |
|        - | 1389 | `		 * when they went lazy (17th session), so this path — the only one an` |
|        - | 1390 | `		 * untyped constant now reaches — has to know about them too. It did not,` |
|        - | 1391 | `		 * which is why every native class constant read NULL: nothing had declared` |
|        - | 1392 | `		 * one until the date family did (DateTimeInterface::ATOM,` |
|        - | 1393 | `		 * DatePeriod::EXCLUDE_START_DATE). A literal cannot throw, so there is no` |
|        - | 1394 | `		 * failure path to mirror below. */` |
|      767 | 1395 | `		PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|      767 | 1396 | `		pAttr->nIdx = pMemObj->nIdx;` |
|      767 | 1397 | `		return SXRET_OK;` |
|        - | 1398 | `	}` |
|      489 | 1399 | `	if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      489 | 1400 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      489 | 1401 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|      489 | 1402 | `		ph7_class_attr *pSaveCycle = pVm->pConstCycleAttr;` |
|        - | 1403 | `		sxu32 nSaveLazyLine;` |
|        - | 1404 | `		sxi32 nSaveLazyDepth;` |
|        - | 1405 | `		sxu32 nSlot;` |
|        - | 1406 | `		sxi32 rcExec;` |
|      489 | 1407 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      489 | 1408 | `		pVm->pConstEvalClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 1409 | `		/* Mark the frame current at eval start: while it stays current, self::/` |
|        - | 1410 | `		 * parent:: in the initializer resolve to pConstEvalClass rather than the` |
|        - | 1411 | `		 * enclosing method's class (VmLocalExec pushes no frame of its own). */` |
|      489 | 1412 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 1413 | `		/* php evaluates this expression HERE, at the access, so that is the line a` |
|        - | 1414 | `		 * throw out of its own bytecode carries. */` |
|      489 | 1415 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|      489 | 1416 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|      489 | 1417 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|      489 | 1418 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|      489 | 1419 | `		pVm->nConstEvalDepth++;` |
|      489 | 1420 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      489 | 1421 | `		pVm->nConstEvalDepth--;` |
|      489 | 1422 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|      489 | 1423 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|      489 | 1424 | `		pVm->pConstEvalClass = pSaveCtx;` |
|      489 | 1425 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|      489 | 1426 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      489 | 1427 | `		nSlot = pMemObj->nIdx; /* the pool can move below; address the slot by index */` |
|      484 | 1428 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT` |
|      460 | 1429 | `		 \|\| pVm->pConstCycleAttr != pSaveCycle ){` |
|        - | 1430 | `			/* The initializer FAILED. Do not memoize the slot: php evaluates a` |
|        - | 1431 | `			 * class constant's expression at each access until one of them` |
|        - | 1432 | ``			 * succeeds, so `class C { const K = UNDEF; }` raises`` |
|        - | 1433 | ``			 * `Undefined constant "UNDEF"` on EVERY read of C::K, not just the`` |
|        - | 1434 | `			 * first. Memoizing left the constant reading NULL, in silence, for` |
|        - | 1435 | `			 * the rest of the run — and made a static default that named it` |
|        - | 1436 | `			 * (whose own evaluation is deferred to first access) find it` |
|        - | 1437 | `			 * materialized and raise nothing at all. Give the reserved slot back` |
|        - | 1438 | `			 * and leave nIdx unset, which is what keys the on-demand path.` |
|        - | 1439 | `			 * A recorded CYCLE is a failure the same way: it does not throw where` |
|        - | 1440 | `			 * it is found — an inner level only records it — but the value is` |
|        - | 1441 | `			 * unusable and the next access must be able to detect it again.` |
|        - | 1442 | `			 * No loop: each access runs the initializer once and raises. */` |
|       35 | 1443 | `			VmRecycleMemObj(&(*pVm),nSlot);` |
|       35 | 1444 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 1445 | `				/* Hand the status to the caller to park/route. */` |
|       32 | 1446 | `				return rcExec;` |
|        - | 1447 | `			}` |
|        3 | 1448 | `			if( pVm->nConstEvalDepth == 0 ){` |
|        - | 1449 | `				/* Outermost level: raise the cycle here, at opcode level, where it` |
|        - | 1450 | `				 * routes to a catch. Deeper in, the record travels outward. */` |
|        3 | 1451 | `				return VmConstCycleThrow(&(*pVm));` |
|        - | 1452 | `			}` |
|      ! 0 | 1453 | `			return SXRET_OK;` |
|        - | 1454 | `		}` |
|      457 | 1455 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - | 1456 | `			/* Typed constant (PHP 8.3) whose value only exists now: check BEFORE` |
|        - | 1457 | `			 * memoizing, so a mismatch leaves the slot unmaterialized and the next` |
|        - | 1458 | `			 * access raises again — php re-runs the whole materialization each` |
|        - | 1459 | `			 * time. A pass may widen int -> float in place, which is the value` |
|        - | 1460 | `			 * memoized below. The check can THROW, and constructing that TypeError` |
|        - | 1461 | `			 * runs php code that used to grow (and realloc) aMemObj — so the slot is` |
|        - | 1462 | `			 * addressed by index from here on, never through pMemObj. Redundant` |
|        - | 1463 | `			 * now the table is segmented; left for the harvest sweep. */` |
|        5 | 1464 | `			sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,1 /* lazy */);` |
|        5 | 1465 | `			if( rcType != SXRET_OK ){` |
|        5 | 1466 | `				VmRecycleMemObj(&(*pVm),nSlot);` |
|        5 | 1467 | `				return rcType;` |
|        - | 1468 | `			}` |
|      ! 0 | 1469 | `		}` |
|        - | 1470 | `		/* Memoize the value. */` |
|      453 | 1471 | `		pAttr->nIdx = nSlot;` |
|      453 | 1472 | `		PH7_VmRefObjInstall(&(*pVm),nSlot,0,0,VM_REF_IDX_KEEP);` |
|      453 | 1473 | `		return SXRET_OK;` |
|        - | 1474 | `	}` |
|      ! 0 | 1475 | `	pAttr->nIdx = pMemObj->nIdx;` |
|      ! 0 | 1476 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      ! 0 | 1477 | `	return SXRET_OK;` |
|      615 | 1478 | `}` |
|        - | 1479 | `/*` |
|        - | 1480 | ` * Public seam for the constant-slot readers outside vm.c (reflection,` |
|        - | 1481 | ` * get_class_vars): class constants evaluate lazily, so a listing-style read` |
|        - | 1482 | ` * must materialize the slot first. Returns SXRET_OK or a throw status.` |
|        - | 1483 | ` */` |
|      824 | 1484 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        3 | 1485 | `{` |
|      827 | 1486 | `	if( pAttr->nIdx != SXU32_HIGH \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      356 | 1487 | `		return SXRET_OK;` |
|        - | 1488 | `	}` |
|      472 | 1489 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       15 | 1490 | `		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|        - | 1491 | `	}` |
|      458 | 1492 | `	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|      401 | 1493 | `}` |
|        - | 1494 | `/*` |
|        - | 1495 | `` * Throw php's catchable Error for an append (`$a[] = v`) whose saturated`` |
|        - | 1496 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap` |
|        - | 1497 | ` * layer; the store opcodes route the returned PH7_EXCEPTION through the` |
|        - | 1498 | ` * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.` |
|        - | 1499 | ` */` |
|        8 | 1500 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)` |
|        2 | 1501 | `{` |
|        - | 1502 | `	SyBlob sMsg;` |
|       10 | 1503 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       10 | 1504 | `	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");` |
|       10 | 1505 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        2 | 1506 | `}` |
|        - | 1507 | `/*` |
|        - | 1508 | `` * php's fatal for `$GLOBALS[] = ...` — appending to the global symbol table`` |
|        - | 1509 | ` * has no name to bind. A compile-time fatal in php (NOT a catchable Error);` |
|        - | 1510 | ` * raised at the store site here with the same message and the same` |
|        - | 1511 | ` * non-catchable outcome. Returns PH7_ABORT (dispatched via` |
|        - | 1512 | ` * PH7_DISPATCH_ENFORCE_RC at the store sites).` |
|        - | 1513 | ` */` |
|        2 | 1514 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm)` |
|        1 | 1515 | `{` |
|        3 | 1516 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot append to $GLOBALS");` |
|        3 | 1517 | `	pVm->iExitStatus = 255;` |
|        3 | 1518 | `	pVm->bHaltRequested = 1;` |
|        3 | 1519 | `	return PH7_ABORT;` |
|        1 | 1520 | `}` |
|        - | 1521 | `/*` |
|        - | 1522 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - | 1523 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - | 1524 | ` * possible.` |
|        - | 1525 | ` */` |
|   100188 | 1526 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven,` |
|        - | 1527 | `	int bViaRef)` |
|        5 | 1528 | `{` |
|   100193 | 1529 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|   100193 | 1530 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        - | 1531 | `	char zType[192];` |
|   150287 | 1532 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|    50094 | 1533 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|        - | 1534 | `	/* php words a write that arrived through a REFERENCE differently: the slot` |
|        - | 1535 | `	 * is the property's, but the assignment names no property, so the sentence` |
|        - | 1536 | `	 * says which property is HOLDING the reference. */` |
|   100193 | 1537 | `	const char *zWhat = bViaRef ? "reference held by property" : "property";` |
|        - | 1538 | `	SyBlob sMsg;` |
|   100193 | 1539 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 1540 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - | 1541 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|   100193 | 1542 | `	if( pOwner ){` |
|   100193 | 1543 | `		SyBlobFormat(&sMsg,"Cannot assign %s to %s %z::$%z of type %s",` |
|    50094 | 1544 | `			zGiven,zWhat,&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|    50099 | 1545 | `	}else{` |
|      ! 0 | 1546 | `		SyBlobFormat(&sMsg,"Cannot assign %s to %s $%z of type %s",` |
|      ! 0 | 1547 | `			zGiven,zWhat,&pAttr->sName,zTypeText);` |
|        - | 1548 | `	}` |
|   100193 | 1549 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        5 | 1550 | `}` |
|        - | 1551 | `/*` |
|        - | 1552 | ` * Of the pseudo-types a property may be declared with, the two whose mask holds` |
|        - | 1553 | `` * an array. `object` does not, and neither does any real class or interface --`` |
|        - | 1554 | `` * php looks at the type MASK, so `ArrayAccess` and `Traversable` are refused`` |
|        - | 1555 | `` * exactly like `int`.`` |
|        - | 1556 | ` */` |
|        2 | 1557 | `static int VmPseudoTypeAcceptsArray(const SyString *pClass)` |
|        1 | 1558 | `{` |
|        3 | 1559 | `	return (pClass->nByte == 8 && SyStrnicmp(pClass->zString,"iterable",8) == 0)` |
|        3 | 1560 | `	    \|\| (pClass->nByte == 5 && SyStrnicmp(pClass->zString,"mixed",5) == 0);` |
|        1 | 1561 | `}` |
|        - | 1562 | `/*` |
|        - | 1563 | ` * TRUE when a property's declared type admits an ARRAY, which is what decides` |
|        - | 1564 | ` * whether a dimension write to it may AUTO-INITIALIZE one. php asks the type's` |
|        - | 1565 | `` * mask (`MAY_BE_ARRAY`), so `array`, `?array`, `iterable`, `mixed` and any union`` |
|        - | 1566 | ``  * with an array alternative say yes and every class type says no -- `ArrayAccess` `` |
|        - | 1567 | `` * and `Traversable` included, which is the part a "does it behave like an array"`` |
|        - | 1568 | ` * reading would get wrong.` |
|        - | 1569 | ` */` |
|       12 | 1570 | `static int VmAttrTypeAcceptsArray(ph7_class_attr *pAttr)` |
|        2 | 1571 | `{` |
|       14 | 1572 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1573 | `		return 1; /* untyped: a dimension write vivifies as it always has */` |
|        - | 1574 | `	}` |
|       14 | 1575 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      ! 0 | 1576 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pAttr->aUnionAlts);` |
|        - | 1577 | `		sxu32 i;` |
|      ! 0 | 1578 | `		for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; ++i ){` |
|      ! 0 | 1579 | `			if( aAlt[i].nType == MEMOBJ_HASHMAP ){` |
|      ! 0 | 1580 | `				return 1;` |
|        - | 1581 | `			}` |
|      ! 0 | 1582 | `			if( aAlt[i].nType == SXU32_HIGH && VmPseudoTypeAcceptsArray(&aAlt[i].sClass) ){` |
|      ! 0 | 1583 | `				return 1;` |
|        - | 1584 | `			}` |
|      ! 0 | 1585 | `		}` |
|      ! 0 | 1586 | `		return 0;` |
|        - | 1587 | `	}` |
|       14 | 1588 | `	if( pAttr->nType == MEMOBJ_HASHMAP ){` |
|        9 | 1589 | `		return 1;` |
|        - | 1590 | `	}` |
|        6 | 1591 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        3 | 1592 | `		return VmPseudoTypeAcceptsArray(&pAttr->sClass);` |
|        - | 1593 | `	}` |
|        3 | 1594 | `	return 0;` |
|        8 | 1595 | `}` |
|        - | 1596 | `/*` |
|        - | 1597 | ` * Throw php's TypeError for a dimension write that would have to auto-initialize` |
|        - | 1598 | ` * an array inside a property whose declared type has no room for one.` |
|        - | 1599 | ` */` |
|        2 | 1600 | `PH7_PRIVATE sxi32 VmThrowAutoInitArrayError(ph7_vm *pVm,VmClassAttr *pVmAttr)` |
|        1 | 1601 | `{` |
|        3 | 1602 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        3 | 1603 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        - | 1604 | `	char zType[256];` |
|        4 | 1605 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|        1 | 1606 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|        - | 1607 | `	SyBlob sMsg;` |
|        3 | 1608 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 1609 | `	if( pOwner ){` |
|        3 | 1610 | `		SyBlobFormat(&sMsg,"Cannot auto-initialize an array inside property %z::$%z of type %s",` |
|        1 | 1611 | `			&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|        2 | 1612 | `	}else{` |
|      ! 0 | 1613 | `		SyBlobFormat(&sMsg,"Cannot auto-initialize an array inside property $%z of type %s",` |
|      ! 0 | 1614 | `			&pAttr->sName,zTypeText);` |
|        - | 1615 | `	}` |
|        3 | 1616 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        1 | 1617 | `}` |
|        - | 1618 | `/*` |
|        - | 1619 | ` * Decide what a DIMENSION write to an uninitialized typed property does, which` |
|        - | 1620 | `` * is php's `zend_handle_fetch_obj_flags`: auto-initialize an empty array when the`` |
|        - | 1621 | ` * declared type admits one, and refuse otherwise. Returns SXRET_OK with the slot` |
|        - | 1622 | ` * left holding a fresh empty array, or the thrown TypeError.` |
|        - | 1623 | ` */` |
|       12 | 1624 | `PH7_PRIVATE sxi32 VmAutoInitArrayProperty(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pSlot)` |
|        2 | 1625 | `{` |
|       14 | 1626 | `	if( pVmAttr->pAttr == 0 \|\| pSlot == 0 ){` |
|      ! 0 | 1627 | `		return SXRET_OK;` |
|        - | 1628 | `	}` |
|       14 | 1629 | `	if( !VmAttrTypeAcceptsArray(pVmAttr->pAttr) ){` |
|        3 | 1630 | `		return VmThrowAutoInitArrayError(pVm,pVmAttr);` |
|        - | 1631 | `	}` |
|       11 | 1632 | `	PH7_MemObjRelease(pSlot);` |
|       11 | 1633 | `	PH7_MemObjToHashmap(pSlot);` |
|       11 | 1634 | `	pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       11 | 1635 | `	return SXRET_OK;` |
|        8 | 1636 | `}` |
|        - | 1637 | `/*` |
|        - | 1638 | ` * TRUE when a property's declared type admits NULL, which is what decides whether a` |
|        - | 1639 | `` * REFERENCE fetch of an uninitialized one may bind at all. `?T` and a `T\|null` union`` |
|        - | 1640 | `` * both carry the nullable flag; `mixed` is the one type that admits null without it.`` |
|        - | 1641 | ` */` |
|       30 | 1642 | `static int VmAttrTypeAcceptsNull(ph7_class_attr *pAttr)` |
|        1 | 1643 | `{` |
|       31 | 1644 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1645 | `		return 1; /* untyped: the slot already holds NULL */` |
|        - | 1646 | `	}` |
|       31 | 1647 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|        7 | 1648 | `		return 1;` |
|        - | 1649 | `	}` |
|       24 | 1650 | `	if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        4 | 1651 | `	 && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|        3 | 1652 | `		return 1;` |
|        - | 1653 | `	}` |
|       23 | 1654 | `	return 0;` |
|       16 | 1655 | `}` |
|        - | 1656 | `/*` |
|        - | 1657 | ` * Decide what a REFERENCE fetch of an uninitialized typed property does, which is` |
|        - | 1658 | `` * php's `zend_handle_fetch_obj_flags` under BP_VAR_W: the slot becomes NULL and the`` |
|        - | 1659 | ` * bind reaches it when the declared type admits null, and the fetch is refused` |
|        - | 1660 | `` * otherwise -- php's `Cannot access uninitialized non-nullable property C::$p by`` |
|        - | 1661 | `` * reference`. It is NOT the auto-initialize-array rule: that one belongs to a`` |
|        - | 1662 | `` * DIMENSION write, and running it here made `?int $t` an array and `int $t` the`` |
|        - | 1663 | ` * wrong TypeError.` |
|        - | 1664 | ` */` |
|       30 | 1665 | `PH7_PRIVATE sxi32 VmRefUninitTypedProperty(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr,ph7_value *pSlot)` |
|        1 | 1666 | `{` |
|        - | 1667 | `	ph7_class_attr *pAttr;` |
|        - | 1668 | `	ph7_class *pOwner;` |
|        - | 1669 | `	SyBlob sMsg;` |
|       31 | 1670 | `	if( pVmAttr == 0 \|\| pVmAttr->pAttr == 0 ){` |
|      ! 0 | 1671 | `		return SXRET_OK;` |
|        - | 1672 | `	}` |
|       31 | 1673 | `	pAttr = pVmAttr->pAttr;` |
|       31 | 1674 | `	if( VmAttrTypeAcceptsNull(pAttr) ){` |
|        9 | 1675 | `		if( pSlot ){` |
|        9 | 1676 | `			PH7_MemObjRelease(pSlot);` |
|        9 | 1677 | `			MemObjSetType(pSlot,MEMOBJ_NULL);` |
|        4 | 1678 | `		}` |
|        9 | 1679 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        9 | 1680 | `		return SXRET_OK;` |
|        - | 1681 | `	}` |
|       23 | 1682 | `	pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|       23 | 1683 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       23 | 1684 | `	SyBlobFormat(&sMsg,"Cannot access uninitialized non-nullable property %z::$%z by reference",` |
|       11 | 1685 | `		pOwner ? &pOwner->sDisp : &pClass->sDisp,&pAttr->sName);` |
|       23 | 1686 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       16 | 1687 | `}` |
|        - | 1688 | `/*` |
|        - | 1689 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - | 1690 | ` */` |
|   100028 | 1691 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 1692 | `{` |
|   100033 | 1693 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|   100033 | 1694 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        - | 1695 | `	SyBlob sMsg;` |
|   100033 | 1696 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   100033 | 1697 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|    50014 | 1698 | `		zKind,&pOwner->sDisp,&pAttr->sName);` |
|   100033 | 1699 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 1700 | `}` |
|        - | 1701 | `/*` |
|        - | 1702 | ` * Throw the PHP-compatible Error raised on an illegal write to a readonly` |
|        - | 1703 | ` * property (PHP 8.1). bModify TRUE → a write to an already-initialized property` |
|        - | 1704 | ` * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from` |
|        - | 1705 | ` * a scope that cannot satisfy the readonly set-scope ("Cannot modify` |
|        - | 1706 | ` * protected(set) readonly property C::$x from {global scope\|scope X}").` |
|        - | 1707 | ` */` |
|        - | 1708 | `/*` |
|        - | 1709 | ` * Throw the PHP 8.4 Error raised on a write to an asymmetric-visibility` |
|        - | 1710 | ` * property from a scope its set-visibility excludes:` |
|        - | 1711 | ` * "Cannot modify private(set) property C::$x from {global scope\|scope X}".` |
|        - | 1712 | ` */` |
|       18 | 1713 | `static sxi32 VmThrowSetVisibilityErrorEx(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,` |
|        - | 1714 | `	int bIndirect)` |
|        2 | 1715 | `{` |
|       20 | 1716 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|       20 | 1717 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|       20 | 1718 | `	const char *zVis = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? "private(set)" : "protected(set)";` |
|        - | 1719 | `	/* php words a write that only reaches the property THROUGH something it holds` |
|        - | 1720 | ``	 * -- `$o->arr['k'] = v`, a by-reference bind -- as an INDIRECT modification,`` |
|        - | 1721 | `	 * the same distinction its readonly sentence makes. */` |
|       20 | 1722 | `	const char *zVerb = bIndirect ? "indirectly modify" : "modify";` |
|        - | 1723 | `	SyBlob sMsg;` |
|       20 | 1724 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       20 | 1725 | `	if( pActive ){` |
|        3 | 1726 | `		SyBlobFormat(&sMsg,"Cannot %s %s property %z::$%z from scope %z",` |
|        1 | 1727 | `			zVerb,zVis,&pOwner->sDisp,&pAttr->sName,&pActive->sDisp);` |
|        2 | 1728 | `	}else{` |
|       18 | 1729 | `		SyBlobFormat(&sMsg,"Cannot %s %s property %z::$%z from global scope",` |
|        8 | 1730 | `			zVerb,zVis,&pOwner->sDisp,&pAttr->sName);` |
|        - | 1731 | `	}` |
|       20 | 1732 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        2 | 1733 | `}` |
|       16 | 1734 | `static sxi32 VmThrowSetVisibilityError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1735 | `{` |
|       17 | 1736 | `	return VmThrowSetVisibilityErrorEx(pVm,pClass,pAttr,0);` |
|        1 | 1737 | `}` |
|        - | 1738 | `/*` |
|        - | 1739 | ` * Check the PHP 8.4 asymmetric set-visibility of a property write against the` |
|        - | 1740 | ` * active class scope. private(set): only the DECLARING class scope may write` |
|        - | 1741 | ` * (subclasses excluded); protected(set): the declaring class or a subclass.` |
|        - | 1742 | ` * Returns SXRET_OK when allowed, else the throw status.` |
|        - | 1743 | ` */` |
|       38 | 1744 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr)` |
|        1 | 1745 | `{` |
|       39 | 1746 | `	ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pOwner);` |
|       39 | 1747 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|        - | 1748 | `	int bOk;` |
|       39 | 1749 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|       33 | 1750 | `		bOk = (pActive != 0 && pActive == pDecl);` |
|       17 | 1751 | `	}else{` |
|        7 | 1752 | `		bOk = (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|        - | 1753 | `	}` |
|       39 | 1754 | `	if( !bOk ){` |
|       17 | 1755 | `		return VmThrowSetVisibilityError(pVm,pOwner,pAttr);` |
|        - | 1756 | `	}` |
|       23 | 1757 | `	return SXRET_OK;` |
|       20 | 1758 | `}` |
|        - | 1759 | `/*` |
|        - | 1760 | ` * php's write refusal for a native property whose handler takes NO write` |
|        - | 1761 | ` * (PH7_CLASS_ATTR_NATIVE_NOWRITE). The sentence is the readonly one -- php's` |
|        - | 1762 | ` * date_period_write_property says exactly that -- but the property carries no` |
|        - | 1763 | ` * readonly FLAG, so this is spelled apart from VmThrowReadonlyError rather than` |
|        - | 1764 | ` * reached through it: Reflection reports isReadOnly() false for DatePeriod's` |
|        - | 1765 | ` * seven in both engines, and the readonly rules (write-once, set-scope, the` |
|        - | 1766 | ` * __clone re-initialization window) do not apply to a handler that never` |
|        - | 1767 | ` * accepts one.` |
|        - | 1768 | ` */` |
|       40 | 1769 | `PH7_PRIVATE sxi32 VmThrowNativeNoWrite(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1770 | `{` |
|       41 | 1771 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 1772 | `	SyBlob sMsg;` |
|       41 | 1773 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       41 | 1774 | `	SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sDisp,&pAttr->sName);` |
|       41 | 1775 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1776 | `}` |
|        - | 1777 | `/*` |
|        - | 1778 | `` * And its unset half, which php words differently: `Cannot unset C::$p`, with`` |
|        - | 1779 | ` * neither "readonly" nor "property" in it.` |
|        - | 1780 | ` */` |
|       24 | 1781 | `PH7_PRIVATE sxi32 VmThrowNativeNoUnset(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1782 | `{` |
|        - | 1783 | `	SyBlob sMsg;` |
|       25 | 1784 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 1785 | `	/* The OBJECT's class, not the declaring one -- php's two refusals disagree` |
|        - | 1786 | `	 * about which to print, and a subclass of DatePeriod shows it: the write says` |
|        - | 1787 | ``	 * `DatePeriod::$interval` and the unset says `SubDp::$interval`. */`` |
|       25 | 1788 | `	SyBlobFormat(&sMsg,"Cannot unset %z::$%z",&pClass->sDisp,&pAttr->sName);` |
|       25 | 1789 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1790 | `}` |
|        - | 1791 | `/*` |
|        - | 1792 | ` * And php's THIRD refusal for a property its handler will not have written:` |
|        - | 1793 | `` * `Property p is read only`, which names neither the class nor the `$`.`` |
|        - | 1794 | `` * PDOStatement's `queryString` is php's case, and the shapes it applies to are`` |
|        - | 1795 | ` * the plain store and the unset alone -- see PH7_CLASS_ATTR_NATIVE_RDONLY.` |
|        - | 1796 | ` */` |
|       10 | 1797 | `PH7_PRIVATE sxi32 VmThrowNativeReadOnly(ph7_vm *pVm,ph7_class_attr *pAttr)` |
|        1 | 1798 | `{` |
|        - | 1799 | `	SyBlob sMsg;` |
|       11 | 1800 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       11 | 1801 | `	SyBlobFormat(&sMsg,"Property %z is read only",&pAttr->sName);` |
|       11 | 1802 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1803 | `}` |
|        - | 1804 | `/*` |
|        - | 1805 | `` * php's answer to `unset($o->p)` where p is READONLY. Two of the three cases`` |
|        - | 1806 | ` * refuse, and the sentences are not the write ones:` |
|        - | 1807 | ` *` |
|        - | 1808 | ` *   * an INITIALIZED one refuses from every scope, its own included --` |
|        - | 1809 | `` *     `Cannot unset readonly property C::$p`. Destroying it would re-arm the`` |
|        - | 1810 | ` *     write-once latch, which is exactly what readonly exists to prevent;` |
|        - | 1811 | ` *` |
|        - | 1812 | ` *   * an UNINITIALIZED one is a WRITE-shaped act, so it takes the set-visibility` |
|        - | 1813 | ` *     rules: allowed from the declaring class or a subclass (php lets a lazy` |
|        - | 1814 | ` *     proxy re-arm one that way), and otherwise the asymmetric-visibility` |
|        - | 1815 | `` *     refusal. php words that one two ways -- an EXPLICIT `private(set)` gets the`` |
|        - | 1816 | ` *     ordinary asymmetric sentence with no "readonly" in it, and everything else` |
|        - | 1817 | `` *     gets readonly's own implicit `protected(set) readonly`.`` |
|        - | 1818 | ` *` |
|        - | 1819 | ` * Answers SXRET_OK when the unset may proceed, else the throw status.` |
|        - | 1820 | ` */` |
|       24 | 1821 | `PH7_PRIVATE sxi32 VmCheckReadonlyUnset(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr)` |
|        1 | 1822 | `{` |
|       25 | 1823 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - | 1824 | `	ph7_class *pOwner;` |
|        - | 1825 | `	ph7_class *pActive;` |
|        - | 1826 | `	SyBlob sMsg;` |
|        - | 1827 | `	int bInit,bScope;` |
|       25 | 1828 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) == 0 ){` |
|      ! 0 | 1829 | `		return SXRET_OK;` |
|        - | 1830 | `	}` |
|       25 | 1831 | `	pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|       25 | 1832 | `	pActive = VmCurrentSelf(pVm);` |
|       25 | 1833 | `	bInit = (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0;` |
|       25 | 1834 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        7 | 1835 | `		bScope = (pActive != 0 && pActive == pOwner);` |
|        4 | 1836 | `	}else{` |
|       19 | 1837 | `		bScope = (pActive != 0 && pOwner != 0 && PH7_VmInstanceOf(pActive,pOwner));` |
|        - | 1838 | `	}` |
|       25 | 1839 | `	if( !bInit && bScope ){` |
|        9 | 1840 | `		return SXRET_OK;` |
|        - | 1841 | `	}` |
|       17 | 1842 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 | 1843 | `	if( bInit ){` |
|        9 | 1844 | `		SyBlobFormat(&sMsg,"Cannot unset readonly property %z::$%z",&pOwner->sDisp,&pAttr->sName);` |
|        5 | 1845 | `	}else{` |
|       13 | 1846 | `		const char *zWhat = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET)` |
|        4 | 1847 | `			? "private(set)" : "protected(set) readonly";` |
|        9 | 1848 | `		if( pActive ){` |
|        3 | 1849 | `			SyBlobFormat(&sMsg,"Cannot unset %s property %z::$%z from scope %z",` |
|        1 | 1850 | `				zWhat,&pOwner->sDisp,&pAttr->sName,&pActive->sDisp);` |
|        2 | 1851 | `		}else{` |
|        7 | 1852 | `			SyBlobFormat(&sMsg,"Cannot unset %s property %z::$%z from global scope",` |
|        3 | 1853 | `				zWhat,&pOwner->sDisp,&pAttr->sName);` |
|        - | 1854 | `		}` |
|        - | 1855 | `	}` |
|       17 | 1856 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       13 | 1857 | `}` |
|       52 | 1858 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|        5 | 1859 | `{` |
|       57 | 1860 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 1861 | `	SyBlob sMsg;` |
|       57 | 1862 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       57 | 1863 | `	if( bModify ){` |
|       53 | 1864 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sDisp,&pAttr->sName);` |
|       29 | 1865 | `	}else{` |
|        6 | 1866 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        6 | 1867 | `		if( pActive ){` |
|      ! 0 | 1868 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|      ! 0 | 1869 | `				&pOwner->sDisp,&pAttr->sName,&pActive->sDisp);` |
|      ! 0 | 1870 | `		}else{` |
|        6 | 1871 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|        2 | 1872 | `				&pOwner->sDisp,&pAttr->sName);` |
|        - | 1873 | `		}` |
|        - | 1874 | `	}` |
|       57 | 1875 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 1876 | `}` |
|        - | 1877 | `/*` |
|        - | 1878 | ` * INDIRECT modification: this SLOT is about to be reached as something other than` |
|        - | 1879 | `` * a plain store -- aliased by `=&`, handed to a by-reference parameter, walked by`` |
|        - | 1880 | ` * a by-reference foreach, or used as the BASE of a subscript write. php screens` |
|        - | 1881 | ` * every one of them where it screens a store, because the alias outlives the` |
|        - | 1882 | ` * statement and the next write through it would reach the property with no` |
|        - | 1883 | ` * handler and no readonly latch in the way.` |
|        - | 1884 | ` *` |
|        - | 1885 | `` * Two sentences. A php-readonly property gets its own: `Cannot indirectly modify`` |
|        - | 1886 | `` * readonly property C::$p`, raised whatever the scope and whether or not the`` |
|        - | 1887 | ` * property has been initialized -- the reference is refused before the` |
|        - | 1888 | ` * uninitialized read is. A NATIVE class whose handler refuses every write` |
|        - | 1889 | ` * (PH7_CLASS_ATTR_NATIVE_NOWRITE) gets that handler's own sentence, the one a` |
|        - | 1890 | ` * plain store to it gets.` |
|        - | 1891 | ` *` |
|        - | 1892 | ` * Answers SXRET_OK to proceed, or the throw status. Asked by the sites that reach` |
|        - | 1893 | ` * a property through its memobj index rather than through its declaration.` |
|        - | 1894 | ` */` |
|    46322 | 1895 | `PH7_PRIVATE sxi32 PH7_VmCheckIndirectModify(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 1896 | `{` |
|        - | 1897 | `	SyHashEntry *pSlot;` |
|        - | 1898 | `	VmClassAttr *pVmAttr;` |
|        - | 1899 | `	ph7_class_attr *pAttr;` |
|    46327 | 1900 | `	if( nIdx == SXU32_HIGH \|\| !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|    46203 | 1901 | `		return SXRET_OK;` |
|        - | 1902 | `	}` |
|    40045 | 1903 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    40045 | 1904 | `	if( pSlot == 0 ){` |
|      ! 0 | 1905 | `		return SXRET_OK;` |
|        - | 1906 | `	}` |
|      127 | 1907 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      127 | 1908 | `	pAttr = pVmAttr->pAttr;` |
|      127 | 1909 | `	if( pAttr == 0 ){` |
|      ! 0 | 1910 | `		return SXRET_OK;` |
|        - | 1911 | `	}` |
|      127 | 1912 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|       13 | 1913 | `		return VmThrowNativeNoWrite(pVm,PH7_VmAttrOwner(pVmAttr),pAttr);` |
|        - | 1914 | `	}` |
|      115 | 1915 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       33 | 1916 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        - | 1917 | `		SyBlob sMsg;` |
|       33 | 1918 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       33 | 1919 | `		SyBlobFormat(&sMsg,"Cannot indirectly modify readonly property %z::$%z",` |
|       16 | 1920 | `			&pOwner->sDisp,&pAttr->sName);` |
|       33 | 1921 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 1922 | `	}` |
|        - | 1923 | `	/* An asymmetric set-visibility (PHP 8.4) gates the indirect write exactly as` |
|        - | 1924 | ``	 * readonly does, and from the same scopes: `$o->arr['k'] = v` on a`` |
|        - | 1925 | ``	 * `public private(set) array $arr` is refused outside the declaring class.`` |
|        - | 1926 | `	 * Only readonly was screened here, so that write landed in SILENCE. */` |
|       83 | 1927 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        3 | 1928 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        3 | 1929 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        5 | 1930 | `		int bOk = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET)` |
|        2 | 1931 | `			? (pActive != 0 && pActive == pDecl)` |
|        2 | 1932 | `			: (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|        3 | 1933 | `		if( !bOk ){` |
|        3 | 1934 | `			return VmThrowSetVisibilityErrorEx(pVm,PH7_VmAttrOwner(pVmAttr),pAttr,1);` |
|        - | 1935 | `		}` |
|      ! 0 | 1936 | `	}` |
|       81 | 1937 | `	return SXRET_OK;` |
|    23158 | 1938 | `}` |
|        - | 1939 | `/*` |
|        - | 1940 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|        - | 1941 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|        - | 1942 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|        - | 1943 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|        - | 1944 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|        - | 1945 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|        - | 1946 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|        - | 1947 | ` */` |
|   955226 | 1948 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 1949 | `{` |
|        - | 1950 | `	SyHashEntry *pSlot;` |
|        - | 1951 | `	VmClassAttr *pVmAttr;` |
|   955231 | 1952 | `	if( nIdx == SXU32_HIGH \|\| !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|   952901 | 1953 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|        - | 1954 | `	}` |
|   301506 | 1955 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|   301506 | 1956 | `	if( pSlot == 0 ){` |
|      ! 0 | 1957 | `		return SXRET_OK; /* Not a typed slot */` |
|        - | 1958 | `	}` |
|     2334 | 1959 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     2334 | 1960 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE) ){` |
|        5 | 1961 | `		return VmThrowNativeNoWrite(pVm,PH7_VmAttrOwner(pVmAttr),pVmAttr->pAttr);` |
|        - | 1962 | `	}` |
|     2330 | 1963 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|       12 | 1964 | `		return VmThrowReadonlyError(pVm,PH7_VmAttrOwner(pVmAttr),pVmAttr->pAttr,1);` |
|        - | 1965 | `	}` |
|     2316 | 1966 | `	if( pVmAttr->pAttr` |
|     2320 | 1967 | `	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET)) ){` |
|        - | 1968 | ``		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */`` |
|        9 | 1969 | `		return VmCheckSetVisibility(pVm,PH7_VmAttrOwner(pVmAttr),pVmAttr->pAttr);` |
|        - | 1970 | `	}` |
|     2312 | 1971 | `	return SXRET_OK;` |
|   478656 | 1972 | `}` |
|        - | 1973 | `/*` |
|        - | 1974 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - | 1975 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - | 1976 | ` * For class types, instanceof is verified.` |
|        - | 1977 | ` *` |
|        - | 1978 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - | 1979 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - | 1980 | ` */` |
|        - | 1981 |  |
|        - | 1982 | `/*` |
|        - | 1983 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - | 1984 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - | 1985 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - | 1986 | ` *   0 if it's not strictly numeric.` |
|        - | 1987 | ` */` |
|       38 | 1988 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        3 | 1989 | `{` |
|        - | 1990 | `	const char *z, *zEnd, *zTail;` |
|        - | 1991 | `	sxu32 n;` |
|       41 | 1992 | `	sxu8 bReal = 0;` |
|        - | 1993 | `	sxi32 rc;` |
|       41 | 1994 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       24 | 1995 | `		return 0;` |
|        - | 1996 | `	}` |
|       18 | 1997 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 | 1998 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 | 1999 | `	zEnd = z + n;` |
|       18 | 2000 | `	if( n == 0 ) return 0;` |
|       18 | 2001 | `	zTail = 0;` |
|       18 | 2002 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 | 2003 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 | 2004 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 | 2005 | `	if( zTail != zEnd ) return 0;` |
|       15 | 2006 | `	return bReal ? 2 : 1;` |
|       22 | 2007 | `}` |
|        - | 2008 |  |
|        - | 2009 | `/*` |
|        - | 2010 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|        - | 2011 | `` * PH7 parses `true`/`false`/`iterable`/`callable`/`mixed` as class-name atoms`` |
|        - | 2012 | ` * (they are not scalar keywords), so without this every enforcement site —` |
|        - | 2013 | ` * return, parameter, property, union alternative — would have to string-match` |
|        - | 2014 | ` * the name itself.` |
|        - | 2015 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|        - | 2016 | ` * to extend when another literal/pseudo type is added.` |
|        - | 2017 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|        - | 2018 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|        - | 2019 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|        - | 2020 | ` */` |
|     9782 | 2021 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|        5 | 2022 | `{` |
|     9787 | 2023 | `	const char *z = pClass->zString;` |
|     9787 | 2024 | `	sxu32 n = pClass->nByte;` |
|     9787 | 2025 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|      349 | 2026 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|        - | 2027 | `	}` |
|     9443 | 2028 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       28 | 2029 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|        - | 2030 | `	}` |
|     9417 | 2031 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|     3063 | 2032 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|        - | 2033 | `	}` |
|     6359 | 2034 | `	if( n == 8 && SyStrnicmp(z,"callable",8) == 0 ){` |
|        - | 2035 | ``		/* php's `callable` type is is_callable() itself — a function-name string,`` |
|        - | 2036 | `		 * a "C::m" string, a [target,method] pair, a Closure, or an __invoke` |
|        - | 2037 | `		 * object; scope-sensitive, so a private method is callable only from` |
|        - | 2038 | `		 * inside. Same predicate the builtin answers with, so the type and` |
|        - | 2039 | `		 * is_callable() can never disagree. (Parameters and return values only:` |
|        - | 2040 | ``		 * the compiler rejects `callable` on a property or class constant, as`` |
|        - | 2041 | `		 * php does.) */` |
|     4705 | 2042 | `		return PH7_VmIsCallable(pVm,pValue,TRUE) ? 1 : 0;` |
|        - | 2043 | `	}` |
|     1659 | 2044 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        - | 2045 | `		/* iterable === array \| Traversable */` |
|       89 | 2046 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       17 | 2047 | `			return 1;` |
|        - | 2048 | `		}` |
|       74 | 2049 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|       46 | 2050 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       46 | 2051 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|       29 | 2052 | `				return 1;` |
|        - | 2053 | `			}` |
|        7 | 2054 | `		}` |
|       46 | 2055 | `		return 0;` |
|        - | 2056 | `	}` |
|     1575 | 2057 | `	return -1;` |
|     4787 | 2058 | `}` |
|        - | 2059 | `/*` |
|        - | 2060 | `` * The scope a `self`/`parent` written in a TYPE HINT resolves against: the class`` |
|        - | 2061 | ` * the hint was DECLARED in, never the class the value happens to be reached` |
|        - | 2062 | ` * through. php binds the keyword where the hint is written, so` |
|        - | 2063 | `` * `class P { public function s(): self {…} }` still expects a P when called on a`` |
|        - | 2064 | `` * `Q extends P` — resolving against the runtime (late-static-binding) class made`` |
|        - | 2065 | `` * such a return, and a `self`-typed property written from an inherited method,`` |
|        - | 2066 | ` * throw a TypeError over perfectly valid code.` |
|        - | 2067 | ` *` |
|        - | 2068 | ` * pDecl is the declaring class (0 when the site cannot name one); pUsing is the` |
|        - | 2069 | ` * composing/runtime class to stand in for a TRAIT — php flattens a trait into` |
|        - | 2070 | ` * the using class, and the trait itself sits in no instanceof hierarchy — or 0` |
|        - | 2071 | ` * to fall back to the active self. This is the same rule OP_CALL already applies` |
|        - | 2072 | ` * to PARAMETER hints when it computes pSelfHint (vm_exec.c), and the one` |
|        - | 2073 | `` * VmResolveTypeClass applies to `parent`.`` |
|        - | 2074 | ` */` |
|   121864 | 2075 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing)` |
|        5 | 2076 | `{` |
|   121869 | 2077 | `	if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|   106949 | 2078 | `		return pDecl;` |
|        - | 2079 | `	}` |
|    14925 | 2080 | `	return pUsing ? pUsing : VmCurrentSelf(pVm);` |
|    60871 | 2081 | `}` |
|        - | 2082 | `/*` |
|        - | 2083 | ` * The scope a member's declared type is PRINTED against, which is not the scope` |
|        - | 2084 | ` * it is CHECKED against.` |
|        - | 2085 | ` *` |
|        - | 2086 | `` * php resolves `self`/`parent` in a member's stored type TEXT at compile time,`` |
|        - | 2087 | ` * and a TRAIT has no class to resolve them to -- so what it stores for a trait` |
|        - | 2088 | `` * member keeps the keyword, and every display of that text says `self` however`` |
|        - | 2089 | `` * many classes composed the trait: `uninitialized(self)` in var_dump,`` |
|        - | 2090 | `` * `of type ?parent` in the assign TypeError, `self` from`` |
|        - | 2091 | ` * ReflectionProperty::getType(). The CHECK still resolves against the composing` |
|        - | 2092 | ` * class, which is what VmHintScopeClass answers; this is its display twin, and` |
|        - | 2093 | ` * telling the two apart is what the recorded reference-property row was waiting for.` |
|        - | 2094 | ` */` |
|   101032 | 2095 | `PH7_PRIVATE ph7_class *VmHintScopeDeclared(ph7_class *pDecl)` |
|        5 | 2096 | `{` |
|   101037 | 2097 | `	return ( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ) ? pDecl : 0;` |
|        5 | 2098 | `}` |
|        - | 2099 | `/*` |
|        - | 2100 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - | 2101 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - | 2102 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - | 2103 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - | 2104 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - | 2105 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - | 2106 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - | 2107 | ` * throw.` |
|        - | 2108 | ` *` |
|        - | 2109 | `` * The class match for object values resolves `self`/`parent` alternatives`` |
|        - | 2110 | ` * against *pSelf* — the DECLARING scope of the hint the union came from, which` |
|        - | 2111 | ` * every caller computes through VmHintScopeClass (the self-stack top is the` |
|        - | 2112 | ` * runtime class, which is the wrong answer for an inherited hint).` |
|        - | 2113 | ` */` |
|        - | 2114 | `/*` |
|        - | 2115 | ` * Resolve a class/interface name from a type declaration to its ph7_class*,` |
|        - | 2116 | `` * handling the `self`/`parent` aliases against the supplied scope class pSelf —`` |
|        - | 2117 | ` * the class the hint was DECLARED in, which every enforcement site computes` |
|        - | 2118 | ` * through VmHintScopeClass above (OP_CALL's pSelfHint for parameters) — and` |
|        - | 2119 | `` * `static`, which is php's late-static-binding CALLED class and so ignores pSelf.`` |
|        - | 2120 | ` * Used by every type-enforcement site so the resolution rule — including the` |
|        - | 2121 | ` * iLoadable flag — lives in one place.` |
|        - | 2122 | ` *` |
|        - | 2123 | ` * The three keyword names are matched case-INSENSITIVELY, like every other type` |
|        - | 2124 | ` * keyword (VmCheckPseudoType's mixed/true/false/iterable) and like php: a hint` |
|        - | 2125 | `` * spelled `SELF` used to miss the exact-match arm, fall through to the class`` |
|        - | 2126 | ` * table, find nothing, and leave the caller with 0 — which every caller reads as` |
|        - | 2127 | ` * "unresolvable, accept anything", so the hint enforced NOTHING.` |
|        - | 2128 | ` *` |
|        - | 2129 | ` * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-` |
|        - | 2130 | ` * compatibility target, where the type may legitimately be an interface or` |
|        - | 2131 | ` * abstract class (TRUE would filter those out → the check is skipped → any` |
|        - | 2132 | ` * object wrongly accepted). Centralizing FALSE here keeps a future caller from` |
|        - | 2133 | `` * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly`` |
|        - | 2134 | ` * with TRUE; it does not go through this helper.)` |
|        - | 2135 | ` */` |
|     1680 | 2136 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)` |
|        5 | 2137 | `{` |
|     1685 | 2138 | `	if( pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0 ){` |
|      157 | 2139 | `		return pSelf;` |
|        - | 2140 | `	}` |
|     1533 | 2141 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0 ){` |
|        - | 2142 | ``		/* php 8.0 `static` return type: the LATE-STATIC-BINDING class, i.e. the`` |
|        - | 2143 | ``		 * class the call was made through — so `P::s(): static` returning a P is a`` |
|        - | 2144 | ``		 * TypeError once s() is reached on a `Q extends P`. It is deliberately NOT`` |
|        - | 2145 | `		 * pSelf (that is where the hint was written); php allows the keyword in a` |
|        - | 2146 | `		 * return position only, but resolving it here keeps every site consistent. */` |
|       46 | 2147 | `		return PH7_VmPeekTopClass(pVm);` |
|        - | 2148 | `	}` |
|     1491 | 2149 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0 ){` |
|        - | 2150 | `		/* A trait method's declaring class is the trait (shared by pointer); parent::` |
|        - | 2151 | `		 * resolves against the class that USED it, matching the self:: trait rule. */` |
|       35 | 2152 | `		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|      ! 0 | 2153 | `			pSelf = PH7_VmTraitUsingClass(pVm,pSelf,PH7_VmPeekTopClass(pVm));` |
|      ! 0 | 2154 | `		}` |
|       35 | 2155 | `		return pSelf ? pSelf->pBase : 0;` |
|        - | 2156 | `	}` |
|     1459 | 2157 | `	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);` |
|      845 | 2158 | `}` |
|        - | 2159 | `/*` |
|        - | 2160 | ` * Is *pCN* one of the three SCOPE KEYWORDS rather than a class name? Those are` |
|        - | 2161 | `` * the only hints VmResolveTypeClass may legitimately answer 0 for — `self` with`` |
|        - | 2162 | `` * no active scope, `parent` with no base class — which php rejects at COMPILE`` |
|        - | 2163 | ` * time, so there is no runtime behaviour to be faithful to and the caller keeps` |
|        - | 2164 | ` * accepting. Any OTHER name that resolves to nothing is a class that does not` |
|        - | 2165 | ` * exist, and that is a mismatch (see VmClassHintMatches).` |
|        - | 2166 | `` * (vm_builtin_call.c carries the same list for CALLABLE strings — `'self::m'`,`` |
|        - | 2167 | `` * `['parent','m']` — where the rule is about dispatch, not types.)`` |
|        - | 2168 | ` */` |
|   101998 | 2169 | `static int VmHintIsScopeKeyword(const SyString *pCN)` |
|        5 | 2170 | `{` |
|   102028 | 2171 | `	return (pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0)` |
|   101974 | 2172 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0)` |
|   152997 | 2173 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0);` |
|        5 | 2174 | `}` |
|        - | 2175 | `/*` |
|        - | 2176 | ` * Does *pVal* satisfy the single (non-union) CLASS hint *pName*, written in the` |
|        - | 2177 | ` * scope *pScope* (see VmHintScopeClass)? THE one implementation of the rule,` |
|        - | 2178 | ` * shared by the argument, variadic-element, return, property, class-constant and` |
|        - | 2179 | ` * typed-default checks — each of which then formats its own message. The` |
|        - | 2180 | ` * resolved class is handed back through *ppResolved for the message builder` |
|        - | 2181 | ` * (VmClassHintTypeName prints the resolved name, or the name as written when` |
|        - | 2182 | ` * nothing resolved).` |
|        - | 2183 | ` *` |
|        - | 2184 | ` * A name that resolves to NOTHING used to make every one of those sites skip its` |
|        - | 2185 | ` * check, so a hint naming a class that does not exist enforced nothing at all:` |
|        - | 2186 | `` * `function f(Missing $c){} f(new Oth);` ran the body where php throws. Nothing`` |
|        - | 2187 | ` * can be an instance of a class that does not exist, so that is a mismatch.` |
|        - | 2188 | ` * A scope KEYWORD is the exception, and only half of one: with no scope to` |
|        - | 2189 | ` * resolve against there is no class to compare to — a position php rejects at` |
|        - | 2190 | ` * compile time, so any object passes — but a class hint still demands an object.` |
|        - | 2191 | ` *` |
|        - | 2192 | ` * Null never reaches here: a nullable hint accepts it at every call site before` |
|        - | 2193 | ` * the class branch. The resolution AUTOLOADS (PH7_VmExtractClass), so a` |
|        - | 2194 | ` * not-yet-loaded class is loaded and genuinely checked; only a name no` |
|        - | 2195 | ` * autoloader can produce fails.` |
|        - | 2196 | ` */` |
|     1444 | 2197 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|        - | 2198 | `	ph7_value *pVal,ph7_class **ppResolved)` |
|        5 | 2199 | `{` |
|     1449 | 2200 | `	ph7_class *pExpected = VmResolveTypeClass(pVm,pName,pScope);` |
|     1449 | 2201 | `	*ppResolved = pExpected;` |
|     1449 | 2202 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       51 | 2203 | `		return 0;` |
|        - | 2204 | `	}` |
|     1401 | 2205 | `	if( pExpected == 0 ){` |
|       13 | 2206 | `		return VmHintIsScopeKeyword(pName);` |
|        - | 2207 | `	}` |
|     1389 | 2208 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pExpected);` |
|      727 | 2209 | `}` |
|        - | 2210 | `/* A character that can be part of a type NAME, as opposed to the punctuation` |
|        - | 2211 | `` * that separates the parts of a declared type (`?A`, `A\|B`, `(A&B)\|null`). */`` |
|   412470 | 2212 | `static int VmHintNameChar(int c)` |
|        5 | 2213 | `{` |
|   823907 | 2214 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|   411452 | 2215 | `		\|\| c == ' ' \|\| c == '\t');` |
|        5 | 2216 | `}` |
|        - | 2217 | `/*` |
|        - | 2218 | ` * Render a DECLARED type text for a message with the scope keywords RESOLVED,` |
|        - | 2219 | `` * the way php prints it: `of type self` reads `of type P`, `self\|false` reads`` |
|        - | 2220 | `` * `P\|false`, `?static` reads `?Q`. The declared text is what the compiler`` |
|        - | 2221 | `` * canonicalised (php's own member order, `?T` shorthand and all), so rewriting`` |
|        - | 2222 | ` * it name-by-name keeps that shape — only the three names that cannot be` |
|        - | 2223 | ` * resolved until the call site are substituted.` |
|        - | 2224 | ` *` |
|        - | 2225 | ` * A single CLASS hint has its own builder, VmClassHintTypeName, which resolves` |
|        - | 2226 | ` * from the ph7_class* the check already produced; this one is for the texts that` |
|        - | 2227 | ` * are only ever available as source: unions/intersections, and the property /` |
|        - | 2228 | ` * class-constant messages, which print the declared type whatever its shape.` |
|        - | 2229 | ` * An unresolvable keyword is left as written (there is no class to name).` |
|        - | 2230 | ` *` |
|        - | 2231 | `` * `iterable` is the one name php SPELLS DIFFERENTLY in a message than in the`` |
|        - | 2232 | `` * canonical text: Reflection prints `iterable`/`?iterable` (which is what the`` |
|        - | 2233 | ` * compiler stores, and what a COMPOUND type already has expanded in place —` |
|        - | 2234 | `` * `iterable\|int` is stored `Traversable\|array\|int`), while every diagnostic`` |
|        - | 2235 | ` * names the two types it stands for. Only the standalone spellings can still` |
|        - | 2236 | ` * reach here, so the substitution is over the whole text rather than per token —` |
|        - | 2237 | `` * `?iterable` is `Traversable\|array\|null`, not `?Traversable\|array`.`` |
|        - | 2238 | ` */` |
|   100398 | 2239 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|        - | 2240 | `	char *zBuf,sxu32 nBuf)` |
|        5 | 2241 | `{` |
|   150602 | 2242 | `	return VmHintTextResolvedEx(&(*pVm),pDeclared,pScope,` |
|    50199 | 2243 | `		PH7_HINT_TEXT_ITERABLE\|PH7_HINT_TEXT_STATIC,zBuf,nBuf);` |
|        5 | 2244 | `}` |
|        - | 2245 | `/*` |
|        - | 2246 | ` * The two halves of the rewrite above, asked for separately.` |
|        - | 2247 | ` *` |
|        - | 2248 | `` * PH7_HINT_TEXT_ITERABLE expands a standalone `iterable`; every DIAGNOSTIC wants`` |
|        - | 2249 | `` * that and Reflection wants none of it (php prints `iterable` there).`` |
|        - | 2250 | `` * PH7_HINT_TEXT_STATIC resolves `static` beside `self`/`parent`; a diagnostic`` |
|        - | 2251 | ` * names the class it stands for, while Reflection and the declaration renderer` |
|        - | 2252 | ` * both print the keyword, php having no class to name until the call.` |
|        - | 2253 | ` */` |
|   101860 | 2254 | `PH7_PRIVATE const char *VmHintTextResolvedEx(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|        - | 2255 | `	int iFlags,char *zBuf,sxu32 nBuf)` |
|        5 | 2256 | `{` |
|        - | 2257 | `	const char *z;` |
|   101865 | 2258 | `	sxu32 n, i = 0, nAt = 0;` |
|   101865 | 2259 | `	if( nBuf == 0 ){` |
|      ! 0 | 2260 | `		return "";` |
|        - | 2261 | `	}` |
|   101865 | 2262 | `	z = pDeclared ? pDeclared->zString : 0;` |
|   101865 | 2263 | `	n = z ? pDeclared->nByte : 0;` |
|   101865 | 2264 | `	if( z && (iFlags & PH7_HINT_TEXT_ITERABLE) ){` |
|   100403 | 2265 | `		const char *zIter = 0;` |
|   100403 | 2266 | `		if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        9 | 2267 | `			zIter = "Traversable\|array";` |
|   100400 | 2268 | `		}else if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|        3 | 2269 | `			zIter = "Traversable\|array\|null";` |
|        1 | 2270 | `		}` |
|   100403 | 2271 | `		if( zIter ){` |
|       11 | 2272 | `			sxu32 nIter = SyStrlen(zIter);` |
|       11 | 2273 | `			if( nIter > nBuf - 1 ){` |
|      ! 0 | 2274 | `				nIter = nBuf - 1;` |
|      ! 0 | 2275 | `			}` |
|       11 | 2276 | `			SyMemcpy(zIter,zBuf,nIter);` |
|       11 | 2277 | `			zBuf[nIter] = 0;` |
|       11 | 2278 | `			return zBuf;` |
|        - | 2279 | `		}` |
|    50195 | 2280 | `	}` |
|   204485 | 2281 | `	while( i < n && nAt + 1 < nBuf ){` |
|        - | 2282 | `		sxu32 nStart, nCopy;` |
|        - | 2283 | `		SyString sTok;` |
|        - | 2284 | `		const SyString *pOut;` |
|   102633 | 2285 | `		if( !VmHintNameChar(z[i]) ){` |
|      647 | 2286 | `			zBuf[nAt++] = z[i++];` |
|      647 | 2287 | `			continue;` |
|        - | 2288 | `		}` |
|   101991 | 2289 | `		nStart = i;` |
|   411437 | 2290 | `		while( i < n && VmHintNameChar(z[i]) ){` |
|   309451 | 2291 | `			i++;` |
|        5 | 2292 | `		}` |
|   101991 | 2293 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|   101991 | 2294 | `		pOut = &sTok;` |
|   101986 | 2295 | `		if( VmHintIsScopeKeyword(&sTok)` |
|    51040 | 2296 | `		 && ( (iFlags & PH7_HINT_TEXT_STATIC)` |
|       52 | 2297 | `		   \|\| sTok.nByte != sizeof("static")-1` |
|       24 | 2298 | `		   \|\| SyStrnicmp(sTok.zString,"static",sizeof("static")-1) != 0 ) ){` |
|       68 | 2299 | `			ph7_class *pRes = VmResolveTypeClass(pVm,&sTok,pScope);` |
|       68 | 2300 | `			if( pRes ){` |
|       54 | 2301 | `				pOut = &pRes->sName;` |
|       26 | 2302 | `			}` |
|       33 | 2303 | `		}` |
|   101991 | 2304 | `		nCopy = pOut->nByte;` |
|   101991 | 2305 | `		if( nCopy > nBuf - nAt - 1 ){` |
|      ! 0 | 2306 | `			nCopy = nBuf - nAt - 1;` |
|      ! 0 | 2307 | `		}` |
|   101991 | 2308 | `		if( nCopy > 0 ){` |
|   101991 | 2309 | `			SyMemcpy(pOut->zString,&zBuf[nAt],nCopy);` |
|   101991 | 2310 | `			nAt += nCopy;` |
|    50993 | 2311 | `		}` |
|        5 | 2312 | `	}` |
|   101857 | 2313 | `	zBuf[nAt] = 0;` |
|   101857 | 2314 | `	return zBuf;` |
|    50935 | 2315 | `}` |
|        - | 2316 | `/*` |
|        - | 2317 | ` * PHL's number model flags a whole-valued real MEMOBJ_REAL\|MEMOBJ_INT (the` |
|        - | 2318 | ` * float-identity leniency — see the typed-constant note above` |
|        - | 2319 | ` * VmEnforceConstantType). Observer sites (is_int, gettype, var_dump, ===)` |
|        - | 2320 | ` * treat REAL as dominant, so such a value READS as a float. But every` |
|        - | 2321 | `` * TYPE-CHECK mask test (`iFlags & nType`) accepts it through its INT bit,`` |
|        - | 2322 | ` * so an int-typed parameter / return / property / union member silently` |
|        - | 2323 | ` * kept the value LOOKING like a float where php produces a genuine int` |
|        - | 2324 | ` * (weak-mode float->int here is lossless by construction). Call this after` |
|        - | 2325 | ` * a mask ACCEPT to materialize the int. No-op for any other value/type` |
|        - | 2326 | ` * pairing — including SXU32_HIGH and int\|float-style masks (REAL bit` |
|        - | 2327 | ` * present).` |
|        - | 2328 | ` *` |
|        - | 2329 | `` * Recorded leniency (strict_types): php rejects a float literal (`f(1.0)`)`` |
|        - | 2330 | ` * under strict_types, but the SAME dual-flagged shape also comes out of` |
|        - | 2331 | ``  * PHL arithmetic/builtins where php produces a genuine INT — `pow(2,3)` `` |
|        - | 2332 | ` * is php int(8), PHL whole-real float(8). PHL cannot tell those apart by` |
|        - | 2333 | ` * flags, so strict mode accepts-and-materializes both rather than` |
|        - | 2334 | `` * rejecting the php-valid `f(pow(2,3))`.`` |
|        - | 2335 | ` */` |
|    69564 | 2336 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType)` |
|        5 | 2337 | `{` |
|    69564 | 2338 | `	if( (nType & MEMOBJ_INT) && (nType & MEMOBJ_REAL) == 0` |
|    31247 | 2339 | `	 && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        - | 2340 | `		/* NOT PH7_MemObjToInteger — that no-ops when the INT bit is already` |
|        - | 2341 | `		 * set, which is exactly the dual-flag case. x.iVal already holds the` |
|        - | 2342 | `		 * exact integer (MemObjTryIntger only sets the INT bit when the` |
|        - | 2343 | `		 * real->int->real round-trip is lossless); drop the REAL identity. */` |
|       55 | 2344 | `		pVal->x.iVal = (sxi64)pVal->rVal;` |
|       55 | 2345 | `		SyBlobRelease(&pVal->sBlob);` |
|       55 | 2346 | `		MemObjSetType(pVal, MEMOBJ_INT);` |
|       25 | 2347 | `	}` |
|    69569 | 2348 | `}` |
|     3358 | 2349 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|        - | 2350 | `	ph7_class *pSelf)` |
|        5 | 2351 | `{` |
|        - | 2352 | `	sxu32 i;` |
|        - | 2353 | `	sxu32 nAlts;` |
|        - | 2354 | `	ph7_type_alt *aAlts;` |
|        - | 2355 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - | 2356 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|     3363 | 2357 | `	int bHasIntersection = 0;` |
|        - | 2358 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|     3363 | 2359 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       19 | 2360 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - | 2361 | `	}` |
|     3347 | 2362 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|     3347 | 2363 | `	nAlts = SySetUsed(pAlts);` |
|        - | 2364 | `	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the` |
|        - | 2365 | `	 * value must match ALL its members); singleton groups are ordinary union` |
|        - | 2366 | `	 * alternatives (match ANY). Group ids are NOT dense in the stored set —` |
|        - | 2367 | ``	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be`` |
|        - | 2368 | `	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */` |
|   110291 | 2369 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    10069 | 2370 | `	for( i = 0; i < nAlts; i++ ){` |
|     6727 | 2371 | `		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){` |
|       46 | 2372 | `			bHasIntersection = 1;` |
|       21 | 2373 | `		}` |
|     3360 | 2374 | `	}` |
|        - | 2375 | `	/* Intersection phase: an object satisfies an intersection group iff it is` |
|        - | 2376 | `	 * instanceof every member. Members are always class types (enforced at parse),` |
|        - | 2377 | `	 * so a non-object value can never satisfy a group. Skipped entirely for a pure` |
|        - | 2378 | `	 * union (the common case), which then pays nothing for the group machinery. */` |
|     3347 | 2379 | `	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){` |
|       36 | 2380 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 2381 | `		sxu32 g;` |
|      422 | 2382 | `		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){` |
|        - | 2383 | `			int bAll;` |
|      410 | 2384 | `			if( aGroupCount[g] < 2 ) continue;` |
|       36 | 2385 | `			bAll = 1;` |
|       88 | 2386 | `			for( i = 0; i < nAlts; i++ ){` |
|        - | 2387 | `				ph7_class *pExpected;` |
|       68 | 2388 | `				if( aAlts[i].nGroup != g ) continue;` |
|       64 | 2389 | `				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }` |
|       64 | 2390 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|       64 | 2391 | `				if( pExpected == 0 \|\| !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       15 | 2392 | `					bAll = 0;` |
|       15 | 2393 | `					break;` |
|        - | 2394 | `				}` |
|       28 | 2395 | `			}` |
|       36 | 2396 | `			if( bAll ) return SXRET_OK;` |
|        9 | 2397 | `		}` |
|        6 | 2398 | `	}` |
|        - | 2399 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|        - | 2400 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|        - | 2401 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|        - | 2402 | ``	 * `iterable\|Foo`). Only singleton-group (ordinary union) atoms apply here. */`` |
|     9961 | 2403 | `	for( i = 0; i < nAlts; i++ ){` |
|     6681 | 2404 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     6632 | 2405 | `		if( aAlts[i].nType == SXU32_HIGH` |
|     4932 | 2406 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|       47 | 2407 | `			return SXRET_OK;` |
|        - | 2408 | `		}` |
|     3294 | 2409 | `	}` |
|     3285 | 2410 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|     3285 | 2411 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|     9879 | 2412 | `	for( i = 0; i < nAlts; i++ ){` |
|     6599 | 2413 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|     6555 | 2414 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|     3381 | 2415 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|     3377 | 2416 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|     1382 | 2417 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|     1156 | 2418 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|     1116 | 2419 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|        5 | 2420 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|     3274 | 2421 | `	}` |
|        - | 2422 | `	/* Object handling */` |
|     3285 | 2423 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      105 | 2424 | `		if( bHasObjAlt ) return SXRET_OK;` |
|      105 | 2425 | `		if( bHasClassAlt ){` |
|       91 | 2426 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      225 | 2427 | `			for( i = 0; i < nAlts; i++ ){` |
|        - | 2428 | `				ph7_class *pExpected;` |
|      169 | 2429 | `				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      161 | 2430 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|      115 | 2431 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|      115 | 2432 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       34 | 2433 | `					return SXRET_OK;` |
|        - | 2434 | `				}` |
|       44 | 2435 | `			}` |
|       28 | 2436 | `		}` |
|       74 | 2437 | `		return SXERR_INVALID;` |
|        - | 2438 | `	}` |
|        - | 2439 | `	/* Array handling */` |
|     3185 | 2440 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|     2006 | 2441 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - | 2442 | `	}` |
|        - | 2443 | `	/* Scalar handling — exact match first. REAL before INT: a whole-valued` |
|        - | 2444 | `	 * real carries MEMOBJ_REAL\|MEMOBJ_INT (float-identity leniency), reads as` |
|        - | 2445 | ``	 * a float, and must prefer a `float` member (php: 1.0 into int\|float`` |
|        - | 2446 | ``	 * stays float); absent one, an `int` member takes it as a genuine int`` |
|        - | 2447 | `	 * (php: 1.0 into int\|string is int(1)) — materialize so it stops READING` |
|        - | 2448 | `	 * as a float. A pure int never carries REAL, so its arm is unaffected. */` |
|     1184 | 2449 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|       23 | 2450 | `		if( bHasFloat ) return SXRET_OK;` |
|        5 | 2451 | `	}` |
|     1176 | 2452 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|      129 | 2453 | `		if( bHasInt ){` |
|      105 | 2454 | `			VmMaterializeIntTyped(pValue, MEMOBJ_INT);` |
|      105 | 2455 | `			return SXRET_OK;` |
|        - | 2456 | `		}` |
|       12 | 2457 | `	}` |
|     1076 | 2458 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|     1048 | 2459 | `		if( bHasString ) return SXRET_OK;` |
|        8 | 2460 | `	}` |
|       48 | 2461 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|        3 | 2462 | `		if( bHasBool ) return SXRET_OK;` |
|        1 | 2463 | `	}` |
|       48 | 2464 | `	if( bStrict ){` |
|        - | 2465 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|        8 | 2466 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 | 2467 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 | 2468 | `			return SXRET_OK;` |
|        - | 2469 | `		}` |
|        8 | 2470 | `		return SXERR_INVALID;` |
|        - | 2471 | `	}` |
|        - | 2472 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - | 2473 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - | 2474 | `	 * to match PHP's union RFC. */` |
|        - | 2475 | `	{` |
|       41 | 2476 | `		int kind = VmStringNumericKind(pValue);` |
|       41 | 2477 | `		if( bHasInt ){` |
|        - | 2478 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - | 2479 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 | 2480 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 2481 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 | 2482 | `				return SXRET_OK;` |
|        - | 2483 | `			}` |
|       18 | 2484 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 2485 | `				ph7_real r = pValue->rVal;` |
|        - | 2486 | ``				/* Range first: `(sxi64)r` is undefined outside it, and NaN`` |
|        - | 2487 | `				 * and the infinities are not exact ints either way. */` |
|        - | 2488 | `				/* (double)r: see VmValueIsLossyToInt -- a no-op where ph7_real` |
|        - | 2489 | `				 * is double, and the narrowing MSVC turns into an error under` |
|        - | 2490 | `				 * PH7_OMIT_FLOATING_POINT otherwise. */` |
|      ! 0 | 2491 | `				if( PH7_RealFitsInt64((double)r) && r == (ph7_real)(sxi64)r ){` |
|      ! 0 | 2492 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 | 2493 | `					return SXRET_OK;` |
|        - | 2494 | `				}` |
|      ! 0 | 2495 | `			}` |
|       18 | 2496 | `			if( kind == 1 ){` |
|        9 | 2497 | `				PH7_MemObjToInteger(pValue);` |
|        9 | 2498 | `				return SXRET_OK;` |
|        - | 2499 | `			}` |
|        4 | 2500 | `		}` |
|       33 | 2501 | `		if( bHasFloat ){` |
|       10 | 2502 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 | 2503 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 | 2504 | `				return SXRET_OK;` |
|        - | 2505 | `			}` |
|       10 | 2506 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 | 2507 | `				PH7_MemObjToReal(pValue);` |
|        7 | 2508 | `				return SXRET_OK;` |
|        - | 2509 | `			}` |
|        1 | 2510 | `		}` |
|       26 | 2511 | `		if( bHasString ){` |
|      ! 0 | 2512 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 | 2513 | `				PH7_MemObjToString(pValue);` |
|      ! 0 | 2514 | `				return SXRET_OK;` |
|        - | 2515 | `			}` |
|      ! 0 | 2516 | `		}` |
|       26 | 2517 | `		if( bHasBool ){` |
|        3 | 2518 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|        3 | 2519 | `				PH7_MemObjToBool(pValue);` |
|        3 | 2520 | `				return SXRET_OK;` |
|        - | 2521 | `			}` |
|      ! 0 | 2522 | `		}` |
|        - | 2523 | `	}` |
|       24 | 2524 | `	return SXERR_INVALID;` |
|     1681 | 2525 | `}` |
|        - | 2526 |  |
|        - | 2527 | `/*` |
|        - | 2528 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - | 2529 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - | 2530 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - | 2531 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - | 2532 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - | 2533 | ` */` |
|      520 | 2534 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        5 | 2535 | `{` |
|        - | 2536 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|        - | 2537 | `	 * null value satisfies it (and a null value matches via the flag test` |
|        - | 2538 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|        - | 2539 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|        - | 2540 | `	 * silently swallow any argument. */` |
|      525 | 2541 | `	if( nType == MEMOBJ_NULL ){` |
|        3 | 2542 | `		return SXERR_INVALID;` |
|        - | 2543 | `	}` |
|        - | 2544 | `	/* Array and scalar never coerce into each other. php throws a TypeError` |
|        - | 2545 | `	 * rather than wrapping a scalar into a 1-element array or stringifying an` |
|        - | 2546 | ``	 * array (`function f(array $a){} f(true)` — php: "must be of type array,`` |
|        - | 2547 | ``	 * true given"; PHL used to run the body with $a=[true], and `f(int $a){}`` |
|        - | 2548 | ``	 * f([1,2])` truncated the array to int(1)). The typed-property store and`` |
|        - | 2549 | `	 * return-type paths already guard this inline; enforcing it here closes the` |
|        - | 2550 | `	 * same hole for typed PARAMETERS (positional/variadic/union) and generator` |
|        - | 2551 | `	 * params, which all funnel their scalar coercion through this helper. An` |
|        - | 2552 | `	 * object value against an array type is caught here too (never valid);` |
|        - | 2553 | `	 * object->scalar stays a separate case handled by the callers. */` |
|      523 | 2554 | `	if( ((pVal->iFlags & MEMOBJ_HASHMAP) != 0) != (nType == MEMOBJ_HASHMAP) ){` |
|       61 | 2555 | `		return SXERR_INVALID;` |
|        - | 2556 | `	}` |
|      465 | 2557 | `	if( bStrict ){` |
|        - | 2558 | `		/* Only int -> float widening is allowed implicitly. */` |
|       59 | 2559 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 | 2560 | `			PH7_MemObjToReal(pVal);` |
|        3 | 2561 | `			return SXRET_OK;` |
|        - | 2562 | `		}` |
|       57 | 2563 | `		return SXERR_INVALID;` |
|        - | 2564 | `	}` |
|        - | 2565 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|        - | 2566 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|        - | 2567 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|        - | 2568 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|        - | 2569 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|        - | 2570 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|        - | 2571 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|      411 | 2572 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       20 | 2573 | `		return SXERR_INVALID;` |
|        - | 2574 | `	}` |
|        - | 2575 | `	/* An object satisfies a scalar type in exactly ONE php weak-mode case: an` |
|        - | 2576 | ``	 * object with __toString() coerces to a `string` parameter (its __toString`` |
|        - | 2577 | `	 * is invoked by the string cast below). Every other scalar target —` |
|        - | 2578 | ``	 * int/float/bool, or a `string` target on an object WITHOUT __toString —`` |
|        - | 2579 | `	 * is a TypeError. Without this, PH7_MemObjCastMethod() silently produced` |
|        - | 2580 | `	 * int(1)/float(1)/bool(true) for any object and "Object" for a` |
|        - | 2581 | `	 * non-stringable object passed to a string parameter. (Strict mode already` |
|        - | 2582 | `	 * rejected all of these in the bStrict block above; the array<->object case` |
|        - | 2583 | `	 * is caught by the array guard.) */` |
|      393 | 2584 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       22 | 2585 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       27 | 2586 | `		if( !(nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       10 | 2587 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       13 | 2588 | `			return SXERR_INVALID;` |
|        - | 2589 | `		}` |
|        4 | 2590 | `	}` |
|      376 | 2591 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|      303 | 2592 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|      277 | 2593 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|      101 | 2594 | `		return SXERR_INVALID;` |
|        - | 2595 | `	}` |
|      285 | 2596 | `	if( nType == MEMOBJ_INT && VmValueIsLossyToInt(pVal) ){` |
|        - | 2597 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion;` |
|        - | 2598 | `		 * PHL rejects it (the scope policy). SXERR_INVALID routes to the caller's TypeError,` |
|        - | 2599 | `		 * exactly like the null / non-numeric-string cases above. An INTEGRAL` |
|        - | 2600 | `		 * float loses nothing and coerces normally. One predicate answers this` |
|        - | 2601 | `		 * for the typed parameters and returns that reach here, for the typed` |
|        - | 2602 | `		 * PROPERTY store (which has its own weak path below) and for the` |
|        - | 2603 | `		 * integer-only operators. */` |
|      ! 0 | 2604 | `		return SXERR_INVALID;` |
|        - | 2605 | `	}` |
|      280 | 2606 | `	if( nType == MEMOBJ_STRING && (pVal->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|       56 | 2607 | `	 && pVal->pVm && PH7_IS_NAN(pVal->rVal) ){` |
|        - | 2608 | ``		/* A userland `string` parameter, return or property taking a NaN: php's`` |
|        - | 2609 | `		 * weak coercion warns there exactly as its ZPP does for an internal one` |
|        - | 2610 | ``		 * (`unexpected NAN value was coerced to string`). The cast below is the`` |
|        - | 2611 | `		 * silent conversion -- it is shared with the engine's own -- so the` |
|        - | 2612 | `		 * diagnostic is raised here, where the DECLARED type is known. */` |
|        3 | 2613 | `		VmErrorFormat(pVal->pVm,PH7_CTX_WARNING,` |
|        - | 2614 | `			"unexpected NAN value was coerced to string");` |
|        1 | 2615 | `	}` |
|        - | 2616 | `	{` |
|      285 | 2617 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|      285 | 2618 | `		if( xCast ) xCast(pVal);` |
|        - | 2619 | `	}` |
|      285 | 2620 | `	return SXRET_OK;` |
|      265 | 2621 | `}` |
|        - | 2622 |  |
|        - | 2623 | `/*` |
|        - | 2624 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - | 2625 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - | 2626 | ` *` |
|        - | 2627 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - | 2628 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - | 2629 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - | 2630 | ` * back to a static literal and ignore zBuf entirely.` |
|        - | 2631 | ` */` |
|      300 | 2632 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        5 | 2633 | `{` |
|      305 | 2634 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|      305 | 2635 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|      305 | 2636 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|      305 | 2637 | `		if( pDeclared->zString && nCopy > 0 ){` |
|      305 | 2638 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|      150 | 2639 | `		}` |
|      305 | 2640 | `		zBuf[nCopy] = 0;` |
|      305 | 2641 | `		return zBuf;` |
|        - | 2642 | `	}` |
|      ! 0 | 2643 | `	switch( nType ){` |
|      ! 0 | 2644 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 | 2645 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 | 2646 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 | 2647 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 | 2648 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 | 2649 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 | 2650 | `		default:             return "scalar";` |
|        - | 2651 | `	}` |
|      155 | 2652 | `}` |
|        - | 2653 |  |
|        - | 2654 | `/*` |
|        - | 2655 | ` * Render the expected-type text of a single (non-union) CLASS or pseudo-type hint` |
|        - | 2656 | ` * the way php writes it in a TypeError:` |
|        - | 2657 | ` *` |
|        - | 2658 | `` *   - the RESOLVED class, so `self`/`parent` name the class they stand for`` |
|        - | 2659 | `` *     (`?self` in class Cee reads `?Cee`); an unresolvable name stays as written;`` |
|        - | 2660 | `` *   - `iterable` expanded to its real alternatives, `Traversable\|array`;`` |
|        - | 2661 | `` *   - a leading `?` whenever the hint is nullable — including the implicit`` |
|        - | 2662 | `` *     `Cee $c = null` form, which php also prints as `?Cee`.`` |
|        - | 2663 | ` *` |
|        - | 2664 | ` * The scalar half of this is VmScalarTypeName (which reads the declared text` |
|        - | 2665 | `` * straight off the formal, `?` included). A class hint cannot do that: its text`` |
|        - | 2666 | `` * is resolved per call site, so the `?` has to be re-applied here — which is why`` |
|        - | 2667 | `` * the message used to say `Cee` where php says `?Cee`.`` |
|        - | 2668 | ` */` |
|      182 | 2669 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|        - | 2670 | `	int bNullable,char *zBuf,sxu32 nBuf)` |
|        5 | 2671 | `{` |
|      187 | 2672 | `	const SyString *pName = pResolved ? &pResolved->sName : pAsWritten;` |
|        - | 2673 | `	sxu32 nCopy;` |
|      187 | 2674 | `	sxu32 nAt = 0;` |
|      187 | 2675 | `	if( nBuf == 0 ){` |
|      ! 0 | 2676 | `		return "";` |
|        - | 2677 | `	}` |
|      182 | 2678 | `	if( pName && SyStringLength(pName) == sizeof("iterable")-1 && pName->zString` |
|       79 | 2679 | `	 && SyStrnicmp(pName->zString,"iterable",sizeof("iterable")-1) == 0 ){` |
|       22 | 2680 | `		const char *zIter = bNullable ? "Traversable\|array\|null" : "Traversable\|array";` |
|       22 | 2681 | `		nCopy = SyStrlen(zIter);` |
|       22 | 2682 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       22 | 2683 | `		SyMemcpy(zIter,zBuf,nCopy);` |
|       22 | 2684 | `		zBuf[nCopy] = 0;` |
|       22 | 2685 | `		return zBuf;` |
|        - | 2686 | `	}` |
|      169 | 2687 | `	if( bNullable && nBuf > 1 ){` |
|       23 | 2688 | `		zBuf[nAt++] = '?';` |
|       10 | 2689 | `	}` |
|      169 | 2690 | `	nCopy = (pName && pName->zString) ? SyStringLength(pName) : 0;` |
|      169 | 2691 | `	if( nCopy >= nBuf - nAt ) nCopy = nBuf - nAt - 1;` |
|      169 | 2692 | `	if( nCopy > 0 ) SyMemcpy(pName->zString,&zBuf[nAt],nCopy);` |
|      169 | 2693 | `	zBuf[nAt + nCopy] = 0;` |
|      169 | 2694 | `	return zBuf;` |
|       96 | 2695 | `}` |
|        - | 2696 |  |
|        - | 2697 | `/*` |
|        - | 2698 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - | 2699 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - | 2700 | ` */` |
|      144 | 2701 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        5 | 2702 | `{` |
|      149 | 2703 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      221 | 2704 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|      144 | 2705 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|      149 | 2706 | `	return zBuf;` |
|        5 | 2707 | `}` |
|        - | 2708 |  |
|        - | 2709 | `/*` |
|        - | 2710 | ` * php's write_property handler (ph7_class::xSet): a native class whose properties` |
|        - | 2711 | ` * are its own C struct converts the incoming value the way that struct demands —` |
|        - | 2712 | ` * and may refuse the write outright. The value is rewritten IN PLACE, so what the` |
|        - | 2713 | ` * caller goes on to store is what the hook left behind.` |
|        - | 2714 | ` */` |
|      618 | 2715 | `static sxi32 VmRunNativeSet(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pValue)` |
|        4 | 2716 | `{` |
|        - | 2717 | `	PH7_NativeSetCtx sSet;` |
|      622 | 2718 | `	if( PH7_VmAttrInst(pVmAttr) == 0 ){` |
|      ! 0 | 2719 | `		return SXRET_OK;   /* a class static: no object for a handler to run on */` |
|        - | 2720 | `	}` |
|      622 | 2721 | `	sSet.pName = &pVmAttr->pAttr->sName;` |
|      622 | 2722 | `	sSet.pValue = pValue;` |
|      622 | 2723 | `	sSet.zThrowClass = 0;` |
|      622 | 2724 | `	sSet.zThrowMsg[0] = 0;` |
|      622 | 2725 | `	if( PH7_ClassNativeSet(PH7_VmAttrInst(pVmAttr),&sSet) && sSet.zThrowClass ){` |
|        8 | 2726 | `		return VmThrowFixedError(pVm,sSet.zThrowClass,sSet.zThrowMsg);` |
|        - | 2727 | `	}` |
|      615 | 2728 | `	return SXRET_OK;` |
|      313 | 2729 | `}` |
|        - | 2730 | `/*` |
|        - | 2731 | ` * The same handler, asked of a SLOT that an opcode has already mutated in place.` |
|        - | 2732 | `` * `$i->f++` and `$i->f--` never pass a value through the store filter — they`` |
|        - | 2733 | ` * increment the slot where it lies — so the conversion has to be applied after` |
|        - | 2734 | ` * the fact, which is exactly what php does (it reads, increments, and writes` |
|        - | 2735 | `` * back through the handler: `$i->f = 1.456008; ++$i->f` leaves the property at`` |
|        - | 2736 | ` * 2.456007, the microsecond truncation of the sum). Answers SXRET_OK when the` |
|        - | 2737 | ` * slot is not a native one.` |
|        - | 2738 | ` */` |
|  7355390 | 2739 | `static void VmFilterBitSet(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 2740 | `{` |
|  7355395 | 2741 | `	if( pVm->bFilterBitsOff ){` |
|      ! 0 | 2742 | `		return;` |
|        - | 2743 | `	}` |
|  7355395 | 2744 | `	if( nIdx >= pVm->nFilterBits ){` |
|     1455 | 2745 | `		sxu32 nNew = pVm->nFilterBits ? pVm->nFilterBits : 1024;` |
|        - | 2746 | `		unsigned char *pNew;` |
|        - | 2747 | `		/* An index this large cannot be a real slot, and doubling toward it would` |
|        - | 2748 | `		 * wrap. Treat it exactly like a failed allocation. */` |
|     1455 | 2749 | `		if( nIdx >= (1u << 30) ){` |
|      ! 0 | 2750 | `			pVm->bFilterBitsOff = 1;` |
|      ! 0 | 2751 | `			return;` |
|        - | 2752 | `		}` |
|     1501 | 2753 | `		while( nNew <= nIdx ){` |
|       49 | 2754 | `			nNew <<= 1;` |
|        3 | 2755 | `		}` |
|     1455 | 2756 | `		pNew = (unsigned char *)SyMemBackendAlloc(&pVm->sAllocator,nNew >> 3);` |
|     1455 | 2757 | `		if( pNew == 0 ){` |
|        - | 2758 | `			/* This slot IS filtered and the bitmap cannot say so. It must not answer` |
|        - | 2759 | `			 * for anything else either, or the screens in front of it would skip a` |
|        - | 2760 | `			 * type check, a readonly refusal or a native write handler. */` |
|      ! 0 | 2761 | `			pVm->bFilterBitsOff = 1;` |
|      ! 0 | 2762 | `			return;` |
|        - | 2763 | `		}` |
|     1455 | 2764 | `		SyZero(pNew,nNew >> 3);` |
|     1455 | 2765 | `		if( pVm->pFilterBits ){` |
|       25 | 2766 | `			SyMemcpy(pVm->pFilterBits,pNew,pVm->nFilterBits >> 3);` |
|       25 | 2767 | `			SyMemBackendFree(&pVm->sAllocator,pVm->pFilterBits);` |
|       11 | 2768 | `		}` |
|     1455 | 2769 | `		pVm->pFilterBits = pNew;` |
|     1455 | 2770 | `		pVm->nFilterBits = nNew;` |
|      722 | 2771 | `	}` |
|  7355395 | 2772 | `	pVm->pFilterBits[nIdx >> 3] \|= (unsigned char)(1 << (nIdx & 7));` |
|  3677547 | 2773 | `}` |
|        - | 2774 | `/*` |
|        - | 2775 | ` * Register a property slot with the store filter, and drop it again. These two` |
|        - | 2776 | ` * are the ONLY writers of pVm->hTypedSlot: the predicate that decides membership` |
|        - | 2777 | ` * lives here once (a declared type, a native write handler, or both), and the` |
|        - | 2778 | ` * handler COUNT that lets the mutation opcodes skip the table entirely is kept` |
|        - | 2779 | ` * beside it -- registering in one place and forgetting to drop in another is` |
|        - | 2780 | ` * exactly how a recycled memobj index would inherit a stale entry.` |
|        - | 2781 | ` *` |
|        - | 2782 | ` * They also keep the SLOT BITMAP the hot-path screens read (see pFilterBits): it` |
|        - | 2783 | ` * answers the membership question without hashing, and because it is written here` |
|        - | 2784 | ` * and nowhere else it cannot drift from the table it screens.` |
|        - | 2785 | ` */` |
| 10463389 | 2786 | `PH7_PRIVATE sxi32 PH7_VmStoreFilterRegister(ph7_vm *pVm,VmClassAttr *pVmAttr)` |
|        5 | 2787 | `{` |
| 10463394 | 2788 | `	if( !PH7_ATTR_STORE_FILTERED(pVmAttr->pAttr) ){` |
|  3108004 | 2789 | `		return SXRET_OK;` |
|        - | 2790 | `	}` |
|  7355395 | 2791 | `	if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|      ! 0 | 2792 | `		return SXERR_MEM;` |
|        - | 2793 | `	}` |
|  7355395 | 2794 | `	VmFilterBitSet(&(*pVm),pVmAttr->nIdx);` |
|  7355395 | 2795 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|    10123 | 2796 | `		pVm->nNativeSetSlot++;` |
|     5059 | 2797 | `	}` |
|  7355395 | 2798 | `	return SXRET_OK;` |
|  5231092 | 2799 | `}` |
|  9757853 | 2800 | `PH7_PRIVATE void PH7_VmStoreFilterDrop(ph7_vm *pVm,ph7_class_attr *pAttr,sxu32 nIdx)` |
|        5 | 2801 | `{` |
|  9757858 | 2802 | `	if( pAttr == 0 \|\| !PH7_ATTR_STORE_FILTERED(pAttr) ){` |
|  2903187 | 2803 | `		return;` |
|        - | 2804 | `	}` |
|  6854676 | 2805 | `	if( SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32),0) == SXRET_OK ){` |
|  6854676 | 2806 | `		if( nIdx < pVm->nFilterBits ){` |
|  6854676 | 2807 | `			pVm->pFilterBits[nIdx >> 3] &= (unsigned char)~(1 << (nIdx & 7));` |
|  3427188 | 2808 | `		}` |
|  6854676 | 2809 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) && pVm->nNativeSetSlot > 0 ){` |
|    10109 | 2810 | `			pVm->nNativeSetSlot--;` |
|     5052 | 2811 | `		}` |
|  3427188 | 2812 | `	}` |
|  4878334 | 2813 | `}` |
|   955112 | 2814 | `PH7_PRIVATE sxi32 PH7_VmNativeSetSlot(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        5 | 2815 | `{` |
|        - | 2816 | `	SyHashEntry *pSlot;` |
|        - | 2817 | `	VmClassAttr *pVmAttr;` |
|   955117 | 2818 | `	if( nIdx == SXU32_HIGH \|\| pVm->nNativeSetSlot == 0 \|\| !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|   953099 | 2819 | `		return SXRET_OK;` |
|        - | 2820 | `	}` |
|   155469 | 2821 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|   155469 | 2822 | `	if( pSlot == 0 ){` |
|      ! 0 | 2823 | `		return SXRET_OK;` |
|        - | 2824 | `	}` |
|     2019 | 2825 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     2019 | 2826 | `	if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) == 0 ){` |
|     2013 | 2827 | `		return SXRET_OK;` |
|        - | 2828 | `	}` |
|        7 | 2829 | `	return VmRunNativeSet(pVm,pVmAttr,pValue);` |
|   478599 | 2830 | `}` |
|  1073858 | 2831 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int iStoreFlags)` |
|        5 | 2832 | `{` |
|  1073863 | 2833 | `	int bViaRef = (iStoreFlags & VM_TYPED_STORE_VIA_REF) != 0;` |
|  1073863 | 2834 | `	int bCloneInit = (iStoreFlags & VM_TYPED_STORE_CLONE_INIT) != 0;` |
|        - | 2835 | `	SyHashEntry *pSlot;` |
|        - | 2836 | `	VmClassAttr *pVmAttr;` |
|        - | 2837 | `	ph7_class_attr *pAttr;` |
|        - | 2838 | `	ph7_class *pHintScope;` |
|        - | 2839 | `	char zGivenBuf[128];` |
|        - | 2840 | `	/* php decides a typed-property store by the strict_types mode of the file the` |
|        - | 2841 | `	 * ASSIGNMENT sits in — not the class's — which is what the executing` |
|        - | 2842 | `	 * instruction's own unit mode says (pVm->bCurStrict, published under the` |
|        - | 2843 | `	 * nLine != 0 gate so an engine-dispatched write keeps the calling file's). */` |
|  1073863 | 2844 | `	int bStrict = pVm->bCurStrict ? 1 : 0;` |
|  1073863 | 2845 | `	if( !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|   972147 | 2846 | `		return SXRET_OK; /* Not a filtered slot -- the common answer, and free */` |
|        - | 2847 | `	}` |
|  1018087 | 2848 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  1018087 | 2849 | `	if( pSlot == 0 ){` |
|      ! 0 | 2850 | `		return SXRET_OK; /* Not a typed slot */` |
|        - | 2851 | `	}` |
|   101721 | 2852 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   101721 | 2853 | `	pAttr = pVmAttr->pAttr;` |
|   101721 | 2854 | `	if( pAttr == 0 ){` |
|      ! 0 | 2855 | `		return SXRET_OK;` |
|        - | 2856 | `	}` |
|   101721 | 2857 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|        - | 2858 | `		/* php's write_property handler for this class refuses outright, and its` |
|        - | 2859 | `		 * sentence is the readonly one -- without the readonly FLAG, which is why` |
|        - | 2860 | `		 * Reflection still reports isReadOnly() false for DatePeriod's seven. The` |
|        - | 2861 | `		 * C bodies that fill them write the slot directly and never come here. */` |
|       23 | 2862 | `		return VmThrowNativeNoWrite(pVm,PH7_VmAttrOwner(pVmAttr),pAttr);` |
|        - | 2863 | `	}` |
|   101699 | 2864 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|      616 | 2865 | `		sxi32 rcNat = VmRunNativeSet(pVm,pVmAttr,pValue);` |
|      616 | 2866 | `		if( rcNat != SXRET_OK ){` |
|        8 | 2867 | `			return rcNat;` |
|        - | 2868 | `		}` |
|      303 | 2869 | `	}` |
|   101693 | 2870 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      609 | 2871 | `		return SXRET_OK;` |
|        - | 2872 | `	}` |
|        - | 2873 | ``	/* `self`/`parent` in the declared type resolve against the class that DECLARED`` |
|        - | 2874 | `	 * the property (a trait's members count as the composing class), not the` |
|        - | 2875 | `	 * instance's runtime class — see VmHintScopeClass. */` |
|   101087 | 2876 | `	pHintScope = VmHintScopeClass(pVm,pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        - | 2877 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|        - | 2878 | `	 * property may be written exactly once and only from within the declaring` |
|        - | 2879 | `	 * class scope (its set-scope is protected). */` |
|   101087 | 2880 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 2881 | `		/* A readonly property is always typed and default-less, so it starts` |
|        - | 2882 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|        - | 2883 | `		 * write below — making it the write-once latch (a type-rejected write` |
|        - | 2884 | `		 * leaves it set, so a later valid initialization still works). */` |
|      149 | 2885 | `		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|        - | 2886 | `			/* Already initialized: any further write is forbidden, any scope —` |
|        - | 2887 | `			 * checked BEFORE the set-visibility scope, matching php's order.` |
|        - | 2888 | `			 * Exceptions that fall through to the set-scope check below:` |
|        - | 2889 | `			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and` |
|        - | 2890 | `			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,` |
|        - | 2891 | `			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */` |
|       44 | 2892 | `			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|       44 | 2893 | `			if( !(pCloneFr && pCloneFr->pThis` |
|       22 | 2894 | `				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){` |
|       42 | 2895 | `				return VmThrowReadonlyError(pVm,PH7_VmAttrOwner(pVmAttr),pAttr,1);` |
|        - | 2896 | `			}` |
|        1 | 2897 | `		}` |
|       53 | 2898 | `	}` |
|   101049 | 2899 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        - | 2900 | `		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.` |
|        - | 2901 | `		 * An explicit set-visibility replaces readonly's implicit protected(set). */` |
|       31 | 2902 | `		sxi32 rcVis = VmCheckSetVisibility(pVm,PH7_VmAttrOwner(pVmAttr),pAttr);` |
|       31 | 2903 | `		if( rcVis != SXRET_OK ){` |
|       15 | 2904 | `			return rcVis;` |
|        1 | 2905 | `		}` |
|   101027 | 2906 | `	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 2907 | `		/* First write (or a clone re-init) must come from within the declaring` |
|        - | 2908 | `		 * class scope (readonly's set-scope is protected — a subclass may set). */` |
|      109 | 2909 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|      109 | 2910 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        - | 2911 | `		/* A readonly property imported from a TRAIT is, per php, declared in the` |
|        - | 2912 | `		 * USING class (traits are flattened in). The trait is not in any instanceof` |
|        - | 2913 | `		 * hierarchy, so use the composing class (PH7_VmAttrOwner(pVmAttr)) as the set-scope. */` |
|      109 | 2914 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && PH7_VmAttrOwner(pVmAttr) ){` |
|      ! 0 | 2915 | `			pDecl = PH7_VmAttrOwner(pVmAttr);` |
|      ! 0 | 2916 | `		}` |
|      109 | 2917 | `		if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|        6 | 2918 | `			return VmThrowReadonlyError(pVm,PH7_VmAttrOwner(pVmAttr),pAttr,0);` |
|        - | 2919 | `		}` |
|       50 | 2920 | `	}` |
|        - | 2921 | `	/* Union type: dispatch to the shared coercion helper, under the mode of the` |
|        - | 2922 | `	 * file the ASSIGNMENT is written in — php applies strict_types to a typed` |
|        - | 2923 | `` 	 * property store exactly as to an argument (`$o->u = 1.5` on an `int\|string` `` |
|        - | 2924 | `	 * is its TypeError there), which this used to deny outright. */` |
|   101031 | 2925 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       92 | 2926 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       58 | 2927 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|       29 | 2928 | `			bStrict,pHintScope);` |
|       63 | 2929 | `		if( rc == SXRET_OK ){` |
|       38 | 2930 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       38 | 2931 | `			return SXRET_OK;` |
|        - | 2932 | `		}` |
|       29 | 2933 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 2934 | `			char zBuf[128];` |
|       25 | 2935 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        7 | 2936 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)),bViaRef);` |
|        - | 2937 | `		}` |
|       18 | 2938 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        5 | 2939 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2940 | `	}` |
|        - | 2941 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|        - | 2942 | `	 * includes null). */` |
|   100973 | 2943 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       32 | 2944 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|       26 | 2945 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        2 | 2946 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|       26 | 2947 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       26 | 2948 | `			return SXRET_OK;` |
|        - | 2949 | `		}` |
|       12 | 2950 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null",bViaRef);` |
|        - | 2951 | `	}` |
|        - | 2952 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|        - | 2953 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|        - | 2954 | `	 * type error. */` |
|   100941 | 2955 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 2956 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      ! 0 | 2957 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2958 | `	}` |
|        - | 2959 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - | 2960 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - | 2961 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|   100941 | 2962 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 | 2963 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 | 2964 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        5 | 2965 | `			return SXRET_OK;` |
|        - | 2966 | `		}` |
|       10 | 2967 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 | 2968 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2969 | `	}` |
|        - | 2970 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|        - | 2971 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|        - | 2972 | `	 * handled by the nullable check above). Checked by value before the generic` |
|        - | 2973 | `	 * class-instanceof branch, which would resolve no such class and then` |
|        - | 2974 | `	 * wrongly accept any object / reject arrays. */` |
|   100931 | 2975 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       93 | 2976 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|       93 | 2977 | `		if( rcPseudo == 1 ){` |
|       13 | 2978 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       13 | 2979 | `			return SXRET_OK;` |
|        - | 2980 | `		}` |
|       81 | 2981 | `		if( rcPseudo == 0 ){` |
|       11 | 2982 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 | 2983 | `				VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2984 | `		}` |
|        - | 2985 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|       35 | 2986 | `	}` |
|   100913 | 2987 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - | 2988 | `		/* Class / interface type. Resolve self/parent relative to the DECLARING` |
|        - | 2989 | `		 * class (pHintScope), not the instance's runtime class. */` |
|       74 | 2990 | `		ph7_class *pExpected = 0;` |
|       74 | 2991 | `		if( !VmClassHintMatches(pVm,&pAttr->sClass,pHintScope,pValue,&pExpected) ){` |
|        - | 2992 | `			char zBuf[128];` |
|       39 | 2993 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       24 | 2994 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|       18 | 2995 | `					? VmFormatValueClassName(pValue,zBuf,sizeof(zBuf))` |
|       15 | 2996 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 2997 | `		}` |
|       50 | 2998 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       50 | 2999 | `		return SXRET_OK;` |
|        - | 3000 | `	}` |
|        - | 3001 | `	/* Scalar type, strict mode: no coercion at all, and the one widening is` |
|        - | 3002 | `	 * int -> float. The flag test the weak path below uses cannot answer this on` |
|        - | 3003 | `	 * its own — an integer-valued real carries MEMOBJ_INT as a cached` |
|        - | 3004 | ``	 * representation, so `$o->i = 5.0` would read as a match — hence the value's`` |
|        - | 3005 | `	 * own type is asked in ph7_type_name()'s order, float before int. */` |
|   100843 | 3006 | `	if( bStrict ){` |
|        - | 3007 | `		int bOk;` |
|       29 | 3008 | `		if( ph7_value_is_bool(pValue) ){` |
|        5 | 3009 | `			bOk = (pAttr->nType == MEMOBJ_BOOL);` |
|       27 | 3010 | `		}else if( ph7_value_is_float(pValue) ){` |
|        3 | 3011 | `			bOk = (pAttr->nType == MEMOBJ_REAL);` |
|       24 | 3012 | `		}else if( ph7_value_is_int(pValue) ){` |
|        9 | 3013 | `			bOk = (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL);` |
|       19 | 3014 | `		}else if( ph7_value_is_string(pValue) ){` |
|       15 | 3015 | `			bOk = (pAttr->nType == MEMOBJ_STRING);` |
|        8 | 3016 | `		}else{` |
|        - | 3017 | `			/* array / resource / an object against a scalar type: no coercion in` |
|        - | 3018 | `			 * either mode, so the flag test is the whole answer (an object never` |
|        - | 3019 | `			 * carries the target's flag, and __toString is a coercion strict mode` |
|        - | 3020 | `			 * does not perform). */` |
|      ! 0 | 3021 | `			bOk = ((pValue->iFlags & pAttr->nType) != 0) && !(pValue->iFlags & MEMOBJ_OBJ);` |
|        - | 3022 | `		}` |
|       29 | 3023 | `		if( !bOk ){` |
|        - | 3024 | `			char zObjBuf[128];` |
|       31 | 3025 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       20 | 3026 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|      ! 0 | 3027 | `					? VmFormatValueClassName(pValue,zObjBuf,sizeof(zObjBuf))` |
|       20 | 3028 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3029 | `		}` |
|        9 | 3030 | `		if( pAttr->nType == MEMOBJ_REAL && !ph7_value_is_float(pValue) ){` |
|        3 | 3031 | `			PH7_MemObjToReal(pValue); /* the int -> float widening */` |
|        2 | 3032 | `		}else{` |
|        7 | 3033 | `			VmMaterializeIntTyped(pValue,pAttr->nType);` |
|        - | 3034 | `		}` |
|        9 | 3035 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        9 | 3036 | `		return SXRET_OK;` |
|        - | 3037 | `	}` |
|        - | 3038 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - | 3039 | `	 * helpers used by function-argument hints. Reject object→scalar, EXCEPT an` |
|        - | 3040 | ``	 * object with __toString() stored into a `string` property — php coerces it`` |
|        - | 3041 | `	 * via __toString, so fall through to the string cast below. */` |
|   100815 | 3042 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       16 | 3043 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       18 | 3044 | `		if( !(pAttr->nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|        4 | 3045 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|        - | 3046 | `			char zBuf[128];` |
|       20 | 3047 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        6 | 3048 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)),bViaRef);` |
|        - | 3049 | `		}` |
|        1 | 3050 | `	}` |
|        - | 3051 | ``	/* An `int` slot takes the same lossy refusal a typed PARAMETER and a return`` |
|        - | 3052 | `	 * take (VmCoerceScalarWeak's own rule, the scope policy) -- and it had none of it, so this` |
|        - | 3053 | `` 	 * one weak path was storing a number the script never wrote: `$o->i = 1.9` `` |
|        - | 3054 | ``	 * stored 1 in silence, `$o->i = 1e20` stored PHP_INT_MIN, and`` |
|        - | 3055 | ``	 * `$o->i = "99999999999999999999"` stored PHP_INT_MAX. php refuses the last`` |
|        - | 3056 | ``	 * two outright (`Cannot assign float to property C::$i of type int`) and`` |
|        - | 3057 | `	 * deprecates the first; PHL refuses all three, with the message the other two` |
|        - | 3058 | `	 * write-sites already use. Asked before the cast branches below, so a value` |
|        - | 3059 | `	 * that arrives carrying a cached int representation is asked too. */` |
|   100803 | 3060 | `	if( pAttr->nType == MEMOBJ_INT && VmValueIsLossyToInt(pValue) ){` |
|       38 | 3061 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       12 | 3062 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3063 | `	}` |
|   100779 | 3064 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|   100107 | 3065 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|   100107 | 3066 | `		if( xCast ){` |
|        - | 3067 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|   100107 | 3068 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       15 | 3069 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        4 | 3070 | `					VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3071 | `			}` |
|   100099 | 3072 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       18 | 3073 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        5 | 3074 | `					VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3075 | `			}` |
|        - | 3076 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - | 3077 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - | 3078 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|   100084 | 3079 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|   100078 | 3080 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|   100082 | 3081 | `			 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|   100048 | 3082 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string",bViaRef);` |
|        - | 3083 | `			}` |
|       44 | 3084 | `			xCast(pValue);` |
|       20 | 3085 | `		}` |
|       24 | 3086 | `	}else{` |
|        - | 3087 | `		/* Mask matched — an int property accepting a whole-real must` |
|        - | 3088 | `		 * materialize it as a genuine int (php: $o->i = 1.0 stores int(1)). */` |
|      677 | 3089 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|        - | 3090 | `	}` |
|      717 | 3091 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      717 | 3092 | `	return SXRET_OK;` |
|   537994 | 3093 | `}` |
|        - | 3094 | `/*` |
|        - | 3095 | ` * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])` |
|        - | 3096 | ` * to the freshly-produced clone. The write happens in the caller's scope, so:` |
|        - | 3097 | ` *   - visibility is enforced (a private/protected property is only writable from` |
|        - | 3098 | ` *     a scope that could normally reach it — else a catchable Error),` |
|        - | 3099 | ` *   - a readonly property may be RE-initialized here (bCloneInit) provided the` |
|        - | 3100 | ` *     caller's scope could set it (else the readonly set-scope Error),` |
|        - | 3101 | ` *   - typed properties are coerced/validated exactly as a normal store, and` |
|        - | 3102 | ` *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2` |
|        - | 3103 | ` *     deprecation notice is not emitted yet).` |
|        - | 3104 | ` * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.` |
|        - | 3105 | ` */` |
|       30 | 3106 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,` |
|        - | 3107 | `	const char *zName,sxu32 nName,ph7_value *pValue)` |
|        1 | 3108 | `{` |
|       31 | 3109 | `	ph7_class *pClass = pClone->pClass;` |
|        - | 3110 | `	SyHashEntry *pEntry;` |
|        - | 3111 | `	VmClassAttr *pVmAttr;` |
|        - | 3112 | `	ph7_class_attr *pAttr;` |
|        - | 3113 | `	ph7_value *pSlot;` |
|        - | 3114 | `	sxi32 rc;` |
|       31 | 3115 | `	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;` |
|       31 | 3116 | `	if( pEntry == 0 ){` |
|        - | 3117 | `		/* Unknown property: PHP creates a dynamic property (deprecated on a class` |
|        - | 3118 | `		 * without #[AllowDynamicProperties], but still created — the notice is a` |
|        - | 3119 | `		 * deferred residual). */` |
|      ! 0 | 3120 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);` |
|      ! 0 | 3121 | `		if( pSlot == 0 ){` |
|      ! 0 | 3122 | `			return PH7_VmMemoryError(pVm);` |
|        - | 3123 | `		}` |
|      ! 0 | 3124 | `		PH7_MemObjStore(pValue,pSlot);` |
|      ! 0 | 3125 | `		return SXRET_OK;` |
|        - | 3126 | `	}` |
|       31 | 3127 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       31 | 3128 | `	pAttr = pVmAttr->pAttr;` |
|        - | 3129 | `	/* Static / class-constant "properties" are not per-instance state. */` |
|       31 | 3130 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 3131 | `		SyBlob sMsg;` |
|      ! 0 | 3132 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 3133 | `		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",` |
|      ! 0 | 3134 | `			&pClass->sDisp,&pAttr->sName);` |
|      ! 0 | 3135 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 3136 | `	}` |
|        - | 3137 | `	/* Visibility: enforced against the current (calling) frame's scope. A public` |
|        - | 3138 | `	 * readonly property passes here and is handled by the readonly set-scope check` |
|        - | 3139 | `	 * inside VmEnforcePropertyTypeOnStore below. */` |
|       31 | 3140 | `	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 | 3141 | `		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";` |
|        5 | 3142 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        - | 3143 | `		SyBlob sMsg;` |
|        5 | 3144 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 3145 | `		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sDisp,&pAttr->sName);` |
|        5 | 3146 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 3147 | `	}` |
|        - | 3148 | `	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */` |
|       27 | 3149 | `	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,VM_TYPED_STORE_CLONE_INIT);` |
|       27 | 3150 | `	if( rc != SXRET_OK ){` |
|        3 | 3151 | `		return rc;` |
|        - | 3152 | `	}` |
|        - | 3153 | `	/* Commit the (possibly coerced) value into the property slot. */` |
|       25 | 3154 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|       25 | 3155 | `	if( pSlot ){` |
|       25 | 3156 | `		PH7_MemObjStore(pValue,pSlot);` |
|       12 | 3157 | `	}` |
|       25 | 3158 | `	return SXRET_OK;` |
|       16 | 3159 | `}` |
|        - | 3160 | `/*` |
|        - | 3161 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|        - | 3162 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|        - | 3163 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|        - | 3164 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|        - | 3165 | ` */` |
|       12 | 3166 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|        3 | 3167 | `{` |
|       15 | 3168 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 3169 | `	char zBuf[128],zType[192];` |
|        - | 3170 | `	const char *zGiven;` |
|       21 | 3171 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|        6 | 3172 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|       15 | 3173 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3174 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 | 3175 | `	}else{` |
|       15 | 3176 | `		zGiven = ph7_type_name(pValue);` |
|        - | 3177 | `	}` |
|       15 | 3178 | `	if( bLazy ){` |
|        - | 3179 | `		/* The value only materialized at the first ACCESS, because the initializer` |
|        - | 3180 | `		 * could not be evaluated at the declaration (it named a constant that did` |
|        - | 3181 | `		 * not exist yet). php reports THAT with its own wording and its own kind:` |
|        - | 3182 | ``		 * a CATCHABLE `TypeError: Cannot assign string to class constant C::K of`` |
|        - | 3183 | ``		 * type int`, raised at the access site — not the declaration-time fatal`` |
|        - | 3184 | `		 * below. The caller leaves the slot unmaterialized, so every later access` |
|        - | 3185 | `		 * re-evaluates and re-raises, as php's does. */` |
|        - | 3186 | `		SyBlob sMsg;` |
|        5 | 3187 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 3188 | `		SyBlobFormat(&sMsg,"Cannot assign %s to class constant %z::%z of type %s",` |
|        2 | 3189 | `			zGiven,&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|        5 | 3190 | `		return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        - | 3191 | `	}` |
|        - | 3192 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|        - | 3193 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|        - | 3194 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|        - | 3195 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|        - | 3196 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|        - | 3197 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|        - | 3198 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|       11 | 3199 | `	if( pVm->sCodeGen.xErr ){` |
|       11 | 3200 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|        - | 3201 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|        3 | 3202 | `			zGiven,&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|        5 | 3203 | `	}else{` |
|        4 | 3204 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 3205 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|        1 | 3206 | `			zGiven,&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|        - | 3207 | `	}` |
|       11 | 3208 | `	pVm->iExitStatus = 255;` |
|       11 | 3209 | `	pVm->bHaltRequested = 1;` |
|       11 | 3210 | `	return SXERR_ABORT;` |
|        9 | 3211 | `}` |
|        - | 3212 | `/*` |
|        - | 3213 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|        - | 3214 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|        - | 3215 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|        - | 3216 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|        - | 3217 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|        - | 3218 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|        - | 3219 | ` */` |
|       54 | 3220 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|        5 | 3221 | `{` |
|       59 | 3222 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|        - | 3223 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|       59 | 3224 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 | 3225 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|        3 | 3226 | `			return SXRET_OK;` |
|        - | 3227 | `		}` |
|      ! 0 | 3228 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|      ! 0 | 3229 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|      ! 0 | 3230 | `			return SXRET_OK;` |
|        - | 3231 | `		}` |
|      ! 0 | 3232 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3233 | `	}` |
|        - | 3234 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|       57 | 3235 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       12 | 3236 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|       11 | 3237 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|        8 | 3238 | `			return SXRET_OK;` |
|        - | 3239 | `		}` |
|        3 | 3240 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3241 | `	}` |
|        - | 3242 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|       48 | 3243 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 3244 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3245 | `	}` |
|        - | 3246 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|       48 | 3247 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 | 3248 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3249 | `			return SXRET_OK;` |
|        - | 3250 | `		}` |
|      ! 0 | 3251 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3252 | `	}` |
|        - | 3253 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|        - | 3254 | `	 * a real class/interface verified by instanceof. */` |
|       48 | 3255 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        3 | 3256 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|        3 | 3257 | `		if( rcPseudo == 1 ){` |
|      ! 0 | 3258 | `			return SXRET_OK;` |
|        - | 3259 | `		}` |
|        3 | 3260 | `		if( rcPseudo == 0 ){` |
|      ! 0 | 3261 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3262 | `		}` |
|        - | 3263 | `		/* rcPseudo == -1: a real class/interface type. A class constant's` |
|        - | 3264 | `		 * self/parent resolve against the declaring class. */` |
|        - | 3265 | `		{` |
|        3 | 3266 | `			ph7_class *pExpected = 0;` |
|        4 | 3267 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|        1 | 3268 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|        3 | 3269 | `				return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3270 | `			}` |
|        - | 3271 | `		}` |
|      ! 0 | 3272 | `		return SXRET_OK;` |
|        - | 3273 | `	}` |
|        - | 3274 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|        - | 3275 | `	 * implicit widening. Everything else is a type error.` |
|        - | 3276 | `	 *` |
|        - | 3277 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|        - | 3278 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,`` |
|        - | 3279 | `` 	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int` `` |
|        - | 3280 | ``	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)`` |
|        - | 3281 | ``	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so`` |
|        - | 3282 | ``	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal`` |
|        - | 3283 | ``	 * case `const int X = 1.0` is caught earlier, at definition time, by the`` |
|        - | 3284 | `	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal` |
|        - | 3285 | ``	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)`` |
|        - | 3286 | `	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed` |
|        - | 3287 | `	 * residual needs PHL's float-identity/division model, which is out of scope. */` |
|       46 | 3288 | `	if( pValue->iFlags & pAttr->nType ){` |
|       33 | 3289 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       33 | 3290 | `		return SXRET_OK;` |
|        - | 3291 | `	}` |
|       14 | 3292 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|        3 | 3293 | `		PH7_MemObjToReal(pValue);` |
|        3 | 3294 | `		return SXRET_OK;` |
|        - | 3295 | `	}` |
|       11 | 3296 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       32 | 3297 | `}` |
|        - | 3298 | `/*` |
|        - | 3299 | ` * php's CATCHABLE TypeError for a typed property DEFAULT whose computed value` |
|        - | 3300 | ` * does not match the declared type: "Cannot assign <kind> to property` |
|        - | 3301 | ` * C::$p of type T". Same value-kind naming as the constant fatal above.` |
|        - | 3302 | ` * Returns PH7_ABORT or PH7_EXCEPTION (VmThrowBuiltinError's protocol).` |
|        - | 3303 | ` */` |
|       34 | 3304 | `static sxi32 VmDefaultPropertyTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        3 | 3305 | `{` |
|       37 | 3306 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 3307 | `	const char *zGiven;` |
|        - | 3308 | `	char zBuf[128],zType[192];` |
|       54 | 3309 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|       17 | 3310 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|        - | 3311 | `	SyBlob sMsg;` |
|       37 | 3312 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3313 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 | 3314 | `	}else{` |
|       37 | 3315 | `		zGiven = ph7_type_name(pValue);` |
|        - | 3316 | `	}` |
|       37 | 3317 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       37 | 3318 | `	SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|       17 | 3319 | `		zGiven,&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|       37 | 3320 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        3 | 3321 | `}` |
|        - | 3322 | `/*` |
|        - | 3323 | ` * Enforce a typed INSTANCE property's computed DEFAULT value against its` |
|        - | 3324 | ` * declared type at instantiation time. php applies the typed-CONSTANT rule` |
|        - | 3325 | ` * here, not the weak store rule: the only implicit coercion is int -> float` |
|        - | 3326 | `` * widening — `public int $p = "5"` is a TypeError even in weak mode — but`` |
|        - | 3327 | ` * unlike a typed constant the failure is a CATCHABLE TypeError raised when` |
|        - | 3328 | `` * the default is materialized (at `new`), php-exact since PHL evaluates`` |
|        - | 3329 | ` * instance defaults per-instantiation. Matching structure of` |
|        - | 3330 | ` * VmEnforceConstantType above; only the throw differs (catchable, property` |
|        - | 3331 | ` * wording). The whole-real dual-flag leniency applies here too (php rejects` |
|        - | 3332 | `` * `public int $p = FLC` with FLC = 2.0; PHL cannot tell FLC's shape from a`` |
|        - | 3333 | ` * php-int-producing builtin, so it accepts-and-materializes — recorded).` |
|        - | 3334 | ` * Returns SXRET_OK, or PH7_ABORT/PH7_EXCEPTION after throwing.` |
|        - | 3335 | ` */` |
|        - | 3336 | `/*` |
|        - | 3337 | ` * The CHECK core of typed-default enforcement: validate (and possibly coerce` |
|        - | 3338 | ` * in place — int -> float widening, whole-real materialization) a computed` |
|        - | 3339 | ` * DEFAULT value against the property's declared type using the typed-CONSTANT` |
|        - | 3340 | ` * rule. Returns SXRET_OK on accept or SXERR_INVALID on mismatch WITHOUT` |
|        - | 3341 | ` * throwing, so the static-property mount path can defer the failure (php` |
|        - | 3342 | ` * evaluates static defaults lazily — see VM_CLASS_ATTR_TYPE_DEFER) while the` |
|        - | 3343 | ` * instance path throws immediately via the wrapper below.` |
|        - | 3344 | ` */` |
|      628 | 3345 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        5 | 3346 | `{` |
|      633 | 3347 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|      633 | 3348 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       78 | 3349 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|       74 | 3350 | `			return SXRET_OK;` |
|        - | 3351 | `		}` |
|        4 | 3352 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        4 | 3353 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|        3 | 3354 | `			return SXRET_OK;` |
|        - | 3355 | `		}` |
|        3 | 3356 | `		return SXERR_INVALID;` |
|        - | 3357 | `	}` |
|      559 | 3358 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       39 | 3359 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|       30 | 3360 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|       30 | 3361 | `			return SXRET_OK;` |
|        - | 3362 | `		}` |
|      ! 0 | 3363 | `		return SXERR_INVALID;` |
|        - | 3364 | `	}` |
|      533 | 3365 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 3366 | `		return SXERR_INVALID;` |
|        - | 3367 | `	}` |
|      533 | 3368 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 | 3369 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3370 | `			return SXRET_OK;` |
|        - | 3371 | `		}` |
|      ! 0 | 3372 | `		return SXERR_INVALID;` |
|        - | 3373 | `	}` |
|      533 | 3374 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        5 | 3375 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|        5 | 3376 | `		if( rcPseudo == 1 ){` |
|        5 | 3377 | `			return SXRET_OK;` |
|        - | 3378 | `		}` |
|      ! 0 | 3379 | `		if( rcPseudo == 0 ){` |
|      ! 0 | 3380 | `			return SXERR_INVALID;` |
|        - | 3381 | `		}` |
|        - | 3382 | `		{` |
|        - | 3383 | `			/* self/parent in the hint resolve against the declaring class. */` |
|      ! 0 | 3384 | `			ph7_class *pExpected = 0;` |
|      ! 0 | 3385 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|      ! 0 | 3386 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|      ! 0 | 3387 | `				return SXERR_INVALID;` |
|        - | 3388 | `			}` |
|        - | 3389 | `		}` |
|      ! 0 | 3390 | `		return SXRET_OK;` |
|        - | 3391 | `	}` |
|      529 | 3392 | `	if( pValue->iFlags & pAttr->nType ){` |
|      493 | 3393 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|      493 | 3394 | `		return SXRET_OK;` |
|        - | 3395 | `	}` |
|       39 | 3396 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|        6 | 3397 | `		PH7_MemObjToReal(pValue);` |
|        6 | 3398 | `		return SXRET_OK;` |
|        - | 3399 | `	}` |
|       35 | 3400 | `	return SXERR_INVALID;` |
|      319 | 3401 | `}` |
|      560 | 3402 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        5 | 3403 | `{` |
|      565 | 3404 | `	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){` |
|      553 | 3405 | `		return SXRET_OK;` |
|        - | 3406 | `	}` |
|       13 | 3407 | `	return VmDefaultPropertyTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      285 | 3408 | `}` |
|        - | 3409 | `/*` |
|        - | 3410 | ` * Deferred typed-STATIC-default failure (VM_CLASS_ATTR_TYPE_DEFER): scan the` |
|        - | 3411 | ` * class chain for a static typed slot whose mount-time default failed its` |
|        - | 3412 | ` * type check, and throw php's catchable "Cannot assign <kind> to property` |
|        - | 3413 | ` * C::$s of type T" TypeError for the first one found. php evaluates static` |
|        - | 3414 | ` * defaults lazily, so the failure surfaces at the FIRST static-property` |
|        - | 3415 | ` * access (read/write/isset — any property of the class) or instantiation; a` |
|        - | 3416 | ` * never-touched class stays silent, and the throw repeats on every access` |
|        - | 3417 | ` * (the flag is not cleared — php's table materialization keeps failing too).` |
|        - | 3418 | ` * Returns SXRET_OK when nothing is pending (the class flag is only a hint),` |
|        - | 3419 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw.` |
|        - | 3420 | ` */` |
|       26 | 3421 | `static sxi32 VmThrowDeferredStaticType(ph7_vm *pVm,ph7_class *pClass)` |
|        3 | 3422 | `{` |
|        - | 3423 | `	ph7_class *pScan;` |
|       33 | 3424 | `	for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|        - | 3425 | `		SyHashEntry *pEntry;` |
|       29 | 3426 | `		SyHashResetLoopCursor(&pScan->hAttr);` |
|       33 | 3427 | `		while( (pEntry = SyHashGetNextEntry(&pScan->hAttr)) != 0 ){` |
|       29 | 3428 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       26 | 3429 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)) ==` |
|        - | 3430 | `				(PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)` |
|       24 | 3431 | `			 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|       25 | 3432 | `			 && pAttr->nIdx != SXU32_HIGH ){` |
|       25 | 3433 | `				SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       25 | 3434 | `				if( pSlot ){` |
|       25 | 3435 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|       25 | 3436 | `					if( pVmAttr->iState & VM_CLASS_ATTR_TYPE_DEFER ){` |
|       25 | 3437 | `						ph7_value *pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|        - | 3438 | `						ph7_value sNull;` |
|       25 | 3439 | `						if( pValue == 0 ){` |
|      ! 0 | 3440 | `							PH7_MemObjInit(&(*pVm),&sNull);` |
|      ! 0 | 3441 | `							pValue = &sNull;` |
|      ! 0 | 3442 | `						}` |
|       25 | 3443 | `						return VmDefaultPropertyTypeError(&(*pVm),PH7_VmAttrOwner(pVmAttr),pAttr,pValue);` |
|        - | 3444 | `					}` |
|      ! 0 | 3445 | `				}` |
|      ! 0 | 3446 | `			}` |
|        1 | 3447 | `		}` |
|        3 | 3448 | `	}` |
|        5 | 3449 | `	return SXRET_OK;` |
|       16 | 3450 | `}` |
|        - | 3451 | `/*` |
|        - | 3452 | ` * TRUE when [pClass] (or any of its bases) still owes its static table a` |
|        - | 3453 | ` * materialization: an initializer that threw at mount and was deferred` |
|        - | 3454 | ` * (PH7_CLASS_ATTR_STATIC_DEFER) or a typed default that failed its check` |
|        - | 3455 | ` * (VM_CLASS_ATTR_TYPE_DEFER). The gate the access sites test before paying for` |
|        - | 3456 | ` * PH7_VmMaterializeClassStatics. The BASES are walked here rather than relying` |
|        - | 3457 | ` * on the flag being copied down at mount, because classes mount in hash order:` |
|        - | 3458 | ` * a subclass can be mounted before the base whose default failed.` |
|        - | 3459 | ` */` |
|  2242290 | 3460 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass)` |
|        5 | 3461 | `{` |
|  4495745 | 3462 | `	while( pClass ){` |
|  2253537 | 3463 | `		if( pClass->iFlags & PH7_CLASS_STATIC_DEFER ){` |
|       85 | 3464 | `			return 1;` |
|        - | 3465 | `		}` |
|  2253455 | 3466 | `		pClass = pClass->pBase;` |
|        5 | 3467 | `	}` |
|  2242213 | 3468 | `	return 0;` |
|  1121129 | 3469 | `}` |
|        - | 3470 | `/*` |
|        - | 3471 | ` * Re-run the initializers of the static properties whose evaluation was` |
|        - | 3472 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|        - | 3473 | ` *` |
|        - | 3474 | ` * php builds a class's static table on first use, evaluating each slot's` |
|        - | 3475 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|        - | 3476 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|        - | 3477 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|        - | 3478 | ` * the access: a constant define()d after the class declaration resolves.` |
|        - | 3479 | ` *` |
|        - | 3480 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|        - | 3481 | ` * the base's broken default, not its own), then declaration order within a` |
|        - | 3482 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|        - | 3483 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|        - | 3484 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|        - | 3485 | ` * re-raises on every access too.` |
|        - | 3486 | ` */` |
|       86 | 3487 | `static sxi32 VmCollectDeferredStaticDefaults(ph7_class *pClass,SySet *pOut,int *pbLeft)` |
|        3 | 3488 | `{` |
|        - | 3489 | `	SyHashEntry *pEntry;` |
|        - | 3490 | `	sxi32 rc;` |
|       89 | 3491 | `	if( pClass->pBase ){` |
|        6 | 3492 | `		rc = VmCollectDeferredStaticDefaults(pClass->pBase,pOut,pbLeft);` |
|        6 | 3493 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3494 | `			return rc;` |
|        - | 3495 | `		}` |
|        2 | 3496 | `	}` |
|       89 | 3497 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      209 | 3498 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      123 | 3499 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      123 | 3500 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|        - | 3501 | `			/* Not pending. An inherited slot the base pass already collected` |
|        - | 3502 | `			 * lands here too (a subclass's hAttr shares the base's attribute);` |
|        - | 3503 | `			 * the evaluation loop re-tests the flag, so a duplicate is a no-op. */` |
|       61 | 3504 | `			continue;` |
|        - | 3505 | `		}` |
|       65 | 3506 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - | 3507 | `			/* Pending but already RUNNING: an initializer that reads a static` |
|        - | 3508 | `			 * property re-enters this walk through OP_MEMBER, and re-running the` |
|        - | 3509 | `			 * initializer it is inside would not terminate. The in-flight slot` |
|        - | 3510 | `			 * reads as it stands, like the constant path's cycle guard — and the` |
|        - | 3511 | `			 * class keeps its hint flag so a later access retries. */` |
|      ! 0 | 3512 | `			*pbLeft = 1;` |
|      ! 0 | 3513 | `			continue;` |
|        - | 3514 | `		}` |
|       65 | 3515 | `		rc = SySetPut(pOut,(const void *)&pAttr);` |
|       65 | 3516 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3517 | `			return rc;` |
|        - | 3518 | `		}` |
|        3 | 3519 | `	}` |
|       89 | 3520 | `	return SXRET_OK;` |
|       46 | 3521 | `}` |
|        - | 3522 | `/*` |
|        - | 3523 | ` * Re-run the initializers of the static properties whose evaluation was` |
|        - | 3524 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|        - | 3525 | ` *` |
|        - | 3526 | ` * php builds a class's static table on first use, evaluating each slot's` |
|        - | 3527 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|        - | 3528 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|        - | 3529 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|        - | 3530 | ` * the access: a constant define()d after the class declaration resolves.` |
|        - | 3531 | ` *` |
|        - | 3532 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|        - | 3533 | ` * the base's broken default, not its own), then declaration order within a` |
|        - | 3534 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|        - | 3535 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|        - | 3536 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|        - | 3537 | ` * re-raises on every access too.` |
|        - | 3538 | ` *` |
|        - | 3539 | ` * The pending slots are COLLECTED before any of them runs: an initializer is` |
|        - | 3540 | ` * user-visible execution that can re-enter this walk, and SyHash carries a` |
|        - | 3541 | ` * single shared loop cursor, so evaluating mid-walk would let the nested walk` |
|        - | 3542 | ` * cut the outer one short.` |
|        - | 3543 | ` */` |
|       82 | 3544 | `static sxi32 VmEvalDeferredStaticDefaults(ph7_vm *pVm,ph7_class *pClass,int *pbLeft)` |
|        3 | 3545 | `{` |
|        - | 3546 | `	SySet aPending; /* ph7_class_attr * , php's static-table order */` |
|        - | 3547 | `	ph7_class_attr **apPending;` |
|        - | 3548 | `	sxu32 n,nUsed;` |
|        - | 3549 | `	sxi32 rc;` |
|       85 | 3550 | `	SySetInit(&aPending,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|       85 | 3551 | `	rc = VmCollectDeferredStaticDefaults(pClass,&aPending,pbLeft);` |
|       85 | 3552 | `	apPending = (ph7_class_attr **)SySetBasePtr(&aPending);` |
|       85 | 3553 | `	nUsed = SySetUsed(&aPending);` |
|       89 | 3554 | `	for( n = 0 ; rc == SXRET_OK && n < nUsed ; ++n ){` |
|       63 | 3555 | `		ph7_class_attr *pAttr = apPending[n];` |
|       63 | 3556 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 3557 | `		ph7_class *pSaveCtx;` |
|        - | 3558 | `		void *pSaveFrame;` |
|        - | 3559 | `		sxu32 nSaveLazyLine;` |
|        - | 3560 | `		sxi32 nSaveLazyDepth;` |
|        - | 3561 | `		ph7_value *pMemObj;` |
|        - | 3562 | `		sxi32 rcExec;` |
|       63 | 3563 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|      ! 0 | 3564 | `			continue; /* the base pass already ran this shared slot */` |
|        - | 3565 | `		}` |
|       63 | 3566 | `		pMemObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|       63 | 3567 | `		if( pMemObj == 0 ){` |
|      ! 0 | 3568 | `			continue;` |
|        - | 3569 | `		}` |
|       63 | 3570 | `		pSaveCtx = pVm->pConstEvalClass;` |
|       63 | 3571 | `		pSaveFrame = pVm->pConstEvalFrame;` |
|       63 | 3572 | `		pVm->pConstEvalClass = pOwner;` |
|        - | 3573 | `		/* Unlike the mount pass, this runs at an arbitrary point in execution —` |
|        - | 3574 | `		 * possibly inside a METHOD of another class. Mark the frame current at` |
|        - | 3575 | `		 * eval start so self::/parent:: in the initializer resolve against the` |
|        - | 3576 | `		 * DECLARING class rather than that method's, exactly as the class-constant` |
|        - | 3577 | `		 * on-demand path does (VmLocalExec pushes no frame of its own). */` |
|       63 | 3578 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 3579 | `		/* The initializer runs HERE, at the access: that is the line php reports` |
|        - | 3580 | `		 * for a throw out of its own bytecode (see PH7_VmStampThrowableSite). */` |
|       63 | 3581 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|       63 | 3582 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|       63 | 3583 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|       63 | 3584 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|       63 | 3585 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, as at mount */` |
|       63 | 3586 | `		pVm->nConstEvalDepth++;` |
|       63 | 3587 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|       63 | 3588 | `		pVm->nConstEvalDepth--;` |
|       63 | 3589 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|       63 | 3590 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|       63 | 3591 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       63 | 3592 | `		pVm->pConstEvalClass = pSaveCtx;` |
|       63 | 3593 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|       63 | 3594 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 3595 | `			/* Raised at the access site, where it belongs: hand the status to the` |
|        - | 3596 | `			 * caller to route (a catch here is the user's own). */` |
|       59 | 3597 | `			rc = rcExec;` |
|       59 | 3598 | `			break;` |
|        - | 3599 | `		}` |
|        5 | 3600 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER;` |
|        5 | 3601 | `		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|        - | 3602 | `			/* The initializer named a self-referencing constant. Like the mount` |
|        - | 3603 | `			 * path, the innermost evaluation only RECORDS it; raise it here, at` |
|        - | 3604 | `			 * the access, where a catch can see it. */` |
|      ! 0 | 3605 | `			rc = VmConstCycleThrow(&(*pVm));` |
|      ! 0 | 3606 | `			break;` |
|        - | 3607 | `		}` |
|        4 | 3608 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|        3 | 3609 | `		 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        - | 3610 | `			/* Now that a value exists, apply the typed-default rule the mount pass` |
|        - | 3611 | `			 * had to skip. Flag the slot as well so every later access re-throws` |
|        - | 3612 | `			 * through the deferred-type scan, as php's failing materialization does. */` |
|      ! 0 | 3613 | `			SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|      ! 0 | 3614 | `			if( pSlot && VmCheckTypedDefault(&(*pVm),pOwner,pAttr,pMemObj) != SXRET_OK ){` |
|      ! 0 | 3615 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      ! 0 | 3616 | `				pVmAttr->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|      ! 0 | 3617 | `				rc = VmDefaultPropertyTypeError(&(*pVm),PH7_VmAttrOwner(pVmAttr),pAttr,pMemObj);` |
|      ! 0 | 3618 | `				break;` |
|        - | 3619 | `			}` |
|      ! 0 | 3620 | `		}` |
|        3 | 3621 | `	}` |
|       85 | 3622 | `	if( rc != SXRET_OK ){` |
|       59 | 3623 | `		*pbLeft = 1; /* whatever is still flagged stays pending for the next access */` |
|       28 | 3624 | `	}` |
|       85 | 3625 | `	SySetRelease(&aPending);` |
|       85 | 3626 | `	return rc;` |
|        3 | 3627 | `}` |
|        - | 3628 | `/*` |
|        - | 3629 | ` * Materialize [pClass]'s static table, php's way: evaluate whatever the mount` |
|        - | 3630 | ` * pass deferred, then raise any typed-default failure. Called by the sites php` |
|        - | 3631 | ` * materializes at — the first static-PROPERTY access (read, write, isset; a` |
|        - | 3632 | ` * class CONSTANT or a static METHOD CALL does not materialize, php-exact) and` |
|        - | 3633 | ` * instantiation. Returns SXRET_OK when the table is (or already was) whole,` |
|        - | 3634 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw for the caller to route.` |
|        - | 3635 | ` */` |
|       82 | 3636 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass)` |
|        3 | 3637 | `{` |
|       85 | 3638 | `	int bLeft = 0;` |
|       85 | 3639 | `	sxi32 rc = VmEvalDeferredStaticDefaults(&(*pVm),pClass,&bLeft);` |
|       85 | 3640 | `	if( rc == SXRET_OK ){` |
|       29 | 3641 | `		rc = VmThrowDeferredStaticType(&(*pVm),pClass);` |
|       13 | 3642 | `	}` |
|       85 | 3643 | `	if( rc == SXRET_OK && !bLeft ){` |
|        - | 3644 | `		/* The table is whole and nothing failed: retire the hint on the whole` |
|        - | 3645 | `		 * chain, so the ordinary static accesses that follow stop paying for the` |
|        - | 3646 | `		 * scan. A re-mount (VM reset) re-arms it, and a failure above leaves it` |
|        - | 3647 | `		 * set — php's materialization keeps failing too. */` |
|        - | 3648 | `		ph7_class *pScan;` |
|        9 | 3649 | `		for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|        5 | 3650 | `			pScan->iFlags &= ~PH7_CLASS_STATIC_DEFER;` |
|        3 | 3651 | `		}` |
|        2 | 3652 | `	}` |
|       85 | 3653 | `	return rc;` |
|        3 | 3654 | `}` |
|        - | 3655 |  |
|        - | 3656 | `/*` |
|        - | 3657 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - | 3658 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - | 3659 | ` * information.` |
|        - | 3660 | ` * ------------------------------------` |
|        - | 3661 | ` * Simple boring wrapper function.` |
|        - | 3662 | ` * ------------------------------------` |
|        - | 3663 | ` */` |
|        - | 3664 | `/*` |
|        - | 3665 | ` * php's RUNTIME fatal (E_ERROR), in the shape php prints it.` |
|        - | 3666 | ` *` |
|        - | 3667 | ``  * PHL's COMPILE-time fatal has been php's for a long time -- the `PHP Fatal error:  ` `` |
|        - | 3668 | `` * label, the sentence, ` in FILE on line N`, then the `Stack trace:` block and its`` |
|        - | 3669 | `` * `#N {main}` terminator (PH7_GenCompileError). Its RUNTIME one was not: the ordinary`` |
|        - | 3670 | ` * diagnostic printer behind VmErrorFormat(PH7_CTX_ERR, ...) labels the same event` |
|        - | 3671 | `` * `PHP Error:  ` and stops at the location, so a refusal php reports with four lines`` |
|        - | 3672 | ` * and a frame chain came out here as one line and no chain at all.` |
|        - | 3673 | ` *` |
|        - | 3674 | ` * This is that renderer, and it borrows both halves rather than writing either: the` |
|        - | 3675 | ` * label the compile-time path prints, and the trace builder the uncaught-exception` |
|        - | 3676 | ` * path walks the frame chain with (PH7_FATAL_TRACE_RUNTIME, the kind that KEEPS the` |
|        - | 3677 | ` * include/require that loaded the unit -- a runtime refusal happens with that` |
|        - | 3678 | ` * activation live, where a compiler's does not).` |
|        - | 3679 | ` *` |
|        - | 3680 | ` * It is deliberately NOT the ordinary diagnostic path: php never runs a` |
|        - | 3681 | `` * set_error_handler() for an E_ERROR, so no handler is consulted, and `@` does not`` |
|        - | 3682 | ` * suppress one either. error_get_last() DOES see it, which a` |
|        - | 3683 | ` * register_shutdown_function() callback can still read. Requesting the halt and the` |
|        - | 3684 | ` * exit status is the caller's, as it is at every other fatal site.` |
|        - | 3685 | ` */` |
|        - | 3686 | `/*` |
|        - | 3687 | ` * Write one copy of a COMPILE-time diagnostic.` |
|        - | 3688 | ` *` |
|        - | 3689 | ` * A compile-time refusal can happen before the host has wired the VM's streams` |
|        - | 3690 | ``  * at all -- the main script's own compile CREATES the VM, so a `phl bad.php` `` |
|        - | 3691 | `` * diagnostic is raised with `sVmConsumer`/`sVmErrConsumer` still empty. The`` |
|        - | 3692 | ` * engine-level compile-error consumer (PH7_CONFIG_ERR_OUTPUT) is the one channel` |
|        - | 3693 | ` * that always exists, so it is the fallback for either copy.` |
|        - | 3694 | ` *` |
|        - | 3695 | `` * The terminator is `\n` on every platform, which is what this path has always`` |
|        - | 3696 | `` * written -- VmWriteDiagnostic's `\r\n` belongs to the runtime one and is not`` |
|        - | 3697 | ` * borrowed here.` |
|        - | 3698 | ` */` |
|     1350 | 3699 | `static sxi32 VmWriteCompileCopy(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bDisplay)` |
|        4 | 3700 | `{` |
|     1354 | 3701 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|     1354 | 3702 | `	if( pCons && pCons->xConsumer ){` |
|       40 | 3703 | `		sxi32 rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|       40 | 3704 | `		if( bDisplay ){` |
|        3 | 3705 | `			VmTrackOutput(pVm,SyBlobLength(pMsg));` |
|        1 | 3706 | `		}` |
|       40 | 3707 | `		return rc;` |
|        - | 3708 | `	}` |
|        - | 3709 | `	/* No VM stream yet -- the main script's own compile. The DISPLAY copy is` |
|        - | 3710 | `	 * program output and belongs on the host's output stream; falling through to` |
|        - | 3711 | `	 * the compile-error consumer below put php's stdout text on stderr, so` |
|        - | 3712 | ``	 * `-d display_errors=1` moved nothing. */`` |
|     1318 | 3713 | `	if( bDisplay && pVm->pEngine && pVm->pEngine->xConf.xOut ){` |
|       34 | 3714 | `		return pVm->pEngine->xConf.xOut(SyBlobData(pMsg),SyBlobLength(pMsg),` |
|       22 | 3715 | `			pVm->pEngine->xConf.pOutData);` |
|        - | 3716 | `	}` |
|     1296 | 3717 | `	if( pVm->pEngine && pVm->pEngine->xConf.xErr ){` |
|     1942 | 3718 | `		return pVm->pEngine->xConf.xErr(SyBlobData(pMsg),SyBlobLength(pMsg),` |
|     1292 | 3719 | `			pVm->pEngine->xConf.pErrData);` |
|        - | 3720 | `	}` |
|      ! 0 | 3721 | `	return SXRET_OK;` |
|      679 | 3722 | `}` |
|        - | 3723 | `/*` |
|        - | 3724 | ` * A COMPILE-time diagnostic's two copies, the same pair and the same two ini` |
|        - | 3725 | ` * gates a runtime one gets (VmEmitFatalReport). The compiler used to write ONE` |
|        - | 3726 | ` * copy, in the log shape, to the engine's compile-error consumer -- which the CLI` |
|        - | 3727 | `` * pointed at STDOUT, so `phl -l bad.php` and `phl bad.php` both put php's stderr`` |
|        - | 3728 | ` * text into program output, where anything capturing stdout reads it as data.` |
|        - | 3729 | ` *` |
|        - | 3730 | ` * The caller hands over the LABEL and a header-less BODY.` |
|        - | 3731 | ` */` |
|        - | 3732 | `/*` |
|        - | 3733 | ` * The program-output stream to give a compile diagnostic's DISPLAY copy, or NULL` |
|        - | 3734 | ` * when the host has not wired one yet.` |
|        - | 3735 | ` *` |
|        - | 3736 | ` * PH7_VmMakeReady installs PH7_VmBlobConsumer as the VM's output consumer, and` |
|        - | 3737 | ` * MakeReady runs INSIDE ph7_compile_file -- before the host has replaced it. A` |
|        - | 3738 | ` * diagnostic written there lands in an internal blob nobody reads, which is a` |
|        - | 3739 | ` * silent LOSS, not a fallback: a typed class constant's mount-time fatal is` |
|        - | 3740 | ` * raised in exactly that window and disappeared entirely. An output buffer` |
|        - | 3741 | ` * (ob_start) also swaps this consumer and is NOT that case -- php buffers its` |
|        - | 3742 | ` * display copy too -- so the test is the blob consumer by identity, not merely` |
|        - | 3743 | ` * "something is installed".` |
|        - | 3744 | ` */` |
|     1316 | 3745 | `static ph7_output_consumer * VmCompileDisplaySink(ph7_vm *pVm)` |
|        4 | 3746 | `{` |
|     1320 | 3747 | `	if( pVm->sVmConsumer.xConsumer && pVm->sVmConsumer.xConsumer != PH7_VmBlobConsumer ){` |
|        3 | 3748 | `		return &pVm->sVmConsumer;` |
|        - | 3749 | `	}` |
|     1318 | 3750 | `	return 0;` |
|      662 | 3751 | `}` |
|     1374 | 3752 | `PH7_PRIVATE sxi32 PH7_VmEmitCompileDiagnostic(ph7_vm *pVm,sxi32 iErr,const char *zLabel,` |
|        - | 3753 | `	const char *zBody,sxu32 nBody,const char *zBare,sxu32 nBare,sxu32 nLine)` |
|        5 | 3754 | `{` |
|        - | 3755 | `	SyBlob sCopy,sHold;` |
|     1379 | 3756 | `	sxi32 rc = SXRET_OK;` |
|     1379 | 3757 | `	if( pVm == 0 ){` |
|      ! 0 | 3758 | `		return SXRET_OK;` |
|        - | 3759 | `	}` |
|        - | 3760 | `	/*` |
|        - | 3761 | `	 * A compile diagnostic owes the same three things a runtime one does` |
|        - | 3762 | `	 * (PH7_VmThrowError): the user handler, error_get_last(), and the` |
|        - | 3763 | `	 * error_reporting() mask. It used to owe NONE of them, so a library that` |
|        - | 3764 | ``	 * silences a probe -- `error_reporting(0); class_exists('X');` , which is`` |
|        - | 3765 | `	 * how a php-console-style optional dependency is tested and how monolog's` |
|        - | 3766 | `	 * suite loads one -- got the compiler's warning printed anyway.` |
|        - | 3767 | `	 *` |
|        - | 3768 | `	 * Who sees what is php's split. E_COMPILE_WARNING/E_COMPILE_ERROR/E_PARSE` |
|        - | 3769 | ``	 * are on `set_error_handler`'s own exclusion list, so those go straight to`` |
|        - | 3770 | ``	 * default processing; a compile-time E_WARNING (php raises `"continue"`` |
|        - | 3771 | ``	 * targeting switch` and the magic-visibility rules at that level) reaches`` |
|        - | 3772 | `	 * the handler like any other. error_get_last() records what reached` |
|        - | 3773 | `	 * DEFAULT processing, masked or not -- so it is recorded here even when` |
|        - | 3774 | `	 * the mask hides the print, and NOT when a handler claimed it.` |
|        - | 3775 | `	 *` |
|        - | 3776 | `	 * Both texts are the CALLER's buffer -- the code generator's one-message` |
|        - | 3777 | ``	 * store -- and a user handler can compile (an `include`, an `eval`), which`` |
|        - | 3778 | `	 * resets and reallocates exactly that buffer. Everything below reads the` |
|        - | 3779 | `	 * copy instead; only the copy's lifetime is this function's.` |
|        - | 3780 | `	 */` |
|     1379 | 3781 | `	SyBlobInit(&sHold,&pVm->sAllocator);` |
|     1379 | 3782 | `	SyBlobAppend(&sHold,zBody,nBody);` |
|     1379 | 3783 | `	zBody = (const char *)SyBlobData(&sHold);` |
|     1379 | 3784 | `	zBare = zBody;` |
|     1379 | 3785 | `	if( nBare > nBody ){` |
|      ! 0 | 3786 | `		nBare = nBody;` |
|      ! 0 | 3787 | `	}` |
|        - | 3788 | `	{` |
|        - | 3789 | `		/* The handler and error_get_last() take the BARE sentence and the line` |
|        - | 3790 | `		 * as their own fields; only the printed copies carry php's` |
|        - | 3791 | ``		 * ` in FILE on line N` tail, which the caller has already appended to`` |
|        - | 3792 | `		 * zBody. */` |
|     1379 | 3793 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     1374 | 3794 | `		if( iErr == 2 /* E_WARNING */ \|\| iErr == 8 /* E_NOTICE */` |
|     1321 | 3795 | `		 \|\| iErr == 8192 /* E_DEPRECATED */ ){` |
|      101 | 3796 | `			if( !VmInvokeErrorHandler(pVm,iErr,zBare,(sxi32)nBare,pFile,(sxi32)nLine) ){` |
|       30 | 3797 | `				SyBlobRelease(&sHold);` |
|       30 | 3798 | `				return SXRET_OK;` |
|        - | 3799 | `			}` |
|        - | 3800 | `			/* The handler may have moved the file stack (its own include) and it` |
|        - | 3801 | `			 * may have written the copy's backing store's neighbours; re-read` |
|        - | 3802 | `			 * both before they are used again. */` |
|       72 | 3803 | `			zBody = (const char *)SyBlobData(&sHold);` |
|       72 | 3804 | `			zBare = zBody;` |
|       72 | 3805 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       34 | 3806 | `		}` |
|     1350 | 3807 | `		VmRecordLastError(&(*pVm),iErr,zBare,nBare,pFile,nLine);` |
|        - | 3808 | `	}` |
|        - | 3809 | `	/*` |
|        - | 3810 | `	 * A host that has said nothing about the level leaves iErrMask at zero, which` |
|        - | 3811 | ``	 * must not be read as `error_reporting(0)`. bErrMaskSet says whether anybody`` |
|        - | 3812 | `	 * has spoken; until somebody has, the pre-mask behaviour (report it) stands.` |
|        - | 3813 | `	 * The CLI speaks before it compiles -- PH7_CONFIG_ERR_REPORT and the -d/-c` |
|        - | 3814 | `	 * queue are engine-level, so the main script's OWN compile is already gated --` |
|        - | 3815 | `	 * and an embedder that configures nothing still gets the fallback.` |
|        - | 3816 | `	 */` |
|     1350 | 3817 | `	if( pVm->bErrMaskSet && !VmErrReportWants(pVm,iErr) ){` |
|       17 | 3818 | `		SyBlobRelease(&sHold);` |
|       17 | 3819 | `		return SXRET_OK;` |
|        - | 3820 | `	}` |
|     1334 | 3821 | `	if( pVm->bLogErrors ){` |
|     1330 | 3822 | `		SyBlobInit(&sCopy,&pVm->sAllocator);` |
|     1330 | 3823 | `		SyBlobFormat(&sCopy,"PHP %s:  ",zLabel);` |
|     1330 | 3824 | `		SyBlobAppend(&sCopy,zBody,nBody);` |
|        - | 3825 | `		/* Same order VmErrConsumer takes at run time: the error stream, then the` |
|        - | 3826 | `		 * output one for an embedder that wired only that, then the engine's. */` |
|        - | 3827 | ``		/* A compile diagnostic owes the `error_log` destination the same copy a`` |
|        - | 3828 | `		 * runtime one does: a bootstrap that sets the directive from php.ini has` |
|        - | 3829 | `		 * it in hand before the unit compiles. */` |
|     1330 | 3830 | `		if( PH7_VmErrorLogToFile(pVm,(const char *)SyBlobData(&sCopy),SyBlobLength(&sCopy)) ){` |
|      ! 0 | 3831 | `			rc = SXRET_OK;` |
|      ! 0 | 3832 | `		}else{` |
|     1993 | 3833 | `			rc = VmWriteCompileCopy(&(*pVm),` |
|     1326 | 3834 | `				pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : VmCompileDisplaySink(pVm),` |
|        - | 3835 | `				&sCopy,0);` |
|        - | 3836 | `		}` |
|     1330 | 3837 | `		SyBlobRelease(&sCopy);` |
|      663 | 3838 | `	}` |
|     1334 | 3839 | `	if( pVm->iDisplayErrors != PH7_DISPLAY_ERRORS_OFF ){` |
|       25 | 3840 | `		int bErrStream = pVm->iDisplayErrors == PH7_DISPLAY_ERRORS_STDERR;` |
|        - | 3841 | `		sxi32 rc2;` |
|       25 | 3842 | `		SyBlobInit(&sCopy,&pVm->sAllocator);` |
|       25 | 3843 | `		if( !bErrStream ){` |
|        - | 3844 | `			/* php's text-mode display copy is prefixed with a blank line */` |
|       25 | 3845 | `			SyBlobAppend(&sCopy,"\n",sizeof(char));` |
|       12 | 3846 | `		}` |
|       25 | 3847 | `		SyBlobFormat(&sCopy,"%s: ",zLabel);` |
|       25 | 3848 | `		SyBlobAppend(&sCopy,zBody,nBody);` |
|        - | 3849 | ``		/* `display_errors=stderr` sends the display copy down the SAME channel the`` |
|        - | 3850 | `		 * log copy takes, and it is not program output there: no output tracking. */` |
|       49 | 3851 | `		rc2 = VmWriteCompileCopy(&(*pVm),` |
|       12 | 3852 | `			bErrStream` |
|      ! 0 | 3853 | `				? (pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : (ph7_output_consumer *)0)` |
|       24 | 3854 | `				: VmCompileDisplaySink(pVm),` |
|       12 | 3855 | `			&sCopy,!bErrStream);` |
|       25 | 3856 | `		SyBlobRelease(&sCopy);` |
|       25 | 3857 | `		if( rc == SXRET_OK ){` |
|       25 | 3858 | `			rc = rc2;` |
|       12 | 3859 | `		}` |
|       12 | 3860 | `	}` |
|     1334 | 3861 | `	SyBlobRelease(&sHold);` |
|     1334 | 3862 | `	return rc;` |
|      692 | 3863 | `}` |
|        8 | 3864 | `PH7_PRIVATE sxi32 PH7_VmFatalError(ph7_vm *pVm,const char *zFormat,...)` |
|        3 | 3865 | `{` |
|        - | 3866 | `	SyBlob sMsg,sOut;` |
|        - | 3867 | `	SyString *pFile;` |
|        - | 3868 | `	sxu32 nLine;` |
|        - | 3869 | `	va_list ap;` |
|       11 | 3870 | `	if( pVm->nSpeculative > 0 ){` |
|        - | 3871 | `		/* See PH7_VmThrowError: nothing a speculative evaluation raises is observable. */` |
|      ! 0 | 3872 | `		pVm->nSpecDiag++;` |
|      ! 0 | 3873 | `		return SXRET_OK;` |
|        - | 3874 | `	}` |
|       11 | 3875 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       11 | 3876 | `	nLine = VmDiagnosticWhere(&(*pVm),&pFile);` |
|       11 | 3877 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       11 | 3878 | `	va_start(ap,zFormat);` |
|       11 | 3879 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       11 | 3880 | `	va_end(ap);` |
|       15 | 3881 | `	VmRecordLastError(&(*pVm),1 /* E_ERROR */,(const char *)SyBlobData(&sMsg),` |
|        4 | 3882 | `		SyBlobLength(&sMsg),pFile,nLine);` |
|       11 | 3883 | `	if( pVm->bErrReport && (pVm->iErrMask & 1) != 0 ){` |
|       11 | 3884 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       11 | 3885 | `		SyBlobAppend(&sOut,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       11 | 3886 | `		VmDiagnosticLocation(&sOut,pFile,nLine);` |
|       11 | 3887 | `		PH7_GenAppendFatalTrace(&(*pVm),&sOut,PH7_FATAL_TRACE_RUNTIME);` |
|       15 | 3888 | `		VmEmitFatalReport(&(*pVm),"Fatal error",` |
|        8 | 3889 | `			(const char *)SyBlobData(&sOut),SyBlobLength(&sOut));` |
|       11 | 3890 | `		SyBlobRelease(&sOut);` |
|        4 | 3891 | `	}` |
|       11 | 3892 | `	SyBlobRelease(&sMsg);` |
|       11 | 3893 | `	return SXRET_OK;` |
|        7 | 3894 | `}` |
|     2600 | 3895 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        5 | 3896 | `{` |
|        - | 3897 | `	va_list ap;` |
|        - | 3898 | `	sxi32 rc;` |
|     2605 | 3899 | `	va_start(ap,zFormat);` |
|     2605 | 3900 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|     2605 | 3901 | `	va_end(ap);` |
|     2605 | 3902 | `	return rc;` |
|        5 | 3903 | `}` |
|        - | 3904 | `/*` |
|        - | 3905 | ` * php prefixes an argument diagnostic with the class the callee belongs to: the` |
|        - | 3906 | ` * owner the caller already knows for a METHOD, and for a CLOSURE the class it was` |
|        - | 3907 | `` * WRITTEN inside -- `C::{closure:C::m():5}`, which php reads off the function's own`` |
|        - | 3908 | `` * scope and PHL records at compile time. Appends `Name::` and answers 1 when there`` |
|        - | 3909 | ` * is one.` |
|        - | 3910 | ` */` |
|        - | 3911 | `/*` |
|        - | 3912 | ` * The owner NAME an argument diagnostic prefixes the callee with, in FULL (an` |
|        - | 3913 | ` * anonymous class's carries its NUL; the two callers below differ over what they` |
|        - | 3914 | ` * do with it). NULL when the callee has no owner.` |
|        - | 3915 | ` */` |
|     4570 | 3916 | `static SyString * VmArgOwnerName(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee)` |
|        5 | 3917 | `{` |
|     4575 | 3918 | `	SyString *pName = 0;` |
|     4575 | 3919 | `	if( pOwnerClass ){` |
|       89 | 3920 | `		pName = &pOwnerClass->sName;` |
|     4533 | 3921 | `	}else if( pCallee ){` |
|        - | 3922 | ``		/* A closure's scope is REBINDABLE (`Closure::bind($c, null, B::class)`), and`` |
|        - | 3923 | `		 * php reports the scope it is running under -- which the frame records. The` |
|        - | 3924 | `		 * declared one is the answer when there is no frame of the callee's to ask,` |
|        - | 3925 | `		 * or when nothing rebound it. */` |
|     4491 | 3926 | `		VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     4491 | 3927 | `		if( pFrame && pFrame->pUserData == (void *)pCallee && pFrame->pBoundScope ){` |
|        6 | 3928 | `			pName = &pFrame->pBoundScope->sName;` |
|     4489 | 3929 | `		}else if( SyStringLength(&pCallee->sClosureScope) > 0 ){` |
|       11 | 3930 | `			pName = &pCallee->sClosureScope;` |
|        5 | 3931 | `		}` |
|     2243 | 3932 | `	}` |
|     4575 | 3933 | `	return pName;` |
|        5 | 3934 | `}` |
|        - | 3935 | ``/* The name plus php's `::`, cut at the NUL an anonymous class carries -- this is the`` |
|        - | 3936 | ``  * spelling of the family that prints the class and the method as SEPARATE `%s` `` |
|        - | 3937 | `` * (`Too few arguments to function class@anonymous::need()`). */`` |
|       62 | 3938 | `static int VmArgOwnerPrefix(ph7_vm *pVm,SyBlob *pOut,ph7_class *pOwnerClass,ph7_vm_func *pCallee)` |
|        4 | 3939 | `{` |
|       66 | 3940 | `	SyString *pName = VmArgOwnerName(&(*pVm),pOwnerClass,pCallee);` |
|        - | 3941 | `	sxu32 nPos;` |
|       66 | 3942 | `	if( pName == 0 ){` |
|       50 | 3943 | `		return 0;` |
|        - | 3944 | `	}` |
|       17 | 3945 | `	if( SyByteFind(pName->zString,pName->nByte,0,&nPos) != SXRET_OK ){` |
|       15 | 3946 | `		nPos = pName->nByte;` |
|        7 | 3947 | `	}` |
|       17 | 3948 | `	SyBlobAppend(pOut,pName->zString,nPos);` |
|       17 | 3949 | `	SyBlobAppend(pOut,"::",sizeof("::")-1);` |
|       17 | 3950 | `	return 1;` |
|       35 | 3951 | `}` |
|        - | 3952 | `/*` |
|        - | 3953 | `` * php's name for the callee in the `Argument #N ...` family: `Class::method`, or the`` |
|        - | 3954 | ` * bare function/closure name when there is no owner class. php builds it as ONE` |
|        - | 3955 | `` * string (get_active_function_or_method_name()) and then prints it with `%s`, so an`` |
|        - | 3956 | `` * ANONYMOUS owner's NUL cuts the `::method` off with it -- the message php really`` |
|        - | 3957 | `` * prints is `class@anonymous(): Argument #1 ($i) must be of type int, string given`,`` |
|        - | 3958 | ` * and it says exactly that for every method of that class, __invoke and a closure` |
|        - | 3959 | ` * declared in one of its methods included.` |
|        - | 3960 | ` *` |
|        - | 3961 | ` * Reproduced rather than tidied: it is what php's output says, and a library's` |
|        - | 3962 | ` * expected output has it. The neighbouring diagnostics that print the class and the` |
|        - | 3963 | `` * method as SEPARATE `%s` keep both halves -- `Too few arguments to function`` |
|        - | 3964 | `` * class@anonymous::need()`, `class@anonymous::ret(): Return value must be of type` --`` |
|        - | 3965 | ` * which is why this is one family's helper and not a rule about class names.` |
|        - | 3966 | ` */` |
|     4508 | 3967 | `static void VmArgFuncLabel(ph7_vm *pVm,SyBlob *pOut,ph7_class *pOwnerClass,` |
|        - | 3968 | `	ph7_vm_func *pCallee,const char *zShow,int nShow)` |
|        5 | 3969 | `{` |
|     4513 | 3970 | `	SyString *pOwner = VmArgOwnerName(&(*pVm),pOwnerClass,pCallee);` |
|        - | 3971 | `	sxu32 nPos;` |
|     4513 | 3972 | `	if( pOwner && SyByteFind(pOwner->zString,pOwner->nByte,0,&nPos) == SXRET_OK ){` |
|        - | 3973 | `` 		/* Anonymous owner: php's `%s` stops inside the class name, so the `::method` `` |
|        - | 3974 | `		 * it assembled behind the NUL is never printed. */` |
|       11 | 3975 | `		SyBlobAppend(pOut,pOwner->zString,nPos);` |
|       11 | 3976 | `		return;` |
|        - | 3977 | `	}` |
|     4503 | 3978 | `	if( pOwner ){` |
|       77 | 3979 | `		SyBlobAppend(pOut,pOwner->zString,pOwner->nByte);` |
|       77 | 3980 | `		SyBlobAppend(pOut,"::",sizeof("::")-1);` |
|       36 | 3981 | `	}` |
|     4503 | 3982 | `	if( nShow > 0 ){` |
|     4503 | 3983 | `		SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|     2249 | 3984 | `	}` |
|     2259 | 3985 | `}` |
|        - | 3986 | `/*` |
|        - | 3987 | `` * The CALL SITE php names in an argument diagnostic: the `called in FILE on line`` |
|        - | 3988 | `` * N` tail of a TypeError, and the `in FILE on line N` of an ArgumentCountError.`` |
|        - | 3989 | ` * It is the CALLER's position -- which the callee's frame recorded for itself when` |
|        - | 3990 | ` * it was entered (VmEnterFrame). PHL read the top of the INCLUDE stack, and the` |
|        - | 3991 | ` * count error had a hard-coded line 1, so every one of these raised inside a` |
|        - | 3992 | ` * vendor package named the entry script and an arbitrary line.` |
|        - | 3993 | ` *` |
|        - | 3994 | ` * The top frame is only the callee's when the raise happens AFTER VmEnterFrame; a` |
|        - | 3995 | ` * generator/fiber argument install runs before one is pushed, so the identity is` |
|        - | 3996 | ` * checked rather than assumed. With no frame of the callee's to read, the position` |
|        - | 3997 | ` * running right now IS the call site, which is what the fallback names.` |
|        - | 3998 | ` */` |
|      478 | 3999 | `static void VmArgCallSite(ph7_vm *pVm,ph7_vm_func *pCallee,SyString **ppFile,sxu32 *pnLine)` |
|        5 | 4000 | `{` |
|      483 | 4001 | `	VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      483 | 4002 | `	*ppFile = 0;` |
|      483 | 4003 | `	*pnLine = pVm->nCurLine;` |
|      483 | 4004 | `	if( pCallee && pFrame && pFrame->pUserData == (void *)pCallee ){` |
|      483 | 4005 | `		if( SyStringLength(&pFrame->sCallFile) > 0 ){` |
|      463 | 4006 | `			*ppFile = &pFrame->sCallFile;` |
|      229 | 4007 | `		}` |
|      483 | 4008 | `		if( pFrame->nCallLine ){` |
|      463 | 4009 | `			*pnLine = pFrame->nCallLine;` |
|      229 | 4010 | `		}` |
|      239 | 4011 | `	}` |
|      483 | 4012 | `	if( *ppFile == 0 ){` |
|       22 | 4013 | `		*ppFile = PH7_VmExecutingUnitFile(pVm);` |
|       10 | 4014 | `	}` |
|      483 | 4015 | `}` |
|        - | 4016 | `/*` |
|        - | 4017 | ` * TRUE when the callee's own activation was entered by an INTERNAL function reaching` |
|        - | 4018 | `` * for a userland callback -- `array_map`, `usort`, `preg_replace_callback`, an`` |
|        - | 4019 | ` * autoloader, a shutdown function. php reads prev_execute_data and asks whether it is` |
|        - | 4020 | ` * USER code, without walking past it, so such a callback's argument diagnostics carry` |
|        - | 4021 | `` * no `, called in FILE on line N` tail at all. PHL walked to the nearest userland frame`` |
|        - | 4022 | `` * instead and so always found one -- the `array_map(...)` line, which php never prints.`` |
|        - | 4023 | ` *` |
|        - | 4024 | ` * php's two callback FORWARDS are not exceptions to the rule: its compiler elides the` |
|        - | 4025 | `` * frame for `call_user_func()`/`call_user_func_array()`, so the frame above the callee`` |
|        - | 4026 | ` * IS the userland caller and the tail belongs. The same latch that says so here says it` |
|        - | 4027 | ` * for the binding mode and the too-few wording (VM_FRAME_NATIVE_CALLER).` |
|        - | 4028 | ` */` |
|      450 | 4029 | `static int VmArgCallerIsNative(ph7_vm *pVm,ph7_vm_func *pCallee)` |
|        5 | 4030 | `{` |
|      455 | 4031 | `	VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      680 | 4032 | `	return pCallee && pFrame && pFrame->pUserData == (void *)pCallee` |
|      675 | 4033 | `		&& (pFrame->iFlags & VM_FRAME_NATIVE_CALLER) != 0;` |
|        5 | 4034 | `}` |
|        - | 4035 | `/*` |
|        - | 4036 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - | 4037 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - | 4038 | ` */` |
|      482 | 4039 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        5 | 4040 | `{` |
|        - | 4041 | `	ph7_class *pClass;` |
|        - | 4042 | `	ph7_class_instance *pThis;` |
|        - | 4043 | `	ph7_class_method *pCons;` |
|        - | 4044 | `	ph7_value sArg;` |
|        - | 4045 | `	ph7_value *apArg[1];` |
|        - | 4046 | `	SyBlob sMsg;` |
|        - | 4047 | `	SyString sMsgStr;` |
|      487 | 4048 | `	SyString *pFuncName = &pCallee->sName;` |
|        - | 4049 | `	VmFrame *pFrame;` |
|        - | 4050 | `	sxi32 rc;` |
|      487 | 4051 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      487 | 4052 | `	if( pClass == 0 ){` |
|      ! 0 | 4053 | `		return PH7_ABORT;` |
|        - | 4054 | `	}` |
|      487 | 4055 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      487 | 4056 | `	if( pThis == 0 ){` |
|      ! 0 | 4057 | `		return PH7_ABORT;` |
|        - | 4058 | `	}` |
|      487 | 4059 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 4060 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|        - | 4061 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|        - | 4062 | ``	/* A NULL pArgName omits the ` ($name)` clause: php drops the parameter name`` |
|        - | 4063 | `	 * for a VARIADIC-collected element (many values share the one variadic` |
|        - | 4064 | `	 * formal, so no single name applies) — "Argument #2 must be of type …". */` |
|      726 | 4065 | `	if( pOwnerClass ){` |
|        - | 4066 | `		/* A property hook is named after its PROPERTY, never after the method` |
|        - | 4067 | ``		 * PHL synthesizes for it (`C::$p::set`, not `C::__phl_hook_set_p`). */`` |
|        - | 4068 | `		SyBlob sHook;` |
|       77 | 4069 | `		SyBlobInit(&sHook,&pVm->sAllocator);` |
|       77 | 4070 | `		if( PH7_VmHookFuncName(pOwnerClass,pCallee,&sHook) ){` |
|        5 | 4071 | `			if( pArgName ){` |
|        5 | 4072 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|        4 | 4073 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|        2 | 4074 | `					nArg,pArgName,zExpected,zGiven);` |
|        3 | 4075 | `			}else{` |
|      ! 0 | 4076 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|      ! 0 | 4077 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|      ! 0 | 4078 | `					nArg,zExpected,zGiven);` |
|        - | 4079 | `			}` |
|        5 | 4080 | `			SyBlobRelease(&sHook);` |
|        5 | 4081 | `			goto ArgMsgBuilt;` |
|        - | 4082 | `		}` |
|       73 | 4083 | `		SyBlobRelease(&sHook);` |
|      107 | 4084 | `		VmArgFuncLabel(pVm,&sMsg,pOwnerClass,pCallee,` |
|       68 | 4085 | `			SyStringData(pFuncName),(int)SyStringLength(pFuncName));` |
|       73 | 4086 | `		if( pArgName ){` |
|       64 | 4087 | `			SyBlobFormat(&sMsg,"(): Argument #%u ($%z) must be of type %s, %s given",` |
|       30 | 4088 | `				nArg,pArgName,zExpected,zGiven);` |
|       34 | 4089 | `		}else{` |
|       11 | 4090 | `			SyBlobFormat(&sMsg,"(): Argument #%u must be of type %s, %s given",` |
|        4 | 4091 | `				nArg,zExpected,zGiven);` |
|        - | 4092 | `		}` |
|       39 | 4093 | `	}else{` |
|        - | 4094 | `		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */` |
|      415 | 4095 | `		const char *zShow = 0;` |
|      415 | 4096 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|      415 | 4097 | `		VmArgFuncLabel(pVm,&sMsg,0,pCallee,zShow,nShow);` |
|      415 | 4098 | `		if( pArgName ){` |
|      345 | 4099 | `			SyBlobFormat(&sMsg,"(): Argument #%u ($%z) must be of type %s, %s given",` |
|      170 | 4100 | `				nArg,pArgName,zExpected,zGiven);` |
|      175 | 4101 | `		}else{` |
|       75 | 4102 | `			SyBlobFormat(&sMsg,"(): Argument #%u must be of type %s, %s given",` |
|       35 | 4103 | `				nArg,zExpected,zGiven);` |
|        - | 4104 | `		}` |
|        - | 4105 | `	}` |
|      241 | 4106 | `ArgMsgBuilt:` |
|        - | 4107 | `	/* php appends the CALL SITE to a userland callee's type error — internal` |
|        - | 4108 | `	 * (hosted C) functions get the bare message, and so does a callback an internal` |
|        - | 4109 | `	 * function reached for, whose caller frame is that builtin's rather than user` |
|        - | 4110 | `	 * code. nCurLine is the line of the call instruction being bound, which is` |
|        - | 4111 | `	 * exactly php's "called in". */` |
|      487 | 4112 | `	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 && !VmArgCallerIsNative(pVm,pCallee) ){` |
|        - | 4113 | `		SyString *pCallFile;` |
|        - | 4114 | `		sxu32 nCallLine;` |
|      437 | 4115 | `		VmArgCallSite(pVm,pCallee,&pCallFile,&nCallLine);` |
|      437 | 4116 | `		if( pCallFile && pCallFile->nByte > 0 ){` |
|      437 | 4117 | `			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,nCallLine);` |
|      216 | 4118 | `		}` |
|      216 | 4119 | `	}` |
|      487 | 4120 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      487 | 4121 | `	if( pCons ){` |
|      487 | 4122 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      487 | 4123 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      487 | 4124 | `		apArg[0] = &sArg;` |
|      487 | 4125 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      487 | 4126 | `		PH7_MemObjRelease(&sArg);` |
|      241 | 4127 | `	}` |
|      487 | 4128 | `	SyBlobRelease(&sMsg);` |
|      487 | 4129 | `	pFrame = pVm->pFrame;` |
|      487 | 4130 | `	if( pFrame ){` |
|      487 | 4131 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      487 | 4132 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      241 | 4133 | `	}` |
|      487 | 4134 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      487 | 4135 | `	PH7_ClassInstanceUnref(pThis);` |
|      487 | 4136 | `	if( rc == SXERR_ABORT ){` |
|        6 | 4137 | `		return PH7_ABORT;` |
|        - | 4138 | `	}` |
|      483 | 4139 | `	return PH7_EXCEPTION;` |
|      246 | 4140 | `}` |
|        - | 4141 | `/*` |
|        - | 4142 | ` * Type-check (and weak-mode coerce, in place) ONE element collected into a` |
|        - | 4143 | ` * variadic parameter — the shared per-element enforcement for BOTH the` |
|        - | 4144 | ` * positional and the named-argument binding paths of OP_CALL.` |
|        - | 4145 | ` *` |
|        - | 4146 | ` * nArgPos is php's 1-based argument number for the message: a positional` |
|        - | 4147 | ` * element uses its overall call position; a NAMED element always reports` |
|        - | 4148 | ``  * (total positional args) + 1, whichever named element fails. The `($name)` `` |
|        - | 4149 | ` * clause is omitted (pArgName = 0): many values share the one variadic` |
|        - | 4150 | ` * formal, so no single parameter name applies.` |
|        - | 4151 | ` *` |
|        - | 4152 | ` * Returns SXRET_OK when the element passes (possibly coerced), PH7_ABORT or` |
|        - | 4153 | ` * PH7_EXCEPTION after throwing php's TypeError otherwise.` |
|        - | 4154 | ` */` |
|     3159 | 4155 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,` |
|        - | 4156 | `	ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict)` |
|        5 | 4157 | `{` |
|        - | 4158 | `	sxi32 rc;` |
|     3164 | 4159 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|       33 | 4160 | `		if( VmCoerceToUnion(&(*pVm), pVal, &pFormal->aUnionAlts,` |
|       38 | 4161 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0, bCallIsStrict, pSelfHint) != SXRET_OK ){` |
|        - | 4162 | `			const char *zGiven;` |
|       12 | 4163 | `			const char *zExpected = "union";` |
|        - | 4164 | `			char zBuf[128];` |
|        - | 4165 | `			char zTypeBuf[128];` |
|       12 | 4166 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        3 | 4167 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|       10 | 4168 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 4169 | `				zGiven = "null";` |
|      ! 0 | 4170 | `			}else{` |
|        9 | 4171 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|        - | 4172 | `			}` |
|       12 | 4173 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|       16 | 4174 | `				zExpected = VmHintTextResolved(&(*pVm),&pFormal->sTypeName,pSelfHint,` |
|        4 | 4175 | `					zTypeBuf,sizeof(zTypeBuf));` |
|        4 | 4176 | `			}` |
|       12 | 4177 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,zExpected,zGiven);` |
|       12 | 4178 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 4179 | `		}` |
|       17 | 4180 | `		return SXRET_OK;` |
|        - | 4181 | `	}` |
|     3137 | 4182 | `	if( pFormal->nType < 1` |
|     1798 | 4183 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|     2705 | 4184 | `		return SXRET_OK;` |
|        - | 4185 | `	}` |
|      442 | 4186 | `	if( pFormal->nType == SXU32_HIGH ){` |
|        - | 4187 | `		/* Class or pseudo-type (true/false/iterable/mixed) hint — enforced` |
|        - | 4188 | `		 * per element exactly like the non-variadic paths. */` |
|       61 | 4189 | `		SyString *pName = &pFormal->sClass;` |
|        - | 4190 | `		ph7_class *pClass;` |
|       61 | 4191 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|       61 | 4192 | `		if( rcPseudo == 0 ){` |
|        - | 4193 | `			/* Recognised pseudo-type; value mismatches */` |
|        - | 4194 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       14 | 4195 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|        6 | 4196 | `				VmClassHintTypeName(pName,0,` |
|        6 | 4197 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|        3 | 4198 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        8 | 4199 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 4200 | `		}` |
|        - | 4201 | `		/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class, put` |
|        - | 4202 | `		 * through the shared VmClassHintMatches rule (self/parent/static,` |
|        - | 4203 | `		 * interface/abstract hints via iLoadable=FALSE, and a name that resolves` |
|        - | 4204 | `		 * to nothing). Non-nullable here — the guard above skips nullable+null —` |
|        - | 4205 | `		 * so ANY non-object is a TypeError, matching php. */` |
|       55 | 4206 | `		pClass = 0;` |
|       55 | 4207 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|        - | 4208 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       42 | 4209 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       20 | 4210 | `				VmClassHintTypeName(pName,pClass,` |
|       20 | 4211 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|       10 | 4212 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       22 | 4213 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 4214 | `		}` |
|       34 | 4215 | `		return SXRET_OK;` |
|        - | 4216 | `	}` |
|      384 | 4217 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       66 | 4218 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|        - | 4219 | `			char zGivenBuf[128];` |
|        8 | 4220 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|        2 | 4221 | `				"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        6 | 4222 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 4223 | `		}` |
|       62 | 4224 | `		if( VmEnforceScalarType(pVal, pFormal->nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 4225 | `			char zTypeBuf[128];` |
|        - | 4226 | `			char zGivenBuf[128];` |
|       64 | 4227 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       20 | 4228 | `				VmScalarTypeName(pFormal->nType, &pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|       20 | 4229 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       44 | 4230 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 4231 | `		}` |
|       10 | 4232 | `	}else{` |
|        - | 4233 | `		/* Mask matched — an int variadic accepting a whole-real materializes` |
|        - | 4234 | `		 * it as a genuine int (php: f(int ...$a) with 1.0 collects int(1)). */` |
|      322 | 4235 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|        - | 4236 | `	}` |
|      340 | 4237 | `	return SXRET_OK;` |
|     1472 | 4238 | `}` |
|        - | 4239 | `/*` |
|        - | 4240 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|        - | 4241 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|        - | 4242 | ` * before a required parameter as implicitly required), excluding a trailing` |
|        - | 4243 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|        - | 4244 | ` * pick php's wording — "exactly N expected" when required == total,` |
|        - | 4245 | ` * "at least N" when trailing optionals exist.` |
|        - | 4246 | ` */` |
|    11762 | 4247 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|        5 | 4248 | `{` |
|    11767 | 4249 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|    11767 | 4250 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|    11767 | 4251 | `	sxu32 nRequired = 0;` |
|        - | 4252 | `	sxu32 n;` |
|    11767 | 4253 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|      974 | 4254 | `		nFormal--;` |
|      437 | 4255 | `	}` |
|    52161 | 4256 | `	for( n = 0 ; n < nFormal ; ++n ){` |
|    40399 | 4257 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|    15040 | 4258 | `			nRequired = n + 1;` |
|     7499 | 4259 | `		}` |
|    20167 | 4260 | `	}` |
|    11767 | 4261 | `	*pnNonVariadic = nFormal;` |
|    11767 | 4262 | `	return nRequired;` |
|        5 | 4263 | `}` |
|        - | 4264 | `/*` |
|        - | 4265 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|        - | 4266 | ` * with too few arguments:` |
|        - | 4267 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|        - | 4268 | ` *   {exactly\|at least} M expected` |
|        - | 4269 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|        - | 4270 | ` * info yet (the runtime line-tracking gate), so the line is a` |
|        - | 4271 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|        - | 4272 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|        - | 4273 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|        - | 4274 | ` */` |
|       56 | 4275 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 4276 | `	ph7_vm_func *pCallee,sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|        3 | 4277 | `{` |
|        - | 4278 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|        - | 4279 | `	SyBlob sMsg;` |
|        - | 4280 | ``	/* A CLOSURE has no name of its own: php calls it `{closure:file:line}` (or`` |
|        - | 4281 | ``	 * `{closure:enclosing():line}` for one written inside a function), and this`` |
|        - | 4282 | `	 * message was the last of the four argument diagnostics still printing the` |
|        - | 4283 | ``	 * engine's internal `[closure_N]` instead. */`` |
|        - | 4284 | `	{` |
|       59 | 4285 | `		const char *zShow = 0;` |
|       59 | 4286 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|       59 | 4287 | `		if( nShow < 1 ){` |
|      ! 0 | 4288 | `			zShow = SyStringData(pFuncName);` |
|      ! 0 | 4289 | `			nShow = (int)SyStringLength(pFuncName);` |
|      ! 0 | 4290 | `		}` |
|       59 | 4291 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       59 | 4292 | `		SyBlobAppend(&sMsg,"Too few arguments to function ",sizeof("Too few arguments to function ")-1);` |
|       59 | 4293 | `		VmArgOwnerPrefix(pVm,&sMsg,pOwnerClass,pCallee);` |
|       59 | 4294 | `		SyBlobFormat(&sMsg,"%.*s(), %u passed",nShow,zShow,nPassed);` |
|        - | 4295 | `	}` |
|       59 | 4296 | `	if( bCallSite ){` |
|        - | 4297 | `		SyString *pFile;` |
|        - | 4298 | `		sxu32 nCallLine;` |
|       49 | 4299 | `		VmArgCallSite(pVm,pCallee,&pFile,&nCallLine);` |
|       26 | 4300 | `		SyBlobFormat(&sMsg," in %z on line %u",` |
|       46 | 4301 | `			(pFile && pFile->nByte > 0) ? (const SyString *)pFile : &sUnknown,nCallLine);` |
|       23 | 4302 | `	}` |
|       59 | 4303 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|       28 | 4304 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|        - | 4305 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       59 | 4306 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        3 | 4307 | `}` |
|        - | 4308 | `/*` |
|        - | 4309 | ` * Throw php's catchable Error for a by-reference parameter handed something that` |
|        - | 4310 | ` * cannot be referenced:` |
|        - | 4311 | ` *   C::m(): Argument #1 ($x) could not be passed by reference` |
|        - | 4312 | ` * php refuses this at the CALL, before the callee's ZPP runs, and it decides from` |
|        - | 4313 | ` * the argument's compile-time SHAPE (VmCallArgMap.nNonLvalMask) rather than from` |
|        - | 4314 | ` * the value that arrived. The class prefix follows the same rule as the too-few` |
|        - | 4315 | ` * ArgumentCountError above — php names the method's owner, and PHL used to report` |
|        - | 4316 | `` * the bare `m()`.`` |
|        - | 4317 | ` */` |
|        - | 4318 | `/*` |
|        - | 4319 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` — a WARNING,`` |
|        - | 4320 | ` * raised where a by-REFERENCE parameter is handed something the site cannot alias, and` |
|        - | 4321 | ` * then the callee operates on a copy. Two sites reach it: call_user_func_array(), whose` |
|        - | 4322 | ` * argument-array element is a plain VALUE rather than a reference, and Fiber::start(),` |
|        - | 4323 | `` * whose own `...$args` are by value whatever the body declares. The callee is named the`` |
|        - | 4324 | ` * way every other argument diagnostic names it — a method with its class, a closure with` |
|        - | 4325 | `` * php's `{closure:file:line}`.`` |
|        - | 4326 | ` */` |
|       76 | 4327 | `PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,` |
|        - | 4328 | `	ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName)` |
|        1 | 4329 | `{` |
|       77 | 4330 | `	const char *zShow = 0;` |
|       77 | 4331 | `	int nShow = PH7_VmFuncDisplayName(&(*pVm),pCallee,&zShow);` |
|        - | 4332 | ``	/* A NULL pArgName omits the ` ($name)` clause, php's wording for an element`` |
|        - | 4333 | `	 * collected by a by-ref VARIADIC tail: many values share one formal, so no` |
|        - | 4334 | `	 * single name applies (the refusal message splits the same way). */` |
|       77 | 4335 | `	if( pOwnerClass && pArgName ){` |
|       34 | 4336 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 4337 | `			"%z::%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       11 | 4338 | `			&pOwnerClass->sDisp,nShow,zShow,nArgPos,pArgName);` |
|       66 | 4339 | `	}else if( pOwnerClass ){` |
|      ! 0 | 4340 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 4341 | `			"%z::%.*s(): Argument #%u must be passed by reference, value given",` |
|      ! 0 | 4342 | `			&pOwnerClass->sDisp,nShow,zShow,nArgPos);` |
|       55 | 4343 | `	}else if( pArgName ){` |
|       76 | 4344 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 4345 | `			"%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       25 | 4346 | `			nShow,zShow,nArgPos,pArgName);` |
|       26 | 4347 | `	}else{` |
|        7 | 4348 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 4349 | `			"%.*s(): Argument #%u must be passed by reference, value given",` |
|        2 | 4350 | `			nShow,zShow,nArgPos);` |
|        - | 4351 | `	}` |
|       77 | 4352 | `}` |
|     4030 | 4353 | `PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 4354 | `	ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName)` |
|        3 | 4355 | `{` |
|        - | 4356 | `	SyBlob sMsg;` |
|     4033 | 4357 | `	const char *zShow = 0;` |
|     4033 | 4358 | `	int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|     4033 | 4359 | `	if( nShow < 1 ){` |
|      ! 0 | 4360 | `		zShow = SyStringData(pFuncName);` |
|      ! 0 | 4361 | `		nShow = (int)SyStringLength(pFuncName);` |
|      ! 0 | 4362 | `	}` |
|     4033 | 4363 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     4033 | 4364 | `	VmArgFuncLabel(pVm,&sMsg,pOwnerClass,pCallee,zShow,nShow);` |
|     4033 | 4365 | `	SyBlobFormat(&sMsg,"(): Argument #%u ($%z) could not be passed by reference",` |
|     2015 | 4366 | `		nArgPos,pArgName);` |
|        - | 4367 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|     4033 | 4368 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 4369 | `}` |
|        - | 4370 | `/*` |
|        - | 4371 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|        - | 4372 | ` * called with too few arguments, in php's ZPP wording:` |
|        - | 4373 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|        - | 4374 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|        - | 4375 | ` * pluralized on the expected count).` |
|        - | 4376 | ` *` |
|        - | 4377 | ` * nMaxDeclared is the TOTAL formal count, variadic tail included — php's ZPP` |
|        - | 4378 | ` * says "exactly" only when the callable accepts no more than it requires, and` |
|        - | 4379 | `` * a variadic tail leaves no maximum at all (`max()` is "at least 1"). That is`` |
|        - | 4380 | ` * NOT the user-function rule VmThrowTooFewArgs applies: php words` |
|        - | 4381 | `` * `function f($a, ...$b)` as "exactly 1 expected", ignoring the variadic.`` |
|        - | 4382 | ` */` |
|       24 | 4383 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 4384 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared)` |
|        1 | 4385 | `{` |
|        - | 4386 | `	SyBlob sMsg;` |
|       25 | 4387 | `	const char *zKind = (nRequired >= nMaxDeclared) ? "exactly" : "at least";` |
|       25 | 4388 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|       25 | 4389 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       25 | 4390 | `	if( pOwnerClass ){` |
|      ! 0 | 4391 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      ! 0 | 4392 | `			&pOwnerClass->sDisp,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|      ! 0 | 4393 | `	}else{` |
|       25 | 4394 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|       12 | 4395 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|        - | 4396 | `	}` |
|        - | 4397 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       25 | 4398 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 4399 | `}` |
|        - | 4400 | `/*` |
|        - | 4401 | ` * php's ArgumentCountError for an INTERNAL (builtin-chunk) callable handed too` |
|        - | 4402 | ` * MANY arguments, in php's ZPP wording:` |
|        - | 4403 | ` *   count_chars() expects at most 2 arguments, 3 given` |
|        - | 4404 | ` * "exactly" when the bounds coincide, "at most" when optional parameters make` |
|        - | 4405 | ` * them differ; the plural follows the MAXIMUM, so a zero-parameter builtin` |
|        - | 4406 | ` * reports "exactly 0 arguments". A variadic tail leaves php no maximum at all,` |
|        - | 4407 | ` * so the caller must not route such a callee here.` |
|        - | 4408 | ` */` |
|       22 | 4409 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 4410 | `	sxu32 nPassed,sxu32 nMax,sxu32 nRequired)` |
|        1 | 4411 | `{` |
|        - | 4412 | `	SyBlob sMsg;` |
|       23 | 4413 | `	const char *zKind = (nRequired >= nMax) ? "exactly" : "at most";` |
|       23 | 4414 | `	const char *zPlural = (nMax == 1) ? "" : "s";` |
|       23 | 4415 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       23 | 4416 | `	if( pOwnerClass ){` |
|      ! 0 | 4417 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      ! 0 | 4418 | `			&pOwnerClass->sDisp,pFuncName,zKind,nMax,zPlural,nPassed);` |
|      ! 0 | 4419 | `	}else{` |
|       23 | 4420 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|       11 | 4421 | `			pFuncName,zKind,nMax,zPlural,nPassed);` |
|        - | 4422 | `	}` |
|        - | 4423 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       23 | 4424 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 4425 | `}` |
|        - | 4426 | `/*` |
|        - | 4427 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|        - | 4428 | ` * named or positional argument resolved to:` |
|        - | 4429 | ` *   C::f(): Argument #N ($x) not passed` |
|        - | 4430 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|        - | 4431 | ` */` |
|        6 | 4432 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 4433 | `	ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName)` |
|        2 | 4434 | `{` |
|        - | 4435 | `	SyBlob sMsg;` |
|        8 | 4436 | `	const char *zShow = 0;` |
|        8 | 4437 | `	int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|        8 | 4438 | `	if( nShow < 1 ){` |
|      ! 0 | 4439 | `		zShow = SyStringData(pFuncName);` |
|      ! 0 | 4440 | `		nShow = (int)SyStringLength(pFuncName);` |
|      ! 0 | 4441 | `	}` |
|        8 | 4442 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        8 | 4443 | `	VmArgOwnerPrefix(pVm,&sMsg,pOwnerClass,pCallee);` |
|        8 | 4444 | `	SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) not passed",nShow,zShow,nArg,pArgName);` |
|        - | 4445 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|        8 | 4446 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        2 | 4447 | `}` |
|        - | 4448 | `/*` |
|        - | 4449 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - | 4450 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - | 4451 | ` */` |
|        - | 4452 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|        - | 4453 | ` * The message is copied into the instance by __construct, so the caller owns` |
|        - | 4454 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|      150 | 4455 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|        5 | 4456 | `{` |
|        - | 4457 | `	ph7_class *pClass;` |
|        - | 4458 | `	ph7_class_instance *pThis;` |
|        - | 4459 | `	ph7_class_method *pCons;` |
|        - | 4460 | `	ph7_value sArg;` |
|        - | 4461 | `	ph7_value *apArg[1];` |
|        - | 4462 | `	SyString sMsgStr;` |
|        - | 4463 | `	VmFrame *pFrame;` |
|        - | 4464 | `	sxi32 rc;` |
|      155 | 4465 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      155 | 4466 | `	if( pClass == 0 ){` |
|      ! 0 | 4467 | `		return PH7_ABORT;` |
|        - | 4468 | `	}` |
|      155 | 4469 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      155 | 4470 | `	if( pThis == 0 ){` |
|      ! 0 | 4471 | `		return PH7_ABORT;` |
|        - | 4472 | `	}` |
|      155 | 4473 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      155 | 4474 | `	if( pCons ){` |
|      155 | 4475 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|      155 | 4476 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      155 | 4477 | `		apArg[0] = &sArg;` |
|      155 | 4478 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      155 | 4479 | `		PH7_MemObjRelease(&sArg);` |
|       75 | 4480 | `	}` |
|      155 | 4481 | `	pFrame = pVm->pFrame;` |
|      155 | 4482 | `	if( pFrame ){` |
|      155 | 4483 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      155 | 4484 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       75 | 4485 | `	}` |
|      155 | 4486 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      155 | 4487 | `	PH7_ClassInstanceUnref(pThis);` |
|      155 | 4488 | `	if( rc == SXERR_ABORT ){` |
|        6 | 4489 | `		return PH7_ABORT;` |
|        - | 4490 | `	}` |
|      151 | 4491 | `	return PH7_EXCEPTION;` |
|       80 | 4492 | `}` |
|        - | 4493 | `/*` |
|        - | 4494 | `` * php names a property HOOK after the property it belongs to — `C::$p::get()`,`` |
|        - | 4495 | `` * `C::$p::set()` — never after a method, because in php a hook is not one. PHL`` |
|        - | 4496 | `` * synthesizes each hook as a hidden method `__phl_hook_get_NAME` /`` |
|        - | 4497 | `` * `__phl_hook_set_NAME` on the declaring class, so the php-facing name is a`` |
|        - | 4498 | ` * prefix rewrite. Returns TRUE (and writes pOut) only for such a method; every` |
|        - | 4499 | ` * other callee falls through to the ordinary Class::method rendering.` |
|        - | 4500 | ` */` |
|        - | 4501 | `#define PH7_HOOK_METH_PFX "__phl_hook_"` |
|   647555 | 4502 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind)` |
|        5 | 4503 | `{` |
|   647560 | 4504 | `	const sxu32 nPfx = sizeof(PH7_HOOK_METH_PFX)-1;` |
|   647560 | 4505 | `	if( pName->zString == 0 \|\| pName->nByte <= nPfx + 4 ){` |
|   647106 | 4506 | `		return 0;` |
|        - | 4507 | `	}` |
|      459 | 4508 | `	if( SyMemcmp(pName->zString,PH7_HOOK_METH_PFX,nPfx) != 0 ){` |
|      417 | 4509 | `		return 0;` |
|        - | 4510 | `	}` |
|       46 | 4511 | `	if( SyMemcmp(&pName->zString[nPfx],"get_",4) == 0 ){` |
|       34 | 4512 | `		*pzKind = "get";` |
|       30 | 4513 | `	}else if( SyMemcmp(&pName->zString[nPfx],"set_",4) == 0 ){` |
|       15 | 4514 | `		*pzKind = "set";` |
|        9 | 4515 | `	}else{` |
|      ! 0 | 4516 | `		return 0;` |
|        - | 4517 | `	}` |
|       46 | 4518 | `	SyStringInitFromBuf(pProp,&pName->zString[nPfx+4],pName->nByte-(nPfx+4));` |
|       46 | 4519 | `	return 1;` |
|   323730 | 4520 | `}` |
|      168 | 4521 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        5 | 4522 | `{` |
|        - | 4523 | `	SyString sProp;` |
|        - | 4524 | `	const char *zKind;` |
|      173 | 4525 | `	if( pClass == 0 \|\| !PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|      163 | 4526 | `		return 0;` |
|        - | 4527 | `	}` |
|       12 | 4528 | `	SyBlobFormat(pOut,"%z::$%z::%s",&pClass->sDisp,&sProp,zKind);` |
|       12 | 4529 | `	return 1;` |
|       89 | 4530 | `}` |
|        - | 4531 | `/*` |
|        - | 4532 | ` * The callee name php puts in front of a return-side message. A METHOD is` |
|        - | 4533 | ` * qualified with its DECLARING class ("P::m", even when called on a subclass);` |
|        - | 4534 | ` * anything else uses its display name (which is also what strips a closure's` |
|        - | 4535 | ` * internal key). The argument-side messages take the owner class as a parameter` |
|        - | 4536 | ` * instead — they are thrown from call sites that already resolved it.` |
|        - | 4537 | ` */` |
|      174 | 4538 | `static void VmReturnFuncName(ph7_vm *pVm,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        5 | 4539 | `{` |
|      179 | 4540 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        - | 4541 | `		/* ...and a TRAIT method's declaring class is the trait, which php does not` |
|        - | 4542 | `		 * have at run time: it composed the method INTO the using class, and names` |
|        - | 4543 | `` 		 * that class here. `trait T { function m(): self {…} } class C { use T; }` `` |
|        - | 4544 | ``		 * reported `T::m(): Return value must be…` where php says `C::m()`, while`` |
|        - | 4545 | `		 * the ARGUMENT-side twin (which is handed an already-resolved owner) said` |
|        - | 4546 | ``		 * `C::m()` in the same function. PH7_VmMemberOwnerClass is the shared walk;`` |
|        - | 4547 | `		 * it needs the class the call was made THROUGH to find the user, and answers` |
|        - | 4548 | `		 * 0 only when there is none — keep the declaring class for that. */` |
|      101 | 4549 | `		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|      101 | 4550 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pDecl,PH7_VmPeekTopClass(pVm));` |
|      101 | 4551 | `		if( pOwner == 0 ){` |
|      ! 0 | 4552 | `			pOwner = pDecl;` |
|      ! 0 | 4553 | `		}` |
|      101 | 4554 | `		if( PH7_VmHookFuncName(pOwner,pFunc,pOut) ){` |
|        7 | 4555 | `			return;` |
|        - | 4556 | `		}` |
|       95 | 4557 | `		SyBlobFormat(pOut,"%z::%z",&pOwner->sDisp,&pFunc->sName);` |
|       95 | 4558 | `		return;` |
|        - | 4559 | `	}` |
|        - | 4560 | `	{` |
|       83 | 4561 | `		const char *zShow = 0;` |
|       83 | 4562 | `		int nShow = PH7_VmFuncDisplayName(pVm,pFunc,&zShow);` |
|       83 | 4563 | `		if( zShow && nShow > 0 ){` |
|       83 | 4564 | `			SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|       39 | 4565 | `		}` |
|        - | 4566 | `	}` |
|       92 | 4567 | `}` |
|      146 | 4568 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zExpected,const char *zGiven)` |
|        5 | 4569 | `{` |
|        - | 4570 | `	SyBlob sMsg,sName;` |
|        - | 4571 | `	sxi32 rc;` |
|      151 | 4572 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      151 | 4573 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|      151 | 4574 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|      151 | 4575 | `	SyBlobFormat(&sMsg,"%.*s(): Return value must be of type %s, %s returned",` |
|      146 | 4576 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpected,zGiven);` |
|      151 | 4577 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|      151 | 4578 | `	SyBlobRelease(&sName);` |
|      151 | 4579 | `	SyBlobRelease(&sMsg);` |
|      151 | 4580 | `	return rc;` |
|        5 | 4581 | `}` |
|        - | 4582 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|        - | 4583 | `` * explicit `return` at compile time, so this fires only for an implicit return.`` |
|        - | 4584 | ` * php calls it a "method" when it is one. */` |
|        4 | 4585 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|        2 | 4586 | `{` |
|        - | 4587 | `	SyBlob sMsg,sName;` |
|        - | 4588 | `	sxi32 rc;` |
|        6 | 4589 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        6 | 4590 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|        6 | 4591 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|        6 | 4592 | `	SyBlobFormat(&sMsg,"%.*s(): never-returning %s must not implicitly return",` |
|        4 | 4593 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),` |
|        4 | 4594 | `		(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? "method" : "function");` |
|        6 | 4595 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|        6 | 4596 | `	SyBlobRelease(&sName);` |
|        6 | 4597 | `	SyBlobRelease(&sMsg);` |
|        6 | 4598 | `	return rc;` |
|        2 | 4599 | `}` |
|        - | 4600 | `/*` |
|        - | 4601 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - | 4602 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - | 4603 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - | 4604 | ` *` |
|        - | 4605 | ` * A class name LONGER than the buffer is answered from the class itself rather` |
|        - | 4606 | ` * than cut down to fit: PH7_NewClass() duplicates the name with` |
|        - | 4607 | ` * SyMemBackendStrDup(), which appends the NUL, and the class outlives the` |
|        - | 4608 | ` * message being built. php prints the whole name however long it is, and a` |
|        - | 4609 | ` * truncated one would name a class that does not exist.` |
|        - | 4610 | ` */` |
|     1652 | 4611 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        5 | 4612 | `{` |
|     1657 | 4613 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      164 | 4614 | `		return pVal->x.iVal ? "true" : "false";` |
|        - | 4615 | `	}` |
|     1498 | 4616 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      154 | 4617 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      154 | 4618 | `		if( pThis && pThis->pClass ){` |
|      154 | 4619 | `			SyString *pName = &pThis->pClass->sName;` |
|      154 | 4620 | `			if( pName->nByte >= nBuf ){` |
|        3 | 4621 | `				return pName->zString;` |
|        - | 4622 | `			}` |
|      152 | 4623 | `			SyMemcpy(pName->zString,zBuf,pName->nByte);` |
|      152 | 4624 | `			zBuf[pName->nByte] = 0;` |
|      152 | 4625 | `			return zBuf;` |
|        - | 4626 | `		}` |
|      ! 0 | 4627 | `		return "object";` |
|        - | 4628 | `	}` |
|     1349 | 4629 | `	return ph7_type_name(pVal);` |
|      826 | 4630 | `}` |
|        - | 4631 | `/*` |
|        - | 4632 | ` * Throw a PHP-compatible Error when unpacking ('...$expr') receives something` |
|        - | 4633 | ` * that is neither an array nor a Traversable. Matches the message and class PHP` |
|        - | 4634 | ` * raises ("Only arrays and Traversables can be unpacked, X given").` |
|        - | 4635 | ` *` |
|        - | 4636 | ` * php picks the class from BOTH the value and the site: an ARRAY-literal unpack` |
|        - | 4637 | `` * (`[...$x]`) is \TypeError for an object and plain \Error for every scalar,`` |
|        - | 4638 | `` * while an ARGUMENT unpack (`f(...$x)`, `new C(...$x)`) is \TypeError for all of`` |
|        - | 4639 | ` * them — bArgUnpack says which site is asking.` |
|        - | 4640 | ` */` |
|       90 | 4641 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad,int bArgUnpack)` |
|        4 | 4642 | `{` |
|        - | 4643 | `	ph7_class *pClass;` |
|        - | 4644 | `	ph7_class_instance *pThis;` |
|        - | 4645 | `	ph7_class_method *pCons;` |
|        - | 4646 | `	ph7_value sArg;` |
|        - | 4647 | `	ph7_value *apArg[1];` |
|        - | 4648 | `	SyBlob sMsg;` |
|        - | 4649 | `	SyString sMsgStr;` |
|        - | 4650 | `	VmFrame *pFrame;` |
|        - | 4651 | `	sxi32 rc;` |
|       94 | 4652 | `	const char *zErrClass = (bArgUnpack \|\| (pBad->iFlags & MEMOBJ_OBJ)) ? "TypeError" : "Error";` |
|        - | 4653 | `	char zNameBuf[64];` |
|       94 | 4654 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       94 | 4655 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       94 | 4656 | `	if( pClass == 0 ){` |
|      ! 0 | 4657 | `		return PH7_ABORT;` |
|        - | 4658 | `	}` |
|       94 | 4659 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       94 | 4660 | `	if( pThis == 0 ){` |
|      ! 0 | 4661 | `		return PH7_ABORT;` |
|        - | 4662 | `	}` |
|       94 | 4663 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       94 | 4664 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       94 | 4665 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       94 | 4666 | `	if( pCons ){` |
|       94 | 4667 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       94 | 4668 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       94 | 4669 | `		apArg[0] = &sArg;` |
|       94 | 4670 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       94 | 4671 | `		PH7_MemObjRelease(&sArg);` |
|       45 | 4672 | `	}` |
|       94 | 4673 | `	SyBlobRelease(&sMsg);` |
|       94 | 4674 | `	pFrame = pVm->pFrame;` |
|       94 | 4675 | `	if( pFrame ){` |
|       94 | 4676 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       94 | 4677 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       45 | 4678 | `	}` |
|       94 | 4679 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       94 | 4680 | `	PH7_ClassInstanceUnref(pThis);` |
|       94 | 4681 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4682 | `		return PH7_ABORT;` |
|        - | 4683 | `	}` |
|       94 | 4684 | `	return PH7_EXCEPTION;` |
|       49 | 4685 | `}` |
|        - | 4686 | `/*` |
|        - | 4687 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - | 4688 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - | 4689 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - | 4690 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - | 4691 | ` */` |
|        - | 4692 | `/*` |
|        - | 4693 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|        - | 4694 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|        - | 4695 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|        - | 4696 | ` * type field.` |
|        - | 4697 | ` */` |
|   879923 | 4698 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|        5 | 4699 | `{` |
|   879928 | 4700 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|        5 | 4701 | `}` |
|    26192 | 4702 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        5 | 4703 | `{` |
|    26197 | 4704 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|    26197 | 4705 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|        - | 4706 | `	const char *zGiven;` |
|        - | 4707 | `	ph7_class *pHintScope;` |
|        - | 4708 | `	char zBuf[128];` |
|        - | 4709 | `	char zTypeBuf[128];` |
|        - | 4710 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|    26197 | 4711 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|      ! 0 | 4712 | `		return SXRET_OK;` |
|        - | 4713 | `	}` |
|        - | 4714 | `	/* never return type: the function must not return at all. An explicit` |
|        - | 4715 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|        - | 4716 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|        - | 4717 | `	 * the call site). */` |
|    26197 | 4718 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        6 | 4719 | `		return VmThrowNeverReturnError(pVm,pFunc);` |
|        - | 4720 | `	}` |
|        - | 4721 | `	/* void return type: the function must not produce a value. */` |
|    26193 | 4722 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     5213 | 4723 | `		if( pValue == 0 ){` |
|     5209 | 4724 | `			return SXRET_OK;` |
|        - | 4725 | `		}` |
|        - | 4726 | ``		/* `set => expr` is php's SHORTHAND for assigning expr to the backing`` |
|        - | 4727 | `		 * store, not a return: php compiles no return statement there at all,` |
|        - | 4728 | `		 * and still reports the hook's return type as void. PHL carries the` |
|        - | 4729 | `		 * value out of the hook body to hand it to the write-back dispatcher,` |
|        - | 4730 | `		 * so the one implicit value this arm must not reject is that one. */` |
|        6 | 4731 | `		if( pFunc->iFlags & VM_FUNC_HOOK_SET_EXPR ){` |
|        6 | 4732 | `			return SXRET_OK;` |
|        - | 4733 | `		}` |
|        - | 4734 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - | 4735 | `		 * still counts as "returned a value" here. */` |
|      ! 0 | 4736 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 | 4737 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"void",zGiven);` |
|        - | 4738 | `	}` |
|        - | 4739 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|        - | 4740 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|        - | 4741 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|    20985 | 4742 | `	if( pValue == 0 ){` |
|       33 | 4743 | `		const char *zExpected = "value";` |
|       33 | 4744 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       48 | 4745 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,` |
|       15 | 4746 | `				VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0),zTypeBuf,sizeof(zTypeBuf));` |
|       15 | 4747 | `		}` |
|        - | 4748 | `` 		/* php's word for "no value at all" is `none`, not `null` — `null returned` `` |
|        - | 4749 | ``		 * is what it says for an explicit `return null;`, a different program. */`` |
|       33 | 4750 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,"none");` |
|        - | 4751 | `	}` |
|        - | 4752 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|        - | 4753 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|        - | 4754 | `	 * matching how every other typed return reports a missing value.) */` |
|    20955 | 4755 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|        5 | 4756 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 | 4757 | `			return SXRET_OK;` |
|        - | 4758 | `		}` |
|        4 | 4759 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"null",` |
|        1 | 4760 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 4761 | `	}` |
|        - | 4762 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|        - | 4763 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|        - | 4764 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|    20951 | 4765 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|       49 | 4766 | `		return SXRET_OK;` |
|        - | 4767 | `	}` |
|        - | 4768 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|        - | 4769 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|        - | 4770 | `	 * Check by value before the real-class instanceof branch below. */` |
|    20907 | 4771 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      843 | 4772 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|      843 | 4773 | `		if( rcPseudo == 1 ){` |
|      183 | 4774 | `			return SXRET_OK;` |
|        - | 4775 | `		}` |
|      665 | 4776 | `		if( rcPseudo == 0 ){` |
|       18 | 4777 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        8 | 4778 | `				VmClassHintTypeName(&pFunc->sReturnClass,0,bNullable,zTypeBuf,sizeof(zTypeBuf)),` |
|        4 | 4779 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 4780 | `		}` |
|        - | 4781 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|      326 | 4782 | `	}` |
|        - | 4783 | `	/* The two branches below are the only ones that can name a class, so the` |
|        - | 4784 | `	 * hint scope is resolved here rather than on every scalar/void return.` |
|        - | 4785 | ``	 * `self`/`parent` in a return hint resolve against the class that DECLARED`` |
|        - | 4786 | `	 * the callee — the check runs inside the callee's own frame, so the same walk` |
|        - | 4787 | ``	 * `self::` uses answers it (a closure's bound scope included). The self-STACK`` |
|        - | 4788 | `` 	 * top is the late-static-binding class, which made an inherited `: self` `` |
|        - | 4789 | `	 * demand the SUBCLASS. See VmHintScopeClass. */` |
|    20721 | 4790 | `	pHintScope = VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0);` |
|        - | 4791 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|        - | 4792 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|        - | 4793 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|    20721 | 4794 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - | 4795 | `		sxi32 rcU;` |
|     3057 | 4796 | `		const char *zExpected = "union";` |
|     3057 | 4797 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict, pHintScope);` |
|     3057 | 4798 | `		if( rcU == SXRET_OK ){` |
|     3047 | 4799 | `			return SXRET_OK;` |
|        - | 4800 | `		}` |
|       13 | 4801 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       11 | 4802 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|        7 | 4803 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 4804 | `			zGiven = "null";` |
|      ! 0 | 4805 | `		}else{` |
|        3 | 4806 | `			zGiven = VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|        - | 4807 | `		}` |
|       13 | 4808 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       18 | 4809 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,pHintScope,` |
|        5 | 4810 | `				zTypeBuf,sizeof(zTypeBuf));` |
|        5 | 4811 | `		}` |
|       13 | 4812 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,zGiven);` |
|        - | 4813 | `	}` |
|        - | 4814 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - | 4815 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - | 4816 | `	 * it into the TypeError message. */` |
|    17669 | 4817 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      657 | 4818 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|      657 | 4819 | `		ph7_class *pExpected = 0;` |
|      657 | 4820 | `		if( !VmClassHintMatches(pVm,pClassName,pHintScope,pValue,&pExpected) ){` |
|       36 | 4821 | `			if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       30 | 4822 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       17 | 4823 | `			}else{` |
|        7 | 4824 | `				zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|        - | 4825 | `			}` |
|       52 | 4826 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       16 | 4827 | `				VmClassHintTypeName(pClassName,pExpected,bNullable,zTypeBuf,sizeof(zTypeBuf)),zGiven);` |
|        - | 4828 | `		}` |
|      625 | 4829 | `		return SXRET_OK;` |
|        - | 4830 | `	}` |
|        - | 4831 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|        - | 4832 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|        - | 4833 | `	 * non-nullable scalar return — a TypeError. */` |
|    17017 | 4834 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       26 | 4835 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        8 | 4836 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - | 4837 | `			"null");` |
|        - | 4838 | `	}` |
|        - | 4839 | ``	/* Exact match? Done. An `: int` return accepting a whole-real`` |
|        - | 4840 | ``	 * materializes it (php: `return 1.0` from `: int` yields int(1)). */`` |
|    17001 | 4841 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|    16877 | 4842 | `		VmMaterializeIntTyped(pValue,pFunc->nReturnType);` |
|    16877 | 4843 | `		return SXRET_OK;` |
|        - | 4844 | `	}` |
|        - | 4845 | `	/* Object->scalar is never compatible, EXCEPT an object with __toString()` |
|        - | 4846 | ``	 * returned as a `string`: php coerces it in weak mode. Fall through to`` |
|        - | 4847 | `	 * VmEnforceScalarType below, which invokes __toString in weak mode and` |
|        - | 4848 | `	 * still rejects the object under strict_types. */` |
|      129 | 4849 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       22 | 4850 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       31 | 4851 | `		if( !(pFunc->nReturnType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       18 | 4852 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       20 | 4853 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       29 | 4854 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        9 | 4855 | `				VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        9 | 4856 | `				zGiven);` |
|        - | 4857 | `		}` |
|        1 | 4858 | `	}` |
|        - | 4859 | `	/* Array <-> scalar is never compatible. */` |
|      111 | 4860 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|       32 | 4861 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       10 | 4862 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       10 | 4863 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 4864 | `	}` |
|        - | 4865 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - | 4866 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - | 4867 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - | 4868 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|       86 | 4869 | `	if( !bStrict` |
|       85 | 4870 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|       51 | 4871 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|       56 | 4872 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|       16 | 4873 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        4 | 4874 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - | 4875 | `			"string");` |
|        - | 4876 | `	}` |
|       81 | 4877 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|       79 | 4878 | `		return SXRET_OK;` |
|        - | 4879 | `	}` |
|        4 | 4880 | `	return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        1 | 4881 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 | 4882 | `		VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|    12892 | 4883 | `}` |
|        - | 4884 | `/*` |
|        - | 4885 | ` * Report a fatal named-argument error.` |
|        - | 4886 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - | 4887 | ` */` |
|       24 | 4888 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        4 | 4889 | `{` |
|        - | 4890 | `	SyBlob sMsg;` |
|        - | 4891 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|        - | 4892 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|        - | 4893 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|        - | 4894 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|        - | 4895 | `	 * unconditional fatal even inside try/catch. */` |
|       28 | 4896 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       28 | 4897 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|       28 | 4898 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        4 | 4899 | `}` |
|        - | 4900 | `/*` |
|        - | 4901 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - | 4902 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - | 4903 | ` * information.` |
|        - | 4904 | ` * ------------------------------------` |
|        - | 4905 | ` * Simple boring wrapper function.` |
|        - | 4906 | ` * ------------------------------------` |
|        - | 4907 | ` */` |
|    33553 | 4908 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        5 | 4909 | `{` |
|        - | 4910 | `	sxi32 rc;` |
|    33558 | 4911 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|    33558 | 4912 | `	return rc;` |
|        5 | 4913 | `}` |
|        - | 4914 | `/*` |
|        - | 4915 | ` * Resolve function context from the current frame.` |
|        - | 4916 | ` */` |
|        - | 4917 | `/*` |
|        - | 4918 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|        - | 4919 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|        - | 4920 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|        - | 4921 | ` * straight at the function's own name otherwise.` |
|        - | 4922 | ` */` |
|   647333 | 4923 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|        5 | 4924 | `{` |
|   647338 | 4925 | `	const char *zName = pFunc->sName.zString;` |
|   647338 | 4926 | `	int nName = (int)pFunc->sName.nByte;` |
|   698387 | 4927 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|   702074 | 4928 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|        - | 4929 | `	/* A property hook is not a method in php and never shows the name PHL` |
|        - | 4930 | ``	 * synthesizes for it: php calls it `$p::get` / `$p::set` — which is what`` |
|        - | 4931 | ``	 * `__FUNCTION__`, `debug_backtrace()['function']` and a Throwable's trace`` |
|        - | 4932 | `	 * report from inside one, and what makes the trace line read` |
|        - | 4933 | ``	 * `C->$p::get()`. `__METHOD__` prepends the class and lands on php's`` |
|        - | 4934 | ``	 * `C::$p::get` for free. */`` |
|        - | 4935 | `	{` |
|        - | 4936 | `		SyString sProp;` |
|        - | 4937 | `		const char *zKind;` |
|   647338 | 4938 | `		if( PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|       43 | 4939 | `			int n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|       13 | 4940 | `				"$%z::%s",&sProp,zKind);` |
|       30 | 4941 | `			*pzOut = pVm->zDisplayName;` |
|       30 | 4942 | `			return n;` |
|        - | 4943 | `		}` |
|        - | 4944 | `	}` |
|   647312 | 4945 | `	if( bClosure ){` |
|        - | 4946 | `		int n;` |
|     7623 | 4947 | `		if( pFunc->sClosureName.nByte > 0 ){` |
|        - | 4948 | `			/* The compiler built php's name: it words the ENCLOSING scope, which a` |
|        - | 4949 | ``			 * file/line pair cannot reach (`{closure:Foo::bar():3}`). */`` |
|     7623 | 4950 | `			*pzOut = pFunc->sClosureName.zString;` |
|     7623 | 4951 | `			return (int)pFunc->sClosureName.nByte;` |
|        - | 4952 | `		}` |
|      ! 0 | 4953 | `		if( pFunc->sFile.nByte > 0 ){` |
|      ! 0 | 4954 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|      ! 0 | 4955 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|      ! 0 | 4956 | `		}else{` |
|      ! 0 | 4957 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|        - | 4958 | `		}` |
|      ! 0 | 4959 | `		*pzOut = pVm->zDisplayName;` |
|      ! 0 | 4960 | `		return n;` |
|        - | 4961 | `	}` |
|   639694 | 4962 | `	*pzOut = zName;` |
|   639694 | 4963 | `	return nName;` |
|   323619 | 4964 | `}` |
|        - | 4965 | `/*` |
|        - | 4966 | `` * php's name for the ACTIVE function -- the one its `name(): ` diagnostic`` |
|        - | 4967 | ` * qualifier prints. A method is rendered with its declaring class ("W::go"),` |
|        - | 4968 | `` * a closure with php's `{closure:file:line}`, and the global scope with php's`` |
|        - | 4969 | ` * own "main". VmGetFrameContext answers the same question in the shape the` |
|        - | 4970 | ` * uncaught-exception reporter wants (a bare display name, nothing at global` |
|        - | 4971 | ` * scope); this one is for a diagnostic raised from INSIDE an internal` |
|        - | 4972 | ` * function on the caller's behalf, which is how php attributes libxml's` |
|        - | 4973 | `` * errors -- `$el->nodeValue = 'a&b'` warns under the caller's name, not under`` |
|        - | 4974 | ` * the accessor's.` |
|        - | 4975 | ` */` |
|       86 | 4976 | `PH7_PRIVATE void PH7_VmActiveFuncName(ph7_vm *pVm,SyBlob *pOut)` |
|        2 | 4977 | `{` |
|       88 | 4978 | `	VmFrame *pFrame = pVm->pFrame;` |
|       88 | 4979 | `	ph7_vm_func *pFunc = 0;` |
|       88 | 4980 | `	if( pFrame ){` |
|       88 | 4981 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       88 | 4982 | `		if( pFrame->pParent ){` |
|       26 | 4983 | `			pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       12 | 4984 | `		}` |
|       43 | 4985 | `	}` |
|       88 | 4986 | `	if( pFunc ){` |
|       26 | 4987 | `		VmReturnFuncName(&(*pVm),pFunc,pOut);` |
|       14 | 4988 | `	}else{` |
|       64 | 4989 | `		SyBlobAppend(pOut,"main",sizeof("main")-1);` |
|        - | 4990 | `	}` |
|       88 | 4991 | `	SyBlobNullAppend(pOut);` |
|       88 | 4992 | `}` |
|     1186 | 4993 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        4 | 4994 | `{` |
|        - | 4995 | `	VmFrame *pFrame;` |
|        - | 4996 | `	ph7_vm_func *pFunc;` |
|     1190 | 4997 | `	*pzFuncName = 0;` |
|     1190 | 4998 | `	*pnFuncLen = 0;` |
|     1190 | 4999 | `	pFrame = pVm->pFrame;` |
|     1190 | 5000 | `	if( pFrame == 0 ){` |
|      ! 0 | 5001 | `		return;` |
|        - | 5002 | `	}` |
|     1190 | 5003 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1190 | 5004 | `	if( pFrame->pParent == 0 ){` |
|     1144 | 5005 | `		return;` |
|        - | 5006 | `	}` |
|       50 | 5007 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 | 5008 | `	if( pFunc == 0 ){` |
|      ! 0 | 5009 | `		return;` |
|        - | 5010 | `	}` |
|       50 | 5011 | `	*pnFuncLen = PH7_VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|      597 | 5012 | `}` |
|        - | 5013 | `/*` |
|        - | 5014 | ` * Append the exception's own rendered stack trace (php's "#N file(line):` |
|        - | 5015 | ` * func()" lines plus the "#N {main}" terminator) to pOut. Returns 1 when it` |
|        - | 5016 | ` * emitted a trace. The instance's trace walks the FULL frame chain, so this is` |
|        - | 5017 | ` * what the uncaught report should print; getTraceAsString() lives in the` |
|        - | 5018 | ` * built-in library and already produces php's exact byte format, which keeps` |
|        - | 5019 | ` * one renderer for both the caught (userland) and uncaught (engine) paths.` |
|        - | 5020 | ` * Returns 0 when there is no instance or no such method, leaving the caller to` |
|        - | 5021 | ` * synthesize what it can.` |
|        - | 5022 | ` */` |
|      620 | 5023 | `static int VmAppendExceptionTrace(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 5024 | `{` |
|        - | 5025 | `	ph7_class_method *pGetTrace;` |
|        - | 5026 | `	ph7_value sTrace;` |
|        - | 5027 | `	const char *zTmp;` |
|        - | 5028 | `	int nTmp;` |
|      624 | 5029 | `	int bDone = 0;` |
|        - | 5030 | `	int bSaved;` |
|      624 | 5031 | `	if( pThis == 0 ){` |
|      ! 0 | 5032 | `		return 0;` |
|        - | 5033 | `	}` |
|      624 | 5034 | `	if( pVm->bRenderingUncaught ){` |
|        - | 5035 | `		/* Already inside a report: do not run userland trace code again. */` |
|      ! 0 | 5036 | `		return 0;` |
|        - | 5037 | `	}` |
|      624 | 5038 | `	pGetTrace = PH7_ClassExtractMethod(pThis->pClass,"getTraceAsString",sizeof("getTraceAsString")-1);` |
|      624 | 5039 | `	if( pGetTrace == 0 ){` |
|      ! 0 | 5040 | `		return 0;` |
|        - | 5041 | `	}` |
|      624 | 5042 | `	PH7_MemObjInit(pVm,&sTrace);` |
|        - | 5043 | `	/* getTraceAsString() is userland code from the builtin prelude; if it (or` |
|        - | 5044 | `	 * anything it calls) throws, the throw would be reported by this very` |
|        - | 5045 | `	 * renderer. Fence the window so that report falls back to the synthesized` |
|        - | 5046 | `	 * trace rather than re-entering here forever. */` |
|      624 | 5047 | `	bSaved = pVm->bRenderingUncaught;` |
|      624 | 5048 | `	pVm->bRenderingUncaught = 1;` |
|      624 | 5049 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetTrace,&sTrace,0,0) == SXRET_OK ){` |
|      624 | 5050 | `		zTmp = ph7_value_to_string(&sTrace,&nTmp);` |
|      624 | 5051 | `		if( zTmp && nTmp > 0 ){` |
|      624 | 5052 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      624 | 5053 | `			bDone = 1;` |
|      310 | 5054 | `		}` |
|      310 | 5055 | `	}` |
|      624 | 5056 | `	PH7_MemObjRelease(&sTrace);` |
|      624 | 5057 | `	pVm->bRenderingUncaught = bSaved;` |
|      624 | 5058 | `	return bDone;` |
|      314 | 5059 | `}` |
|        - | 5060 | `/*` |
|        - | 5061 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|        - | 5062 | ` *` |
|        - | 5063 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|        - | 5064 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|        - | 5065 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|        - | 5066 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|        - | 5067 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|        - | 5068 | ` *             trailer.` |
|        - | 5069 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|        - | 5070 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|        - | 5071 | ` * call; this routine only appends.` |
|        - | 5072 | ` */` |
|      622 | 5073 | `static void VmRenderUncaughtEntry(` |
|        - | 5074 | `	ph7_vm *pVm,SyBlob *pOut,` |
|        - | 5075 | `	ph7_class_instance *pExc, /* the exception being reported (0 when the caller has no instance) */` |
|        - | 5076 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|        - | 5077 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|        - | 5078 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|        - | 5079 | `	sxu32 nCallLine,   /* line of the call that entered the throwing frame (0 -> same) */` |
|        - | 5080 | `	SyString *pThrowFile, /* file the exception was raised IN (0/empty -> derive it) */` |
|        - | 5081 | `	const char **pzLabel) /* OUT (head entry only): php's label for the report */` |
|        4 | 5082 | `{` |
|        - | 5083 | `	SyString *pFile;` |
|        - | 5084 | `	SyString *pCallFile;` |
|        - | 5085 | `	int bParseErr,bCompileErr;` |
|      626 | 5086 | `	if( nThrowLine == 0 ){` |
|      ! 0 | 5087 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|      ! 0 | 5088 | `	}` |
|      626 | 5089 | `	if( nCallLine == 0 ){` |
|      626 | 5090 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      626 | 5091 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|      311 | 5092 | `	}` |
|      626 | 5093 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 | 5094 | `		zClass = "Exception";` |
|      ! 0 | 5095 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 | 5096 | `	}` |
|      626 | 5097 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      578 | 5098 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      287 | 5099 | `	}` |
|        - | 5100 | ``	/* WHERE php says the exception was raised. php's `zend_exception_error` reads the`` |
|        - | 5101 | ``	 * throwable's OWN `file`/`line` properties, which are stamped where it was`` |
|        - | 5102 | `	 * constructed -- so an exception that escapes a vendor package names that` |
|        - | 5103 | `	 * package's file, however many frames it unwound through on the way out. PHL read` |
|        - | 5104 | `	 * the top of the INCLUDE stack instead, which is the entry script once anything` |
|        - | 5105 | `	 * defined elsewhere is running: every uncaught report in a composer tree -- a` |
|        - | 5106 | ``	 * `throw`, an undefined function or method, a TypeError, a DivisionByZeroError,`` |
|        - | 5107 | ``	 * each link of a `$previous` chain -- named the wrong file, in the one message a`` |
|        - | 5108 | ``	 * user reads when a program dies. (`getFile()` was already right; only the report`` |
|        - | 5109 | `	 * was not.) A caller with no instance to ask -- the internal Error reports -- has` |
|        - | 5110 | `	 * a live frame instead, so it takes the file the RUNNING code is in, the same` |
|        - | 5111 | `	 * source every other diagnostic uses (see VmDiagnosticWhere).` |
|        - | 5112 | `	 *` |
|        - | 5113 | `	 * pCallFile stays the include-stack top: the synthesized trace frame below names a` |
|        - | 5114 | `	 * CALL SITE, which is the caller's file, not the throw's. */` |
|      626 | 5115 | `	pCallFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      626 | 5116 | `	pFile = pCallFile;` |
|      626 | 5117 | `	if( pThrowFile && pThrowFile->nByte > 0 ){` |
|      626 | 5118 | `		pFile = pThrowFile;` |
|      315 | 5119 | `	}else{` |
|      ! 0 | 5120 | `		SyString *pUnit = PH7_VmExecutingUnitFile(&(*pVm));` |
|      ! 0 | 5121 | `		if( pUnit && pUnit->nByte > 0 ){` |
|      ! 0 | 5122 | `			pFile = pUnit;` |
|      ! 0 | 5123 | `		}` |
|        - | 5124 | `	}` |
|      642 | 5125 | `	bParseErr = (nClass == sizeof("ParseError")-1` |
|      622 | 5126 | `	          && SyMemcmp(zClass,"ParseError",nClass) == 0);` |
|      626 | 5127 | `	bCompileErr = (nClass == sizeof("CompileError")-1` |
|      622 | 5128 | `	            && SyMemcmp(zClass,"CompileError",nClass) == 0);` |
|      626 | 5129 | `	if( bFirst && bLast && (bParseErr \|\| bCompileErr) ){` |
|        - | 5130 | `		/* php's zend_exception_error asks for the class by IDENTITY: an uncaught` |
|        - | 5131 | `		 * ParseError is reported as the E_PARSE it stands for and an uncaught` |
|        - | 5132 | `		 * CompileError as an E_COMPILE_ERROR -- one plain line naming the file and` |
|        - | 5133 | `		 * the line, with no "Uncaught", no stack trace and no "thrown in" trailer.` |
|        - | 5134 | `		 * (A user SUBCLASS of either is an ordinary uncaught exception, which is why` |
|        - | 5135 | `		 * this is a name match and not an instanceof.) */` |
|        3 | 5136 | `		*pzLabel = bParseErr ? "Parse error" : "Fatal error";` |
|        3 | 5137 | `		if( zMsg && nMsg > 0 ){` |
|        3 | 5138 | `			SyBlobAppend(pOut,zMsg,nMsg);` |
|        1 | 5139 | `		}` |
|        3 | 5140 | `		if( pFile ){` |
|        3 | 5141 | `			SyBlobFormat(pOut," in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|        1 | 5142 | `		}` |
|        3 | 5143 | `		return;` |
|        - | 5144 | `	}` |
|      624 | 5145 | `	if( bFirst ){` |
|      614 | 5146 | `		*pzLabel = "Fatal error";` |
|      614 | 5147 | `		SyBlobAppend(pOut,"Uncaught ",sizeof("Uncaught ")-1);` |
|      309 | 5148 | `	}else{` |
|       14 | 5149 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|        - | 5150 | `	}` |
|      624 | 5151 | `	SyBlobAppend(pOut,zClass,nClass);` |
|      624 | 5152 | `	if( zMsg && nMsg > 0 ){` |
|      624 | 5153 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|      624 | 5154 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|      310 | 5155 | `	}` |
|      624 | 5156 | `	if( pFile ){` |
|      624 | 5157 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|      310 | 5158 | `	}` |
|      624 | 5159 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|        - | 5160 | `	/* Prefer the exception's OWN trace: it walks the full frame chain (shared` |
|        - | 5161 | `	 * with debug_backtrace) and getTraceAsString() already renders php's exact` |
|        - | 5162 | `	 * "#N file(line): func()" body plus the "#N {main}" terminator. The` |
|        - | 5163 | `	 * synthesized fallback below can only ever describe ONE frame, so it` |
|        - | 5164 | `	 * silently dropped every intermediate frame of a nested call chain and, at` |
|        - | 5165 | `	 * file scope, invented a "#0 file(line): {main}" entry that php does not` |
|        - | 5166 | `	 * print ({main} is the bottom marker, not a called frame). */` |
|      624 | 5167 | `	if( pVm->bRenderingUncaught \|\| !VmAppendExceptionTrace(pVm,pExc,pOut) ){` |
|      ! 0 | 5168 | `		int bFrame = 0;` |
|      ! 0 | 5169 | `		if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 | 5170 | `			if( pCallFile ){` |
|        - | 5171 | `				/* php reports a trace frame at its CALL SITE, not at the line` |
|        - | 5172 | `				 * running inside it. */` |
|      ! 0 | 5173 | `				SyBlobFormat(pOut,"#0 %.*s(%u): %.*s()\n",` |
|      ! 0 | 5174 | `					(int)pCallFile->nByte,pCallFile->zString,nCallLine,nFuncLen,zFuncName);` |
|      ! 0 | 5175 | `			}else{` |
|      ! 0 | 5176 | `				SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|        - | 5177 | `			}` |
|      ! 0 | 5178 | `			bFrame = 1;` |
|      ! 0 | 5179 | `		}` |
|        - | 5180 | `		/* {main} closes the trace, numbered after whatever frames precede it. */` |
|      ! 0 | 5181 | `		SyBlobAppend(pOut,bFrame ? "#1 {main}" : "#0 {main}",` |
|      ! 0 | 5182 | `			bFrame ? sizeof("#1 {main}")-1 : sizeof("#0 {main}")-1);` |
|      ! 0 | 5183 | `	}` |
|      624 | 5184 | `	if( bLast && pFile ){` |
|      614 | 5185 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|      614 | 5186 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|      305 | 5187 | `	}` |
|      315 | 5188 | `}` |
|        - | 5189 | `/*` |
|        - | 5190 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|        - | 5191 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|        - | 5192 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|        - | 5193 | ` */` |
|      ! 0 | 5194 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|      ! 0 | 5195 | `{` |
|        - | 5196 | `	SyBlob sOut;` |
|      ! 0 | 5197 | `	const char *zLabel = "Fatal error";` |
|        - | 5198 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|        - | 5199 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|      ! 0 | 5200 | `	pVm->iExitStatus = 255;` |
|      ! 0 | 5201 | `	if( !pVm->bErrReport ){` |
|      ! 0 | 5202 | `		return PH7_OK;` |
|        - | 5203 | `	}` |
|      ! 0 | 5204 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      ! 0 | 5205 | `	VmRenderUncaughtEntry(pVm,&sOut,0,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0,0,&zLabel);` |
|      ! 0 | 5206 | `	VmEmitFatalReport(pVm,zLabel,(const char *)SyBlobData(&sOut),SyBlobLength(&sOut));` |
|      ! 0 | 5207 | `	SyBlobRelease(&sOut);` |
|      ! 0 | 5208 | `	return PH7_ABORT;` |
|      ! 0 | 5209 | `}` |
|        - | 5210 | `/*` |
|        - | 5211 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|        - | 5212 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|        - | 5213 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|        - | 5214 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|        - | 5215 | ` */` |
|        - | 5216 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|      622 | 5217 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|        4 | 5218 | `{` |
|        - | 5219 | `	ph7_value *pValue;` |
|        - | 5220 | `	ph7_class_instance *pPrev;` |
|        - | 5221 | `	ph7_class *pThrowable;` |
|      626 | 5222 | `	if( pThis == 0 ){` |
|      ! 0 | 5223 | `		return 0;` |
|        - | 5224 | `	}` |
|      626 | 5225 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|      626 | 5226 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      616 | 5227 | `		return 0;` |
|        - | 5228 | `	}` |
|       14 | 5229 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 5230 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|        - | 5231 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|        - | 5232 | `	 * never renders a stray object as an exception entry. */` |
|       14 | 5233 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|       14 | 5234 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|      ! 0 | 5235 | `		return 0;` |
|        - | 5236 | `	}` |
|       14 | 5237 | `	return pPrev;` |
|      315 | 5238 | `}` |
|        - | 5239 | `/*` |
|        - | 5240 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|        - | 5241 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|        - | 5242 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|        - | 5243 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|        - | 5244 | ` */` |
|       16 | 5245 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|        3 | 5246 | `{` |
|        - | 5247 | `	ph7_value *pValue;` |
|       19 | 5248 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|      ! 0 | 5249 | `		return;` |
|        - | 5250 | `	}` |
|       19 | 5251 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|       19 | 5252 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|        3 | 5253 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|        - | 5254 | `	}` |
|       17 | 5255 | `	pPrev->iRef++;` |
|        - | 5256 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|        - | 5257 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|       17 | 5258 | `	PH7_MemObjRelease(pValue);` |
|       17 | 5259 | `	pValue->x.pOther = pPrev;` |
|       17 | 5260 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|       11 | 5261 | `}` |
|        - | 5262 | `/*` |
|        - | 5263 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|        - | 5264 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|        - | 5265 | ` * absent or yields an empty string.` |
|        - | 5266 | ` */` |
|        - | 5267 | `/*` |
|        - | 5268 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|        - | 5269 | ` * 0 when the class exposes no getLine().` |
|        - | 5270 | ` */` |
|      622 | 5271 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|        4 | 5272 | `{` |
|        - | 5273 | `	ph7_class_method *pGetLine;` |
|        - | 5274 | `	ph7_value sLine;` |
|      626 | 5275 | `	sxu32 nLine = 0;` |
|      626 | 5276 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|      626 | 5277 | `	if( pGetLine == 0 ){` |
|      ! 0 | 5278 | `		return 0;` |
|        - | 5279 | `	}` |
|      626 | 5280 | `	PH7_MemObjInit(pVm,&sLine);` |
|      626 | 5281 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|      626 | 5282 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|      626 | 5283 | `		if( n > 0 ){` |
|      626 | 5284 | `			nLine = (sxu32)n;` |
|      311 | 5285 | `		}` |
|      311 | 5286 | `	}` |
|      626 | 5287 | `	PH7_MemObjRelease(&sLine);` |
|      626 | 5288 | `	return nLine;` |
|      315 | 5289 | `}` |
|        - | 5290 | `/*` |
|        - | 5291 | `` * The throwable's own `file` -- what php's uncaught report names. Read through`` |
|        - | 5292 | `` * `getFile()`, the way the message and the line beside it are read: the accessor is`` |
|        - | 5293 | `` * `final` in php (and refused here too), so it can only ever answer the property.`` |
|        - | 5294 | ` */` |
|      622 | 5295 | `static void VmExtractExceptionFile(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 5296 | `{` |
|        - | 5297 | `	ph7_class_method *pGetFile;` |
|        - | 5298 | `	ph7_value sFile;` |
|        - | 5299 | `	const char *zTmp;` |
|        - | 5300 | `	int nTmp;` |
|      626 | 5301 | `	pGetFile = PH7_ClassExtractMethod(pThis->pClass,"getFile",sizeof("getFile")-1);` |
|      626 | 5302 | `	if( pGetFile == 0 ){` |
|      ! 0 | 5303 | `		return;` |
|        - | 5304 | `	}` |
|      626 | 5305 | `	PH7_MemObjInit(pVm,&sFile);` |
|      626 | 5306 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetFile,&sFile,0,0) == SXRET_OK ){` |
|      626 | 5307 | `		zTmp = ph7_value_to_string(&sFile,&nTmp);` |
|      626 | 5308 | `		if( zTmp && nTmp > 0 ){` |
|      626 | 5309 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      311 | 5310 | `		}` |
|      311 | 5311 | `	}` |
|      626 | 5312 | `	PH7_MemObjRelease(&sFile);` |
|      315 | 5313 | `}` |
|      622 | 5314 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 5315 | `{` |
|        - | 5316 | `	ph7_class_method *pGetMessage;` |
|        - | 5317 | `	ph7_value sMsg;` |
|        - | 5318 | `	const char *zTmp;` |
|        - | 5319 | `	int nTmp;` |
|      626 | 5320 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      626 | 5321 | `	if( pGetMessage == 0 ){` |
|      ! 0 | 5322 | `		return;` |
|        - | 5323 | `	}` |
|      626 | 5324 | `	PH7_MemObjInit(pVm,&sMsg);` |
|      626 | 5325 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      626 | 5326 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      626 | 5327 | `		if( zTmp && nTmp > 0 ){` |
|      626 | 5328 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      311 | 5329 | `		}` |
|      311 | 5330 | `	}` |
|      626 | 5331 | `	PH7_MemObjRelease(&sMsg);` |
|      315 | 5332 | `}` |
|        - | 5333 | `/*` |
|        - | 5334 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|        - | 5335 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|        - | 5336 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|        - | 5337 | ` * outermost (the actually-uncaught) exception.` |
|        - | 5338 | ` *` |
|        - | 5339 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|        - | 5340 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|        - | 5341 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|        - | 5342 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|        - | 5343 | ` */` |
|        - | 5344 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|      612 | 5345 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|        4 | 5346 | `{` |
|        - | 5347 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|      616 | 5348 | `	int nChain = 0;` |
|        - | 5349 | `	int i;` |
|        - | 5350 | `	SyBlob sOut;` |
|        - | 5351 | `	/* The report's label is the HEAD entry's: only that one can be a bare` |
|        - | 5352 | ``	 * `Parse error` (an uncaught ParseError reports as the E_PARSE it stands`` |
|        - | 5353 | ``	 * for), and a chain's head is always an ordinary `Fatal error`. */`` |
|      616 | 5354 | `	const char *zLabel = "Fatal error";` |
|        - | 5355 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|        - | 5356 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|        - | 5357 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|      616 | 5358 | `	pVm->iExitStatus = 255;` |
|      616 | 5359 | `	if( !pVm->bErrReport ){` |
|      ! 0 | 5360 | `		return PH7_OK;` |
|        - | 5361 | `	}` |
|        - | 5362 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|        - | 5363 | `	 * collected) or the hard cap. */` |
|     1238 | 5364 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|      638 | 5365 | `		for( i = 0 ; i < nChain ; ++i ){` |
|       16 | 5366 | `			if( apChain[i] == pThis ){` |
|      ! 0 | 5367 | `				pThis = 0; /* cycle: stop the walk */` |
|      ! 0 | 5368 | `				break;` |
|        - | 5369 | `			}` |
|       10 | 5370 | `		}` |
|      626 | 5371 | `		if( pThis == 0 ){` |
|      ! 0 | 5372 | `			break;` |
|        - | 5373 | `		}` |
|      626 | 5374 | `		apChain[nChain++] = pThis;` |
|      626 | 5375 | `		pThis = VmExceptionGetPrevious(pThis);` |
|        4 | 5376 | `	}` |
|      616 | 5377 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        - | 5378 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|        - | 5379 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|     1238 | 5380 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|      626 | 5381 | `		ph7_class_instance *pEnt = apChain[i];` |
|        - | 5382 | `		SyBlob sMsg;` |
|        - | 5383 | `		SyBlob sFile;` |
|        - | 5384 | `		SyString sThrowFile;` |
|        - | 5385 | `		sxu32 nEntLine;` |
|      626 | 5386 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      626 | 5387 | `		SyBlobInit(&sFile,&pVm->sAllocator);` |
|      626 | 5388 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|        - | 5389 | `		/* Each link of the chain reports its OWN file and line: php prints the` |
|        - | 5390 | `		 * deepest as "Uncaught", every outer one as "Next ...", and they routinely` |
|        - | 5391 | `		 * come from different packages. Both accessors run PHP code, so read them` |
|        - | 5392 | `		 * before the render rather than inside its argument list. */` |
|      626 | 5393 | `		VmExtractExceptionFile(pVm,pEnt,&sFile);` |
|      626 | 5394 | `		nEntLine = VmExtractExceptionLine(pVm,pEnt);` |
|      626 | 5395 | `		SyStringInitFromBuf(&sThrowFile,SyBlobData(&sFile),SyBlobLength(&sFile));` |
|      937 | 5396 | `		VmRenderUncaughtEntry(pVm,&sOut,pEnt,` |
|      622 | 5397 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|      622 | 5398 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|      311 | 5399 | `			zFuncName,nFuncLen,` |
|      622 | 5400 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|      311 | 5401 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|      311 | 5402 | `			nEntLine,0,&sThrowFile,&zLabel);` |
|      626 | 5403 | `		SyBlobRelease(&sFile);` |
|      626 | 5404 | `		SyBlobRelease(&sMsg);` |
|      315 | 5405 | `	}` |
|      616 | 5406 | `	VmEmitFatalReport(pVm,zLabel,(const char *)SyBlobData(&sOut),SyBlobLength(&sOut));` |
|      616 | 5407 | `	SyBlobRelease(&sOut);` |
|      616 | 5408 | `	return PH7_ABORT;` |
|      310 | 5409 | `}` |
|        - | 5410 | `/*` |
|        - | 5411 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - | 5412 | ` *` |
|        - | 5413 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - | 5414 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - | 5415 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - | 5416 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - | 5417 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - | 5418 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - | 5419 | ` */` |
|  1566299 | 5420 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|        5 | 5421 | `{` |
|  1566304 | 5422 | `	if( pVm->bCoalesceArmed ){` |
|       13 | 5423 | `		if( pVm->pCoalesceObj ){` |
|       13 | 5424 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        5 | 5425 | `		}` |
|       13 | 5426 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|       13 | 5427 | `		pVm->pCoalesceObj = 0;` |
|       13 | 5428 | `		pVm->bCoalesceArmed = 0;` |
|        5 | 5429 | `	}` |
|  1566304 | 5430 | `}` |
|        - | 5431 | `/*` |
|        - | 5432 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - | 5433 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - | 5434 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - | 5435 | ` * build the SyBlob themselves and pass its data + length.` |
|        - | 5436 | ` *` |
|        - | 5437 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - | 5438 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - | 5439 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - | 5440 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - | 5441 | ` */` |
|   144814 | 5442 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|        - | 5443 | `	ph7_vm *pVm,` |
|        - | 5444 | `	const char *zClass,` |
|        - | 5445 | `	const char *zMsg,` |
|        - | 5446 | `	sxu32 nMsg` |
|        5 | 5447 | `){` |
|        - | 5448 | `	ph7_class *pClass;` |
|        - | 5449 | `	ph7_class_instance *pThis;` |
|        - | 5450 | `	ph7_class_method *pCons;` |
|        - | 5451 | `	VmFrame *pFrame;` |
|        - | 5452 | `	sxi32 rc;` |
|   144819 | 5453 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|   144819 | 5454 | `	if( pClass == 0 ){` |
|      ! 0 | 5455 | `		return SXERR_ABORT;` |
|        - | 5456 | `	}` |
|   144819 | 5457 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|   144819 | 5458 | `	if( pThis == 0 ){` |
|      ! 0 | 5459 | `		return SXERR_ABORT;` |
|        - | 5460 | `	}` |
|   144819 | 5461 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   144819 | 5462 | `	if( pCons && VmExcCtorEnter(pVm) ){` |
|        - | 5463 | `		ph7_value sArg;` |
|        - | 5464 | `		ph7_value *apArg[1];` |
|        - | 5465 | `		SyString sMsgStr;` |
|   144819 | 5466 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|   144819 | 5467 | `		PH7_MemObjInit(pVm,&sArg);` |
|   144819 | 5468 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|   144819 | 5469 | `		apArg[0] = &sArg;` |
|   144819 | 5470 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|   144819 | 5471 | `		PH7_MemObjRelease(&sArg);` |
|   144819 | 5472 | `		pVm->nExcCtorDepth--;` |
|    72407 | 5473 | `	}` |
|   144819 | 5474 | `	pFrame = pVm->pFrame;` |
|   144819 | 5475 | `	if( pFrame ){` |
|   144819 | 5476 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   144819 | 5477 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    72407 | 5478 | `	}` |
|   144819 | 5479 | `	rc = VmThrowException(pVm,pThis);` |
|   144819 | 5480 | `	PH7_ClassInstanceUnref(pThis);` |
|   144819 | 5481 | `	return rc;` |
|    72412 | 5482 | `}` |
|        - | 5483 | `/*` |
|        - | 5484 | ` * A native compare handler REFUSED the pair (ph7_class::xCmp wrote a class name` |
|        - | 5485 | ` * into its context, which PH7_ClassNativeCmp parked on the VM). php raises that` |
|        - | 5486 | ` * exception out of the comparison itself; PH7_MemObjCmp cannot, because it is` |
|        - | 5487 | ` * also the comparator sort(), in_array(), max() and switch drive, none of which` |
|        - | 5488 | ` * is a throw boundary. So the refusal waits here until a site that CAN route a` |
|        - | 5489 | ` * throw asks for it — the comparison opcodes and the switch arm raise it where` |
|        - | 5490 | ` * the expression's value would have landed, and the host-call boundary raises it` |
|        - | 5491 | ` * on the builtin's own context, which is where every other builtin throw is` |
|        - | 5492 | ` * reported from. Both doors clear it first: a raise that itself unwinds must not` |
|        - | 5493 | ` * leave the record standing for the next comparison to fire again.` |
|        - | 5494 | ` */` |
| 10635985 | 5495 | `PH7_PRIVATE int PH7_CmpRefusalPending(ph7_vm *pVm)` |
|        5 | 5496 | `{` |
| 10635990 | 5497 | `	return pVm->zCmpRefusalClass != 0;` |
|        5 | 5498 | `}` |
|       22 | 5499 | `PH7_PRIVATE void PH7_CmpRefusalNesting(ph7_vm *pVm)` |
|        2 | 5500 | `{` |
|        - | 5501 | `	static const char zMsg[] = "Nesting level too deep - recursive dependency?";` |
|       24 | 5502 | `	if( pVm->zCmpRefusalClass != 0 ){` |
|        - | 5503 | `		/* FIRST refusal wins, like every other writer of this record: a driver that` |
|        - | 5504 | `		 * keeps comparing after one must not overwrite the message the script sees. */` |
|      ! 0 | 5505 | `		return;` |
|        - | 5506 | `	}` |
|       24 | 5507 | `	pVm->zCmpRefusalClass = "Error";` |
|       24 | 5508 | `	SyMemcpy(zMsg,pVm->zCmpRefusalMsg,sizeof(zMsg));` |
|       13 | 5509 | `}` |
|     8015 | 5510 | `PH7_PRIVATE void PH7_CmpRefusalClear(ph7_vm *pVm)` |
|        5 | 5511 | `{` |
|     8020 | 5512 | `	pVm->zCmpRefusalClass = 0;` |
|     8020 | 5513 | `	pVm->zCmpRefusalMsg[0] = 0;` |
|     8020 | 5514 | `}` |
|       56 | 5515 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaise(ph7_vm *pVm)` |
|        3 | 5516 | `{` |
|       59 | 5517 | `	const char *zClass = pVm->zCmpRefusalClass;` |
|        - | 5518 | `	char zMsg[sizeof(pVm->zCmpRefusalMsg)];` |
|       59 | 5519 | `	if( zClass == 0 ){` |
|      ! 0 | 5520 | `		return SXRET_OK;` |
|        - | 5521 | `	}` |
|       59 | 5522 | `	SyMemcpy(pVm->zCmpRefusalMsg,zMsg,sizeof(zMsg));` |
|       59 | 5523 | `	PH7_CmpRefusalClear(&(*pVm));` |
|       59 | 5524 | `	return VmThrowFromVm(&(*pVm),zClass,zMsg,(sxu32)SyStrlen(zMsg));` |
|       31 | 5525 | `}` |
|       18 | 5526 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaiseCtx(ph7_context *pCtx)` |
|        2 | 5527 | `{` |
|       20 | 5528 | `	ph7_vm *pVm = pCtx->pVm;` |
|       20 | 5529 | `	const char *zClass = pVm->zCmpRefusalClass;` |
|        - | 5530 | `	char zMsg[sizeof(pVm->zCmpRefusalMsg)];` |
|       20 | 5531 | `	if( zClass == 0 ){` |
|      ! 0 | 5532 | `		return SXRET_OK;` |
|        - | 5533 | `	}` |
|       20 | 5534 | `	SyMemcpy(pVm->zCmpRefusalMsg,zMsg,sizeof(zMsg));` |
|       20 | 5535 | `	PH7_CmpRefusalClear(&(*pVm));` |
|       20 | 5536 | `	return PH7_VmThrowException(pCtx,zClass,"%s",zMsg);` |
|       11 | 5537 | `}` |
|        - | 5538 | `/*` |
|        - | 5539 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|        - | 5540 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|        - | 5541 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|        - | 5542 | ` *` |
|        - | 5543 | ` *   int/float/bool/null      arithmetic proceeds` |
|        - | 5544 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|        - | 5545 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|        - | 5546 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|        - | 5547 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|        - | 5548 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|        - | 5549 | ` *   object/resource          TypeError, naming the object's CLASS` |
|        - | 5550 | ` *` |
|        - | 5551 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|        - | 5552 | ` */` |
|        - | 5553 | `/*` |
|        - | 5554 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|        - | 5555 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|        - | 5556 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|        - | 5557 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|        - | 5558 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|        - | 5559 | ` * to depth 1).` |
|        - | 5560 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|        - | 5561 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list); bit8 =` |
|        - | 5562 | ` *   lead with the INTERNAL functions and methods currently running, which is` |
|        - | 5563 | ` *   php's exception trace and is NOT debug_backtrace() (php leaves its own` |
|        - | 5564 | ` *   internal frame off the array it hands back).` |
|        - | 5565 | ` * php's exception trace passes IGNORE_ARGS and no PROVIDE_OBJECT -- its default` |
|        - | 5566 | ` * frame shape is file/line/function[/class/type], matching the default` |
|        - | 5567 | ` * zend.exception_ignore_args=On.` |
|        - | 5568 | ` */` |
|  1466311 | 5569 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,ph7_value *pList)` |
|        5 | 5570 | `{` |
|        - | 5571 | `	SyString *pFile;` |
|        - | 5572 | `	VmFrame *pFrame;` |
|        - | 5573 | `	ph7_value *pValue;` |
|  1466316 | 5574 | `	sxi32 nDone = 0;` |
|  1466316 | 5575 | `	pValue = ph7_new_scalar(&(*pVm));` |
|  1466316 | 5576 | `	if( pValue == 0 ){` |
|      ! 0 | 5577 | `		return;` |
|        - | 5578 | `	}` |
|  1466316 | 5579 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - | 5580 | `	/* The INTERNAL functions and methods running right now come first: php gives each` |
|        - | 5581 | `	 * an execute_data of its own, so a throw raised inside a C body names that body as` |
|        - | 5582 | ``	 * frame #0 (`#0 file(line): str_repeat()`), and a native method names its class`` |
|        - | 5583 | ``	 * too (`#0 file(line): SplFileObject->__construct()`). Only the run entered from`` |
|        - | 5584 | `	 * THIS activation is emitted here -- one entered from further out is reached by the` |
|        - | 5585 | `	 * VM_FRAME_NATIVE_CALLER path below, which already renders it, and emitting it` |
|        - | 5586 | `	 * twice would double every array_map() line. Off for debug_backtrace(), which php` |
|        - | 5587 | `	 * leaves its own frame out of. */` |
|  1466316 | 5588 | `	if( (iOptions & 8) != 0 ){` |
|  1466140 | 5589 | `		VmNativeCall *pNat = pVm->pNativeCall;` |
|  1479249 | 5590 | `		while( pNat && pNat->pFrame == (void *)pVm->pFrame ){` |
|        - | 5591 | `			ph7_value *pNatEntry;` |
|    13114 | 5592 | `			if( iLimit != 0 && nDone >= iLimit ){` |
|      ! 0 | 5593 | `				break;` |
|        - | 5594 | `			}` |
|    13114 | 5595 | `			pNatEntry = ph7_new_array(&(*pVm));` |
|    13114 | 5596 | `			if( pNatEntry == 0 ){` |
|      ! 0 | 5597 | `				break;` |
|        - | 5598 | `			}` |
|    13114 | 5599 | `			nDone++;` |
|        - | 5600 | `			/* A call made from inside ANOTHER internal function has no source position` |
|        - | 5601 | `			 * at all, and php omits both keys rather than inventing one -- that is the` |
|        - | 5602 | ``			 * `#0 [internal function]: str_repeat()` under `array_map('str_repeat',…)`.`` |
|        - | 5603 | `			 * The predecessor sharing this record's activation is exactly that case. */` |
|    13114 | 5604 | `			if( pNat->pPrev == 0 \|\| pNat->pPrev->pFrame != pNat->pFrame ){` |
|    12896 | 5605 | `				SyString *pNatFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|    12896 | 5606 | `				if( pNatFile == 0 ){` |
|      ! 0 | 5607 | `					pNatFile = pFile;` |
|      ! 0 | 5608 | `				}` |
|    12896 | 5609 | `				if( pNatFile ){` |
|    12896 | 5610 | `					ph7_value_string(pValue,pNatFile->zString,(int)pNatFile->nByte);` |
|    12896 | 5611 | `					ph7_array_add_strkey_elem(pNatEntry,"file",pValue);` |
|    12896 | 5612 | `					ph7_value_reset_string_cursor(pValue);` |
|     6420 | 5613 | `				}` |
|    12896 | 5614 | `				ph7_value_int(pValue,(int)(pNat->nLine ? pNat->nLine : 1));` |
|    12896 | 5615 | `				ph7_array_add_strkey_elem(pNatEntry,"line",pValue);` |
|     6420 | 5616 | `			}` |
|    13114 | 5617 | `			ph7_value_string(pValue,pNat->pName->zString,(int)pNat->pName->nByte);` |
|    13114 | 5618 | `			ph7_array_add_strkey_elem(pNatEntry,"function",pValue);` |
|    13114 | 5619 | `			ph7_value_reset_string_cursor(pValue);` |
|    13114 | 5620 | `			if( pNat->pClass ){` |
|     5854 | 5621 | `				ph7_value_string(pValue,pNat->pClass->sName.zString,` |
|     3900 | 5622 | `					(int)pNat->pClass->sName.nByte);` |
|     3905 | 5623 | `				ph7_array_add_strkey_elem(pNatEntry,"class",pValue);` |
|     3905 | 5624 | `				ph7_value_reset_string_cursor(pValue);` |
|     3905 | 5625 | `				ph7_value_string(pValue,pNat->bStatic ? "::" : "->",2);` |
|     3905 | 5626 | `				ph7_array_add_strkey_elem(pNatEntry,"type",pValue);` |
|     3905 | 5627 | `				ph7_value_reset_string_cursor(pValue);` |
|     1949 | 5628 | `			}` |
|        - | 5629 | `			/* No 'args' even when they were asked for: php has no zval vector to show` |
|        - | 5630 | `			 * for an internal frame's arguments here, and no 'object' either. */` |
|    13114 | 5631 | `			ph7_array_add_elem(pList,0,pNatEntry);` |
|    13114 | 5632 | `			ph7_release_value(&(*pVm),pNatEntry);` |
|    13114 | 5633 | `			pNat = pNat->pPrev;` |
|        5 | 5634 | `		}` |
|   733040 | 5635 | `	}` |
|  1466316 | 5636 | `	pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|  2108897 | 5637 | `	while( pFrame ){` |
|        - | 5638 | `		/* The include/require/eval activations started from THIS frame come first:` |
|        - | 5639 | `		 * they are still running, so they are inner to whatever called the frame.` |
|        - | 5640 | `		 * php shows each as a frame whose function is the construct's name, whose` |
|        - | 5641 | `		 * file and line are the call site, and whose single argument is the unit --` |
|        - | 5642 | `		 * except for the innermost entry of the whole trace, which php leaves` |
|        - | 5643 | `		 * argument-less. Nothing else records them: an include shares its caller's` |
|        - | 5644 | `		 * variable scope and pushes no VmFrame. */` |
|  2108897 | 5645 | `		sxu32 nInc = SySetUsed(&pVm->aIncFrame);` |
|  2108897 | 5646 | `		if( (iOptions & 4) != 0 && nInc > 0 && nDone == 0 ){` |
|        - | 5647 | `			/* The caller is a compile-time refusal, which php raises BEFORE it pushes` |
|        - | 5648 | `			 * the include/require/eval activation that is loading this unit -- so that` |
|        - | 5649 | `			 * one innermost activation is not on php's trace. (A class REDECLARATION` |
|        - | 5650 | `			 * is php's run-time refusal and does carry it; the caller says which.) */` |
|        6 | 5651 | `			VmIncFrame *pTop = (VmIncFrame *)SySetAt(&pVm->aIncFrame,nInc - 1);` |
|        6 | 5652 | `			if( pTop && pTop->pFrame == (void *)pFrame ){` |
|        6 | 5653 | `				nInc--;` |
|        2 | 5654 | `			}` |
|        2 | 5655 | `		}` |
|  2206342 | 5656 | `		while( nInc > 0 ){` |
|    97464 | 5657 | `			VmIncFrame *pInc = (VmIncFrame *)SySetAt(&pVm->aIncFrame,--nInc);` |
|        - | 5658 | `			ph7_value *pIncEntry;` |
|    97464 | 5659 | `			if( pInc == 0 \|\| pInc->pFrame != (void *)pFrame ){` |
|    36636 | 5660 | `				continue;` |
|        - | 5661 | `			}` |
|    60832 | 5662 | `			if( iLimit != 0 && nDone >= iLimit ){` |
|       15 | 5663 | `				break;` |
|        - | 5664 | `			}` |
|    60818 | 5665 | `			pIncEntry = ph7_new_array(&(*pVm));` |
|    60818 | 5666 | `			if( pIncEntry == 0 ){` |
|      ! 0 | 5667 | `				break;` |
|        - | 5668 | `			}` |
|    60818 | 5669 | `			nDone++;` |
|    60818 | 5670 | `			if( SyStringLength(&pInc->sFile) > 0 ){` |
|    60818 | 5671 | `				ph7_value_string(pValue,pInc->sFile.zString,(int)pInc->sFile.nByte);` |
|    60818 | 5672 | `				ph7_array_add_strkey_elem(pIncEntry,"file",pValue);` |
|    60818 | 5673 | `				ph7_value_reset_string_cursor(pValue);` |
|    30406 | 5674 | `			}` |
|    60818 | 5675 | `			ph7_value_int(pValue,(int)pInc->nLine);` |
|    60818 | 5676 | `			ph7_array_add_strkey_elem(pIncEntry,"line",pValue);` |
|    60818 | 5677 | `			ph7_value_string(pValue,pInc->zName,-1);` |
|    60818 | 5678 | `			ph7_array_add_strkey_elem(pIncEntry,"function",pValue);` |
|    60818 | 5679 | `			ph7_value_reset_string_cursor(pValue);` |
|    60818 | 5680 | `			if( SyStringLength(&pInc->sPath) > 0 && ph7_array_count(pList) > 0 ){` |
|    36347 | 5681 | `				ph7_value *pArgs = ph7_new_array(&(*pVm));` |
|    36347 | 5682 | `				if( pArgs ){` |
|    36347 | 5683 | `					ph7_value *pArg = ph7_new_scalar(&(*pVm));` |
|    36347 | 5684 | `					if( pArg ){` |
|    36347 | 5685 | `						ph7_value_string(pArg,pInc->sPath.zString,(int)pInc->sPath.nByte);` |
|    36347 | 5686 | `						ph7_array_add_elem(pArgs,0,pArg);` |
|    36347 | 5687 | `						ph7_release_value(&(*pVm),pArg);` |
|    18172 | 5688 | `					}` |
|    36347 | 5689 | `					ph7_array_add_strkey_elem(pIncEntry,"args",pArgs);` |
|    36347 | 5690 | `					ph7_release_value(&(*pVm),pArgs);` |
|    18172 | 5691 | `				}` |
|    18172 | 5692 | `			}` |
|    60818 | 5693 | `			ph7_array_add_elem(pList,0,pIncEntry);` |
|    60818 | 5694 | `			ph7_release_value(&(*pVm),pIncEntry);` |
|        5 | 5695 | `		}` |
|        - | 5696 | `		/* $limit stops the walk after that many frames, 0 meaning "no limit".` |
|        - | 5697 | `		 * The test is php's own, on the NARROWED value: a limit that wraps` |
|        - | 5698 | `		 * negative reports NOTHING (frame 0 is already >= it), which is why` |
|        - | 5699 | `		 * debug_backtrace(0, PHP_INT_MAX) answers an empty array. */` |
|  2108897 | 5700 | `		if( iLimit != 0 && nDone >= iLimit ){` |
|       42 | 5701 | `			break;` |
|        - | 5702 | `		}` |
|  2108857 | 5703 | `		nDone++;` |
|  2108857 | 5704 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        - | 5705 | `		ph7_value *pEntry;` |
|  2108857 | 5706 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|        - | 5707 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|   733113 | 5708 | `			break;` |
|        - | 5709 | `		}` |
|   642586 | 5710 | `		pEntry = ph7_new_array(&(*pVm));` |
|   642586 | 5711 | `		if( pEntry == 0 ){` |
|      ! 0 | 5712 | `			break;` |
|        - | 5713 | `		}` |
|        - | 5714 | `		/* php's key order: file, line, function[, class, type][, object][, args]. */` |
|        - | 5715 | `		{` |
|        - | 5716 | `			/* A callback an INTERNAL function reached for has no userland call site, and` |
|        - | 5717 | ``			 * php says so by OMITTING both keys -- `array_keys()` on such a frame answers`` |
|        - | 5718 | ``			 * `['function']` alone, and the renderer prints `[internal function]: `. The`` |
|        - | 5719 | `			 * builtin that reached for it becomes a frame of its own, below. PHL gave the` |
|        - | 5720 | `			 * callback the builtin's OWN call site and emitted no second frame, so a` |
|        - | 5721 | `			 * program walking a trace saw one frame where php has two, and saw a file on` |
|        - | 5722 | `			 * a frame php leaves fileless. (Respect\Validation's exception walks a trace` |
|        - | 5723 | `			 * until a frame has no file; here it never stopped, and blamed an eval()'d` |
|        - | 5724 | `			 * unit for a throw written in a test file.) */` |
|   963929 | 5725 | `			SyString *pFrameFile = SyStringLength(&pFrame->sCallFile) > 0` |
|   642581 | 5726 | `				? &pFrame->sCallFile : pFile;` |
|   642586 | 5727 | `			if( pFrameFile && (pFrame->iFlags & VM_FRAME_NATIVE_CALLER) == 0 ){` |
|   642437 | 5728 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|   642437 | 5729 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|   642437 | 5730 | `				ph7_value_reset_string_cursor(pValue);` |
|   321165 | 5731 | `			}` |
|        - | 5732 | `		}` |
|   642586 | 5733 | `		if( (pFrame->iFlags & VM_FRAME_NATIVE_CALLER) == 0 ){` |
|   642437 | 5734 | `			ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|   642437 | 5735 | `			ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|   321165 | 5736 | `		}` |
|        - | 5737 | `		{` |
|   642586 | 5738 | `			const char *zDisp = 0;` |
|   642586 | 5739 | `			int nDisp = PH7_VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|   642586 | 5740 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|        - | 5741 | `		}` |
|   642586 | 5742 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|   642586 | 5743 | `		ph7_value_reset_string_cursor(pValue);` |
|        - | 5744 | `		{` |
|        - | 5745 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|        - | 5746 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|        - | 5747 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|        - | 5748 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|        - | 5749 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|        - | 5750 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|        - | 5751 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|   642586 | 5752 | `			SyString *pClsName = 0;` |
|   642586 | 5753 | `			const char *zType = "->";` |
|   642586 | 5754 | `			int bStatic = 0;` |
|   642586 | 5755 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        - | 5756 | `				/* php's separator says what the CALLEE is, not how the caller` |
|        - | 5757 | ``				 * happened to reach it: a static method is `::` even when the`` |
|        - | 5758 | ``				 * calling frame has a $this bound (`self::s()` from inside an`` |
|        - | 5759 | ``				 * instance method), which the pThis test reported as `->`.`` |
|        - | 5760 | `				 * ph7_class_method embeds its ph7_vm_func FIRST, so the method's` |
|        - | 5761 | `				 * own flags are one cast away. */` |
|   500691 | 5762 | `				ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|   500691 | 5763 | `				bStatic = (((ph7_class_method *)pFunc)->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|   500691 | 5764 | `				if( (pDecl->iFlags & PH7_CLASS_TRAIT) != 0 && pFrame->pSelfClass ){` |
|        - | 5765 | `					/* php COMPOSES a trait method into the using class, so a frame running` |
|        - | 5766 | `					 * one reports that class and never the trait. The class the call was` |
|        - | 5767 | `					 * made THROUGH names it -- the receiver's for an instance call, the` |
|        - | 5768 | `					 * named one for a static call -- and its ancestry is walked, so` |
|        - | 5769 | ``					 * `class Base { use T; } class Kid extends Base {}` is Base from a Kid`` |
|        - | 5770 | `					 * instance, where php composed the method. */` |
|       22 | 5771 | `					pDecl = PH7_VmTraitUsingClass(&(*pVm),pDecl,pFrame->pSelfClass);` |
|       10 | 5772 | `				}` |
|   500691 | 5773 | `				pClsName = &pDecl->sName;` |
|   500691 | 5774 | `				zType = bStatic ? "::" : "->";` |
|   392243 | 5775 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|        3 | 5776 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|        1 | 5777 | `			}` |
|   642586 | 5778 | `			if( pClsName ){` |
|   500693 | 5779 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|   500693 | 5780 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|   500693 | 5781 | `				ph7_value_reset_string_cursor(pValue);` |
|   500693 | 5782 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|   500693 | 5783 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|   500693 | 5784 | `				ph7_value_reset_string_cursor(pValue);` |
|   500688 | 5785 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pFrame->pThis` |
|       36 | 5786 | `				 && !bStatic ){` |
|       28 | 5787 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|       28 | 5788 | `					if( pObjVal ){` |
|       28 | 5789 | `						pFrame->pThis->iRef++;` |
|       28 | 5790 | `						pObjVal->x.pOther = pFrame->pThis;` |
|       28 | 5791 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|       28 | 5792 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|       28 | 5793 | `						ph7_release_value(&(*pVm),pObjVal);` |
|       13 | 5794 | `					}` |
|       13 | 5795 | `				}` |
|   250344 | 5796 | `			}` |
|        - | 5797 | `		}` |
|   642586 | 5798 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|      127 | 5799 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|      127 | 5800 | `			if( pArg ){` |
|        - | 5801 | `				/* The arguments the caller actually PASSED, which is not the same` |
|        - | 5802 | `				 * list as the frame's installed slots -- see PH7_VmFrameActualArgs` |
|        - | 5803 | `				 * (shared with func_get_args()). */` |
|      127 | 5804 | `				PH7_VmFrameActualArgs(&(*pVm),pFrame,pArg);` |
|      127 | 5805 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|      127 | 5806 | `				ph7_release_value(&(*pVm),pArg);` |
|       62 | 5807 | `			}` |
|       62 | 5808 | `		}` |
|   642586 | 5809 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|   642586 | 5810 | `		ph7_release_value(&(*pVm),pEntry);` |
|   642586 | 5811 | `		if( (pFrame->iFlags & VM_FRAME_NATIVE_CALLER) != 0 && pFrame->pNativeCaller ){` |
|        - | 5812 | `			/* php gives the INTERNAL function that reached for the callback a frame of` |
|        - | 5813 | `			 * its own, and that one carries the userland call site the callback's frame` |
|        - | 5814 | ``			 * just declined. It has `file`, `line` and `function` and nothing else --`` |
|        - | 5815 | `			 * no class, no type, no object, and no args even when args were asked for` |
|        - | 5816 | `			 * (php has no zval array to show for an internal frame's arguments here).` |
|        - | 5817 | `			 *` |
|        - | 5818 | `			 * Not every such dispatch gets one: php's two FORWARDS, call_user_func()` |
|        - | 5819 | `			 * and call_user_func_array(), are ELIDED BY THE COMPILER when written` |
|        - | 5820 | `			 * literally, so the callback's frame is the caller's own -- and those` |
|        - | 5821 | `			 * spellings never reach here, because the forwards pass the caller's mode` |
|        - | 5822 | `` 			 * through PH7_VmCallUserFunctionWithMap and set no latch. A `$n('cuf')($c)` `` |
|        - | 5823 | `			 * through a variable is NOT elided, does set the latch, and does get this` |
|        - | 5824 | `			 * frame, exactly as php's does. */` |
|        - | 5825 | `			ph7_value *pNat;` |
|      151 | 5826 | `			if( iLimit != 0 && nDone >= iLimit ){` |
|      ! 0 | 5827 | `				break;` |
|        - | 5828 | `			}` |
|      151 | 5829 | `			pNat = ph7_new_array(&(*pVm));` |
|      151 | 5830 | `			if( pNat == 0 ){` |
|      ! 0 | 5831 | `				break;` |
|        - | 5832 | `			}` |
|      151 | 5833 | `			nDone++;` |
|        - | 5834 | `			{` |
|      225 | 5835 | `				SyString *pNatFile = SyStringLength(&pFrame->sCallFile) > 0` |
|      146 | 5836 | `					? &pFrame->sCallFile : pFile;` |
|      151 | 5837 | `				if( pNatFile ){` |
|      151 | 5838 | `					ph7_value_string(pValue,pNatFile->zString,(int)pNatFile->nByte);` |
|      151 | 5839 | `					ph7_array_add_strkey_elem(pNat,"file",pValue);` |
|      151 | 5840 | `					ph7_value_reset_string_cursor(pValue);` |
|       72 | 5841 | `				}` |
|        - | 5842 | `			}` |
|      151 | 5843 | `			ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|      151 | 5844 | `			ph7_array_add_strkey_elem(pNat,"line",pValue);` |
|      223 | 5845 | `			ph7_value_string(pValue,pFrame->pNativeCaller->zString,` |
|      146 | 5846 | `				(int)pFrame->pNativeCaller->nByte);` |
|      151 | 5847 | `			ph7_array_add_strkey_elem(pNat,"function",pValue);` |
|      151 | 5848 | `			ph7_value_reset_string_cursor(pValue);` |
|      151 | 5849 | `			ph7_array_add_elem(pList,0,pNat);` |
|      151 | 5850 | `			ph7_release_value(&(*pVm),pNat);` |
|       72 | 5851 | `		}` |
|   642586 | 5852 | `		pFrame = pFrame->pParent ? VmSkipExceptionFrames(pFrame->pParent) : 0;` |
|        5 | 5853 | `	}` |
|  1466316 | 5854 | `	ph7_release_value(&(*pVm),pValue);` |
|   733133 | 5855 | `}` |
|        - | 5856 | `/*` |
|        - | 5857 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|        - | 5858 | ` *` |
|        - | 5859 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|        - | 5860 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|        - | 5861 | ` * calls parent::__construct still reports the right position. The embedded` |
|        - | 5862 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|        - | 5863 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|        - | 5864 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|        - | 5865 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|        - | 5866 | ` */` |
|  1627223 | 5867 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|        5 | 5868 | `{` |
|        - | 5869 | `	static const char *azField[] = { "file", "line", "trace" };` |
|        - | 5870 | `	ph7_class *pThrowable;` |
|        - | 5871 | `	SyString *pFile;` |
|        - | 5872 | `	SyString *pSiteFile;` |
|        - | 5873 | `	sxu32 nPreLine;` |
|        - | 5874 | `	sxu32 n;` |
|  1627228 | 5875 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|      ! 0 | 5876 | `		return;` |
|        - | 5877 | `	}` |
|  1627228 | 5878 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|  1627228 | 5879 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|   161093 | 5880 | `		return;` |
|        - | 5881 | `	}` |
|  1466140 | 5882 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - | 5883 | ``	/* getFile() is the file the `new` is WRITTEN in, which is the same question`` |
|        - | 5884 | `	 * __FILE__ asks and PH7_VmExecutingUnitFile is the one answer to: the running` |
|        - | 5885 | `	 * function's DEFINING file, the include-stack top for code at a loaded unit's` |
|        - | 5886 | `	 * top level, and an eval()'d chunk's own name inside one. This asked it by` |
|        - | 5887 | `	 * hand and knew only the first half, so an undefined function called at the` |
|        - | 5888 | `	 * top level of a file some METHOD included was reported in the method's file` |
|        - | 5889 | `	 * (PHPUnit's TestSuiteLoader is exactly that shape, and named itself instead` |
|        - | 5890 | `	 * of the test file it had just loaded). */` |
|  1466140 | 5891 | `	pSiteFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|  1466140 | 5892 | `	if( pSiteFile == 0 ){` |
|      ! 0 | 5893 | `		pSiteFile = pFile;` |
|      ! 0 | 5894 | `	}` |
|        - | 5895 | `	/* ...unless a prelude builtin is what is running: php has no frame for one,` |
|        - | 5896 | `	 * so the throw it makes is reported at the call the program wrote. Without` |
|        - | 5897 | `	 * this every exception these ~24 builtins raise answered getLine() 1 -- the` |
|        - | 5898 | `	 * whole embedded chunk is a single source line. */` |
|        - | 5899 | `	{` |
|  1466140 | 5900 | `		SyString *pPreFile = 0;` |
|  1466140 | 5901 | `		nPreLine = 0;` |
|  1466140 | 5902 | `		if( PH7_VmPreludeBuiltinFrame(&(*pVm),&pPreFile,&nPreLine) ){` |
|      120 | 5903 | `			if( pPreFile ){` |
|      120 | 5904 | `				pSiteFile = pPreFile;` |
|       59 | 5905 | `			}` |
|      120 | 5906 | `			if( nPreLine < 1 ){` |
|      ! 0 | 5907 | `				nPreLine = 1;` |
|      ! 0 | 5908 | `			}` |
|       59 | 5909 | `		}` |
|        - | 5910 | `	}` |
|  5864545 | 5911 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|        - | 5912 | `		SyHashEntry *pEntry;` |
|        - | 5913 | `		VmClassAttr *pVmAttr;` |
|        - | 5914 | `		ph7_value *pAttrValue;` |
|  4398410 | 5915 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
|  4398410 | 5916 | `		if( pEntry == 0 ){` |
|      ! 0 | 5917 | `			continue;` |
|        - | 5918 | `		}` |
|  4398410 | 5919 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  4398410 | 5920 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  4398410 | 5921 | `		if( pAttrValue == 0 ){` |
|      ! 0 | 5922 | `			continue;` |
|        - | 5923 | `		}` |
|        - | 5924 | ``		/* The stamp IS this slot's initialization. `Error` declares`` |
|        - | 5925 | ``		 * `protected int $line` with no default (php's stub, and php's own`` |
|        - | 5926 | `		 * Reflection agrees), so its slot starts UNINITIALIZED -- and writing` |
|        - | 5927 | `		 * it here without clearing that flag left every Error, TypeError and` |
|        - | 5928 | ``		 * ValueError reporting `uninitialized(int)` in var_dump, one property`` |
|        - | 5929 | `		 * short in the (array) cast, get_object_vars(), get_mangled_object_vars(),` |
|        - | 5930 | `		 * json_encode(), serialize() and array_walk(), while getLine() answered` |
|        - | 5931 | `		 * the real number. Exception hid it by declaring a default. */` |
|  4398410 | 5932 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|  4398410 | 5933 | `		if( n == 0 ){` |
|  1466140 | 5934 | `			if( pSiteFile ){` |
|  1466140 | 5935 | `				PH7_MemObjRelease(pAttrValue);` |
|  1466140 | 5936 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|   733045 | 5937 | `			}` |
|  3665315 | 5938 | `		}else if( n == 1 ){` |
|        - | 5939 | `			/* nLazyInitLine wins while a lazily-evaluated class initializer runs its` |
|        - | 5940 | `			 * OWN bytecode: php evaluates that expression AT THE ACCESS, so` |
|        - | 5941 | ``			 * `class C { public static $s = UNDEF; } ... C::$s;` reports the`` |
|        - | 5942 | `			 * access's line, not the declaration's. The depth test keeps the window` |
|        - | 5943 | `			 * off everything the initializer calls — an autoloader, a nested` |
|        - | 5944 | `			 * constant's evaluation — which report their own lines in both engines.` |
|        - | 5945 | `			 * (Enum cases never set it: php reports the CASE's own line there, and` |
|        - | 5946 | `			 * PHL already matches.) */` |
|  2932216 | 5947 | `			sxu32 nLine = nPreLine ? nPreLine` |
|  2932094 | 5948 | `				: (pVm->nLazyInitLine && pVm->nVmExecDepth == pVm->nLazyInitDepth)` |
|       40 | 5949 | `				? pVm->nLazyInitLine` |
|  1466018 | 5950 | `				: (pVm->nCurLine ? pVm->nCurLine : 1);` |
|  1466140 | 5951 | `			PH7_MemObjRelease(pAttrValue);` |
|  1466140 | 5952 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)nLine);` |
|   733045 | 5953 | `		}else{` |
|        - | 5954 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|        - | 5955 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|        - | 5956 | `			 * shared walk with IGNORE_ARGS + no PROVIDE_OBJECT to match php's default` |
|        - | 5957 | `			 * exception-trace shape (file/line/function[/class/type]). */` |
|  1466140 | 5958 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
|  1466140 | 5959 | `			if( pList == 0 ){` |
|      ! 0 | 5960 | `				continue;` |
|        - | 5961 | `			}` |
|  1466140 | 5962 | `			VmBuildBacktrace(&(*pVm),2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/\|8,0,pList);` |
|        - | 5963 | `			/* Building the trace reserves new memobjs, which used to realloc` |
|        - | 5964 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO the pool,` |
|        - | 5965 | `			 * from PH7_ClassInstanceExtractAttrValue above). Redundant since P1` |
|        - | 5966 | `			 * (fixed segments); left for the harvest sweep. */` |
|  1466140 | 5967 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  1466140 | 5968 | `			if( pAttrValue ){` |
|  1466140 | 5969 | `				PH7_MemObjRelease(pAttrValue);` |
|  1466140 | 5970 | `				PH7_MemObjStore(pList,pAttrValue);` |
|   733040 | 5971 | `			}` |
|  1466140 | 5972 | `			ph7_release_value(&(*pVm),pList);` |
|        - | 5973 | `		}` |
|  2199125 | 5974 | `	}` |
|   813449 | 5975 | `}` |
|     6900 | 5976 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|        5 | 5977 | `{` |
|     6905 | 5978 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       56 | 5979 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       56 | 5980 | `		if( pInst && pInst->pClass ){` |
|       56 | 5981 | `			return pInst->pClass->sName.zString;` |
|        - | 5982 | `		}` |
|      ! 0 | 5983 | `	}` |
|     6851 | 5984 | `	return ph7_type_name(pVal);` |
|     3455 | 5985 | `}` |
|        - | 5986 | `/*` |
|        - | 5987 | ` * php's VALUE name (zend_zval_value_name, 8.3+) rather than its TYPE name: a` |
|        - | 5988 | `` * boolean is named by the value it holds — `false` / `true` — everywhere php`` |
|        - | 5989 | ` * describes an operand it could not use as one ("on false", "false given").` |
|        - | 5990 | ` * Deliberately NOT the whole diagnostic surface: the ZPP messages,` |
|        - | 5991 | `` * `Value of type bool is not callable` and `Unsupported operand types: bool +`` |
|        - | 5992 | `` * array` stay on the TYPE name, which is why this sits beside VmArithTypeName`` |
|        - | 5993 | ` * instead of replacing it.` |
|        - | 5994 | ` */` |
|      128 | 5995 | `PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal)` |
|        3 | 5996 | `{` |
|      131 | 5997 | `	if( (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_OBJ)) == MEMOBJ_BOOL ){` |
|       19 | 5998 | `		return pVal->x.iVal ? "true" : "false";` |
|        - | 5999 | `	}` |
|      113 | 6000 | `	return VmArithTypeName(&(*pVal));` |
|       67 | 6001 | `}` |
|        - | 6002 | `/*` |
|        - | 6003 | ` * php's ++/-- operand contract: an array, object or resource operand is a` |
|        - | 6004 | ` * TypeError ("Cannot increment array", "Cannot decrement P", "Cannot increment` |
|        - | 6005 | ` * resource"), never the silent no-op PH7 used to perform. Word it once here so` |
|        - | 6006 | ` * the plain opcode path and the hooked-property path cannot drift apart.` |
|        - | 6007 | ` * bIncr selects the verb; the value itself is left untouched by php.` |
|        - | 6008 | ` */` |
|       40 | 6009 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut)` |
|        1 | 6010 | `{` |
|       41 | 6011 | `	const char *zVerb = bIncr ? "increment" : "decrement";` |
|       41 | 6012 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       19 | 6013 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       19 | 6014 | `		if( pInst && pInst->pClass ){` |
|       19 | 6015 | `			SyBlobFormat(pMsgOut,"Cannot %s %z",zVerb,&pInst->pClass->sDisp);` |
|       19 | 6016 | `			return;` |
|        - | 6017 | `		}` |
|      ! 0 | 6018 | `	}` |
|       23 | 6019 | `	SyBlobFormat(pMsgOut,"Cannot %s %s",zVerb,ph7_type_name(pVal));` |
|       21 | 6020 | `}` |
|        - | 6021 | `/*` |
|        - | 6022 | `` * php's `&`, `\|` and `^` over TWO STRINGS: a per-BYTE operation whose result is`` |
|        - | 6023 | `` * a binary STRING, not an integer — `"abc" & "abd"` is "ab`", and PHL answered`` |
|        - | 6024 | `` * int(0) for every such pair because it cast both sides to int first. `&` and`` |
|        - | 6025 | `` * `^` stop at the SHORTER operand; `\|` runs to the LONGER one, the missing bytes`` |
|        - | 6026 | ` * reading as 0, so the longer operand's tail is copied verbatim. cOp is '&', '\|'` |
|        - | 6027 | ` * or '^'; the result is appended to *pOut (the caller owns it).` |
|        - | 6028 | ` */` |
|      250 | 6029 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut)` |
|        1 | 6030 | `{` |
|      251 | 6031 | `	const unsigned char *zL = (const unsigned char *)SyBlobData(&pLeft->sBlob);` |
|      251 | 6032 | `	const unsigned char *zR = (const unsigned char *)SyBlobData(&pRight->sBlob);` |
|      251 | 6033 | `	sxu32 nL = SyBlobLength(&pLeft->sBlob);` |
|      251 | 6034 | `	sxu32 nR = SyBlobLength(&pRight->sBlob);` |
|      251 | 6035 | `	sxu32 nMin = nL < nR ? nL : nR;` |
|        - | 6036 | `	sxu32 i;` |
|      523 | 6037 | `	for( i = 0 ; i < nMin ; ++i ){` |
|        - | 6038 | `		unsigned char c;` |
|      273 | 6039 | `		if( cOp == '\|' ){` |
|       93 | 6040 | `			c = (unsigned char)(zL[i] \| zR[i]);` |
|      227 | 6041 | `		}else if( cOp == '^' ){` |
|       89 | 6042 | `			c = (unsigned char)(zL[i] ^ zR[i]);` |
|       45 | 6043 | `		}else{` |
|       93 | 6044 | `			c = (unsigned char)(zL[i] & zR[i]);` |
|        - | 6045 | `		}` |
|      273 | 6046 | `		SyBlobAppend(pOut,(const void *)&c,sizeof(char));` |
|      137 | 6047 | `	}` |
|      251 | 6048 | `	if( cOp == '\|' && nL != nR ){` |
|       63 | 6049 | `		const unsigned char *zTail = nL > nR ? &zL[nMin] : &zR[nMin];` |
|       63 | 6050 | `		sxu32 nTail = (nL > nR ? nL : nR) - nMin;` |
|       63 | 6051 | `		SyBlobAppend(pOut,(const void *)zTail,nTail);` |
|       31 | 6052 | `	}` |
|      251 | 6053 | `}` |
|        - | 6054 | `/*` |
|        - | 6055 | ` * php's operand name in the bitwise-not diagnostic. It is NOT the arithmetic` |
|        - | 6056 | `` * type name: zend spells a BOOL there as the literal `true`/`false` (where`` |
|        - | 6057 | `` * "Unsupported operand types" says `bool`), and an object as its CLASS. Only the`` |
|        - | 6058 | `` * types `~` refuses reach this — a string is complemented byte-wise and a`` |
|        - | 6059 | ` * number is complemented as an integer — and an UNINITIALIZED slot is php's` |
|        - | 6060 | ` * null, which is what an undefined variable answers.` |
|        - | 6061 | ` */` |
|       26 | 6062 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut)` |
|        1 | 6063 | `{` |
|       27 | 6064 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       11 | 6065 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       11 | 6066 | `		if( pInst && pInst->pClass ){` |
|       11 | 6067 | `			SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %z",&pInst->pClass->sDisp);` |
|       11 | 6068 | `			return;` |
|        - | 6069 | `		}` |
|      ! 0 | 6070 | `	}` |
|       17 | 6071 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        7 | 6072 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",` |
|        4 | 6073 | `			pVal->x.iVal ? "true" : "false");` |
|       15 | 6074 | `	}else if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_RES\|MEMOBJ_OBJ) ){` |
|       11 | 6075 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",ph7_type_name(pVal));` |
|        6 | 6076 | `	}else{` |
|        3 | 6077 | `		SyBlobAppend(pMsgOut,"Cannot perform bitwise not on null",` |
|        - | 6078 | `			sizeof("Cannot perform bitwise not on null")-1);` |
|        - | 6079 | `	}` |
|       14 | 6080 | `}` |
|        - | 6081 | `/*` |
|        - | 6082 | `` * php compiles unary minus as `$x * -1` and unary plus as `$x * 1`, so both`` |
|        - | 6083 | `` * inherit the MULTIPLICATION operand contract -- message included: `-$o` is`` |
|        - | 6084 | `` * "Unsupported operand types: P * int", `-[1]` is "array * int" and `-"abc"` is`` |
|        - | 6085 | ` * "string * int", while a leading-numeric string ("-\"12abc\"") warns and` |
|        - | 6086 | ` * computes with the prefix. Classify pVal against that contract.` |
|        - | 6087 | ` */` |
|   107530 | 6088 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut)` |
|        5 | 6089 | `{` |
|        - | 6090 | `	ph7_value sInt;` |
|        - | 6091 | `	sxi32 rc;` |
|   107535 | 6092 | `	PH7_MemObjInitFromInt(&(*pVm),&sInt,1);` |
|   107535 | 6093 | `	rc = VmArithOperandCheck(&(*pVm),pVal,&sInt,"*",pMsgOut);` |
|   107535 | 6094 | `	PH7_MemObjRelease(&sInt);` |
|   107535 | 6095 | `	return rc;` |
|        5 | 6096 | `}` |
|        - | 6097 | `/*` |
|        - | 6098 | ` * One arithmetic operator's whole prologue: php's do_operation handler first,` |
|        - | 6099 | ` * then the ordinary operand contract.` |
|        - | 6100 | ` *` |
|        - | 6101 | ` * php asks the LEFT operand's class for a handler and falls back to the RIGHT` |
|        - | 6102 | `` * one's, which is why an `int + Number` works as well as a `Number + int`. A`` |
|        - | 6103 | ` * handler that answers writes into pDest (the slot the opcode was going to` |
|        - | 6104 | ` * leave its result in), so the caller's only job is to skip the numeric` |
|        - | 6105 | ` * arithmetic. A handler that REFUSES hands back an exception class and a` |
|        - | 6106 | ` * message, and the caller throws them where it would have thrown the TypeError` |
|        - | 6107 | ` * -- after settling the operand stack.` |
|        - | 6108 | ` */` |
|        - | 6109 | ``/* Does this value's class declare php's do_operation? `++`/`--` ask before they`` |
|        - | 6110 | ` * refuse an object, since everything below that refusal is numeric. */` |
|  1062720 | 6111 | `PH7_PRIVATE int PH7_ValueHasArithHandler(ph7_value *pVal)` |
|        5 | 6112 | `{` |
|        - | 6113 | `	ph7_class_instance *pInst;` |
|  1062725 | 6114 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 ){` |
|  1062689 | 6115 | `		return 0;` |
|        - | 6116 | `	}` |
|       37 | 6117 | `	pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       37 | 6118 | `	return pInst->pClass != 0 && pInst->pClass->xArith != 0;` |
|   532397 | 6119 | `}` |
|   602731 | 6120 | `PH7_PRIVATE int VmArithOperandStep(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,` |
|        - | 6121 | `	const char *zOp,ph7_value *pDest,const char **pzClass,SyBlob *pMsgOut)` |
|        5 | 6122 | `{` |
|   602736 | 6123 | `	ph7_class_instance *pInst = 0;` |
|        - | 6124 | `	int i;` |
|   602736 | 6125 | `	*pzClass = "TypeError";` |
|  1808068 | 6126 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  1205405 | 6127 | `		ph7_value *pSide = i == 0 ? pLeft : pRight;` |
|  1205405 | 6128 | `		if( (pSide->iFlags & MEMOBJ_OBJ) != 0 && pSide->x.pOther ){` |
|       79 | 6129 | `			ph7_class_instance *pCand = (ph7_class_instance *)pSide->x.pOther;` |
|       79 | 6130 | `			if( pCand->pClass && pCand->pClass->xArith ){` |
|       70 | 6131 | `				pInst = pCand;` |
|       70 | 6132 | `				break;` |
|        - | 6133 | `			}` |
|        4 | 6134 | `		}` |
|   604044 | 6135 | `	}` |
|   602736 | 6136 | `	if( pInst ){` |
|        - | 6137 | `		PH7_NativeArithCtx sCtx;` |
|        - | 6138 | `		ph7_value sRes;` |
|       70 | 6139 | `		PH7_MemObjInit(&(*pVm),&sRes);` |
|       70 | 6140 | `		sCtx.zOp = zOp;` |
|       70 | 6141 | `		sCtx.pLeft = pLeft;` |
|       70 | 6142 | `		sCtx.pRight = pRight;` |
|       70 | 6143 | `		sCtx.pResult = &sRes;` |
|       70 | 6144 | `		sCtx.bHandled = 0;` |
|       70 | 6145 | `		sCtx.zThrowClass = 0;` |
|       70 | 6146 | `		sCtx.zThrowMsg[0] = 0;` |
|       70 | 6147 | `		pInst->pClass->xArith(&(*pVm),pInst,&sCtx);` |
|       70 | 6148 | `		if( sCtx.zThrowClass ){` |
|       11 | 6149 | `			PH7_MemObjRelease(&sRes);` |
|       11 | 6150 | `			*pzClass = sCtx.zThrowClass;` |
|       11 | 6151 | `			SyBlobAppend(pMsgOut,sCtx.zThrowMsg,(sxu32)SyStrlen(sCtx.zThrowMsg));` |
|       32 | 6152 | `			return PH7_ARITH_REFUSED;` |
|        - | 6153 | `		}` |
|       60 | 6154 | `		if( sCtx.bHandled ){` |
|       44 | 6155 | `			PH7_MemObjStore(&sRes,pDest);` |
|       44 | 6156 | `			PH7_MemObjRelease(&sRes);` |
|       44 | 6157 | `			return PH7_ARITH_HANDLED;` |
|        - | 6158 | `		}` |
|       18 | 6159 | `		PH7_MemObjRelease(&sRes);` |
|        8 | 6160 | `	}` |
|   602684 | 6161 | `	if( VmArithOperandCheck(&(*pVm),pLeft,pRight,zOp,pMsgOut) != SXRET_OK ){` |
|     1890 | 6162 | `		return PH7_ARITH_REFUSED;` |
|        - | 6163 | `	}` |
|   600796 | 6164 | `	return PH7_ARITH_ORDINARY;` |
|   302057 | 6165 | `}` |
|   724645 | 6166 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|        5 | 6167 | `{` |
|   724650 | 6168 | `	int bBadL = 0, bBadR = 0;` |
|        - | 6169 | `	int i;` |
|        - | 6170 | `	ph7_value *apOperand[2];` |
|   724650 | 6171 | `	apOperand[0] = pLeft;` |
|   724650 | 6172 | `	apOperand[1] = pRight;` |
|        - | 6173 | `	/* array + array is php's union operator, not arithmetic */` |
|   724645 | 6174 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|   515488 | 6175 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|     6432 | 6176 | `		return SXRET_OK;` |
|        - | 6177 | `	}` |
|  2149383 | 6178 | `	for( i = 0 ; i < 2 ; ++i ){` |
|  1434545 | 6179 | `		ph7_value *pVal = apOperand[i];` |
|  1434545 | 6180 | `		int bBad = 0;` |
|  1434545 | 6181 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     1253 | 6182 | `			bBad = 1;` |
|     1253 | 6183 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       55 | 6184 | `				ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|        - | 6185 | `				/* A class whose cast_object really answers a number is an operand` |
|        - | 6186 | ``				 * php accepts: `$xml->qty + 1` adds to the element's text. */`` |
|       55 | 6187 | `				if( pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass) ){` |
|        5 | 6188 | `					bBad = 0;` |
|        2 | 6189 | `				}` |
|       29 | 6190 | `			}` |
|  1433920 | 6191 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     4623 | 6192 | `			const char *zTail = 0;` |
|     4623 | 6193 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|     4623 | 6194 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|        - | 6195 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|     2135 | 6196 | `				bBad = 1;` |
|     1068 | 6197 | `			}else{` |
|        - | 6198 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|        - | 6199 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|        - | 6200 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|     2517 | 6201 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|       30 | 6202 | `					zTail++;` |
|        2 | 6203 | `				}` |
|     2489 | 6204 | `				if( zTail < zEnd ){` |
|     1187 | 6205 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|      592 | 6206 | `				}` |
|        - | 6207 | `			}` |
|     2310 | 6208 | `		}` |
|  1434545 | 6209 | `		if( bBad ){` |
|     3382 | 6210 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|        - | 6211 | `			/* php converts the operands one at a time and STOPS at the first one it` |
|        - | 6212 | `			 * refuses — the second is never looked at, so it never says anything about` |
|        - | 6213 | ``			 * it. `"abc" + "5x"` is the TypeError alone, where walking both operands`` |
|        - | 6214 | ``			 * first announced `A non-numeric value encountered` for the "5x" php never`` |
|        - | 6215 | ``			 * reached. The other order is unaffected: `"5x" + "abc"` warns for the left`` |
|        - | 6216 | `			 * operand and then throws, in both engines. */` |
|     3382 | 6217 | `			break;` |
|        - | 6218 | `		}` |
|   716939 | 6219 | `	}` |
|   718223 | 6220 | `	if( bBadL \|\| bBadR ){` |
|        - | 6221 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|        - | 6222 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|        - | 6223 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|     5072 | 6224 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|     1690 | 6225 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|     3382 | 6226 | `		return SXERR_INVALID;` |
|        - | 6227 | `	}` |
|   714843 | 6228 | `	return SXRET_OK;` |
|   362999 | 6229 | `}` |
|        - | 6230 | `/*` |
|        - | 6231 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - | 6232 | ` * iCode becomes the exception's $code (php's second constructor argument);` |
|        - | 6233 | ` * pass 0 for the engine errors that leave it at its default.` |
|        - | 6234 | ` */` |
|    12879 | 6235 | `static sxi32 VmThrowInternalAp(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,va_list ap)` |
|        5 | 6236 | `{` |
|        - | 6237 | `	ph7_vm *pVm;` |
|        - | 6238 | `	ph7_class *pClass;` |
|        - | 6239 | `	ph7_class_instance *pThis;` |
|        - | 6240 | `	ph7_class_method *pCons;` |
|        - | 6241 | `	ph7_value sArg,sCode;` |
|        - | 6242 | `	ph7_value *apArg[2];` |
|        - | 6243 | `	SyBlob sMsg;` |
|        - | 6244 | `	SyString sMsgStr;` |
|        - | 6245 | `	VmFrame *pFrame;` |
|        - | 6246 | `	sxi32 rc;` |
|        - | 6247 |  |
|    12884 | 6248 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 | 6249 | `		return PH7_ABORT;` |
|        - | 6250 | `	}` |
|    12884 | 6251 | `	pVm = pCtx->pVm;` |
|    12884 | 6252 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 | 6253 | `		zClass = "Error";` |
|      ! 0 | 6254 | `	}` |
|        - | 6255 | `	/* The two degraded paths below cannot build the exception object, so they report an` |
|        - | 6256 | `	 * uncaught fatal with a trace instead. They record whatever status that reporting` |
|        - | 6257 | `	 * settled on, so the "nThrowRc mirrors what this function returned" invariant holds` |
|        - | 6258 | `	 * on every exit and a caller that returns PH7_OK anyway still cannot resume past a` |
|        - | 6259 | `	 * reported error (VmHostFuncThrowRc). */` |
|    12884 | 6260 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|    12884 | 6261 | `	if( pClass == 0 ){` |
|      ! 0 | 6262 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - | 6263 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 | 6264 | `			zClass` |
|        - | 6265 | `			);` |
|      ! 0 | 6266 | `		return pCtx->nThrowRc;` |
|        - | 6267 | `	}` |
|    12884 | 6268 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|    12884 | 6269 | `	if( pThis == 0 ){` |
|      ! 0 | 6270 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - | 6271 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - | 6272 | `			);` |
|      ! 0 | 6273 | `		return pCtx->nThrowRc;` |
|        - | 6274 | `	}` |
|        - | 6275 |  |
|    12884 | 6276 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    12884 | 6277 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - | 6278 |  |
|    12884 | 6279 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    12884 | 6280 | `	if( pCons ){` |
|    12884 | 6281 | `		int nArg = 1;` |
|    12884 | 6282 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|    12884 | 6283 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    12884 | 6284 | `		apArg[0] = &sArg;` |
|    12884 | 6285 | `		if( iCode != 0 ){` |
|      418 | 6286 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|      418 | 6287 | `			apArg[1] = &sCode;` |
|      418 | 6288 | `			nArg = 2;` |
|      208 | 6289 | `		}` |
|    12884 | 6290 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|    12884 | 6291 | `		if( iCode != 0 ){` |
|      418 | 6292 | `			PH7_MemObjRelease(&sCode);` |
|      208 | 6293 | `		}` |
|    12884 | 6294 | `		PH7_MemObjRelease(&sArg);` |
|     6415 | 6295 | `	}` |
|    12884 | 6296 | `	SyBlobRelease(&sMsg);` |
|        - | 6297 |  |
|    12884 | 6298 | `	pFrame = pVm->pFrame;` |
|    12884 | 6299 | `	if( pFrame ){` |
|    12884 | 6300 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    12884 | 6301 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     6415 | 6302 | `	}` |
|    12884 | 6303 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    12884 | 6304 | `	PH7_ClassInstanceUnref(pThis);` |
|    12884 | 6305 | `	if( rc == SXERR_ABORT ){` |
|      535 | 6306 | `		pCtx->nThrowRc = PH7_ABORT;` |
|      535 | 6307 | `		return PH7_ABORT;` |
|        - | 6308 | `	}` |
|        - | 6309 | `	/* Record the status on the CALL CONTEXT as well. A host function that raises` |
|        - | 6310 | `	 * here and then returns PH7_OK anyway — because the throw sits in a shared` |
|        - | 6311 | `	 * argument-validation helper whose callers have no status channel — would` |
|        - | 6312 | `	 * otherwise let OP_CALL treat the call as a normal return: the catch has` |
|        - | 6313 | `	 * already run in place, so execution would carry on INSIDE the try the throw` |
|        - | 6314 | `	 * abandoned (and, uncaught, past the reported fatal). VmHostFuncThrowRc()` |
|        - | 6315 | `	 * re-reads this at the boundary; a caller that DOES propagate its rc is` |
|        - | 6316 | `	 * unaffected (the escalation only fires on a non-throwing status). Same` |
|        - | 6317 | `	 * rationale as VmBoundaryPark for callback throws, one call-frame narrower. */` |
|    12354 | 6318 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|    12354 | 6319 | `	return PH7_EXCEPTION;` |
|     6420 | 6320 | `}` |
|    12457 | 6321 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        5 | 6322 | `{` |
|        - | 6323 | `	va_list ap;` |
|        - | 6324 | `	sxi32 rc;` |
|    12462 | 6325 | `	va_start(ap,zFormat);` |
|    12462 | 6326 | `	rc = VmThrowInternalAp(pCtx,zClass,0,zFormat,ap);` |
|    12462 | 6327 | `	va_end(ap);` |
|    12462 | 6328 | `	return rc;` |
|        5 | 6329 | `}` |
|        - | 6330 | `/* Same, carrying php's exception $code (JsonException gets json_last_error()). */` |
|      422 | 6331 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...)` |
|        2 | 6332 | `{` |
|        - | 6333 | `	va_list ap;` |
|        - | 6334 | `	sxi32 rc;` |
|      424 | 6335 | `	va_start(ap,zFormat);` |
|      424 | 6336 | `	rc = VmThrowInternalAp(pCtx,zClass,iCode,zFormat,ap);` |
|      424 | 6337 | `	va_end(ap);` |
|      424 | 6338 | `	return rc;` |
|        2 | 6339 | `}` |
|        - | 6340 | `/*` |
|        - | 6341 | ` * The status a host function's own throw should have returned. Consulted at the` |
|        - | 6342 | ` * single OP_CALL host boundary right after the C routine returns: it upgrades a` |
|        - | 6343 | ` * normal-looking status to the one PH7_VmThrowException recorded on the context,` |
|        - | 6344 | ` * and is the identity when the routine never threw or already reported it.` |
|        - | 6345 | ` *` |
|        - | 6346 | ` * PH7_SUSPEND passes through with ABORT/EXCEPTION even though it is not a "throw` |
|        - | 6347 | ` * already reported" status: it is a control transfer the fiber machinery must` |
|        - | 6348 | ` * honour (the CALL's state is saved and the operand stack is left mid-flight), and` |
|        - | 6349 | ` * no path produces both — every Fiber throw returns PH7_EXCEPTION.` |
|        - | 6350 | ` */` |
|  6735530 | 6351 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc)` |
|        5 | 6352 | `{` |
|  6735530 | 6353 | `	if( pCtx->nThrowRc == 0` |
|  3374338 | 6354 | `	 \|\| rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND ){` |
|  6730923 | 6355 | `		return rc;` |
|        - | 6356 | `	}` |
|     4617 | 6357 | `	return pCtx->nThrowRc;` |
|  3368855 | 6358 | `}` |
|        - | 6359 | `/*` |
|        - | 6360 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - | 6361 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - | 6362 | ` */` |
|      ! 0 | 6363 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 | 6364 | `{` |
|        - | 6365 | `	ph7_vm *pVm;` |
|        - | 6366 | `	SyBlob sMsg;` |
|      ! 0 | 6367 | `	const char *zFuncName = 0;` |
|      ! 0 | 6368 | `	int nFuncLen = 0;` |
|        - | 6369 | `	va_list ap;` |
|        - | 6370 | `	sxi32 rc;` |
|        - | 6371 |  |
|      ! 0 | 6372 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 | 6373 | `		return PH7_OK;` |
|        - | 6374 | `	}` |
|      ! 0 | 6375 | `	pVm = pCtx->pVm;` |
|      ! 0 | 6376 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 | 6377 | `		zClass = "Error";` |
|      ! 0 | 6378 | `	}` |
|        - | 6379 |  |
|      ! 0 | 6380 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 6381 |  |
|      ! 0 | 6382 | `	va_start(ap,zFormat);` |
|      ! 0 | 6383 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 | 6384 | `	va_end(ap);` |
|        - | 6385 |  |
|      ! 0 | 6386 | `	if( pCtx->pFunc ){` |
|      ! 0 | 6387 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 | 6388 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 | 6389 | `	}` |
|      ! 0 | 6390 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 | 6391 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 | 6392 | `	}` |
|      ! 0 | 6393 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 | 6394 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 | 6395 | `	SyBlobRelease(&sMsg);` |
|      ! 0 | 6396 | `	return rc;` |
|      ! 0 | 6397 | `}` |
|        - | 6398 | `/*` |
|        - | 6399 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 6400 | ` * exception is triggered.` |
|        - | 6401 | ` */` |
|      614 | 6402 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|        - | 6403 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 6404 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 6405 | `	)` |
|        4 | 6406 | `{` |
|        - | 6407 | `	ph7_value *apArg[2],sArg;` |
|      618 | 6408 | `	int nArg = 1;` |
|        - | 6409 | `	sxi32 rc;` |
|      618 | 6410 | `	if( pVm->nMuteThrow > 0 ){` |
|        - | 6411 | `		/* A MUTED initializer (VmEvalDefaultMuted: a class static property's` |
|        - | 6412 | `		 * default at mount) — php has not reached this code, so nothing may be` |
|        - | 6413 | `		 * observable: no exception handler runs, no report is printed and the` |
|        - | 6414 | `		 * exit status stays put. Unwind the mini-program at once; the mount path` |
|        - | 6415 | `		 * rolls the attempt back and re-runs the initializer at first access. */` |
|      ! 0 | 6416 | `		return SXERR_ABORT;` |
|        - | 6417 | `	}` |
|      618 | 6418 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 6419 | `		/* Nesting limit reached */` |
|      ! 0 | 6420 | `		return SXRET_OK;` |
|        - | 6421 | `	}` |
|        - | 6422 | `	/* Call any exception handler if available */` |
|      618 | 6423 | `	PH7_MemObjInit(pVm,&sArg);` |
|      618 | 6424 | `	if( pThis ){` |
|        - | 6425 | `		/* Load the exception instance */` |
|      618 | 6426 | `		sArg.x.pOther = pThis;` |
|      618 | 6427 | `		pThis->iRef++;` |
|      618 | 6428 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      311 | 6429 | `	}else{` |
|      ! 0 | 6430 | `		nArg = 0;` |
|        - | 6431 | `	}` |
|      618 | 6432 | `	apArg[0] = &sArg;` |
|        - | 6433 | `	/* Call the exception handler if available */` |
|      618 | 6434 | `	pVm->nExceptDepth++;` |
|        - | 6435 | `	{` |
|        - | 6436 | `		/* Hidden for the duration of its own call, exactly like the error handler` |
|        - | 6437 | `		 * above: an exception escaping the handler is not handed back to it, and a` |
|        - | 6438 | `		 * set_exception_handler() from inside replaces an EMPTY entry. */` |
|        - | 6439 | `		ph7_value sRunning;` |
|      618 | 6440 | `		PH7_MemObjInit(pVm,&sRunning);` |
|      618 | 6441 | `		PH7_MemObjStore(&pVm->sExceptionCB,&sRunning);` |
|      618 | 6442 | `		PH7_MemObjRelease(&pVm->sExceptionCB);` |
|      618 | 6443 | `		MemObjSetType(&pVm->sExceptionCB,MEMOBJ_NULL);` |
|      618 | 6444 | `		rc = PH7_VmCallUserFunction(&(*pVm),&sRunning,nArg,apArg,0);` |
|      618 | 6445 | `		if( !ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|      618 | 6446 | `			PH7_MemObjStore(&sRunning,&pVm->sExceptionCB);` |
|      307 | 6447 | `		}` |
|      618 | 6448 | `		PH7_MemObjRelease(&sRunning);` |
|        - | 6449 | `	}` |
|      618 | 6450 | `	pVm->nExceptDepth--;` |
|      618 | 6451 | `	if( rc != SXRET_OK ){` |
|        - | 6452 | `		const char *zFuncName;` |
|        - | 6453 | `		int nFuncLen;` |
|      616 | 6454 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|        - | 6455 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|      616 | 6456 | `		if( pThis ){` |
|        - | 6457 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|        - | 6458 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|        - | 6459 | `			 * renders byte-identically to the historical single-entry report. */` |
|      616 | 6460 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|      310 | 6461 | `		}else{` |
|        - | 6462 | `			/* No instance (internal report path) — default-class single entry. */` |
|      ! 0 | 6463 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|        - | 6464 | `		}` |
|        - | 6465 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      616 | 6466 | `		rc = SXERR_ABORT;` |
|      306 | 6467 | `	}` |
|      618 | 6468 | `	PH7_MemObjRelease(&sArg);` |
|      618 | 6469 | `	return rc;` |
|      311 | 6470 | `}` |
|        - | 6471 | `/*` |
|        - | 6472 | ` * Throw a user exception.` |
|        - | 6473 | ` *` |
|        - | 6474 | ` * Exception dispatch follows this sequence:` |
|        - | 6475 | ` *` |
|        - | 6476 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 6477 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 6478 | ` *` |
|        - | 6479 | ` * 2. If NO catch matches:` |
|        - | 6480 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 6481 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 6482 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 6483 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 6484 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 6485 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 6486 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 6487 | ` *` |
|        - | 6488 | ` * 3. If a catch DOES match:` |
|        - | 6489 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 6490 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 6491 | ` *       inside the catch body from immediately propagating past our` |
|        - | 6492 | ` *       finally block.` |
|        - | 6493 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 6494 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 6495 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 6496 | ` *       in pPendingException (step 2c).` |
|        - | 6497 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 6498 | ` *    d. Run finally (if present).` |
|        - | 6499 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 6500 | ` *       that handlers are restored and finally has run.` |
|        - | 6501 | ` */` |
|        - | 6502 | `/*` |
|        - | 6503 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|        - | 6504 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|        - | 6505 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|        - | 6506 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|        - | 6507 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|        - | 6508 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|        - | 6509 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|        - | 6510 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|        - | 6511 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|        - | 6512 | ` */` |
|      318 | 6513 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|        5 | 6514 | `{` |
|      345 | 6515 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|       87 | 6516 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       87 | 6517 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|       87 | 6518 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|       14 | 6519 | `			break; /* reached an outer exec's / legacy handler */` |
|        - | 6520 | `		}` |
|       61 | 6521 | `		(void)SySetPop(&pVm->aException);` |
|       61 | 6522 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|       61 | 6523 | `		if( pT->iHasFinally ){` |
|       37 | 6524 | `			*pPc = pT->iFinallyPc;` |
|       37 | 6525 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|       37 | 6526 | `			return 1;` |
|        - | 6527 | `		}` |
|        - | 6528 | `		/* No finally: tear the try's transparent frame down now. */` |
|       27 | 6529 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        6 | 6530 | `			VmLeaveFrame(&(*pVm));` |
|        2 | 6531 | `		}` |
|       27 | 6532 | `		VmExcRelease(&(*pVm),pT);` |
|        5 | 6533 | `	}` |
|      289 | 6534 | `	return 0;` |
|      164 | 6535 | `}` |
|        - | 6536 | `/*` |
|        - | 6537 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|        - | 6538 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|        - | 6539 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|        - | 6540 | ` *` |
|        - | 6541 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|        - | 6542 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|        - | 6543 | ` *    and redirect to the catch body (iHandlerPc).` |
|        - | 6544 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|        - | 6545 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|        - | 6546 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|        - | 6547 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|        - | 6548 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|        - | 6549 | ` */` |
|        - | 6550 | `/*` |
|        - | 6551 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|        - | 6552 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|        - | 6553 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|        - | 6554 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|        - | 6555 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|        - | 6556 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|        - | 6557 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|        - | 6558 | ` * case) is unchanged: no wrapper.` |
|        - | 6559 | ` */` |
|        - | 6560 | `/*` |
|        - | 6561 | ` * php runs a catch body and a finally body in the scope of the function that` |
|        - | 6562 | ` * DECLARED the try. This engine runs them AT THE THROW SITE (the in-place catch),` |
|        - | 6563 | ` * and the throw site can be several calls deeper -- so pVm->aSelf, the` |
|        - | 6564 | `` * late-static-binding stack that `static::`, `new static` and get_called_class()`` |
|        - | 6565 | ` * read, still carries the class of every call the throw left open above the try.` |
|        - | 6566 | ` * A handler entered that way answered the THROWING method's class: for` |
|        - | 6567 | `` * `try { (new T)->boom(); } catch (E $e) { new static(); }` php constructs the`` |
|        - | 6568 | ` * catching class and this engine constructed T.` |
|        - | 6569 | ` *` |
|        - | 6570 | ` * So park that slice for the duration of the handler and put it back after --` |
|        - | 6571 | ` * the same treatment the exception stack beside it already gets. The depth to` |
|        - | 6572 | ` * park back to is the one the try recorded when it opened (nSelfDepth).` |
|        - | 6573 | ` */` |
|        - | 6574 | `typedef struct VmParkedSelf VmParkedSelf;` |
|        - | 6575 | `struct VmParkedSelf` |
|        - | 6576 | `{` |
|        - | 6577 | `	ph7_class **apSaved; /* Entries above the try's depth, or NULL when there are none */` |
|        - | 6578 | `	sxu32 nSaved;        /* How many */` |
|        - | 6579 | `	sxu32 nBase;         /* Depth the handler runs at */` |
|        - | 6580 | `};` |
|  1465463 | 6581 | `static void VmParkSelfForHandler(ph7_vm *pVm,ph7_exception *pException,VmParkedSelf *pPark)` |
|        5 | 6582 | `{` |
|  1465468 | 6583 | `	sxu32 nUsed = SySetUsed(&pVm->aSelf);` |
|  1465468 | 6584 | `	pPark->apSaved = 0;` |
|  1465468 | 6585 | `	pPark->nSaved = 0;` |
|  1465468 | 6586 | `	pPark->nBase = nUsed;` |
|  1465468 | 6587 | `	if( pException == 0 \|\| nUsed <= pException->nSelfDepth ){` |
|        - | 6588 | `		/* The throw never left the try owner's activation (or a coroutine already` |
|        - | 6589 | `		 * parked this slice): the top is the owner's class already. */` |
|   965042 | 6590 | `		return;` |
|        - | 6591 | `	}` |
|   500431 | 6592 | `	pPark->nBase = pException->nSelfDepth;` |
|   500431 | 6593 | `	pPark->nSaved = nUsed - pPark->nBase;` |
|  1000857 | 6594 | `	pPark->apSaved = (ph7_class **)SyMemBackendAlloc(&pVm->sAllocator,` |
|   500426 | 6595 | `		pPark->nSaved * sizeof(ph7_class *));` |
|   500431 | 6596 | `	if( pPark->apSaved == 0 ){` |
|        - | 6597 | `		/* No room to remember them: leave the stack as it is rather than lose it. */` |
|      ! 0 | 6598 | `		pPark->nSaved = 0;` |
|      ! 0 | 6599 | `		pPark->nBase = nUsed;` |
|      ! 0 | 6600 | `		return;` |
|        - | 6601 | `	}` |
|   750644 | 6602 | `	SyMemcpy((const void *)((ph7_class **)SySetBasePtr(&pVm->aSelf) + pPark->nBase),` |
|   500426 | 6603 | `		(void *)pPark->apSaved,pPark->nSaved * sizeof(ph7_class *));` |
|   500431 | 6604 | `	SySetTruncate(&pVm->aSelf,pPark->nBase);` |
|   732709 | 6605 | `}` |
|  1465463 | 6606 | `static void VmUnparkSelfForHandler(ph7_vm *pVm,VmParkedSelf *pPark)` |
|        5 | 6607 | `{` |
|        - | 6608 | `	sxu32 k;` |
|  1465468 | 6609 | `	if( pPark->apSaved == 0 ){` |
|   965042 | 6610 | `		return;` |
|        - | 6611 | `	}` |
|        - | 6612 | `	/* Whatever the handler body pushed and did not pop is its own business and must` |
|        - | 6613 | `	 * not sit under the entries that belong to the still-open calls above. */` |
|   500431 | 6614 | `	if( SySetUsed(&pVm->aSelf) > pPark->nBase ){` |
|      ! 0 | 6615 | `		SySetTruncate(&pVm->aSelf,pPark->nBase);` |
|      ! 0 | 6616 | `	}` |
|  1000861 | 6617 | `	for( k = 0 ; k < pPark->nSaved ; ++k ){` |
|   500435 | 6618 | `		SySetPut(&pVm->aSelf,(const void *)&pPark->apSaved[k]);` |
|   250220 | 6619 | `	}` |
|   500431 | 6620 | `	SyMemBackendFree(&pVm->sAllocator,(void *)pPark->apSaved);` |
|   500431 | 6621 | `	pPark->apSaved = 0;` |
|   732709 | 6622 | `}` |
|    20262 | 6623 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|        5 | 6624 | `{` |
|    20267 | 6625 | `	VmFrame *pWrap = 0;` |
|        - | 6626 | `	VmFrame *pThrowSite;` |
|        - | 6627 | `	sxi32 rc;` |
|    20267 | 6628 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|    20157 | 6629 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 6630 | `	}` |
|      114 | 6631 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|        - | 6632 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|      ! 0 | 6633 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 6634 | `	}` |
|      114 | 6635 | `	pThrowSite = pWrap->pParent;` |
|      114 | 6636 | `	pWrap->pParent = pOwner;` |
|      114 | 6637 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|      114 | 6638 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 6639 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|        - | 6640 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|        - | 6641 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|        - | 6642 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|      114 | 6643 | `	if( pVm->pFrame == pWrap ){` |
|      114 | 6644 | `		VmLeaveFrame(&(*pVm));` |
|       55 | 6645 | `	}` |
|      114 | 6646 | `	pVm->pFrame = pThrowSite;` |
|      114 | 6647 | `	return rc;` |
|    10136 | 6648 | `}` |
|        - | 6649 | `/*` |
|        - | 6650 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|        - | 6651 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|        - | 6652 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|        - | 6653 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|        - | 6654 | ` */` |
|        - | 6655 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|      104 | 6656 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - | 6657 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|        5 | 6658 | `{` |
|      109 | 6659 | `	if( pCatch ){` |
|       89 | 6660 | `		pException->iInCatch = 1;` |
|       89 | 6661 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|       89 | 6662 | `		if( pThis ){ pThis->iRef++; }` |
|       89 | 6663 | `		pException->pInflight = pThis;` |
|       89 | 6664 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|       89 | 6665 | `		pVm->pInlineFrame = (void *)pException->pFrame;` |
|       89 | 6666 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|       89 | 6667 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|       89 | 6668 | `		return SXRET_OK;` |
|        - | 6669 | `	}` |
|       24 | 6670 | `	if( pException->iHasFinally ){` |
|        - | 6671 | `		VmFinallyAction sAct;` |
|       19 | 6672 | `		SyZero(&sAct,sizeof(sAct));` |
|       19 | 6673 | `		sAct.eKind = PH7_FA_RETHROW;` |
|       19 | 6674 | `		if( pThis ){ pThis->iRef++; }` |
|       19 | 6675 | `		sAct.pExc = pThis;` |
|       19 | 6676 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       19 | 6677 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|       19 | 6678 | `		pVm->pInlineFrame = (void *)pException->pFrame;` |
|       19 | 6679 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|       19 | 6680 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|       19 | 6681 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|       19 | 6682 | `		return SXRET_OK;` |
|        - | 6683 | `	}` |
|        - | 6684 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|        - | 6685 | `	 * flat native stack instead of mutual recursion. */` |
|        6 | 6686 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|      ! 0 | 6687 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 | 6688 | `	}` |
|        6 | 6689 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|        6 | 6690 | `	return VM_THROW_KEEP_UNWINDING;` |
|       57 | 6691 | `}` |
|  1466019 | 6692 | `PH7_PRIVATE sxi32 VmThrowException(` |
|        - | 6693 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 6694 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 6695 | `	)` |
|        5 | 6696 | `{` |
|        - | 6697 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 6698 | `	ph7_exception **apException;` |
|   732982 | 6699 | `	ph7_exception *pException;` |
|    50114 | 6700 | `Rethrow:` |
|        - | 6701 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|        - | 6702 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|        - | 6703 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|        - | 6704 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|        - | 6705 | `	 * so the throw path must be too). */` |
|        - | 6706 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 6707 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 6708 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|  1566252 | 6709 | `	VmCoalesceDisarm(pVm);` |
|        - | 6710 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|        - | 6711 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|        - | 6712 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|        - | 6713 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|        - | 6714 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|        - | 6715 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|        - | 6716 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|        - | 6717 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
|  1566247 | 6718 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|       25 | 6719 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|       19 | 6720 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|        8 | 6721 | `	}` |
|        - | 6722 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|        - | 6723 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|        - | 6724 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|        - | 6725 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|        - | 6726 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|        - | 6727 | `	 * that owns the pending return, so it must leave that return intact. */` |
|        - | 6728 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|        - | 6729 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|        - | 6730 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|        - | 6731 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
|  1566252 | 6732 | `	VmClearResumeTarget(&(*pVm));` |
|        - | 6733 | `	/* Point to the stack of loaded exceptions */` |
|  1566252 | 6734 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|  1566252 | 6735 | `	pException = 0;` |
|  1566252 | 6736 | `	pCatch = 0;` |
|  1566252 | 6737 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 6738 | `		ph7_exception_block *aCatch;` |
|        - | 6739 | `		ph7_class *pClass;` |
|        - | 6740 | `		SyString *aNames;` |
|        - | 6741 | `		sxu32 nNames;` |
|        - | 6742 | `		int matched;` |
|        - | 6743 | `		sxu32 j,k;` |
|        - | 6744 | `		/* Locate the appropriate block to execute */` |
|  1465500 | 6745 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|  1465500 | 6746 | `		(void)SySetPop(&pVm->aException);` |
|  1465500 | 6747 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|        - | 6748 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|        - | 6749 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|        - | 6750 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
|  1465530 | 6751 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 6752 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|  1445320 | 6753 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|  1445320 | 6754 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|  1445320 | 6755 | `			matched = 0;` |
|  1445376 | 6756 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 6757 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 6758 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 6759 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|  1445346 | 6760 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|  1445346 | 6761 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 6762 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 6763 | `					continue;` |
|        - | 6764 | `				}` |
|  1445346 | 6765 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|  1445290 | 6766 | `					matched = 1;` |
|  1445290 | 6767 | `					break;` |
|        - | 6768 | `				}` |
|       32 | 6769 | `			}` |
|  1445320 | 6770 | `			if( matched ){` |
|        - | 6771 | `				/* Catch block found,break immediately */` |
|  1445290 | 6772 | `				pCatch = &aCatch[j];` |
|  1445290 | 6773 | `				break;` |
|        - | 6774 | `			}` |
|       18 | 6775 | `		}` |
|   732720 | 6776 | `	}` |
|        - | 6777 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|        - | 6778 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|        - | 6779 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|        - | 6780 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|        - | 6781 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|        - | 6782 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|        - | 6783 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
|  1566252 | 6784 | `	if( pException ){` |
|  1465500 | 6785 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|   732720 | 6786 | `	}` |
|        - | 6787 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|        - | 6788 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|        - | 6789 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
|  1566252 | 6790 | `	if( pException && pException->iInlined ){` |
|      109 | 6791 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|      109 | 6792 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|        - | 6793 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|        6 | 6794 | `			goto Rethrow;` |
|        - | 6795 | `		}` |
|      105 | 6796 | `		return rcInline;` |
|        - | 6797 | `	}` |
|        - | 6798 | `	/* Execute the cached block if available */` |
|  1566148 | 6799 | `	if( pCatch == 0 ){` |
|        - | 6800 | `		sxi32 rc;` |
|        - | 6801 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|   120947 | 6802 | `		if( pException && pException->iHasFinally ){` |
|    20179 | 6803 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|    20179 | 6804 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|    20179 | 6805 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|        - | 6806 | `			/* Seeded at the declaration: cl cannot prove the helper below writes` |
|        - | 6807 | `			 * every field through the pointer, and /WX turns C4701 into an error. */` |
|    20179 | 6808 | `			VmParkedSelf sParkSelf = {0,0,0};` |
|    20179 | 6809 | `			pException->iFinallyDone = 1;` |
|        - | 6810 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|        - | 6811 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|    20179 | 6812 | `			pVm->pInflightException = pThis;` |
|    20179 | 6813 | `			pVm->nInflightExcBase = nExcBefore;` |
|        - | 6814 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|        - | 6815 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|    20179 | 6816 | `			VmParkSelfForHandler(&(*pVm),pException,&sParkSelf);` |
|    20179 | 6817 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|    20179 | 6818 | `			VmUnparkSelfForHandler(&(*pVm),&sParkSelf);` |
|    20179 | 6819 | `			pVm->pInflightException = pSaveInflight;` |
|    20179 | 6820 | `			pVm->nInflightExcBase = nSaveBase;` |
|    20179 | 6821 | `			if( rc == SXERR_ABORT ){` |
|        3 | 6822 | `				VmExcRelease(&(*pVm),pException);` |
|    10023 | 6823 | `				return SXERR_ABORT;` |
|        - | 6824 | `			}` |
|        - | 6825 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|        - | 6826 | `			 * semantics). The finally stored it on the body frame it returns from` |
|        - | 6827 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|        - | 6828 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|        - | 6829 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|        - | 6830 | `			 * takes the value instead of unwinding) and resume in place.` |
|        - | 6831 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|        - | 6832 | `			 * the same transport an in-place catch uses — and unwind as an` |
|        - | 6833 | `			 * exception; the owner's activation consumes the resume` |
|        - | 6834 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|        - | 6835 | `			 * its bHasRet tail materializes the return. */` |
|        - | 6836 | `			{` |
|    20177 | 6837 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    20177 | 6838 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|    20177 | 6839 | `				if( pOwnerFrame->bHasRet ){` |
|    20029 | 6840 | `					if( pOwnerFrame == pThrowFrame ){` |
|    20026 | 6841 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|        - | 6842 | `						/* Record the landing pad like the cross-frame case below.` |
|        - | 6843 | `						 * OP_THROW lands on its compiler-given nJump anyway, but a` |
|        - | 6844 | `						 * VM-RAISED throw site (undefined function, non-callable,` |
|        - | 6845 | `						 * arith TypeError…) has no nJump: without a recorded target` |
|        - | 6846 | `						 * its router unwound as an exception and the Unwind discard` |
|        - | 6847 | ``						 * dropped the parked return — `function f(){ try {`` |
|        - | 6848 | ``						 * nosuchfn(); } finally { return 5; } }` returned null`` |
|        - | 6849 | `						 * where php returns 5. The pad's OP_POP_EXCEPTION tears the` |
|        - | 6850 | `						 * try frame down and its bHasRet tail materializes the` |
|        - | 6851 | `						 * return, same as the in-place-catch landing. */` |
|    30038 | 6852 | `						VmSetResumeTarget(&(*pVm),pOwnerFrame,pException->iLandingPc,` |
|    10012 | 6853 | `							pException->pOwnerInstr,pException->iStackDepth);` |
|    20026 | 6854 | `						VmExcRelease(&(*pVm),pException);` |
|    20026 | 6855 | `						return SXRET_OK;` |
|        - | 6856 | `					}` |
|        4 | 6857 | `					VmSetResumeTarget(&(*pVm),pOwnerFrame,pException->iLandingPc,` |
|        1 | 6858 | `						pException->pOwnerInstr,pException->iStackDepth);` |
|        3 | 6859 | `					VmExcRelease(&(*pVm),pException);` |
|        3 | 6860 | `					return PH7_EXCEPTION;` |
|        - | 6861 | `				}` |
|        - | 6862 | `			}` |
|        - | 6863 | `			/* The finally threw an exception that superseded pThis — it either` |
|        - | 6864 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|        - | 6865 | `			 * (which consumed an entry from the exception stack). Either way the` |
|        - | 6866 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|        - | 6867 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|      150 | 6868 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|       16 | 6869 | `				VmExcRelease(&(*pVm),pException);` |
|       16 | 6870 | `				return PH7_EXCEPTION;` |
|        - | 6871 | `			}` |
|       66 | 6872 | `		}` |
|        - | 6873 | `		/* Check if there is an outer exception handler on the stack */` |
|   100905 | 6874 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 6875 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|        - | 6876 | `			 * iteration per unwound level instead of one native frame. */` |
|      149 | 6877 | `			VmExcRelease(&(*pVm),pException);` |
|      149 | 6878 | `			goto Rethrow;` |
|        - | 6879 | `		}` |
|   100759 | 6880 | `		if( pVm->nMuteThrow > 0 ){` |
|        - | 6881 | `			/* MUTED evaluation (VmEvalDefaultMuted: a class static property's` |
|        - | 6882 | `			 * default at class mount, which php would not have evaluated yet).` |
|        - | 6883 | `			 * Nothing outside the initializer may observe this throw: no` |
|        - | 6884 | `			 * deferral into pPendingException for an enclosing catch to pick up,` |
|        - | 6885 | `			 * no handler, no report. The tries pushed INSIDE the eval had their` |
|        - | 6886 | `			 * chance above; hand the status back so the mini-program unwinds and` |
|        - | 6887 | `			 * the mount path rolls the whole attempt back. */` |
|       54 | 6888 | `			VmExcRelease(&(*pVm),pException);` |
|       54 | 6889 | `			return SXERR_ABORT;` |
|        - | 6890 | `		}` |
|        - | 6891 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 6892 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 6893 | `		 * exception instead of reporting it uncaught.` |
|        - | 6894 | `		 */` |
|   100709 | 6895 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 6896 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 6897 | `			 * by looking for a catch frame on the stack.` |
|        - | 6898 | `			 */` |
|   100709 | 6899 | `			VmFrame *pF = pVm->pFrame;` |
|   100709 | 6900 | `			int inCatch = 0;` |
|   101417 | 6901 | `			while( pF ){` |
|   100795 | 6902 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|   100086 | 6903 | `					inCatch = 1;` |
|   100086 | 6904 | `					break;` |
|        - | 6905 | `				}` |
|      713 | 6906 | `				pF = pF->pParent;` |
|        5 | 6907 | `			}` |
|   100709 | 6908 | `			if( inCatch ){` |
|        - | 6909 | `				/* Defer — will be re-thrown after finally runs */` |
|   100086 | 6910 | `				pThis->iRef++;` |
|   100086 | 6911 | `				pVm->pPendingException = pThis;` |
|   100086 | 6912 | `				VmExcRelease(&(*pVm),pException);` |
|   100086 | 6913 | `				return SXRET_OK;` |
|        - | 6914 | `			}` |
|      311 | 6915 | `		}` |
|        - | 6916 | `#ifdef PH7_CORO_STACK` |
|      627 | 6917 | `		if( pVm->pCoroCtx && pThis && pVm->pCoroCtx->pEscaped == 0 ){` |
|        - | 6918 | `			/* Not uncaught -- OUT OF A FIBER. A fiber running on its own native` |
|        - | 6919 | `			 * stack owns its handler stack outright, so "nothing here matches" is` |
|        - | 6920 | `			 * the end of the FIBER, not of the script: php terminates the fiber and` |
|        - | 6921 | `			 * re-raises the exception at the start()/resume() that ran it, where the` |
|        - | 6922 | `			 * resumer's own handlers are. Carry the instance out on the ctx (one` |
|        - | 6923 | `			 * reference, given back by VmFiberRaiseEscaped) and unwind the body with` |
|        - | 6924 | `			 * the status OP_THROW routes as an exception. */` |
|       10 | 6925 | `			pThis->iRef++;` |
|       10 | 6926 | `			pVm->pCoroCtx->pEscaped = pThis;` |
|       10 | 6927 | `			VmExcRelease(&(*pVm),pException);` |
|       10 | 6928 | `			return PH7_EXCEPTION;` |
|        - | 6929 | `		}` |
|        - | 6930 | `#endif` |
|        - | 6931 | `		/* Truly uncaught */` |
|      618 | 6932 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      618 | 6933 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 6934 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 6935 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 6936 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 6937 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 6938 | `			}` |
|      ! 0 | 6939 | `		}` |
|      618 | 6940 | `		VmExcRelease(&(*pVm),pException);` |
|      618 | 6941 | `		return rc;` |
|      ! 0 | 6942 | `	}else{` |
|  1445206 | 6943 | `		VmFrame *pFrame = pVm->pFrame;` |
|  1445206 | 6944 | `		ph7_exception **apSaved = 0;` |
|        - | 6945 | `		sxu32 nSavedCount;` |
|        - | 6946 | `		/* Seeded at the declaration -- see the other one. */` |
|  1445206 | 6947 | `		VmParkedSelf sParkSelf = {0,0,0};` |
|        - | 6948 | `		sxi32 rc;` |
|        - | 6949 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|        - | 6950 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|        - | 6951 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|        - | 6952 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|        - | 6953 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
|  1445206 | 6954 | `		VmFrame *pCatchBody = pException->pFrame;` |
|  1445206 | 6955 | `		sxu32 iCatchPc = pException->iLandingPc;` |
|  1445206 | 6956 | `		void *pCatchInstr = pException->pOwnerInstr;` |
|  1445206 | 6957 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  1445206 | 6958 | `		if( pException->pFrame == pFrame ){` |
|   834330 | 6959 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|   417162 | 6960 | `		}` |
|        - | 6961 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 6962 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 6963 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 6964 | `		 */` |
|  1445206 | 6965 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|  1445206 | 6966 | `		if( nSavedCount > 0 ){` |
|   150476 | 6967 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|    50157 | 6968 | `				nSavedCount * sizeof(ph7_exception *));` |
|   100319 | 6969 | `			if( apSaved ){` |
|   150476 | 6970 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|    50157 | 6971 | `					nSavedCount * sizeof(ph7_exception *));` |
|   100319 | 6972 | `				SySetReset(&pVm->aException);` |
|    50157 | 6973 | `			}` |
|    50157 | 6974 | `		}` |
|        - | 6975 | `		/* Create the catch frame (made transparent below) */` |
|  1445206 | 6976 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|  1445206 | 6977 | `		if( rc == SXRET_OK ){` |
|        - | 6978 | `			ph7_value *pObj;` |
|        - | 6979 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|        - | 6980 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|        - | 6981 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|        - | 6982 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|        - | 6983 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|        - | 6984 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|        - | 6985 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|        - | 6986 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|        - | 6987 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
|  1445206 | 6988 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|        - | 6989 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|        - | 6990 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|        - | 6991 | `			 * against the live current scope rather than a freed frame. */` |
|  1445206 | 6992 | `			if( pCatchBody ){` |
|  1445206 | 6993 | `				pFrame->pParent = pCatchBody;` |
|   722573 | 6994 | `			}` |
|        - | 6995 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|        - | 6996 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|        - | 6997 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|        - | 6998 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|        - | 6999 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|        - | 7000 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|        - | 7001 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|        - | 7002 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
|  1445206 | 7003 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|        - | 7004 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
|  2167834 | 7005 | `			pObj = (pCatch->sThis.nByte > 0)` |
|  1445199 | 7006 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
|  1445206 | 7007 | `			if( pObj ){` |
|        - | 7008 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|        - | 7009 | `				 * so it may already hold a value from a prior catch or assignment.` |
|        - | 7010 | `				 * Pin the new instance, then release the slot's prior contents` |
|        - | 7011 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|        - | 7012 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|        - | 7013 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
|  1445202 | 7014 | `				pThis->iRef++;` |
|  1445202 | 7015 | `				PH7_MemObjRelease(pObj);` |
|  1445202 | 7016 | `				pObj->x.pOther = pThis;` |
|  1445202 | 7017 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   722571 | 7018 | `			}` |
|        - | 7019 | `			/* Execute the catch block, at the try owner's late-static-binding depth. */` |
|  1445206 | 7020 | `			VmParkSelfForHandler(&(*pVm),pException,&sParkSelf);` |
|  1445206 | 7021 | `			rc = VmLocalExec(&(*pVm),pCatch->pByteCode,0,TRUE);` |
|  1445206 | 7022 | `			VmUnparkSelfForHandler(&(*pVm),&sParkSelf);` |
|        - | 7023 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|        - | 7024 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|        - | 7025 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|        - | 7026 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|        - | 7027 | `			 * unbalanced — never pop somebody else's frame. */` |
|  1445206 | 7028 | `			if( pVm->pFrame == pFrame ){` |
|  1445204 | 7029 | `				VmLeaveFrame(&(*pVm));` |
|   722572 | 7030 | `			}` |
|  1445206 | 7031 | `			pVm->pFrame = pThrowSite;` |
|   722573 | 7032 | `		}` |
|        - | 7033 | `		/* Restore the outer exception handlers */` |
|  1445206 | 7034 | `		if( apSaved ){` |
|        - | 7035 | `			sxu32 k;` |
|        - | 7036 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|        - | 7037 | `			 * the catch body) are normally already consumed; on an abnormal` |
|        - | 7038 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|        - | 7039 | `			 * linger — release those activations before discarding the set. */` |
|   100319 | 7040 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|   100319 | 7041 | `			SySetReset(&pVm->aException);` |
|   201287 | 7042 | `			for(k = 0; k < nSavedCount; k++){` |
|   100973 | 7043 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|    50489 | 7044 | `			}` |
|   100319 | 7045 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|    50157 | 7046 | `		}` |
|        - | 7047 | `		/* Execute the finally block after catch */` |
|  1445206 | 7048 | `		if( pException->iHasFinally ){` |
|        - | 7049 | `			sxi32 rcf;` |
|        - | 7050 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|        - | 7051 | `			 * from, its pending-return write generation (set if the catch above` |
|        - | 7052 | `			 * returned), and the exception-stack depth. After the finally we use` |
|        - | 7053 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|        - | 7054 | `			 * catch-return. */` |
|        - | 7055 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|        - | 7056 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|        - | 7057 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|        - | 7058 | `			 * supersede decision belong to the owner, not the thrower. */` |
|       93 | 7059 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|       93 | 7060 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|       93 | 7061 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|        - | 7062 | `			/* The exception in flight while this finally runs is the catch body's` |
|        - | 7063 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|        - | 7064 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|        - | 7065 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|        - | 7066 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|       93 | 7067 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|       93 | 7068 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|       93 | 7069 | `			pException->iFinallyDone = 1;` |
|       93 | 7070 | `			pVm->pInflightException = pVm->pPendingException;` |
|       93 | 7071 | `			pVm->nInflightExcBase = nExcBefore;` |
|        - | 7072 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|       93 | 7073 | `			VmParkSelfForHandler(&(*pVm),pException,&sParkSelf);` |
|       93 | 7074 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|       93 | 7075 | `			VmUnparkSelfForHandler(&(*pVm),&sParkSelf);` |
|       93 | 7076 | `			pVm->pInflightException = pSaveInflight;` |
|       93 | 7077 | `			pVm->nInflightExcBase = nSaveBase;` |
|       93 | 7078 | `			if( rcf == SXERR_ABORT ){` |
|      ! 0 | 7079 | `				VmExcRelease(&(*pVm),pException);` |
|      ! 0 | 7080 | `				return SXERR_ABORT;` |
|        - | 7081 | `			}` |
|        - | 7082 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|        - | 7083 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|        - | 7084 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|        - | 7085 | `			 * either case that exception supersedes this try's catch-return — but` |
|        - | 7086 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|        - | 7087 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|        - | 7088 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|       93 | 7089 | `			if( rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|       20 | 7090 | `				if( pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|       13 | 7091 | `					VmClearFramePending(pBody);` |
|        5 | 7092 | `				}` |
|        - | 7093 | ``				/* Same rule for a `break`/`continue` parked by the catch`` |
|        - | 7094 | `				 * (OP_CATCH_JMP): the escaping exception supersedes the loop exit.` |
|        - | 7095 | `				 * Unconditional — a jump has no nRetGen equivalent, nothing can` |
|        - | 7096 | `				 * legitimately have re-armed it during the finally. */` |
|       20 | 7097 | `				pBody->nCatchJmpPc = 0;` |
|        8 | 7098 | `			}` |
|       93 | 7099 | `			if( rcf == PH7_EXCEPTION ){` |
|        - | 7100 | `				/* The finally's exception propagated past this try; drop any deferred` |
|        - | 7101 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|        - | 7102 | `				 * reaches the frame that caught the finally's throw. */` |
|       20 | 7103 | `				if( pVm->pPendingException ){` |
|      ! 0 | 7104 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|      ! 0 | 7105 | `					pVm->pPendingException = 0;` |
|      ! 0 | 7106 | `				}` |
|       20 | 7107 | `				VmExcRelease(&(*pVm),pException);` |
|       20 | 7108 | `				return PH7_EXCEPTION;` |
|        - | 7109 | `			}` |
|       36 | 7110 | `		}` |
|  1445190 | 7111 | `		if( rc == SXERR_ABORT ){` |
|       10 | 7112 | `			VmExcRelease(&(*pVm),pException);` |
|       10 | 7113 | `			return SXERR_ABORT;` |
|        - | 7114 | `		}` |
|        - | 7115 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 7116 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 7117 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|        - | 7118 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|        - | 7119 | `		 * the catch frame having been left above), which swallows the in-flight` |
|        - | 7120 | `		 * exception (PHP semantics).` |
|        - | 7121 | `		 */` |
|  1445182 | 7122 | `		if( pVm->pPendingException ){` |
|        - | 7123 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|   100086 | 7124 | `			VmFrame *pOwner = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|   100086 | 7125 | `			if( !pOwner->bHasRet ){` |
|   100082 | 7126 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        - | 7127 | ``				/* Unlike a `return`, a parked `break`/`continue` does NOT swallow the`` |
|        - | 7128 | `				 * re-throw: control unwinds past the loop, so drop the loop exit rather` |
|        - | 7129 | `				 * than leave it armed for an unrelated later landing. */` |
|   100082 | 7130 | `				pOwner->nCatchJmpPc = 0;` |
|   100082 | 7131 | `				pVm->pPendingException = 0;` |
|   100082 | 7132 | `				VmExcRelease(&(*pVm),pException);` |
|        - | 7133 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|   100082 | 7134 | `				pThis = pReThrow;` |
|   100082 | 7135 | `				goto Rethrow;` |
|        - | 7136 | `			}` |
|        - | 7137 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|        6 | 7138 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|        6 | 7139 | `			pVm->pPendingException = 0;` |
|        2 | 7140 | `		}` |
|        - | 7141 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|        - | 7142 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|        - | 7143 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|        - | 7144 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
|  1345104 | 7145 | `		VmSetResumeTarget(&(*pVm),pCatchBody,iCatchPc,pCatchInstr,pException->iStackDepth);` |
|        - | 7146 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|        - | 7147 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|        - | 7148 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
|  1345104 | 7149 | `		VmExcRelease(&(*pVm),pException);` |
|        - | 7150 | `	}` |
|  1345104 | 7151 | `	return SXRET_OK;` |
|   732987 | 7152 | `}` |
|        - | 7153 |  |
