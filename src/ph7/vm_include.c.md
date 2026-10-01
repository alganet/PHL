# src/ph7/vm_include.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 669/761 lines (87.91%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `#include <stdlib.h> /* realpath, free */` |
|        - |    8 | `/*` |
|        - |    9 | ` * Section:` |
|        - |   10 | ` *    Dynamic code loading: VmEvalChunk, eval(), the include path` |
|        - |   11 | ` *    machinery, VmExecIncludedFile and the include/require family, the` |
|        - |   12 | ` *    spl_autoload group and the __call/__callStatic packing body.` |
|        - |   13 | ` *    Registration rows stay in vm.c's aVmFunc[].` |
|        - |   14 | ` * Status:` |
|        - |   15 | ` *    Stable.` |
|        - |   16 | ` */` |
|        - |   17 | `/*` |
|        - |   18 | ` * php prints a compile-time FATAL where it is raised, and it is not catchable.` |
|        - |   19 | ` * eval() compiles with the generator's logging OFF -- its PARSE errors are a` |
|        - |   20 | ` * ParseError the caller may catch, and printing them there would double the` |
|        - |   21 | ` * diagnostic -- so a refusal of E_ERROR severity has to reach the screen from` |
|        - |   22 | ` * here instead. Formatted exactly as PH7_GenCompileError would have: the bare` |
|        - |   23 | ` * text, then the file and line of the offence.` |
|        - |   24 | ` */` |
|        2 |   25 | `static void VmReportCompileFatal(ph7_vm *pVm,SyBlob *pMsg,sxu32 nLine)` |
|        1 |   26 | `{` |
|        - |   27 | `	SyBlob sOut;` |
|        3 |   28 | `	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        3 |   29 | `	if( pVm->pEngine->xConf.xErr == 0 \|\| SyBlobLength(pMsg) < 1 ){` |
|      ! 0 |   30 | `		return;` |
|        - |   31 | `	}` |
|        3 |   32 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        3 |   33 | `	SyBlobAppend(&sOut,"PHP Fatal error:  ",sizeof("PHP Fatal error:  ")-1);` |
|        3 |   34 | `	SyBlobAppend(&sOut,SyBlobData(pMsg),SyBlobLength(pMsg));` |
|        3 |   35 | `	if( pFile ){` |
|        3 |   36 | `		SyBlobFormat(&sOut," in %.*s on line %u",(int)pFile->nByte,pFile->zString,nLine);` |
|        1 |   37 | `	}` |
|        3 |   38 | `	PH7_GenAppendFatalTrace(&(*pVm),&sOut,PH7_FATAL_TRACE_COMPILE);` |
|        3 |   39 | `	SyBlobAppend(&sOut,"\n",sizeof(char));` |
|        3 |   40 | `	pVm->pEngine->xConf.xErr(SyBlobData(&sOut),SyBlobLength(&sOut),pVm->pEngine->xConf.pErrData);` |
|        3 |   41 | `	SyBlobRelease(&sOut);` |
|        2 |   42 | `}` |
|        - |   43 | `/*` |
|        - |   44 | ` * Record an ACTIVE include/require/eval so a backtrace can show it: php gives each` |
|        - |   45 | ` * one a frame of its own between the loaded unit's frames and the caller's. Nothing` |
|        - |   46 | ` * else in this engine records it -- an include shares its caller's variable scope,` |
|        - |   47 | ` * so it pushes no VmFrame.` |
|        - |   48 | ` *` |
|        - |   49 | ` * pPath is the unit being loaded (php's single argument for the frame) and is empty` |
|        - |   50 | ` * for eval(), which php shows argument-less. The call SITE is read here rather than` |
|        - |   51 | ` * later for the same reason a frame's is (see VmEnterFrame): the include stack moves` |
|        - |   52 | `` * on, and a `try` block's frame carries no function to read a file from.`` |
|        - |   53 | ` */` |
|    21312 |   54 | `PH7_PRIVATE void PH7_VmIncFramePush(ph7_vm *pVm,const char *zName,SyString *pPath)` |
|        5 |   55 | `{` |
|        - |   56 | `	VmIncFrame sInc;` |
|    21317 |   57 | `	VmFrame *pCaller = pVm->pFrame;` |
|        - |   58 | `	SyString *pFile;` |
|    21317 |   59 | `	SyZero(&sInc,sizeof(sInc));` |
|    51335 |   60 | `	while( pCaller && pCaller->pParent` |
|    40684 |   61 | `	    && (pCaller->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|     9727 |   62 | `		pCaller = pCaller->pParent;` |
|        5 |   63 | `	}` |
|    21317 |   64 | `	sInc.pFrame = (void *)pCaller;` |
|    21317 |   65 | `	sInc.nLine = pVm->nCurLine;` |
|    21317 |   66 | `	sInc.zName = zName;` |
|        - |   67 | `	/* The unit this construct is LOADING is already on the include stack by the` |
|        - |   68 | `	 * time we get here, so it must not be mistaken for the one the construct is` |
|        - |   69 | `	 * WRITTEN in: pop it for the question and put it back. */` |
|    21317 |   70 | `	if( pPath ){` |
|    11343 |   71 | `		SyString sTop = *(SyString *)SySetPeek(&pVm->aFiles);` |
|    11343 |   72 | `		(void)SySetPop(&pVm->aFiles);` |
|    11343 |   73 | `		pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|    11343 |   74 | `		if( pFile ){` |
|    11343 |   75 | `			sInc.sFile = *pFile;` |
|     5669 |   76 | `		}` |
|    11343 |   77 | `		SySetPut(&pVm->aFiles,(const void *)&sTop);` |
|     5674 |   78 | `	}else{` |
|     9979 |   79 | `		pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|     9979 |   80 | `		if( pFile ){` |
|     9979 |   81 | `			sInc.sFile = *pFile;` |
|     4987 |   82 | `		}` |
|        - |   83 | `	}` |
|    21317 |   84 | `	if( pPath ){` |
|    11343 |   85 | `		sInc.sPath = *pPath;` |
|     5669 |   86 | `	}` |
|    21317 |   87 | `	SySetPut(&pVm->aIncFrame,(const void *)&sInc);` |
|    21317 |   88 | `}` |
|    21312 |   89 | `PH7_PRIVATE void PH7_VmIncFramePop(ph7_vm *pVm)` |
|        5 |   90 | `{` |
|    21317 |   91 | `	(void)SySetPop(&pVm->aIncFrame);` |
|    21317 |   92 | `}` |
|        - |   93 | `/*` |
|        - |   94 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |   95 | ` * Refer to the eval() language construct implementation for more` |
|        - |   96 | ` * information.` |
|        - |   97 | ` */` |
|    28161 |   98 | `PH7_PRIVATE sxi32 VmEvalChunk(` |
|        - |   99 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  100 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  101 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  102 | `	int iFlags,         /* Compile flag */` |
|        - |  103 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  104 | `	)` |
|        5 |  105 | `{` |
|        - |  106 | `	SySet *pByteCode,aByteCode;` |
|    28166 |  107 | `	ProcConsumer xErr = 0;` |
|    28166 |  108 | `	void *pErrData = 0;` |
|        - |  109 | `	ph7_gen_state sSavedGen;` |
|        - |  110 | `	int bNested;` |
|    28166 |  111 | `	sxi32 rcThrow = SXRET_OK; /* a ParseError raised for a failed compile, or a throw the` |
|        - |  112 | `	                           * evaluated chunk raised — either way the caller must unwind */` |
|        - |  113 | `	/* Initialize bytecode container */` |
|    28166 |  114 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    28166 |  115 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  116 | `	/* Reset the code generator */` |
|    28166 |  117 | `	if( bTrueReturn ){` |
|        - |  118 | `		/* Included file,log compile-time errors */` |
|    11471 |  119 | `		xErr = pVm->pEngine->xConf.xErr;` |
|    11471 |  120 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     5733 |  121 | `	}` |
|        - |  122 | `	/* A non-zero cursor means an OUTER compile is in flight — this eval/include` |
|        - |  123 | `	 * was reached from inside it (an autoload fired while resolving a base class).` |
|        - |  124 | `	 * Wiping the generator would corrupt that outer parse, so snapshot it and hand` |
|        - |  125 | `	 * the nested unit a fresh one; a plain runtime eval/include just resets. */` |
|    28166 |  126 | `	bNested = (pVm->sCodeGen.pIn != 0);` |
|    28166 |  127 | `	if( bNested ){` |
|        5 |  128 | `		PH7_CompilerSaveState(pVm,&sSavedGen,xErr,pErrData);` |
|        3 |  129 | `	}else{` |
|    28162 |  130 | `		PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  131 | `	}` |
|        - |  132 | `	/* An include/require's PARSE error is php's catchable ParseError, thrown from` |
|        - |  133 | `	 * the include and printed only if it goes uncaught -- so the generator must not` |
|        - |  134 | `	 * print it. A refusal of E_ERROR severity is php's uncatchable compile fatal and` |
|        - |  135 | `	 * still prints where it is raised. */` |
|    28166 |  136 | `	pVm->sCodeGen.bParseThrows = (bTrueReturn && pCtx) ? 1 : 0;` |
|        - |  137 | `	/* Swap bytecode container */` |
|    28166 |  138 | `	pByteCode = pVm->pByteContainer;` |
|    28166 |  139 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  140 | `	/* Compile the chunk */` |
|    28166 |  141 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|        - |  142 | `	/* Record THIS unit's error count where the nested state restore below cannot` |
|        - |  143 | `	 * wipe it — VmExecDeferredClass reads it to detect a failed re-compile. */` |
|    28166 |  144 | `	pVm->nLastEvalErr = pVm->sCodeGen.nErr;` |
|    42241 |  145 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  146 | `		/* Compilation error. php makes this a CATCHABLE ParseError -- for eval()` |
|        - |  147 | `		 * and for include/require alike -- where PHL merely returned false, so` |
|        - |  148 | ``		 * `eval('bad syntax')` silently produced a value and a broken include`` |
|        - |  149 | `		 * printed its diagnostic and then CARRIED ON, leaving a half-compiled unit` |
|        - |  150 | `		 * behind for later code to trip over. A refusal of E_ERROR severity is` |
|        - |  151 | `		 * php's uncatchable compile fatal instead: it has already printed itself,` |
|        - |  152 | `		 * and the program stops here. */` |
|      112 |  153 | `		SyBlob *pErr = &pVm->sCodeGen.sErrBuf;` |
|      112 |  154 | `		sxu32 nErrLine = pVm->sCodeGen.nFirstErrLine;` |
|      112 |  155 | `		if( SyBlobLength(&pVm->sCodeGen.sFirstErr) > 0 ){` |
|      112 |  156 | `			pErr = &pVm->sCodeGen.sFirstErr;` |
|       54 |  157 | `		}` |
|      112 |  158 | `		if( pVm->sCodeGen.nFatal > 0 ){` |
|       12 |  159 | `			if( pCtx ){` |
|       12 |  160 | `				ph7_result_bool(pCtx,0);` |
|        5 |  161 | `			}` |
|       12 |  162 | `			if( !bTrueReturn ){` |
|        - |  163 | `				/* eval(): the generator had no consumer, so say it here. */` |
|        3 |  164 | `				VmReportCompileFatal(pVm,&pVm->sCodeGen.sFirstErr,nErrLine);` |
|        1 |  165 | `			}` |
|        - |  166 | `			/* php exits 255 and runs nothing else. The include builtins cascade a` |
|        - |  167 | `			 * requested halt exactly as they do for an exit() inside the file. */` |
|       12 |  168 | `			pVm->iExitStatus = 255;` |
|       12 |  169 | `			pVm->bHaltRequested = 1;` |
|       12 |  170 | `			rcThrow = PH7_ABORT;` |
|      107 |  171 | `		}else if( pCtx ){` |
|        - |  172 | `			/* php's ParseError names the OFFENDING file and line, not the line the` |
|        - |  173 | `			 * include was written on -- the throw is stamped from nCurLine, so aim` |
|        - |  174 | `			 * it at the refusal and put the caller's line back afterwards. */` |
|      102 |  175 | `			sxu32 nSaveLine = pVm->nCurLine;` |
|      102 |  176 | `			ph7_result_bool(pCtx,0);` |
|      102 |  177 | `			if( nErrLine > 0 ){` |
|      102 |  178 | `				pVm->nCurLine = nErrLine;` |
|       49 |  179 | `			}` |
|      102 |  180 | `			if( SyBlobLength(pErr) > 0 ){` |
|      151 |  181 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","%.*s",` |
|       98 |  182 | `					(int)SyBlobLength(pErr),(const char *)SyBlobData(pErr));` |
|       53 |  183 | `			}else{` |
|      ! 0 |  184 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","syntax error");` |
|        - |  185 | `			}` |
|      102 |  186 | `			pVm->nCurLine = nSaveLine;` |
|       49 |  187 | `		}` |
|       58 |  188 | `	}else{` |
|        - |  189 | `		/* Mount any newly defined classes. Skipped while the VM is still` |
|        - |  190 | `		 * initializing (builtin chunks): mounting evaluates class-constant/` |
|        - |  191 | `		 * static initializers and installs reference-table entries, and the` |
|        - |  192 | `		 * runtime structures those need (apRefObj, the per-exec object pool` |
|        - |  193 | `		 * baseline) do not exist before PH7_VmMakeReady — which mounts every` |
|        - |  194 | `		 * class unconditionally anyway. */` |
|        - |  195 | `		SyHashEntry *pEntry;` |
|        - |  196 | `		ph7_class *pClass;` |
|        - |  197 | `		ph7_value sResult; /* Return value */` |
|        - |  198 | `		sxi32 rc;` |
|    28058 |  199 | `		if( pVm->nMagic != PH7_VM_INIT ){` |
|    21337 |  200 | `			SyHashResetLoopCursor(&pVm->hClass);` |
| 11943857 |  201 | `			while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
| 11911861 |  202 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  203 | `				/* Only mount classes that haven't been mounted yet */` |
| 11911861 |  204 | `				if( !pClass->bMounted ){` |
|   964589 |  205 | `					rc = VmMountUserClass(pVm,pClass);` |
|   964589 |  206 | `					if( rc != SXRET_OK ){` |
|        - |  207 | `						/* Mount failure (likely memory error) */` |
|        3 |  208 | `						if( pCtx ){` |
|        3 |  209 | `							ph7_result_bool(pCtx,0);` |
|        1 |  210 | `						}` |
|        3 |  211 | `						goto Cleanup;` |
|        - |  212 | `					}` |
|   482291 |  213 | `				}` |
|        5 |  214 | `			}` |
|    10665 |  215 | `		}` |
|    28056 |  216 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  217 | `			/* Out of memory */` |
|      ! 0 |  218 | `			if( pCtx ){` |
|      ! 0 |  219 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  220 | `			}` |
|      ! 0 |  221 | `			goto Cleanup;` |
|        - |  222 | `		}` |
|    28056 |  223 | `		if( bTrueReturn ){` |
|        - |  224 | `			/* php's include/require answer INT 1 when the file returned nothing` |
|        - |  225 | ``			 * of its own — not `true`. It is the value a script tests, stores`` |
|        - |  226 | ``			 * and compares, and `include $f === true` was false on php and true`` |
|        - |  227 | `			 * here. */` |
|    11457 |  228 | `			PH7_MemObjInitFromInt(pVm,&sResult,1);` |
|     5731 |  229 | `		}else{` |
|        - |  230 | `			/* Assume a null return value */` |
|    16604 |  231 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  232 | `		}` |
|        - |  233 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here` |
|        - |  234 | `		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by` |
|        - |  235 | `		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the` |
|        - |  236 | `		 * native-nesting fatal instead of overflowing the C stack. The PHP` |
|        - |  237 | `		 * call-depth cap is OP_CALL-only (BYTECODE.md stage 5).` |
|        - |  238 | `		 *` |
|        - |  239 | `		 * The nested exec shares the caller's VM frame, so a throw the CALLER's own` |
|        - |  240 | `		 * try catches runs that catch IN PLACE and comes back as a status — which was` |
|        - |  241 | `		 * dropped here, and the statement php abandons then carried on after the catch` |
|        - |  242 | `` 		 * had already run (`try { $r = eval('throw new E;'); echo "x"; } catch …` `` |
|        - |  243 | `		 * printed the catch AND the echo). Same rule and same guard as the match-arm /` |
|        - |  244 | `		 * switch-case / property-default sites: compare the recorded-resume fields` |
|        - |  245 | `		 * against a pre-exec SNAPSHOT, never against 0. */` |
|        - |  246 | `		{` |
|    28056 |  247 | `			const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|    28056 |  248 | `			const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|    28056 |  249 | `			rc = VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    28056 |  250 | `			if( pCtx ){` |
|        - |  251 | `				/* Set the execution result */` |
|    21207 |  252 | `				ph7_result_value(pCtx,&sResult);` |
|    10601 |  253 | `			}` |
|    28056 |  254 | `			PH7_MemObjRelease(&sResult);` |
|    28056 |  255 | `			if( rc != PH7_ABORT && VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){` |
|     3600 |  256 | `				rcThrow = PH7_EXCEPTION;` |
|     1799 |  257 | `			}` |
|        - |  258 | `		}` |
|        - |  259 | `	}` |
|    14085 |  260 | `Cleanup:` |
|        - |  261 | `	/* Cleanup the mess left behind */` |
|    28166 |  262 | `	pVm->pByteContainer = pByteCode;` |
|        - |  263 | `	/* Hand back the call-site cache records this chunk's OP_CALLs claimed, BEFORE the` |
|        - |  264 | `	 * instructions holding their indices disappear. */` |
|    28166 |  265 | `	PH7_VmCallSiteReleaseChunk(pVm,&aByteCode);` |
|    28166 |  266 | `	SySetRelease(&aByteCode);` |
|        - |  267 | `	/* Restore the outer compile's generator state if this was a nested unit. */` |
|    28166 |  268 | `	if( bNested ){` |
|        5 |  269 | `		PH7_CompilerRestoreState(pVm,&sSavedGen);` |
|        2 |  270 | `	}` |
|        - |  271 | `	/* A ParseError raised above must reach the caller so the VM unwinds the rest` |
|        - |  272 | ``	 * of the statement; returning OK left `eval('bad'); echo 'x';` running the`` |
|        - |  273 | `	 * echo even though php had already thrown. */` |
|    28166 |  274 | `	return rcThrow;` |
|        5 |  275 | `}` |
|        - |  276 | `/*` |
|        - |  277 | ` * Execute a deferred class declaration (OP_CLASS_DEFER — see the deferral` |
|        - |  278 | ` * block comment in compile_class.c). At this point the declaration's` |
|        - |  279 | ` * execution order has been honored: any spl_autoload_register() statement` |
|        - |  280 | ` * that precedes it in the program has RUN, so each recorded dependency either` |
|        - |  281 | ` * resolves (possibly by autoload, fired inside PH7_VmExtractClass) or is` |
|        - |  282 | ` * genuinely missing. A missing one is reported through *ppMissing — the` |
|        - |  283 | ` * OP_CLASS_DEFER dispatcher throws php's catchable` |
|        - |  284 | `` * `Class/Interface/Trait "X" not found` Error. Once the dependencies resolve,`` |
|        - |  285 | ` * the captured chunk re-compiles through VmEvalChunk (which also mounts the` |
|        - |  286 | ` * newly installed classes); a compile failure there (e.g. a body syntax error` |
|        - |  287 | ` * whose diagnosis was deferred along with the declaration) has already been` |
|        - |  288 | ` * reported through the engine's error consumer, so it aborts execution.` |
|        - |  289 | ` * The site is idempotent: bDone short-circuits re-execution (a loop around an` |
|        - |  290 | ` * anonymous class instantiates the same installed class, php's` |
|        - |  291 | ` * one-class-per-site semantics).` |
|        - |  292 | ` */` |
|      142 |  293 | `PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing)` |
|        5 |  294 | `{` |
|        - |  295 | `	VmDeferredReq *aReq;` |
|        - |  296 | `	sxu32 n;` |
|      147 |  297 | `	*ppMissing = 0;` |
|      147 |  298 | `	if( pDefer->bDone ){` |
|        5 |  299 | `		return SXRET_OK;` |
|        - |  300 | `	}` |
|      143 |  301 | `	aReq = (VmDeferredReq *)SySetBasePtr(&pDefer->aRequired);` |
|      189 |  302 | `	for( n = 0 ; n < SySetUsed(&pDefer->aRequired) ; ++n ){` |
|       61 |  303 | `		if( PH7_VmExtractClass(&(*pVm),aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|       12 |  304 | `			*ppMissing = &aReq[n];` |
|       12 |  305 | `			return SXRET_OK; /* caller throws */` |
|        - |  306 | `		}` |
|       28 |  307 | `	}` |
|      133 |  308 | `	if( pDefer->sAnonName.nByte > 0 ){` |
|        8 |  309 | `		pVm->sDeferAnonName = pDefer->sAnonName;` |
|        3 |  310 | `	}` |
|      133 |  311 | `	VmEvalChunk(&(*pVm),0,&pDefer->sText,PH7_PHP_ONLY,TRUE);` |
|      133 |  312 | `	pVm->sDeferAnonName.zString = 0;` |
|      133 |  313 | `	pVm->sDeferAnonName.nByte = 0;` |
|      128 |  314 | `	if( pVm->nLastEvalErr > 0` |
|      133 |  315 | `	 \|\| PH7_VmExtractClass(&(*pVm),pDefer->sSelfName.zString,pDefer->sSelfName.nByte,FALSE,0) == 0 ){` |
|        - |  316 | `		/* The re-compile failed — a deferred-along syntax error or a` |
|        - |  317 | `		 * redeclaration fatal. It was already reported through the engine's` |
|        - |  318 | `		 * error consumer; halt like a fatal (php exits 255 for both). */` |
|      ! 0 |  319 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  320 | `		return SXERR_ABORT;` |
|        - |  321 | `	}` |
|      133 |  322 | `	pDefer->bDone = 1;` |
|      133 |  323 | `	return SXRET_OK;` |
|       76 |  324 | `}` |
|        - |  325 | `/*` |
|        - |  326 | ` * php names an eval()'d compilation unit after the SITE that evaluated it:` |
|        - |  327 | `` * `<file>(<line>) : eval()'d code`, where <file> is the unit the eval() was`` |
|        - |  328 | ` * written in -- itself such a name when the eval is nested -- and <line> the line` |
|        - |  329 | ` * it sits on. That name is not a decoration on one message: it is the unit's` |
|        - |  330 | `` * identity, so `__FILE__`, every diagnostic's location, a Throwable's getFile(),`` |
|        - |  331 | ` * and ReflectionFunction/ReflectionClass::getFileName() for anything the chunk` |
|        - |  332 | ` * declares all read it. PHL left the chunk sharing its caller's name, so a` |
|        - |  333 | ` * template engine that compiles to PHP and evals it (twig, and every cache-less` |
|        - |  334 | ` * renderer of that shape) reported positions in the COMPILER's file -- and the` |
|        - |  335 | ` * caller could not map them back, because its own class had the compiler's name` |
|        - |  336 | ` * on it too.` |
|        - |  337 | ` *` |
|        - |  338 | ` * Interned per site rather than per call: the name is copied by value into every` |
|        - |  339 | ` * function and class record compiled out of the chunk, so it must outlive the` |
|        - |  340 | ` * eval, and an eval inside a loop repeats one site.` |
|        - |  341 | ` */` |
|     9974 |  342 | `static sxi32 VmEvalUnitName(ph7_vm *pVm,SyString *pOut)` |
|        5 |  343 | `{` |
|     9979 |  344 | `	SyString *pHost = PH7_VmExecutingUnitFile(&(*pVm));` |
|        - |  345 | `	SyString *aName;` |
|        - |  346 | `	SyString sKey;` |
|        - |  347 | `	SyBlob sName;` |
|        - |  348 | `	char *zDup;` |
|        - |  349 | `	sxu32 n;` |
|     9979 |  350 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|     9979 |  351 | `	if( pHost && pHost->nByte > 0 ){` |
|     9979 |  352 | `		SyBlobAppend(&sName,pHost->zString,pHost->nByte);` |
|     4987 |  353 | `	}` |
|     9979 |  354 | `	SyBlobFormat(&sName,"(%u) : eval()'d code",pVm->nCurLine);` |
|     9979 |  355 | `	if( SyBlobLength(&sName) < 1 ){` |
|      ! 0 |  356 | `		SyBlobRelease(&sName);` |
|      ! 0 |  357 | `		return SXERR_MEM;` |
|        - |  358 | `	}` |
|     9979 |  359 | `	SyStringInitFromBuf(&sKey,SyBlobData(&sName),SyBlobLength(&sName));` |
|     9979 |  360 | `	aName = (SyString *)SySetBasePtr(&pVm->aEvalFile);` |
|   179495 |  361 | `	for( n = 0 ; n < SySetUsed(&pVm->aEvalFile) ; ++n ){` |
|   179339 |  362 | `		if( SyStringCmp(&sKey,&aName[n],SyMemcmp) == 0 ){` |
|     9823 |  363 | `			*pOut = aName[n];` |
|     9823 |  364 | `			SyBlobRelease(&sName);` |
|     9823 |  365 | `			return SXRET_OK;` |
|        - |  366 | `		}` |
|    84761 |  367 | `	}` |
|      159 |  368 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,sKey.zString,sKey.nByte);` |
|      159 |  369 | `	n = sKey.nByte;` |
|      159 |  370 | `	SyBlobRelease(&sName);` |
|      159 |  371 | `	if( zDup == 0 ){` |
|      ! 0 |  372 | `		return SXERR_MEM;` |
|        - |  373 | `	}` |
|      159 |  374 | `	SyStringInitFromBuf(&sKey,zDup,n);` |
|      159 |  375 | `	if( SySetPut(&pVm->aEvalFile,(const void *)&sKey) != SXRET_OK ){` |
|      ! 0 |  376 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  377 | `		return SXERR_MEM;` |
|        - |  378 | `	}` |
|      159 |  379 | `	*pOut = sKey;` |
|      159 |  380 | `	return SXRET_OK;` |
|     4992 |  381 | `}` |
|        - |  382 | `/*` |
|        - |  383 | ` * value eval(string $code)` |
|        - |  384 | ` *   Evaluate a string as PHP code.` |
|        - |  385 | ` * Parameter` |
|        - |  386 | ` *  code: PHP code to evaluate.` |
|        - |  387 | ` * Return` |
|        - |  388 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  389 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  390 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  391 | ` */` |
|     9980 |  392 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  393 | `{` |
|        - |  394 | `	SyString sChunk;    /* Chunk to evaluate */` |
|        - |  395 | `	sxi32 rc;` |
|     9985 |  396 | `	if( nArg < 1 ){` |
|        - |  397 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  398 | `		ph7_result_null(pCtx);` |
|      ! 0 |  399 | `		return SXRET_OK;` |
|        - |  400 | `	}` |
|        - |  401 | `	/* Chunk to evaluate. Coerced USER-VISIBLY like every other string argument` |
|        - |  402 | `	 * spelled as a language construct: an object with no __toString() is php's` |
|        - |  403 | `	 * Error, where PHL used to hand the parser the literal "Object" and answer a` |
|        - |  404 | `	 * ParseError. */` |
|        - |  405 | `	{` |
|     9985 |  406 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sChunk.zString,(int *)&sChunk.nByte);` |
|     9985 |  407 | `		if( rcSv != SXRET_OK ){` |
|        3 |  408 | `			return rcSv;` |
|        - |  409 | `		}` |
|        - |  410 | `	}` |
|     9983 |  411 | `	if( sChunk.nByte < 1 ){` |
|        - |  412 | `		/* php: eval('') compiles nothing and yields FALSE (a whitespace-only` |
|        - |  413 | `		 * chunk still compiles, and yields NULL through the normal path). */` |
|        5 |  414 | `		ph7_result_bool(pCtx,0);` |
|        5 |  415 | `		return SXRET_OK;` |
|        - |  416 | `	}` |
|        - |  417 | `	/* Eval the chunk.` |
|        - |  418 | `	 *` |
|        - |  419 | `` 	 * php compiles it as though it began right after a `<?php`, which means a `?>` `` |
|        - |  420 | `` 	 * inside it LEAVES php mode: the text after it is echoed and a later `<?php` `` |
|        - |  421 | ``	 * re-enters. `eval('?>' . file_get_contents($f))` is the ordinary way to run a`` |
|        - |  422 | `` 	 * php FILE's bytes, and composer reloads its own `vendor/composer/installed.php` `` |
|        - |  423 | `	 * exactly that way. PHL handed the whole chunk to the compiler as ONE php token` |
|        - |  424 | ``	 * (PH7_PHP_ONLY), so the `?>` was a syntax error and composer could not boot.`` |
|        - |  425 | `	 *` |
|        - |  426 | `	 * Prepending the opening tag and letting the ordinary raw tokenizer split it is` |
|        - |  427 | `	 * php's rule itself, with no second implementation of it. The prefix carries no` |
|        - |  428 | `	 * newline, so every line still reports the number the caller wrote it on. */` |
|        - |  429 | `	{` |
|        - |  430 | `		SyBlob sTagged;` |
|        - |  431 | `		SyString sTaggedStr;` |
|        - |  432 | `		SyString sUnit;` |
|        - |  433 | `		int bUnit;` |
|     9979 |  434 | `		SyBlobInit(&sTagged,&pCtx->pVm->sAllocator);` |
|     9974 |  435 | `		if( SyBlobAppend(&sTagged,"<?php ",sizeof("<?php ")-1) != SXRET_OK` |
|     9979 |  436 | `		 \|\| SyBlobAppend(&sTagged,sChunk.zString,sChunk.nByte) != SXRET_OK ){` |
|      ! 0 |  437 | `			SyBlobRelease(&sTagged);` |
|      ! 0 |  438 | `			return PH7_ContextMemoryError(pCtx);` |
|        - |  439 | `		}` |
|     9979 |  440 | `		SyStringInitFromBuf(&sTaggedStr,SyBlobData(&sTagged),SyBlobLength(&sTagged));` |
|        - |  441 | `		/* php gives eval() a trace frame of its own, exactly as it does an include --` |
|        - |  442 | `		 * argument-less, where an include names the unit it loaded. */` |
|        - |  443 | `		/* Name the chunk BEFORE the trace frame goes on: PH7_VmIncFramePush records` |
|        - |  444 | `		 * an entry whose pFrame is the CURRENT frame, and PH7_VmExecutingUnitFile` |
|        - |  445 | `		 * answers such a frame from the include-stack TOP -- so asking after the push` |
|        - |  446 | `		 * names the unit being loaded rather than the one holding the eval(), and a` |
|        - |  447 | `		 * template engine's compiled chunk came out named after the SCRIPT instead of` |
|        - |  448 | `		 * the library method that evaluated it. */` |
|     9979 |  449 | `		bUnit = (VmEvalUnitName(pCtx->pVm,&sUnit) == SXRET_OK);` |
|     9979 |  450 | `		PH7_VmIncFramePush(pCtx->pVm,"eval",0);` |
|        - |  451 | `		/* Now push the chunk's own unit name. The include-frame entry is what makes` |
|        - |  452 | `		 * the include stack answer for code running at the chunk's TOP level -- which` |
|        - |  453 | `		 * pushes no VmFrame of its own -- so the two go together. */` |
|     9979 |  454 | `		bUnit = bUnit && (SySetPut(&pCtx->pVm->aFiles,(const void *)&sUnit) == SXRET_OK);` |
|     9979 |  455 | `		rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sTaggedStr,0,FALSE);` |
|     9979 |  456 | `		if( bUnit ){` |
|     9979 |  457 | `			(void)SySetPop(&pCtx->pVm->aFiles);` |
|     4987 |  458 | `		}` |
|     9979 |  459 | `		PH7_VmIncFramePop(pCtx->pVm);` |
|     9979 |  460 | `		SyBlobRelease(&sTagged);` |
|        - |  461 | `	}` |
|     9979 |  462 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  463 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       73 |  464 | `		return PH7_ABORT;` |
|        - |  465 | `	}` |
|        - |  466 | `	/* Propagate a ParseError from a failed compile (php unwinds; PHL used to` |
|        - |  467 | `	 * keep executing the statement that contained the eval). */` |
|     9909 |  468 | `	return rc;` |
|     4995 |  469 | `}` |
|        - |  470 | `/*` |
|        - |  471 | ` * Check if a file path is already included.` |
|        - |  472 | ` */` |
|    11370 |  473 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        5 |  474 | `{` |
|        - |  475 | `	SyString *aEntries;` |
|        - |  476 | `	sxu32 n;` |
|    11375 |  477 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  478 | `	/* Perform a linear search */` |
| 31337811 |  479 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 31326491 |  480 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  481 | `			/* Already included */` |
|       55 |  482 | `			return TRUE;` |
|        - |  483 | `		}` |
| 15663222 |  484 | `	}` |
|    11325 |  485 | `	return FALSE;` |
|     5690 |  486 | `}` |
|        - |  487 | `/*` |
|        - |  488 | ` * Push a file path in the appropriate VM container.` |
|        - |  489 | ` */` |
|    18091 |  490 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 |  491 | `{` |
|        - |  492 | `	SyString sPath;` |
|        - |  493 | `	char *zDup;` |
|        - |  494 | `	sxi32 rc;` |
|    18096 |  495 | `	if( nLen < 0 ){` |
|     6726 |  496 | `		nLen = SyStrlen(zPath);` |
|     3356 |  497 | `	}` |
|        - |  498 | `	/* Duplicate the file path first */` |
|    18096 |  499 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    18096 |  500 | `	if( zDup == 0 ){` |
|      ! 0 |  501 | `		return SXERR_MEM;` |
|        - |  502 | `	}` |
|        - |  503 | `#ifdef __UNIXES__` |
|        - |  504 | `	/* php records the REALPATH'd absolute path for __FILE__/__DIR__/getFileName` |
|        - |  505 | `	 * and include-once dedup: realpath() resolves '.', '..' and symlinks (e.g.` |
|        - |  506 | `	 * macOS /tmp -> /private/tmp). It needs an existing file, so on failure keep` |
|        - |  507 | `	 * the raw path (eval'd code, php:///data:// wrappers, a missing include that` |
|        - |  508 | `	 * errors elsewhere). */` |
|        - |  509 | `	{` |
|    18091 |  510 | `		char *zReal = realpath(zDup,0); /* POSIX: malloc'd result */` |
|    18091 |  511 | `		if( zReal ){` |
|    18003 |  512 | `			sxu32 nReal = SyStrlen(zReal);` |
|    18003 |  513 | `			char *zRealDup = SyMemBackendStrDup(&pVm->sAllocator,zReal,nReal);` |
|    18003 |  514 | `			free(zReal);` |
|    18003 |  515 | `			if( zRealDup ){` |
|    18003 |  516 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|    18003 |  517 | `				zDup = zRealDup;` |
|    18003 |  518 | `				nLen = (int)nReal;` |
|     8997 |  519 | `			}` |
|     8997 |  520 | `		}` |
|        - |  521 | `	}` |
|        - |  522 | `#endif` |
|        - |  523 | `#ifdef __WINNT__` |
|        - |  524 | `	/* Windows counterpart of the realpath() above: canonicalize to an absolute` |
|        - |  525 | `	 * path (resolves '.'/'..' , '/' -> '\'), case-preserved to match php — NOT` |
|        - |  526 | `	 * lowercased as the legacy PH7 code did. Like the POSIX branch, keep the raw` |
|        - |  527 | `	 * path when it does not name an existing file (the "Command line code" marker,` |
|        - |  528 | `	 * eval'd code, stream wrappers). _fullpath() is lexical, so pair it with a VFS` |
|        - |  529 | `	 * existence check. */` |
|        - |  530 | `	{` |
|        5 |  531 | `		char *zFull = _fullpath(0,zDup,0); /* MSVC CRT: malloc'd absolute path */` |
|        5 |  532 | `		if( zFull ){` |
|        5 |  533 | `			if( pVm->pEngine->pVfs && pVm->pEngine->pVfs->xFileExists && pVm->pEngine->pVfs->xFileExists(zFull) == PH7_OK ){` |
|        5 |  534 | `				sxu32 nFull = SyStrlen(zFull);` |
|        5 |  535 | `				char *zFullDup = SyMemBackendStrDup(&pVm->sAllocator,zFull,nFull);` |
|        5 |  536 | `				if( zFullDup ){` |
|        5 |  537 | `					SyMemBackendFree(&pVm->sAllocator,zDup);` |
|        5 |  538 | `					zDup = zFullDup;` |
|        5 |  539 | `					nLen = (int)nFull;` |
|        - |  540 | `				}` |
|        - |  541 | `			}` |
|        5 |  542 | `			free(zFull);` |
|        - |  543 | `		}` |
|        - |  544 | `	}` |
|        - |  545 | `#endif` |
|        - |  546 | ``	/* A `phar://` url has no realpath() to ask, so its own canonical form is`` |
|        - |  547 | `	 * built here: the archive half as the url spells it, the ENTRY half with its` |
|        - |  548 | ``	 * `.` and `..` segments collapsed the way the archive's reader already`` |
|        - |  549 | `	 * resolves them. Without it two spellings of one entry are two rows in the` |
|        - |  550 | `	 * once-registry.` |
|        - |  551 | `	 *` |
|        - |  552 | `	 * Behind the same guard as ext/phar itself: vm_phar.c's whole body, and the` |
|        - |  553 | ``	 * prototype block this calls into, are `#ifndef PH7_DISABLE_BUILTIN_FUNC`, so`` |
|        - |  554 | `	 * the tiny build has no archive reader to ask and no phar:// url to ask about. */` |
|        - |  555 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  556 | `	{` |
|        - |  557 | `		SyBlob sPhar;` |
|    18096 |  558 | `		SyBlobInit(&sPhar,&pVm->sAllocator);` |
|    18096 |  559 | `		if( PH7_PharCanonicalUrl(pVm,zDup,nLen,&sPhar) ){` |
|       30 |  560 | `			char *zPharDup = SyMemBackendStrDup(&pVm->sAllocator,` |
|       20 |  561 | `				(const char *)SyBlobData(&sPhar),SyBlobLength(&sPhar));` |
|       20 |  562 | `			if( zPharDup ){` |
|       20 |  563 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       20 |  564 | `				zDup = zPharDup;` |
|       20 |  565 | `				nLen = (int)SyBlobLength(&sPhar);` |
|       10 |  566 | `			}` |
|       10 |  567 | `		}` |
|    18096 |  568 | `		SyBlobRelease(&sPhar);` |
|        - |  569 | `	}` |
|        - |  570 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  571 | `	/* Install the file path */` |
|    18096 |  572 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    18096 |  573 | `	if( !bMain ){` |
|    11375 |  574 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  575 | `			/* Already included */` |
|       55 |  576 | `			*pNew = 0;` |
|       30 |  577 | `		}else{` |
|        - |  578 | `			/* Insert in the corresponding container */` |
|    11325 |  579 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    11325 |  580 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  581 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  582 | `				return rc;` |
|        - |  583 | `			}` |
|    11325 |  584 | `			*pNew = 1;` |
|        - |  585 | `		}` |
|     5685 |  586 | `	}` |
|    18096 |  587 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    18096 |  588 | `	return SXRET_OK;` |
|     9046 |  589 | `}` |
|        - |  590 | `/*` |
|        - |  591 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  592 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  593 | ` * indicates failure.` |
|        - |  594 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  595 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  596 | ` * operations.` |
|        - |  597 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  598 | ` * this function is a no-op.` |
|        - |  599 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  600 | ` * constructs for more information.` |
|        - |  601 | ` */` |
|    11386 |  602 | `static sxi32 VmExecIncludedFile(` |
|        - |  603 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  604 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  605 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  606 | `	 )` |
|        5 |  607 | `{` |
|        - |  608 | `	sxi32 rc;` |
|        - |  609 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  610 | `	const ph7_io_stream *pStream;` |
|        - |  611 | `	SyBlob sContents;` |
|        - |  612 | `	void *pHandle;` |
|        - |  613 | `	ph7_vm *pVm;` |
|        - |  614 | `	int isNew;` |
|        - |  615 | `	/* Initialize fields */` |
|    11391 |  616 | `	pVm = pCtx->pVm;` |
|    11391 |  617 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|    11391 |  618 | `	isNew = 0;` |
|        - |  619 | `	/* Extract the associated stream. The lookup ADVANCES the pointer past the` |
|        - |  620 | `	 * scheme, so it walks a copy: advancing the caller's SyString left its nByte` |
|        - |  621 | `	 * describing the whole url and its zString seven bytes in, and the failure` |
|        - |  622 | `	 * message below then printed that many bytes from there -- past the end of the` |
|        - |  623 | ``	 * string, so `include 'file:///nope'` reported whatever followed it in memory. */`` |
|        - |  624 | `	{` |
|    11391 |  625 | `		const char *zOpen = pPath->zString;` |
|    11391 |  626 | `		pStream = PH7_VmGetStreamDevice(pVm,&zOpen,(int)pPath->nByte);` |
|        - |  627 | `		/*` |
|        - |  628 | `		 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  629 | `		 * in a read-only mode.` |
|        - |  630 | `		 */` |
|    11391 |  631 | `		pHandle = PH7_StreamOpenHandle(pVm,pStream,zOpen,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew,ph7_function_name(pCtx));` |
|        - |  632 | `	}` |
|    11391 |  633 | `	if( pHandle == 0 ){` |
|       19 |  634 | `		return SXERR_IO;` |
|        - |  635 | `	}` |
|    11375 |  636 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|    11375 |  637 | `	if( IncludeOnce && !isNew ){` |
|        - |  638 | `		/* Already included */` |
|       36 |  639 | `		rc = SXERR_EXISTS;` |
|       20 |  640 | `	}else{` |
|        - |  641 | `		/* Read the whole file contents */` |
|    11343 |  642 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|    11343 |  643 | `		if( rc == SXRET_OK ){` |
|        - |  644 | `			SyString sScript;` |
|        - |  645 | `			/* Compile and execute the script. A throw the included file raised — and the` |
|        - |  646 | `			 * INCLUDING statement's own try caught in place — comes back as PH7_EXCEPTION` |
|        - |  647 | `			 * and travels out to the include builtin's caller, which unwinds the rest of` |
|        - |  648 | `			 * the statement instead of resuming it. It is not an IO failure, so the` |
|        - |  649 | `			 * callers must tell the two apart before warning. */` |
|    11343 |  650 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|        - |  651 | `			/* php shows the construct itself as a trace frame; the path it names is` |
|        - |  652 | `			 * the RESOLVED one, which is what aFiles was just given. */` |
|    17012 |  653 | `			PH7_VmIncFramePush(pVm,ph7_function_name(pCtx),` |
|    11338 |  654 | `				(SyString *)SySetPeek(&pVm->aFiles));` |
|    11343 |  655 | `			rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|    11343 |  656 | `			PH7_VmIncFramePop(pVm);` |
|    11343 |  657 | `			if( rc != PH7_EXCEPTION ){` |
|    11329 |  658 | `				rc = SXRET_OK;` |
|     5662 |  659 | `			}` |
|     5669 |  660 | `		}` |
|        - |  661 | `	}` |
|        - |  662 | `	/* Pop from the set of included file */` |
|    11375 |  663 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  664 | `	/* Close the handle */` |
|    11375 |  665 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  666 | `	/* Release the working buffer */` |
|    11375 |  667 | `	SyBlobRelease(&sContents);` |
|        - |  668 | `#else` |
|        - |  669 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  670 | `	SXUNUSED(pPath);` |
|        - |  671 | `	SXUNUSED(IncludeOnce);` |
|        - |  672 | `	rc = SXERR_IO;` |
|        - |  673 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    11375 |  674 | `	return rc;` |
|     5698 |  675 | `}` |
|        - |  676 | `/*` |
|        - |  677 | ` * php keeps include_path in ONE place -- the INI table -- and get_include_path(),` |
|        - |  678 | ` * ini_get('include_path'), ini_get_all() and the resolver all read that one string.` |
|        - |  679 | ` * PHL had TWO: pVm->aPaths, which is what the include walk actually uses and which` |
|        - |  680 | `` * only set_include_path() ever wrote, and the `include_path` INI slot, which is what`` |
|        - |  681 | ` * ini_get() answers and which only ini_set()/-d ever wrote. Neither told the other,` |
|        - |  682 | `` * so `ini_set('include_path', $dir)` -- the ordinary way a bootstrap file points the`` |
|        - |  683 | `` * engine at a library -- moved NOTHING, and `set_include_path($dir)` left ini_get()`` |
|        - |  684 | ` * naming the value that was no longer in force. The set below is the single store;` |
|        - |  685 | ` * the INI slot is a view of it (vm_builtin_ini.c).` |
|        - |  686 | ` */` |
|      116 |  687 | `PH7_PRIVATE int PH7_VmIncludePathSep(void)` |
|        3 |  688 | `{` |
|        - |  689 | `#ifdef __WINNT__` |
|        3 |  690 | `	return ';';` |
|        - |  691 | `#else` |
|        - |  692 | `	/* Assume UNIX path separator */` |
|      116 |  693 | `	return ':';` |
|        - |  694 | `#endif` |
|        3 |  695 | `}` |
|        - |  696 | `/*` |
|        - |  697 | ` * Split an include_path STRING into its segments and make them the VM's set.` |
|        - |  698 | ` *` |
|        - |  699 | ` * Segments are kept VERBATIM. php hands back the string it was given, so a` |
|        - |  700 | ` * trailing slash, a leading space and an EMPTY segment all survive the round trip` |
|        - |  701 | ` * -- and the walk means something for each of them: php builds "<segment>/<file>"` |
|        - |  702 | ` * from every entry, which is how an empty segment comes to try "/file".` |
|        - |  703 | ` */` |
|       24 |  704 | `PH7_PRIVATE void PH7_VmSetIncludePath(ph7_vm *pVm,const char *zPath,sxu32 nByte)` |
|        2 |  705 | `{` |
|        - |  706 | `	const char *z,*zEnd,*zDup;` |
|       26 |  707 | `	int dir_sep = PH7_VmIncludePathSep();` |
|       26 |  708 | `	SySetReset(&pVm->aPaths);` |
|       26 |  709 | `	if( nByte < 1 ){` |
|      ! 0 |  710 | `		return;` |
|        - |  711 | `	}` |
|        - |  712 | `	/* ONE VM-lifetime copy backs every segment (the SyString entries alias it),` |
|        - |  713 | `	 * where the old splitter duped each segment separately on every call. */` |
|       26 |  714 | `	zDup = (const char *)SyMemBackendDup(&pVm->sAllocator,zPath,nByte);` |
|       26 |  715 | `	if( zDup == 0 ){` |
|      ! 0 |  716 | `		return;` |
|        - |  717 | `	}` |
|       26 |  718 | `	z = zDup;` |
|       26 |  719 | `	zEnd = &zDup[nByte];` |
|       20 |  720 | `	for(;;){` |
|       34 |  721 | `		const char *zStart = z;` |
|        - |  722 | `		SyString sPath;` |
|      337 |  723 | `		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }` |
|       34 |  724 | `		SyStringInitFromBuf(&sPath,zStart,(sxu32)(z-zStart));` |
|       34 |  725 | `		SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|       34 |  726 | `		if( z >= zEnd ){` |
|       26 |  727 | `			break;` |
|        - |  728 | `		}` |
|        9 |  729 | `		z++; /* skip the separator */` |
|        1 |  730 | `	}` |
|       14 |  731 | `}` |
|        - |  732 | `/*` |
|        - |  733 | ` * php's LAST RESORT for a relative name the include_path did not answer: the` |
|        - |  734 | ` * directory of the file that is EXECUTING, not the process's cwd. It is why` |
|        - |  735 | `` * `include 'helper.php'` next to the script keeps working when the script was`` |
|        - |  736 | ` * started from somewhere else, and why a library's own relative includes` |
|        - |  737 | ` * resolve at all. PHL had nothing of the kind, so` |
|        - |  738 | ` *` |
|        - |  739 | ` *   cd / && phl /srv/app/main.php   with   include 'lib.php'   next to main.php` |
|        - |  740 | ` *` |
|        - |  741 | ` * failed on this engine and ran on php. aFiles is the include-nesting stack,` |
|        - |  742 | ` * so its top is the innermost executing file -- which is what php uses too.` |
|        - |  743 | ` *` |
|        - |  744 | `` * Answers 0 when that name has no directory part at all (`-r`'s "Command line`` |
|        - |  745 | ` * code"). php's own arithmetic degenerates to the bare, cwd-relative name` |
|        - |  746 | ` * there, which the default include_path's "." entry already tries.` |
|        - |  747 | ` */` |
|       20 |  748 | `PH7_PRIVATE int PH7_VmExecutingDir(ph7_vm *pVm,SyString *pOut)` |
|        2 |  749 | `{` |
|       22 |  750 | `	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  751 | `	sxu32 n;` |
|       22 |  752 | `	if( pFile == 0 \|\| pFile->nByte < 2 ){` |
|      ! 0 |  753 | `		return 0;` |
|        - |  754 | `	}` |
|       22 |  755 | `	n = pFile->nByte;` |
|      454 |  756 | `	while( n > 0 ){` |
|      454 |  757 | `		int c = pFile->zString[n-1];` |
|      452 |  758 | `		if( c == '/'` |
|        - |  759 | `#ifdef __WINNT__` |
|        2 |  760 | `		 \|\| c == '\\'` |
|        - |  761 | `#endif` |
|        - |  762 | `		){` |
|       22 |  763 | `			break;` |
|        - |  764 | `		}` |
|      434 |  765 | `		n--;` |
|        2 |  766 | `	}` |
|       22 |  767 | `	if( n < 2 ){` |
|        - |  768 | `		/* No separator, or one sitting at index 0 -- php skips the fallback for` |
|        - |  769 | `		 * a root-level script exactly the same way. */` |
|      ! 0 |  770 | `		return 0;` |
|        - |  771 | `	}` |
|       22 |  772 | `	SyStringInitFromBuf(pOut,pFile->zString,n-1); /* without the separator */` |
|       22 |  773 | `	return 1;` |
|       12 |  774 | `}` |
|        - |  775 | `/*` |
|        - |  776 | ` * Join the set back into php's one string. Splitting on the separator and` |
|        - |  777 | ` * joining with it round-trips exactly, which is what ini_get() promises.` |
|        - |  778 | ` */` |
|       92 |  779 | `PH7_PRIVATE void PH7_VmGetIncludePath(ph7_vm *pVm,SyBlob *pOut)` |
|        3 |  780 | `{` |
|       95 |  781 | `	SyString *aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       95 |  782 | `	char cSep = (char)PH7_VmIncludePathSep();` |
|        - |  783 | `	sxu32 n;` |
|      203 |  784 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|      111 |  785 | `		if( n > 0 ){` |
|       17 |  786 | `			SyBlobAppend(pOut,(const void *)&cSep,sizeof(char));` |
|        8 |  787 | `		}` |
|      111 |  788 | `		SyBlobAppend(pOut,aEntry[n].zString,aEntry[n].nByte);` |
|       56 |  789 | `	}` |
|       95 |  790 | `}` |
|        - |  791 | `/*` |
|        - |  792 | ` * string get_include_path(void)` |
|        - |  793 | ` *  Gets the current include_path configuration option.` |
|        - |  794 | ` * Parameter` |
|        - |  795 | ` *  None` |
|        - |  796 | ` * Return` |
|        - |  797 | ` *  Included paths as a string` |
|        - |  798 | ` */` |
|       20 |  799 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  800 | `{` |
|        - |  801 | `	SyBlob sOut;` |
|       10 |  802 | `	SXUNUSED(nArg); /* cc warning */` |
|       10 |  803 | `	SXUNUSED(apArg);` |
|       23 |  804 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|       23 |  805 | `	PH7_VmGetIncludePath(pCtx->pVm,&sOut);` |
|        - |  806 | `	/* php's answer is always a STRING: an empty set is "", never NULL, which is` |
|        - |  807 | `	 * what this used to leave behind when nothing had ever been appended. */` |
|       23 |  808 | `	if( SyBlobLength(&sOut) < 1 ){` |
|      ! 0 |  809 | `		ph7_result_string(pCtx,"",0);` |
|      ! 0 |  810 | `	}else{` |
|       23 |  811 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|        - |  812 | `	}` |
|       23 |  813 | `	SyBlobRelease(&sOut);` |
|       23 |  814 | `	return PH7_OK;` |
|        3 |  815 | `}` |
|        - |  816 | `/*` |
|        - |  817 | ` * string\|false set_include_path(string $include_path)` |
|        - |  818 | ` *  Sets the include_path configuration option for the duration of the script,` |
|        - |  819 | ` *  returning the OLD value (php contract).` |
|        - |  820 | ` */` |
|       14 |  821 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  822 | `{` |
|       16 |  823 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  824 | `	const char *zNew;` |
|       16 |  825 | `	int nLen = 0;` |
|        - |  826 | `	SyBlob sOld;` |
|       16 |  827 | `	if( nArg < 1 ){` |
|      ! 0 |  828 | `		return PH7_OK;` |
|        - |  829 | `	}` |
|        - |  830 | `	/* The OLD value php answers is the effective one, read before the write. */` |
|       16 |  831 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|       16 |  832 | `	PH7_VmGetIncludePath(pVm,&sOld);` |
|       16 |  833 | `	zNew = ph7_value_to_string(apArg[0],&nLen);` |
|       16 |  834 | `	if( nLen < 1 ){` |
|        - |  835 | `		/* php registers include_path with OnUpdateStringUnempty, so the EMPTY` |
|        - |  836 | `		 * string is refused outright: the directive keeps the value it had and` |
|        - |  837 | `		 * the call answers FALSE. PHL used to accept it, wiping the set and` |
|        - |  838 | `		 * leaving get_include_path() with nothing to answer. */` |
|        3 |  839 | `		SyBlobRelease(&sOld);` |
|        3 |  840 | `		ph7_result_bool(pCtx,0);` |
|        3 |  841 | `		return PH7_OK;` |
|        - |  842 | `	}` |
|       14 |  843 | `	PH7_VmSetIncludePath(pVm,zNew,(sxu32)nLen);` |
|       14 |  844 | `	if( SyBlobLength(&sOld) < 1 ){` |
|      ! 0 |  845 | `		ph7_result_string(pCtx,"",0);` |
|      ! 0 |  846 | `	}else{` |
|       14 |  847 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|        - |  848 | `	}` |
|       14 |  849 | `	SyBlobRelease(&sOld);` |
|       14 |  850 | `	return PH7_OK;` |
|        9 |  851 | `}` |
|        - |  852 | `/*` |
|        - |  853 | ` * string get_get_included_files(void)` |
|        - |  854 | ` *  Gets the current include_path configuration option.` |
|        - |  855 | ` * Parameter` |
|        - |  856 | ` *  None` |
|        - |  857 | ` * Return` |
|        - |  858 | ` *  Included paths as a string` |
|        - |  859 | ` */` |
|        8 |  860 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  861 | `{` |
|        9 |  862 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  863 | `	ph7_value *pArray,*pWorker;` |
|        - |  864 | `	SyString *pEntry;` |
|        - |  865 | `	int c,d;` |
|        - |  866 | `	/* Create an array and a working value */` |
|        9 |  867 | `	pArray  = ph7_context_new_array(pCtx);` |
|        9 |  868 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        9 |  869 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  870 | `		/* Out of memory,return null */` |
|      ! 0 |  871 | `		ph7_result_null(pCtx);` |
|      ! 0 |  872 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  873 | `		SXUNUSED(apArg);` |
|      ! 0 |  874 | `		return PH7_OK;` |
|        - |  875 | `	}` |
|        9 |  876 | `	c = d = '/';` |
|        - |  877 | `#ifdef __WINNT__` |
|        1 |  878 | `	d = '\\';` |
|        - |  879 | `#endif` |
|        - |  880 | `	/* Iterate throw entries */` |
|        9 |  881 | `	SySetResetCursor(pFiles);` |
|       25 |  882 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  883 | `		const char *zBase,*zEnd;` |
|        - |  884 | `		int iLen;` |
|        - |  885 | `		/* reset the string cursor */` |
|       17 |  886 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  887 | `		/* Extract base name */` |
|       17 |  888 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  889 | `		/* Ignore trailing '/' */` |
|       25 |  890 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  891 | `			zEnd--;` |
|      ! 0 |  892 | `		}` |
|       17 |  893 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|      445 |  894 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|      421 |  895 | `			zEnd--;` |
|        1 |  896 | `		}` |
|       17 |  897 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|       17 |  898 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  899 | `		/* Copy entry name */` |
|       17 |  900 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  901 | `		/* Perform the insertion */` |
|       17 |  902 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  903 | `	}` |
|        - |  904 | `	/* All done,return the created array */` |
|        9 |  905 | `	ph7_result_value(pCtx,pArray);` |
|        - |  906 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  907 | `	 * by the engine as soon we return from this foreign` |
|        - |  908 | `	 * function.` |
|        - |  909 | `	 */` |
|        9 |  910 | `	return PH7_OK;` |
|        5 |  911 | `}` |
|        - |  912 | `/*` |
|        - |  913 | ` * include:` |
|        - |  914 | ` * According to the PHP reference manual.` |
|        - |  915 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  916 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  917 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  918 | ` *  include() will finally check in the calling script's own directory` |
|        - |  919 | ` *  and the current working directory before failing. The include()` |
|        - |  920 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  921 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  922 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  923 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  924 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  925 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  926 | ` *  directory to find the requested file.` |
|        - |  927 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  928 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  929 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  930 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  931 | ` */` |
|    11218 |  932 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  933 | `{` |
|        - |  934 | `	SyString sFile;` |
|        - |  935 | `	sxi32 rc;` |
|    11223 |  936 | `	if( nArg < 1 ){` |
|        - |  937 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  938 | `		ph7_result_null(pCtx);` |
|      ! 0 |  939 | `		return SXRET_OK;` |
|        - |  940 | `	}` |
|        - |  941 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  942 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  943 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  944 | `	{` |
|    11223 |  945 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|    11223 |  946 | `		if( rcSv != SXRET_OK ){` |
|        3 |  947 | `			return rcSv;` |
|        - |  948 | `		}` |
|        - |  949 | `	}` |
|    11221 |  950 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - |  951 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - |  952 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - |  953 | `		 * this used to answer NULL and carry on. */` |
|        2 |  954 | `		return PH7_EXCEPTION;` |
|        - |  955 | `	}` |
|        - |  956 | `	/* Open,compile and execute the desired script */` |
|    11219 |  957 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|    11219 |  958 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - |  959 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - |  960 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - |  961 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - |  962 | `		 * wins, so it is tested first. */` |
|        6 |  963 | `		return rc;` |
|        - |  964 | `	}` |
|    11215 |  965 | `	if( rc != SXRET_OK ){` |
|        - |  966 | `		/* Emit a warning and return false */` |
|       14 |  967 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|       14 |  968 | `		ph7_result_bool(pCtx,0);` |
|        6 |  969 | `	}` |
|    11215 |  970 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  971 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 |  972 | `		return PH7_ABORT;` |
|        - |  973 | `	}` |
|    11211 |  974 | `	return SXRET_OK;` |
|     5614 |  975 | `}` |
|        - |  976 | `/*` |
|        - |  977 | ` * include_once:` |
|        - |  978 | ` *  According to the PHP reference manual.` |
|        - |  979 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  980 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  981 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  982 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  983 | ` *   just once.` |
|        - |  984 | ` */` |
|       30 |  985 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  986 | `{` |
|        - |  987 | `	SyString sFile;` |
|        - |  988 | `	sxi32 rc;` |
|       34 |  989 | `	if( nArg < 1 ){` |
|        - |  990 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  991 | `		ph7_result_null(pCtx);` |
|      ! 0 |  992 | `		return SXRET_OK;` |
|        - |  993 | `	}` |
|        - |  994 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  995 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  996 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  997 | `	{` |
|       34 |  998 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       34 |  999 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1000 | `			return rcSv;` |
|        - | 1001 | `		}` |
|        - | 1002 | `	}` |
|       32 | 1003 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1004 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1005 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1006 | `		 * this used to answer NULL and carry on. */` |
|      ! 0 | 1007 | `		return PH7_EXCEPTION;` |
|        - | 1008 | `	}` |
|        - | 1009 | `	/* Open,compile and execute the desired script */` |
|       32 | 1010 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       32 | 1011 | `	if( rc == SXERR_EXISTS ){` |
|        - | 1012 | `		/* File already included,return TRUE */` |
|       21 | 1013 | `		ph7_result_bool(pCtx,1);` |
|       21 | 1014 | `		return SXRET_OK;` |
|        - | 1015 | `	}` |
|       14 | 1016 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1017 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1018 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1019 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1020 | `		 * wins, so it is tested first. */` |
|        3 | 1021 | `		return rc;` |
|        - | 1022 | `	}` |
|       11 | 1023 | `	if( rc != SXRET_OK ){` |
|        - | 1024 | `		/* Emit a warning and return false */` |
|      ! 0 | 1025 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 1026 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1027 | ` 	}` |
|       11 | 1028 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1029 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 1030 | `		return PH7_ABORT;` |
|        - | 1031 | `	}` |
|       11 | 1032 | `	return SXRET_OK;` |
|       19 | 1033 | `}` |
|        - | 1034 | `/*` |
|        - | 1035 | ` * require.` |
|        - | 1036 | ` *  According to the PHP reference manual.` |
|        - | 1037 | ` *   require() is identical to include() except upon failure it will` |
|        - | 1038 | ` *   also produce a fatal level error.` |
|        - | 1039 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 1040 | ` *   emits a warning  which allows the script to continue.` |
|        - | 1041 | ` */` |
|      122 | 1042 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1043 | `{` |
|        - | 1044 | `	SyString sFile;` |
|        - | 1045 | `	sxi32 rc;` |
|      127 | 1046 | `	if( nArg < 1 ){` |
|        - | 1047 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 1048 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1049 | `		return SXRET_OK;` |
|        - | 1050 | `	}` |
|        - | 1051 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - | 1052 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - | 1053 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - | 1054 | `	{` |
|      127 | 1055 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|      127 | 1056 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1057 | `			return rcSv;` |
|        - | 1058 | `		}` |
|        - | 1059 | `	}` |
|      125 | 1060 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1061 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1062 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1063 | `		 * this used to answer NULL and carry on. */` |
|        2 | 1064 | `		return PH7_EXCEPTION;` |
|        - | 1065 | `	}` |
|        - | 1066 | `	/* Open,compile and execute the desired script */` |
|      123 | 1067 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|      123 | 1068 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1069 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1070 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1071 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1072 | `		 * wins, so it is tested first. */` |
|        9 | 1073 | `		return rc;` |
|        - | 1074 | `	}` |
|      117 | 1075 | `	if( rc != SXRET_OK ){` |
|        - | 1076 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 1077 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 1078 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1079 | `		return PH7_ABORT;` |
|        - | 1080 | `	}` |
|      117 | 1081 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1082 | `		/* exit/die inside the included file: cascade the halt */` |
|       10 | 1083 | `		return PH7_ABORT;` |
|        - | 1084 | `	}` |
|      109 | 1085 | `	return SXRET_OK;` |
|       66 | 1086 | `}` |
|        - | 1087 | `/*` |
|        - | 1088 | ` * require_once:` |
|        - | 1089 | ` *  According to the PHP reference manual.` |
|        - | 1090 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 1091 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 1092 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 1093 | ` *   and how it differs from its non _once siblings.` |
|        - | 1094 | ` */` |
|       24 | 1095 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 1096 | `{` |
|        - | 1097 | `	SyString sFile;` |
|        - | 1098 | `	sxi32 rc;` |
|       28 | 1099 | `	if( nArg < 1 ){` |
|        - | 1100 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 1101 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1102 | `		return SXRET_OK;` |
|        - | 1103 | `	}` |
|        - | 1104 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - | 1105 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - | 1106 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - | 1107 | `	{` |
|       28 | 1108 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       28 | 1109 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1110 | `			return rcSv;` |
|        - | 1111 | `		}` |
|        - | 1112 | `	}` |
|       25 | 1113 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1114 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1115 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1116 | `		 * this used to answer NULL and carry on. */` |
|      ! 0 | 1117 | `		return PH7_EXCEPTION;` |
|        - | 1118 | `	}` |
|        - | 1119 | `	/* Open,compile and execute the desired script */` |
|       25 | 1120 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       25 | 1121 | `	if( rc == SXERR_EXISTS ){` |
|        - | 1122 | `		/* File already included,return TRUE */` |
|       16 | 1123 | `		ph7_result_bool(pCtx,1);` |
|       16 | 1124 | `		return SXRET_OK;` |
|        - | 1125 | `	}` |
|       11 | 1126 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1127 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1128 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1129 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1130 | `		 * wins, so it is tested first. */` |
|        3 | 1131 | `		return rc;` |
|        - | 1132 | `	}` |
|        8 | 1133 | `	if( rc != SXRET_OK ){` |
|        - | 1134 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 1135 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 1136 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1137 | `		return PH7_ABORT;` |
|        - | 1138 | `	}` |
|        8 | 1139 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1140 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 1141 | `		return PH7_ABORT;` |
|        - | 1142 | `	}` |
|        8 | 1143 | `	return SXRET_OK;` |
|       16 | 1144 | `}` |
|        - | 1145 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 1146 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 1147 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 1148 | `/*` |
|        - | 1149 | ` * Section:` |
|        - | 1150 | ` *  SPL Autoloading functions.` |
|        - | 1151 | ` * Status:` |
|        - | 1152 | ` *  Stable.` |
|        - | 1153 | ` */` |
|        - | 1154 | `/*` |
|        - | 1155 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 1156 | ` *  Register given function as __autoload() implementation.` |
|        - | 1157 | ` * Parameters` |
|        - | 1158 | ` *  callback` |
|        - | 1159 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 1160 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 1161 | ` *  throw` |
|        - | 1162 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 1163 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 1164 | ` *  prepend` |
|        - | 1165 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 1166 | ` *   autoload stack instead of appending it.` |
|        - | 1167 | ` * Return` |
|        - | 1168 | ` *  TRUE on success, FALSE on failure.` |
|        - | 1169 | ` */` |
|       70 | 1170 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1171 | `{` |
|        - | 1172 | `	VmAutoloadCB sEntry;` |
|       75 | 1173 | `	ph7_vm *pVm = pCtx->pVm;` |
|       75 | 1174 | `	int iPrepend = 0;` |
|        - | 1175 | `	sxu32 n;` |
|       75 | 1176 | `	if( nArg < 1 ){` |
|        - | 1177 | `		/* No callback provided — register default spl_autoload.` |
|        - | 1178 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 1179 | `		/* Check for duplicates first */` |
|        9 | 1180 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 1181 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        4 | 1182 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 1183 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 1184 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 1185 | `				ph7_result_bool(pCtx,1);` |
|        5 | 1186 | `				return SXRET_OK;` |
|        - | 1187 | `			}` |
|      ! 0 | 1188 | `		}` |
|        5 | 1189 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 1190 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 1191 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 1192 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 1193 | `		ph7_result_bool(pCtx,1);` |
|        5 | 1194 | `		return SXRET_OK;` |
|        - | 1195 | `	}` |
|        - | 1196 | `	/* Validate that the callback is callable */` |
|       67 | 1197 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 1198 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 1199 | `		if( nArg >= 2 ){` |
|      ! 0 | 1200 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 1201 | `		}` |
|      ! 0 | 1202 | `		if( iThrow ){` |
|      ! 0 | 1203 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 1204 | `				"Argument is not callable");` |
|      ! 0 | 1205 | `		}` |
|      ! 0 | 1206 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1207 | `		return SXRET_OK;` |
|        - | 1208 | `	}` |
|        - | 1209 | `	/* Check for duplicates */` |
|       87 | 1210 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       22 | 1211 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       22 | 1212 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 1213 | `			/* Already registered */` |
|      ! 0 | 1214 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 1215 | `			return SXRET_OK;` |
|        - | 1216 | `		}` |
|       12 | 1217 | `	}` |
|        - | 1218 | `	/* Check prepend flag */` |
|       67 | 1219 | `	if( nArg >= 3 ){` |
|        3 | 1220 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 1221 | `	}` |
|        - | 1222 | `	/* Store the callback */` |
|       67 | 1223 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       67 | 1224 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       67 | 1225 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       68 | 1226 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 1227 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 1228 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 1229 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 1230 | `		VmAutoloadCB *aBase;` |
|        3 | 1231 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 1232 | `		/* Rotate: move last entry to front */` |
|        3 | 1233 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 1234 | `		if( aBase ){` |
|        - | 1235 | `			VmAutoloadCB sTemp;` |
|        - | 1236 | `			sxu32 i;` |
|        3 | 1237 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 1238 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 1239 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 1240 | `			}` |
|        3 | 1241 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 1242 | `		}` |
|        2 | 1243 | `	}else{` |
|       65 | 1244 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 1245 | `	}` |
|       67 | 1246 | `	ph7_result_bool(pCtx,1);` |
|       67 | 1247 | `	return SXRET_OK;` |
|       40 | 1248 | `}` |
|        - | 1249 | `/*` |
|        - | 1250 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 1251 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 1252 | ` * Parameters` |
|        - | 1253 | ` *  callback` |
|        - | 1254 | ` *   The autoload function being unregistered.` |
|        - | 1255 | ` * Return` |
|        - | 1256 | ` *  TRUE on success, FALSE on failure.` |
|        - | 1257 | ` */` |
|       40 | 1258 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1259 | `{` |
|       45 | 1260 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1261 | `	sxu32 n,nEntry;` |
|       45 | 1262 | `	if( nArg < 1 ){` |
|      ! 0 | 1263 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1264 | `		return SXRET_OK;` |
|        - | 1265 | `	}` |
|       45 | 1266 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       49 | 1267 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       47 | 1268 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       47 | 1269 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 1270 | `			/* Found — remove by shifting remaining entries down */` |
|       43 | 1271 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 1272 | `			sxu32 i;` |
|       43 | 1273 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       59 | 1274 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       18 | 1275 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|       10 | 1276 | `			}` |
|        - | 1277 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       43 | 1278 | `			SySetPop(&pVm->aAutoload);` |
|       43 | 1279 | `			ph7_result_bool(pCtx,1);` |
|       43 | 1280 | `			return SXRET_OK;` |
|        - | 1281 | `		}` |
|        3 | 1282 | `	}` |
|        3 | 1283 | `	ph7_result_bool(pCtx,0);` |
|        3 | 1284 | `	return SXRET_OK;` |
|       25 | 1285 | `}` |
|        - | 1286 | `/*` |
|        - | 1287 | ` * array spl_autoload_functions(void)` |
|        - | 1288 | ` *  Return all registered __autoload() functions.` |
|        - | 1289 | ` * Return` |
|        - | 1290 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 1291 | ` *  an empty array is returned.` |
|        - | 1292 | ` */` |
|       20 | 1293 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1294 | `{` |
|       21 | 1295 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1296 | `	ph7_value *pArray;` |
|        - | 1297 | `	sxu32 n,nEntry;` |
|       10 | 1298 | `	SXUNUSED(nArg);` |
|       10 | 1299 | `	SXUNUSED(apArg);` |
|       21 | 1300 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 1301 | `	if( pArray == 0 ){` |
|      ! 0 | 1302 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1303 | `		return SXRET_OK;` |
|        - | 1304 | `	}` |
|       21 | 1305 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 1306 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 1307 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 1308 | `		if( pEntry ){` |
|       15 | 1309 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 1310 | `		}` |
|        8 | 1311 | `	}` |
|       21 | 1312 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 1313 | `	return SXRET_OK;` |
|       11 | 1314 | `}` |
|        - | 1315 | `/*` |
|        - | 1316 | ` * string spl_autoload_extensions([?string $file_extensions = null])` |
|        - | 1317 | ` *  Register and return default file extensions for spl_autoload().` |
|        - | 1318 | ` * Return` |
|        - | 1319 | ` *  The list in force AFTER the call, which is the new one when a string was` |
|        - | 1320 | ` *  handed over and the standing one otherwise.` |
|        - | 1321 | ` *` |
|        - | 1322 | ` * The list is per-VM state rather than an ini directive (php keeps it in SPL's` |
|        - | 1323 | `` * own globals), and `null` means READ: it is the one argument value that does`` |
|        - | 1324 | ``  * not write. Every other value writes what it stringifies to, so `false` `` |
|        - | 1325 | ` * empties the list -- and an empty list is a real setting, not a reset.` |
|        - | 1326 | ` */` |
|       18 | 1327 | `PH7_PRIVATE int vm_builtin_spl_autoload_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1328 | `{` |
|       19 | 1329 | `	ph7_vm *pVm = pCtx->pVm;` |
|       19 | 1330 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|        - | 1331 | `		const char *zExt;` |
|        - | 1332 | `		int nExt;` |
|        7 | 1333 | `		zExt = ph7_value_to_string(apArg[0],&nExt);` |
|        7 | 1334 | `		SyBlobReset(&pVm->sAutoloadExt);` |
|        7 | 1335 | `		if( nExt > 0 ){` |
|        5 | 1336 | `			SyBlobAppend(&pVm->sAutoloadExt,zExt,(sxu32)nExt);` |
|        2 | 1337 | `		}` |
|        3 | 1338 | `	}` |
|       28 | 1339 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pVm->sAutoloadExt),` |
|       18 | 1340 | `		(int)SyBlobLength(&pVm->sAutoloadExt));` |
|       19 | 1341 | `	return SXRET_OK;` |
|        1 | 1342 | `}` |
|        - | 1343 | `/*` |
|        - | 1344 | ` * void spl_autoload_call(string $class)` |
|        - | 1345 | ` *  Try all registered autoloaders to load the requested class.` |
|        - | 1346 | ` *` |
|        - | 1347 | ` * php runs the stack whether or not the class is already declared -- the` |
|        - | 1348 | ` * verb is "call the autoloaders", not "load if missing" -- and stops as soon` |
|        - | 1349 | ` * as one of them declares it. The answer is always NULL.` |
|        - | 1350 | ` */` |
|        6 | 1351 | `PH7_PRIVATE int vm_builtin_spl_autoload_call(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1352 | `{` |
|        - | 1353 | `	const char *zClass;` |
|        - | 1354 | `	int nClass;` |
|        7 | 1355 | `	if( nArg < 1 ){` |
|      ! 0 | 1356 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1357 | `		return SXRET_OK;` |
|        - | 1358 | `	}` |
|        7 | 1359 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        - | 1360 | `	{` |
|        - | 1361 | `		/* php runs the stack for the EMPTY name too -- the autoloaders see a` |
|        - | 1362 | `		 * "" they have to answer for, rather than a call that never happened. */` |
|        7 | 1363 | `		sxu32 nByte = nClass > 0 ? (sxu32)nClass : 0;` |
|        7 | 1364 | `		if( zClass == 0 ){` |
|      ! 0 | 1365 | `			zClass = "";` |
|      ! 0 | 1366 | `		}` |
|        7 | 1367 | `		PH7_VmClassNameAnchor(&zClass,&nByte);` |
|        7 | 1368 | `		PH7_VmTriggerAutoload(pCtx->pVm,zClass,nByte,0);` |
|        - | 1369 | `	}` |
|        7 | 1370 | `	ph7_result_null(pCtx);` |
|        7 | 1371 | `	return SXRET_OK;` |
|        4 | 1372 | `}` |
|        - | 1373 | `/*` |
|        - | 1374 | ` * array spl_classes()` |
|        - | 1375 | ` *  Return an array with the name of every class and interface SPL declares.` |
|        - | 1376 | ` *` |
|        - | 1377 | ` * php answers a map whose key and value are both the name, in the order its` |
|        - | 1378 | ` * own list is written -- which is alphabetical. The set is SPL's own and not` |
|        - | 1379 | ` * "everything iterable": Traversable, Countable and the exception BASE are` |
|        - | 1380 | ` * php's Core, so none of the three is here, while the five SPL INTERFACES` |
|        - | 1381 | ` * (OuterIterator, RecursiveIterator, SeekableIterator, SplObserver,` |
|        - | 1382 | ` * SplSubject) are.` |
|        - | 1383 | ` */` |
|        2 | 1384 | `PH7_PRIVATE int vm_builtin_spl_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1385 | `{` |
|        - | 1386 | `	static const char * const azSpl[] = {` |
|        - | 1387 | `		"AppendIterator", "ArrayIterator", "ArrayObject", "BadFunctionCallException",` |
|        - | 1388 | `		"BadMethodCallException", "CachingIterator", "CallbackFilterIterator",` |
|        - | 1389 | `		"DirectoryIterator", "DomainException", "EmptyIterator", "FilesystemIterator",` |
|        - | 1390 | `		"FilterIterator", "GlobIterator", "InfiniteIterator", "InvalidArgumentException",` |
|        - | 1391 | `		"IteratorIterator", "LengthException", "LimitIterator", "LogicException",` |
|        - | 1392 | `		"MultipleIterator", "NoRewindIterator", "OuterIterator", "OutOfBoundsException",` |
|        - | 1393 | `		"OutOfRangeException", "OverflowException", "ParentIterator", "RangeException",` |
|        - | 1394 | `		"RecursiveArrayIterator", "RecursiveCachingIterator",` |
|        - | 1395 | `		"RecursiveCallbackFilterIterator", "RecursiveDirectoryIterator",` |
|        - | 1396 | `		"RecursiveFilterIterator", "RecursiveIterator", "RecursiveIteratorIterator",` |
|        - | 1397 | `		"RecursiveRegexIterator", "RecursiveTreeIterator", "RegexIterator",` |
|        - | 1398 | `		"RuntimeException", "SeekableIterator", "SplDoublyLinkedList", "SplFileInfo",` |
|        - | 1399 | `		"SplFileObject", "SplFixedArray", "SplHeap", "SplMinHeap", "SplMaxHeap",` |
|        - | 1400 | `		"SplObjectStorage", "SplObserver", "SplPriorityQueue", "SplQueue", "SplStack",` |
|        - | 1401 | `		"SplSubject", "SplTempFileObject", "UnderflowException", "UnexpectedValueException"` |
|        - | 1402 | `	};` |
|        - | 1403 | `	ph7_value *pArray,*pVal;` |
|        - | 1404 | `	sxu32 n;` |
|        1 | 1405 | `	SXUNUSED(nArg);` |
|        1 | 1406 | `	SXUNUSED(apArg);` |
|        3 | 1407 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 1408 | `	pVal = ph7_context_new_scalar(pCtx);` |
|        3 | 1409 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|      ! 0 | 1410 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1411 | `		return SXRET_OK;` |
|        - | 1412 | `	}` |
|      113 | 1413 | `	for( n = 0 ; n < SX_ARRAYSIZE(azSpl) ; ++n ){` |
|      111 | 1414 | `		ph7_value_string(pVal,azSpl[n],-1);` |
|      111 | 1415 | `		ph7_array_add_strkey_elem(pArray,azSpl[n],pVal);` |
|      111 | 1416 | `		ph7_value_reset_string_cursor(pVal);` |
|       56 | 1417 | `	}` |
|        3 | 1418 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 1419 | `	return SXRET_OK;` |
|        2 | 1420 | `}` |
|        - | 1421 | `/*` |
|        - | 1422 | ` * void spl_autoload(string $class [, ?string $file_extensions = null ])` |
|        - | 1423 | ` *  Default implementation of __autoload().` |
|        - | 1424 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 1425 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 1426 | ` * Parameters` |
|        - | 1427 | ` *  class` |
|        - | 1428 | ` *   The class name being searched.` |
|        - | 1429 | ` *  file_extensions` |
|        - | 1430 | ` *   Comma-separated list of file extensions to try. When none is handed over,` |
|        - | 1431 | ` *   the list is whatever spl_autoload_extensions() holds -- which is where the` |
|        - | 1432 | ` *   ORDER comes from, and the order decides the answer: php's default tries` |
|        - | 1433 | `` *   `.inc` BEFORE `.php`, and this engine had the pair hardcoded the other way`` |
|        - | 1434 | `` *   round, so a directory carrying both `foo.inc` and `foo.php` loaded the one`` |
|        - | 1435 | ` *   php does not.` |
|        - | 1436 | ` */` |
|        2 | 1437 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1438 | `{` |
|        - | 1439 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 1440 | `	SyBlob sPath;` |
|        - | 1441 | `	int nClass,nExt,iByte;` |
|        - | 1442 | `	sxi32 rc;` |
|        3 | 1443 | `	if( nArg < 1 ){` |
|      ! 0 | 1444 | `		return SXRET_OK;` |
|        - | 1445 | `	}` |
|        3 | 1446 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 1447 | `	if( nClass < 1 ){` |
|      ! 0 | 1448 | `		return SXRET_OK;` |
|        - | 1449 | `	}` |
|        - | 1450 | `	/* The list to try: the argument when there is one, and the standing` |
|        - | 1451 | `	 * spl_autoload_extensions() setting otherwise. */` |
|        3 | 1452 | `	if( nArg >= 2 && !ph7_value_is_null(apArg[1]) ){` |
|        - | 1453 | `		/* An argument that is EMPTY is an empty list and not a request for the` |
|        - | 1454 | ``		 * default one, so `spl_autoload($c,"")` searches nothing at all. */`` |
|      ! 0 | 1455 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 1456 | `	}else{` |
|        3 | 1457 | `		zExt = (const char *)SyBlobData(&pCtx->pVm->sAutoloadExt);` |
|        3 | 1458 | `		nExt = (int)SyBlobLength(&pCtx->pVm->sAutoloadExt);` |
|        - | 1459 | `	}` |
|        - | 1460 | `	/* php walks the list as a C string, so a NUL byte in it ends the search. */` |
|       21 | 1461 | `	for( iByte = 0 ; iByte < nExt ; ++iByte ){` |
|       19 | 1462 | `		if( zExt[iByte] == 0 ){` |
|      ! 0 | 1463 | `			nExt = iByte;` |
|      ! 0 | 1464 | `			break;` |
|        - | 1465 | `		}` |
|       10 | 1466 | `	}` |
|        3 | 1467 | `	if( nExt < 1 ){` |
|      ! 0 | 1468 | `		return SXRET_OK;` |
|        - | 1469 | `	}` |
|        3 | 1470 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 1471 | `	/* Iterate over comma-separated extensions */` |
|        3 | 1472 | `	zEnd = zExt + nExt;` |
|        3 | 1473 | `	zCur = zExt;` |
|        7 | 1474 | `	while( zCur < zEnd ){` |
|        - | 1475 | `		const char *zComma;` |
|        - | 1476 | `		SyString sFile;` |
|        - | 1477 | `		int i;` |
|        - | 1478 | `		/* Find next comma or end */` |
|        5 | 1479 | `		zComma = zCur;` |
|       21 | 1480 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 1481 | `			zComma++;` |
|        1 | 1482 | `		}` |
|        - | 1483 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 1484 | `		SyBlobReset(&sPath);` |
|       69 | 1485 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 1486 | `			char c = zClass[i];` |
|       65 | 1487 | `			if( c == '\\' ){` |
|      ! 0 | 1488 | `				c = '/';` |
|       65 | 1489 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 1490 | `				c = c + ('a' - 'A');` |
|        6 | 1491 | `			}` |
|       65 | 1492 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 1493 | `		}` |
|        - | 1494 | `		/* Append extension */` |
|        5 | 1495 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 1496 | `		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,` |
|        - | 1497 | `		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a` |
|        - | 1498 | `		 * terminator, so without this the strlen reads past the buffer (a` |
|        - | 1499 | `		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL` |
|        - | 1500 | `		 * is not counted in SyBlobLength(), so the SyString length stays correct. */` |
|        5 | 1501 | `		SyBlobNullAppend(&sPath);` |
|        - | 1502 | `		/* Try to include the file */` |
|        5 | 1503 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 1504 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 1505 | `		if( rc == SXRET_OK \|\| rc == PH7_EXCEPTION ){` |
|        - | 1506 | `			/* Included — or it threw, which ends the search too: the remaining` |
|        - | 1507 | `			 * extensions are not tried after a file has already run. */` |
|      ! 0 | 1508 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 1509 | `			return rc == PH7_EXCEPTION ? rc : SXRET_OK;` |
|        - | 1510 | `		}` |
|        - | 1511 | `		/* Move past the comma */` |
|        5 | 1512 | `		zCur = zComma;` |
|        5 | 1513 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 1514 | `			zCur++;` |
|        1 | 1515 | `		}` |
|        1 | 1516 | `	}` |
|        3 | 1517 | `	SyBlobRelease(&sPath);` |
|        3 | 1518 | `	return SXRET_OK;` |
|        2 | 1519 | `}` |
|        - | 1520 | `/* Table of built-in VM functions. */` |
|        - | 1521 | `/*` |
|        - | 1522 | ` * Packing body for __call / __callStatic (band A #3b).` |
|        - | 1523 | ` * OP_MEMBER, on a missing or inaccessible method whose class declares the magic` |
|        - | 1524 | ` * handler, stashes {receiver, class, original name} on the VM and marks the callee` |
|        - | 1525 | ` * slot MEMOBJ_AUX_MAGICCALL; the normal OP_CALL machinery then collects the` |
|        - | 1526 | ` * ORIGINAL argument list (incl. spreads) and hands it here, which packs it into a` |
|        - | 1527 | ` * php array and invokes` |
|        - | 1528 | ` *   $recv->__call($name, $args)   /   Class::__callStatic($name, $args)` |
|        - | 1529 | ` * returning the handler's value as the call's result. A throw propagates via the` |
|        - | 1530 | ` * returned status (and the boundary rail).` |
|        - | 1531 | ` *` |
|        - | 1532 | `` * This used to be a REGISTERED host function named `__phl_magic_call` whose name`` |
|        - | 1533 | ` * the four OP_MEMBER sites wrote into the callee slot — so the engine's own` |
|        - | 1534 | ` * dispatch was spelled as a global PHP function that function_exists() and` |
|        - | 1535 | ` * get_defined_functions() both reported, and that any script could call. It is` |
|        - | 1536 | ` * reached through the VM's own function record now (PH7_VmMagicCallFunc); the` |
|        - | 1537 | ` * mark on the slot is the only thing that selects it, and there is no name.` |
|        - | 1538 | ` */` |
|      126 | 1539 | `static int VmMagicCallDispatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 1540 | `{` |
|      129 | 1541 | `	ph7_vm *pVm = pCtx->pVm;` |
|      129 | 1542 | `	ph7_class_instance *pRecv = pVm->pMagicCallThis;` |
|      129 | 1543 | `	ph7_class *pClass = pVm->pMagicCallClass;` |
|        - | 1544 | `	ph7_value sResult;` |
|        - | 1545 | `	SyString sMethName;` |
|        - | 1546 | `	sxi32 rc;` |
|        - | 1547 | `	/* Consume the pending dispatch (one-shot) */` |
|      129 | 1548 | `	pVm->pMagicCallThis = 0;` |
|      129 | 1549 | `	pVm->pMagicCallClass = 0;` |
|      129 | 1550 | `	if( pClass == 0 ){` |
|        - | 1551 | `		/* Defensive: the mark and the latch are set together by OP_MEMBER, so a` |
|        - | 1552 | `		 * classless arrival is unreachable. It was reachable while this body wore a` |
|        - | 1553 | ``		 * function NAME — anyone could call `__phl_magic_call()` — which is exactly`` |
|        - | 1554 | `		 * what the mark retired. */` |
|      ! 0 | 1555 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1556 | `		return PH7_OK;` |
|        - | 1557 | `	}` |
|      129 | 1558 | `	SyStringInitFromBuf(&sMethName,SyBlobData(&pVm->sMagicCallName),SyBlobLength(&pVm->sMagicCallName));` |
|      129 | 1559 | `	PH7_MemObjInit(pVm,&sResult);` |
|        - | 1560 | `	/* The one packing site, shared with every CALLABLE spelling of the same call` |
|        - | 1561 | `	 * (PH7_VmDispatchMagicCall, vm_builtin_call.c): the handler lookup, the $args array` |
|        - | 1562 | `	 * — named arguments keyed by name, as php keys them — and the engine-dispatch` |
|        - | 1563 | `	 * visibility rule all live there. Two copies of this is how the syntax path and the` |
|        - | 1564 | `	 * callable path came to disagree about the argument names in the first place.` |
|        - | 1565 | `	 * SXERR_NOTFOUND is unreachable: OP_MEMBER verified the handler exists. */` |
|      192 | 1566 | `	rc = PH7_VmDispatchMagicCall(pVm,pClass,pRecv,SyStringData(&sMethName),` |
|       63 | 1567 | `		SyStringLength(&sMethName),&sResult,nArg,apArg,pCtx->pArgMap);` |
|      129 | 1568 | `	if( rc == SXRET_OK ){` |
|      123 | 1569 | `		ph7_result_value(pCtx,&sResult);` |
|       60 | 1570 | `	}` |
|      129 | 1571 | `	PH7_MemObjRelease(&sResult);` |
|      129 | 1572 | `	if( pRecv ){` |
|       87 | 1573 | `		PH7_ClassInstanceUnref(pRecv);` |
|       42 | 1574 | `	}` |
|      129 | 1575 | `	SyBlobReset(&pVm->sMagicCallName);` |
|      129 | 1576 | `	return (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) ? rc : PH7_OK;` |
|       66 | 1577 | `}` |
|        - | 1578 | `/*` |
|        - | 1579 | ` * The function record OP_CALL dispatches a MEMOBJ_AUX_MAGICCALL callee slot through,` |
|        - | 1580 | ` * built on first use and owned by the VM (the allocator frees it with everything else).` |
|        - | 1581 | ` *` |
|        - | 1582 | ` * It is deliberately NOT installed in pVm->hHostFunction: the same property that makes` |
|        - | 1583 | ` * a native class method unreachable except by dispatching the method (see` |
|        - | 1584 | ` * PH7_NativeClassInstallMethod) makes this body unreachable except by the engine's own` |
|        - | 1585 | ` * __call routing. It carries no signature and no arity bounds, so the OP_CALL choke` |
|        - | 1586 | ` * point's ZPP and ArgumentCountError screens are inert for it — the handler's OWN` |
|        - | 1587 | ` * declared parameters are what php enforces, and it is a PHP method with a frame of its` |
|        - | 1588 | ` * own. sName is a diagnostic label only; nothing that reads it can be reached from here.` |
|        - | 1589 | ` */` |
|      126 | 1590 | `PH7_PRIVATE ph7_user_func * PH7_VmMagicCallFunc(ph7_vm *pVm)` |
|        3 | 1591 | `{` |
|        - | 1592 | `	SyString sName;` |
|      129 | 1593 | `	if( pVm->pMagicCallFunc == 0 ){` |
|       13 | 1594 | `		SyStringInitFromBuf(&sName,"__call",sizeof("__call")-1);` |
|       15 | 1595 | `		if( PH7_NewForeignFunction(&(*pVm),&sName,VmMagicCallDispatch,0,` |
|       13 | 1596 | `			&pVm->pMagicCallFunc) != SXRET_OK ){` |
|      ! 0 | 1597 | `			pVm->pMagicCallFunc = 0;` |
|      ! 0 | 1598 | `		}` |
|        5 | 1599 | `	}` |
|      129 | 1600 | `	return pVm->pMagicCallFunc;` |
|        3 | 1601 | `}` |
|        - | 1602 |  |
