# src/ph7/vm_include.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 465/552 lines (84.24%)

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
|    96264 |   22 | `PH7_PRIVATE sxi32 VmEvalChunk(` |
|        - |   23 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |   24 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |   25 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |   26 | `	int iFlags,         /* Compile flag */` |
|        - |   27 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |   28 | `	)` |
|        5 |   29 | `{` |
|        - |   30 | `	SySet *pByteCode,aByteCode;` |
|        - |   31 | `	SyBlob sSavedNs;` |
|    96269 |   32 | `	ProcConsumer xErr = 0;` |
|    96269 |   33 | `	void *pErrData = 0;` |
|        - |   34 | `	ph7_gen_state sSavedGen;` |
|        - |   35 | `	int bNested;` |
|    96269 |   36 | `	sxi32 rcThrow = SXRET_OK; /* status of a ParseError raised for a failed eval() compile */` |
|        - |   37 | `	/* Initialize bytecode container */` |
|    96269 |   38 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    96269 |   39 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |   40 | `	/* Reset the code generator */` |
|    96269 |   41 | `	if( bTrueReturn ){` |
|        - |   42 | `		/* Included file,log compile-time errors */` |
|     9215 |   43 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9215 |   44 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4605 |   45 | `	}` |
|        - |   46 | `	/* A non-zero cursor means an OUTER compile is in flight — this eval/include` |
|        - |   47 | `	 * was reached from inside it (an autoload fired while resolving a base class).` |
|        - |   48 | `	 * Wiping the generator would corrupt that outer parse, so snapshot it and hand` |
|        - |   49 | `	 * the nested unit a fresh one; a plain runtime eval/include just resets. */` |
|    96269 |   50 | `	bNested = (pVm->sCodeGen.pIn != 0);` |
|    96269 |   51 | `	if( bNested ){` |
|        5 |   52 | `		PH7_CompilerSaveState(pVm,&sSavedGen,xErr,pErrData);` |
|        3 |   53 | `	}else{` |
|    96265 |   54 | `		PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |   55 | `	}` |
|        - |   56 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |   57 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |   58 | `	 * the caller's namespace is restored. */` |
|    96269 |   59 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    96269 |   60 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    96269 |   61 | `	if( bTrueReturn ){` |
|        - |   62 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9215 |   63 | `		SyBlobReset(&pVm->sNamespace);` |
|     4605 |   64 | `	}` |
|        - |   65 | `	/* Swap bytecode container */` |
|    96269 |   66 | `	pByteCode = pVm->pByteContainer;` |
|    96269 |   67 | `	pVm->pByteContainer = &aByteCode;` |
|        - |   68 | `	/* Compile the chunk */` |
|    96269 |   69 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|        - |   70 | `	/* Record THIS unit's error count where the nested state restore below cannot` |
|        - |   71 | `	 * wipe it — VmExecDeferredClass reads it to detect a failed re-compile. */` |
|    96269 |   72 | `	pVm->nLastEvalErr = pVm->sCodeGen.nErr;` |
|   144400 |   73 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |   74 | `		/* Compilation error. php makes this a CATCHABLE ParseError for eval()` |
|        - |   75 | `		 * ("syntax error, unexpected ..."), where PHL merely returned false —` |
|        - |   76 | ``		 * so `eval('bad syntax')` silently produced a value instead of throwing.`` |
|        - |   77 | `		 * include/require keep the false return: their parse error is a printed` |
|        - |   78 | `		 * fatal, not an exception. */` |
|        7 |   79 | `		if( pCtx && !bTrueReturn ){` |
|        5 |   80 | `			SyBlob *pErr = &pVm->sCodeGen.sErrBuf;` |
|        5 |   81 | `			ph7_result_bool(pCtx,0);` |
|        5 |   82 | `			if( SyBlobLength(pErr) > 0 ){` |
|        7 |   83 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","%.*s",` |
|        4 |   84 | `					(int)SyBlobLength(pErr),(const char *)SyBlobData(pErr));` |
|        3 |   85 | `			}else{` |
|      ! 0 |   86 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","syntax error");` |
|        1 |   87 | `			}` |
|        2 |   88 | `		}else if( pCtx ){` |
|      ! 0 |   89 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |   90 | `		}` |
|        3 |   91 | `	}else{` |
|        - |   92 | `		/* Mount any newly defined classes. Skipped while the VM is still` |
|        - |   93 | `		 * initializing (builtin chunks): mounting evaluates class-constant/` |
|        - |   94 | `		 * static initializers and installs reference-table entries, and the` |
|        - |   95 | `		 * runtime structures those need (apRefObj, the per-exec object pool` |
|        - |   96 | `		 * baseline) do not exist before PH7_VmMakeReady — which mounts every` |
|        - |   97 | `		 * class unconditionally anyway. */` |
|        - |   98 | `		SyHashEntry *pEntry;` |
|        - |   99 | `		ph7_class *pClass;` |
|        - |  100 | `		ph7_value sResult; /* Return value */` |
|        - |  101 | `		sxi32 rc;` |
|    96265 |  102 | `		if( pVm->nMagic != PH7_VM_INIT ){` |
|     9325 |  103 | `			SyHashResetLoopCursor(&pVm->hClass);` |
|  2528247 |  104 | `			while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|  2514269 |  105 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  106 | `				/* Only mount classes that haven't been mounted yet */` |
|  2514269 |  107 | `				if( !pClass->bMounted ){` |
|   255459 |  108 | `					rc = VmMountUserClass(pVm,pClass);` |
|   255459 |  109 | `					if( rc != SXRET_OK ){` |
|        - |  110 | `						/* Mount failure (likely memory error) */` |
|        3 |  111 | `						if( pCtx ){` |
|        3 |  112 | `							ph7_result_bool(pCtx,0);` |
|        1 |  113 | `						}` |
|        3 |  114 | `						goto Cleanup;` |
|        - |  115 | `					}` |
|   127726 |  116 | `				}` |
|        5 |  117 | `			}` |
|     4659 |  118 | `		}` |
|    96263 |  119 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  120 | `			/* Out of memory */` |
|      ! 0 |  121 | `			if( pCtx ){` |
|      ! 0 |  122 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  123 | `			}` |
|      ! 0 |  124 | `			goto Cleanup;` |
|        - |  125 | `		}` |
|    96263 |  126 | `		if( bTrueReturn ){` |
|        - |  127 | `			/* Assume a boolean true return value */` |
|     9215 |  128 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4610 |  129 | `		}else{` |
|        - |  130 | `			/* Assume a null return value */` |
|    87053 |  131 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  132 | `		}` |
|        - |  133 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here` |
|        - |  134 | `		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by` |
|        - |  135 | `		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the` |
|        - |  136 | `		 * native-nesting fatal instead of overflowing the C stack. The PHP` |
|        - |  137 | `		 * call-depth cap is OP_CALL-only (BYTECODE.md stage 5). */` |
|    96263 |  138 | `		VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    96263 |  139 | `		if( pCtx ){` |
|        - |  140 | `			/* Set the execution result */` |
|     9307 |  141 | `			ph7_result_value(pCtx,&sResult);` |
|     4651 |  142 | `		}` |
|    96263 |  143 | `		PH7_MemObjRelease(&sResult);` |
|        - |  144 | `	}` |
|    48132 |  145 | `Cleanup:` |
|        - |  146 | `	/* Cleanup the mess left behind */` |
|    96269 |  147 | `	pVm->pByteContainer = pByteCode;` |
|    96269 |  148 | `	SySetRelease(&aByteCode);` |
|        - |  149 | `	/* Restore caller's namespace state */` |
|    96269 |  150 | `	SyBlobReset(&pVm->sNamespace);` |
|    96269 |  151 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    96269 |  152 | `	SyBlobRelease(&sSavedNs);` |
|        - |  153 | `	/* Restore the outer compile's generator state if this was a nested unit. */` |
|    96269 |  154 | `	if( bNested ){` |
|        5 |  155 | `		PH7_CompilerRestoreState(pVm,&sSavedGen);` |
|        2 |  156 | `	}` |
|        - |  157 | `	/* A ParseError raised above must reach the caller so the VM unwinds the rest` |
|        - |  158 | ``	 * of the statement; returning OK left `eval('bad'); echo 'x';` running the`` |
|        - |  159 | `	 * echo even though php had already thrown. */` |
|    96269 |  160 | `	return rcThrow;` |
|        5 |  161 | `}` |
|        - |  162 | `/*` |
|        - |  163 | ` * Execute a deferred class declaration (OP_CLASS_DEFER — see the deferral` |
|        - |  164 | ` * block comment in compile_class.c). At this point the declaration's` |
|        - |  165 | ` * execution order has been honored: any spl_autoload_register() statement` |
|        - |  166 | ` * that precedes it in the program has RUN, so each recorded dependency either` |
|        - |  167 | ` * resolves (possibly by autoload, fired inside PH7_VmExtractClass) or is` |
|        - |  168 | ` * genuinely missing. A missing one is reported through *ppMissing — the` |
|        - |  169 | ` * OP_CLASS_DEFER dispatcher throws php's catchable` |
|        - |  170 | `` * `Class/Interface/Trait "X" not found` Error. Once the dependencies resolve,`` |
|        - |  171 | ` * the captured chunk re-compiles through VmEvalChunk (which also mounts the` |
|        - |  172 | ` * newly installed classes); a compile failure there (e.g. a body syntax error` |
|        - |  173 | ` * whose diagnosis was deferred along with the declaration) has already been` |
|        - |  174 | ` * reported through the engine's error consumer, so it aborts execution.` |
|        - |  175 | ` * The site is idempotent: bDone short-circuits re-execution (a loop around an` |
|        - |  176 | ` * anonymous class instantiates the same installed class, php's` |
|        - |  177 | ` * one-class-per-site semantics).` |
|        - |  178 | ` */` |
|       30 |  179 | `PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing)` |
|        2 |  180 | `{` |
|        - |  181 | `	VmDeferredReq *aReq;` |
|        - |  182 | `	sxu32 n;` |
|       32 |  183 | `	*ppMissing = 0;` |
|       32 |  184 | `	if( pDefer->bDone ){` |
|        5 |  185 | `		return SXRET_OK;` |
|        - |  186 | `	}` |
|       28 |  187 | `	aReq = (VmDeferredReq *)SySetBasePtr(&pDefer->aRequired);` |
|       48 |  188 | `	for( n = 0 ; n < SySetUsed(&pDefer->aRequired) ; ++n ){` |
|       32 |  189 | `		if( PH7_VmExtractClass(&(*pVm),aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){` |
|       11 |  190 | `			*ppMissing = &aReq[n];` |
|       11 |  191 | `			return SXRET_OK; /* caller throws */` |
|        - |  192 | `		}` |
|       12 |  193 | `	}` |
|       18 |  194 | `	if( pDefer->sAnonName.nByte > 0 ){` |
|        5 |  195 | `		pVm->sDeferAnonName = pDefer->sAnonName;` |
|        2 |  196 | `	}` |
|       18 |  197 | `	VmEvalChunk(&(*pVm),0,&pDefer->sText,PH7_PHP_ONLY,TRUE);` |
|       18 |  198 | `	pVm->sDeferAnonName.zString = 0;` |
|       18 |  199 | `	pVm->sDeferAnonName.nByte = 0;` |
|       16 |  200 | `	if( pVm->nLastEvalErr > 0` |
|       18 |  201 | `	 \|\| PH7_VmExtractClass(&(*pVm),pDefer->sSelfName.zString,pDefer->sSelfName.nByte,FALSE,0) == 0 ){` |
|        - |  202 | `		/* The re-compile failed — a deferred-along syntax error or a` |
|        - |  203 | `		 * redeclaration fatal. It was already reported through the engine's` |
|        - |  204 | `		 * error consumer; halt like a fatal (php exits 255 for both). */` |
|      ! 0 |  205 | `		pVm->iExitStatus = 255;` |
|      ! 0 |  206 | `		return SXERR_ABORT;` |
|        - |  207 | `	}` |
|       18 |  208 | `	pDefer->bDone = 1;` |
|       18 |  209 | `	return SXRET_OK;` |
|       17 |  210 | `}` |
|        - |  211 | `/*` |
|        - |  212 | ` * Compile an embedded builtin PHP chunk into the VM. Thin exported wrapper` |
|        - |  213 | ` * around the static VmEvalChunk for builtin libraries that live outside` |
|        - |  214 | ` * this file (e.g. the Reflection classes in vm_builtin_reflection.c).` |
|        - |  215 | ` */` |
|    78660 |  216 | `PH7_PRIVATE sxi32 PH7_VmEvalBuiltinChunk(ph7_vm *pVm,const char *zSrc,sxu32 nLen)` |
|        5 |  217 | `{` |
|        - |  218 | `	SyString sChunk;` |
|    78665 |  219 | `	SyStringInitFromBuf(&sChunk,zSrc,nLen);` |
|    78665 |  220 | `	return VmEvalChunk(&(*pVm),0,&sChunk,PH7_PHP_ONLY,FALSE);` |
|        5 |  221 | `}` |
|        - |  222 | `/*` |
|        - |  223 | ` * value eval(string $code)` |
|        - |  224 | ` *   Evaluate a string as PHP code.` |
|        - |  225 | ` * Parameter` |
|        - |  226 | ` *  code: PHP code to evaluate.` |
|        - |  227 | ` * Return` |
|        - |  228 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  229 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  230 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  231 | ` */` |
|      116 |  232 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  233 | `{` |
|        - |  234 | `	SyString sChunk;    /* Chunk to evaluate */` |
|        - |  235 | `	sxi32 rc;` |
|      121 |  236 | `	if( nArg < 1 ){` |
|        - |  237 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  238 | `		ph7_result_null(pCtx);` |
|      ! 0 |  239 | `		return SXRET_OK;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* Chunk to evaluate */` |
|      121 |  242 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|      121 |  243 | `	if( sChunk.nByte < 1 ){` |
|        - |  244 | `		/* php: eval('') compiles nothing and yields FALSE (a whitespace-only` |
|        - |  245 | `		 * chunk still compiles, and yields NULL through the normal path). */` |
|        3 |  246 | `		ph7_result_bool(pCtx,0);` |
|        3 |  247 | `		return SXRET_OK;` |
|        - |  248 | `	}` |
|        - |  249 | `	/* Eval the chunk */` |
|      119 |  250 | `	rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|      119 |  251 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  252 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       69 |  253 | `		return PH7_ABORT;` |
|        - |  254 | `	}` |
|        - |  255 | `	/* Propagate a ParseError from a failed compile (php unwinds; PHL used to` |
|        - |  256 | `	 * keep executing the statement that contained the eval). */` |
|       51 |  257 | `	return rc;` |
|       63 |  258 | `}` |
|        - |  259 | `/*` |
|        - |  260 | ` * Check if a file path is already included.` |
|        - |  261 | ` */` |
|     9206 |  262 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        3 |  263 | `{` |
|        - |  264 | `	SyString *aEntries;` |
|        - |  265 | `	sxu32 n;` |
|     9209 |  266 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  267 | `	/* Perform a linear search */` |
| 21077387 |  268 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 21068194 |  269 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  270 | `			/* Already included */` |
|       16 |  271 | `			return TRUE;` |
|        - |  272 | `		}` |
| 10534091 |  273 | `	}` |
|     9195 |  274 | `	return FALSE;` |
|     4606 |  275 | `}` |
|        - |  276 | `/*` |
|        - |  277 | ` * Push a file path in the appropriate VM container.` |
|        - |  278 | ` */` |
|    13346 |  279 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 |  280 | `{` |
|        - |  281 | `	SyString sPath;` |
|        - |  282 | `	char *zDup;` |
|        - |  283 | `	sxi32 rc;` |
|    13351 |  284 | `	if( nLen < 0 ){` |
|     4145 |  285 | `		nLen = SyStrlen(zPath);` |
|     2070 |  286 | `	}` |
|        - |  287 | `	/* Duplicate the file path first */` |
|    13351 |  288 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    13351 |  289 | `	if( zDup == 0 ){` |
|      ! 0 |  290 | `		return SXERR_MEM;` |
|        - |  291 | `	}` |
|        - |  292 | `#ifdef __UNIXES__` |
|        - |  293 | `	/* php records the REALPATH'd absolute path for __FILE__/__DIR__/getFileName` |
|        - |  294 | `	 * and include-once dedup: realpath() resolves '.', '..' and symlinks (e.g.` |
|        - |  295 | `	 * macOS /tmp -> /private/tmp). It needs an existing file, so on failure keep` |
|        - |  296 | `	 * the raw path (eval'd code, php:///data:// wrappers, a missing include that` |
|        - |  297 | `	 * errors elsewhere). */` |
|        - |  298 | `	{` |
|    13346 |  299 | `		char *zReal = realpath(zDup,0); /* POSIX: malloc'd result */` |
|    13346 |  300 | `		if( zReal ){` |
|    13310 |  301 | `			sxu32 nReal = SyStrlen(zReal);` |
|    13310 |  302 | `			char *zRealDup = SyMemBackendStrDup(&pVm->sAllocator,zReal,nReal);` |
|    13310 |  303 | `			free(zReal);` |
|    13310 |  304 | `			if( zRealDup ){` |
|    13310 |  305 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|    13310 |  306 | `				zDup = zRealDup;` |
|    13310 |  307 | `				nLen = (int)nReal;` |
|     6655 |  308 | `			}` |
|     6655 |  309 | `		}` |
|        - |  310 | `	}` |
|        - |  311 | `#endif` |
|        - |  312 | `#ifdef __WINNT__` |
|        - |  313 | `	/* Windows counterpart of the realpath() above: canonicalize to an absolute` |
|        - |  314 | `	 * path (resolves '.'/'..' , '/' -> '\'), case-preserved to match php — NOT` |
|        - |  315 | `	 * lowercased as the legacy PH7 code did. Like the POSIX branch, keep the raw` |
|        - |  316 | `	 * path when it does not name an existing file (the "Command line code" marker,` |
|        - |  317 | `	 * eval'd code, stream wrappers). _fullpath() is lexical, so pair it with a VFS` |
|        - |  318 | `	 * existence check. */` |
|        - |  319 | `	{` |
|        5 |  320 | `		char *zFull = _fullpath(0,zDup,0); /* MSVC CRT: malloc'd absolute path */` |
|        5 |  321 | `		if( zFull ){` |
|        5 |  322 | `			if( pVm->pEngine->pVfs && pVm->pEngine->pVfs->xFileExists && pVm->pEngine->pVfs->xFileExists(zFull) == PH7_OK ){` |
|        5 |  323 | `				sxu32 nFull = SyStrlen(zFull);` |
|        5 |  324 | `				char *zFullDup = SyMemBackendStrDup(&pVm->sAllocator,zFull,nFull);` |
|        5 |  325 | `				if( zFullDup ){` |
|        5 |  326 | `					SyMemBackendFree(&pVm->sAllocator,zDup);` |
|        5 |  327 | `					zDup = zFullDup;` |
|        5 |  328 | `					nLen = (int)nFull;` |
|        - |  329 | `				}` |
|        - |  330 | `			}` |
|        5 |  331 | `			free(zFull);` |
|        - |  332 | `		}` |
|        - |  333 | `	}` |
|        - |  334 | `#endif` |
|        - |  335 | `	/* Install the file path */` |
|    13351 |  336 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    13351 |  337 | `	if( !bMain ){` |
|     9209 |  338 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  339 | `			/* Already included */` |
|       16 |  340 | `			*pNew = 0;` |
|        9 |  341 | `		}else{` |
|        - |  342 | `			/* Insert in the corresponding container */` |
|     9195 |  343 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|     9195 |  344 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  345 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  346 | `				return rc;` |
|        - |  347 | `			}` |
|     9195 |  348 | `			*pNew = 1;` |
|        - |  349 | `		}` |
|     4603 |  350 | `	}` |
|    13351 |  351 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    13351 |  352 | `	return SXRET_OK;` |
|     6678 |  353 | `}` |
|        - |  354 | `/*` |
|        - |  355 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  356 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  357 | ` * indicates failure.` |
|        - |  358 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  359 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  360 | ` * operations.` |
|        - |  361 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  362 | ` * this function is a no-op.` |
|        - |  363 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  364 | ` * constructs for more information.` |
|        - |  365 | ` */` |
|     9212 |  366 | `static sxi32 VmExecIncludedFile(` |
|        - |  367 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  368 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  369 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  370 | `	 )` |
|        3 |  371 | `{` |
|        - |  372 | `	sxi32 rc;` |
|        - |  373 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  374 | `	const ph7_io_stream *pStream;` |
|        - |  375 | `	SyBlob sContents;` |
|        - |  376 | `	void *pHandle;` |
|        - |  377 | `	ph7_vm *pVm;` |
|        - |  378 | `	int isNew;` |
|        - |  379 | `	/* Initialize fields */` |
|     9215 |  380 | `	pVm = pCtx->pVm;` |
|     9215 |  381 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9215 |  382 | `	isNew = 0;` |
|        - |  383 | `	/* Extract the associated stream */` |
|     9215 |  384 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  385 | `	/*` |
|        - |  386 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  387 | `	 * in a read-only mode.` |
|        - |  388 | `	 */` |
|     9215 |  389 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9215 |  390 | `	if( pHandle == 0 ){` |
|        8 |  391 | `		return SXERR_IO;` |
|        - |  392 | `	}` |
|     9209 |  393 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9209 |  394 | `	if( IncludeOnce && !isNew ){` |
|        - |  395 | `		/* Already included */` |
|       14 |  396 | `		rc = SXERR_EXISTS;` |
|        8 |  397 | `	}else{` |
|        - |  398 | `		/* Read the whole file contents */` |
|     9197 |  399 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9197 |  400 | `		if( rc == SXRET_OK ){` |
|        - |  401 | `			SyString sScript;` |
|        - |  402 | `			/* Compile and execute the script */` |
|     9197 |  403 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9197 |  404 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4597 |  405 | `		}` |
|        - |  406 | `	}` |
|        - |  407 | `	/* Pop from the set of included file */` |
|     9209 |  408 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  409 | `	/* Close the handle */` |
|     9209 |  410 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  411 | `	/* Release the working buffer */` |
|     9209 |  412 | `	SyBlobRelease(&sContents);` |
|        - |  413 | `#else` |
|        - |  414 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  415 | `	SXUNUSED(pPath);` |
|        - |  416 | `	SXUNUSED(IncludeOnce);` |
|        - |  417 | `	rc = SXERR_IO;` |
|        - |  418 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9209 |  419 | `	return rc;` |
|     4609 |  420 | `}` |
|        - |  421 | `/*` |
|        - |  422 | ` * string get_include_path(void)` |
|        - |  423 | ` *  Gets the current include_path configuration option.` |
|        - |  424 | ` * Parameter` |
|        - |  425 | ` *  None` |
|        - |  426 | ` * Return` |
|        - |  427 | ` *  Included paths as a string` |
|        - |  428 | ` */` |
|        8 |  429 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  430 | `{` |
|       10 |  431 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  432 | `	SyString *aEntry;` |
|        - |  433 | `	int dir_sep;` |
|        - |  434 | `	sxu32 n;` |
|        - |  435 | `#ifdef __WINNT__` |
|        2 |  436 | `	dir_sep = ';';` |
|        - |  437 | `#else` |
|        - |  438 | `	/* Assume UNIX path separator */` |
|        8 |  439 | `	dir_sep = ':';` |
|        - |  440 | `#endif` |
|        4 |  441 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 |  442 | `	SXUNUSED(apArg);` |
|        - |  443 | `	/* Point to the list of import paths */` |
|       10 |  444 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       20 |  445 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|       12 |  446 | `		SyString *pEntry = &aEntry[n];` |
|       12 |  447 | `		if( n > 0 ){` |
|        - |  448 | `			/* Append dir seprator */` |
|        2 |  449 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|        1 |  450 | `		}` |
|        - |  451 | `		/* Append path */` |
|       12 |  452 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        7 |  453 | `	}` |
|       10 |  454 | `	return PH7_OK;` |
|        2 |  455 | `}` |
|        - |  456 | `/*` |
|        - |  457 | ` * string\|false set_include_path(string $include_path)` |
|        - |  458 | ` *  Sets the include_path configuration option for the duration of the script,` |
|        - |  459 | ` *  returning the OLD value (php contract).` |
|        - |  460 | ` */` |
|        6 |  461 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  462 | `{` |
|        7 |  463 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  464 | `	SyString *aEntry;` |
|        - |  465 | `	const char *zNew, *z, *zEnd;` |
|        - |  466 | `	int dir_sep, nLen;` |
|        - |  467 | `	sxu32 n;` |
|        - |  468 | `#ifdef __WINNT__` |
|        1 |  469 | `	dir_sep = ';';` |
|        - |  470 | `#else` |
|        6 |  471 | `	dir_sep = ':';` |
|        - |  472 | `#endif` |
|        - |  473 | `	/* Build the OLD include_path first: it is this call's return value */` |
|        7 |  474 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       15 |  475 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        9 |  476 | `		if( n > 0 ){ ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char)); }` |
|        9 |  477 | `		ph7_result_string(pCtx,aEntry[n].zString,(int)aEntry[n].nByte);` |
|        5 |  478 | `	}` |
|        7 |  479 | `	if( nArg < 1 ){` |
|      ! 0 |  480 | `		return PH7_OK;` |
|        - |  481 | `	}` |
|        - |  482 | `	/* Replace the path set with the separated segments of the new value.` |
|        - |  483 | `	 * Segments are duped into the VM allocator (reclaimed at VM teardown) so` |
|        - |  484 | `	 * the SyString entries stay valid, mirroring the config-time literals. */` |
|        7 |  485 | `	zNew = ph7_value_to_string(apArg[0],&nLen);` |
|        7 |  486 | `	SySetReset(&pVm->aPaths);` |
|        7 |  487 | `	z = zNew; zEnd = &zNew[nLen];` |
|       15 |  488 | `	while( z < zEnd ){` |
|        9 |  489 | `		const char *zStart = z;` |
|        - |  490 | `		SyString sPath;` |
|       49 |  491 | `		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }` |
|        9 |  492 | `		if( z > zStart ){` |
|        9 |  493 | `			char *zDup = (char *)SyMemBackendDup(&pVm->sAllocator,zStart,(sxu32)(z-zStart));` |
|        9 |  494 | `			if( zDup ){` |
|        9 |  495 | `				SyStringInitFromBuf(&sPath,zDup,(sxu32)(z-zStart));` |
|        - |  496 | `#ifdef __WINNT__` |
|        1 |  497 | `				SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  498 | `#endif` |
|        9 |  499 | `				SyStringTrimTrailingChar(&sPath,'/');` |
|        9 |  500 | `				SyStringFullTrim(&sPath);` |
|        9 |  501 | `				if( sPath.nByte > 0 ){` |
|        9 |  502 | `					SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|        4 |  503 | `				}` |
|        4 |  504 | `			}` |
|        4 |  505 | `		}` |
|        9 |  506 | `		if( z < zEnd ){ z++; } /* skip the separator */` |
|        1 |  507 | `	}` |
|        7 |  508 | `	return PH7_OK;` |
|        4 |  509 | `}` |
|        - |  510 | `/*` |
|        - |  511 | ` * string get_get_included_files(void)` |
|        - |  512 | ` *  Gets the current include_path configuration option.` |
|        - |  513 | ` * Parameter` |
|        - |  514 | ` *  None` |
|        - |  515 | ` * Return` |
|        - |  516 | ` *  Included paths as a string` |
|        - |  517 | ` */` |
|        2 |  518 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  519 | `{` |
|        3 |  520 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  521 | `	ph7_value *pArray,*pWorker;` |
|        - |  522 | `	SyString *pEntry;` |
|        - |  523 | `	int c,d;` |
|        - |  524 | `	/* Create an array and a working value */` |
|        3 |  525 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  526 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  527 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  528 | `		/* Out of memory,return null */` |
|      ! 0 |  529 | `		ph7_result_null(pCtx);` |
|      ! 0 |  530 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  531 | `		SXUNUSED(apArg);` |
|      ! 0 |  532 | `		return PH7_OK;` |
|        - |  533 | `	}` |
|        3 |  534 | `	c = d = '/';` |
|        - |  535 | `#ifdef __WINNT__` |
|        1 |  536 | `	d = '\\';` |
|        - |  537 | `#endif` |
|        - |  538 | `	/* Iterate throw entries */` |
|        3 |  539 | `	SySetResetCursor(pFiles);` |
|        7 |  540 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  541 | `		const char *zBase,*zEnd;` |
|        - |  542 | `		int iLen;` |
|        - |  543 | `		/* reset the string cursor */` |
|        5 |  544 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  545 | `		/* Extract base name */` |
|        5 |  546 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  547 | `		/* Ignore trailing '/' */` |
|        7 |  548 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  549 | `			zEnd--;` |
|      ! 0 |  550 | `		}` |
|        5 |  551 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|       79 |  552 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|       73 |  553 | `			zEnd--;` |
|        1 |  554 | `		}` |
|        5 |  555 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|        5 |  556 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  557 | `		/* Copy entry name */` |
|        5 |  558 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  559 | `		/* Perform the insertion */` |
|        5 |  560 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  561 | `	}` |
|        - |  562 | `	/* All done,return the created array */` |
|        3 |  563 | `	ph7_result_value(pCtx,pArray);` |
|        - |  564 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  565 | `	 * by the engine as soon we return from this foreign` |
|        - |  566 | `	 * function.` |
|        - |  567 | `	 */` |
|        3 |  568 | `	return PH7_OK;` |
|        2 |  569 | `}` |
|        - |  570 | `/*` |
|        - |  571 | ` * include:` |
|        - |  572 | ` * According to the PHP reference manual.` |
|        - |  573 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  574 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  575 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  576 | ` *  include() will finally check in the calling script's own directory` |
|        - |  577 | ` *  and the current working directory before failing. The include()` |
|        - |  578 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  579 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  580 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  581 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  582 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  583 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  584 | ` *  directory to find the requested file.` |
|        - |  585 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  586 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  587 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  588 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  589 | ` */` |
|     9172 |  590 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  591 | `{` |
|        - |  592 | `	SyString sFile;` |
|        - |  593 | `	sxi32 rc;` |
|     9175 |  594 | `	if( nArg < 1 ){` |
|        - |  595 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  596 | `		ph7_result_null(pCtx);` |
|      ! 0 |  597 | `		return SXRET_OK;` |
|        - |  598 | `	}` |
|        - |  599 | `	/* File to include */` |
|     9175 |  600 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9175 |  601 | `	if( sFile.nByte < 1 ){` |
|        - |  602 | `		/* Empty string,return NULL */` |
|      ! 0 |  603 | `		ph7_result_null(pCtx);` |
|      ! 0 |  604 | `		return SXRET_OK;` |
|        - |  605 | `	}` |
|        - |  606 | `	/* Open,compile and execute the desired script */` |
|     9175 |  607 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9175 |  608 | `	if( rc != SXRET_OK ){` |
|        - |  609 | `		/* Emit a warning and return false */` |
|        3 |  610 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  611 | `		ph7_result_bool(pCtx,0);` |
|        1 |  612 | `	}` |
|     9175 |  613 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  614 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 |  615 | `		return PH7_ABORT;` |
|        - |  616 | `	}` |
|     9170 |  617 | `	return SXRET_OK;` |
|     4589 |  618 | `}` |
|        - |  619 | `/*` |
|        - |  620 | ` * include_once:` |
|        - |  621 | ` *  According to the PHP reference manual.` |
|        - |  622 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  623 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  624 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  625 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  626 | ` *   just once.` |
|        - |  627 | ` */` |
|       16 |  628 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  629 | `{` |
|        - |  630 | `	SyString sFile;` |
|        - |  631 | `	sxi32 rc;` |
|       18 |  632 | `	if( nArg < 1 ){` |
|        - |  633 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  634 | `		ph7_result_null(pCtx);` |
|      ! 0 |  635 | `		return SXRET_OK;` |
|        - |  636 | `	}` |
|        - |  637 | `	/* File to include */` |
|       18 |  638 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       18 |  639 | `	if( sFile.nByte < 1 ){` |
|        - |  640 | `		/* Empty string,return NULL */` |
|      ! 0 |  641 | `		ph7_result_null(pCtx);` |
|      ! 0 |  642 | `		return SXRET_OK;` |
|        - |  643 | `	}` |
|        - |  644 | `	/* Open,compile and execute the desired script */` |
|       18 |  645 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       18 |  646 | `	if( rc == SXERR_EXISTS ){` |
|        - |  647 | `		/* File already included,return TRUE */` |
|       12 |  648 | `		ph7_result_bool(pCtx,1);` |
|       12 |  649 | `		return SXRET_OK;` |
|        - |  650 | `	}` |
|        8 |  651 | `	if( rc != SXRET_OK ){` |
|        - |  652 | `		/* Emit a warning and return false */` |
|      ! 0 |  653 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  654 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  655 | ` 	}` |
|        8 |  656 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  657 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  658 | `		return PH7_ABORT;` |
|        - |  659 | `	}` |
|        8 |  660 | `	return SXRET_OK;` |
|       10 |  661 | `}` |
|        - |  662 | `/*` |
|        - |  663 | ` * require.` |
|        - |  664 | ` *  According to the PHP reference manual.` |
|        - |  665 | ` *   require() is identical to include() except upon failure it will` |
|        - |  666 | ` *   also produce a fatal level error.` |
|        - |  667 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  668 | ` *   emits a warning  which allows the script to continue.` |
|        - |  669 | ` */` |
|       14 |  670 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  671 | `{` |
|        - |  672 | `	SyString sFile;` |
|        - |  673 | `	sxi32 rc;` |
|       17 |  674 | `	if( nArg < 1 ){` |
|        - |  675 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  676 | `		ph7_result_null(pCtx);` |
|      ! 0 |  677 | `		return SXRET_OK;` |
|        - |  678 | `	}` |
|        - |  679 | `	/* File to include */` |
|       17 |  680 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       17 |  681 | `	if( sFile.nByte < 1 ){` |
|        - |  682 | `		/* Empty string,return NULL */` |
|      ! 0 |  683 | `		ph7_result_null(pCtx);` |
|      ! 0 |  684 | `		return SXRET_OK;` |
|        - |  685 | `	}` |
|        - |  686 | `	/* Open,compile and execute the desired script */` |
|       17 |  687 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|       17 |  688 | `	if( rc != SXRET_OK ){` |
|        - |  689 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  690 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  691 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  692 | `		return PH7_ABORT;` |
|        - |  693 | `	}` |
|       17 |  694 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  695 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  696 | `		return PH7_ABORT;` |
|        - |  697 | `	}` |
|       17 |  698 | `	return SXRET_OK;` |
|       10 |  699 | `}` |
|        - |  700 | `/*` |
|        - |  701 | ` * require_once:` |
|        - |  702 | ` *  According to the PHP reference manual.` |
|        - |  703 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  704 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  705 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  706 | ` *   and how it differs from its non _once siblings.` |
|        - |  707 | ` */` |
|        6 |  708 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  709 | `{` |
|        - |  710 | `	SyString sFile;` |
|        - |  711 | `	sxi32 rc;` |
|        8 |  712 | `	if( nArg < 1 ){` |
|        - |  713 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  714 | `		ph7_result_null(pCtx);` |
|      ! 0 |  715 | `		return SXRET_OK;` |
|        - |  716 | `	}` |
|        - |  717 | `	/* File to include */` |
|        8 |  718 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 |  719 | `	if( sFile.nByte < 1 ){` |
|        - |  720 | `		/* Empty string,return NULL */` |
|      ! 0 |  721 | `		ph7_result_null(pCtx);` |
|      ! 0 |  722 | `		return SXRET_OK;` |
|        - |  723 | `	}` |
|        - |  724 | `	/* Open,compile and execute the desired script */` |
|        8 |  725 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        8 |  726 | `	if( rc == SXERR_EXISTS ){` |
|        - |  727 | `		/* File already included,return TRUE */` |
|        3 |  728 | `		ph7_result_bool(pCtx,1);` |
|        3 |  729 | `		return SXRET_OK;` |
|        - |  730 | `	}` |
|        6 |  731 | `	if( rc != SXRET_OK ){` |
|        - |  732 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  733 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  734 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  735 | `		return PH7_ABORT;` |
|        - |  736 | `	}` |
|        6 |  737 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  738 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  739 | `		return PH7_ABORT;` |
|        - |  740 | `	}` |
|        6 |  741 | `	return SXRET_OK;` |
|        5 |  742 | `}` |
|        - |  743 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  744 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  745 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  746 | `/*` |
|        - |  747 | ` * Section:` |
|        - |  748 | ` *  SPL Autoloading functions.` |
|        - |  749 | ` * Status:` |
|        - |  750 | ` *  Stable.` |
|        - |  751 | ` */` |
|        - |  752 | `/*` |
|        - |  753 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - |  754 | ` *  Register given function as __autoload() implementation.` |
|        - |  755 | ` * Parameters` |
|        - |  756 | ` *  callback` |
|        - |  757 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - |  758 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - |  759 | ` *  throw` |
|        - |  760 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - |  761 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - |  762 | ` *  prepend` |
|        - |  763 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - |  764 | ` *   autoload stack instead of appending it.` |
|        - |  765 | ` * Return` |
|        - |  766 | ` *  TRUE on success, FALSE on failure.` |
|        - |  767 | ` */` |
|       46 |  768 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  769 | `{` |
|        - |  770 | `	VmAutoloadCB sEntry;` |
|       51 |  771 | `	ph7_vm *pVm = pCtx->pVm;` |
|       51 |  772 | `	int iPrepend = 0;` |
|        - |  773 | `	sxu32 n;` |
|       51 |  774 | `	if( nArg < 1 ){` |
|        - |  775 | `		/* No callback provided — register default spl_autoload.` |
|        - |  776 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - |  777 | `		/* Check for duplicates first */` |
|        9 |  778 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 |  779 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        4 |  780 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 |  781 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 |  782 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 |  783 | `				ph7_result_bool(pCtx,1);` |
|        5 |  784 | `				return SXRET_OK;` |
|        - |  785 | `			}` |
|      ! 0 |  786 | `		}` |
|        5 |  787 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 |  788 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 |  789 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 |  790 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 |  791 | `		ph7_result_bool(pCtx,1);` |
|        5 |  792 | `		return SXRET_OK;` |
|        - |  793 | `	}` |
|        - |  794 | `	/* Validate that the callback is callable */` |
|       43 |  795 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 |  796 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 |  797 | `		if( nArg >= 2 ){` |
|      ! 0 |  798 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  799 | `		}` |
|      ! 0 |  800 | `		if( iThrow ){` |
|      ! 0 |  801 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - |  802 | `				"Argument is not callable");` |
|      ! 0 |  803 | `		}` |
|      ! 0 |  804 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  805 | `		return SXRET_OK;` |
|        - |  806 | `	}` |
|        - |  807 | `	/* Check for duplicates */` |
|       61 |  808 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 |  809 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 |  810 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  811 | `			/* Already registered */` |
|      ! 0 |  812 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  813 | `			return SXRET_OK;` |
|        - |  814 | `		}` |
|       11 |  815 | `	}` |
|        - |  816 | `	/* Check prepend flag */` |
|       43 |  817 | `	if( nArg >= 3 ){` |
|        3 |  818 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 |  819 | `	}` |
|        - |  820 | `	/* Store the callback */` |
|       43 |  821 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       43 |  822 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       43 |  823 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       44 |  824 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - |  825 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - |  826 | `		 * We do this by appending first, then rotating the array. */` |
|        3 |  827 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - |  828 | `		VmAutoloadCB *aBase;` |
|        3 |  829 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  830 | `		/* Rotate: move last entry to front */` |
|        3 |  831 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 |  832 | `		if( aBase ){` |
|        - |  833 | `			VmAutoloadCB sTemp;` |
|        - |  834 | `			sxu32 i;` |
|        3 |  835 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 |  836 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 |  837 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 |  838 | `			}` |
|        3 |  839 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 |  840 | `		}` |
|        2 |  841 | `	}else{` |
|       41 |  842 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  843 | `	}` |
|       43 |  844 | `	ph7_result_bool(pCtx,1);` |
|       43 |  845 | `	return SXRET_OK;` |
|       28 |  846 | `}` |
|        - |  847 | `/*` |
|        - |  848 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - |  849 | ` *  Unregister a given function as __autoload() implementation.` |
|        - |  850 | ` * Parameters` |
|        - |  851 | ` *  callback` |
|        - |  852 | ` *   The autoload function being unregistered.` |
|        - |  853 | ` * Return` |
|        - |  854 | ` *  TRUE on success, FALSE on failure.` |
|        - |  855 | ` */` |
|       32 |  856 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  857 | `{` |
|       37 |  858 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  859 | `	sxu32 n,nEntry;` |
|       37 |  860 | `	if( nArg < 1 ){` |
|      ! 0 |  861 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  862 | `		return SXRET_OK;` |
|        - |  863 | `	}` |
|       37 |  864 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       41 |  865 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       39 |  866 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       39 |  867 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  868 | `			/* Found — remove by shifting remaining entries down */` |
|       35 |  869 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - |  870 | `			sxu32 i;` |
|       35 |  871 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       49 |  872 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 |  873 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 |  874 | `			}` |
|        - |  875 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       35 |  876 | `			SySetPop(&pVm->aAutoload);` |
|       35 |  877 | `			ph7_result_bool(pCtx,1);` |
|       35 |  878 | `			return SXRET_OK;` |
|        - |  879 | `		}` |
|        3 |  880 | `	}` |
|        3 |  881 | `	ph7_result_bool(pCtx,0);` |
|        3 |  882 | `	return SXRET_OK;` |
|       21 |  883 | `}` |
|        - |  884 | `/*` |
|        - |  885 | ` * array spl_autoload_functions(void)` |
|        - |  886 | ` *  Return all registered __autoload() functions.` |
|        - |  887 | ` * Return` |
|        - |  888 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - |  889 | ` *  an empty array is returned.` |
|        - |  890 | ` */` |
|       20 |  891 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  892 | `{` |
|       21 |  893 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  894 | `	ph7_value *pArray;` |
|        - |  895 | `	sxu32 n,nEntry;` |
|       10 |  896 | `	SXUNUSED(nArg);` |
|       10 |  897 | `	SXUNUSED(apArg);` |
|       21 |  898 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 |  899 | `	if( pArray == 0 ){` |
|      ! 0 |  900 | `		ph7_result_null(pCtx);` |
|      ! 0 |  901 | `		return SXRET_OK;` |
|        - |  902 | `	}` |
|       21 |  903 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 |  904 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 |  905 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 |  906 | `		if( pEntry ){` |
|       15 |  907 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 |  908 | `		}` |
|        8 |  909 | `	}` |
|       21 |  910 | `	ph7_result_value(pCtx,pArray);` |
|       21 |  911 | `	return SXRET_OK;` |
|       11 |  912 | `}` |
|        - |  913 | `/*` |
|        - |  914 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - |  915 | ` *  Default implementation of __autoload().` |
|        - |  916 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - |  917 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - |  918 | ` * Parameters` |
|        - |  919 | ` *  class` |
|        - |  920 | ` *   The class name being searched.` |
|        - |  921 | ` *  file_extensions` |
|        - |  922 | ` *   Comma-separated list of file extensions to try.` |
|        - |  923 | ` */` |
|        2 |  924 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  925 | `{` |
|        - |  926 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - |  927 | `	SyBlob sPath;` |
|        - |  928 | `	int nClass;` |
|        - |  929 | `	sxi32 rc;` |
|        3 |  930 | `	if( nArg < 1 ){` |
|      ! 0 |  931 | `		return SXRET_OK;` |
|        - |  932 | `	}` |
|        3 |  933 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 |  934 | `	if( nClass < 1 ){` |
|      ! 0 |  935 | `		return SXRET_OK;` |
|        - |  936 | `	}` |
|        - |  937 | `	/* Default extensions */` |
|        3 |  938 | `	zExt = ".php,.inc";` |
|        3 |  939 | `	if( nArg >= 2 ){` |
|        - |  940 | `		int nExt;` |
|      ! 0 |  941 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 |  942 | `		if( nExt < 1 ){` |
|      ! 0 |  943 | `			zExt = ".php,.inc";` |
|      ! 0 |  944 | `		}` |
|      ! 0 |  945 | `	}` |
|        3 |  946 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - |  947 | `	/* Iterate over comma-separated extensions */` |
|        3 |  948 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 |  949 | `	zCur = zExt;` |
|        7 |  950 | `	while( zCur < zEnd ){` |
|        - |  951 | `		const char *zComma;` |
|        - |  952 | `		SyString sFile;` |
|        - |  953 | `		int i;` |
|        - |  954 | `		/* Find next comma or end */` |
|        5 |  955 | `		zComma = zCur;` |
|       21 |  956 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 |  957 | `			zComma++;` |
|        1 |  958 | `		}` |
|        - |  959 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 |  960 | `		SyBlobReset(&sPath);` |
|       69 |  961 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 |  962 | `			char c = zClass[i];` |
|       65 |  963 | `			if( c == '\\' ){` |
|      ! 0 |  964 | `				c = '/';` |
|       65 |  965 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 |  966 | `				c = c + ('a' - 'A');` |
|        6 |  967 | `			}` |
|       65 |  968 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 |  969 | `		}` |
|        - |  970 | `		/* Append extension */` |
|        5 |  971 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - |  972 | `		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,` |
|        - |  973 | `		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a` |
|        - |  974 | `		 * terminator, so without this the strlen reads past the buffer (a` |
|        - |  975 | `		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL` |
|        - |  976 | `		 * is not counted in SyBlobLength(), so the SyString length stays correct. */` |
|        5 |  977 | `		SyBlobNullAppend(&sPath);` |
|        - |  978 | `		/* Try to include the file */` |
|        5 |  979 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 |  980 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 |  981 | `		if( rc == SXRET_OK ){` |
|        - |  982 | `			/* File included successfully */` |
|      ! 0 |  983 | `			SyBlobRelease(&sPath);` |
|      ! 0 |  984 | `			return SXRET_OK;` |
|        - |  985 | `		}` |
|        - |  986 | `		/* Move past the comma */` |
|        5 |  987 | `		zCur = zComma;` |
|        5 |  988 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 |  989 | `			zCur++;` |
|        1 |  990 | `		}` |
|        1 |  991 | `	}` |
|        3 |  992 | `	SyBlobRelease(&sPath);` |
|        3 |  993 | `	return SXRET_OK;` |
|        2 |  994 | `}` |
|        - |  995 | `/* Table of built-in VM functions. */` |
|        - |  996 | `/*` |
|        - |  997 | ` * Hidden packing trampoline for __call / __callStatic (band A #3b).` |
|        - |  998 | ` * OP_MEMBER, on a missing method whose class declares the magic handler,` |
|        - |  999 | ` * stashes {receiver, class, original name} on the VM and redirects the` |
|        - | 1000 | ` * callee name to this host function; the normal OP_CALL machinery then` |
|        - | 1001 | ` * collects the ORIGINAL argument list (incl. spreads) and hands it here,` |
|        - | 1002 | ` * which packs it into a php array and invokes` |
|        - | 1003 | ` *   $recv->__call($name, $args)   /   Class::__callStatic($name, $args)` |
|        - | 1004 | ` * returning the handler's value as the call's result. A throw propagates` |
|        - | 1005 | ` * via the returned status (and the boundary rail). Like the __gen_* /` |
|        - | 1006 | ` * __reflect_* thunks, this is a PHL-internal global — calling it directly` |
|        - | 1007 | ` * yields NULL (documented engine-specific surface).` |
|        - | 1008 | ` */` |
|       10 | 1009 | `PH7_PRIVATE int vm_builtin_magic_call(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1010 | `{` |
|       11 | 1011 | `	ph7_vm *pVm = pCtx->pVm;` |
|       11 | 1012 | `	ph7_class_instance *pRecv = pVm->pMagicCallThis;` |
|       11 | 1013 | `	ph7_class *pClass = pVm->pMagicCallClass;` |
|       11 | 1014 | `	ph7_class_method *pMeth = 0;` |
|        - | 1015 | `	ph7_hashmap *pMap;` |
|        - | 1016 | `	ph7_value sNameVal,sArgsVal,sResult;` |
|        - | 1017 | `	ph7_value *apCall[2];` |
|        - | 1018 | `	SyString sMethName;` |
|        - | 1019 | `	sxi32 rc;` |
|        - | 1020 | `	int i;` |
|        - | 1021 | `	/* Consume the pending dispatch (one-shot) */` |
|       11 | 1022 | `	pVm->pMagicCallThis = 0;` |
|       11 | 1023 | `	pVm->pMagicCallClass = 0;` |
|       11 | 1024 | `	if( pClass == 0 ){` |
|        - | 1025 | `		/* Not a magic dispatch (direct user invocation): no-op */` |
|      ! 0 | 1026 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1027 | `		return PH7_OK;` |
|        - | 1028 | `	}` |
|       16 | 1029 | `	pMeth = PH7_ClassExtractMethod(pClass,` |
|        5 | 1030 | `		pRecv ? "__call" : "__callStatic",` |
|        5 | 1031 | `		pRecv ? sizeof("__call")-1 : sizeof("__callStatic")-1);` |
|       11 | 1032 | `	if( pMeth == 0 ){` |
|        - | 1033 | `		/* Unreachable: OP_MEMBER verified the handler exists */` |
|      ! 0 | 1034 | `		if( pRecv ){` |
|      ! 0 | 1035 | `			PH7_ClassInstanceUnref(pRecv);` |
|      ! 0 | 1036 | `		}` |
|      ! 0 | 1037 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1038 | `		return PH7_OK;` |
|        - | 1039 | `	}` |
|       11 | 1040 | `	pMap = PH7_NewHashmap(pVm,0,0);` |
|       11 | 1041 | `	if( pMap == 0 ){` |
|      ! 0 | 1042 | `		if( pRecv ){` |
|      ! 0 | 1043 | `			PH7_ClassInstanceUnref(pRecv);` |
|      ! 0 | 1044 | `		}` |
|      ! 0 | 1045 | `		PH7_VmMemoryError(pVm);` |
|      ! 0 | 1046 | `		return PH7_ABORT;` |
|        - | 1047 | `	}` |
|       25 | 1048 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       15 | 1049 | `		PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        8 | 1050 | `	}` |
|       11 | 1051 | `	SyStringInitFromBuf(&sMethName,SyBlobData(&pVm->sMagicCallName),SyBlobLength(&pVm->sMagicCallName));` |
|       11 | 1052 | `	PH7_MemObjInitFromString(pVm,&sNameVal,&sMethName);` |
|       11 | 1053 | `	sNameVal.nIdx = SXU32_HIGH;` |
|       11 | 1054 | `	PH7_MemObjInitFromArray(pVm,&sArgsVal,pMap);` |
|       11 | 1055 | `	sArgsVal.nIdx = SXU32_HIGH;` |
|       11 | 1056 | `	PH7_MemObjInit(pVm,&sResult);` |
|       11 | 1057 | `	apCall[0] = &sNameVal;` |
|       11 | 1058 | `	apCall[1] = &sArgsVal;` |
|       11 | 1059 | `	rc = PH7_VmCallClassMethod(pVm,pRecv,pMeth,&sResult,2,apCall);` |
|       11 | 1060 | `	if( rc == SXRET_OK ){` |
|        9 | 1061 | `		ph7_result_value(pCtx,&sResult);` |
|        4 | 1062 | `	}` |
|       11 | 1063 | `	PH7_MemObjRelease(&sResult);` |
|       11 | 1064 | `	PH7_MemObjRelease(&sNameVal);` |
|       11 | 1065 | `	PH7_MemObjRelease(&sArgsVal); /* drops the packed array */` |
|       11 | 1066 | `	if( pRecv ){` |
|        9 | 1067 | `		PH7_ClassInstanceUnref(pRecv);` |
|        4 | 1068 | `	}` |
|       11 | 1069 | `	SyBlobReset(&pVm->sMagicCallName);` |
|       11 | 1070 | `	return (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) ? rc : PH7_OK;` |
|        6 | 1071 | `}` |
|        - | 1072 |  |
