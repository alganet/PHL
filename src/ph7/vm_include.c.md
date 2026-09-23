# src/ph7/vm_include.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 470/549 lines (85.61%)

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
|        - |   18 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |   19 | ` * Refer to the eval() language construct implementation for more` |
|        - |   20 | ` * information.` |
|        - |   21 | ` */` |
|    14898 |   22 | `PH7_PRIVATE sxi32 VmEvalChunk(` |
|        - |   23 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |   24 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |   25 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |   26 | `	int iFlags,         /* Compile flag */` |
|        - |   27 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |   28 | `	)` |
|        5 |   29 | `{` |
|        - |   30 | `	SySet *pByteCode,aByteCode;` |
|    14903 |   31 | `	ProcConsumer xErr = 0;` |
|    14903 |   32 | `	void *pErrData = 0;` |
|        - |   33 | `	ph7_gen_state sSavedGen;` |
|        - |   34 | `	int bNested;` |
|    14903 |   35 | `	sxi32 rcThrow = SXRET_OK; /* a ParseError raised for a failed compile, or a throw the` |
|        - |   36 | `	                           * evaluated chunk raised — either way the caller must unwind */` |
|        - |   37 | `	/* Initialize bytecode container */` |
|    14903 |   38 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    14903 |   39 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |   40 | `	/* Reset the code generator */` |
|    14903 |   41 | `	if( bTrueReturn ){` |
|        - |   42 | `		/* Included file,log compile-time errors */` |
|     9975 |   43 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9975 |   44 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4985 |   45 | `	}` |
|        - |   46 | `	/* A non-zero cursor means an OUTER compile is in flight — this eval/include` |
|        - |   47 | `	 * was reached from inside it (an autoload fired while resolving a base class).` |
|        - |   48 | `	 * Wiping the generator would corrupt that outer parse, so snapshot it and hand` |
|        - |   49 | `	 * the nested unit a fresh one; a plain runtime eval/include just resets. */` |
|    14903 |   50 | `	bNested = (pVm->sCodeGen.pIn != 0);` |
|    14903 |   51 | `	if( bNested ){` |
|        5 |   52 | `		PH7_CompilerSaveState(pVm,&sSavedGen,xErr,pErrData);` |
|        3 |   53 | `	}else{` |
|    14899 |   54 | `		PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |   55 | `	}` |
|        - |   56 | `	/* Swap bytecode container */` |
|    14903 |   57 | `	pByteCode = pVm->pByteContainer;` |
|    14903 |   58 | `	pVm->pByteContainer = &aByteCode;` |
|        - |   59 | `	/* Compile the chunk */` |
|    14903 |   60 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|        - |   61 | `	/* Record THIS unit's error count where the nested state restore below cannot` |
|        - |   62 | `	 * wipe it — VmExecDeferredClass reads it to detect a failed re-compile. */` |
|    14903 |   63 | `	pVm->nLastEvalErr = pVm->sCodeGen.nErr;` |
|    22351 |   64 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |   65 | `		/* Compilation error. php makes this a CATCHABLE ParseError for eval()` |
|        - |   66 | `		 * ("syntax error, unexpected ..."), where PHL merely returned false —` |
|        - |   67 | ``		 * so `eval('bad syntax')` silently produced a value instead of throwing.`` |
|        - |   68 | `		 * include/require keep the false return: their parse error is a printed` |
|        - |   69 | `		 * fatal, not an exception. */` |
|       61 |   70 | `		if( pCtx && !bTrueReturn ){` |
|       41 |   71 | `			SyBlob *pErr = &pVm->sCodeGen.sErrBuf;` |
|       41 |   72 | `			ph7_result_bool(pCtx,0);` |
|       41 |   73 | `			if( SyBlobLength(pErr) > 0 ){` |
|       61 |   74 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","%.*s",` |
|       40 |   75 | `					(int)SyBlobLength(pErr),(const char *)SyBlobData(pErr));` |
|       21 |   76 | `			}else{` |
|      ! 0 |   77 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","syntax error");` |
|        1 |   78 | `			}` |
|       20 |   79 | `		}else if( pCtx ){` |
|      ! 0 |   80 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |   81 | `		}` |
|       21 |   82 | `	}else{` |
|        - |   83 | `		/* Mount any newly defined classes. Skipped while the VM is still` |
|        - |   84 | `		 * initializing (builtin chunks): mounting evaluates class-constant/` |
|        - |   85 | `		 * static initializers and installs reference-table entries, and the` |
|        - |   86 | `		 * runtime structures those need (apRefObj, the per-exec object pool` |
|        - |   87 | `		 * baseline) do not exist before PH7_VmMakeReady — which mounts every` |
|        - |   88 | `		 * class unconditionally anyway. */` |
|        - |   89 | `		SyHashEntry *pEntry;` |
|        - |   90 | `		ph7_class *pClass;` |
|        - |   91 | `		ph7_value sResult; /* Return value */` |
|        - |   92 | `		sxi32 rc;` |
|    14863 |   93 | `		if( pVm->nMagic != PH7_VM_INIT ){` |
|    10193 |   94 | `			SyHashResetLoopCursor(&pVm->hClass);` |
|  3433223 |   95 | `			while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|  3417943 |   96 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|        - |   97 | `				/* Only mount classes that haven't been mounted yet */` |
|  3417943 |   98 | `				if( !pClass->bMounted ){` |
|   301209 |   99 | `					rc = VmMountUserClass(pVm,pClass);` |
|   301209 |  100 | `					if( rc != SXRET_OK ){` |
|        - |  101 | `						/* Mount failure (likely memory error) */` |
|        3 |  102 | `						if( pCtx ){` |
|        3 |  103 | `							ph7_result_bool(pCtx,0);` |
|        1 |  104 | `						}` |
|        3 |  105 | `						goto Cleanup;` |
|        - |  106 | `					}` |
|   150601 |  107 | `				}` |
|        5 |  108 | `			}` |
|     5093 |  109 | `		}` |
|    14861 |  110 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  111 | `			/* Out of memory */` |
|      ! 0 |  112 | `			if( pCtx ){` |
|      ! 0 |  113 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  114 | `			}` |
|      ! 0 |  115 | `			goto Cleanup;` |
|        - |  116 | `		}` |
|    14861 |  117 | `		if( bTrueReturn ){` |
|        - |  118 | `			/* Assume a boolean true return value */` |
|     9975 |  119 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4990 |  120 | `		}else{` |
|        - |  121 | `			/* Assume a null return value */` |
|     4891 |  122 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  123 | `		}` |
|        - |  124 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here` |
|        - |  125 | `		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by` |
|        - |  126 | `		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the` |
|        - |  127 | `		 * native-nesting fatal instead of overflowing the C stack. The PHP` |
|        - |  128 | `		 * call-depth cap is OP_CALL-only (BYTECODE.md stage 5).` |
|        - |  129 | `		 *` |
|        - |  130 | `		 * The nested exec shares the caller's VM frame, so a throw the CALLER's own` |
|        - |  131 | `		 * try catches runs that catch IN PLACE and comes back as a status — which was` |
|        - |  132 | `		 * dropped here, and the statement php abandons then carried on after the catch` |
|        - |  133 | `` 		 * had already run (`try { $r = eval('throw new E;'); echo "x"; } catch …` `` |
|        - |  134 | `		 * printed the catch AND the echo). Same rule and same guard as the match-arm /` |
|        - |  135 | `		 * switch-case / property-default sites: compare the recorded-resume fields` |
|        - |  136 | `		 * against a pre-exec SNAPSHOT, never against 0. */` |
|        - |  137 | `		{` |
|    14861 |  138 | `			const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|    14861 |  139 | `			const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|    14861 |  140 | `			rc = VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    14861 |  141 | `			if( pCtx ){` |
|        - |  142 | `				/* Set the execution result */` |
|    10175 |  143 | `				ph7_result_value(pCtx,&sResult);` |
|     5085 |  144 | `			}` |
|    14861 |  145 | `			PH7_MemObjRelease(&sResult);` |
|    14861 |  146 | `			if( rc != PH7_ABORT && VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){` |
|       18 |  147 | `				rcThrow = PH7_EXCEPTION;` |
|        8 |  148 | `			}` |
|        - |  149 | `		}` |
|        - |  150 | `	}` |
|     7449 |  151 | `Cleanup:` |
|        - |  152 | `	/* Cleanup the mess left behind */` |
|    14903 |  153 | `	pVm->pByteContainer = pByteCode;` |
|    14903 |  154 | `	SySetRelease(&aByteCode);` |
|        - |  155 | `	/* Restore the outer compile's generator state if this was a nested unit. */` |
|    14903 |  156 | `	if( bNested ){` |
|        5 |  157 | `		PH7_CompilerRestoreState(pVm,&sSavedGen);` |
|        2 |  158 | `	}` |
|        - |  159 | `	/* A ParseError raised above must reach the caller so the VM unwinds the rest` |
|        - |  160 | ``	 * of the statement; returning OK left `eval('bad'); echo 'x';` running the`` |
|        - |  161 | `	 * echo even though php had already thrown. */` |
|    14903 |  162 | `	return rcThrow;` |
|        5 |  163 | `}` |
|        - |  164 | `/*` |
|        - |  165 | ` * Execute a deferred class declaration (OP_CLASS_DEFER — see the deferral` |
|        - |  166 | ` * block comment in compile_class.c). At this point the declaration's` |
|        - |  167 | ` * execution order has been honored: any spl_autoload_register() statement` |
|        - |  168 | ` * that precedes it in the program has RUN, so each recorded dependency either` |
|        - |  169 | ` * resolves (possibly by autoload, fired inside PH7_VmExtractClass) or is` |
|        - |  170 | ` * genuinely missing. A missing one is reported through *ppMissing — the` |
|        - |  171 | ` * OP_CLASS_DEFER dispatcher throws php's catchable` |
|        - |  172 | `` * `Class/Interface/Trait "X" not found` Error. Once the dependencies resolve,`` |
|        - |  173 | ` * the captured chunk re-compiles through VmEvalChunk (which also mounts the` |
|        - |  174 | ` * newly installed classes); a compile failure there (e.g. a body syntax error` |
|        - |  175 | ` * whose diagnosis was deferred along with the declaration) has already been` |
|        - |  176 | ` * reported through the engine's error consumer, so it aborts execution.` |
|        - |  177 | ` * The site is idempotent: bDone short-circuits re-execution (a loop around an` |
|        - |  178 | ` * anonymous class instantiates the same installed class, php's` |
|        - |  179 | ` * one-class-per-site semantics).` |
|        - |  180 | ` */` |
|       30 |  181 | `PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing)` |
|        2 |  182 | `{` |
|        - |  183 | `	VmDeferredReq *aReq;` |
|        - |  184 | `	sxu32 n;` |
|       32 |  185 | `	*ppMissing = 0;` |
|       32 |  186 | `	if( pDefer->bDone ){` |
|        5 |  187 | `		return SXRET_OK;` |
|        - |  188 | `	}` |
|       28 |  189 | `	aReq = (VmDeferredReq *)SySetBasePtr(&pDefer->aRequired);` |
|       48 |  190 | `	for( n = 0 ; n < SySetUsed(&pDefer->aRequired) ; ++n ){` |
|       32 |  191 | `		if( PH7_VmExtractClass(&(*pVm),aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|       12 |  192 | `			*ppMissing = &aReq[n];` |
|       12 |  193 | `			return SXRET_OK; /* caller throws */` |
|        - |  194 | `		}` |
|       12 |  195 | `	}` |
|       18 |  196 | `	if( pDefer->sAnonName.nByte > 0 ){` |
|        5 |  197 | `		pVm->sDeferAnonName = pDefer->sAnonName;` |
|        2 |  198 | `	}` |
|       18 |  199 | `	VmEvalChunk(&(*pVm),0,&pDefer->sText,PH7_PHP_ONLY,TRUE);` |
|       18 |  200 | `	pVm->sDeferAnonName.zString = 0;` |
|       18 |  201 | `	pVm->sDeferAnonName.nByte = 0;` |
|       16 |  202 | `	if( pVm->nLastEvalErr > 0` |
|       18 |  203 | `	 \|\| PH7_VmExtractClass(&(*pVm),pDefer->sSelfName.zString,pDefer->sSelfName.nByte,FALSE,0) == 0 ){` |
|        - |  204 | `		/* The re-compile failed — a deferred-along syntax error or a` |
|        - |  205 | `		 * redeclaration fatal. It was already reported through the engine's` |
|        - |  206 | `		 * error consumer; halt like a fatal (php exits 255 for both). */` |
|      ! 0 |  207 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  208 | `		return SXERR_ABORT;` |
|        - |  209 | `	}` |
|       18 |  210 | `	pDefer->bDone = 1;` |
|       18 |  211 | `	return SXRET_OK;` |
|       17 |  212 | `}` |
|        - |  213 | `/*` |
|        - |  214 | ` * value eval(string $code)` |
|        - |  215 | ` *   Evaluate a string as PHP code.` |
|        - |  216 | ` * Parameter` |
|        - |  217 | ` *  code: PHP code to evaluate.` |
|        - |  218 | ` * Return` |
|        - |  219 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  220 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  221 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  222 | ` */` |
|      262 |  223 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  224 | `{` |
|        - |  225 | `	SyString sChunk;    /* Chunk to evaluate */` |
|        - |  226 | `	sxi32 rc;` |
|      267 |  227 | `	if( nArg < 1 ){` |
|        - |  228 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  229 | `		ph7_result_null(pCtx);` |
|      ! 0 |  230 | `		return SXRET_OK;` |
|        - |  231 | `	}` |
|        - |  232 | `	/* Chunk to evaluate. Coerced USER-VISIBLY like every other string argument` |
|        - |  233 | `	 * spelled as a language construct: an object with no __toString() is php's` |
|        - |  234 | `	 * Error, where PHL used to hand the parser the literal "Object" and answer a` |
|        - |  235 | `	 * ParseError. */` |
|        - |  236 | `	{` |
|      267 |  237 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sChunk.zString,(int *)&sChunk.nByte);` |
|      267 |  238 | `		if( rcSv != SXRET_OK ){` |
|        3 |  239 | `			return rcSv;` |
|        - |  240 | `		}` |
|        - |  241 | `	}` |
|      265 |  242 | `	if( sChunk.nByte < 1 ){` |
|        - |  243 | `		/* php: eval('') compiles nothing and yields FALSE (a whitespace-only` |
|        - |  244 | `		 * chunk still compiles, and yields NULL through the normal path). */` |
|        3 |  245 | `		ph7_result_bool(pCtx,0);` |
|        3 |  246 | `		return SXRET_OK;` |
|        - |  247 | `	}` |
|        - |  248 | `	/* Eval the chunk */` |
|      263 |  249 | `	rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|      263 |  250 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  251 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       71 |  252 | `		return PH7_ABORT;` |
|        - |  253 | `	}` |
|        - |  254 | `	/* Propagate a ParseError from a failed compile (php unwinds; PHL used to` |
|        - |  255 | `	 * keep executing the statement that contained the eval). */` |
|      195 |  256 | `	return rc;` |
|      136 |  257 | `}` |
|        - |  258 | `/*` |
|        - |  259 | ` * Check if a file path is already included.` |
|        - |  260 | ` */` |
|     9968 |  261 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        5 |  262 | `{` |
|        - |  263 | `	SyString *aEntries;` |
|        - |  264 | `	sxu32 n;` |
|     9973 |  265 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  266 | `	/* Perform a linear search */` |
| 24512233 |  267 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 24502279 |  268 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  269 | `			/* Already included */` |
|       19 |  270 | `			return TRUE;` |
|        - |  271 | `		}` |
| 12251133 |  272 | `	}` |
|     9957 |  273 | `	return FALSE;` |
|     4989 |  274 | `}` |
|        - |  275 | `/*` |
|        - |  276 | ` * Push a file path in the appropriate VM container.` |
|        - |  277 | ` */` |
|    14638 |  278 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 |  279 | `{` |
|        - |  280 | `	SyString sPath;` |
|        - |  281 | `	char *zDup;` |
|        - |  282 | `	sxi32 rc;` |
|    14643 |  283 | `	if( nLen < 0 ){` |
|     4675 |  284 | `		nLen = SyStrlen(zPath);` |
|     2335 |  285 | `	}` |
|        - |  286 | `	/* Duplicate the file path first */` |
|    14643 |  287 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    14643 |  288 | `	if( zDup == 0 ){` |
|      ! 0 |  289 | `		return SXERR_MEM;` |
|        - |  290 | `	}` |
|        - |  291 | `#ifdef __UNIXES__` |
|        - |  292 | `	/* php records the REALPATH'd absolute path for __FILE__/__DIR__/getFileName` |
|        - |  293 | `	 * and include-once dedup: realpath() resolves '.', '..' and symlinks (e.g.` |
|        - |  294 | `	 * macOS /tmp -> /private/tmp). It needs an existing file, so on failure keep` |
|        - |  295 | `	 * the raw path (eval'd code, php:///data:// wrappers, a missing include that` |
|        - |  296 | `	 * errors elsewhere). */` |
|        - |  297 | `	{` |
|    14638 |  298 | `		char *zReal = realpath(zDup,0); /* POSIX: malloc'd result */` |
|    14638 |  299 | `		if( zReal ){` |
|    14602 |  300 | `			sxu32 nReal = SyStrlen(zReal);` |
|    14602 |  301 | `			char *zRealDup = SyMemBackendStrDup(&pVm->sAllocator,zReal,nReal);` |
|    14602 |  302 | `			free(zReal);` |
|    14602 |  303 | `			if( zRealDup ){` |
|    14602 |  304 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|    14602 |  305 | `				zDup = zRealDup;` |
|    14602 |  306 | `				nLen = (int)nReal;` |
|     7301 |  307 | `			}` |
|     7301 |  308 | `		}` |
|        - |  309 | `	}` |
|        - |  310 | `#endif` |
|        - |  311 | `#ifdef __WINNT__` |
|        - |  312 | `	/* Windows counterpart of the realpath() above: canonicalize to an absolute` |
|        - |  313 | `	 * path (resolves '.'/'..' , '/' -> '\'), case-preserved to match php — NOT` |
|        - |  314 | `	 * lowercased as the legacy PH7 code did. Like the POSIX branch, keep the raw` |
|        - |  315 | `	 * path when it does not name an existing file (the "Command line code" marker,` |
|        - |  316 | `	 * eval'd code, stream wrappers). _fullpath() is lexical, so pair it with a VFS` |
|        - |  317 | `	 * existence check. */` |
|        - |  318 | `	{` |
|        5 |  319 | `		char *zFull = _fullpath(0,zDup,0); /* MSVC CRT: malloc'd absolute path */` |
|        5 |  320 | `		if( zFull ){` |
|        5 |  321 | `			if( pVm->pEngine->pVfs && pVm->pEngine->pVfs->xFileExists && pVm->pEngine->pVfs->xFileExists(zFull) == PH7_OK ){` |
|        5 |  322 | `				sxu32 nFull = SyStrlen(zFull);` |
|        5 |  323 | `				char *zFullDup = SyMemBackendStrDup(&pVm->sAllocator,zFull,nFull);` |
|        5 |  324 | `				if( zFullDup ){` |
|        5 |  325 | `					SyMemBackendFree(&pVm->sAllocator,zDup);` |
|        5 |  326 | `					zDup = zFullDup;` |
|        5 |  327 | `					nLen = (int)nFull;` |
|        - |  328 | `				}` |
|        - |  329 | `			}` |
|        5 |  330 | `			free(zFull);` |
|        - |  331 | `		}` |
|        - |  332 | `	}` |
|        - |  333 | `#endif` |
|        - |  334 | `	/* Install the file path */` |
|    14643 |  335 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    14643 |  336 | `	if( !bMain ){` |
|     9973 |  337 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  338 | `			/* Already included */` |
|       19 |  339 | `			*pNew = 0;` |
|       11 |  340 | `		}else{` |
|        - |  341 | `			/* Insert in the corresponding container */` |
|     9957 |  342 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|     9957 |  343 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  344 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  345 | `				return rc;` |
|        - |  346 | `			}` |
|     9957 |  347 | `			*pNew = 1;` |
|        - |  348 | `		}` |
|     4984 |  349 | `	}` |
|    14643 |  350 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    14643 |  351 | `	return SXRET_OK;` |
|     7324 |  352 | `}` |
|        - |  353 | `/*` |
|        - |  354 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  355 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  356 | ` * indicates failure.` |
|        - |  357 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  358 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  359 | ` * operations.` |
|        - |  360 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  361 | ` * this function is a no-op.` |
|        - |  362 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  363 | ` * constructs for more information.` |
|        - |  364 | ` */` |
|     9974 |  365 | `static sxi32 VmExecIncludedFile(` |
|        - |  366 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  367 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  368 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  369 | `	 )` |
|        5 |  370 | `{` |
|        - |  371 | `	sxi32 rc;` |
|        - |  372 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  373 | `	const ph7_io_stream *pStream;` |
|        - |  374 | `	SyBlob sContents;` |
|        - |  375 | `	void *pHandle;` |
|        - |  376 | `	ph7_vm *pVm;` |
|        - |  377 | `	int isNew;` |
|        - |  378 | `	/* Initialize fields */` |
|     9979 |  379 | `	pVm = pCtx->pVm;` |
|     9979 |  380 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9979 |  381 | `	isNew = 0;` |
|        - |  382 | `	/* Extract the associated stream */` |
|     9979 |  383 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  384 | `	/*` |
|        - |  385 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  386 | `	 * in a read-only mode.` |
|        - |  387 | `	 */` |
|     9979 |  388 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9979 |  389 | `	if( pHandle == 0 ){` |
|        8 |  390 | `		return SXERR_IO;` |
|        - |  391 | `	}` |
|     9973 |  392 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9973 |  393 | `	if( IncludeOnce && !isNew ){` |
|        - |  394 | `		/* Already included */` |
|       17 |  395 | `		rc = SXERR_EXISTS;` |
|       10 |  396 | `	}else{` |
|        - |  397 | `		/* Read the whole file contents */` |
|     9959 |  398 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9959 |  399 | `		if( rc == SXRET_OK ){` |
|        - |  400 | `			SyString sScript;` |
|        - |  401 | `			/* Compile and execute the script. A throw the included file raised — and the` |
|        - |  402 | `			 * INCLUDING statement's own try caught in place — comes back as PH7_EXCEPTION` |
|        - |  403 | `			 * and travels out to the include builtin's caller, which unwinds the rest of` |
|        - |  404 | `			 * the statement instead of resuming it. It is not an IO failure, so the` |
|        - |  405 | `			 * callers must tell the two apart before warning. */` |
|     9959 |  406 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9959 |  407 | `			rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     9959 |  408 | `			if( rc != PH7_EXCEPTION ){` |
|     9951 |  409 | `				rc = SXRET_OK;` |
|     4973 |  410 | `			}` |
|     4977 |  411 | `		}` |
|        - |  412 | `	}` |
|        - |  413 | `	/* Pop from the set of included file */` |
|     9973 |  414 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  415 | `	/* Close the handle */` |
|     9973 |  416 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  417 | `	/* Release the working buffer */` |
|     9973 |  418 | `	SyBlobRelease(&sContents);` |
|        - |  419 | `#else` |
|        - |  420 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  421 | `	SXUNUSED(pPath);` |
|        - |  422 | `	SXUNUSED(IncludeOnce);` |
|        - |  423 | `	rc = SXERR_IO;` |
|        - |  424 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9973 |  425 | `	return rc;` |
|     4992 |  426 | `}` |
|        - |  427 | `/*` |
|        - |  428 | ` * string get_include_path(void)` |
|        - |  429 | ` *  Gets the current include_path configuration option.` |
|        - |  430 | ` * Parameter` |
|        - |  431 | ` *  None` |
|        - |  432 | ` * Return` |
|        - |  433 | ` *  Included paths as a string` |
|        - |  434 | ` */` |
|        8 |  435 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  436 | `{` |
|       10 |  437 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  438 | `	SyString *aEntry;` |
|        - |  439 | `	int dir_sep;` |
|        - |  440 | `	sxu32 n;` |
|        - |  441 | `#ifdef __WINNT__` |
|        2 |  442 | `	dir_sep = ';';` |
|        - |  443 | `#else` |
|        - |  444 | `	/* Assume UNIX path separator */` |
|        8 |  445 | `	dir_sep = ':';` |
|        - |  446 | `#endif` |
|        4 |  447 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 |  448 | `	SXUNUSED(apArg);` |
|        - |  449 | `	/* Point to the list of import paths */` |
|       10 |  450 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       20 |  451 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|       12 |  452 | `		SyString *pEntry = &aEntry[n];` |
|       12 |  453 | `		if( n > 0 ){` |
|        - |  454 | `			/* Append dir seprator */` |
|        2 |  455 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|        1 |  456 | `		}` |
|        - |  457 | `		/* Append path */` |
|       12 |  458 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        7 |  459 | `	}` |
|       10 |  460 | `	return PH7_OK;` |
|        2 |  461 | `}` |
|        - |  462 | `/*` |
|        - |  463 | ` * string\|false set_include_path(string $include_path)` |
|        - |  464 | ` *  Sets the include_path configuration option for the duration of the script,` |
|        - |  465 | ` *  returning the OLD value (php contract).` |
|        - |  466 | ` */` |
|        6 |  467 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  468 | `{` |
|        7 |  469 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  470 | `	SyString *aEntry;` |
|        - |  471 | `	const char *zNew, *z, *zEnd;` |
|        - |  472 | `	int dir_sep, nLen;` |
|        - |  473 | `	sxu32 n;` |
|        - |  474 | `#ifdef __WINNT__` |
|        1 |  475 | `	dir_sep = ';';` |
|        - |  476 | `#else` |
|        6 |  477 | `	dir_sep = ':';` |
|        - |  478 | `#endif` |
|        - |  479 | `	/* Build the OLD include_path first: it is this call's return value */` |
|        7 |  480 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       15 |  481 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        9 |  482 | `		if( n > 0 ){ ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char)); }` |
|        9 |  483 | `		ph7_result_string(pCtx,aEntry[n].zString,(int)aEntry[n].nByte);` |
|        5 |  484 | `	}` |
|        7 |  485 | `	if( nArg < 1 ){` |
|      ! 0 |  486 | `		return PH7_OK;` |
|        - |  487 | `	}` |
|        - |  488 | `	/* Replace the path set with the separated segments of the new value.` |
|        - |  489 | `	 * Segments are duped into the VM allocator (reclaimed at VM teardown) so` |
|        - |  490 | `	 * the SyString entries stay valid, mirroring the config-time literals. */` |
|        7 |  491 | `	zNew = ph7_value_to_string(apArg[0],&nLen);` |
|        7 |  492 | `	SySetReset(&pVm->aPaths);` |
|        7 |  493 | `	z = zNew; zEnd = &zNew[nLen];` |
|       15 |  494 | `	while( z < zEnd ){` |
|        9 |  495 | `		const char *zStart = z;` |
|        - |  496 | `		SyString sPath;` |
|       49 |  497 | `		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }` |
|        9 |  498 | `		if( z > zStart ){` |
|        9 |  499 | `			char *zDup = (char *)SyMemBackendDup(&pVm->sAllocator,zStart,(sxu32)(z-zStart));` |
|        9 |  500 | `			if( zDup ){` |
|        9 |  501 | `				SyStringInitFromBuf(&sPath,zDup,(sxu32)(z-zStart));` |
|        - |  502 | `#ifdef __WINNT__` |
|        1 |  503 | `				SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  504 | `#endif` |
|        9 |  505 | `				SyStringTrimTrailingChar(&sPath,'/');` |
|        9 |  506 | `				SyStringFullTrim(&sPath);` |
|        9 |  507 | `				if( sPath.nByte > 0 ){` |
|        9 |  508 | `					SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|        4 |  509 | `				}` |
|        4 |  510 | `			}` |
|        4 |  511 | `		}` |
|        9 |  512 | `		if( z < zEnd ){ z++; } /* skip the separator */` |
|        1 |  513 | `	}` |
|        7 |  514 | `	return PH7_OK;` |
|        4 |  515 | `}` |
|        - |  516 | `/*` |
|        - |  517 | ` * string get_get_included_files(void)` |
|        - |  518 | ` *  Gets the current include_path configuration option.` |
|        - |  519 | ` * Parameter` |
|        - |  520 | ` *  None` |
|        - |  521 | ` * Return` |
|        - |  522 | ` *  Included paths as a string` |
|        - |  523 | ` */` |
|        2 |  524 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  525 | `{` |
|        3 |  526 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  527 | `	ph7_value *pArray,*pWorker;` |
|        - |  528 | `	SyString *pEntry;` |
|        - |  529 | `	int c,d;` |
|        - |  530 | `	/* Create an array and a working value */` |
|        3 |  531 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  532 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  533 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  534 | `		/* Out of memory,return null */` |
|      ! 0 |  535 | `		ph7_result_null(pCtx);` |
|      ! 0 |  536 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  537 | `		SXUNUSED(apArg);` |
|      ! 0 |  538 | `		return PH7_OK;` |
|        - |  539 | `	}` |
|        3 |  540 | `	c = d = '/';` |
|        - |  541 | `#ifdef __WINNT__` |
|        1 |  542 | `	d = '\\';` |
|        - |  543 | `#endif` |
|        - |  544 | `	/* Iterate throw entries */` |
|        3 |  545 | `	SySetResetCursor(pFiles);` |
|        7 |  546 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  547 | `		const char *zBase,*zEnd;` |
|        - |  548 | `		int iLen;` |
|        - |  549 | `		/* reset the string cursor */` |
|        5 |  550 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  551 | `		/* Extract base name */` |
|        5 |  552 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  553 | `		/* Ignore trailing '/' */` |
|        7 |  554 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  555 | `			zEnd--;` |
|      ! 0 |  556 | `		}` |
|        5 |  557 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|       79 |  558 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|       73 |  559 | `			zEnd--;` |
|        1 |  560 | `		}` |
|        5 |  561 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|        5 |  562 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  563 | `		/* Copy entry name */` |
|        5 |  564 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  565 | `		/* Perform the insertion */` |
|        5 |  566 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  567 | `	}` |
|        - |  568 | `	/* All done,return the created array */` |
|        3 |  569 | `	ph7_result_value(pCtx,pArray);` |
|        - |  570 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  571 | `	 * by the engine as soon we return from this foreign` |
|        - |  572 | `	 * function.` |
|        - |  573 | `	 */` |
|        3 |  574 | `	return PH7_OK;` |
|        2 |  575 | `}` |
|        - |  576 | `/*` |
|        - |  577 | ` * include:` |
|        - |  578 | ` * According to the PHP reference manual.` |
|        - |  579 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  580 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  581 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  582 | ` *  include() will finally check in the calling script's own directory` |
|        - |  583 | ` *  and the current working directory before failing. The include()` |
|        - |  584 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  585 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  586 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  587 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  588 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  589 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  590 | ` *  directory to find the requested file.` |
|        - |  591 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  592 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  593 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  594 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  595 | ` */` |
|     9900 |  596 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  597 | `{` |
|        - |  598 | `	SyString sFile;` |
|        - |  599 | `	sxi32 rc;` |
|     9904 |  600 | `	if( nArg < 1 ){` |
|        - |  601 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  602 | `		ph7_result_null(pCtx);` |
|      ! 0 |  603 | `		return SXRET_OK;` |
|        - |  604 | `	}` |
|        - |  605 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  606 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  607 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  608 | `	{` |
|     9904 |  609 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|     9904 |  610 | `		if( rcSv != SXRET_OK ){` |
|        3 |  611 | `			return rcSv;` |
|        - |  612 | `		}` |
|        - |  613 | `	}` |
|     9902 |  614 | `	if( sFile.nByte < 1 ){` |
|        - |  615 | `		/* Empty string,return NULL */` |
|      ! 0 |  616 | `		ph7_result_null(pCtx);` |
|      ! 0 |  617 | `		return SXRET_OK;` |
|        - |  618 | `	}` |
|        - |  619 | `	/* Open,compile and execute the desired script */` |
|     9902 |  620 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9902 |  621 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - |  622 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - |  623 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - |  624 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - |  625 | `		 * wins, so it is tested first. */` |
|        3 |  626 | `		return rc;` |
|        - |  627 | `	}` |
|     9900 |  628 | `	if( rc != SXRET_OK ){` |
|        - |  629 | `		/* Emit a warning and return false */` |
|        3 |  630 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  631 | `		ph7_result_bool(pCtx,0);` |
|        1 |  632 | `	}` |
|     9900 |  633 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  634 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 |  635 | `		return PH7_ABORT;` |
|        - |  636 | `	}` |
|     9895 |  637 | `	return SXRET_OK;` |
|     4954 |  638 | `}` |
|        - |  639 | `/*` |
|        - |  640 | ` * include_once:` |
|        - |  641 | ` *  According to the PHP reference manual.` |
|        - |  642 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  643 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  644 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  645 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  646 | ` *   just once.` |
|        - |  647 | ` */` |
|       20 |  648 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  649 | `{` |
|        - |  650 | `	SyString sFile;` |
|        - |  651 | `	sxi32 rc;` |
|       23 |  652 | `	if( nArg < 1 ){` |
|        - |  653 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  654 | `		ph7_result_null(pCtx);` |
|      ! 0 |  655 | `		return SXRET_OK;` |
|        - |  656 | `	}` |
|        - |  657 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  658 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  659 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  660 | `	{` |
|       23 |  661 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       23 |  662 | `		if( rcSv != SXRET_OK ){` |
|        3 |  663 | `			return rcSv;` |
|        - |  664 | `		}` |
|        - |  665 | `	}` |
|       21 |  666 | `	if( sFile.nByte < 1 ){` |
|        - |  667 | `		/* Empty string,return NULL */` |
|      ! 0 |  668 | `		ph7_result_null(pCtx);` |
|      ! 0 |  669 | `		return SXRET_OK;` |
|        - |  670 | `	}` |
|        - |  671 | `	/* Open,compile and execute the desired script */` |
|       21 |  672 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       21 |  673 | `	if( rc == SXERR_EXISTS ){` |
|        - |  674 | `		/* File already included,return TRUE */` |
|       12 |  675 | `		ph7_result_bool(pCtx,1);` |
|       12 |  676 | `		return SXRET_OK;` |
|        - |  677 | `	}` |
|       11 |  678 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - |  679 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - |  680 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - |  681 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - |  682 | `		 * wins, so it is tested first. */` |
|        3 |  683 | `		return rc;` |
|        - |  684 | `	}` |
|        8 |  685 | `	if( rc != SXRET_OK ){` |
|        - |  686 | `		/* Emit a warning and return false */` |
|      ! 0 |  687 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  688 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  689 | ` 	}` |
|        8 |  690 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  691 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  692 | `		return PH7_ABORT;` |
|        - |  693 | `	}` |
|        8 |  694 | `	return SXRET_OK;` |
|       13 |  695 | `}` |
|        - |  696 | `/*` |
|        - |  697 | ` * require.` |
|        - |  698 | ` *  According to the PHP reference manual.` |
|        - |  699 | ` *   require() is identical to include() except upon failure it will` |
|        - |  700 | ` *   also produce a fatal level error.` |
|        - |  701 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  702 | ` *   emits a warning  which allows the script to continue.` |
|        - |  703 | ` */` |
|       46 |  704 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  705 | `{` |
|        - |  706 | `	SyString sFile;` |
|        - |  707 | `	sxi32 rc;` |
|       49 |  708 | `	if( nArg < 1 ){` |
|        - |  709 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  710 | `		ph7_result_null(pCtx);` |
|      ! 0 |  711 | `		return SXRET_OK;` |
|        - |  712 | `	}` |
|        - |  713 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  714 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  715 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  716 | `	{` |
|       49 |  717 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       49 |  718 | `		if( rcSv != SXRET_OK ){` |
|        3 |  719 | `			return rcSv;` |
|        - |  720 | `		}` |
|        - |  721 | `	}` |
|       47 |  722 | `	if( sFile.nByte < 1 ){` |
|        - |  723 | `		/* Empty string,return NULL */` |
|      ! 0 |  724 | `		ph7_result_null(pCtx);` |
|      ! 0 |  725 | `		return SXRET_OK;` |
|        - |  726 | `	}` |
|        - |  727 | `	/* Open,compile and execute the desired script */` |
|       47 |  728 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|       47 |  729 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - |  730 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - |  731 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - |  732 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - |  733 | `		 * wins, so it is tested first. */` |
|        3 |  734 | `		return rc;` |
|        - |  735 | `	}` |
|       45 |  736 | `	if( rc != SXRET_OK ){` |
|        - |  737 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  738 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  739 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  740 | `		return PH7_ABORT;` |
|        - |  741 | `	}` |
|       45 |  742 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  743 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  744 | `		return PH7_ABORT;` |
|        - |  745 | `	}` |
|       45 |  746 | `	return SXRET_OK;` |
|       26 |  747 | `}` |
|        - |  748 | `/*` |
|        - |  749 | ` * require_once:` |
|        - |  750 | ` *  According to the PHP reference manual.` |
|        - |  751 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  752 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  753 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  754 | ` *   and how it differs from its non _once siblings.` |
|        - |  755 | ` */` |
|       12 |  756 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  757 | `{` |
|        - |  758 | `	SyString sFile;` |
|        - |  759 | `	sxi32 rc;` |
|       15 |  760 | `	if( nArg < 1 ){` |
|        - |  761 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  762 | `		ph7_result_null(pCtx);` |
|      ! 0 |  763 | `		return SXRET_OK;` |
|        - |  764 | `	}` |
|        - |  765 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  766 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  767 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  768 | `	{` |
|       15 |  769 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       15 |  770 | `		if( rcSv != SXRET_OK ){` |
|        3 |  771 | `			return rcSv;` |
|        - |  772 | `		}` |
|        - |  773 | `	}` |
|       13 |  774 | `	if( sFile.nByte < 1 ){` |
|        - |  775 | `		/* Empty string,return NULL */` |
|      ! 0 |  776 | `		ph7_result_null(pCtx);` |
|      ! 0 |  777 | `		return SXRET_OK;` |
|        - |  778 | `	}` |
|        - |  779 | `	/* Open,compile and execute the desired script */` |
|       13 |  780 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       13 |  781 | `	if( rc == SXERR_EXISTS ){` |
|        - |  782 | `		/* File already included,return TRUE */` |
|        6 |  783 | `		ph7_result_bool(pCtx,1);` |
|        6 |  784 | `		return SXRET_OK;` |
|        - |  785 | `	}` |
|        9 |  786 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - |  787 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - |  788 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - |  789 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - |  790 | `		 * wins, so it is tested first. */` |
|        3 |  791 | `		return rc;` |
|        - |  792 | `	}` |
|        6 |  793 | `	if( rc != SXRET_OK ){` |
|        - |  794 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  795 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  796 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  797 | `		return PH7_ABORT;` |
|        - |  798 | `	}` |
|        6 |  799 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  800 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  801 | `		return PH7_ABORT;` |
|        - |  802 | `	}` |
|        6 |  803 | `	return SXRET_OK;` |
|        9 |  804 | `}` |
|        - |  805 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  806 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  807 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  808 | `/*` |
|        - |  809 | ` * Section:` |
|        - |  810 | ` *  SPL Autoloading functions.` |
|        - |  811 | ` * Status:` |
|        - |  812 | ` *  Stable.` |
|        - |  813 | ` */` |
|        - |  814 | `/*` |
|        - |  815 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - |  816 | ` *  Register given function as __autoload() implementation.` |
|        - |  817 | ` * Parameters` |
|        - |  818 | ` *  callback` |
|        - |  819 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - |  820 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - |  821 | ` *  throw` |
|        - |  822 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - |  823 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - |  824 | ` *  prepend` |
|        - |  825 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - |  826 | ` *   autoload stack instead of appending it.` |
|        - |  827 | ` * Return` |
|        - |  828 | ` *  TRUE on success, FALSE on failure.` |
|        - |  829 | ` */` |
|       58 |  830 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  831 | `{` |
|        - |  832 | `	VmAutoloadCB sEntry;` |
|       63 |  833 | `	ph7_vm *pVm = pCtx->pVm;` |
|       63 |  834 | `	int iPrepend = 0;` |
|        - |  835 | `	sxu32 n;` |
|       63 |  836 | `	if( nArg < 1 ){` |
|        - |  837 | `		/* No callback provided — register default spl_autoload.` |
|        - |  838 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - |  839 | `		/* Check for duplicates first */` |
|        9 |  840 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 |  841 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        4 |  842 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 |  843 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 |  844 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 |  845 | `				ph7_result_bool(pCtx,1);` |
|        5 |  846 | `				return SXRET_OK;` |
|        - |  847 | `			}` |
|      ! 0 |  848 | `		}` |
|        5 |  849 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 |  850 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 |  851 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 |  852 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 |  853 | `		ph7_result_bool(pCtx,1);` |
|        5 |  854 | `		return SXRET_OK;` |
|        - |  855 | `	}` |
|        - |  856 | `	/* Validate that the callback is callable */` |
|       55 |  857 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 |  858 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 |  859 | `		if( nArg >= 2 ){` |
|      ! 0 |  860 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  861 | `		}` |
|      ! 0 |  862 | `		if( iThrow ){` |
|      ! 0 |  863 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - |  864 | `				"Argument is not callable");` |
|      ! 0 |  865 | `		}` |
|      ! 0 |  866 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  867 | `		return SXRET_OK;` |
|        - |  868 | `	}` |
|        - |  869 | `	/* Check for duplicates */` |
|       73 |  870 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 |  871 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 |  872 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  873 | `			/* Already registered */` |
|      ! 0 |  874 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  875 | `			return SXRET_OK;` |
|        - |  876 | `		}` |
|       11 |  877 | `	}` |
|        - |  878 | `	/* Check prepend flag */` |
|       55 |  879 | `	if( nArg >= 3 ){` |
|        3 |  880 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 |  881 | `	}` |
|        - |  882 | `	/* Store the callback */` |
|       55 |  883 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       55 |  884 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       55 |  885 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       56 |  886 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - |  887 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - |  888 | `		 * We do this by appending first, then rotating the array. */` |
|        3 |  889 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - |  890 | `		VmAutoloadCB *aBase;` |
|        3 |  891 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  892 | `		/* Rotate: move last entry to front */` |
|        3 |  893 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 |  894 | `		if( aBase ){` |
|        - |  895 | `			VmAutoloadCB sTemp;` |
|        - |  896 | `			sxu32 i;` |
|        3 |  897 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 |  898 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 |  899 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 |  900 | `			}` |
|        3 |  901 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 |  902 | `		}` |
|        2 |  903 | `	}else{` |
|       53 |  904 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  905 | `	}` |
|       55 |  906 | `	ph7_result_bool(pCtx,1);` |
|       55 |  907 | `	return SXRET_OK;` |
|       34 |  908 | `}` |
|        - |  909 | `/*` |
|        - |  910 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - |  911 | ` *  Unregister a given function as __autoload() implementation.` |
|        - |  912 | ` * Parameters` |
|        - |  913 | ` *  callback` |
|        - |  914 | ` *   The autoload function being unregistered.` |
|        - |  915 | ` * Return` |
|        - |  916 | ` *  TRUE on success, FALSE on failure.` |
|        - |  917 | ` */` |
|       36 |  918 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  919 | `{` |
|       41 |  920 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  921 | `	sxu32 n,nEntry;` |
|       41 |  922 | `	if( nArg < 1 ){` |
|      ! 0 |  923 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  924 | `		return SXRET_OK;` |
|        - |  925 | `	}` |
|       41 |  926 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       45 |  927 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       43 |  928 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       43 |  929 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  930 | `			/* Found — remove by shifting remaining entries down */` |
|       39 |  931 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - |  932 | `			sxu32 i;` |
|       39 |  933 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       53 |  934 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 |  935 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 |  936 | `			}` |
|        - |  937 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       39 |  938 | `			SySetPop(&pVm->aAutoload);` |
|       39 |  939 | `			ph7_result_bool(pCtx,1);` |
|       39 |  940 | `			return SXRET_OK;` |
|        - |  941 | `		}` |
|        3 |  942 | `	}` |
|        3 |  943 | `	ph7_result_bool(pCtx,0);` |
|        3 |  944 | `	return SXRET_OK;` |
|       23 |  945 | `}` |
|        - |  946 | `/*` |
|        - |  947 | ` * array spl_autoload_functions(void)` |
|        - |  948 | ` *  Return all registered __autoload() functions.` |
|        - |  949 | ` * Return` |
|        - |  950 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - |  951 | ` *  an empty array is returned.` |
|        - |  952 | ` */` |
|       20 |  953 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  954 | `{` |
|       21 |  955 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  956 | `	ph7_value *pArray;` |
|        - |  957 | `	sxu32 n,nEntry;` |
|       10 |  958 | `	SXUNUSED(nArg);` |
|       10 |  959 | `	SXUNUSED(apArg);` |
|       21 |  960 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 |  961 | `	if( pArray == 0 ){` |
|      ! 0 |  962 | `		ph7_result_null(pCtx);` |
|      ! 0 |  963 | `		return SXRET_OK;` |
|        - |  964 | `	}` |
|       21 |  965 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 |  966 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 |  967 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 |  968 | `		if( pEntry ){` |
|       15 |  969 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 |  970 | `		}` |
|        8 |  971 | `	}` |
|       21 |  972 | `	ph7_result_value(pCtx,pArray);` |
|       21 |  973 | `	return SXRET_OK;` |
|       11 |  974 | `}` |
|        - |  975 | `/*` |
|        - |  976 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - |  977 | ` *  Default implementation of __autoload().` |
|        - |  978 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - |  979 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - |  980 | ` * Parameters` |
|        - |  981 | ` *  class` |
|        - |  982 | ` *   The class name being searched.` |
|        - |  983 | ` *  file_extensions` |
|        - |  984 | ` *   Comma-separated list of file extensions to try.` |
|        - |  985 | ` */` |
|        2 |  986 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  987 | `{` |
|        - |  988 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - |  989 | `	SyBlob sPath;` |
|        - |  990 | `	int nClass;` |
|        - |  991 | `	sxi32 rc;` |
|        3 |  992 | `	if( nArg < 1 ){` |
|      ! 0 |  993 | `		return SXRET_OK;` |
|        - |  994 | `	}` |
|        3 |  995 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 |  996 | `	if( nClass < 1 ){` |
|      ! 0 |  997 | `		return SXRET_OK;` |
|        - |  998 | `	}` |
|        - |  999 | `	/* Default extensions */` |
|        3 | 1000 | `	zExt = ".php,.inc";` |
|        3 | 1001 | `	if( nArg >= 2 ){` |
|        - | 1002 | `		int nExt;` |
|      ! 0 | 1003 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 1004 | `		if( nExt < 1 ){` |
|      ! 0 | 1005 | `			zExt = ".php,.inc";` |
|      ! 0 | 1006 | `		}` |
|      ! 0 | 1007 | `	}` |
|        3 | 1008 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 1009 | `	/* Iterate over comma-separated extensions */` |
|        3 | 1010 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 1011 | `	zCur = zExt;` |
|        7 | 1012 | `	while( zCur < zEnd ){` |
|        - | 1013 | `		const char *zComma;` |
|        - | 1014 | `		SyString sFile;` |
|        - | 1015 | `		int i;` |
|        - | 1016 | `		/* Find next comma or end */` |
|        5 | 1017 | `		zComma = zCur;` |
|       21 | 1018 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 1019 | `			zComma++;` |
|        1 | 1020 | `		}` |
|        - | 1021 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 1022 | `		SyBlobReset(&sPath);` |
|       69 | 1023 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 1024 | `			char c = zClass[i];` |
|       65 | 1025 | `			if( c == '\\' ){` |
|      ! 0 | 1026 | `				c = '/';` |
|       65 | 1027 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 1028 | `				c = c + ('a' - 'A');` |
|        6 | 1029 | `			}` |
|       65 | 1030 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 1031 | `		}` |
|        - | 1032 | `		/* Append extension */` |
|        5 | 1033 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 1034 | `		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,` |
|        - | 1035 | `		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a` |
|        - | 1036 | `		 * terminator, so without this the strlen reads past the buffer (a` |
|        - | 1037 | `		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL` |
|        - | 1038 | `		 * is not counted in SyBlobLength(), so the SyString length stays correct. */` |
|        5 | 1039 | `		SyBlobNullAppend(&sPath);` |
|        - | 1040 | `		/* Try to include the file */` |
|        5 | 1041 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 1042 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 1043 | `		if( rc == SXRET_OK \|\| rc == PH7_EXCEPTION ){` |
|        - | 1044 | `			/* Included — or it threw, which ends the search too: the remaining` |
|        - | 1045 | `			 * extensions are not tried after a file has already run. */` |
|      ! 0 | 1046 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 1047 | `			return rc == PH7_EXCEPTION ? rc : SXRET_OK;` |
|        - | 1048 | `		}` |
|        - | 1049 | `		/* Move past the comma */` |
|        5 | 1050 | `		zCur = zComma;` |
|        5 | 1051 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 1052 | `			zCur++;` |
|        1 | 1053 | `		}` |
|        1 | 1054 | `	}` |
|        3 | 1055 | `	SyBlobRelease(&sPath);` |
|        3 | 1056 | `	return SXRET_OK;` |
|        2 | 1057 | `}` |
|        - | 1058 | `/* Table of built-in VM functions. */` |
|        - | 1059 | `/*` |
|        - | 1060 | ` * Packing body for __call / __callStatic (band A #3b).` |
|        - | 1061 | ` * OP_MEMBER, on a missing or inaccessible method whose class declares the magic` |
|        - | 1062 | ` * handler, stashes {receiver, class, original name} on the VM and marks the callee` |
|        - | 1063 | ` * slot MEMOBJ_AUX_MAGICCALL; the normal OP_CALL machinery then collects the` |
|        - | 1064 | ` * ORIGINAL argument list (incl. spreads) and hands it here, which packs it into a` |
|        - | 1065 | ` * php array and invokes` |
|        - | 1066 | ` *   $recv->__call($name, $args)   /   Class::__callStatic($name, $args)` |
|        - | 1067 | ` * returning the handler's value as the call's result. A throw propagates via the` |
|        - | 1068 | ` * returned status (and the boundary rail).` |
|        - | 1069 | ` *` |
|        - | 1070 | `` * This used to be a REGISTERED host function named `__phl_magic_call` whose name`` |
|        - | 1071 | ` * the four OP_MEMBER sites wrote into the callee slot — so the engine's own` |
|        - | 1072 | ` * dispatch was spelled as a global PHP function that function_exists() and` |
|        - | 1073 | ` * get_defined_functions() both reported, and that any script could call. It is` |
|        - | 1074 | ` * reached through the VM's own function record now (PH7_VmMagicCallFunc); the` |
|        - | 1075 | ` * mark on the slot is the only thing that selects it, and there is no name.` |
|        - | 1076 | ` */` |
|      122 | 1077 | `static int VmMagicCallDispatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 1078 | `{` |
|      125 | 1079 | `	ph7_vm *pVm = pCtx->pVm;` |
|      125 | 1080 | `	ph7_class_instance *pRecv = pVm->pMagicCallThis;` |
|      125 | 1081 | `	ph7_class *pClass = pVm->pMagicCallClass;` |
|        - | 1082 | `	ph7_value sResult;` |
|        - | 1083 | `	SyString sMethName;` |
|        - | 1084 | `	sxi32 rc;` |
|        - | 1085 | `	/* Consume the pending dispatch (one-shot) */` |
|      125 | 1086 | `	pVm->pMagicCallThis = 0;` |
|      125 | 1087 | `	pVm->pMagicCallClass = 0;` |
|      125 | 1088 | `	if( pClass == 0 ){` |
|        - | 1089 | `		/* Defensive: the mark and the latch are set together by OP_MEMBER, so a` |
|        - | 1090 | `		 * classless arrival is unreachable. It was reachable while this body wore a` |
|        - | 1091 | ``		 * function NAME — anyone could call `__phl_magic_call()` — which is exactly`` |
|        - | 1092 | `		 * what the mark retired. */` |
|      ! 0 | 1093 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1094 | `		return PH7_OK;` |
|        - | 1095 | `	}` |
|      125 | 1096 | `	SyStringInitFromBuf(&sMethName,SyBlobData(&pVm->sMagicCallName),SyBlobLength(&pVm->sMagicCallName));` |
|      125 | 1097 | `	PH7_MemObjInit(pVm,&sResult);` |
|        - | 1098 | `	/* The one packing site, shared with every CALLABLE spelling of the same call` |
|        - | 1099 | `	 * (PH7_VmDispatchMagicCall, vm_builtin_call.c): the handler lookup, the $args array` |
|        - | 1100 | `	 * — named arguments keyed by name, as php keys them — and the engine-dispatch` |
|        - | 1101 | `	 * visibility rule all live there. Two copies of this is how the syntax path and the` |
|        - | 1102 | `	 * callable path came to disagree about the argument names in the first place.` |
|        - | 1103 | `	 * SXERR_NOTFOUND is unreachable: OP_MEMBER verified the handler exists. */` |
|      186 | 1104 | `	rc = PH7_VmDispatchMagicCall(pVm,pClass,pRecv,SyStringData(&sMethName),` |
|       61 | 1105 | `		SyStringLength(&sMethName),&sResult,nArg,apArg,pCtx->pArgMap);` |
|      125 | 1106 | `	if( rc == SXRET_OK ){` |
|      119 | 1107 | `		ph7_result_value(pCtx,&sResult);` |
|       58 | 1108 | `	}` |
|      125 | 1109 | `	PH7_MemObjRelease(&sResult);` |
|      125 | 1110 | `	if( pRecv ){` |
|       84 | 1111 | `		PH7_ClassInstanceUnref(pRecv);` |
|       41 | 1112 | `	}` |
|      125 | 1113 | `	SyBlobReset(&pVm->sMagicCallName);` |
|      125 | 1114 | `	return (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) ? rc : PH7_OK;` |
|       64 | 1115 | `}` |
|        - | 1116 | `/*` |
|        - | 1117 | ` * The function record OP_CALL dispatches a MEMOBJ_AUX_MAGICCALL callee slot through,` |
|        - | 1118 | ` * built on first use and owned by the VM (the allocator frees it with everything else).` |
|        - | 1119 | ` *` |
|        - | 1120 | ` * It is deliberately NOT installed in pVm->hHostFunction: the same property that makes` |
|        - | 1121 | ` * a native class method unreachable except by dispatching the method (see` |
|        - | 1122 | ` * PH7_NativeClassInstallMethod) makes this body unreachable except by the engine's own` |
|        - | 1123 | ` * __call routing. It carries no signature and no arity bounds, so the OP_CALL choke` |
|        - | 1124 | ` * point's ZPP and ArgumentCountError screens are inert for it — the handler's OWN` |
|        - | 1125 | ` * declared parameters are what php enforces, and it is a PHP method with a frame of its` |
|        - | 1126 | ` * own. sName is a diagnostic label only; nothing that reads it can be reached from here.` |
|        - | 1127 | ` */` |
|      122 | 1128 | `PH7_PRIVATE ph7_user_func * PH7_VmMagicCallFunc(ph7_vm *pVm)` |
|        3 | 1129 | `{` |
|        - | 1130 | `	SyString sName;` |
|      125 | 1131 | `	if( pVm->pMagicCallFunc == 0 ){` |
|       11 | 1132 | `		SyStringInitFromBuf(&sName,"__call",sizeof("__call")-1);` |
|       12 | 1133 | `		if( PH7_NewForeignFunction(&(*pVm),&sName,VmMagicCallDispatch,0,` |
|       11 | 1134 | `			&pVm->pMagicCallFunc) != SXRET_OK ){` |
|      ! 0 | 1135 | `			pVm->pMagicCallFunc = 0;` |
|      ! 0 | 1136 | `		}` |
|        4 | 1137 | `	}` |
|      125 | 1138 | `	return pVm->pMagicCallFunc;` |
|        3 | 1139 | `}` |
|        - | 1140 |  |
