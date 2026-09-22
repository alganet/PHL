# src/ph7/vm_include.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 467/554 lines (84.30%)

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
|        - |   12 | ` *    spl_autoload group and the __phl_magic_call trampoline.` |
|        - |   13 | ` *    Registration rows stay in vm.c's aVmFunc[].` |
|        - |   14 | ` * Status:` |
|        - |   15 | ` *    Stable.` |
|        - |   16 | ` */` |
|        - |   17 | `/*` |
|        - |   18 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |   19 | ` * Refer to the eval() language construct implementation for more` |
|        - |   20 | ` * information.` |
|        - |   21 | ` */` |
|   104686 |   22 | `PH7_PRIVATE sxi32 VmEvalChunk(` |
|        - |   23 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |   24 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |   25 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |   26 | `	int iFlags,         /* Compile flag */` |
|        - |   27 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |   28 | `	)` |
|        5 |   29 | `{` |
|        - |   30 | `	SySet *pByteCode,aByteCode;` |
|   104691 |   31 | `	ProcConsumer xErr = 0;` |
|   104691 |   32 | `	void *pErrData = 0;` |
|        - |   33 | `	ph7_gen_state sSavedGen;` |
|        - |   34 | `	int bNested;` |
|   104691 |   35 | `	sxi32 rcThrow = SXRET_OK; /* status of a ParseError raised for a failed eval() compile */` |
|        - |   36 | `	/* Initialize bytecode container */` |
|   104691 |   37 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|   104691 |   38 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |   39 | `	/* Reset the code generator */` |
|   104691 |   40 | `	if( bTrueReturn ){` |
|        - |   41 | `		/* Included file,log compile-time errors */` |
|     9387 |   42 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9387 |   43 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4691 |   44 | `	}` |
|        - |   45 | `	/* A non-zero cursor means an OUTER compile is in flight — this eval/include` |
|        - |   46 | `	 * was reached from inside it (an autoload fired while resolving a base class).` |
|        - |   47 | `	 * Wiping the generator would corrupt that outer parse, so snapshot it and hand` |
|        - |   48 | `	 * the nested unit a fresh one; a plain runtime eval/include just resets. */` |
|   104691 |   49 | `	bNested = (pVm->sCodeGen.pIn != 0);` |
|   104691 |   50 | `	if( bNested ){` |
|        5 |   51 | `		PH7_CompilerSaveState(pVm,&sSavedGen,xErr,pErrData);` |
|        3 |   52 | `	}else{` |
|   104687 |   53 | `		PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |   54 | `	}` |
|        - |   55 | `	/* Swap bytecode container */` |
|   104691 |   56 | `	pByteCode = pVm->pByteContainer;` |
|   104691 |   57 | `	pVm->pByteContainer = &aByteCode;` |
|        - |   58 | `	/* Compile the chunk */` |
|   104691 |   59 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|        - |   60 | `	/* Record THIS unit's error count where the nested state restore below cannot` |
|        - |   61 | `	 * wipe it — VmExecDeferredClass reads it to detect a failed re-compile. */` |
|   104691 |   62 | `	pVm->nLastEvalErr = pVm->sCodeGen.nErr;` |
|   157033 |   63 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |   64 | `		/* Compilation error. php makes this a CATCHABLE ParseError for eval()` |
|        - |   65 | `		 * ("syntax error, unexpected ..."), where PHL merely returned false —` |
|        - |   66 | ``		 * so `eval('bad syntax')` silently produced a value instead of throwing.`` |
|        - |   67 | `		 * include/require keep the false return: their parse error is a printed` |
|        - |   68 | `		 * fatal, not an exception. */` |
|       58 |   69 | `		if( pCtx && !bTrueReturn ){` |
|       39 |   70 | `			SyBlob *pErr = &pVm->sCodeGen.sErrBuf;` |
|       39 |   71 | `			ph7_result_bool(pCtx,0);` |
|       39 |   72 | `			if( SyBlobLength(pErr) > 0 ){` |
|       58 |   73 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","%.*s",` |
|       38 |   74 | `					(int)SyBlobLength(pErr),(const char *)SyBlobData(pErr));` |
|       20 |   75 | `			}else{` |
|      ! 0 |   76 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","syntax error");` |
|        1 |   77 | `			}` |
|       19 |   78 | `		}else if( pCtx ){` |
|      ! 0 |   79 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |   80 | `		}` |
|       20 |   81 | `	}else{` |
|        - |   82 | `		/* Mount any newly defined classes. Skipped while the VM is still` |
|        - |   83 | `		 * initializing (builtin chunks): mounting evaluates class-constant/` |
|        - |   84 | `		 * static initializers and installs reference-table entries, and the` |
|        - |   85 | `		 * runtime structures those need (apRefObj, the per-exec object pool` |
|        - |   86 | `		 * baseline) do not exist before PH7_VmMakeReady — which mounts every` |
|        - |   87 | `		 * class unconditionally anyway. */` |
|        - |   88 | `		SyHashEntry *pEntry;` |
|        - |   89 | `		ph7_class *pClass;` |
|        - |   90 | `		ph7_value sResult; /* Return value */` |
|        - |   91 | `		sxi32 rc;` |
|   104653 |   92 | `		if( pVm->nMagic != PH7_VM_INIT ){` |
|     9565 |   93 | `			SyHashResetLoopCursor(&pVm->hClass);` |
|  2733683 |   94 | `			while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|  2719345 |   95 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|        - |   96 | `				/* Only mount classes that haven't been mounted yet */` |
|  2719345 |   97 | `				if( !pClass->bMounted ){` |
|   275895 |   98 | `					rc = VmMountUserClass(pVm,pClass);` |
|   275895 |   99 | `					if( rc != SXRET_OK ){` |
|        - |  100 | `						/* Mount failure (likely memory error) */` |
|        3 |  101 | `						if( pCtx ){` |
|        3 |  102 | `							ph7_result_bool(pCtx,0);` |
|        1 |  103 | `						}` |
|        3 |  104 | `						goto Cleanup;` |
|        - |  105 | `					}` |
|   137944 |  106 | `				}` |
|        5 |  107 | `			}` |
|     4779 |  108 | `		}` |
|   104651 |  109 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  110 | `			/* Out of memory */` |
|      ! 0 |  111 | `			if( pCtx ){` |
|      ! 0 |  112 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  113 | `			}` |
|      ! 0 |  114 | `			goto Cleanup;` |
|        - |  115 | `		}` |
|   104651 |  116 | `		if( bTrueReturn ){` |
|        - |  117 | `			/* Assume a boolean true return value */` |
|     9387 |  118 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4696 |  119 | `		}else{` |
|        - |  120 | `			/* Assume a null return value */` |
|    95269 |  121 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  122 | `		}` |
|        - |  123 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here` |
|        - |  124 | `		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by` |
|        - |  125 | `		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the` |
|        - |  126 | `		 * native-nesting fatal instead of overflowing the C stack. The PHP` |
|        - |  127 | `		 * call-depth cap is OP_CALL-only (BYTECODE.md stage 5). */` |
|   104651 |  128 | `		VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|   104651 |  129 | `		if( pCtx ){` |
|        - |  130 | `			/* Set the execution result */` |
|     9547 |  131 | `			ph7_result_value(pCtx,&sResult);` |
|     4771 |  132 | `		}` |
|   104651 |  133 | `		PH7_MemObjRelease(&sResult);` |
|        - |  134 | `	}` |
|    52343 |  135 | `Cleanup:` |
|        - |  136 | `	/* Cleanup the mess left behind */` |
|   104691 |  137 | `	pVm->pByteContainer = pByteCode;` |
|   104691 |  138 | `	SySetRelease(&aByteCode);` |
|        - |  139 | `	/* Restore the outer compile's generator state if this was a nested unit. */` |
|   104691 |  140 | `	if( bNested ){` |
|        5 |  141 | `		PH7_CompilerRestoreState(pVm,&sSavedGen);` |
|        2 |  142 | `	}` |
|        - |  143 | `	/* A ParseError raised above must reach the caller so the VM unwinds the rest` |
|        - |  144 | ``	 * of the statement; returning OK left `eval('bad'); echo 'x';` running the`` |
|        - |  145 | `	 * echo even though php had already thrown. */` |
|   104691 |  146 | `	return rcThrow;` |
|        5 |  147 | `}` |
|        - |  148 | `/*` |
|        - |  149 | ` * Execute a deferred class declaration (OP_CLASS_DEFER — see the deferral` |
|        - |  150 | ` * block comment in compile_class.c). At this point the declaration's` |
|        - |  151 | ` * execution order has been honored: any spl_autoload_register() statement` |
|        - |  152 | ` * that precedes it in the program has RUN, so each recorded dependency either` |
|        - |  153 | ` * resolves (possibly by autoload, fired inside PH7_VmExtractClass) or is` |
|        - |  154 | ` * genuinely missing. A missing one is reported through *ppMissing — the` |
|        - |  155 | ` * OP_CLASS_DEFER dispatcher throws php's catchable` |
|        - |  156 | `` * `Class/Interface/Trait "X" not found` Error. Once the dependencies resolve,`` |
|        - |  157 | ` * the captured chunk re-compiles through VmEvalChunk (which also mounts the` |
|        - |  158 | ` * newly installed classes); a compile failure there (e.g. a body syntax error` |
|        - |  159 | ` * whose diagnosis was deferred along with the declaration) has already been` |
|        - |  160 | ` * reported through the engine's error consumer, so it aborts execution.` |
|        - |  161 | ` * The site is idempotent: bDone short-circuits re-execution (a loop around an` |
|        - |  162 | ` * anonymous class instantiates the same installed class, php's` |
|        - |  163 | ` * one-class-per-site semantics).` |
|        - |  164 | ` */` |
|       30 |  165 | `PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing)` |
|        2 |  166 | `{` |
|        - |  167 | `	VmDeferredReq *aReq;` |
|        - |  168 | `	sxu32 n;` |
|       32 |  169 | `	*ppMissing = 0;` |
|       32 |  170 | `	if( pDefer->bDone ){` |
|        5 |  171 | `		return SXRET_OK;` |
|        - |  172 | `	}` |
|       28 |  173 | `	aReq = (VmDeferredReq *)SySetBasePtr(&pDefer->aRequired);` |
|       48 |  174 | `	for( n = 0 ; n < SySetUsed(&pDefer->aRequired) ; ++n ){` |
|       32 |  175 | `		if( PH7_VmExtractClass(&(*pVm),aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|       12 |  176 | `			*ppMissing = &aReq[n];` |
|       12 |  177 | `			return SXRET_OK; /* caller throws */` |
|        - |  178 | `		}` |
|       12 |  179 | `	}` |
|       18 |  180 | `	if( pDefer->sAnonName.nByte > 0 ){` |
|        5 |  181 | `		pVm->sDeferAnonName = pDefer->sAnonName;` |
|        2 |  182 | `	}` |
|       18 |  183 | `	VmEvalChunk(&(*pVm),0,&pDefer->sText,PH7_PHP_ONLY,TRUE);` |
|       18 |  184 | `	pVm->sDeferAnonName.zString = 0;` |
|       18 |  185 | `	pVm->sDeferAnonName.nByte = 0;` |
|       16 |  186 | `	if( pVm->nLastEvalErr > 0` |
|       18 |  187 | `	 \|\| PH7_VmExtractClass(&(*pVm),pDefer->sSelfName.zString,pDefer->sSelfName.nByte,FALSE,0) == 0 ){` |
|        - |  188 | `		/* The re-compile failed — a deferred-along syntax error or a` |
|        - |  189 | `		 * redeclaration fatal. It was already reported through the engine's` |
|        - |  190 | `		 * error consumer; halt like a fatal (php exits 255 for both). */` |
|      ! 0 |  191 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  192 | `		return SXERR_ABORT;` |
|        - |  193 | `	}` |
|       18 |  194 | `	pDefer->bDone = 1;` |
|       18 |  195 | `	return SXRET_OK;` |
|       17 |  196 | `}` |
|        - |  197 | `/*` |
|        - |  198 | ` * Compile an embedded builtin PHP chunk into the VM. Thin exported wrapper` |
|        - |  199 | ` * around the static VmEvalChunk for builtin libraries that live outside` |
|        - |  200 | ` * this file (e.g. the Reflection classes in vm_builtin_reflection.c).` |
|        - |  201 | ` */` |
|    86032 |  202 | `PH7_PRIVATE sxi32 PH7_VmEvalBuiltinChunk(ph7_vm *pVm,const char *zSrc,sxu32 nLen)` |
|        5 |  203 | `{` |
|        - |  204 | `	SyString sChunk;` |
|    86037 |  205 | `	SyStringInitFromBuf(&sChunk,zSrc,nLen);` |
|    86037 |  206 | `	return VmEvalChunk(&(*pVm),0,&sChunk,PH7_PHP_ONLY,FALSE);` |
|        5 |  207 | `}` |
|        - |  208 | `/*` |
|        - |  209 | ` * value eval(string $code)` |
|        - |  210 | ` *   Evaluate a string as PHP code.` |
|        - |  211 | ` * Parameter` |
|        - |  212 | ` *  code: PHP code to evaluate.` |
|        - |  213 | ` * Return` |
|        - |  214 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  215 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  216 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  217 | ` */` |
|      220 |  218 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  219 | `{` |
|        - |  220 | `	SyString sChunk;    /* Chunk to evaluate */` |
|        - |  221 | `	sxi32 rc;` |
|      225 |  222 | `	if( nArg < 1 ){` |
|        - |  223 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  224 | `		ph7_result_null(pCtx);` |
|      ! 0 |  225 | `		return SXRET_OK;` |
|        - |  226 | `	}` |
|        - |  227 | `	/* Chunk to evaluate. Coerced USER-VISIBLY like every other string argument` |
|        - |  228 | `	 * spelled as a language construct: an object with no __toString() is php's` |
|        - |  229 | `	 * Error, where PHL used to hand the parser the literal "Object" and answer a` |
|        - |  230 | `	 * ParseError. */` |
|        - |  231 | `	{` |
|      225 |  232 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sChunk.zString,(int *)&sChunk.nByte);` |
|      225 |  233 | `		if( rcSv != SXRET_OK ){` |
|        3 |  234 | `			return rcSv;` |
|        - |  235 | `		}` |
|        - |  236 | `	}` |
|      223 |  237 | `	if( sChunk.nByte < 1 ){` |
|        - |  238 | `		/* php: eval('') compiles nothing and yields FALSE (a whitespace-only` |
|        - |  239 | `		 * chunk still compiles, and yields NULL through the normal path). */` |
|        3 |  240 | `		ph7_result_bool(pCtx,0);` |
|        3 |  241 | `		return SXRET_OK;` |
|        - |  242 | `	}` |
|        - |  243 | `	/* Eval the chunk */` |
|      221 |  244 | `	rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|      221 |  245 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  246 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       69 |  247 | `		return PH7_ABORT;` |
|        - |  248 | `	}` |
|        - |  249 | `	/* Propagate a ParseError from a failed compile (php unwinds; PHL used to` |
|        - |  250 | `	 * keep executing the statement that contained the eval). */` |
|      153 |  251 | `	return rc;` |
|      115 |  252 | `}` |
|        - |  253 | `/*` |
|        - |  254 | ` * Check if a file path is already included.` |
|        - |  255 | ` */` |
|     9378 |  256 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        5 |  257 | `{` |
|        - |  258 | `	SyString *aEntries;` |
|        - |  259 | `	sxu32 n;` |
|     9383 |  260 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  261 | `	/* Perform a linear search */` |
| 21855681 |  262 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 21846314 |  263 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  264 | `			/* Already included */` |
|       16 |  265 | `			return TRUE;` |
|        - |  266 | `		}` |
| 10923151 |  267 | `	}` |
|     9369 |  268 | `	return FALSE;` |
|     4694 |  269 | `}` |
|        - |  270 | `/*` |
|        - |  271 | ` * Push a file path in the appropriate VM container.` |
|        - |  272 | ` */` |
|    13906 |  273 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 |  274 | `{` |
|        - |  275 | `	SyString sPath;` |
|        - |  276 | `	char *zDup;` |
|        - |  277 | `	sxi32 rc;` |
|    13911 |  278 | `	if( nLen < 0 ){` |
|     4533 |  279 | `		nLen = SyStrlen(zPath);` |
|     2264 |  280 | `	}` |
|        - |  281 | `	/* Duplicate the file path first */` |
|    13911 |  282 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    13911 |  283 | `	if( zDup == 0 ){` |
|      ! 0 |  284 | `		return SXERR_MEM;` |
|        - |  285 | `	}` |
|        - |  286 | `#ifdef __UNIXES__` |
|        - |  287 | `	/* php records the REALPATH'd absolute path for __FILE__/__DIR__/getFileName` |
|        - |  288 | `	 * and include-once dedup: realpath() resolves '.', '..' and symlinks (e.g.` |
|        - |  289 | `	 * macOS /tmp -> /private/tmp). It needs an existing file, so on failure keep` |
|        - |  290 | `	 * the raw path (eval'd code, php:///data:// wrappers, a missing include that` |
|        - |  291 | `	 * errors elsewhere). */` |
|        - |  292 | `	{` |
|    13906 |  293 | `		char *zReal = realpath(zDup,0); /* POSIX: malloc'd result */` |
|    13906 |  294 | `		if( zReal ){` |
|    13870 |  295 | `			sxu32 nReal = SyStrlen(zReal);` |
|    13870 |  296 | `			char *zRealDup = SyMemBackendStrDup(&pVm->sAllocator,zReal,nReal);` |
|    13870 |  297 | `			free(zReal);` |
|    13870 |  298 | `			if( zRealDup ){` |
|    13870 |  299 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|    13870 |  300 | `				zDup = zRealDup;` |
|    13870 |  301 | `				nLen = (int)nReal;` |
|     6935 |  302 | `			}` |
|     6935 |  303 | `		}` |
|        - |  304 | `	}` |
|        - |  305 | `#endif` |
|        - |  306 | `#ifdef __WINNT__` |
|        - |  307 | `	/* Windows counterpart of the realpath() above: canonicalize to an absolute` |
|        - |  308 | `	 * path (resolves '.'/'..' , '/' -> '\'), case-preserved to match php — NOT` |
|        - |  309 | `	 * lowercased as the legacy PH7 code did. Like the POSIX branch, keep the raw` |
|        - |  310 | `	 * path when it does not name an existing file (the "Command line code" marker,` |
|        - |  311 | `	 * eval'd code, stream wrappers). _fullpath() is lexical, so pair it with a VFS` |
|        - |  312 | `	 * existence check. */` |
|        - |  313 | `	{` |
|        5 |  314 | `		char *zFull = _fullpath(0,zDup,0); /* MSVC CRT: malloc'd absolute path */` |
|        5 |  315 | `		if( zFull ){` |
|        5 |  316 | `			if( pVm->pEngine->pVfs && pVm->pEngine->pVfs->xFileExists && pVm->pEngine->pVfs->xFileExists(zFull) == PH7_OK ){` |
|        5 |  317 | `				sxu32 nFull = SyStrlen(zFull);` |
|        5 |  318 | `				char *zFullDup = SyMemBackendStrDup(&pVm->sAllocator,zFull,nFull);` |
|        5 |  319 | `				if( zFullDup ){` |
|        5 |  320 | `					SyMemBackendFree(&pVm->sAllocator,zDup);` |
|        5 |  321 | `					zDup = zFullDup;` |
|        5 |  322 | `					nLen = (int)nFull;` |
|        - |  323 | `				}` |
|        - |  324 | `			}` |
|        5 |  325 | `			free(zFull);` |
|        - |  326 | `		}` |
|        - |  327 | `	}` |
|        - |  328 | `#endif` |
|        - |  329 | `	/* Install the file path */` |
|    13911 |  330 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    13911 |  331 | `	if( !bMain ){` |
|     9383 |  332 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  333 | `			/* Already included */` |
|       16 |  334 | `			*pNew = 0;` |
|        9 |  335 | `		}else{` |
|        - |  336 | `			/* Insert in the corresponding container */` |
|     9369 |  337 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|     9369 |  338 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  339 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  340 | `				return rc;` |
|        - |  341 | `			}` |
|     9369 |  342 | `			*pNew = 1;` |
|        - |  343 | `		}` |
|     4689 |  344 | `	}` |
|    13911 |  345 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    13911 |  346 | `	return SXRET_OK;` |
|     6958 |  347 | `}` |
|        - |  348 | `/*` |
|        - |  349 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  350 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  351 | ` * indicates failure.` |
|        - |  352 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  353 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  354 | ` * operations.` |
|        - |  355 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  356 | ` * this function is a no-op.` |
|        - |  357 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  358 | ` * constructs for more information.` |
|        - |  359 | ` */` |
|     9384 |  360 | `static sxi32 VmExecIncludedFile(` |
|        - |  361 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  362 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  363 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  364 | `	 )` |
|        5 |  365 | `{` |
|        - |  366 | `	sxi32 rc;` |
|        - |  367 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  368 | `	const ph7_io_stream *pStream;` |
|        - |  369 | `	SyBlob sContents;` |
|        - |  370 | `	void *pHandle;` |
|        - |  371 | `	ph7_vm *pVm;` |
|        - |  372 | `	int isNew;` |
|        - |  373 | `	/* Initialize fields */` |
|     9389 |  374 | `	pVm = pCtx->pVm;` |
|     9389 |  375 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9389 |  376 | `	isNew = 0;` |
|        - |  377 | `	/* Extract the associated stream */` |
|     9389 |  378 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  379 | `	/*` |
|        - |  380 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  381 | `	 * in a read-only mode.` |
|        - |  382 | `	 */` |
|     9389 |  383 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9389 |  384 | `	if( pHandle == 0 ){` |
|        8 |  385 | `		return SXERR_IO;` |
|        - |  386 | `	}` |
|     9383 |  387 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9383 |  388 | `	if( IncludeOnce && !isNew ){` |
|        - |  389 | `		/* Already included */` |
|       14 |  390 | `		rc = SXERR_EXISTS;` |
|        8 |  391 | `	}else{` |
|        - |  392 | `		/* Read the whole file contents */` |
|     9371 |  393 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9371 |  394 | `		if( rc == SXRET_OK ){` |
|        - |  395 | `			SyString sScript;` |
|        - |  396 | `			/* Compile and execute the script */` |
|     9371 |  397 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9371 |  398 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4683 |  399 | `		}` |
|        - |  400 | `	}` |
|        - |  401 | `	/* Pop from the set of included file */` |
|     9383 |  402 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  403 | `	/* Close the handle */` |
|     9383 |  404 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  405 | `	/* Release the working buffer */` |
|     9383 |  406 | `	SyBlobRelease(&sContents);` |
|        - |  407 | `#else` |
|        - |  408 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  409 | `	SXUNUSED(pPath);` |
|        - |  410 | `	SXUNUSED(IncludeOnce);` |
|        - |  411 | `	rc = SXERR_IO;` |
|        - |  412 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9383 |  413 | `	return rc;` |
|     4697 |  414 | `}` |
|        - |  415 | `/*` |
|        - |  416 | ` * string get_include_path(void)` |
|        - |  417 | ` *  Gets the current include_path configuration option.` |
|        - |  418 | ` * Parameter` |
|        - |  419 | ` *  None` |
|        - |  420 | ` * Return` |
|        - |  421 | ` *  Included paths as a string` |
|        - |  422 | ` */` |
|        8 |  423 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  424 | `{` |
|       10 |  425 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  426 | `	SyString *aEntry;` |
|        - |  427 | `	int dir_sep;` |
|        - |  428 | `	sxu32 n;` |
|        - |  429 | `#ifdef __WINNT__` |
|        2 |  430 | `	dir_sep = ';';` |
|        - |  431 | `#else` |
|        - |  432 | `	/* Assume UNIX path separator */` |
|        8 |  433 | `	dir_sep = ':';` |
|        - |  434 | `#endif` |
|        4 |  435 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 |  436 | `	SXUNUSED(apArg);` |
|        - |  437 | `	/* Point to the list of import paths */` |
|       10 |  438 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       20 |  439 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|       12 |  440 | `		SyString *pEntry = &aEntry[n];` |
|       12 |  441 | `		if( n > 0 ){` |
|        - |  442 | `			/* Append dir seprator */` |
|        2 |  443 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|        1 |  444 | `		}` |
|        - |  445 | `		/* Append path */` |
|       12 |  446 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        7 |  447 | `	}` |
|       10 |  448 | `	return PH7_OK;` |
|        2 |  449 | `}` |
|        - |  450 | `/*` |
|        - |  451 | ` * string\|false set_include_path(string $include_path)` |
|        - |  452 | ` *  Sets the include_path configuration option for the duration of the script,` |
|        - |  453 | ` *  returning the OLD value (php contract).` |
|        - |  454 | ` */` |
|        6 |  455 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  456 | `{` |
|        7 |  457 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  458 | `	SyString *aEntry;` |
|        - |  459 | `	const char *zNew, *z, *zEnd;` |
|        - |  460 | `	int dir_sep, nLen;` |
|        - |  461 | `	sxu32 n;` |
|        - |  462 | `#ifdef __WINNT__` |
|        1 |  463 | `	dir_sep = ';';` |
|        - |  464 | `#else` |
|        6 |  465 | `	dir_sep = ':';` |
|        - |  466 | `#endif` |
|        - |  467 | `	/* Build the OLD include_path first: it is this call's return value */` |
|        7 |  468 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       15 |  469 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        9 |  470 | `		if( n > 0 ){ ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char)); }` |
|        9 |  471 | `		ph7_result_string(pCtx,aEntry[n].zString,(int)aEntry[n].nByte);` |
|        5 |  472 | `	}` |
|        7 |  473 | `	if( nArg < 1 ){` |
|      ! 0 |  474 | `		return PH7_OK;` |
|        - |  475 | `	}` |
|        - |  476 | `	/* Replace the path set with the separated segments of the new value.` |
|        - |  477 | `	 * Segments are duped into the VM allocator (reclaimed at VM teardown) so` |
|        - |  478 | `	 * the SyString entries stay valid, mirroring the config-time literals. */` |
|        7 |  479 | `	zNew = ph7_value_to_string(apArg[0],&nLen);` |
|        7 |  480 | `	SySetReset(&pVm->aPaths);` |
|        7 |  481 | `	z = zNew; zEnd = &zNew[nLen];` |
|       15 |  482 | `	while( z < zEnd ){` |
|        9 |  483 | `		const char *zStart = z;` |
|        - |  484 | `		SyString sPath;` |
|       49 |  485 | `		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }` |
|        9 |  486 | `		if( z > zStart ){` |
|        9 |  487 | `			char *zDup = (char *)SyMemBackendDup(&pVm->sAllocator,zStart,(sxu32)(z-zStart));` |
|        9 |  488 | `			if( zDup ){` |
|        9 |  489 | `				SyStringInitFromBuf(&sPath,zDup,(sxu32)(z-zStart));` |
|        - |  490 | `#ifdef __WINNT__` |
|        1 |  491 | `				SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  492 | `#endif` |
|        9 |  493 | `				SyStringTrimTrailingChar(&sPath,'/');` |
|        9 |  494 | `				SyStringFullTrim(&sPath);` |
|        9 |  495 | `				if( sPath.nByte > 0 ){` |
|        9 |  496 | `					SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|        4 |  497 | `				}` |
|        4 |  498 | `			}` |
|        4 |  499 | `		}` |
|        9 |  500 | `		if( z < zEnd ){ z++; } /* skip the separator */` |
|        1 |  501 | `	}` |
|        7 |  502 | `	return PH7_OK;` |
|        4 |  503 | `}` |
|        - |  504 | `/*` |
|        - |  505 | ` * string get_get_included_files(void)` |
|        - |  506 | ` *  Gets the current include_path configuration option.` |
|        - |  507 | ` * Parameter` |
|        - |  508 | ` *  None` |
|        - |  509 | ` * Return` |
|        - |  510 | ` *  Included paths as a string` |
|        - |  511 | ` */` |
|        2 |  512 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  513 | `{` |
|        3 |  514 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  515 | `	ph7_value *pArray,*pWorker;` |
|        - |  516 | `	SyString *pEntry;` |
|        - |  517 | `	int c,d;` |
|        - |  518 | `	/* Create an array and a working value */` |
|        3 |  519 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  520 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  521 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  522 | `		/* Out of memory,return null */` |
|      ! 0 |  523 | `		ph7_result_null(pCtx);` |
|      ! 0 |  524 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  525 | `		SXUNUSED(apArg);` |
|      ! 0 |  526 | `		return PH7_OK;` |
|        - |  527 | `	}` |
|        3 |  528 | `	c = d = '/';` |
|        - |  529 | `#ifdef __WINNT__` |
|        1 |  530 | `	d = '\\';` |
|        - |  531 | `#endif` |
|        - |  532 | `	/* Iterate throw entries */` |
|        3 |  533 | `	SySetResetCursor(pFiles);` |
|        7 |  534 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  535 | `		const char *zBase,*zEnd;` |
|        - |  536 | `		int iLen;` |
|        - |  537 | `		/* reset the string cursor */` |
|        5 |  538 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  539 | `		/* Extract base name */` |
|        5 |  540 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  541 | `		/* Ignore trailing '/' */` |
|        7 |  542 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  543 | `			zEnd--;` |
|      ! 0 |  544 | `		}` |
|        5 |  545 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|       79 |  546 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|       73 |  547 | `			zEnd--;` |
|        1 |  548 | `		}` |
|        5 |  549 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|        5 |  550 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  551 | `		/* Copy entry name */` |
|        5 |  552 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  553 | `		/* Perform the insertion */` |
|        5 |  554 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  555 | `	}` |
|        - |  556 | `	/* All done,return the created array */` |
|        3 |  557 | `	ph7_result_value(pCtx,pArray);` |
|        - |  558 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  559 | `	 * by the engine as soon we return from this foreign` |
|        - |  560 | `	 * function.` |
|        - |  561 | `	 */` |
|        3 |  562 | `	return PH7_OK;` |
|        2 |  563 | `}` |
|        - |  564 | `/*` |
|        - |  565 | ` * include:` |
|        - |  566 | ` * According to the PHP reference manual.` |
|        - |  567 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  568 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  569 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  570 | ` *  include() will finally check in the calling script's own directory` |
|        - |  571 | ` *  and the current working directory before failing. The include()` |
|        - |  572 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  573 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  574 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  575 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  576 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  577 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  578 | ` *  directory to find the requested file.` |
|        - |  579 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  580 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  581 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  582 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  583 | ` */` |
|     9344 |  584 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  585 | `{` |
|        - |  586 | `	SyString sFile;` |
|        - |  587 | `	sxi32 rc;` |
|     9348 |  588 | `	if( nArg < 1 ){` |
|        - |  589 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  590 | `		ph7_result_null(pCtx);` |
|      ! 0 |  591 | `		return SXRET_OK;` |
|        - |  592 | `	}` |
|        - |  593 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  594 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  595 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  596 | `	{` |
|     9348 |  597 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|     9348 |  598 | `		if( rcSv != SXRET_OK ){` |
|        3 |  599 | `			return rcSv;` |
|        - |  600 | `		}` |
|        - |  601 | `	}` |
|     9346 |  602 | `	if( sFile.nByte < 1 ){` |
|        - |  603 | `		/* Empty string,return NULL */` |
|      ! 0 |  604 | `		ph7_result_null(pCtx);` |
|      ! 0 |  605 | `		return SXRET_OK;` |
|        - |  606 | `	}` |
|        - |  607 | `	/* Open,compile and execute the desired script */` |
|     9346 |  608 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9346 |  609 | `	if( rc != SXRET_OK ){` |
|        - |  610 | `		/* Emit a warning and return false */` |
|        3 |  611 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  612 | `		ph7_result_bool(pCtx,0);` |
|        1 |  613 | `	}` |
|     9346 |  614 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  615 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 |  616 | `		return PH7_ABORT;` |
|        - |  617 | `	}` |
|     9340 |  618 | `	return SXRET_OK;` |
|     4676 |  619 | `}` |
|        - |  620 | `/*` |
|        - |  621 | ` * include_once:` |
|        - |  622 | ` *  According to the PHP reference manual.` |
|        - |  623 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  624 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  625 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  626 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  627 | ` *   just once.` |
|        - |  628 | ` */` |
|       18 |  629 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  630 | `{` |
|        - |  631 | `	SyString sFile;` |
|        - |  632 | `	sxi32 rc;` |
|       21 |  633 | `	if( nArg < 1 ){` |
|        - |  634 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  635 | `		ph7_result_null(pCtx);` |
|      ! 0 |  636 | `		return SXRET_OK;` |
|        - |  637 | `	}` |
|        - |  638 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  639 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  640 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  641 | `	{` |
|       21 |  642 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       21 |  643 | `		if( rcSv != SXRET_OK ){` |
|        3 |  644 | `			return rcSv;` |
|        - |  645 | `		}` |
|        - |  646 | `	}` |
|       18 |  647 | `	if( sFile.nByte < 1 ){` |
|        - |  648 | `		/* Empty string,return NULL */` |
|      ! 0 |  649 | `		ph7_result_null(pCtx);` |
|      ! 0 |  650 | `		return SXRET_OK;` |
|        - |  651 | `	}` |
|        - |  652 | `	/* Open,compile and execute the desired script */` |
|       18 |  653 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       18 |  654 | `	if( rc == SXERR_EXISTS ){` |
|        - |  655 | `		/* File already included,return TRUE */` |
|       12 |  656 | `		ph7_result_bool(pCtx,1);` |
|       12 |  657 | `		return SXRET_OK;` |
|        - |  658 | `	}` |
|        8 |  659 | `	if( rc != SXRET_OK ){` |
|        - |  660 | `		/* Emit a warning and return false */` |
|      ! 0 |  661 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  662 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  663 | ` 	}` |
|        8 |  664 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  665 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  666 | `		return PH7_ABORT;` |
|        - |  667 | `	}` |
|        8 |  668 | `	return SXRET_OK;` |
|       12 |  669 | `}` |
|        - |  670 | `/*` |
|        - |  671 | ` * require.` |
|        - |  672 | ` *  According to the PHP reference manual.` |
|        - |  673 | ` *   require() is identical to include() except upon failure it will` |
|        - |  674 | ` *   also produce a fatal level error.` |
|        - |  675 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  676 | ` *   emits a warning  which allows the script to continue.` |
|        - |  677 | ` */` |
|       18 |  678 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  679 | `{` |
|        - |  680 | `	SyString sFile;` |
|        - |  681 | `	sxi32 rc;` |
|       21 |  682 | `	if( nArg < 1 ){` |
|        - |  683 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  684 | `		ph7_result_null(pCtx);` |
|      ! 0 |  685 | `		return SXRET_OK;` |
|        - |  686 | `	}` |
|        - |  687 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  688 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  689 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  690 | `	{` |
|       21 |  691 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       21 |  692 | `		if( rcSv != SXRET_OK ){` |
|        3 |  693 | `			return rcSv;` |
|        - |  694 | `		}` |
|        - |  695 | `	}` |
|       19 |  696 | `	if( sFile.nByte < 1 ){` |
|        - |  697 | `		/* Empty string,return NULL */` |
|      ! 0 |  698 | `		ph7_result_null(pCtx);` |
|      ! 0 |  699 | `		return SXRET_OK;` |
|        - |  700 | `	}` |
|        - |  701 | `	/* Open,compile and execute the desired script */` |
|       19 |  702 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|       19 |  703 | `	if( rc != SXRET_OK ){` |
|        - |  704 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  705 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  706 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  707 | `		return PH7_ABORT;` |
|        - |  708 | `	}` |
|       19 |  709 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  710 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  711 | `		return PH7_ABORT;` |
|        - |  712 | `	}` |
|       19 |  713 | `	return SXRET_OK;` |
|       12 |  714 | `}` |
|        - |  715 | `/*` |
|        - |  716 | ` * require_once:` |
|        - |  717 | ` *  According to the PHP reference manual.` |
|        - |  718 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  719 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  720 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  721 | ` *   and how it differs from its non _once siblings.` |
|        - |  722 | ` */` |
|        8 |  723 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  724 | `{` |
|        - |  725 | `	SyString sFile;` |
|        - |  726 | `	sxi32 rc;` |
|       11 |  727 | `	if( nArg < 1 ){` |
|        - |  728 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  729 | `		ph7_result_null(pCtx);` |
|      ! 0 |  730 | `		return SXRET_OK;` |
|        - |  731 | `	}` |
|        - |  732 | `	/* File to include. php coerces the path USER-VISIBLY, so an object with no` |
|        - |  733 | `	 * __toString() is the catchable "could not be converted to string" Error --` |
|        - |  734 | `	 * PHL used to include the literal path "Object" and warn about the IO error. */` |
|        - |  735 | `	{` |
|       11 |  736 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);` |
|       11 |  737 | `		if( rcSv != SXRET_OK ){` |
|        3 |  738 | `			return rcSv;` |
|        - |  739 | `		}` |
|        - |  740 | `	}` |
|        8 |  741 | `	if( sFile.nByte < 1 ){` |
|        - |  742 | `		/* Empty string,return NULL */` |
|      ! 0 |  743 | `		ph7_result_null(pCtx);` |
|      ! 0 |  744 | `		return SXRET_OK;` |
|        - |  745 | `	}` |
|        - |  746 | `	/* Open,compile and execute the desired script */` |
|        8 |  747 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        8 |  748 | `	if( rc == SXERR_EXISTS ){` |
|        - |  749 | `		/* File already included,return TRUE */` |
|        3 |  750 | `		ph7_result_bool(pCtx,1);` |
|        3 |  751 | `		return SXRET_OK;` |
|        - |  752 | `	}` |
|        6 |  753 | `	if( rc != SXRET_OK ){` |
|        - |  754 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  755 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  756 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  757 | `		return PH7_ABORT;` |
|        - |  758 | `	}` |
|        6 |  759 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  760 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  761 | `		return PH7_ABORT;` |
|        - |  762 | `	}` |
|        6 |  763 | `	return SXRET_OK;` |
|        7 |  764 | `}` |
|        - |  765 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  766 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  767 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  768 | `/*` |
|        - |  769 | ` * Section:` |
|        - |  770 | ` *  SPL Autoloading functions.` |
|        - |  771 | ` * Status:` |
|        - |  772 | ` *  Stable.` |
|        - |  773 | ` */` |
|        - |  774 | `/*` |
|        - |  775 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - |  776 | ` *  Register given function as __autoload() implementation.` |
|        - |  777 | ` * Parameters` |
|        - |  778 | ` *  callback` |
|        - |  779 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - |  780 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - |  781 | ` *  throw` |
|        - |  782 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - |  783 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - |  784 | ` *  prepend` |
|        - |  785 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - |  786 | ` *   autoload stack instead of appending it.` |
|        - |  787 | ` * Return` |
|        - |  788 | ` *  TRUE on success, FALSE on failure.` |
|        - |  789 | ` */` |
|       54 |  790 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  791 | `{` |
|        - |  792 | `	VmAutoloadCB sEntry;` |
|       59 |  793 | `	ph7_vm *pVm = pCtx->pVm;` |
|       59 |  794 | `	int iPrepend = 0;` |
|        - |  795 | `	sxu32 n;` |
|       59 |  796 | `	if( nArg < 1 ){` |
|        - |  797 | `		/* No callback provided — register default spl_autoload.` |
|        - |  798 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - |  799 | `		/* Check for duplicates first */` |
|        9 |  800 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 |  801 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        4 |  802 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 |  803 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 |  804 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 |  805 | `				ph7_result_bool(pCtx,1);` |
|        5 |  806 | `				return SXRET_OK;` |
|        - |  807 | `			}` |
|      ! 0 |  808 | `		}` |
|        5 |  809 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 |  810 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 |  811 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 |  812 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 |  813 | `		ph7_result_bool(pCtx,1);` |
|        5 |  814 | `		return SXRET_OK;` |
|        - |  815 | `	}` |
|        - |  816 | `	/* Validate that the callback is callable */` |
|       51 |  817 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 |  818 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 |  819 | `		if( nArg >= 2 ){` |
|      ! 0 |  820 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  821 | `		}` |
|      ! 0 |  822 | `		if( iThrow ){` |
|      ! 0 |  823 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - |  824 | `				"Argument is not callable");` |
|      ! 0 |  825 | `		}` |
|      ! 0 |  826 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  827 | `		return SXRET_OK;` |
|        - |  828 | `	}` |
|        - |  829 | `	/* Check for duplicates */` |
|       69 |  830 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 |  831 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 |  832 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  833 | `			/* Already registered */` |
|      ! 0 |  834 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  835 | `			return SXRET_OK;` |
|        - |  836 | `		}` |
|       11 |  837 | `	}` |
|        - |  838 | `	/* Check prepend flag */` |
|       51 |  839 | `	if( nArg >= 3 ){` |
|        3 |  840 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 |  841 | `	}` |
|        - |  842 | `	/* Store the callback */` |
|       51 |  843 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       51 |  844 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       51 |  845 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       52 |  846 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - |  847 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - |  848 | `		 * We do this by appending first, then rotating the array. */` |
|        3 |  849 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - |  850 | `		VmAutoloadCB *aBase;` |
|        3 |  851 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  852 | `		/* Rotate: move last entry to front */` |
|        3 |  853 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 |  854 | `		if( aBase ){` |
|        - |  855 | `			VmAutoloadCB sTemp;` |
|        - |  856 | `			sxu32 i;` |
|        3 |  857 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 |  858 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 |  859 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 |  860 | `			}` |
|        3 |  861 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 |  862 | `		}` |
|        2 |  863 | `	}else{` |
|       49 |  864 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  865 | `	}` |
|       51 |  866 | `	ph7_result_bool(pCtx,1);` |
|       51 |  867 | `	return SXRET_OK;` |
|       32 |  868 | `}` |
|        - |  869 | `/*` |
|        - |  870 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - |  871 | ` *  Unregister a given function as __autoload() implementation.` |
|        - |  872 | ` * Parameters` |
|        - |  873 | ` *  callback` |
|        - |  874 | ` *   The autoload function being unregistered.` |
|        - |  875 | ` * Return` |
|        - |  876 | ` *  TRUE on success, FALSE on failure.` |
|        - |  877 | ` */` |
|       32 |  878 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  879 | `{` |
|       37 |  880 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  881 | `	sxu32 n,nEntry;` |
|       37 |  882 | `	if( nArg < 1 ){` |
|      ! 0 |  883 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  884 | `		return SXRET_OK;` |
|        - |  885 | `	}` |
|       37 |  886 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       41 |  887 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       39 |  888 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       39 |  889 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  890 | `			/* Found — remove by shifting remaining entries down */` |
|       35 |  891 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - |  892 | `			sxu32 i;` |
|       35 |  893 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       49 |  894 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 |  895 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 |  896 | `			}` |
|        - |  897 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       35 |  898 | `			SySetPop(&pVm->aAutoload);` |
|       35 |  899 | `			ph7_result_bool(pCtx,1);` |
|       35 |  900 | `			return SXRET_OK;` |
|        - |  901 | `		}` |
|        3 |  902 | `	}` |
|        3 |  903 | `	ph7_result_bool(pCtx,0);` |
|        3 |  904 | `	return SXRET_OK;` |
|       21 |  905 | `}` |
|        - |  906 | `/*` |
|        - |  907 | ` * array spl_autoload_functions(void)` |
|        - |  908 | ` *  Return all registered __autoload() functions.` |
|        - |  909 | ` * Return` |
|        - |  910 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - |  911 | ` *  an empty array is returned.` |
|        - |  912 | ` */` |
|       20 |  913 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  914 | `{` |
|       21 |  915 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  916 | `	ph7_value *pArray;` |
|        - |  917 | `	sxu32 n,nEntry;` |
|       10 |  918 | `	SXUNUSED(nArg);` |
|       10 |  919 | `	SXUNUSED(apArg);` |
|       21 |  920 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 |  921 | `	if( pArray == 0 ){` |
|      ! 0 |  922 | `		ph7_result_null(pCtx);` |
|      ! 0 |  923 | `		return SXRET_OK;` |
|        - |  924 | `	}` |
|       21 |  925 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 |  926 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 |  927 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 |  928 | `		if( pEntry ){` |
|       15 |  929 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 |  930 | `		}` |
|        8 |  931 | `	}` |
|       21 |  932 | `	ph7_result_value(pCtx,pArray);` |
|       21 |  933 | `	return SXRET_OK;` |
|       11 |  934 | `}` |
|        - |  935 | `/*` |
|        - |  936 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - |  937 | ` *  Default implementation of __autoload().` |
|        - |  938 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - |  939 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - |  940 | ` * Parameters` |
|        - |  941 | ` *  class` |
|        - |  942 | ` *   The class name being searched.` |
|        - |  943 | ` *  file_extensions` |
|        - |  944 | ` *   Comma-separated list of file extensions to try.` |
|        - |  945 | ` */` |
|        2 |  946 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  947 | `{` |
|        - |  948 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - |  949 | `	SyBlob sPath;` |
|        - |  950 | `	int nClass;` |
|        - |  951 | `	sxi32 rc;` |
|        3 |  952 | `	if( nArg < 1 ){` |
|      ! 0 |  953 | `		return SXRET_OK;` |
|        - |  954 | `	}` |
|        3 |  955 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 |  956 | `	if( nClass < 1 ){` |
|      ! 0 |  957 | `		return SXRET_OK;` |
|        - |  958 | `	}` |
|        - |  959 | `	/* Default extensions */` |
|        3 |  960 | `	zExt = ".php,.inc";` |
|        3 |  961 | `	if( nArg >= 2 ){` |
|        - |  962 | `		int nExt;` |
|      ! 0 |  963 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 |  964 | `		if( nExt < 1 ){` |
|      ! 0 |  965 | `			zExt = ".php,.inc";` |
|      ! 0 |  966 | `		}` |
|      ! 0 |  967 | `	}` |
|        3 |  968 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - |  969 | `	/* Iterate over comma-separated extensions */` |
|        3 |  970 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 |  971 | `	zCur = zExt;` |
|        7 |  972 | `	while( zCur < zEnd ){` |
|        - |  973 | `		const char *zComma;` |
|        - |  974 | `		SyString sFile;` |
|        - |  975 | `		int i;` |
|        - |  976 | `		/* Find next comma or end */` |
|        5 |  977 | `		zComma = zCur;` |
|       21 |  978 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 |  979 | `			zComma++;` |
|        1 |  980 | `		}` |
|        - |  981 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 |  982 | `		SyBlobReset(&sPath);` |
|       69 |  983 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 |  984 | `			char c = zClass[i];` |
|       65 |  985 | `			if( c == '\\' ){` |
|      ! 0 |  986 | `				c = '/';` |
|       65 |  987 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 |  988 | `				c = c + ('a' - 'A');` |
|        6 |  989 | `			}` |
|       65 |  990 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 |  991 | `		}` |
|        - |  992 | `		/* Append extension */` |
|        5 |  993 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - |  994 | `		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,` |
|        - |  995 | `		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a` |
|        - |  996 | `		 * terminator, so without this the strlen reads past the buffer (a` |
|        - |  997 | `		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL` |
|        - |  998 | `		 * is not counted in SyBlobLength(), so the SyString length stays correct. */` |
|        5 |  999 | `		SyBlobNullAppend(&sPath);` |
|        - | 1000 | `		/* Try to include the file */` |
|        5 | 1001 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 1002 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 1003 | `		if( rc == SXRET_OK ){` |
|        - | 1004 | `			/* File included successfully */` |
|      ! 0 | 1005 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 1006 | `			return SXRET_OK;` |
|        - | 1007 | `		}` |
|        - | 1008 | `		/* Move past the comma */` |
|        5 | 1009 | `		zCur = zComma;` |
|        5 | 1010 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 1011 | `			zCur++;` |
|        1 | 1012 | `		}` |
|        1 | 1013 | `	}` |
|        3 | 1014 | `	SyBlobRelease(&sPath);` |
|        3 | 1015 | `	return SXRET_OK;` |
|        2 | 1016 | `}` |
|        - | 1017 | `/* Table of built-in VM functions. */` |
|        - | 1018 | `/*` |
|        - | 1019 | ` * Hidden packing trampoline for __call / __callStatic (band A #3b).` |
|        - | 1020 | ` * OP_MEMBER, on a missing method whose class declares the magic handler,` |
|        - | 1021 | ` * stashes {receiver, class, original name} on the VM and redirects the` |
|        - | 1022 | ` * callee name to this host function; the normal OP_CALL machinery then` |
|        - | 1023 | ` * collects the ORIGINAL argument list (incl. spreads) and hands it here,` |
|        - | 1024 | ` * which packs it into a php array and invokes` |
|        - | 1025 | ` *   $recv->__call($name, $args)   /   Class::__callStatic($name, $args)` |
|        - | 1026 | ` * returning the handler's value as the call's result. A throw propagates` |
|        - | 1027 | ` * via the returned status (and the boundary rail). Like the __gen_* /` |
|        - | 1028 | ` * __reflect_* thunks, this is a PHL-internal global — calling it directly` |
|        - | 1029 | ` * yields NULL (documented engine-specific surface).` |
|        - | 1030 | ` */` |
|       26 | 1031 | `PH7_PRIVATE int vm_builtin_magic_call(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 1032 | `{` |
|       30 | 1033 | `	ph7_vm *pVm = pCtx->pVm;` |
|       30 | 1034 | `	ph7_class_instance *pRecv = pVm->pMagicCallThis;` |
|       30 | 1035 | `	ph7_class *pClass = pVm->pMagicCallClass;` |
|       30 | 1036 | `	ph7_class_method *pMeth = 0;` |
|        - | 1037 | `	ph7_hashmap *pMap;` |
|        - | 1038 | `	ph7_value sNameVal,sArgsVal,sResult;` |
|        - | 1039 | `	ph7_value *apCall[2];` |
|        - | 1040 | `	SyString sMethName;` |
|        - | 1041 | `	sxi32 rc;` |
|        - | 1042 | `	int i;` |
|        - | 1043 | `	/* Consume the pending dispatch (one-shot) */` |
|       30 | 1044 | `	pVm->pMagicCallThis = 0;` |
|       30 | 1045 | `	pVm->pMagicCallClass = 0;` |
|       30 | 1046 | `	if( pClass == 0 ){` |
|        - | 1047 | `		/* Not a magic dispatch (direct user invocation): no-op */` |
|      ! 0 | 1048 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1049 | `		return PH7_OK;` |
|        - | 1050 | `	}` |
|       43 | 1051 | `	pMeth = PH7_ClassExtractMethod(pClass,` |
|       13 | 1052 | `		pRecv ? "__call" : "__callStatic",` |
|       13 | 1053 | `		pRecv ? sizeof("__call")-1 : sizeof("__callStatic")-1);` |
|       30 | 1054 | `	if( pMeth == 0 ){` |
|        - | 1055 | `		/* Unreachable: OP_MEMBER verified the handler exists */` |
|      ! 0 | 1056 | `		if( pRecv ){` |
|      ! 0 | 1057 | `			PH7_ClassInstanceUnref(pRecv);` |
|      ! 0 | 1058 | `		}` |
|      ! 0 | 1059 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1060 | `		return PH7_OK;` |
|        - | 1061 | `	}` |
|       30 | 1062 | `	pMap = PH7_NewHashmap(pVm,0,0);` |
|       30 | 1063 | `	if( pMap == 0 ){` |
|      ! 0 | 1064 | `		if( pRecv ){` |
|      ! 0 | 1065 | `			PH7_ClassInstanceUnref(pRecv);` |
|      ! 0 | 1066 | `		}` |
|      ! 0 | 1067 | `		PH7_VmMemoryError(pVm);` |
|      ! 0 | 1068 | `		return PH7_ABORT;` |
|        - | 1069 | `	}` |
|       68 | 1070 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       41 | 1071 | `		PH7_HashmapInsert(pMap,0,apArg[i]);` |
|       22 | 1072 | `	}` |
|       30 | 1073 | `	SyStringInitFromBuf(&sMethName,SyBlobData(&pVm->sMagicCallName),SyBlobLength(&pVm->sMagicCallName));` |
|       30 | 1074 | `	PH7_MemObjInitFromString(pVm,&sNameVal,&sMethName);` |
|       30 | 1075 | `	sNameVal.nIdx = SXU32_HIGH;` |
|       30 | 1076 | `	PH7_MemObjInitFromArray(pVm,&sArgsVal,pMap);` |
|       30 | 1077 | `	sArgsVal.nIdx = SXU32_HIGH;` |
|       30 | 1078 | `	PH7_MemObjInit(pVm,&sResult);` |
|       30 | 1079 | `	apCall[0] = &sNameVal;` |
|       30 | 1080 | `	apCall[1] = &sArgsVal;` |
|       30 | 1081 | `	rc = PH7_VmCallMagicMethod(pVm,pRecv,pMeth,&sResult,2,apCall);` |
|       30 | 1082 | `	if( rc == SXRET_OK ){` |
|       28 | 1083 | `		ph7_result_value(pCtx,&sResult);` |
|       12 | 1084 | `	}` |
|       30 | 1085 | `	PH7_MemObjRelease(&sResult);` |
|       30 | 1086 | `	PH7_MemObjRelease(&sNameVal);` |
|       30 | 1087 | `	PH7_MemObjRelease(&sArgsVal); /* drops the packed array */` |
|       30 | 1088 | `	if( pRecv ){` |
|       19 | 1089 | `		PH7_ClassInstanceUnref(pRecv);` |
|        8 | 1090 | `	}` |
|       30 | 1091 | `	SyBlobReset(&pVm->sMagicCallName);` |
|       30 | 1092 | `	return (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) ? rc : PH7_OK;` |
|       17 | 1093 | `}` |
|        - | 1094 |  |
