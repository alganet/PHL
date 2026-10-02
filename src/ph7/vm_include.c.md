# src/ph7/vm_include.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 738/825 lines (89.45%)

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
|        - |    8 | `#include <errno.h>` |
|        - |    9 | `/*` |
|        - |   10 | ` * Section:` |
|        - |   11 | ` *    Dynamic code loading: VmEvalChunk, eval(), the include path` |
|        - |   12 | ` *    machinery, VmExecIncludedFile and the include/require family, the` |
|        - |   13 | ` *    spl_autoload group and the __call/__callStatic packing body.` |
|        - |   14 | ` *    Registration rows stay in vm.c's aVmFunc[].` |
|        - |   15 | ` * Status:` |
|        - |   16 | ` *    Stable.` |
|        - |   17 | ` */` |
|        - |   18 | `/*` |
|        - |   19 | ` * php prints a compile-time FATAL where it is raised, and it is not catchable.` |
|        - |   20 | ` * eval() compiles with the generator's logging OFF -- its PARSE errors are a` |
|        - |   21 | ` * ParseError the caller may catch, and printing them there would double the` |
|        - |   22 | ` * diagnostic -- so a refusal of E_ERROR severity has to reach the screen from` |
|        - |   23 | ` * here instead. Formatted exactly as PH7_GenCompileError would have: the bare` |
|        - |   24 | ` * text, then the file and line of the offence.` |
|        - |   25 | ` */` |
|        2 |   26 | `static void VmReportCompileFatal(ph7_vm *pVm,SyBlob *pMsg,sxu32 nLine)` |
|        1 |   27 | `{` |
|        - |   28 | `	SyBlob sOut;` |
|        3 |   29 | `	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        3 |   30 | `	if( pVm->pEngine->xConf.xErr == 0 \|\| SyBlobLength(pMsg) < 1 ){` |
|      ! 0 |   31 | `		return;` |
|        - |   32 | `	}` |
|        3 |   33 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        3 |   34 | `	SyBlobAppend(&sOut,SyBlobData(pMsg),SyBlobLength(pMsg));` |
|        3 |   35 | `	if( pFile ){` |
|        3 |   36 | `		SyBlobFormat(&sOut," in %.*s on line %u",(int)pFile->nByte,pFile->zString,nLine);` |
|        1 |   37 | `	}` |
|        3 |   38 | `	PH7_GenAppendFatalTrace(&(*pVm),&sOut,PH7_FATAL_TRACE_COMPILE);` |
|        - |   39 | `	/* The label, php's two copies, the error_reporting() gate and` |
|        - |   40 | `	 * error_get_last() are the shared compile-diagnostic emitter's, so eval()'s` |
|        - |   41 | `	 * fatal cannot drift from the compiler's own. E_COMPILE_ERROR is php's bit` |
|        - |   42 | `	 * for it, and the BARE sentence is what pMsg still holds. */` |
|        4 |   43 | `	PH7_VmEmitCompileDiagnostic(&(*pVm),64 /* E_COMPILE_ERROR */,"Fatal error",` |
|        2 |   44 | `		(const char *)SyBlobData(&sOut),SyBlobLength(&sOut),` |
|        2 |   45 | `		(const char *)SyBlobData(pMsg),SyBlobLength(pMsg),nLine);` |
|        3 |   46 | `	SyBlobRelease(&sOut);` |
|        2 |   47 | `}` |
|        - |   48 | `/*` |
|        - |   49 | ` * Record an ACTIVE include/require/eval so a backtrace can show it: php gives each` |
|        - |   50 | ` * one a frame of its own between the loaded unit's frames and the caller's. Nothing` |
|        - |   51 | ` * else in this engine records it -- an include shares its caller's variable scope,` |
|        - |   52 | ` * so it pushes no VmFrame.` |
|        - |   53 | ` *` |
|        - |   54 | ` * pPath is the unit being loaded (php's single argument for the frame) and is empty` |
|        - |   55 | ` * for eval(), which php shows argument-less. The call SITE is read here rather than` |
|        - |   56 | ` * later for the same reason a frame's is (see VmEnterFrame): the include stack moves` |
|        - |   57 | `` * on, and a `try` block's frame carries no function to read a file from.`` |
|        - |   58 | ` */` |
|    21498 |   59 | `PH7_PRIVATE void PH7_VmIncFramePush(ph7_vm *pVm,const char *zName,SyString *pPath)` |
|        5 |   60 | `{` |
|        - |   61 | `	VmIncFrame sInc;` |
|    21503 |   62 | `	VmFrame *pCaller = pVm->pFrame;` |
|        - |   63 | `	SyString *pFile;` |
|    21503 |   64 | `	SyZero(&sInc,sizeof(sInc));` |
|    51646 |   65 | `	while( pCaller && pCaller->pParent` |
|    40902 |   66 | `	    && (pCaller->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|     9729 |   67 | `		pCaller = pCaller->pParent;` |
|        5 |   68 | `	}` |
|    21503 |   69 | `	sInc.pFrame = (void *)pCaller;` |
|    21503 |   70 | `	sInc.nLine = pVm->nCurLine;` |
|    21503 |   71 | `	sInc.zName = zName;` |
|        - |   72 | `	/* The unit this construct is LOADING is already on the include stack by the` |
|        - |   73 | `	 * time we get here, so it must not be mistaken for the one the construct is` |
|        - |   74 | `	 * WRITTEN in: pop it for the question and put it back. */` |
|    21503 |   75 | `	if( pPath ){` |
|    11477 |   76 | `		SyString sTop = *(SyString *)SySetPeek(&pVm->aFiles);` |
|    11477 |   77 | `		(void)SySetPop(&pVm->aFiles);` |
|    11477 |   78 | `		pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|    11477 |   79 | `		if( pFile ){` |
|    11477 |   80 | `			sInc.sFile = *pFile;` |
|     5736 |   81 | `		}` |
|    11477 |   82 | `		SySetPut(&pVm->aFiles,(const void *)&sTop);` |
|     5741 |   83 | `	}else{` |
|    10031 |   84 | `		pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|    10031 |   85 | `		if( pFile ){` |
|    10031 |   86 | `			sInc.sFile = *pFile;` |
|     5013 |   87 | `		}` |
|        - |   88 | `	}` |
|    21503 |   89 | `	if( pPath ){` |
|    11477 |   90 | `		sInc.sPath = *pPath;` |
|     5736 |   91 | `	}` |
|    21503 |   92 | `	SySetPut(&pVm->aIncFrame,(const void *)&sInc);` |
|    21503 |   93 | `}` |
|    21498 |   94 | `PH7_PRIVATE void PH7_VmIncFramePop(ph7_vm *pVm)` |
|        5 |   95 | `{` |
|    21503 |   96 | `	(void)SySetPop(&pVm->aIncFrame);` |
|    21503 |   97 | `}` |
|        - |   98 | `/*` |
|        - |   99 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  100 | ` * Refer to the eval() language construct implementation for more` |
|        - |  101 | ` * information.` |
|        - |  102 | ` */` |
|    29565 |  103 | `PH7_PRIVATE sxi32 VmEvalChunk(` |
|        - |  104 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  105 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  106 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  107 | `	int iFlags,         /* Compile flag */` |
|        - |  108 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  109 | `	)` |
|        5 |  110 | `{` |
|        - |  111 | `	SySet *pByteCode,aByteCode;` |
|    29570 |  112 | `	ProcConsumer xErr = 0;` |
|    29570 |  113 | `	void *pErrData = 0;` |
|        - |  114 | `	ph7_gen_state sSavedGen;` |
|        - |  115 | `	int bNested;` |
|    29570 |  116 | `	sxi32 rcThrow = SXRET_OK; /* a ParseError raised for a failed compile, or a throw the` |
|        - |  117 | `	                           * evaluated chunk raised — either way the caller must unwind */` |
|        - |  118 | `	/* Initialize bytecode container */` |
|    29570 |  119 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    29570 |  120 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  121 | `	/* Reset the code generator */` |
|    29570 |  122 | `	if( bTrueReturn ){` |
|        - |  123 | `		/* Included file,log compile-time errors */` |
|    11619 |  124 | `		xErr = pVm->pEngine->xConf.xErr;` |
|    11619 |  125 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     5807 |  126 | `	}` |
|        - |  127 | `	/* A non-zero cursor means an OUTER compile is in flight — this eval/include` |
|        - |  128 | `	 * was reached from inside it (an autoload fired while resolving a base class).` |
|        - |  129 | `	 * Wiping the generator would corrupt that outer parse, so snapshot it and hand` |
|        - |  130 | `	 * the nested unit a fresh one; a plain runtime eval/include just resets. */` |
|    29570 |  131 | `	bNested = (pVm->sCodeGen.pIn != 0);` |
|    29570 |  132 | `	if( bNested ){` |
|       12 |  133 | `		PH7_CompilerSaveState(pVm,&sSavedGen,xErr,pErrData);` |
|        7 |  134 | `	}else{` |
|    29560 |  135 | `		PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  136 | `	}` |
|        - |  137 | `	/* An include/require's PARSE error is php's catchable ParseError, thrown from` |
|        - |  138 | `	 * the include and printed only if it goes uncaught -- so the generator must not` |
|        - |  139 | `	 * print it. A refusal of E_ERROR severity is php's uncatchable compile fatal and` |
|        - |  140 | `	 * still prints where it is raised. */` |
|    29570 |  141 | `	pVm->sCodeGen.bParseThrows = (bTrueReturn && pCtx) ? 1 : 0;` |
|        - |  142 | `	/* Swap bytecode container */` |
|    29570 |  143 | `	pByteCode = pVm->pByteContainer;` |
|    29570 |  144 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  145 | `	/* Compile the chunk */` |
|    29570 |  146 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|        - |  147 | `	/* Record THIS unit's error count where the nested state restore below cannot` |
|        - |  148 | `	 * wipe it — VmExecDeferredClass reads it to detect a failed re-compile. */` |
|    29570 |  149 | `	pVm->nLastEvalErr = pVm->sCodeGen.nErr;` |
|    44346 |  150 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  151 | `		/* Compilation error. php makes this a CATCHABLE ParseError -- for eval()` |
|        - |  152 | `		 * and for include/require alike -- where PHL merely returned false, so` |
|        - |  153 | ``		 * `eval('bad syntax')` silently produced a value and a broken include`` |
|        - |  154 | `		 * printed its diagnostic and then CARRIED ON, leaving a half-compiled unit` |
|        - |  155 | `		 * behind for later code to trip over. A refusal of E_ERROR severity is` |
|        - |  156 | `		 * php's uncatchable compile fatal instead: it has already printed itself,` |
|        - |  157 | `		 * and the program stops here. */` |
|      115 |  158 | `		SyBlob *pErr = &pVm->sCodeGen.sErrBuf;` |
|      115 |  159 | `		sxu32 nErrLine = pVm->sCodeGen.nFirstErrLine;` |
|      115 |  160 | `		if( SyBlobLength(&pVm->sCodeGen.sFirstErr) > 0 ){` |
|      115 |  161 | `			pErr = &pVm->sCodeGen.sFirstErr;` |
|       55 |  162 | `		}` |
|      115 |  163 | `		if( pVm->sCodeGen.nFatal > 0 ){` |
|       13 |  164 | `			if( pCtx ){` |
|       13 |  165 | `				ph7_result_bool(pCtx,0);` |
|        5 |  166 | `			}` |
|       13 |  167 | `			if( !bTrueReturn ){` |
|        - |  168 | `				/* eval(): the generator had no consumer, so say it here. */` |
|        3 |  169 | `				VmReportCompileFatal(pVm,&pVm->sCodeGen.sFirstErr,nErrLine);` |
|        1 |  170 | `			}` |
|        - |  171 | `			/* php exits 255 and runs nothing else. The include builtins cascade a` |
|        - |  172 | `			 * requested halt exactly as they do for an exit() inside the file. */` |
|       13 |  173 | `			pVm->iExitStatus = 255;` |
|       13 |  174 | `			pVm->bHaltRequested = 1;` |
|       13 |  175 | `			rcThrow = PH7_ABORT;` |
|      110 |  176 | `		}else if( pCtx ){` |
|        - |  177 | `			/* php's ParseError names the OFFENDING file and line, not the line the` |
|        - |  178 | `			 * include was written on -- the throw is stamped from nCurLine, so aim` |
|        - |  179 | `			 * it at the refusal and put the caller's line back afterwards. */` |
|      105 |  180 | `			sxu32 nSaveLine = pVm->nCurLine;` |
|      105 |  181 | `			ph7_result_bool(pCtx,0);` |
|      105 |  182 | `			if( nErrLine > 0 ){` |
|      105 |  183 | `				pVm->nCurLine = nErrLine;` |
|       50 |  184 | `			}` |
|      105 |  185 | `			if( SyBlobLength(pErr) > 0 ){` |
|      155 |  186 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","%.*s",` |
|      100 |  187 | `					(int)SyBlobLength(pErr),(const char *)SyBlobData(pErr));` |
|       55 |  188 | `			}else{` |
|      ! 0 |  189 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","syntax error");` |
|        - |  190 | `			}` |
|      105 |  191 | `			pVm->nCurLine = nSaveLine;` |
|       50 |  192 | `		}` |
|       60 |  193 | `	}else{` |
|        - |  194 | `		/* Mount any newly defined classes. Skipped while the VM is still` |
|        - |  195 | `		 * initializing (builtin chunks): mounting evaluates class-constant/` |
|        - |  196 | `		 * static initializers and installs reference-table entries, and the` |
|        - |  197 | `		 * runtime structures those need (apRefObj, the per-exec object pool` |
|        - |  198 | `		 * baseline) do not exist before PH7_VmMakeReady — which mounts every` |
|        - |  199 | `		 * class unconditionally anyway. */` |
|        - |  200 | `		SyHashEntry *pEntry;` |
|        - |  201 | `		ph7_class *pClass;` |
|        - |  202 | `		ph7_value sResult; /* Return value */` |
|        - |  203 | `		sxi32 rc;` |
|    29460 |  204 | `		if( pVm->nMagic != PH7_VM_INIT ){` |
|    21535 |  205 | `			SyHashResetLoopCursor(&pVm->hClass);` |
| 12176014 |  206 | `			while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
| 12143721 |  207 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  208 | `				/* Only mount classes that haven't been mounted yet */` |
| 12143721 |  209 | `				if( !pClass->bMounted ){` |
|  1000625 |  210 | `					rc = VmMountUserClass(pVm,pClass);` |
|  1000625 |  211 | `					if( rc != SXRET_OK ){` |
|        - |  212 | `						/* Mount failure (likely memory error) */` |
|        3 |  213 | `						if( pCtx ){` |
|        3 |  214 | `							ph7_result_bool(pCtx,0);` |
|        1 |  215 | `						}` |
|        3 |  216 | `						goto Cleanup;` |
|        - |  217 | `					}` |
|   500309 |  218 | `				}` |
|        5 |  219 | `			}` |
|    10764 |  220 | `		}` |
|    29458 |  221 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  222 | `			/* Out of memory */` |
|      ! 0 |  223 | `			if( pCtx ){` |
|      ! 0 |  224 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  225 | `			}` |
|      ! 0 |  226 | `			goto Cleanup;` |
|        - |  227 | `		}` |
|    29458 |  228 | `		if( bTrueReturn ){` |
|        - |  229 | `			/* php's include/require answer INT 1 when the file returned nothing` |
|        - |  230 | ``			 * of its own — not `true`. It is the value a script tests, stores`` |
|        - |  231 | ``			 * and compares, and `include $f === true` was false on php and true`` |
|        - |  232 | `			 * here. */` |
|    11605 |  233 | `			PH7_MemObjInitFromInt(pVm,&sResult,1);` |
|     5805 |  234 | `		}else{` |
|        - |  235 | `			/* Assume a null return value */` |
|    17858 |  236 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  237 | `		}` |
|        - |  238 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here` |
|        - |  239 | `		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by` |
|        - |  240 | `		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the` |
|        - |  241 | `		 * native-nesting fatal instead of overflowing the C stack. The PHP` |
|        - |  242 | `		 * call-depth cap is OP_CALL-only.` |
|        - |  243 | `		 *` |
|        - |  244 | `		 * The nested exec shares the caller's VM frame, so a throw the CALLER's own` |
|        - |  245 | `		 * try catches runs that catch IN PLACE and comes back as a status — which was` |
|        - |  246 | `		 * dropped here, and the statement php abandons then carried on after the catch` |
|        - |  247 | `` 		 * had already run (`try { $r = eval('throw new E;'); echo "x"; } catch …` `` |
|        - |  248 | `		 * printed the catch AND the echo). Same rule and same guard as the match-arm /` |
|        - |  249 | `		 * switch-case / property-default sites: compare the recorded-resume fields` |
|        - |  250 | `		 * against a pre-exec SNAPSHOT, never against 0. */` |
|        - |  251 | `		{` |
|    29458 |  252 | `			const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|    29458 |  253 | `			const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|    29458 |  254 | `			rc = VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    29458 |  255 | `			if( pCtx ){` |
|        - |  256 | `				/* Set the execution result */` |
|    21391 |  257 | `				ph7_result_value(pCtx,&sResult);` |
|    10693 |  258 | `			}` |
|    29458 |  259 | `			PH7_MemObjRelease(&sResult);` |
|    29458 |  260 | `			if( rc != PH7_ABORT && VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){` |
|     3600 |  261 | `				rcThrow = PH7_EXCEPTION;` |
|     1799 |  262 | `			}` |
|        - |  263 | `		}` |
|        - |  264 | `	}` |
|    14788 |  265 | `Cleanup:` |
|        - |  266 | `	/* Cleanup the mess left behind */` |
|    29570 |  267 | `	pVm->pByteContainer = pByteCode;` |
|        - |  268 | `	/* Hand back the call-site cache records this chunk's OP_CALLs claimed, BEFORE the` |
|        - |  269 | `	 * instructions holding their indices disappear. */` |
|    29570 |  270 | `	PH7_VmCallSiteReleaseChunk(pVm,&aByteCode);` |
|    29570 |  271 | `	SySetRelease(&aByteCode);` |
|        - |  272 | `	/* Restore the outer compile's generator state if this was a nested unit. */` |
|    29570 |  273 | `	if( bNested ){` |
|       12 |  274 | `		PH7_CompilerRestoreState(pVm,&sSavedGen);` |
|        5 |  275 | `	}` |
|        - |  276 | `	/* A ParseError raised above must reach the caller so the VM unwinds the rest` |
|        - |  277 | ``	 * of the statement; returning OK left `eval('bad'); echo 'x';` running the`` |
|        - |  278 | `	 * echo even though php had already thrown. */` |
|    29570 |  279 | `	return rcThrow;` |
|        5 |  280 | `}` |
|        - |  281 | `/*` |
|        - |  282 | ` * Execute a deferred class declaration (OP_CLASS_DEFER — see the deferral` |
|        - |  283 | ` * block comment in compile_class.c). At this point the declaration's` |
|        - |  284 | ` * execution order has been honored: any spl_autoload_register() statement` |
|        - |  285 | ` * that precedes it in the program has RUN, so each recorded dependency either` |
|        - |  286 | ` * resolves (possibly by autoload, fired inside PH7_VmExtractClass) or is` |
|        - |  287 | ` * genuinely missing. A missing one is reported through *ppMissing — the` |
|        - |  288 | ` * OP_CLASS_DEFER dispatcher throws php's catchable` |
|        - |  289 | `` * `Class/Interface/Trait "X" not found` Error. Once the dependencies resolve,`` |
|        - |  290 | ` * the captured chunk re-compiles through VmEvalChunk (which also mounts the` |
|        - |  291 | ` * newly installed classes); a compile failure there (e.g. a body syntax error` |
|        - |  292 | ` * whose diagnosis was deferred along with the declaration) has already been` |
|        - |  293 | ` * reported through the engine's error consumer, so it aborts execution.` |
|        - |  294 | ` * The site is idempotent: bDone short-circuits re-execution (a loop around an` |
|        - |  295 | ` * anonymous class instantiates the same installed class, php's` |
|        - |  296 | ` * one-class-per-site semantics).` |
|        - |  297 | ` */` |
|      156 |  298 | `PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing)` |
|        5 |  299 | `{` |
|        - |  300 | `	VmDeferredReq *aReq;` |
|        - |  301 | `	sxu32 n;` |
|      161 |  302 | `	*ppMissing = 0;` |
|      161 |  303 | `	if( pDefer->bDone ){` |
|        5 |  304 | `		return SXRET_OK;` |
|        - |  305 | `	}` |
|      157 |  306 | `	aReq = (VmDeferredReq *)SySetBasePtr(&pDefer->aRequired);` |
|      241 |  307 | `	for( n = 0 ; n < SySetUsed(&pDefer->aRequired) ; ++n ){` |
|       98 |  308 | `		if( PH7_VmExtractClass(&(*pVm),aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|       11 |  309 | `			*ppMissing = &aReq[n];` |
|       11 |  310 | `			return SXRET_OK; /* caller throws */` |
|        - |  311 | `		}` |
|       46 |  312 | `	}` |
|      147 |  313 | `	if( pDefer->sAnonName.nByte > 0 ){` |
|        8 |  314 | `		pVm->sDeferAnonName = pDefer->sAnonName;` |
|        3 |  315 | `	}` |
|      147 |  316 | `	VmEvalChunk(&(*pVm),0,&pDefer->sText,PH7_PHP_ONLY,TRUE);` |
|      147 |  317 | `	pVm->sDeferAnonName.zString = 0;` |
|      147 |  318 | `	pVm->sDeferAnonName.nByte = 0;` |
|      142 |  319 | `	if( pVm->nLastEvalErr > 0` |
|      147 |  320 | `	 \|\| PH7_VmExtractClass(&(*pVm),pDefer->sSelfName.zString,pDefer->sSelfName.nByte,FALSE,0) == 0 ){` |
|        - |  321 | `		/* The re-compile failed — a deferred-along syntax error or a` |
|        - |  322 | `		 * redeclaration fatal. It was already reported through the engine's` |
|        - |  323 | `		 * error consumer; halt like a fatal (php exits 255 for both). */` |
|      ! 0 |  324 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  325 | `		return SXERR_ABORT;` |
|        - |  326 | `	}` |
|      147 |  327 | `	pDefer->bDone = 1;` |
|      147 |  328 | `	return SXRET_OK;` |
|       83 |  329 | `}` |
|        - |  330 | `/*` |
|        - |  331 | ` * php names an eval()'d compilation unit after the SITE that evaluated it:` |
|        - |  332 | `` * `<file>(<line>) : eval()'d code`, where <file> is the unit the eval() was`` |
|        - |  333 | ` * written in -- itself such a name when the eval is nested -- and <line> the line` |
|        - |  334 | ` * it sits on. That name is not a decoration on one message: it is the unit's` |
|        - |  335 | `` * identity, so `__FILE__`, every diagnostic's location, a Throwable's getFile(),`` |
|        - |  336 | ` * and ReflectionFunction/ReflectionClass::getFileName() for anything the chunk` |
|        - |  337 | ` * declares all read it. PHL left the chunk sharing its caller's name, so a` |
|        - |  338 | ` * template engine that compiles to PHP and evals it (twig, and every cache-less` |
|        - |  339 | ` * renderer of that shape) reported positions in the COMPILER's file -- and the` |
|        - |  340 | ` * caller could not map them back, because its own class had the compiler's name` |
|        - |  341 | ` * on it too.` |
|        - |  342 | ` *` |
|        - |  343 | ` * Interned per site rather than per call: the name is copied by value into every` |
|        - |  344 | ` * function and class record compiled out of the chunk, so it must outlive the` |
|        - |  345 | ` * eval, and an eval inside a loop repeats one site.` |
|        - |  346 | ` */` |
|    10026 |  347 | `static sxi32 VmEvalUnitName(ph7_vm *pVm,SyString *pOut)` |
|        5 |  348 | `{` |
|    10031 |  349 | `	SyString *pHost = PH7_VmExecutingUnitFile(&(*pVm));` |
|        - |  350 | `	SyString *aName;` |
|        - |  351 | `	SyString sKey;` |
|        - |  352 | `	SyBlob sName;` |
|        - |  353 | `	char *zDup;` |
|        - |  354 | `	sxu32 n;` |
|    10031 |  355 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|    10031 |  356 | `	if( pHost && pHost->nByte > 0 ){` |
|    10031 |  357 | `		SyBlobAppend(&sName,pHost->zString,pHost->nByte);` |
|     5013 |  358 | `	}` |
|    10031 |  359 | `	SyBlobFormat(&sName,"(%u) : eval()'d code",pVm->nCurLine);` |
|    10031 |  360 | `	if( SyBlobLength(&sName) < 1 ){` |
|      ! 0 |  361 | `		SyBlobRelease(&sName);` |
|      ! 0 |  362 | `		return SXERR_MEM;` |
|        - |  363 | `	}` |
|    10031 |  364 | `	SyStringInitFromBuf(&sKey,SyBlobData(&sName),SyBlobLength(&sName));` |
|    10031 |  365 | `	aName = (SyString *)SySetBasePtr(&pVm->aEvalFile);` |
|   181439 |  366 | `	for( n = 0 ; n < SySetUsed(&pVm->aEvalFile) ; ++n ){` |
|   181264 |  367 | `		if( SyStringCmp(&sKey,&aName[n],SyMemcmp) == 0 ){` |
|     9855 |  368 | `			*pOut = aName[n];` |
|     9855 |  369 | `			SyBlobRelease(&sName);` |
|     9855 |  370 | `			return SXRET_OK;` |
|        - |  371 | `		}` |
|    85708 |  372 | `	}` |
|      179 |  373 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,sKey.zString,sKey.nByte);` |
|      179 |  374 | `	n = sKey.nByte;` |
|      179 |  375 | `	SyBlobRelease(&sName);` |
|      179 |  376 | `	if( zDup == 0 ){` |
|      ! 0 |  377 | `		return SXERR_MEM;` |
|        - |  378 | `	}` |
|      179 |  379 | `	SyStringInitFromBuf(&sKey,zDup,n);` |
|      179 |  380 | `	if( SySetPut(&pVm->aEvalFile,(const void *)&sKey) != SXRET_OK ){` |
|      ! 0 |  381 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  382 | `		return SXERR_MEM;` |
|        - |  383 | `	}` |
|      179 |  384 | `	*pOut = sKey;` |
|      179 |  385 | `	return SXRET_OK;` |
|     5018 |  386 | `}` |
|        - |  387 | `/*` |
|        - |  388 | ` * value eval(string $code)` |
|        - |  389 | ` *   Evaluate a string as PHP code.` |
|        - |  390 | ` * Parameter` |
|        - |  391 | ` *  code: PHP code to evaluate.` |
|        - |  392 | ` * Return` |
|        - |  393 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  394 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  395 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  396 | ` */` |
|    10032 |  397 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  398 | `{` |
|        - |  399 | `	SyString sChunk;    /* Chunk to evaluate */` |
|        - |  400 | `	sxi32 rc;` |
|    10037 |  401 | `	if( nArg < 1 ){` |
|        - |  402 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  403 | `		ph7_result_null(pCtx);` |
|      ! 0 |  404 | `		return SXRET_OK;` |
|        - |  405 | `	}` |
|        - |  406 | `	/* Chunk to evaluate. Coerced USER-VISIBLY like every other string argument` |
|        - |  407 | `	 * spelled as a language construct: an object with no __toString() is php's` |
|        - |  408 | `	 * Error, where PHL used to hand the parser the literal "Object" and answer a` |
|        - |  409 | `	 * ParseError. */` |
|        - |  410 | `	{` |
|    10037 |  411 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sChunk.zString,(int *)&sChunk.nByte);` |
|    10037 |  412 | `		if( rcSv != SXRET_OK ){` |
|        3 |  413 | `			return rcSv;` |
|        - |  414 | `		}` |
|        - |  415 | `	}` |
|    10035 |  416 | `	if( sChunk.nByte < 1 ){` |
|        - |  417 | `		/* php: eval('') compiles nothing and yields FALSE (a whitespace-only` |
|        - |  418 | `		 * chunk still compiles, and yields NULL through the normal path). */` |
|        5 |  419 | `		ph7_result_bool(pCtx,0);` |
|        5 |  420 | `		return SXRET_OK;` |
|        - |  421 | `	}` |
|        - |  422 | `	/* Eval the chunk.` |
|        - |  423 | `	 *` |
|        - |  424 | `` 	 * php compiles it as though it began right after a `<?php`, which means a `?>` `` |
|        - |  425 | `` 	 * inside it LEAVES php mode: the text after it is echoed and a later `<?php` `` |
|        - |  426 | ``	 * re-enters. `eval('?>' . file_get_contents($f))` is the ordinary way to run a`` |
|        - |  427 | `` 	 * php FILE's bytes, and composer reloads its own `vendor/composer/installed.php` `` |
|        - |  428 | `	 * exactly that way. PHL handed the whole chunk to the compiler as ONE php token` |
|        - |  429 | ``	 * (PH7_PHP_ONLY), so the `?>` was a syntax error and composer could not boot.`` |
|        - |  430 | `	 *` |
|        - |  431 | `	 * Prepending the opening tag and letting the ordinary raw tokenizer split it is` |
|        - |  432 | `	 * php's rule itself, with no second implementation of it. The prefix carries no` |
|        - |  433 | `	 * newline, so every line still reports the number the caller wrote it on. */` |
|        - |  434 | `	{` |
|        - |  435 | `		SyBlob sTagged;` |
|        - |  436 | `		SyString sTaggedStr;` |
|        - |  437 | `		SyString sUnit;` |
|        - |  438 | `		int bUnit;` |
|    10031 |  439 | `		SyBlobInit(&sTagged,&pCtx->pVm->sAllocator);` |
|    10026 |  440 | `		if( SyBlobAppend(&sTagged,"<?php ",sizeof("<?php ")-1) != SXRET_OK` |
|    10031 |  441 | `		 \|\| SyBlobAppend(&sTagged,sChunk.zString,sChunk.nByte) != SXRET_OK ){` |
|      ! 0 |  442 | `			SyBlobRelease(&sTagged);` |
|      ! 0 |  443 | `			return PH7_ContextMemoryError(pCtx);` |
|        - |  444 | `		}` |
|    10031 |  445 | `		SyStringInitFromBuf(&sTaggedStr,SyBlobData(&sTagged),SyBlobLength(&sTagged));` |
|        - |  446 | `		/* php gives eval() a trace frame of its own, exactly as it does an include --` |
|        - |  447 | `		 * argument-less, where an include names the unit it loaded. */` |
|        - |  448 | `		/* Name the chunk BEFORE the trace frame goes on: PH7_VmIncFramePush records` |
|        - |  449 | `		 * an entry whose pFrame is the CURRENT frame, and PH7_VmExecutingUnitFile` |
|        - |  450 | `		 * answers such a frame from the include-stack TOP -- so asking after the push` |
|        - |  451 | `		 * names the unit being loaded rather than the one holding the eval(), and a` |
|        - |  452 | `		 * template engine's compiled chunk came out named after the SCRIPT instead of` |
|        - |  453 | `		 * the library method that evaluated it. */` |
|    10031 |  454 | `		bUnit = (VmEvalUnitName(pCtx->pVm,&sUnit) == SXRET_OK);` |
|    10031 |  455 | `		PH7_VmIncFramePush(pCtx->pVm,"eval",0);` |
|        - |  456 | `		/* Now push the chunk's own unit name. The include-frame entry is what makes` |
|        - |  457 | `		 * the include stack answer for code running at the chunk's TOP level -- which` |
|        - |  458 | `		 * pushes no VmFrame of its own -- so the two go together. */` |
|    10031 |  459 | `		bUnit = bUnit && (SySetPut(&pCtx->pVm->aFiles,(const void *)&sUnit) == SXRET_OK);` |
|    10031 |  460 | `		rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sTaggedStr,0,FALSE);` |
|    10031 |  461 | `		if( bUnit ){` |
|    10031 |  462 | `			(void)SySetPop(&pCtx->pVm->aFiles);` |
|     5013 |  463 | `		}` |
|    10031 |  464 | `		PH7_VmIncFramePop(pCtx->pVm);` |
|    10031 |  465 | `		SyBlobRelease(&sTagged);` |
|        - |  466 | `	}` |
|    10031 |  467 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  468 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       71 |  469 | `		return PH7_ABORT;` |
|        - |  470 | `	}` |
|        - |  471 | `	/* Propagate a ParseError from a failed compile (php unwinds; PHL used to` |
|        - |  472 | `	 * keep executing the statement that contained the eval). */` |
|     9961 |  473 | `	return rc;` |
|     5021 |  474 | `}` |
|        - |  475 | `/*` |
|        - |  476 | ` * Check if a file path is already included.` |
|        - |  477 | ` */` |
|    11510 |  478 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        5 |  479 | `{` |
|        - |  480 | `	SyString *aEntries;` |
|        - |  481 | `	sxu32 n;` |
|    11515 |  482 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  483 | `	/* Perform a linear search */` |
| 31655709 |  484 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 31644269 |  485 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  486 | `			/* Already included */` |
|       75 |  487 | `			return TRUE;` |
|        - |  488 | `		}` |
| 15822102 |  489 | `	}` |
|    11445 |  490 | `	return FALSE;` |
|     5760 |  491 | `}` |
|        - |  492 | `/*` |
|        - |  493 | ` * Push a file path in the appropriate VM container.` |
|        - |  494 | ` */` |
|    19435 |  495 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 |  496 | `{` |
|        - |  497 | `	SyString sPath;` |
|        - |  498 | `	char *zDup;` |
|        - |  499 | `	sxi32 rc;` |
|    19440 |  500 | `	if( nLen < 0 ){` |
|     7930 |  501 | `		nLen = SyStrlen(zPath);` |
|     3957 |  502 | `	}` |
|        - |  503 | `	/* Duplicate the file path first */` |
|    19440 |  504 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    19440 |  505 | `	if( zDup == 0 ){` |
|      ! 0 |  506 | `		return SXERR_MEM;` |
|        - |  507 | `	}` |
|        - |  508 | ``	/* php's lint mode (`phl -l`) hands the compiler the file handle it opened`` |
|        - |  509 | `	 * under the name it was GIVEN -- it never runs the unit, so nothing needs the` |
|        - |  510 | `	 * canonical name -- where a run expands the main script's path first. That is` |
|        - |  511 | ``	 * why a parse error names `./x.php` under `-l` and `/abs/dir/x.php` under a`` |
|        - |  512 | `	 * run, and it is the file name a lint diagnostic is matched against.` |
|        - |  513 | `	 * Only the MAIN unit: an include compiled from a checked file would still` |
|        - |  514 | `	 * want the canonical name, and lint mode compiles no includes anyway. */` |
|    19440 |  515 | `	if( bMain && pVm->bSyntaxCheck ){` |
|      354 |  516 | `		goto Install;` |
|        - |  517 | `	}` |
|        - |  518 | `#ifdef __UNIXES__` |
|        - |  519 | `	/* php records the REALPATH'd absolute path for __FILE__/__DIR__/getFileName` |
|        - |  520 | `	 * and include-once dedup: realpath() resolves '.', '..' and symlinks (e.g.` |
|        - |  521 | `	 * macOS /tmp -> /private/tmp). It needs an existing file, so on failure keep` |
|        - |  522 | `	 * the raw path (eval'd code, php:///data:// wrappers, a missing include that` |
|        - |  523 | `	 * errors elsewhere). */` |
|        - |  524 | `	{` |
|    19085 |  525 | `		char *zReal = realpath(zDup,0); /* POSIX: malloc'd result */` |
|    19085 |  526 | `		if( zReal ){` |
|    18457 |  527 | `			sxu32 nReal = SyStrlen(zReal);` |
|    18457 |  528 | `			char *zRealDup = SyMemBackendStrDup(&pVm->sAllocator,zReal,nReal);` |
|    18457 |  529 | `			free(zReal);` |
|    18457 |  530 | `			if( zRealDup ){` |
|    18457 |  531 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|    18457 |  532 | `				zDup = zRealDup;` |
|    18457 |  533 | `				nLen = (int)nReal;` |
|     9223 |  534 | `			}` |
|     9223 |  535 | `		}` |
|        - |  536 | `	}` |
|        - |  537 | `#endif` |
|        - |  538 | `#ifdef __WINNT__` |
|        - |  539 | `	/* Windows counterpart of the realpath() above: canonicalize to an absolute` |
|        - |  540 | `	 * path (resolves '.'/'..' , '/' -> '\'), case-preserved to match php — NOT` |
|        - |  541 | `	 * lowercased as the legacy PH7 code did. Like the POSIX branch, keep the raw` |
|        - |  542 | `	 * path when it does not name an existing file (the "Command line code" marker,` |
|        - |  543 | `	 * eval'd code, stream wrappers). _fullpath() is lexical, so pair it with a VFS` |
|        - |  544 | `	 * existence check. */` |
|        - |  545 | `	{` |
|        5 |  546 | `		char *zFull = _fullpath(0,zDup,0); /* MSVC CRT: malloc'd absolute path */` |
|        5 |  547 | `		if( zFull ){` |
|        5 |  548 | `			if( pVm->pEngine->pVfs && pVm->pEngine->pVfs->xFileExists && pVm->pEngine->pVfs->xFileExists(zFull) == PH7_OK ){` |
|        5 |  549 | `				sxu32 nFull = SyStrlen(zFull);` |
|        5 |  550 | `				char *zFullDup = SyMemBackendStrDup(&pVm->sAllocator,zFull,nFull);` |
|        5 |  551 | `				if( zFullDup ){` |
|        5 |  552 | `					SyMemBackendFree(&pVm->sAllocator,zDup);` |
|        5 |  553 | `					zDup = zFullDup;` |
|        5 |  554 | `					nLen = (int)nFull;` |
|        - |  555 | `				}` |
|        - |  556 | `			}` |
|        5 |  557 | `			free(zFull);` |
|        - |  558 | `		}` |
|        - |  559 | `	}` |
|        - |  560 | `#endif` |
|        - |  561 | ``	/* A `phar://` url has no realpath() to ask, so its own canonical form is`` |
|        - |  562 | `	 * built here: the archive half as the url spells it, the ENTRY half with its` |
|        - |  563 | ``	 * `.` and `..` segments collapsed the way the archive's reader already`` |
|        - |  564 | `	 * resolves them. Without it two spellings of one entry are two rows in the` |
|        - |  565 | `	 * once-registry.` |
|        - |  566 | `	 *` |
|        - |  567 | `	 * Behind the same guard as ext/phar itself: vm_phar.c's whole body, and the` |
|        - |  568 | ``	 * prototype block this calls into, are `#ifndef PH7_DISABLE_BUILTIN_FUNC`, so`` |
|        - |  569 | `	 * the tiny build has no archive reader to ask and no phar:// url to ask about. */` |
|        - |  570 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  571 | `	{` |
|        - |  572 | `		SyBlob sPhar;` |
|    19090 |  573 | `		SyBlobInit(&sPhar,&pVm->sAllocator);` |
|    19090 |  574 | `		if( PH7_PharCanonicalUrl(pVm,zDup,nLen,&sPhar) ){` |
|       30 |  575 | `			char *zPharDup = SyMemBackendStrDup(&pVm->sAllocator,` |
|       20 |  576 | `				(const char *)SyBlobData(&sPhar),SyBlobLength(&sPhar));` |
|       20 |  577 | `			if( zPharDup ){` |
|       20 |  578 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       20 |  579 | `				zDup = zPharDup;` |
|       20 |  580 | `				nLen = (int)SyBlobLength(&sPhar);` |
|       10 |  581 | `			}` |
|       10 |  582 | `		}` |
|    19090 |  583 | `		SyBlobRelease(&sPhar);` |
|     9537 |  584 | `	}` |
|        - |  585 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9723 |  586 | `Install:` |
|        - |  587 | `	/* Install the file path */` |
|    19440 |  588 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    19440 |  589 | `	if( !bMain ){` |
|    11515 |  590 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  591 | `			/* Already included */` |
|       75 |  592 | `			*pNew = 0;` |
|       40 |  593 | `		}else{` |
|        - |  594 | `			/* Insert in the corresponding container */` |
|    11445 |  595 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    11445 |  596 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  597 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  598 | `				return rc;` |
|        - |  599 | `			}` |
|    11445 |  600 | `			*pNew = 1;` |
|        - |  601 | `		}` |
|     5755 |  602 | `	}` |
|    19440 |  603 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    19440 |  604 | `	return SXRET_OK;` |
|     9717 |  605 | `}` |
|        - |  606 | `/*` |
|        - |  607 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  608 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  609 | ` * indicates failure.` |
|        - |  610 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  611 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  612 | ` * operations.` |
|        - |  613 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  614 | ` * this function is a no-op.` |
|        - |  615 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  616 | ` * constructs for more information.` |
|        - |  617 | ` */` |
|    11548 |  618 | `static sxi32 VmExecIncludedFile(` |
|        - |  619 | `	 ph7_context *pCtx,   /* Call Context */` |
|        - |  620 | `	 SyString *pPath,     /* Script path or URL*/` |
|        - |  621 | `	 int IncludeOnce,     /* TRUE if called from include_once() or require_once() */` |
|        - |  622 | `	 const char **pzWhy   /* OUT: php's reason for a failed open, for the caller's warning */` |
|        - |  623 | `	 )` |
|        5 |  624 | `{` |
|        - |  625 | `	sxi32 rc;` |
|    11553 |  626 | `	if( pzWhy ){` |
|    11549 |  627 | `		*pzWhy = "operation failed";` |
|     5772 |  628 | `	}` |
|        - |  629 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  630 | `	const ph7_io_stream *pStream;` |
|        - |  631 | `	SyBlob sContents;` |
|        - |  632 | `	void *pHandle;` |
|        - |  633 | `	ph7_vm *pVm;` |
|        - |  634 | `	int isNew;` |
|        - |  635 | `	int bFellBack;` |
|        - |  636 | `	/* Initialize fields */` |
|    11553 |  637 | `	pVm = pCtx->pVm;` |
|    11553 |  638 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|    11553 |  639 | `	isNew = 0;` |
|    11553 |  640 | `	bFellBack = 0;` |
|        - |  641 | `	/* Extract the associated stream. The lookup ADVANCES the pointer past the` |
|        - |  642 | `	 * scheme, so it walks a copy: advancing the caller's SyString left its nByte` |
|        - |  643 | `	 * describing the whole url and its zString seven bytes in, and the failure` |
|        - |  644 | `	 * message below then printed that many bytes from there -- past the end of the` |
|        - |  645 | ``	 * string, so `include 'file:///nope'` reported whatever followed it in memory. */`` |
|        - |  646 | `	{` |
|    11553 |  647 | `		const char *zOpen = pPath->zString;` |
|    11553 |  648 | `		pStream = PH7_VmGetStreamDevice(pVm,&zOpen,(int)pPath->nByte);` |
|    11553 |  649 | `		if( pStream == 0 ){` |
|        - |  650 | `			/* A scheme NOBODY is registered under is not a refusal in php: its` |
|        - |  651 | `			 * lookup warns, forgets the protocol, and hands the WHOLE uri --` |
|        - |  652 | `			 * scheme and all -- to the plain-files wrapper, which resolves it` |
|        - |  653 | `			 * against the include_path like any other relative name. So` |
|        - |  654 | ``			 * `include 'zzz://hit.php'` warns once and then RUNS ./zzz:/hit.php,`` |
|        - |  655 | ``			 * where this engine reported `Invalid argument` and included`` |
|        - |  656 | `			 * nothing. The two cases php words differently are excluded here and` |
|        - |  657 | ``			 * keep the failing path below: a `file://` with an authority php`` |
|        - |  658 | ``			 * will not reach, and a `file://` the configuration switched off. */`` |
|       19 |  659 | `			int nScheme = 0;` |
|       19 |  660 | `			if( PH7_VmStreamDeviceIsRemoteHost(pPath->zString,(int)pPath->nByte,&nScheme) ){` |
|        - |  661 | ``				/* `file://host/path`. A wrapper WAS found and declined the name,`` |
|        - |  662 | `				 * so php says so in its own sentence and gives the open a reason` |
|        - |  663 | `				 * of the lookup's rather than an errno nothing set. The failed` |
|        - |  664 | `				 * open's own line is the caller's, which is why only the first` |
|        - |  665 | `				 * half is raised here. */` |
|        4 |  666 | `				PH7_VmThrowWarningFmt(pVm,"%s(): Remote host file access not supported, %.*s",` |
|        2 |  667 | `					ph7_function_name(pCtx),(int)pPath->nByte,pPath->zString);` |
|        3 |  668 | `				if( pzWhy ){` |
|        3 |  669 | `					*pzWhy = "no suitable wrapper could be found";` |
|        1 |  670 | `				}` |
|        3 |  671 | `				return SXERR_IO;` |
|        - |  672 | `			}` |
|       16 |  673 | `			if( nScheme > 0` |
|       17 |  674 | `			 && !PH7_VmStreamSchemeDisabled(pVm,"file",(int)sizeof("file")-1) ){` |
|        - |  675 | `				/* One sentence per wrapper lookup that still SEES the scheme.` |
|        - |  676 | `				 * php resolves the path before it opens it -- and the _once` |
|        - |  677 | `				 * forms resolve it once more, to answer whether it has already` |
|        - |  678 | `				 * been included -- so a miss costs two sentences and a _once` |
|        - |  679 | `				 * miss three. A resolve that succeeds hands an absolute plain` |
|        - |  680 | `				 * path to everything after it, so a hit costs exactly one` |
|        - |  681 | `				 * whichever construct asked. */` |
|       17 |  682 | `				VfsThrowUnknownWrapperWarning(pCtx,pPath->zString);` |
|       17 |  683 | `				pStream = PH7_VmFindStreamDevice(pVm,"file",(int)sizeof("file")-1);` |
|       17 |  684 | `				zOpen = pPath->zString;` |
|       17 |  685 | `				bFellBack = 1;` |
|        8 |  686 | `			}` |
|        8 |  687 | `		}` |
|        - |  688 | `		/*` |
|        - |  689 | `		 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  690 | `		 * in a read-only mode.` |
|        - |  691 | `		 */` |
|    11551 |  692 | `		pHandle = PH7_StreamOpenHandle(pVm,pStream,zOpen,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew,ph7_function_name(pCtx));` |
|        - |  693 | `	}` |
|    11551 |  694 | `	if( pHandle == 0 ){` |
|        - |  695 | `		/* The reason belongs to THIS open and nothing else: read it before any` |
|        - |  696 | `		 * other stream operation can re-arm it. A wrapper that logged one of its` |
|        - |  697 | `		 * own wins; the plain-file wrapper logs none and reports its errno. */` |
|       39 |  698 | `		if( bFellBack ){` |
|        - |  699 | `			/* The lookups the failed resolve above did not spare: the open's` |
|        - |  700 | `			 * own, plus the _once forms' extra resolve. And the reason is the` |
|        - |  701 | `			 * plain-files wrapper's, not the scheme's -- php has stopped talking` |
|        - |  702 | `			 * about the wrapper by the time it words the failure. */` |
|       11 |  703 | `			VfsThrowUnknownWrapperWarning(pCtx,pPath->zString);` |
|       11 |  704 | `			if( IncludeOnce ){` |
|        5 |  705 | `				VfsThrowUnknownWrapperWarning(pCtx,pPath->zString);` |
|        2 |  706 | `			}` |
|       11 |  707 | `			if( pzWhy ){` |
|       11 |  708 | `				*pzWhy = PH7_VfsOpenStrerror(ENOENT);` |
|        5 |  709 | `			}` |
|       11 |  710 | `			return SXERR_IO;` |
|        - |  711 | `		}` |
|       29 |  712 | `		if( pzWhy ){` |
|       24 |  713 | `			*pzWhy = pVm->zOpenErr ? pVm->zOpenErr : PH7_VfsOpenStrerror(errno);` |
|       11 |  714 | `		}` |
|       29 |  715 | `		return SXERR_IO;` |
|        - |  716 | `	}` |
|    11515 |  717 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|    11515 |  718 | `	if( IncludeOnce && !isNew ){` |
|        - |  719 | `		/* Already included */` |
|       42 |  720 | `		rc = SXERR_EXISTS;` |
|       23 |  721 | `	}else{` |
|        - |  722 | `		/* Read the whole file contents */` |
|    11477 |  723 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|    11477 |  724 | `		if( rc != SXRET_OK ){` |
|        - |  725 | `			/* php refuses a target that is not a REGULAR file -- a directory is` |
|        - |  726 | `			 * the one every script meets -- inside the open itself, so what a` |
|        - |  727 | `			 * script reads is the open's generic reason and not the read's` |
|        - |  728 | ``			 * errno. `include '/etc'` says "No such file or directory" there`` |
|        - |  729 | `			 * and "Is a directory" here. */` |
|      ! 0 |  730 | `			if( pzWhy ){` |
|      ! 0 |  731 | `				*pzWhy = PH7_VfsOpenStrerror(ENOENT);` |
|      ! 0 |  732 | `			}` |
|      ! 0 |  733 | `		}` |
|    11477 |  734 | `		if( rc == SXRET_OK ){` |
|        - |  735 | `			SyString sScript;` |
|        - |  736 | `			/* Compile and execute the script. A throw the included file raised — and the` |
|        - |  737 | `			 * INCLUDING statement's own try caught in place — comes back as PH7_EXCEPTION` |
|        - |  738 | `			 * and travels out to the include builtin's caller, which unwinds the rest of` |
|        - |  739 | `			 * the statement instead of resuming it. It is not an IO failure, so the` |
|        - |  740 | `			 * callers must tell the two apart before warning. */` |
|    11477 |  741 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|        - |  742 | `			/* php shows the construct itself as a trace frame; the path it names is` |
|        - |  743 | `			 * the RESOLVED one, which is what aFiles was just given. */` |
|    17213 |  744 | `			PH7_VmIncFramePush(pVm,ph7_function_name(pCtx),` |
|    11472 |  745 | `				(SyString *)SySetPeek(&pVm->aFiles));` |
|    11477 |  746 | `			rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|    11477 |  747 | `			PH7_VmIncFramePop(pVm);` |
|    11477 |  748 | `			if( rc != PH7_EXCEPTION ){` |
|    11463 |  749 | `				rc = SXRET_OK;` |
|     5729 |  750 | `			}` |
|     5736 |  751 | `		}` |
|        - |  752 | `	}` |
|        - |  753 | `	/* Pop from the set of included file */` |
|    11515 |  754 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  755 | `	/* Close the handle */` |
|    11515 |  756 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  757 | `	/* Release the working buffer */` |
|    11515 |  758 | `	SyBlobRelease(&sContents);` |
|        - |  759 | `#else` |
|        - |  760 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  761 | `	SXUNUSED(pPath);` |
|        - |  762 | `	SXUNUSED(IncludeOnce);` |
|        - |  763 | `	rc = SXERR_IO;` |
|        - |  764 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    11515 |  765 | `	return rc;` |
|     5779 |  766 | `}` |
|        - |  767 | `/*` |
|        - |  768 | ` * php keeps include_path in ONE place -- the INI table -- and get_include_path(),` |
|        - |  769 | ` * ini_get('include_path'), ini_get_all() and the resolver all read that one string.` |
|        - |  770 | ` * PHL had TWO: pVm->aPaths, which is what the include walk actually uses and which` |
|        - |  771 | `` * only set_include_path() ever wrote, and the `include_path` INI slot, which is what`` |
|        - |  772 | ` * ini_get() answers and which only ini_set()/-d ever wrote. Neither told the other,` |
|        - |  773 | `` * so `ini_set('include_path', $dir)` -- the ordinary way a bootstrap file points the`` |
|        - |  774 | `` * engine at a library -- moved NOTHING, and `set_include_path($dir)` left ini_get()`` |
|        - |  775 | ` * naming the value that was no longer in force. The set below is the single store;` |
|        - |  776 | ` * the INI slot is a view of it (vm_builtin_ini.c).` |
|        - |  777 | ` */` |
|      162 |  778 | `PH7_PRIVATE int PH7_VmIncludePathSep(void)` |
|        4 |  779 | `{` |
|        - |  780 | `#ifdef __WINNT__` |
|        4 |  781 | `	return ';';` |
|        - |  782 | `#else` |
|        - |  783 | `	/* Assume UNIX path separator */` |
|      162 |  784 | `	return ':';` |
|        - |  785 | `#endif` |
|        4 |  786 | `}` |
|        - |  787 | `/*` |
|        - |  788 | ` * Split an include_path STRING into its segments and make them the VM's set.` |
|        - |  789 | ` *` |
|        - |  790 | ` * Segments are kept VERBATIM. php hands back the string it was given, so a` |
|        - |  791 | ` * trailing slash, a leading space and an EMPTY segment all survive the round trip` |
|        - |  792 | ` * -- and the walk means something for each of them: php builds "<segment>/<file>"` |
|        - |  793 | ` * from every entry, which is how an empty segment comes to try "/file".` |
|        - |  794 | ` */` |
|       30 |  795 | `PH7_PRIVATE void PH7_VmSetIncludePath(ph7_vm *pVm,const char *zPath,sxu32 nByte)` |
|        2 |  796 | `{` |
|        - |  797 | `	const char *z,*zEnd,*zDup;` |
|       32 |  798 | `	int dir_sep = PH7_VmIncludePathSep();` |
|       32 |  799 | `	SySetReset(&pVm->aPaths);` |
|       32 |  800 | `	if( nByte < 1 ){` |
|      ! 0 |  801 | `		return;` |
|        - |  802 | `	}` |
|        - |  803 | `	/* ONE VM-lifetime copy backs every segment (the SyString entries alias it),` |
|        - |  804 | `	 * where the old splitter duped each segment separately on every call. */` |
|       32 |  805 | `	zDup = (const char *)SyMemBackendDup(&pVm->sAllocator,zPath,nByte);` |
|       32 |  806 | `	if( zDup == 0 ){` |
|      ! 0 |  807 | `		return;` |
|        - |  808 | `	}` |
|       32 |  809 | `	z = zDup;` |
|       32 |  810 | `	zEnd = &zDup[nByte];` |
|       23 |  811 | `	for(;;){` |
|       40 |  812 | `		const char *zStart = z;` |
|        - |  813 | `		SyString sPath;` |
|      349 |  814 | `		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }` |
|       40 |  815 | `		SyStringInitFromBuf(&sPath,zStart,(sxu32)(z-zStart));` |
|       40 |  816 | `		SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|       40 |  817 | `		if( z >= zEnd ){` |
|       32 |  818 | `			break;` |
|        - |  819 | `		}` |
|       10 |  820 | `		z++; /* skip the separator */` |
|        2 |  821 | `	}` |
|       17 |  822 | `}` |
|        - |  823 | `/*` |
|        - |  824 | ` * php's LAST RESORT for a relative name the include_path did not answer: the` |
|        - |  825 | ` * directory of the file that is EXECUTING, not the process's cwd. It is why` |
|        - |  826 | `` * `include 'helper.php'` next to the script keeps working when the script was`` |
|        - |  827 | ` * started from somewhere else, and why a library's own relative includes` |
|        - |  828 | ` * resolve at all. PHL had nothing of the kind, so` |
|        - |  829 | ` *` |
|        - |  830 | ` *   cd / && phl /srv/app/main.php   with   include 'lib.php'   next to main.php` |
|        - |  831 | ` *` |
|        - |  832 | ` * failed on this engine and ran on php.` |
|        - |  833 | ` *` |
|        - |  834 | ` * WHICH file is executing is the whole question, and the include-nesting stack` |
|        - |  835 | ` * is the wrong answer to it. php reads the running op array's own filename` |
|        - |  836 | `` * (`zend_get_executed_filename_ex()`), which is the file the include statement`` |
|        - |  837 | `` * is WRITTEN in -- so a method that says `require 'A.php'` looks beside the file`` |
|        - |  838 | ` * that declared the method. The include stack only agrees with that while a` |
|        - |  839 | ` * unit's top-level code is running: it is popped as soon as an include returns,` |
|        - |  840 | ` * so by the time the library's own function is CALLED the stack's top is the` |
|        - |  841 | ` * entry script, and the fallback searched the caller's directory instead of the` |
|        - |  842 | ` * library's. That is exactly the shape a composer package has -- one bootstrap` |
|        - |  843 | ` * file, functions that pull siblings in by bare name -- and it is why a fixture` |
|        - |  844 | ` * requiring its own sibling failed here. PH7_VmExecutingUnitFile is the engine's` |
|        - |  845 | ` * answer to php's question, and every diagnostic's location already uses it.` |
|        - |  846 | ` *` |
|        - |  847 | `` * Answers 0 when that name has no directory part at all (`-r`'s "Command line`` |
|        - |  848 | ` * code"). php's own arithmetic degenerates to the bare, cwd-relative name` |
|        - |  849 | ` * there, which the default include_path's "." entry already tries.` |
|        - |  850 | ` */` |
|       64 |  851 | `PH7_PRIVATE int PH7_VmExecutingDir(ph7_vm *pVm,SyString *pOut)` |
|        4 |  852 | `{` |
|       68 |  853 | `	SyString *pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|        - |  854 | `	sxu32 n;` |
|       68 |  855 | `	if( pFile == 0 \|\| pFile->nByte < 2 ){` |
|      ! 0 |  856 | `		return 0;` |
|        - |  857 | `	}` |
|       68 |  858 | `	n = pFile->nByte;` |
|     1412 |  859 | `	while( n > 0 ){` |
|     1412 |  860 | `		int c = pFile->zString[n-1];` |
|     1408 |  861 | `		if( c == '/'` |
|        - |  862 | `#ifdef __WINNT__` |
|        4 |  863 | `		 \|\| c == '\\'` |
|        - |  864 | `#endif` |
|        - |  865 | `		){` |
|       68 |  866 | `			break;` |
|        - |  867 | `		}` |
|     1348 |  868 | `		n--;` |
|        4 |  869 | `	}` |
|       68 |  870 | `	if( n < 2 ){` |
|        - |  871 | `		/* No separator, or one sitting at index 0 -- php skips the fallback for` |
|        - |  872 | `		 * a root-level script exactly the same way. */` |
|      ! 0 |  873 | `		return 0;` |
|        - |  874 | `	}` |
|       68 |  875 | `	SyStringInitFromBuf(pOut,pFile->zString,n-1); /* without the separator */` |
|       68 |  876 | `	return 1;` |
|       36 |  877 | `}` |
|        - |  878 | `/*` |
|        - |  879 | ` * Join the set back into php's one string. Splitting on the separator and` |
|        - |  880 | ` * joining with it round-trips exactly, which is what ini_get() promises.` |
|        - |  881 | ` */` |
|      132 |  882 | `PH7_PRIVATE void PH7_VmGetIncludePath(ph7_vm *pVm,SyBlob *pOut)` |
|        4 |  883 | `{` |
|      136 |  884 | `	SyString *aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|      136 |  885 | `	char cSep = (char)PH7_VmIncludePathSep();` |
|        - |  886 | `	sxu32 n;` |
|      286 |  887 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|      154 |  888 | `		if( n > 0 ){` |
|       20 |  889 | `			SyBlobAppend(pOut,(const void *)&cSep,sizeof(char));` |
|        9 |  890 | `		}` |
|      154 |  891 | `		SyBlobAppend(pOut,aEntry[n].zString,aEntry[n].nByte);` |
|       78 |  892 | `	}` |
|      136 |  893 | `}` |
|        - |  894 | `/*` |
|        - |  895 | ` * string get_include_path(void)` |
|        - |  896 | ` *  Gets the current include_path configuration option.` |
|        - |  897 | ` * Parameter` |
|        - |  898 | ` *  None` |
|        - |  899 | ` * Return` |
|        - |  900 | ` *  Included paths as a string` |
|        - |  901 | ` */` |
|       20 |  902 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  903 | `{` |
|        - |  904 | `	SyBlob sOut;` |
|       10 |  905 | `	SXUNUSED(nArg); /* cc warning */` |
|       10 |  906 | `	SXUNUSED(apArg);` |
|       23 |  907 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|       23 |  908 | `	PH7_VmGetIncludePath(pCtx->pVm,&sOut);` |
|        - |  909 | `	/* php's answer is always a STRING: an empty set is "", never NULL, which is` |
|        - |  910 | `	 * what this used to leave behind when nothing had ever been appended. */` |
|       23 |  911 | `	if( SyBlobLength(&sOut) < 1 ){` |
|      ! 0 |  912 | `		ph7_result_string(pCtx,"",0);` |
|      ! 0 |  913 | `	}else{` |
|       23 |  914 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|        - |  915 | `	}` |
|       23 |  916 | `	SyBlobRelease(&sOut);` |
|       23 |  917 | `	return PH7_OK;` |
|        3 |  918 | `}` |
|        - |  919 | `/*` |
|        - |  920 | ` * string\|false set_include_path(string $include_path)` |
|        - |  921 | ` *  Sets the include_path configuration option for the duration of the script,` |
|        - |  922 | ` *  returning the OLD value (php contract).` |
|        - |  923 | ` */` |
|       20 |  924 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  925 | `{` |
|       22 |  926 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  927 | `	const char *zNew;` |
|       22 |  928 | `	int nLen = 0;` |
|        - |  929 | `	SyBlob sOld;` |
|       22 |  930 | `	if( nArg < 1 ){` |
|      ! 0 |  931 | `		return PH7_OK;` |
|        - |  932 | `	}` |
|        - |  933 | `	/* The OLD value php answers is the effective one, read before the write. */` |
|       22 |  934 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|       22 |  935 | `	PH7_VmGetIncludePath(pVm,&sOld);` |
|       22 |  936 | `	zNew = ph7_value_to_string(apArg[0],&nLen);` |
|       22 |  937 | `	if( nLen < 1 ){` |
|        - |  938 | `		/* php registers include_path with OnUpdateStringUnempty, so the EMPTY` |
|        - |  939 | `		 * string is refused outright: the directive keeps the value it had and` |
|        - |  940 | `		 * the call answers FALSE. PHL used to accept it, wiping the set and` |
|        - |  941 | `		 * leaving get_include_path() with nothing to answer. */` |
|        3 |  942 | `		SyBlobRelease(&sOld);` |
|        3 |  943 | `		ph7_result_bool(pCtx,0);` |
|        3 |  944 | `		return PH7_OK;` |
|        - |  945 | `	}` |
|       20 |  946 | `	PH7_VmSetIncludePath(pVm,zNew,(sxu32)nLen);` |
|       20 |  947 | `	if( SyBlobLength(&sOld) < 1 ){` |
|      ! 0 |  948 | `		ph7_result_string(pCtx,"",0);` |
|      ! 0 |  949 | `	}else{` |
|       20 |  950 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|        - |  951 | `	}` |
|       20 |  952 | `	SyBlobRelease(&sOld);` |
|       20 |  953 | `	return PH7_OK;` |
|       12 |  954 | `}` |
|        - |  955 | `/*` |
|        - |  956 | ` * string get_get_included_files(void)` |
|        - |  957 | ` *  Gets the current include_path configuration option.` |
|        - |  958 | ` * Parameter` |
|        - |  959 | ` *  None` |
|        - |  960 | ` * Return` |
|        - |  961 | ` *  Included paths as a string` |
|        - |  962 | ` */` |
|        8 |  963 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  964 | `{` |
|        9 |  965 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  966 | `	ph7_value *pArray,*pWorker;` |
|        - |  967 | `	SyString *pEntry;` |
|        - |  968 | `	int c,d;` |
|        - |  969 | `	/* Create an array and a working value */` |
|        9 |  970 | `	pArray  = ph7_context_new_array(pCtx);` |
|        9 |  971 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        9 |  972 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  973 | `		/* Out of memory,return null */` |
|      ! 0 |  974 | `		ph7_result_null(pCtx);` |
|      ! 0 |  975 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  976 | `		SXUNUSED(apArg);` |
|      ! 0 |  977 | `		return PH7_OK;` |
|        - |  978 | `	}` |
|        9 |  979 | `	c = d = '/';` |
|        - |  980 | `#ifdef __WINNT__` |
|        1 |  981 | `	d = '\\';` |
|        - |  982 | `#endif` |
|        - |  983 | `	/* Iterate throw entries */` |
|        9 |  984 | `	SySetResetCursor(pFiles);` |
|       25 |  985 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  986 | `		const char *zBase,*zEnd;` |
|        - |  987 | `		int iLen;` |
|        - |  988 | `		/* reset the string cursor */` |
|       17 |  989 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  990 | `		/* Extract base name */` |
|       17 |  991 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  992 | `		/* Ignore trailing '/' */` |
|       25 |  993 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  994 | `			zEnd--;` |
|      ! 0 |  995 | `		}` |
|       17 |  996 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|      445 |  997 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|      421 |  998 | `			zEnd--;` |
|        1 |  999 | `		}` |
|       17 | 1000 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|       17 | 1001 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 1002 | `		/* Copy entry name */` |
|       17 | 1003 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 1004 | `		/* Perform the insertion */` |
|       17 | 1005 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 1006 | `	}` |
|        - | 1007 | `	/* All done,return the created array */` |
|        9 | 1008 | `	ph7_result_value(pCtx,pArray);` |
|        - | 1009 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 1010 | `	 * by the engine as soon we return from this foreign` |
|        - | 1011 | `	 * function.` |
|        - | 1012 | `	 */` |
|        9 | 1013 | `	return PH7_OK;` |
|        5 | 1014 | `}` |
|        - | 1015 | `/*` |
|        - | 1016 | ` * php raises TWO diagnostics for an include it could not open, and this engine` |
|        - | 1017 | ``  * raised one sentence of its own -- `include(): IO error while importing: 'x'` `` |
|        - | 1018 | ` * -- which names neither the reason the open failed nor the include_path that` |
|        - | 1019 | ` * was searched, and which no handler written against php can recognise:` |
|        - | 1020 | ` *` |
|        - | 1021 | ` *   Warning: include(x.php): Failed to open stream: No such file or directory` |
|        - | 1022 | ` *   Warning: include(): Failed opening 'x.php' for inclusion (include_path='.')` |
|        - | 1023 | ` *` |
|        - | 1024 | ` * The first is the stream layer's, and is the same sentence fopen() raises for` |
|        - | 1025 | ` * the same failure; the second is the language construct's, and is the only one` |
|        - | 1026 | ` * that says where it looked. Both name the path AS WRITTEN.` |
|        - | 1027 | ` *` |
|        - | 1028 | ` * require's second diagnostic is not a warning at all. php 8 THROWS an Error --` |
|        - | 1029 | `` * `Failed opening required 'x.php' (include_path='.')`, no function prefix, no`` |
|        - | 1030 | ` * "for inclusion" -- so a script may catch a missing dependency and carry on.` |
|        - | 1031 | ` * PHL reported it through the native fatal path, which ended the run and could` |
|        - | 1032 | ` * not be caught at all.` |
|        - | 1033 | ` */` |
|       34 | 1034 | `static sxi32 VmIncludeFailure(ph7_context *pCtx,SyString *pFile,const char *zWhy,int bRequire)` |
|        2 | 1035 | `{` |
|       36 | 1036 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1037 | `	SyString sPath;` |
|        - | 1038 | `	SyBlob sIncPath;` |
|       36 | 1039 | `	sxi32 rc = PH7_OK;` |
|        - | 1040 | `	/* The stream layer's sentence. Raised through the VM rather than the call` |
|        - | 1041 | ``	 * context, because the context's own prefix is `name(): ` and php's here`` |
|        - | 1042 | `	 * carries the path inside the parentheses. */` |
|       53 | 1043 | `	PH7_VmThrowWarningFmt(pVm,"%s(%z): Failed to open stream: %s",` |
|       17 | 1044 | `		ph7_function_name(pCtx),pFile,zWhy ? zWhy : "operation failed");` |
|       36 | 1045 | `	SyBlobInit(&sIncPath,&pVm->sAllocator);` |
|       36 | 1046 | `	PH7_VmGetIncludePath(pVm,&sIncPath);` |
|       36 | 1047 | `	SyStringInitFromBuf(&sPath,(const char *)SyBlobData(&sIncPath),SyBlobLength(&sIncPath));` |
|       36 | 1048 | `	if( bRequire ){` |
|       16 | 1049 | `		rc = PH7_VmThrowException(pCtx,"Error",` |
|        5 | 1050 | `			"Failed opening required '%z' (include_path='%z')",pFile,&sPath);` |
|        6 | 1051 | `	}else{` |
|       38 | 1052 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       12 | 1053 | `			"Failed opening '%z' for inclusion (include_path='%z')",pFile,&sPath);` |
|        - | 1054 | `	}` |
|       36 | 1055 | `	SyBlobRelease(&sIncPath);` |
|       36 | 1056 | `	return rc;` |
|        2 | 1057 | `}` |
|        - | 1058 | `/*` |
|        - | 1059 | ` * include:` |
|        - | 1060 | ` * According to the PHP reference manual.` |
|        - | 1061 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 1062 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 1063 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 1064 | ` *  include() will finally check in the calling script's own directory` |
|        - | 1065 | ` *  and the current working directory before failing. The include()` |
|        - | 1066 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 1067 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 1068 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 1069 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 1070 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 1071 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 1072 | ` *  directory to find the requested file.` |
|        - | 1073 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 1074 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 1075 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 1076 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 1077 | ` */` |
|    11280 | 1078 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1079 | `{` |
|        - | 1080 | `	SyString sFile;` |
|    11285 | 1081 | `	const char *zWhy = 0;` |
|        - | 1082 | `	sxi32 rc;` |
|    11285 | 1083 | `	if( nArg < 1 ){` |
|        - | 1084 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 1085 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1086 | `		return SXRET_OK;` |
|        - | 1087 | `	}` |
|        - | 1088 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - | 1089 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - | 1090 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - | 1091 | `	{` |
|    11285 | 1092 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|    11285 | 1093 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1094 | `			return rcSv;` |
|        - | 1095 | `		}` |
|        - | 1096 | `	}` |
|    11283 | 1097 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1098 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1099 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1100 | `		 * this used to answer NULL and carry on. */` |
|        2 | 1101 | `		return PH7_EXCEPTION;` |
|        - | 1102 | `	}` |
|        - | 1103 | `	/* Open,compile and execute the desired script */` |
|    11281 | 1104 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE,&zWhy);` |
|    11281 | 1105 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1106 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1107 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1108 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1109 | `		 * wins, so it is tested first. */` |
|        5 | 1110 | `		return rc;` |
|        - | 1111 | `	}` |
|    11277 | 1112 | `	if( rc != SXRET_OK ){` |
|       22 | 1113 | `		VmIncludeFailure(pCtx,&sFile,zWhy,FALSE);` |
|       22 | 1114 | `		ph7_result_bool(pCtx,0);` |
|       10 | 1115 | `	}` |
|    11277 | 1116 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1117 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 | 1118 | `		return PH7_ABORT;` |
|        - | 1119 | `	}` |
|    11273 | 1120 | `	return SXRET_OK;` |
|     5645 | 1121 | `}` |
|        - | 1122 | `/*` |
|        - | 1123 | ` * include_once:` |
|        - | 1124 | ` *  According to the PHP reference manual.` |
|        - | 1125 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 1126 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 1127 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 1128 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 1129 | ` *   just once.` |
|        - | 1130 | ` */` |
|       38 | 1131 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1132 | `{` |
|        - | 1133 | `	SyString sFile;` |
|       43 | 1134 | `	const char *zWhy = 0;` |
|        - | 1135 | `	sxi32 rc;` |
|       43 | 1136 | `	if( nArg < 1 ){` |
|        - | 1137 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 1138 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1139 | `		return SXRET_OK;` |
|        - | 1140 | `	}` |
|        - | 1141 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - | 1142 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - | 1143 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - | 1144 | `	{` |
|       43 | 1145 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       43 | 1146 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1147 | `			return rcSv;` |
|        - | 1148 | `		}` |
|        - | 1149 | `	}` |
|       41 | 1150 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1151 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1152 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1153 | `		 * this used to answer NULL and carry on. */` |
|      ! 0 | 1154 | `		return PH7_EXCEPTION;` |
|        - | 1155 | `	}` |
|        - | 1156 | `	/* Open,compile and execute the desired script */` |
|       41 | 1157 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE,&zWhy);` |
|       41 | 1158 | `	if( rc == SXERR_EXISTS ){` |
|        - | 1159 | `		/* File already included,return TRUE */` |
|       25 | 1160 | `		ph7_result_bool(pCtx,1);` |
|       25 | 1161 | `		return SXRET_OK;` |
|        - | 1162 | `	}` |
|       19 | 1163 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1164 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1165 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1166 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1167 | `		 * wins, so it is tested first. */` |
|        3 | 1168 | `		return rc;` |
|        - | 1169 | `	}` |
|       16 | 1170 | `	if( rc != SXRET_OK ){` |
|        5 | 1171 | `		VmIncludeFailure(pCtx,&sFile,zWhy,FALSE);` |
|        5 | 1172 | `		ph7_result_bool(pCtx,0);` |
|        2 | 1173 | `	}` |
|       16 | 1174 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1175 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 1176 | `		return PH7_ABORT;` |
|        - | 1177 | `	}` |
|       16 | 1178 | `	return SXRET_OK;` |
|       24 | 1179 | `}` |
|        - | 1180 | `/*` |
|        - | 1181 | ` * require.` |
|        - | 1182 | ` *  According to the PHP reference manual.` |
|        - | 1183 | ` *   require() is identical to include() except upon failure it will` |
|        - | 1184 | ` *   also produce a fatal level error.` |
|        - | 1185 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 1186 | ` *   emits a warning  which allows the script to continue.` |
|        - | 1187 | ` */` |
|      208 | 1188 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1189 | `{` |
|        - | 1190 | `	SyString sFile;` |
|      213 | 1191 | `	const char *zWhy = 0;` |
|        - | 1192 | `	sxi32 rc;` |
|      213 | 1193 | `	if( nArg < 1 ){` |
|        - | 1194 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 1195 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1196 | `		return SXRET_OK;` |
|        - | 1197 | `	}` |
|        - | 1198 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - | 1199 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - | 1200 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - | 1201 | `	{` |
|      213 | 1202 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|      213 | 1203 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1204 | `			return rcSv;` |
|        - | 1205 | `		}` |
|        - | 1206 | `	}` |
|      211 | 1207 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1208 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1209 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1210 | `		 * this used to answer NULL and carry on. */` |
|        2 | 1211 | `		return PH7_EXCEPTION;` |
|        - | 1212 | `	}` |
|        - | 1213 | `	/* Open,compile and execute the desired script */` |
|      209 | 1214 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE,&zWhy);` |
|      209 | 1215 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1216 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1217 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1218 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1219 | `		 * wins, so it is tested first. */` |
|        8 | 1220 | `		return rc;` |
|        - | 1221 | `	}` |
|      203 | 1222 | `	if( rc != SXRET_OK ){` |
|        - | 1223 | `		/* php THROWS: the include is over, but the SCRIPT need not be. */` |
|        7 | 1224 | `		sxi32 rcThrow = VmIncludeFailure(pCtx,&sFile,zWhy,TRUE);` |
|        7 | 1225 | `		ph7_result_bool(pCtx,0);` |
|        7 | 1226 | `		return rcThrow == PH7_OK ? PH7_EXCEPTION : rcThrow;` |
|        - | 1227 | `	}` |
|      197 | 1228 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1229 | `		/* exit/die inside the included file: cascade the halt */` |
|       10 | 1230 | `		return PH7_ABORT;` |
|        - | 1231 | `	}` |
|      189 | 1232 | `	return SXRET_OK;` |
|      109 | 1233 | `}` |
|        - | 1234 | `/*` |
|        - | 1235 | ` * require_once:` |
|        - | 1236 | ` *  According to the PHP reference manual.` |
|        - | 1237 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 1238 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 1239 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 1240 | ` *   and how it differs from its non _once siblings.` |
|        - | 1241 | ` */` |
|       30 | 1242 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 1243 | `{` |
|        - | 1244 | `	SyString sFile;` |
|       34 | 1245 | `	const char *zWhy = 0;` |
|        - | 1246 | `	sxi32 rc;` |
|       34 | 1247 | `	if( nArg < 1 ){` |
|        - | 1248 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 1249 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1250 | `		return SXRET_OK;` |
|        - | 1251 | `	}` |
|        - | 1252 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - | 1253 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - | 1254 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - | 1255 | `	{` |
|       34 | 1256 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       34 | 1257 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1258 | `			return rcSv;` |
|        - | 1259 | `		}` |
|        - | 1260 | `	}` |
|       32 | 1261 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1262 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1263 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1264 | `		 * this used to answer NULL and carry on. */` |
|      ! 0 | 1265 | `		return PH7_EXCEPTION;` |
|        - | 1266 | `	}` |
|        - | 1267 | `	/* Open,compile and execute the desired script */` |
|       32 | 1268 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE,&zWhy);` |
|       32 | 1269 | `	if( rc == SXERR_EXISTS ){` |
|        - | 1270 | `		/* File already included,return TRUE */` |
|       18 | 1271 | `		ph7_result_bool(pCtx,1);` |
|       18 | 1272 | `		return SXRET_OK;` |
|        - | 1273 | `	}` |
|       16 | 1274 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1275 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1276 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1277 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1278 | `		 * wins, so it is tested first. */` |
|        3 | 1279 | `		return rc;` |
|        - | 1280 | `	}` |
|       13 | 1281 | `	if( rc != SXRET_OK ){` |
|        - | 1282 | `		/* php THROWS: the include is over, but the SCRIPT need not be. */` |
|        5 | 1283 | `		sxi32 rcThrow = VmIncludeFailure(pCtx,&sFile,zWhy,TRUE);` |
|        5 | 1284 | `		ph7_result_bool(pCtx,0);` |
|        5 | 1285 | `		return rcThrow == PH7_OK ? PH7_EXCEPTION : rcThrow;` |
|        - | 1286 | `	}` |
|        8 | 1287 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1288 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 1289 | `		return PH7_ABORT;` |
|        - | 1290 | `	}` |
|        8 | 1291 | `	return SXRET_OK;` |
|       19 | 1292 | `}` |
|        - | 1293 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 1294 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 1295 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 1296 | `/*` |
|        - | 1297 | ` * Section:` |
|        - | 1298 | ` *  SPL Autoloading functions.` |
|        - | 1299 | ` * Status:` |
|        - | 1300 | ` *  Stable.` |
|        - | 1301 | ` */` |
|        - | 1302 | `/*` |
|        - | 1303 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 1304 | ` *  Register given function as __autoload() implementation.` |
|        - | 1305 | ` * Parameters` |
|        - | 1306 | ` *  callback` |
|        - | 1307 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 1308 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 1309 | ` *  throw` |
|        - | 1310 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 1311 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 1312 | ` *  prepend` |
|        - | 1313 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 1314 | ` *   autoload stack instead of appending it.` |
|        - | 1315 | ` * Return` |
|        - | 1316 | ` *  TRUE on success, FALSE on failure.` |
|        - | 1317 | ` */` |
|       72 | 1318 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1319 | `{` |
|        - | 1320 | `	VmAutoloadCB sEntry;` |
|       77 | 1321 | `	ph7_vm *pVm = pCtx->pVm;` |
|       77 | 1322 | `	int iPrepend = 0;` |
|        - | 1323 | `	sxu32 n;` |
|       77 | 1324 | `	if( nArg < 1 ){` |
|        - | 1325 | `		/* No callback provided — register default spl_autoload.` |
|        - | 1326 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 1327 | `		/* Check for duplicates first */` |
|        9 | 1328 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 1329 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        4 | 1330 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 1331 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 1332 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 1333 | `				ph7_result_bool(pCtx,1);` |
|        5 | 1334 | `				return SXRET_OK;` |
|        - | 1335 | `			}` |
|      ! 0 | 1336 | `		}` |
|        5 | 1337 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 1338 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 1339 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 1340 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 1341 | `		ph7_result_bool(pCtx,1);` |
|        5 | 1342 | `		return SXRET_OK;` |
|        - | 1343 | `	}` |
|        - | 1344 | `	/* Validate that the callback is callable */` |
|       69 | 1345 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 1346 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 1347 | `		if( nArg >= 2 ){` |
|      ! 0 | 1348 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 1349 | `		}` |
|      ! 0 | 1350 | `		if( iThrow ){` |
|      ! 0 | 1351 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 1352 | `				"Argument is not callable");` |
|      ! 0 | 1353 | `		}` |
|      ! 0 | 1354 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1355 | `		return SXRET_OK;` |
|        - | 1356 | `	}` |
|        - | 1357 | `	/* Check for duplicates */` |
|       91 | 1358 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       24 | 1359 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 1360 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 1361 | `			/* Already registered */` |
|      ! 0 | 1362 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 1363 | `			return SXRET_OK;` |
|        - | 1364 | `		}` |
|       13 | 1365 | `	}` |
|        - | 1366 | `	/* Check prepend flag */` |
|       69 | 1367 | `	if( nArg >= 3 ){` |
|        3 | 1368 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 1369 | `	}` |
|        - | 1370 | `	/* Store the callback */` |
|       69 | 1371 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       69 | 1372 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       69 | 1373 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       70 | 1374 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 1375 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 1376 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 1377 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 1378 | `		VmAutoloadCB *aBase;` |
|        3 | 1379 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 1380 | `		/* Rotate: move last entry to front */` |
|        3 | 1381 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 1382 | `		if( aBase ){` |
|        - | 1383 | `			VmAutoloadCB sTemp;` |
|        - | 1384 | `			sxu32 i;` |
|        3 | 1385 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 1386 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 1387 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 1388 | `			}` |
|        3 | 1389 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 1390 | `		}` |
|        2 | 1391 | `	}else{` |
|       67 | 1392 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 1393 | `	}` |
|       69 | 1394 | `	ph7_result_bool(pCtx,1);` |
|       69 | 1395 | `	return SXRET_OK;` |
|       41 | 1396 | `}` |
|        - | 1397 | `/*` |
|        - | 1398 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 1399 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 1400 | ` * Parameters` |
|        - | 1401 | ` *  callback` |
|        - | 1402 | ` *   The autoload function being unregistered.` |
|        - | 1403 | ` * Return` |
|        - | 1404 | ` *  TRUE on success, FALSE on failure.` |
|        - | 1405 | ` */` |
|       40 | 1406 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1407 | `{` |
|       45 | 1408 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1409 | `	sxu32 n,nEntry;` |
|       45 | 1410 | `	if( nArg < 1 ){` |
|      ! 0 | 1411 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1412 | `		return SXRET_OK;` |
|        - | 1413 | `	}` |
|       45 | 1414 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       49 | 1415 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       47 | 1416 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       47 | 1417 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 1418 | `			/* Found — remove by shifting remaining entries down */` |
|       43 | 1419 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 1420 | `			sxu32 i;` |
|       43 | 1421 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       59 | 1422 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       18 | 1423 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|       10 | 1424 | `			}` |
|        - | 1425 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       43 | 1426 | `			SySetPop(&pVm->aAutoload);` |
|       43 | 1427 | `			ph7_result_bool(pCtx,1);` |
|       43 | 1428 | `			return SXRET_OK;` |
|        - | 1429 | `		}` |
|        3 | 1430 | `	}` |
|        3 | 1431 | `	ph7_result_bool(pCtx,0);` |
|        3 | 1432 | `	return SXRET_OK;` |
|       25 | 1433 | `}` |
|        - | 1434 | `/*` |
|        - | 1435 | ` * array spl_autoload_functions(void)` |
|        - | 1436 | ` *  Return all registered __autoload() functions.` |
|        - | 1437 | ` * Return` |
|        - | 1438 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 1439 | ` *  an empty array is returned.` |
|        - | 1440 | ` */` |
|       20 | 1441 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1442 | `{` |
|       21 | 1443 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1444 | `	ph7_value *pArray;` |
|        - | 1445 | `	sxu32 n,nEntry;` |
|       10 | 1446 | `	SXUNUSED(nArg);` |
|       10 | 1447 | `	SXUNUSED(apArg);` |
|       21 | 1448 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 1449 | `	if( pArray == 0 ){` |
|      ! 0 | 1450 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1451 | `		return SXRET_OK;` |
|        - | 1452 | `	}` |
|       21 | 1453 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 1454 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 1455 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 1456 | `		if( pEntry ){` |
|       15 | 1457 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 1458 | `		}` |
|        8 | 1459 | `	}` |
|       21 | 1460 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 1461 | `	return SXRET_OK;` |
|       11 | 1462 | `}` |
|        - | 1463 | `/*` |
|        - | 1464 | ` * string spl_autoload_extensions([?string $file_extensions = null])` |
|        - | 1465 | ` *  Register and return default file extensions for spl_autoload().` |
|        - | 1466 | ` * Return` |
|        - | 1467 | ` *  The list in force AFTER the call, which is the new one when a string was` |
|        - | 1468 | ` *  handed over and the standing one otherwise.` |
|        - | 1469 | ` *` |
|        - | 1470 | ` * The list is per-VM state rather than an ini directive (php keeps it in SPL's` |
|        - | 1471 | `` * own globals), and `null` means READ: it is the one argument value that does`` |
|        - | 1472 | ``  * not write. Every other value writes what it stringifies to, so `false` `` |
|        - | 1473 | ` * empties the list -- and an empty list is a real setting, not a reset.` |
|        - | 1474 | ` */` |
|       18 | 1475 | `PH7_PRIVATE int vm_builtin_spl_autoload_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1476 | `{` |
|       19 | 1477 | `	ph7_vm *pVm = pCtx->pVm;` |
|       19 | 1478 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|        - | 1479 | `		const char *zExt;` |
|        - | 1480 | `		int nExt;` |
|        7 | 1481 | `		zExt = ph7_value_to_string(apArg[0],&nExt);` |
|        7 | 1482 | `		SyBlobReset(&pVm->sAutoloadExt);` |
|        7 | 1483 | `		if( nExt > 0 ){` |
|        5 | 1484 | `			SyBlobAppend(&pVm->sAutoloadExt,zExt,(sxu32)nExt);` |
|        2 | 1485 | `		}` |
|        3 | 1486 | `	}` |
|       28 | 1487 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pVm->sAutoloadExt),` |
|       18 | 1488 | `		(int)SyBlobLength(&pVm->sAutoloadExt));` |
|       19 | 1489 | `	return SXRET_OK;` |
|        1 | 1490 | `}` |
|        - | 1491 | `/*` |
|        - | 1492 | ` * void spl_autoload_call(string $class)` |
|        - | 1493 | ` *  Try all registered autoloaders to load the requested class.` |
|        - | 1494 | ` *` |
|        - | 1495 | ` * php runs the stack whether or not the class is already declared -- the` |
|        - | 1496 | ` * verb is "call the autoloaders", not "load if missing" -- and stops as soon` |
|        - | 1497 | ` * as one of them declares it. The answer is always NULL.` |
|        - | 1498 | ` */` |
|        6 | 1499 | `PH7_PRIVATE int vm_builtin_spl_autoload_call(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1500 | `{` |
|        - | 1501 | `	const char *zClass;` |
|        - | 1502 | `	int nClass;` |
|        7 | 1503 | `	if( nArg < 1 ){` |
|      ! 0 | 1504 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1505 | `		return SXRET_OK;` |
|        - | 1506 | `	}` |
|        7 | 1507 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        - | 1508 | `	{` |
|        - | 1509 | `		/* php runs the stack for the EMPTY name too -- the autoloaders see a` |
|        - | 1510 | `		 * "" they have to answer for, rather than a call that never happened. */` |
|        7 | 1511 | `		sxu32 nByte = nClass > 0 ? (sxu32)nClass : 0;` |
|        7 | 1512 | `		if( zClass == 0 ){` |
|      ! 0 | 1513 | `			zClass = "";` |
|      ! 0 | 1514 | `		}` |
|        7 | 1515 | `		PH7_VmClassNameAnchor(&zClass,&nByte);` |
|        7 | 1516 | `		PH7_VmTriggerAutoload(pCtx->pVm,zClass,nByte,0);` |
|        - | 1517 | `	}` |
|        7 | 1518 | `	ph7_result_null(pCtx);` |
|        7 | 1519 | `	return SXRET_OK;` |
|        4 | 1520 | `}` |
|        - | 1521 | `/*` |
|        - | 1522 | ` * array spl_classes()` |
|        - | 1523 | ` *  Return an array with the name of every class and interface SPL declares.` |
|        - | 1524 | ` *` |
|        - | 1525 | ` * php answers a map whose key and value are both the name, in the order its` |
|        - | 1526 | ` * own list is written -- which is alphabetical. The set is SPL's own and not` |
|        - | 1527 | ` * "everything iterable": Traversable, Countable and the exception BASE are` |
|        - | 1528 | ` * php's Core, so none of the three is here, while the five SPL INTERFACES` |
|        - | 1529 | ` * (OuterIterator, RecursiveIterator, SeekableIterator, SplObserver,` |
|        - | 1530 | ` * SplSubject) are.` |
|        - | 1531 | ` */` |
|        2 | 1532 | `PH7_PRIVATE int vm_builtin_spl_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1533 | `{` |
|        - | 1534 | `	static const char * const azSpl[] = {` |
|        - | 1535 | `		"AppendIterator", "ArrayIterator", "ArrayObject", "BadFunctionCallException",` |
|        - | 1536 | `		"BadMethodCallException", "CachingIterator", "CallbackFilterIterator",` |
|        - | 1537 | `		"DirectoryIterator", "DomainException", "EmptyIterator", "FilesystemIterator",` |
|        - | 1538 | `		"FilterIterator", "GlobIterator", "InfiniteIterator", "InvalidArgumentException",` |
|        - | 1539 | `		"IteratorIterator", "LengthException", "LimitIterator", "LogicException",` |
|        - | 1540 | `		"MultipleIterator", "NoRewindIterator", "OuterIterator", "OutOfBoundsException",` |
|        - | 1541 | `		"OutOfRangeException", "OverflowException", "ParentIterator", "RangeException",` |
|        - | 1542 | `		"RecursiveArrayIterator", "RecursiveCachingIterator",` |
|        - | 1543 | `		"RecursiveCallbackFilterIterator", "RecursiveDirectoryIterator",` |
|        - | 1544 | `		"RecursiveFilterIterator", "RecursiveIterator", "RecursiveIteratorIterator",` |
|        - | 1545 | `		"RecursiveRegexIterator", "RecursiveTreeIterator", "RegexIterator",` |
|        - | 1546 | `		"RuntimeException", "SeekableIterator", "SplDoublyLinkedList", "SplFileInfo",` |
|        - | 1547 | `		"SplFileObject", "SplFixedArray", "SplHeap", "SplMinHeap", "SplMaxHeap",` |
|        - | 1548 | `		"SplObjectStorage", "SplObserver", "SplPriorityQueue", "SplQueue", "SplStack",` |
|        - | 1549 | `		"SplSubject", "SplTempFileObject", "UnderflowException", "UnexpectedValueException"` |
|        - | 1550 | `	};` |
|        - | 1551 | `	ph7_value *pArray,*pVal;` |
|        - | 1552 | `	sxu32 n;` |
|        1 | 1553 | `	SXUNUSED(nArg);` |
|        1 | 1554 | `	SXUNUSED(apArg);` |
|        3 | 1555 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 1556 | `	pVal = ph7_context_new_scalar(pCtx);` |
|        3 | 1557 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|      ! 0 | 1558 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1559 | `		return SXRET_OK;` |
|        - | 1560 | `	}` |
|      113 | 1561 | `	for( n = 0 ; n < SX_ARRAYSIZE(azSpl) ; ++n ){` |
|      111 | 1562 | `		ph7_value_string(pVal,azSpl[n],-1);` |
|      111 | 1563 | `		ph7_array_add_strkey_elem(pArray,azSpl[n],pVal);` |
|      111 | 1564 | `		ph7_value_reset_string_cursor(pVal);` |
|       56 | 1565 | `	}` |
|        3 | 1566 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 1567 | `	return SXRET_OK;` |
|        2 | 1568 | `}` |
|        - | 1569 | `/*` |
|        - | 1570 | ` * void spl_autoload(string $class [, ?string $file_extensions = null ])` |
|        - | 1571 | ` *  Default implementation of __autoload().` |
|        - | 1572 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 1573 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 1574 | ` * Parameters` |
|        - | 1575 | ` *  class` |
|        - | 1576 | ` *   The class name being searched.` |
|        - | 1577 | ` *  file_extensions` |
|        - | 1578 | ` *   Comma-separated list of file extensions to try. When none is handed over,` |
|        - | 1579 | ` *   the list is whatever spl_autoload_extensions() holds -- which is where the` |
|        - | 1580 | ` *   ORDER comes from, and the order decides the answer: php's default tries` |
|        - | 1581 | `` *   `.inc` BEFORE `.php`, and this engine had the pair hardcoded the other way`` |
|        - | 1582 | `` *   round, so a directory carrying both `foo.inc` and `foo.php` loaded the one`` |
|        - | 1583 | ` *   php does not.` |
|        - | 1584 | ` */` |
|        2 | 1585 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1586 | `{` |
|        - | 1587 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 1588 | `	SyBlob sPath;` |
|        - | 1589 | `	int nClass,nExt,iByte;` |
|        - | 1590 | `	sxi32 rc;` |
|        3 | 1591 | `	if( nArg < 1 ){` |
|      ! 0 | 1592 | `		return SXRET_OK;` |
|        - | 1593 | `	}` |
|        3 | 1594 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 1595 | `	if( nClass < 1 ){` |
|      ! 0 | 1596 | `		return SXRET_OK;` |
|        - | 1597 | `	}` |
|        - | 1598 | `	/* The list to try: the argument when there is one, and the standing` |
|        - | 1599 | `	 * spl_autoload_extensions() setting otherwise. */` |
|        3 | 1600 | `	if( nArg >= 2 && !ph7_value_is_null(apArg[1]) ){` |
|        - | 1601 | `		/* An argument that is EMPTY is an empty list and not a request for the` |
|        - | 1602 | ``		 * default one, so `spl_autoload($c,"")` searches nothing at all. */`` |
|      ! 0 | 1603 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 1604 | `	}else{` |
|        3 | 1605 | `		zExt = (const char *)SyBlobData(&pCtx->pVm->sAutoloadExt);` |
|        3 | 1606 | `		nExt = (int)SyBlobLength(&pCtx->pVm->sAutoloadExt);` |
|        - | 1607 | `	}` |
|        - | 1608 | `	/* php walks the list as a C string, so a NUL byte in it ends the search. */` |
|       21 | 1609 | `	for( iByte = 0 ; iByte < nExt ; ++iByte ){` |
|       19 | 1610 | `		if( zExt[iByte] == 0 ){` |
|      ! 0 | 1611 | `			nExt = iByte;` |
|      ! 0 | 1612 | `			break;` |
|        - | 1613 | `		}` |
|       10 | 1614 | `	}` |
|        3 | 1615 | `	if( nExt < 1 ){` |
|      ! 0 | 1616 | `		return SXRET_OK;` |
|        - | 1617 | `	}` |
|        3 | 1618 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 1619 | `	/* Iterate over comma-separated extensions */` |
|        3 | 1620 | `	zEnd = zExt + nExt;` |
|        3 | 1621 | `	zCur = zExt;` |
|        7 | 1622 | `	while( zCur < zEnd ){` |
|        - | 1623 | `		const char *zComma;` |
|        - | 1624 | `		SyString sFile;` |
|        - | 1625 | `		int i;` |
|        - | 1626 | `		/* Find next comma or end */` |
|        5 | 1627 | `		zComma = zCur;` |
|       21 | 1628 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 1629 | `			zComma++;` |
|        1 | 1630 | `		}` |
|        - | 1631 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 1632 | `		SyBlobReset(&sPath);` |
|       69 | 1633 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 1634 | `			char c = zClass[i];` |
|       65 | 1635 | `			if( c == '\\' ){` |
|      ! 0 | 1636 | `				c = '/';` |
|       65 | 1637 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 1638 | `				c = c + ('a' - 'A');` |
|        6 | 1639 | `			}` |
|       65 | 1640 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 1641 | `		}` |
|        - | 1642 | `		/* Append extension */` |
|        5 | 1643 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 1644 | `		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,` |
|        - | 1645 | `		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a` |
|        - | 1646 | `		 * terminator, so without this the strlen reads past the buffer (a` |
|        - | 1647 | `		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL` |
|        - | 1648 | `		 * is not counted in SyBlobLength(), so the SyString length stays correct. */` |
|        5 | 1649 | `		SyBlobNullAppend(&sPath);` |
|        - | 1650 | `		/* Try to include the file */` |
|        5 | 1651 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 1652 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE,0);` |
|        5 | 1653 | `		if( rc == SXRET_OK \|\| rc == PH7_EXCEPTION ){` |
|        - | 1654 | `			/* Included — or it threw, which ends the search too: the remaining` |
|        - | 1655 | `			 * extensions are not tried after a file has already run. */` |
|      ! 0 | 1656 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 1657 | `			return rc == PH7_EXCEPTION ? rc : SXRET_OK;` |
|        - | 1658 | `		}` |
|        - | 1659 | `		/* Move past the comma */` |
|        5 | 1660 | `		zCur = zComma;` |
|        5 | 1661 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 1662 | `			zCur++;` |
|        1 | 1663 | `		}` |
|        1 | 1664 | `	}` |
|        3 | 1665 | `	SyBlobRelease(&sPath);` |
|        3 | 1666 | `	return SXRET_OK;` |
|        2 | 1667 | `}` |
|        - | 1668 | `/* Table of built-in VM functions. */` |
|        - | 1669 | `/*` |
|        - | 1670 | ` * Packing body for __call / __callStatic (band A #3b).` |
|        - | 1671 | ` * OP_MEMBER, on a missing or inaccessible method whose class declares the magic` |
|        - | 1672 | ` * handler, stashes {receiver, class, original name} on the VM and marks the callee` |
|        - | 1673 | ` * slot MEMOBJ_AUX_MAGICCALL; the normal OP_CALL machinery then collects the` |
|        - | 1674 | ` * ORIGINAL argument list (incl. spreads) and hands it here, which packs it into a` |
|        - | 1675 | ` * php array and invokes` |
|        - | 1676 | ` *   $recv->__call($name, $args)   /   Class::__callStatic($name, $args)` |
|        - | 1677 | ` * returning the handler's value as the call's result. A throw propagates via the` |
|        - | 1678 | ` * returned status (and the boundary rail).` |
|        - | 1679 | ` *` |
|        - | 1680 | `` * This used to be a REGISTERED host function named `__phl_magic_call` whose name`` |
|        - | 1681 | ` * the four OP_MEMBER sites wrote into the callee slot — so the engine's own` |
|        - | 1682 | ` * dispatch was spelled as a global PHP function that function_exists() and` |
|        - | 1683 | ` * get_defined_functions() both reported, and that any script could call. It is` |
|        - | 1684 | ` * reached through the VM's own function record now (PH7_VmMagicCallFunc); the` |
|        - | 1685 | ` * mark on the slot is the only thing that selects it, and there is no name.` |
|        - | 1686 | ` */` |
|      126 | 1687 | `static int VmMagicCallDispatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 1688 | `{` |
|      130 | 1689 | `	ph7_vm *pVm = pCtx->pVm;` |
|      130 | 1690 | `	ph7_class_instance *pRecv = pVm->pMagicCallThis;` |
|      130 | 1691 | `	ph7_class *pClass = pVm->pMagicCallClass;` |
|        - | 1692 | `	ph7_value sResult;` |
|        - | 1693 | `	SyString sMethName;` |
|        - | 1694 | `	sxi32 rc;` |
|        - | 1695 | `	/* Consume the pending dispatch (one-shot) */` |
|      130 | 1696 | `	pVm->pMagicCallThis = 0;` |
|      130 | 1697 | `	pVm->pMagicCallClass = 0;` |
|      130 | 1698 | `	if( pClass == 0 ){` |
|        - | 1699 | `		/* Defensive: the mark and the latch are set together by OP_MEMBER, so a` |
|        - | 1700 | `		 * classless arrival is unreachable. It was reachable while this body wore a` |
|        - | 1701 | ``		 * function NAME — anyone could call `__phl_magic_call()` — which is exactly`` |
|        - | 1702 | `		 * what the mark retired. */` |
|      ! 0 | 1703 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1704 | `		return PH7_OK;` |
|        - | 1705 | `	}` |
|      130 | 1706 | `	SyStringInitFromBuf(&sMethName,SyBlobData(&pVm->sMagicCallName),SyBlobLength(&pVm->sMagicCallName));` |
|      130 | 1707 | `	PH7_MemObjInit(pVm,&sResult);` |
|        - | 1708 | `	/* The one packing site, shared with every CALLABLE spelling of the same call` |
|        - | 1709 | `	 * (PH7_VmDispatchMagicCall, vm_builtin_call.c): the handler lookup, the $args array` |
|        - | 1710 | `	 * — named arguments keyed by name, as php keys them — and the engine-dispatch` |
|        - | 1711 | `	 * visibility rule all live there. Two copies of this is how the syntax path and the` |
|        - | 1712 | `	 * callable path came to disagree about the argument names in the first place.` |
|        - | 1713 | `	 * SXERR_NOTFOUND is unreachable: OP_MEMBER verified the handler exists. */` |
|      193 | 1714 | `	rc = PH7_VmDispatchMagicCall(pVm,pClass,pRecv,SyStringData(&sMethName),` |
|       63 | 1715 | `		SyStringLength(&sMethName),&sResult,nArg,apArg,pCtx->pArgMap);` |
|      130 | 1716 | `	if( rc == SXRET_OK ){` |
|      124 | 1717 | `		ph7_result_value(pCtx,&sResult);` |
|       60 | 1718 | `	}` |
|      130 | 1719 | `	PH7_MemObjRelease(&sResult);` |
|      130 | 1720 | `	if( pRecv ){` |
|       88 | 1721 | `		PH7_ClassInstanceUnref(pRecv);` |
|       42 | 1722 | `	}` |
|      130 | 1723 | `	SyBlobReset(&pVm->sMagicCallName);` |
|      130 | 1724 | `	return (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) ? rc : PH7_OK;` |
|       67 | 1725 | `}` |
|        - | 1726 | `/*` |
|        - | 1727 | ` * The function record OP_CALL dispatches a MEMOBJ_AUX_MAGICCALL callee slot through,` |
|        - | 1728 | ` * built on first use and owned by the VM (the allocator frees it with everything else).` |
|        - | 1729 | ` *` |
|        - | 1730 | ` * It is deliberately NOT installed in pVm->hHostFunction: the same property that makes` |
|        - | 1731 | ` * a native class method unreachable except by dispatching the method (see` |
|        - | 1732 | ` * PH7_NativeClassInstallMethod) makes this body unreachable except by the engine's own` |
|        - | 1733 | ` * __call routing. It carries no signature and no arity bounds, so the OP_CALL choke` |
|        - | 1734 | ` * point's ZPP and ArgumentCountError screens are inert for it — the handler's OWN` |
|        - | 1735 | ` * declared parameters are what php enforces, and it is a PHP method with a frame of its` |
|        - | 1736 | ` * own. sName is a diagnostic label only; nothing that reads it can be reached from here.` |
|        - | 1737 | ` */` |
|      126 | 1738 | `PH7_PRIVATE ph7_user_func * PH7_VmMagicCallFunc(ph7_vm *pVm)` |
|        4 | 1739 | `{` |
|        - | 1740 | `	SyString sName;` |
|      130 | 1741 | `	if( pVm->pMagicCallFunc == 0 ){` |
|       14 | 1742 | `		SyStringInitFromBuf(&sName,"__call",sizeof("__call")-1);` |
|       15 | 1743 | `		if( PH7_NewForeignFunction(&(*pVm),&sName,VmMagicCallDispatch,0,` |
|       14 | 1744 | `			&pVm->pMagicCallFunc) != SXRET_OK ){` |
|      ! 0 | 1745 | `			pVm->pMagicCallFunc = 0;` |
|      ! 0 | 1746 | `		}` |
|        5 | 1747 | `	}` |
|      130 | 1748 | `	return pVm->pMagicCallFunc;` |
|        4 | 1749 | `}` |
|        - | 1750 |  |
