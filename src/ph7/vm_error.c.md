# src/ph7/vm_error.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3715/4125 lines (90.06%)

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
|    36182 |   28 | `static void VmRecordLastError(ph7_vm *pVm,sxi32 iErr,const char *zMsg,sxu32 nMsg,SyString *pFile,sxu32 nLine)` |
|        5 |   29 | `{` |
|    36187 |   30 | `	pVm->nLastErrType = iErr;` |
|    36187 |   31 | `	pVm->nLastErrLine = nLine;` |
|    36187 |   32 | `	SyBlobReset(&pVm->sLastErrMsg);` |
|    36187 |   33 | `	if( zMsg && nMsg > 0 ){` |
|    36187 |   34 | `		SyBlobAppend(&pVm->sLastErrMsg,zMsg,nMsg);` |
|    18088 |   35 | `	}` |
|    36187 |   36 | `	SyBlobReset(&pVm->sLastErrFile);` |
|    36187 |   37 | `	if( pFile ){` |
|    36187 |   38 | `		SyBlobAppend(&pVm->sLastErrFile,pFile->zString,pFile->nByte);` |
|    18088 |   39 | `	}` |
|    36187 |   40 | `}` |
|        - |   41 | `/*` |
|        - |   42 | ` * The stream a diagnostic's LOG copy goes to: the registered stderr consumer,` |
|        - |   43 | ` * or -- for embedders that never wired one -- the program-output consumer, so a` |
|        - |   44 | ` * diagnostic is never silently swallowed.` |
|        - |   45 | ` */` |
|      992 |   46 | `static ph7_output_consumer * VmErrConsumer(ph7_vm *pVm)` |
|        4 |   47 | `{` |
|      996 |   48 | `	return pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : &pVm->sVmConsumer;` |
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
|      240 |   68 | `PH7_PRIVATE int PH7_VmDisplayErrorsMode(const char *zVal,sxu32 nVal)` |
|        4 |   69 | `{` |
|      244 |   70 | `	sxu64 uAcc = 0;` |
|      244 |   71 | `	sxu32 i = 0;` |
|      244 |   72 | `	int bNeg = 0, bSat = 0;` |
|        - |   73 | `	unsigned int uMode;` |
|      244 |   74 | `	if( zVal == 0 \|\| nVal < 1 ){` |
|       16 |   75 | `		return PH7_DISPLAY_ERRORS_OFF;` |
|        - |   76 | `	}` |
|      228 |   77 | `	if( nVal == 2 && SyStrnicmp(zVal,"on",2) == 0 ){ return PH7_DISPLAY_ERRORS_STDOUT; }` |
|      228 |   78 | `	if( nVal == 3 && SyStrnicmp(zVal,"yes",3) == 0 ){ return PH7_DISPLAY_ERRORS_STDOUT; }` |
|      228 |   79 | `	if( nVal == 4 && SyStrnicmp(zVal,"true",4) == 0 ){ return PH7_DISPLAY_ERRORS_STDOUT; }` |
|      228 |   80 | `	if( nVal == 6 && SyStrnicmp(zVal,"stderr",6) == 0 ){ return PH7_DISPLAY_ERRORS_STDERR; }` |
|      208 |   81 | `	if( nVal == 6 && SyStrnicmp(zVal,"stdout",6) == 0 ){ return PH7_DISPLAY_ERRORS_STDOUT; }` |
|        - |   82 | `	/* strtol()'s own lead-in: the blanks it skips, then one optional sign */` |
|      302 |   83 | `	while( i < nVal && (zVal[i] == ' ' \|\| zVal[i] == '\t' \|\| zVal[i] == '\n'` |
|      196 |   84 | `	    \|\| zVal[i] == '\v' \|\| zVal[i] == '\f' \|\| zVal[i] == '\r') ){` |
|        4 |   85 | `		i++;` |
|      ! 0 |   86 | `	}` |
|      200 |   87 | `	if( i < nVal && (zVal[i] == '+' \|\| zVal[i] == '-') ){` |
|       12 |   88 | `		bNeg = zVal[i] == '-';` |
|       12 |   89 | `		i++;` |
|        6 |   90 | `	}` |
|      432 |   91 | `	while( i < nVal && zVal[i] >= '0' && zVal[i] <= '9' ){` |
|        - |   92 | `		/* strtol() saturates instead of wrapping, and the remaining digits cannot` |
|        - |   93 | `		 * move a saturated value: LONG_MAX's low byte is 0xFF and LONG_MIN's 0x00.` |
|        - |   94 | `		 * The two limits are not symmetric, which is exactly the difference` |
|        - |   95 | ``		 * between `-9223372036854775808` (off) and `-9223372036854775809` (off by`` |
|        - |   96 | `		 * saturation, not by truncation) -- and between the latter and` |
|        - |   97 | ``		 * `9223372036854775808`, which saturates the other way and is stdout. */`` |
|      236 |   98 | `		sxu64 uLimit = bNeg ? (sxu64)9223372036854775808ULL : (sxu64)9223372036854775807ULL;` |
|      236 |   99 | `		if( uAcc > uLimit / 10 ){` |
|      ! 0 |  100 | `			bSat = 1;` |
|      ! 0 |  101 | `			break;` |
|        - |  102 | `		}` |
|      236 |  103 | `		uAcc = uAcc * 10 + (sxu64)(zVal[i] - '0');` |
|      236 |  104 | `		if( uAcc > uLimit ){` |
|      ! 0 |  105 | `			bSat = 1;` |
|      ! 0 |  106 | `			break;` |
|        - |  107 | `		}` |
|      236 |  108 | `		i++;` |
|        4 |  109 | `	}` |
|      200 |  110 | `	if( bSat ){` |
|      ! 0 |  111 | `		uMode = bNeg ? 0u : 255u;` |
|      ! 0 |  112 | `	}else{` |
|      200 |  113 | `		uMode = (unsigned int)((bNeg ? (sxu64)(0 - uAcc) : uAcc) & 0xFF);` |
|        - |  114 | `	}` |
|      196 |  115 | `	if( uMode != PH7_DISPLAY_ERRORS_OFF` |
|      180 |  116 | `	 && uMode != PH7_DISPLAY_ERRORS_STDOUT` |
|      106 |  117 | `	 && uMode != PH7_DISPLAY_ERRORS_STDERR ){` |
|       16 |  118 | `		return PH7_DISPLAY_ERRORS_STDOUT;` |
|        - |  119 | `	}` |
|      184 |  120 | `	return (int)uMode;` |
|      124 |  121 | `}` |
|        - |  122 | `/*` |
|        - |  123 | ` * Append the platform newline and hand a finished diagnostic blob to a consumer.` |
|        - |  124 | ` * bTrack counts the bytes toward program output length (only the stdout DISPLAY` |
|        - |  125 | ` * copy is program output; the stderr LOG copy is not, and must not perturb` |
|        - |  126 | ` * headers_sent()/output accounting).` |
|        - |  127 | ` */` |
|     1088 |  128 | `static sxi32 VmWriteDiagnostic(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bTrack)` |
|        4 |  129 | `{` |
|        - |  130 | `	sxi32 rc;` |
|        - |  131 | `	/* Append a new line */` |
|        - |  132 | `#ifdef __WINNT__` |
|        4 |  133 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  134 | `#else` |
|     1088 |  135 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  136 | `#endif` |
|        - |  137 | `	/* Invoke the output consumer callback */` |
|     1092 |  138 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|     1092 |  139 | `	if( bTrack ){` |
|      102 |  140 | `		VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|       49 |  141 | `	}` |
|     1092 |  142 | `	return rc;` |
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
|     2474 |  159 | `PH7_PRIVATE int PH7_VmErrorLogToFile(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
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
|     2479 |  172 | `	int nPath, rc = 0;` |
|     2479 |  173 | `	nPath = (int)SyBlobLength(&pVm->sErrLogPath);` |
|     2479 |  174 | `	if( nPath < 1 ){` |
|     2471 |  175 | `		return 0;` |
|        - |  176 | `	}` |
|        - |  177 | `	/* The stream layer opens by C string, and the two doors that write this blob` |
|        - |  178 | `	 * reset it in place -- so an unterminated path inherits the TAIL of whatever` |
|        - |  179 | ``	 * longer one preceded it, and `-d error_log=/no/such/dir/x.log` followed by`` |
|        - |  180 | `	 * ini_set('error_log','/tmp/a.log') opened "/tmp/a.logh/x.log" and quietly` |
|        - |  181 | `	 * fell back to the stream. Both doors terminate; this reads the bytes before` |
|        - |  182 | `	 * the terminator. */` |
|       10 |  183 | `	zPath = (const char *)SyBlobData(&pVm->sErrLogPath);` |
|       10 |  184 | `	pStream = PH7_VmGetStreamDevice(pVm,&zPath,nPath);` |
|       10 |  185 | `	if( pStream == 0 \|\| pStream->xWrite == 0 ){` |
|      ! 0 |  186 | `		return 0;` |
|        - |  187 | `	}` |
|        - |  188 | `	/* No warning on the way in: php's logger is SILENT about a destination it` |
|        - |  189 | `	 * cannot open -- raising one here would be a diagnostic about a diagnostic,` |
|        - |  190 | `	 * and the recursion is not the only reason php does not. */` |
|       10 |  191 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,zPath,` |
|        - |  192 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND,FALSE,0,FALSE,0,0);` |
|       10 |  193 | `	if( pHandle == 0 ){` |
|        3 |  194 | `		return 0;` |
|        - |  195 | `	}` |
|        8 |  196 | `	SyBlobInit(&sLine,&pVm->sAllocator);` |
|        8 |  197 | `	PH7_VmLogTimestamp(pVm,&sLine);` |
|        8 |  198 | `	SyBlobAppend(&sLine,zMsg,nMsg);` |
|        - |  199 | `#ifdef __WINNT__` |
|        2 |  200 | `	SyBlobAppend(&sLine,"\r\n",sizeof("\r\n")-1);` |
|        - |  201 | `#else` |
|        6 |  202 | `	SyBlobAppend(&sLine,"\n",sizeof(char));` |
|        - |  203 | `#endif` |
|        - |  204 | `	/* One write: php builds the whole line and hands it over in one go, which is` |
|        - |  205 | `	 * what keeps two processes appending to one log from interleaving mid-line. */` |
|        6 |  206 | `	if( SyBlobLength(&sLine) > 0` |
|        8 |  207 | `	 && pStream->xWrite(pHandle,SyBlobData(&sLine),(ph7_int64)SyBlobLength(&sLine)) > 0 ){` |
|        8 |  208 | `		rc = 1;` |
|        3 |  209 | `	}` |
|        8 |  210 | `	SyBlobRelease(&sLine);` |
|        8 |  211 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        8 |  212 | `	return rc;` |
|        - |  213 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     1242 |  214 | `}` |
|        - |  215 | `/*` |
|        - |  216 | `` * Hand a finished LOG copy to whichever sink `error_log` names. The file wins`` |
|        - |  217 | ` * when it is set and takes the write; the error stream is what is left.` |
|        - |  218 | ` */` |
|      944 |  219 | `static sxi32 VmWriteErrorLog(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg)` |
|        4 |  220 | `{` |
|      948 |  221 | `	if( PH7_VmErrorLogToFile(pVm,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg)) ){` |
|        3 |  222 | `		return SXRET_OK;` |
|        - |  223 | `	}` |
|      946 |  224 | `	return VmWriteDiagnostic(pVm,pCons,pMsg,0);` |
|      476 |  225 | `}` |
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
|      632 |  244 | `static sxi32 VmEmitFatalReport(ph7_vm *pVm,const char *zLabel,const char *zBody,sxu32 nBody)` |
|        4 |  245 | `{` |
|        - |  246 | `	SyBlob sCopy;` |
|      636 |  247 | `	sxi32 rc = SXRET_OK;` |
|      636 |  248 | `	if( pVm->bLogErrors ){` |
|      632 |  249 | `		SyBlobInit(&sCopy,&pVm->sAllocator);` |
|      632 |  250 | `		SyBlobFormat(&sCopy,"PHP %s:  ",zLabel);` |
|      632 |  251 | `		SyBlobAppend(&sCopy,zBody,nBody);` |
|      632 |  252 | `		rc = VmWriteErrorLog(pVm,VmErrConsumer(pVm),&sCopy);` |
|      632 |  253 | `		SyBlobRelease(&sCopy);` |
|      314 |  254 | `	}` |
|      636 |  255 | `	if( pVm->iDisplayErrors != PH7_DISPLAY_ERRORS_OFF ){` |
|        8 |  256 | `		int bErrStream = pVm->iDisplayErrors == PH7_DISPLAY_ERRORS_STDERR;` |
|        - |  257 | `		sxi32 rc2;` |
|        8 |  258 | `		SyBlobInit(&sCopy,&pVm->sAllocator);` |
|        8 |  259 | `		if( !bErrStream ){` |
|        - |  260 | `			/* php's text-mode display copy is prefixed with a blank line */` |
|        3 |  261 | `			SyBlobAppend(&sCopy,"\n",sizeof(char));` |
|        1 |  262 | `		}` |
|        8 |  263 | `		SyBlobFormat(&sCopy,"%s: ",zLabel);` |
|        8 |  264 | `		SyBlobAppend(&sCopy,zBody,nBody);` |
|       13 |  265 | `		rc2 = VmWriteDiagnostic(pVm,` |
|        5 |  266 | `			bErrStream ? VmErrConsumer(pVm) : &pVm->sVmConsumer,&sCopy,!bErrStream);` |
|        8 |  267 | `		SyBlobRelease(&sCopy);` |
|        - |  268 | `		/* keep the first failure rather than letting a later successful write mask it */` |
|        8 |  269 | `		if( rc == SXRET_OK ){` |
|        8 |  270 | `			rc = rc2;` |
|        3 |  271 | `		}` |
|        3 |  272 | `	}` |
|      636 |  273 | `	return rc;` |
|        4 |  274 | `}` |
|        - |  275 | `/*` |
|        - |  276 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  277 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  278 | ` * information.` |
|        - |  279 | ` */` |
|        - |  280 | `/*` |
|        - |  281 | ` * Is the diagnostic being reported raised by an INTERNAL function -- one called from` |
|        - |  282 | ` * the activation now running, or a builtin whose body is embedded PHP? php calls the` |
|        - |  283 | ` * error handler from EG(current_execute_data), which is that function's frame when it` |
|        - |  284 | ` * has one; a folded call links no record (bFoldedCallee), and an opcode's own` |
|        - |  285 | ` * diagnostic has no function running at all. A builtin that loaded the unit now` |
|        - |  286 | ` * running (spl_autoload()) shares this activation but not its include depth: that` |
|        - |  287 | ` * unit's own code raised the diagnostic, not the builtin.` |
|        - |  288 | ` */` |
|     7061 |  289 | `static int VmErrRaisedByInternal(ph7_vm *pVm)` |
|        5 |  290 | `{` |
|     7066 |  291 | `	VmNativeCall *pNat = pVm->pNativeCall;` |
|     7346 |  292 | `	while( pNat && pNat->bElided ){` |
|      285 |  293 | `		pNat = pNat->pPrev;` |
|        5 |  294 | `	}` |
|     7061 |  295 | `	if( pNat && pNat->pFrame == (void *)pVm->pFrame` |
|     4040 |  296 | `	 && pNat->nIncDepth == SySetUsed(&pVm->aIncFrame) ){` |
|     4026 |  297 | `		return 1;` |
|        - |  298 | `	}` |
|     3045 |  299 | `	return PH7_VmPreludeBuiltinFrame(pVm,0,0) != 0;` |
|     3518 |  300 | `}` |
|    41707 |  301 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        5 |  302 | `{` |
|        - |  303 | `	/* A handler is only called for the levels it was REGISTERED for. php ANDs` |
|        - |  304 | `	 * set_error_handler()'s $error_levels against the error's own bit and, when` |
|        - |  305 | `	 * it misses, does NOT walk down to an outer handler -- the diagnostic falls` |
|        - |  306 | `	 * straight through to the engine's own reporting, which is what returning` |
|        - |  307 | `	 * TRUE below means. */` |
|    41707 |  308 | `	if( !ph7_value_is_null(&pVm->sErrCB)` |
|    24402 |  309 | `	 && (pVm->iErrCBLevels & (sxi64)PH7_VmErrPhpBit(iErr)) != 0 ){` |
|        - |  310 | `		ph7_value apArg[4];` |
|        - |  311 | `		ph7_value *apArgPtr[4];` |
|        - |  312 | `		ph7_value sResult;` |
|        - |  313 | `		ph7_value sRunning;` |
|        - |  314 | `		SyString sErr;` |
|        - |  315 | `		int bReport;` |
|        - |  316 | `		/* PH7_CTX_NOTICE is the engine's own severity token, 3 — a number php has no` |
|        - |  317 | `		 * E_* constant for. The reporting mask and the "Notice: " label already read` |
|        - |  318 | `		 * it as E_NOTICE; the handler was the one place it leaked, so a userland` |
|        - |  319 | ``		 * `set_error_handler` saw `$errno === 3` where php passes 8 and an`` |
|        - |  320 | ``		 * `if ($errno & E_NOTICE)` test simply never fired. */`` |
|     7080 |  321 | `		if( iErr == PH7_CTX_NOTICE ){` |
|      455 |  322 | `			iErr = 8; /* E_NOTICE */` |
|      225 |  323 | `		}` |
|        - |  324 | `		/* Prepare arguments */` |
|     7080 |  325 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  326 | `			/* use explicit message length to avoid reading past buffer */` |
|     7080 |  327 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|     7080 |  328 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|     7080 |  329 | `		if( pFile ){` |
|     7080 |  330 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|     7080 |  331 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|     3525 |  332 | `		}else{` |
|      ! 0 |  333 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  334 | `		}` |
|     7080 |  335 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|     7080 |  336 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  337 | `		/* Set up pointer array */` |
|     7080 |  338 | `		apArgPtr[0] = &apArg[0];` |
|     7080 |  339 | `		apArgPtr[1] = &apArg[1];` |
|     7080 |  340 | `		apArgPtr[2] = &apArg[2];` |
|     7080 |  341 | `		apArgPtr[3] = &apArg[3];` |
|        - |  342 | `		/* php HIDES the handler for the duration of its own call: a diagnostic the` |
|        - |  343 | `		 * handler itself raises reaches the engine's reporting instead of` |
|        - |  344 | `		 * re-entering (PHL recursed until the stack ran out and printed nothing at` |
|        - |  345 | ``		 * all), and `set_error_handler()` called from inside one therefore replaces`` |
|        - |  346 | `		 * an EMPTY entry. What the handler leaves behind decides who is installed` |
|        - |  347 | `		 * when it returns: an untouched slot gets the original back, and anything` |
|        - |  348 | `		 * the handler installed itself STAYS. */` |
|     7080 |  349 | `		PH7_MemObjInit(pVm,&sRunning);` |
|     7080 |  350 | `		PH7_MemObjStore(&pVm->sErrCB,&sRunning);` |
|     7080 |  351 | `		PH7_MemObjRelease(&pVm->sErrCB);` |
|     7080 |  352 | `		MemObjSetType(&pVm->sErrCB,MEMOBJ_NULL);` |
|        - |  353 | `		/* Call the handler */` |
|        - |  354 | `		{` |
|        - |  355 | `			char zReason[256];` |
|        - |  356 | `			const char *zWhy;` |
|        - |  357 | `			sxi32 rcCb;` |
|        - |  358 | `			/* php stores the handler as a bare value and resolves it again at every` |
|        - |  359 | `			 * call, against the scope the diagnostic was raised in -- not the one` |
|        - |  360 | `			 * set_error_handler() ran in. A private or protected method registered` |
|        - |  361 | `			 * from inside its class is therefore uncallable from outside it, and` |
|        - |  362 | ``			 * zend_call_function() throws `Invalid callback C::m, <reason>` in`` |
|        - |  363 | `			 * place of the diagnostic. The dispatcher skipped it in silence and the` |
|        - |  364 | `			 * engine reported the diagnostic itself. */` |
|     7080 |  365 | `			zWhy = PH7_VmCallableReason(pVm,&sRunning,zReason,sizeof(zReason));` |
|     7080 |  366 | `			if( zWhy ){` |
|        - |  367 | `				SyBlob sMsg;` |
|       15 |  368 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       15 |  369 | `				SyBlobAppend(&sMsg,"Invalid callback ",sizeof("Invalid callback ")-1);` |
|       15 |  370 | `				PH7_VmCallableName(pVm,&sRunning,&sMsg);` |
|       15 |  371 | `				SyBlobFormat(&sMsg,", %s",zWhy);` |
|       15 |  372 | `				rcCb = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - |  373 | `				/* No caller of this dispatcher routes a status: park it for the` |
|        - |  374 | `				 * fetch-point router, as the user-call dispatcher does for a` |
|        - |  375 | `				 * handler that throws by itself. */` |
|       15 |  376 | `				VmBoundaryPark(pVm,rcCb);` |
|     7073 |  377 | `			}else if( VmErrRaisedByInternal(pVm) ){` |
|     4054 |  378 | `				rcCb = PH7_VmCallUserFunction(pVm,&sRunning,4,apArgPtr,&sResult);` |
|     2012 |  379 | `			}else{` |
|        - |  380 | `				/* Raised by the running userland code itself -- an opcode, or a` |
|        - |  381 | ``				 * call php's compiler folded into one (`sprintf('%s', $a)`,`` |
|        - |  382 | ``				 * `strval($a)`): php calls the handler from that frame, so it is`` |
|        - |  383 | `				 * no internal callback. Its frame carries the call site and no` |
|        - |  384 | `				 * builtin sits above it, its arguments bind in that file's mode,` |
|        - |  385 | `				 * and a too-few error names where it was "passed in". */` |
|     3017 |  386 | `				rcCb = PH7_VmCallUserFunctionWithMap(pVm,&sRunning,4,apArgPtr,&sResult,0);` |
|        - |  387 | `			}` |
|     7080 |  388 | `			if( ph7_value_is_null(&pVm->sErrCB) ){` |
|     7078 |  389 | `				PH7_MemObjStore(&sRunning,&pVm->sErrCB);` |
|     3519 |  390 | `			}` |
|     7080 |  391 | `			PH7_MemObjRelease(&sRunning);` |
|     7080 |  392 | `			if( rcCb == PH7_EXCEPTION \|\| rcCb == PH7_ABORT ){` |
|        - |  393 | `				/* The handler threw (or aborted) instead of returning: php never` |
|        - |  394 | `				 * reports the original diagnostic then — the exception supersedes` |
|        - |  395 | `				 * it (and is routed by the boundary parking / fetch-point router).` |
|        - |  396 | `				 * Reporting it here would print a spurious Warning AFTER the` |
|        - |  397 | `				 * user's catch already ran. */` |
|      175 |  398 | `				PH7_MemObjRelease(&apArg[0]);` |
|      175 |  399 | `				PH7_MemObjRelease(&apArg[1]);` |
|      175 |  400 | `				PH7_MemObjRelease(&apArg[2]);` |
|      175 |  401 | `				PH7_MemObjRelease(&apArg[3]);` |
|      175 |  402 | `				PH7_MemObjRelease(&sResult);` |
|      175 |  403 | `				return FALSE;` |
|        - |  404 | `			}` |
|        - |  405 | `		}` |
|        - |  406 | `		/* Does the engine still report this diagnostic itself?` |
|        - |  407 | `		 *` |
|        - |  408 | ``		 * php's rule is IDENTITY, not truthiness: `zend_user_error_handler` falls`` |
|        - |  409 | `		 * through to the built-in reporter only when the handler returned the` |
|        - |  410 | `		 * BOOLEAN false. EVERY other answer means handled -- including the two` |
|        - |  411 | ``		 * commonest shapes there are, a bare `return;` and a handler with no return`` |
|        - |  412 | `		 * statement at all, both of which arrive here as NULL.` |
|        - |  413 | `		 *` |
|        - |  414 | `		 * This coerced the answer to bool, so seven of php's fourteen answers` |
|        - |  415 | `		 * reported through: null, a bare return, 0, 0.0, "", "0" and []. Every` |
|        - |  416 | ``		 * `set_error_handler(function () {})` in the world -- the idiom a library`` |
|        - |  417 | `		 * uses to SILENCE a call it expects to fail -- printed the diagnostic` |
|        - |  418 | `		 * anyway. It is the single largest divergence in the ecosystem gate: 930 of` |
|        - |  419 | ``		 * twig's 1949 accepted baseline lines are one `unserialize()` inside exactly`` |
|        - |  420 | `		 * that idiom, in symfony/phpunit-bridge.` |
|        - |  421 | `		 *` |
|        - |  422 | `		 * Read before the release, and before any coercion. */` |
|     6910 |  423 | `		bReport = (sResult.iFlags & MEMOBJ_BOOL) != 0 && sResult.x.iVal == 0;` |
|        - |  424 | `		/* Release */` |
|     6910 |  425 | `		PH7_MemObjRelease(&apArg[0]);` |
|     6910 |  426 | `		PH7_MemObjRelease(&apArg[1]);` |
|     6910 |  427 | `		PH7_MemObjRelease(&apArg[2]);` |
|     6910 |  428 | `		PH7_MemObjRelease(&apArg[3]);` |
|     6910 |  429 | `		PH7_MemObjRelease(&sResult);` |
|     6910 |  430 | `		return bReport ? TRUE : FALSE;` |
|        - |  431 | `	}` |
|        - |  432 | `	/* No handler, always call error handler */` |
|    34637 |  433 | `	return TRUE;` |
|    20838 |  434 | `}` |
|        - |  435 | `/*` |
|        - |  436 | ` * php's diagnostic label for a severity/errno (display shape; the caller` |
|        - |  437 | ` * appends ": " after it). The PH7_CTX_* levels map to their php analogs, and` |
|        - |  438 | ` * raw E_* errnos passed through by builtins/deprecation sites map by value.` |
|        - |  439 | ` * PH7_CTX_ERR keeps the engine's native "Error" label: many CTX_ERR` |
|        - |  440 | ` * diagnostics are PHL-specific continue-running notices php never prints, so` |
|        - |  441 | ` * claiming php's "Fatal error" there would mislabel them — the per-diagnostic` |
|        - |  442 | ` * severity reclassification is the remaining audit tail. Note the raw` |
|        - |  443 | ` * errno reaches user handlers untouched (e.g. 8192 for E_DEPRECATED); this` |
|        - |  444 | ` * only picks the DISPLAY label.` |
|        - |  445 | ` */` |
|        - |  446 | `/*` |
|        - |  447 | ` * Map an internal severity onto php's error_reporting bit, then ask whether the current` |
|        - |  448 | ` * error_reporting() level wants it. PH7 only had the bErrReport boolean, so ANY non-zero` |
|        - |  449 | `` * level reported everything and `error_reporting(E_ALL & ~E_DEPRECATED)` still printed`` |
|        - |  450 | ` * every deprecation.` |
|        - |  451 | ` */` |
|    38295 |  452 | `PH7_PRIVATE sxi32 PH7_VmErrPhpBit(sxi32 iErr)` |
|        5 |  453 | `{` |
|    38300 |  454 | `	switch( iErr ){` |
|    17401 |  455 | `	case PH7_CTX_WARNING:            /* == 2 == E_WARNING */` |
|    34766 |  456 | `		return 2;` |
|       43 |  457 | `	case 512  /* E_USER_WARNING */:` |
|       90 |  458 | `		return 512;` |
|      290 |  459 | `	case PH7_CTX_NOTICE:             /* 3 */` |
|        - |  460 | `	case 8    /* E_NOTICE */:` |
|      585 |  461 | `		return 8;` |
|       39 |  462 | `	case 1024 /* E_USER_NOTICE */:` |
|       82 |  463 | `		return 1024;` |
|      596 |  464 | `	case 8192 /* E_DEPRECATED */:` |
|     1197 |  465 | `		return 8192;` |
|       24 |  466 | `	case 16384 /* E_USER_DEPRECATED */:` |
|       51 |  467 | `		return 16384;` |
|      ! 0 |  468 | `	case 256  /* E_USER_ERROR */:` |
|      ! 0 |  469 | `		return 256;` |
|        - |  470 | `	/* The COMPILER's three, which php reports under bits of their own rather` |
|        - |  471 | `	 * than under E_WARNING/E_ERROR: a diagnostic the compiler raises is` |
|        - |  472 | `	 * E_COMPILE_WARNING or E_COMPILE_ERROR, and the parser's own refusal is` |
|        - |  473 | ``	 * E_PARSE. `error_reporting(E_ALL & ~E_COMPILE_WARNING)` hides the first`` |
|        - |  474 | `	 * and leaves a compile-time E_WARNING standing, which is a distinction` |
|        - |  475 | `	 * only these rows can express. */` |
|      248 |  476 | `	case 4    /* E_PARSE */:` |
|      500 |  477 | `		return 4;` |
|      499 |  478 | `	case 64   /* E_COMPILE_ERROR */:` |
|     1002 |  479 | `		return 64;` |
|       13 |  480 | `	case 128  /* E_COMPILE_WARNING */:` |
|       29 |  481 | `		return 128;` |
|       15 |  482 | `	default:` |
|       34 |  483 | `		return 1; /* E_ERROR and everything else fatal-ish */` |
|        - |  484 | `	}` |
|    19132 |  485 | `}` |
|    36168 |  486 | `static int VmErrReportWants(ph7_vm *pVm,sxi32 iErr)` |
|        5 |  487 | `{` |
|    36173 |  488 | `	if( !pVm->bErrReport ){` |
|     4969 |  489 | `		return 0;` |
|        - |  490 | `	}` |
|    31207 |  491 | `	return (pVm->iErrMask & PH7_VmErrPhpBit(iErr)) != 0;` |
|    18086 |  492 | `}` |
|      456 |  493 | `static const char * VmDiagnosticLabel(sxi32 iErr)` |
|        4 |  494 | `{` |
|      460 |  495 | `	switch(iErr){` |
|      179 |  496 | `	case PH7_CTX_WARNING:          /* == 2 == E_WARNING */` |
|        - |  497 | `	case 512  /* E_USER_WARNING */:` |
|      362 |  498 | `		return "Warning";` |
|       23 |  499 | `	case PH7_CTX_NOTICE:           /* 3 */` |
|        - |  500 | `	case 8    /* E_NOTICE */:` |
|        - |  501 | `	case 1024 /* E_USER_NOTICE */:` |
|       50 |  502 | `		return "Notice";` |
|       11 |  503 | `	case 8192  /* E_DEPRECATED */:` |
|        - |  504 | `	case 16384 /* E_USER_DEPRECATED */:` |
|       25 |  505 | `		return "Deprecated";` |
|      ! 0 |  506 | `	case 256 /* E_USER_ERROR */:` |
|      ! 0 |  507 | `		return "Fatal error";` |
|       15 |  508 | `	default:` |
|       34 |  509 | `		return "Error";` |
|        - |  510 | `	}` |
|      232 |  511 | `}` |
|        - |  512 | `/*` |
|        - |  513 | ` * Append php's display-shape diagnostic header/trailer around a message:` |
|        - |  514 | `` * `LABEL: <func(): >message in FILE on line LINE`. The LINE is a fixed 1`` |
|        - |  515 | `` * pending the runtime-line-tracking gate (correct for `-r` one-liners;`` |
|        - |  516 | ` * cross-engine tests wildcard it with %d) — the SHAPE is php's, so tests can` |
|        - |  517 | ` * be authored cross-engine with --EXPECTF--.` |
|        - |  518 | ` */` |
|      140 |  519 | `static void VmDiagnosticHeader(SyBlob *pWorker,sxi32 iErr)` |
|        4 |  520 | `{` |
|      144 |  521 | `	SyBlobFormat(pWorker,"%s: ",VmDiagnosticLabel(iErr));` |
|      144 |  522 | `}` |
|        - |  523 | `/*` |
|        - |  524 | ` * WHERE a diagnostic happened, as php reports it.` |
|        - |  525 | ` *` |
|        - |  526 | ` * Normally the file being executed and the current line, with php's floor of 1 for a` |
|        - |  527 | ` * line the engine never recorded. But a diagnostic can be raised with no PHP frame` |
|        - |  528 | `` * under it at all -- php tests `EG(current_execute_data) == NULL` and then has nothing`` |
|        - |  529 | `` * to name, so `zend_get_executed_filename()` answers the literal string "Unknown" and`` |
|        - |  530 | ` * the line is 0. The shutdown destructor pass is where PHL reaches that state: the` |
|        - |  531 | ` * refusal of a non-public __destruct is raised BETWEEN bodies, after the program's last` |
|        - |  532 | ` * statement. A diagnostic raised INSIDE a destructor body has a frame again and reports` |
|        - |  533 | ` * its real file and line, which is why this is a flag the raise site sets and not the` |
|        - |  534 | ` * whole phase.` |
|        - |  535 | ` */` |
|    41625 |  536 | `static sxu32 VmDiagnosticWhere(ph7_vm *pVm,SyString **ppFile)` |
|        5 |  537 | `{` |
|        - |  538 | `	static SyString sNoFrame = { "Unknown", sizeof("Unknown")-1 };` |
|    41630 |  539 | `	if( pVm->bNoFrameLoc ){` |
|       12 |  540 | `		*ppFile = &sNoFrame;` |
|       12 |  541 | `		return 0;` |
|        - |  542 | `	}` |
|        - |  543 | `	{` |
|        - |  544 | `		/* A prelude builtin has no frame at all in php -- it is an INTERNAL` |
|        - |  545 | `		 * function there -- so a diagnostic raised under one is reported at the` |
|        - |  546 | `		 * call the program wrote, not inside the chunk (which is one line, so` |
|        - |  547 | ``		 * every one of them said `on line 1`). */`` |
|    41620 |  548 | `		SyString *pCallFile = 0;` |
|    41620 |  549 | `		sxu32 nCallLine = 0;` |
|    41620 |  550 | `		if( PH7_VmPreludeBuiltinFrame(&(*pVm),&pCallFile,&nCallLine) ){` |
|       67 |  551 | `			if( pCallFile ){` |
|       67 |  552 | `				*ppFile = pCallFile;` |
|       31 |  553 | `			}` |
|       67 |  554 | `			return nCallLine ? nCallLine : 1;` |
|        - |  555 | `		}` |
|        - |  556 | `	}` |
|        - |  557 | `	{` |
|        - |  558 | `		/* The file the RUNNING code is in, which is the defining file of the` |
|        - |  559 | `		 * innermost active function -- not the top of the include stack. The two` |
|        - |  560 | `		 * agree only while top-level code is running: once a call reaches a` |
|        - |  561 | `		 * function defined in another unit, the include stack has moved on, so` |
|        - |  562 | `		 * every diagnostic raised inside a library named the ENTRY SCRIPT. In a` |
|        - |  563 | `		 * composer tree that is every warning any vendor package raises, and it is` |
|        - |  564 | `		 * what a framework's error handler logs. The line was already right. */` |
|    41558 |  565 | `		SyString *pUnit = PH7_VmExecutingUnitFile(&(*pVm));` |
|    41558 |  566 | `		if( pUnit && pUnit->nByte > 0 ){` |
|    41558 |  567 | `			*ppFile = pUnit;` |
|    20756 |  568 | `		}` |
|        - |  569 | `	}` |
|    41558 |  570 | `	return pVm->nCurLine ? pVm->nCurLine : 1;` |
|    20797 |  571 | `}` |
|      470 |  572 | `static void VmDiagnosticLocation(SyBlob *pWorker,SyString *pFile,sxu32 nLine)` |
|        4 |  573 | `{` |
|      474 |  574 | `	if( pFile ){` |
|        - |  575 | `		/* nLine arrives already normalized by VmDiagnosticWhere: "no line recorded"` |
|        - |  576 | `		 * is php's 1, and a raise with no frame under it is php's literal 0. */` |
|      474 |  577 | `		SyBlobFormat(pWorker," in %.*s on line %u",(int)pFile->nByte,pFile->zString,nLine);` |
|      235 |  578 | `	}` |
|      474 |  579 | `}` |
|        - |  580 | `/*` |
|        - |  581 | ``  * php's LOG-shape diagnostic header: `PHP LABEL:  <func(): >` -- the `PHP ` `` |
|        - |  582 | ` * prefix and TWO spaces after the colon, matching the compile-error path` |
|        - |  583 | ` * (compile.c) and stock CLI's stderr log copy.` |
|        - |  584 | ` */` |
|      316 |  585 | `static void VmDiagnosticLogHeader(SyBlob *pWorker,sxi32 iErr)` |
|        4 |  586 | `{` |
|      320 |  587 | `	SyBlobAppend(pWorker,"PHP ",sizeof("PHP ")-1);` |
|      320 |  588 | `	SyBlobFormat(pWorker,"%s:  ",VmDiagnosticLabel(iErr));` |
|      320 |  589 | `}` |
|        - |  590 | `/*` |
|        - |  591 | `` * Prepend php's `func(): ` qualifier to a diagnostic body.`` |
|        - |  592 | ` *` |
|        - |  593 | ` * php puts the raising function's name in the MESSAGE, not in the printed header,` |
|        - |  594 | ` * so its user error handler ($errstr), its error_get_last()['message'] and its` |
|        - |  595 | ` * printed copy all carry the same text. PHL used to add it in the two header` |
|        - |  596 | ` * builders above, i.e. only to the PRINTED copy -- so a set_error_handler() that` |
|        - |  597 | ` * matched on the function name never fired, and error_get_last() answered a body` |
|        - |  598 | ` * php never produces. Building it into the message here is the single place that` |
|        - |  599 | ` * fixes all three. Builtins that already spell the qualifier into their own text` |
|        - |  600 | `` * (`fopen(%s): Failed to open stream`) pass a NULL name and are untouched.`` |
|        - |  601 | ` */` |
|    39674 |  602 | `static void VmDiagnosticQualify(SyBlob *pOut,SyString *pFuncName)` |
|        5 |  603 | `{` |
|    39679 |  604 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|     2236 |  605 | `		SyBlobAppend(pOut,pFuncName->zString,pFuncName->nByte);` |
|     2236 |  606 | `		SyBlobAppend(pOut,"(): ",sizeof("(): ")-1);` |
|     1104 |  607 | `	}` |
|    39679 |  608 | `}` |
|        - |  609 | `/*` |
|        - |  610 | ` * Emit a runtime diagnostic as php's two copies, each behind its own ini gate` |
|        - |  611 | ` * (the caller has already cleared the error_reporting() mask and the '@' gate):` |
|        - |  612 | ` *   - LOG copy     -> the error (stderr) stream when log_errors is on:` |
|        - |  613 | ``  *                     `PHP LABEL:  BODY in FILE on line N` `` |
|        - |  614 | ` *   - DISPLAY copy -> the stream display_errors NAMES: program output (stdout) with` |
|        - |  615 | `` *                     a leading blank line, `\nLABEL: BODY in FILE on line N`, or --`` |
|        - |  616 | `` *                     for the `stderr` spelling -- the error stream with no blank`` |
|        - |  617 | ` *                     line. php writes the stderr form with fprintf(), outside the` |
|        - |  618 | ` *                     output layer, so an ob_start() never captures it and it does` |
|        - |  619 | ` *                     not count toward the program's output length.` |
|        - |  620 | `` * BODY already carries php's `func(): ` qualifier (VmDiagnosticQualify).`` |
|        - |  621 | ` * Stock CLI php (display_errors off, log_errors on) writes only the stderr copy,` |
|        - |  622 | ` * keeping program stdout clean. BODY/location are shared; only the header and the` |
|        - |  623 | ` * display copy's leading blank line differ. sWorker is reused across the two` |
|        - |  624 | ` * copies; BODY must live in a separate buffer (it does at both call sites).` |
|        - |  625 | ` */` |
|      478 |  626 | `static sxi32 VmEmitDiagnostic(ph7_vm *pVm,sxi32 iErr,` |
|        - |  627 | `	const char *zBody,sxu32 nBody,SyString *pFile,sxu32 nLine)` |
|        4 |  628 | `{` |
|      482 |  629 | `	SyBlob *pWorker = &pVm->sWorker;` |
|      482 |  630 | `	sxi32 rc = SXRET_OK;` |
|      482 |  631 | `	if( pVm->bLogErrors ){` |
|      320 |  632 | `		SyBlobReset(pWorker);` |
|      320 |  633 | `		VmDiagnosticLogHeader(pWorker,iErr);` |
|      320 |  634 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      320 |  635 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      320 |  636 | `		rc = VmWriteErrorLog(pVm,VmErrConsumer(pVm),pWorker);` |
|      158 |  637 | `	}` |
|      482 |  638 | `	if( pVm->iDisplayErrors != PH7_DISPLAY_ERRORS_OFF ){` |
|      144 |  639 | `		int bErrStream = pVm->iDisplayErrors == PH7_DISPLAY_ERRORS_STDERR;` |
|        - |  640 | `		sxi32 rc2;` |
|      144 |  641 | `		SyBlobReset(pWorker);` |
|      144 |  642 | `		if( !bErrStream ){` |
|        - |  643 | `			/* php's text-mode display copy is prefixed with a blank line */` |
|      100 |  644 | `			SyBlobAppend(pWorker,"\n",sizeof(char));` |
|       48 |  645 | `		}` |
|      144 |  646 | `		VmDiagnosticHeader(pWorker,iErr);` |
|      144 |  647 | `		SyBlobAppend(pWorker,zBody,nBody);` |
|      144 |  648 | `		VmDiagnosticLocation(pWorker,pFile,nLine);` |
|      236 |  649 | `		rc2 = VmWriteDiagnostic(pVm,` |
|       92 |  650 | `			bErrStream ? VmErrConsumer(pVm) : &pVm->sVmConsumer,pWorker,!bErrStream);` |
|        - |  651 | `		/* keep the first failure (e.g. a PH7_ABORT from a broken stderr) rather` |
|        - |  652 | `		 * than letting a later successful write mask it */` |
|      144 |  653 | `		if( rc == SXRET_OK ){` |
|      144 |  654 | `			rc = rc2;` |
|       70 |  655 | `		}` |
|       70 |  656 | `	}` |
|      482 |  657 | `	return rc;` |
|        4 |  658 | `}` |
|     2893 |  659 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  660 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  661 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  662 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  663 | `	const char *zMessage /* Null terminated error message */` |
|        - |  664 | `	)` |
|        5 |  665 | `{` |
|        - |  666 | `	SyBlob sMsg;` |
|        - |  667 | `	SyString *pFile;` |
|        - |  668 | `	sxu32 nMsg;` |
|        - |  669 | `	sxu32 nLine;` |
|     2898 |  670 | `	sxi32 rc = SXRET_OK;` |
|        - |  671 | `	char zMemMsg[128];` |
|     2898 |  672 | `	if( VmMemLimitMessage(&(*pVm),zMemMsg,sizeof(zMemMsg)) ){` |
|        - |  673 | `		/* Every engine "out of memory" wording downstream of the ceiling becomes` |
|        - |  674 | `		 * php's one sentence. Severity 256 is what makes the label read "Fatal` |
|        - |  675 | `		 * error": PHL's label table maps E_ERROR(1) to its own "Error", and this` |
|        - |  676 | `		 * diagnostic is one users match against php's output, not against the` |
|        - |  677 | `		 * engine's house style. */` |
|      ! 0 |  678 | `		zMessage = zMemMsg;` |
|      ! 0 |  679 | `		iErr = 256;` |
|      ! 0 |  680 | `		pFuncName = 0;` |
|      ! 0 |  681 | `	}` |
|     2898 |  682 | `	nMsg = (sxu32)SyStrlen(zMessage);` |
|     2898 |  683 | `	if( pVm->nSpeculative > 0 ){` |
|        - |  684 | `		/* Speculative evaluation (PH7_VmEvalConstExpr): the value is being LOOKED at,` |
|        - |  685 | `		 * not produced, so this diagnostic never happened. Count it -- the caller reads` |
|        - |  686 | `		 * the counter as "php's compiler would not have folded this". */` |
|      ! 0 |  687 | `		pVm->nSpecDiag++;` |
|      ! 0 |  688 | `		return SXRET_OK;` |
|        - |  689 | `	}` |
|        - |  690 | `	/* Peek the processed file if available */` |
|     2898 |  691 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     2898 |  692 | `	nLine = VmDiagnosticWhere(&(*pVm),&pFile);` |
|     2898 |  693 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     2898 |  694 | `	if( pFuncName && pFuncName->nByte > 0 ){` |
|        - |  695 | `		/* Qualify only when there IS a name: PH7_VmMemoryError() reports an` |
|        - |  696 | `		 * out-of-memory fatal through this path with none, and must not need` |
|        - |  697 | `		 * an allocation to say so. */` |
|      961 |  698 | `		VmDiagnosticQualify(&sMsg,pFuncName);` |
|      961 |  699 | `		SyBlobAppend(&sMsg,zMessage,nMsg);` |
|      961 |  700 | `		zMessage = (const char *)SyBlobData(&sMsg);` |
|      961 |  701 | `		nMsg = SyBlobLength(&sMsg);` |
|      472 |  702 | `	}` |
|        - |  703 | `	/* Check for user error handler. php calls it whatever error_reporting() says` |
|        - |  704 | `	 * (see VmThrowErrorAp) -- the mask gates only the printed copy below. */` |
|     2898 |  705 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)nMsg, pFile, (sxi32)nLine) ){` |
|      357 |  706 | `		VmRecordLastError(&(*pVm),iErr,zMessage,nMsg,pFile,nLine);` |
|      357 |  707 | `		if( VmErrReportWants(pVm,iErr) && pVm->nErrSuppress == 0 ){` |
|        - |  708 | `			/* error_reporting() masks a severity out of the DISPLAY, and inside` |
|        - |  709 | `			 * '@' php still runs the handler (done just above) but prints` |
|        - |  710 | `			 * nothing itself. */` |
|      202 |  711 | `			rc = VmEmitDiagnostic(pVm,iErr,zMessage,nMsg,pFile,nLine);` |
|       99 |  712 | `		}` |
|      176 |  713 | `	}` |
|     2898 |  714 | `	SyBlobRelease(&sMsg);` |
|     2898 |  715 | `	return rc;` |
|     1444 |  716 | `}` |
|        - |  717 | `/*` |
|        - |  718 | ` * Raise an out-of-memory fatal and request a clean VM halt.` |
|        - |  719 | ` *` |
|        - |  720 | ` * This is the single choke point for surfacing an allocation failure that would` |
|        - |  721 | ` * otherwise produce a silently-wrong result (a truncated string/array returned` |
|        - |  722 | ` * with a success status). It mirrors PHP's non-catchable OOM fatal: it emits a` |
|        - |  723 | ` * fatal-level diagnostic, sets a nonzero process exit status, and requests a` |
|        - |  724 | ` * VM-wide halt that unwinds via the OP_CALL/abort path — which still runs` |
|        - |  725 | ` * register_shutdown_function() callbacks (see PH7_VmByteCodeExec). Callers` |
|        - |  726 | `` * return the value of this function (PH7_ABORT) directly, or `goto Abort` after`` |
|        - |  727 | ` * calling it from a VM op.` |
|        - |  728 | ` */` |
|      ! 0 |  729 | `PH7_PRIVATE sxi32 PH7_VmMemoryError(ph7_vm *pVm)` |
|      ! 0 |  730 | `{` |
|      ! 0 |  731 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory");` |
|        - |  732 | `	/* Non-catchable, terminate with a PHP-like fatal exit status */` |
|      ! 0 |  733 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  734 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  735 | `	return PH7_ABORT;` |
|      ! 0 |  736 | `}` |
|        - |  737 | `/*` |
|        - |  738 | ` * Context wrapper around PH7_VmMemoryError() for foreign/builtin functions.` |
|        - |  739 | ` */` |
|      ! 0 |  740 | `PH7_PRIVATE sxi32 PH7_ContextMemoryError(ph7_context *pCtx)` |
|      ! 0 |  741 | `{` |
|      ! 0 |  742 | `	return PH7_VmMemoryError(pCtx->pVm);` |
|      ! 0 |  743 | `}` |
|        - |  744 | `/*` |
|        - |  745 | ` * php 8.1: an implicit float->int conversion deprecates when it loses` |
|        - |  746 | ` * precision. Used by the integer-only OPERATORS (%, \|, &, ^, <<, >>, ~),` |
|        - |  747 | ` * which truncate their operands. An integral float like 2.0 is silent.` |
|        - |  748 | ` * (Builtin int PARAMETERS need the same treatment — they coerce through each` |
|        - |  749 | ` * function's own ph7_value_to_int call, so they ride the recorded ZPP sweep.)` |
|        - |  750 | ` */` |
|        - |  751 | `/*` |
|        - |  752 | `` * TRUE when reading pVal as an `int` would lose what it holds -- the event php`` |
|        - |  753 | `` * 8.1 only DEPRECATES (`Implicit conversion from float 1.9 / float-string "1.9"`` |
|        - |  754 | `` * to int loses precision`) and the scope policy refuses outright.`` |
|        - |  755 | ` *` |
|        - |  756 | ` * Two kinds of value can lose something, and php treats them as one: a FLOAT,` |
|        - |  757 | ` * and a numeric STRING whose bytes spell a double. Which strings those are is` |
|        - |  758 | ` * not just the ones carrying a '.' or an exponent — an integer-shaped run too` |
|        - |  759 | ` * long for an int64 is a double in php too ("99999999999999999999"), and it` |
|        - |  760 | ` * loses digits exactly the same way. So the question is asked of the NUMBER the` |
|        - |  761 | ` * value converts to, whatever spelling it arrived in.` |
|        - |  762 | ` *` |
|        - |  763 | ` * A value is lossy when that number is not an exact int64: outside the range at` |
|        - |  764 | ` * all (NaN and the infinities included), or carrying a fraction. An integral` |
|        - |  765 | `` * float in range (`4.0 % 3`, `$o->i = 5.0`) loses nothing and is not lossy.`` |
|        - |  766 | ` */` |
|   133237 |  767 | `PH7_PRIVATE int VmValueIsLossyToInt(ph7_value *pVal)` |
|        5 |  768 | `{` |
|        - |  769 | `	ph7_real r;` |
|   133242 |  770 | `	if( pVal == 0 ){` |
|      ! 0 |  771 | `		return FALSE;` |
|        - |  772 | `	}` |
|   133242 |  773 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|       36 |  774 | `		r = pVal->rVal;` |
|   133247 |  775 | `	}else if( (pVal->iFlags & MEMOBJ_STRING) && pVal->pVm ){` |
|        - |  776 | `		/* Asked of a COPY: the operand is still needed intact when the answer is` |
|        - |  777 | `		 * no, and a numeric conversion would replace it. */` |
|        - |  778 | `		ph7_value sProbe;` |
|        - |  779 | `		SyString sStr;` |
|        - |  780 | `		int bReal;` |
|   101209 |  781 | `		const char *z = (const char *)SyBlobData(&pVal->sBlob);` |
|   101209 |  782 | `		sxu32 n = SyBlobLength(&pVal->sBlob), i;` |
|        - |  783 | `		/* A string with no '.', no exponent and fewer bytes than the shortest` |
|        - |  784 | `` 		 * out-of-range integer cannot spell a double, so the ordinary `$s % 2` `` |
|        - |  785 | `		 * answers without building anything. Conservative on purpose: it may` |
|        - |  786 | `		 * still probe a string that turns out to be an int, never the reverse. */` |
|   101209 |  787 | `		if( n < 19 ){` |
|   303079 |  788 | `			for( i = 0 ; i < n ; ++i ){` |
|   201927 |  789 | `				if( z[i] == '.' \|\| z[i] == 'e' \|\| z[i] == 'E' ){` |
|       25 |  790 | `					break;` |
|        - |  791 | `				}` |
|   100943 |  792 | `			}` |
|   101203 |  793 | `			if( i >= n ){` |
|   101161 |  794 | `				return FALSE;` |
|        - |  795 | `			}` |
|       23 |  796 | `		}` |
|       54 |  797 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       54 |  798 | `		PH7_MemObjInitFromString(pVal->pVm,&sProbe,&sStr);` |
|       54 |  799 | `		PH7_MemObjToNumeric(&sProbe);` |
|       54 |  800 | `		bReal = (sProbe.iFlags & MEMOBJ_REAL) != 0;` |
|       54 |  801 | `		r = sProbe.rVal;` |
|       54 |  802 | `		PH7_MemObjRelease(&sProbe);` |
|       54 |  803 | `		if( !bReal ){` |
|        9 |  804 | `			return FALSE;` |
|        - |  805 | `		}` |
|       24 |  806 | `	}else{` |
|    32004 |  807 | `		return FALSE;` |
|        - |  808 | `	}` |
|        - |  809 | `	/* The bounds are tested in DOUBLE space, BEFORE the cast: (sxi64)r is` |
|        - |  810 | `	 * undefined outside them, and a test of an undefined cast's result is one an` |
|        - |  811 | `	 * optimiser is entitled to delete -- which is exactly how the printf family's` |
|        - |  812 | `	 * PHP_INT_MIN guard disappeared. NaN fails both comparisons and either` |
|        - |  813 | `	 * infinity fails one, so all three are lossy without a libm predicate. */` |
|        - |  814 | `	/* The cast is a no-op wherever ph7_real is the double this screen is written` |
|        - |  815 | `	 * for. Under PH7_OMIT_FLOATING_POINT ph7_real is sxi64, and handing an` |
|        - |  816 | `	 * integer to a double parameter is a narrowing MSVC reports as C4244 --` |
|        - |  817 | `	 * which /WX makes a build error, so the tiny build is where it bites. */` |
|       80 |  818 | `	if( !PH7_RealFitsInt64((double)r) ){` |
|       26 |  819 | `		return TRUE;` |
|        - |  820 | `	}` |
|       56 |  821 | `	return r != (ph7_real)(sxi64)r;` |
|    66606 |  822 | `}` |
|        - |  823 | ``/* php only DEPRECATES a lossy float(-string) -> int operand (`5 % 2.7`,`` |
|        - |  824 | `` * `3 \| 1.5`, `"1.9" % 2`); PHL targets php's non-deprecated surface and rejects`` |
|        - |  825 | `` * it with a TypeError. An INTEGRAL float (`4.0 % 3`) loses nothing and is`` |
|        - |  826 | ` * accepted. Returns SXRET_OK to continue, or the throw status for the caller to` |
|        - |  827 | ` * route via PH7_DISPATCH_ENFORCE_RC. */` |
|    32557 |  828 | `PH7_PRIVATE sxi32 VmRejectFloatOperand(ph7_vm *pVm,ph7_value *pVal)` |
|        5 |  829 | `{` |
|    32562 |  830 | `	if( !VmValueIsLossyToInt(pVal) ){` |
|    32536 |  831 | `		return SXRET_OK;` |
|        - |  832 | `	}` |
|       27 |  833 | `	return VmThrowFixedError(pVm,"TypeError",` |
|       26 |  834 | `		(pVal->iFlags & MEMOBJ_REAL)` |
|        - |  835 | `			? "Implicit conversion from float to int loses precision"` |
|        - |  836 | `			: "Implicit conversion from float-string to int loses precision");` |
|    16266 |  837 | `}` |
|        - |  838 | `/*` |
|        - |  839 | ` * Single source of truth for the PHP call-depth cap policy.` |
|        - |  840 | ` * Only OP_CALL tests this — a PHP->PHP call is the sole thing that grows` |
|        - |  841 | ` * nRecursionDepth. Native re-entries (eval/include, coroutine start/resume,` |
|        - |  842 | ` * C->PHP callbacks) are bounded separately by nMaxNativeDepth in the` |
|        - |  843 | ` * VmByteCodeExec wrapper, since PHP recursion no longer grows the C stack.` |
|        - |  844 | ` */` |
|  2513646 |  845 | `PH7_PRIVATE int VmRecursionExceeded(ph7_vm *pVm)` |
|        5 |  846 | `{` |
|        - |  847 | `	/* nMaxDepth == 0 means unbounded (the host default): PHP call depth is` |
|        - |  848 | `	 * heap-bound, so only an embedder-configured cap can trip. */` |
|  2513651 |  849 | `	return pVm->nMaxDepth > 0 && pVm->nRecursionDepth > pVm->nMaxDepth;` |
|        5 |  850 | `}` |
|        - |  851 | `/*` |
|        - |  852 | ` * Single source of truth for the NATIVE VmByteCodeExec nesting cap (the C-stack` |
|        - |  853 | ` * guard) — the twin of VmRecursionExceeded for the other axis. Tested by the` |
|        - |  854 | ` * VmByteCodeExec wrapper and, before mutating VM state, by the coroutine` |
|        - |  855 | ` * start/resume entries (which splice frames in before that wrapper runs). Always` |
|        - |  856 | ` * bounded (unlike the PHP cap there is no unbounded mode — the whole point is to` |
|        - |  857 | ` * keep native re-entries off a finite C stack).` |
|        - |  858 | ` */` |
|  4597333 |  859 | `PH7_PRIVATE int VmNativeNestingExceeded(ph7_vm *pVm)` |
|        5 |  860 | `{` |
|  4597338 |  861 | `	return pVm->nVmExecDepth >= pVm->nMaxNativeDepth;` |
|        5 |  862 | `}` |
|        - |  863 | `/*` |
|        - |  864 | ` * Raise the recursion-limit fatal and request a clean VM halt. Mirrors` |
|        - |  865 | ` * PH7_VmMemoryError and PHP 8.3's non-catchable "Maximum call stack size` |
|        - |  866 | ` * reached": a catchable Error can't be used here because PH7 runs the catch` |
|        - |  867 | ` * body (and renders an uncaught exception) inline at the throw-site depth —` |
|        - |  868 | ` * which is already over the cap, so getMessage()/__toString()/the catch body` |
|        - |  869 | ` * would re-trip the limit and recurse forever. A clean fatal removes the old` |
|        - |  870 | ` * silent "return NULL and continue" hazard while keeping the promise that deep` |
|        - |  871 | ` * recursion never panics: it unwinds via the abort path and still runs` |
|        - |  872 | ` * register_shutdown_function() callbacks. Raised by OP_CALL only (the sole` |
|        - |  873 | ` * site testing the PHP call-depth cap); native nesting has its own fatal` |
|        - |  874 | ` * (VmNativeNestingFatal).` |
|        - |  875 | ` *` |
|        - |  876 | ` * Halt is requested BEFORE emitting the diagnostic, and a re-entry guard makes` |
|        - |  877 | ` * this idempotent, so an error handler that itself recurses past the cap can't` |
|        - |  878 | ` * re-enter and loop.` |
|        - |  879 | ` */` |
|        2 |  880 | `PH7_PRIVATE sxi32 VmRecursionFatal(ph7_vm *pVm)` |
|        1 |  881 | `{` |
|        3 |  882 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  883 | `		return PH7_ABORT;` |
|        - |  884 | `	}` |
|        3 |  885 | `	pVm->iExitStatus = 255;` |
|        3 |  886 | `	pVm->bHaltRequested = 1;` |
|        3 |  887 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum recursion depth of %d reached",pVm->nMaxDepth);` |
|        3 |  888 | `	return PH7_ABORT;` |
|        2 |  889 | `}` |
|        - |  890 | `/*` |
|        - |  891 | ` * Sibling of VmRecursionFatal for the NATIVE nesting bound (see the` |
|        - |  892 | ` * VmByteCodeExec wrapper): same clean-halt semantics, its own message so the` |
|        - |  893 | ` * two limits are distinguishable. Non-catchable for the same at-depth reason.` |
|        - |  894 | ` */` |
|        4 |  895 | `PH7_PRIVATE sxi32 VmNativeNestingFatal(ph7_vm *pVm)` |
|        1 |  896 | `{` |
|        5 |  897 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  898 | `		return PH7_ABORT;` |
|        - |  899 | `	}` |
|        5 |  900 | `	pVm->iExitStatus = 255;` |
|        5 |  901 | `	pVm->bHaltRequested = 1;` |
|        5 |  902 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Maximum native nesting depth reached");` |
|        5 |  903 | `	return PH7_ABORT;` |
|        3 |  904 | `}` |
|        - |  905 | `/*` |
|        - |  906 | `` * ext/pcntl's `Error installing signal handler for %d`, and it is the same`` |
|        - |  907 | ` * clean-halt fatal as the two above rather than a catchable Error -- because` |
|        - |  908 | ` * php's is not catchable either: a disposition sigaction() refuses (SIGKILL,` |
|        - |  909 | ` * SIGSTOP) is an E_ERROR under php, not the ValueError every other pcntl_signal()` |
|        - |  910 | ` * refusal is. Raised only from PH7_builtin_pcntl_signal.` |
|        - |  911 | ` */` |
|      ! 0 |  912 | `PH7_PRIVATE sxi32 PH7_VmSignalInstallFatal(ph7_vm *pVm,int signo)` |
|      ! 0 |  913 | `{` |
|      ! 0 |  914 | `	if( pVm->bHaltRequested ){` |
|      ! 0 |  915 | `		return PH7_ABORT;` |
|        - |  916 | `	}` |
|      ! 0 |  917 | `	pVm->iExitStatus = 255;` |
|      ! 0 |  918 | `	pVm->bHaltRequested = 1;` |
|      ! 0 |  919 | `	VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Error installing signal handler for %d",signo);` |
|      ! 0 |  920 | `	return PH7_ABORT;` |
|      ! 0 |  921 | `}` |
|        - |  922 | `/*` |
|        - |  923 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  924 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  925 | ` * information.` |
|        - |  926 | ` */` |
|        - |  927 | `/*` |
|        - |  928 | ` * Did memory_limit just stop this script, and if so what does php call it?` |
|        - |  929 | ` *` |
|        - |  930 | ` * The allocation that crossed the ceiling returned NULL into whichever site asked` |
|        - |  931 | ` * for it, and that site then complains in its OWN words -- "PH7 is running out of` |
|        - |  932 | ` * memory while loading variable", and a dozen others, each naming the operation` |
|        - |  933 | ` * that happened to be unlucky. php has ONE sentence for this, and it is the one` |
|        - |  934 | ` * every framework's OOM triage greps for, so the first diagnostic raised after the` |
|        - |  935 | ` * ceiling is hit becomes that sentence whatever the site meant to say.` |
|        - |  936 | ` *` |
|        - |  937 | ` * Both raise funnels consult this (the va_list one and the plain-message one): which` |
|        - |  938 | ` * of them a given allocation failure happens to reach is an accident of the site, and` |
|        - |  939 | ` * the user-visible answer must not be.` |
|        - |  940 | ` *` |
|        - |  941 | ` * Answers 0 and writes nothing when no ceiling was hit. Clears the flag, so a script` |
|        - |  942 | ` * that somehow survives its own allocation failure is not told twice -- and so the` |
|        - |  943 | ` * PH7_VmThrowError this returns into does not see it again and recurse.` |
|        - |  944 | ` */` |
|    41611 |  945 | `static int VmMemLimitMessage(ph7_vm *pVm,char *zBuf,sxu32 nBuf)` |
|        5 |  946 | `{` |
|    41616 |  947 | `	if( pVm->sAllocator.nMemTried == 0 ){` |
|    41616 |  948 | `		return 0;` |
|        - |  949 | `	}` |
|      ! 0 |  950 | `	SyBufferFormat(zBuf,nBuf,` |
|        - |  951 | `		"Allowed memory size of %u bytes exhausted (tried to allocate %u bytes)",` |
|      ! 0 |  952 | `		pVm->sAllocator.nMemLimitHit,pVm->sAllocator.nMemTried);` |
|      ! 0 |  953 | `	pVm->sAllocator.nMemTried = 0;` |
|      ! 0 |  954 | `	return 1;` |
|    20790 |  955 | `}` |
|    38718 |  956 | `static sxi32 VmThrowErrorAp(` |
|        - |  957 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  958 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  959 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  960 | `	const char *zFormat, /* Format message */` |
|        - |  961 | `	va_list ap           /* Variable list of arguments */` |
|        - |  962 | `	)` |
|        5 |  963 | `{` |
|        - |  964 | `	SyBlob sMsg;` |
|        - |  965 | `	SyString *pFile;` |
|        - |  966 | `	sxu32 nLine;` |
|    38723 |  967 | `	sxi32 rc = SXRET_OK;` |
|        - |  968 | `	char zMemMsg[128];` |
|    38723 |  969 | `	if( pVm->nSpeculative > 0 ){` |
|        - |  970 | `		/* See PH7_VmThrowError: nothing a speculative evaluation raises is observable. */` |
|      ! 0 |  971 | `		pVm->nSpecDiag++;` |
|      ! 0 |  972 | `		return SXRET_OK;` |
|        - |  973 | `	}` |
|    38723 |  974 | `	if( VmMemLimitMessage(&(*pVm),zMemMsg,sizeof(zMemMsg)) ){` |
|      ! 0 |  975 | `		return PH7_VmThrowError(&(*pVm),0,256,zMemMsg);` |
|        - |  976 | `	}` |
|        - |  977 | `	/* Peek the processed file if available */` |
|    38723 |  978 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    38723 |  979 | `	nLine = VmDiagnosticWhere(&(*pVm),&pFile);` |
|        - |  980 | ``	/* Format the raw message behind php's `func(): ` qualifier */`` |
|    38723 |  981 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|    38723 |  982 | `	VmDiagnosticQualify(&sMsg,pFuncName);` |
|    38723 |  983 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  984 | `	/* Check if a user error handler is installed. php calls it for EVERY diagnostic,` |
|        - |  985 | `	 * whatever error_reporting() says -- the mask only gates the built-in printer,` |
|        - |  986 | `	 * and a handler is expected to consult error_reporting() itself. Testing the` |
|        - |  987 | `	 * mask up here instead skipped the handler entirely for a masked severity. */` |
|    38723 |  988 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, (sxi32)nLine) ){` |
|        - |  989 | `		/* No handler or handler returned TRUE, normal processing — unless the` |
|        - |  990 | `		 * expression is under '@', which suppresses the printed diagnostic. */` |
|    34229 |  991 | `		VmRecordLastError(&(*pVm),iErr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),pFile,nLine);` |
|    34229 |  992 | `		if( !VmErrReportWants(pVm,iErr) \|\| pVm->nErrSuppress > 0 ){` |
|    33949 |  993 | `			SyBlobRelease(&sMsg);` |
|    33949 |  994 | `			return SXRET_OK;` |
|        - |  995 | `		}` |
|      424 |  996 | `		rc = VmEmitDiagnostic(pVm,iErr,(const char *)SyBlobData(&sMsg),` |
|      140 |  997 | `			SyBlobLength(&sMsg),pFile,nLine);` |
|      140 |  998 | `	}` |
|     4779 |  999 | `	SyBlobRelease(&sMsg);` |
|     4779 | 1000 | `	return rc;` |
|    19351 | 1001 | `}` |
|        - | 1002 | `/*` |
|        - | 1003 | ``  * Return the class currently active on the self-stack (the innermost `self` `` |
|        - | 1004 | ` * scope), or NULL when executing outside any class context.` |
|        - | 1005 | ` */` |
|    15750 | 1006 | `PH7_PRIVATE ph7_class * VmCurrentSelf(ph7_vm *pVm)` |
|        5 | 1007 | `{` |
|    15755 | 1008 | `	if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      217 | 1009 | `		ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      217 | 1010 | `		return apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        - | 1011 | `	}` |
|    15543 | 1012 | `	return 0;` |
|     7814 | 1013 | `}` |
|        - | 1014 | `/*` |
|        - | 1015 | ` * May the engine run an exception class's __construct for a throw it is raising` |
|        - | 1016 | ` * itself? Yes, until the nesting gets absurd. The constructor CALL can throw in` |
|        - | 1017 | ` * turn (a message-formatting error, or — the case that forced this — a` |
|        - | 1018 | ` * constructor whose own class is not method-mounted yet, which OP_CALL reports` |
|        - | 1019 | ` * as an undefined function and therefore as another engine throw). Each such` |
|        - | 1020 | ` * throw would construct another exception and recurse until the native-nesting` |
|        - | 1021 | ` * cap halted the VM with no diagnostic. A small cap keeps legitimate nesting` |
|        - | 1022 | ` * (an engine throw from inside a user exception's constructor) working and` |
|        - | 1023 | ` * stops the self-feeding case at four levels: the innermost exception is simply` |
|        - | 1024 | ` * left with an empty message. On TRUE the caller must decrement nExcCtorDepth` |
|        - | 1025 | ` * after the call.` |
|        - | 1026 | ` */` |
|        - | 1027 | `#define VM_EXC_CTOR_MAX_DEPTH 4` |
|   352071 | 1028 | `static int VmExcCtorEnter(ph7_vm *pVm)` |
|        5 | 1029 | `{` |
|   352076 | 1030 | `	if( pVm->nExcCtorDepth >= VM_EXC_CTOR_MAX_DEPTH ){` |
|      ! 0 | 1031 | `		return 0;` |
|        - | 1032 | `	}` |
|   352076 | 1033 | `	pVm->nExcCtorDepth++;` |
|   352076 | 1034 | `	return 1;` |
|   176039 | 1035 | `}` |
|        - | 1036 | `/*` |
|        - | 1037 | ` * Instantiate a built-in error class (e.g. "Error"/"TypeError"), construct it` |
|        - | 1038 | ` * with the message held in *pMsg, and throw it from the current frame. Consumes` |
|        - | 1039 | ` * and releases *pMsg. Returns PH7_EXCEPTION on success, or PH7_ABORT when the` |
|        - | 1040 | ` * class is unavailable or the engine is aborting. Shared scaffolding for the` |
|        - | 1041 | ` * typed-property / uninitialized-property / readonly error throwers.` |
|        - | 1042 | ` */` |
|   206941 | 1043 | `PH7_PRIVATE sxi32 VmThrowBuiltinErrorCode(ph7_vm *pVm,const char *zClass,sxu32 nClass,` |
|        - | 1044 | `	SyBlob *pMsg,sxi32 iCode)` |
|        5 | 1045 | `{` |
|        - | 1046 | `	ph7_class *pErrClass;` |
|        - | 1047 | `	ph7_class_instance *pThis;` |
|        - | 1048 | `	ph7_class_method *pCons;` |
|        - | 1049 | `	VmFrame *pFrame;` |
|        - | 1050 | `	sxi32 rc;` |
|   206946 | 1051 | `	pErrClass = PH7_VmExtractClass(&(*pVm),zClass,nClass,TRUE,0);` |
|   206946 | 1052 | `	if( pErrClass == 0 ){` |
|      ! 0 | 1053 | `		SyBlobRelease(pMsg);` |
|      ! 0 | 1054 | `		return PH7_ABORT;` |
|        - | 1055 | `	}` |
|   206946 | 1056 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|   206946 | 1057 | `	if( pThis == 0 ){` |
|      ! 0 | 1058 | `		SyBlobRelease(pMsg);` |
|      ! 0 | 1059 | `		return PH7_ABORT;` |
|        - | 1060 | `	}` |
|   206946 | 1061 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|   206946 | 1062 | `	if( pCons && VmExcCtorEnter(&(*pVm)) ){` |
|        - | 1063 | `		ph7_value sArg,sCode;` |
|        - | 1064 | `		ph7_value *apArg[2];` |
|        - | 1065 | `		SyString sMsgStr;` |
|   206946 | 1066 | `		int nArg = 1;` |
|   206946 | 1067 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|   206946 | 1068 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|   206946 | 1069 | `		apArg[0] = &sArg;` |
|   206946 | 1070 | `		if( iCode != 0 ){` |
|        - | 1071 | `			/* php's $code, second constructor argument -- the DOMException codes a` |
|        - | 1072 | `			 * program compares against (DOM_NOT_FOUND_ERR & co) travel this way. */` |
|       67 | 1073 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|       67 | 1074 | `			apArg[1] = &sCode;` |
|       67 | 1075 | `			nArg = 2;` |
|       33 | 1076 | `		}` |
|   206946 | 1077 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|   206946 | 1078 | `		if( iCode != 0 ){` |
|       67 | 1079 | `			PH7_MemObjRelease(&sCode);` |
|       33 | 1080 | `		}` |
|   206946 | 1081 | `		PH7_MemObjRelease(&sArg);` |
|   206946 | 1082 | `		pVm->nExcCtorDepth--;` |
|   103469 | 1083 | `	}` |
|   206946 | 1084 | `	SyBlobRelease(pMsg);` |
|   206946 | 1085 | `	pFrame = pVm->pFrame;` |
|   206946 | 1086 | `	if( pFrame ){` |
|   206946 | 1087 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   206946 | 1088 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|   103469 | 1089 | `	}` |
|   206946 | 1090 | `	rc = VmThrowException(&(*pVm),pThis);` |
|   206946 | 1091 | `	PH7_ClassInstanceUnref(pThis);` |
|   206946 | 1092 | `	if( rc == SXERR_ABORT ){` |
|       40 | 1093 | `		return PH7_ABORT;` |
|        - | 1094 | `	}` |
|   206910 | 1095 | `	return PH7_EXCEPTION;` |
|   103474 | 1096 | `}` |
|        - | 1097 | `/* The same with php's default $code of 0, which is what all but the DOM refusals` |
|        - | 1098 | ` * carry. */` |
|   206360 | 1099 | `PH7_PRIVATE sxi32 VmThrowBuiltinError(ph7_vm *pVm,const char *zClass,sxu32 nClass,SyBlob *pMsg)` |
|        5 | 1100 | `{` |
|   206365 | 1101 | `	return VmThrowBuiltinErrorCode(pVm,zClass,nClass,pMsg,0);` |
|        5 | 1102 | `}` |
|        - | 1103 | `/*` |
|        - | 1104 | ` * Throw a built-in error class (e.g. "Error") carrying a FIXED message string.` |
|        - | 1105 | ` * Thin wrapper over VmThrowBuiltinError for the several dispatch-loop sites that` |
|        - | 1106 | ` * raise a constant-message catchable Error; returns PH7_EXCEPTION (or PH7_ABORT` |
|        - | 1107 | ` * when the class is unavailable / the engine is aborting) so the caller routes the` |
|        - | 1108 | ` * result through its normal goto Exception / goto Abort.` |
|        - | 1109 | ` */` |
|      322 | 1110 | `PH7_PRIVATE sxi32 VmThrowFixedError(ph7_vm *pVm, const char *zClass, const char *zMsg)` |
|        5 | 1111 | `{` |
|      327 | 1112 | `	return VmThrowFixedErrorCode(pVm,zClass,0,zMsg);` |
|        5 | 1113 | `}` |
|        - | 1114 | `/* The same, carrying php's $code: what a native class's property handler refused` |
|        - | 1115 | ` * with (PH7_NativePropCtx::iThrowCode) is raised through here. */` |
|      581 | 1116 | `PH7_PRIVATE sxi32 VmThrowFixedErrorCode(ph7_vm *pVm,const char *zClass,sxi32 iCode,` |
|        - | 1117 | `	const char *zMsg)` |
|        5 | 1118 | `{` |
|        - | 1119 | `	SyBlob sMsg;` |
|      586 | 1120 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|      586 | 1121 | `	SyBlobAppend(&sMsg, zMsg, SyStrlen(zMsg));` |
|      586 | 1122 | `	return VmThrowBuiltinErrorCode(pVm, zClass, SyStrlen(zClass), &sMsg, iCode);` |
|        5 | 1123 | `}` |
|        - | 1124 | `/*` |
|        - | 1125 | ` * Enum case singletons (PHP 8.1).` |
|        - | 1126 | ` *` |
|        - | 1127 | `` * Each `case` of an enum is a class constant (PH7_CLASS_ATTR_ENUMCASE) whose`` |
|        - | 1128 | ` * slot holds THE singleton instance of the enum class for that case; strict` |
|        - | 1129 | `` * `===` between two accesses is then the ordinary instance-pointer identity.`` |
|        - | 1130 | ` * Materialization is lazy and all-at-once on first access, matching php: the` |
|        - | 1131 | ` * backing-value type check and the duplicate-value check only fire when a` |
|        - | 1132 | ` * case (or cases()/from()/tryFrom()) is first touched.` |
|        - | 1133 | ` */` |
|        - | 1134 | `/* Write [pSrcVal] into the instance property [zProp] of [pObj], clearing the` |
|        - | 1135 | ` * readonly write-once latch so later user writes raise php's "Cannot modify` |
|        - | 1136 | ` * readonly property" through the normal store path. */` |
|        - | 1137 |  |
|        - | 1138 | ``/* Return the backing value (the `value` property) of an already-materialized`` |
|        - | 1139 | ` * enum case, or 0 when unavailable (pure enum / not yet materialized). */` |
|      514 | 1140 | `PH7_PRIVATE ph7_value * VmEnumCaseBackingValue(ph7_vm *pVm,ph7_class_attr *pCase)` |
|        3 | 1141 | `{` |
|      517 | 1142 | `	ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pCase->nIdx);` |
|        - | 1143 | `	ph7_class_instance *pObj;` |
|        - | 1144 | `	SyHashEntry *pEntry;` |
|      517 | 1145 | `	if( pSlot == 0 \|\| (pSlot->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      101 | 1146 | `		return 0;` |
|        - | 1147 | `	}` |
|      419 | 1148 | `	pObj = (ph7_class_instance *)pSlot->x.pOther;` |
|      419 | 1149 | `	pEntry = SyHashGet(&pObj->hAttr,"value",sizeof("value")-1);` |
|      419 | 1150 | `	if( pEntry == 0 ){` |
|      ! 0 | 1151 | `		return 0;` |
|        - | 1152 | `	}` |
|      419 | 1153 | `	return (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,((VmClassAttr *)pEntry->pUserData)->nIdx);` |
|      260 | 1154 | `}` |
|        - | 1155 | `/*` |
|        - | 1156 | ` * Raise the pending self-referencing-constant Error recorded by an inner` |
|        - | 1157 | ` * initializer evaluation (pConstCycleAttr). Called only at nConstEvalDepth 0 —` |
|        - | 1158 | ` * a throw inside an initializer mini-exec cannot be routed to a user catch` |
|        - | 1159 | ` * (pre-existing engine restriction), so the outermost, opcode-level evaluation` |
|        - | 1160 | ` * raises it. Returns the throw status to park/route.` |
|        - | 1161 | ` */` |
|        2 | 1162 | `PH7_PRIVATE sxi32 VmConstCycleThrow(ph7_vm *pVm)` |
|        1 | 1163 | `{` |
|        - | 1164 | `	SyBlob sMsg;` |
|        3 | 1165 | `	ph7_class_attr *pAttr = pVm->pConstCycleAttr;` |
|        3 | 1166 | `	ph7_class *pOwner = pVm->pConstCycleClass;` |
|        3 | 1167 | `	pVm->pConstCycleAttr = 0;` |
|        3 | 1168 | `	pVm->pConstCycleClass = 0;` |
|        3 | 1169 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 1170 | `	SyBlobFormat(&sMsg,"Cannot declare self-referencing constant %z::%z",` |
|        1 | 1171 | `		pOwner ? &pOwner->sDisp : &pAttr->sName,&pAttr->sName);` |
|        3 | 1172 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1173 | `}` |
|        - | 1174 | `/*` |
|        - | 1175 | ` * Materialize ONE case singleton of an enum class. php 8.1 semantics: cases` |
|        - | 1176 | ` * materialize lazily and individually on first access — the backing-value` |
|        - | 1177 | ` * type check fires per case, and the duplicate-value check compares only` |
|        - | 1178 | ` * against cases that have already materialized (a broken sibling case does` |
|        - | 1179 | ` * not poison a valid one). Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT` |
|        - | 1180 | ` * of a thrown catchable error — TypeError (backing type mismatch) or Error` |
|        - | 1181 | ` * (duplicate value / self-reference) — which the caller routes` |
|        - | 1182 | ` * (VmBoundaryPark at an opcode site, direct return from a builtin thunk).` |
|        - | 1183 | ` */` |
|      643 | 1184 | `PH7_PRIVATE sxi32 VmEnumMaterializeCase(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pCase)` |
|        5 | 1185 | `{` |
|        - | 1186 | `	ph7_class_attr **apCase;` |
|        - | 1187 | `	ph7_class_instance *pObj;` |
|        - | 1188 | `	ph7_value *pSlot;` |
|        - | 1189 | `	ph7_value sBacking,sPropVal;` |
|        - | 1190 | `	sxu32 i;` |
|      648 | 1191 | `	if( pCase->nIdx != SXU32_HIGH ){` |
|      442 | 1192 | `		return SXRET_OK;` |
|        - | 1193 | `	}` |
|      208 | 1194 | `	if( pCase->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - | 1195 | ``		/* `case A = self::A->value` — record the cycle; the outermost`` |
|        - | 1196 | `		 * evaluation raises it (see VmConstCycleThrow). */` |
|      ! 0 | 1197 | `		if( pVm->pConstCycleAttr == 0 ){` |
|      ! 0 | 1198 | `			pVm->pConstCycleAttr = pCase;` |
|      ! 0 | 1199 | `			pVm->pConstCycleClass = pClass;` |
|      ! 0 | 1200 | `		}` |
|      ! 0 | 1201 | `		return SXRET_OK;` |
|        - | 1202 | `	}` |
|      208 | 1203 | `	PH7_MemObjInit(pVm,&sBacking);` |
|      208 | 1204 | `	if( pClass->nEnumBacking != 0 ){` |
|      137 | 1205 | `		if( pCase->pNativeValue ){` |
|        - | 1206 | `			/* A NATIVE enum states its backing value as a literal: there is no` |
|        - | 1207 | `			 * compiler to have emitted the byte-code branch below, and a literal` |
|        - | 1208 | `			 * is what that byte-code would have produced anyway. */` |
|       34 | 1209 | `			PH7_NativeLiteralValue(&(*pVm),pCase->pNativeValue,&sBacking);` |
|      121 | 1210 | `		}else if( SySetUsed(&pCase->aByteCode) > 0 ){` |
|        - | 1211 | ``			/* pConstEvalClass: `case A = self::OFF + 1` resolves self::, and the`` |
|        - | 1212 | `			 * frame marker keeps it reachable when the first access happens` |
|        - | 1213 | `			 * inside another class's method (VmLocalExec pushes no frame, so` |
|        - | 1214 | `			 * that method's frame is still the current one). */` |
|      105 | 1215 | `			ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      105 | 1216 | `			void *pSaveFrame = pVm->pConstEvalFrame;` |
|        - | 1217 | `			sxi32 rcExec;` |
|      105 | 1218 | `			pVm->pConstEvalClass = pClass;` |
|      105 | 1219 | `			pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|      105 | 1220 | `			pCase->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      105 | 1221 | `			pVm->nConstEvalDepth++;` |
|      105 | 1222 | `			rcExec = VmLocalExec(&(*pVm),&pCase->aByteCode,&sBacking,FALSE);` |
|      105 | 1223 | `			pVm->nConstEvalDepth--;` |
|      105 | 1224 | `			pCase->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      105 | 1225 | `			pVm->pConstEvalClass = pSaveCtx;` |
|      105 | 1226 | `			pVm->pConstEvalFrame = pSaveFrame;` |
|      105 | 1227 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 1228 | `				/* The backing expression raised: abandon materialization and` |
|        - | 1229 | `				 * hand the status to the caller to park/route. */` |
|        3 | 1230 | `				PH7_MemObjRelease(&sBacking);` |
|        3 | 1231 | `				return rcExec;` |
|        - | 1232 | `			}` |
|      103 | 1233 | `			if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|      ! 0 | 1234 | `				PH7_MemObjRelease(&sBacking);` |
|      ! 0 | 1235 | `				return VmConstCycleThrow(&(*pVm));` |
|        - | 1236 | `			}` |
|       49 | 1237 | `		}` |
|      135 | 1238 | `		if( (sBacking.iFlags & pClass->nEnumBacking) == 0 ){` |
|        - | 1239 | `			/* php: TypeError, checked lazily at first case access */` |
|        - | 1240 | `			SyBlob sMsg;` |
|        3 | 1241 | `			const char *zGiven = ph7_type_name(&sBacking);` |
|        3 | 1242 | `			PH7_MemObjRelease(&sBacking);` |
|        3 | 1243 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        2 | 1244 | `			SyBlobFormat(&sMsg,"Enum case type %s does not match enum backing type %s",` |
|        2 | 1245 | `				zGiven,(pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string");` |
|        3 | 1246 | `			return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        - | 1247 | `		}` |
|      133 | 1248 | `		if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        - | 1249 | `			/* Normalize a whole-real (PHL flags them MEMOBJ_REAL\|MEMOBJ_INT,` |
|        - | 1250 | `			 * the typed-constant leniency) to a genuine int. */` |
|       30 | 1251 | `			PH7_MemObjToInteger(&sBacking);` |
|       16 | 1252 | `		}else{` |
|      105 | 1253 | `			PH7_MemObjToString(&sBacking);` |
|        - | 1254 | `		}` |
|        - | 1255 | `		/* php: two cases sharing one backing value are an Error — compared` |
|        - | 1256 | `		 * against already-materialized cases only (php registers values as` |
|        - | 1257 | `		 * each case evaluates). */` |
|      133 | 1258 | `		apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      451 | 1259 | `		for( i = 0 ; i < SySetUsed(&pClass->aEnumCases) ; i++ ){` |
|        - | 1260 | `			ph7_value *pPrev;` |
|      325 | 1261 | `			int bDup = 0;` |
|      325 | 1262 | `			if( apCase[i] == pCase ){` |
|      131 | 1263 | `				continue;` |
|        - | 1264 | `			}` |
|      197 | 1265 | `			pPrev = VmEnumCaseBackingValue(&(*pVm),apCase[i]);` |
|      197 | 1266 | `			if( pPrev ){` |
|       99 | 1267 | `				if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       16 | 1268 | `					bDup = (pPrev->x.iVal == sBacking.x.iVal);` |
|        9 | 1269 | `				}else{` |
|      100 | 1270 | `					bDup = SyBlobLength(&pPrev->sBlob) == SyBlobLength(&sBacking.sBlob)` |
|       82 | 1271 | `						&& SyMemcmp(SyBlobData(&pPrev->sBlob),SyBlobData(&sBacking.sBlob),` |
|       30 | 1272 | `							SyBlobLength(&sBacking.sBlob)) == 0;` |
|        - | 1273 | `				}` |
|       48 | 1274 | `			}` |
|      197 | 1275 | `			if( bDup ){` |
|        - | 1276 | `				/* php prints the two cases in DECLARATION order regardless of` |
|        - | 1277 | `				 * which one is being evaluated. */` |
|        3 | 1278 | `				ph7_class_attr *pFirst = apCase[i], *pSecond = pCase;` |
|        - | 1279 | `				SyBlob sMsg;` |
|        - | 1280 | `				sxu32 j;` |
|        5 | 1281 | `				for( j = 0 ; j < SySetUsed(&pClass->aEnumCases) ; j++ ){` |
|        5 | 1282 | `					if( apCase[j] == pCase ){ break; }` |
|        2 | 1283 | `				}` |
|        3 | 1284 | `				if( j < i ){` |
|      ! 0 | 1285 | `					pFirst = pCase;` |
|      ! 0 | 1286 | `					pSecond = apCase[i];` |
|      ! 0 | 1287 | `				}` |
|        3 | 1288 | `				PH7_MemObjRelease(&sBacking);` |
|        3 | 1289 | `				SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 1290 | `				SyBlobFormat(&sMsg,"Duplicate value in enum %z for cases %z and %z",` |
|        1 | 1291 | `					&pClass->sDisp,&pFirst->sName,&pSecond->sName);` |
|        3 | 1292 | `				return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 1293 | `			}` |
|       99 | 1294 | `		}` |
|       63 | 1295 | `	}` |
|        - | 1296 | `	/* Create the singleton and fill its readonly props */` |
|      202 | 1297 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|      202 | 1298 | `	if( pObj == 0 ){` |
|      ! 0 | 1299 | `		PH7_MemObjRelease(&sBacking);` |
|      ! 0 | 1300 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1301 | `			"Cannot create enum case %z::%z due to a memory failure",` |
|      ! 0 | 1302 | `			&pClass->sDisp,&pCase->sName);` |
|      ! 0 | 1303 | `		return PH7_ABORT;` |
|        - | 1304 | `	}` |
|      202 | 1305 | `	PH7_MemObjInitFromString(pVm,&sPropVal,&pCase->sName);` |
|      202 | 1306 | `	PH7_NativeSetProp(&(*pVm),pObj,"name",sizeof("name")-1,&sPropVal);` |
|      202 | 1307 | `	PH7_MemObjRelease(&sPropVal);` |
|      202 | 1308 | `	if( pClass->nEnumBacking != 0 ){` |
|      131 | 1309 | `		PH7_NativeSetProp(&(*pVm),pObj,"value",sizeof("value")-1,&sBacking);` |
|       63 | 1310 | `	}` |
|      202 | 1311 | `	PH7_MemObjRelease(&sBacking);` |
|        - | 1312 | `	/* Park the singleton in the case's constant slot. The slot takes over` |
|        - | 1313 | `	 * the instance's initial iRef=1 (synthesized-object invariant). */` |
|      202 | 1314 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      202 | 1315 | `	if( pSlot == 0 ){` |
|      ! 0 | 1316 | `		PH7_ClassInstanceUnref(pObj);` |
|      ! 0 | 1317 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 1318 | `			"Cannot reserve a memory object for enum case %z::%z",` |
|      ! 0 | 1319 | `			&pClass->sDisp,&pCase->sName);` |
|      ! 0 | 1320 | `		return PH7_ABORT;` |
|        - | 1321 | `	}` |
|      202 | 1322 | `	pSlot->x.pOther = pObj;` |
|      202 | 1323 | `	MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|      202 | 1324 | `	PH7_VmRefObjInstall(&(*pVm),pSlot->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      202 | 1325 | `	pCase->nIdx = pSlot->nIdx;` |
|      202 | 1326 | `	return SXRET_OK;` |
|      324 | 1327 | `}` |
|        - | 1328 | `/*` |
|        - | 1329 | ` * Materialize EVERY case singleton of [pClass], in declaration order — the` |
|        - | 1330 | ` * cases()/from()/tryFrom() entry point (php equally evaluates all cases` |
|        - | 1331 | ` * there, so a broken case surfaces its error at the same point).` |
|        - | 1332 | ` */` |
|      291 | 1333 | `PH7_PRIVATE sxi32 VmEnumMaterialize(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1334 | `{` |
|        - | 1335 | `	ph7_class_attr **apCase;` |
|        - | 1336 | `	sxu32 n;` |
|      296 | 1337 | `	if( (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|      ! 0 | 1338 | `		return SXRET_OK;` |
|        - | 1339 | `	}` |
|      296 | 1340 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      909 | 1341 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|      624 | 1342 | `		sxi32 rc = VmEnumMaterializeCase(&(*pVm),pClass,apCase[n]);` |
|      624 | 1343 | `		if( rc != SXRET_OK ){` |
|        8 | 1344 | `			return rc;` |
|        - | 1345 | `		}` |
|      309 | 1346 | `	}` |
|      290 | 1347 | `	return SXRET_OK;` |
|      150 | 1348 | `}` |
|        - | 1349 | `/*` |
|        - | 1350 | ` * Resolve [pName] (a string ph7_value holding an enum FQN) to its enum class,` |
|        - | 1351 | ` * or 0 when the name does not name an enum.` |
|        - | 1352 | ` */` |
|      234 | 1353 | `PH7_PRIVATE ph7_class * VmExtractEnumClass(ph7_vm *pVm,ph7_value *pName)` |
|        3 | 1354 | `{` |
|        - | 1355 | `	ph7_class *pClass;` |
|      237 | 1356 | `	if( (pName->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pName->sBlob) < 1 ){` |
|      ! 0 | 1357 | `		return 0;` |
|        - | 1358 | `	}` |
|      353 | 1359 | `	pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pName->sBlob),` |
|      116 | 1360 | `		SyBlobLength(&pName->sBlob),FALSE,0);` |
|      241 | 1361 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|        6 | 1362 | `		pClass = pClass->pNextName;` |
|        2 | 1363 | `	}` |
|      237 | 1364 | `	return pClass;` |
|      119 | 1365 | `}` |
|        - | 1366 | `/*` |
|        - | 1367 | ` * The line a LAZY class initializer about to run should report a throw at: the` |
|        - | 1368 | ` * line of the access that triggered it. Answers 0 — meaning "keep the` |
|        - | 1369 | ` * initializer's own line" — when the access site is INTERNAL code (a prelude` |
|        - | 1370 | ` * chunk, e.g. ReflectionClass::getConstants() calling the materializer): its` |
|        - | 1371 | ` * line numbers belong to an embedded source that PH7_VmStampThrowableSite will` |
|        - | 1372 | ` * not name, so reporting one would pair a foreign line with the user's file.` |
|        - | 1373 | ` */` |
|     1158 | 1374 | `static sxu32 VmLazyInitLineHere(ph7_vm *pVm)` |
|        5 | 1375 | `{` |
|     1163 | 1376 | `	VmFrame *pInner = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     1158 | 1377 | `	if( pInner && pInner->pUserData` |
|      677 | 1378 | `	 && ((ph7_vm_func *)pInner->pUserData)->sFile.nByte == 0 ){` |
|      ! 0 | 1379 | `		return 0;` |
|        - | 1380 | `	}` |
|     1163 | 1381 | `	return pVm->nCurLine;` |
|      584 | 1382 | `}` |
|        - | 1383 | `/*` |
|        - | 1384 | ` * Hand a reserved memory-object slot back to the free list. Takes the INDEX,` |
|        - | 1385 | ` * not the pointer -- which used to be load-bearing, because a nested evaluation` |
|        - | 1386 | ` * (an initializer, or the constructor of the very TypeError being raised) grew` |
|        - | 1387 | ` * and REALLOC'd the pool and left a pointer taken before it dangling. P1's fixed` |
|        - | 1388 | ` * segments retired that; an index is still the right currency for a free, since` |
|        - | 1389 | ` * the free list is keyed by one. The slot's contents are released first:` |
|        - | 1390 | ` * PH7_ReserveMemObj re-inits a recycled slot without releasing it, so a string` |
|        - | 1391 | ` * blob / array / object left in there would be orphaned once per evaluation,` |
|        - | 1392 | ` * which for a constant that re-evaluates on every access grows without bound.` |
|        - | 1393 | ` */` |
|       36 | 1394 | `static void VmRecycleMemObj(ph7_vm *pVm,sxu32 nIdx)` |
|        3 | 1395 | `{` |
|       39 | 1396 | `	ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|       39 | 1397 | `	if( pObj == 0 ){` |
|      ! 0 | 1398 | `		return;` |
|        - | 1399 | `	}` |
|       39 | 1400 | `	PH7_MemObjRelease(pObj);` |
|       39 | 1401 | `	VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       21 | 1402 | `}` |
|        - | 1403 | `/*` |
|        - | 1404 | ` * Evaluate a class constant's initializer on demand.` |
|        - | 1405 | ` *` |
|        - | 1406 | ` * Constant slots are normally filled eagerly at class mount, but a constant` |
|        - | 1407 | ` * whose initializer references ANOTHER not-yet-mounted constant (same class —` |
|        - | 1408 | `` * `const B = self::A + 1` — or a class mounted later in hash order) reaches`` |
|        - | 1409 | ` * OP_MEMBER with nIdx still unset; before this helper the load silently` |
|        - | 1410 | ` * produced NULL (a mount-order-dependent silent wrong answer, surfaced by the` |
|        - | 1411 | ` * enum work, 13 Jul 2026). Evaluates the initializer now — with` |
|        - | 1412 | ` * pConstEvalClass set so self::/parent:: resolve — memoizes the slot, and` |
|        - | 1413 | ` * leaves the mount loop's later visit to skip it (nIdx already set).` |
|        - | 1414 | ` * A re-entrant evaluation of the SAME constant is php's catchable` |
|        - | 1415 | ` * "Cannot declare self-referencing constant" Error.` |
|        - | 1416 | ` */` |
|     1276 | 1417 | `PH7_PRIVATE sxi32 VmClassConstEvalOnDemand(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 1418 | `{` |
|        - | 1419 | `	ph7_value *pMemObj;` |
|     1276 | 1420 | `	if( pAttr->nIdx != SXU32_HIGH` |
|     1276 | 1421 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|     1281 | 1422 | `		\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0 ){` |
|      ! 0 | 1423 | `		return SXRET_OK;` |
|        - | 1424 | `	}` |
|     1281 | 1425 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - | 1426 | `		/* Cycle: record it for the OUTERMOST evaluation to raise` |
|        - | 1427 | `		 * (VmConstCycleThrow) — a throw at this inner level would be lost` |
|        - | 1428 | `		 * inside the initializer mini-exec. Loads NULL benignly here. */` |
|        3 | 1429 | `		if( pVm->pConstCycleAttr == 0 ){` |
|        3 | 1430 | `			pVm->pConstCycleAttr = pAttr;` |
|        3 | 1431 | `			pVm->pConstCycleClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        1 | 1432 | `		}` |
|        3 | 1433 | `		return SXRET_OK;` |
|        - | 1434 | `	}` |
|     1279 | 1435 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1279 | 1436 | `	if( pMemObj == 0 ){` |
|      ! 0 | 1437 | `		return SXERR_MEM;` |
|        - | 1438 | `	}` |
|     1279 | 1439 | `	if( pAttr->pNativeValue ){` |
|        - | 1440 | `		/* A NATIVE class's constant carries a literal instead of byte-code. The` |
|        - | 1441 | `		 * mount loop materializes those, but it stopped visiting untyped constants` |
|        - | 1442 | `		 * when they went lazy (17th session), so this path — the only one an` |
|        - | 1443 | `		 * untyped constant now reaches — has to know about them too. It did not,` |
|        - | 1444 | `		 * which is why every native class constant read NULL: nothing had declared` |
|        - | 1445 | `		 * one until the date family did (DateTimeInterface::ATOM,` |
|        - | 1446 | `		 * DatePeriod::EXCLUDE_START_DATE). A literal cannot throw, so there is no` |
|        - | 1447 | `		 * failure path to mirror below. */` |
|      781 | 1448 | `		PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|      781 | 1449 | `		pAttr->nIdx = pMemObj->nIdx;` |
|      781 | 1450 | `		return SXRET_OK;` |
|        - | 1451 | `	}` |
|      503 | 1452 | `	if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      503 | 1453 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      503 | 1454 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|      503 | 1455 | `		ph7_class_attr *pSaveCycle = pVm->pConstCycleAttr;` |
|        - | 1456 | `		sxu32 nSaveLazyLine;` |
|        - | 1457 | `		sxi32 nSaveLazyDepth;` |
|        - | 1458 | `		sxu32 nSlot;` |
|        - | 1459 | `		sxi32 rcExec;` |
|      503 | 1460 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING;` |
|      503 | 1461 | `		pVm->pConstEvalClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 1462 | `		/* Mark the frame current at eval start: while it stays current, self::/` |
|        - | 1463 | `		 * parent:: in the initializer resolve to pConstEvalClass rather than the` |
|        - | 1464 | `		 * enclosing method's class (VmLocalExec pushes no frame of its own). */` |
|      503 | 1465 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 1466 | `		/* php evaluates this expression HERE, at the access, so that is the line a` |
|        - | 1467 | `		 * throw out of its own bytecode carries. */` |
|      503 | 1468 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|      503 | 1469 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|      503 | 1470 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|      503 | 1471 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|      503 | 1472 | `		pVm->nConstEvalDepth++;` |
|      503 | 1473 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      503 | 1474 | `		pVm->nConstEvalDepth--;` |
|      503 | 1475 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|      503 | 1476 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|      503 | 1477 | `		pVm->pConstEvalClass = pSaveCtx;` |
|      503 | 1478 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|      503 | 1479 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|      503 | 1480 | `		nSlot = pMemObj->nIdx; /* the pool can move below; address the slot by index */` |
|      498 | 1481 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT` |
|      474 | 1482 | `		 \|\| pVm->pConstCycleAttr != pSaveCycle ){` |
|        - | 1483 | `			/* The initializer FAILED. Do not memoize the slot: php evaluates a` |
|        - | 1484 | `			 * class constant's expression at each access until one of them` |
|        - | 1485 | ``			 * succeeds, so `class C { const K = UNDEF; }` raises`` |
|        - | 1486 | ``			 * `Undefined constant "UNDEF"` on EVERY read of C::K, not just the`` |
|        - | 1487 | `			 * first. Memoizing left the constant reading NULL, in silence, for` |
|        - | 1488 | `			 * the rest of the run — and made a static default that named it` |
|        - | 1489 | `			 * (whose own evaluation is deferred to first access) find it` |
|        - | 1490 | `			 * materialized and raise nothing at all. Give the reserved slot back` |
|        - | 1491 | `			 * and leave nIdx unset, which is what keys the on-demand path.` |
|        - | 1492 | `			 * A recorded CYCLE is a failure the same way: it does not throw where` |
|        - | 1493 | `			 * it is found — an inner level only records it — but the value is` |
|        - | 1494 | `			 * unusable and the next access must be able to detect it again.` |
|        - | 1495 | `			 * No loop: each access runs the initializer once and raises. */` |
|       35 | 1496 | `			VmRecycleMemObj(&(*pVm),nSlot);` |
|       35 | 1497 | `			if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 1498 | `				/* Hand the status to the caller to park/route. */` |
|       32 | 1499 | `				return rcExec;` |
|        - | 1500 | `			}` |
|        3 | 1501 | `			if( pVm->nConstEvalDepth == 0 ){` |
|        - | 1502 | `				/* Outermost level: raise the cycle here, at opcode level, where it` |
|        - | 1503 | `				 * routes to a catch. Deeper in, the record travels outward. */` |
|        3 | 1504 | `				return VmConstCycleThrow(&(*pVm));` |
|        - | 1505 | `			}` |
|      ! 0 | 1506 | `			return SXRET_OK;` |
|        - | 1507 | `		}` |
|      471 | 1508 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - | 1509 | `			/* Typed constant (PHP 8.3) whose value only exists now: check BEFORE` |
|        - | 1510 | `			 * memoizing, so a mismatch leaves the slot unmaterialized and the next` |
|        - | 1511 | `			 * access raises again — php re-runs the whole materialization each` |
|        - | 1512 | `			 * time. A pass may widen int -> float in place, which is the value` |
|        - | 1513 | `			 * memoized below. The check can THROW, and constructing that TypeError` |
|        - | 1514 | `			 * runs php code that used to grow (and realloc) aMemObj — so the slot is` |
|        - | 1515 | `			 * addressed by index from here on, never through pMemObj. Redundant` |
|        - | 1516 | `			 * now the table is segmented; left for the harvest sweep. */` |
|        5 | 1517 | `			sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,1 /* lazy */);` |
|        5 | 1518 | `			if( rcType != SXRET_OK ){` |
|        5 | 1519 | `				VmRecycleMemObj(&(*pVm),nSlot);` |
|        5 | 1520 | `				return rcType;` |
|        - | 1521 | `			}` |
|      ! 0 | 1522 | `		}` |
|        - | 1523 | `		/* Memoize the value. */` |
|      467 | 1524 | `		pAttr->nIdx = nSlot;` |
|      467 | 1525 | `		PH7_VmRefObjInstall(&(*pVm),nSlot,0,0,VM_REF_IDX_KEEP);` |
|      467 | 1526 | `		return SXRET_OK;` |
|        - | 1527 | `	}` |
|      ! 0 | 1528 | `	pAttr->nIdx = pMemObj->nIdx;` |
|      ! 0 | 1529 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      ! 0 | 1530 | `	return SXRET_OK;` |
|      629 | 1531 | `}` |
|        - | 1532 | `/*` |
|        - | 1533 | ` * Public seam for the constant-slot readers outside vm.c (reflection,` |
|        - | 1534 | ` * get_class_vars): class constants evaluate lazily, so a listing-style read` |
|        - | 1535 | ` * must materialize the slot first. Returns SXRET_OK or a throw status.` |
|        - | 1536 | ` */` |
|      868 | 1537 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassConst(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 1538 | `{` |
|      873 | 1539 | `	if( pAttr->nIdx != SXU32_HIGH \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      380 | 1540 | `		return SXRET_OK;` |
|        - | 1541 | `	}` |
|      494 | 1542 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       24 | 1543 | `		return VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|        - | 1544 | `	}` |
|      471 | 1545 | `	return VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|      425 | 1546 | `}` |
|        - | 1547 | `/*` |
|        - | 1548 | `` * Throw php's catchable Error for an append (`$a[] = v`) whose saturated`` |
|        - | 1549 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Called by the hashmap` |
|        - | 1550 | ` * layer; the store opcodes route the returned PH7_EXCEPTION through the` |
|        - | 1551 | ` * standard dispatch (PH7_DISPATCH_ENFORCE_RC), builtins return it as-is.` |
|        - | 1552 | ` */` |
|        8 | 1553 | `PH7_PRIVATE sxi32 PH7_VmThrowArrayNextIndexError(ph7_vm *pVm)` |
|        2 | 1554 | `{` |
|        - | 1555 | `	SyBlob sMsg;` |
|       10 | 1556 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       10 | 1557 | `	SyBlobFormat(&sMsg,"Cannot add element to the array as the next element is already occupied");` |
|       10 | 1558 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        2 | 1559 | `}` |
|        - | 1560 | `/*` |
|        - | 1561 | `` * php's fatal for `$GLOBALS[] = ...` — appending to the global symbol table`` |
|        - | 1562 | ` * has no name to bind. A compile-time fatal in php (NOT a catchable Error);` |
|        - | 1563 | ` * raised at the store site here with the same message and the same` |
|        - | 1564 | ` * non-catchable outcome. Returns PH7_ABORT (dispatched via` |
|        - | 1565 | ` * PH7_DISPATCH_ENFORCE_RC at the store sites).` |
|        - | 1566 | ` */` |
|        2 | 1567 | `PH7_PRIVATE sxi32 PH7_VmThrowGlobalsAppendError(ph7_vm *pVm)` |
|        1 | 1568 | `{` |
|        3 | 1569 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot append to $GLOBALS");` |
|        3 | 1570 | `	pVm->iExitStatus = 255;` |
|        3 | 1571 | `	pVm->bHaltRequested = 1;` |
|        3 | 1572 | `	return PH7_ABORT;` |
|        1 | 1573 | `}` |
|        - | 1574 | `/*` |
|        - | 1575 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - | 1576 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - | 1577 | ` * possible.` |
|        - | 1578 | ` */` |
|   100188 | 1579 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven,` |
|        - | 1580 | `	int bViaRef)` |
|        5 | 1581 | `{` |
|   100193 | 1582 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|   100193 | 1583 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        - | 1584 | `	char zType[192];` |
|   150287 | 1585 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|    50094 | 1586 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|        - | 1587 | `	/* php words a write that arrived through a REFERENCE differently: the slot` |
|        - | 1588 | `	 * is the property's, but the assignment names no property, so the sentence` |
|        - | 1589 | `	 * says which property is HOLDING the reference. */` |
|   100193 | 1590 | `	const char *zWhat = bViaRef ? "reference held by property" : "property";` |
|        - | 1591 | `	SyBlob sMsg;` |
|   100193 | 1592 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 1593 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - | 1594 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|   100193 | 1595 | `	if( pOwner ){` |
|   100193 | 1596 | `		SyBlobFormat(&sMsg,"Cannot assign %s to %s %z::$%z of type %s",` |
|    50094 | 1597 | `			zGiven,zWhat,&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|    50099 | 1598 | `	}else{` |
|      ! 0 | 1599 | `		SyBlobFormat(&sMsg,"Cannot assign %s to %s $%z of type %s",` |
|      ! 0 | 1600 | `			zGiven,zWhat,&pAttr->sName,zTypeText);` |
|        - | 1601 | `	}` |
|   100193 | 1602 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        5 | 1603 | `}` |
|        - | 1604 | `/*` |
|        - | 1605 | ` * Of the pseudo-types a property may be declared with, the two whose mask holds` |
|        - | 1606 | `` * an array. `object` does not, and neither does any real class or interface --`` |
|        - | 1607 | `` * php looks at the type MASK, so `ArrayAccess` and `Traversable` are refused`` |
|        - | 1608 | `` * exactly like `int`.`` |
|        - | 1609 | ` */` |
|        2 | 1610 | `static int VmPseudoTypeAcceptsArray(const SyString *pClass)` |
|        1 | 1611 | `{` |
|        3 | 1612 | `	return (pClass->nByte == 8 && SyStrnicmp(pClass->zString,"iterable",8) == 0)` |
|        3 | 1613 | `	    \|\| (pClass->nByte == 5 && SyStrnicmp(pClass->zString,"mixed",5) == 0);` |
|        1 | 1614 | `}` |
|        - | 1615 | `/*` |
|        - | 1616 | ` * TRUE when a property's declared type admits an ARRAY, which is what decides` |
|        - | 1617 | ` * whether a dimension write to it may AUTO-INITIALIZE one. php asks the type's` |
|        - | 1618 | `` * mask (`MAY_BE_ARRAY`), so `array`, `?array`, `iterable`, `mixed` and any union`` |
|        - | 1619 | ``  * with an array alternative say yes and every class type says no -- `ArrayAccess` `` |
|        - | 1620 | `` * and `Traversable` included, which is the part a "does it behave like an array"`` |
|        - | 1621 | ` * reading would get wrong.` |
|        - | 1622 | ` */` |
|       12 | 1623 | `static int VmAttrTypeAcceptsArray(ph7_class_attr *pAttr)` |
|        2 | 1624 | `{` |
|       14 | 1625 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1626 | `		return 1; /* untyped: a dimension write vivifies as it always has */` |
|        - | 1627 | `	}` |
|       14 | 1628 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      ! 0 | 1629 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pAttr->aUnionAlts);` |
|        - | 1630 | `		sxu32 i;` |
|      ! 0 | 1631 | `		for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; ++i ){` |
|      ! 0 | 1632 | `			if( aAlt[i].nType == MEMOBJ_HASHMAP ){` |
|      ! 0 | 1633 | `				return 1;` |
|        - | 1634 | `			}` |
|      ! 0 | 1635 | `			if( aAlt[i].nType == SXU32_HIGH && VmPseudoTypeAcceptsArray(&aAlt[i].sClass) ){` |
|      ! 0 | 1636 | `				return 1;` |
|        - | 1637 | `			}` |
|      ! 0 | 1638 | `		}` |
|      ! 0 | 1639 | `		return 0;` |
|        - | 1640 | `	}` |
|       14 | 1641 | `	if( pAttr->nType == MEMOBJ_HASHMAP ){` |
|        9 | 1642 | `		return 1;` |
|        - | 1643 | `	}` |
|        6 | 1644 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        3 | 1645 | `		return VmPseudoTypeAcceptsArray(&pAttr->sClass);` |
|        - | 1646 | `	}` |
|        3 | 1647 | `	return 0;` |
|        8 | 1648 | `}` |
|        - | 1649 | `/*` |
|        - | 1650 | ` * Throw php's TypeError for a dimension write that would have to auto-initialize` |
|        - | 1651 | ` * an array inside a property whose declared type has no room for one.` |
|        - | 1652 | ` */` |
|        2 | 1653 | `PH7_PRIVATE sxi32 VmThrowAutoInitArrayError(ph7_vm *pVm,VmClassAttr *pVmAttr)` |
|        1 | 1654 | `{` |
|        3 | 1655 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        3 | 1656 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        - | 1657 | `	char zType[256];` |
|        4 | 1658 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|        1 | 1659 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|        - | 1660 | `	SyBlob sMsg;` |
|        3 | 1661 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        3 | 1662 | `	if( pOwner ){` |
|        3 | 1663 | `		SyBlobFormat(&sMsg,"Cannot auto-initialize an array inside property %z::$%z of type %s",` |
|        1 | 1664 | `			&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|        2 | 1665 | `	}else{` |
|      ! 0 | 1666 | `		SyBlobFormat(&sMsg,"Cannot auto-initialize an array inside property $%z of type %s",` |
|      ! 0 | 1667 | `			&pAttr->sName,zTypeText);` |
|        - | 1668 | `	}` |
|        3 | 1669 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        1 | 1670 | `}` |
|        - | 1671 | `/*` |
|        - | 1672 | ` * Decide what a DIMENSION write to an uninitialized typed property does, which` |
|        - | 1673 | `` * is php's `zend_handle_fetch_obj_flags`: auto-initialize an empty array when the`` |
|        - | 1674 | ` * declared type admits one, and refuse otherwise. Returns SXRET_OK with the slot` |
|        - | 1675 | ` * left holding a fresh empty array, or the thrown TypeError.` |
|        - | 1676 | ` */` |
|       12 | 1677 | `PH7_PRIVATE sxi32 VmAutoInitArrayProperty(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pSlot)` |
|        2 | 1678 | `{` |
|       14 | 1679 | `	if( pVmAttr->pAttr == 0 \|\| pSlot == 0 ){` |
|      ! 0 | 1680 | `		return SXRET_OK;` |
|        - | 1681 | `	}` |
|       14 | 1682 | `	if( !VmAttrTypeAcceptsArray(pVmAttr->pAttr) ){` |
|        3 | 1683 | `		return VmThrowAutoInitArrayError(pVm,pVmAttr);` |
|        - | 1684 | `	}` |
|       11 | 1685 | `	PH7_MemObjRelease(pSlot);` |
|       11 | 1686 | `	PH7_MemObjToHashmap(pSlot);` |
|       11 | 1687 | `	pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       11 | 1688 | `	return SXRET_OK;` |
|        8 | 1689 | `}` |
|        - | 1690 | `/*` |
|        - | 1691 | ` * TRUE when a property's declared type admits NULL, which is what decides whether a` |
|        - | 1692 | `` * REFERENCE fetch of an uninitialized one may bind at all. `?T` and a `T\|null` union`` |
|        - | 1693 | `` * both carry the nullable flag; `mixed` is the one type that admits null without it.`` |
|        - | 1694 | ` */` |
|       30 | 1695 | `static int VmAttrTypeAcceptsNull(ph7_class_attr *pAttr)` |
|        1 | 1696 | `{` |
|       31 | 1697 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1698 | `		return 1; /* untyped: the slot already holds NULL */` |
|        - | 1699 | `	}` |
|       31 | 1700 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|        7 | 1701 | `		return 1;` |
|        - | 1702 | `	}` |
|       24 | 1703 | `	if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        4 | 1704 | `	 && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|        3 | 1705 | `		return 1;` |
|        - | 1706 | `	}` |
|       23 | 1707 | `	return 0;` |
|       16 | 1708 | `}` |
|        - | 1709 | `/*` |
|        - | 1710 | ` * Decide what a REFERENCE fetch of an uninitialized typed property does, which is` |
|        - | 1711 | `` * php's `zend_handle_fetch_obj_flags` under BP_VAR_W: the slot becomes NULL and the`` |
|        - | 1712 | ` * bind reaches it when the declared type admits null, and the fetch is refused` |
|        - | 1713 | `` * otherwise -- php's `Cannot access uninitialized non-nullable property C::$p by`` |
|        - | 1714 | `` * reference`. It is NOT the auto-initialize-array rule: that one belongs to a`` |
|        - | 1715 | `` * DIMENSION write, and running it here made `?int $t` an array and `int $t` the`` |
|        - | 1716 | ` * wrong TypeError.` |
|        - | 1717 | ` */` |
|       30 | 1718 | `PH7_PRIVATE sxi32 VmRefUninitTypedProperty(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr,ph7_value *pSlot)` |
|        1 | 1719 | `{` |
|        - | 1720 | `	ph7_class_attr *pAttr;` |
|        - | 1721 | `	ph7_class *pOwner;` |
|        - | 1722 | `	SyBlob sMsg;` |
|       31 | 1723 | `	if( pVmAttr == 0 \|\| pVmAttr->pAttr == 0 ){` |
|      ! 0 | 1724 | `		return SXRET_OK;` |
|        - | 1725 | `	}` |
|       31 | 1726 | `	pAttr = pVmAttr->pAttr;` |
|       31 | 1727 | `	if( VmAttrTypeAcceptsNull(pAttr) ){` |
|        9 | 1728 | `		if( pSlot ){` |
|        9 | 1729 | `			PH7_MemObjRelease(pSlot);` |
|        9 | 1730 | `			MemObjSetType(pSlot,MEMOBJ_NULL);` |
|        4 | 1731 | `		}` |
|        9 | 1732 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        9 | 1733 | `		return SXRET_OK;` |
|        - | 1734 | `	}` |
|       23 | 1735 | `	pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|       23 | 1736 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       23 | 1737 | `	SyBlobFormat(&sMsg,"Cannot access uninitialized non-nullable property %z::$%z by reference",` |
|       11 | 1738 | `		pOwner ? &pOwner->sDisp : &pClass->sDisp,&pAttr->sName);` |
|       23 | 1739 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       16 | 1740 | `}` |
|        - | 1741 | `/*` |
|        - | 1742 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - | 1743 | ` */` |
|   100028 | 1744 | `PH7_PRIVATE sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 1745 | `{` |
|   100033 | 1746 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|   100033 | 1747 | `	const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        - | 1748 | `	SyBlob sMsg;` |
|   100033 | 1749 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   100033 | 1750 | `	SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|    50014 | 1751 | `		zKind,&pOwner->sDisp,&pAttr->sName);` |
|   100033 | 1752 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 1753 | `}` |
|        - | 1754 | `/*` |
|        - | 1755 | ` * Throw the PHP-compatible Error raised on an illegal write to a readonly` |
|        - | 1756 | ` * property (PHP 8.1). bModify TRUE → a write to an already-initialized property` |
|        - | 1757 | ` * ("Cannot modify readonly property C::$x"); bModify FALSE → a first write from` |
|        - | 1758 | ` * a scope that cannot satisfy the readonly set-scope ("Cannot modify` |
|        - | 1759 | ` * protected(set) readonly property C::$x from {global scope\|scope X}").` |
|        - | 1760 | ` */` |
|        - | 1761 | `/*` |
|        - | 1762 | ` * Throw the PHP 8.4 Error raised on a write to an asymmetric-visibility` |
|        - | 1763 | ` * property from a scope its set-visibility excludes:` |
|        - | 1764 | ` * "Cannot modify private(set) property C::$x from {global scope\|scope X}".` |
|        - | 1765 | ` */` |
|       18 | 1766 | `static sxi32 VmThrowSetVisibilityErrorEx(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,` |
|        - | 1767 | `	int bIndirect)` |
|        2 | 1768 | `{` |
|       20 | 1769 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|       20 | 1770 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|       20 | 1771 | `	const char *zVis = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? "private(set)" : "protected(set)";` |
|        - | 1772 | `	/* php words a write that only reaches the property THROUGH something it holds` |
|        - | 1773 | ``	 * -- `$o->arr['k'] = v`, a by-reference bind -- as an INDIRECT modification,`` |
|        - | 1774 | `	 * the same distinction its readonly sentence makes. */` |
|       20 | 1775 | `	const char *zVerb = bIndirect ? "indirectly modify" : "modify";` |
|        - | 1776 | `	SyBlob sMsg;` |
|       20 | 1777 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       20 | 1778 | `	if( pActive ){` |
|        3 | 1779 | `		SyBlobFormat(&sMsg,"Cannot %s %s property %z::$%z from scope %z",` |
|        1 | 1780 | `			zVerb,zVis,&pOwner->sDisp,&pAttr->sName,&pActive->sDisp);` |
|        2 | 1781 | `	}else{` |
|       18 | 1782 | `		SyBlobFormat(&sMsg,"Cannot %s %s property %z::$%z from global scope",` |
|        8 | 1783 | `			zVerb,zVis,&pOwner->sDisp,&pAttr->sName);` |
|        - | 1784 | `	}` |
|       20 | 1785 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        2 | 1786 | `}` |
|       16 | 1787 | `static sxi32 VmThrowSetVisibilityError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 | 1788 | `{` |
|       17 | 1789 | `	return VmThrowSetVisibilityErrorEx(pVm,pClass,pAttr,0);` |
|        1 | 1790 | `}` |
|        - | 1791 | `/*` |
|        - | 1792 | ` * Check the PHP 8.4 asymmetric set-visibility of a property write against the` |
|        - | 1793 | ` * active class scope. private(set): only the DECLARING class scope may write` |
|        - | 1794 | ` * (subclasses excluded); protected(set): the declaring class or a subclass.` |
|        - | 1795 | ` * Returns SXRET_OK when allowed, else the throw status.` |
|        - | 1796 | ` */` |
|       38 | 1797 | `PH7_PRIVATE sxi32 VmCheckSetVisibility(ph7_vm *pVm,ph7_class *pOwner,ph7_class_attr *pAttr)` |
|        1 | 1798 | `{` |
|       39 | 1799 | `	ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pOwner);` |
|       39 | 1800 | `	ph7_class *pActive = VmCurrentSelf(pVm);` |
|        - | 1801 | `	int bOk;` |
|       39 | 1802 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|       33 | 1803 | `		bOk = (pActive != 0 && pActive == pDecl);` |
|       17 | 1804 | `	}else{` |
|        7 | 1805 | `		bOk = (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|        - | 1806 | `	}` |
|       39 | 1807 | `	if( !bOk ){` |
|       17 | 1808 | `		return VmThrowSetVisibilityError(pVm,pOwner,pAttr);` |
|        - | 1809 | `	}` |
|       23 | 1810 | `	return SXRET_OK;` |
|       20 | 1811 | `}` |
|        - | 1812 | `/*` |
|        - | 1813 | ` * php's write refusal for a native property whose handler takes NO write` |
|        - | 1814 | ` * (PH7_CLASS_ATTR_NATIVE_NOWRITE). The sentence is the readonly one -- php's` |
|        - | 1815 | ` * date_period_write_property says exactly that -- but the property carries no` |
|        - | 1816 | ` * readonly FLAG, so this is spelled apart from VmThrowReadonlyError rather than` |
|        - | 1817 | ` * reached through it: Reflection reports isReadOnly() false for DatePeriod's` |
|        - | 1818 | ` * seven in both engines, and the readonly rules (write-once, set-scope, the` |
|        - | 1819 | ` * __clone re-initialization window) do not apply to a handler that never` |
|        - | 1820 | ` * accepts one.` |
|        - | 1821 | ` */` |
|       48 | 1822 | `PH7_PRIVATE sxi32 VmThrowNativeNoWrite(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        4 | 1823 | `{` |
|       52 | 1824 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 1825 | `	SyBlob sMsg;` |
|       52 | 1826 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       52 | 1827 | `	SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sDisp,&pAttr->sName);` |
|       52 | 1828 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        4 | 1829 | `}` |
|        - | 1830 | `/*` |
|        - | 1831 | `` * And its unset half, which php words differently: `Cannot unset C::$p`, with`` |
|        - | 1832 | ` * neither "readonly" nor "property" in it.` |
|        - | 1833 | ` */` |
|       30 | 1834 | `PH7_PRIVATE sxi32 VmThrowNativeNoUnset(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        3 | 1835 | `{` |
|        - | 1836 | `	SyBlob sMsg;` |
|       33 | 1837 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 1838 | `	/* The OBJECT's class, not the declaring one -- php's two refusals disagree` |
|        - | 1839 | `	 * about which to print, and a subclass of DatePeriod shows it: the write says` |
|        - | 1840 | ``	 * `DatePeriod::$interval` and the unset says `SubDp::$interval`. */`` |
|       33 | 1841 | `	SyBlobFormat(&sMsg,"Cannot unset %z::$%z",&pClass->sDisp,&pAttr->sName);` |
|       33 | 1842 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 1843 | `}` |
|        - | 1844 | `/*` |
|        - | 1845 | ` * And php's THIRD refusal for a property its handler will not have written:` |
|        - | 1846 | `` * `Property p is read only`, which names neither the class nor the `$`.`` |
|        - | 1847 | `` * PDOStatement's `queryString` is php's case, and the shapes it applies to are`` |
|        - | 1848 | ` * the plain store and the unset alone -- see PH7_CLASS_ATTR_NATIVE_RDONLY.` |
|        - | 1849 | ` */` |
|       10 | 1850 | `PH7_PRIVATE sxi32 VmThrowNativeReadOnly(ph7_vm *pVm,ph7_class_attr *pAttr)` |
|        1 | 1851 | `{` |
|        - | 1852 | `	SyBlob sMsg;` |
|       11 | 1853 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       11 | 1854 | `	SyBlobFormat(&sMsg,"Property %z is read only",&pAttr->sName);` |
|       11 | 1855 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        1 | 1856 | `}` |
|        - | 1857 | `/*` |
|        - | 1858 | `` * php's answer to `unset($o->p)` where p is READONLY. Two of the three cases`` |
|        - | 1859 | ` * refuse, and the sentences are not the write ones:` |
|        - | 1860 | ` *` |
|        - | 1861 | ` *   * an INITIALIZED one refuses from every scope, its own included --` |
|        - | 1862 | `` *     `Cannot unset readonly property C::$p`. Destroying it would re-arm the`` |
|        - | 1863 | ` *     write-once latch, which is exactly what readonly exists to prevent;` |
|        - | 1864 | ` *` |
|        - | 1865 | ` *   * an UNINITIALIZED one is a WRITE-shaped act, so it takes the set-visibility` |
|        - | 1866 | ` *     rules: allowed from the declaring class or a subclass (php lets a lazy` |
|        - | 1867 | ` *     proxy re-arm one that way), and otherwise the asymmetric-visibility` |
|        - | 1868 | `` *     refusal. php words that one two ways -- an EXPLICIT `private(set)` gets the`` |
|        - | 1869 | ` *     ordinary asymmetric sentence with no "readonly" in it, and everything else` |
|        - | 1870 | `` *     gets readonly's own implicit `protected(set) readonly`.`` |
|        - | 1871 | ` *` |
|        - | 1872 | ` * Answers SXRET_OK when the unset may proceed, else the throw status.` |
|        - | 1873 | ` */` |
|       24 | 1874 | `PH7_PRIVATE sxi32 VmCheckReadonlyUnset(ph7_vm *pVm,ph7_class *pClass,VmClassAttr *pVmAttr)` |
|        1 | 1875 | `{` |
|       25 | 1876 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - | 1877 | `	ph7_class *pOwner;` |
|        - | 1878 | `	ph7_class *pActive;` |
|        - | 1879 | `	SyBlob sMsg;` |
|        - | 1880 | `	int bInit,bScope;` |
|       25 | 1881 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) == 0 ){` |
|      ! 0 | 1882 | `		return SXRET_OK;` |
|        - | 1883 | `	}` |
|       25 | 1884 | `	pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|       25 | 1885 | `	pActive = VmCurrentSelf(pVm);` |
|       25 | 1886 | `	bInit = (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0;` |
|       25 | 1887 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        7 | 1888 | `		bScope = (pActive != 0 && pActive == pOwner);` |
|        4 | 1889 | `	}else{` |
|       19 | 1890 | `		bScope = (pActive != 0 && pOwner != 0 && PH7_VmInstanceOf(pActive,pOwner));` |
|        - | 1891 | `	}` |
|       25 | 1892 | `	if( !bInit && bScope ){` |
|        9 | 1893 | `		return SXRET_OK;` |
|        - | 1894 | `	}` |
|       17 | 1895 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 | 1896 | `	if( bInit ){` |
|        9 | 1897 | `		SyBlobFormat(&sMsg,"Cannot unset readonly property %z::$%z",&pOwner->sDisp,&pAttr->sName);` |
|        5 | 1898 | `	}else{` |
|       13 | 1899 | `		const char *zWhat = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET)` |
|        4 | 1900 | `			? "private(set)" : "protected(set) readonly";` |
|        9 | 1901 | `		if( pActive ){` |
|        3 | 1902 | `			SyBlobFormat(&sMsg,"Cannot unset %s property %z::$%z from scope %z",` |
|        1 | 1903 | `				zWhat,&pOwner->sDisp,&pAttr->sName,&pActive->sDisp);` |
|        2 | 1904 | `		}else{` |
|        7 | 1905 | `			SyBlobFormat(&sMsg,"Cannot unset %s property %z::$%z from global scope",` |
|        3 | 1906 | `				zWhat,&pOwner->sDisp,&pAttr->sName);` |
|        - | 1907 | `		}` |
|        - | 1908 | `	}` |
|       17 | 1909 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       13 | 1910 | `}` |
|       54 | 1911 | `static sxi32 VmThrowReadonlyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,int bModify)` |
|        5 | 1912 | `{` |
|       59 | 1913 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 1914 | `	SyBlob sMsg;` |
|       59 | 1915 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       59 | 1916 | `	if( bModify ){` |
|       55 | 1917 | `		SyBlobFormat(&sMsg,"Cannot modify readonly property %z::$%z",&pOwner->sDisp,&pAttr->sName);` |
|       30 | 1918 | `	}else{` |
|        6 | 1919 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        6 | 1920 | `		if( pActive ){` |
|      ! 0 | 1921 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from scope %z",` |
|      ! 0 | 1922 | `				&pOwner->sDisp,&pAttr->sName,&pActive->sDisp);` |
|      ! 0 | 1923 | `		}else{` |
|        6 | 1924 | `			SyBlobFormat(&sMsg,"Cannot modify protected(set) readonly property %z::$%z from global scope",` |
|        2 | 1925 | `				&pOwner->sDisp,&pAttr->sName);` |
|        - | 1926 | `		}` |
|        - | 1927 | `	}` |
|       59 | 1928 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 1929 | `}` |
|        - | 1930 | `/*` |
|        - | 1931 | ` * INDIRECT modification: this SLOT is about to be reached as something other than` |
|        - | 1932 | `` * a plain store -- aliased by `=&`, handed to a by-reference parameter, walked by`` |
|        - | 1933 | ` * a by-reference foreach, or used as the BASE of a subscript write. php screens` |
|        - | 1934 | ` * every one of them where it screens a store, because the alias outlives the` |
|        - | 1935 | ` * statement and the next write through it would reach the property with no` |
|        - | 1936 | ` * handler and no readonly latch in the way.` |
|        - | 1937 | ` *` |
|        - | 1938 | `` * Two sentences. A php-readonly property gets its own: `Cannot indirectly modify`` |
|        - | 1939 | `` * readonly property C::$p`, raised whatever the scope and whether or not the`` |
|        - | 1940 | ` * property has been initialized -- the reference is refused before the` |
|        - | 1941 | ` * uninitialized read is. A NATIVE class whose handler refuses every write` |
|        - | 1942 | ` * (PH7_CLASS_ATTR_NATIVE_NOWRITE) gets that handler's own sentence, the one a` |
|        - | 1943 | ` * plain store to it gets.` |
|        - | 1944 | ` *` |
|        - | 1945 | ` * Answers SXRET_OK to proceed, or the throw status. Asked by the sites that reach` |
|        - | 1946 | ` * a property through its memobj index rather than through its declaration.` |
|        - | 1947 | ` */` |
|    46618 | 1948 | `PH7_PRIVATE sxi32 PH7_VmCheckIndirectModify(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 1949 | `{` |
|        - | 1950 | `	SyHashEntry *pSlot;` |
|        - | 1951 | `	VmClassAttr *pVmAttr;` |
|        - | 1952 | `	ph7_class_attr *pAttr;` |
|    46623 | 1953 | `	if( nIdx == SXU32_HIGH \|\| !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|    46499 | 1954 | `		return SXRET_OK;` |
|        - | 1955 | `	}` |
|    40309 | 1956 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    40309 | 1957 | `	if( pSlot == 0 ){` |
|      ! 0 | 1958 | `		return SXRET_OK;` |
|        - | 1959 | `	}` |
|      127 | 1960 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      127 | 1961 | `	pAttr = pVmAttr->pAttr;` |
|      127 | 1962 | `	if( pAttr == 0 ){` |
|      ! 0 | 1963 | `		return SXRET_OK;` |
|        - | 1964 | `	}` |
|      127 | 1965 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|       13 | 1966 | `		return VmThrowNativeNoWrite(pVm,PH7_VmAttrOwner(pVmAttr),pAttr);` |
|        - | 1967 | `	}` |
|      115 | 1968 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       33 | 1969 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        - | 1970 | `		SyBlob sMsg;` |
|       33 | 1971 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       33 | 1972 | `		SyBlobFormat(&sMsg,"Cannot indirectly modify readonly property %z::$%z",` |
|       16 | 1973 | `			&pOwner->sDisp,&pAttr->sName);` |
|       33 | 1974 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 1975 | `	}` |
|        - | 1976 | `	/* An asymmetric set-visibility (PHP 8.4) gates the indirect write exactly as` |
|        - | 1977 | ``	 * readonly does, and from the same scopes: `$o->arr['k'] = v` on a`` |
|        - | 1978 | ``	 * `public private(set) array $arr` is refused outside the declaring class.`` |
|        - | 1979 | `	 * Only readonly was screened here, so that write landed in SILENCE. */` |
|       83 | 1980 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        3 | 1981 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        3 | 1982 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        5 | 1983 | `		int bOk = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET)` |
|        2 | 1984 | `			? (pActive != 0 && pActive == pDecl)` |
|        2 | 1985 | `			: (pActive != 0 && pDecl != 0 && PH7_VmInstanceOf(pActive,pDecl));` |
|        3 | 1986 | `		if( !bOk ){` |
|        3 | 1987 | `			return VmThrowSetVisibilityErrorEx(pVm,PH7_VmAttrOwner(pVmAttr),pAttr,1);` |
|        - | 1988 | `		}` |
|      ! 0 | 1989 | `	}` |
|       81 | 1990 | `	return SXRET_OK;` |
|    23306 | 1991 | `}` |
|        - | 1992 | `/*` |
|        - | 1993 | `` * Reject an in-place mutation (`++`/`--`) of a readonly property. The increment`` |
|        - | 1994 | ` * and decrement opcodes mutate the per-instance slot directly, bypassing` |
|        - | 1995 | ` * VmEnforcePropertyTypeOnStore, so they consult the typed-slot table here. A` |
|        - | 1996 | `` * readonly property reached by `++`/`--` is necessarily already initialized (an`` |
|        - | 1997 | ` * uninitialized read is rejected earlier at OP_MEMBER), so the mutation is always` |
|        - | 1998 | ` * the "Cannot modify readonly property" case. Returns SXRET_OK to proceed, or the` |
|        - | 1999 | ` * PH7_EXCEPTION/PH7_ABORT produced by the throw.` |
|        - | 2000 | ` */` |
| 12862912 | 2001 | `PH7_PRIVATE sxi32 VmCheckReadonlyMutate(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 2002 | `{` |
|        - | 2003 | `	SyHashEntry *pSlot;` |
|        - | 2004 | `	VmClassAttr *pVmAttr;` |
| 12862917 | 2005 | `	if( nIdx == SXU32_HIGH \|\| !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
| 12860589 | 2006 | `		return SXRET_OK; /* Non-lvalue operand, or no typed/readonly properties — skip */` |
|        - | 2007 | `	}` |
| 12090250 | 2008 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
| 12090250 | 2009 | `	if( pSlot == 0 ){` |
|      ! 0 | 2010 | `		return SXRET_OK; /* Not a typed slot */` |
|        - | 2011 | `	}` |
|     2332 | 2012 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     2332 | 2013 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE) ){` |
|        5 | 2014 | `		return VmThrowNativeNoWrite(pVm,PH7_VmAttrOwner(pVmAttr),pVmAttr->pAttr);` |
|        - | 2015 | `	}` |
|     2328 | 2016 | `	if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|       12 | 2017 | `		return VmThrowReadonlyError(pVm,PH7_VmAttrOwner(pVmAttr),pVmAttr->pAttr,1);` |
|        - | 2018 | `	}` |
|     2314 | 2019 | `	if( pVmAttr->pAttr` |
|     2318 | 2020 | `	 && (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET)) ){` |
|        - | 2021 | ``		/* `++`/`--` is a write: enforce the asymmetric set-visibility (PHP 8.4) */`` |
|        9 | 2022 | `		return VmCheckSetVisibility(pVm,PH7_VmAttrOwner(pVmAttr),pVmAttr->pAttr);` |
|        - | 2023 | `	}` |
|     2310 | 2024 | `	return SXRET_OK;` |
|  6432540 | 2025 | `}` |
|        - | 2026 | `/*` |
|        - | 2027 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - | 2028 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - | 2029 | ` * For class types, instanceof is verified.` |
|        - | 2030 | ` *` |
|        - | 2031 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - | 2032 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - | 2033 | ` */` |
|        - | 2034 |  |
|        - | 2035 | `/*` |
|        - | 2036 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - | 2037 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - | 2038 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - | 2039 | ` *   0 if it's not strictly numeric.` |
|        - | 2040 | ` */` |
|       40 | 2041 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 | 2042 | `{` |
|        - | 2043 | `	const char *z, *zEnd, *zTail;` |
|        - | 2044 | `	sxu32 n;` |
|       42 | 2045 | `	sxu8 bReal = 0;` |
|        - | 2046 | `	sxi32 rc;` |
|       42 | 2047 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       23 | 2048 | `		return 0;` |
|        - | 2049 | `	}` |
|       20 | 2050 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       20 | 2051 | `	n = SyBlobLength(&pValue->sBlob);` |
|       20 | 2052 | `	zEnd = z + n;` |
|       20 | 2053 | `	if( n == 0 ) return 0;` |
|       20 | 2054 | `	zTail = 0;` |
|       20 | 2055 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       20 | 2056 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 | 2057 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 | 2058 | `	if( zTail != zEnd ) return 0;` |
|       15 | 2059 | `	return bReal ? 2 : 1;` |
|       22 | 2060 | `}` |
|        - | 2061 |  |
|        - | 2062 | `/*` |
|        - | 2063 | ` * Check a value against a "pseudo-type" stored as an SXU32_HIGH class-name atom.` |
|        - | 2064 | `` * PH7 parses `true`/`false`/`iterable`/`callable`/`mixed` as class-name atoms`` |
|        - | 2065 | ` * (they are not scalar keywords), so without this every enforcement site —` |
|        - | 2066 | ` * return, parameter, property, union alternative — would have to string-match` |
|        - | 2067 | ` * the name itself.` |
|        - | 2068 | ` * Centralising it here keeps the four sites consistent and is the single place` |
|        - | 2069 | ` * to extend when another literal/pseudo type is added.` |
|        - | 2070 | ` *   returns  1 : recognised pseudo-type AND the value satisfies it` |
|        - | 2071 | ` *            0 : recognised pseudo-type AND the value does NOT satisfy it` |
|        - | 2072 | ` *           -1 : not a pseudo-type (caller should treat sClass as a real class)` |
|        - | 2073 | ` */` |
|     8774 | 2074 | `PH7_PRIVATE int VmCheckPseudoType(ph7_vm *pVm, ph7_value *pValue, const SyString *pClass)` |
|        5 | 2075 | `{` |
|     8779 | 2076 | `	const char *z = pClass->zString;` |
|     8779 | 2077 | `	sxu32 n = pClass->nByte;` |
|     8779 | 2078 | `	if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|      352 | 2079 | ``		return 1; /* `mixed` accepts any value, including null */`` |
|        - | 2080 | `	}` |
|     8431 | 2081 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       31 | 2082 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal != 0 ) ? 1 : 0;` |
|        - | 2083 | `	}` |
|     8403 | 2084 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|      105 | 2085 | `		return ( (pValue->iFlags & MEMOBJ_BOOL) && pValue->x.iVal == 0 ) ? 1 : 0;` |
|        - | 2086 | `	}` |
|     8303 | 2087 | `	if( n == 8 && SyStrnicmp(z,"callable",8) == 0 ){` |
|        - | 2088 | ``		/* php's `callable` type is is_callable() itself — a function-name string,`` |
|        - | 2089 | `		 * a "C::m" string, a [target,method] pair, a Closure, or an __invoke` |
|        - | 2090 | `		 * object; scope-sensitive, so a private method is callable only from` |
|        - | 2091 | `		 * inside. Same predicate the builtin answers with, so the type and` |
|        - | 2092 | `		 * is_callable() can never disagree. (Parameters and return values only:` |
|        - | 2093 | ``		 * the compiler rejects `callable` on a property or class constant, as`` |
|        - | 2094 | `		 * php does.) A user declaration checks it the way is_callable() does, so it` |
|        - | 2095 | `		 * raises the scope deprecation too. */` |
|     6021 | 2096 | `		if( pVm->bReturnTypeFence ){` |
|        - | 2097 | `			/* A RETURN value: php makes the exception an error handler throws on the` |
|        - | 2098 | `			 * deprecation the previous of its return TypeError, so the handler runs` |
|        - | 2099 | `			 * behind the fence and the value counts as not callable. (Its own calls` |
|        - | 2100 | `			 * are not a return check: the flag is down while it runs.) */` |
|        - | 2101 | `			ph7_class_instance *pExc;` |
|       41 | 2102 | `			pVm->bReturnTypeFence = 0;` |
|       41 | 2103 | `			PH7_VmCallableDeprecationFenced(pVm,pValue,&pExc);` |
|       41 | 2104 | `			pVm->bReturnTypeFence = 1;` |
|       41 | 2105 | `			if( pExc ){` |
|        9 | 2106 | `				if( pVm->pReturnTypeExc ){` |
|      ! 0 | 2107 | `					PH7_ClassInstanceUnref(pVm->pReturnTypeExc);` |
|      ! 0 | 2108 | `				}` |
|        9 | 2109 | `				pVm->pReturnTypeExc = pExc;` |
|        9 | 2110 | `				return 0;` |
|        - | 2111 | `			}` |
|       18 | 2112 | `		}else{` |
|     5983 | 2113 | `			PH7_VmCallableDeprecation(pVm,pValue);` |
|        - | 2114 | `		}` |
|     6013 | 2115 | `		return PH7_VmIsCallable(pVm,pValue,TRUE) ? 1 : 0;` |
|        - | 2116 | `	}` |
|     2287 | 2117 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        - | 2118 | `		/* iterable === array \| Traversable */` |
|      103 | 2119 | `		if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       30 | 2120 | `			return 1;` |
|        - | 2121 | `		}` |
|       76 | 2122 | `		if( (pValue->iFlags & MEMOBJ_OBJ) && pVm->pTraversableClass ){` |
|       45 | 2123 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       45 | 2124 | `			if( PH7_VmInstanceOf(pInst->pClass,pVm->pTraversableClass) ){` |
|       29 | 2125 | `				return 1;` |
|        - | 2126 | `			}` |
|        7 | 2127 | `		}` |
|       48 | 2128 | `		return 0;` |
|        - | 2129 | `	}` |
|     2189 | 2130 | `	return -1;` |
|     4286 | 2131 | `}` |
|        - | 2132 | `/*` |
|        - | 2133 | `` * The scope a `self`/`parent` written in a TYPE HINT resolves against: the class`` |
|        - | 2134 | ` * the hint was DECLARED in, never the class the value happens to be reached` |
|        - | 2135 | ` * through. php binds the keyword where the hint is written, so` |
|        - | 2136 | `` * `class P { public function s(): self {…} }` still expects a P when called on a`` |
|        - | 2137 | `` * `Q extends P` — resolving against the runtime (late-static-binding) class made`` |
|        - | 2138 | `` * such a return, and a `self`-typed property written from an inherited method,`` |
|        - | 2139 | ` * throw a TypeError over perfectly valid code.` |
|        - | 2140 | ` *` |
|        - | 2141 | ` * pDecl is the declaring class (0 when the site cannot name one); pUsing is the` |
|        - | 2142 | ` * composing/runtime class to stand in for a TRAIT — php flattens a trait into` |
|        - | 2143 | ` * the using class, and the trait itself sits in no instanceof hierarchy — or 0` |
|        - | 2144 | ` * to fall back to the active self. This is the same rule OP_CALL already applies` |
|        - | 2145 | ` * to PARAMETER hints when it computes pSelfHint (vm_exec.c), and the one` |
|        - | 2146 | `` * VmResolveTypeClass applies to `parent`.`` |
|        - | 2147 | ` */` |
|   122742 | 2148 | `PH7_PRIVATE ph7_class *VmHintScopeClass(ph7_vm *pVm, ph7_class *pDecl, ph7_class *pUsing)` |
|        5 | 2149 | `{` |
|   122747 | 2150 | `	if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|   107153 | 2151 | `		return pDecl;` |
|        - | 2152 | `	}` |
|    15599 | 2153 | `	return pUsing ? pUsing : VmCurrentSelf(pVm);` |
|    61310 | 2154 | `}` |
|        - | 2155 | `/*` |
|        - | 2156 | ` * The scope a member's declared type is PRINTED against, which is not the scope` |
|        - | 2157 | ` * it is CHECKED against.` |
|        - | 2158 | ` *` |
|        - | 2159 | `` * php resolves `self`/`parent` in a member's stored type TEXT at compile time,`` |
|        - | 2160 | ` * and a TRAIT has no class to resolve them to -- so what it stores for a trait` |
|        - | 2161 | `` * member keeps the keyword, and every display of that text says `self` however`` |
|        - | 2162 | `` * many classes composed the trait: `uninitialized(self)` in var_dump,`` |
|        - | 2163 | `` * `of type ?parent` in the assign TypeError, `self` from`` |
|        - | 2164 | ` * ReflectionProperty::getType(). The CHECK still resolves against the composing` |
|        - | 2165 | ` * class, which is what VmHintScopeClass answers; this is its display twin, and` |
|        - | 2166 | ` * telling the two apart is what the recorded reference-property row was waiting for.` |
|        - | 2167 | ` */` |
|   101498 | 2168 | `PH7_PRIVATE ph7_class *VmHintScopeDeclared(ph7_class *pDecl)` |
|        5 | 2169 | `{` |
|   101503 | 2170 | `	return ( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ) ? pDecl : 0;` |
|        5 | 2171 | `}` |
|        - | 2172 | `/*` |
|        - | 2173 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - | 2174 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - | 2175 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - | 2176 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - | 2177 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - | 2178 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - | 2179 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - | 2180 | ` * throw.` |
|        - | 2181 | ` *` |
|        - | 2182 | `` * The class match for object values resolves `self`/`parent` alternatives`` |
|        - | 2183 | ` * against *pSelf* — the DECLARING scope of the hint the union came from, which` |
|        - | 2184 | ` * every caller computes through VmHintScopeClass (the self-stack top is the` |
|        - | 2185 | ` * runtime class, which is the wrong answer for an inherited hint).` |
|        - | 2186 | ` */` |
|        - | 2187 | `/*` |
|        - | 2188 | ` * Resolve a class/interface name from a type declaration to its ph7_class*,` |
|        - | 2189 | `` * handling the `self`/`parent` aliases against the supplied scope class pSelf —`` |
|        - | 2190 | ` * the class the hint was DECLARED in, which every enforcement site computes` |
|        - | 2191 | ` * through VmHintScopeClass above (OP_CALL's pSelfHint for parameters) — and` |
|        - | 2192 | `` * `static`, which is php's late-static-binding CALLED class and so ignores pSelf.`` |
|        - | 2193 | ` * Used by every type-enforcement site so the resolution rule — including the` |
|        - | 2194 | ` * iLoadable flag — lives in one place.` |
|        - | 2195 | ` *` |
|        - | 2196 | ` * The three keyword names are matched case-INSENSITIVELY, like every other type` |
|        - | 2197 | ` * keyword (VmCheckPseudoType's mixed/true/false/iterable) and like php: a hint` |
|        - | 2198 | `` * spelled `SELF` used to miss the exact-match arm, fall through to the class`` |
|        - | 2199 | ` * table, find nothing, and leave the caller with 0 — which every caller reads as` |
|        - | 2200 | ` * "unresolvable, accept anything", so the hint enforced NOTHING.` |
|        - | 2201 | ` *` |
|        - | 2202 | ` * Always resolves with iLoadable=FALSE: every caller is an instanceof/type-` |
|        - | 2203 | ` * compatibility target, where the type may legitimately be an interface or` |
|        - | 2204 | ` * abstract class (TRUE would filter those out → the check is skipped → any` |
|        - | 2205 | ` * object wrongly accepted). Centralizing FALSE here keeps a future caller from` |
|        - | 2206 | `` * reintroducing that bug. (Instantiation/`new` uses PH7_VmExtractClass directly`` |
|        - | 2207 | ` * with TRUE; it does not go through this helper.)` |
|        - | 2208 | ` */` |
|     2294 | 2209 | `PH7_PRIVATE ph7_class *VmResolveTypeClass(ph7_vm *pVm, const SyString *pCN, ph7_class *pSelf)` |
|        5 | 2210 | `{` |
|     2299 | 2211 | `	if( pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0 ){` |
|      169 | 2212 | `		return pSelf;` |
|        - | 2213 | `	}` |
|     2135 | 2214 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0 ){` |
|        - | 2215 | ``		/* php 8.0 `static` return type: the LATE-STATIC-BINDING class, i.e. the`` |
|        - | 2216 | ``		 * class the call was made through — so `P::s(): static` returning a P is a`` |
|        - | 2217 | ``		 * TypeError once s() is reached on a `Q extends P`. It is deliberately NOT`` |
|        - | 2218 | `		 * pSelf (that is where the hint was written); php allows the keyword in a` |
|        - | 2219 | `		 * return position only, but resolving it here keeps every site consistent. */` |
|       49 | 2220 | `		return PH7_VmPeekTopClass(pVm);` |
|        - | 2221 | `	}` |
|     2091 | 2222 | `	if( pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0 ){` |
|        - | 2223 | `		/* A trait method's declaring class is the trait (shared by pointer); parent::` |
|        - | 2224 | `		 * resolves against the class that USED it, matching the self:: trait rule. */` |
|       39 | 2225 | `		if( pSelf && (pSelf->iFlags & PH7_CLASS_TRAIT) ){` |
|      ! 0 | 2226 | `			pSelf = PH7_VmTraitUsingClass(pVm,pSelf,PH7_VmPeekTopClass(pVm));` |
|      ! 0 | 2227 | `		}` |
|       39 | 2228 | `		return pSelf ? pSelf->pBase : 0;` |
|        - | 2229 | `	}` |
|     2057 | 2230 | `	return PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,FALSE,0);` |
|     1152 | 2231 | `}` |
|        - | 2232 | `/*` |
|        - | 2233 | ` * Is *pCN* one of the three SCOPE KEYWORDS rather than a class name? Those are` |
|        - | 2234 | `` * the only hints VmResolveTypeClass may legitimately answer 0 for — `self` with`` |
|        - | 2235 | `` * no active scope, `parent` with no base class — which php rejects at COMPILE`` |
|        - | 2236 | ` * time, so there is no runtime behaviour to be faithful to and the caller keeps` |
|        - | 2237 | ` * accepting. Any OTHER name that resolves to nothing is a class that does not` |
|        - | 2238 | ` * exist, and that is a mismatch (see VmClassHintMatches).` |
|        - | 2239 | `` * (vm_builtin_call.c carries the same list for CALLABLE strings — `'self::m'`,`` |
|        - | 2240 | `` * `['parent','m']` — where the rule is about dispatch, not types.)`` |
|        - | 2241 | ` */` |
|   102502 | 2242 | `static int VmHintIsScopeKeyword(const SyString *pCN)` |
|        5 | 2243 | `{` |
|   102541 | 2244 | `	return (pCN->nByte == 4 && SyStrnicmp(pCN->zString,"self",4) == 0)` |
|   102476 | 2245 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"parent",6) == 0)` |
|   153753 | 2246 | `		\|\| (pCN->nByte == 6 && SyStrnicmp(pCN->zString,"static",6) == 0);` |
|        5 | 2247 | `}` |
|        - | 2248 | `/*` |
|        - | 2249 | ` * Does *pVal* satisfy the single (non-union) CLASS hint *pName*, written in the` |
|        - | 2250 | ` * scope *pScope* (see VmHintScopeClass)? THE one implementation of the rule,` |
|        - | 2251 | ` * shared by the argument, variadic-element, return, property, class-constant and` |
|        - | 2252 | ` * typed-default checks — each of which then formats its own message. The` |
|        - | 2253 | ` * resolved class is handed back through *ppResolved for the message builder` |
|        - | 2254 | ` * (VmClassHintTypeName prints the resolved name, or the name as written when` |
|        - | 2255 | ` * nothing resolved).` |
|        - | 2256 | ` *` |
|        - | 2257 | ` * A name that resolves to NOTHING used to make every one of those sites skip its` |
|        - | 2258 | ` * check, so a hint naming a class that does not exist enforced nothing at all:` |
|        - | 2259 | `` * `function f(Missing $c){} f(new Oth);` ran the body where php throws. Nothing`` |
|        - | 2260 | ` * can be an instance of a class that does not exist, so that is a mismatch.` |
|        - | 2261 | ` * A scope KEYWORD is the exception, and only half of one: with no scope to` |
|        - | 2262 | ` * resolve against there is no class to compare to — a position php rejects at` |
|        - | 2263 | ` * compile time, so any object passes — but a class hint still demands an object.` |
|        - | 2264 | ` *` |
|        - | 2265 | ` * Null never reaches here: a nullable hint accepts it at every call site before` |
|        - | 2266 | ` * the class branch. The resolution AUTOLOADS (PH7_VmExtractClass), so a` |
|        - | 2267 | ` * not-yet-loaded class is loaded and genuinely checked; only a name no` |
|        - | 2268 | ` * autoloader can produce fails.` |
|        - | 2269 | ` */` |
|     2052 | 2270 | `PH7_PRIVATE int VmClassHintMatches(ph7_vm *pVm,const SyString *pName,ph7_class *pScope,` |
|        - | 2271 | `	ph7_value *pVal,ph7_class **ppResolved)` |
|        5 | 2272 | `{` |
|     2057 | 2273 | `	ph7_class *pExpected = VmResolveTypeClass(pVm,pName,pScope);` |
|     2057 | 2274 | `	*ppResolved = pExpected;` |
|     2057 | 2275 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       55 | 2276 | `		return 0;` |
|        - | 2277 | `	}` |
|     2007 | 2278 | `	if( pExpected == 0 ){` |
|       13 | 2279 | `		return VmHintIsScopeKeyword(pName);` |
|        - | 2280 | `	}` |
|     1995 | 2281 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass,pExpected);` |
|     1031 | 2282 | `}` |
|        - | 2283 | `/* A character that can be part of a type NAME, as opposed to the punctuation` |
|        - | 2284 | `` * that separates the parts of a declared type (`?A`, `A\|B`, `(A&B)\|null`). */`` |
|   416532 | 2285 | `static int VmHintNameChar(int c)` |
|        5 | 2286 | `{` |
|   831771 | 2287 | `	return !(c == '\|' \|\| c == '&' \|\| c == '?' \|\| c == '(' \|\| c == ')'` |
|   415254 | 2288 | `		\|\| c == ' ' \|\| c == '\t');` |
|        5 | 2289 | `}` |
|        - | 2290 | `/*` |
|        - | 2291 | ` * Render a DECLARED type text for a message with the scope keywords RESOLVED,` |
|        - | 2292 | `` * the way php prints it: `of type self` reads `of type P`, `self\|false` reads`` |
|        - | 2293 | `` * `P\|false`, `?static` reads `?Q`. The declared text is what the compiler`` |
|        - | 2294 | `` * canonicalised (php's own member order, `?T` shorthand and all), so rewriting`` |
|        - | 2295 | ` * it name-by-name keeps that shape — only the three names that cannot be` |
|        - | 2296 | ` * resolved until the call site are substituted.` |
|        - | 2297 | ` *` |
|        - | 2298 | ` * A single CLASS hint has its own builder, VmClassHintTypeName, which resolves` |
|        - | 2299 | ` * from the ph7_class* the check already produced; this one is for the texts that` |
|        - | 2300 | ` * are only ever available as source: unions/intersections, and the property /` |
|        - | 2301 | ` * class-constant messages, which print the declared type whatever its shape.` |
|        - | 2302 | ` * An unresolvable keyword is left as written (there is no class to name).` |
|        - | 2303 | ` *` |
|        - | 2304 | `` * `iterable` is the one name php SPELLS DIFFERENTLY in a message than in the`` |
|        - | 2305 | `` * canonical text: Reflection prints `iterable`/`?iterable` (which is what the`` |
|        - | 2306 | ` * compiler stores, and what a COMPOUND type already has expanded in place —` |
|        - | 2307 | `` * `iterable\|int` is stored `Traversable\|array\|int`), while every diagnostic`` |
|        - | 2308 | ` * names the two types it stands for. Only the standalone spellings can still` |
|        - | 2309 | ` * reach here, so the substitution is over the whole text rather than per token —` |
|        - | 2310 | `` * `?iterable` is `Traversable\|array\|null`, not `?Traversable\|array`.`` |
|        - | 2311 | ` */` |
|   100470 | 2312 | `PH7_PRIVATE const char *VmHintTextResolved(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|        - | 2313 | `	char *zBuf,sxu32 nBuf)` |
|        5 | 2314 | `{` |
|   150710 | 2315 | `	return VmHintTextResolvedEx(&(*pVm),pDeclared,pScope,` |
|    50235 | 2316 | `		PH7_HINT_TEXT_ITERABLE\|PH7_HINT_TEXT_STATIC,zBuf,nBuf);` |
|        5 | 2317 | `}` |
|        - | 2318 | `/*` |
|        - | 2319 | ` * The two halves of the rewrite above, asked for separately.` |
|        - | 2320 | ` *` |
|        - | 2321 | `` * PH7_HINT_TEXT_ITERABLE expands a standalone `iterable`; every DIAGNOSTIC wants`` |
|        - | 2322 | `` * that and Reflection wants none of it (php prints `iterable` there).`` |
|        - | 2323 | `` * PH7_HINT_TEXT_STATIC resolves `static` beside `self`/`parent`; a diagnostic`` |
|        - | 2324 | ` * names the class it stands for, while Reflection and the declaration renderer` |
|        - | 2325 | ` * both print the keyword, php having no class to name until the call.` |
|        - | 2326 | ` */` |
|   102292 | 2327 | `PH7_PRIVATE const char *VmHintTextResolvedEx(ph7_vm *pVm,const SyString *pDeclared,ph7_class *pScope,` |
|        - | 2328 | `	int iFlags,char *zBuf,sxu32 nBuf)` |
|        5 | 2329 | `{` |
|        - | 2330 | `	const char *z;` |
|   102297 | 2331 | `	sxu32 n, i = 0, nAt = 0;` |
|   102297 | 2332 | `	if( nBuf == 0 ){` |
|      ! 0 | 2333 | `		return "";` |
|        - | 2334 | `	}` |
|   102297 | 2335 | `	z = pDeclared ? pDeclared->zString : 0;` |
|   102297 | 2336 | `	n = z ? pDeclared->nByte : 0;` |
|   102297 | 2337 | `	if( z && (iFlags & PH7_HINT_TEXT_ITERABLE) ){` |
|   100475 | 2338 | `		const char *zIter = 0;` |
|   100475 | 2339 | `		if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        9 | 2340 | `			zIter = "Traversable\|array";` |
|   100472 | 2341 | `		}else if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|        3 | 2342 | `			zIter = "Traversable\|array\|null";` |
|        1 | 2343 | `		}` |
|   100475 | 2344 | `		if( zIter ){` |
|       11 | 2345 | `			sxu32 nIter = SyStrlen(zIter);` |
|       11 | 2346 | `			if( nIter > nBuf - 1 ){` |
|      ! 0 | 2347 | `				nIter = nBuf - 1;` |
|      ! 0 | 2348 | `			}` |
|       11 | 2349 | `			SyMemcpy(zIter,zBuf,nIter);` |
|       11 | 2350 | `			zBuf[nIter] = 0;` |
|       11 | 2351 | `			return zBuf;` |
|        - | 2352 | `		}` |
|    50231 | 2353 | `	}` |
|   205609 | 2354 | `	while( i < n && nAt + 1 < nBuf ){` |
|        - | 2355 | `		sxu32 nStart, nCopy;` |
|        - | 2356 | `		SyString sTok;` |
|        - | 2357 | `		const SyString *pOut;` |
|   103325 | 2358 | `		if( !VmHintNameChar(z[i]) ){` |
|      835 | 2359 | `			zBuf[nAt++] = z[i++];` |
|      835 | 2360 | `			continue;` |
|        - | 2361 | `		}` |
|   102495 | 2362 | `		nStart = i;` |
|   415239 | 2363 | `		while( i < n && VmHintNameChar(z[i]) ){` |
|   312749 | 2364 | `			i++;` |
|        5 | 2365 | `		}` |
|   102495 | 2366 | `		SyStringInitFromBuf(&sTok,&z[nStart],i - nStart);` |
|   102495 | 2367 | `		pOut = &sTok;` |
|   102490 | 2368 | `		if( VmHintIsScopeKeyword(&sTok)` |
|    51295 | 2369 | `		 && ( (iFlags & PH7_HINT_TEXT_STATIC)` |
|       55 | 2370 | `		   \|\| sTok.nByte != sizeof("static")-1` |
|       24 | 2371 | `		   \|\| SyStrnicmp(sTok.zString,"static",sizeof("static")-1) != 0 ) ){` |
|       76 | 2372 | `			ph7_class *pRes = VmResolveTypeClass(pVm,&sTok,pScope);` |
|       76 | 2373 | `			if( pRes ){` |
|       57 | 2374 | `				pOut = &pRes->sName;` |
|       27 | 2375 | `			}` |
|       36 | 2376 | `		}` |
|   102495 | 2377 | `		nCopy = pOut->nByte;` |
|   102495 | 2378 | `		if( nCopy > nBuf - nAt - 1 ){` |
|      ! 0 | 2379 | `			nCopy = nBuf - nAt - 1;` |
|      ! 0 | 2380 | `		}` |
|   102495 | 2381 | `		if( nCopy > 0 ){` |
|   102495 | 2382 | `			SyMemcpy(pOut->zString,&zBuf[nAt],nCopy);` |
|   102495 | 2383 | `			nAt += nCopy;` |
|    51245 | 2384 | `		}` |
|        5 | 2385 | `	}` |
|   102289 | 2386 | `	zBuf[nAt] = 0;` |
|   102289 | 2387 | `	return zBuf;` |
|    51151 | 2388 | `}` |
|        - | 2389 | `/*` |
|        - | 2390 | ` * PHL's number model flags a whole-valued real MEMOBJ_REAL\|MEMOBJ_INT (the` |
|        - | 2391 | ` * float-identity leniency — see the typed-constant note above` |
|        - | 2392 | ` * VmEnforceConstantType). Observer sites (is_int, gettype, var_dump, ===)` |
|        - | 2393 | ` * treat REAL as dominant, so such a value READS as a float. But every` |
|        - | 2394 | `` * TYPE-CHECK mask test (`iFlags & nType`) accepts it through its INT bit,`` |
|        - | 2395 | ` * so an int-typed parameter / return / property / union member silently` |
|        - | 2396 | ` * kept the value LOOKING like a float where php produces a genuine int` |
|        - | 2397 | ` * (weak-mode float->int here is lossless by construction). Call this after` |
|        - | 2398 | ` * a mask ACCEPT to materialize the int. No-op for any other value/type` |
|        - | 2399 | ` * pairing — including SXU32_HIGH and int\|float-style masks (REAL bit` |
|        - | 2400 | ` * present).` |
|        - | 2401 | ` *` |
|        - | 2402 | `` * Recorded leniency (strict_types): php rejects a float literal (`f(1.0)`)`` |
|        - | 2403 | ` * under strict_types, but the SAME dual-flagged shape also comes out of` |
|        - | 2404 | ``  * PHL arithmetic/builtins where php produces a genuine INT — `pow(2,3)` `` |
|        - | 2405 | ` * is php int(8), PHL whole-real float(8). PHL cannot tell those apart by` |
|        - | 2406 | ` * flags, so strict mode accepts-and-materializes both rather than` |
|        - | 2407 | `` * rejecting the php-valid `f(pow(2,3))`.`` |
|        - | 2408 | ` */` |
|    58384 | 2409 | `PH7_PRIVATE void VmMaterializeIntTyped(ph7_value *pVal, sxu32 nType)` |
|        5 | 2410 | `{` |
|    58384 | 2411 | `	if( (nType & MEMOBJ_INT) && (nType & MEMOBJ_REAL) == 0` |
|    32061 | 2412 | `	 && (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        - | 2413 | `		/* NOT PH7_MemObjToInteger — that no-ops when the INT bit is already` |
|        - | 2414 | `		 * set, which is exactly the dual-flag case. x.iVal already holds the` |
|        - | 2415 | `		 * exact integer (MemObjTryIntger only sets the INT bit when the` |
|        - | 2416 | `		 * real->int->real round-trip is lossless); drop the REAL identity. */` |
|       57 | 2417 | `		pVal->x.iVal = (sxi64)pVal->rVal;` |
|       57 | 2418 | `		SyBlobRelease(&pVal->sBlob);` |
|       57 | 2419 | `		MemObjSetType(pVal, MEMOBJ_INT);` |
|       27 | 2420 | `	}` |
|    58389 | 2421 | `}` |
|     3734 | 2422 | `PH7_PRIVATE sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict,` |
|        - | 2423 | `	ph7_class *pSelf)` |
|        5 | 2424 | `{` |
|        - | 2425 | `	sxu32 i;` |
|        - | 2426 | `	sxu32 nAlts;` |
|        - | 2427 | `	ph7_type_alt *aAlts;` |
|        - | 2428 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - | 2429 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|     3739 | 2430 | `	int bHasIntersection = 0;` |
|        - | 2431 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|     3739 | 2432 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       20 | 2433 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - | 2434 | `	}` |
|     3723 | 2435 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|     3723 | 2436 | `	nAlts = SySetUsed(pAlts);` |
|        - | 2437 | `	/* Tally OR-group sizes: a group of ≥2 alternatives is an intersection (the` |
|        - | 2438 | `	 * value must match ALL its members); singleton groups are ordinary union` |
|        - | 2439 | `	 * alternatives (match ANY). Group ids are NOT dense in the stored set —` |
|        - | 2440 | ``	 * `null`-only parts are dropped at store time, leaving gaps — so an id can be`` |
|        - | 2441 | `	 * up to (parts-1) ≥ nAlts; index the full PHL_UNION_MAX_ALTS-wide tally. */` |
|   122699 | 2442 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    11343 | 2443 | `	for( i = 0; i < nAlts; i++ ){` |
|     7625 | 2444 | `		if( aAlts[i].nGroup < PHL_UNION_MAX_ALTS && ++aGroupCount[aAlts[i].nGroup] == 2 ){` |
|       49 | 2445 | `			bHasIntersection = 1;` |
|       23 | 2446 | `		}` |
|     3809 | 2447 | `	}` |
|        - | 2448 | `	/* Intersection phase: an object satisfies an intersection group iff it is` |
|        - | 2449 | `	 * instanceof every member. Members are always class types (enforced at parse),` |
|        - | 2450 | `	 * so a non-object value can never satisfy a group. Skipped entirely for a pure` |
|        - | 2451 | `	 * union (the common case), which then pays nothing for the group machinery. */` |
|     3723 | 2452 | `	if( bHasIntersection && (pValue->iFlags & MEMOBJ_OBJ) ){` |
|       35 | 2453 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 2454 | `		sxu32 g;` |
|      421 | 2455 | `		for( g = 0; g < PHL_UNION_MAX_ALTS; g++ ){` |
|        - | 2456 | `			int bAll;` |
|      409 | 2457 | `			if( aGroupCount[g] < 2 ) continue;` |
|       35 | 2458 | `			bAll = 1;` |
|       87 | 2459 | `			for( i = 0; i < nAlts; i++ ){` |
|        - | 2460 | `				ph7_class *pExpected;` |
|       67 | 2461 | `				if( aAlts[i].nGroup != g ) continue;` |
|       63 | 2462 | `				if( aAlts[i].nType != SXU32_HIGH ){ bAll = 0; break; }` |
|       63 | 2463 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|       63 | 2464 | `				if( pExpected == 0 \|\| !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       15 | 2465 | `					bAll = 0;` |
|       15 | 2466 | `					break;` |
|        - | 2467 | `				}` |
|       27 | 2468 | `			}` |
|       35 | 2469 | `			if( bAll ) return SXRET_OK;` |
|        9 | 2470 | `		}` |
|        6 | 2471 | `	}` |
|        - | 2472 | ``	/* Pseudo-type alternatives (true/false/iterable; `mixed` never unions) are`` |
|        - | 2473 | `	 * stored as SXU32_HIGH name atoms and need value-checking, not instanceof.` |
|        - | 2474 | ``	 * A match on any one accepts the value (handles e.g. `true\|int`, `?true`,`` |
|        - | 2475 | ``	 * `iterable\|Foo`). Only singleton-group (ordinary union) atoms apply here.`` |
|        - | 2476 | `` 	 * php takes a value its own type's arm names before it asks `callable` `` |
|        - | 2477 | ``	 * anything, so `callable\|string` given 'self::m' raises no callable`` |
|        - | 2478 | `	 * deprecation: accept a plain string or an array on that arm first. */` |
|     3698 | 2479 | `	if( (pValue->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) == MEMOBJ_STRING` |
|     3121 | 2480 | `	 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|     3177 | 2481 | `		sxu32 nOwn = (pValue->iFlags & MEMOBJ_HASHMAP) ? MEMOBJ_HASHMAP : MEMOBJ_STRING;` |
|     3291 | 2482 | `		for( i = 0; i < nAlts; i++ ){` |
|     3257 | 2483 | `			if( aGroupCount[aAlts[i].nGroup] < 2 && aAlts[i].nType == nOwn ){` |
|     3143 | 2484 | `				return SXRET_OK;` |
|        - | 2485 | `			}` |
|       62 | 2486 | `		}` |
|       17 | 2487 | `	}` |
|     1413 | 2488 | `	for( i = 0; i < nAlts; i++ ){` |
|     1043 | 2489 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      986 | 2490 | `		if( aAlts[i].nType == SXU32_HIGH` |
|      704 | 2491 | `		 && VmCheckPseudoType(pVm, pValue, &aAlts[i].sClass) == 1 ){` |
|      195 | 2492 | `			return SXRET_OK;` |
|        - | 2493 | `		}` |
|      403 | 2494 | `	}` |
|      375 | 2495 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|      375 | 2496 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|     1167 | 2497 | `	for( i = 0; i < nAlts; i++ ){` |
|      797 | 2498 | `		if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      745 | 2499 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      541 | 2500 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      537 | 2501 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      531 | 2502 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|      283 | 2503 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|      163 | 2504 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|       14 | 2505 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|      375 | 2506 | `	}` |
|        - | 2507 | `	/* Object handling */` |
|      375 | 2508 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      105 | 2509 | `		if( bHasObjAlt ) return SXRET_OK;` |
|      105 | 2510 | `		if( bHasClassAlt ){` |
|       91 | 2511 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      225 | 2512 | `			for( i = 0; i < nAlts; i++ ){` |
|        - | 2513 | `				ph7_class *pExpected;` |
|      169 | 2514 | `				if( aGroupCount[aAlts[i].nGroup] >= 2 ) continue;` |
|      161 | 2515 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|      115 | 2516 | `				pExpected = VmResolveTypeClass(pVm,&aAlts[i].sClass,pSelf);` |
|      115 | 2517 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|       34 | 2518 | `					return SXRET_OK;` |
|        - | 2519 | `				}` |
|       45 | 2520 | `			}` |
|       28 | 2521 | `		}` |
|       74 | 2522 | `		return SXERR_INVALID;` |
|        - | 2523 | `	}` |
|        - | 2524 | `	/* Array handling */` |
|      275 | 2525 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 2526 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - | 2527 | `	}` |
|        - | 2528 | `	/* Scalar handling — exact match first. REAL before INT: a whole-valued` |
|        - | 2529 | `	 * real carries MEMOBJ_REAL\|MEMOBJ_INT (float-identity leniency), reads as` |
|        - | 2530 | ``	 * a float, and must prefer a `float` member (php: 1.0 into int\|float`` |
|        - | 2531 | ``	 * stays float); absent one, an `int` member takes it as a genuine int`` |
|        - | 2532 | `	 * (php: 1.0 into int\|string is int(1)) — materialize so it stops READING` |
|        - | 2533 | `	 * as a float. A pure int never carries REAL, so its arm is unaffected. */` |
|      263 | 2534 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|       71 | 2535 | `		if( bHasFloat ) return SXRET_OK;` |
|        6 | 2536 | `	}` |
|      209 | 2537 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|      187 | 2538 | `		if( bHasInt ){` |
|      151 | 2539 | `			VmMaterializeIntTyped(pValue, MEMOBJ_INT);` |
|      151 | 2540 | `			return SXRET_OK;` |
|        - | 2541 | `		}` |
|       18 | 2542 | `	}` |
|       63 | 2543 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       20 | 2544 | `		if( bHasString ) return SXRET_OK;` |
|        9 | 2545 | `	}` |
|       63 | 2546 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|        3 | 2547 | `		if( bHasBool ) return SXRET_OK;` |
|        1 | 2548 | `	}` |
|       63 | 2549 | `	if( bStrict ){` |
|        - | 2550 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|       23 | 2551 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|       16 | 2552 | `			PH7_MemObjToReal(pValue);` |
|       16 | 2553 | `			return SXRET_OK;` |
|        - | 2554 | `		}` |
|        8 | 2555 | `		return SXERR_INVALID;` |
|        - | 2556 | `	}` |
|        - | 2557 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - | 2558 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - | 2559 | `	 * to match PHP's union RFC. */` |
|        - | 2560 | `	{` |
|       42 | 2561 | `		int kind = VmStringNumericKind(pValue);` |
|       42 | 2562 | `		if( bHasInt ){` |
|        - | 2563 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - | 2564 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       20 | 2565 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 2566 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 | 2567 | `				return SXRET_OK;` |
|        - | 2568 | `			}` |
|       20 | 2569 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 | 2570 | `				ph7_real r = pValue->rVal;` |
|        - | 2571 | ``				/* Range first: `(sxi64)r` is undefined outside it, and NaN`` |
|        - | 2572 | `				 * and the infinities are not exact ints either way. */` |
|        - | 2573 | `				/* (double)r: see VmValueIsLossyToInt -- a no-op where ph7_real` |
|        - | 2574 | `				 * is double, and the narrowing MSVC turns into an error under` |
|        - | 2575 | `				 * PH7_OMIT_FLOATING_POINT otherwise. */` |
|      ! 0 | 2576 | `				if( PH7_RealFitsInt64((double)r) && r == (ph7_real)(sxi64)r ){` |
|      ! 0 | 2577 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 | 2578 | `					return SXRET_OK;` |
|        - | 2579 | `				}` |
|      ! 0 | 2580 | `			}` |
|       20 | 2581 | `			if( kind == 1 ){` |
|        9 | 2582 | `				PH7_MemObjToInteger(pValue);` |
|        9 | 2583 | `				return SXRET_OK;` |
|        - | 2584 | `			}` |
|        5 | 2585 | `		}` |
|       34 | 2586 | `		if( bHasFloat ){` |
|       10 | 2587 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 | 2588 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 | 2589 | `				return SXRET_OK;` |
|        - | 2590 | `			}` |
|       10 | 2591 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 | 2592 | `				PH7_MemObjToReal(pValue);` |
|        7 | 2593 | `				return SXRET_OK;` |
|        - | 2594 | `			}` |
|        1 | 2595 | `		}` |
|       27 | 2596 | `		if( bHasString ){` |
|      ! 0 | 2597 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 | 2598 | `				PH7_MemObjToString(pValue);` |
|      ! 0 | 2599 | `				return SXRET_OK;` |
|        - | 2600 | `			}` |
|      ! 0 | 2601 | `		}` |
|       27 | 2602 | `		if( bHasBool ){` |
|        3 | 2603 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|        3 | 2604 | `				PH7_MemObjToBool(pValue);` |
|        3 | 2605 | `				return SXRET_OK;` |
|        - | 2606 | `			}` |
|      ! 0 | 2607 | `		}` |
|        - | 2608 | `	}` |
|       25 | 2609 | `	return SXERR_INVALID;` |
|     1869 | 2610 | `}` |
|        - | 2611 |  |
|        - | 2612 | `/*` |
|        - | 2613 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - | 2614 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - | 2615 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - | 2616 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - | 2617 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - | 2618 | ` */` |
|      734 | 2619 | `PH7_PRIVATE sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        5 | 2620 | `{` |
|        - | 2621 | ``	/* A standalone `null` type is not a weak-coercion target: only an actual`` |
|        - | 2622 | `	 * null value satisfies it (and a null value matches via the flag test` |
|        - | 2623 | `	 * before this is ever called, so pVal is non-null here). Reject rather than` |
|        - | 2624 | ``	 * casting the value to null — otherwise a `null`-typed parameter would`` |
|        - | 2625 | `	 * silently swallow any argument. */` |
|      739 | 2626 | `	if( nType == MEMOBJ_NULL ){` |
|        3 | 2627 | `		return SXERR_INVALID;` |
|        - | 2628 | `	}` |
|        - | 2629 | `	/* Array and scalar never coerce into each other. php throws a TypeError` |
|        - | 2630 | `	 * rather than wrapping a scalar into a 1-element array or stringifying an` |
|        - | 2631 | ``	 * array (`function f(array $a){} f(true)` — php: "must be of type array,`` |
|        - | 2632 | ``	 * true given"; PHL used to run the body with $a=[true], and `f(int $a){}`` |
|        - | 2633 | ``	 * f([1,2])` truncated the array to int(1)). The typed-property store and`` |
|        - | 2634 | `	 * return-type paths already guard this inline; enforcing it here closes the` |
|        - | 2635 | `	 * same hole for typed PARAMETERS (positional/variadic/union) and generator` |
|        - | 2636 | `	 * params, which all funnel their scalar coercion through this helper. An` |
|        - | 2637 | `	 * object value against an array type is caught here too (never valid);` |
|        - | 2638 | `	 * object->scalar stays a separate case handled by the callers. */` |
|      737 | 2639 | `	if( ((pVal->iFlags & MEMOBJ_HASHMAP) != 0) != (nType == MEMOBJ_HASHMAP) ){` |
|       84 | 2640 | `		return SXERR_INVALID;` |
|        - | 2641 | `	}` |
|      657 | 2642 | `	if( bStrict ){` |
|        - | 2643 | `		/* Only int -> float widening is allowed implicitly. */` |
|      133 | 2644 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|       11 | 2645 | `			PH7_MemObjToReal(pVal);` |
|       11 | 2646 | `			return SXRET_OK;` |
|        - | 2647 | `		}` |
|      125 | 2648 | `		return SXERR_INVALID;` |
|        - | 2649 | `	}` |
|        - | 2650 | `	/* Weak mode, but PHP still rejects some coercions with a TypeError rather` |
|        - | 2651 | `	 * than silently fabricating a value (mirroring the return-type path above):` |
|        - | 2652 | `	 *   - null to a non-nullable scalar (a null reaching here is always` |
|        - | 2653 | `	 *     non-nullable — the caller's guards skip nullable+null, and a` |
|        - | 2654 | ``	 *     `Type $x = null` default is flagged implicitly nullable at compile time).`` |
|        - | 2655 | `	 *   - a non-numeric string to int/float ("abc"/"12abc"). Only a strictly` |
|        - | 2656 | `	 *     numeric string (optional surrounding whitespace) coerces. */` |
|      529 | 2657 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       23 | 2658 | `		return SXERR_INVALID;` |
|        - | 2659 | `	}` |
|        - | 2660 | `	/* An object satisfies a scalar type in exactly ONE php weak-mode case: an` |
|        - | 2661 | ``	 * object with __toString() coerces to a `string` parameter (its __toString`` |
|        - | 2662 | `	 * is invoked by the string cast below). Every other scalar target —` |
|        - | 2663 | ``	 * int/float/bool, or a `string` target on an object WITHOUT __toString —`` |
|        - | 2664 | `	 * is a TypeError. Without this, PH7_MemObjCastMethod() silently produced` |
|        - | 2665 | `	 * int(1)/float(1)/bool(true) for any object and "Object" for a` |
|        - | 2666 | `	 * non-stringable object passed to a string parameter. (Strict mode already` |
|        - | 2667 | `	 * rejected all of these in the bStrict block above; the array<->object case` |
|        - | 2668 | `	 * is caught by the array guard.) */` |
|      509 | 2669 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       26 | 2670 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       32 | 2671 | `		if( !(nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       12 | 2672 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       18 | 2673 | `			return SXERR_INVALID;` |
|        - | 2674 | `		}` |
|        4 | 2675 | `	}` |
|      488 | 2676 | `	if( (nType == MEMOBJ_INT \|\| nType == MEMOBJ_REAL)` |
|      400 | 2677 | `		&& (pVal->iFlags & MEMOBJ_STRING)` |
|      374 | 2678 | `		&& !PH7_MemObjStringIsNumeric(pVal) ){` |
|      157 | 2679 | `		return SXERR_INVALID;` |
|        - | 2680 | `	}` |
|      341 | 2681 | `	if( nType == MEMOBJ_INT && VmValueIsLossyToInt(pVal) ){` |
|        - | 2682 | `		/* php 8.1 only DEPRECATES a lossy float(-string) -> int weak coercion;` |
|        - | 2683 | `		 * PHL rejects it (the scope policy). SXERR_INVALID routes to the caller's TypeError,` |
|        - | 2684 | `		 * exactly like the null / non-numeric-string cases above. An INTEGRAL` |
|        - | 2685 | `		 * float loses nothing and coerces normally. One predicate answers this` |
|        - | 2686 | `		 * for the typed parameters and returns that reach here, for the typed` |
|        - | 2687 | `		 * PROPERTY store (which has its own weak path below) and for the` |
|        - | 2688 | `		 * integer-only operators. */` |
|      ! 0 | 2689 | `		return SXERR_INVALID;` |
|        - | 2690 | `	}` |
|      336 | 2691 | `	if( nType == MEMOBJ_STRING && (pVal->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|       71 | 2692 | `	 && pVal->pVm && PH7_IS_NAN(pVal->rVal) ){` |
|        - | 2693 | ``		/* A userland `string` parameter, return or property taking a NaN: php's`` |
|        - | 2694 | `		 * weak coercion warns there exactly as its ZPP does for an internal one` |
|        - | 2695 | ``		 * (`unexpected NAN value was coerced to string`). The cast below is the`` |
|        - | 2696 | `		 * silent conversion -- it is shared with the engine's own -- so the` |
|        - | 2697 | `		 * diagnostic is raised here, where the DECLARED type is known. */` |
|        3 | 2698 | `		VmErrorFormat(pVal->pVm,PH7_CTX_WARNING,` |
|        - | 2699 | `			"unexpected NAN value was coerced to string");` |
|        1 | 2700 | `	}` |
|        - | 2701 | `	{` |
|      341 | 2702 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|      341 | 2703 | `		if( xCast ) xCast(pVal);` |
|        - | 2704 | `	}` |
|      341 | 2705 | `	return SXRET_OK;` |
|      372 | 2706 | `}` |
|        - | 2707 |  |
|        - | 2708 | `/*` |
|        - | 2709 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - | 2710 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - | 2711 | ` *` |
|        - | 2712 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - | 2713 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - | 2714 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - | 2715 | ` * back to a static literal and ignore zBuf entirely.` |
|        - | 2716 | ` */` |
|      452 | 2717 | `PH7_PRIVATE const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        5 | 2718 | `{` |
|      457 | 2719 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|      457 | 2720 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|      457 | 2721 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|      457 | 2722 | `		if( pDeclared->zString && nCopy > 0 ){` |
|      457 | 2723 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|      226 | 2724 | `		}` |
|      457 | 2725 | `		zBuf[nCopy] = 0;` |
|      457 | 2726 | `		return zBuf;` |
|        - | 2727 | `	}` |
|      ! 0 | 2728 | `	switch( nType ){` |
|      ! 0 | 2729 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 | 2730 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 | 2731 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 | 2732 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 | 2733 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 | 2734 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 | 2735 | `		default:             return "scalar";` |
|        - | 2736 | `	}` |
|      231 | 2737 | `}` |
|        - | 2738 |  |
|        - | 2739 | `/*` |
|        - | 2740 | ` * Render the expected-type text of a single (non-union) CLASS or pseudo-type hint` |
|        - | 2741 | ` * the way php writes it in a TypeError:` |
|        - | 2742 | ` *` |
|        - | 2743 | `` *   - the RESOLVED class, so `self`/`parent` name the class they stand for`` |
|        - | 2744 | `` *     (`?self` in class Cee reads `?Cee`); an unresolvable name stays as written;`` |
|        - | 2745 | `` *   - `iterable` expanded to its real alternatives, `Traversable\|array`;`` |
|        - | 2746 | `` *   - a leading `?` whenever the hint is nullable — including the implicit`` |
|        - | 2747 | `` *     `Cee $c = null` form, which php also prints as `?Cee`.`` |
|        - | 2748 | ` *` |
|        - | 2749 | ` * The scalar half of this is VmScalarTypeName (which reads the declared text` |
|        - | 2750 | `` * straight off the formal, `?` included). A class hint cannot do that: its text`` |
|        - | 2751 | `` * is resolved per call site, so the `?` has to be re-applied here — which is why`` |
|        - | 2752 | `` * the message used to say `Cee` where php says `?Cee`.`` |
|        - | 2753 | ` */` |
|      196 | 2754 | `PH7_PRIVATE const char *VmClassHintTypeName(const SyString *pAsWritten,ph7_class *pResolved,` |
|        - | 2755 | `	int bNullable,char *zBuf,sxu32 nBuf)` |
|        5 | 2756 | `{` |
|      201 | 2757 | `	const SyString *pName = pResolved ? &pResolved->sName : pAsWritten;` |
|        - | 2758 | `	sxu32 nCopy;` |
|      201 | 2759 | `	sxu32 nAt = 0;` |
|      201 | 2760 | `	if( nBuf == 0 ){` |
|      ! 0 | 2761 | `		return "";` |
|        - | 2762 | `	}` |
|      196 | 2763 | `	if( pName && SyStringLength(pName) == sizeof("iterable")-1 && pName->zString` |
|       85 | 2764 | `	 && SyStrnicmp(pName->zString,"iterable",sizeof("iterable")-1) == 0 ){` |
|       21 | 2765 | `		const char *zIter = bNullable ? "Traversable\|array\|null" : "Traversable\|array";` |
|       21 | 2766 | `		nCopy = SyStrlen(zIter);` |
|       21 | 2767 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       21 | 2768 | `		SyMemcpy(zIter,zBuf,nCopy);` |
|       21 | 2769 | `		zBuf[nCopy] = 0;` |
|       21 | 2770 | `		return zBuf;` |
|        - | 2771 | `	}` |
|      183 | 2772 | `	if( bNullable && nBuf > 1 ){` |
|       25 | 2773 | `		zBuf[nAt++] = '?';` |
|       11 | 2774 | `	}` |
|      183 | 2775 | `	nCopy = (pName && pName->zString) ? SyStringLength(pName) : 0;` |
|      183 | 2776 | `	if( nCopy >= nBuf - nAt ) nCopy = nBuf - nAt - 1;` |
|      183 | 2777 | `	if( nCopy > 0 ) SyMemcpy(pName->zString,&zBuf[nAt],nCopy);` |
|      183 | 2778 | `	zBuf[nAt + nCopy] = 0;` |
|      183 | 2779 | `	return zBuf;` |
|      103 | 2780 | `}` |
|        - | 2781 |  |
|        - | 2782 | `/*` |
|        - | 2783 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - | 2784 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - | 2785 | ` */` |
|      144 | 2786 | `PH7_PRIVATE const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        5 | 2787 | `{` |
|      149 | 2788 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|      221 | 2789 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|      144 | 2790 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|      149 | 2791 | `	return zBuf;` |
|        5 | 2792 | `}` |
|        - | 2793 |  |
|        - | 2794 | `/*` |
|        - | 2795 | ` * php's write_property handler (ph7_class::xSet): a native class whose properties` |
|        - | 2796 | ` * are its own C struct converts the incoming value the way that struct demands —` |
|        - | 2797 | ` * and may refuse the write outright. The value is rewritten IN PLACE, so what the` |
|        - | 2798 | ` * caller goes on to store is what the hook left behind.` |
|        - | 2799 | ` */` |
|      618 | 2800 | `static sxi32 VmRunNativeSet(ph7_vm *pVm,VmClassAttr *pVmAttr,ph7_value *pValue)` |
|        3 | 2801 | `{` |
|        - | 2802 | `	PH7_NativeSetCtx sSet;` |
|      621 | 2803 | `	if( PH7_VmAttrInst(pVmAttr) == 0 ){` |
|      ! 0 | 2804 | `		return SXRET_OK;   /* a class static: no object for a handler to run on */` |
|        - | 2805 | `	}` |
|      621 | 2806 | `	sSet.pName = &pVmAttr->pAttr->sName;` |
|      621 | 2807 | `	sSet.pValue = pValue;` |
|      621 | 2808 | `	sSet.zThrowClass = 0;` |
|      621 | 2809 | `	sSet.zThrowMsg[0] = 0;` |
|      621 | 2810 | `	if( PH7_ClassNativeSet(PH7_VmAttrInst(pVmAttr),&sSet) && sSet.zThrowClass ){` |
|        7 | 2811 | `		return VmThrowFixedError(pVm,sSet.zThrowClass,sSet.zThrowMsg);` |
|        - | 2812 | `	}` |
|      615 | 2813 | `	return SXRET_OK;` |
|      312 | 2814 | `}` |
|        - | 2815 | `/*` |
|        - | 2816 | ` * The same handler, asked of a SLOT that an opcode has already mutated in place.` |
|        - | 2817 | `` * `$i->f++` and `$i->f--` never pass a value through the store filter — they`` |
|        - | 2818 | ` * increment the slot where it lies — so the conversion has to be applied after` |
|        - | 2819 | ` * the fact, which is exactly what php does (it reads, increments, and writes` |
|        - | 2820 | `` * back through the handler: `$i->f = 1.456008; ++$i->f` leaves the property at`` |
|        - | 2821 | ` * 2.456007, the microsecond truncation of the sum). Answers SXRET_OK when the` |
|        - | 2822 | ` * slot is not a native one.` |
|        - | 2823 | ` */` |
|  7439154 | 2824 | `static void VmFilterBitSet(ph7_vm *pVm,sxu32 nIdx)` |
|        5 | 2825 | `{` |
|  7439159 | 2826 | `	if( pVm->bFilterBitsOff ){` |
|      ! 0 | 2827 | `		return;` |
|        - | 2828 | `	}` |
|  7439159 | 2829 | `	if( nIdx >= pVm->nFilterBits ){` |
|     1681 | 2830 | `		sxu32 nNew = pVm->nFilterBits ? pVm->nFilterBits : 1024;` |
|        - | 2831 | `		unsigned char *pNew;` |
|        - | 2832 | `		/* An index this large cannot be a real slot, and doubling toward it would` |
|        - | 2833 | `		 * wrap. Treat it exactly like a failed allocation. */` |
|     1681 | 2834 | `		if( nIdx >= (1u << 30) ){` |
|      ! 0 | 2835 | `			pVm->bFilterBitsOff = 1;` |
|      ! 0 | 2836 | `			return;` |
|        - | 2837 | `		}` |
|     1729 | 2838 | `		while( nNew <= nIdx ){` |
|       52 | 2839 | `			nNew <<= 1;` |
|        4 | 2840 | `		}` |
|     1681 | 2841 | `		pNew = (unsigned char *)SyMemBackendAlloc(&pVm->sAllocator,nNew >> 3);` |
|     1681 | 2842 | `		if( pNew == 0 ){` |
|        - | 2843 | `			/* This slot IS filtered and the bitmap cannot say so. It must not answer` |
|        - | 2844 | `			 * for anything else either, or the screens in front of it would skip a` |
|        - | 2845 | `			 * type check, a readonly refusal or a native write handler. */` |
|      ! 0 | 2846 | `			pVm->bFilterBitsOff = 1;` |
|      ! 0 | 2847 | `			return;` |
|        - | 2848 | `		}` |
|     1681 | 2849 | `		SyZero(pNew,nNew >> 3);` |
|     1681 | 2850 | `		if( pVm->pFilterBits ){` |
|       30 | 2851 | `			SyMemcpy(pVm->pFilterBits,pNew,pVm->nFilterBits >> 3);` |
|       30 | 2852 | `			SyMemBackendFree(&pVm->sAllocator,pVm->pFilterBits);` |
|       13 | 2853 | `		}` |
|     1681 | 2854 | `		pVm->pFilterBits = pNew;` |
|     1681 | 2855 | `		pVm->nFilterBits = nNew;` |
|      835 | 2856 | `	}` |
|  7439159 | 2857 | `	pVm->pFilterBits[nIdx >> 3] \|= (unsigned char)(1 << (nIdx & 7));` |
|  3719429 | 2858 | `}` |
|        - | 2859 | `/*` |
|        - | 2860 | ` * Register a property slot with the store filter, and drop it again. These two` |
|        - | 2861 | ` * are the ONLY writers of pVm->hTypedSlot: the predicate that decides membership` |
|        - | 2862 | ` * lives here once (a declared type, a native write handler, or both), and the` |
|        - | 2863 | ` * handler COUNT that lets the mutation opcodes skip the table entirely is kept` |
|        - | 2864 | ` * beside it -- registering in one place and forgetting to drop in another is` |
|        - | 2865 | ` * exactly how a recycled memobj index would inherit a stale entry.` |
|        - | 2866 | ` *` |
|        - | 2867 | ` * They also keep the SLOT BITMAP the hot-path screens read (see pFilterBits): it` |
|        - | 2868 | ` * answers the membership question without hashing, and because it is written here` |
|        - | 2869 | ` * and nowhere else it cannot drift from the table it screens.` |
|        - | 2870 | ` */` |
| 10825921 | 2871 | `PH7_PRIVATE sxi32 PH7_VmStoreFilterRegister(ph7_vm *pVm,VmClassAttr *pVmAttr)` |
|        5 | 2872 | `{` |
| 10825926 | 2873 | `	if( !PH7_ATTR_STORE_FILTERED(pVmAttr->pAttr) ){` |
|  3386772 | 2874 | `		return SXRET_OK;` |
|        - | 2875 | `	}` |
|  7439159 | 2876 | `	if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr) != SXRET_OK ){` |
|      ! 0 | 2877 | `		return SXERR_MEM;` |
|        - | 2878 | `	}` |
|  7439159 | 2879 | `	VmFilterBitSet(&(*pVm),pVmAttr->nIdx);` |
|  7439159 | 2880 | `	if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|    10123 | 2881 | `		pVm->nNativeSetSlot++;` |
|     5059 | 2882 | `	}` |
|  7439159 | 2883 | `	return SXRET_OK;` |
|  5412235 | 2884 | `}` |
| 10110960 | 2885 | `PH7_PRIVATE void PH7_VmStoreFilterDrop(ph7_vm *pVm,ph7_class_attr *pAttr,sxu32 nIdx)` |
|        5 | 2886 | `{` |
| 10110965 | 2887 | `	if( pAttr == 0 \|\| !PH7_ATTR_STORE_FILTERED(pAttr) ){` |
|  3174070 | 2888 | `		return;` |
|        - | 2889 | `	}` |
|  6936900 | 2890 | `	if( SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32),0) == SXRET_OK ){` |
|  6936900 | 2891 | `		if( nIdx < pVm->nFilterBits ){` |
|  6936900 | 2892 | `			pVm->pFilterBits[nIdx >> 3] &= (unsigned char)~(1 << (nIdx & 7));` |
|  3468300 | 2893 | `		}` |
|  6936900 | 2894 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) && pVm->nNativeSetSlot > 0 ){` |
|    10109 | 2895 | `			pVm->nNativeSetSlot--;` |
|     5052 | 2896 | `		}` |
|  3468300 | 2897 | `	}` |
|  5054766 | 2898 | `}` |
| 12862798 | 2899 | `PH7_PRIVATE sxi32 PH7_VmNativeSetSlot(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        5 | 2900 | `{` |
|        - | 2901 | `	SyHashEntry *pSlot;` |
|        - | 2902 | `	VmClassAttr *pVmAttr;` |
| 12862803 | 2903 | `	if( nIdx == SXU32_HIGH \|\| pVm->nNativeSetSlot == 0 \|\| !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
| 12860785 | 2904 | `		return SXRET_OK;` |
|        - | 2905 | `	}` |
|   167573 | 2906 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|   167573 | 2907 | `	if( pSlot == 0 ){` |
|      ! 0 | 2908 | `		return SXRET_OK;` |
|        - | 2909 | `	}` |
|     2019 | 2910 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     2019 | 2911 | `	if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) == 0 ){` |
|     2013 | 2912 | `		return SXRET_OK;` |
|        - | 2913 | `	}` |
|        7 | 2914 | `	return VmRunNativeSet(pVm,pVmAttr,pValue);` |
|  6432483 | 2915 | `}` |
|  1120774 | 2916 | `PH7_PRIVATE sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue,int iStoreFlags)` |
|        5 | 2917 | `{` |
|  1120779 | 2918 | `	int bViaRef = (iStoreFlags & VM_TYPED_STORE_VIA_REF) != 0;` |
|  1120779 | 2919 | `	int bCloneInit = (iStoreFlags & VM_TYPED_STORE_CLONE_INIT) != 0;` |
|        - | 2920 | `	SyHashEntry *pSlot;` |
|        - | 2921 | `	VmClassAttr *pVmAttr;` |
|        - | 2922 | `	ph7_class_attr *pAttr;` |
|        - | 2923 | `	ph7_class *pHintScope;` |
|        - | 2924 | `	char zGivenBuf[128];` |
|        - | 2925 | `	/* php decides a typed-property store by the strict_types mode of the file the` |
|        - | 2926 | `	 * ASSIGNMENT sits in — not the class's — which is what the executing` |
|        - | 2927 | `	 * instruction's own unit mode says (pVm->bCurStrict, published under the` |
|        - | 2928 | `	 * nLine != 0 gate so an engine-dispatched write keeps the calling file's). */` |
|  1120779 | 2929 | `	int bStrict = pVm->bCurStrict ? 1 : 0;` |
|  1120779 | 2930 | `	if( !PH7_VM_STORE_FILTERED(pVm,nIdx) ){` |
|  1019033 | 2931 | `		return SXRET_OK; /* Not a filtered slot -- the common answer, and free */` |
|        - | 2932 | `	}` |
|  1064993 | 2933 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|  1064993 | 2934 | `	if( pSlot == 0 ){` |
|      ! 0 | 2935 | `		return SXRET_OK; /* Not a typed slot */` |
|        - | 2936 | `	}` |
|   101751 | 2937 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   101751 | 2938 | `	pAttr = pVmAttr->pAttr;` |
|   101751 | 2939 | `	if( pAttr == 0 ){` |
|      ! 0 | 2940 | `		return SXRET_OK;` |
|        - | 2941 | `	}` |
|   101751 | 2942 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|        - | 2943 | `		/* php's write_property handler for this class refuses outright, and its` |
|        - | 2944 | `		 * sentence is the readonly one -- without the readonly FLAG, which is why` |
|        - | 2945 | `		 * Reflection still reports isReadOnly() false for DatePeriod's seven. The` |
|        - | 2946 | `		 * C bodies that fill them write the slot directly and never come here. */` |
|       34 | 2947 | `		return VmThrowNativeNoWrite(pVm,PH7_VmAttrOwner(pVmAttr),pAttr);` |
|        - | 2948 | `	}` |
|   101721 | 2949 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET ){` |
|      615 | 2950 | `		sxi32 rcNat = VmRunNativeSet(pVm,pVmAttr,pValue);` |
|      615 | 2951 | `		if( rcNat != SXRET_OK ){` |
|        7 | 2952 | `			return rcNat;` |
|        - | 2953 | `		}` |
|      303 | 2954 | `	}` |
|   101715 | 2955 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      609 | 2956 | `		return SXRET_OK;` |
|        - | 2957 | `	}` |
|        - | 2958 | ``	/* `self`/`parent` in the declared type resolve against the class that DECLARED`` |
|        - | 2959 | `	 * the property (a trait's members count as the composing class), not the` |
|        - | 2960 | `	 * instance's runtime class — see VmHintScopeClass. */` |
|   101109 | 2961 | `	pHintScope = VmHintScopeClass(pVm,pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        - | 2962 | `	/* readonly enforcement (PHP 8.1), checked before type coercion. A readonly` |
|        - | 2963 | `	 * property may be written exactly once and only from within the declaring` |
|        - | 2964 | `	 * class scope (its set-scope is protected). */` |
|   101109 | 2965 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 2966 | `		/* A readonly property is always typed and default-less, so it starts` |
|        - | 2967 | `		 * VM_CLASS_ATTR_UNINIT and that flag is cleared only by a *successful*` |
|        - | 2968 | `		 * write below — making it the write-once latch (a type-rejected write` |
|        - | 2969 | `		 * leaves it set, so a later valid initialization still works). */` |
|      157 | 2970 | `		if( !bCloneInit && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|        - | 2971 | `			/* Already initialized: any further write is forbidden, any scope —` |
|        - | 2972 | `			 * checked BEFORE the set-visibility scope, matching php's order.` |
|        - | 2973 | `			 * Exceptions that fall through to the set-scope check below:` |
|        - | 2974 | `			 *   - PHP 8.5 clone($o,[...]) with-updates (bCloneInit), and` |
|        - | 2975 | `			 *   - PHP 8.3 __clone(): the object under clone (the executing $this,` |
|        - | 2976 | `			 *     flagged VM_INSTANCE_CLONING) may re-initialize its readonly props. */` |
|       47 | 2977 | `			VmFrame *pCloneFr = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|       47 | 2978 | `			if( !(pCloneFr && pCloneFr->pThis` |
|       23 | 2979 | `				&& (pCloneFr->pThis->iFlags & VM_INSTANCE_CLONING)) ){` |
|       45 | 2980 | `				return VmThrowReadonlyError(pVm,PH7_VmAttrOwner(pVmAttr),pAttr,1);` |
|        - | 2981 | `			}` |
|        1 | 2982 | `		}` |
|       56 | 2983 | `	}` |
|   101069 | 2984 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|        - | 2985 | `		/* Asymmetric set-visibility (PHP 8.4): EVERY write is scope-checked.` |
|        - | 2986 | `		 * An explicit set-visibility replaces readonly's implicit protected(set). */` |
|       31 | 2987 | `		sxi32 rcVis = VmCheckSetVisibility(pVm,PH7_VmAttrOwner(pVmAttr),pAttr);` |
|       31 | 2988 | `		if( rcVis != SXRET_OK ){` |
|       15 | 2989 | `			return rcVis;` |
|        1 | 2990 | `		}` |
|   101047 | 2991 | `	}else if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 2992 | `		/* First write (or a clone re-init) must come from within the declaring` |
|        - | 2993 | `		 * class scope (readonly's set-scope is protected — a subclass may set). */` |
|      115 | 2994 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|      115 | 2995 | `		ph7_class *pActive = VmCurrentSelf(pVm);` |
|        - | 2996 | `		/* A readonly property imported from a TRAIT is, per php, declared in the` |
|        - | 2997 | `		 * USING class (traits are flattened in). The trait is not in any instanceof` |
|        - | 2998 | `		 * hierarchy, so use the composing class (PH7_VmAttrOwner(pVmAttr)) as the set-scope. */` |
|      115 | 2999 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) && PH7_VmAttrOwner(pVmAttr) ){` |
|      ! 0 | 3000 | `			pDecl = PH7_VmAttrOwner(pVmAttr);` |
|      ! 0 | 3001 | `		}` |
|      115 | 3002 | `		if( pActive == 0 \|\| pDecl == 0 \|\| !PH7_VmInstanceOf(pActive,pDecl) ){` |
|        6 | 3003 | `			return VmThrowReadonlyError(pVm,PH7_VmAttrOwner(pVmAttr),pAttr,0);` |
|        - | 3004 | `		}` |
|       53 | 3005 | `	}` |
|        - | 3006 | `	/* Union type: dispatch to the shared coercion helper, under the mode of the` |
|        - | 3007 | `	 * file the ASSIGNMENT is written in — php applies strict_types to a typed` |
|        - | 3008 | `` 	 * property store exactly as to an argument (`$o->u = 1.5` on an `int\|string` `` |
|        - | 3009 | `	 * is its TypeError there), which this used to deny outright. */` |
|   101051 | 3010 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       92 | 3011 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       58 | 3012 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|       29 | 3013 | `			bStrict,pHintScope);` |
|       63 | 3014 | `		if( rc == SXRET_OK ){` |
|       37 | 3015 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       37 | 3016 | `			return SXRET_OK;` |
|        - | 3017 | `		}` |
|       28 | 3018 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 3019 | `			char zBuf[128];` |
|       23 | 3020 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        7 | 3021 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)),bViaRef);` |
|        - | 3022 | `		}` |
|       19 | 3023 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        5 | 3024 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3025 | `	}` |
|        - | 3026 | ``	/* NULL handling: allowed if the type is nullable, or is `mixed` (which`` |
|        - | 3027 | `	 * includes null). */` |
|   100993 | 3028 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       32 | 3029 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE)` |
|       25 | 3030 | `		 \|\| (pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|        2 | 3031 | `		     && SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0) ){` |
|       25 | 3032 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       25 | 3033 | `			return SXRET_OK;` |
|        - | 3034 | `		}` |
|       12 | 3035 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null",bViaRef);` |
|        - | 3036 | `	}` |
|        - | 3037 | ``	/* standalone `null` property type (PHP 8.2): a null value was already`` |
|        - | 3038 | `	 * accepted by the nullable check above, so any non-null value here is a` |
|        - | 3039 | `	 * type error. */` |
|   100961 | 3040 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 3041 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|      ! 0 | 3042 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3043 | `	}` |
|        - | 3044 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - | 3045 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - | 3046 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|   100961 | 3047 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 | 3048 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 | 3049 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|        5 | 3050 | `			return SXRET_OK;` |
|        - | 3051 | `		}` |
|       10 | 3052 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 | 3053 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3054 | `	}` |
|        - | 3055 | ``	/* Pseudo-types stored as class-name atoms: `iterable` (array\|Traversable),`` |
|        - | 3056 | ``	 * `true`/`false` (matching bool), `mixed` (any value — its null case is`` |
|        - | 3057 | `	 * handled by the nullable check above). Checked by value before the generic` |
|        - | 3058 | `	 * class-instanceof branch, which would resolve no such class and then` |
|        - | 3059 | `	 * wrongly accept any object / reject arrays. */` |
|   100951 | 3060 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       93 | 3061 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pAttr->sClass);` |
|       93 | 3062 | `		if( rcPseudo == 1 ){` |
|       13 | 3063 | `			pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       13 | 3064 | `			return SXRET_OK;` |
|        - | 3065 | `		}` |
|       81 | 3066 | `		if( rcPseudo == 0 ){` |
|       11 | 3067 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 | 3068 | `				VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3069 | `		}` |
|        - | 3070 | `		/* rcPseudo == -1: real class — fall through to the instanceof branch. */` |
|       35 | 3071 | `	}` |
|   100933 | 3072 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - | 3073 | `		/* Class / interface type. Resolve self/parent relative to the DECLARING` |
|        - | 3074 | `		 * class (pHintScope), not the instance's runtime class. */` |
|       75 | 3075 | `		ph7_class *pExpected = 0;` |
|       75 | 3076 | `		if( !VmClassHintMatches(pVm,&pAttr->sClass,pHintScope,pValue,&pExpected) ){` |
|        - | 3077 | `			char zBuf[128];` |
|       41 | 3078 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       24 | 3079 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|       18 | 3080 | `					? VmFormatValueClassName(pValue,zBuf,sizeof(zBuf))` |
|       15 | 3081 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3082 | `		}` |
|       51 | 3083 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       51 | 3084 | `		return SXRET_OK;` |
|        - | 3085 | `	}` |
|        - | 3086 | `	/* Scalar type, strict mode: no coercion at all, and the one widening is` |
|        - | 3087 | `	 * int -> float. The flag test the weak path below uses cannot answer this on` |
|        - | 3088 | `	 * its own — an integer-valued real carries MEMOBJ_INT as a cached` |
|        - | 3089 | ``	 * representation, so `$o->i = 5.0` would read as a match — hence the value's`` |
|        - | 3090 | `	 * own type is asked in ph7_type_name()'s order, float before int. */` |
|   100863 | 3091 | `	if( bStrict ){` |
|        - | 3092 | `		int bOk;` |
|       41 | 3093 | `		if( ph7_value_is_bool(pValue) ){` |
|        5 | 3094 | `			bOk = (pAttr->nType == MEMOBJ_BOOL);` |
|       39 | 3095 | `		}else if( ph7_value_is_float(pValue) ){` |
|        6 | 3096 | `			bOk = (pAttr->nType == MEMOBJ_REAL);` |
|       34 | 3097 | `		}else if( ph7_value_is_int(pValue) ){` |
|       18 | 3098 | `			bOk = (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL);` |
|       23 | 3099 | `		}else if( ph7_value_is_string(pValue) ){` |
|       15 | 3100 | `			bOk = (pAttr->nType == MEMOBJ_STRING);` |
|        8 | 3101 | `		}else{` |
|        - | 3102 | `			/* array / resource / an object against a scalar type: no coercion in` |
|        - | 3103 | `			 * either mode, so the flag test is the whole answer (an object never` |
|        - | 3104 | `			 * carries the target's flag, and __toString is a coercion strict mode` |
|        - | 3105 | `			 * does not perform). */` |
|      ! 0 | 3106 | `			bOk = ((pValue->iFlags & pAttr->nType) != 0) && !(pValue->iFlags & MEMOBJ_OBJ);` |
|        - | 3107 | `		}` |
|       41 | 3108 | `		if( !bOk ){` |
|        - | 3109 | `			char zObjBuf[128];` |
|       31 | 3110 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       20 | 3111 | `				(pValue->iFlags & MEMOBJ_OBJ)` |
|      ! 0 | 3112 | `					? VmFormatValueClassName(pValue,zObjBuf,sizeof(zObjBuf))` |
|       20 | 3113 | `					: VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3114 | `		}` |
|       21 | 3115 | `		if( pAttr->nType == MEMOBJ_REAL && !ph7_value_is_float(pValue) ){` |
|        6 | 3116 | `			PH7_MemObjToReal(pValue); /* the int -> float widening */` |
|        4 | 3117 | `		}else{` |
|       16 | 3118 | `			VmMaterializeIntTyped(pValue,pAttr->nType);` |
|        - | 3119 | `		}` |
|       21 | 3120 | `		pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_TYPE_DEFER);` |
|       21 | 3121 | `		return SXRET_OK;` |
|        - | 3122 | `	}` |
|        - | 3123 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - | 3124 | `	 * helpers used by function-argument hints. Reject object→scalar, EXCEPT an` |
|        - | 3125 | ``	 * object with __toString() stored into a `string` property — php coerces it`` |
|        - | 3126 | `	 * via __toString, so fall through to the string cast below. */` |
|   100825 | 3127 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       15 | 3128 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       17 | 3129 | `		if( !(pAttr->nType == MEMOBJ_STRING && pInst && pInst->pClass` |
|        4 | 3130 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|        - | 3131 | `			char zBuf[128];` |
|       19 | 3132 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        6 | 3133 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)),bViaRef);` |
|        - | 3134 | `		}` |
|        1 | 3135 | `	}` |
|        - | 3136 | ``	/* An `int` slot takes the same lossy refusal a typed PARAMETER and a return`` |
|        - | 3137 | `	 * take (VmCoerceScalarWeak's own rule, the scope policy) -- and it had none of it, so this` |
|        - | 3138 | `` 	 * one weak path was storing a number the script never wrote: `$o->i = 1.9` `` |
|        - | 3139 | ``	 * stored 1 in silence, `$o->i = 1e20` stored PHP_INT_MIN, and`` |
|        - | 3140 | ``	 * `$o->i = "99999999999999999999"` stored PHP_INT_MAX. php refuses the last`` |
|        - | 3141 | ``	 * two outright (`Cannot assign float to property C::$i of type int`) and`` |
|        - | 3142 | `	 * deprecates the first; PHL refuses all three, with the message the other two` |
|        - | 3143 | `	 * write-sites already use. Asked before the cast branches below, so a value` |
|        - | 3144 | `	 * that arrives carrying a cached int representation is asked too. */` |
|   100813 | 3145 | `	if( pAttr->nType == MEMOBJ_INT && VmValueIsLossyToInt(pValue) ){` |
|       38 | 3146 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|       12 | 3147 | `			VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3148 | `	}` |
|   100789 | 3149 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|   100107 | 3150 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|   100107 | 3151 | `		if( xCast ){` |
|        - | 3152 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|   100107 | 3153 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       15 | 3154 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        4 | 3155 | `					VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3156 | `			}` |
|   100099 | 3157 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|       18 | 3158 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        5 | 3159 | `					VmValueGivenName(pValue,zGivenBuf,sizeof(zGivenBuf)),bViaRef);` |
|        - | 3160 | `			}` |
|        - | 3161 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - | 3162 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - | 3163 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|   100084 | 3164 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|   100078 | 3165 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|   100082 | 3166 | `			 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|   100048 | 3167 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string",bViaRef);` |
|        - | 3168 | `			}` |
|       44 | 3169 | `			xCast(pValue);` |
|       20 | 3170 | `		}` |
|       24 | 3171 | `	}else{` |
|        - | 3172 | `		/* Mask matched — an int property accepting a whole-real must` |
|        - | 3173 | `		 * materialize it as a genuine int (php: $o->i = 1.0 stores int(1)). */` |
|      687 | 3174 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|        - | 3175 | `	}` |
|      727 | 3176 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      727 | 3177 | `	return SXRET_OK;` |
|   561463 | 3178 | `}` |
|        - | 3179 | `/*` |
|        - | 3180 | ` * Apply ONE property update from a PHP 8.5 clone($obj, [ 'name' => value, ... ])` |
|        - | 3181 | ` * to the freshly-produced clone. The write happens in the caller's scope, so:` |
|        - | 3182 | ` *   - visibility is enforced (a private/protected property is only writable from` |
|        - | 3183 | ` *     a scope that could normally reach it — else a catchable Error),` |
|        - | 3184 | ` *   - a readonly property may be RE-initialized here (bCloneInit) provided the` |
|        - | 3185 | ` *     caller's scope could set it (else the readonly set-scope Error),` |
|        - | 3186 | ` *   - typed properties are coerced/validated exactly as a normal store, and` |
|        - | 3187 | ` *   - an unknown name creates a dynamic property (PHP still creates it; the 8.2` |
|        - | 3188 | ` *     deprecation notice is not emitted yet).` |
|        - | 3189 | ` * Returns SXRET_OK, or PH7_EXCEPTION/PH7_ABORT (already thrown) on failure.` |
|        - | 3190 | ` */` |
|       30 | 3191 | `PH7_PRIVATE sxi32 VmCloneApplyUpdate(ph7_vm *pVm,ph7_class_instance *pClone,` |
|        - | 3192 | `	const char *zName,sxu32 nName,ph7_value *pValue)` |
|        1 | 3193 | `{` |
|       31 | 3194 | `	ph7_class *pClass = pClone->pClass;` |
|        - | 3195 | `	SyHashEntry *pEntry;` |
|        - | 3196 | `	VmClassAttr *pVmAttr;` |
|        - | 3197 | `	ph7_class_attr *pAttr;` |
|        - | 3198 | `	ph7_value *pSlot;` |
|        - | 3199 | `	sxi32 rc;` |
|       31 | 3200 | `	pEntry = (nName > 0) ? SyHashGet(&pClone->hAttr,(const void *)zName,nName) : 0;` |
|       31 | 3201 | `	if( pEntry == 0 ){` |
|        - | 3202 | `		/* Unknown property: PHP creates a dynamic property (deprecated on a class` |
|        - | 3203 | `		 * without #[AllowDynamicProperties], but still created — the notice is a` |
|        - | 3204 | `		 * deferred residual). */` |
|      ! 0 | 3205 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pClone,zName,nName,0);` |
|      ! 0 | 3206 | `		if( pSlot == 0 ){` |
|      ! 0 | 3207 | `			return PH7_VmMemoryError(pVm);` |
|        - | 3208 | `		}` |
|      ! 0 | 3209 | `		PH7_MemObjStore(pValue,pSlot);` |
|      ! 0 | 3210 | `		return SXRET_OK;` |
|        - | 3211 | `	}` |
|       31 | 3212 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       31 | 3213 | `	pAttr = pVmAttr->pAttr;` |
|        - | 3214 | `	/* Static / class-constant "properties" are not per-instance state. */` |
|       31 | 3215 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|        - | 3216 | `		SyBlob sMsg;` |
|      ! 0 | 3217 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      ! 0 | 3218 | `		SyBlobFormat(&sMsg,"Cannot update static property %z::$%z via clone()",` |
|      ! 0 | 3219 | `			&pClass->sDisp,&pAttr->sName);` |
|      ! 0 | 3220 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 3221 | `	}` |
|        - | 3222 | `	/* Visibility: enforced against the current (calling) frame's scope. A public` |
|        - | 3223 | `	 * readonly property passes here and is handled by the readonly set-scope check` |
|        - | 3224 | `	 * inside VmEnforcePropertyTypeOnStore below. */` |
|       31 | 3225 | `	if( !PH7_VmClassMemberAccess(pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        5 | 3226 | `		const char *zProt = (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected";` |
|        5 | 3227 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,PH7_VmAttrOwner(pVmAttr));` |
|        - | 3228 | `		SyBlob sMsg;` |
|        5 | 3229 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 3230 | `		SyBlobFormat(&sMsg,"Cannot access %s property %z::$%z",zProt,&pOwner->sDisp,&pAttr->sName);` |
|        5 | 3231 | `		return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        - | 3232 | `	}` |
|        - | 3233 | `	/* Typed + readonly (clone re-init) enforcement — may coerce pValue in place. */` |
|       27 | 3234 | `	rc = VmEnforcePropertyTypeOnStore(pVm,pVmAttr->nIdx,pValue,VM_TYPED_STORE_CLONE_INIT);` |
|       27 | 3235 | `	if( rc != SXRET_OK ){` |
|        3 | 3236 | `		return rc;` |
|        - | 3237 | `	}` |
|        - | 3238 | `	/* Commit the (possibly coerced) value into the property slot. */` |
|       25 | 3239 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|       25 | 3240 | `	if( pSlot ){` |
|       25 | 3241 | `		PH7_MemObjStore(pValue,pSlot);` |
|       12 | 3242 | `	}` |
|       25 | 3243 | `	return SXRET_OK;` |
|       16 | 3244 | `}` |
|        - | 3245 | `/*` |
|        - | 3246 | ` * Raise the non-catchable fatal PHP emits when a typed class constant is given` |
|        - | 3247 | ` * a value incompatible with its declared type. Mirrors PH7_VmMemoryError: it` |
|        - | 3248 | ` * prints the diagnostic, sets a nonzero exit status, requests a clean halt and` |
|        - | 3249 | ` * returns PH7_ABORT (so the caller unwinds and shutdown callbacks still run).` |
|        - | 3250 | ` */` |
|       12 | 3251 | `static sxi32 VmConstantTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|        3 | 3252 | `{` |
|       15 | 3253 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 3254 | `	char zBuf[128],zType[192];` |
|        - | 3255 | `	const char *zGiven;` |
|       21 | 3256 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|        6 | 3257 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|       15 | 3258 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3259 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 | 3260 | `	}else{` |
|       15 | 3261 | `		zGiven = ph7_type_name(pValue);` |
|        - | 3262 | `	}` |
|       15 | 3263 | `	if( bLazy ){` |
|        - | 3264 | `		/* The value only materialized at the first ACCESS, because the initializer` |
|        - | 3265 | `		 * could not be evaluated at the declaration (it named a constant that did` |
|        - | 3266 | `		 * not exist yet). php reports THAT with its own wording and its own kind:` |
|        - | 3267 | ``		 * a CATCHABLE `TypeError: Cannot assign string to class constant C::K of`` |
|        - | 3268 | ``		 * type int`, raised at the access site — not the declaration-time fatal`` |
|        - | 3269 | `		 * below. The caller leaves the slot unmaterialized, so every later access` |
|        - | 3270 | `		 * re-evaluates and re-raises, as php's does. */` |
|        - | 3271 | `		SyBlob sMsg;` |
|        5 | 3272 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        5 | 3273 | `		SyBlobFormat(&sMsg,"Cannot assign %s to class constant %z::%z of type %s",` |
|        2 | 3274 | `			zGiven,&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|        5 | 3275 | `		return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        - | 3276 | `	}` |
|        - | 3277 | `	/* A class is normally mounted during the compile/VmMakeReady phase, where the` |
|        - | 3278 | `	 * code-generator's error consumer is active but the host VM output consumer is` |
|        - | 3279 | `	 * not yet installed — so the diagnostic is routed through PH7_GenCompileError,` |
|        - | 3280 | `	 * matching the other compile-time fatals ("PHP Fatal error:  ... in F on line N").` |
|        - | 3281 | `	 * A class declared at runtime inside plain eval() reaches here with the codegen` |
|        - | 3282 | `	 * consumer cleared (VmEvalChunk nulls it); fall back to the VM output consumer` |
|        - | 3283 | `	 * so the fatal is still reported rather than the program halting silently. */` |
|       11 | 3284 | `	if( pVm->sCodeGen.xErr ){` |
|       11 | 3285 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,pAttr->nLine,` |
|        - | 3286 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|        3 | 3287 | `			zGiven,&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|        5 | 3288 | `	}else{` |
|        4 | 3289 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - | 3290 | `			"Cannot use %s as value for class constant %z::%z of type %s",` |
|        1 | 3291 | `			zGiven,&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|        - | 3292 | `	}` |
|       11 | 3293 | `	pVm->iExitStatus = 255;` |
|       11 | 3294 | `	pVm->bHaltRequested = 1;` |
|       11 | 3295 | `	return SXERR_ABORT;` |
|        9 | 3296 | `}` |
|        - | 3297 | `/*` |
|        - | 3298 | ` * Enforce a typed class constant's value against its declared type (PHP 8.3).` |
|        - | 3299 | ` * Unlike typed properties (weak mode), constants are checked strictly: the only` |
|        - | 3300 | `` * implicit coercion allowed is int -> float widening (so `const float X = 1` is`` |
|        - | 3301 | `` * accepted but `const int X = "5"` is not), matching PHP. On entry pValue holds`` |
|        - | 3302 | ` * the computed constant value (it may be widened in place). Returns SXRET_OK on` |
|        - | 3303 | ` * accept, or PH7_ABORT after raising the non-catchable fatal on mismatch.` |
|        - | 3304 | ` */` |
|       76 | 3305 | `PH7_PRIVATE sxi32 VmEnforceConstantType(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,int bLazy)` |
|        5 | 3306 | `{` |
|       81 | 3307 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|        - | 3308 | ``	/* NULL value: allowed only for nullable, standalone `null`, or `mixed`. */`` |
|       81 | 3309 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 | 3310 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|        3 | 3311 | `			return SXRET_OK;` |
|        - | 3312 | `		}` |
|      ! 0 | 3313 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|      ! 0 | 3314 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|      ! 0 | 3315 | `			return SXRET_OK;` |
|        - | 3316 | `		}` |
|      ! 0 | 3317 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3318 | `	}` |
|        - | 3319 | `	/* Union type: reuse the shared coercion helper in strict mode. */` |
|       79 | 3320 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       18 | 3321 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|       15 | 3322 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|       13 | 3323 | `			return SXRET_OK;` |
|        - | 3324 | `		}` |
|        3 | 3325 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3326 | `	}` |
|        - | 3327 | ``	/* standalone `null` type: a non-null value is a mismatch. */`` |
|       66 | 3328 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 3329 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3330 | `	}` |
|        - | 3331 | ``	/* Bare `object` type: any class instance, nothing else. */`` |
|       66 | 3332 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 | 3333 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3334 | `			return SXRET_OK;` |
|        - | 3335 | `		}` |
|      ! 0 | 3336 | `		return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3337 | `	}` |
|        - | 3338 | `	/* Class-name atom: pseudo-types (mixed/true/false/iterable) by value, else` |
|        - | 3339 | `	 * a real class/interface verified by instanceof. */` |
|       66 | 3340 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        3 | 3341 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|        3 | 3342 | `		if( rcPseudo == 1 ){` |
|      ! 0 | 3343 | `			return SXRET_OK;` |
|        - | 3344 | `		}` |
|        3 | 3345 | `		if( rcPseudo == 0 ){` |
|      ! 0 | 3346 | `			return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3347 | `		}` |
|        - | 3348 | `		/* rcPseudo == -1: a real class/interface type. A class constant's` |
|        - | 3349 | `		 * self/parent resolve against the declaring class. */` |
|        - | 3350 | `		{` |
|        3 | 3351 | `			ph7_class *pExpected = 0;` |
|        4 | 3352 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|        1 | 3353 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|        3 | 3354 | `				return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|        - | 3355 | `			}` |
|        - | 3356 | `		}` |
|      ! 0 | 3357 | `		return SXRET_OK;` |
|        - | 3358 | `	}` |
|        - | 3359 | `	/* Scalar type, strict: an exact flag match, or the single int -> float` |
|        - | 3360 | `	 * implicit widening. Everything else is a type error.` |
|        - | 3361 | `	 *` |
|        - | 3362 | `	 * Known lenient divergence: PHL's number model leaves a whole-valued real` |
|        - | 3363 | ``	 * flagged MEMOBJ_REAL\|MEMOBJ_INT, so a computed whole-real (e.g. `1.0 + 0.0`,`` |
|        - | 3364 | `` 	 * or the evenly-dividing `4/2` — `/` always yields a real) satisfies a `: int` `` |
|        - | 3365 | ``	 * constant here. PHP accepts `const int X = 4/2` (its `/` yields a genuine int)`` |
|        - | 3366 | ``	 * but rejects `const int X = 1.0 + 0.0`; PHL cannot tell them apart by flag, so`` |
|        - | 3367 | ``	 * it accepts both rather than rejecting the valid `4/2`. The common bare-literal`` |
|        - | 3368 | ``	 * case `const int X = 1.0` is caught earlier, at definition time, by the`` |
|        - | 3369 | `	 * syntactic check in GenStateCompileClassConstant (compile.c) — the literal` |
|        - | 3370 | ``	 * shape is the only reliable signal. A fractional real (`1.5`, MEMOBJ_REAL only)`` |
|        - | 3371 | `	 * carries no MEMOBJ_INT and is correctly rejected here. Tightening the computed` |
|        - | 3372 | `	 * residual needs PHL's float-identity/division model, which is out of scope. */` |
|       64 | 3373 | `	if( pValue->iFlags & pAttr->nType ){` |
|       42 | 3374 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|       42 | 3375 | `		return SXRET_OK;` |
|        - | 3376 | `	}` |
|       24 | 3377 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|       14 | 3378 | `		PH7_MemObjToReal(pValue);` |
|       14 | 3379 | `		return SXRET_OK;` |
|        - | 3380 | `	}` |
|       11 | 3381 | `	return VmConstantTypeError(&(*pVm),pClass,pAttr,pValue,bLazy);` |
|       43 | 3382 | `}` |
|        - | 3383 | `/*` |
|        - | 3384 | ` * php's CATCHABLE TypeError for a typed property DEFAULT whose computed value` |
|        - | 3385 | ` * does not match the declared type: "Cannot assign <kind> to property` |
|        - | 3386 | ` * C::$p of type T". Same value-kind naming as the constant fatal above.` |
|        - | 3387 | ` * Returns PH7_ABORT or PH7_EXCEPTION (VmThrowBuiltinError's protocol).` |
|        - | 3388 | ` */` |
|       50 | 3389 | `static sxi32 VmDefaultPropertyTypeError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        4 | 3390 | `{` |
|       54 | 3391 | `	ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 3392 | `	const char *zGiven;` |
|        - | 3393 | `	char zBuf[128],zType[192];` |
|       79 | 3394 | `	const char *zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|       25 | 3395 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|        - | 3396 | `	SyBlob sMsg;` |
|       54 | 3397 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3398 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 | 3399 | `	}else{` |
|       54 | 3400 | `		zGiven = ph7_type_name(pValue);` |
|        - | 3401 | `	}` |
|       54 | 3402 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       54 | 3403 | `	SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %s",` |
|       25 | 3404 | `		zGiven,&pOwner->sDisp,&pAttr->sName,zTypeText);` |
|       54 | 3405 | `	return VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|        4 | 3406 | `}` |
|        - | 3407 | `/*` |
|        - | 3408 | ` * Enforce a typed INSTANCE property's computed DEFAULT value against its` |
|        - | 3409 | ` * declared type at instantiation time. php applies the typed-CONSTANT rule` |
|        - | 3410 | ` * here, not the weak store rule: the only implicit coercion is int -> float` |
|        - | 3411 | `` * widening — `public int $p = "5"` is a TypeError even in weak mode — but`` |
|        - | 3412 | ` * unlike a typed constant the failure is a CATCHABLE TypeError raised when` |
|        - | 3413 | `` * the default is materialized (at `new`), php-exact since PHL evaluates`` |
|        - | 3414 | ` * instance defaults per-instantiation. Matching structure of` |
|        - | 3415 | ` * VmEnforceConstantType above; only the throw differs (catchable, property` |
|        - | 3416 | ` * wording). The whole-real dual-flag leniency applies here too (php rejects` |
|        - | 3417 | `` * `public int $p = FLC` with FLC = 2.0; PHL cannot tell FLC's shape from a`` |
|        - | 3418 | ` * php-int-producing builtin, so it accepts-and-materializes — recorded).` |
|        - | 3419 | ` * Returns SXRET_OK, or PH7_ABORT/PH7_EXCEPTION after throwing.` |
|        - | 3420 | ` */` |
|        - | 3421 | `/*` |
|        - | 3422 | ` * The CHECK core of typed-default enforcement: validate (and possibly coerce` |
|        - | 3423 | ` * in place — int -> float widening, whole-real materialization) a computed` |
|        - | 3424 | ` * DEFAULT value against the property's declared type using the typed-CONSTANT` |
|        - | 3425 | ` * rule. Returns SXRET_OK on accept or SXERR_INVALID on mismatch WITHOUT` |
|        - | 3426 | ` * throwing, so the static-property mount path can defer the failure (php` |
|        - | 3427 | ` * evaluates static defaults lazily — see VM_CLASS_ATTR_TYPE_DEFER) while the` |
|        - | 3428 | ` * instance path throws immediately via the wrapper below.` |
|        - | 3429 | ` */` |
|     1968 | 3430 | `PH7_PRIVATE sxi32 VmCheckTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        5 | 3431 | `{` |
|     1973 | 3432 | `	int bNullable = (pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0;` |
|     1973 | 3433 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|      251 | 3434 | `		if( bNullable \|\| pAttr->nType == MEMOBJ_NULL ){` |
|      235 | 3435 | `			return SXRET_OK;` |
|        - | 3436 | `		}` |
|       16 | 3437 | `		if( pAttr->nType == SXU32_HIGH && pAttr->sClass.nByte == 5` |
|       15 | 3438 | `			&& SyStrnicmp(pAttr->sClass.zString,"mixed",5) == 0 ){` |
|       14 | 3439 | `			return SXRET_OK;` |
|        - | 3440 | `		}` |
|        5 | 3441 | `		return SXERR_INVALID;` |
|        - | 3442 | `	}` |
|     1727 | 3443 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|      180 | 3444 | `		if( VmCoerceToUnion(&(*pVm),pValue,&pAttr->aUnionAlts,bNullable,1 /* strict */,` |
|      125 | 3445 | `			VmHintScopeClass(pVm,pAttr->pDeclClass,pClass)) == SXRET_OK ){` |
|      125 | 3446 | `			return SXRET_OK;` |
|        - | 3447 | `		}` |
|      ! 0 | 3448 | `		return SXERR_INVALID;` |
|        - | 3449 | `	}` |
|     1607 | 3450 | `	if( pAttr->nType == MEMOBJ_NULL ){` |
|      ! 0 | 3451 | `		return SXERR_INVALID;` |
|        - | 3452 | `	}` |
|     1607 | 3453 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|      ! 0 | 3454 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 3455 | `			return SXRET_OK;` |
|        - | 3456 | `		}` |
|      ! 0 | 3457 | `		return SXERR_INVALID;` |
|        - | 3458 | `	}` |
|     1607 | 3459 | `	if( pAttr->nType == SXU32_HIGH ){` |
|       26 | 3460 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pValue,&pAttr->sClass);` |
|       26 | 3461 | `		if( rcPseudo == 1 ){` |
|       26 | 3462 | `			return SXRET_OK;` |
|        - | 3463 | `		}` |
|      ! 0 | 3464 | `		if( rcPseudo == 0 ){` |
|      ! 0 | 3465 | `			return SXERR_INVALID;` |
|        - | 3466 | `		}` |
|        - | 3467 | `		{` |
|        - | 3468 | `			/* self/parent in the hint resolve against the declaring class. */` |
|      ! 0 | 3469 | `			ph7_class *pExpected = 0;` |
|      ! 0 | 3470 | `			if( !VmClassHintMatches(pVm,&pAttr->sClass,` |
|      ! 0 | 3471 | `				VmHintScopeClass(pVm,pAttr->pDeclClass,pClass),pValue,&pExpected) ){` |
|      ! 0 | 3472 | `				return SXERR_INVALID;` |
|        - | 3473 | `			}` |
|        - | 3474 | `		}` |
|      ! 0 | 3475 | `		return SXRET_OK;` |
|        - | 3476 | `	}` |
|     1583 | 3477 | `	if( pValue->iFlags & pAttr->nType ){` |
|     1461 | 3478 | `		VmMaterializeIntTyped(pValue,pAttr->nType);` |
|     1461 | 3479 | `		return SXRET_OK;` |
|        - | 3480 | `	}` |
|      126 | 3481 | `	if( pAttr->nType == MEMOBJ_REAL && (pValue->iFlags & MEMOBJ_INT) ){` |
|       76 | 3482 | `		PH7_MemObjToReal(pValue);` |
|       76 | 3483 | `		return SXRET_OK;` |
|        - | 3484 | `	}` |
|       54 | 3485 | `	return SXERR_INVALID;` |
|      989 | 3486 | `}` |
|        - | 3487 | `/*` |
|        - | 3488 | ` * php's COMPILE-time refusal of a typed property default (zend_compile_prop_decl):` |
|        - | 3489 | ` * a default its compiler FOLDED to a value is held to the same rule as above then` |
|        - | 3490 | `` * and there, so `public int $x = "a";` stops the file before a line of it runs,`` |
|        - | 3491 | ` * whether or not the class is ever instantiated. What did not fold (a constant` |
|        - | 3492 | ` * name, a class constant) is left to the instantiation-time TypeError.` |
|        - | 3493 | ` *` |
|        - | 3494 | `` * Two sentences, and php's own value naming: a bool is `true`/`false` here, where`` |
|        - | 3495 | `` * the TypeError says `bool`. A null default php explains with the nullable spelling`` |
|        - | 3496 | `` * of the type (`?int`, `string\|int\|null`) -- except on a pure intersection, which`` |
|        - | 3497 | ` * has no such spelling and takes the ordinary sentence.` |
|        - | 3498 | ` * Returns TRUE with the sentence appended to pMsg, or FALSE when the value fits.` |
|        - | 3499 | ` */` |
|      622 | 3500 | `PH7_PRIVATE int VmTypedDefaultRefusal(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue,SyBlob *pMsg)` |
|        5 | 3501 | `{` |
|        - | 3502 | `	char zType[192];` |
|        - | 3503 | `	const char *zTypeText, *z;` |
|      627 | 3504 | `	int bUnion = 0, bInter = 0;` |
|      627 | 3505 | `	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){` |
|      619 | 3506 | `		return 0;` |
|        - | 3507 | `	}` |
|       16 | 3508 | `	zTypeText = VmHintTextResolved(pVm,&pAttr->sTypeName,` |
|        4 | 3509 | `		VmHintScopeDeclared(pAttr->pDeclClass),zType,sizeof(zType));` |
|       50 | 3510 | `	for( z = zTypeText ; *z ; ++z ){` |
|       42 | 3511 | `		if( *z == '\|' ){` |
|        3 | 3512 | `			bUnion = 1;` |
|       41 | 3513 | `		}else if( *z == '&' ){` |
|      ! 0 | 3514 | `			bInter = 1;` |
|      ! 0 | 3515 | `		}` |
|       23 | 3516 | `	}` |
|       12 | 3517 | `	if( (pValue->iFlags & MEMOBJ_NULL) && (bUnion \|\| !bInter) ){` |
|        4 | 3518 | `		SyBlobFormat(pMsg,"Default value for property of type %s may not be null. "` |
|        - | 3519 | `			"Use the nullable type %s%s%s to allow null default value",` |
|        1 | 3520 | `			zTypeText,bUnion ? "" : "?",zTypeText,bUnion ? "\|null" : "");` |
|        2 | 3521 | `	}else{` |
|       12 | 3522 | `		const char *zGiven = (pValue->iFlags & MEMOBJ_BOOL)` |
|        6 | 3523 | `			? (pValue->x.iVal ? "true" : "false") : ph7_type_name(pValue);` |
|       12 | 3524 | `		SyBlobFormat(pMsg,"Cannot use %s as default value for property %z::$%z of type %s",` |
|        3 | 3525 | `			zGiven,&pClass->sDisp,&pAttr->sName,zTypeText);` |
|        - | 3526 | `	}` |
|       12 | 3527 | `	return 1;` |
|      316 | 3528 | `}` |
|        - | 3529 | `/*` |
|        - | 3530 | ` * One declared type atom against a FOLDED default, php's ZEND_TYPE_CONTAINS_CODE:` |
|        - | 3531 | ` * the value's own type is in the atom, and nothing coerces. No predicate runs --` |
|        - | 3532 | `` * `callable` holds no folded value (php: `callable $f = "strlen"` is refused), and a`` |
|        - | 3533 | ` * class holds only an object, which a folded default never is.` |
|        - | 3534 | ` */` |
|    25533 | 3535 | `static int VmArgDefaultAtomHolds(sxu32 nType,const SyString *pClass,ph7_value *pValue)` |
|        5 | 3536 | `{` |
|    25538 | 3537 | `	if( nType == SXU32_HIGH ){` |
|       24 | 3538 | `		const char *z = pClass->zString;` |
|       24 | 3539 | `		sxu32 n = pClass->nByte;` |
|       24 | 3540 | `		if( n == 5 && SyStrnicmp(z,"mixed",5) == 0 ){` |
|        3 | 3541 | `			return 1;` |
|        - | 3542 | `		}` |
|       22 | 3543 | `		if( (n == 4 && SyStrnicmp(z,"true",4) == 0) \|\| (n == 5 && SyStrnicmp(z,"false",5) == 0) ){` |
|        6 | 3544 | `			return (pValue->iFlags & MEMOBJ_BOOL) && (pValue->x.iVal != 0) == (n == 4);` |
|        - | 3545 | `		}` |
|       18 | 3546 | `		if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|        8 | 3547 | `			return (pValue->iFlags & MEMOBJ_HASHMAP) != 0;` |
|        - | 3548 | `		}` |
|       11 | 3549 | `		return 0;` |
|        - | 3550 | `	}` |
|    25518 | 3551 | `	if( nType == MEMOBJ_OBJ \|\| nType == MEMOBJ_NULL ){` |
|      ! 0 | 3552 | `		return (pValue->iFlags & nType) != 0;` |
|        - | 3553 | `	}` |
|    25518 | 3554 | `	if( (pValue->iFlags & MEMOBJ_BOOL) && (nType & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 | 3555 | `		return 0;` |
|        - | 3556 | `	}` |
|    25539 | 3557 | `	return (pValue->iFlags & nType) != 0` |
|    25513 | 3558 | `		\|\| ((nType & MEMOBJ_REAL) && (pValue->iFlags & MEMOBJ_INT));` |
|    12754 | 3559 | `}` |
|        - | 3560 | `/*` |
|        - | 3561 | ` * php's COMPILE-time refusal of a parameter default (zend_compile_params): a` |
|        - | 3562 | ` * default its compiler FOLDED is held to the declared type there and then, so` |
|        - | 3563 | `` * `function f(int $x = "a")` stops the file before a line of it runs, whether or`` |
|        - | 3564 | ` * not f is ever called. The rule is the property's, the type's own codes plus int` |
|        - | 3565 | `` * for a float; the sentence names a bool `bool`, and a self/parent by the class it`` |
|        - | 3566 | ` * resolves to (a trait's keeps the keyword). A null default reaches here only from a` |
|        - | 3567 | ` * PROMOTED parameter: a plain one php makes implicitly nullable instead.` |
|        - | 3568 | ` * Returns TRUE with the sentence appended to pMsg, or FALSE when the value fits.` |
|        - | 3569 | ` */` |
|    25523 | 3570 | `PH7_PRIVATE int VmArgDefaultRefusal(ph7_vm *pVm,ph7_class *pScope,ph7_vm_func_arg *pArg,ph7_value *pValue,SyBlob *pMsg)` |
|        5 | 3571 | `{` |
|        - | 3572 | `	char zType[192];` |
|        - | 3573 | `	const char *zTypeText;` |
|    25528 | 3574 | `	int bFits = 0;` |
|    25528 | 3575 | `	if( (pValue->iFlags & MEMOBJ_NULL) && (pArg->iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        3 | 3576 | `		bFits = 1;` |
|    25527 | 3577 | `	}else if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       27 | 3578 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pArg->aUnionAlts);` |
|        - | 3579 | `		sxu32 n;` |
|       61 | 3580 | `		for( n = 0 ; n < SySetUsed(&pArg->aUnionAlts) && !bFits ; ++n ){` |
|       39 | 3581 | `			bFits = VmArgDefaultAtomHolds(aAlt[n].nType,&aAlt[n].sClass,pValue);` |
|       22 | 3582 | `		}` |
|       16 | 3583 | `	}else{` |
|    25504 | 3584 | `		bFits = VmArgDefaultAtomHolds(pArg->nType,&pArg->sClass,pValue);` |
|        - | 3585 | `	}` |
|    25528 | 3586 | `	if( bFits ){` |
|    25512 | 3587 | `		return 0;` |
|        - | 3588 | `	}` |
|       28 | 3589 | `	zTypeText = VmHintTextResolved(pVm,&pArg->sTypeName,VmHintScopeDeclared(pScope),` |
|        8 | 3590 | `		zType,sizeof(zType));` |
|       28 | 3591 | `	SyBlobFormat(pMsg,"Cannot use %s as default value for parameter $%z of type %s",` |
|        8 | 3592 | `		ph7_type_name(pValue),&pArg->sName,zTypeText);` |
|       20 | 3593 | `	return 1;` |
|    12749 | 3594 | `}` |
|        - | 3595 | `/*` |
|        - | 3596 | ` * TRUE when php CONVERTS a folded parameter default it accepted: the one widening` |
|        - | 3597 | ` * zend_is_valid_default_value makes, an int let into a type that lists float but not` |
|        - | 3598 | `` * int (`float $x = 1`, `float\|string $x = 1`, `bool\|float $x = 4`). php stores the`` |
|        - | 3599 | `` * converted value, so getDefaultValue() reads float(1) and the export prints `1.0`;`` |
|        - | 3600 | `` * `int\|float $x = 1` keeps the int, since the type holds it as written.`` |
|        - | 3601 | ` */` |
|    25507 | 3602 | `PH7_PRIVATE int VmArgDefaultWidens(ph7_vm_func_arg *pArg,ph7_value *pValue)` |
|        5 | 3603 | `{` |
|        - | 3604 | `	const ph7_type_alt *aAlt;` |
|        - | 3605 | `	ph7_type_alt sOne;` |
|        - | 3606 | `	sxu32 nAlt,n;` |
|    25512 | 3607 | `	int bReal = 0;` |
|    25512 | 3608 | `	if( (pValue->iFlags & MEMOBJ_INT) == 0 \|\| (pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL)) ){` |
|    16965 | 3609 | `		return 0;` |
|        - | 3610 | `	}` |
|     8552 | 3611 | `	if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       20 | 3612 | `		aAlt = (const ph7_type_alt *)SySetBasePtr(&pArg->aUnionAlts);` |
|       20 | 3613 | `		nAlt = SySetUsed(&pArg->aUnionAlts);` |
|       12 | 3614 | `	}else{` |
|     8536 | 3615 | `		sOne.nType = pArg->nType;` |
|     8536 | 3616 | `		sOne.sClass = pArg->sClass;` |
|     8536 | 3617 | `		sOne.nGroup = 0;` |
|     8536 | 3618 | `		aAlt = &sOne;` |
|     8536 | 3619 | `		nAlt = 1;` |
|        - | 3620 | `	}` |
|     8590 | 3621 | `	for( n = 0 ; n < nAlt ; ++n ){` |
|     8562 | 3622 | `		sxu32 nType = aAlt[n].nType;` |
|     8562 | 3623 | `		if( nType == SXU32_HIGH ){` |
|        3 | 3624 | `			if( aAlt[n].sClass.nByte == 5 && SyStrnicmp(aAlt[n].sClass.zString,"mixed",5) == 0 ){` |
|      ! 0 | 3625 | `				return 0;` |
|        - | 3626 | `			}` |
|        3 | 3627 | `			continue;` |
|        - | 3628 | `		}` |
|     8560 | 3629 | `		if( nType == MEMOBJ_OBJ \|\| nType == MEMOBJ_NULL ){` |
|      ! 0 | 3630 | `			continue;` |
|        - | 3631 | `		}` |
|     8560 | 3632 | `		if( nType & MEMOBJ_INT ){` |
|     8524 | 3633 | `			return 0;` |
|        - | 3634 | `		}` |
|       40 | 3635 | `		if( nType & MEMOBJ_REAL ){` |
|       31 | 3636 | `			bReal = 1;` |
|       14 | 3637 | `		}` |
|       22 | 3638 | `	}` |
|       31 | 3639 | `	return bReal;` |
|    12741 | 3640 | `}` |
|     1240 | 3641 | `PH7_PRIVATE sxi32 VmEnforceTypedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        5 | 3642 | `{` |
|     1245 | 3643 | `	if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pValue) == SXRET_OK ){` |
|     1217 | 3644 | `		return SXRET_OK;` |
|        - | 3645 | `	}` |
|       30 | 3646 | `	return VmDefaultPropertyTypeError(&(*pVm),pClass,pAttr,pValue);` |
|      625 | 3647 | `}` |
|        - | 3648 | `/*` |
|        - | 3649 | ` * Deferred typed-STATIC-default failure (VM_CLASS_ATTR_TYPE_DEFER): scan the` |
|        - | 3650 | ` * class chain for a static typed slot whose mount-time default failed its` |
|        - | 3651 | ` * type check, and throw php's catchable "Cannot assign <kind> to property` |
|        - | 3652 | ` * C::$s of type T" TypeError for the first one found. php evaluates static` |
|        - | 3653 | ` * defaults lazily, so the failure surfaces at the FIRST static-property` |
|        - | 3654 | ` * access (read/write/isset — any property of the class) or instantiation; a` |
|        - | 3655 | ` * never-touched class stays silent, and the throw repeats on every access` |
|        - | 3656 | ` * (the flag is not cleared — php's table materialization keeps failing too).` |
|        - | 3657 | ` * Returns SXRET_OK when nothing is pending (the class flag is only a hint),` |
|        - | 3658 | ` * else the PH7_EXCEPTION/PH7_ABORT of the throw.` |
|        - | 3659 | ` */` |
|      340 | 3660 | `static sxi32 VmThrowDeferredStaticType(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 3661 | `{` |
|        - | 3662 | `	ph7_class *pScan;` |
|      765 | 3663 | `	for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|        - | 3664 | `		SyHashEntry *pEntry;` |
|      429 | 3665 | `		SyHashResetLoopCursor(&pScan->hAttr);` |
|     1269 | 3666 | `		while( (pEntry = SyHashGetNextEntry(&pScan->hAttr)) != 0 ){` |
|      849 | 3667 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      844 | 3668 | `			if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)) ==` |
|        - | 3669 | `				(PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_TYPED)` |
|      439 | 3670 | `			 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|       39 | 3671 | `			 && pAttr->nIdx != SXU32_HIGH ){` |
|       39 | 3672 | `				SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       39 | 3673 | `				if( pSlot ){` |
|       39 | 3674 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|       39 | 3675 | `					if( pVmAttr->iState & VM_CLASS_ATTR_TYPE_DEFER ){` |
|        6 | 3676 | `						ph7_value *pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|        - | 3677 | `						ph7_value sNull;` |
|        6 | 3678 | `						if( pValue == 0 ){` |
|      ! 0 | 3679 | `							PH7_MemObjInit(&(*pVm),&sNull);` |
|      ! 0 | 3680 | `							pValue = &sNull;` |
|      ! 0 | 3681 | `						}` |
|        6 | 3682 | `						return VmDefaultPropertyTypeError(&(*pVm),PH7_VmAttrOwner(pVmAttr),pAttr,pValue);` |
|        - | 3683 | `					}` |
|       15 | 3684 | `				}` |
|       15 | 3685 | `			}` |
|        5 | 3686 | `		}` |
|      215 | 3687 | `	}` |
|      341 | 3688 | `	return SXRET_OK;` |
|      175 | 3689 | `}` |
|        - | 3690 | `/*` |
|        - | 3691 | ` * TRUE when [pClass] (or any of its bases) still owes its static table a` |
|        - | 3692 | ` * materialization: an initializer that threw at mount and was deferred` |
|        - | 3693 | ` * (PH7_CLASS_ATTR_STATIC_DEFER) or a typed default that failed its check` |
|        - | 3694 | ` * (VM_CLASS_ATTR_TYPE_DEFER). The gate the access sites test before paying for` |
|        - | 3695 | ` * PH7_VmMaterializeClassStatics. The BASES are walked here rather than relying` |
|        - | 3696 | ` * on the flag being copied down at mount, because classes mount in hash order:` |
|        - | 3697 | ` * a subclass can be mounted before the base whose default failed.` |
|        - | 3698 | ` */` |
|  2246456 | 3699 | `PH7_PRIVATE int VmClassStaticDeferPending(ph7_class *pClass)` |
|        5 | 3700 | `{` |
|  4505269 | 3701 | `	while( pClass ){` |
|  2259369 | 3702 | `		if( pClass->iFlags & PH7_CLASS_STATIC_DEFER ){` |
|      561 | 3703 | `			return 1;` |
|        - | 3704 | `		}` |
|  2258813 | 3705 | `		pClass = pClass->pBase;` |
|        5 | 3706 | `	}` |
|  2245905 | 3707 | `	return 0;` |
|  1123212 | 3708 | `}` |
|        - | 3709 | `/*` |
|        - | 3710 | ` * Re-run the initializers of the static properties whose evaluation was` |
|        - | 3711 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|        - | 3712 | ` *` |
|        - | 3713 | ` * php builds a class's static table on first use, evaluating each slot's` |
|        - | 3714 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|        - | 3715 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|        - | 3716 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|        - | 3717 | ` * the access: a constant define()d after the class declaration resolves.` |
|        - | 3718 | ` *` |
|        - | 3719 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|        - | 3720 | ` * the base's broken default, not its own), then declaration order within a` |
|        - | 3721 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|        - | 3722 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|        - | 3723 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|        - | 3724 | ` * re-raises on every access too.` |
|        - | 3725 | ` */` |
|      502 | 3726 | `static sxi32 VmCollectDeferredStaticDefaults(ph7_class *pClass,SySet *pOut,int *pbLeft)` |
|        5 | 3727 | `{` |
|        - | 3728 | `	SyHashEntry *pEntry;` |
|        - | 3729 | `	sxi32 rc;` |
|      507 | 3730 | `	if( pClass->pBase ){` |
|       93 | 3731 | `		rc = VmCollectDeferredStaticDefaults(pClass->pBase,pOut,pbLeft);` |
|       93 | 3732 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3733 | `			return rc;` |
|        - | 3734 | `		}` |
|       44 | 3735 | `	}` |
|      507 | 3736 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     1463 | 3737 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      961 | 3738 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      961 | 3739 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|        - | 3740 | `			/* Not pending. An inherited slot the base pass already collected` |
|        - | 3741 | `			 * lands here too (a subclass's hAttr shares the base's attribute);` |
|        - | 3742 | `			 * the evaluation loop re-tests the flag, so a duplicate is a no-op. */` |
|      873 | 3743 | `			continue;` |
|        - | 3744 | `		}` |
|       91 | 3745 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_EVALING ){` |
|        - | 3746 | `			/* Pending but already RUNNING: an initializer that reads a static` |
|        - | 3747 | `			 * property re-enters this walk through OP_MEMBER, and re-running the` |
|        - | 3748 | `			 * initializer it is inside would not terminate. The in-flight slot` |
|        - | 3749 | `			 * reads as it stands, like the constant path's cycle guard — and the` |
|        - | 3750 | `			 * class keeps its hint flag so a later access retries. */` |
|      ! 0 | 3751 | `			*pbLeft = 1;` |
|      ! 0 | 3752 | `			continue;` |
|        - | 3753 | `		}` |
|       91 | 3754 | `		rc = SySetPut(pOut,(const void *)&pAttr);` |
|       91 | 3755 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3756 | `			return rc;` |
|        - | 3757 | `		}` |
|        3 | 3758 | `	}` |
|      507 | 3759 | `	return SXRET_OK;` |
|      256 | 3760 | `}` |
|        - | 3761 | `/*` |
|        - | 3762 | ` * Re-run the initializers of the static properties whose evaluation was` |
|        - | 3763 | ` * DEFERRED at class mount because they threw (PH7_CLASS_ATTR_STATIC_DEFER).` |
|        - | 3764 | ` *` |
|        - | 3765 | ` * php builds a class's static table on first use, evaluating each slot's` |
|        - | 3766 | `` * initializer THERE — so `class C { public static $s = UNDEF; }` is silent at`` |
|        - | 3767 | `` * the declaration and raises `Undefined constant "UNDEF"` at the first access,`` |
|        - | 3768 | ` * catchably, at THAT line. It also means the re-run sees the world as it is at` |
|        - | 3769 | ` * the access: a constant define()d after the class declaration resolves.` |
|        - | 3770 | ` *` |
|        - | 3771 | ` * Order is php's table order: the BASE's slots first (a subclass access raises` |
|        - | 3772 | ` * the base's broken default, not its own), then declaration order within a` |
|        - | 3773 | ` * class. A slot that evaluates cleanly is memoized (the flag is cleared) and a` |
|        - | 3774 | ` * later failure of a SIBLING slot re-runs only what is still pending, matching` |
|        - | 3775 | ` * php's partially-materialized table. A slot that throws keeps its flag: php` |
|        - | 3776 | ` * re-raises on every access too.` |
|        - | 3777 | ` *` |
|        - | 3778 | ` * The pending slots are COLLECTED before any of them runs: an initializer is` |
|        - | 3779 | ` * user-visible execution that can re-enter this walk, and SyHash carries a` |
|        - | 3780 | ` * single shared loop cursor, so evaluating mid-walk would let the nested walk` |
|        - | 3781 | ` * cut the outer one short.` |
|        - | 3782 | ` */` |
|      414 | 3783 | `static sxi32 VmEvalDeferredStaticDefaults(ph7_vm *pVm,ph7_class *pClass,int *pbLeft)` |
|        5 | 3784 | `{` |
|        - | 3785 | `	SySet aPending; /* ph7_class_attr * , php's static-table order */` |
|        - | 3786 | `	ph7_class_attr **apPending;` |
|        - | 3787 | `	sxu32 n,nUsed;` |
|        - | 3788 | `	sxi32 rc;` |
|      419 | 3789 | `	SySetInit(&aPending,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|      419 | 3790 | `	rc = VmCollectDeferredStaticDefaults(pClass,&aPending,pbLeft);` |
|      419 | 3791 | `	apPending = (ph7_class_attr **)SySetBasePtr(&aPending);` |
|      419 | 3792 | `	nUsed = SySetUsed(&aPending);` |
|      429 | 3793 | `	for( n = 0 ; rc == SXRET_OK && n < nUsed ; ++n ){` |
|       87 | 3794 | `		ph7_class_attr *pAttr = apPending[n];` |
|       87 | 3795 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|        - | 3796 | `		ph7_class *pSaveCtx;` |
|        - | 3797 | `		void *pSaveFrame;` |
|        - | 3798 | `		sxu32 nSaveLazyLine;` |
|        - | 3799 | `		sxi32 nSaveLazyDepth;` |
|        - | 3800 | `		ph7_value *pMemObj;` |
|        - | 3801 | `		sxi32 rcExec;` |
|       87 | 3802 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|      ! 0 | 3803 | `			continue; /* the base pass already ran this shared slot */` |
|        - | 3804 | `		}` |
|       87 | 3805 | `		pMemObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|       87 | 3806 | `		if( pMemObj == 0 ){` |
|      ! 0 | 3807 | `			continue;` |
|        - | 3808 | `		}` |
|       87 | 3809 | `		pSaveCtx = pVm->pConstEvalClass;` |
|       87 | 3810 | `		pSaveFrame = pVm->pConstEvalFrame;` |
|       87 | 3811 | `		pVm->pConstEvalClass = pOwner;` |
|        - | 3812 | `		/* Unlike the mount pass, this runs at an arbitrary point in execution —` |
|        - | 3813 | `		 * possibly inside a METHOD of another class. Mark the frame current at` |
|        - | 3814 | `		 * eval start so self::/parent:: in the initializer resolve against the` |
|        - | 3815 | `		 * DECLARING class rather than that method's, exactly as the class-constant` |
|        - | 3816 | `		 * on-demand path does (VmLocalExec pushes no frame of its own). */` |
|       87 | 3817 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 3818 | `		/* The initializer runs HERE, at the access: that is the line php reports` |
|        - | 3819 | `		 * for a throw out of its own bytecode (see PH7_VmStampThrowableSite). */` |
|       87 | 3820 | `		nSaveLazyLine = pVm->nLazyInitLine;` |
|       87 | 3821 | `		nSaveLazyDepth = pVm->nLazyInitDepth;` |
|       87 | 3822 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|       87 | 3823 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1; /* the activation VmLocalExec is about to push */` |
|       87 | 3824 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, as at mount */` |
|       87 | 3825 | `		pVm->nConstEvalDepth++;` |
|       87 | 3826 | `		rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|       87 | 3827 | `		pVm->nConstEvalDepth--;` |
|       87 | 3828 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|       87 | 3829 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|       87 | 3830 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       87 | 3831 | `		pVm->pConstEvalClass = pSaveCtx;` |
|       87 | 3832 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|       87 | 3833 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        - | 3834 | `			/* Raised at the access site, where it belongs: hand the status to the` |
|        - | 3835 | `			 * caller to route (a catch here is the user's own). */` |
|       58 | 3836 | `			rc = rcExec;` |
|       67 | 3837 | `			break;` |
|        - | 3838 | `		}` |
|       31 | 3839 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER;` |
|       31 | 3840 | `		if( pVm->pConstCycleAttr && pVm->nConstEvalDepth == 0 ){` |
|        - | 3841 | `			/* The initializer named a self-referencing constant. Like the mount` |
|        - | 3842 | `			 * path, the innermost evaluation only RECORDS it; raise it here, at` |
|        - | 3843 | `			 * the access, where a catch can see it. */` |
|      ! 0 | 3844 | `			rc = VmConstCycleThrow(&(*pVm));` |
|      ! 0 | 3845 | `			break;` |
|        - | 3846 | `		}` |
|       28 | 3847 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       28 | 3848 | `		 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        - | 3849 | `			/* Now that a value exists, apply the typed-default rule the mount pass` |
|        - | 3850 | `			 * had to skip. Flag the slot as well so every later access re-throws` |
|        - | 3851 | `			 * through the deferred-type scan, as php's failing materialization does. */` |
|       25 | 3852 | `			SyHashEntry *pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       25 | 3853 | `			if( pSlot && VmCheckTypedDefault(&(*pVm),pOwner,pAttr,pMemObj) != SXRET_OK ){` |
|       20 | 3854 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|       20 | 3855 | `				pVmAttr->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|       20 | 3856 | `				rc = VmDefaultPropertyTypeError(&(*pVm),PH7_VmAttrOwner(pVmAttr),pAttr,pMemObj);` |
|       20 | 3857 | `				break;` |
|        - | 3858 | `			}` |
|        2 | 3859 | `		}` |
|        7 | 3860 | `	}` |
|      419 | 3861 | `	if( rc != SXRET_OK ){` |
|       76 | 3862 | `		*pbLeft = 1; /* whatever is still flagged stays pending for the next access */` |
|       37 | 3863 | `	}` |
|      419 | 3864 | `	SySetRelease(&aPending);` |
|      419 | 3865 | `	return rc;` |
|        5 | 3866 | `}` |
|        - | 3867 | `/*` |
|        - | 3868 | ` * TRUE for an INSTANCE property whose default the class's resolution holds to its` |
|        - | 3869 | ` * declared type: typed, with an initializer of its own (a native literal states its` |
|        - | 3870 | ` * type itself).` |
|        - | 3871 | ` */` |
|     1122 | 3872 | `static int VmIsTypedInstanceDefault(ph7_class_attr *pAttr)` |
|        5 | 3873 | `{` |
|     1510 | 3874 | `	return (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|      561 | 3875 | `			== PH7_CLASS_ATTR_TYPED` |
|      944 | 3876 | `		&& (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0` |
|      752 | 3877 | `		&& pAttr->pNativeValue == 0` |
|     1505 | 3878 | `		&& SySetUsed(&pAttr->aByteCode) > 0;` |
|        5 | 3879 | `}` |
|        - | 3880 | `/*` |
|        - | 3881 | ` * The instance half of php's class resolution (zend_update_class_constants): every` |
|        - | 3882 | ` * typed INSTANCE default is evaluated and held to its type -- strictly, so an int` |
|        - | 3883 | ` * into a float widens and nothing else converts -- the base class first, and before` |
|        - | 3884 | ` * the static table. A default the compiler folded was checked there already; one it` |
|        - | 3885 | `` * could not (`float $f = K`, `string $s = K`) is answered here, the first time`` |
|        - | 3886 | `` * anything resolves the class: `new`, a static-property access, get_class_vars(),`` |
|        - | 3887 | ` * getDefaultProperties(). The pending set is collected before any initializer runs,` |
|        - | 3888 | ` * for the reason VmEvalDeferredStaticDefaults gives.` |
|        - | 3889 | ` */` |
|      490 | 3890 | `static sxi32 VmCheckInstanceDefaults(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 3891 | `{` |
|        - | 3892 | `	SySet aTyped; /* ph7_class_attr * , declaration order */` |
|        - | 3893 | `	ph7_class_attr **apTyped;` |
|        - | 3894 | `	SyHashEntry *pEntry;` |
|        - | 3895 | `	sxu32 n,nUsed;` |
|      495 | 3896 | `	sxi32 rc = SXRET_OK;` |
|      495 | 3897 | `	if( pClass->pBase && VmClassStaticDeferPending(pClass->pBase) ){` |
|       49 | 3898 | `		rc = VmCheckInstanceDefaults(&(*pVm),pClass->pBase);` |
|       49 | 3899 | `		if( rc != SXRET_OK ){` |
|        3 | 3900 | `			return rc;` |
|        - | 3901 | `		}` |
|       21 | 3902 | `	}` |
|      493 | 3903 | `	SySetInit(&aTyped,&pVm->sAllocator,sizeof(ph7_class_attr *));` |
|      493 | 3904 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     1437 | 3905 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      949 | 3906 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      949 | 3907 | `		if( VmIsTypedInstanceDefault(pAttr) ){` |
|      583 | 3908 | `			rc = SySetPut(&aTyped,(const void *)&pAttr);` |
|      583 | 3909 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3910 | `				break;` |
|        - | 3911 | `			}` |
|      289 | 3912 | `		}` |
|        5 | 3913 | `	}` |
|      493 | 3914 | `	apTyped = (ph7_class_attr **)SySetBasePtr(&aTyped);` |
|      493 | 3915 | `	nUsed = SySetUsed(&aTyped);` |
|     1069 | 3916 | `	for( n = 0 ; rc == SXRET_OK && n < nUsed ; ++n ){` |
|      581 | 3917 | `		ph7_class_attr *pAttr = apTyped[n];` |
|      581 | 3918 | `		ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      581 | 3919 | `		void *pSaveFrame = pVm->pConstEvalFrame;` |
|      581 | 3920 | `		sxu32 nSaveLazyLine = pVm->nLazyInitLine;` |
|      581 | 3921 | `		sxi32 nSaveLazyDepth = pVm->nLazyInitDepth;` |
|        - | 3922 | `		ph7_value sValue;` |
|        - | 3923 | `		sxi32 rcExec;` |
|      581 | 3924 | `		PH7_MemObjInit(&(*pVm),&sValue);` |
|        - | 3925 | ``		/* The evaluation context `new` gives an instance default, at THIS line. */`` |
|      581 | 3926 | `		pVm->pConstEvalClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|      581 | 3927 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|      581 | 3928 | `		pVm->nLazyInitLine = VmLazyInitLineHere(&(*pVm));` |
|      581 | 3929 | `		pVm->nLazyInitDepth = pVm->nVmExecDepth + 1;` |
|      581 | 3930 | `		rcExec = VmLocalExec(&(*pVm),&pAttr->aByteCode,&sValue,FALSE);` |
|      581 | 3931 | `		pVm->nLazyInitLine = nSaveLazyLine;` |
|      581 | 3932 | `		pVm->nLazyInitDepth = nSaveLazyDepth;` |
|      581 | 3933 | `		pVm->pConstEvalClass = pSaveCtx;` |
|      581 | 3934 | `		pVm->pConstEvalFrame = pSaveFrame;` |
|      581 | 3935 | `		if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|        6 | 3936 | `			rc = rcExec;` |
|        4 | 3937 | `		}else{` |
|      577 | 3938 | `			rc = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,&sValue);` |
|        - | 3939 | `		}` |
|      581 | 3940 | `		PH7_MemObjRelease(&sValue);` |
|      293 | 3941 | `	}` |
|      493 | 3942 | `	SySetRelease(&aTyped);` |
|      493 | 3943 | `	return rc;` |
|      250 | 3944 | `}` |
|        - | 3945 | `/*` |
|        - | 3946 | ` * A typed INSTANCE property's default as its class's resolution leaves it: once the` |
|        - | 3947 | ` * declaring class has resolved, php's default table holds the converted value, so` |
|        - | 3948 | ` * getDefaultValue(), getDefaultProperties(), get_class_vars() and the export read` |
|        - | 3949 | `` * `float $f = K` as float(1); before that they read the constant's own int. A`` |
|        - | 3950 | ` * static's default is never rewritten -- php resolves it into the live table -- and` |
|        - | 3951 | ` * a default the compiler folded carries its conversion in its own byte-code.` |
|        - | 3952 | ` */` |
|      178 | 3953 | `PH7_PRIVATE void PH7_VmResolvedDefault(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr,ph7_value *pValue)` |
|        4 | 3954 | `{` |
|        - | 3955 | `	ph7_class *pOwner;` |
|      182 | 3956 | `	if( !VmIsTypedInstanceDefault(pAttr) ){` |
|       80 | 3957 | `		return;` |
|        - | 3958 | `	}` |
|      105 | 3959 | `	pOwner = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|      105 | 3960 | `	if( pOwner == 0 \|\| VmClassStaticDeferPending(pOwner) ){` |
|       69 | 3961 | `		return;` |
|        - | 3962 | `	}` |
|       39 | 3963 | `	VmCheckTypedDefault(&(*pVm),pOwner,pAttr,pValue);` |
|       93 | 3964 | `}` |
|        - | 3965 | `/*` |
|        - | 3966 | ` * Materialize [pClass]'s static table, php's way: hold the typed instance defaults` |
|        - | 3967 | ` * to their types, evaluate whatever the mount pass deferred, then raise any` |
|        - | 3968 | ` * typed-default failure. Called by the sites php resolves a class at — the first` |
|        - | 3969 | ` * static-PROPERTY access (read, write, isset; a class CONSTANT or a static METHOD` |
|        - | 3970 | ` * CALL does not materialize, php-exact), instantiation, get_class_vars() and` |
|        - | 3971 | ` * getDefaultProperties(). Returns SXRET_OK when the class is (or already was)` |
|        - | 3972 | ` * resolved, else the PH7_EXCEPTION/PH7_ABORT of the throw for the caller to route.` |
|        - | 3973 | ` */` |
|      446 | 3974 | `PH7_PRIVATE sxi32 PH7_VmMaterializeClassStatics(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 3975 | `{` |
|      451 | 3976 | `	int bLeft = 0;` |
|      451 | 3977 | `	sxi32 rc = VmCheckInstanceDefaults(&(*pVm),pClass);` |
|      451 | 3978 | `	if( rc == SXRET_OK ){` |
|      419 | 3979 | `		rc = VmEvalDeferredStaticDefaults(&(*pVm),pClass,&bLeft);` |
|      207 | 3980 | `	}` |
|      451 | 3981 | `	if( rc == SXRET_OK ){` |
|      345 | 3982 | `		rc = VmThrowDeferredStaticType(&(*pVm),pClass);` |
|      170 | 3983 | `	}` |
|      451 | 3984 | `	if( rc == SXRET_OK && !bLeft ){` |
|        - | 3985 | `		/* The table is whole and nothing failed: retire the hint on the whole` |
|        - | 3986 | `		 * chain, so the ordinary static accesses that follow stop paying for the` |
|        - | 3987 | `		 * scan. A re-mount (VM reset) re-arms it, and a failure above leaves it` |
|        - | 3988 | `		 * set — php's materialization keeps failing too. */` |
|        - | 3989 | `		ph7_class *pScan;` |
|      761 | 3990 | `		for( pScan = pClass ; pScan ; pScan = pScan->pBase ){` |
|      425 | 3991 | `			pScan->iFlags &= ~PH7_CLASS_STATIC_DEFER;` |
|      215 | 3992 | `		}` |
|      168 | 3993 | `	}` |
|      451 | 3994 | `	return rc;` |
|        5 | 3995 | `}` |
|        - | 3996 |  |
|        - | 3997 | `/*` |
|        - | 3998 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - | 3999 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - | 4000 | ` * information.` |
|        - | 4001 | ` * ------------------------------------` |
|        - | 4002 | ` * Simple boring wrapper function.` |
|        - | 4003 | ` * ------------------------------------` |
|        - | 4004 | ` */` |
|        - | 4005 | `/*` |
|        - | 4006 | ` * php's RUNTIME fatal (E_ERROR), in the shape php prints it.` |
|        - | 4007 | ` *` |
|        - | 4008 | ``  * PHL's COMPILE-time fatal has been php's for a long time -- the `PHP Fatal error:  ` `` |
|        - | 4009 | `` * label, the sentence, ` in FILE on line N`, then the `Stack trace:` block and its`` |
|        - | 4010 | `` * `#N {main}` terminator (PH7_GenCompileError). Its RUNTIME one was not: the ordinary`` |
|        - | 4011 | ` * diagnostic printer behind VmErrorFormat(PH7_CTX_ERR, ...) labels the same event` |
|        - | 4012 | `` * `PHP Error:  ` and stops at the location, so a refusal php reports with four lines`` |
|        - | 4013 | ` * and a frame chain came out here as one line and no chain at all.` |
|        - | 4014 | ` *` |
|        - | 4015 | ` * This is that renderer, and it borrows both halves rather than writing either: the` |
|        - | 4016 | ` * label the compile-time path prints, and the trace builder the uncaught-exception` |
|        - | 4017 | ` * path walks the frame chain with (PH7_FATAL_TRACE_RUNTIME, the kind that KEEPS the` |
|        - | 4018 | ` * include/require that loaded the unit -- a runtime refusal happens with that` |
|        - | 4019 | ` * activation live, where a compiler's does not).` |
|        - | 4020 | ` *` |
|        - | 4021 | ` * It is deliberately NOT the ordinary diagnostic path: php never runs a` |
|        - | 4022 | `` * set_error_handler() for an E_ERROR, so no handler is consulted, and `@` does not`` |
|        - | 4023 | ` * suppress one either. error_get_last() DOES see it, which a` |
|        - | 4024 | ` * register_shutdown_function() callback can still read. Requesting the halt and the` |
|        - | 4025 | ` * exit status is the caller's, as it is at every other fatal site.` |
|        - | 4026 | ` */` |
|        - | 4027 | `/*` |
|        - | 4028 | ` * Write one copy of a COMPILE-time diagnostic.` |
|        - | 4029 | ` *` |
|        - | 4030 | ` * A compile-time refusal can happen before the host has wired the VM's streams` |
|        - | 4031 | ``  * at all -- the main script's own compile CREATES the VM, so a `phl bad.php` `` |
|        - | 4032 | `` * diagnostic is raised with `sVmConsumer`/`sVmErrConsumer` still empty. The`` |
|        - | 4033 | ` * engine-level compile-error consumer (PH7_CONFIG_ERR_OUTPUT) is the one channel` |
|        - | 4034 | ` * that always exists, so it is the fallback for either copy.` |
|        - | 4035 | ` *` |
|        - | 4036 | `` * The terminator is `\n` on every platform, which is what this path has always`` |
|        - | 4037 | `` * written -- VmWriteDiagnostic's `\r\n` belongs to the runtime one and is not`` |
|        - | 4038 | ` * borrowed here.` |
|        - | 4039 | ` */` |
|     1594 | 4040 | `static sxi32 VmWriteCompileCopy(ph7_vm *pVm,ph7_output_consumer *pCons,SyBlob *pMsg,int bDisplay)` |
|        4 | 4041 | `{` |
|     1598 | 4042 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|     1598 | 4043 | `	if( pCons && pCons->xConsumer ){` |
|       56 | 4044 | `		sxi32 rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|       56 | 4045 | `		if( bDisplay ){` |
|        6 | 4046 | `			VmTrackOutput(pVm,SyBlobLength(pMsg));` |
|        2 | 4047 | `		}` |
|       56 | 4048 | `		return rc;` |
|        - | 4049 | `	}` |
|        - | 4050 | `	/* No VM stream yet -- the main script's own compile. The DISPLAY copy is` |
|        - | 4051 | `	 * program output and belongs on the host's output stream; falling through to` |
|        - | 4052 | `	 * the compile-error consumer below put php's stdout text on stderr, so` |
|        - | 4053 | ``	 * `-d display_errors=1` moved nothing. */`` |
|     1546 | 4054 | `	if( bDisplay && pVm->pEngine && pVm->pEngine->xConf.xOut ){` |
|      111 | 4055 | `		return pVm->pEngine->xConf.xOut(SyBlobData(pMsg),SyBlobLength(pMsg),` |
|       72 | 4056 | `			pVm->pEngine->xConf.pOutData);` |
|        - | 4057 | `	}` |
|     1474 | 4058 | `	if( pVm->pEngine && pVm->pEngine->xConf.xErr ){` |
|     2209 | 4059 | `		return pVm->pEngine->xConf.xErr(SyBlobData(pMsg),SyBlobLength(pMsg),` |
|     1470 | 4060 | `			pVm->pEngine->xConf.pErrData);` |
|        - | 4061 | `	}` |
|      ! 0 | 4062 | `	return SXRET_OK;` |
|      801 | 4063 | `}` |
|        - | 4064 | `/*` |
|        - | 4065 | ` * A COMPILE-time diagnostic's two copies, the same pair and the same two ini` |
|        - | 4066 | ` * gates a runtime one gets (VmEmitFatalReport). The compiler used to write ONE` |
|        - | 4067 | ` * copy, in the log shape, to the engine's compile-error consumer -- which the CLI` |
|        - | 4068 | `` * pointed at STDOUT, so `phl -l bad.php` and `phl bad.php` both put php's stderr`` |
|        - | 4069 | ` * text into program output, where anything capturing stdout reads it as data.` |
|        - | 4070 | ` *` |
|        - | 4071 | ` * The caller hands over the LABEL and a header-less BODY.` |
|        - | 4072 | ` */` |
|        - | 4073 | `/*` |
|        - | 4074 | ` * The program-output stream to give a compile diagnostic's DISPLAY copy, or NULL` |
|        - | 4075 | ` * when the host has not wired one yet.` |
|        - | 4076 | ` *` |
|        - | 4077 | ` * PH7_VmMakeReady installs PH7_VmBlobConsumer as the VM's output consumer, and` |
|        - | 4078 | ` * MakeReady runs INSIDE ph7_compile_file -- before the host has replaced it. A` |
|        - | 4079 | ` * diagnostic written there lands in an internal blob nobody reads, which is a` |
|        - | 4080 | ` * silent LOSS, not a fallback: a typed class constant's mount-time fatal is` |
|        - | 4081 | ` * raised in exactly that window and disappeared entirely. An output buffer` |
|        - | 4082 | ` * (ob_start) also swaps this consumer and is NOT that case -- php buffers its` |
|        - | 4083 | ` * display copy too -- so the test is the blob consumer by identity, not merely` |
|        - | 4084 | ` * "something is installed".` |
|        - | 4085 | ` */` |
|     1546 | 4086 | `static ph7_output_consumer * VmCompileDisplaySink(ph7_vm *pVm)` |
|        4 | 4087 | `{` |
|     1550 | 4088 | `	if( pVm->sVmConsumer.xConsumer && pVm->sVmConsumer.xConsumer != PH7_VmBlobConsumer ){` |
|        6 | 4089 | `		return &pVm->sVmConsumer;` |
|        - | 4090 | `	}` |
|     1546 | 4091 | `	return 0;` |
|      777 | 4092 | `}` |
|     1620 | 4093 | `PH7_PRIVATE sxi32 PH7_VmEmitCompileDiagnostic(ph7_vm *pVm,sxi32 iErr,const char *zLabel,` |
|        - | 4094 | `	const char *zBody,sxu32 nBody,const char *zBare,sxu32 nBare,sxu32 nLine)` |
|        5 | 4095 | `{` |
|        - | 4096 | `	SyBlob sCopy,sHold;` |
|     1625 | 4097 | `	sxi32 rc = SXRET_OK;` |
|     1625 | 4098 | `	if( pVm == 0 ){` |
|      ! 0 | 4099 | `		return SXRET_OK;` |
|        - | 4100 | `	}` |
|        - | 4101 | `	/*` |
|        - | 4102 | `	 * A compile diagnostic owes the same three things a runtime one does` |
|        - | 4103 | `	 * (PH7_VmThrowError): the user handler, error_get_last(), and the` |
|        - | 4104 | `	 * error_reporting() mask. It used to owe NONE of them, so a library that` |
|        - | 4105 | ``	 * silences a probe -- `error_reporting(0); class_exists('X');` , which is`` |
|        - | 4106 | `	 * how a php-console-style optional dependency is tested and how monolog's` |
|        - | 4107 | `	 * suite loads one -- got the compiler's warning printed anyway.` |
|        - | 4108 | `	 *` |
|        - | 4109 | `	 * Who sees what is php's split. E_COMPILE_WARNING/E_COMPILE_ERROR/E_PARSE` |
|        - | 4110 | ``	 * are on `set_error_handler`'s own exclusion list, so those go straight to`` |
|        - | 4111 | ``	 * default processing; a compile-time E_WARNING (php raises `"continue"`` |
|        - | 4112 | ``	 * targeting switch` and the magic-visibility rules at that level) reaches`` |
|        - | 4113 | `	 * the handler like any other. error_get_last() records what reached` |
|        - | 4114 | `	 * DEFAULT processing, masked or not -- so it is recorded here even when` |
|        - | 4115 | `	 * the mask hides the print, and NOT when a handler claimed it.` |
|        - | 4116 | `	 *` |
|        - | 4117 | `	 * Both texts are the CALLER's buffer -- the code generator's one-message` |
|        - | 4118 | ``	 * store -- and a user handler can compile (an `include`, an `eval`), which`` |
|        - | 4119 | `	 * resets and reallocates exactly that buffer. Everything below reads the` |
|        - | 4120 | `	 * copy instead; only the copy's lifetime is this function's.` |
|        - | 4121 | `	 */` |
|     1625 | 4122 | `	SyBlobInit(&sHold,&pVm->sAllocator);` |
|     1625 | 4123 | `	SyBlobAppend(&sHold,zBody,nBody);` |
|     1625 | 4124 | `	zBody = (const char *)SyBlobData(&sHold);` |
|     1625 | 4125 | `	zBare = zBody;` |
|     1625 | 4126 | `	if( nBare > nBody ){` |
|      ! 0 | 4127 | `		nBare = nBody;` |
|      ! 0 | 4128 | `	}` |
|        - | 4129 | `	{` |
|        - | 4130 | `		/* The handler and error_get_last() take the BARE sentence and the line` |
|        - | 4131 | `		 * as their own fields; only the printed copies carry php's` |
|        - | 4132 | ``		 * ` in FILE on line N` tail, which the caller has already appended to`` |
|        - | 4133 | `		 * zBody. */` |
|     1625 | 4134 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     1620 | 4135 | `		if( iErr == 2 /* E_WARNING */ \|\| iErr == 8 /* E_NOTICE */` |
|     1567 | 4136 | `		 \|\| iErr == 8192 /* E_DEPRECATED */ ){` |
|      100 | 4137 | `			if( !VmInvokeErrorHandler(pVm,iErr,zBare,(sxi32)nBare,pFile,(sxi32)nLine) ){` |
|       30 | 4138 | `				SyBlobRelease(&sHold);` |
|       30 | 4139 | `				return SXRET_OK;` |
|        - | 4140 | `			}` |
|        - | 4141 | `			/* The handler may have moved the file stack (its own include) and it` |
|        - | 4142 | `			 * may have written the copy's backing store's neighbours; re-read` |
|        - | 4143 | `			 * both before they are used again. */` |
|       71 | 4144 | `			zBody = (const char *)SyBlobData(&sHold);` |
|       71 | 4145 | `			zBare = zBody;` |
|       71 | 4146 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       34 | 4147 | `		}` |
|     1596 | 4148 | `		VmRecordLastError(&(*pVm),iErr,zBare,nBare,pFile,nLine);` |
|        - | 4149 | `	}` |
|        - | 4150 | `	/*` |
|        - | 4151 | `	 * A host that has said nothing about the level leaves iErrMask at zero, which` |
|        - | 4152 | ``	 * must not be read as `error_reporting(0)`. bErrMaskSet says whether anybody`` |
|        - | 4153 | `	 * has spoken; until somebody has, the pre-mask behaviour (report it) stands.` |
|        - | 4154 | `	 * The CLI speaks before it compiles -- PH7_CONFIG_ERR_REPORT and the -d/-c` |
|        - | 4155 | `	 * queue are engine-level, so the main script's OWN compile is already gated --` |
|        - | 4156 | `	 * and an embedder that configures nothing still gets the fallback.` |
|        - | 4157 | `	 */` |
|     1596 | 4158 | `	if( pVm->bErrMaskSet && !VmErrReportWants(pVm,iErr) ){` |
|       17 | 4159 | `		SyBlobRelease(&sHold);` |
|       17 | 4160 | `		return SXRET_OK;` |
|        - | 4161 | `	}` |
|     1580 | 4162 | `	if( pVm->bLogErrors ){` |
|     1522 | 4163 | `		SyBlobInit(&sCopy,&pVm->sAllocator);` |
|     1522 | 4164 | `		SyBlobFormat(&sCopy,"PHP %s:  ",zLabel);` |
|     1522 | 4165 | `		SyBlobAppend(&sCopy,zBody,nBody);` |
|        - | 4166 | `		/* Same order VmErrConsumer takes at run time: the error stream, then the` |
|        - | 4167 | `		 * output one for an embedder that wired only that, then the engine's. */` |
|        - | 4168 | ``		/* A compile diagnostic owes the `error_log` destination the same copy a`` |
|        - | 4169 | `		 * runtime one does: a bootstrap that sets the directive from php.ini has` |
|        - | 4170 | `		 * it in hand before the unit compiles. */` |
|     1522 | 4171 | `		if( PH7_VmErrorLogToFile(pVm,(const char *)SyBlobData(&sCopy),SyBlobLength(&sCopy)) ){` |
|      ! 0 | 4172 | `			rc = SXRET_OK;` |
|      ! 0 | 4173 | `		}else{` |
|     2281 | 4174 | `			rc = VmWriteCompileCopy(&(*pVm),` |
|     1518 | 4175 | `				pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : VmCompileDisplaySink(pVm),` |
|        - | 4176 | `				&sCopy,0);` |
|        - | 4177 | `		}` |
|     1522 | 4178 | `		SyBlobRelease(&sCopy);` |
|      759 | 4179 | `	}` |
|     1580 | 4180 | `	if( pVm->iDisplayErrors != PH7_DISPLAY_ERRORS_OFF ){` |
|       79 | 4181 | `		int bErrStream = pVm->iDisplayErrors == PH7_DISPLAY_ERRORS_STDERR;` |
|        - | 4182 | `		sxi32 rc2;` |
|       79 | 4183 | `		SyBlobInit(&sCopy,&pVm->sAllocator);` |
|       79 | 4184 | `		if( !bErrStream ){` |
|        - | 4185 | `			/* php's text-mode display copy is prefixed with a blank line */` |
|       79 | 4186 | `			SyBlobAppend(&sCopy,"\n",sizeof(char));` |
|       38 | 4187 | `		}` |
|       79 | 4188 | `		SyBlobFormat(&sCopy,"%s: ",zLabel);` |
|       79 | 4189 | `		SyBlobAppend(&sCopy,zBody,nBody);` |
|        - | 4190 | ``		/* `display_errors=stderr` sends the display copy down the SAME channel the`` |
|        - | 4191 | `		 * log copy takes, and it is not program output there: no output tracking. */` |
|      155 | 4192 | `		rc2 = VmWriteCompileCopy(&(*pVm),` |
|       38 | 4193 | `			bErrStream` |
|      ! 0 | 4194 | `				? (pVm->sVmErrConsumer.xConsumer ? &pVm->sVmErrConsumer : (ph7_output_consumer *)0)` |
|       76 | 4195 | `				: VmCompileDisplaySink(pVm),` |
|       38 | 4196 | `			&sCopy,!bErrStream);` |
|       79 | 4197 | `		SyBlobRelease(&sCopy);` |
|       79 | 4198 | `		if( rc == SXRET_OK ){` |
|       79 | 4199 | `			rc = rc2;` |
|       38 | 4200 | `		}` |
|       38 | 4201 | `	}` |
|     1580 | 4202 | `	SyBlobRelease(&sHold);` |
|     1580 | 4203 | `	return rc;` |
|      815 | 4204 | `}` |
|       14 | 4205 | `PH7_PRIVATE sxi32 PH7_VmFatalError(ph7_vm *pVm,const char *zFormat,...)` |
|        3 | 4206 | `{` |
|        - | 4207 | `	SyBlob sMsg,sOut;` |
|        - | 4208 | `	SyString *pFile;` |
|        - | 4209 | `	sxu32 nLine;` |
|        - | 4210 | `	va_list ap;` |
|       17 | 4211 | `	if( pVm->nSpeculative > 0 ){` |
|        - | 4212 | `		/* See PH7_VmThrowError: nothing a speculative evaluation raises is observable. */` |
|      ! 0 | 4213 | `		pVm->nSpecDiag++;` |
|      ! 0 | 4214 | `		return SXRET_OK;` |
|        - | 4215 | `	}` |
|       17 | 4216 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       17 | 4217 | `	nLine = VmDiagnosticWhere(&(*pVm),&pFile);` |
|       17 | 4218 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 | 4219 | `	va_start(ap,zFormat);` |
|       17 | 4220 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|       17 | 4221 | `	va_end(ap);` |
|       24 | 4222 | `	VmRecordLastError(&(*pVm),1 /* E_ERROR */,(const char *)SyBlobData(&sMsg),` |
|        7 | 4223 | `		SyBlobLength(&sMsg),pFile,nLine);` |
|       17 | 4224 | `	if( pVm->bErrReport && (pVm->iErrMask & 1) != 0 ){` |
|       17 | 4225 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       17 | 4226 | `		SyBlobAppend(&sOut,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       17 | 4227 | `		VmDiagnosticLocation(&sOut,pFile,nLine);` |
|       17 | 4228 | `		PH7_GenAppendFatalTrace(&(*pVm),&sOut,PH7_FATAL_TRACE_RUNTIME);` |
|       24 | 4229 | `		VmEmitFatalReport(&(*pVm),"Fatal error",` |
|       14 | 4230 | `			(const char *)SyBlobData(&sOut),SyBlobLength(&sOut));` |
|       17 | 4231 | `		SyBlobRelease(&sOut);` |
|        7 | 4232 | `	}` |
|       17 | 4233 | `	SyBlobRelease(&sMsg);` |
|       17 | 4234 | `	return SXRET_OK;` |
|       10 | 4235 | `}` |
|     2800 | 4236 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        5 | 4237 | `{` |
|        - | 4238 | `	va_list ap;` |
|        - | 4239 | `	sxi32 rc;` |
|     2805 | 4240 | `	va_start(ap,zFormat);` |
|     2805 | 4241 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|     2805 | 4242 | `	va_end(ap);` |
|     2805 | 4243 | `	return rc;` |
|        5 | 4244 | `}` |
|        - | 4245 | `/*` |
|        - | 4246 | ` * php prefixes an argument diagnostic with the class the callee belongs to: the` |
|        - | 4247 | ` * owner the caller already knows for a METHOD, and for a CLOSURE the class it was` |
|        - | 4248 | `` * WRITTEN inside -- `C::{closure:C::m():5}`, which php reads off the function's own`` |
|        - | 4249 | `` * scope and PHL records at compile time. Appends `Name::` and answers 1 when there`` |
|        - | 4250 | ` * is one.` |
|        - | 4251 | ` */` |
|        - | 4252 | `/*` |
|        - | 4253 | ` * The owner NAME an argument diagnostic prefixes the callee with, in FULL (an` |
|        - | 4254 | ` * anonymous class's carries its NUL; the two callers below differ over what they` |
|        - | 4255 | ` * do with it). NULL when the callee has no owner.` |
|        - | 4256 | ` */` |
|     4806 | 4257 | `static SyString * VmArgOwnerName(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee)` |
|        5 | 4258 | `{` |
|     4811 | 4259 | `	SyString *pName = 0;` |
|     4811 | 4260 | `	if( pOwnerClass ){` |
|      129 | 4261 | `		pName = &pOwnerClass->sName;` |
|     4749 | 4262 | `	}else if( pCallee ){` |
|        - | 4263 | ``		/* A closure's scope is REBINDABLE (`Closure::bind($c, null, B::class)`), and`` |
|        - | 4264 | `		 * php reports the scope it is running under -- which the frame records. The` |
|        - | 4265 | `		 * declared one is the answer when there is no frame of the callee's to ask,` |
|        - | 4266 | `		 * or when nothing rebound it. */` |
|     4687 | 4267 | `		VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     4687 | 4268 | `		if( pFrame && pFrame->pUserData == (void *)pCallee && pFrame->pBoundScope ){` |
|        6 | 4269 | `			pName = &pFrame->pBoundScope->sName;` |
|     4685 | 4270 | `		}else if( SyStringLength(&pCallee->sClosureScope) > 0 ){` |
|       18 | 4271 | `			pName = &pCallee->sClosureScope;` |
|        8 | 4272 | `		}` |
|     2341 | 4273 | `	}` |
|     4811 | 4274 | `	return pName;` |
|        5 | 4275 | `}` |
|        - | 4276 | ``/* The name plus php's `::`, cut at the NUL an anonymous class carries -- this is the`` |
|        - | 4277 | ``  * spelling of the family that prints the class and the method as SEPARATE `%s` `` |
|        - | 4278 | `` * (`Too few arguments to function class@anonymous::need()`). */`` |
|      138 | 4279 | `static int VmArgOwnerPrefix(ph7_vm *pVm,SyBlob *pOut,ph7_class *pOwnerClass,ph7_vm_func *pCallee)` |
|        5 | 4280 | `{` |
|      143 | 4281 | `	SyString *pName = VmArgOwnerName(&(*pVm),pOwnerClass,pCallee);` |
|        - | 4282 | `	sxu32 nPos;` |
|      143 | 4283 | `	if( pName == 0 ){` |
|      113 | 4284 | `		return 0;` |
|        - | 4285 | `	}` |
|       33 | 4286 | `	if( SyByteFind(pName->zString,pName->nByte,0,&nPos) != SXRET_OK ){` |
|       31 | 4287 | `		nPos = pName->nByte;` |
|       14 | 4288 | `	}` |
|       33 | 4289 | `	SyBlobAppend(pOut,pName->zString,nPos);` |
|       33 | 4290 | `	SyBlobAppend(pOut,"::",sizeof("::")-1);` |
|       33 | 4291 | `	return 1;` |
|       74 | 4292 | `}` |
|        - | 4293 | `/*` |
|        - | 4294 | `` * php's name for the callee in the `Argument #N ...` family: `Class::method`, or the`` |
|        - | 4295 | ` * bare function/closure name when there is no owner class. php builds it as ONE` |
|        - | 4296 | `` * string (get_active_function_or_method_name()) and then prints it with `%s`, so an`` |
|        - | 4297 | `` * ANONYMOUS owner's NUL cuts the `::method` off with it -- the message php really`` |
|        - | 4298 | `` * prints is `class@anonymous(): Argument #1 ($i) must be of type int, string given`,`` |
|        - | 4299 | ` * and it says exactly that for every method of that class, __invoke and a closure` |
|        - | 4300 | ` * declared in one of its methods included.` |
|        - | 4301 | ` *` |
|        - | 4302 | ` * Reproduced rather than tidied: it is what php's output says, and a library's` |
|        - | 4303 | ` * expected output has it. The neighbouring diagnostics that print the class and the` |
|        - | 4304 | `` * method as SEPARATE `%s` keep both halves -- `Too few arguments to function`` |
|        - | 4305 | `` * class@anonymous::need()`, `class@anonymous::ret(): Return value must be of type` --`` |
|        - | 4306 | ` * which is why this is one family's helper and not a rule about class names.` |
|        - | 4307 | ` */` |
|     4668 | 4308 | `static void VmArgFuncLabel(ph7_vm *pVm,SyBlob *pOut,ph7_class *pOwnerClass,` |
|        - | 4309 | `	ph7_vm_func *pCallee,const char *zShow,int nShow)` |
|        5 | 4310 | `{` |
|     4673 | 4311 | `	SyString *pOwner = VmArgOwnerName(&(*pVm),pOwnerClass,pCallee);` |
|        - | 4312 | `	sxu32 nPos;` |
|     4673 | 4313 | `	if( pOwner && SyByteFind(pOwner->zString,pOwner->nByte,0,&nPos) == SXRET_OK ){` |
|        - | 4314 | `` 		/* Anonymous owner: php's `%s` stops inside the class name, so the `::method` `` |
|        - | 4315 | `		 * it assembled behind the NUL is never printed. */` |
|       11 | 4316 | `		SyBlobAppend(pOut,pOwner->zString,nPos);` |
|       11 | 4317 | `		return;` |
|        - | 4318 | `	}` |
|     4663 | 4319 | `	if( pOwner ){` |
|      109 | 4320 | `		SyBlobAppend(pOut,pOwner->zString,pOwner->nByte);` |
|      109 | 4321 | `		SyBlobAppend(pOut,"::",sizeof("::")-1);` |
|       52 | 4322 | `	}` |
|     4663 | 4323 | `	if( nShow > 0 ){` |
|     4663 | 4324 | `		SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|     2329 | 4325 | `	}` |
|     2339 | 4326 | `}` |
|        - | 4327 | `/*` |
|        - | 4328 | ` * Where php reports an argument refusal: INSIDE the callee, on the refused` |
|        - | 4329 | ` * parameter's own declaration line -- the RECV opcode that refuses it carries that` |
|        - | 4330 | ` * line and runs in the callee's op array, whatever door the call came through.` |
|        - | 4331 | ` * PHL stamped the call's line instead, so every such getLine() named the caller` |
|        - | 4332 | ` * (and a fiber body's or a generator's, bound before its frame exists, the` |
|        - | 4333 | ` * caller's file too). An INTERNAL callee (a prelude builtin) is left alone: php` |
|        - | 4334 | ` * has no frame for one and reports its refusals at the call.` |
|        - | 4335 | ` *` |
|        - | 4336 | ` * pName selects the formal by name; NULL is a variadic-collected element, which` |
|        - | 4337 | ` * belongs to the last formal. iFormal (0-based) is used when pName is NULL and` |
|        - | 4338 | ` * bByIndex is set.` |
|        - | 4339 | ` */` |
|      780 | 4340 | `static void VmArgSiteArm(ph7_vm *pVm,ph7_vm_func *pCallee,SyString *pName,sxu32 iFormal,int bByIndex)` |
|        5 | 4341 | `{` |
|        - | 4342 | `	ph7_vm_func_arg *aFormal;` |
|      785 | 4343 | `	ph7_vm_func_arg *pFormal = 0;` |
|        - | 4344 | `	sxu32 nFormal;` |
|        - | 4345 | `	sxu32 n;` |
|      785 | 4346 | `	if( pCallee == 0 \|\| (pCallee->iFlags & VM_FUNC_INTERNAL) ){` |
|       33 | 4347 | `		return;` |
|        - | 4348 | `	}` |
|      753 | 4349 | `	aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pCallee->aArgs);` |
|      753 | 4350 | `	nFormal = SySetUsed(&pCallee->aArgs);` |
|      753 | 4351 | `	if( nFormal < 1 ){` |
|      ! 0 | 4352 | `		return;` |
|        - | 4353 | `	}` |
|      753 | 4354 | `	if( pName ){` |
|      541 | 4355 | `		for( n = 0 ; n < nFormal ; n++ ){` |
|      541 | 4356 | `			if( SyStringCmp(&aFormal[n].sName,pName,SyMemcmp) == 0 ){` |
|      521 | 4357 | `				pFormal = &aFormal[n];` |
|      521 | 4358 | `				break;` |
|        - | 4359 | `			}` |
|       14 | 4360 | `		}` |
|      495 | 4361 | `	}else if( bByIndex ){` |
|      143 | 4362 | `		if( iFormal < nFormal ){` |
|      143 | 4363 | `			pFormal = &aFormal[iFormal];` |
|       74 | 4364 | `		}` |
|      167 | 4365 | `	}else if( aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       98 | 4366 | `		pFormal = &aFormal[nFormal - 1];` |
|       47 | 4367 | `	}` |
|      753 | 4368 | `	if( pFormal == 0 \|\| pFormal->nLine == 0 ){` |
|        5 | 4369 | `		return;` |
|        - | 4370 | `	}` |
|      749 | 4371 | `	pVm->nArgSiteLine = pFormal->nLine;` |
|      749 | 4372 | `	pVm->pArgSiteFile = &pCallee->sFile;` |
|      395 | 4373 | `}` |
|      780 | 4374 | `static void VmArgSiteDisarm(ph7_vm *pVm)` |
|        5 | 4375 | `{` |
|      785 | 4376 | `	pVm->nArgSiteLine = 0;` |
|      785 | 4377 | `	pVm->pArgSiteFile = 0;` |
|      785 | 4378 | `}` |
|        - | 4379 | `/*` |
|        - | 4380 | `` * The CALL SITE php names in an argument diagnostic: the `called in FILE on line`` |
|        - | 4381 | `` * N` tail of a TypeError, and the `in FILE on line N` of an ArgumentCountError.`` |
|        - | 4382 | ` * It is the CALLER's position -- which the callee's frame recorded for itself when` |
|        - | 4383 | ` * it was entered (VmEnterFrame). PHL read the top of the INCLUDE stack, and the` |
|        - | 4384 | ` * count error had a hard-coded line 1, so every one of these raised inside a` |
|        - | 4385 | ` * vendor package named the entry script and an arbitrary line.` |
|        - | 4386 | ` *` |
|        - | 4387 | ` * The top frame is only the callee's when the raise happens AFTER VmEnterFrame; a` |
|        - | 4388 | ` * generator/fiber argument install runs before one is pushed, so the identity is` |
|        - | 4389 | ` * checked rather than assumed. With no frame of the callee's to read, the position` |
|        - | 4390 | ` * running right now IS the call site, which is what the fallback names.` |
|        - | 4391 | ` */` |
|      626 | 4392 | `static void VmArgCallSite(ph7_vm *pVm,ph7_vm_func *pCallee,SyString **ppFile,sxu32 *pnLine)` |
|        5 | 4393 | `{` |
|      631 | 4394 | `	VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      631 | 4395 | `	*ppFile = 0;` |
|      631 | 4396 | `	*pnLine = pVm->nCurLine;` |
|      631 | 4397 | `	if( pCallee && pFrame && pFrame->pUserData == (void *)pCallee ){` |
|      631 | 4398 | `		if( SyStringLength(&pFrame->sCallFile) > 0 ){` |
|      631 | 4399 | `			*ppFile = &pFrame->sCallFile;` |
|      313 | 4400 | `		}` |
|      631 | 4401 | `		if( pFrame->nCallLine ){` |
|      631 | 4402 | `			*pnLine = pFrame->nCallLine;` |
|      313 | 4403 | `		}` |
|      313 | 4404 | `	}` |
|      631 | 4405 | `	if( *ppFile == 0 ){` |
|      ! 0 | 4406 | `		*ppFile = PH7_VmExecutingUnitFile(pVm);` |
|      ! 0 | 4407 | `	}` |
|      631 | 4408 | `}` |
|        - | 4409 | `/*` |
|        - | 4410 | ` * TRUE when the callee's own activation was entered by an INTERNAL function reaching` |
|        - | 4411 | `` * for a userland callback -- `array_map`, `usort`, `preg_replace_callback`, an`` |
|        - | 4412 | ` * autoloader, a shutdown function. php reads prev_execute_data and asks whether it is` |
|        - | 4413 | ` * USER code, without walking past it, so such a callback's argument diagnostics carry` |
|        - | 4414 | `` * no `, called in FILE on line N` tail at all. PHL walked to the nearest userland frame`` |
|        - | 4415 | `` * instead and so always found one -- the `array_map(...)` line, which php never prints.`` |
|        - | 4416 | ` *` |
|        - | 4417 | ` * php's two callback FORWARDS are not exceptions to the rule: its compiler elides the` |
|        - | 4418 | `` * frame for `call_user_func()`/`call_user_func_array()`, so the frame above the callee`` |
|        - | 4419 | ` * IS the userland caller and the tail belongs. The same latch that says so here says it` |
|        - | 4420 | ` * for the binding mode and the too-few wording (VM_FRAME_NATIVE_CALLER).` |
|        - | 4421 | ` */` |
|      610 | 4422 | `static int VmArgCallerIsNative(ph7_vm *pVm,ph7_vm_func *pCallee)` |
|        5 | 4423 | `{` |
|      615 | 4424 | `	VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      920 | 4425 | `	return pCallee && pFrame && pFrame->pUserData == (void *)pCallee` |
|      915 | 4426 | `		&& (pFrame->iFlags & VM_FRAME_NATIVE_CALLER) != 0;` |
|        5 | 4427 | `}` |
|        - | 4428 | `/*` |
|        - | 4429 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - | 4430 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - | 4431 | ` */` |
|      642 | 4432 | `PH7_PRIVATE sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,ph7_class *pOwnerClass,ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        5 | 4433 | `{` |
|        - | 4434 | `	ph7_class *pClass;` |
|        - | 4435 | `	ph7_class_instance *pThis;` |
|        - | 4436 | `	ph7_class_method *pCons;` |
|        - | 4437 | `	ph7_value sArg;` |
|        - | 4438 | `	ph7_value *apArg[1];` |
|        - | 4439 | `	SyBlob sMsg;` |
|        - | 4440 | `	SyString sMsgStr;` |
|      647 | 4441 | `	SyString *pFuncName = &pCallee->sName;` |
|        - | 4442 | `	VmFrame *pFrame;` |
|        - | 4443 | `	sxi32 rc;` |
|      647 | 4444 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      647 | 4445 | `	if( pClass == 0 ){` |
|      ! 0 | 4446 | `		return PH7_ABORT;` |
|        - | 4447 | `	}` |
|      647 | 4448 | `	VmArgSiteArm(&(*pVm),pCallee,pArgName,0,0);` |
|      647 | 4449 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      647 | 4450 | `	VmArgSiteDisarm(&(*pVm));` |
|      647 | 4451 | `	if( pThis == 0 ){` |
|      ! 0 | 4452 | `		return PH7_ABORT;` |
|        - | 4453 | `	}` |
|      647 | 4454 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 4455 | `	/* PHP qualifies a method's type-error with its declaring class ("Class::m()"); a free` |
|        - | 4456 | `	 * function uses the bare name. pOwnerClass is NULL for free functions/closures. */` |
|        - | 4457 | ``	/* A NULL pArgName omits the ` ($name)` clause: php drops the parameter name`` |
|        - | 4458 | `	 * for a VARIADIC-collected element (many values share the one variadic` |
|        - | 4459 | `	 * formal, so no single name applies) — "Argument #2 must be of type …". */` |
|      966 | 4460 | `	if( pOwnerClass ){` |
|        - | 4461 | `		/* A property hook is named after its PROPERTY, never after the method` |
|        - | 4462 | ``		 * PHL synthesizes for it (`C::$p::set`, not `C::__phl_hook_set_p`). */`` |
|        - | 4463 | `		SyBlob sHook;` |
|      105 | 4464 | `		SyBlobInit(&sHook,&pVm->sAllocator);` |
|      105 | 4465 | `		if( PH7_VmHookFuncName(pOwnerClass,pCallee,&sHook) ){` |
|        5 | 4466 | `			if( pArgName ){` |
|        5 | 4467 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) must be of type %s, %s given",` |
|        4 | 4468 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|        2 | 4469 | `					nArg,pArgName,zExpected,zGiven);` |
|        3 | 4470 | `			}else{` |
|      ! 0 | 4471 | `				SyBlobFormat(&sMsg,"%.*s(): Argument #%u must be of type %s, %s given",` |
|      ! 0 | 4472 | `					(int)SyBlobLength(&sHook),(const char *)SyBlobData(&sHook),` |
|      ! 0 | 4473 | `					nArg,zExpected,zGiven);` |
|        - | 4474 | `			}` |
|        5 | 4475 | `			SyBlobRelease(&sHook);` |
|        5 | 4476 | `			goto ArgMsgBuilt;` |
|        - | 4477 | `		}` |
|      101 | 4478 | `		SyBlobRelease(&sHook);` |
|      149 | 4479 | `		VmArgFuncLabel(pVm,&sMsg,pOwnerClass,pCallee,` |
|       96 | 4480 | `			SyStringData(pFuncName),(int)SyStringLength(pFuncName));` |
|      101 | 4481 | `		if( pArgName ){` |
|       87 | 4482 | `			SyBlobFormat(&sMsg,"(): Argument #%u ($%z) must be of type %s, %s given",` |
|       41 | 4483 | `				nArg,pArgName,zExpected,zGiven);` |
|       46 | 4484 | `		}else{` |
|       18 | 4485 | `			SyBlobFormat(&sMsg,"(): Argument #%u must be of type %s, %s given",` |
|        7 | 4486 | `				nArg,zExpected,zGiven);` |
|        - | 4487 | `		}` |
|       53 | 4488 | `	}else{` |
|        - | 4489 | `		/* A closure's internal lookup key ("[closure_3]") is not what php shows. */` |
|      547 | 4490 | `		const char *zShow = 0;` |
|      547 | 4491 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|      547 | 4492 | `		VmArgFuncLabel(pVm,&sMsg,0,pCallee,zShow,nShow);` |
|      547 | 4493 | `		if( pArgName ){` |
|      465 | 4494 | `			SyBlobFormat(&sMsg,"(): Argument #%u ($%z) must be of type %s, %s given",` |
|      230 | 4495 | `				nArg,pArgName,zExpected,zGiven);` |
|      235 | 4496 | `		}else{` |
|       87 | 4497 | `			SyBlobFormat(&sMsg,"(): Argument #%u must be of type %s, %s given",` |
|       41 | 4498 | `				nArg,zExpected,zGiven);` |
|        - | 4499 | `		}` |
|        - | 4500 | `	}` |
|      321 | 4501 | `ArgMsgBuilt:` |
|        - | 4502 | `	/* php appends the CALL SITE to a userland callee's type error — internal` |
|        - | 4503 | `	 * (hosted C) functions get the bare message, and so does a callback an internal` |
|        - | 4504 | `	 * function reached for, whose caller frame is that builtin's rather than user` |
|        - | 4505 | `	 * code. nCurLine is the line of the call instruction being bound, which is` |
|        - | 4506 | `	 * exactly php's "called in". */` |
|      647 | 4507 | `	if( (pCallee->iFlags & VM_FUNC_INTERNAL) == 0 && !VmArgCallerIsNative(pVm,pCallee) ){` |
|        - | 4508 | `		SyString *pCallFile;` |
|        - | 4509 | `		sxu32 nCallLine;` |
|      563 | 4510 | `		VmArgCallSite(pVm,pCallee,&pCallFile,&nCallLine);` |
|      563 | 4511 | `		if( pCallFile && pCallFile->nByte > 0 ){` |
|      563 | 4512 | `			SyBlobFormat(&sMsg,", called in %z on line %u",pCallFile,nCallLine);` |
|      279 | 4513 | `		}` |
|      279 | 4514 | `	}` |
|      647 | 4515 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      647 | 4516 | `	if( pCons ){` |
|      647 | 4517 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      647 | 4518 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      647 | 4519 | `		apArg[0] = &sArg;` |
|      647 | 4520 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      647 | 4521 | `		PH7_MemObjRelease(&sArg);` |
|      321 | 4522 | `	}` |
|      647 | 4523 | `	SyBlobRelease(&sMsg);` |
|      647 | 4524 | `	pFrame = pVm->pFrame;` |
|      647 | 4525 | `	if( pFrame ){` |
|      647 | 4526 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      647 | 4527 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      321 | 4528 | `	}` |
|      647 | 4529 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      647 | 4530 | `	PH7_ClassInstanceUnref(pThis);` |
|      647 | 4531 | `	if( rc == SXERR_ABORT ){` |
|        6 | 4532 | `		return PH7_ABORT;` |
|        - | 4533 | `	}` |
|      643 | 4534 | `	return PH7_EXCEPTION;` |
|      326 | 4535 | `}` |
|        - | 4536 | `/*` |
|        - | 4537 | ` * Type-check (and weak-mode coerce, in place) ONE element collected into a` |
|        - | 4538 | ` * variadic parameter — the shared per-element enforcement for BOTH the` |
|        - | 4539 | ` * positional and the named-argument binding paths of OP_CALL.` |
|        - | 4540 | ` *` |
|        - | 4541 | ` * nArgPos is php's 1-based argument number for the message: a positional` |
|        - | 4542 | ` * element uses its overall call position; a NAMED element always reports` |
|        - | 4543 | ``  * (total positional args) + 1, whichever named element fails. The `($name)` `` |
|        - | 4544 | ` * clause is omitted (pArgName = 0): many values share the one variadic` |
|        - | 4545 | ` * formal, so no single parameter name applies.` |
|        - | 4546 | ` *` |
|        - | 4547 | ` * Returns SXRET_OK when the element passes (possibly coerced), PH7_ABORT or` |
|        - | 4548 | ` * PH7_EXCEPTION after throwing php's TypeError otherwise.` |
|        - | 4549 | ` */` |
|     3577 | 4550 | `PH7_PRIVATE sxi32 VmVariadicElementTypeCheck(ph7_vm *pVm,ph7_class *pSelfHint,ph7_vm_func *pCallee,` |
|        - | 4551 | `	ph7_vm_func_arg *pFormal,ph7_value *pVal,sxu32 nArgPos,int bCallIsStrict)` |
|        5 | 4552 | `{` |
|        - | 4553 | `	sxi32 rc;` |
|     3582 | 4554 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|       33 | 4555 | `		if( VmCoerceToUnion(&(*pVm), pVal, &pFormal->aUnionAlts,` |
|       37 | 4556 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0, bCallIsStrict, pSelfHint) != SXRET_OK ){` |
|        - | 4557 | `			const char *zGiven;` |
|       11 | 4558 | `			const char *zExpected = "union";` |
|        - | 4559 | `			char zBuf[128];` |
|        - | 4560 | `			char zTypeBuf[128];` |
|       11 | 4561 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        3 | 4562 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|       10 | 4563 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 4564 | `				zGiven = "null";` |
|      ! 0 | 4565 | `			}else{` |
|        9 | 4566 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|        - | 4567 | `			}` |
|       11 | 4568 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|       15 | 4569 | `				zExpected = VmHintTextResolved(&(*pVm),&pFormal->sTypeName,pSelfHint,` |
|        4 | 4570 | `					zTypeBuf,sizeof(zTypeBuf));` |
|        4 | 4571 | `			}` |
|       11 | 4572 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,zExpected,zGiven);` |
|       11 | 4573 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 4574 | `		}` |
|       17 | 4575 | `		return SXRET_OK;` |
|        - | 4576 | `	}` |
|     3555 | 4577 | `	if( pFormal->nType < 1` |
|     2028 | 4578 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|     3081 | 4579 | `		return SXRET_OK;` |
|        - | 4580 | `	}` |
|      484 | 4581 | `	if( pFormal->nType == SXU32_HIGH ){` |
|        - | 4582 | `		/* Class or pseudo-type (true/false/iterable/mixed) hint — enforced` |
|        - | 4583 | `		 * per element exactly like the non-variadic paths. */` |
|       64 | 4584 | `		SyString *pName = &pFormal->sClass;` |
|        - | 4585 | `		ph7_class *pClass;` |
|       64 | 4586 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|       64 | 4587 | `		if( rcPseudo == 0 ){` |
|        - | 4588 | `			/* Recognised pseudo-type; value mismatches */` |
|        - | 4589 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       14 | 4590 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|        6 | 4591 | `				VmClassHintTypeName(pName,0,` |
|        6 | 4592 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|        3 | 4593 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        8 | 4594 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 4595 | `		}` |
|        - | 4596 | `		/* rcPseudo==1 -> matched pseudo-type (accept); -1 -> real class, put` |
|        - | 4597 | `		 * through the shared VmClassHintMatches rule (self/parent/static,` |
|        - | 4598 | `		 * interface/abstract hints via iLoadable=FALSE, and a name that resolves` |
|        - | 4599 | `		 * to nothing). Non-nullable here — the guard above skips nullable+null —` |
|        - | 4600 | `		 * so ANY non-object is a TypeError, matching php. */` |
|       58 | 4601 | `		pClass = 0;` |
|       58 | 4602 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|        - | 4603 | `			char zTypeBuf[128],zGivenBuf[128];` |
|       46 | 4604 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       22 | 4605 | `				VmClassHintTypeName(pName,pClass,` |
|       22 | 4606 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|       11 | 4607 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       24 | 4608 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 4609 | `		}` |
|       36 | 4610 | `		return SXRET_OK;` |
|        - | 4611 | `	}` |
|      422 | 4612 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       85 | 4613 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|        - | 4614 | `			char zGivenBuf[128];` |
|        8 | 4615 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|        2 | 4616 | `				"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|        6 | 4617 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 4618 | `		}` |
|       81 | 4619 | `		if( VmEnforceScalarType(pVal, pFormal->nType, bCallIsStrict) != SXRET_OK ){` |
|        - | 4620 | `			char zTypeBuf[128];` |
|        - | 4621 | `			char zGivenBuf[128];` |
|       89 | 4622 | `			rc = VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pCallee,nArgPos,0,` |
|       28 | 4623 | `				VmScalarTypeName(pFormal->nType, &pFormal->sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|       28 | 4624 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf)));` |
|       61 | 4625 | `			return (rc == PH7_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|        - | 4626 | `		}` |
|       12 | 4627 | `	}else{` |
|        - | 4628 | `		/* Mask matched — an int variadic accepting a whole-real materializes` |
|        - | 4629 | `		 * it as a genuine int (php: f(int ...$a) with 1.0 collects int(1)). */` |
|      342 | 4630 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|        - | 4631 | `	}` |
|      362 | 4632 | `	return SXRET_OK;` |
|     1681 | 4633 | `}` |
|        - | 4634 | `/*` |
|        - | 4635 | ` * Count php's REQUIRED arity for a user function: formals up to and including` |
|        - | 4636 | ` * the LAST one with no default value (php 8 treats an optional declared` |
|        - | 4637 | ` * before a required parameter as implicitly required), excluding a trailing` |
|        - | 4638 | ` * variadic. Also reports the total non-variadic formal count so callers can` |
|        - | 4639 | ` * pick php's wording — "exactly N expected" when required == total,` |
|        - | 4640 | ` * "at least N" when trailing optionals exist.` |
|        - | 4641 | ` */` |
|    13184 | 4642 | `PH7_PRIVATE sxu32 VmFuncRequiredArgCount(ph7_vm_func *pFunc,sxu32 *pnNonVariadic)` |
|        5 | 4643 | `{` |
|    13189 | 4644 | `	ph7_vm_func_arg *aFormal = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|    13189 | 4645 | `	sxu32 nFormal = SySetUsed(&pFunc->aArgs);` |
|    13189 | 4646 | `	sxu32 nRequired = 0;` |
|        - | 4647 | `	sxu32 n;` |
|    13189 | 4648 | `	if( nFormal > 0 && (aFormal[nFormal - 1].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|     1322 | 4649 | `		nFormal--;` |
|      611 | 4650 | `	}` |
|    55933 | 4651 | `	for( n = 0 ; n < nFormal ; ++n ){` |
|    42749 | 4652 | `		if( SySetUsed(&aFormal[n].aByteCode) < 1 ){` |
|    16404 | 4653 | `			nRequired = n + 1;` |
|     8181 | 4654 | `		}` |
|    21342 | 4655 | `	}` |
|    13189 | 4656 | `	*pnNonVariadic = nFormal;` |
|    13189 | 4657 | `	return nRequired;` |
|        5 | 4658 | `}` |
|        - | 4659 | `/*` |
|        - | 4660 | ` * Throw php's catchable ArgumentCountError for a user function/method called` |
|        - | 4661 | ` * with too few arguments:` |
|        - | 4662 | ` *   Too few arguments to function C::f(), N passed in FILE on line L and` |
|        - | 4663 | ` *   {exactly\|at least} M expected` |
|        - | 4664 | ` * php embeds the CALL SITE's file+line mid-message; VmInstr carries no line` |
|        - | 4665 | ` * info yet (the runtime line-tracking gate), so the line is a` |
|        - | 4666 | ` * fixed 1 — the SHAPE stays php-exact for --EXPECTF-- tests and self-heals` |
|        - | 4667 | ` * when line tracking lands. bCallSite=FALSE omits the segment entirely,` |
|        - | 4668 | ` * matching php for Fiber::start() (no userland call site in the message).` |
|        - | 4669 | ` */` |
|       94 | 4670 | `PH7_PRIVATE sxi32 VmThrowTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 4671 | `	ph7_vm_func *pCallee,sxu32 nPassed,sxu32 nRequired,sxu32 nNonVariadic,int bCallSite)` |
|        5 | 4672 | `{` |
|        - | 4673 | `	static const SyString sUnknown = { "unknown", sizeof("unknown") - 1 };` |
|        - | 4674 | `	SyBlob sMsg;` |
|        - | 4675 | `	sxi32 rc;` |
|        - | 4676 | ``	/* A CLOSURE has no name of its own: php calls it `{closure:file:line}` (or`` |
|        - | 4677 | ``	 * `{closure:enclosing():line}` for one written inside a function), and this`` |
|        - | 4678 | `	 * message was the last of the four argument diagnostics still printing the` |
|        - | 4679 | ``	 * engine's internal `[closure_N]` instead. */`` |
|        - | 4680 | `	{` |
|       99 | 4681 | `		const char *zShow = 0;` |
|       99 | 4682 | `		int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|       99 | 4683 | `		if( nShow < 1 ){` |
|      ! 0 | 4684 | `			zShow = SyStringData(pFuncName);` |
|      ! 0 | 4685 | `			nShow = (int)SyStringLength(pFuncName);` |
|      ! 0 | 4686 | `		}` |
|       99 | 4687 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       99 | 4688 | `		SyBlobAppend(&sMsg,"Too few arguments to function ",sizeof("Too few arguments to function ")-1);` |
|       99 | 4689 | `		VmArgOwnerPrefix(pVm,&sMsg,pOwnerClass,pCallee);` |
|       99 | 4690 | `		SyBlobFormat(&sMsg,"%.*s(), %u passed",nShow,zShow,nPassed);` |
|        - | 4691 | `	}` |
|       99 | 4692 | `	if( bCallSite ){` |
|        - | 4693 | `		SyString *pFile;` |
|        - | 4694 | `		sxu32 nCallLine;` |
|       73 | 4695 | `		VmArgCallSite(pVm,pCallee,&pFile,&nCallLine);` |
|       39 | 4696 | `		SyBlobFormat(&sMsg," in %z on line %u",` |
|       68 | 4697 | `			(pFile && pFile->nByte > 0) ? (const SyString *)pFile : &sUnknown,nCallLine);` |
|       34 | 4698 | `	}` |
|       99 | 4699 | `	SyBlobFormat(&sMsg," and %s %u expected",` |
|       47 | 4700 | `		nRequired >= nNonVariadic ? "exactly" : "at least",nRequired);` |
|        - | 4701 | `	/* php raises it from the RECV of the first parameter nobody passed. */` |
|       99 | 4702 | `	VmArgSiteArm(pVm,pCallee,0,nPassed,1);` |
|        - | 4703 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       99 | 4704 | `	rc = VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       99 | 4705 | `	VmArgSiteDisarm(pVm);` |
|       99 | 4706 | `	return rc;` |
|        5 | 4707 | `}` |
|        - | 4708 | `/*` |
|        - | 4709 | ` * Throw php's catchable Error for a by-reference parameter handed something that` |
|        - | 4710 | ` * cannot be referenced:` |
|        - | 4711 | ` *   C::m(): Argument #1 ($x) could not be passed by reference` |
|        - | 4712 | ` * php refuses this at the CALL, before the callee's ZPP runs, and it decides from` |
|        - | 4713 | ` * the argument's compile-time SHAPE (VmCallArgMap.nNonLvalMask) rather than from` |
|        - | 4714 | ` * the value that arrived. The class prefix follows the same rule as the too-few` |
|        - | 4715 | ` * ArgumentCountError above — php names the method's owner, and PHL used to report` |
|        - | 4716 | `` * the bare `m()`.`` |
|        - | 4717 | ` */` |
|        - | 4718 | `/*` |
|        - | 4719 | `` * php's `X(): Argument #N ($p) must be passed by reference, value given` — a WARNING,`` |
|        - | 4720 | ` * raised where a by-REFERENCE parameter is handed something the site cannot alias, and` |
|        - | 4721 | ` * then the callee operates on a copy. Two sites reach it: call_user_func_array(), whose` |
|        - | 4722 | ` * argument-array element is a plain VALUE rather than a reference, and Fiber::start(),` |
|        - | 4723 | `` * whose own `...$args` are by value whatever the body declares. The callee is named the`` |
|        - | 4724 | ` * way every other argument diagnostic names it — a method with its class, a closure with` |
|        - | 4725 | `` * php's `{closure:file:line}`.`` |
|        - | 4726 | ` */` |
|      158 | 4727 | `PH7_PRIVATE void PH7_VmWarnByRefValueGiven(ph7_vm *pVm,ph7_class *pOwnerClass,` |
|        - | 4728 | `	ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName)` |
|        3 | 4729 | `{` |
|      161 | 4730 | `	const char *zShow = 0;` |
|      161 | 4731 | `	int nShow = PH7_VmFuncDisplayName(&(*pVm),pCallee,&zShow);` |
|        - | 4732 | ``	/* A NULL pArgName omits the ` ($name)` clause, php's wording for an element`` |
|        - | 4733 | `	 * collected by a by-ref VARIADIC tail: many values share one formal, so no` |
|        - | 4734 | `	 * single name applies (the refusal message splits the same way). */` |
|      161 | 4735 | `	if( pOwnerClass && pArgName ){` |
|       99 | 4736 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 4737 | `			"%z::%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       32 | 4738 | `			&pOwnerClass->sDisp,nShow,zShow,nArgPos,pArgName);` |
|      129 | 4739 | `	}else if( pOwnerClass ){` |
|      ! 0 | 4740 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 4741 | `			"%z::%.*s(): Argument #%u must be passed by reference, value given",` |
|      ! 0 | 4742 | `			&pOwnerClass->sDisp,nShow,zShow,nArgPos);` |
|       97 | 4743 | `	}else if( pArgName ){` |
|      126 | 4744 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 4745 | `			"%.*s(): Argument #%u ($%z) must be passed by reference, value given",` |
|       41 | 4746 | `			nShow,zShow,nArgPos,pArgName);` |
|       44 | 4747 | `	}else{` |
|       20 | 4748 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - | 4749 | `			"%.*s(): Argument #%u must be passed by reference, value given",` |
|        6 | 4750 | `			nShow,zShow,nArgPos);` |
|        - | 4751 | `	}` |
|      161 | 4752 | `}` |
|     4030 | 4753 | `PH7_PRIVATE sxi32 VmThrowByRefRefusal(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 4754 | `	ph7_vm_func *pCallee,sxu32 nArgPos,SyString *pArgName)` |
|        3 | 4755 | `{` |
|        - | 4756 | `	SyBlob sMsg;` |
|     4033 | 4757 | `	const char *zShow = 0;` |
|     4033 | 4758 | `	int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|     4033 | 4759 | `	if( nShow < 1 ){` |
|      ! 0 | 4760 | `		zShow = SyStringData(pFuncName);` |
|      ! 0 | 4761 | `		nShow = (int)SyStringLength(pFuncName);` |
|      ! 0 | 4762 | `	}` |
|     4033 | 4763 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     4033 | 4764 | `	VmArgFuncLabel(pVm,&sMsg,pOwnerClass,pCallee,zShow,nShow);` |
|     4033 | 4765 | `	SyBlobFormat(&sMsg,"(): Argument #%u ($%z) could not be passed by reference",` |
|     2015 | 4766 | `		nArgPos,pArgName);` |
|        - | 4767 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|     4033 | 4768 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        3 | 4769 | `}` |
|        - | 4770 | `/*` |
|        - | 4771 | ` * Throw php's ArgumentCountError for an INTERNAL (builtin-chunk) method` |
|        - | 4772 | ` * called with too few arguments, in php's ZPP wording:` |
|        - | 4773 | ` *   ReflectionProperty::__construct() expects exactly 2 arguments, 0 given` |
|        - | 4774 | ` * (php words internal callables this way — no call-site segment, argument(s)` |
|        - | 4775 | ` * pluralized on the expected count).` |
|        - | 4776 | ` *` |
|        - | 4777 | ` * nMaxDeclared is the TOTAL formal count, variadic tail included — php's ZPP` |
|        - | 4778 | ` * says "exactly" only when the callable accepts no more than it requires, and` |
|        - | 4779 | `` * a variadic tail leaves no maximum at all (`max()` is "at least 1"). That is`` |
|        - | 4780 | ` * NOT the user-function rule VmThrowTooFewArgs applies: php words` |
|        - | 4781 | `` * `function f($a, ...$b)` as "exactly 1 expected", ignoring the variadic.`` |
|        - | 4782 | ` */` |
|       26 | 4783 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooFewArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 4784 | `	sxu32 nPassed,sxu32 nRequired,sxu32 nMaxDeclared)` |
|        1 | 4785 | `{` |
|        - | 4786 | `	SyBlob sMsg;` |
|       27 | 4787 | `	const char *zKind = (nRequired >= nMaxDeclared) ? "exactly" : "at least";` |
|       27 | 4788 | `	const char *zPlural = (nRequired == 1) ? "" : "s";` |
|       27 | 4789 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       27 | 4790 | `	if( pOwnerClass ){` |
|      ! 0 | 4791 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      ! 0 | 4792 | `			&pOwnerClass->sDisp,pFuncName,zKind,nRequired,zPlural,nPassed);` |
|      ! 0 | 4793 | `	}else{` |
|       27 | 4794 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|       13 | 4795 | `			pFuncName,zKind,nRequired,zPlural,nPassed);` |
|        - | 4796 | `	}` |
|        - | 4797 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       27 | 4798 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 4799 | `}` |
|        - | 4800 | `/*` |
|        - | 4801 | ` * php's ArgumentCountError for an INTERNAL (builtin-chunk) callable handed too` |
|        - | 4802 | ` * MANY arguments, in php's ZPP wording:` |
|        - | 4803 | ` *   count_chars() expects at most 2 arguments, 3 given` |
|        - | 4804 | ` * "exactly" when the bounds coincide, "at most" when optional parameters make` |
|        - | 4805 | ` * them differ; the plural follows the MAXIMUM, so a zero-parameter builtin` |
|        - | 4806 | ` * reports "exactly 0 arguments". A variadic tail leaves php no maximum at all,` |
|        - | 4807 | ` * so the caller must not route such a callee here.` |
|        - | 4808 | ` */` |
|       22 | 4809 | `PH7_PRIVATE sxi32 VmThrowBuiltinTooManyArgs(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 4810 | `	sxu32 nPassed,sxu32 nMax,sxu32 nRequired)` |
|        1 | 4811 | `{` |
|        - | 4812 | `	SyBlob sMsg;` |
|       23 | 4813 | `	const char *zKind = (nRequired >= nMax) ? "exactly" : "at most";` |
|       23 | 4814 | `	const char *zPlural = (nMax == 1) ? "" : "s";` |
|       23 | 4815 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       23 | 4816 | `	if( pOwnerClass ){` |
|      ! 0 | 4817 | `		SyBlobFormat(&sMsg,"%z::%z() expects %s %u argument%s, %u given",` |
|      ! 0 | 4818 | `			&pOwnerClass->sDisp,pFuncName,zKind,nMax,zPlural,nPassed);` |
|      ! 0 | 4819 | `	}else{` |
|       23 | 4820 | `		SyBlobFormat(&sMsg,"%z() expects %s %u argument%s, %u given",` |
|       11 | 4821 | `			pFuncName,zKind,nMax,zPlural,nPassed);` |
|        - | 4822 | `	}` |
|        - | 4823 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       23 | 4824 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 4825 | `}` |
|        - | 4826 | `/*` |
|        - | 4827 | ` * php's refusal of an unknown-name EXTRA by an INTERNAL (builtin-chunk) variadic,` |
|        - | 4828 | ` * raised from inside the callee like PH7_VmRefuseExtraNamed is for a host function:` |
|        - | 4829 | ` *   array_replace_recursive() does not accept unknown named parameters` |
|        - | 4830 | ` */` |
|       20 | 4831 | `PH7_PRIVATE sxi32 VmThrowBuiltinExtraNamed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName)` |
|        1 | 4832 | `{` |
|        - | 4833 | `	SyBlob sMsg;` |
|       21 | 4834 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       21 | 4835 | `	if( pOwnerClass ){` |
|      ! 0 | 4836 | `		SyBlobFormat(&sMsg,"%z::%z() does not accept unknown named parameters",` |
|      ! 0 | 4837 | `			&pOwnerClass->sDisp,pFuncName);` |
|      ! 0 | 4838 | `	}else{` |
|       21 | 4839 | `		SyBlobFormat(&sMsg,"%z() does not accept unknown named parameters",pFuncName);` |
|        - | 4840 | `	}` |
|        - | 4841 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       21 | 4842 | `	return VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|        1 | 4843 | `}` |
|        - | 4844 | `/*` |
|        - | 4845 | ` * Throw php's named-call ArgumentCountError for a required parameter no` |
|        - | 4846 | ` * named or positional argument resolved to:` |
|        - | 4847 | ` *   C::f(): Argument #N ($x) not passed` |
|        - | 4848 | ` * (php's named-hole shape — no file/line or expected-count segment).` |
|        - | 4849 | ` */` |
|       44 | 4850 | `PH7_PRIVATE sxi32 VmThrowArgNotPassed(ph7_vm *pVm,ph7_class *pOwnerClass,SyString *pFuncName,` |
|        - | 4851 | `	ph7_vm_func *pCallee,sxu32 nArg,SyString *pArgName)` |
|        4 | 4852 | `{` |
|        - | 4853 | `	SyBlob sMsg;` |
|        - | 4854 | `	sxi32 rc;` |
|       48 | 4855 | `	const char *zShow = 0;` |
|       48 | 4856 | `	int nShow = PH7_VmFuncDisplayName(pVm,pCallee,&zShow);` |
|       48 | 4857 | `	if( nShow < 1 ){` |
|      ! 0 | 4858 | `		zShow = SyStringData(pFuncName);` |
|      ! 0 | 4859 | `		nShow = (int)SyStringLength(pFuncName);` |
|      ! 0 | 4860 | `	}` |
|       48 | 4861 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       48 | 4862 | `	VmArgOwnerPrefix(pVm,&sMsg,pOwnerClass,pCallee);` |
|       48 | 4863 | `	SyBlobFormat(&sMsg,"%.*s(): Argument #%u ($%z) not passed",nShow,zShow,nArg,pArgName);` |
|       48 | 4864 | `	VmArgSiteArm(pVm,pCallee,0,nArg - 1,1);` |
|        - | 4865 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       48 | 4866 | `	rc = VmThrowBuiltinError(pVm,"ArgumentCountError",sizeof("ArgumentCountError")-1,&sMsg);` |
|       48 | 4867 | `	VmArgSiteDisarm(pVm);` |
|       48 | 4868 | `	return rc;` |
|        4 | 4869 | `}` |
|        - | 4870 | `/*` |
|        - | 4871 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - | 4872 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - | 4873 | ` */` |
|        - | 4874 | `/* Build a catchable TypeError from a pre-formatted message blob and throw it.` |
|        - | 4875 | ` * The message is copied into the instance by __construct, so the caller owns` |
|        - | 4876 | ` * (and releases) pMsg. Sets VM_FRAME_THROW so the terminal OP_DONE unwinds. */` |
|        - | 4877 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev);` |
|      158 | 4878 | `static sxi32 VmThrowTypeErrorMsg(ph7_vm *pVm,SyBlob *pMsg)` |
|        5 | 4879 | `{` |
|        - | 4880 | `	ph7_class *pClass;` |
|        - | 4881 | `	ph7_class_instance *pThis;` |
|        - | 4882 | `	ph7_class_method *pCons;` |
|        - | 4883 | `	ph7_value sArg;` |
|        - | 4884 | `	ph7_value *apArg[1];` |
|        - | 4885 | `	SyString sMsgStr;` |
|        - | 4886 | `	VmFrame *pFrame;` |
|        - | 4887 | `	sxi32 rc;` |
|      163 | 4888 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|      163 | 4889 | `	if( pClass == 0 ){` |
|      ! 0 | 4890 | `		return PH7_ABORT;` |
|        - | 4891 | `	}` |
|      163 | 4892 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      163 | 4893 | `	if( pThis == 0 ){` |
|      ! 0 | 4894 | `		return PH7_ABORT;` |
|        - | 4895 | `	}` |
|      163 | 4896 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      163 | 4897 | `	if( pCons ){` |
|      163 | 4898 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(pMsg),SyBlobLength(pMsg));` |
|      163 | 4899 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      163 | 4900 | `		apArg[0] = &sArg;` |
|      163 | 4901 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      163 | 4902 | `		PH7_MemObjRelease(&sArg);` |
|       79 | 4903 | `	}` |
|      163 | 4904 | `	if( pVm->pReturnTypeExc ){` |
|        - | 4905 | ``		/* The return check's `callable` arm refused because an error handler threw. */`` |
|        9 | 4906 | `		VmExceptionLinkPrevious(pThis,pVm->pReturnTypeExc);` |
|        9 | 4907 | `		PH7_ClassInstanceUnref(pVm->pReturnTypeExc);` |
|        9 | 4908 | `		pVm->pReturnTypeExc = 0;` |
|        4 | 4909 | `	}` |
|      163 | 4910 | `	pFrame = pVm->pFrame;` |
|      163 | 4911 | `	if( pFrame ){` |
|      163 | 4912 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      163 | 4913 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       79 | 4914 | `	}` |
|      163 | 4915 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      163 | 4916 | `	PH7_ClassInstanceUnref(pThis);` |
|      163 | 4917 | `	if( rc == SXERR_ABORT ){` |
|        6 | 4918 | `		return PH7_ABORT;` |
|        - | 4919 | `	}` |
|      159 | 4920 | `	return PH7_EXCEPTION;` |
|       84 | 4921 | `}` |
|        - | 4922 | `/*` |
|        - | 4923 | `` * php names a property HOOK after the property it belongs to — `C::$p::get()`,`` |
|        - | 4924 | `` * `C::$p::set()` — never after a method, because in php a hook is not one. PHL`` |
|        - | 4925 | `` * synthesizes each hook as a hidden method `__phl_hook_get_NAME` /`` |
|        - | 4926 | `` * `__phl_hook_set_NAME` on the declaring class, so the php-facing name is a`` |
|        - | 4927 | ` * prefix rewrite. Returns TRUE (and writes pOut) only for such a method; every` |
|        - | 4928 | ` * other callee falls through to the ordinary Class::method rendering.` |
|        - | 4929 | ` */` |
|        - | 4930 | `#define PH7_HOOK_METH_PFX "__phl_hook_"` |
|   652925 | 4931 | `PH7_PRIVATE int PH7_VmHookSplitName(SyString *pName,SyString *pProp,const char **pzKind)` |
|        5 | 4932 | `{` |
|   652930 | 4933 | `	const sxu32 nPfx = sizeof(PH7_HOOK_METH_PFX)-1;` |
|   652930 | 4934 | `	if( pName->zString == 0 \|\| pName->nByte <= nPfx + 4 ){` |
|   652450 | 4935 | `		return 0;` |
|        - | 4936 | `	}` |
|      485 | 4937 | `	if( SyMemcmp(pName->zString,PH7_HOOK_METH_PFX,nPfx) != 0 ){` |
|      442 | 4938 | `		return 0;` |
|        - | 4939 | `	}` |
|       47 | 4940 | `	if( SyMemcmp(&pName->zString[nPfx],"get_",4) == 0 ){` |
|       34 | 4941 | `		*pzKind = "get";` |
|       30 | 4942 | `	}else if( SyMemcmp(&pName->zString[nPfx],"set_",4) == 0 ){` |
|       15 | 4943 | `		*pzKind = "set";` |
|        9 | 4944 | `	}else{` |
|      ! 0 | 4945 | `		return 0;` |
|        - | 4946 | `	}` |
|       47 | 4947 | `	SyStringInitFromBuf(pProp,&pName->zString[nPfx+4],pName->nByte-(nPfx+4));` |
|       47 | 4948 | `	return 1;` |
|   326415 | 4949 | `}` |
|      204 | 4950 | `PH7_PRIVATE int PH7_VmHookFuncName(ph7_class *pClass,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        5 | 4951 | `{` |
|        - | 4952 | `	SyString sProp;` |
|        - | 4953 | `	const char *zKind;` |
|      209 | 4954 | `	if( pClass == 0 \|\| !PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|      199 | 4955 | `		return 0;` |
|        - | 4956 | `	}` |
|       12 | 4957 | `	SyBlobFormat(pOut,"%z::$%z::%s",&pClass->sDisp,&sProp,zKind);` |
|       12 | 4958 | `	return 1;` |
|      107 | 4959 | `}` |
|        - | 4960 | `/*` |
|        - | 4961 | ` * The callee name php puts in front of a return-side message. A METHOD is` |
|        - | 4962 | ` * qualified with its DECLARING class ("P::m", even when called on a subclass);` |
|        - | 4963 | ` * anything else uses its display name (which is also what strips a closure's` |
|        - | 4964 | ` * internal key). The argument-side messages take the owner class as a parameter` |
|        - | 4965 | ` * instead — they are thrown from call sites that already resolved it.` |
|        - | 4966 | ` */` |
|      222 | 4967 | `static void VmReturnFuncName(ph7_vm *pVm,ph7_vm_func *pFunc,SyBlob *pOut)` |
|        5 | 4968 | `{` |
|      227 | 4969 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        - | 4970 | `		/* ...and a TRAIT method's declaring class is the trait, which php does not` |
|        - | 4971 | `		 * have at run time: it composed the method INTO the using class, and names` |
|        - | 4972 | `` 		 * that class here. `trait T { function m(): self {…} } class C { use T; }` `` |
|        - | 4973 | ``		 * reported `T::m(): Return value must be…` where php says `C::m()`, while`` |
|        - | 4974 | `		 * the ARGUMENT-side twin (which is handed an already-resolved owner) said` |
|        - | 4975 | ``		 * `C::m()` in the same function. PH7_VmMemberOwnerClass is the shared walk;`` |
|        - | 4976 | `		 * it needs the class the call was made THROUGH to find the user, and answers` |
|        - | 4977 | `		 * 0 only when there is none — keep the declaring class for that. */` |
|      109 | 4978 | `		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|      109 | 4979 | `		ph7_class *pOwner = PH7_VmMemberOwnerClass(pDecl,PH7_VmPeekTopClass(pVm));` |
|      109 | 4980 | `		if( pOwner == 0 ){` |
|      ! 0 | 4981 | `			pOwner = pDecl;` |
|      ! 0 | 4982 | `		}` |
|      109 | 4983 | `		if( PH7_VmHookFuncName(pOwner,pFunc,pOut) ){` |
|        7 | 4984 | `			return;` |
|        - | 4985 | `		}` |
|      103 | 4986 | `		SyBlobFormat(pOut,"%z::%z",&pOwner->sDisp,&pFunc->sName);` |
|      103 | 4987 | `		return;` |
|        - | 4988 | `	}` |
|        - | 4989 | `	{` |
|      123 | 4990 | `		const char *zShow = 0;` |
|      123 | 4991 | `		int nShow = PH7_VmFuncDisplayName(pVm,pFunc,&zShow);` |
|      123 | 4992 | `		if( zShow && nShow > 0 ){` |
|      123 | 4993 | `			SyBlobAppend(pOut,zShow,(sxu32)nShow);` |
|       59 | 4994 | `		}` |
|        - | 4995 | `	}` |
|      116 | 4996 | `}` |
|      154 | 4997 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zExpected,const char *zGiven)` |
|        5 | 4998 | `{` |
|        - | 4999 | `	SyBlob sMsg,sName;` |
|        - | 5000 | `	sxi32 rc;` |
|      159 | 5001 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      159 | 5002 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|      159 | 5003 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|      159 | 5004 | `	SyBlobFormat(&sMsg,"%.*s(): Return value must be of type %s, %s returned",` |
|      154 | 5005 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpected,zGiven);` |
|      159 | 5006 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|      159 | 5007 | `	SyBlobRelease(&sName);` |
|      159 | 5008 | `	SyBlobRelease(&sMsg);` |
|      159 | 5009 | `	return rc;` |
|        5 | 5010 | `}` |
|        - | 5011 | `/* A never-returning function that returned normally (fall-off). PHP bans an` |
|        - | 5012 | `` * explicit `return` at compile time, so this fires only for an implicit return.`` |
|        - | 5013 | ` * php calls it a "method" when it is one. */` |
|        4 | 5014 | `static sxi32 VmThrowNeverReturnError(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|        2 | 5015 | `{` |
|        - | 5016 | `	SyBlob sMsg,sName;` |
|        - | 5017 | `	sxi32 rc;` |
|        6 | 5018 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        6 | 5019 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|        6 | 5020 | `	VmReturnFuncName(pVm,pFunc,&sName);` |
|        6 | 5021 | `	SyBlobFormat(&sMsg,"%.*s(): never-returning %s must not implicitly return",` |
|        4 | 5022 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),` |
|        4 | 5023 | `		(pFunc->iFlags & VM_FUNC_CLASS_METHOD) ? "method" : "function");` |
|        6 | 5024 | `	rc = VmThrowTypeErrorMsg(pVm,&sMsg);` |
|        6 | 5025 | `	SyBlobRelease(&sName);` |
|        6 | 5026 | `	SyBlobRelease(&sMsg);` |
|        6 | 5027 | `	return rc;` |
|        2 | 5028 | `}` |
|        - | 5029 | `/*` |
|        - | 5030 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - | 5031 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - | 5032 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - | 5033 | ` *` |
|        - | 5034 | ` * A class name LONGER than the buffer is answered from the class itself rather` |
|        - | 5035 | ` * than cut down to fit: PH7_NewClass() duplicates the name with` |
|        - | 5036 | ` * SyMemBackendStrDup(), which appends the NUL, and the class outlives the` |
|        - | 5037 | ` * message being built. php prints the whole name however long it is, and a` |
|        - | 5038 | ` * truncated one would name a class that does not exist.` |
|        - | 5039 | ` */` |
|     1912 | 5040 | `PH7_PRIVATE const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        5 | 5041 | `{` |
|     1917 | 5042 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      178 | 5043 | `		return pVal->x.iVal ? "true" : "false";` |
|        - | 5044 | `	}` |
|     1744 | 5045 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      164 | 5046 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      164 | 5047 | `		if( pThis && pThis->pClass ){` |
|      164 | 5048 | `			SyString *pName = &pThis->pClass->sName;` |
|      164 | 5049 | `			if( pName->nByte >= nBuf ){` |
|        3 | 5050 | `				return pName->zString;` |
|        - | 5051 | `			}` |
|      162 | 5052 | `			SyMemcpy(pName->zString,zBuf,pName->nByte);` |
|      162 | 5053 | `			zBuf[pName->nByte] = 0;` |
|      162 | 5054 | `			return zBuf;` |
|        - | 5055 | `		}` |
|      ! 0 | 5056 | `		return "object";` |
|        - | 5057 | `	}` |
|     1585 | 5058 | `	return ph7_type_name(pVal);` |
|      956 | 5059 | `}` |
|        - | 5060 | `/*` |
|        - | 5061 | ` * Throw a PHP-compatible Error when unpacking ('...$expr') receives something` |
|        - | 5062 | ` * that is neither an array nor a Traversable. Matches the message and class PHP` |
|        - | 5063 | ` * raises ("Only arrays and Traversables can be unpacked, X given").` |
|        - | 5064 | ` *` |
|        - | 5065 | ` * php picks the class from BOTH the value and the site: an ARRAY-literal unpack` |
|        - | 5066 | `` * (`[...$x]`) is \TypeError for an object and plain \Error for every scalar,`` |
|        - | 5067 | `` * while an ARGUMENT unpack (`f(...$x)`, `new C(...$x)`) is \TypeError for all of`` |
|        - | 5068 | ` * them — bArgUnpack says which site is asking.` |
|        - | 5069 | ` */` |
|       90 | 5070 | `PH7_PRIVATE sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad,int bArgUnpack)` |
|        5 | 5071 | `{` |
|        - | 5072 | `	ph7_class *pClass;` |
|        - | 5073 | `	ph7_class_instance *pThis;` |
|        - | 5074 | `	ph7_class_method *pCons;` |
|        - | 5075 | `	ph7_value sArg;` |
|        - | 5076 | `	ph7_value *apArg[1];` |
|        - | 5077 | `	SyBlob sMsg;` |
|        - | 5078 | `	SyString sMsgStr;` |
|        - | 5079 | `	VmFrame *pFrame;` |
|        - | 5080 | `	sxi32 rc;` |
|       95 | 5081 | `	const char *zErrClass = (bArgUnpack \|\| (pBad->iFlags & MEMOBJ_OBJ)) ? "TypeError" : "Error";` |
|        - | 5082 | `	char zNameBuf[64];` |
|       95 | 5083 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       95 | 5084 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       95 | 5085 | `	if( pClass == 0 ){` |
|      ! 0 | 5086 | `		return PH7_ABORT;` |
|        - | 5087 | `	}` |
|       95 | 5088 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       95 | 5089 | `	if( pThis == 0 ){` |
|      ! 0 | 5090 | `		return PH7_ABORT;` |
|        - | 5091 | `	}` |
|       95 | 5092 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       95 | 5093 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       95 | 5094 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       95 | 5095 | `	if( pCons ){` |
|       95 | 5096 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       95 | 5097 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       95 | 5098 | `		apArg[0] = &sArg;` |
|       95 | 5099 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       95 | 5100 | `		PH7_MemObjRelease(&sArg);` |
|       45 | 5101 | `	}` |
|       95 | 5102 | `	SyBlobRelease(&sMsg);` |
|       95 | 5103 | `	pFrame = pVm->pFrame;` |
|       95 | 5104 | `	if( pFrame ){` |
|       95 | 5105 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       95 | 5106 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       45 | 5107 | `	}` |
|       95 | 5108 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       95 | 5109 | `	PH7_ClassInstanceUnref(pThis);` |
|       95 | 5110 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 5111 | `		return PH7_ABORT;` |
|        - | 5112 | `	}` |
|       95 | 5113 | `	return PH7_EXCEPTION;` |
|       50 | 5114 | `}` |
|        - | 5115 | `/*` |
|        - | 5116 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - | 5117 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - | 5118 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - | 5119 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - | 5120 | ` */` |
|        - | 5121 | `/*` |
|        - | 5122 | ` * TRUE if a function declares a return type that must be enforced — a single` |
|        - | 5123 | ` * type (nReturnType) OR a union/intersection (aReturnUnion, where nReturnType is` |
|        - | 5124 | ` * left 0). The return-enforcement gates must consult both, not just the single` |
|        - | 5125 | ` * type field.` |
|        - | 5126 | ` */` |
|   941502 | 5127 | `PH7_PRIVATE int VmFuncHasReturnType(ph7_vm_func *pFunc)` |
|        5 | 5128 | `{` |
|   941507 | 5129 | `	return pFunc->nReturnType > 0 \|\| SySetUsed(&pFunc->aReturnUnion) > 0;` |
|        5 | 5130 | `}` |
|        - | 5131 | `static sxi32 VmEnforceReturnTypeCheck(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue);` |
|    27768 | 5132 | `PH7_PRIVATE sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        5 | 5133 | `{` |
|    27773 | 5134 | `	int bFenceIn = pVm->bReturnTypeFence;` |
|    27773 | 5135 | `	ph7_class_instance *pExcIn = pVm->pReturnTypeExc;` |
|        - | 5136 | `	sxi32 rc;` |
|    27773 | 5137 | `	pVm->bReturnTypeFence = 1;` |
|    27773 | 5138 | `	pVm->pReturnTypeExc = 0;` |
|    27773 | 5139 | `	rc = VmEnforceReturnTypeCheck(&(*pVm),pFunc,pValue);` |
|    27773 | 5140 | `	if( pVm->pReturnTypeExc ){` |
|      ! 0 | 5141 | `		PH7_ClassInstanceUnref(pVm->pReturnTypeExc); /* another arm took the value */` |
|      ! 0 | 5142 | `	}` |
|    27773 | 5143 | `	pVm->bReturnTypeFence = bFenceIn;` |
|    27773 | 5144 | `	pVm->pReturnTypeExc = pExcIn;` |
|    27773 | 5145 | `	return rc;` |
|        5 | 5146 | `}` |
|    27768 | 5147 | `static sxi32 VmEnforceReturnTypeCheck(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        5 | 5148 | `{` |
|    27773 | 5149 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|    27773 | 5150 | `	int bNullable = (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ? 1 : 0;` |
|        - | 5151 | `	const char *zGiven;` |
|        - | 5152 | `	ph7_class *pHintScope;` |
|        - | 5153 | `	char zBuf[128];` |
|        - | 5154 | `	char zTypeBuf[128];` |
|        - | 5155 | `	/* Untyped function: no enforcement (no single type and no union/intersection). */` |
|    27773 | 5156 | `	if( !VmFuncHasReturnType(pFunc) ){` |
|      ! 0 | 5157 | `		return SXRET_OK;` |
|        - | 5158 | `	}` |
|        - | 5159 | `	/* never return type: the function must not return at all. An explicit` |
|        - | 5160 | ``	 * `return` is a compile error, so reaching here means the function ran off`` |
|        - | 5161 | `	 * the end normally (a throw/exit is skipped by the VM_FRAME_THROW guard at` |
|        - | 5162 | `	 * the call site). */` |
|    27773 | 5163 | `	if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        6 | 5164 | `		return VmThrowNeverReturnError(pVm,pFunc);` |
|        - | 5165 | `	}` |
|        - | 5166 | `	/* void return type: the function must not produce a value. */` |
|    27769 | 5167 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     6015 | 5168 | `		if( pValue == 0 ){` |
|     6005 | 5169 | `			return SXRET_OK;` |
|        - | 5170 | `		}` |
|        - | 5171 | ``		/* `set => expr` is php's SHORTHAND for assigning expr to the backing`` |
|        - | 5172 | `		 * store, not a return: php compiles no return statement there at all,` |
|        - | 5173 | `		 * and still reports the hook's return type as void. PHL carries the` |
|        - | 5174 | `		 * value out of the hook body to hand it to the write-back dispatcher,` |
|        - | 5175 | `		 * so the one implicit value this arm must not reject is that one. */` |
|       12 | 5176 | `		if( pFunc->iFlags & VM_FUNC_HOOK_SET_EXPR ){` |
|       12 | 5177 | `			return SXRET_OK;` |
|        - | 5178 | `		}` |
|        - | 5179 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - | 5180 | `		 * still counts as "returned a value" here. */` |
|      ! 0 | 5181 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 | 5182 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"void",zGiven);` |
|        - | 5183 | `	}` |
|        - | 5184 | ``	/* Fell off the end or a bare `return;` with no value: PHP requires any typed`` |
|        - | 5185 | `	 * return (even a nullable one) to return a value explicitly — only an explicit` |
|        - | 5186 | ``	 * `return null;` satisfies a nullable type, which is handled below. */`` |
|    21759 | 5187 | `	if( pValue == 0 ){` |
|       32 | 5188 | `		const char *zExpected = "value";` |
|       32 | 5189 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       47 | 5190 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,` |
|       15 | 5191 | `				VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0),zTypeBuf,sizeof(zTypeBuf));` |
|       15 | 5192 | `		}` |
|        - | 5193 | `` 		/* php's word for "no value at all" is `none`, not `null` — `null returned` `` |
|        - | 5194 | ``		 * is what it says for an explicit `return null;`, a different program. */`` |
|       32 | 5195 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,"none");` |
|        - | 5196 | `	}` |
|        - | 5197 | ``	/* standalone `null` return type (PHP 8.2): an explicit non-null return is a`` |
|        - | 5198 | `	 * TypeError. (Falling off the end is handled by the generic check above,` |
|        - | 5199 | `	 * matching how every other typed return reports a missing value.) */` |
|    21729 | 5200 | `	if( pFunc->nReturnType == MEMOBJ_NULL ){` |
|        5 | 5201 | `		if( pValue->iFlags & MEMOBJ_NULL ){` |
|        3 | 5202 | `			return SXRET_OK;` |
|        - | 5203 | `		}` |
|        4 | 5204 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,"null",` |
|        1 | 5205 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 5206 | `	}` |
|        - | 5207 | ``	/* An explicit `return null` satisfies any nullable return type (`?T`, `T\|null`,`` |
|        - | 5208 | ``	 * `A\|B\|null`) uniformly — handle it before the per-shape branches below, none`` |
|        - | 5209 | `	 * of which (scalar/class/union) carry their own nullable check. */` |
|    21725 | 5210 | `	if( (pValue->iFlags & MEMOBJ_NULL) && bNullable ){` |
|       49 | 5211 | `		return SXRET_OK;` |
|        - | 5212 | `	}` |
|        - | 5213 | ``	/* Pseudo-types parsed as class-name atoms: `mixed` (any value),`` |
|        - | 5214 | ``	 * `true`/`false` (the matching bool literal), `iterable` (array\|Traversable).`` |
|        - | 5215 | `	 * Check by value before the real-class instanceof branch below. */` |
|    21681 | 5216 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|     1059 | 5217 | `		int rcPseudo = VmCheckPseudoType(pVm, pValue, &pFunc->sReturnClass);` |
|     1059 | 5218 | `		if( rcPseudo == 1 ){` |
|      192 | 5219 | `			return SXRET_OK;` |
|        - | 5220 | `		}` |
|      871 | 5221 | `		if( rcPseudo == 0 ){` |
|       31 | 5222 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       14 | 5223 | `				VmClassHintTypeName(&pFunc->sReturnClass,0,bNullable,zTypeBuf,sizeof(zTypeBuf)),` |
|        7 | 5224 | `				VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 5225 | `		}` |
|        - | 5226 | `		/* rcPseudo == -1: a real class — fall through to the instanceof branch. */` |
|      426 | 5227 | `	}` |
|        - | 5228 | `	/* The two branches below are the only ones that can name a class, so the` |
|        - | 5229 | `	 * hint scope is resolved here rather than on every scalar/void return.` |
|        - | 5230 | ``	 * `self`/`parent` in a return hint resolve against the class that DECLARED`` |
|        - | 5231 | `	 * the callee — the check runs inside the callee's own frame, so the same walk` |
|        - | 5232 | ``	 * `self::` uses answers it (a closure's bound scope included). The self-STACK`` |
|        - | 5233 | `` 	 * top is the late-static-binding class, which made an inherited `: self` `` |
|        - | 5234 | `	 * demand the SUBCLASS. See VmHintScopeClass. */` |
|    21479 | 5235 | `	pHintScope = VmHintScopeClass(pVm,PH7_VmPeekDeclaringClass(pVm),0);` |
|        - | 5236 | `	/* Union/intersection return type — delegate. A null alternative is not stored` |
|        - | 5237 | `	 * in aReturnUnion (dropped at parse), so nullability comes from the func's` |
|        - | 5238 | `	 * VM_FUNC_RETURN_NULLABLE flag (already consumed above for an explicit null). */` |
|    21479 | 5239 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - | 5240 | `		sxi32 rcU;` |
|     3191 | 5241 | `		const char *zExpected = "union";` |
|     3191 | 5242 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict, pHintScope);` |
|     3191 | 5243 | `		if( rcU == SXRET_OK ){` |
|     3179 | 5244 | `			return SXRET_OK;` |
|        - | 5245 | `		}` |
|       15 | 5246 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       11 | 5247 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|        9 | 5248 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 5249 | `			zGiven = "null";` |
|      ! 0 | 5250 | `		}else{` |
|        5 | 5251 | `			zGiven = VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|        - | 5252 | `		}` |
|       15 | 5253 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|       21 | 5254 | `			zExpected = VmHintTextResolved(pVm,&pFunc->sReturnTypeName,pHintScope,` |
|        6 | 5255 | `				zTypeBuf,sizeof(zTypeBuf));` |
|        6 | 5256 | `		}` |
|       15 | 5257 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,zExpected,zGiven);` |
|        - | 5258 | `	}` |
|        - | 5259 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - | 5260 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - | 5261 | `	 * it into the TypeError message. */` |
|    18293 | 5262 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|      857 | 5263 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|      857 | 5264 | `		ph7_class *pExpected = 0;` |
|      857 | 5265 | `		if( !VmClassHintMatches(pVm,pClassName,pHintScope,pValue,&pExpected) ){` |
|       37 | 5266 | `			if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       30 | 5267 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       17 | 5268 | `			}else{` |
|        8 | 5269 | `				zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : VmValueGivenName(pValue,zBuf,sizeof(zBuf));` |
|        - | 5270 | `			}` |
|       53 | 5271 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       16 | 5272 | `				VmClassHintTypeName(pClassName,pExpected,bNullable,zTypeBuf,sizeof(zTypeBuf)),zGiven);` |
|        - | 5273 | `		}` |
|      825 | 5274 | `		return SXRET_OK;` |
|        - | 5275 | `	}` |
|        - | 5276 | `	/* Scalar return type. A nullable scalar accepting null was already handled by` |
|        - | 5277 | `	 * the unified MEMOBJ_NULL+bNullable check above, so any null reaching here is a` |
|        - | 5278 | `	 * non-nullable scalar return — a TypeError. */` |
|    17441 | 5279 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       25 | 5280 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        8 | 5281 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - | 5282 | `			"null");` |
|        - | 5283 | `	}` |
|        - | 5284 | ``	/* Exact match? Done. An `: int` return accepting a whole-real`` |
|        - | 5285 | ``	 * materializes it (php: `return 1.0` from `: int` yields int(1)). */`` |
|    17425 | 5286 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|    17301 | 5287 | `		VmMaterializeIntTyped(pValue,pFunc->nReturnType);` |
|    17301 | 5288 | `		return SXRET_OK;` |
|        - | 5289 | `	}` |
|        - | 5290 | `	/* Object->scalar is never compatible, EXCEPT an object with __toString()` |
|        - | 5291 | ``	 * returned as a `string`: php coerces it in weak mode. Fall through to`` |
|        - | 5292 | `	 * VmEnforceScalarType below, which invokes __toString in weak mode and` |
|        - | 5293 | `	 * still rejects the object under strict_types. */` |
|      129 | 5294 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       21 | 5295 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       30 | 5296 | `		if( !(pFunc->nReturnType == MEMOBJ_STRING && pInst && pInst->pClass` |
|       18 | 5297 | `		      && PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1)) ){` |
|       19 | 5298 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|       28 | 5299 | `			return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        9 | 5300 | `				VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        9 | 5301 | `				zGiven);` |
|        - | 5302 | `		}` |
|        1 | 5303 | `	}` |
|        - | 5304 | `	/* Array <-> scalar is never compatible. */` |
|      111 | 5305 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|       32 | 5306 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|       10 | 5307 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|       10 | 5308 | `			VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|        - | 5309 | `	}` |
|        - | 5310 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - | 5311 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - | 5312 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - | 5313 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|       86 | 5314 | `	if( !bStrict` |
|       85 | 5315 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|       51 | 5316 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|       56 | 5317 | `	 && !PH7_MemObjStringIsNumeric(pValue) ){` |
|       16 | 5318 | `		return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        4 | 5319 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - | 5320 | `			"string");` |
|        - | 5321 | `	}` |
|       81 | 5322 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|       79 | 5323 | `		return SXRET_OK;` |
|        - | 5324 | `	}` |
|        4 | 5325 | `	return VmThrowTypeErrorForReturn(pVm,pFunc,` |
|        1 | 5326 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 | 5327 | `		VmValueGivenName(pValue,zBuf,sizeof(zBuf)));` |
|    13680 | 5328 | `}` |
|        - | 5329 | `/*` |
|        - | 5330 | ` * Report a fatal named-argument error.` |
|        - | 5331 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - | 5332 | ` */` |
|      326 | 5333 | `PH7_PRIVATE sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        5 | 5334 | `{` |
|        - | 5335 | `	SyBlob sMsg;` |
|        - | 5336 | `` 	/* php raises these as CATCHABLE \Error (`try { f(b:1); } catch (Error $e)` `` |
|        - | 5337 | `	 * runs the catch), so build a real exception object and hand the resulting` |
|        - | 5338 | `	 * PH7_EXCEPTION back for the caller to route. This used to report straight` |
|        - | 5339 | `	 * to the uncaught renderer, which made every named-argument mistake an` |
|        - | 5340 | `	 * unconditional fatal even inside try/catch. */` |
|      331 | 5341 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      331 | 5342 | `	SyBlobAppend(&sMsg,zMsg,nMsg);` |
|      331 | 5343 | `	return VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 5344 | `}` |
|        - | 5345 | `/*` |
|        - | 5346 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - | 5347 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - | 5348 | ` * information.` |
|        - | 5349 | ` * ------------------------------------` |
|        - | 5350 | ` * Simple boring wrapper function.` |
|        - | 5351 | ` * ------------------------------------` |
|        - | 5352 | ` */` |
|    35918 | 5353 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        5 | 5354 | `{` |
|        - | 5355 | `	sxi32 rc;` |
|    35923 | 5356 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|    35923 | 5357 | `	return rc;` |
|        5 | 5358 | `}` |
|        - | 5359 | `/*` |
|        - | 5360 | ` * Resolve function context from the current frame.` |
|        - | 5361 | ` */` |
|        - | 5362 | `/*` |
|        - | 5363 | ` * The name to SHOW for a function. Closures/lambdas carry a synthesized internal name` |
|        - | 5364 | ` * ("[closure_3]") that doubles as their lookup key; php reports them as` |
|        - | 5365 | ` * "{closure:/path/file.php:22}". Result points into pVm->zDisplayName for those, or` |
|        - | 5366 | ` * straight at the function's own name otherwise.` |
|        - | 5367 | ` */` |
|   652665 | 5368 | `PH7_PRIVATE int PH7_VmFuncDisplayName(ph7_vm *pVm,ph7_vm_func *pFunc,const char **pzOut)` |
|        5 | 5369 | `{` |
|   652670 | 5370 | `	const char *zName = pFunc->sName.zString;` |
|   652670 | 5371 | `	int nName = (int)pFunc->sName.nByte;` |
|   704010 | 5372 | `	int bClosure = (nName > 9 && SyMemcmp(zName,"[closure_",9) == 0)` |
|   708932 | 5373 | `		\|\| (nName > 8 && SyMemcmp(zName,"[lambda_",8) == 0);` |
|        - | 5374 | `	/* A property hook is not a method in php and never shows the name PHL` |
|        - | 5375 | ``	 * synthesizes for it: php calls it `$p::get` / `$p::set` — which is what`` |
|        - | 5376 | ``	 * `__FUNCTION__`, `debug_backtrace()['function']` and a Throwable's trace`` |
|        - | 5377 | `	 * report from inside one, and what makes the trace line read` |
|        - | 5378 | ``	 * `C->$p::get()`. `__METHOD__` prepends the class and lands on php's`` |
|        - | 5379 | ``	 * `C::$p::get` for free. */`` |
|        - | 5380 | `	{` |
|        - | 5381 | `		SyString sProp;` |
|        - | 5382 | `		const char *zKind;` |
|   652670 | 5383 | `		if( PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind) ){` |
|       44 | 5384 | `			int n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|       13 | 5385 | `				"$%z::%s",&sProp,zKind);` |
|       31 | 5386 | `			*pzOut = pVm->zDisplayName;` |
|       31 | 5387 | `			return n;` |
|        - | 5388 | `		}` |
|        - | 5389 | `	}` |
|   652644 | 5390 | `	if( bClosure ){` |
|        - | 5391 | `		int n;` |
|    10505 | 5392 | `		if( pFunc->sClosureName.nByte > 0 ){` |
|        - | 5393 | `			/* The compiler built php's name: it words the ENCLOSING scope, which a` |
|        - | 5394 | ``			 * file/line pair cannot reach (`{closure:Foo::bar():3}`). */`` |
|    10505 | 5395 | `			*pzOut = pFunc->sClosureName.zString;` |
|    10505 | 5396 | `			return (int)pFunc->sClosureName.nByte;` |
|        - | 5397 | `		}` |
|      ! 0 | 5398 | `		if( pFunc->sFile.nByte > 0 ){` |
|      ! 0 | 5399 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),` |
|      ! 0 | 5400 | `				"{closure:%.*s:%u}",(int)pFunc->sFile.nByte,pFunc->sFile.zString,pFunc->nLine);` |
|      ! 0 | 5401 | `		}else{` |
|      ! 0 | 5402 | `			n = (int)SyBufferFormat(pVm->zDisplayName,sizeof(pVm->zDisplayName),"{closure}");` |
|        - | 5403 | `		}` |
|      ! 0 | 5404 | `		*pzOut = pVm->zDisplayName;` |
|      ! 0 | 5405 | `		return n;` |
|        - | 5406 | `	}` |
|   642144 | 5407 | `	*pzOut = zName;` |
|   642144 | 5408 | `	return nName;` |
|   326285 | 5409 | `}` |
|        - | 5410 | `/*` |
|        - | 5411 | `` * php's name for the ACTIVE function -- the one its `name(): ` diagnostic`` |
|        - | 5412 | ` * qualifier prints. A method is rendered with its declaring class ("W::go"),` |
|        - | 5413 | `` * a closure with php's `{closure:file:line}`, and the global scope with php's`` |
|        - | 5414 | ` * own "main". VmGetFrameContext answers the same question in the shape the` |
|        - | 5415 | ` * uncaught-exception reporter wants (a bare display name, nothing at global` |
|        - | 5416 | ` * scope); this one is for a diagnostic raised from INSIDE an internal` |
|        - | 5417 | ` * function on the caller's behalf, which is how php attributes libxml's` |
|        - | 5418 | `` * errors -- `$el->nodeValue = 'a&b'` warns under the caller's name, not under`` |
|        - | 5419 | ` * the accessor's.` |
|        - | 5420 | ` */` |
|      186 | 5421 | `PH7_PRIVATE void PH7_VmActiveFuncName(ph7_vm *pVm,SyBlob *pOut)` |
|        4 | 5422 | `{` |
|      190 | 5423 | `	VmFrame *pFrame = pVm->pFrame;` |
|      190 | 5424 | `	ph7_vm_func *pFunc = 0;` |
|      190 | 5425 | `	if( pFrame ){` |
|      190 | 5426 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      190 | 5427 | `		if( pFrame->pParent ){` |
|       68 | 5428 | `			pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       32 | 5429 | `		}` |
|       93 | 5430 | `	}` |
|      190 | 5431 | `	if( pFunc ){` |
|       68 | 5432 | `		VmReturnFuncName(&(*pVm),pFunc,pOut);` |
|       36 | 5433 | `	}else{` |
|      126 | 5434 | `		SyBlobAppend(pOut,"main",sizeof("main")-1);` |
|        - | 5435 | `	}` |
|      190 | 5436 | `	SyBlobNullAppend(pOut);` |
|      190 | 5437 | `}` |
|     1202 | 5438 | `PH7_PRIVATE void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        4 | 5439 | `{` |
|        - | 5440 | `	VmFrame *pFrame;` |
|        - | 5441 | `	ph7_vm_func *pFunc;` |
|     1206 | 5442 | `	*pzFuncName = 0;` |
|     1206 | 5443 | `	*pnFuncLen = 0;` |
|     1206 | 5444 | `	pFrame = pVm->pFrame;` |
|     1206 | 5445 | `	if( pFrame == 0 ){` |
|      ! 0 | 5446 | `		return;` |
|        - | 5447 | `	}` |
|     1206 | 5448 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1206 | 5449 | `	if( pFrame->pParent == 0 ){` |
|     1164 | 5450 | `		return;` |
|        - | 5451 | `	}` |
|       46 | 5452 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       46 | 5453 | `	if( pFunc == 0 ){` |
|      ! 0 | 5454 | `		return;` |
|        - | 5455 | `	}` |
|       46 | 5456 | `	*pnFuncLen = PH7_VmFuncDisplayName(&(*pVm),pFunc,pzFuncName);` |
|      605 | 5457 | `}` |
|        - | 5458 | `/*` |
|        - | 5459 | ` * Append the exception's own rendered stack trace (php's "#N file(line):` |
|        - | 5460 | ` * func()" lines plus the "#N {main}" terminator) to pOut. Returns 1 when it` |
|        - | 5461 | ` * emitted a trace. The instance's trace walks the FULL frame chain, so this is` |
|        - | 5462 | ` * what the uncaught report should print; getTraceAsString() lives in the` |
|        - | 5463 | ` * built-in library and already produces php's exact byte format, which keeps` |
|        - | 5464 | ` * one renderer for both the caught (userland) and uncaught (engine) paths.` |
|        - | 5465 | ` * Returns 0 when there is no instance or no such method, leaving the caller to` |
|        - | 5466 | ` * synthesize what it can.` |
|        - | 5467 | ` */` |
|      628 | 5468 | `static int VmAppendExceptionTrace(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 5469 | `{` |
|        - | 5470 | `	ph7_class_method *pGetTrace;` |
|        - | 5471 | `	ph7_value sTrace;` |
|        - | 5472 | `	const char *zTmp;` |
|        - | 5473 | `	int nTmp;` |
|      632 | 5474 | `	int bDone = 0;` |
|        - | 5475 | `	int bSaved;` |
|      632 | 5476 | `	if( pThis == 0 ){` |
|      ! 0 | 5477 | `		return 0;` |
|        - | 5478 | `	}` |
|      632 | 5479 | `	if( pVm->bRenderingUncaught ){` |
|        - | 5480 | `		/* Already inside a report: do not run userland trace code again. */` |
|      ! 0 | 5481 | `		return 0;` |
|        - | 5482 | `	}` |
|      632 | 5483 | `	pGetTrace = PH7_ClassExtractMethod(pThis->pClass,"getTraceAsString",sizeof("getTraceAsString")-1);` |
|      632 | 5484 | `	if( pGetTrace == 0 ){` |
|      ! 0 | 5485 | `		return 0;` |
|        - | 5486 | `	}` |
|      632 | 5487 | `	PH7_MemObjInit(pVm,&sTrace);` |
|        - | 5488 | `	/* getTraceAsString() is userland code from the builtin prelude; if it (or` |
|        - | 5489 | `	 * anything it calls) throws, the throw would be reported by this very` |
|        - | 5490 | `	 * renderer. Fence the window so that report falls back to the synthesized` |
|        - | 5491 | `	 * trace rather than re-entering here forever. */` |
|      632 | 5492 | `	bSaved = pVm->bRenderingUncaught;` |
|      632 | 5493 | `	pVm->bRenderingUncaught = 1;` |
|      632 | 5494 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetTrace,&sTrace,0,0) == SXRET_OK ){` |
|      632 | 5495 | `		zTmp = ph7_value_to_string(&sTrace,&nTmp);` |
|      632 | 5496 | `		if( zTmp && nTmp > 0 ){` |
|      632 | 5497 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      632 | 5498 | `			bDone = 1;` |
|      314 | 5499 | `		}` |
|      314 | 5500 | `	}` |
|      632 | 5501 | `	PH7_MemObjRelease(&sTrace);` |
|      632 | 5502 | `	pVm->bRenderingUncaught = bSaved;` |
|      632 | 5503 | `	return bDone;` |
|      318 | 5504 | `}` |
|        - | 5505 | `/*` |
|        - | 5506 | ` * Render one exception entry of an uncaught-exception report into pOut.` |
|        - | 5507 | ` *` |
|        - | 5508 | ``  * The output is the single-exception PHP format, factored so a `$previous` `` |
|        - | 5509 | ` * chain can emit several entries into one blob (see VmReportUncaughtChain):` |
|        - | 5510 | ` *   bFirst -> the head entry is prefixed "PHP Fatal error:  Uncaught "; a` |
|        - | 5511 | ` *             chained entry is prefixed "\n\nNext " (blank-line separated).` |
|        - | 5512 | ` *   bLast  -> only the tail entry appends the "  thrown in <file> on line 1"` |
|        - | 5513 | ` *             trailer.` |
|        - | 5514 | ` * A single entry (bFirst && bLast) is byte-identical to the historical output.` |
|        - | 5515 | ` * The caller owns the blob lifecycle (init/release) and the output consumer` |
|        - | 5516 | ` * call; this routine only appends.` |
|        - | 5517 | ` */` |
|      630 | 5518 | `static void VmRenderUncaughtEntry(` |
|        - | 5519 | `	ph7_vm *pVm,SyBlob *pOut,` |
|        - | 5520 | `	ph7_class_instance *pExc, /* the exception being reported (0 when the caller has no instance) */` |
|        - | 5521 | `	const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,` |
|        - | 5522 | `	const char *zFuncName,int nFuncLen,int bFirst,int bLast,` |
|        - | 5523 | `	sxu32 nThrowLine,  /* line the exception was raised at (0 -> the line running now) */` |
|        - | 5524 | `	sxu32 nCallLine,   /* line of the call that entered the throwing frame (0 -> same) */` |
|        - | 5525 | `	SyString *pThrowFile, /* file the exception was raised IN (0/empty -> derive it) */` |
|        - | 5526 | `	const char **pzLabel) /* OUT (head entry only): php's label for the report */` |
|        4 | 5527 | `{` |
|        - | 5528 | `	SyString *pFile;` |
|        - | 5529 | `	SyString *pCallFile;` |
|        - | 5530 | `	int bParseErr,bCompileErr;` |
|      634 | 5531 | `	if( nThrowLine == 0 ){` |
|      ! 0 | 5532 | `		nThrowLine = pVm->nCurLine ? pVm->nCurLine : 1;` |
|      ! 0 | 5533 | `	}` |
|      634 | 5534 | `	if( nCallLine == 0 ){` |
|      634 | 5535 | `		VmFrame *pTraceFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|      634 | 5536 | `		nCallLine = (pTraceFrame && pTraceFrame->nCallLine) ? pTraceFrame->nCallLine : nThrowLine;` |
|      315 | 5537 | `	}` |
|      634 | 5538 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 | 5539 | `		zClass = "Exception";` |
|      ! 0 | 5540 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 | 5541 | `	}` |
|      634 | 5542 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      588 | 5543 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      292 | 5544 | `	}` |
|        - | 5545 | ``	/* WHERE php says the exception was raised. php's `zend_exception_error` reads the`` |
|        - | 5546 | ``	 * throwable's OWN `file`/`line` properties, which are stamped where it was`` |
|        - | 5547 | `	 * constructed -- so an exception that escapes a vendor package names that` |
|        - | 5548 | `	 * package's file, however many frames it unwound through on the way out. PHL read` |
|        - | 5549 | `	 * the top of the INCLUDE stack instead, which is the entry script once anything` |
|        - | 5550 | `	 * defined elsewhere is running: every uncaught report in a composer tree -- a` |
|        - | 5551 | ``	 * `throw`, an undefined function or method, a TypeError, a DivisionByZeroError,`` |
|        - | 5552 | ``	 * each link of a `$previous` chain -- named the wrong file, in the one message a`` |
|        - | 5553 | ``	 * user reads when a program dies. (`getFile()` was already right; only the report`` |
|        - | 5554 | `	 * was not.) A caller with no instance to ask -- the internal Error reports -- has` |
|        - | 5555 | `	 * a live frame instead, so it takes the file the RUNNING code is in, the same` |
|        - | 5556 | `	 * source every other diagnostic uses (see VmDiagnosticWhere).` |
|        - | 5557 | `	 *` |
|        - | 5558 | `	 * pCallFile stays the include-stack top: the synthesized trace frame below names a` |
|        - | 5559 | `	 * CALL SITE, which is the caller's file, not the throw's. */` |
|      634 | 5560 | `	pCallFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      634 | 5561 | `	pFile = pCallFile;` |
|      634 | 5562 | `	if( pThrowFile && pThrowFile->nByte > 0 ){` |
|      634 | 5563 | `		pFile = pThrowFile;` |
|      319 | 5564 | `	}else{` |
|      ! 0 | 5565 | `		SyString *pUnit = PH7_VmExecutingUnitFile(&(*pVm));` |
|      ! 0 | 5566 | `		if( pUnit && pUnit->nByte > 0 ){` |
|      ! 0 | 5567 | `			pFile = pUnit;` |
|      ! 0 | 5568 | `		}` |
|        - | 5569 | `	}` |
|      650 | 5570 | `	bParseErr = (nClass == sizeof("ParseError")-1` |
|      630 | 5571 | `	          && SyMemcmp(zClass,"ParseError",nClass) == 0);` |
|      634 | 5572 | `	bCompileErr = (nClass == sizeof("CompileError")-1` |
|      630 | 5573 | `	            && SyMemcmp(zClass,"CompileError",nClass) == 0);` |
|      634 | 5574 | `	if( bFirst && bLast && (bParseErr \|\| bCompileErr) ){` |
|        - | 5575 | `		/* php's zend_exception_error asks for the class by IDENTITY: an uncaught` |
|        - | 5576 | `		 * ParseError is reported as the E_PARSE it stands for and an uncaught` |
|        - | 5577 | `		 * CompileError as an E_COMPILE_ERROR -- one plain line naming the file and` |
|        - | 5578 | `		 * the line, with no "Uncaught", no stack trace and no "thrown in" trailer.` |
|        - | 5579 | `		 * (A user SUBCLASS of either is an ordinary uncaught exception, which is why` |
|        - | 5580 | `		 * this is a name match and not an instanceof.) */` |
|        3 | 5581 | `		*pzLabel = bParseErr ? "Parse error" : "Fatal error";` |
|        3 | 5582 | `		if( zMsg && nMsg > 0 ){` |
|        3 | 5583 | `			SyBlobAppend(pOut,zMsg,nMsg);` |
|        1 | 5584 | `		}` |
|        3 | 5585 | `		if( pFile ){` |
|        3 | 5586 | `			SyBlobFormat(pOut," in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|        1 | 5587 | `		}` |
|        3 | 5588 | `		return;` |
|        - | 5589 | `	}` |
|      632 | 5590 | `	if( bFirst ){` |
|      620 | 5591 | `		*pzLabel = "Fatal error";` |
|      620 | 5592 | `		SyBlobAppend(pOut,"Uncaught ",sizeof("Uncaught ")-1);` |
|      312 | 5593 | `	}else{` |
|       16 | 5594 | `		SyBlobAppend(pOut,"\n\nNext ",sizeof("\n\nNext ")-1);` |
|        - | 5595 | `	}` |
|      632 | 5596 | `	SyBlobAppend(pOut,zClass,nClass);` |
|      632 | 5597 | `	if( zMsg && nMsg > 0 ){` |
|      632 | 5598 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|      632 | 5599 | `		SyBlobAppend(pOut,zMsg,nMsg);` |
|      314 | 5600 | `	}` |
|      632 | 5601 | `	if( pFile ){` |
|      632 | 5602 | `		SyBlobFormat(pOut," in %.*s:%u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|      314 | 5603 | `	}` |
|      632 | 5604 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|        - | 5605 | `	/* Prefer the exception's OWN trace: it walks the full frame chain (shared` |
|        - | 5606 | `	 * with debug_backtrace) and getTraceAsString() already renders php's exact` |
|        - | 5607 | `	 * "#N file(line): func()" body plus the "#N {main}" terminator. The` |
|        - | 5608 | `	 * synthesized fallback below can only ever describe ONE frame, so it` |
|        - | 5609 | `	 * silently dropped every intermediate frame of a nested call chain and, at` |
|        - | 5610 | `	 * file scope, invented a "#0 file(line): {main}" entry that php does not` |
|        - | 5611 | `	 * print ({main} is the bottom marker, not a called frame). */` |
|      632 | 5612 | `	if( pVm->bRenderingUncaught \|\| !VmAppendExceptionTrace(pVm,pExc,pOut) ){` |
|      ! 0 | 5613 | `		int bFrame = 0;` |
|      ! 0 | 5614 | `		if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 | 5615 | `			if( pCallFile ){` |
|        - | 5616 | `				/* php reports a trace frame at its CALL SITE, not at the line` |
|        - | 5617 | `				 * running inside it. */` |
|      ! 0 | 5618 | `				SyBlobFormat(pOut,"#0 %.*s(%u): %.*s()\n",` |
|      ! 0 | 5619 | `					(int)pCallFile->nByte,pCallFile->zString,nCallLine,nFuncLen,zFuncName);` |
|      ! 0 | 5620 | `			}else{` |
|      ! 0 | 5621 | `				SyBlobFormat(pOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|        - | 5622 | `			}` |
|      ! 0 | 5623 | `			bFrame = 1;` |
|      ! 0 | 5624 | `		}` |
|        - | 5625 | `		/* {main} closes the trace, numbered after whatever frames precede it. */` |
|      ! 0 | 5626 | `		SyBlobAppend(pOut,bFrame ? "#1 {main}" : "#0 {main}",` |
|      ! 0 | 5627 | `			bFrame ? sizeof("#1 {main}")-1 : sizeof("#0 {main}")-1);` |
|      ! 0 | 5628 | `	}` |
|      632 | 5629 | `	if( bLast && pFile ){` |
|      620 | 5630 | `		SyBlobAppend(pOut,"\n",sizeof("\n")-1);` |
|      620 | 5631 | `		SyBlobFormat(pOut,"  thrown in %.*s on line %u",(int)pFile->nByte,pFile->zString,nThrowLine);` |
|      308 | 5632 | `	}` |
|      319 | 5633 | `}` |
|        - | 5634 | `/*` |
|        - | 5635 | ` * Emit a PHP-compatible uncaught exception message and stack trace for a` |
|        - | 5636 | `` * single exception (no `$previous` chain). Used by the callers that have no`` |
|        - | 5637 | ` * exception instance to walk (internal Error reports, type errors, ...).` |
|        - | 5638 | ` */` |
|      ! 0 | 5639 | `PH7_PRIVATE sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|      ! 0 | 5640 | `{` |
|        - | 5641 | `	SyBlob sOut;` |
|      ! 0 | 5642 | `	const char *zLabel = "Fatal error";` |
|        - | 5643 | `	/* An uncaught exception is a fatal: php exits 255 whether or not the` |
|        - | 5644 | `	 * report is displayed. Set the status before the bErrReport gate. */` |
|      ! 0 | 5645 | `	pVm->iExitStatus = 255;` |
|      ! 0 | 5646 | `	if( !pVm->bErrReport ){` |
|      ! 0 | 5647 | `		return PH7_OK;` |
|        - | 5648 | `	}` |
|      ! 0 | 5649 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      ! 0 | 5650 | `	VmRenderUncaughtEntry(pVm,&sOut,0,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen,TRUE,TRUE,0,0,0,&zLabel);` |
|      ! 0 | 5651 | `	VmEmitFatalReport(pVm,zLabel,(const char *)SyBlobData(&sOut),SyBlobLength(&sOut));` |
|      ! 0 | 5652 | `	SyBlobRelease(&sOut);` |
|      ! 0 | 5653 | `	return PH7_ABORT;` |
|      ! 0 | 5654 | `}` |
|        - | 5655 | `/*` |
|        - | 5656 | `` * Return the `$previous` exception linked on pThis (the Throwable data model:`` |
|        - | 5657 | ` * a protected $previous property, inherited by every user subclass), or NULL` |
|        - | 5658 | ` * if there is none / it isn't an object. Reads the per-instance attribute` |
|        - | 5659 | ` * directly (no getPrevious() dispatch), mirroring PHP's internal reporter.` |
|        - | 5660 | ` */` |
|        - | 5661 | `static const SyString sExcPrevName = { "previous", sizeof("previous") - 1 };` |
|      630 | 5662 | `static ph7_class_instance * VmExceptionGetPrevious(ph7_class_instance *pThis)` |
|        4 | 5663 | `{` |
|        - | 5664 | `	ph7_value *pValue;` |
|        - | 5665 | `	ph7_class_instance *pPrev;` |
|        - | 5666 | `	ph7_class *pThrowable;` |
|      634 | 5667 | `	if( pThis == 0 ){` |
|      ! 0 | 5668 | `		return 0;` |
|        - | 5669 | `	}` |
|      634 | 5670 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|      634 | 5671 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      622 | 5672 | `		return 0;` |
|        - | 5673 | `	}` |
|       16 | 5674 | `	pPrev = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 5675 | `	/* PHP's $previous is always a Throwable; a PHL subclass could assign a` |
|        - | 5676 | `	 * non-Throwable to the (untyped, protected) slot — ignore it so the report` |
|        - | 5677 | `	 * never renders a stray object as an exception entry. */` |
|       16 | 5678 | `	pThrowable = PH7_VmExtractClass(pThis->pVm,"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|       16 | 5679 | `	if( pThrowable && pPrev && !PH7_VmInstanceOf(pPrev->pClass,pThrowable) ){` |
|      ! 0 | 5680 | `		return 0;` |
|        - | 5681 | `	}` |
|       16 | 5682 | `	return pPrev;` |
|      319 | 5683 | `}` |
|        - | 5684 | `/*` |
|        - | 5685 | `` * Link pPrev as the `$previous` of pThis, but ONLY if pThis has no previous`` |
|        - | 5686 | ` * yet (PHP keeps an explicitly-constructed previous and never overrides it).` |
|        - | 5687 | ` * pThis takes a ref on pPrev so it outlives the superseded exception; the` |
|        - | 5688 | ` * slot is released by the normal instance teardown. No-op on any miss.` |
|        - | 5689 | ` */` |
|    15269 | 5690 | `static void VmExceptionLinkPrevious(ph7_class_instance *pThis,ph7_class_instance *pPrev)` |
|        5 | 5691 | `{` |
|        - | 5692 | `	ph7_value *pValue;` |
|    15274 | 5693 | `	if( pThis == 0 \|\| pPrev == 0 \|\| pThis == pPrev ){` |
|    15222 | 5694 | `		return;` |
|        - | 5695 | `	}` |
|       56 | 5696 | `	pValue = PH7_ClassInstanceFetchAttr(pThis,&sExcPrevName);` |
|       56 | 5697 | `	if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_OBJ) != 0 ){` |
|        3 | 5698 | `		return; /* No slot (not our Exception/Error model), or pThis already has a previous — keep it */` |
|        - | 5699 | `	}` |
|       54 | 5700 | `	pPrev->iRef++;` |
|        - | 5701 | `	/* The slot is non-OBJ here, but may hold a scalar (a subclass that wrote a` |
|        - | 5702 | `	 * non-Throwable); release frees any such buffer before the overwrite. */` |
|       54 | 5703 | `	PH7_MemObjRelease(pValue);` |
|       54 | 5704 | `	pValue->x.pOther = pPrev;` |
|       54 | 5705 | `	MemObjSetType(pValue,MEMOBJ_OBJ);` |
|     7615 | 5706 | `}` |
|        - | 5707 | `/*` |
|        - | 5708 | ` * Append the message of the exception instance pThis to pOut by invoking its` |
|        - | 5709 | ` * getMessage() (so a user override is honored). A no-op if the method is` |
|        - | 5710 | ` * absent or yields an empty string.` |
|        - | 5711 | ` */` |
|        - | 5712 | `/*` |
|        - | 5713 | `` * Read a Throwable's `line` (the site it was created at — see PH7_VmStampThrowableSite).`` |
|        - | 5714 | ` * 0 when the class exposes no getLine().` |
|        - | 5715 | ` */` |
|      630 | 5716 | `static sxu32 VmExtractExceptionLine(ph7_vm *pVm,ph7_class_instance *pThis)` |
|        4 | 5717 | `{` |
|        - | 5718 | `	ph7_class_method *pGetLine;` |
|        - | 5719 | `	ph7_value sLine;` |
|      634 | 5720 | `	sxu32 nLine = 0;` |
|      634 | 5721 | `	pGetLine = PH7_ClassExtractMethod(pThis->pClass,"getLine",sizeof("getLine")-1);` |
|      634 | 5722 | `	if( pGetLine == 0 ){` |
|      ! 0 | 5723 | `		return 0;` |
|        - | 5724 | `	}` |
|      634 | 5725 | `	PH7_MemObjInit(pVm,&sLine);` |
|      634 | 5726 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetLine,&sLine,0,0) == SXRET_OK ){` |
|      634 | 5727 | `		sxi64 n = ph7_value_to_int64(&sLine);` |
|      634 | 5728 | `		if( n > 0 ){` |
|      634 | 5729 | `			nLine = (sxu32)n;` |
|      315 | 5730 | `		}` |
|      315 | 5731 | `	}` |
|      634 | 5732 | `	PH7_MemObjRelease(&sLine);` |
|      634 | 5733 | `	return nLine;` |
|      319 | 5734 | `}` |
|        - | 5735 | `/*` |
|        - | 5736 | `` * The throwable's own `file` -- what php's uncaught report names. Read through`` |
|        - | 5737 | `` * `getFile()`, the way the message and the line beside it are read: the accessor is`` |
|        - | 5738 | `` * `final` in php (and refused here too), so it can only ever answer the property.`` |
|        - | 5739 | ` */` |
|      630 | 5740 | `static void VmExtractExceptionFile(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 5741 | `{` |
|        - | 5742 | `	ph7_class_method *pGetFile;` |
|        - | 5743 | `	ph7_value sFile;` |
|        - | 5744 | `	const char *zTmp;` |
|        - | 5745 | `	int nTmp;` |
|      634 | 5746 | `	pGetFile = PH7_ClassExtractMethod(pThis->pClass,"getFile",sizeof("getFile")-1);` |
|      634 | 5747 | `	if( pGetFile == 0 ){` |
|      ! 0 | 5748 | `		return;` |
|        - | 5749 | `	}` |
|      634 | 5750 | `	PH7_MemObjInit(pVm,&sFile);` |
|      634 | 5751 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetFile,&sFile,0,0) == SXRET_OK ){` |
|      634 | 5752 | `		zTmp = ph7_value_to_string(&sFile,&nTmp);` |
|      634 | 5753 | `		if( zTmp && nTmp > 0 ){` |
|      634 | 5754 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      315 | 5755 | `		}` |
|      315 | 5756 | `	}` |
|      634 | 5757 | `	PH7_MemObjRelease(&sFile);` |
|      319 | 5758 | `}` |
|      630 | 5759 | `static void VmExtractExceptionMessage(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|        4 | 5760 | `{` |
|        - | 5761 | `	ph7_class_method *pGetMessage;` |
|        - | 5762 | `	ph7_value sMsg;` |
|        - | 5763 | `	const char *zTmp;` |
|        - | 5764 | `	int nTmp;` |
|      634 | 5765 | `	pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      634 | 5766 | `	if( pGetMessage == 0 ){` |
|      ! 0 | 5767 | `		return;` |
|        - | 5768 | `	}` |
|      634 | 5769 | `	PH7_MemObjInit(pVm,&sMsg);` |
|      634 | 5770 | `	if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      634 | 5771 | `		zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      634 | 5772 | `		if( zTmp && nTmp > 0 ){` |
|      634 | 5773 | `			SyBlobAppend(pOut,zTmp,(sxu32)nTmp);` |
|      315 | 5774 | `		}` |
|      315 | 5775 | `	}` |
|      634 | 5776 | `	PH7_MemObjRelease(&sMsg);` |
|      319 | 5777 | `}` |
|        - | 5778 | `/*` |
|        - | 5779 | ` * Emit a PHP-compatible uncaught-exception report for pThis, walking its` |
|        - | 5780 | `` * `$previous` chain. PHP prints the DEEPEST previous as "Uncaught", then each`` |
|        - | 5781 | ` * outer one as "Next ...", with a single "thrown in ..." trailer after the` |
|        - | 5782 | ` * outermost (the actually-uncaught) exception.` |
|        - | 5783 | ` *` |
|        - | 5784 | ` * The walk starts at pThis (outermost) and follows $previous inward; entries` |
|        - | 5785 | ` * are rendered in reverse (deepest first). A cyclic $previous (e.g.` |
|        - | 5786 | ` * $a->previous = $a, or A<->B) is broken on the first already-seen instance so` |
|        - | 5787 | ` * it is not duplicated, and the depth is hard-capped as a final backstop.` |
|        - | 5788 | ` */` |
|        - | 5789 | `#define VM_EXCEPTION_CHAIN_MAX 64` |
|      618 | 5790 | `static sxi32 VmReportUncaughtChain(ph7_vm *pVm,ph7_class_instance *pThis,const char *zFuncName,int nFuncLen)` |
|        4 | 5791 | `{` |
|        - | 5792 | `	ph7_class_instance *apChain[VM_EXCEPTION_CHAIN_MAX];` |
|      622 | 5793 | `	int nChain = 0;` |
|        - | 5794 | `	int i;` |
|        - | 5795 | `	SyBlob sOut;` |
|        - | 5796 | `	/* The report's label is the HEAD entry's: only that one can be a bare` |
|        - | 5797 | ``	 * `Parse error` (an uncaught ParseError reports as the E_PARSE it stands`` |
|        - | 5798 | ``	 * for), and a chain's head is always an ordinary `Fatal error`. */`` |
|      622 | 5799 | `	const char *zLabel = "Fatal error";` |
|        - | 5800 | `	/* Same rule as VmReportUncaughtException: an uncaught exception is a` |
|        - | 5801 | `	 * fatal — php exits 255 whether or not the report is displayed. One rule` |
|        - | 5802 | `	 * per report entry point, so a future direct caller can't miss it. */` |
|      622 | 5803 | `	pVm->iExitStatus = 255;` |
|      622 | 5804 | `	if( !pVm->bErrReport ){` |
|      ! 0 | 5805 | `		return PH7_OK;` |
|        - | 5806 | `	}` |
|        - | 5807 | `	/* Collect outermost -> deepest, stopping on a cycle (an instance already` |
|        - | 5808 | `	 * collected) or the hard cap. */` |
|     1252 | 5809 | `	while( pThis && nChain < VM_EXCEPTION_CHAIN_MAX ){` |
|      648 | 5810 | `		for( i = 0 ; i < nChain ; ++i ){` |
|       18 | 5811 | `			if( apChain[i] == pThis ){` |
|      ! 0 | 5812 | `				pThis = 0; /* cycle: stop the walk */` |
|      ! 0 | 5813 | `				break;` |
|        - | 5814 | `			}` |
|       11 | 5815 | `		}` |
|      634 | 5816 | `		if( pThis == 0 ){` |
|      ! 0 | 5817 | `			break;` |
|        - | 5818 | `		}` |
|      634 | 5819 | `		apChain[nChain++] = pThis;` |
|      634 | 5820 | `		pThis = VmExceptionGetPrevious(pThis);` |
|        4 | 5821 | `	}` |
|      622 | 5822 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        - | 5823 | `	/* Render deepest -> outermost: index nChain-1 is the deepest ("Uncaught"),` |
|        - | 5824 | `	 * index 0 is the outermost (gets the "thrown in" trailer). */` |
|     1252 | 5825 | `	for( i = nChain - 1 ; i >= 0 ; --i ){` |
|      634 | 5826 | `		ph7_class_instance *pEnt = apChain[i];` |
|        - | 5827 | `		SyBlob sMsg;` |
|        - | 5828 | `		SyBlob sFile;` |
|        - | 5829 | `		SyString sThrowFile;` |
|        - | 5830 | `		sxu32 nEntLine;` |
|      634 | 5831 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      634 | 5832 | `		SyBlobInit(&sFile,&pVm->sAllocator);` |
|      634 | 5833 | `		VmExtractExceptionMessage(pVm,pEnt,&sMsg);` |
|        - | 5834 | `		/* Each link of the chain reports its OWN file and line: php prints the` |
|        - | 5835 | `		 * deepest as "Uncaught", every outer one as "Next ...", and they routinely` |
|        - | 5836 | `		 * come from different packages. Both accessors run PHP code, so read them` |
|        - | 5837 | `		 * before the render rather than inside its argument list. */` |
|      634 | 5838 | `		VmExtractExceptionFile(pVm,pEnt,&sFile);` |
|      634 | 5839 | `		nEntLine = VmExtractExceptionLine(pVm,pEnt);` |
|      634 | 5840 | `		SyStringInitFromBuf(&sThrowFile,SyBlobData(&sFile),SyBlobLength(&sFile));` |
|      949 | 5841 | `		VmRenderUncaughtEntry(pVm,&sOut,pEnt,` |
|      630 | 5842 | `			pEnt->pClass->sName.zString,pEnt->pClass->sName.nByte,` |
|      630 | 5843 | `			(const char *)SyBlobData(&sMsg),(sxu32)SyBlobLength(&sMsg),` |
|      315 | 5844 | `			zFuncName,nFuncLen,` |
|      630 | 5845 | `			(i == nChain - 1) ? TRUE : FALSE,   /* bFirst: deepest entry */` |
|      315 | 5846 | `			(i == 0) ? TRUE : FALSE,            /* bLast: outermost entry */` |
|      315 | 5847 | `			nEntLine,0,&sThrowFile,&zLabel);` |
|      634 | 5848 | `		SyBlobRelease(&sFile);` |
|      634 | 5849 | `		SyBlobRelease(&sMsg);` |
|      319 | 5850 | `	}` |
|      622 | 5851 | `	VmEmitFatalReport(pVm,zLabel,(const char *)SyBlobData(&sOut),SyBlobLength(&sOut));` |
|      622 | 5852 | `	SyBlobRelease(&sOut);` |
|      622 | 5853 | `	return PH7_ABORT;` |
|      313 | 5854 | `}` |
|        - | 5855 | `/*` |
|        - | 5856 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - | 5857 | ` *` |
|        - | 5858 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - | 5859 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - | 5860 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - | 5861 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - | 5862 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - | 5863 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - | 5864 | ` */` |
|  1570069 | 5865 | `PH7_PRIVATE void VmCoalesceDisarm(ph7_vm *pVm)` |
|        5 | 5866 | `{` |
|  1570074 | 5867 | `	if( pVm->bCoalesceArmed ){` |
|       13 | 5868 | `		if( pVm->pCoalesceObj ){` |
|       13 | 5869 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        5 | 5870 | `		}` |
|       13 | 5871 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|       13 | 5872 | `		pVm->pCoalesceObj = 0;` |
|       13 | 5873 | `		pVm->bCoalesceArmed = 0;` |
|        5 | 5874 | `	}` |
|  1570074 | 5875 | `}` |
|        - | 5876 | `/*` |
|        - | 5877 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - | 5878 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - | 5879 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - | 5880 | ` * build the SyBlob themselves and pass its data + length.` |
|        - | 5881 | ` *` |
|        - | 5882 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - | 5883 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - | 5884 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - | 5885 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - | 5886 | ` */` |
|   145130 | 5887 | `PH7_PRIVATE sxi32 VmThrowFromVm(` |
|        - | 5888 | `	ph7_vm *pVm,` |
|        - | 5889 | `	const char *zClass,` |
|        - | 5890 | `	const char *zMsg,` |
|        - | 5891 | `	sxu32 nMsg` |
|        5 | 5892 | `){` |
|        - | 5893 | `	ph7_class *pClass;` |
|        - | 5894 | `	ph7_class_instance *pThis;` |
|        - | 5895 | `	ph7_class_method *pCons;` |
|        - | 5896 | `	VmFrame *pFrame;` |
|        - | 5897 | `	sxi32 rc;` |
|   145135 | 5898 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|   145135 | 5899 | `	if( pClass == 0 ){` |
|      ! 0 | 5900 | `		return SXERR_ABORT;` |
|        - | 5901 | `	}` |
|   145135 | 5902 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|   145135 | 5903 | `	if( pThis == 0 ){` |
|      ! 0 | 5904 | `		return SXERR_ABORT;` |
|        - | 5905 | `	}` |
|   145135 | 5906 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   145135 | 5907 | `	if( pCons && VmExcCtorEnter(pVm) ){` |
|        - | 5908 | `		ph7_value sArg;` |
|        - | 5909 | `		ph7_value *apArg[1];` |
|        - | 5910 | `		SyString sMsgStr;` |
|   145135 | 5911 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|   145135 | 5912 | `		PH7_MemObjInit(pVm,&sArg);` |
|   145135 | 5913 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|   145135 | 5914 | `		apArg[0] = &sArg;` |
|   145135 | 5915 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|   145135 | 5916 | `		PH7_MemObjRelease(&sArg);` |
|   145135 | 5917 | `		pVm->nExcCtorDepth--;` |
|    72565 | 5918 | `	}` |
|   145135 | 5919 | `	pFrame = pVm->pFrame;` |
|   145135 | 5920 | `	if( pFrame ){` |
|   145135 | 5921 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|   145135 | 5922 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|    72565 | 5923 | `	}` |
|   145135 | 5924 | `	rc = VmThrowException(pVm,pThis);` |
|   145135 | 5925 | `	PH7_ClassInstanceUnref(pThis);` |
|   145135 | 5926 | `	return rc;` |
|    72570 | 5927 | `}` |
|        - | 5928 | `/*` |
|        - | 5929 | ` * A native compare handler REFUSED the pair (ph7_class::xCmp wrote a class name` |
|        - | 5930 | ` * into its context, which PH7_ClassNativeCmp parked on the VM). php raises that` |
|        - | 5931 | ` * exception out of the comparison itself; PH7_MemObjCmp cannot, because it is` |
|        - | 5932 | ` * also the comparator sort(), in_array(), max() and switch drive, none of which` |
|        - | 5933 | ` * is a throw boundary. So the refusal waits here until a site that CAN route a` |
|        - | 5934 | ` * throw asks for it — the comparison opcodes and the switch arm raise it where` |
|        - | 5935 | ` * the expression's value would have landed, and the host-call boundary raises it` |
|        - | 5936 | ` * on the builtin's own context, which is where every other builtin throw is` |
|        - | 5937 | ` * reported from. Both doors clear it first: a raise that itself unwinds must not` |
|        - | 5938 | ` * leave the record standing for the next comparison to fire again.` |
|        - | 5939 | ` */` |
| 57931299 | 5940 | `PH7_PRIVATE int PH7_CmpRefusalPending(ph7_vm *pVm)` |
|        5 | 5941 | `{` |
| 57931304 | 5942 | `	return pVm->zCmpRefusalClass != 0;` |
|        5 | 5943 | `}` |
|       22 | 5944 | `PH7_PRIVATE void PH7_CmpRefusalNesting(ph7_vm *pVm)` |
|        2 | 5945 | `{` |
|        - | 5946 | `	static const char zMsg[] = "Nesting level too deep - recursive dependency?";` |
|       24 | 5947 | `	if( pVm->zCmpRefusalClass != 0 ){` |
|        - | 5948 | `		/* FIRST refusal wins, like every other writer of this record: a driver that` |
|        - | 5949 | `		 * keeps comparing after one must not overwrite the message the script sees. */` |
|      ! 0 | 5950 | `		return;` |
|        - | 5951 | `	}` |
|       24 | 5952 | `	pVm->zCmpRefusalClass = "Error";` |
|       24 | 5953 | `	SyMemcpy(zMsg,pVm->zCmpRefusalMsg,sizeof(zMsg));` |
|       13 | 5954 | `}` |
|     8535 | 5955 | `PH7_PRIVATE void PH7_CmpRefusalClear(ph7_vm *pVm)` |
|        5 | 5956 | `{` |
|     8540 | 5957 | `	pVm->zCmpRefusalClass = 0;` |
|     8540 | 5958 | `	pVm->zCmpRefusalMsg[0] = 0;` |
|     8540 | 5959 | `}` |
|       56 | 5960 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaise(ph7_vm *pVm)` |
|        3 | 5961 | `{` |
|       59 | 5962 | `	const char *zClass = pVm->zCmpRefusalClass;` |
|        - | 5963 | `	char zMsg[sizeof(pVm->zCmpRefusalMsg)];` |
|       59 | 5964 | `	if( zClass == 0 ){` |
|      ! 0 | 5965 | `		return SXRET_OK;` |
|        - | 5966 | `	}` |
|       59 | 5967 | `	SyMemcpy(pVm->zCmpRefusalMsg,zMsg,sizeof(zMsg));` |
|       59 | 5968 | `	PH7_CmpRefusalClear(&(*pVm));` |
|       59 | 5969 | `	return VmThrowFromVm(&(*pVm),zClass,zMsg,(sxu32)SyStrlen(zMsg));` |
|       31 | 5970 | `}` |
|       18 | 5971 | `PH7_PRIVATE sxi32 PH7_CmpRefusalRaiseCtx(ph7_context *pCtx)` |
|        2 | 5972 | `{` |
|       20 | 5973 | `	ph7_vm *pVm = pCtx->pVm;` |
|       20 | 5974 | `	const char *zClass = pVm->zCmpRefusalClass;` |
|        - | 5975 | `	char zMsg[sizeof(pVm->zCmpRefusalMsg)];` |
|       20 | 5976 | `	if( zClass == 0 ){` |
|      ! 0 | 5977 | `		return SXRET_OK;` |
|        - | 5978 | `	}` |
|       20 | 5979 | `	SyMemcpy(pVm->zCmpRefusalMsg,zMsg,sizeof(zMsg));` |
|       20 | 5980 | `	PH7_CmpRefusalClear(&(*pVm));` |
|       20 | 5981 | `	return PH7_VmThrowException(pCtx,zClass,"%s",zMsg);` |
|       11 | 5982 | `}` |
|        - | 5983 | `/*` |
|        - | 5984 | ` * php's arithmetic operand contract, which PH7 never enforced — every case below was a` |
|        - | 5985 | ``  * SILENT WRONG ANSWER: `5 + "abc"` evaluated to int(5), `1 * "x"` to int(0), `[1] + 1` `` |
|        - | 5986 | `` * returned the array, and `5 / "x"` raised DivisionByZero instead of a TypeError.`` |
|        - | 5987 | ` *` |
|        - | 5988 | ` *   int/float/bool/null      arithmetic proceeds` |
|        - | 5989 | ` *   fully numeric string     proceeds ("1e3", " 5 ")` |
|        - | 5990 | ` *   LEADING-numeric string   proceeds on the numeric prefix, with a warning` |
|        - | 5991 | ` *                            ("5abc" + 1 == 6, "A non-numeric value encountered")` |
|        - | 5992 | ` *   non-numeric string       TypeError: Unsupported operand types: int + string` |
|        - | 5993 | ` *   array                    TypeError, EXCEPT array + array, which is php's union` |
|        - | 5994 | ` *   object/resource          TypeError, naming the object's CLASS` |
|        - | 5995 | ` *` |
|        - | 5996 | ` * Returns SXRET_OK to proceed, or the status of the thrown TypeError.` |
|        - | 5997 | ` */` |
|        - | 5998 | `/*` |
|        - | 5999 | `` * A running internal call's `args`, unless they were asked away`` |
|        - | 6000 | ` * (DEBUG_BACKTRACE_IGNORE_ARGS): php lists an internal frame's arguments exactly as it` |
|        - | 6001 | `` * lists a user frame's, and always with the key, so `array_map('f', [1])` shows`` |
|        - | 6002 | ` * ['f', [1]] above f's frame and a call that passed nothing shows [].` |
|        - | 6003 | ` */` |
|    15341 | 6004 | `static void VmTraceNativeArgs(ph7_vm *pVm,sxi32 iOptions,VmNativeCall *pNat,ph7_value *pEntry)` |
|        5 | 6005 | `{` |
|        - | 6006 | `	ph7_value *pArg;` |
|        - | 6007 | `	int i;` |
|    15346 | 6008 | `	if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) != 0 ){` |
|    15142 | 6009 | `		return;` |
|        - | 6010 | `	}` |
|      209 | 6011 | `	pArg = ph7_new_array(&(*pVm));` |
|      209 | 6012 | `	if( pArg == 0 ){` |
|      ! 0 | 6013 | `		return;` |
|        - | 6014 | `	}` |
|      509 | 6015 | `	for( i = 0 ; pNat && i < pNat->nArg ; i++ ){` |
|      305 | 6016 | `		if( pNat->apArg[i] ){` |
|      305 | 6017 | `			ph7_array_add_elem(pArg,0,pNat->apArg[i]);` |
|      150 | 6018 | `		}` |
|      155 | 6019 | `	}` |
|      209 | 6020 | `	ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|      209 | 6021 | `	ph7_release_value(&(*pVm),pArg);` |
|     7649 | 6022 | `}` |
|        - | 6023 | `/*` |
|        - | 6024 | ` * The frame of the INTERNAL function or method that reached for pFrame's body as a` |
|        - | 6025 | ` * callback (VM_FRAME_NATIVE_CALLER's pNativeCaller, or the Fiber method that last` |
|        - | 6026 | ` * entered a fiber's body, VM_FRAME_FIBER). It carries the userland call site the` |
|        - | 6027 | `` * callback's own frame declines, and php gives it `file`, `line` and `function`, plus`` |
|        - | 6028 | `` * `class`, `object` (PROVIDE_OBJECT only) and `type` for a method -- in that order,`` |
|        - | 6029 | `` * which is not a user frame's -- and then `args`, which only a fiber's entry has the`` |
|        - | 6030 | ` * record to list; a dispatch no record describes shows none.` |
|        - | 6031 | ` */` |
|      102 | 6032 | `static void VmTraceNativeCallerEntry(ph7_vm *pVm,sxi32 iOptions,VmFrame *pFrame,` |
|        - | 6033 | `	SyString *pFile,ph7_value *pValue,ph7_value *pList)` |
|        5 | 6034 | `{` |
|      107 | 6035 | `	SyString *pNatFile = SyStringLength(&pFrame->sCallFile) > 0 ? &pFrame->sCallFile : pFile;` |
|      107 | 6036 | `	ph7_class_instance *pRecv = pFrame->pNativeCallerThis;` |
|      107 | 6037 | `	ph7_value *pNat = ph7_new_array(&(*pVm));` |
|      107 | 6038 | `	if( pNat == 0 ){` |
|      ! 0 | 6039 | `		return;` |
|        - | 6040 | `	}` |
|      107 | 6041 | `	if( pNatFile ){` |
|      107 | 6042 | `		ph7_value_string(pValue,pNatFile->zString,(int)pNatFile->nByte);` |
|      107 | 6043 | `		ph7_array_add_strkey_elem(pNat,"file",pValue);` |
|      107 | 6044 | `		ph7_value_reset_string_cursor(pValue);` |
|       51 | 6045 | `	}` |
|      107 | 6046 | `	ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|      107 | 6047 | `	ph7_array_add_strkey_elem(pNat,"line",pValue);` |
|      107 | 6048 | `	ph7_value_string(pValue,pFrame->pNativeCaller->zString,(int)pFrame->pNativeCaller->nByte);` |
|      107 | 6049 | `	ph7_array_add_strkey_elem(pNat,"function",pValue);` |
|      107 | 6050 | `	ph7_value_reset_string_cursor(pValue);` |
|      107 | 6051 | `	if( pRecv && pRecv->pClass ){` |
|      105 | 6052 | `		ph7_value_string(pValue,pRecv->pClass->sName.zString,(int)pRecv->pClass->sName.nByte);` |
|      105 | 6053 | `		ph7_array_add_strkey_elem(pNat,"class",pValue);` |
|      105 | 6054 | `		ph7_value_reset_string_cursor(pValue);` |
|      105 | 6055 | `		if( iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/ ){` |
|       11 | 6056 | `			ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|       11 | 6057 | `			if( pObjVal ){` |
|       11 | 6058 | `				pRecv->iRef++;` |
|       11 | 6059 | `				pObjVal->x.pOther = pRecv;` |
|       11 | 6060 | `				MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|       11 | 6061 | `				ph7_array_add_strkey_elem(pNat,"object",pObjVal);` |
|       11 | 6062 | `				ph7_release_value(&(*pVm),pObjVal);` |
|        4 | 6063 | `			}` |
|        4 | 6064 | `		}` |
|      105 | 6065 | `		ph7_value_string(pValue,"->",2);` |
|      105 | 6066 | `		ph7_array_add_strkey_elem(pNat,"type",pValue);` |
|      105 | 6067 | `		ph7_value_reset_string_cursor(pValue);` |
|       50 | 6068 | `	}` |
|      107 | 6069 | `	if( pFrame->iFlags & VM_FRAME_FIBER ){` |
|      105 | 6070 | `		VmTraceNativeArgs(&(*pVm),iOptions,pFrame->pNativeCallerRec,pNat);` |
|       50 | 6071 | `	}` |
|      107 | 6072 | `	ph7_array_add_elem(pList,0,pNat);` |
|      107 | 6073 | `	ph7_release_value(&(*pVm),pNat);` |
|       56 | 6074 | `}` |
|        - | 6075 | `/*` |
|        - | 6076 | ` * The running internal call made before pNat, stepping over any a folded forward left` |
|        - | 6077 | ` * (VmNativeCall.bElided): php's compiler turned that call_user_func into a direct call,` |
|        - | 6078 | ` * so what it ran was reached from wherever the forward itself was.` |
|        - | 6079 | ` */` |
|    30832 | 6080 | `static VmNativeCall * VmNativeCallPrev(VmNativeCall *pNat)` |
|        5 | 6081 | `{` |
|    31089 | 6082 | `	for( pNat = pNat->pPrev ; pNat && pNat->bElided ; pNat = pNat->pPrev ){` |
|      130 | 6083 | `	}` |
|    30837 | 6084 | `	return pNat;` |
|        5 | 6085 | `}` |
|        - | 6086 | `/*` |
|        - | 6087 | ` * Are two running internal calls one NEST -- the second reached from inside the first,` |
|        - | 6088 | ` * with no userland code between? They share an activation, and they share the include` |
|        - | 6089 | ` * depth too: a builtin that loads a unit (spl_autoload()) runs it in its caller's` |
|        - | 6090 | ` * activation, so a call that unit makes is on the same frame but is not inside the` |
|        - | 6091 | ` * builtin as far as php's trace goes. It has a call site of its own.` |
|        - | 6092 | ` */` |
|    16627 | 6093 | `static int VmNativeCallSameNest(const VmNativeCall *pA,const VmNativeCall *pB)` |
|        5 | 6094 | `{` |
|    16632 | 6095 | `	return pA && pB && pA->pFrame == pB->pFrame && pA->nIncDepth == pB->nIncDepth;` |
|        5 | 6096 | `}` |
|        - | 6097 | `/*` |
|        - | 6098 | ` * Render pNat and every internal call of its nest it was reached from, innermost first:` |
|        - | 6099 | `` * each inner one with no file or line -- php's `[internal function]` -- and the`` |
|        - | 6100 | ` * outermost, the one userland code called, at pOuterFile and nOuterLine (the record's` |
|        - | 6101 | ` * own line when that is 0). Answers how many entries it added.` |
|        - | 6102 | ` */` |
|      654 | 6103 | `static sxi32 VmTraceNativeNest(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,sxi32 nDone,` |
|        - | 6104 | `	VmNativeCall *pNat,SyString *pOuterFile,sxu32 nOuterLine,ph7_value *pValue,ph7_value *pList)` |
|        5 | 6105 | `{` |
|      659 | 6106 | `	VmNativeCall *pFirst = pNat;` |
|      659 | 6107 | `	sxi32 nAdded = 0;` |
|     1391 | 6108 | `	for( ; VmNativeCallSameNest(pNat,pFirst) ; pNat = VmNativeCallPrev(pNat) ){` |
|        - | 6109 | `		ph7_value *pEntry;` |
|      737 | 6110 | `		if( iLimit != 0 && nDone + nAdded >= iLimit ){` |
|      ! 0 | 6111 | `			break;` |
|        - | 6112 | `		}` |
|      737 | 6113 | `		pEntry = ph7_new_array(&(*pVm));` |
|      737 | 6114 | `		if( pEntry == 0 ){` |
|      ! 0 | 6115 | `			break;` |
|        - | 6116 | `		}` |
|      737 | 6117 | `		nAdded++;` |
|      737 | 6118 | `		if( !VmNativeCallSameNest(VmNativeCallPrev(pNat),pNat) ){` |
|        - | 6119 | `			/* The outermost: the one userland code called. */` |
|      657 | 6120 | `			if( pOuterFile ){` |
|      657 | 6121 | `				ph7_value_string(pValue,pOuterFile->zString,(int)pOuterFile->nByte);` |
|      657 | 6122 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|      657 | 6123 | `				ph7_value_reset_string_cursor(pValue);` |
|      325 | 6124 | `			}` |
|      657 | 6125 | `			ph7_value_int(pValue,(int)(nOuterLine ? nOuterLine : (pNat->nLine ? pNat->nLine : 1)));` |
|      657 | 6126 | `			ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|      325 | 6127 | `		}` |
|      737 | 6128 | `		ph7_value_string(pValue,pNat->pName->zString,(int)pNat->pName->nByte);` |
|      737 | 6129 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|      737 | 6130 | `		ph7_value_reset_string_cursor(pValue);` |
|      737 | 6131 | `		if( pNat->pClass ){` |
|      153 | 6132 | `			ph7_value_string(pValue,pNat->pClass->sName.zString,(int)pNat->pClass->sName.nByte);` |
|      153 | 6133 | `			ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|      153 | 6134 | `			ph7_value_reset_string_cursor(pValue);` |
|      153 | 6135 | `			ph7_value_string(pValue,pNat->bStatic ? "::" : "->",2);` |
|      153 | 6136 | `			ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|      153 | 6137 | `			ph7_value_reset_string_cursor(pValue);` |
|       74 | 6138 | `		}` |
|      737 | 6139 | `		VmTraceNativeArgs(&(*pVm),iOptions,pNat,pEntry);` |
|      737 | 6140 | `		ph7_array_add_elem(pList,0,pEntry);` |
|      737 | 6141 | `		ph7_release_value(&(*pVm),pEntry);` |
|      370 | 6142 | `	}` |
|      659 | 6143 | `	return nAdded;` |
|        5 | 6144 | `}` |
|        - | 6145 | `/*` |
|        - | 6146 | ` * The INTERNAL calls that reached for pFrame's body as a callback. There can be more` |
|        - | 6147 | `` * than one: `array_map('call_user_func', ['f'])` runs f from inside call_user_func,`` |
|        - | 6148 | ` * itself run from inside array_map, and php gives each a frame -- the inner ones with` |
|        - | 6149 | ` * no file or line, the outermost with the userland call site. They are exactly the` |
|        - | 6150 | ` * innermost nest of running records entered from the activation above pFrame` |
|        - | 6151 | ` * (VmNativeCall), since that activation is suspended in the outermost of them; a native` |
|        - | 6152 | ` * method is named the way php names it, by its declaring class and its own separator.` |
|        - | 6153 | ` * Answers how many entries it added; 0 leaves the frame's single pNativeCaller to` |
|        - | 6154 | ` * VmTraceNativeCallerEntry, which is also the only rendering for a dispatch no record` |
|        - | 6155 | ` * describes (a shutdown function, an autoloader an opcode triggered).` |
|        - | 6156 | ` */` |
|      608 | 6157 | `static sxi32 VmTraceNativeCallerChain(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,sxi32 nDone,` |
|        - | 6158 | `	VmFrame *pFrame,SyString *pFile,ph7_value *pValue,ph7_value *pList)` |
|        5 | 6159 | `{` |
|      613 | 6160 | `	VmNativeCall *pNat = pVm->pNativeCall;` |
|     1086 | 6161 | `	while( pNat && (pNat->bElided \|\| pNat->pFrame != (void *)pFrame->pParent) ){` |
|      175 | 6162 | `		pNat = pNat->pPrev;` |
|        5 | 6163 | `	}` |
|      611 | 6164 | `	return VmTraceNativeNest(&(*pVm),iOptions,iLimit,nDone,pNat,` |
|      608 | 6165 | `		SyStringLength(&pFrame->sCallFile) > 0 ? &pFrame->sCallFile : pFile,` |
|      608 | 6166 | `		pFrame->nCallLine ? pFrame->nCallLine : 1,pValue,pList);` |
|        5 | 6167 | `}` |
|        - | 6168 | `/*` |
|        - | 6169 | ` * VmSkipExceptionFrames for a trace. A fiber's TRAMPOLINE body frame is transparent --` |
|        - | 6170 | ` * the callee it dispatched pushed the frame the trace shows -- but the Fiber method` |
|        - | 6171 | ` * that entered it is still a frame of php's, so it is emitted on the way past.` |
|        - | 6172 | ` */` |
|  2117932 | 6173 | `static VmFrame * VmTraceSkipFrames(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,sxi32 *pnDone,` |
|        - | 6174 | `	VmFrame *pFrame,SyString *pFile,ph7_value *pValue,ph7_value *pList)` |
|        5 | 6175 | `{` |
|  3687478 | 6176 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|  1569541 | 6177 | `		if( (pFrame->iFlags & VM_FRAME_FIBER) && pFrame->pNativeCaller` |
|       21 | 6178 | `		 && (iLimit == 0 \|\| *pnDone < iLimit) ){` |
|       19 | 6179 | `			(*pnDone)++;` |
|       19 | 6180 | `			VmTraceNativeCallerEntry(&(*pVm),iOptions,pFrame,pFile,pValue,pList);` |
|        8 | 6181 | `		}` |
|  1569546 | 6182 | `		pFrame = pFrame->pParent;` |
|        5 | 6183 | `	}` |
|  2117937 | 6184 | `	return pFrame;` |
|        5 | 6185 | `}` |
|        - | 6186 | `/*` |
|        - | 6187 | ` * Build php's backtrace (innermost active call first) into pList: one map per` |
|        - | 6188 | ` * ACTIVE call frame, describing the callee (function/class) and the CALL SITE` |
|        - | 6189 | ` * position -- so a frame can see who called it. Shared by debug_backtrace() and` |
|        - | 6190 | ` * the Throwable trace stamp so BOTH walk the full frame chain identically (the` |
|        - | 6191 | ` * stamp used to emit only the innermost frame, truncating every exception trace` |
|        - | 6192 | ` * to depth 1).` |
|        - | 6193 | ` *   iOptions bit1 = DEBUG_BACKTRACE_PROVIDE_OBJECT (attach the frame's $this as` |
|        - | 6194 | ` *   'object'); bit2 = DEBUG_BACKTRACE_IGNORE_ARGS (omit the 'args' list); bit8 =` |
|        - | 6195 | ` *   lead with the INTERNAL functions and methods currently running, which is` |
|        - | 6196 | ` *   php's exception trace and is NOT debug_backtrace() (php leaves its own` |
|        - | 6197 | ` *   internal frame off the array it hands back).` |
|        - | 6198 | ` * php's exception trace passes no PROVIDE_OBJECT, and IGNORE_ARGS while` |
|        - | 6199 | ` * zend.exception_ignore_args is On -- then its frame shape is` |
|        - | 6200 | ` * file/line/function[/class/type].` |
|        - | 6201 | ` */` |
|  1470351 | 6202 | `PH7_PRIVATE void VmBuildBacktrace(ph7_vm *pVm,sxi32 iOptions,sxi32 iLimit,ph7_value *pList)` |
|        5 | 6203 | `{` |
|        - | 6204 | `	SyString *pFile;` |
|        - | 6205 | `	VmFrame *pFrame;` |
|        - | 6206 | `	ph7_value *pValue;` |
|  1470356 | 6207 | `	sxi32 nDone = 0;` |
|  1470356 | 6208 | `	pValue = ph7_new_scalar(&(*pVm));` |
|  1470356 | 6209 | `	if( pValue == 0 ){` |
|      ! 0 | 6210 | `		return;` |
|        - | 6211 | `	}` |
|  1470356 | 6212 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - | 6213 | `	/* The INTERNAL functions and methods running right now come first: php gives each` |
|        - | 6214 | `	 * an execute_data of its own, so a throw raised inside a C body names that body as` |
|        - | 6215 | ``	 * frame #0 (`#0 file(line): str_repeat()`), and a native method names its class`` |
|        - | 6216 | ``	 * too (`#0 file(line): SplFileObject->__construct()`). Only the run entered from`` |
|        - | 6217 | `	 * THIS activation is emitted here -- one entered from further out is reached by the` |
|        - | 6218 | `	 * VM_FRAME_NATIVE_CALLER path below, which already renders it, and emitting it` |
|        - | 6219 | `	 * twice would double every array_map() line. Off for debug_backtrace(), which php` |
|        - | 6220 | `	 * leaves its own frame out of. */` |
|  1470356 | 6221 | `	if( (iOptions & 8) != 0 ){` |
|  1469896 | 6222 | `		VmNativeCall *pNat = pVm->pNativeCall;` |
|  1469896 | 6223 | `		if( pNat && pNat->bElided ){` |
|      355 | 6224 | `			pNat = VmNativeCallPrev(pNat);` |
|      175 | 6225 | `		}` |
|        - | 6226 | `		/* ...and only the nest the running code called: a builtin that loaded the` |
|        - | 6227 | `		 * unit now running (spl_autoload()) is on this activation too, but is reached` |
|        - | 6228 | `		 * by the include walk below, where php's trace puts it. */` |
|  1491955 | 6229 | `		while( pNat && pNat->pFrame == (void *)pVm->pFrame` |
|   757091 | 6230 | `		    && pNat->nIncDepth == SySetUsed(&pVm->aIncFrame) ){` |
|        - | 6231 | `			ph7_value *pNatEntry;` |
|    14514 | 6232 | `			if( iLimit != 0 && nDone >= iLimit ){` |
|      ! 0 | 6233 | `				break;` |
|        - | 6234 | `			}` |
|    14514 | 6235 | `			pNatEntry = ph7_new_array(&(*pVm));` |
|    14514 | 6236 | `			if( pNatEntry == 0 ){` |
|      ! 0 | 6237 | `				break;` |
|        - | 6238 | `			}` |
|    14514 | 6239 | `			nDone++;` |
|        - | 6240 | `			/* A call made from inside ANOTHER internal function has no source position` |
|        - | 6241 | `			 * at all, and php omits both keys rather than inventing one -- that is the` |
|        - | 6242 | ``			 * `#0 [internal function]: str_repeat()` under `array_map('str_repeat',…)`.`` |
|        - | 6243 | `			 * The predecessor sharing this record's activation is exactly that case, and` |
|        - | 6244 | `			 * so is a C function a fiber runs AS its body: its record sits on the fiber` |
|        - | 6245 | `			 * trampoline's transparent frame, called by Fiber->start() rather than by` |
|        - | 6246 | `			 * any bytecode. */` |
|    14509 | 6247 | `			if( !VmNativeCallSameNest(VmNativeCallPrev(pNat),pNat)` |
|    14434 | 6248 | `			 && (pVm->pFrame->iFlags & (VM_FRAME_FIBER\|VM_FRAME_EXCEPTION))` |
|     7149 | 6249 | `					!= (VM_FRAME_FIBER\|VM_FRAME_EXCEPTION) ){` |
|    14342 | 6250 | `				SyString *pNatFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|    14342 | 6251 | `				if( pNatFile == 0 ){` |
|      ! 0 | 6252 | `					pNatFile = pFile;` |
|      ! 0 | 6253 | `				}` |
|    14342 | 6254 | `				if( pNatFile ){` |
|    14342 | 6255 | `					ph7_value_string(pValue,pNatFile->zString,(int)pNatFile->nByte);` |
|    14342 | 6256 | `					ph7_array_add_strkey_elem(pNatEntry,"file",pValue);` |
|    14342 | 6257 | `					ph7_value_reset_string_cursor(pValue);` |
|     7143 | 6258 | `				}` |
|    14342 | 6259 | `				ph7_value_int(pValue,(int)(pNat->nLine ? pNat->nLine : 1));` |
|    14342 | 6260 | `				ph7_array_add_strkey_elem(pNatEntry,"line",pValue);` |
|     7143 | 6261 | `			}` |
|    14514 | 6262 | `			ph7_value_string(pValue,pNat->pName->zString,(int)pNat->pName->nByte);` |
|    14514 | 6263 | `			ph7_array_add_strkey_elem(pNatEntry,"function",pValue);` |
|    14514 | 6264 | `			ph7_value_reset_string_cursor(pValue);` |
|    14514 | 6265 | `			if( pNat->pClass ){` |
|     7660 | 6266 | `				ph7_value_string(pValue,pNat->pClass->sName.zString,` |
|     5104 | 6267 | `					(int)pNat->pClass->sName.nByte);` |
|     5109 | 6268 | `				ph7_array_add_strkey_elem(pNatEntry,"class",pValue);` |
|     5109 | 6269 | `				ph7_value_reset_string_cursor(pValue);` |
|     5109 | 6270 | `				ph7_value_string(pValue,pNat->bStatic ? "::" : "->",2);` |
|     5109 | 6271 | `				ph7_array_add_strkey_elem(pNatEntry,"type",pValue);` |
|     5109 | 6272 | `				ph7_value_reset_string_cursor(pValue);` |
|     2551 | 6273 | `			}` |
|        - | 6274 | `			/* No 'object' here: php has none for an internal frame a throw left. */` |
|    14514 | 6275 | `			VmTraceNativeArgs(&(*pVm),iOptions,pNat,pNatEntry);` |
|    14514 | 6276 | `			ph7_array_add_elem(pList,0,pNatEntry);` |
|    14514 | 6277 | `			ph7_release_value(&(*pVm),pNatEntry);` |
|    14514 | 6278 | `			pNat = VmNativeCallPrev(pNat);` |
|        5 | 6279 | `		}` |
|   734918 | 6280 | `	}` |
|  2205559 | 6281 | `	pFrame = pVm->pFrame` |
|  1470351 | 6282 | `		? VmTraceSkipFrames(&(*pVm),iOptions,iLimit,&nDone,pVm->pFrame,pFile,pValue,pList) : 0;` |
|  2117937 | 6283 | `	while( pFrame ){` |
|        - | 6284 | `		/* The include/require/eval activations started from THIS frame come first:` |
|        - | 6285 | `		 * they are still running, so they are inner to whatever called the frame.` |
|        - | 6286 | `		 * php shows each as a frame whose function is the construct's name, whose` |
|        - | 6287 | `		 * file and line are the call site, and whose single argument is the unit --` |
|        - | 6288 | `		 * except for the innermost entry of the whole trace, which php leaves` |
|        - | 6289 | `		 * argument-less. Nothing else records them: an include shares its caller's` |
|        - | 6290 | `		 * variable scope and pushes no VmFrame. */` |
|  2117937 | 6291 | `		sxu32 nInc = SySetUsed(&pVm->aIncFrame);` |
|  2117937 | 6292 | `		if( (iOptions & 4) != 0 && nInc > 0 && nDone == 0 ){` |
|        - | 6293 | `			/* The caller is a compile-time refusal, which php raises BEFORE it pushes` |
|        - | 6294 | `			 * the include/require/eval activation that is loading this unit -- so that` |
|        - | 6295 | `			 * one innermost activation is not on php's trace. (A class REDECLARATION` |
|        - | 6296 | `			 * is php's run-time refusal and does carry it; the caller says which.) */` |
|        9 | 6297 | `			VmIncFrame *pTop = (VmIncFrame *)SySetAt(&pVm->aIncFrame,nInc - 1);` |
|        9 | 6298 | `			if( pTop && pTop->pFrame == (void *)pFrame && pTop->pNat == 0 ){` |
|        9 | 6299 | `				nInc--;` |
|        3 | 6300 | `			}` |
|        3 | 6301 | `		}` |
|  2220578 | 6302 | `		while( nInc > 0 ){` |
|   102660 | 6303 | `			VmIncFrame *pInc = (VmIncFrame *)SySetAt(&pVm->aIncFrame,--nInc);` |
|        - | 6304 | `			ph7_value *pIncEntry;` |
|   102660 | 6305 | `			if( pInc == 0 \|\| pInc->pFrame != (void *)pFrame ){` |
|    38806 | 6306 | `				continue;` |
|        - | 6307 | `			}` |
|    63858 | 6308 | `			if( iLimit != 0 && nDone >= iLimit ){` |
|       15 | 6309 | `				break;` |
|        - | 6310 | `			}` |
|    63844 | 6311 | `			if( pInc->pNat ){` |
|        - | 6312 | `				/* A builtin loaded this unit (spl_autoload()): php has no include` |
|        - | 6313 | `				 * frame, only the builtin's and those of the internal calls that` |
|        - | 6314 | `				 * reached for it, the outermost at the line userland wrote. */` |
|       48 | 6315 | `				nDone += VmTraceNativeNest(&(*pVm),iOptions,iLimit,nDone,pInc->pNat,` |
|       46 | 6316 | `					SyStringLength(&pInc->sFile) > 0 ? &pInc->sFile : 0,0,pValue,pList);` |
|       48 | 6317 | `				continue;` |
|        - | 6318 | `			}` |
|    63798 | 6319 | `			pIncEntry = ph7_new_array(&(*pVm));` |
|    63798 | 6320 | `			if( pIncEntry == 0 ){` |
|      ! 0 | 6321 | `				break;` |
|        - | 6322 | `			}` |
|    63798 | 6323 | `			nDone++;` |
|    63798 | 6324 | `			if( SyStringLength(&pInc->sFile) > 0 ){` |
|    63798 | 6325 | `				ph7_value_string(pValue,pInc->sFile.zString,(int)pInc->sFile.nByte);` |
|    63798 | 6326 | `				ph7_array_add_strkey_elem(pIncEntry,"file",pValue);` |
|    63798 | 6327 | `				ph7_value_reset_string_cursor(pValue);` |
|    31896 | 6328 | `			}` |
|    63798 | 6329 | `			ph7_value_int(pValue,(int)pInc->nLine);` |
|    63798 | 6330 | `			ph7_array_add_strkey_elem(pIncEntry,"line",pValue);` |
|    63798 | 6331 | `			ph7_value_string(pValue,pInc->zName,-1);` |
|    63798 | 6332 | `			ph7_array_add_strkey_elem(pIncEntry,"function",pValue);` |
|    63798 | 6333 | `			ph7_value_reset_string_cursor(pValue);` |
|    63798 | 6334 | `			if( SyStringLength(&pInc->sPath) > 0 && ph7_array_count(pList) > 0 ){` |
|    38363 | 6335 | `				ph7_value *pArgs = ph7_new_array(&(*pVm));` |
|    38363 | 6336 | `				if( pArgs ){` |
|    38363 | 6337 | `					ph7_value *pArg = ph7_new_scalar(&(*pVm));` |
|    38363 | 6338 | `					if( pArg ){` |
|    38363 | 6339 | `						ph7_value_string(pArg,pInc->sPath.zString,(int)pInc->sPath.nByte);` |
|    38363 | 6340 | `						ph7_array_add_elem(pArgs,0,pArg);` |
|    38363 | 6341 | `						ph7_release_value(&(*pVm),pArg);` |
|    19180 | 6342 | `					}` |
|    38363 | 6343 | `					ph7_array_add_strkey_elem(pIncEntry,"args",pArgs);` |
|    38363 | 6344 | `					ph7_release_value(&(*pVm),pArgs);` |
|    19180 | 6345 | `				}` |
|    19180 | 6346 | `			}` |
|    63798 | 6347 | `			ph7_array_add_elem(pList,0,pIncEntry);` |
|    63798 | 6348 | `			ph7_release_value(&(*pVm),pIncEntry);` |
|        5 | 6349 | `		}` |
|        - | 6350 | `		/* $limit stops the walk after that many frames, 0 meaning "no limit".` |
|        - | 6351 | `		 * The test is php's own, on the NARROWED value: a limit that wraps` |
|        - | 6352 | `		 * negative reports NOTHING (frame 0 is already >= it), which is why` |
|        - | 6353 | `		 * debug_backtrace(0, PHP_INT_MAX) answers an empty array. */` |
|  2117937 | 6354 | `		if( iLimit != 0 && nDone >= iLimit ){` |
|       42 | 6355 | `			break;` |
|        - | 6356 | `		}` |
|  2117897 | 6357 | `		nDone++;` |
|  2117897 | 6358 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        - | 6359 | `		ph7_value *pEntry;` |
|  2117897 | 6360 | `		if( pFrame->pParent == 0 \|\| pFunc == 0 ){` |
|        - | 6361 | `			/* The global frame is not a call: php stops before it (no "{main}"). */` |
|   735132 | 6362 | `			break;` |
|        - | 6363 | `		}` |
|   647588 | 6364 | `		pEntry = ph7_new_array(&(*pVm));` |
|   647588 | 6365 | `		if( pEntry == 0 ){` |
|      ! 0 | 6366 | `			break;` |
|        - | 6367 | `		}` |
|        - | 6368 | `		/* php's key order: file, line, function[, class, type][, object][, args]. */` |
|        - | 6369 | `		{` |
|        - | 6370 | `			/* A callback an INTERNAL function reached for has no userland call site, and` |
|        - | 6371 | ``			 * php says so by OMITTING both keys -- `array_keys()` on such a frame answers`` |
|        - | 6372 | ``			 * `['function']` alone, and the renderer prints `[internal function]: `. The`` |
|        - | 6373 | `			 * builtin that reached for it becomes a frame of its own, below. PHL gave the` |
|        - | 6374 | `			 * callback the builtin's OWN call site and emitted no second frame, so a` |
|        - | 6375 | `			 * program walking a trace saw one frame where php has two, and saw a file on` |
|        - | 6376 | `			 * a frame php leaves fileless. (Respect\Validation's exception walks a trace` |
|        - | 6377 | `			 * until a frame has no file; here it never stopped, and blamed an eval()'d` |
|        - | 6378 | `			 * unit for a throw written in a test file.) */` |
|   971432 | 6379 | `			SyString *pFrameFile = SyStringLength(&pFrame->sCallFile) > 0` |
|   647583 | 6380 | `				? &pFrame->sCallFile : pFile;` |
|   647588 | 6381 | `			if( pFrameFile && (pFrame->iFlags & (VM_FRAME_NATIVE_CALLER\|VM_FRAME_NATIVE_TRACE\|VM_FRAME_FIBER)) == 0 ){` |
|   646873 | 6382 | `				ph7_value_string(pValue,pFrameFile->zString,(int)pFrameFile->nByte);` |
|   646873 | 6383 | `				ph7_array_add_strkey_elem(pEntry,"file",pValue);` |
|   646873 | 6384 | `				ph7_value_reset_string_cursor(pValue);` |
|   323383 | 6385 | `			}` |
|        - | 6386 | `		}` |
|   647588 | 6387 | `		if( (pFrame->iFlags & (VM_FRAME_NATIVE_CALLER\|VM_FRAME_NATIVE_TRACE\|VM_FRAME_FIBER)) == 0 ){` |
|   646873 | 6388 | `			ph7_value_int(pValue,(int)(pFrame->nCallLine ? pFrame->nCallLine : 1));` |
|   646873 | 6389 | `			ph7_array_add_strkey_elem(pEntry,"line",pValue);` |
|   323383 | 6390 | `		}` |
|        - | 6391 | `		{` |
|   647588 | 6392 | `			const char *zDisp = 0;` |
|   647588 | 6393 | `			int nDisp = PH7_VmFuncDisplayName(&(*pVm),pFunc,&zDisp);` |
|   647588 | 6394 | `			ph7_value_string(pValue,zDisp,nDisp);` |
|        - | 6395 | `		}` |
|   647588 | 6396 | `		ph7_array_add_strkey_elem(pEntry,"function",pValue);` |
|   647588 | 6397 | `		ph7_value_reset_string_cursor(pValue);` |
|        - | 6398 | `		{` |
|        - | 6399 | `			/* php's 'class' is the DECLARING class of the executing method (where it` |
|        - | 6400 | `			 * is defined), NOT the runtime $this class — an inherited method called` |
|        - | 6401 | `			 * on a subclass reports the base. pFunc->pUserData is that declaring` |
|        - | 6402 | `			 * class (oo.c installs methods with it). 'type' is '->' for an instance` |
|        - | 6403 | `			 * call and '::' for a static one (so static frames get class/type too,` |
|        - | 6404 | `			 * which the old $this-only path dropped). Fall back to the $this class` |
|        - | 6405 | `			 * for a bound closure (has $this but is not VM_FUNC_CLASS_METHOD). */` |
|   647588 | 6406 | `			SyString *pClsName = 0;` |
|   647588 | 6407 | `			const char *zType = "->";` |
|   647588 | 6408 | `			int bStatic = 0;` |
|   647588 | 6409 | `			ph7_class_instance *pRecv = pFrame->pThis;` |
|   652336 | 6410 | `			if( (pFunc->iFlags & VM_FUNC_CLOSURE) && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        - | 6411 | ``				/* A closure frame is its SCOPE's: `C->{closure:C::m():5}()` for one made in`` |
|        - | 6412 | ``				 * an instance method, `C::` for a static one, and nothing for one written at`` |
|        - | 6413 | `				 * the top level and never bound. The receiver a closure made in a method` |
|        - | 6414 | `				 * carries is a frame VARIABLE here, not pFrame->pThis, which only a rebind` |
|        - | 6415 | ``				 * sets; php's separator and `object` follow that receiver. */`` |
|        - | 6416 | `				ph7_class *pScope;` |
|     9447 | 6417 | `				pRecv = PH7_VmFrameThis(&(*pVm),pFrame);` |
|    14195 | 6418 | `				pScope = (pFrame->iFlags & VM_FRAME_UNSCOPED) ? 0` |
|    14122 | 6419 | `					: PH7_VmClosureFuncScope(&(*pVm),pFunc,pFrame->pBoundScope,pRecv != 0,` |
|     4687 | 6420 | `						pFrame->pSelfClass);` |
|     9447 | 6421 | `				if( pScope ){` |
|      377 | 6422 | `					pClsName = &pScope->sName;` |
|      377 | 6423 | `					bStatic = pRecv == 0;` |
|      377 | 6424 | `					zType = bStatic ? "::" : "->";` |
|      192 | 6425 | `				}` |
|   642840 | 6426 | `			}else if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        - | 6427 | `				/* php's separator says what the CALLEE is, not how the caller` |
|        - | 6428 | ``				 * happened to reach it: a static method is `::` even when the`` |
|        - | 6429 | ``				 * calling frame has a $this bound (`self::s()` from inside an`` |
|        - | 6430 | ``				 * instance method), which the pThis test reported as `->`.`` |
|        - | 6431 | `				 * ph7_class_method embeds its ph7_vm_func FIRST, so the method's` |
|        - | 6432 | `				 * own flags are one cast away. */` |
|   501139 | 6433 | `				ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|   501139 | 6434 | `				bStatic = (((ph7_class_method *)pFunc)->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|   501139 | 6435 | `				if( (pDecl->iFlags & PH7_CLASS_TRAIT) != 0 && pFrame->pSelfClass ){` |
|        - | 6436 | `					/* php COMPOSES a trait method into the using class, so a frame running` |
|        - | 6437 | `					 * one reports that class and never the trait. The class the call was` |
|        - | 6438 | `					 * made THROUGH names it -- the receiver's for an instance call, the` |
|        - | 6439 | `					 * named one for a static call -- and its ancestry is walked, so` |
|        - | 6440 | ``					 * `class Base { use T; } class Kid extends Base {}` is Base from a Kid`` |
|        - | 6441 | `					 * instance, where php composed the method. */` |
|       22 | 6442 | `					pDecl = PH7_VmTraitUsingClass(&(*pVm),pDecl,pFrame->pSelfClass);` |
|       10 | 6443 | `				}` |
|   501139 | 6444 | `				pClsName = &pDecl->sName;` |
|   501139 | 6445 | `				zType = bStatic ? "::" : "->";` |
|   387579 | 6446 | `			}else if( pFrame->pThis && pFrame->pThis->pClass ){` |
|      ! 0 | 6447 | `				pClsName = &pFrame->pThis->pClass->sName;` |
|      ! 0 | 6448 | `			}` |
|   647588 | 6449 | `			if( pClsName ){` |
|   501513 | 6450 | `				ph7_value_string(pValue,pClsName->zString,(int)pClsName->nByte);` |
|   501513 | 6451 | `				ph7_array_add_strkey_elem(pEntry,"class",pValue);` |
|   501513 | 6452 | `				ph7_value_reset_string_cursor(pValue);` |
|   501513 | 6453 | `				ph7_value_string(pValue,zType,(int)SyStrlen(zType));` |
|   501513 | 6454 | `				ph7_array_add_strkey_elem(pEntry,"type",pValue);` |
|   501513 | 6455 | `				ph7_value_reset_string_cursor(pValue);` |
|   501508 | 6456 | `				if( (iOptions & 1 /*DEBUG_BACKTRACE_PROVIDE_OBJECT*/) && pRecv` |
|      118 | 6457 | `				 && !bStatic ){` |
|      103 | 6458 | `					ph7_value *pObjVal = ph7_new_scalar(&(*pVm));` |
|      103 | 6459 | `					if( pObjVal ){` |
|      103 | 6460 | `						pRecv->iRef++;` |
|      103 | 6461 | `						pObjVal->x.pOther = pRecv;` |
|      103 | 6462 | `						MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|      103 | 6463 | `						ph7_array_add_strkey_elem(pEntry,"object",pObjVal);` |
|      103 | 6464 | `						ph7_release_value(&(*pVm),pObjVal);` |
|       49 | 6465 | `					}` |
|       49 | 6466 | `				}` |
|   250754 | 6467 | `			}` |
|        - | 6468 | `		}` |
|   647588 | 6469 | `		if( (iOptions & 2 /*DEBUG_BACKTRACE_IGNORE_ARGS*/) == 0 ){` |
|      753 | 6470 | `			ph7_value *pArg = ph7_new_array(&(*pVm));` |
|      753 | 6471 | `			if( pArg ){` |
|        - | 6472 | `				/* The arguments the caller actually PASSED, which is not the same` |
|        - | 6473 | `				 * list as the frame's installed slots -- see PH7_VmFrameActualArgs` |
|        - | 6474 | `				 * (shared with func_get_args()). */` |
|      753 | 6475 | `				PH7_VmFrameActualArgs(&(*pVm),pFrame,pArg,1);` |
|      753 | 6476 | `				ph7_array_add_strkey_elem(pEntry,"args",pArg);` |
|      753 | 6477 | `				ph7_release_value(&(*pVm),pArg);` |
|      374 | 6478 | `			}` |
|      374 | 6479 | `		}` |
|   647588 | 6480 | `		ph7_array_add_elem(pList,0/* Automatic index assign*/,pEntry);` |
|   647588 | 6481 | `		ph7_release_value(&(*pVm),pEntry);` |
|   647583 | 6482 | `		if( (pFrame->iFlags & (VM_FRAME_NATIVE_CALLER\|VM_FRAME_NATIVE_TRACE\|VM_FRAME_FIBER)) != 0` |
|   324103 | 6483 | `		 && pFrame->pNativeCaller ){` |
|        - | 6484 | `			/* php gives the INTERNAL function that reached for the callback a frame of` |
|        - | 6485 | `			 * its own, and that one carries the userland call site the callback's frame` |
|        - | 6486 | `			 * just declined (VmTraceNativeCallerEntry). A fiber's body is one of these:` |
|        - | 6487 | `			 * php runs it as a callback of the Fiber method that entered it last.` |
|        - | 6488 | `			 *` |
|        - | 6489 | `			 * Not every such dispatch gets one: php's two FORWARDS, call_user_func()` |
|        - | 6490 | `			 * and call_user_func_array(), are ELIDED BY THE COMPILER when written` |
|        - | 6491 | `			 * literally, so the callback's frame is the caller's own -- and those` |
|        - | 6492 | `			 * spellings never reach here, because the forwards pass the caller's mode` |
|        - | 6493 | `` 			 * through PH7_VmCallUserFunctionWithMap and set no latch. A `$n('cuf')($c)` `` |
|        - | 6494 | `			 * through a variable is NOT elided, does set the latch, and does get this` |
|        - | 6495 | `			 * frame, exactly as php's does. */` |
|        - | 6496 | `			sxi32 nChain;` |
|      699 | 6497 | `			if( iLimit != 0 && nDone >= iLimit ){` |
|        3 | 6498 | `				break;` |
|        - | 6499 | `			}` |
|     1044 | 6500 | `			nChain = (pFrame->iFlags & VM_FRAME_FIBER) == 0` |
|      608 | 6501 | `				? VmTraceNativeCallerChain(&(*pVm),iOptions,iLimit,nDone,pFrame,pFile,pValue,pList)` |
|      347 | 6502 | `				: 0;` |
|      697 | 6503 | `			if( nChain > 0 ){` |
|      611 | 6504 | `				nDone += nChain;` |
|      307 | 6505 | `			}else{` |
|       91 | 6506 | `				nDone++;` |
|       91 | 6507 | `				VmTraceNativeCallerEntry(&(*pVm),iOptions,pFrame,pFile,pValue,pList);` |
|        - | 6508 | `			}` |
|      345 | 6509 | `		}` |
|   647586 | 6510 | `		pFrame = pFrame->pParent` |
|   647581 | 6511 | `			? VmTraceSkipFrames(&(*pVm),iOptions,iLimit,&nDone,pFrame->pParent,pFile,pValue,pList)` |
|   323843 | 6512 | `			: 0;` |
|        5 | 6513 | `	}` |
|  1470356 | 6514 | `	ph7_release_value(&(*pVm),pValue);` |
|   735153 | 6515 | `}` |
|        - | 6516 | `/*` |
|        - | 6517 | ` * Stamp a freshly created Throwable with the site it was created at.` |
|        - | 6518 | ` *` |
|        - | 6519 | `` * php records `file`/`line` on the OBJECT at creation time -- not inside`` |
|        - | 6520 | ` * Exception::__construct -- so a subclass that overrides the constructor and never` |
|        - | 6521 | ` * calls parent::__construct still reports the right position. The embedded` |
|        - | 6522 | `` * Exception/Error constructors used to assign `$this->line = __LINE__`, which`` |
|        - | 6523 | ` * resolved against the EMBEDDED chunk (always line 1); they no longer touch either` |
|        - | 6524 | ` * field, and this runs for every instantiation path (OP_NEW and the engine's own` |
|        - | 6525 | ` * VmThrowBuiltinError / VmThrowFixedError).` |
|        - | 6526 | ` */` |
|  1679380 | 6527 | `PH7_PRIVATE void PH7_VmStampThrowableSite(ph7_vm *pVm,ph7_class_instance *pThis)` |
|        5 | 6528 | `{` |
|        - | 6529 | `	static const char *azField[] = { "file", "line", "trace" };` |
|        - | 6530 | `	ph7_class *pThrowable;` |
|        - | 6531 | `	SyString *pFile;` |
|        - | 6532 | `	SyString *pSiteFile;` |
|        - | 6533 | `	sxu32 nPreLine;` |
|        - | 6534 | `	sxu32 n;` |
|  1679385 | 6535 | `	if( pThis == 0 \|\| pThis->pClass == 0 ){` |
|      ! 0 | 6536 | `		return;` |
|        - | 6537 | `	}` |
|  1679385 | 6538 | `	pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|  1679385 | 6539 | `	if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|   209494 | 6540 | `		return;` |
|        - | 6541 | `	}` |
|  1469896 | 6542 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - | 6543 | ``	/* getFile() is the file the `new` is WRITTEN in, which is the same question`` |
|        - | 6544 | `	 * __FILE__ asks and PH7_VmExecutingUnitFile is the one answer to: the running` |
|        - | 6545 | `	 * function's DEFINING file, the include-stack top for code at a loaded unit's` |
|        - | 6546 | `	 * top level, and an eval()'d chunk's own name inside one. This asked it by` |
|        - | 6547 | `	 * hand and knew only the first half, so an undefined function called at the` |
|        - | 6548 | `	 * top level of a file some METHOD included was reported in the method's file` |
|        - | 6549 | `	 * (PHPUnit's TestSuiteLoader is exactly that shape, and named itself instead` |
|        - | 6550 | `	 * of the test file it had just loaded). */` |
|  1469896 | 6551 | `	pSiteFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|  1469896 | 6552 | `	if( pSiteFile == 0 ){` |
|      ! 0 | 6553 | `		pSiteFile = pFile;` |
|      ! 0 | 6554 | `	}` |
|        - | 6555 | `	/* ...unless a prelude builtin is what is running: php has no frame for one,` |
|        - | 6556 | `	 * so the throw it makes is reported at the call the program wrote. Without` |
|        - | 6557 | `	 * this every exception these ~24 builtins raise answered getLine() 1 -- the` |
|        - | 6558 | `	 * whole embedded chunk is a single source line. */` |
|        - | 6559 | `	{` |
|  1469896 | 6560 | `		SyString *pPreFile = 0;` |
|  1469896 | 6561 | `		nPreLine = 0;` |
|  1469896 | 6562 | `		if( PH7_VmPreludeBuiltinFrame(&(*pVm),&pPreFile,&nPreLine) ){` |
|      145 | 6563 | `			if( pPreFile ){` |
|      145 | 6564 | `				pSiteFile = pPreFile;` |
|       71 | 6565 | `			}` |
|      145 | 6566 | `			if( nPreLine < 1 ){` |
|      ! 0 | 6567 | `				nPreLine = 1;` |
|      ! 0 | 6568 | `			}` |
|       71 | 6569 | `		}` |
|        - | 6570 | `	}` |
|        - | 6571 | `	/* ...and an argument refusal belongs to the refused parameter (VmArgSiteArm),` |
|        - | 6572 | `	 * which wins over both: a prelude builtin that calls back into user code is` |
|        - | 6573 | `	 * still not where the callee's parameter is written. One-shot. */` |
|  1469896 | 6574 | `	if( pVm->nArgSiteLine ){` |
|      749 | 6575 | `		if( pVm->pArgSiteFile && SyStringLength(pVm->pArgSiteFile) > 0 ){` |
|      749 | 6576 | `			pSiteFile = (SyString *)pVm->pArgSiteFile;` |
|      372 | 6577 | `		}` |
|      749 | 6578 | `		nPreLine = pVm->nArgSiteLine;` |
|      749 | 6579 | `		pVm->nArgSiteLine = 0;` |
|      749 | 6580 | `		pVm->pArgSiteFile = 0;` |
|      372 | 6581 | `	}` |
|  5879569 | 6582 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|        - | 6583 | `		SyHashEntry *pEntry;` |
|        - | 6584 | `		VmClassAttr *pVmAttr;` |
|        - | 6585 | `		ph7_value *pAttrValue;` |
|  4409678 | 6586 | `		pEntry = SyHashGet(&pThis->hAttr,(const void *)azField[n],SyStrlen(azField[n]));` |
|  4409678 | 6587 | `		if( pEntry == 0 ){` |
|      ! 0 | 6588 | `			continue;` |
|        - | 6589 | `		}` |
|  4409678 | 6590 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|  4409678 | 6591 | `		pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  4409678 | 6592 | `		if( pAttrValue == 0 ){` |
|      ! 0 | 6593 | `			continue;` |
|        - | 6594 | `		}` |
|        - | 6595 | ``		/* The stamp IS this slot's initialization. `Error` declares`` |
|        - | 6596 | ``		 * `protected int $line` with no default (php's stub, and php's own`` |
|        - | 6597 | `		 * Reflection agrees), so its slot starts UNINITIALIZED -- and writing` |
|        - | 6598 | `		 * it here without clearing that flag left every Error, TypeError and` |
|        - | 6599 | ``		 * ValueError reporting `uninitialized(int)` in var_dump, one property`` |
|        - | 6600 | `		 * short in the (array) cast, get_object_vars(), get_mangled_object_vars(),` |
|        - | 6601 | `		 * json_encode(), serialize() and array_walk(), while getLine() answered` |
|        - | 6602 | `		 * the real number. Exception hid it by declaring a default. */` |
|  4409678 | 6603 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|  4409678 | 6604 | `		if( n == 0 ){` |
|  1469896 | 6605 | `			if( pSiteFile ){` |
|  1469896 | 6606 | `				PH7_MemObjRelease(pAttrValue);` |
|  1469896 | 6607 | `				PH7_MemObjInitFromString(&(*pVm),pAttrValue,pSiteFile);` |
|   734923 | 6608 | `			}` |
|  3674705 | 6609 | `		}else if( n == 1 ){` |
|        - | 6610 | `			/* nLazyInitLine wins while a lazily-evaluated class initializer runs its` |
|        - | 6611 | `			 * OWN bytecode: php evaluates that expression AT THE ACCESS, so` |
|        - | 6612 | ``			 * `class C { public static $s = UNDEF; } ... C::$s;` reports the`` |
|        - | 6613 | `			 * access's line, not the declaration's. The depth test keeps the window` |
|        - | 6614 | `			 * off everything the initializer calls — an autoloader, a nested` |
|        - | 6615 | `			 * constant's evaluation — which report their own lines in both engines.` |
|        - | 6616 | `			 * (Enum cases never set it: php reports the CASE's own line there, and` |
|        - | 6617 | `			 * PHL already matches.) */` |
|  2939344 | 6618 | `			sxu32 nLine = nPreLine ? nPreLine` |
|  2938454 | 6619 | `				: (pVm->nLazyInitLine && pVm->nVmExecDepth == pVm->nLazyInitDepth)` |
|       42 | 6620 | `				? pVm->nLazyInitLine` |
|  1469006 | 6621 | `				: (pVm->nCurLine ? pVm->nCurLine : 1);` |
|  1469896 | 6622 | `			PH7_MemObjRelease(pAttrValue);` |
|  1469896 | 6623 | `			PH7_MemObjInitFromInt(&(*pVm),pAttrValue,(sxi64)nLine);` |
|   734923 | 6624 | `		}else{` |
|        - | 6625 | `			/* trace: php captures the FULL backtrace at the CREATION site (innermost` |
|        - | 6626 | ``			 * = the function that ran `new`, reported at its call site). Reuse the`` |
|        - | 6627 | `			 * shared walk with no PROVIDE_OBJECT, and with IGNORE_ARGS exactly when` |
|        - | 6628 | `			 * zend.exception_ignore_args says so -- php reads the directive here, at` |
|        - | 6629 | `			 * construction, so an ini_set() made after the throw changes nothing. */` |
|  1469896 | 6630 | `			ph7_value *pList = ph7_new_array(&(*pVm));` |
|  1469896 | 6631 | `			if( pList == 0 ){` |
|      ! 0 | 6632 | `				continue;` |
|        - | 6633 | `			}` |
|  2204814 | 6634 | `			VmBuildBacktrace(&(*pVm),` |
|  1469891 | 6635 | `				(PH7_VmIniGetBool(&(*pVm),"zend.exception_ignore_args",1) ? 2 : 0)\|8,0,pList);` |
|        - | 6636 | `			/* Building the trace reserves new memobjs, which used to realloc` |
|        - | 6637 | `			 * pVm->aMemObj and INVALIDATE pAttrValue (a pointer INTO the pool,` |
|        - | 6638 | `			 * from PH7_ClassInstanceExtractAttrValue above). Redundant since P1` |
|        - | 6639 | `			 * (fixed segments); left for the harvest sweep. */` |
|  1469896 | 6640 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|  1469896 | 6641 | `			if( pAttrValue ){` |
|  1469896 | 6642 | `				PH7_MemObjRelease(pAttrValue);` |
|  1469896 | 6643 | `				PH7_MemObjStore(pList,pAttrValue);` |
|   734918 | 6644 | `			}` |
|  1469896 | 6645 | `			ph7_release_value(&(*pVm),pList);` |
|        - | 6646 | `		}` |
|  2204759 | 6647 | `	}` |
|   839527 | 6648 | `}` |
|     6934 | 6649 | `PH7_PRIVATE const char * VmArithTypeName(ph7_value *pVal)` |
|        4 | 6650 | `{` |
|     6938 | 6651 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       56 | 6652 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       56 | 6653 | `		if( pInst && pInst->pClass ){` |
|       56 | 6654 | `			return pInst->pClass->sName.zString;` |
|        - | 6655 | `		}` |
|      ! 0 | 6656 | `	}` |
|     6884 | 6657 | `	return ph7_type_name(pVal);` |
|     3471 | 6658 | `}` |
|        - | 6659 | `/*` |
|        - | 6660 | ` * php's VALUE name (zend_zval_value_name, 8.3+) rather than its TYPE name: a` |
|        - | 6661 | `` * boolean is named by the value it holds — `false` / `true` — everywhere php`` |
|        - | 6662 | ` * describes an operand it could not use as one ("on false", "false given").` |
|        - | 6663 | ` * Deliberately NOT the whole diagnostic surface: the ZPP messages,` |
|        - | 6664 | `` * `Value of type bool is not callable` and `Unsupported operand types: bool +`` |
|        - | 6665 | `` * array` stay on the TYPE name, which is why this sits beside VmArithTypeName`` |
|        - | 6666 | ` * instead of replacing it.` |
|        - | 6667 | ` */` |
|      166 | 6668 | `PH7_PRIVATE const char * VmArithValueName(ph7_value *pVal)` |
|        3 | 6669 | `{` |
|      169 | 6670 | `	if( (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_OBJ)) == MEMOBJ_BOOL ){` |
|       23 | 6671 | `		return pVal->x.iVal ? "true" : "false";` |
|        - | 6672 | `	}` |
|      147 | 6673 | `	return VmArithTypeName(&(*pVal));` |
|       86 | 6674 | `}` |
|        - | 6675 | `/*` |
|        - | 6676 | ` * php's ++/-- operand contract: an array, object or resource operand is a` |
|        - | 6677 | ` * TypeError ("Cannot increment array", "Cannot decrement P", "Cannot increment` |
|        - | 6678 | ` * resource"), never the silent no-op PH7 used to perform. Word it once here so` |
|        - | 6679 | ` * the plain opcode path and the hooked-property path cannot drift apart.` |
|        - | 6680 | ` * bIncr selects the verb; the value itself is left untouched by php.` |
|        - | 6681 | ` */` |
|       40 | 6682 | `PH7_PRIVATE void VmIncDecTypeErrorMsg(ph7_value *pVal,int bIncr,SyBlob *pMsgOut)` |
|        1 | 6683 | `{` |
|       41 | 6684 | `	const char *zVerb = bIncr ? "increment" : "decrement";` |
|       41 | 6685 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       19 | 6686 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       19 | 6687 | `		if( pInst && pInst->pClass ){` |
|       19 | 6688 | `			SyBlobFormat(pMsgOut,"Cannot %s %z",zVerb,&pInst->pClass->sDisp);` |
|       19 | 6689 | `			return;` |
|        - | 6690 | `		}` |
|      ! 0 | 6691 | `	}` |
|       23 | 6692 | `	SyBlobFormat(pMsgOut,"Cannot %s %s",zVerb,ph7_type_name(pVal));` |
|       21 | 6693 | `}` |
|        - | 6694 | `/*` |
|        - | 6695 | `` * php's `&`, `\|` and `^` over TWO STRINGS: a per-BYTE operation whose result is`` |
|        - | 6696 | `` * a binary STRING, not an integer — `"abc" & "abd"` is "ab`", and PHL answered`` |
|        - | 6697 | `` * int(0) for every such pair because it cast both sides to int first. `&` and`` |
|        - | 6698 | `` * `^` stop at the SHORTER operand; `\|` runs to the LONGER one, the missing bytes`` |
|        - | 6699 | ` * reading as 0, so the longer operand's tail is copied verbatim. cOp is '&', '\|'` |
|        - | 6700 | ` * or '^'; the result is appended to *pOut (the caller owns it).` |
|        - | 6701 | ` */` |
|      250 | 6702 | `PH7_PRIVATE void VmStringBitwise(ph7_value *pLeft,ph7_value *pRight,int cOp,SyBlob *pOut)` |
|        1 | 6703 | `{` |
|      251 | 6704 | `	const unsigned char *zL = (const unsigned char *)SyBlobData(&pLeft->sBlob);` |
|      251 | 6705 | `	const unsigned char *zR = (const unsigned char *)SyBlobData(&pRight->sBlob);` |
|      251 | 6706 | `	sxu32 nL = SyBlobLength(&pLeft->sBlob);` |
|      251 | 6707 | `	sxu32 nR = SyBlobLength(&pRight->sBlob);` |
|      251 | 6708 | `	sxu32 nMin = nL < nR ? nL : nR;` |
|        - | 6709 | `	sxu32 i;` |
|      523 | 6710 | `	for( i = 0 ; i < nMin ; ++i ){` |
|        - | 6711 | `		unsigned char c;` |
|      273 | 6712 | `		if( cOp == '\|' ){` |
|       93 | 6713 | `			c = (unsigned char)(zL[i] \| zR[i]);` |
|      227 | 6714 | `		}else if( cOp == '^' ){` |
|       89 | 6715 | `			c = (unsigned char)(zL[i] ^ zR[i]);` |
|       45 | 6716 | `		}else{` |
|       93 | 6717 | `			c = (unsigned char)(zL[i] & zR[i]);` |
|        - | 6718 | `		}` |
|      273 | 6719 | `		SyBlobAppend(pOut,(const void *)&c,sizeof(char));` |
|      137 | 6720 | `	}` |
|      251 | 6721 | `	if( cOp == '\|' && nL != nR ){` |
|       63 | 6722 | `		const unsigned char *zTail = nL > nR ? &zL[nMin] : &zR[nMin];` |
|       63 | 6723 | `		sxu32 nTail = (nL > nR ? nL : nR) - nMin;` |
|       63 | 6724 | `		SyBlobAppend(pOut,(const void *)zTail,nTail);` |
|       31 | 6725 | `	}` |
|      251 | 6726 | `}` |
|        - | 6727 | `/*` |
|        - | 6728 | ` * php's operand name in the bitwise-not diagnostic. It is NOT the arithmetic` |
|        - | 6729 | `` * type name: zend spells a BOOL there as the literal `true`/`false` (where`` |
|        - | 6730 | `` * "Unsupported operand types" says `bool`), and an object as its CLASS. Only the`` |
|        - | 6731 | `` * types `~` refuses reach this — a string is complemented byte-wise and a`` |
|        - | 6732 | ` * number is complemented as an integer — and an UNINITIALIZED slot is php's` |
|        - | 6733 | ` * null, which is what an undefined variable answers.` |
|        - | 6734 | ` */` |
|       26 | 6735 | `PH7_PRIVATE void VmBitNotTypeErrorMsg(ph7_value *pVal,SyBlob *pMsgOut)` |
|        1 | 6736 | `{` |
|       27 | 6737 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       11 | 6738 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       11 | 6739 | `		if( pInst && pInst->pClass ){` |
|       11 | 6740 | `			SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %z",&pInst->pClass->sDisp);` |
|       11 | 6741 | `			return;` |
|        - | 6742 | `		}` |
|      ! 0 | 6743 | `	}` |
|       17 | 6744 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        7 | 6745 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",` |
|        4 | 6746 | `			pVal->x.iVal ? "true" : "false");` |
|       15 | 6747 | `	}else if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_RES\|MEMOBJ_OBJ) ){` |
|       11 | 6748 | `		SyBlobFormat(pMsgOut,"Cannot perform bitwise not on %s",ph7_type_name(pVal));` |
|        6 | 6749 | `	}else{` |
|        3 | 6750 | `		SyBlobAppend(pMsgOut,"Cannot perform bitwise not on null",` |
|        - | 6751 | `			sizeof("Cannot perform bitwise not on null")-1);` |
|        - | 6752 | `	}` |
|       14 | 6753 | `}` |
|        - | 6754 | `/*` |
|        - | 6755 | `` * php compiles unary minus as `$x * -1` and unary plus as `$x * 1`, so both`` |
|        - | 6756 | `` * inherit the MULTIPLICATION operand contract -- message included: `-$o` is`` |
|        - | 6757 | `` * "Unsupported operand types: P * int", `-[1]` is "array * int" and `-"abc"` is`` |
|        - | 6758 | ` * "string * int", while a leading-numeric string ("-\"12abc\"") warns and` |
|        - | 6759 | ` * computes with the prefix. Classify pVal against that contract.` |
|        - | 6760 | ` */` |
|   113228 | 6761 | `PH7_PRIVATE sxi32 VmUnaryArithOperandCheck(ph7_vm *pVm,ph7_value *pVal,SyBlob *pMsgOut)` |
|        5 | 6762 | `{` |
|        - | 6763 | `	ph7_value sInt;` |
|        - | 6764 | `	sxi32 rc;` |
|   113233 | 6765 | `	PH7_MemObjInitFromInt(&(*pVm),&sInt,1);` |
|   113233 | 6766 | `	rc = VmArithOperandCheck(&(*pVm),pVal,&sInt,"*",pMsgOut);` |
|   113233 | 6767 | `	PH7_MemObjRelease(&sInt);` |
|   113233 | 6768 | `	return rc;` |
|        5 | 6769 | `}` |
|        - | 6770 | `/*` |
|        - | 6771 | ` * One arithmetic operator's whole prologue: php's do_operation handler first,` |
|        - | 6772 | ` * then the ordinary operand contract.` |
|        - | 6773 | ` *` |
|        - | 6774 | ` * php asks the LEFT operand's class for a handler and falls back to the RIGHT` |
|        - | 6775 | `` * one's, which is why an `int + Number` works as well as a `Number + int`. A`` |
|        - | 6776 | ` * handler that answers writes into pDest (the slot the opcode was going to` |
|        - | 6777 | ` * leave its result in), so the caller's only job is to skip the numeric` |
|        - | 6778 | ` * arithmetic. A handler that REFUSES hands back an exception class and a` |
|        - | 6779 | ` * message, and the caller throws them where it would have thrown the TypeError` |
|        - | 6780 | ` * -- after settling the operand stack.` |
|        - | 6781 | ` */` |
|        - | 6782 | ``/* Does this value's class declare php's do_operation? `++`/`--` ask before they`` |
|        - | 6783 | ` * refuse an object, since everything below that refusal is numeric. */` |
| 12976104 | 6784 | `PH7_PRIVATE int PH7_ValueHasArithHandler(ph7_value *pVal)` |
|        5 | 6785 | `{` |
|        - | 6786 | `	ph7_class_instance *pInst;` |
| 12976109 | 6787 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 ){` |
| 12976073 | 6788 | `		return 0;` |
|        - | 6789 | `	}` |
|       37 | 6790 | `	pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       37 | 6791 | `	return pInst->pClass != 0 && pInst->pClass->xArith != 0;` |
|  6489130 | 6792 | `}` |
|  7308421 | 6793 | `PH7_PRIVATE int VmArithOperandStep(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,` |
|        - | 6794 | `	const char *zOp,ph7_value *pDest,const char **pzClass,SyBlob *pMsgOut)` |
|        5 | 6795 | `{` |
|  7308426 | 6796 | `	ph7_class_instance *pInst = 0;` |
|        - | 6797 | `	int i;` |
|  7308426 | 6798 | `	*pzClass = "TypeError";` |
| 21925138 | 6799 | `	for( i = 0 ; i < 2 ; ++i ){` |
| 14616785 | 6800 | `		ph7_value *pSide = i == 0 ? pLeft : pRight;` |
| 14616785 | 6801 | `		if( (pSide->iFlags & MEMOBJ_OBJ) != 0 && pSide->x.pOther ){` |
|       79 | 6802 | `			ph7_class_instance *pCand = (ph7_class_instance *)pSide->x.pOther;` |
|       79 | 6803 | `			if( pCand->pClass && pCand->pClass->xArith ){` |
|       70 | 6804 | `				pInst = pCand;` |
|       70 | 6805 | `				break;` |
|        - | 6806 | `			}` |
|        4 | 6807 | `		}` |
|  7309818 | 6808 | `	}` |
|  7308426 | 6809 | `	if( pInst ){` |
|        - | 6810 | `		PH7_NativeArithCtx sCtx;` |
|        - | 6811 | `		ph7_value sRes;` |
|       70 | 6812 | `		PH7_MemObjInit(&(*pVm),&sRes);` |
|       70 | 6813 | `		sCtx.zOp = zOp;` |
|       70 | 6814 | `		sCtx.pLeft = pLeft;` |
|       70 | 6815 | `		sCtx.pRight = pRight;` |
|       70 | 6816 | `		sCtx.pResult = &sRes;` |
|       70 | 6817 | `		sCtx.bHandled = 0;` |
|       70 | 6818 | `		sCtx.zThrowClass = 0;` |
|       70 | 6819 | `		sCtx.zThrowMsg[0] = 0;` |
|       70 | 6820 | `		pInst->pClass->xArith(&(*pVm),pInst,&sCtx);` |
|       70 | 6821 | `		if( sCtx.zThrowClass ){` |
|       11 | 6822 | `			PH7_MemObjRelease(&sRes);` |
|       11 | 6823 | `			*pzClass = sCtx.zThrowClass;` |
|       11 | 6824 | `			SyBlobAppend(pMsgOut,sCtx.zThrowMsg,(sxu32)SyStrlen(sCtx.zThrowMsg));` |
|       32 | 6825 | `			return PH7_ARITH_REFUSED;` |
|        - | 6826 | `		}` |
|       60 | 6827 | `		if( sCtx.bHandled ){` |
|       44 | 6828 | `			PH7_MemObjStore(&sRes,pDest);` |
|       44 | 6829 | `			PH7_MemObjRelease(&sRes);` |
|       44 | 6830 | `			return PH7_ARITH_HANDLED;` |
|        - | 6831 | `		}` |
|       18 | 6832 | `		PH7_MemObjRelease(&sRes);` |
|        8 | 6833 | `	}` |
|  7308374 | 6834 | `	if( VmArithOperandCheck(&(*pVm),pLeft,pRight,zOp,pMsgOut) != SXRET_OK ){` |
|     1890 | 6835 | `		return PH7_ARITH_REFUSED;` |
|        - | 6836 | `	}` |
|  7306486 | 6837 | `	return PH7_ARITH_ORDINARY;` |
|  3654944 | 6838 | `}` |
|  7436110 | 6839 | `PH7_PRIVATE sxi32 VmArithOperandCheck(ph7_vm *pVm,ph7_value *pLeft,ph7_value *pRight,const char *zOp,SyBlob *pMsgOut)` |
|        5 | 6840 | `{` |
|  7436115 | 6841 | `	int bBadL = 0, bBadR = 0;` |
|        - | 6842 | `	int i;` |
|        - | 6843 | `	ph7_value *apOperand[2];` |
|  7436115 | 6844 | `	apOperand[0] = pLeft;` |
|  7436115 | 6845 | `	apOperand[1] = pRight;` |
|        - | 6846 | `	/* array + array is php's union operator, not arithmetic */` |
|  7436110 | 6847 | `	if( zOp[0] == '+' && zOp[1] == '\0'` |
|  7201292 | 6848 | `	 && (pLeft->iFlags & MEMOBJ_HASHMAP) && (pRight->iFlags & MEMOBJ_HASHMAP) ){` |
|     6806 | 6849 | `		return SXRET_OK;` |
|        - | 6850 | `	}` |
| 22282656 | 6851 | `	for( i = 0 ; i < 2 ; ++i ){` |
| 14856727 | 6852 | `		ph7_value *pVal = apOperand[i];` |
| 14856727 | 6853 | `		int bBad = 0;` |
| 14856727 | 6854 | `		if( pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     1253 | 6855 | `			bBad = 1;` |
|     1253 | 6856 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       55 | 6857 | `				ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|        - | 6858 | `				/* A class whose cast_object really answers a number is an operand` |
|        - | 6859 | ``				 * php accepts: `$xml->qty + 1` adds to the element's text. */`` |
|       55 | 6860 | `				if( pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass) ){` |
|        5 | 6861 | `					bBad = 0;` |
|        2 | 6862 | `				}` |
|       29 | 6863 | `			}` |
| 14856102 | 6864 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     4624 | 6865 | `			const char *zTail = 0;` |
|     4624 | 6866 | `			const char *zEnd = (const char *)SyBlobData(&pVal->sBlob) + SyBlobLength(&pVal->sBlob);` |
|     4624 | 6867 | `			if( !PH7_MemObjStringNumericPrefix(pVal,&zTail) ){` |
|        - | 6868 | `				/* Nothing numeric at all ("abc", "") -> TypeError. */` |
|     2135 | 6869 | `				bBad = 1;` |
|     1068 | 6870 | `			}else{` |
|        - | 6871 | `				/* Starts with a number. php only calls it numeric when the WHOLE string` |
|        - | 6872 | `				 * is consumed (bar trailing space); a leftover tail ("5abc", "0x1A") is a` |
|        - | 6873 | `				 * leading-numeric string -- php warns and computes with the prefix. */` |
|     2518 | 6874 | `				while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|       30 | 6875 | `					zTail++;` |
|        2 | 6876 | `				}` |
|     2490 | 6877 | `				if( zTail < zEnd ){` |
|     1187 | 6878 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"A non-numeric value encountered");` |
|      592 | 6879 | `				}` |
|        - | 6880 | `			}` |
|     2310 | 6881 | `		}` |
| 14856727 | 6882 | `		if( bBad ){` |
|     3382 | 6883 | `			if( i == 0 ){ bBadL = 1; } else { bBadR = 1; }` |
|        - | 6884 | `			/* php converts the operands one at a time and STOPS at the first one it` |
|        - | 6885 | `			 * refuses — the second is never looked at, so it never says anything about` |
|        - | 6886 | ``			 * it. `"abc" + "5x"` is the TypeError alone, where walking both operands`` |
|        - | 6887 | ``			 * first announced `A non-numeric value encountered` for the "5x" php never`` |
|        - | 6888 | ``			 * reached. The other order is unaffected: `"5x" + "abc"` warns for the left`` |
|        - | 6889 | `			 * operand and then throws, in both engines. */` |
|     3382 | 6890 | `			break;` |
|        - | 6891 | `		}` |
|  7428115 | 6892 | `	}` |
|  7429314 | 6893 | `	if( bBadL \|\| bBadR ){` |
|        - | 6894 | `		/* Only CLASSIFY here — the caller must settle the operand stack BEFORE the throw,` |
|        - | 6895 | `		 * or the catch runs with the abandoned operands still on it and execution resumes` |
|        - | 6896 | ``		 * inside the try (the catch fired, then `5 + "abc"` carried on and produced 5). */`` |
|     5072 | 6897 | `		SyBlobFormat(pMsgOut,"Unsupported operand types: %s %s %s",` |
|     1690 | 6898 | `			VmArithTypeName(pLeft),zOp,VmArithTypeName(pRight));` |
|     3382 | 6899 | `		return SXERR_INVALID;` |
|        - | 6900 | `	}` |
|  7425934 | 6901 | `	return SXRET_OK;` |
|  3718774 | 6902 | `}` |
|        - | 6903 | `/*` |
|        - | 6904 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - | 6905 | ` * iCode becomes the exception's $code (php's second constructor argument);` |
|        - | 6906 | ` * pass 0 for the engine errors that leave it at its default.` |
|        - | 6907 | ` */` |
|    15245 | 6908 | `static sxi32 VmThrowInternalAp(ph7_context *pCtx,const char *zClass,sxi32 iCode,ph7_class_instance *pPrev,const char *zFormat,va_list ap)` |
|        5 | 6909 | `{` |
|        - | 6910 | `	ph7_vm *pVm;` |
|        - | 6911 | `	ph7_class *pClass;` |
|        - | 6912 | `	ph7_class_instance *pThis;` |
|        - | 6913 | `	ph7_class_method *pCons;` |
|        - | 6914 | `	ph7_value sArg,sCode;` |
|        - | 6915 | `	ph7_value *apArg[2];` |
|        - | 6916 | `	SyBlob sMsg;` |
|        - | 6917 | `	SyString sMsgStr;` |
|        - | 6918 | `	VmFrame *pFrame;` |
|        - | 6919 | `	sxi32 rc;` |
|        - | 6920 |  |
|    15250 | 6921 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 | 6922 | `		return PH7_ABORT;` |
|        - | 6923 | `	}` |
|    15250 | 6924 | `	pVm = pCtx->pVm;` |
|    15250 | 6925 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 | 6926 | `		zClass = "Error";` |
|      ! 0 | 6927 | `	}` |
|        - | 6928 | `	/* The two degraded paths below cannot build the exception object, so they report an` |
|        - | 6929 | `	 * uncaught fatal with a trace instead. They record whatever status that reporting` |
|        - | 6930 | `	 * settled on, so the "nThrowRc mirrors what this function returned" invariant holds` |
|        - | 6931 | `	 * on every exit and a caller that returns PH7_OK anyway still cannot resume past a` |
|        - | 6932 | `	 * reported error (VmHostFuncThrowRc). */` |
|    15250 | 6933 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|    15250 | 6934 | `	if( pClass == 0 ){` |
|      ! 0 | 6935 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - | 6936 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 | 6937 | `			zClass` |
|        - | 6938 | `			);` |
|      ! 0 | 6939 | `		return pCtx->nThrowRc;` |
|        - | 6940 | `	}` |
|    15250 | 6941 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|    15250 | 6942 | `	if( pThis == 0 ){` |
|      ! 0 | 6943 | `		pCtx->nThrowRc = PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - | 6944 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - | 6945 | `			);` |
|      ! 0 | 6946 | `		return pCtx->nThrowRc;` |
|        - | 6947 | `	}` |
|        - | 6948 |  |
|    15250 | 6949 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    15250 | 6950 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - | 6951 |  |
|    15250 | 6952 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|    15250 | 6953 | `	if( pCons ){` |
|    15250 | 6954 | `		int nArg = 1;` |
|    15250 | 6955 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|    15250 | 6956 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|    15250 | 6957 | `		apArg[0] = &sArg;` |
|    15250 | 6958 | `		if( iCode != 0 ){` |
|     1265 | 6959 | `			PH7_MemObjInitFromInt(&(*pVm),&sCode,(sxi64)iCode);` |
|     1265 | 6960 | `			apArg[1] = &sCode;` |
|     1265 | 6961 | `			nArg = 2;` |
|      630 | 6962 | `		}` |
|    15250 | 6963 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,nArg,apArg);` |
|    15250 | 6964 | `		if( iCode != 0 ){` |
|     1265 | 6965 | `			PH7_MemObjRelease(&sCode);` |
|      630 | 6966 | `		}` |
|    15250 | 6967 | `		PH7_MemObjRelease(&sArg);` |
|     7598 | 6968 | `	}` |
|    15250 | 6969 | `	SyBlobRelease(&sMsg);` |
|    15250 | 6970 | `	VmExceptionLinkPrevious(pThis,pPrev);` |
|        - | 6971 |  |
|    15250 | 6972 | `	pFrame = pVm->pFrame;` |
|    15250 | 6973 | `	if( pFrame ){` |
|    15250 | 6974 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|    15250 | 6975 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     7598 | 6976 | `	}` |
|    15250 | 6977 | `	rc = VmThrowException(&(*pVm),pThis);` |
|    15250 | 6978 | `	PH7_ClassInstanceUnref(pThis);` |
|    15250 | 6979 | `	if( rc == SXERR_ABORT ){` |
|      537 | 6980 | `		pCtx->nThrowRc = PH7_ABORT;` |
|      537 | 6981 | `		return PH7_ABORT;` |
|        - | 6982 | `	}` |
|        - | 6983 | `	/* Record the status on the CALL CONTEXT as well. A host function that raises` |
|        - | 6984 | `	 * here and then returns PH7_OK anyway — because the throw sits in a shared` |
|        - | 6985 | `	 * argument-validation helper whose callers have no status channel — would` |
|        - | 6986 | `	 * otherwise let OP_CALL treat the call as a normal return: the catch has` |
|        - | 6987 | `	 * already run in place, so execution would carry on INSIDE the try the throw` |
|        - | 6988 | `	 * abandoned (and, uncaught, past the reported fatal). VmHostFuncThrowRc()` |
|        - | 6989 | `	 * re-reads this at the boundary; a caller that DOES propagate its rc is` |
|        - | 6990 | `	 * unaffected (the escalation only fires on a non-throwing status). Same` |
|        - | 6991 | `	 * rationale as VmBoundaryPark for callback throws, one call-frame narrower. */` |
|    14718 | 6992 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|    14718 | 6993 | `	return PH7_EXCEPTION;` |
|     7603 | 6994 | `}` |
|    13933 | 6995 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        5 | 6996 | `{` |
|        - | 6997 | `	va_list ap;` |
|        - | 6998 | `	sxi32 rc;` |
|    13938 | 6999 | `	va_start(ap,zFormat);` |
|    13938 | 7000 | `	rc = VmThrowInternalAp(pCtx,zClass,0,0,zFormat,ap);` |
|    13938 | 7001 | `	va_end(ap);` |
|    13938 | 7002 | `	return rc;` |
|        5 | 7003 | `}` |
|        - | 7004 | `/* Same, with pPrev (may be 0) as the new exception's $previous. */` |
|       46 | 7005 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionPrev(ph7_context *pCtx,ph7_class_instance *pPrev,const char *zClass,const char *zFormat,...)` |
|        2 | 7006 | `{` |
|        - | 7007 | `	va_list ap;` |
|        - | 7008 | `	sxi32 rc;` |
|       48 | 7009 | `	va_start(ap,zFormat);` |
|       48 | 7010 | `	rc = VmThrowInternalAp(pCtx,zClass,0,pPrev,zFormat,ap);` |
|       48 | 7011 | `	va_end(ap);` |
|       48 | 7012 | `	return rc;` |
|        2 | 7013 | `}` |
|        - | 7014 | `/* Same, carrying php's exception $code (JsonException gets json_last_error()). */` |
|     1266 | 7015 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionCode(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zFormat,...)` |
|        5 | 7016 | `{` |
|        - | 7017 | `	va_list ap;` |
|        - | 7018 | `	sxi32 rc;` |
|     1271 | 7019 | `	va_start(ap,zFormat);` |
|     1271 | 7020 | `	rc = VmThrowInternalAp(pCtx,zClass,iCode,0,zFormat,ap);` |
|     1271 | 7021 | `	va_end(ap);` |
|     1271 | 7022 | `	return rc;` |
|        5 | 7023 | `}` |
|        - | 7024 | `/*` |
|        - | 7025 | ` * The status a host function's own throw should have returned. Consulted at the` |
|        - | 7026 | ` * single OP_CALL host boundary right after the C routine returns: it upgrades a` |
|        - | 7027 | ` * normal-looking status to the one PH7_VmThrowException recorded on the context,` |
|        - | 7028 | ` * and is the identity when the routine never threw or already reported it.` |
|        - | 7029 | ` *` |
|        - | 7030 | ` * PH7_SUSPEND passes through with ABORT/EXCEPTION even though it is not a "throw` |
|        - | 7031 | ` * already reported" status: it is a control transfer the fiber machinery must` |
|        - | 7032 | ` * honour (the CALL's state is saved and the operand stack is left mid-flight), and` |
|        - | 7033 | ` * no path produces both — every Fiber throw returns PH7_EXCEPTION.` |
|        - | 7034 | ` */` |
| 19219210 | 7035 | `PH7_PRIVATE sxi32 VmHostFuncThrowRc(ph7_context *pCtx,sxi32 rc)` |
|        5 | 7036 | `{` |
| 19219210 | 7037 | `	if( pCtx->nThrowRc == 0` |
|  9617422 | 7038 | `	 \|\| rc == PH7_ABORT \|\| rc == PH7_EXCEPTION \|\| rc == PH7_SUSPEND ){` |
| 19214537 | 7039 | `		return rc;` |
|        - | 7040 | `	}` |
|     4683 | 7041 | `	return pCtx->nThrowRc;` |
|  9610813 | 7042 | `}` |
|        - | 7043 | `/*` |
|        - | 7044 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - | 7045 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - | 7046 | ` */` |
|      ! 0 | 7047 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 | 7048 | `{` |
|        - | 7049 | `	ph7_vm *pVm;` |
|        - | 7050 | `	SyBlob sMsg;` |
|      ! 0 | 7051 | `	const char *zFuncName = 0;` |
|      ! 0 | 7052 | `	int nFuncLen = 0;` |
|        - | 7053 | `	va_list ap;` |
|        - | 7054 | `	sxi32 rc;` |
|        - | 7055 |  |
|      ! 0 | 7056 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 | 7057 | `		return PH7_OK;` |
|        - | 7058 | `	}` |
|      ! 0 | 7059 | `	pVm = pCtx->pVm;` |
|      ! 0 | 7060 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 | 7061 | `		zClass = "Error";` |
|      ! 0 | 7062 | `	}` |
|        - | 7063 |  |
|      ! 0 | 7064 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - | 7065 |  |
|      ! 0 | 7066 | `	va_start(ap,zFormat);` |
|      ! 0 | 7067 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 | 7068 | `	va_end(ap);` |
|        - | 7069 |  |
|      ! 0 | 7070 | `	if( pCtx->pFunc ){` |
|      ! 0 | 7071 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 | 7072 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 | 7073 | `	}` |
|      ! 0 | 7074 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 | 7075 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 | 7076 | `	}` |
|      ! 0 | 7077 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 | 7078 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 | 7079 | `	SyBlobRelease(&sMsg);` |
|      ! 0 | 7080 | `	return rc;` |
|      ! 0 | 7081 | `}` |
|        - | 7082 | `/*` |
|        - | 7083 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 7084 | ` * exception is triggered.` |
|        - | 7085 | ` */` |
|      620 | 7086 | `PH7_PRIVATE sxi32 VmUncaughtException(` |
|        - | 7087 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 7088 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 7089 | `	)` |
|        4 | 7090 | `{` |
|        - | 7091 | `	ph7_value *apArg[2],sArg;` |
|      624 | 7092 | `	int nArg = 1;` |
|        - | 7093 | `	sxi32 rc;` |
|      624 | 7094 | `	if( pVm->nMuteThrow > 0 ){` |
|        - | 7095 | `		/* A MUTED initializer (VmEvalDefaultMuted: a class static property's` |
|        - | 7096 | `		 * default at mount) — php has not reached this code, so nothing may be` |
|        - | 7097 | `		 * observable: no exception handler runs, no report is printed and the` |
|        - | 7098 | `		 * exit status stays put. Unwind the mini-program at once; the mount path` |
|        - | 7099 | `		 * rolls the attempt back and re-runs the initializer at first access. */` |
|      ! 0 | 7100 | `		return SXERR_ABORT;` |
|        - | 7101 | `	}` |
|      624 | 7102 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 7103 | `		/* Nesting limit reached */` |
|      ! 0 | 7104 | `		return SXRET_OK;` |
|        - | 7105 | `	}` |
|        - | 7106 | `	/* Call any exception handler if available */` |
|      624 | 7107 | `	PH7_MemObjInit(pVm,&sArg);` |
|      624 | 7108 | `	if( pThis ){` |
|        - | 7109 | `		/* Load the exception instance */` |
|      624 | 7110 | `		sArg.x.pOther = pThis;` |
|      624 | 7111 | `		pThis->iRef++;` |
|      624 | 7112 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      314 | 7113 | `	}else{` |
|      ! 0 | 7114 | `		nArg = 0;` |
|        - | 7115 | `	}` |
|      624 | 7116 | `	apArg[0] = &sArg;` |
|        - | 7117 | `	/* Call the exception handler if available */` |
|      624 | 7118 | `	pVm->nExceptDepth++;` |
|        - | 7119 | `	{` |
|        - | 7120 | `		/* Hidden for the duration of its own call, exactly like the error handler` |
|        - | 7121 | `		 * above: an exception escaping the handler is not handed back to it, and a` |
|        - | 7122 | `		 * set_exception_handler() from inside replaces an EMPTY entry. */` |
|        - | 7123 | `		ph7_value sRunning;` |
|      624 | 7124 | `		PH7_MemObjInit(pVm,&sRunning);` |
|      624 | 7125 | `		PH7_MemObjStore(&pVm->sExceptionCB,&sRunning);` |
|      624 | 7126 | `		PH7_MemObjRelease(&pVm->sExceptionCB);` |
|      624 | 7127 | `		MemObjSetType(&pVm->sExceptionCB,MEMOBJ_NULL);` |
|      624 | 7128 | `		rc = PH7_VmCallUserFunction(&(*pVm),&sRunning,nArg,apArg,0);` |
|      624 | 7129 | `		if( !ph7_value_is_callable(&pVm->sExceptionCB) ){` |
|      624 | 7130 | `			PH7_MemObjStore(&sRunning,&pVm->sExceptionCB);` |
|      310 | 7131 | `		}` |
|      624 | 7132 | `		PH7_MemObjRelease(&sRunning);` |
|        - | 7133 | `	}` |
|      624 | 7134 | `	pVm->nExceptDepth--;` |
|      624 | 7135 | `	if( rc != SXRET_OK ){` |
|        - | 7136 | `		const char *zFuncName;` |
|        - | 7137 | `		int nFuncLen;` |
|      622 | 7138 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|        - | 7139 | `		/* Both report entry points below stamp iExitStatus = 255 themselves. */` |
|      622 | 7140 | `		if( pThis ){` |
|        - | 7141 | `			/* Walk the $previous chain: deepest is "Uncaught", each outer one` |
|        - | 7142 | `			 * "Next ..." (PHP). A non-chained exception is a length-1 chain and` |
|        - | 7143 | `			 * renders byte-identically to the historical single-entry report. */` |
|      622 | 7144 | `			VmReportUncaughtChain(pVm,pThis,zFuncName,nFuncLen);` |
|      313 | 7145 | `		}else{` |
|        - | 7146 | `			/* No instance (internal report path) — default-class single entry. */` |
|      ! 0 | 7147 | `			VmReportUncaughtException(pVm,0,0,0,0,zFuncName,nFuncLen);` |
|        - | 7148 | `		}` |
|        - | 7149 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      622 | 7150 | `		rc = SXERR_ABORT;` |
|      309 | 7151 | `	}` |
|      624 | 7152 | `	PH7_MemObjRelease(&sArg);` |
|      624 | 7153 | `	return rc;` |
|      314 | 7154 | `}` |
|        - | 7155 | `/*` |
|        - | 7156 | ` * Throw a user exception.` |
|        - | 7157 | ` *` |
|        - | 7158 | ` * Exception dispatch follows this sequence:` |
|        - | 7159 | ` *` |
|        - | 7160 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 7161 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 7162 | ` *` |
|        - | 7163 | ` * 2. If NO catch matches:` |
|        - | 7164 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 7165 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 7166 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 7167 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 7168 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 7169 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 7170 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 7171 | ` *` |
|        - | 7172 | ` * 3. If a catch DOES match:` |
|        - | 7173 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 7174 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 7175 | ` *       inside the catch body from immediately propagating past our` |
|        - | 7176 | ` *       finally block.` |
|        - | 7177 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 7178 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 7179 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 7180 | ` *       in pPendingException (step 2c).` |
|        - | 7181 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 7182 | ` *    d. Run finally (if present).` |
|        - | 7183 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 7184 | ` *       that handlers are restored and finally has run.` |
|        - | 7185 | ` */` |
|        - | 7186 | `/*` |
|        - | 7187 | ` * ROOT C: advance a pending return/break through the chain of enclosing INLINE trys.` |
|        - | 7188 | ` * Pops each enclosing try's handler (this function's inline trys, innermost first) off` |
|        - | 7189 | ` * aException; when one has a finally, sets *pPc to its iFinallyPc and returns 1 (the` |
|        - | 7190 | ` * caller re-queues the action and jumps there — OP_END_FINALLY calls back in after the` |
|        - | 7191 | ` * finally runs). A try without a finally is torn down (handler + transparent frame) and` |
|        - | 7192 | ` * the walk continues. Returns 0 when no more of this function's inline trys remain (the` |
|        - | 7193 | ` * action is terminal: materialize the return / take the break jump). A try WITH a finally` |
|        - | 7194 | ` * keeps its transparent frame — OP_END_FINALLY leaves it; a try WITHOUT one is left here.` |
|        - | 7195 | ` * pStop, when non-NULL, bounds the walk (used by break/continue: stop after crossing it).` |
|        - | 7196 | ` */` |
|      346 | 7197 | `PH7_PRIVATE int VmFinallyAdvance(ph7_vm *pVm, VmInstr *aInstr, int *pnCross, sxu32 *pPc)` |
|        5 | 7198 | `{` |
|      373 | 7199 | `	while( *pnCross != 0 && SySetUsed(&pVm->aException) > 0 ){` |
|       97 | 7200 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       97 | 7201 | `		ph7_exception *pT = ap[SySetUsed(&pVm->aException) - 1];` |
|       97 | 7202 | `		if( !pT->iInlined \|\| pT->pOwnerInstr != (void *)aInstr ){` |
|       20 | 7203 | `			break; /* reached an outer exec's / legacy handler */` |
|        - | 7204 | `		}` |
|       63 | 7205 | `		(void)SySetPop(&pVm->aException);` |
|       63 | 7206 | `		if( *pnCross > 0 ){ (*pnCross)--; }   /* bounded (break/continue); -1 stays unbounded */` |
|       63 | 7207 | `		if( pT->iHasFinally ){` |
|       39 | 7208 | `			*pPc = pT->iFinallyPc;` |
|       39 | 7209 | `			VmExcRelease(&(*pVm),pT); /* popped for good; the redirect uses the pc value */` |
|       39 | 7210 | `			return 1;` |
|        - | 7211 | `		}` |
|        - | 7212 | `		/* No finally: tear the try's transparent frame down now. */` |
|       26 | 7213 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|        6 | 7214 | `			VmLeaveFrame(&(*pVm));` |
|        2 | 7215 | `		}` |
|       26 | 7216 | `		VmExcRelease(&(*pVm),pT);` |
|        4 | 7217 | `	}` |
|      315 | 7218 | `	return 0;` |
|      178 | 7219 | `}` |
|        - | 7220 | `/*` |
|        - | 7221 | ` * ROOT C: classify a throw against an INLINE try (generator body) and set up a` |
|        - | 7222 | ` * pc-redirect for the throw site — never runs bytecode itself. pException has been` |
|        - | 7223 | ` * popped off aException by the caller; pCatch is the matching catch block or 0.` |
|        - | 7224 | ` *` |
|        - | 7225 | ` *  - catch matched: re-push the handler marked iInCatch (so a throw inside the catch` |
|        - | 7226 | ` *    still runs this try's finally), hold a ref to the exception for OP_CATCH to bind,` |
|        - | 7227 | ` *    and redirect to the catch body (iHandlerPc).` |
|        - | 7228 | ` *  - no catch but finally: queue a RETHROW action and redirect to the finally` |
|        - | 7229 | ` *    (iFinallyPc); OP_END_FINALLY re-raises after the finally runs.` |
|        - | 7230 | ` *  - no catch, no finally: leave this try's transparent frame and propagate to the` |
|        - | 7231 | ` *    next handler (inline or legacy) by re-entering VmThrowException.` |
|        - | 7232 | ` * The operand-stack drain to iStackDepth happens at the throw site (PH7_INLINE_RESUME_BREAK).` |
|        - | 7233 | ` */` |
|        - | 7234 | `/*` |
|        - | 7235 | ` * BYTECODE stage 2b: run a legacy (detached) finally mini-program in the scope` |
|        - | 7236 | ` * of the try-OWNING body — a transparent VM_FRAME_EXCEPTION wrapper re-parented` |
|        - | 7237 | ` * onto pOwner, exactly the catch body's mechanism (see the re-parent note in` |
|        - | 7238 | ` * VmThrowException's catch path). Before this, a finally reached by a throw` |
|        - | 7239 | ` * from a NESTED call ran against the throw-site frame: it read the wrong` |
|        - | 7240 | `` * function's variables and a `return` inside it parked on the wrong body`` |
|        - | 7241 | ` * (the finally_return_cross_frame twin). Same-frame execution (the common` |
|        - | 7242 | ` * case) is unchanged: no wrapper.` |
|        - | 7243 | ` */` |
|        - | 7244 | `/*` |
|        - | 7245 | ` * php runs a catch body and a finally body in the scope of the function that` |
|        - | 7246 | ` * DECLARED the try. This engine runs them AT THE THROW SITE (the in-place catch),` |
|        - | 7247 | ` * and the throw site can be several calls deeper -- so pVm->aSelf, the` |
|        - | 7248 | `` * late-static-binding stack that `static::`, `new static` and get_called_class()`` |
|        - | 7249 | ` * read, still carries the class of every call the throw left open above the try.` |
|        - | 7250 | ` * A handler entered that way answered the THROWING method's class: for` |
|        - | 7251 | `` * `try { (new T)->boom(); } catch (E $e) { new static(); }` php constructs the`` |
|        - | 7252 | ` * catching class and this engine constructed T.` |
|        - | 7253 | ` *` |
|        - | 7254 | ` * So park that slice for the duration of the handler and put it back after --` |
|        - | 7255 | ` * the same treatment the exception stack beside it already gets. The depth to` |
|        - | 7256 | ` * park back to is the one the try recorded when it opened (nSelfDepth).` |
|        - | 7257 | ` */` |
|        - | 7258 | `typedef struct VmParkedSelf VmParkedSelf;` |
|        - | 7259 | `struct VmParkedSelf` |
|        - | 7260 | `{` |
|        - | 7261 | `	ph7_class **apSaved; /* Entries above the try's depth, or NULL when there are none */` |
|        - | 7262 | `	sxu32 nSaved;        /* How many */` |
|        - | 7263 | `	sxu32 nBase;         /* Depth the handler runs at */` |
|        - | 7264 | `};` |
|  1469035 | 7265 | `static void VmParkSelfForHandler(ph7_vm *pVm,ph7_exception *pException,VmParkedSelf *pPark)` |
|        5 | 7266 | `{` |
|  1469040 | 7267 | `	sxu32 nUsed = SySetUsed(&pVm->aSelf);` |
|  1469040 | 7268 | `	pPark->apSaved = 0;` |
|  1469040 | 7269 | `	pPark->nSaved = 0;` |
|  1469040 | 7270 | `	pPark->nBase = nUsed;` |
|  1469040 | 7271 | `	if( pException == 0 \|\| nUsed <= pException->nSelfDepth ){` |
|        - | 7272 | `		/* The throw never left the try owner's activation (or a coroutine already` |
|        - | 7273 | `		 * parked this slice): the top is the owner's class already. */` |
|   968412 | 7274 | `		return;` |
|        - | 7275 | `	}` |
|   500633 | 7276 | `	pPark->nBase = pException->nSelfDepth;` |
|   500633 | 7277 | `	pPark->nSaved = nUsed - pPark->nBase;` |
|  1001261 | 7278 | `	pPark->apSaved = (ph7_class **)SyMemBackendAlloc(&pVm->sAllocator,` |
|   500628 | 7279 | `		pPark->nSaved * sizeof(ph7_class *));` |
|   500633 | 7280 | `	if( pPark->apSaved == 0 ){` |
|        - | 7281 | `		/* No room to remember them: leave the stack as it is rather than lose it. */` |
|      ! 0 | 7282 | `		pPark->nSaved = 0;` |
|      ! 0 | 7283 | `		pPark->nBase = nUsed;` |
|      ! 0 | 7284 | `		return;` |
|        - | 7285 | `	}` |
|   750947 | 7286 | `	SyMemcpy((const void *)((ph7_class **)SySetBasePtr(&pVm->aSelf) + pPark->nBase),` |
|   500628 | 7287 | `		(void *)pPark->apSaved,pPark->nSaved * sizeof(ph7_class *));` |
|   500633 | 7288 | `	SySetTruncate(&pVm->aSelf,pPark->nBase);` |
|   734495 | 7289 | `}` |
|  1469035 | 7290 | `static void VmUnparkSelfForHandler(ph7_vm *pVm,VmParkedSelf *pPark)` |
|        5 | 7291 | `{` |
|        - | 7292 | `	sxu32 k;` |
|  1469040 | 7293 | `	if( pPark->apSaved == 0 ){` |
|   968412 | 7294 | `		return;` |
|        - | 7295 | `	}` |
|        - | 7296 | `	/* Whatever the handler body pushed and did not pop is its own business and must` |
|        - | 7297 | `	 * not sit under the entries that belong to the still-open calls above. */` |
|   500633 | 7298 | `	if( SySetUsed(&pVm->aSelf) > pPark->nBase ){` |
|      ! 0 | 7299 | `		SySetTruncate(&pVm->aSelf,pPark->nBase);` |
|      ! 0 | 7300 | `	}` |
|  1001303 | 7301 | `	for( k = 0 ; k < pPark->nSaved ; ++k ){` |
|   500675 | 7302 | `		SySetPut(&pVm->aSelf,(const void *)&pPark->apSaved[k]);` |
|   250340 | 7303 | `	}` |
|   500633 | 7304 | `	SyMemBackendFree(&pVm->sAllocator,(void *)pPark->apSaved);` |
|   500633 | 7305 | `	pPark->apSaved = 0;` |
|   734495 | 7306 | `}` |
|    20268 | 7307 | `static sxi32 VmExecFinallyInOwner(ph7_vm *pVm,SySet *pByteCode,VmFrame *pOwner)` |
|        5 | 7308 | `{` |
|    20273 | 7309 | `	VmFrame *pWrap = 0;` |
|        - | 7310 | `	VmFrame *pThrowSite;` |
|        - | 7311 | `	sxi32 rc;` |
|    20273 | 7312 | `	if( pOwner == 0 \|\| pOwner == VmSkipExceptionFrames(pVm->pFrame) ){` |
|    20163 | 7313 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 7314 | `	}` |
|      114 | 7315 | `	if( SXRET_OK != VmEnterFrame(&(*pVm),0,0,&pWrap) ){` |
|        - | 7316 | `		/* OOM: degrade to in-place execution rather than losing the finally. */` |
|      ! 0 | 7317 | `		return VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 7318 | `	}` |
|      114 | 7319 | `	pThrowSite = pWrap->pParent;` |
|      114 | 7320 | `	pWrap->pParent = pOwner;` |
|      114 | 7321 | `	pWrap->iFlags \|= VM_FRAME_EXCEPTION;` |
|      114 | 7322 | `	rc = VmLocalExec(&(*pVm),pByteCode,0,TRUE);` |
|        - | 7323 | `	/* Leave the wrapper (pVm->pFrame becomes pOwner via the re-parent), then` |
|        - | 7324 | `	 * restore the real throw site so the unwind continues normally. Guarded:` |
|        - | 7325 | `	 * a finally that suspends/aborts mid-mini-program can leave the frame` |
|        - | 7326 | `	 * chain unbalanced — never pop somebody else's frame. */` |
|      114 | 7327 | `	if( pVm->pFrame == pWrap ){` |
|      114 | 7328 | `		VmLeaveFrame(&(*pVm));` |
|       55 | 7329 | `	}` |
|      114 | 7330 | `	pVm->pFrame = pThrowSite;` |
|      114 | 7331 | `	return rc;` |
|    10139 | 7332 | `}` |
|        - | 7333 | `/*` |
|        - | 7334 | ` * VmThrowInline -> VmThrowException private protocol: "no catch/finally in` |
|        - | 7335 | ` * this inline try — keep unwinding outward" (the caller loops back to its` |
|        - | 7336 | ` * Rethrow label). Aliased so the generic retry code's other contract (the` |
|        - | 7337 | ` * sxmem xMemError release-and-retry callback) is not confused with this one.` |
|        - | 7338 | ` */` |
|        - | 7339 | `#define VM_THROW_KEEP_UNWINDING SXERR_RETRY` |
|      108 | 7340 | `static sxi32 VmThrowInline(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - | 7341 | `	ph7_exception *pException, ph7_exception_block *pCatch)` |
|        5 | 7342 | `{` |
|      113 | 7343 | `	if( pCatch ){` |
|       93 | 7344 | `		pException->iInCatch = 1;` |
|       93 | 7345 | `		SySetPut(&pVm->aException,(const void *)&pException);` |
|       93 | 7346 | `		if( pThis ){ pThis->iRef++; }` |
|       93 | 7347 | `		pException->pInflight = pThis;` |
|       93 | 7348 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|       93 | 7349 | `		pVm->pInlineFrame = (void *)pException->pFrame;` |
|       93 | 7350 | `		pVm->iInlinePc = pCatch->iHandlerPc;` |
|       93 | 7351 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|       93 | 7352 | `		return SXRET_OK;` |
|        - | 7353 | `	}` |
|       25 | 7354 | `	if( pException->iHasFinally ){` |
|        - | 7355 | `		VmFinallyAction sAct;` |
|       20 | 7356 | `		SyZero(&sAct,sizeof(sAct));` |
|       20 | 7357 | `		sAct.eKind = PH7_FA_RETHROW;` |
|       20 | 7358 | `		if( pThis ){ pThis->iRef++; }` |
|       20 | 7359 | `		sAct.pExc = pThis;` |
|       20 | 7360 | `		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);` |
|       20 | 7361 | `		pVm->pInlineInstr = pException->pOwnerInstr;` |
|       20 | 7362 | `		pVm->pInlineFrame = (void *)pException->pFrame;` |
|       20 | 7363 | `		pVm->iInlinePc = pException->iFinallyPc;` |
|       20 | 7364 | `		pVm->iInlineDrain = pException->iStackDepth;` |
|       20 | 7365 | `		VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|       20 | 7366 | `		return SXRET_OK;` |
|        - | 7367 | `	}` |
|        - | 7368 | `	/* No catch, no finally: drop this try's frame and continue unwinding —` |
|        - | 7369 | `	 * flat native stack instead of mutual recursion. */` |
|        6 | 7370 | `	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|      ! 0 | 7371 | `		VmLeaveFrame(&(*pVm));` |
|      ! 0 | 7372 | `	}` |
|        6 | 7373 | `	VmExcRelease(&(*pVm),pException); /* not re-pushed: activation ends here */` |
|        6 | 7374 | `	return VM_THROW_KEEP_UNWINDING;` |
|       59 | 7375 | `}` |
|        - | 7376 | `/* The exception-stack depth a throw may unwind to: the fence, when one is up. */` |
|  1670965 | 7377 | `static sxu32 VmThrowFloor(ph7_vm *pVm)` |
|        5 | 7378 | `{` |
|  1670970 | 7379 | `	return pVm->nThrowFence > 0 ? pVm->nThrowFence - 1 : 0;` |
|        5 | 7380 | `}` |
|  1469707 | 7381 | `PH7_PRIVATE sxi32 VmThrowException(` |
|        - | 7382 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 7383 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 7384 | `	)` |
|        5 | 7385 | `{` |
|        - | 7386 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 7387 | `	ph7_exception **apException;` |
|   734826 | 7388 | `	ph7_exception *pException;` |
|    50117 | 7389 | `Rethrow:` |
|        - | 7390 | `	/* Unwinding to the next outer handler loops back here instead of the old` |
|        - | 7391 | `	 * self tail-call: a throw from N frames deep runs N finallys at constant` |
|        - | 7392 | `	 * native depth (the ASan deep-tier stress overflowed the C stack on the` |
|        - | 7393 | `	 * recursive form; the dispatch-loop trampoline made PHP depth heap-bound,` |
|        - | 7394 | `	 * so the throw path must be too). */` |
|        - | 7395 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 7396 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 7397 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|  1569946 | 7398 | `	VmCoalesceDisarm(pVm);` |
|        - | 7399 | `	/* Finally-supersede chaining (PHP): when a finally runs for an in-flight` |
|        - | 7400 | `	 * exception (pInflightException) and a NEW exception thrown by that finally` |
|        - | 7401 | `	 * is leaving it — i.e. the exception stack has unwound to/below the depth it` |
|        - | 7402 | `	 * had when the finally started (nInflightExcBase), so no finally-local catch` |
|        - | 7403 | `	 * will handle it — link the in-flight one as its $previous. A finally` |
|        - | 7404 | `	 * exception caught locally (a try/catch inside the finally) sits ABOVE the` |
|        - | 7405 | `	 * base, so it is not chained, matching PHP. VmExceptionLinkPrevious is a no-op` |
|        - | 7406 | `	 * if pThis already carries a previous, so re-entry across propagation is safe. */` |
|  1569941 | 7407 | `	if( pVm->pInflightException && pThis && pThis != pVm->pInflightException` |
|       25 | 7408 | `	 && SySetUsed(&pVm->aException) <= pVm->nInflightExcBase ){` |
|       20 | 7409 | `		VmExceptionLinkPrevious(pThis,pVm->pInflightException);` |
|        8 | 7410 | `	}` |
|        - | 7411 | ``	/* A throw supersedes a pending catch/finally `return` ONLY when it actually`` |
|        - | 7412 | `	 * unwinds past that return's body frame. We do NOT clear anything here: each` |
|        - | 7413 | `	 * body's pending return lives on its own frame and is discarded at that body's` |
|        - | 7414 | `	 * Exception/Abort exit (or its terminal throw-unwind OP_DONE). A throw caught` |
|        - | 7415 | `	 * locally — e.g. an inline try/catch inside a finally — never unwinds the body` |
|        - | 7416 | `	 * that owns the pending return, so it must leave that return intact. */` |
|        - | 7417 | `	/* A fresh throw invalidates any unconsumed in-place-catch resume target (ROOT B):` |
|        - | 7418 | `	 * the previous catch's landing is no longer where control should resume. Cleared` |
|        - | 7419 | `	 * here so nested in-place catches resolve correctly — the OUTERMOST catch to finish` |
|        - | 7420 | `	 * records last (on its SXRET_OK return below) and therefore owns the resume. */` |
|  1569946 | 7421 | `	VmClearResumeTarget(&(*pVm));` |
|        - | 7422 | `	/* Point to the stack of loaded exceptions */` |
|  1569946 | 7423 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|  1569946 | 7424 | `	pException = 0;` |
|  1569946 | 7425 | `	pCatch = 0;` |
|  1569946 | 7426 | `	if( SySetUsed(&pVm->aException) > VmThrowFloor(pVm) ){` |
|        - | 7427 | `		ph7_exception_block *aCatch;` |
|        - | 7428 | `		ph7_class *pClass;` |
|        - | 7429 | `		SyString *aNames;` |
|        - | 7430 | `		sxu32 nNames;` |
|        - | 7431 | `		int matched;` |
|        - | 7432 | `		sxu32 j,k;` |
|        - | 7433 | `		/* Locate the appropriate block to execute */` |
|  1469076 | 7434 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|  1469076 | 7435 | `		(void)SySetPop(&pVm->aException);` |
|  1469076 | 7436 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|        - | 7437 | `		/* ROOT C: a handler re-pushed for the duration of a catch body (iInCatch) does` |
|        - | 7438 | `		 * NOT re-match its own catches — a throw inside the catch runs the finally then` |
|        - | 7439 | `		 * propagates. Skip the class scan so pCatch stays 0. */` |
|  1469106 | 7440 | `		for( j = 0 ; !pException->iInCatch && j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 7441 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|  1448890 | 7442 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|  1448890 | 7443 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|  1448890 | 7444 | `			matched = 0;` |
|  1448946 | 7445 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 7446 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 7447 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 7448 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|  1448916 | 7449 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|  1448916 | 7450 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 7451 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 7452 | `					continue;` |
|        - | 7453 | `				}` |
|  1448916 | 7454 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|  1448860 | 7455 | `					matched = 1;` |
|  1448860 | 7456 | `					break;` |
|        - | 7457 | `				}` |
|       31 | 7458 | `			}` |
|  1448890 | 7459 | `			if( matched ){` |
|        - | 7460 | `				/* Catch block found,break immediately */` |
|  1448860 | 7461 | `				pCatch = &aCatch[j];` |
|  1448860 | 7462 | `				break;` |
|        - | 7463 | `			}` |
|       18 | 7464 | `		}` |
|   734508 | 7465 | `	}` |
|        - | 7466 | `	/* Restore the '@' error-control depth recorded when this try was entered. The throw` |
|        - | 7467 | `	 * unwinds past the ERR_CTRL that would have closed any window opened inside the try,` |
|        - | 7468 | `	 * so without this the suppression leaks and silences every later diagnostic. Done` |
|        - | 7469 | `	 * once here, where the catching try is known, because the handler is entered by two` |
|        - | 7470 | `	 * different routes below (the inline/generator pc-redirect and the legacy path) —` |
|        - | 7471 | `	 * and on a Rethrow the outermost try that actually catches gets the last word. A` |
|        - | 7472 | ``	 * try/catch nested INSIDE an `@` correctly restores a non-zero depth. */`` |
|  1569946 | 7473 | `	if( pException ){` |
|  1469076 | 7474 | `		pVm->nErrSuppress = pException->iErrSuppress;` |
|   734508 | 7475 | `	}` |
|        - | 7476 | `	/* ROOT C: an inline try (generator body) is not executed here — VmThrowException` |
|        - | 7477 | `	 * only classifies the throw and sets a pc-redirect (pInlineInstr/iInlinePc) that the` |
|        - | 7478 | `	 * throw site jumps to, so catch/finally run in the generator's own dispatch loop. */` |
|  1569946 | 7479 | `	if( pException && pException->iInlined ){` |
|      113 | 7480 | `		sxi32 rcInline = VmThrowInline(&(*pVm),pThis,pException,pCatch);` |
|      113 | 7481 | `		if( rcInline == VM_THROW_KEEP_UNWINDING ){` |
|        - | 7482 | `			/* No catch/finally in that inline try: keep unwinding outward. */` |
|        6 | 7483 | `			goto Rethrow;` |
|        - | 7484 | `		}` |
|      109 | 7485 | `		return rcInline;` |
|        - | 7486 | `	}` |
|        - | 7487 | `	/* Execute the cached block if available */` |
|  1569838 | 7488 | `	if( pCatch == 0 ){` |
|        - | 7489 | `		sxi32 rc;` |
|        - | 7490 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|   121071 | 7491 | `		if( pException && pException->iHasFinally ){` |
|    20185 | 7492 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|    20185 | 7493 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|    20185 | 7494 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|        - | 7495 | `			/* Seeded at the declaration: cl cannot prove the helper below writes` |
|        - | 7496 | `			 * every field through the pointer, and /WX turns C4701 into an error. */` |
|    20185 | 7497 | `			VmParkedSelf sParkSelf = {0,0,0};` |
|    20185 | 7498 | `			pException->iFinallyDone = 1;` |
|        - | 7499 | `			/* Mark pThis in-flight (base = current exception-stack depth) so a throw` |
|        - | 7500 | `			 * from the finally that leaves it chains pThis as $previous; restore after. */` |
|    20185 | 7501 | `			pVm->pInflightException = pThis;` |
|    20185 | 7502 | `			pVm->nInflightExcBase = nExcBefore;` |
|        - | 7503 | `			/* Stage 2b: the finally runs in the try-OWNING body's scope (its own` |
|        - | 7504 | ``			 * variables; a `return` parks on the owning body), like the catch. */`` |
|    20185 | 7505 | `			VmParkSelfForHandler(&(*pVm),pException,&sParkSelf);` |
|    20185 | 7506 | `			rc = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pException->pFrame);` |
|    20185 | 7507 | `			VmUnparkSelfForHandler(&(*pVm),&sParkSelf);` |
|    20185 | 7508 | `			pVm->pInflightException = pSaveInflight;` |
|    20185 | 7509 | `			pVm->nInflightExcBase = nSaveBase;` |
|    20185 | 7510 | `			if( rc == SXERR_ABORT ){` |
|        3 | 7511 | `				VmExcRelease(&(*pVm),pException);` |
|    10023 | 7512 | `				return SXERR_ABORT;` |
|        - | 7513 | `			}` |
|        - | 7514 | ``			/* A `return` inside the finally swallows the in-flight exception (PHP`` |
|        - | 7515 | `			 * semantics). The finally stored it on the body frame it returns from` |
|        - | 7516 | `			 * (the try-OWNING body, via the wrapper above); pThis is discarded; the` |
|        - | 7517 | `			 * owner's OP_POP_EXCEPTION landing pad materializes it. Same frame:` |
|        - | 7518 | `			 * clear VM_FRAME_THROW (this body now returns normally, so its caller` |
|        - | 7519 | `			 * takes the value instead of unwinding) and resume in place.` |
|        - | 7520 | `			 * Cross-frame: record the OWNER's landing pad as the resume target —` |
|        - | 7521 | `			 * the same transport an in-place catch uses — and unwind as an` |
|        - | 7522 | `			 * exception; the owner's activation consumes the resume` |
|        - | 7523 | `			 * (VmCallFinish/VmRecordedResume), lands at its OP_POP_EXCEPTION and` |
|        - | 7524 | `			 * its bHasRet tail materializes the return. */` |
|        - | 7525 | `			{` |
|    20183 | 7526 | `				VmFrame *pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    20183 | 7527 | `				VmFrame *pOwnerFrame = pException->pFrame ? pException->pFrame : pThrowFrame;` |
|    20183 | 7528 | `				if( pOwnerFrame->bHasRet ){` |
|    20029 | 7529 | `					if( pOwnerFrame == pThrowFrame ){` |
|    20026 | 7530 | `						pThrowFrame->iFlags &= ~VM_FRAME_THROW;` |
|        - | 7531 | `						/* Record the landing pad like the cross-frame case below.` |
|        - | 7532 | `						 * OP_THROW lands on its compiler-given nJump anyway, but a` |
|        - | 7533 | `						 * VM-RAISED throw site (undefined function, non-callable,` |
|        - | 7534 | `						 * arith TypeError…) has no nJump: without a recorded target` |
|        - | 7535 | `						 * its router unwound as an exception and the Unwind discard` |
|        - | 7536 | ``						 * dropped the parked return — `function f(){ try {`` |
|        - | 7537 | ``						 * nosuchfn(); } finally { return 5; } }` returned null`` |
|        - | 7538 | `						 * where php returns 5. The pad's OP_POP_EXCEPTION tears the` |
|        - | 7539 | `						 * try frame down and its bHasRet tail materializes the` |
|        - | 7540 | `						 * return, same as the in-place-catch landing. */` |
|    30038 | 7541 | `						VmSetResumeTarget(&(*pVm),pOwnerFrame,pException->iLandingPc,` |
|    10012 | 7542 | `							pException->pOwnerInstr,pException->iStackDepth);` |
|    20026 | 7543 | `						VmExcRelease(&(*pVm),pException);` |
|    20026 | 7544 | `						return SXRET_OK;` |
|        - | 7545 | `					}` |
|        4 | 7546 | `					VmSetResumeTarget(&(*pVm),pOwnerFrame,pException->iLandingPc,` |
|        1 | 7547 | `						pException->pOwnerInstr,pException->iStackDepth);` |
|        3 | 7548 | `					VmExcRelease(&(*pVm),pException);` |
|        3 | 7549 | `					return PH7_EXCEPTION;` |
|        - | 7550 | `				}` |
|        - | 7551 | `			}` |
|        - | 7552 | `			/* The finally threw an exception that superseded pThis — it either` |
|        - | 7553 | `			 * escaped (PH7_EXCEPTION) or was caught in place by an outer handler` |
|        - | 7554 | `			 * (which consumed an entry from the exception stack). Either way the` |
|        - | 7555 | `			 * original pThis is discarded; unwind with the finally's exception (the` |
|        - | 7556 | `			 * OP_THROW caller resumes at the catching frame or propagates). */` |
|      157 | 7557 | `			if( rc == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|       17 | 7558 | `				VmExcRelease(&(*pVm),pException);` |
|       17 | 7559 | `				return PH7_EXCEPTION;` |
|        - | 7560 | `			}` |
|       69 | 7561 | `		}` |
|        - | 7562 | `		/* Check if there is an outer exception handler on the stack */` |
|   101029 | 7563 | `		if( SySetUsed(&pVm->aException) > VmThrowFloor(pVm) ){` |
|        - | 7564 | `			/* Re-throw to the outer handler — flat loop (see Rethrow), one` |
|        - | 7565 | `			 * iteration per unwound level instead of one native frame. */` |
|      156 | 7566 | `			VmExcRelease(&(*pVm),pException);` |
|      156 | 7567 | `			goto Rethrow;` |
|        - | 7568 | `		}` |
|   100877 | 7569 | `		if( pVm->nMuteThrow > 0 ){` |
|        - | 7570 | `			/* MUTED evaluation (VmEvalDefaultMuted: a class static property's` |
|        - | 7571 | `			 * default at class mount, which php would not have evaluated yet).` |
|        - | 7572 | `			 * Nothing outside the initializer may observe this throw: no` |
|        - | 7573 | `			 * deferral into pPendingException for an enclosing catch to pick up,` |
|        - | 7574 | `			 * no handler, no report. The tries pushed INSIDE the eval had their` |
|        - | 7575 | `			 * chance above; hand the status back so the mini-program unwinds and` |
|        - | 7576 | `			 * the mount path rolls the whole attempt back. */` |
|       86 | 7577 | `			VmExcRelease(&(*pVm),pException);` |
|       86 | 7578 | `			return SXERR_ABORT;` |
|        - | 7579 | `		}` |
|   100795 | 7580 | `		if( pVm->nThrowFence > 0 && pThis ){` |
|        - | 7581 | `			/* Stopped at the fence: the handlers below it belong to the code that` |
|        - | 7582 | `			 * called the fenced callback, and php leaves this exception pending for` |
|        - | 7583 | `			 * THAT door to wrap rather than letting any of them catch it now. Carry` |
|        - | 7584 | `			 * the instance out (one reference) and unwind the callback's frames. */` |
|       67 | 7585 | `			pThis->iRef++;` |
|       67 | 7586 | `			if( pVm->pFencedExc ){` |
|      ! 0 | 7587 | `				PH7_ClassInstanceUnref(pVm->pFencedExc);` |
|      ! 0 | 7588 | `			}` |
|       67 | 7589 | `			pVm->pFencedExc = pThis;` |
|       67 | 7590 | `			VmExcRelease(&(*pVm),pException);` |
|       67 | 7591 | `			return PH7_EXCEPTION;` |
|        - | 7592 | `		}` |
|        - | 7593 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 7594 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 7595 | `		 * exception instead of reporting it uncaught.` |
|        - | 7596 | `		 */` |
|   100731 | 7597 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 7598 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 7599 | `			 * by looking for a catch frame on the stack.` |
|        - | 7600 | `			 */` |
|   100731 | 7601 | `			VmFrame *pF = pVm->pFrame;` |
|   100731 | 7602 | `			int inCatch = 0;` |
|   101505 | 7603 | `			while( pF ){` |
|   100861 | 7604 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|   100087 | 7605 | `					inCatch = 1;` |
|   100087 | 7606 | `					break;` |
|        - | 7607 | `				}` |
|      779 | 7608 | `				pF = pF->pParent;` |
|        5 | 7609 | `			}` |
|   100731 | 7610 | `			if( inCatch ){` |
|        - | 7611 | `				/* Defer — will be re-thrown after finally runs */` |
|   100087 | 7612 | `				pThis->iRef++;` |
|   100087 | 7613 | `				pVm->pPendingException = pThis;` |
|   100087 | 7614 | `				VmExcRelease(&(*pVm),pException);` |
|   100087 | 7615 | `				return SXRET_OK;` |
|        - | 7616 | `			}` |
|      322 | 7617 | `		}` |
|        - | 7618 | `#ifdef PH7_CORO_STACK` |
|      649 | 7619 | `		if( pVm->pCoroCtx && pThis && pVm->pCoroCtx->pEscaped == 0 ){` |
|        - | 7620 | `			/* Not uncaught -- OUT OF A FIBER. A fiber running on its own native` |
|        - | 7621 | `			 * stack owns its handler stack outright, so "nothing here matches" is` |
|        - | 7622 | `			 * the end of the FIBER, not of the script: php terminates the fiber and` |
|        - | 7623 | `			 * re-raises the exception at the start()/resume() that ran it, where the` |
|        - | 7624 | `			 * resumer's own handlers are. Carry the instance out on the ctx (one` |
|        - | 7625 | `			 * reference, given back by VmFiberRaiseEscaped) and unwind the body with` |
|        - | 7626 | `			 * the status OP_THROW routes as an exception. */` |
|       28 | 7627 | `			pThis->iRef++;` |
|       28 | 7628 | `			pVm->pCoroCtx->pEscaped = pThis;` |
|       28 | 7629 | `			VmExcRelease(&(*pVm),pException);` |
|       28 | 7630 | `			return PH7_EXCEPTION;` |
|        - | 7631 | `		}` |
|        - | 7632 | `#endif` |
|        - | 7633 | `		/* Truly uncaught */` |
|      624 | 7634 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      624 | 7635 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 7636 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 7637 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 7638 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 7639 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 7640 | `			}` |
|      ! 0 | 7641 | `		}` |
|      624 | 7642 | `		VmExcRelease(&(*pVm),pException);` |
|      624 | 7643 | `		return rc;` |
|      ! 0 | 7644 | `	}else{` |
|  1448772 | 7645 | `		VmFrame *pFrame = pVm->pFrame;` |
|  1448772 | 7646 | `		ph7_exception **apSaved = 0;` |
|        - | 7647 | `		sxu32 nSavedCount;` |
|        - | 7648 | `		/* Seeded at the declaration -- see the other one. */` |
|  1448772 | 7649 | `		VmParkedSelf sParkSelf = {0,0,0};` |
|        - | 7650 | `		sxi32 rc;` |
|        - | 7651 | `		/* Snapshot the resume target BEFORE running the catch/finally mini-programs` |
|        - | 7652 | `		 * (which may push/pop nested exceptions): the body frame that owns this` |
|        - | 7653 | `		 * matching try, and its post-try landing pad. Recorded onto the VM only on` |
|        - | 7654 | `		 * the caught (SXRET_OK) fall-through below, so the throwing site resumes at` |
|        - | 7655 | `		 * THIS catching body rather than the lexically-nearest try (ROOT B). */` |
|  1448772 | 7656 | `		VmFrame *pCatchBody = pException->pFrame;` |
|  1448772 | 7657 | `		sxu32 iCatchPc = pException->iLandingPc;` |
|  1448772 | 7658 | `		void *pCatchInstr = pException->pOwnerInstr;` |
|  1448772 | 7659 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|  1448772 | 7660 | `		if( pException->pFrame == pFrame ){` |
|   836264 | 7661 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|   418129 | 7662 | `		}` |
|        - | 7663 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 7664 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 7665 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 7666 | `		 */` |
|  1448772 | 7667 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|  1448772 | 7668 | `		if( nSavedCount > 0 ){` |
|   150515 | 7669 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|    50170 | 7670 | `				nSavedCount * sizeof(ph7_exception *));` |
|   100345 | 7671 | `			if( apSaved ){` |
|   150515 | 7672 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|    50170 | 7673 | `					nSavedCount * sizeof(ph7_exception *));` |
|   100345 | 7674 | `				SySetReset(&pVm->aException);` |
|    50170 | 7675 | `			}` |
|    50170 | 7676 | `		}` |
|        - | 7677 | `		/* Create the catch frame (made transparent below) */` |
|  1448772 | 7678 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|  1448772 | 7679 | `		if( rc == SXRET_OK ){` |
|        - | 7680 | `			ph7_value *pObj;` |
|        - | 7681 | `			/* VmEnterFrame parented this frame to the CURRENT frame — which, for a` |
|        - | 7682 | `			 * throw raised in a nested call, is the deeper throw-site frame, not the` |
|        - | 7683 | `			 * body that declared the try. Re-parent onto the try-owning body` |
|        - | 7684 | `			 * (pCatchBody = pException->pFrame) and remember the throw site to restore` |
|        - | 7685 | `			 * after. This makes the transparent wrapper resolve the catch's variable` |
|        - | 7686 | ``			 * scope AND its `return` target against the body that owns the try, so a`` |
|        - | 7687 | ``			 * `return` parks on that body — matching the recorded resume target. Before`` |
|        - | 7688 | `			 * this, an in-place catch for a deep throw parked its return on the callee's` |
|        - | 7689 | `			 * frame, which the unwind then discarded (ROOT B, face c). */` |
|  1448772 | 7690 | `			VmFrame *pThrowSite = pFrame->pParent;` |
|        - | 7691 | `			/* A NULL pCatchBody (owner invalidated at park — its frame died with` |
|        - | 7692 | `			 * a lossy deep suspend) keeps the natural parent: the catch then runs` |
|        - | 7693 | `			 * against the live current scope rather than a freed frame. */` |
|  1448772 | 7694 | `			if( pCatchBody ){` |
|  1448772 | 7695 | `				pFrame->pParent = pCatchBody;` |
|   724356 | 7696 | `			}` |
|        - | 7697 | `			/* Transparent wrapper: the catch body shares the enclosing variable` |
|        - | 7698 | `			 * scope (PHP semantics). VM_FRAME_EXCEPTION makes VmSkipExceptionFrames` |
|        - | 7699 | `			 * resolve variables — and bind $e — against the real enclosing frame, so` |
|        - | 7700 | `			 * outer locals, $this and a closure held in a variable are all visible` |
|        - | 7701 | `			 * inside the catch (and $e/any var written there persists afterwards).` |
|        - | 7702 | `			 * VM_FRAME_CATCH is kept for the deferred-exception walk. iExceptionJump` |
|        - | 7703 | `			 * stays 0, so the try-frame-only paths (all guarded by iExceptionJump>0)` |
|        - | 7704 | `			 * are unaffected. Must be set BEFORE binding $e below. */` |
|  1448772 | 7705 | `			pFrame->iFlags \|= VM_FRAME_CATCH \| VM_FRAME_EXCEPTION;` |
|        - | 7706 | `			/* sThis empty => PHP 8.0 non-capturing catch: caught, not bound. */` |
|  2173183 | 7707 | `			pObj = (pCatch->sThis.nByte > 0)` |
|  1448765 | 7708 | `				? VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE) : 0;` |
|  1448772 | 7709 | `			if( pObj ){` |
|        - | 7710 | `				/* The catch variable now resolves in the (shared) enclosing frame,` |
|        - | 7711 | `				 * so it may already hold a value from a prior catch or assignment.` |
|        - | 7712 | `				 * Pin the new instance, then release the slot's prior contents` |
|        - | 7713 | `				 * (runs its __destruct / frees the old value) before rebinding —` |
|        - | 7714 | `				 * iRef++ first keeps a re-thrown same exception alive across the` |
|        - | 7715 | `				 * release. Mirrors PH7_MemObjStore's overwrite-then-release. */` |
|  1448768 | 7716 | `				pThis->iRef++;` |
|  1448768 | 7717 | `				PH7_MemObjRelease(pObj);` |
|  1448768 | 7718 | `				pObj->x.pOther = pThis;` |
|  1448768 | 7719 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|   724354 | 7720 | `			}` |
|        - | 7721 | `			/* Execute the catch block, at the try owner's late-static-binding depth. */` |
|  1448772 | 7722 | `			VmParkSelfForHandler(&(*pVm),pException,&sParkSelf);` |
|  1448772 | 7723 | `			rc = VmLocalExec(&(*pVm),pCatch->pByteCode,0,TRUE);` |
|  1448772 | 7724 | `			VmUnparkSelfForHandler(&(*pVm),&sParkSelf);` |
|        - | 7725 | `			/* Leave the frame (sets pVm->pFrame = pCatchBody via the re-parent), then` |
|        - | 7726 | `			 * restore the real throw-site frame so the unwind continues normally.` |
|        - | 7727 | `			 * Guarded like VmExecFinallyInOwner's wrapper teardown: a catch body` |
|        - | 7728 | `			 * that suspends/aborts mid-mini-program can leave the frame chain` |
|        - | 7729 | `			 * unbalanced — never pop somebody else's frame. */` |
|  1448772 | 7730 | `			if( pVm->pFrame == pFrame ){` |
|  1448770 | 7731 | `				VmLeaveFrame(&(*pVm));` |
|   724355 | 7732 | `			}` |
|  1448772 | 7733 | `			pVm->pFrame = pThrowSite;` |
|   724356 | 7734 | `		}` |
|        - | 7735 | `		/* Restore the outer exception handlers */` |
|  1448772 | 7736 | `		if( apSaved ){` |
|        - | 7737 | `			sxu32 k;` |
|        - | 7738 | `			/* Entries pushed during catch execution (nested try blocks inside` |
|        - | 7739 | `			 * the catch body) are normally already consumed; on an abnormal` |
|        - | 7740 | `			 * mini-program exit (e.g. a suspend escaping the catch) they can` |
|        - | 7741 | `			 * linger — release those activations before discarding the set. */` |
|   100345 | 7742 | `			VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|   100345 | 7743 | `			SySetReset(&pVm->aException);` |
|   201339 | 7744 | `			for(k = 0; k < nSavedCount; k++){` |
|   100999 | 7745 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|    50502 | 7746 | `			}` |
|   100345 | 7747 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|    50170 | 7748 | `		}` |
|        - | 7749 | `		/* Execute the finally block after catch */` |
|  1448772 | 7750 | `		if( pException->iHasFinally ){` |
|        - | 7751 | `			sxi32 rcf;` |
|        - | 7752 | `			/* Snapshot, before the finally runs: the body frame this try returns` |
|        - | 7753 | `			 * from, its pending-return write generation (set if the catch above` |
|        - | 7754 | `			 * returned), and the exception-stack depth. After the finally we use` |
|        - | 7755 | `			 * these to decide whether the finally's throw superseded THIS try's` |
|        - | 7756 | `			 * catch-return. */` |
|        - | 7757 | `			/* Stage 2b: anchor on the try-OWNING body (pCatchBody) — for a deep` |
|        - | 7758 | `			 * throw the current frame is the THROW SITE, and both the catch's` |
|        - | 7759 | `			 * return (parked via the re-parented wrapper) and the finally's` |
|        - | 7760 | `			 * supersede decision belong to the owner, not the thrower. */` |
|       93 | 7761 | `			VmFrame *pBody = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|       93 | 7762 | `			sxu32 nGenBefore = pBody->nRetGen;` |
|       93 | 7763 | `			sxu32 nExcBefore = SySetUsed(&pVm->aException);` |
|        - | 7764 | `			/* The exception in flight while this finally runs is the catch body's` |
|        - | 7765 | `			 * re-throw (deferred in pPendingException), if any — NOT the original` |
|        - | 7766 | `			 * pThis, which the catch already handled. A finally throw chains to that` |
|        - | 7767 | ``			 * re-throw (PHP: `catch{throw C}finally{throw B}` => B->previous == C;`` |
|        - | 7768 | `			 * a normally-handled catch leaves nothing in flight => B->previous null). */` |
|       93 | 7769 | `			ph7_class_instance *pSaveInflight = pVm->pInflightException;` |
|       93 | 7770 | `			sxu32 nSaveBase = pVm->nInflightExcBase;` |
|       93 | 7771 | `			pException->iFinallyDone = 1;` |
|       93 | 7772 | `			pVm->pInflightException = pVm->pPendingException;` |
|       93 | 7773 | `			pVm->nInflightExcBase = nExcBefore;` |
|        - | 7774 | `			/* Stage 2b: the finally runs in the owner's scope, like the catch. */` |
|       93 | 7775 | `			VmParkSelfForHandler(&(*pVm),pException,&sParkSelf);` |
|       93 | 7776 | `			rcf = VmExecFinallyInOwner(&(*pVm),&pException->sFinally,pCatchBody);` |
|       93 | 7777 | `			VmUnparkSelfForHandler(&(*pVm),&sParkSelf);` |
|       93 | 7778 | `			pVm->pInflightException = pSaveInflight;` |
|       93 | 7779 | `			pVm->nInflightExcBase = nSaveBase;` |
|       93 | 7780 | `			if( rcf == SXERR_ABORT ){` |
|      ! 0 | 7781 | `				VmExcRelease(&(*pVm),pException);` |
|      ! 0 | 7782 | `				return SXERR_ABORT;` |
|        - | 7783 | `			}` |
|        - | 7784 | `			/* Did the finally throw an exception that escaped THIS try? Two shapes:` |
|        - | 7785 | `			 * it propagated out (rcf == PH7_EXCEPTION), or it was caught in place by` |
|        - | 7786 | `			 * a handler that lived BELOW this try (the exception stack shrank). In` |
|        - | 7787 | `			 * either case that exception supersedes this try's catch-return — but` |
|        - | 7788 | `			 * ONLY if the slot still holds it (nRetGen unchanged). If an outer catch` |
|        - | 7789 | `			 * (same body frame) ran during the finally and OVERWROTE the slot with` |
|        - | 7790 | `			 * its own return, nRetGen advanced and that return must survive. */` |
|       93 | 7791 | `			if( rcf == PH7_EXCEPTION \|\| SySetUsed(&pVm->aException) < nExcBefore ){` |
|       20 | 7792 | `				if( pBody->bHasRet && pBody->nRetGen == nGenBefore ){` |
|       13 | 7793 | `					VmClearFramePending(pBody);` |
|        5 | 7794 | `				}` |
|        - | 7795 | ``				/* Same rule for a `break`/`continue` parked by the catch`` |
|        - | 7796 | `				 * (OP_CATCH_JMP): the escaping exception supersedes the loop exit.` |
|        - | 7797 | `				 * Unconditional — a jump has no nRetGen equivalent, nothing can` |
|        - | 7798 | `				 * legitimately have re-armed it during the finally. */` |
|       20 | 7799 | `				pBody->nCatchJmpPc = 0;` |
|        8 | 7800 | `			}` |
|       93 | 7801 | `			if( rcf == PH7_EXCEPTION ){` |
|        - | 7802 | `				/* The finally's exception propagated past this try; drop any deferred` |
|        - | 7803 | `				 * re-throw and signal the OP_THROW site to unwind THIS function so it` |
|        - | 7804 | `				 * reaches the frame that caught the finally's throw. */` |
|       20 | 7805 | `				if( pVm->pPendingException ){` |
|      ! 0 | 7806 | `					PH7_ClassInstanceUnref(pVm->pPendingException);` |
|      ! 0 | 7807 | `					pVm->pPendingException = 0;` |
|      ! 0 | 7808 | `				}` |
|       20 | 7809 | `				VmExcRelease(&(*pVm),pException);` |
|       20 | 7810 | `				return PH7_EXCEPTION;` |
|        - | 7811 | `			}` |
|       36 | 7812 | `		}` |
|  1448756 | 7813 | `		if( rc == SXERR_ABORT ){` |
|       10 | 7814 | `			VmExcRelease(&(*pVm),pException);` |
|       10 | 7815 | `			return SXERR_ABORT;` |
|        - | 7816 | `		}` |
|        - | 7817 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 7818 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 7819 | `		 * Now that finally has run and handlers are restored, re-throw —` |
|        - | 7820 | ``		 * unless the catch/finally issued a `return` (parked on this body frame,`` |
|        - | 7821 | `		 * the catch frame having been left above), which swallows the in-flight` |
|        - | 7822 | `		 * exception (PHP semantics).` |
|        - | 7823 | `		 */` |
|  1448748 | 7824 | `		if( pVm->pPendingException ){` |
|        - | 7825 | `			/* Stage 2b: the swallow decision reads the OWNER's parked return. */` |
|   100087 | 7826 | `			VmFrame *pOwner = pCatchBody ? pCatchBody : VmSkipExceptionFrames(pVm->pFrame);` |
|   100087 | 7827 | `			if( !pOwner->bHasRet ){` |
|   100083 | 7828 | `				ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        - | 7829 | ``				/* Unlike a `return`, a parked `break`/`continue` does NOT swallow the`` |
|        - | 7830 | `				 * re-throw: control unwinds past the loop, so drop the loop exit rather` |
|        - | 7831 | `				 * than leave it armed for an unrelated later landing. */` |
|   100083 | 7832 | `				pOwner->nCatchJmpPc = 0;` |
|   100083 | 7833 | `				pVm->pPendingException = 0;` |
|   100083 | 7834 | `				VmExcRelease(&(*pVm),pException);` |
|        - | 7835 | `				/* Continue unwinding with the re-thrown exception (flat loop) */` |
|   100083 | 7836 | `				pThis = pReThrow;` |
|   100083 | 7837 | `				goto Rethrow;` |
|        - | 7838 | `			}` |
|        - | 7839 | `			/* Swallowed by the catch/finally's return: drop the deferred exception. */` |
|        6 | 7840 | `			PH7_ClassInstanceUnref(pVm->pPendingException);` |
|        6 | 7841 | `			pVm->pPendingException = 0;` |
|        2 | 7842 | `		}` |
|        - | 7843 | `		/* The catch (and finally) ran in place and control did NOT unwind past this` |
|        - | 7844 | `		 * try (no PH7_EXCEPTION/ABORT return above). Record the resume target so the` |
|        - | 7845 | `		 * throwing site — which may be several frames below — resumes at this catching` |
|        - | 7846 | `		 * body's landing pad. Consumed one-shot by VmRecordedResume at the resume site. */` |
|  1348670 | 7847 | `		VmSetResumeTarget(&(*pVm),pCatchBody,iCatchPc,pCatchInstr,pException->iStackDepth);` |
|        - | 7848 | `		/* (TICKET 1433-60 is retired by stage 2b: this frees the per-entry` |
|        - | 7849 | ``		 * ACTIVATION; a `goto` re-entering the try mints a fresh one at`` |
|        - | 7850 | `		 * OP_LOAD_EXCEPTION. The compiled object is untouched.) */` |
|  1348670 | 7851 | `		VmExcRelease(&(*pVm),pException);` |
|        - | 7852 | `	}` |
|  1348670 | 7853 | `	return SXRET_OK;` |
|   734831 | 7854 | `}` |
|        - | 7855 |  |
