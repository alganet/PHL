# src/ph7/vm_include.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 500/587 lines (85.18%)

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
|    15576 |   22 | `PH7_PRIVATE sxi32 VmEvalChunk(` |
|        - |   23 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |   24 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |   25 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |   26 | `	int iFlags,         /* Compile flag */` |
|        - |   27 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |   28 | `	)` |
|        5 |   29 | `{` |
|        - |   30 | `	SySet *pByteCode,aByteCode;` |
|    15581 |   31 | `	ProcConsumer xErr = 0;` |
|    15581 |   32 | `	void *pErrData = 0;` |
|        - |   33 | `	ph7_gen_state sSavedGen;` |
|        - |   34 | `	int bNested;` |
|    15581 |   35 | `	sxi32 rcThrow = SXRET_OK; /* a ParseError raised for a failed compile, or a throw the` |
|        - |   36 | `	                           * evaluated chunk raised — either way the caller must unwind */` |
|        - |   37 | `	/* Initialize bytecode container */` |
|    15581 |   38 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    15581 |   39 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |   40 | `	/* Reset the code generator */` |
|    15581 |   41 | `	if( bTrueReturn ){` |
|        - |   42 | `		/* Included file,log compile-time errors */` |
|    10169 |   43 | `		xErr = pVm->pEngine->xConf.xErr;` |
|    10169 |   44 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     5082 |   45 | `	}` |
|        - |   46 | `	/* A non-zero cursor means an OUTER compile is in flight — this eval/include` |
|        - |   47 | `	 * was reached from inside it (an autoload fired while resolving a base class).` |
|        - |   48 | `	 * Wiping the generator would corrupt that outer parse, so snapshot it and hand` |
|        - |   49 | `	 * the nested unit a fresh one; a plain runtime eval/include just resets. */` |
|    15581 |   50 | `	bNested = (pVm->sCodeGen.pIn != 0);` |
|    15581 |   51 | `	if( bNested ){` |
|        5 |   52 | `		PH7_CompilerSaveState(pVm,&sSavedGen,xErr,pErrData);` |
|        3 |   53 | `	}else{` |
|    15577 |   54 | `		PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |   55 | `	}` |
|        - |   56 | `	/* Swap bytecode container */` |
|    15581 |   57 | `	pByteCode = pVm->pByteContainer;` |
|    15581 |   58 | `	pVm->pByteContainer = &aByteCode;` |
|        - |   59 | `	/* Compile the chunk */` |
|    15581 |   60 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|        - |   61 | `	/* Record THIS unit's error count where the nested state restore below cannot` |
|        - |   62 | `	 * wipe it — VmExecDeferredClass reads it to detect a failed re-compile. */` |
|    15581 |   63 | `	pVm->nLastEvalErr = pVm->sCodeGen.nErr;` |
|    23368 |   64 | `	if( pVm->sCodeGen.nErr > 0 ){` |
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
|    15541 |   93 | `		if( pVm->nMagic != PH7_VM_INIT ){` |
|    10395 |   94 | `			SyHashResetLoopCursor(&pVm->hClass);` |
|  3756660 |   95 | `			while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|  3741077 |   96 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|        - |   97 | `				/* Only mount classes that haven't been mounted yet */` |
|  3741077 |   98 | `				if( !pClass->bMounted ){` |
|   353331 |   99 | `					rc = VmMountUserClass(pVm,pClass);` |
|   353331 |  100 | `					if( rc != SXRET_OK ){` |
|        - |  101 | `						/* Mount failure (likely memory error) */` |
|        3 |  102 | `						if( pCtx ){` |
|        3 |  103 | `							ph7_result_bool(pCtx,0);` |
|        1 |  104 | `						}` |
|        3 |  105 | `						goto Cleanup;` |
|        - |  106 | `					}` |
|   176662 |  107 | `				}` |
|        5 |  108 | `			}` |
|     5194 |  109 | `		}` |
|    15539 |  110 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  111 | `			/* Out of memory */` |
|      ! 0 |  112 | `			if( pCtx ){` |
|      ! 0 |  113 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  114 | `			}` |
|      ! 0 |  115 | `			goto Cleanup;` |
|        - |  116 | `		}` |
|    15539 |  117 | `		if( bTrueReturn ){` |
|        - |  118 | `			/* php's include/require answer INT 1 when the file returned nothing` |
|        - |  119 | ``			 * of its own — not `true`. It is the value a script tests, stores`` |
|        - |  120 | ``			 * and compares, and `include $f === true` was false on php and true`` |
|        - |  121 | `			 * here. */` |
|    10169 |  122 | `			PH7_MemObjInitFromInt(pVm,&sResult,1);` |
|     5087 |  123 | `		}else{` |
|        - |  124 | `			/* Assume a null return value */` |
|     5375 |  125 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  126 | `		}` |
|        - |  127 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here` |
|        - |  128 | `		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by` |
|        - |  129 | `		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the` |
|        - |  130 | `		 * native-nesting fatal instead of overflowing the C stack. The PHP` |
|        - |  131 | `		 * call-depth cap is OP_CALL-only (BYTECODE.md stage 5).` |
|        - |  132 | `		 *` |
|        - |  133 | `		 * The nested exec shares the caller's VM frame, so a throw the CALLER's own` |
|        - |  134 | `		 * try catches runs that catch IN PLACE and comes back as a status — which was` |
|        - |  135 | `		 * dropped here, and the statement php abandons then carried on after the catch` |
|        - |  136 | `` 		 * had already run (`try { $r = eval('throw new E;'); echo "x"; } catch …` `` |
|        - |  137 | `		 * printed the catch AND the echo). Same rule and same guard as the match-arm /` |
|        - |  138 | `		 * switch-case / property-default sites: compare the recorded-resume fields` |
|        - |  139 | `		 * against a pre-exec SNAPSHOT, never against 0. */` |
|        - |  140 | `		{` |
|    15539 |  141 | `			const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|    15539 |  142 | `			const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|    15539 |  143 | `			rc = VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    15539 |  144 | `			if( pCtx ){` |
|        - |  145 | `				/* Set the execution result */` |
|    10377 |  146 | `				ph7_result_value(pCtx,&sResult);` |
|     5186 |  147 | `			}` |
|    15539 |  148 | `			PH7_MemObjRelease(&sResult);` |
|    15539 |  149 | `			if( rc != PH7_ABORT && VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){` |
|       18 |  150 | `				rcThrow = PH7_EXCEPTION;` |
|        8 |  151 | `			}` |
|        - |  152 | `		}` |
|        - |  153 | `	}` |
|     7788 |  154 | `Cleanup:` |
|        - |  155 | `	/* Cleanup the mess left behind */` |
|    15581 |  156 | `	pVm->pByteContainer = pByteCode;` |
|    15581 |  157 | `	SySetRelease(&aByteCode);` |
|        - |  158 | `	/* Restore the outer compile's generator state if this was a nested unit. */` |
|    15581 |  159 | `	if( bNested ){` |
|        5 |  160 | `		PH7_CompilerRestoreState(pVm,&sSavedGen);` |
|        2 |  161 | `	}` |
|        - |  162 | `	/* A ParseError raised above must reach the caller so the VM unwinds the rest` |
|        - |  163 | ``	 * of the statement; returning OK left `eval('bad'); echo 'x';` running the`` |
|        - |  164 | `	 * echo even though php had already thrown. */` |
|    15581 |  165 | `	return rcThrow;` |
|        5 |  166 | `}` |
|        - |  167 | `/*` |
|        - |  168 | ` * Execute a deferred class declaration (OP_CLASS_DEFER — see the deferral` |
|        - |  169 | ` * block comment in compile_class.c). At this point the declaration's` |
|        - |  170 | ` * execution order has been honored: any spl_autoload_register() statement` |
|        - |  171 | ` * that precedes it in the program has RUN, so each recorded dependency either` |
|        - |  172 | ` * resolves (possibly by autoload, fired inside PH7_VmExtractClass) or is` |
|        - |  173 | ` * genuinely missing. A missing one is reported through *ppMissing — the` |
|        - |  174 | ` * OP_CLASS_DEFER dispatcher throws php's catchable` |
|        - |  175 | `` * `Class/Interface/Trait "X" not found` Error. Once the dependencies resolve,`` |
|        - |  176 | ` * the captured chunk re-compiles through VmEvalChunk (which also mounts the` |
|        - |  177 | ` * newly installed classes); a compile failure there (e.g. a body syntax error` |
|        - |  178 | ` * whose diagnosis was deferred along with the declaration) has already been` |
|        - |  179 | ` * reported through the engine's error consumer, so it aborts execution.` |
|        - |  180 | ` * The site is idempotent: bDone short-circuits re-execution (a loop around an` |
|        - |  181 | ` * anonymous class instantiates the same installed class, php's` |
|        - |  182 | ` * one-class-per-site semantics).` |
|        - |  183 | ` */` |
|       30 |  184 | `PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing)` |
|        3 |  185 | `{` |
|        - |  186 | `	VmDeferredReq *aReq;` |
|        - |  187 | `	sxu32 n;` |
|       33 |  188 | `	*ppMissing = 0;` |
|       33 |  189 | `	if( pDefer->bDone ){` |
|        5 |  190 | `		return SXRET_OK;` |
|        - |  191 | `	}` |
|       29 |  192 | `	aReq = (VmDeferredReq *)SySetBasePtr(&pDefer->aRequired);` |
|       49 |  193 | `	for( n = 0 ; n < SySetUsed(&pDefer->aRequired) ; ++n ){` |
|       33 |  194 | `		if( PH7_VmExtractClass(&(*pVm),aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|       12 |  195 | `			*ppMissing = &aReq[n];` |
|       12 |  196 | `			return SXRET_OK; /* caller throws */` |
|        - |  197 | `		}` |
|       12 |  198 | `	}` |
|       18 |  199 | `	if( pDefer->sAnonName.nByte > 0 ){` |
|        5 |  200 | `		pVm->sDeferAnonName = pDefer->sAnonName;` |
|        2 |  201 | `	}` |
|       18 |  202 | `	VmEvalChunk(&(*pVm),0,&pDefer->sText,PH7_PHP_ONLY,TRUE);` |
|       18 |  203 | `	pVm->sDeferAnonName.zString = 0;` |
|       18 |  204 | `	pVm->sDeferAnonName.nByte = 0;` |
|       16 |  205 | `	if( pVm->nLastEvalErr > 0` |
|       18 |  206 | `	 \|\| PH7_VmExtractClass(&(*pVm),pDefer->sSelfName.zString,pDefer->sSelfName.nByte,FALSE,0) == 0 ){` |
|        - |  207 | `		/* The re-compile failed — a deferred-along syntax error or a` |
|        - |  208 | `		 * redeclaration fatal. It was already reported through the engine's` |
|        - |  209 | `		 * error consumer; halt like a fatal (php exits 255 for both). */` |
|      ! 0 |  210 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  211 | `		return SXERR_ABORT;` |
|        - |  212 | `	}` |
|       18 |  213 | `	pDefer->bDone = 1;` |
|       18 |  214 | `	return SXRET_OK;` |
|       18 |  215 | `}` |
|        - |  216 | `/*` |
|        - |  217 | ` * value eval(string $code)` |
|        - |  218 | ` *   Evaluate a string as PHP code.` |
|        - |  219 | ` * Parameter` |
|        - |  220 | ` *  code: PHP code to evaluate.` |
|        - |  221 | ` * Return` |
|        - |  222 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  223 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  224 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  225 | ` */` |
|      270 |  226 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  227 | `{` |
|        - |  228 | `	SyString sChunk;    /* Chunk to evaluate */` |
|        - |  229 | `	sxi32 rc;` |
|      275 |  230 | `	if( nArg < 1 ){` |
|        - |  231 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  232 | `		ph7_result_null(pCtx);` |
|      ! 0 |  233 | `		return SXRET_OK;` |
|        - |  234 | `	}` |
|        - |  235 | `	/* Chunk to evaluate. Coerced USER-VISIBLY like every other string argument` |
|        - |  236 | `	 * spelled as a language construct: an object with no __toString() is php's` |
|        - |  237 | `	 * Error, where PHL used to hand the parser the literal "Object" and answer a` |
|        - |  238 | `	 * ParseError. */` |
|        - |  239 | `	{` |
|      275 |  240 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sChunk.zString,(int *)&sChunk.nByte);` |
|      275 |  241 | `		if( rcSv != SXRET_OK ){` |
|        3 |  242 | `			return rcSv;` |
|        - |  243 | `		}` |
|        - |  244 | `	}` |
|      273 |  245 | `	if( sChunk.nByte < 1 ){` |
|        - |  246 | `		/* php: eval('') compiles nothing and yields FALSE (a whitespace-only` |
|        - |  247 | `		 * chunk still compiles, and yields NULL through the normal path). */` |
|        3 |  248 | `		ph7_result_bool(pCtx,0);` |
|        3 |  249 | `		return SXRET_OK;` |
|        - |  250 | `	}` |
|        - |  251 | `	/* Eval the chunk */` |
|      271 |  252 | `	rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|      271 |  253 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  254 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       70 |  255 | `		return PH7_ABORT;` |
|        - |  256 | `	}` |
|        - |  257 | `	/* Propagate a ParseError from a failed compile (php unwinds; PHL used to` |
|        - |  258 | `	 * keep executing the statement that contained the eval). */` |
|      203 |  259 | `	return rc;` |
|      140 |  260 | `}` |
|        - |  261 | `/*` |
|        - |  262 | ` * Check if a file path is already included.` |
|        - |  263 | ` */` |
|    10174 |  264 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        5 |  265 | `{` |
|        - |  266 | `	SyString *aEntries;` |
|        - |  267 | `	sxu32 n;` |
|    10179 |  268 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  269 | `	/* Perform a linear search */` |
| 25419859 |  270 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 25409714 |  271 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  272 | `			/* Already included */` |
|       34 |  273 | `			return TRUE;` |
|        - |  274 | `		}` |
| 12704844 |  275 | `	}` |
|    10149 |  276 | `	return FALSE;` |
|     5092 |  277 | `}` |
|        - |  278 | `/*` |
|        - |  279 | ` * Push a file path in the appropriate VM container.` |
|        - |  280 | ` */` |
|    15320 |  281 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 |  282 | `{` |
|        - |  283 | `	SyString sPath;` |
|        - |  284 | `	char *zDup;` |
|        - |  285 | `	sxi32 rc;` |
|    15325 |  286 | `	if( nLen < 0 ){` |
|     5151 |  287 | `		nLen = SyStrlen(zPath);` |
|     2573 |  288 | `	}` |
|        - |  289 | `	/* Duplicate the file path first */` |
|    15325 |  290 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    15325 |  291 | `	if( zDup == 0 ){` |
|      ! 0 |  292 | `		return SXERR_MEM;` |
|        - |  293 | `	}` |
|        - |  294 | `#ifdef __UNIXES__` |
|        - |  295 | `	/* php records the REALPATH'd absolute path for __FILE__/__DIR__/getFileName` |
|        - |  296 | `	 * and include-once dedup: realpath() resolves '.', '..' and symlinks (e.g.` |
|        - |  297 | `	 * macOS /tmp -> /private/tmp). It needs an existing file, so on failure keep` |
|        - |  298 | `	 * the raw path (eval'd code, php:///data:// wrappers, a missing include that` |
|        - |  299 | `	 * errors elsewhere). */` |
|        - |  300 | `	{` |
|    15320 |  301 | `		char *zReal = realpath(zDup,0); /* POSIX: malloc'd result */` |
|    15320 |  302 | `		if( zReal ){` |
|    15268 |  303 | `			sxu32 nReal = SyStrlen(zReal);` |
|    15268 |  304 | `			char *zRealDup = SyMemBackendStrDup(&pVm->sAllocator,zReal,nReal);` |
|    15268 |  305 | `			free(zReal);` |
|    15268 |  306 | `			if( zRealDup ){` |
|    15268 |  307 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|    15268 |  308 | `				zDup = zRealDup;` |
|    15268 |  309 | `				nLen = (int)nReal;` |
|     7634 |  310 | `			}` |
|     7634 |  311 | `		}` |
|        - |  312 | `	}` |
|        - |  313 | `#endif` |
|        - |  314 | `#ifdef __WINNT__` |
|        - |  315 | `	/* Windows counterpart of the realpath() above: canonicalize to an absolute` |
|        - |  316 | `	 * path (resolves '.'/'..' , '/' -> '\'), case-preserved to match php — NOT` |
|        - |  317 | `	 * lowercased as the legacy PH7 code did. Like the POSIX branch, keep the raw` |
|        - |  318 | `	 * path when it does not name an existing file (the "Command line code" marker,` |
|        - |  319 | `	 * eval'd code, stream wrappers). _fullpath() is lexical, so pair it with a VFS` |
|        - |  320 | `	 * existence check. */` |
|        - |  321 | `	{` |
|        5 |  322 | `		char *zFull = _fullpath(0,zDup,0); /* MSVC CRT: malloc'd absolute path */` |
|        5 |  323 | `		if( zFull ){` |
|        5 |  324 | `			if( pVm->pEngine->pVfs && pVm->pEngine->pVfs->xFileExists && pVm->pEngine->pVfs->xFileExists(zFull) == PH7_OK ){` |
|        5 |  325 | `				sxu32 nFull = SyStrlen(zFull);` |
|        5 |  326 | `				char *zFullDup = SyMemBackendStrDup(&pVm->sAllocator,zFull,nFull);` |
|        5 |  327 | `				if( zFullDup ){` |
|        5 |  328 | `					SyMemBackendFree(&pVm->sAllocator,zDup);` |
|        5 |  329 | `					zDup = zFullDup;` |
|        5 |  330 | `					nLen = (int)nFull;` |
|        - |  331 | `				}` |
|        - |  332 | `			}` |
|        5 |  333 | `			free(zFull);` |
|        - |  334 | `		}` |
|        - |  335 | `	}` |
|        - |  336 | `#endif` |
|        - |  337 | `	/* Install the file path */` |
|    15325 |  338 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    15325 |  339 | `	if( !bMain ){` |
|    10179 |  340 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  341 | `			/* Already included */` |
|       34 |  342 | `			*pNew = 0;` |
|       19 |  343 | `		}else{` |
|        - |  344 | `			/* Insert in the corresponding container */` |
|    10149 |  345 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    10149 |  346 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  347 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  348 | `				return rc;` |
|        - |  349 | `			}` |
|    10149 |  350 | `			*pNew = 1;` |
|        - |  351 | `		}` |
|     5087 |  352 | `	}` |
|    15325 |  353 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    15325 |  354 | `	return SXRET_OK;` |
|     7665 |  355 | `}` |
|        - |  356 | `/*` |
|        - |  357 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  358 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  359 | ` * indicates failure.` |
|        - |  360 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  361 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  362 | ` * operations.` |
|        - |  363 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  364 | ` * this function is a no-op.` |
|        - |  365 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  366 | ` * constructs for more information.` |
|        - |  367 | ` */` |
|    10188 |  368 | `static sxi32 VmExecIncludedFile(` |
|        - |  369 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  370 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  371 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  372 | `	 )` |
|        5 |  373 | `{` |
|        - |  374 | `	sxi32 rc;` |
|        - |  375 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  376 | `	const ph7_io_stream *pStream;` |
|        - |  377 | `	SyBlob sContents;` |
|        - |  378 | `	void *pHandle;` |
|        - |  379 | `	ph7_vm *pVm;` |
|        - |  380 | `	int isNew;` |
|        - |  381 | `	/* Initialize fields */` |
|    10193 |  382 | `	pVm = pCtx->pVm;` |
|    10193 |  383 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|    10193 |  384 | `	isNew = 0;` |
|        - |  385 | `	/* Extract the associated stream */` |
|    10193 |  386 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  387 | `	/*` |
|        - |  388 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  389 | `	 * in a read-only mode.` |
|        - |  390 | `	 */` |
|    10193 |  391 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew,ph7_function_name(pCtx));` |
|    10193 |  392 | `	if( pHandle == 0 ){` |
|       18 |  393 | `		return SXERR_IO;` |
|        - |  394 | `	}` |
|    10179 |  395 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|    10179 |  396 | `	if( IncludeOnce && !isNew ){` |
|        - |  397 | `		/* Already included */` |
|       28 |  398 | `		rc = SXERR_EXISTS;` |
|       16 |  399 | `	}else{` |
|        - |  400 | `		/* Read the whole file contents */` |
|    10155 |  401 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|    10155 |  402 | `		if( rc == SXRET_OK ){` |
|        - |  403 | `			SyString sScript;` |
|        - |  404 | `			/* Compile and execute the script. A throw the included file raised — and the` |
|        - |  405 | `			 * INCLUDING statement's own try caught in place — comes back as PH7_EXCEPTION` |
|        - |  406 | `			 * and travels out to the include builtin's caller, which unwinds the rest of` |
|        - |  407 | `			 * the statement instead of resuming it. It is not an IO failure, so the` |
|        - |  408 | `			 * callers must tell the two apart before warning. */` |
|    10153 |  409 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|    10153 |  410 | `			rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|    10153 |  411 | `			if( rc != PH7_EXCEPTION ){` |
|    10145 |  412 | `				rc = SXRET_OK;` |
|     5070 |  413 | `			}` |
|     5074 |  414 | `		}` |
|        - |  415 | `	}` |
|        - |  416 | `	/* Pop from the set of included file */` |
|    10179 |  417 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  418 | `	/* Close the handle */` |
|    10179 |  419 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  420 | `	/* Release the working buffer */` |
|    10179 |  421 | `	SyBlobRelease(&sContents);` |
|        - |  422 | `#else` |
|        - |  423 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  424 | `	SXUNUSED(pPath);` |
|        - |  425 | `	SXUNUSED(IncludeOnce);` |
|        - |  426 | `	rc = SXERR_IO;` |
|        - |  427 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    10179 |  428 | `	return rc;` |
|     5099 |  429 | `}` |
|        - |  430 | `/*` |
|        - |  431 | ` * php keeps include_path in ONE place -- the INI table -- and get_include_path(),` |
|        - |  432 | ` * ini_get('include_path'), ini_get_all() and the resolver all read that one string.` |
|        - |  433 | ` * PHL had TWO: pVm->aPaths, which is what the include walk actually uses and which` |
|        - |  434 | `` * only set_include_path() ever wrote, and the `include_path` INI slot, which is what`` |
|        - |  435 | ` * ini_get() answers and which only ini_set()/-d ever wrote. Neither told the other,` |
|        - |  436 | `` * so `ini_set('include_path', $dir)` -- the ordinary way a bootstrap file points the`` |
|        - |  437 | `` * engine at a library -- moved NOTHING, and `set_include_path($dir)` left ini_get()`` |
|        - |  438 | ` * naming the value that was no longer in force. The set below is the single store;` |
|        - |  439 | ` * the INI slot is a view of it (vm_builtin_ini.c).` |
|        - |  440 | ` */` |
|       74 |  441 | `PH7_PRIVATE int PH7_VmIncludePathSep(void)` |
|        4 |  442 | `{` |
|        - |  443 | `#ifdef __WINNT__` |
|        4 |  444 | `	return ';';` |
|        - |  445 | `#else` |
|        - |  446 | `	/* Assume UNIX path separator */` |
|       74 |  447 | `	return ':';` |
|        - |  448 | `#endif` |
|        4 |  449 | `}` |
|        - |  450 | `/*` |
|        - |  451 | ` * Split an include_path STRING into its segments and make them the VM's set.` |
|        - |  452 | ` *` |
|        - |  453 | ` * Segments are kept VERBATIM. php hands back the string it was given, so a` |
|        - |  454 | ` * trailing slash, a leading space and an EMPTY segment all survive the round trip` |
|        - |  455 | ` * -- and the walk means something for each of them: php builds "<segment>/<file>"` |
|        - |  456 | ` * from every entry, which is how an empty segment comes to try "/file".` |
|        - |  457 | ` */` |
|       24 |  458 | `PH7_PRIVATE void PH7_VmSetIncludePath(ph7_vm *pVm,const char *zPath,sxu32 nByte)` |
|        3 |  459 | `{` |
|        - |  460 | `	const char *z,*zEnd,*zDup;` |
|       27 |  461 | `	int dir_sep = PH7_VmIncludePathSep();` |
|       27 |  462 | `	SySetReset(&pVm->aPaths);` |
|       27 |  463 | `	if( nByte < 1 ){` |
|      ! 0 |  464 | `		return;` |
|        - |  465 | `	}` |
|        - |  466 | `	/* ONE VM-lifetime copy backs every segment (the SyString entries alias it),` |
|        - |  467 | `	 * where the old splitter duped each segment separately on every call. */` |
|       27 |  468 | `	zDup = (const char *)SyMemBackendDup(&pVm->sAllocator,zPath,nByte);` |
|       27 |  469 | `	if( zDup == 0 ){` |
|      ! 0 |  470 | `		return;` |
|        - |  471 | `	}` |
|       27 |  472 | `	z = zDup;` |
|       27 |  473 | `	zEnd = &zDup[nByte];` |
|       20 |  474 | `	for(;;){` |
|       35 |  475 | `		const char *zStart = z;` |
|        - |  476 | `		SyString sPath;` |
|      338 |  477 | `		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }` |
|       35 |  478 | `		SyStringInitFromBuf(&sPath,zStart,(sxu32)(z-zStart));` |
|       35 |  479 | `		SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|       35 |  480 | `		if( z >= zEnd ){` |
|       27 |  481 | `			break;` |
|        - |  482 | `		}` |
|       10 |  483 | `		z++; /* skip the separator */` |
|        2 |  484 | `	}` |
|       15 |  485 | `}` |
|        - |  486 | `/*` |
|        - |  487 | ` * php's LAST RESORT for a relative name the include_path did not answer: the` |
|        - |  488 | ` * directory of the file that is EXECUTING, not the process's cwd. It is why` |
|        - |  489 | `` * `include 'helper.php'` next to the script keeps working when the script was`` |
|        - |  490 | ` * started from somewhere else, and why a library's own relative includes` |
|        - |  491 | ` * resolve at all. PHL had nothing of the kind, so` |
|        - |  492 | ` *` |
|        - |  493 | ` *   cd / && phl /srv/app/main.php   with   include 'lib.php'   next to main.php` |
|        - |  494 | ` *` |
|        - |  495 | ` * failed on this engine and ran on php. aFiles is the include-nesting stack,` |
|        - |  496 | ` * so its top is the innermost executing file -- which is what php uses too.` |
|        - |  497 | ` *` |
|        - |  498 | `` * Answers 0 when that name has no directory part at all (`-r`'s "Command line`` |
|        - |  499 | ` * code"). php's own arithmetic degenerates to the bare, cwd-relative name` |
|        - |  500 | ` * there, which the default include_path's "." entry already tries.` |
|        - |  501 | ` */` |
|       20 |  502 | `PH7_PRIVATE int PH7_VmExecutingDir(ph7_vm *pVm,SyString *pOut)` |
|        3 |  503 | `{` |
|       23 |  504 | `	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  505 | `	sxu32 n;` |
|       23 |  506 | `	if( pFile == 0 \|\| pFile->nByte < 2 ){` |
|      ! 0 |  507 | `		return 0;` |
|        - |  508 | `	}` |
|       23 |  509 | `	n = pFile->nByte;` |
|      455 |  510 | `	while( n > 0 ){` |
|      455 |  511 | `		int c = pFile->zString[n-1];` |
|      452 |  512 | `		if( c == '/'` |
|        - |  513 | `#ifdef __WINNT__` |
|        3 |  514 | `		 \|\| c == '\\'` |
|        - |  515 | `#endif` |
|        - |  516 | `		){` |
|       23 |  517 | `			break;` |
|        - |  518 | `		}` |
|      435 |  519 | `		n--;` |
|        3 |  520 | `	}` |
|       23 |  521 | `	if( n < 2 ){` |
|        - |  522 | `		/* No separator, or one sitting at index 0 -- php skips the fallback for` |
|        - |  523 | `		 * a root-level script exactly the same way. */` |
|      ! 0 |  524 | `		return 0;` |
|        - |  525 | `	}` |
|       23 |  526 | `	SyStringInitFromBuf(pOut,pFile->zString,n-1); /* without the separator */` |
|       23 |  527 | `	return 1;` |
|       13 |  528 | `}` |
|        - |  529 | `/*` |
|        - |  530 | ` * Join the set back into php's one string. Splitting on the separator and` |
|        - |  531 | ` * joining with it round-trips exactly, which is what ini_get() promises.` |
|        - |  532 | ` */` |
|       50 |  533 | `PH7_PRIVATE void PH7_VmGetIncludePath(ph7_vm *pVm,SyBlob *pOut)` |
|        4 |  534 | `{` |
|       54 |  535 | `	SyString *aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       54 |  536 | `	char cSep = (char)PH7_VmIncludePathSep();` |
|        - |  537 | `	sxu32 n;` |
|      120 |  538 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|       70 |  539 | `		if( n > 0 ){` |
|       17 |  540 | `			SyBlobAppend(pOut,(const void *)&cSep,sizeof(char));` |
|        8 |  541 | `		}` |
|       70 |  542 | `		SyBlobAppend(pOut,aEntry[n].zString,aEntry[n].nByte);` |
|       37 |  543 | `	}` |
|       54 |  544 | `}` |
|        - |  545 | `/*` |
|        - |  546 | ` * string get_include_path(void)` |
|        - |  547 | ` *  Gets the current include_path configuration option.` |
|        - |  548 | ` * Parameter` |
|        - |  549 | ` *  None` |
|        - |  550 | ` * Return` |
|        - |  551 | ` *  Included paths as a string` |
|        - |  552 | ` */` |
|       20 |  553 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  554 | `{` |
|        - |  555 | `	SyBlob sOut;` |
|       10 |  556 | `	SXUNUSED(nArg); /* cc warning */` |
|       10 |  557 | `	SXUNUSED(apArg);` |
|       23 |  558 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|       23 |  559 | `	PH7_VmGetIncludePath(pCtx->pVm,&sOut);` |
|        - |  560 | `	/* php's answer is always a STRING: an empty set is "", never NULL, which is` |
|        - |  561 | `	 * what this used to leave behind when nothing had ever been appended. */` |
|       23 |  562 | `	if( SyBlobLength(&sOut) < 1 ){` |
|      ! 0 |  563 | `		ph7_result_string(pCtx,"",0);` |
|      ! 0 |  564 | `	}else{` |
|       23 |  565 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|        - |  566 | `	}` |
|       23 |  567 | `	SyBlobRelease(&sOut);` |
|       23 |  568 | `	return PH7_OK;` |
|        3 |  569 | `}` |
|        - |  570 | `/*` |
|        - |  571 | ` * string\|false set_include_path(string $include_path)` |
|        - |  572 | ` *  Sets the include_path configuration option for the duration of the script,` |
|        - |  573 | ` *  returning the OLD value (php contract).` |
|        - |  574 | ` */` |
|       14 |  575 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  576 | `{` |
|       17 |  577 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  578 | `	const char *zNew;` |
|       17 |  579 | `	int nLen = 0;` |
|        - |  580 | `	SyBlob sOld;` |
|       17 |  581 | `	if( nArg < 1 ){` |
|      ! 0 |  582 | `		return PH7_OK;` |
|        - |  583 | `	}` |
|        - |  584 | `	/* The OLD value php answers is the effective one, read before the write. */` |
|       17 |  585 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|       17 |  586 | `	PH7_VmGetIncludePath(pVm,&sOld);` |
|       17 |  587 | `	zNew = ph7_value_to_string(apArg[0],&nLen);` |
|       17 |  588 | `	if( nLen < 1 ){` |
|        - |  589 | `		/* php registers include_path with OnUpdateStringUnempty, so the EMPTY` |
|        - |  590 | `		 * string is refused outright: the directive keeps the value it had and` |
|        - |  591 | `		 * the call answers FALSE. PHL used to accept it, wiping the set and` |
|        - |  592 | `		 * leaving get_include_path() with nothing to answer. */` |
|        3 |  593 | `		SyBlobRelease(&sOld);` |
|        3 |  594 | `		ph7_result_bool(pCtx,0);` |
|        3 |  595 | `		return PH7_OK;` |
|        - |  596 | `	}` |
|       15 |  597 | `	PH7_VmSetIncludePath(pVm,zNew,(sxu32)nLen);` |
|       15 |  598 | `	if( SyBlobLength(&sOld) < 1 ){` |
|      ! 0 |  599 | `		ph7_result_string(pCtx,"",0);` |
|      ! 0 |  600 | `	}else{` |
|       15 |  601 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|        - |  602 | `	}` |
|       15 |  603 | `	SyBlobRelease(&sOld);` |
|       15 |  604 | `	return PH7_OK;` |
|       10 |  605 | `}` |
|        - |  606 | `/*` |
|        - |  607 | ` * string get_get_included_files(void)` |
|        - |  608 | ` *  Gets the current include_path configuration option.` |
|        - |  609 | ` * Parameter` |
|        - |  610 | ` *  None` |
|        - |  611 | ` * Return` |
|        - |  612 | ` *  Included paths as a string` |
|        - |  613 | ` */` |
|        2 |  614 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  615 | `{` |
|        3 |  616 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  617 | `	ph7_value *pArray,*pWorker;` |
|        - |  618 | `	SyString *pEntry;` |
|        - |  619 | `	int c,d;` |
|        - |  620 | `	/* Create an array and a working value */` |
|        3 |  621 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  622 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  623 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  624 | `		/* Out of memory,return null */` |
|      ! 0 |  625 | `		ph7_result_null(pCtx);` |
|      ! 0 |  626 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  627 | `		SXUNUSED(apArg);` |
|      ! 0 |  628 | `		return PH7_OK;` |
|        - |  629 | `	}` |
|        3 |  630 | `	c = d = '/';` |
|        - |  631 | `#ifdef __WINNT__` |
|        1 |  632 | `	d = '\\';` |
|        - |  633 | `#endif` |
|        - |  634 | `	/* Iterate throw entries */` |
|        3 |  635 | `	SySetResetCursor(pFiles);` |
|        7 |  636 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  637 | `		const char *zBase,*zEnd;` |
|        - |  638 | `		int iLen;` |
|        - |  639 | `		/* reset the string cursor */` |
|        5 |  640 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  641 | `		/* Extract base name */` |
|        5 |  642 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  643 | `		/* Ignore trailing '/' */` |
|        7 |  644 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  645 | `			zEnd--;` |
|      ! 0 |  646 | `		}` |
|        5 |  647 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|       79 |  648 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|       73 |  649 | `			zEnd--;` |
|        1 |  650 | `		}` |
|        5 |  651 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|        5 |  652 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  653 | `		/* Copy entry name */` |
|        5 |  654 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  655 | `		/* Perform the insertion */` |
|        5 |  656 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  657 | `	}` |
|        - |  658 | `	/* All done,return the created array */` |
|        3 |  659 | `	ph7_result_value(pCtx,pArray);` |
|        - |  660 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  661 | `	 * by the engine as soon we return from this foreign` |
|        - |  662 | `	 * function.` |
|        - |  663 | `	 */` |
|        3 |  664 | `	return PH7_OK;` |
|        2 |  665 | `}` |
|        - |  666 | `/*` |
|        - |  667 | ` * include:` |
|        - |  668 | ` * According to the PHP reference manual.` |
|        - |  669 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  670 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  671 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  672 | ` *  include() will finally check in the calling script's own directory` |
|        - |  673 | ` *  and the current working directory before failing. The include()` |
|        - |  674 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  675 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  676 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  677 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  678 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  679 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  680 | ` *  directory to find the requested file.` |
|        - |  681 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  682 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  683 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  684 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  685 | ` */` |
|    10094 |  686 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  687 | `{` |
|        - |  688 | `	SyString sFile;` |
|        - |  689 | `	sxi32 rc;` |
|    10099 |  690 | `	if( nArg < 1 ){` |
|        - |  691 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  692 | `		ph7_result_null(pCtx);` |
|      ! 0 |  693 | `		return SXRET_OK;` |
|        - |  694 | `	}` |
|        - |  695 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  696 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  697 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  698 | `	{` |
|    10099 |  699 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|    10099 |  700 | `		if( rcSv != SXRET_OK ){` |
|        3 |  701 | `			return rcSv;` |
|        - |  702 | `		}` |
|        - |  703 | `	}` |
|    10097 |  704 | `	if( sFile.nByte < 1 ){` |
|        - |  705 | `		/* Empty string,return NULL */` |
|      ! 0 |  706 | `		ph7_result_null(pCtx);` |
|      ! 0 |  707 | `		return SXRET_OK;` |
|        - |  708 | `	}` |
|        - |  709 | `	/* Open,compile and execute the desired script */` |
|    10097 |  710 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|    10097 |  711 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - |  712 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - |  713 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - |  714 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - |  715 | `		 * wins, so it is tested first. */` |
|        3 |  716 | `		return rc;` |
|        - |  717 | `	}` |
|    10095 |  718 | `	if( rc != SXRET_OK ){` |
|        - |  719 | `		/* Emit a warning and return false */` |
|       16 |  720 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|       16 |  721 | `		ph7_result_bool(pCtx,0);` |
|        6 |  722 | `	}` |
|    10095 |  723 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  724 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 |  725 | `		return PH7_ABORT;` |
|        - |  726 | `	}` |
|    10091 |  727 | `	return SXRET_OK;` |
|     5052 |  728 | `}` |
|        - |  729 | `/*` |
|        - |  730 | ` * include_once:` |
|        - |  731 | ` *  According to the PHP reference manual.` |
|        - |  732 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  733 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  734 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  735 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  736 | ` *   just once.` |
|        - |  737 | ` */` |
|       28 |  738 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  739 | `{` |
|        - |  740 | `	SyString sFile;` |
|        - |  741 | `	sxi32 rc;` |
|       32 |  742 | `	if( nArg < 1 ){` |
|        - |  743 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  744 | `		ph7_result_null(pCtx);` |
|      ! 0 |  745 | `		return SXRET_OK;` |
|        - |  746 | `	}` |
|        - |  747 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  748 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  749 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  750 | `	{` |
|       32 |  751 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       32 |  752 | `		if( rcSv != SXRET_OK ){` |
|        3 |  753 | `			return rcSv;` |
|        - |  754 | `		}` |
|        - |  755 | `	}` |
|       30 |  756 | `	if( sFile.nByte < 1 ){` |
|        - |  757 | `		/* Empty string,return NULL */` |
|      ! 0 |  758 | `		ph7_result_null(pCtx);` |
|      ! 0 |  759 | `		return SXRET_OK;` |
|        - |  760 | `	}` |
|        - |  761 | `	/* Open,compile and execute the desired script */` |
|       30 |  762 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       30 |  763 | `	if( rc == SXERR_EXISTS ){` |
|        - |  764 | `		/* File already included,return TRUE */` |
|       19 |  765 | `		ph7_result_bool(pCtx,1);` |
|       19 |  766 | `		return SXRET_OK;` |
|        - |  767 | `	}` |
|       14 |  768 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - |  769 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - |  770 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - |  771 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - |  772 | `		 * wins, so it is tested first. */` |
|        3 |  773 | `		return rc;` |
|        - |  774 | `	}` |
|       11 |  775 | `	if( rc != SXRET_OK ){` |
|        - |  776 | `		/* Emit a warning and return false */` |
|      ! 0 |  777 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  778 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  779 | ` 	}` |
|       11 |  780 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  781 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  782 | `		return PH7_ABORT;` |
|        - |  783 | `	}` |
|       11 |  784 | `	return SXRET_OK;` |
|       18 |  785 | `}` |
|        - |  786 | `/*` |
|        - |  787 | ` * require.` |
|        - |  788 | ` *  According to the PHP reference manual.` |
|        - |  789 | ` *   require() is identical to include() except upon failure it will` |
|        - |  790 | ` *   also produce a fatal level error.` |
|        - |  791 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  792 | ` *   emits a warning  which allows the script to continue.` |
|        - |  793 | ` */` |
|       54 |  794 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  795 | `{` |
|        - |  796 | `	SyString sFile;` |
|        - |  797 | `	sxi32 rc;` |
|       59 |  798 | `	if( nArg < 1 ){` |
|        - |  799 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  800 | `		ph7_result_null(pCtx);` |
|      ! 0 |  801 | `		return SXRET_OK;` |
|        - |  802 | `	}` |
|        - |  803 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  804 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  805 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  806 | `	{` |
|       59 |  807 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       59 |  808 | `		if( rcSv != SXRET_OK ){` |
|        3 |  809 | `			return rcSv;` |
|        - |  810 | `		}` |
|        - |  811 | `	}` |
|       56 |  812 | `	if( sFile.nByte < 1 ){` |
|        - |  813 | `		/* Empty string,return NULL */` |
|      ! 0 |  814 | `		ph7_result_null(pCtx);` |
|      ! 0 |  815 | `		return SXRET_OK;` |
|        - |  816 | `	}` |
|        - |  817 | `	/* Open,compile and execute the desired script */` |
|       56 |  818 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|       56 |  819 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - |  820 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - |  821 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - |  822 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - |  823 | `		 * wins, so it is tested first. */` |
|        3 |  824 | `		return rc;` |
|        - |  825 | `	}` |
|       53 |  826 | `	if( rc != SXRET_OK ){` |
|        - |  827 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  828 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  829 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  830 | `		return PH7_ABORT;` |
|        - |  831 | `	}` |
|       53 |  832 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  833 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  834 | `		return PH7_ABORT;` |
|        - |  835 | `	}` |
|       53 |  836 | `	return SXRET_OK;` |
|       32 |  837 | `}` |
|        - |  838 | `/*` |
|        - |  839 | ` * require_once:` |
|        - |  840 | ` *  According to the PHP reference manual.` |
|        - |  841 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  842 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  843 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  844 | ` *   and how it differs from its non _once siblings.` |
|        - |  845 | ` */` |
|       16 |  846 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  847 | `{` |
|        - |  848 | `	SyString sFile;` |
|        - |  849 | `	sxi32 rc;` |
|       20 |  850 | `	if( nArg < 1 ){` |
|        - |  851 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  852 | `		ph7_result_null(pCtx);` |
|      ! 0 |  853 | `		return SXRET_OK;` |
|        - |  854 | `	}` |
|        - |  855 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  856 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  857 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  858 | `	{` |
|       20 |  859 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       20 |  860 | `		if( rcSv != SXRET_OK ){` |
|        3 |  861 | `			return rcSv;` |
|        - |  862 | `		}` |
|        - |  863 | `	}` |
|       17 |  864 | `	if( sFile.nByte < 1 ){` |
|        - |  865 | `		/* Empty string,return NULL */` |
|      ! 0 |  866 | `		ph7_result_null(pCtx);` |
|      ! 0 |  867 | `		return SXRET_OK;` |
|        - |  868 | `	}` |
|        - |  869 | `	/* Open,compile and execute the desired script */` |
|       17 |  870 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       17 |  871 | `	if( rc == SXERR_EXISTS ){` |
|        - |  872 | `		/* File already included,return TRUE */` |
|       10 |  873 | `		ph7_result_bool(pCtx,1);` |
|       10 |  874 | `		return SXRET_OK;` |
|        - |  875 | `	}` |
|        9 |  876 | `	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){` |
|        - |  877 | `		/* The included file THREW and the including statement's own try caught it in` |
|        - |  878 | `		 * place: unwind the rest of that statement instead of resuming it, and say` |
|        - |  879 | `		 * nothing — this is not an IO failure. A halt (exit/die in the file) still` |
|        - |  880 | `		 * wins, so it is tested first. */` |
|        3 |  881 | `		return rc;` |
|        - |  882 | `	}` |
|        6 |  883 | `	if( rc != SXRET_OK ){` |
|        - |  884 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  885 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  886 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  887 | `		return PH7_ABORT;` |
|        - |  888 | `	}` |
|        6 |  889 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  890 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  891 | `		return PH7_ABORT;` |
|        - |  892 | `	}` |
|        6 |  893 | `	return SXRET_OK;` |
|       12 |  894 | `}` |
|        - |  895 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  896 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  897 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  898 | `/*` |
|        - |  899 | ` * Section:` |
|        - |  900 | ` *  SPL Autoloading functions.` |
|        - |  901 | ` * Status:` |
|        - |  902 | ` *  Stable.` |
|        - |  903 | ` */` |
|        - |  904 | `/*` |
|        - |  905 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - |  906 | ` *  Register given function as __autoload() implementation.` |
|        - |  907 | ` * Parameters` |
|        - |  908 | ` *  callback` |
|        - |  909 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - |  910 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - |  911 | ` *  throw` |
|        - |  912 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - |  913 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - |  914 | ` *  prepend` |
|        - |  915 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - |  916 | ` *   autoload stack instead of appending it.` |
|        - |  917 | ` * Return` |
|        - |  918 | ` *  TRUE on success, FALSE on failure.` |
|        - |  919 | ` */` |
|       58 |  920 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  921 | `{` |
|        - |  922 | `	VmAutoloadCB sEntry;` |
|       63 |  923 | `	ph7_vm *pVm = pCtx->pVm;` |
|       63 |  924 | `	int iPrepend = 0;` |
|        - |  925 | `	sxu32 n;` |
|       63 |  926 | `	if( nArg < 1 ){` |
|        - |  927 | `		/* No callback provided — register default spl_autoload.` |
|        - |  928 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - |  929 | `		/* Check for duplicates first */` |
|        9 |  930 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 |  931 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        4 |  932 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 |  933 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 |  934 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 |  935 | `				ph7_result_bool(pCtx,1);` |
|        5 |  936 | `				return SXRET_OK;` |
|        - |  937 | `			}` |
|      ! 0 |  938 | `		}` |
|        5 |  939 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 |  940 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 |  941 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 |  942 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 |  943 | `		ph7_result_bool(pCtx,1);` |
|        5 |  944 | `		return SXRET_OK;` |
|        - |  945 | `	}` |
|        - |  946 | `	/* Validate that the callback is callable */` |
|       55 |  947 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 |  948 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 |  949 | `		if( nArg >= 2 ){` |
|      ! 0 |  950 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  951 | `		}` |
|      ! 0 |  952 | `		if( iThrow ){` |
|      ! 0 |  953 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - |  954 | `				"Argument is not callable");` |
|      ! 0 |  955 | `		}` |
|      ! 0 |  956 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  957 | `		return SXRET_OK;` |
|        - |  958 | `	}` |
|        - |  959 | `	/* Check for duplicates */` |
|       73 |  960 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 |  961 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 |  962 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  963 | `			/* Already registered */` |
|      ! 0 |  964 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  965 | `			return SXRET_OK;` |
|        - |  966 | `		}` |
|       11 |  967 | `	}` |
|        - |  968 | `	/* Check prepend flag */` |
|       55 |  969 | `	if( nArg >= 3 ){` |
|        3 |  970 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 |  971 | `	}` |
|        - |  972 | `	/* Store the callback */` |
|       55 |  973 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       55 |  974 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       55 |  975 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       56 |  976 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - |  977 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - |  978 | `		 * We do this by appending first, then rotating the array. */` |
|        3 |  979 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - |  980 | `		VmAutoloadCB *aBase;` |
|        3 |  981 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  982 | `		/* Rotate: move last entry to front */` |
|        3 |  983 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 |  984 | `		if( aBase ){` |
|        - |  985 | `			VmAutoloadCB sTemp;` |
|        - |  986 | `			sxu32 i;` |
|        3 |  987 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 |  988 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 |  989 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 |  990 | `			}` |
|        3 |  991 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 |  992 | `		}` |
|        2 |  993 | `	}else{` |
|       53 |  994 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  995 | `	}` |
|       55 |  996 | `	ph7_result_bool(pCtx,1);` |
|       55 |  997 | `	return SXRET_OK;` |
|       34 |  998 | `}` |
|        - |  999 | `/*` |
|        - | 1000 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 1001 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 1002 | ` * Parameters` |
|        - | 1003 | ` *  callback` |
|        - | 1004 | ` *   The autoload function being unregistered.` |
|        - | 1005 | ` * Return` |
|        - | 1006 | ` *  TRUE on success, FALSE on failure.` |
|        - | 1007 | ` */` |
|       36 | 1008 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1009 | `{` |
|       41 | 1010 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1011 | `	sxu32 n,nEntry;` |
|       41 | 1012 | `	if( nArg < 1 ){` |
|      ! 0 | 1013 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1014 | `		return SXRET_OK;` |
|        - | 1015 | `	}` |
|       41 | 1016 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       45 | 1017 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       43 | 1018 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       43 | 1019 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 1020 | `			/* Found — remove by shifting remaining entries down */` |
|       39 | 1021 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 1022 | `			sxu32 i;` |
|       39 | 1023 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       53 | 1024 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 1025 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 1026 | `			}` |
|        - | 1027 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       39 | 1028 | `			SySetPop(&pVm->aAutoload);` |
|       39 | 1029 | `			ph7_result_bool(pCtx,1);` |
|       39 | 1030 | `			return SXRET_OK;` |
|        - | 1031 | `		}` |
|        3 | 1032 | `	}` |
|        3 | 1033 | `	ph7_result_bool(pCtx,0);` |
|        3 | 1034 | `	return SXRET_OK;` |
|       23 | 1035 | `}` |
|        - | 1036 | `/*` |
|        - | 1037 | ` * array spl_autoload_functions(void)` |
|        - | 1038 | ` *  Return all registered __autoload() functions.` |
|        - | 1039 | ` * Return` |
|        - | 1040 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 1041 | ` *  an empty array is returned.` |
|        - | 1042 | ` */` |
|       20 | 1043 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1044 | `{` |
|       21 | 1045 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1046 | `	ph7_value *pArray;` |
|        - | 1047 | `	sxu32 n,nEntry;` |
|       10 | 1048 | `	SXUNUSED(nArg);` |
|       10 | 1049 | `	SXUNUSED(apArg);` |
|       21 | 1050 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 1051 | `	if( pArray == 0 ){` |
|      ! 0 | 1052 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1053 | `		return SXRET_OK;` |
|        - | 1054 | `	}` |
|       21 | 1055 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 1056 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 1057 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 1058 | `		if( pEntry ){` |
|       15 | 1059 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 1060 | `		}` |
|        8 | 1061 | `	}` |
|       21 | 1062 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 1063 | `	return SXRET_OK;` |
|       11 | 1064 | `}` |
|        - | 1065 | `/*` |
|        - | 1066 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 1067 | ` *  Default implementation of __autoload().` |
|        - | 1068 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 1069 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 1070 | ` * Parameters` |
|        - | 1071 | ` *  class` |
|        - | 1072 | ` *   The class name being searched.` |
|        - | 1073 | ` *  file_extensions` |
|        - | 1074 | ` *   Comma-separated list of file extensions to try.` |
|        - | 1075 | ` */` |
|        2 | 1076 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1077 | `{` |
|        - | 1078 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 1079 | `	SyBlob sPath;` |
|        - | 1080 | `	int nClass;` |
|        - | 1081 | `	sxi32 rc;` |
|        3 | 1082 | `	if( nArg < 1 ){` |
|      ! 0 | 1083 | `		return SXRET_OK;` |
|        - | 1084 | `	}` |
|        3 | 1085 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 1086 | `	if( nClass < 1 ){` |
|      ! 0 | 1087 | `		return SXRET_OK;` |
|        - | 1088 | `	}` |
|        - | 1089 | `	/* Default extensions */` |
|        3 | 1090 | `	zExt = ".php,.inc";` |
|        3 | 1091 | `	if( nArg >= 2 ){` |
|        - | 1092 | `		int nExt;` |
|      ! 0 | 1093 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 1094 | `		if( nExt < 1 ){` |
|      ! 0 | 1095 | `			zExt = ".php,.inc";` |
|      ! 0 | 1096 | `		}` |
|      ! 0 | 1097 | `	}` |
|        3 | 1098 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 1099 | `	/* Iterate over comma-separated extensions */` |
|        3 | 1100 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 1101 | `	zCur = zExt;` |
|        7 | 1102 | `	while( zCur < zEnd ){` |
|        - | 1103 | `		const char *zComma;` |
|        - | 1104 | `		SyString sFile;` |
|        - | 1105 | `		int i;` |
|        - | 1106 | `		/* Find next comma or end */` |
|        5 | 1107 | `		zComma = zCur;` |
|       21 | 1108 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 1109 | `			zComma++;` |
|        1 | 1110 | `		}` |
|        - | 1111 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 1112 | `		SyBlobReset(&sPath);` |
|       69 | 1113 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 1114 | `			char c = zClass[i];` |
|       65 | 1115 | `			if( c == '\\' ){` |
|      ! 0 | 1116 | `				c = '/';` |
|       65 | 1117 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 1118 | `				c = c + ('a' - 'A');` |
|        6 | 1119 | `			}` |
|       65 | 1120 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 1121 | `		}` |
|        - | 1122 | `		/* Append extension */` |
|        5 | 1123 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 1124 | `		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,` |
|        - | 1125 | `		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a` |
|        - | 1126 | `		 * terminator, so without this the strlen reads past the buffer (a` |
|        - | 1127 | `		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL` |
|        - | 1128 | `		 * is not counted in SyBlobLength(), so the SyString length stays correct. */` |
|        5 | 1129 | `		SyBlobNullAppend(&sPath);` |
|        - | 1130 | `		/* Try to include the file */` |
|        5 | 1131 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 1132 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 1133 | `		if( rc == SXRET_OK \|\| rc == PH7_EXCEPTION ){` |
|        - | 1134 | `			/* Included — or it threw, which ends the search too: the remaining` |
|        - | 1135 | `			 * extensions are not tried after a file has already run. */` |
|      ! 0 | 1136 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 1137 | `			return rc == PH7_EXCEPTION ? rc : SXRET_OK;` |
|        - | 1138 | `		}` |
|        - | 1139 | `		/* Move past the comma */` |
|        5 | 1140 | `		zCur = zComma;` |
|        5 | 1141 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 1142 | `			zCur++;` |
|        1 | 1143 | `		}` |
|        1 | 1144 | `	}` |
|        3 | 1145 | `	SyBlobRelease(&sPath);` |
|        3 | 1146 | `	return SXRET_OK;` |
|        2 | 1147 | `}` |
|        - | 1148 | `/* Table of built-in VM functions. */` |
|        - | 1149 | `/*` |
|        - | 1150 | ` * Packing body for __call / __callStatic (band A #3b).` |
|        - | 1151 | ` * OP_MEMBER, on a missing or inaccessible method whose class declares the magic` |
|        - | 1152 | ` * handler, stashes {receiver, class, original name} on the VM and marks the callee` |
|        - | 1153 | ` * slot MEMOBJ_AUX_MAGICCALL; the normal OP_CALL machinery then collects the` |
|        - | 1154 | ` * ORIGINAL argument list (incl. spreads) and hands it here, which packs it into a` |
|        - | 1155 | ` * php array and invokes` |
|        - | 1156 | ` *   $recv->__call($name, $args)   /   Class::__callStatic($name, $args)` |
|        - | 1157 | ` * returning the handler's value as the call's result. A throw propagates via the` |
|        - | 1158 | ` * returned status (and the boundary rail).` |
|        - | 1159 | ` *` |
|        - | 1160 | `` * This used to be a REGISTERED host function named `__phl_magic_call` whose name`` |
|        - | 1161 | ` * the four OP_MEMBER sites wrote into the callee slot — so the engine's own` |
|        - | 1162 | ` * dispatch was spelled as a global PHP function that function_exists() and` |
|        - | 1163 | ` * get_defined_functions() both reported, and that any script could call. It is` |
|        - | 1164 | ` * reached through the VM's own function record now (PH7_VmMagicCallFunc); the` |
|        - | 1165 | ` * mark on the slot is the only thing that selects it, and there is no name.` |
|        - | 1166 | ` */` |
|      126 | 1167 | `static int VmMagicCallDispatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 1168 | `{` |
|      130 | 1169 | `	ph7_vm *pVm = pCtx->pVm;` |
|      130 | 1170 | `	ph7_class_instance *pRecv = pVm->pMagicCallThis;` |
|      130 | 1171 | `	ph7_class *pClass = pVm->pMagicCallClass;` |
|        - | 1172 | `	ph7_value sResult;` |
|        - | 1173 | `	SyString sMethName;` |
|        - | 1174 | `	sxi32 rc;` |
|        - | 1175 | `	/* Consume the pending dispatch (one-shot) */` |
|      130 | 1176 | `	pVm->pMagicCallThis = 0;` |
|      130 | 1177 | `	pVm->pMagicCallClass = 0;` |
|      130 | 1178 | `	if( pClass == 0 ){` |
|        - | 1179 | `		/* Defensive: the mark and the latch are set together by OP_MEMBER, so a` |
|        - | 1180 | `		 * classless arrival is unreachable. It was reachable while this body wore a` |
|        - | 1181 | ``		 * function NAME — anyone could call `__phl_magic_call()` — which is exactly`` |
|        - | 1182 | `		 * what the mark retired. */` |
|      ! 0 | 1183 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1184 | `		return PH7_OK;` |
|        - | 1185 | `	}` |
|      130 | 1186 | `	SyStringInitFromBuf(&sMethName,SyBlobData(&pVm->sMagicCallName),SyBlobLength(&pVm->sMagicCallName));` |
|      130 | 1187 | `	PH7_MemObjInit(pVm,&sResult);` |
|        - | 1188 | `	/* The one packing site, shared with every CALLABLE spelling of the same call` |
|        - | 1189 | `	 * (PH7_VmDispatchMagicCall, vm_builtin_call.c): the handler lookup, the $args array` |
|        - | 1190 | `	 * — named arguments keyed by name, as php keys them — and the engine-dispatch` |
|        - | 1191 | `	 * visibility rule all live there. Two copies of this is how the syntax path and the` |
|        - | 1192 | `	 * callable path came to disagree about the argument names in the first place.` |
|        - | 1193 | `	 * SXERR_NOTFOUND is unreachable: OP_MEMBER verified the handler exists. */` |
|      193 | 1194 | `	rc = PH7_VmDispatchMagicCall(pVm,pClass,pRecv,SyStringData(&sMethName),` |
|       63 | 1195 | `		SyStringLength(&sMethName),&sResult,nArg,apArg,pCtx->pArgMap);` |
|      130 | 1196 | `	if( rc == SXRET_OK ){` |
|      124 | 1197 | `		ph7_result_value(pCtx,&sResult);` |
|       60 | 1198 | `	}` |
|      130 | 1199 | `	PH7_MemObjRelease(&sResult);` |
|      130 | 1200 | `	if( pRecv ){` |
|       88 | 1201 | `		PH7_ClassInstanceUnref(pRecv);` |
|       42 | 1202 | `	}` |
|      130 | 1203 | `	SyBlobReset(&pVm->sMagicCallName);` |
|      130 | 1204 | `	return (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) ? rc : PH7_OK;` |
|       67 | 1205 | `}` |
|        - | 1206 | `/*` |
|        - | 1207 | ` * The function record OP_CALL dispatches a MEMOBJ_AUX_MAGICCALL callee slot through,` |
|        - | 1208 | ` * built on first use and owned by the VM (the allocator frees it with everything else).` |
|        - | 1209 | ` *` |
|        - | 1210 | ` * It is deliberately NOT installed in pVm->hHostFunction: the same property that makes` |
|        - | 1211 | ` * a native class method unreachable except by dispatching the method (see` |
|        - | 1212 | ` * PH7_NativeClassInstallMethod) makes this body unreachable except by the engine's own` |
|        - | 1213 | ` * __call routing. It carries no signature and no arity bounds, so the OP_CALL choke` |
|        - | 1214 | ` * point's ZPP and ArgumentCountError screens are inert for it — the handler's OWN` |
|        - | 1215 | ` * declared parameters are what php enforces, and it is a PHP method with a frame of its` |
|        - | 1216 | ` * own. sName is a diagnostic label only; nothing that reads it can be reached from here.` |
|        - | 1217 | ` */` |
|      126 | 1218 | `PH7_PRIVATE ph7_user_func * PH7_VmMagicCallFunc(ph7_vm *pVm)` |
|        4 | 1219 | `{` |
|        - | 1220 | `	SyString sName;` |
|      130 | 1221 | `	if( pVm->pMagicCallFunc == 0 ){` |
|       14 | 1222 | `		SyStringInitFromBuf(&sName,"__call",sizeof("__call")-1);` |
|       15 | 1223 | `		if( PH7_NewForeignFunction(&(*pVm),&sName,VmMagicCallDispatch,0,` |
|       14 | 1224 | `			&pVm->pMagicCallFunc) != SXRET_OK ){` |
|      ! 0 | 1225 | `			pVm->pMagicCallFunc = 0;` |
|      ! 0 | 1226 | `		}` |
|        5 | 1227 | `	}` |
|      130 | 1228 | `	return pVm->pMagicCallFunc;` |
|        4 | 1229 | `}` |
|        - | 1230 |  |
