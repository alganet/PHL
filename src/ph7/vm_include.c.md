# src/ph7/vm_include.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 769/842 lines (91.33%)

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
|        4 |   26 | `static void VmReportCompileFatal(ph7_vm *pVm,SyBlob *pMsg,sxu32 nLine)` |
|        2 |   27 | `{` |
|        - |   28 | `	SyBlob sOut;` |
|        6 |   29 | `	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        6 |   30 | `	if( pVm->pEngine->xConf.xErr == 0 \|\| SyBlobLength(pMsg) < 1 ){` |
|      ! 0 |   31 | `		return;` |
|        - |   32 | `	}` |
|        6 |   33 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        6 |   34 | `	SyBlobAppend(&sOut,SyBlobData(pMsg),SyBlobLength(pMsg));` |
|        6 |   35 | `	if( pFile ){` |
|        6 |   36 | `		SyBlobFormat(&sOut," in %.*s on line %u",(int)pFile->nByte,pFile->zString,nLine);` |
|        2 |   37 | `	}` |
|        6 |   38 | `	PH7_GenAppendFatalTrace(&(*pVm),&sOut,PH7_FATAL_TRACE_COMPILE);` |
|        - |   39 | `	/* The label, php's two copies, the error_reporting() gate and` |
|        - |   40 | `	 * error_get_last() are the shared compile-diagnostic emitter's, so eval()'s` |
|        - |   41 | `	 * fatal cannot drift from the compiler's own. E_COMPILE_ERROR is php's bit` |
|        - |   42 | `	 * for it, and the BARE sentence is what pMsg still holds. */` |
|        8 |   43 | `	PH7_VmEmitCompileDiagnostic(&(*pVm),64 /* E_COMPILE_ERROR */,"Fatal error",` |
|        4 |   44 | `		(const char *)SyBlobData(&sOut),SyBlobLength(&sOut),` |
|        4 |   45 | `		(const char *)SyBlobData(pMsg),SyBlobLength(pMsg),nLine);` |
|        6 |   46 | `	SyBlobRelease(&sOut);` |
|        4 |   47 | `}` |
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
|    22708 |   59 | `PH7_PRIVATE void PH7_VmIncFramePush(ph7_vm *pVm,const char *zName,SyString *pPath)` |
|        5 |   60 | `{` |
|        - |   61 | `	VmIncFrame sInc;` |
|    22713 |   62 | `	VmFrame *pCaller = pVm->pFrame;` |
|        - |   63 | `	SyString *pFile;` |
|    22713 |   64 | `	SyZero(&sInc,sizeof(sInc));` |
|    54739 |   65 | `	while( pCaller && pCaller->pParent` |
|    43390 |   66 | `	    && (pCaller->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|    10569 |   67 | `		pCaller = pCaller->pParent;` |
|        5 |   68 | `	}` |
|    22713 |   69 | `	sInc.pFrame = (void *)pCaller;` |
|    22713 |   70 | `	sInc.nLine = pVm->nCurLine;` |
|    22713 |   71 | `	sInc.zName = zName;` |
|        - |   72 | `	/* The unit this construct is LOADING is already on the include stack by the` |
|        - |   73 | `	 * time we get here, so it must not be mistaken for the one the construct is` |
|        - |   74 | `	 * WRITTEN in: pop it for the question and put it back. */` |
|    22713 |   75 | `	if( pPath ){` |
|    11797 |   76 | `		SyString sTop = *(SyString *)SySetPeek(&pVm->aFiles);` |
|    11797 |   77 | `		(void)SySetPop(&pVm->aFiles);` |
|    11797 |   78 | `		pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|    11797 |   79 | `		if( pFile ){` |
|    11797 |   80 | `			sInc.sFile = *pFile;` |
|     5896 |   81 | `		}` |
|    11797 |   82 | `		SySetPut(&pVm->aFiles,(const void *)&sTop);` |
|     5901 |   83 | `	}else{` |
|    10921 |   84 | `		pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|    10921 |   85 | `		if( pFile ){` |
|    10921 |   86 | `			sInc.sFile = *pFile;` |
|     5458 |   87 | `		}` |
|        - |   88 | `	}` |
|    22713 |   89 | `	if( pPath ){` |
|    11797 |   90 | `		sInc.sPath = *pPath;` |
|     5896 |   91 | `	}` |
|    22713 |   92 | `	SySetPut(&pVm->aIncFrame,(const void *)&sInc);` |
|    22713 |   93 | `}` |
|    22708 |   94 | `PH7_PRIVATE void PH7_VmIncFramePop(ph7_vm *pVm)` |
|        5 |   95 | `{` |
|    22713 |   96 | `	(void)SySetPop(&pVm->aIncFrame);` |
|    22713 |   97 | `}` |
|        - |   98 | `/*` |
|        - |   99 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  100 | ` * Refer to the eval() language construct implementation for more` |
|        - |  101 | ` * information.` |
|        - |  102 | ` */` |
|    31317 |  103 | `PH7_PRIVATE sxi32 VmEvalChunk(` |
|        - |  104 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  105 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  106 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  107 | `	int iFlags,         /* Compile flag */` |
|        - |  108 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  109 | `	)` |
|        5 |  110 | `{` |
|        - |  111 | `	SySet *pByteCode,aByteCode;` |
|    31322 |  112 | `	ProcConsumer xErr = 0;` |
|    31322 |  113 | `	void *pErrData = 0;` |
|        - |  114 | `	ph7_gen_state sSavedGen;` |
|        - |  115 | `	int bNested;` |
|    31322 |  116 | `	int bSavedUnitDecl = pVm->bUnitDecl;` |
|    31322 |  117 | `	sxu32 nUnitDeclMark = SySetUsed(&pVm->aUnitDecl);` |
|    31322 |  118 | `	sxu32 nHiddenMark = SySetUsed(&pVm->aHiddenClass);` |
|    31322 |  119 | `	sxi32 rcThrow = SXRET_OK; /* a ParseError raised for a failed compile, or a throw the` |
|        - |  120 | `	                           * evaluated chunk raised — either way the caller must unwind */` |
|        - |  121 | `	/* Initialize bytecode container */` |
|    31322 |  122 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    31322 |  123 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  124 | `	/* Reset the code generator */` |
|    31322 |  125 | `	if( bTrueReturn ){` |
|        - |  126 | `		/* Included file,log compile-time errors */` |
|    11961 |  127 | `		xErr = pVm->pEngine->xConf.xErr;` |
|    11961 |  128 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     5978 |  129 | `	}` |
|        - |  130 | `	/* A non-zero cursor means an OUTER compile is in flight — this eval/include` |
|        - |  131 | `	 * was reached from inside it (an autoload fired while resolving a base class).` |
|        - |  132 | `	 * Wiping the generator would corrupt that outer parse, so snapshot it and hand` |
|        - |  133 | `	 * the nested unit a fresh one; a plain runtime eval/include just resets. */` |
|    31322 |  134 | `	bNested = (pVm->sCodeGen.pIn != 0);` |
|    31322 |  135 | `	if( bNested ){` |
|        7 |  136 | `		PH7_CompilerSaveState(pVm,&sSavedGen,xErr,pErrData);` |
|        4 |  137 | `	}else{` |
|    31316 |  138 | `		PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  139 | `	}` |
|        - |  140 | `	/* An include/require's PARSE error is php's catchable ParseError, thrown from` |
|        - |  141 | `	 * the include and printed only if it goes uncaught -- so the generator must not` |
|        - |  142 | `	 * print it. A refusal of E_ERROR severity is php's uncatchable compile fatal and` |
|        - |  143 | `	 * still prints where it is raised. */` |
|    31322 |  144 | `	pVm->sCodeGen.bParseThrows = (bTrueReturn && pCtx) ? 1 : 0;` |
|        - |  145 | `	/* Swap bytecode container */` |
|    31322 |  146 | `	pByteCode = pVm->pByteContainer;` |
|    31322 |  147 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  148 | `	/* Compile the chunk, logging what it declares. The log is OFF while the chunk` |
|        - |  149 | `	 * runs: what its statements declare at run time is theirs to keep, even when an` |
|        - |  150 | `	 * outer unit this one was autoloaded from goes on to fail. */` |
|    31322 |  151 | `	pVm->bUnitDecl = 1;` |
|    31322 |  152 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    31322 |  153 | `	pVm->bUnitDecl = 0;` |
|    31322 |  154 | `	PH7_VmUnitDeclEnd(pVm,nUnitDeclMark,pVm->sCodeGen.nErr > 0);` |
|    31322 |  155 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|      919 |  156 | `		SySetTruncate(&pVm->aHiddenClass,nHiddenMark);` |
|      462 |  157 | `	}else{` |
|        - |  158 | `		/* What php does not early-bind waits for its statement. */` |
|    30408 |  159 | `		PH7_VmHideClasses(pVm,nHiddenMark);` |
|        - |  160 | `	}` |
|        - |  161 | `	/* Record THIS unit's error count where the nested state restore below cannot` |
|        - |  162 | `	 * wipe it — VmExecDeferredClass reads it to detect a failed re-compile. */` |
|    31322 |  163 | `	pVm->nLastEvalErr = pVm->sCodeGen.nErr;` |
|    46974 |  164 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  165 | `		/* Compilation error. php makes this a CATCHABLE ParseError -- for eval()` |
|        - |  166 | `		 * and for include/require alike -- where PHL merely returned false, so` |
|        - |  167 | ``		 * `eval('bad syntax')` silently produced a value and a broken include`` |
|        - |  168 | `		 * printed its diagnostic and then CARRIED ON, leaving a half-compiled unit` |
|        - |  169 | `		 * behind for later code to trip over. A refusal of E_ERROR severity is` |
|        - |  170 | `		 * php's uncatchable compile fatal instead: it has already printed itself,` |
|        - |  171 | `		 * and the program stops here. */` |
|      919 |  172 | `		SyBlob *pErr = &pVm->sCodeGen.sErrBuf;` |
|      919 |  173 | `		sxu32 nErrLine = pVm->sCodeGen.nFirstErrLine;` |
|      919 |  174 | `		if( SyBlobLength(&pVm->sCodeGen.sFirstErr) > 0 ){` |
|      919 |  175 | `			pErr = &pVm->sCodeGen.sFirstErr;` |
|      457 |  176 | `		}` |
|      919 |  177 | `		if( pVm->sCodeGen.nFatal > 0 ){` |
|       15 |  178 | `			if( pCtx ){` |
|       15 |  179 | `				ph7_result_bool(pCtx,0);` |
|        6 |  180 | `			}` |
|       15 |  181 | `			if( !bTrueReturn ){` |
|        - |  182 | `				/* eval(): the generator had no consumer, so say it here. */` |
|        6 |  183 | `				VmReportCompileFatal(pVm,&pVm->sCodeGen.sFirstErr,nErrLine);` |
|        2 |  184 | `			}` |
|        - |  185 | `			/* php exits 255 and runs nothing else. The include builtins cascade a` |
|        - |  186 | `			 * requested halt exactly as they do for an exit() inside the file. */` |
|       15 |  187 | `			pVm->iExitStatus = 255;` |
|       15 |  188 | `			pVm->bHaltRequested = 1;` |
|       15 |  189 | `			rcThrow = PH7_ABORT;` |
|      913 |  190 | `		}else if( pCtx ){` |
|        - |  191 | `			/* php's ParseError names the OFFENDING file and line, not the line the` |
|        - |  192 | `			 * include was written on -- the throw is stamped from nCurLine, so aim` |
|        - |  193 | `			 * it at the refusal and put the caller's line back afterwards. */` |
|      907 |  194 | `			sxu32 nSaveLine = pVm->nCurLine;` |
|      907 |  195 | `			ph7_result_bool(pCtx,0);` |
|      907 |  196 | `			if( nErrLine > 0 ){` |
|      907 |  197 | `				pVm->nCurLine = nErrLine;` |
|      451 |  198 | `			}` |
|      907 |  199 | `			if( SyBlobLength(pErr) > 0 ){` |
|     1358 |  200 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","%.*s",` |
|      902 |  201 | `					(int)SyBlobLength(pErr),(const char *)SyBlobData(pErr));` |
|      456 |  202 | `			}else{` |
|      ! 0 |  203 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","syntax error");` |
|        - |  204 | `			}` |
|      907 |  205 | `			pVm->nCurLine = nSaveLine;` |
|      451 |  206 | `		}` |
|      462 |  207 | `	}else{` |
|        - |  208 | `		/* Mount any newly defined classes. Skipped while the VM is still` |
|        - |  209 | `		 * initializing (builtin chunks): mounting evaluates class-constant/` |
|        - |  210 | `		 * static initializers and installs reference-table entries, and the` |
|        - |  211 | `		 * runtime structures those need (apRefObj, the per-exec object pool` |
|        - |  212 | `		 * baseline) do not exist before PH7_VmMakeReady — which mounts every` |
|        - |  213 | `		 * class unconditionally anyway. */` |
|        - |  214 | `		SyHashEntry *pEntry;` |
|        - |  215 | `		ph7_class *pClass;` |
|        - |  216 | `		ph7_value sResult; /* Return value */` |
|        - |  217 | `		sxi32 rc;` |
|    30408 |  218 | `		if( pVm->nMagic != PH7_VM_INIT ){` |
|    21963 |  219 | `			SyHashResetLoopCursor(&pVm->hClass);` |
| 13789544 |  220 | `			while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
| 13756609 |  221 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  222 | `				/* Only mount classes that haven't been mounted yet */` |
| 13756609 |  223 | `				if( !pClass->bMounted ){` |
|   217821 |  224 | `					rc = VmMountUserClass(pVm,pClass);` |
|   217821 |  225 | `					if( rc != SXRET_OK ){` |
|        - |  226 | `						/* Mount failure (likely memory error) */` |
|        3 |  227 | `						if( pCtx ){` |
|        3 |  228 | `							ph7_result_bool(pCtx,0);` |
|        1 |  229 | `						}` |
|        3 |  230 | `						goto Cleanup;` |
|        - |  231 | `					}` |
|   108907 |  232 | `				}` |
|        5 |  233 | `			}` |
|    10978 |  234 | `		}` |
|    30406 |  235 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  236 | `			/* Out of memory */` |
|      ! 0 |  237 | `			if( pCtx ){` |
|      ! 0 |  238 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  239 | `			}` |
|      ! 0 |  240 | `			goto Cleanup;` |
|        - |  241 | `		}` |
|    30406 |  242 | `		if( bTrueReturn ){` |
|        - |  243 | `			/* php's include/require answer INT 1 when the file returned nothing` |
|        - |  244 | ``			 * of its own — not `true`. It is the value a script tests, stores`` |
|        - |  245 | ``			 * and compares, and `include $f === true` was false on php and true`` |
|        - |  246 | `			 * here. */` |
|    11917 |  247 | `			PH7_MemObjInitFromInt(pVm,&sResult,1);` |
|     5961 |  248 | `		}else{` |
|        - |  249 | `			/* Assume a null return value */` |
|    18494 |  250 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  251 | `		}` |
|        - |  252 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here` |
|        - |  253 | `		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by` |
|        - |  254 | `		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the` |
|        - |  255 | `		 * native-nesting fatal instead of overflowing the C stack. The PHP` |
|        - |  256 | `		 * call-depth cap is OP_CALL-only.` |
|        - |  257 | `		 *` |
|        - |  258 | `		 * The nested exec shares the caller's VM frame, so a throw the CALLER's own` |
|        - |  259 | `		 * try catches runs that catch IN PLACE and comes back as a status — which was` |
|        - |  260 | `		 * dropped here, and the statement php abandons then carried on after the catch` |
|        - |  261 | `` 		 * had already run (`try { $r = eval('throw new E;'); echo "x"; } catch …` `` |
|        - |  262 | `		 * printed the catch AND the echo). Same rule and same guard as the match-arm /` |
|        - |  263 | `		 * switch-case / property-default sites: compare the recorded-resume fields` |
|        - |  264 | `		 * against a pre-exec SNAPSHOT, never against 0. */` |
|        - |  265 | `		{` |
|    30406 |  266 | `			const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|    30406 |  267 | `			const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|    30406 |  268 | `			rc = VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    30406 |  269 | `			if( pCtx ){` |
|        - |  270 | `				/* Set the execution result */` |
|    21797 |  271 | `				ph7_result_value(pCtx,&sResult);` |
|    10896 |  272 | `			}` |
|    30406 |  273 | `			PH7_MemObjRelease(&sResult);` |
|    30406 |  274 | `			if( rc != PH7_ABORT && VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){` |
|     3605 |  275 | `				rcThrow = PH7_EXCEPTION;` |
|     1801 |  276 | `			}` |
|        - |  277 | `		}` |
|        - |  278 | `	}` |
|    15664 |  279 | `Cleanup:` |
|        - |  280 | `	/* Cleanup the mess left behind */` |
|    31322 |  281 | `	pVm->pByteContainer = pByteCode;` |
|        - |  282 | `	/* Hand back the call-site cache records this chunk's OP_CALLs claimed, BEFORE the` |
|        - |  283 | `	 * instructions holding their indices disappear. */` |
|    31322 |  284 | `	PH7_VmCallSiteReleaseChunk(pVm,&aByteCode);` |
|    31322 |  285 | `	SySetRelease(&aByteCode);` |
|        - |  286 | `	/* Restore the outer compile's generator state if this was a nested unit. */` |
|    31322 |  287 | `	if( bNested ){` |
|        7 |  288 | `		PH7_CompilerRestoreState(pVm,&sSavedGen);` |
|        3 |  289 | `	}` |
|    31322 |  290 | `	pVm->bUnitDecl = bSavedUnitDecl;` |
|        - |  291 | `	/* A ParseError raised above must reach the caller so the VM unwinds the rest` |
|        - |  292 | ``	 * of the statement; returning OK left `eval('bad'); echo 'x';` running the`` |
|        - |  293 | `	 * echo even though php had already thrown. */` |
|    31322 |  294 | `	return rcThrow;` |
|        5 |  295 | `}` |
|        - |  296 | `/*` |
|        - |  297 | ` * Execute a deferred class declaration (OP_CLASS_DEFER — see the deferral` |
|        - |  298 | ` * block comment in compile_class.c). At this point the declaration's` |
|        - |  299 | ` * execution order has been honored: any spl_autoload_register() statement` |
|        - |  300 | ` * that precedes it in the program has RUN, so each recorded dependency either` |
|        - |  301 | ` * resolves (possibly by autoload, fired inside PH7_VmExtractClass) or is` |
|        - |  302 | ` * genuinely missing. A missing one is reported through *ppMissing — the` |
|        - |  303 | ` * OP_CLASS_DEFER dispatcher throws php's catchable` |
|        - |  304 | `` * `Class/Interface/Trait "X" not found` Error. Once the dependencies resolve,`` |
|        - |  305 | ` * the captured chunk re-compiles through VmEvalChunk (which also mounts the` |
|        - |  306 | ` * newly installed classes); a compile failure there (e.g. a body syntax error` |
|        - |  307 | ` * whose diagnosis was deferred along with the declaration) has already been` |
|        - |  308 | ` * reported through the engine's error consumer, so it aborts execution.` |
|        - |  309 | ` * The site is idempotent: bDone short-circuits re-execution (a loop around an` |
|        - |  310 | ` * anonymous class instantiates the same installed class, php's` |
|        - |  311 | ` * one-class-per-site semantics).` |
|        - |  312 | ` */` |
|      180 |  313 | `PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing)` |
|        5 |  314 | `{` |
|        - |  315 | `	VmDeferredReq *aReq;` |
|        - |  316 | `	sxu8 bHaltIn;` |
|        - |  317 | `	sxu32 n;` |
|      185 |  318 | `	*ppMissing = 0;` |
|      185 |  319 | `	if( pDefer->bDone ){` |
|        5 |  320 | `		return SXRET_OK;` |
|        - |  321 | `	}` |
|      181 |  322 | `	aReq = (VmDeferredReq *)SySetBasePtr(&pDefer->aRequired);` |
|      285 |  323 | `	for( n = 0 ; n < SySetUsed(&pDefer->aRequired) ; ++n ){` |
|      121 |  324 | `		if( PH7_VmExtractClass(&(*pVm),aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|       14 |  325 | `			*ppMissing = &aReq[n];` |
|       14 |  326 | `			return SXRET_OK; /* caller throws */` |
|        - |  327 | `		}` |
|       57 |  328 | `	}` |
|      169 |  329 | `	if( pDefer->sAnonName.nByte > 0 ){` |
|       10 |  330 | `		pVm->sDeferAnonName = pDefer->sAnonName;` |
|        4 |  331 | `	}` |
|      169 |  332 | `	pVm->bDeclQuietNext = pDefer->bChecked;` |
|      169 |  333 | `	bHaltIn = pVm->bHaltRequested;` |
|      169 |  334 | `	VmEvalChunk(&(*pVm),0,&pDefer->sText,PH7_PHP_ONLY,TRUE);` |
|      169 |  335 | `	pVm->bDeclQuietNext = 0;` |
|      169 |  336 | `	pVm->sDeferAnonName.zString = 0;` |
|      169 |  337 | `	pVm->sDeferAnonName.nByte = 0;` |
|      169 |  338 | `	if( pVm->bHaltRequested && !bHaltIn ){` |
|        - |  339 | `		/* The re-compiled declaration RAN and halted -- a variance pair refused` |
|        - |  340 | `		 * where it settles, a loader's throw made fatal there, an exit() in a` |
|        - |  341 | `		 * loader. The statement after the declaration must not run. */` |
|        5 |  342 | `		return SXERR_ABORT;` |
|        - |  343 | `	}` |
|      160 |  344 | `	if( pVm->nLastEvalErr > 0` |
|      165 |  345 | `	 \|\| PH7_VmExtractClass(&(*pVm),pDefer->sSelfName.zString,pDefer->sSelfName.nByte,FALSE,0) == 0 ){` |
|        - |  346 | `		/* The re-compile failed — a deferred-along syntax error or a` |
|        - |  347 | `		 * redeclaration fatal. It was already reported through the engine's` |
|        - |  348 | `		 * error consumer; halt like a fatal (php exits 255 for both). */` |
|      ! 0 |  349 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  350 | `		return SXERR_ABORT;` |
|        - |  351 | `	}` |
|      165 |  352 | `	pDefer->bDone = 1;` |
|      165 |  353 | `	return SXRET_OK;` |
|       95 |  354 | `}` |
|        - |  355 | `/*` |
|        - |  356 | ` * php names an eval()'d compilation unit after the SITE that evaluated it:` |
|        - |  357 | `` * `<file>(<line>) : eval()'d code`, where <file> is the unit the eval() was`` |
|        - |  358 | ` * written in -- itself such a name when the eval is nested -- and <line> the line` |
|        - |  359 | ` * it sits on. That name is not a decoration on one message: it is the unit's` |
|        - |  360 | `` * identity, so `__FILE__`, every diagnostic's location, a Throwable's getFile(),`` |
|        - |  361 | ` * and ReflectionFunction/ReflectionClass::getFileName() for anything the chunk` |
|        - |  362 | ` * declares all read it. PHL left the chunk sharing its caller's name, so a` |
|        - |  363 | ` * template engine that compiles to PHP and evals it (twig, and every cache-less` |
|        - |  364 | ` * renderer of that shape) reported positions in the COMPILER's file -- and the` |
|        - |  365 | ` * caller could not map them back, because its own class had the compiler's name` |
|        - |  366 | ` * on it too.` |
|        - |  367 | ` *` |
|        - |  368 | ` * Interned per site rather than per call: the name is copied by value into every` |
|        - |  369 | ` * function and class record compiled out of the chunk, so it must outlive the` |
|        - |  370 | ` * eval, and an eval inside a loop repeats one site.` |
|        - |  371 | ` */` |
|    10916 |  372 | `static sxi32 VmEvalUnitName(ph7_vm *pVm,SyString *pOut)` |
|        5 |  373 | `{` |
|    10921 |  374 | `	SyString *pHost = PH7_VmExecutingUnitFile(&(*pVm));` |
|        - |  375 | `	SyString *aName;` |
|        - |  376 | `	SyString sKey;` |
|        - |  377 | `	SyBlob sName;` |
|        - |  378 | `	char *zDup;` |
|        - |  379 | `	sxu32 n;` |
|    10921 |  380 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|    10921 |  381 | `	if( pHost && pHost->nByte > 0 ){` |
|    10921 |  382 | `		SyBlobAppend(&sName,pHost->zString,pHost->nByte);` |
|     5458 |  383 | `	}` |
|    10921 |  384 | `	SyBlobFormat(&sName,"(%u) : eval()'d code",pVm->nCurLine);` |
|    10921 |  385 | `	if( SyBlobLength(&sName) < 1 ){` |
|      ! 0 |  386 | `		SyBlobRelease(&sName);` |
|      ! 0 |  387 | `		return SXERR_MEM;` |
|        - |  388 | `	}` |
|    10921 |  389 | `	SyStringInitFromBuf(&sKey,SyBlobData(&sName),SyBlobLength(&sName));` |
|    10921 |  390 | `	aName = (SyString *)SySetBasePtr(&pVm->aEvalFile);` |
|   239877 |  391 | `	for( n = 0 ; n < SySetUsed(&pVm->aEvalFile) ; ++n ){` |
|   239587 |  392 | `		if( SyStringCmp(&sKey,&aName[n],SyMemcmp) == 0 ){` |
|    10630 |  393 | `			*pOut = aName[n];` |
|    10630 |  394 | `			SyBlobRelease(&sName);` |
|    10630 |  395 | `			return SXRET_OK;` |
|        - |  396 | `		}` |
|   114482 |  397 | `	}` |
|      295 |  398 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,sKey.zString,sKey.nByte);` |
|      295 |  399 | `	n = sKey.nByte;` |
|      295 |  400 | `	SyBlobRelease(&sName);` |
|      295 |  401 | `	if( zDup == 0 ){` |
|      ! 0 |  402 | `		return SXERR_MEM;` |
|        - |  403 | `	}` |
|      295 |  404 | `	SyStringInitFromBuf(&sKey,zDup,n);` |
|      295 |  405 | `	if( SySetPut(&pVm->aEvalFile,(const void *)&sKey) != SXRET_OK ){` |
|      ! 0 |  406 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  407 | `		return SXERR_MEM;` |
|        - |  408 | `	}` |
|      295 |  409 | `	*pOut = sKey;` |
|      295 |  410 | `	return SXRET_OK;` |
|     5463 |  411 | `}` |
|        - |  412 | `/*` |
|        - |  413 | ` * value eval(string $code)` |
|        - |  414 | ` *   Evaluate a string as PHP code.` |
|        - |  415 | ` * Parameter` |
|        - |  416 | ` *  code: PHP code to evaluate.` |
|        - |  417 | ` * Return` |
|        - |  418 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  419 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  420 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  421 | ` */` |
|    10922 |  422 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  423 | `{` |
|        - |  424 | `	SyString sChunk;    /* Chunk to evaluate */` |
|        - |  425 | `	sxi32 rc;` |
|    10927 |  426 | `	if( nArg < 1 ){` |
|        - |  427 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  428 | `		ph7_result_null(pCtx);` |
|      ! 0 |  429 | `		return SXRET_OK;` |
|        - |  430 | `	}` |
|        - |  431 | `	/* Chunk to evaluate. Coerced USER-VISIBLY like every other string argument` |
|        - |  432 | `	 * spelled as a language construct: an object with no __toString() is php's` |
|        - |  433 | `	 * Error, where PHL used to hand the parser the literal "Object" and answer a` |
|        - |  434 | `	 * ParseError. */` |
|        - |  435 | `	{` |
|    10927 |  436 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sChunk.zString,(int *)&sChunk.nByte);` |
|    10927 |  437 | `		if( rcSv != SXRET_OK ){` |
|        3 |  438 | `			return rcSv;` |
|        - |  439 | `		}` |
|        - |  440 | `	}` |
|    10925 |  441 | `	if( sChunk.nByte < 1 ){` |
|        - |  442 | `		/* php: eval('') compiles nothing and yields FALSE (a whitespace-only` |
|        - |  443 | `		 * chunk still compiles, and yields NULL through the normal path). */` |
|        5 |  444 | `		ph7_result_bool(pCtx,0);` |
|        5 |  445 | `		return SXRET_OK;` |
|        - |  446 | `	}` |
|        - |  447 | `	/* Eval the chunk.` |
|        - |  448 | `	 *` |
|        - |  449 | `` 	 * php compiles it as though it began right after a `<?php`, which means a `?>` `` |
|        - |  450 | `` 	 * inside it LEAVES php mode: the text after it is echoed and a later `<?php` `` |
|        - |  451 | ``	 * re-enters. `eval('?>' . file_get_contents($f))` is the ordinary way to run a`` |
|        - |  452 | `` 	 * php FILE's bytes, and composer reloads its own `vendor/composer/installed.php` `` |
|        - |  453 | `	 * exactly that way. PHL handed the whole chunk to the compiler as ONE php token` |
|        - |  454 | ``	 * (PH7_PHP_ONLY), so the `?>` was a syntax error and composer could not boot.`` |
|        - |  455 | `	 *` |
|        - |  456 | `	 * Prepending the opening tag and letting the ordinary raw tokenizer split it is` |
|        - |  457 | `	 * php's rule itself, with no second implementation of it. The prefix carries no` |
|        - |  458 | `	 * newline, so every line still reports the number the caller wrote it on. */` |
|        - |  459 | `	{` |
|        - |  460 | `		SyBlob sTagged;` |
|        - |  461 | `		SyString sTaggedStr;` |
|        - |  462 | `		SyString sUnit;` |
|        - |  463 | `		int bUnit;` |
|    10921 |  464 | `		SyBlobInit(&sTagged,&pCtx->pVm->sAllocator);` |
|    10916 |  465 | `		if( SyBlobAppend(&sTagged,"<?php ",sizeof("<?php ")-1) != SXRET_OK` |
|    10921 |  466 | `		 \|\| SyBlobAppend(&sTagged,sChunk.zString,sChunk.nByte) != SXRET_OK ){` |
|      ! 0 |  467 | `			SyBlobRelease(&sTagged);` |
|      ! 0 |  468 | `			return PH7_ContextMemoryError(pCtx);` |
|        - |  469 | `		}` |
|    10921 |  470 | `		SyStringInitFromBuf(&sTaggedStr,SyBlobData(&sTagged),SyBlobLength(&sTagged));` |
|        - |  471 | `		/* php gives eval() a trace frame of its own, exactly as it does an include --` |
|        - |  472 | `		 * argument-less, where an include names the unit it loaded. */` |
|        - |  473 | `		/* Name the chunk BEFORE the trace frame goes on: PH7_VmIncFramePush records` |
|        - |  474 | `		 * an entry whose pFrame is the CURRENT frame, and PH7_VmExecutingUnitFile` |
|        - |  475 | `		 * answers such a frame from the include-stack TOP -- so asking after the push` |
|        - |  476 | `		 * names the unit being loaded rather than the one holding the eval(), and a` |
|        - |  477 | `		 * template engine's compiled chunk came out named after the SCRIPT instead of` |
|        - |  478 | `		 * the library method that evaluated it. */` |
|    10921 |  479 | `		bUnit = (VmEvalUnitName(pCtx->pVm,&sUnit) == SXRET_OK);` |
|    10921 |  480 | `		PH7_VmIncFramePush(pCtx->pVm,"eval",0);` |
|        - |  481 | `		/* Now push the chunk's own unit name. The include-frame entry is what makes` |
|        - |  482 | `		 * the include stack answer for code running at the chunk's TOP level -- which` |
|        - |  483 | `		 * pushes no VmFrame of its own -- so the two go together. */` |
|    10921 |  484 | `		bUnit = bUnit && (SySetPut(&pCtx->pVm->aFiles,(const void *)&sUnit) == SXRET_OK);` |
|    10921 |  485 | `		rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sTaggedStr,0,FALSE);` |
|    10921 |  486 | `		if( bUnit ){` |
|    10921 |  487 | `			(void)SySetPop(&pCtx->pVm->aFiles);` |
|     5458 |  488 | `		}` |
|    10921 |  489 | `		PH7_VmIncFramePop(pCtx->pVm);` |
|    10921 |  490 | `		SyBlobRelease(&sTagged);` |
|        - |  491 | `	}` |
|    10921 |  492 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  493 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       78 |  494 | `		return PH7_ABORT;` |
|        - |  495 | `	}` |
|        - |  496 | `	/* Propagate a ParseError from a failed compile (php unwinds; PHL used to` |
|        - |  497 | `	 * keep executing the statement that contained the eval). */` |
|    10847 |  498 | `	return rc;` |
|     5466 |  499 | `}` |
|        - |  500 | `/*` |
|        - |  501 | ` * Check if a file path is already included.` |
|        - |  502 | ` */` |
|    11830 |  503 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        5 |  504 | `{` |
|        - |  505 | `	SyString *aEntries;` |
|        - |  506 | `	sxu32 n;` |
|    11835 |  507 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  508 | `	/* Perform a linear search */` |
| 33054963 |  509 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 33043239 |  510 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  511 | `			/* Already included */` |
|      110 |  512 | `			return TRUE;` |
|        - |  513 | `		}` |
| 16521569 |  514 | `	}` |
|    11729 |  515 | `	return FALSE;` |
|     5920 |  516 | `}` |
|        - |  517 | `/*` |
|        - |  518 | ` * Push a file path in the appropriate VM container.` |
|        - |  519 | ` */` |
|    20275 |  520 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 |  521 | `{` |
|        - |  522 | `	SyString sPath;` |
|        - |  523 | `	char *zDup;` |
|        - |  524 | `	sxi32 rc;` |
|    20280 |  525 | `	if( nLen < 0 ){` |
|     8450 |  526 | `		nLen = SyStrlen(zPath);` |
|     4217 |  527 | `	}` |
|        - |  528 | `	/* Duplicate the file path first */` |
|    20280 |  529 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    20280 |  530 | `	if( zDup == 0 ){` |
|      ! 0 |  531 | `		return SXERR_MEM;` |
|        - |  532 | `	}` |
|        - |  533 | ``	/* php's lint mode (`phl -l`) hands the compiler the file handle it opened`` |
|        - |  534 | `	 * under the name it was GIVEN -- it never runs the unit, so nothing needs the` |
|        - |  535 | `	 * canonical name -- where a run expands the main script's path first. That is` |
|        - |  536 | ``	 * why a parse error names `./x.php` under `-l` and `/abs/dir/x.php` under a`` |
|        - |  537 | `	 * run, and it is the file name a lint diagnostic is matched against.` |
|        - |  538 | `	 * Only the MAIN unit: an include compiled from a checked file would still` |
|        - |  539 | `	 * want the canonical name, and lint mode compiles no includes anyway. */` |
|    20280 |  540 | `	if( bMain && pVm->bSyntaxCheck ){` |
|      353 |  541 | `		goto Install;` |
|        - |  542 | `	}` |
|        - |  543 | `#ifdef __UNIXES__` |
|        - |  544 | `	/* php records the REALPATH'd absolute path for __FILE__/__DIR__/getFileName` |
|        - |  545 | `	 * and include-once dedup: realpath() resolves '.', '..' and symlinks (e.g.` |
|        - |  546 | `	 * macOS /tmp -> /private/tmp). It needs an existing file, so on failure keep` |
|        - |  547 | `	 * the raw path (eval'd code, php:///data:// wrappers, a missing include that` |
|        - |  548 | `	 * errors elsewhere). */` |
|        - |  549 | `	{` |
|    19925 |  550 | `		char *zReal = realpath(zDup,0); /* POSIX: malloc'd result */` |
|    19925 |  551 | `		if( zReal ){` |
|    19297 |  552 | `			sxu32 nReal = SyStrlen(zReal);` |
|    19297 |  553 | `			char *zRealDup = SyMemBackendStrDup(&pVm->sAllocator,zReal,nReal);` |
|    19297 |  554 | `			free(zReal);` |
|    19297 |  555 | `			if( zRealDup ){` |
|    19297 |  556 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|    19297 |  557 | `				zDup = zRealDup;` |
|    19297 |  558 | `				nLen = (int)nReal;` |
|     9643 |  559 | `			}` |
|     9643 |  560 | `		}` |
|        - |  561 | `	}` |
|        - |  562 | `#endif` |
|        - |  563 | `#ifdef __WINNT__` |
|        - |  564 | `	/* Windows counterpart of the realpath() above: canonicalize to an absolute` |
|        - |  565 | `	 * path (resolves '.'/'..' , '/' -> '\'), case-preserved to match php — NOT` |
|        - |  566 | `	 * lowercased as the legacy PH7 code did. Like the POSIX branch, keep the raw` |
|        - |  567 | `	 * path when it does not name an existing file (the "Command line code" marker,` |
|        - |  568 | `	 * eval'd code, stream wrappers). _fullpath() is lexical, so pair it with a VFS` |
|        - |  569 | `	 * existence check. */` |
|        - |  570 | `	{` |
|        5 |  571 | `		char *zFull = _fullpath(0,zDup,0); /* MSVC CRT: malloc'd absolute path */` |
|        5 |  572 | `		if( zFull ){` |
|        5 |  573 | `			if( pVm->pEngine->pVfs && pVm->pEngine->pVfs->xFileExists && pVm->pEngine->pVfs->xFileExists(zFull) == PH7_OK ){` |
|        5 |  574 | `				sxu32 nFull = SyStrlen(zFull);` |
|        5 |  575 | `				char *zFullDup = SyMemBackendStrDup(&pVm->sAllocator,zFull,nFull);` |
|        5 |  576 | `				if( zFullDup ){` |
|        5 |  577 | `					SyMemBackendFree(&pVm->sAllocator,zDup);` |
|        5 |  578 | `					zDup = zFullDup;` |
|        5 |  579 | `					nLen = (int)nFull;` |
|        - |  580 | `				}` |
|        - |  581 | `			}` |
|        5 |  582 | `			free(zFull);` |
|        - |  583 | `		}` |
|        - |  584 | `	}` |
|        - |  585 | `#endif` |
|        - |  586 | ``	/* A `phar://` url has no realpath() to ask, so its own canonical form is`` |
|        - |  587 | `	 * built here: the archive half as the url spells it, the ENTRY half with its` |
|        - |  588 | ``	 * `.` and `..` segments collapsed the way the archive's reader already`` |
|        - |  589 | `	 * resolves them. Without it two spellings of one entry are two rows in the` |
|        - |  590 | `	 * once-registry.` |
|        - |  591 | `	 *` |
|        - |  592 | `	 * Behind the same guard as ext/phar itself: vm_phar.c's whole body, and the` |
|        - |  593 | ``	 * prototype block this calls into, are `#ifndef PH7_DISABLE_BUILTIN_FUNC`, so`` |
|        - |  594 | `	 * the tiny build has no archive reader to ask and no phar:// url to ask about. */` |
|        - |  595 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  596 | `	{` |
|        - |  597 | `		SyBlob sPhar;` |
|    19930 |  598 | `		SyBlobInit(&sPhar,&pVm->sAllocator);` |
|    19930 |  599 | `		if( PH7_PharCanonicalUrl(pVm,zDup,nLen,&sPhar) ){` |
|       30 |  600 | `			char *zPharDup = SyMemBackendStrDup(&pVm->sAllocator,` |
|       20 |  601 | `				(const char *)SyBlobData(&sPhar),SyBlobLength(&sPhar));` |
|       20 |  602 | `			if( zPharDup ){` |
|       20 |  603 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       20 |  604 | `				zDup = zPharDup;` |
|       20 |  605 | `				nLen = (int)SyBlobLength(&sPhar);` |
|       10 |  606 | `			}` |
|       10 |  607 | `		}` |
|    19930 |  608 | `		SyBlobRelease(&sPhar);` |
|     9957 |  609 | `	}` |
|        - |  610 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    10143 |  611 | `Install:` |
|        - |  612 | `	/* Install the file path */` |
|    20280 |  613 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    20280 |  614 | `	if( !bMain ){` |
|    11835 |  615 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  616 | `			/* Already included */` |
|      110 |  617 | `			*pNew = 0;` |
|       57 |  618 | `		}else{` |
|        - |  619 | `			/* Insert in the corresponding container */` |
|    11729 |  620 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    11729 |  621 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  622 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  623 | `				return rc;` |
|        - |  624 | `			}` |
|    11729 |  625 | `			*pNew = 1;` |
|        - |  626 | `		}` |
|     5915 |  627 | `	}` |
|    20280 |  628 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    20280 |  629 | `	return SXRET_OK;` |
|    10137 |  630 | `}` |
|        - |  631 | `/*` |
|        - |  632 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  633 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  634 | ` * indicates failure.` |
|        - |  635 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  636 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  637 | ` * operations.` |
|        - |  638 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  639 | ` * this function is a no-op.` |
|        - |  640 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  641 | ` * constructs for more information.` |
|        - |  642 | ` */` |
|    11868 |  643 | `static sxi32 VmExecIncludedFile(` |
|        - |  644 | `	 ph7_context *pCtx,   /* Call Context */` |
|        - |  645 | `	 SyString *pPath,     /* Script path or URL*/` |
|        - |  646 | `	 int IncludeOnce,     /* TRUE if called from include_once() or require_once() */` |
|        - |  647 | `	 const char **pzWhy   /* OUT: php's reason for a failed open, for the caller's warning */` |
|        - |  648 | `	 )` |
|        5 |  649 | `{` |
|        - |  650 | `	sxi32 rc;` |
|    11873 |  651 | `	if( pzWhy ){` |
|    11841 |  652 | `		*pzWhy = "operation failed";` |
|     5918 |  653 | `	}` |
|        - |  654 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  655 | `	const ph7_io_stream *pStream;` |
|        - |  656 | `	SyBlob sContents;` |
|        - |  657 | `	void *pHandle;` |
|        - |  658 | `	ph7_vm *pVm;` |
|        - |  659 | `	int isNew;` |
|        - |  660 | `	int bFellBack;` |
|        - |  661 | `	/* Initialize fields */` |
|    11873 |  662 | `	pVm = pCtx->pVm;` |
|    11873 |  663 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|    11873 |  664 | `	isNew = 0;` |
|    11873 |  665 | `	bFellBack = 0;` |
|        - |  666 | `	/* Extract the associated stream. The lookup ADVANCES the pointer past the` |
|        - |  667 | `	 * scheme, so it walks a copy: advancing the caller's SyString left its nByte` |
|        - |  668 | `	 * describing the whole url and its zString seven bytes in, and the failure` |
|        - |  669 | `	 * message below then printed that many bytes from there -- past the end of the` |
|        - |  670 | ``	 * string, so `include 'file:///nope'` reported whatever followed it in memory. */`` |
|        - |  671 | `	{` |
|    11873 |  672 | `		const char *zOpen = pPath->zString;` |
|    11873 |  673 | `		pStream = PH7_VmGetStreamDevice(pVm,&zOpen,(int)pPath->nByte);` |
|    11873 |  674 | `		if( pStream == 0 ){` |
|        - |  675 | `			/* A scheme NOBODY is registered under is not a refusal in php: its` |
|        - |  676 | `			 * lookup warns, forgets the protocol, and hands the WHOLE uri --` |
|        - |  677 | `			 * scheme and all -- to the plain-files wrapper, which resolves it` |
|        - |  678 | `			 * against the include_path like any other relative name. So` |
|        - |  679 | ``			 * `include 'zzz://hit.php'` warns once and then RUNS ./zzz:/hit.php,`` |
|        - |  680 | ``			 * where this engine reported `Invalid argument` and included`` |
|        - |  681 | `			 * nothing. The two cases php words differently are excluded here and` |
|        - |  682 | ``			 * keep the failing path below: a `file://` with an authority php`` |
|        - |  683 | ``			 * will not reach, and a `file://` the configuration switched off. */`` |
|       19 |  684 | `			int nScheme = 0;` |
|       19 |  685 | `			if( PH7_VmStreamDeviceIsRemoteHost(pPath->zString,(int)pPath->nByte,&nScheme) ){` |
|        - |  686 | ``				/* `file://host/path`. A wrapper WAS found and declined the name,`` |
|        - |  687 | `				 * so php says so in its own sentence and gives the open a reason` |
|        - |  688 | `				 * of the lookup's rather than an errno nothing set. The failed` |
|        - |  689 | `				 * open's own line is the caller's, which is why only the first` |
|        - |  690 | `				 * half is raised here. */` |
|        4 |  691 | `				PH7_VmThrowWarningFmt(pVm,"%s(): Remote host file access not supported, %.*s",` |
|        2 |  692 | `					ph7_function_name(pCtx),(int)pPath->nByte,pPath->zString);` |
|        3 |  693 | `				if( pzWhy ){` |
|        3 |  694 | `					*pzWhy = "no suitable wrapper could be found";` |
|        1 |  695 | `				}` |
|        3 |  696 | `				return SXERR_IO;` |
|        - |  697 | `			}` |
|       16 |  698 | `			if( nScheme > 0` |
|       17 |  699 | `			 && !PH7_VmStreamSchemeDisabled(pVm,"file",(int)sizeof("file")-1) ){` |
|        - |  700 | `				/* One sentence per wrapper lookup that still SEES the scheme.` |
|        - |  701 | `				 * php resolves the path before it opens it -- and the _once` |
|        - |  702 | `				 * forms resolve it once more, to answer whether it has already` |
|        - |  703 | `				 * been included -- so a miss costs two sentences and a _once` |
|        - |  704 | `				 * miss three. A resolve that succeeds hands an absolute plain` |
|        - |  705 | `				 * path to everything after it, so a hit costs exactly one` |
|        - |  706 | `				 * whichever construct asked. */` |
|       17 |  707 | `				VfsThrowUnknownWrapperWarning(pCtx,pPath->zString);` |
|       17 |  708 | `				pStream = PH7_VmFindStreamDevice(pVm,"file",(int)sizeof("file")-1);` |
|       17 |  709 | `				zOpen = pPath->zString;` |
|       17 |  710 | `				bFellBack = 1;` |
|        8 |  711 | `			}` |
|        8 |  712 | `		}` |
|        - |  713 | `		/*` |
|        - |  714 | `		 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  715 | `		 * in a read-only mode.` |
|        - |  716 | `		 */` |
|    11871 |  717 | `		pHandle = PH7_StreamOpenHandle(pVm,pStream,zOpen,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew,ph7_function_name(pCtx));` |
|        - |  718 | `	}` |
|    11871 |  719 | `	if( pHandle == 0 ){` |
|        - |  720 | `		/* The reason belongs to THIS open and nothing else: read it before any` |
|        - |  721 | `		 * other stream operation can re-arm it. A wrapper that logged one of its` |
|        - |  722 | `		 * own wins; the plain-file wrapper logs none and reports its errno. */` |
|       40 |  723 | `		if( bFellBack ){` |
|        - |  724 | `			/* The lookups the failed resolve above did not spare: the open's` |
|        - |  725 | `			 * own, plus the _once forms' extra resolve. And the reason is the` |
|        - |  726 | `			 * plain-files wrapper's, not the scheme's -- php has stopped talking` |
|        - |  727 | `			 * about the wrapper by the time it words the failure. */` |
|       11 |  728 | `			VfsThrowUnknownWrapperWarning(pCtx,pPath->zString);` |
|       11 |  729 | `			if( IncludeOnce ){` |
|        5 |  730 | `				VfsThrowUnknownWrapperWarning(pCtx,pPath->zString);` |
|        2 |  731 | `			}` |
|       11 |  732 | `			if( pzWhy ){` |
|       11 |  733 | `				*pzWhy = PH7_VfsOpenStrerror(ENOENT);` |
|        5 |  734 | `			}` |
|       11 |  735 | `			return SXERR_IO;` |
|        - |  736 | `		}` |
|       30 |  737 | `		if( pzWhy ){` |
|       25 |  738 | `			*pzWhy = pVm->zOpenErr ? pVm->zOpenErr : PH7_VfsOpenStrerror(errno);` |
|       11 |  739 | `		}` |
|       30 |  740 | `		return SXERR_IO;` |
|        - |  741 | `	}` |
|    11835 |  742 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|    11835 |  743 | `	if( IncludeOnce && !isNew ){` |
|        - |  744 | `		/* Already included */` |
|       41 |  745 | `		rc = SXERR_EXISTS;` |
|       22 |  746 | `	}else{` |
|        - |  747 | `		/* Read the whole file contents */` |
|    11797 |  748 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|    11797 |  749 | `		if( rc != SXRET_OK ){` |
|        - |  750 | `			/* php refuses a target that is not a REGULAR file -- a directory is` |
|        - |  751 | `			 * the one every script meets -- inside the open itself, so what a` |
|        - |  752 | `			 * script reads is the open's generic reason and not the read's` |
|        - |  753 | ``			 * errno. `include '/etc'` says "No such file or directory" there`` |
|        - |  754 | `			 * and "Is a directory" here. */` |
|      ! 0 |  755 | `			if( pzWhy ){` |
|      ! 0 |  756 | `				*pzWhy = PH7_VfsOpenStrerror(ENOENT);` |
|      ! 0 |  757 | `			}` |
|      ! 0 |  758 | `		}` |
|    11797 |  759 | `		if( rc == SXRET_OK ){` |
|        - |  760 | `			SyString sScript;` |
|        - |  761 | `			/* Compile and execute the script. A throw the included file raised — and the` |
|        - |  762 | `			 * INCLUDING statement's own try caught in place — comes back as PH7_EXCEPTION` |
|        - |  763 | `			 * and travels out to the include builtin's caller, which unwinds the rest of` |
|        - |  764 | `			 * the statement instead of resuming it. It is not an IO failure, so the` |
|        - |  765 | `			 * callers must tell the two apart before warning. */` |
|    11797 |  766 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|        - |  767 | `			/* php shows the construct itself as a trace frame; the path it names is` |
|        - |  768 | `			 * the RESOLVED one, which is what aFiles was just given. */` |
|    17693 |  769 | `			PH7_VmIncFramePush(pVm,ph7_function_name(pCtx),` |
|    11792 |  770 | `				(SyString *)SySetPeek(&pVm->aFiles));` |
|    11792 |  771 | `			if( pCtx->pFunc && !pCtx->pFunc->bConstruct && pVm->pNativeCall` |
|       33 |  772 | `			 && pVm->pNativeCall->pName == &pCtx->pFunc->sName ){` |
|        - |  773 | `				/* A builtin loading the unit itself (spl_autoload()) is a call, and` |
|        - |  774 | `				 * its running record is the frame php shows -- not an include. */` |
|       30 |  775 | `				((VmIncFrame *)SySetPeek(&pVm->aIncFrame))->pNat = pVm->pNativeCall;` |
|       14 |  776 | `			}` |
|    11797 |  777 | `			rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|    11797 |  778 | `			PH7_VmIncFramePop(pVm);` |
|    11797 |  779 | `			if( rc != PH7_EXCEPTION ){` |
|    11751 |  780 | `				rc = SXRET_OK;` |
|     5873 |  781 | `			}` |
|     5896 |  782 | `		}` |
|        - |  783 | `	}` |
|        - |  784 | `	/* Pop from the set of included file */` |
|    11835 |  785 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  786 | `	/* Close the handle */` |
|    11835 |  787 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  788 | `	/* Release the working buffer */` |
|    11835 |  789 | `	SyBlobRelease(&sContents);` |
|        - |  790 | `#else` |
|        - |  791 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  792 | `	SXUNUSED(pPath);` |
|        - |  793 | `	SXUNUSED(IncludeOnce);` |
|        - |  794 | `	rc = SXERR_IO;` |
|        - |  795 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    11835 |  796 | `	return rc;` |
|     5939 |  797 | `}` |
|        - |  798 | `/*` |
|        - |  799 | ` * php keeps include_path in ONE place -- the INI table -- and get_include_path(),` |
|        - |  800 | ` * ini_get('include_path'), ini_get_all() and the resolver all read that one string.` |
|        - |  801 | ` * PHL had TWO: pVm->aPaths, which is what the include walk actually uses and which` |
|        - |  802 | `` * only set_include_path() ever wrote, and the `include_path` INI slot, which is what`` |
|        - |  803 | ` * ini_get() answers and which only ini_set()/-d ever wrote. Neither told the other,` |
|        - |  804 | `` * so `ini_set('include_path', $dir)` -- the ordinary way a bootstrap file points the`` |
|        - |  805 | `` * engine at a library -- moved NOTHING, and `set_include_path($dir)` left ini_get()`` |
|        - |  806 | ` * naming the value that was no longer in force. The set below is the single store;` |
|        - |  807 | ` * the INI slot is a view of it (vm_builtin_ini.c).` |
|        - |  808 | ` */` |
|      170 |  809 | `PH7_PRIVATE int PH7_VmIncludePathSep(void)` |
|        5 |  810 | `{` |
|        - |  811 | `#ifdef __WINNT__` |
|        5 |  812 | `	return ';';` |
|        - |  813 | `#else` |
|        - |  814 | `	/* Assume UNIX path separator */` |
|      170 |  815 | `	return ':';` |
|        - |  816 | `#endif` |
|        5 |  817 | `}` |
|        - |  818 | `/*` |
|        - |  819 | ` * Split an include_path STRING into its segments and make them the VM's set.` |
|        - |  820 | ` *` |
|        - |  821 | ` * Segments are kept VERBATIM. php hands back the string it was given, so a` |
|        - |  822 | ` * trailing slash, a leading space and an EMPTY segment all survive the round trip` |
|        - |  823 | ` * -- and the walk means something for each of them: php builds "<segment>/<file>"` |
|        - |  824 | ` * from every entry, which is how an empty segment comes to try "/file".` |
|        - |  825 | ` */` |
|       34 |  826 | `PH7_PRIVATE void PH7_VmSetIncludePath(ph7_vm *pVm,const char *zPath,sxu32 nByte)` |
|        3 |  827 | `{` |
|        - |  828 | `	const char *z,*zEnd,*zDup;` |
|       37 |  829 | `	int dir_sep = PH7_VmIncludePathSep();` |
|       37 |  830 | `	SySetReset(&pVm->aPaths);` |
|       37 |  831 | `	if( nByte < 1 ){` |
|      ! 0 |  832 | `		return;` |
|        - |  833 | `	}` |
|        - |  834 | `	/* ONE VM-lifetime copy backs every segment (the SyString entries alias it),` |
|        - |  835 | `	 * where the old splitter duped each segment separately on every call. */` |
|       37 |  836 | `	zDup = (const char *)SyMemBackendDup(&pVm->sAllocator,zPath,nByte);` |
|       37 |  837 | `	if( zDup == 0 ){` |
|      ! 0 |  838 | `		return;` |
|        - |  839 | `	}` |
|       37 |  840 | `	z = zDup;` |
|       37 |  841 | `	zEnd = &zDup[nByte];` |
|       25 |  842 | `	for(;;){` |
|       45 |  843 | `		const char *zStart = z;` |
|        - |  844 | `		SyString sPath;` |
|      576 |  845 | `		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }` |
|       45 |  846 | `		SyStringInitFromBuf(&sPath,zStart,(sxu32)(z-zStart));` |
|       45 |  847 | `		SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|       45 |  848 | `		if( z >= zEnd ){` |
|       37 |  849 | `			break;` |
|        - |  850 | `		}` |
|       10 |  851 | `		z++; /* skip the separator */` |
|        2 |  852 | `	}` |
|       20 |  853 | `}` |
|        - |  854 | `/*` |
|        - |  855 | ` * php's LAST RESORT for a relative name the include_path did not answer: the` |
|        - |  856 | ` * directory of the file that is EXECUTING, not the process's cwd. It is why` |
|        - |  857 | `` * `include 'helper.php'` next to the script keeps working when the script was`` |
|        - |  858 | ` * started from somewhere else, and why a library's own relative includes` |
|        - |  859 | ` * resolve at all. PHL had nothing of the kind, so` |
|        - |  860 | ` *` |
|        - |  861 | ` *   cd / && phl /srv/app/main.php   with   include 'lib.php'   next to main.php` |
|        - |  862 | ` *` |
|        - |  863 | ` * failed on this engine and ran on php.` |
|        - |  864 | ` *` |
|        - |  865 | ` * WHICH file is executing is the whole question, and the include-nesting stack` |
|        - |  866 | ` * is the wrong answer to it. php reads the running op array's own filename` |
|        - |  867 | `` * (`zend_get_executed_filename_ex()`), which is the file the include statement`` |
|        - |  868 | `` * is WRITTEN in -- so a method that says `require 'A.php'` looks beside the file`` |
|        - |  869 | ` * that declared the method. The include stack only agrees with that while a` |
|        - |  870 | ` * unit's top-level code is running: it is popped as soon as an include returns,` |
|        - |  871 | ` * so by the time the library's own function is CALLED the stack's top is the` |
|        - |  872 | ` * entry script, and the fallback searched the caller's directory instead of the` |
|        - |  873 | ` * library's. That is exactly the shape a composer package has -- one bootstrap` |
|        - |  874 | ` * file, functions that pull siblings in by bare name -- and it is why a fixture` |
|        - |  875 | ` * requiring its own sibling failed here. PH7_VmExecutingUnitFile is the engine's` |
|        - |  876 | ` * answer to php's question, and every diagnostic's location already uses it.` |
|        - |  877 | ` *` |
|        - |  878 | `` * Answers 0 when that name has no directory part at all (`-r`'s "Command line`` |
|        - |  879 | ` * code"). php's own arithmetic degenerates to the bare, cwd-relative name` |
|        - |  880 | ` * there, which the default include_path's "." entry already tries.` |
|        - |  881 | ` */` |
|       64 |  882 | `PH7_PRIVATE int PH7_VmExecutingDir(ph7_vm *pVm,SyString *pOut)` |
|        4 |  883 | `{` |
|       68 |  884 | `	SyString *pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|        - |  885 | `	sxu32 n;` |
|       68 |  886 | `	if( pFile == 0 \|\| pFile->nByte < 2 ){` |
|      ! 0 |  887 | `		return 0;` |
|        - |  888 | `	}` |
|       68 |  889 | `	n = pFile->nByte;` |
|     1412 |  890 | `	while( n > 0 ){` |
|     1412 |  891 | `		int c = pFile->zString[n-1];` |
|     1408 |  892 | `		if( c == '/'` |
|        - |  893 | `#ifdef __WINNT__` |
|        4 |  894 | `		 \|\| c == '\\'` |
|        - |  895 | `#endif` |
|        - |  896 | `		){` |
|       68 |  897 | `			break;` |
|        - |  898 | `		}` |
|     1348 |  899 | `		n--;` |
|        4 |  900 | `	}` |
|       68 |  901 | `	if( n < 2 ){` |
|        - |  902 | `		/* No separator, or one sitting at index 0 -- php skips the fallback for` |
|        - |  903 | `		 * a root-level script exactly the same way. */` |
|      ! 0 |  904 | `		return 0;` |
|        - |  905 | `	}` |
|       68 |  906 | `	SyStringInitFromBuf(pOut,pFile->zString,n-1); /* without the separator */` |
|       68 |  907 | `	return 1;` |
|       36 |  908 | `}` |
|        - |  909 | `/*` |
|        - |  910 | ` * Join the set back into php's one string. Splitting on the separator and` |
|        - |  911 | ` * joining with it round-trips exactly, which is what ini_get() promises.` |
|        - |  912 | ` */` |
|      136 |  913 | `PH7_PRIVATE void PH7_VmGetIncludePath(ph7_vm *pVm,SyBlob *pOut)` |
|        5 |  914 | `{` |
|      141 |  915 | `	SyString *aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|      141 |  916 | `	char cSep = (char)PH7_VmIncludePathSep();` |
|        - |  917 | `	sxu32 n;` |
|      295 |  918 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|      159 |  919 | `		if( n > 0 ){` |
|       20 |  920 | `			SyBlobAppend(pOut,(const void *)&cSep,sizeof(char));` |
|        9 |  921 | `		}` |
|      159 |  922 | `		SyBlobAppend(pOut,aEntry[n].zString,aEntry[n].nByte);` |
|       81 |  923 | `	}` |
|      141 |  924 | `}` |
|        - |  925 | `/*` |
|        - |  926 | ` * string get_include_path(void)` |
|        - |  927 | ` *  Gets the current include_path configuration option.` |
|        - |  928 | ` * Parameter` |
|        - |  929 | ` *  None` |
|        - |  930 | ` * Return` |
|        - |  931 | ` *  Included paths as a string` |
|        - |  932 | ` */` |
|       20 |  933 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  934 | `{` |
|        - |  935 | `	SyBlob sOut;` |
|       10 |  936 | `	SXUNUSED(nArg); /* cc warning */` |
|       10 |  937 | `	SXUNUSED(apArg);` |
|       23 |  938 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|       23 |  939 | `	PH7_VmGetIncludePath(pCtx->pVm,&sOut);` |
|        - |  940 | `	/* php's answer is always a STRING: an empty set is "", never NULL, which is` |
|        - |  941 | `	 * what this used to leave behind when nothing had ever been appended. */` |
|       23 |  942 | `	if( SyBlobLength(&sOut) < 1 ){` |
|      ! 0 |  943 | `		ph7_result_string(pCtx,"",0);` |
|      ! 0 |  944 | `	}else{` |
|       23 |  945 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|        - |  946 | `	}` |
|       23 |  947 | `	SyBlobRelease(&sOut);` |
|       23 |  948 | `	return PH7_OK;` |
|        3 |  949 | `}` |
|        - |  950 | `/*` |
|        - |  951 | ` * string\|false set_include_path(string $include_path)` |
|        - |  952 | ` *  Sets the include_path configuration option for the duration of the script,` |
|        - |  953 | ` *  returning the OLD value (php contract).` |
|        - |  954 | ` */` |
|       24 |  955 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  956 | `{` |
|       27 |  957 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  958 | `	const char *zNew;` |
|       27 |  959 | `	int nLen = 0;` |
|        - |  960 | `	SyBlob sOld;` |
|       27 |  961 | `	if( nArg < 1 ){` |
|      ! 0 |  962 | `		return PH7_OK;` |
|        - |  963 | `	}` |
|        - |  964 | `	/* The OLD value php answers is the effective one, read before the write. */` |
|       27 |  965 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|       27 |  966 | `	PH7_VmGetIncludePath(pVm,&sOld);` |
|       27 |  967 | `	zNew = ph7_value_to_string(apArg[0],&nLen);` |
|       27 |  968 | `	if( nLen < 1 ){` |
|        - |  969 | `		/* php registers include_path with OnUpdateStringUnempty, so the EMPTY` |
|        - |  970 | `		 * string is refused outright: the directive keeps the value it had and` |
|        - |  971 | `		 * the call answers FALSE. PHL used to accept it, wiping the set and` |
|        - |  972 | `		 * leaving get_include_path() with nothing to answer. */` |
|        3 |  973 | `		SyBlobRelease(&sOld);` |
|        3 |  974 | `		ph7_result_bool(pCtx,0);` |
|        3 |  975 | `		return PH7_OK;` |
|        - |  976 | `	}` |
|       25 |  977 | `	PH7_VmSetIncludePath(pVm,zNew,(sxu32)nLen);` |
|       25 |  978 | `	if( SyBlobLength(&sOld) < 1 ){` |
|      ! 0 |  979 | `		ph7_result_string(pCtx,"",0);` |
|      ! 0 |  980 | `	}else{` |
|       25 |  981 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|        - |  982 | `	}` |
|       25 |  983 | `	SyBlobRelease(&sOld);` |
|       25 |  984 | `	return PH7_OK;` |
|       15 |  985 | `}` |
|        - |  986 | `/*` |
|        - |  987 | ` * string get_get_included_files(void)` |
|        - |  988 | ` *  Gets the current include_path configuration option.` |
|        - |  989 | ` * Parameter` |
|        - |  990 | ` *  None` |
|        - |  991 | ` * Return` |
|        - |  992 | ` *  Included paths as a string` |
|        - |  993 | ` */` |
|        8 |  994 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  995 | `{` |
|        9 |  996 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  997 | `	ph7_value *pArray,*pWorker;` |
|        - |  998 | `	SyString *pEntry;` |
|        - |  999 | `	int c,d;` |
|        - | 1000 | `	/* Create an array and a working value */` |
|        9 | 1001 | `	pArray  = ph7_context_new_array(pCtx);` |
|        9 | 1002 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        9 | 1003 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 1004 | `		/* Out of memory,return null */` |
|      ! 0 | 1005 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1006 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 1007 | `		SXUNUSED(apArg);` |
|      ! 0 | 1008 | `		return PH7_OK;` |
|        - | 1009 | `	}` |
|        9 | 1010 | `	c = d = '/';` |
|        - | 1011 | `#ifdef __WINNT__` |
|        1 | 1012 | `	d = '\\';` |
|        - | 1013 | `#endif` |
|        - | 1014 | `	/* Iterate throw entries */` |
|        9 | 1015 | `	SySetResetCursor(pFiles);` |
|       25 | 1016 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 1017 | `		const char *zBase,*zEnd;` |
|        - | 1018 | `		int iLen;` |
|        - | 1019 | `		/* reset the string cursor */` |
|       17 | 1020 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 1021 | `		/* Extract base name */` |
|       17 | 1022 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 1023 | `		/* Ignore trailing '/' */` |
|       25 | 1024 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 1025 | `			zEnd--;` |
|      ! 0 | 1026 | `		}` |
|       17 | 1027 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|      445 | 1028 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|      421 | 1029 | `			zEnd--;` |
|        1 | 1030 | `		}` |
|       17 | 1031 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|       17 | 1032 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 1033 | `		/* Copy entry name */` |
|       17 | 1034 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 1035 | `		/* Perform the insertion */` |
|       17 | 1036 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 1037 | `	}` |
|        - | 1038 | `	/* All done,return the created array */` |
|        9 | 1039 | `	ph7_result_value(pCtx,pArray);` |
|        - | 1040 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 1041 | `	 * by the engine as soon we return from this foreign` |
|        - | 1042 | `	 * function.` |
|        - | 1043 | `	 */` |
|        9 | 1044 | `	return PH7_OK;` |
|        5 | 1045 | `}` |
|        - | 1046 | `/*` |
|        - | 1047 | ` * php raises TWO diagnostics for an include it could not open, and this engine` |
|        - | 1048 | ``  * raised one sentence of its own -- `include(): IO error while importing: 'x'` `` |
|        - | 1049 | ` * -- which names neither the reason the open failed nor the include_path that` |
|        - | 1050 | ` * was searched, and which no handler written against php can recognise:` |
|        - | 1051 | ` *` |
|        - | 1052 | ` *   Warning: include(x.php): Failed to open stream: No such file or directory` |
|        - | 1053 | ` *   Warning: include(): Failed opening 'x.php' for inclusion (include_path='.')` |
|        - | 1054 | ` *` |
|        - | 1055 | ` * The first is the stream layer's, and is the same sentence fopen() raises for` |
|        - | 1056 | ` * the same failure; the second is the language construct's, and is the only one` |
|        - | 1057 | ` * that says where it looked. Both name the path AS WRITTEN.` |
|        - | 1058 | ` *` |
|        - | 1059 | ` * require's second diagnostic is not a warning at all. php 8 THROWS an Error --` |
|        - | 1060 | `` * `Failed opening required 'x.php' (include_path='.')`, no function prefix, no`` |
|        - | 1061 | ` * "for inclusion" -- so a script may catch a missing dependency and carry on.` |
|        - | 1062 | ` * PHL reported it through the native fatal path, which ended the run and could` |
|        - | 1063 | ` * not be caught at all.` |
|        - | 1064 | ` */` |
|       34 | 1065 | `static sxi32 VmIncludeFailure(ph7_context *pCtx,SyString *pFile,const char *zWhy,int bRequire)` |
|        3 | 1066 | `{` |
|       37 | 1067 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1068 | `	SyString sPath;` |
|        - | 1069 | `	SyBlob sIncPath;` |
|       37 | 1070 | `	sxi32 rc = PH7_OK;` |
|        - | 1071 | `	/* The stream layer's sentence. Raised through the VM rather than the call` |
|        - | 1072 | ``	 * context, because the context's own prefix is `name(): ` and php's here`` |
|        - | 1073 | `	 * carries the path inside the parentheses. */` |
|       54 | 1074 | `	PH7_VmThrowWarningFmt(pVm,"%s(%z): Failed to open stream: %s",` |
|       17 | 1075 | `		ph7_function_name(pCtx),pFile,zWhy ? zWhy : "operation failed");` |
|       37 | 1076 | `	SyBlobInit(&sIncPath,&pVm->sAllocator);` |
|       37 | 1077 | `	PH7_VmGetIncludePath(pVm,&sIncPath);` |
|       37 | 1078 | `	SyStringInitFromBuf(&sPath,(const char *)SyBlobData(&sIncPath),SyBlobLength(&sIncPath));` |
|       37 | 1079 | `	if( bRequire ){` |
|       16 | 1080 | `		rc = PH7_VmThrowException(pCtx,"Error",` |
|        5 | 1081 | `			"Failed opening required '%z' (include_path='%z')",pFile,&sPath);` |
|        6 | 1082 | `	}else{` |
|       39 | 1083 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       12 | 1084 | `			"Failed opening '%z' for inclusion (include_path='%z')",pFile,&sPath);` |
|        - | 1085 | `	}` |
|       37 | 1086 | `	SyBlobRelease(&sIncPath);` |
|       37 | 1087 | `	return rc;` |
|        3 | 1088 | `}` |
|        - | 1089 | `/*` |
|        - | 1090 | ` * include:` |
|        - | 1091 | ` * According to the PHP reference manual.` |
|        - | 1092 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 1093 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 1094 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 1095 | ` *  include() will finally check in the calling script's own directory` |
|        - | 1096 | ` *  and the current working directory before failing. The include()` |
|        - | 1097 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 1098 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 1099 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 1100 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 1101 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 1102 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 1103 | ` *  directory to find the requested file.` |
|        - | 1104 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 1105 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 1106 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 1107 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 1108 | ` */` |
|    11572 | 1109 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1110 | `{` |
|        - | 1111 | `	SyString sFile;` |
|    11577 | 1112 | `	const char *zWhy = 0;` |
|        - | 1113 | `	sxi32 rc;` |
|    11577 | 1114 | `	if( nArg < 1 ){` |
|        - | 1115 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 1116 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1117 | `		return SXRET_OK;` |
|        - | 1118 | `	}` |
|        - | 1119 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - | 1120 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - | 1121 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - | 1122 | `	{` |
|    11577 | 1123 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|    11577 | 1124 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1125 | `			return rcSv;` |
|        - | 1126 | `		}` |
|        - | 1127 | `	}` |
|    11575 | 1128 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1129 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1130 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1131 | `		 * this used to answer NULL and carry on. */` |
|        2 | 1132 | `		return PH7_EXCEPTION;` |
|        - | 1133 | `	}` |
|        - | 1134 | `	/* Open,compile and execute the desired script */` |
|    11573 | 1135 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE,&zWhy);` |
|    11573 | 1136 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1137 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1138 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1139 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1140 | `		 * wins, so it is tested first. */` |
|       36 | 1141 | `		return rc;` |
|        - | 1142 | `	}` |
|    11539 | 1143 | `	if( rc != SXRET_OK ){` |
|       23 | 1144 | `		VmIncludeFailure(pCtx,&sFile,zWhy,FALSE);` |
|       23 | 1145 | `		ph7_result_bool(pCtx,0);` |
|       10 | 1146 | `	}` |
|    11539 | 1147 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1148 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 | 1149 | `		return PH7_ABORT;` |
|        - | 1150 | `	}` |
|    11535 | 1151 | `	return SXRET_OK;` |
|     5791 | 1152 | `}` |
|        - | 1153 | `/*` |
|        - | 1154 | ` * include_once:` |
|        - | 1155 | ` *  According to the PHP reference manual.` |
|        - | 1156 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 1157 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 1158 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 1159 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 1160 | ` *   just once.` |
|        - | 1161 | ` */` |
|       38 | 1162 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 1163 | `{` |
|        - | 1164 | `	SyString sFile;` |
|       42 | 1165 | `	const char *zWhy = 0;` |
|        - | 1166 | `	sxi32 rc;` |
|       42 | 1167 | `	if( nArg < 1 ){` |
|        - | 1168 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 1169 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1170 | `		return SXRET_OK;` |
|        - | 1171 | `	}` |
|        - | 1172 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - | 1173 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - | 1174 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - | 1175 | `	{` |
|       42 | 1176 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       42 | 1177 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1178 | `			return rcSv;` |
|        - | 1179 | `		}` |
|        - | 1180 | `	}` |
|       40 | 1181 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1182 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1183 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1184 | `		 * this used to answer NULL and carry on. */` |
|      ! 0 | 1185 | `		return PH7_EXCEPTION;` |
|        - | 1186 | `	}` |
|        - | 1187 | `	/* Open,compile and execute the desired script */` |
|       40 | 1188 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE,&zWhy);` |
|       40 | 1189 | `	if( rc == SXERR_EXISTS ){` |
|        - | 1190 | `		/* File already included,return TRUE */` |
|       25 | 1191 | `		ph7_result_bool(pCtx,1);` |
|       25 | 1192 | `		return SXRET_OK;` |
|        - | 1193 | `	}` |
|       18 | 1194 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1195 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1196 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1197 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1198 | `		 * wins, so it is tested first. */` |
|        3 | 1199 | `		return rc;` |
|        - | 1200 | `	}` |
|       16 | 1201 | `	if( rc != SXRET_OK ){` |
|        5 | 1202 | `		VmIncludeFailure(pCtx,&sFile,zWhy,FALSE);` |
|        5 | 1203 | `		ph7_result_bool(pCtx,0);` |
|        2 | 1204 | `	}` |
|       16 | 1205 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1206 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 1207 | `		return PH7_ABORT;` |
|        - | 1208 | `	}` |
|       16 | 1209 | `	return SXRET_OK;` |
|       23 | 1210 | `}` |
|        - | 1211 | `/*` |
|        - | 1212 | ` * require.` |
|        - | 1213 | ` *  According to the PHP reference manual.` |
|        - | 1214 | ` *   require() is identical to include() except upon failure it will` |
|        - | 1215 | ` *   also produce a fatal level error.` |
|        - | 1216 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 1217 | ` *   emits a warning  which allows the script to continue.` |
|        - | 1218 | ` */` |
|      208 | 1219 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1220 | `{` |
|        - | 1221 | `	SyString sFile;` |
|      213 | 1222 | `	const char *zWhy = 0;` |
|        - | 1223 | `	sxi32 rc;` |
|      213 | 1224 | `	if( nArg < 1 ){` |
|        - | 1225 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 1226 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1227 | `		return SXRET_OK;` |
|        - | 1228 | `	}` |
|        - | 1229 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - | 1230 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - | 1231 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - | 1232 | `	{` |
|      213 | 1233 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|      213 | 1234 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1235 | `			return rcSv;` |
|        - | 1236 | `		}` |
|        - | 1237 | `	}` |
|      211 | 1238 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1239 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1240 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1241 | `		 * this used to answer NULL and carry on. */` |
|        2 | 1242 | `		return PH7_EXCEPTION;` |
|        - | 1243 | `	}` |
|        - | 1244 | `	/* Open,compile and execute the desired script */` |
|      209 | 1245 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE,&zWhy);` |
|      209 | 1246 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1247 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1248 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1249 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1250 | `		 * wins, so it is tested first. */` |
|        8 | 1251 | `		return rc;` |
|        - | 1252 | `	}` |
|      203 | 1253 | `	if( rc != SXRET_OK ){` |
|        - | 1254 | `		/* php THROWS: the include is over, but the SCRIPT need not be. */` |
|        7 | 1255 | `		sxi32 rcThrow = VmIncludeFailure(pCtx,&sFile,zWhy,TRUE);` |
|        7 | 1256 | `		ph7_result_bool(pCtx,0);` |
|        7 | 1257 | `		return rcThrow == PH7_OK ? PH7_EXCEPTION : rcThrow;` |
|        - | 1258 | `	}` |
|      197 | 1259 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1260 | `		/* exit/die inside the included file: cascade the halt */` |
|       10 | 1261 | `		return PH7_ABORT;` |
|        - | 1262 | `	}` |
|      189 | 1263 | `	return SXRET_OK;` |
|      109 | 1264 | `}` |
|        - | 1265 | `/*` |
|        - | 1266 | ` * require_once:` |
|        - | 1267 | ` *  According to the PHP reference manual.` |
|        - | 1268 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 1269 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 1270 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 1271 | ` *   and how it differs from its non _once siblings.` |
|        - | 1272 | ` */` |
|       30 | 1273 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 1274 | `{` |
|        - | 1275 | `	SyString sFile;` |
|       34 | 1276 | `	const char *zWhy = 0;` |
|        - | 1277 | `	sxi32 rc;` |
|       34 | 1278 | `	if( nArg < 1 ){` |
|        - | 1279 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 1280 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1281 | `		return SXRET_OK;` |
|        - | 1282 | `	}` |
|        - | 1283 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - | 1284 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - | 1285 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - | 1286 | `	{` |
|       34 | 1287 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       34 | 1288 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1289 | `			return rcSv;` |
|        - | 1290 | `		}` |
|        - | 1291 | `	}` |
|       32 | 1292 | `	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){` |
|        - | 1293 | `		/* php's stream layer refuses an empty path wherever it is opened, and` |
|        - | 1294 | `		 * an include is an open: the ValueError is what a script catches, where` |
|        - | 1295 | `		 * this used to answer NULL and carry on. */` |
|      ! 0 | 1296 | `		return PH7_EXCEPTION;` |
|        - | 1297 | `	}` |
|        - | 1298 | `	/* Open,compile and execute the desired script */` |
|       32 | 1299 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE,&zWhy);` |
|       32 | 1300 | `	if( rc == SXERR_EXISTS ){` |
|        - | 1301 | `		/* File already included,return TRUE */` |
|       18 | 1302 | `		ph7_result_bool(pCtx,1);` |
|       18 | 1303 | `		return SXRET_OK;` |
|        - | 1304 | `	}` |
|       16 | 1305 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - | 1306 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - | 1307 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - | 1308 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - | 1309 | `		 * wins, so it is tested first. */` |
|        3 | 1310 | `		return rc;` |
|        - | 1311 | `	}` |
|       13 | 1312 | `	if( rc != SXRET_OK ){` |
|        - | 1313 | `		/* php THROWS: the include is over, but the SCRIPT need not be. */` |
|        5 | 1314 | `		sxi32 rcThrow = VmIncludeFailure(pCtx,&sFile,zWhy,TRUE);` |
|        5 | 1315 | `		ph7_result_bool(pCtx,0);` |
|        5 | 1316 | `		return rcThrow == PH7_OK ? PH7_EXCEPTION : rcThrow;` |
|        - | 1317 | `	}` |
|        8 | 1318 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - | 1319 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 | 1320 | `		return PH7_ABORT;` |
|        - | 1321 | `	}` |
|        8 | 1322 | `	return SXRET_OK;` |
|       19 | 1323 | `}` |
|        - | 1324 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 1325 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 1326 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 1327 | `/*` |
|        - | 1328 | ` * Section:` |
|        - | 1329 | ` *  SPL Autoloading functions.` |
|        - | 1330 | ` * Status:` |
|        - | 1331 | ` *  Stable.` |
|        - | 1332 | ` */` |
|        - | 1333 | `/*` |
|        - | 1334 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 1335 | ` *  Register given function as __autoload() implementation.` |
|        - | 1336 | ` * Parameters` |
|        - | 1337 | ` *  callback` |
|        - | 1338 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 1339 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 1340 | ` *  throw` |
|        - | 1341 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 1342 | ` *   exceptions on error. Ignored, as in php 8: an uncallable callback is` |
|        - | 1343 | ` *   always a TypeError, and false only raises a notice saying so.` |
|        - | 1344 | ` *  prepend` |
|        - | 1345 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 1346 | ` *   autoload stack instead of appending it.` |
|        - | 1347 | ` * Return` |
|        - | 1348 | ` *  TRUE on success, FALSE on failure.` |
|        - | 1349 | ` */` |
|      156 | 1350 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1351 | `{` |
|        - | 1352 | `	VmAutoloadCB sEntry;` |
|      161 | 1353 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1354 | `	ph7_value sDefault,*pCb;` |
|      161 | 1355 | `	int iPrepend = 0;` |
|        - | 1356 | `	sxu32 n;` |
|      161 | 1357 | `	PH7_MemObjInit(pVm,&sDefault);` |
|      161 | 1358 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|        - | 1359 | `		/* No callback (or null): the default spl_autoload() implementation */` |
|       19 | 1360 | `		PH7_MemObjStringAppend(&sDefault,"spl_autoload",sizeof("spl_autoload")-1);` |
|       19 | 1361 | `		pCb = &sDefault;` |
|       11 | 1362 | `	}else{` |
|        - | 1363 | `		/* php refuses an uncallable callback with a TypeError, whatever $throw says */` |
|      145 | 1364 | `		sxi32 rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);` |
|      145 | 1365 | `		if( rc != PH7_OK ){` |
|       22 | 1366 | `			return rc;` |
|        - | 1367 | `		}` |
|      125 | 1368 | `		pCb = apArg[0];` |
|        - | 1369 | `	}` |
|      141 | 1370 | `	if( nArg >= 2 && !ph7_value_to_bool(apArg[1]) ){` |
|        3 | 1371 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|        - | 1372 | `			"Argument #2 ($do_throw) has been ignored, spl_autoload_register() will always throw");` |
|        1 | 1373 | `	}` |
|        - | 1374 | `	/* Check for duplicates */` |
|      191 | 1375 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       61 | 1376 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       61 | 1377 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,pCb,TRUE,0) == 0 ){` |
|        - | 1378 | `			/* Already registered */` |
|        8 | 1379 | `			PH7_MemObjRelease(&sDefault);` |
|        8 | 1380 | `			ph7_result_bool(pCtx,1);` |
|        8 | 1381 | `			return SXRET_OK;` |
|        - | 1382 | `		}` |
|       30 | 1383 | `	}` |
|        - | 1384 | `	/* Check prepend flag */` |
|      135 | 1385 | `	if( nArg >= 3 ){` |
|       20 | 1386 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        8 | 1387 | `	}` |
|        - | 1388 | `	/* Store the callback */` |
|      135 | 1389 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|      135 | 1390 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|      135 | 1391 | `	PH7_MemObjInit(pVm,&sEntry.sInvoke);` |
|      135 | 1392 | `	PH7_MemObjStore(pCb,&sEntry.sCallback);` |
|      135 | 1393 | `	PH7_VmBindCallbackScope(pVm,pCb,&sEntry.sInvoke);` |
|      143 | 1394 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 1395 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 1396 | `		 * We do this by appending first, then rotating the array. */` |
|       20 | 1397 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 1398 | `		VmAutoloadCB *aBase;` |
|       20 | 1399 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 1400 | `		/* Rotate: move last entry to front */` |
|       20 | 1401 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|       20 | 1402 | `		if( aBase ){` |
|        - | 1403 | `			VmAutoloadCB sTemp;` |
|        - | 1404 | `			sxu32 i;` |
|       20 | 1405 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|       42 | 1406 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|       26 | 1407 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|       15 | 1408 | `			}` |
|       20 | 1409 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        8 | 1410 | `		}` |
|       12 | 1411 | `	}else{` |
|      119 | 1412 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 1413 | `	}` |
|      135 | 1414 | `	PH7_MemObjRelease(&sDefault);` |
|      135 | 1415 | `	ph7_result_bool(pCtx,1);` |
|      135 | 1416 | `	return SXRET_OK;` |
|       83 | 1417 | `}` |
|        - | 1418 | `/*` |
|        - | 1419 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 1420 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 1421 | ` * Parameters` |
|        - | 1422 | ` *  callback` |
|        - | 1423 | ` *   The autoload function being unregistered.` |
|        - | 1424 | ` * Return` |
|        - | 1425 | ` *  TRUE on success, FALSE on failure.` |
|        - | 1426 | ` */` |
|       78 | 1427 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1428 | `{` |
|       83 | 1429 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1430 | `	sxu32 n,nEntry;` |
|        - | 1431 | `	sxi32 rc;` |
|       83 | 1432 | `	if( nArg < 1 ){` |
|      ! 0 | 1433 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1434 | `		return SXRET_OK;` |
|        - | 1435 | `	}` |
|        - | 1436 | `	/* php refuses an uncallable argument before it looks the stack up */` |
|       83 | 1437 | `	rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|       83 | 1438 | `	if( rc != PH7_OK ){` |
|       19 | 1439 | `		return rc;` |
|        - | 1440 | `	}` |
|       65 | 1441 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       81 | 1442 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       75 | 1443 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       75 | 1444 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 1445 | `			/* Found — remove by shifting remaining entries down */` |
|       59 | 1446 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 1447 | `			sxu32 i;` |
|       59 | 1448 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       59 | 1449 | `			PH7_MemObjRelease(&pEntry->sInvoke);` |
|       87 | 1450 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       32 | 1451 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|       18 | 1452 | `			}` |
|        - | 1453 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       59 | 1454 | `			SySetPop(&pVm->aAutoload);` |
|       59 | 1455 | `			ph7_result_bool(pCtx,1);` |
|       59 | 1456 | `			return SXRET_OK;` |
|        - | 1457 | `		}` |
|       10 | 1458 | `	}` |
|        8 | 1459 | `	ph7_result_bool(pCtx,0);` |
|        8 | 1460 | `	return SXRET_OK;` |
|       44 | 1461 | `}` |
|        - | 1462 | `/*` |
|        - | 1463 | ` * array spl_autoload_functions(void)` |
|        - | 1464 | ` *  Return all registered __autoload() functions.` |
|        - | 1465 | ` * Return` |
|        - | 1466 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 1467 | ` *  an empty array is returned.` |
|        - | 1468 | ` */` |
|       38 | 1469 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 1470 | `{` |
|       42 | 1471 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1472 | `	ph7_value *pArray;` |
|        - | 1473 | `	sxu32 n,nEntry;` |
|       19 | 1474 | `	SXUNUSED(nArg);` |
|       19 | 1475 | `	SXUNUSED(apArg);` |
|       42 | 1476 | `	pArray = ph7_context_new_array(pCtx);` |
|       42 | 1477 | `	if( pArray == 0 ){` |
|      ! 0 | 1478 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1479 | `		return SXRET_OK;` |
|        - | 1480 | `	}` |
|       42 | 1481 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       90 | 1482 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       52 | 1483 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       52 | 1484 | `		if( pEntry ){` |
|       52 | 1485 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|       24 | 1486 | `		}` |
|       28 | 1487 | `	}` |
|       42 | 1488 | `	ph7_result_value(pCtx,pArray);` |
|       42 | 1489 | `	return SXRET_OK;` |
|       23 | 1490 | `}` |
|        - | 1491 | `/*` |
|        - | 1492 | ` * string spl_autoload_extensions([?string $file_extensions = null])` |
|        - | 1493 | ` *  Register and return default file extensions for spl_autoload().` |
|        - | 1494 | ` * Return` |
|        - | 1495 | ` *  The list in force AFTER the call, which is the new one when a string was` |
|        - | 1496 | ` *  handed over and the standing one otherwise.` |
|        - | 1497 | ` *` |
|        - | 1498 | ` * The list is per-VM state rather than an ini directive (php keeps it in SPL's` |
|        - | 1499 | `` * own globals), and `null` means READ: it is the one argument value that does`` |
|        - | 1500 | ``  * not write. Every other value writes what it stringifies to, so `false` `` |
|        - | 1501 | ` * empties the list -- and an empty list is a real setting, not a reset.` |
|        - | 1502 | ` */` |
|       18 | 1503 | `PH7_PRIVATE int vm_builtin_spl_autoload_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1504 | `{` |
|       19 | 1505 | `	ph7_vm *pVm = pCtx->pVm;` |
|       19 | 1506 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|        - | 1507 | `		const char *zExt;` |
|        - | 1508 | `		int nExt;` |
|        7 | 1509 | `		zExt = ph7_value_to_string(apArg[0],&nExt);` |
|        7 | 1510 | `		SyBlobReset(&pVm->sAutoloadExt);` |
|        7 | 1511 | `		if( nExt > 0 ){` |
|        5 | 1512 | `			SyBlobAppend(&pVm->sAutoloadExt,zExt,(sxu32)nExt);` |
|        2 | 1513 | `		}` |
|        3 | 1514 | `	}` |
|       28 | 1515 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pVm->sAutoloadExt),` |
|       18 | 1516 | `		(int)SyBlobLength(&pVm->sAutoloadExt));` |
|       19 | 1517 | `	return SXRET_OK;` |
|        1 | 1518 | `}` |
|        - | 1519 | `/*` |
|        - | 1520 | ` * void spl_autoload_call(string $class)` |
|        - | 1521 | ` *  Try all registered autoloaders to load the requested class.` |
|        - | 1522 | ` *` |
|        - | 1523 | ` * php runs the stack whether or not the class is already declared -- the` |
|        - | 1524 | ` * verb is "call the autoloaders", not "load if missing" -- and stops as soon` |
|        - | 1525 | ` * as one of them declares it. The answer is always NULL.` |
|        - | 1526 | ` */` |
|       10 | 1527 | `PH7_PRIVATE int vm_builtin_spl_autoload_call(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 1528 | `{` |
|        - | 1529 | `	const char *zClass;` |
|        - | 1530 | `	int nClass;` |
|       13 | 1531 | `	if( nArg < 1 ){` |
|      ! 0 | 1532 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1533 | `		return SXRET_OK;` |
|        - | 1534 | `	}` |
|       13 | 1535 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        - | 1536 | `	{` |
|        - | 1537 | `		/* php runs the stack for the EMPTY name too -- the autoloaders see a` |
|        - | 1538 | `		 * "" they have to answer for, rather than a call that never happened. */` |
|       13 | 1539 | `		sxu32 nByte = nClass > 0 ? (sxu32)nClass : 0;` |
|       13 | 1540 | `		if( zClass == 0 ){` |
|      ! 0 | 1541 | `			zClass = "";` |
|      ! 0 | 1542 | `		}` |
|       13 | 1543 | `		PH7_VmClassNameAnchor(&zClass,&nByte);` |
|       13 | 1544 | `		PH7_VmTriggerAutoload(pCtx->pVm,zClass,nByte,0);` |
|        - | 1545 | `	}` |
|       13 | 1546 | `	ph7_result_null(pCtx);` |
|       13 | 1547 | `	return SXRET_OK;` |
|        8 | 1548 | `}` |
|        - | 1549 | `/*` |
|        - | 1550 | ` * array spl_classes()` |
|        - | 1551 | ` *  Return an array with the name of every class and interface SPL declares.` |
|        - | 1552 | ` *` |
|        - | 1553 | ` * php answers a map whose key and value are both the name, in the order its` |
|        - | 1554 | ` * own list is written -- which is alphabetical. The set is SPL's own and not` |
|        - | 1555 | ` * "everything iterable": Traversable, Countable and the exception BASE are` |
|        - | 1556 | ` * php's Core, so none of the three is here, while the five SPL INTERFACES` |
|        - | 1557 | ` * (OuterIterator, RecursiveIterator, SeekableIterator, SplObserver,` |
|        - | 1558 | ` * SplSubject) are.` |
|        - | 1559 | ` */` |
|        2 | 1560 | `PH7_PRIVATE int vm_builtin_spl_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1561 | `{` |
|        - | 1562 | `	static const char * const azSpl[] = {` |
|        - | 1563 | `		"AppendIterator", "ArrayIterator", "ArrayObject", "BadFunctionCallException",` |
|        - | 1564 | `		"BadMethodCallException", "CachingIterator", "CallbackFilterIterator",` |
|        - | 1565 | `		"DirectoryIterator", "DomainException", "EmptyIterator", "FilesystemIterator",` |
|        - | 1566 | `		"FilterIterator", "GlobIterator", "InfiniteIterator", "InvalidArgumentException",` |
|        - | 1567 | `		"IteratorIterator", "LengthException", "LimitIterator", "LogicException",` |
|        - | 1568 | `		"MultipleIterator", "NoRewindIterator", "OuterIterator", "OutOfBoundsException",` |
|        - | 1569 | `		"OutOfRangeException", "OverflowException", "ParentIterator", "RangeException",` |
|        - | 1570 | `		"RecursiveArrayIterator", "RecursiveCachingIterator",` |
|        - | 1571 | `		"RecursiveCallbackFilterIterator", "RecursiveDirectoryIterator",` |
|        - | 1572 | `		"RecursiveFilterIterator", "RecursiveIterator", "RecursiveIteratorIterator",` |
|        - | 1573 | `		"RecursiveRegexIterator", "RecursiveTreeIterator", "RegexIterator",` |
|        - | 1574 | `		"RuntimeException", "SeekableIterator", "SplDoublyLinkedList", "SplFileInfo",` |
|        - | 1575 | `		"SplFileObject", "SplFixedArray", "SplHeap", "SplMinHeap", "SplMaxHeap",` |
|        - | 1576 | `		"SplObjectStorage", "SplObserver", "SplPriorityQueue", "SplQueue", "SplStack",` |
|        - | 1577 | `		"SplSubject", "SplTempFileObject", "UnderflowException", "UnexpectedValueException"` |
|        - | 1578 | `	};` |
|        - | 1579 | `	ph7_value *pArray,*pVal;` |
|        - | 1580 | `	sxu32 n;` |
|        1 | 1581 | `	SXUNUSED(nArg);` |
|        1 | 1582 | `	SXUNUSED(apArg);` |
|        3 | 1583 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 1584 | `	pVal = ph7_context_new_scalar(pCtx);` |
|        3 | 1585 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|      ! 0 | 1586 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1587 | `		return SXRET_OK;` |
|        - | 1588 | `	}` |
|      113 | 1589 | `	for( n = 0 ; n < SX_ARRAYSIZE(azSpl) ; ++n ){` |
|      111 | 1590 | `		ph7_value_string(pVal,azSpl[n],-1);` |
|      111 | 1591 | `		ph7_array_add_strkey_elem(pArray,azSpl[n],pVal);` |
|      111 | 1592 | `		ph7_value_reset_string_cursor(pVal);` |
|       56 | 1593 | `	}` |
|        3 | 1594 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 1595 | `	return SXRET_OK;` |
|        2 | 1596 | `}` |
|        - | 1597 | `/*` |
|        - | 1598 | ` * void spl_autoload(string $class [, ?string $file_extensions = null ])` |
|        - | 1599 | ` *  Default implementation of __autoload().` |
|        - | 1600 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 1601 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 1602 | ` * Parameters` |
|        - | 1603 | ` *  class` |
|        - | 1604 | ` *   The class name being searched.` |
|        - | 1605 | ` *  file_extensions` |
|        - | 1606 | ` *   Comma-separated list of file extensions to try. When none is handed over,` |
|        - | 1607 | ` *   the list is whatever spl_autoload_extensions() holds -- which is where the` |
|        - | 1608 | ` *   ORDER comes from, and the order decides the answer: php's default tries` |
|        - | 1609 | `` *   `.inc` BEFORE `.php`, and this engine had the pair hardcoded the other way`` |
|        - | 1610 | `` *   round, so a directory carrying both `foo.inc` and `foo.php` loaded the one`` |
|        - | 1611 | ` *   php does not.` |
|        - | 1612 | ` */` |
|       30 | 1613 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 1614 | `{` |
|        - | 1615 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 1616 | `	SyBlob sPath;` |
|        - | 1617 | `	int nClass,nExt,iByte;` |
|        - | 1618 | `	sxi32 rc;` |
|       33 | 1619 | `	if( nArg < 1 ){` |
|      ! 0 | 1620 | `		return SXRET_OK;` |
|        - | 1621 | `	}` |
|       33 | 1622 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|       33 | 1623 | `	if( nClass < 1 ){` |
|      ! 0 | 1624 | `		return SXRET_OK;` |
|        - | 1625 | `	}` |
|        - | 1626 | `	/* The list to try: the argument when there is one, and the standing` |
|        - | 1627 | `	 * spl_autoload_extensions() setting otherwise. */` |
|       33 | 1628 | `	if( nArg >= 2 && !ph7_value_is_null(apArg[1]) ){` |
|        - | 1629 | `		/* An argument that is EMPTY is an empty list and not a request for the` |
|        - | 1630 | ``		 * default one, so `spl_autoload($c,"")` searches nothing at all. */`` |
|      ! 0 | 1631 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 1632 | `	}else{` |
|       33 | 1633 | `		zExt = (const char *)SyBlobData(&pCtx->pVm->sAutoloadExt);` |
|       33 | 1634 | `		nExt = (int)SyBlobLength(&pCtx->pVm->sAutoloadExt);` |
|        - | 1635 | `	}` |
|        - | 1636 | `	/* php walks the list as a C string, so a NUL byte in it ends the search. */` |
|      303 | 1637 | `	for( iByte = 0 ; iByte < nExt ; ++iByte ){` |
|      273 | 1638 | `		if( zExt[iByte] == 0 ){` |
|      ! 0 | 1639 | `			nExt = iByte;` |
|      ! 0 | 1640 | `			break;` |
|        - | 1641 | `		}` |
|      138 | 1642 | `	}` |
|       33 | 1643 | `	if( nExt < 1 ){` |
|      ! 0 | 1644 | `		return SXRET_OK;` |
|        - | 1645 | `	}` |
|       33 | 1646 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 1647 | `	/* Iterate over comma-separated extensions */` |
|       33 | 1648 | `	zEnd = zExt + nExt;` |
|       33 | 1649 | `	zCur = zExt;` |
|       37 | 1650 | `	while( zCur < zEnd ){` |
|        - | 1651 | `		const char *zComma;` |
|        - | 1652 | `		SyString sFile;` |
|        - | 1653 | `		int i;` |
|        - | 1654 | `		/* Find next comma or end */` |
|       35 | 1655 | `		zComma = zCur;` |
|      163 | 1656 | `		while( zComma < zEnd && *zComma != ',' ){` |
|      131 | 1657 | `			zComma++;` |
|        3 | 1658 | `		}` |
|        - | 1659 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|       35 | 1660 | `		SyBlobReset(&sPath);` |
|      155 | 1661 | `		for( i = 0 ; i < nClass ; i++ ){` |
|      123 | 1662 | `			char c = zClass[i];` |
|      123 | 1663 | `			if( c == '\\' ){` |
|      ! 0 | 1664 | `				c = '/';` |
|      123 | 1665 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       43 | 1666 | `				c = c + ('a' - 'A');` |
|       20 | 1667 | `			}` |
|      123 | 1668 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       63 | 1669 | `		}` |
|        - | 1670 | `		/* Append extension */` |
|       35 | 1671 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 1672 | `		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,` |
|        - | 1673 | `		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a` |
|        - | 1674 | `		 * terminator, so without this the strlen reads past the buffer (a` |
|        - | 1675 | `		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL` |
|        - | 1676 | `		 * is not counted in SyBlobLength(), so the SyString length stays correct. */` |
|       35 | 1677 | `		SyBlobNullAppend(&sPath);` |
|        - | 1678 | `		/* Try to include the file */` |
|       35 | 1679 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       35 | 1680 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE,0);` |
|       35 | 1681 | `		if( rc == SXRET_OK \|\| rc == PH7_EXCEPTION ){` |
|        - | 1682 | `			/* Included — or it threw, which ends the search too: the remaining` |
|        - | 1683 | `			 * extensions are not tried after a file has already run. */` |
|       30 | 1684 | `			SyBlobRelease(&sPath);` |
|       30 | 1685 | `			return rc == PH7_EXCEPTION ? rc : SXRET_OK;` |
|        - | 1686 | `		}` |
|        - | 1687 | `		/* Move past the comma */` |
|        5 | 1688 | `		zCur = zComma;` |
|        5 | 1689 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 1690 | `			zCur++;` |
|        1 | 1691 | `		}` |
|        1 | 1692 | `	}` |
|        3 | 1693 | `	SyBlobRelease(&sPath);` |
|        3 | 1694 | `	return SXRET_OK;` |
|       18 | 1695 | `}` |
|        - | 1696 | `/* Table of built-in VM functions. */` |
|        - | 1697 | `/*` |
|        - | 1698 | ` * Packing body for __call / __callStatic (band A #3b).` |
|        - | 1699 | ` * OP_MEMBER, on a missing or inaccessible method whose class declares the magic` |
|        - | 1700 | ` * handler, stashes {receiver, class, original name} on the VM and marks the callee` |
|        - | 1701 | ` * slot MEMOBJ_AUX_MAGICCALL; the normal OP_CALL machinery then collects the` |
|        - | 1702 | ` * ORIGINAL argument list (incl. spreads) and hands it here, which packs it into a` |
|        - | 1703 | ` * php array and invokes` |
|        - | 1704 | ` *   $recv->__call($name, $args)   /   Class::__callStatic($name, $args)` |
|        - | 1705 | ` * returning the handler's value as the call's result. A throw propagates via the` |
|        - | 1706 | ` * returned status (and the boundary rail).` |
|        - | 1707 | ` *` |
|        - | 1708 | `` * This used to be a REGISTERED host function named `__phl_magic_call` whose name`` |
|        - | 1709 | ` * the four OP_MEMBER sites wrote into the callee slot — so the engine's own` |
|        - | 1710 | ` * dispatch was spelled as a global PHP function that function_exists() and` |
|        - | 1711 | ` * get_defined_functions() both reported, and that any script could call. It is` |
|        - | 1712 | ` * reached through the VM's own function record now (PH7_VmMagicCallFunc); the` |
|        - | 1713 | ` * mark on the slot is the only thing that selects it, and there is no name.` |
|        - | 1714 | ` */` |
|      162 | 1715 | `static int VmMagicCallDispatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 1716 | `{` |
|      166 | 1717 | `	ph7_vm *pVm = pCtx->pVm;` |
|      166 | 1718 | `	ph7_class_instance *pRecv = pVm->pMagicCallThis;` |
|      166 | 1719 | `	ph7_class *pClass = pVm->pMagicCallClass;` |
|      166 | 1720 | `	ph7_class *pLsb = pVm->pMagicCallLsb;` |
|        - | 1721 | `	ph7_value sResult;` |
|        - | 1722 | `	SyString sMethName;` |
|        - | 1723 | `	sxi32 rc;` |
|        - | 1724 | `	/* Consume the pending dispatch (one-shot) */` |
|      166 | 1725 | `	pVm->pMagicCallThis = 0;` |
|      166 | 1726 | `	pVm->pMagicCallClass = 0;` |
|      166 | 1727 | `	pVm->pMagicCallLsb = 0;` |
|      166 | 1728 | `	if( pClass == 0 ){` |
|        - | 1729 | `		/* Defensive: the mark and the latch are set together by OP_MEMBER, so a` |
|        - | 1730 | `		 * classless arrival is unreachable. It was reachable while this body wore a` |
|        - | 1731 | ``		 * function NAME — anyone could call `__phl_magic_call()` — which is exactly`` |
|        - | 1732 | `		 * what the mark retired. */` |
|      ! 0 | 1733 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1734 | `		return PH7_OK;` |
|        - | 1735 | `	}` |
|      166 | 1736 | `	SyStringInitFromBuf(&sMethName,SyBlobData(&pVm->sMagicCallName),SyBlobLength(&pVm->sMagicCallName));` |
|      166 | 1737 | `	PH7_MemObjInit(pVm,&sResult);` |
|        - | 1738 | `	/* The one packing site, shared with every CALLABLE spelling of the same call` |
|        - | 1739 | `	 * (PH7_VmDispatchMagicCall, vm_builtin_call.c): the handler lookup, the $args array` |
|        - | 1740 | `	 * — named arguments keyed by name, as php keys them — and the engine-dispatch` |
|        - | 1741 | `	 * visibility rule all live there. Two copies of this is how the syntax path and the` |
|        - | 1742 | `	 * callable path came to disagree about the argument names in the first place.` |
|        - | 1743 | `	 * SXERR_NOTFOUND is unreachable: OP_MEMBER verified the handler exists. */` |
|      247 | 1744 | `	rc = PH7_VmDispatchMagicCall(pVm,pClass,pLsb,pRecv,SyStringData(&sMethName),` |
|       81 | 1745 | `		SyStringLength(&sMethName),&sResult,nArg,apArg,pCtx->pArgMap);` |
|      166 | 1746 | `	if( rc == SXRET_OK ){` |
|      160 | 1747 | `		ph7_result_value(pCtx,&sResult);` |
|       78 | 1748 | `	}` |
|      166 | 1749 | `	PH7_MemObjRelease(&sResult);` |
|      166 | 1750 | `	if( pRecv ){` |
|       90 | 1751 | `		PH7_ClassInstanceUnref(pRecv);` |
|       43 | 1752 | `	}` |
|      166 | 1753 | `	SyBlobReset(&pVm->sMagicCallName);` |
|      166 | 1754 | `	return (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) ? rc : PH7_OK;` |
|       85 | 1755 | `}` |
|        - | 1756 | `/*` |
|        - | 1757 | ` * The function record OP_CALL dispatches a MEMOBJ_AUX_MAGICCALL callee slot through,` |
|        - | 1758 | ` * built on first use and owned by the VM (the allocator frees it with everything else).` |
|        - | 1759 | ` *` |
|        - | 1760 | ` * It is deliberately NOT installed in pVm->hHostFunction: the same property that makes` |
|        - | 1761 | ` * a native class method unreachable except by dispatching the method (see` |
|        - | 1762 | ` * PH7_NativeClassInstallMethod) makes this body unreachable except by the engine's own` |
|        - | 1763 | ` * __call routing. It carries no signature and no arity bounds, so the OP_CALL choke` |
|        - | 1764 | ` * point's ZPP and ArgumentCountError screens are inert for it — the handler's OWN` |
|        - | 1765 | ` * declared parameters are what php enforces, and it is a PHP method with a frame of its` |
|        - | 1766 | ` * own. sName is a diagnostic label only; nothing that reads it can be reached from here.` |
|        - | 1767 | ` */` |
|      162 | 1768 | `PH7_PRIVATE ph7_user_func * PH7_VmMagicCallFunc(ph7_vm *pVm)` |
|        4 | 1769 | `{` |
|        - | 1770 | `	SyString sName;` |
|      166 | 1771 | `	if( pVm->pMagicCallFunc == 0 ){` |
|       16 | 1772 | `		SyStringInitFromBuf(&sName,"__call",sizeof("__call")-1);` |
|       18 | 1773 | `		if( PH7_NewForeignFunction(&(*pVm),&sName,VmMagicCallDispatch,0,` |
|       16 | 1774 | `			&pVm->pMagicCallFunc) != SXRET_OK ){` |
|      ! 0 | 1775 | `			pVm->pMagicCallFunc = 0;` |
|      ! 0 | 1776 | `		}` |
|        6 | 1777 | `	}` |
|      166 | 1778 | `	return pVm->pMagicCallFunc;` |
|        4 | 1779 | `}` |
|        - | 1780 |  |
