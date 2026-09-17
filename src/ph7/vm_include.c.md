# src/ph7/vm_include.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 442/527 lines (83.87%)

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
|    91012 |   22 | `PH7_PRIVATE sxi32 VmEvalChunk(` |
|        - |   23 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |   24 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |   25 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |   26 | `	int iFlags,         /* Compile flag */` |
|        - |   27 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |   28 | `	)` |
|        5 |   29 | `{` |
|        - |   30 | `	SySet *pByteCode,aByteCode;` |
|        - |   31 | `	SyBlob sSavedNs;` |
|    91017 |   32 | `	ProcConsumer xErr = 0;` |
|    91017 |   33 | `	void *pErrData = 0;` |
|        - |   34 | `	ph7_gen_state sSavedGen;` |
|        - |   35 | `	int bNested;` |
|    91017 |   36 | `	sxi32 rcThrow = SXRET_OK; /* status of a ParseError raised for a failed eval() compile */` |
|        - |   37 | `	/* Initialize bytecode container */` |
|    91017 |   38 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    91017 |   39 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |   40 | `	/* Reset the code generator */` |
|    91017 |   41 | `	if( bTrueReturn ){` |
|        - |   42 | `		/* Included file,log compile-time errors */` |
|     9056 |   43 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9056 |   44 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4526 |   45 | `	}` |
|        - |   46 | `	/* A non-zero cursor means an OUTER compile is in flight — this eval/include` |
|        - |   47 | `	 * was reached from inside it (an autoload fired while resolving a base class).` |
|        - |   48 | `	 * Wiping the generator would corrupt that outer parse, so snapshot it and hand` |
|        - |   49 | `	 * the nested unit a fresh one; a plain runtime eval/include just resets. */` |
|    91017 |   50 | `	bNested = (pVm->sCodeGen.pIn != 0);` |
|    91017 |   51 | `	if( bNested ){` |
|        5 |   52 | `		PH7_CompilerSaveState(pVm,&sSavedGen,xErr,pErrData);` |
|        3 |   53 | `	}else{` |
|    91013 |   54 | `		PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |   55 | `	}` |
|        - |   56 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |   57 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |   58 | `	 * the caller's namespace is restored. */` |
|    91017 |   59 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    91017 |   60 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    91017 |   61 | `	if( bTrueReturn ){` |
|        - |   62 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9056 |   63 | `		SyBlobReset(&pVm->sNamespace);` |
|     4526 |   64 | `	}` |
|        - |   65 | `	/* Swap bytecode container */` |
|    91017 |   66 | `	pByteCode = pVm->pByteContainer;` |
|    91017 |   67 | `	pVm->pByteContainer = &aByteCode;` |
|        - |   68 | `	/* Compile the chunk */` |
|    91017 |   69 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|   136522 |   70 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |   71 | `		/* Compilation error. php makes this a CATCHABLE ParseError for eval()` |
|        - |   72 | `		 * ("syntax error, unexpected ..."), where PHL merely returned false —` |
|        - |   73 | ``		 * so `eval('bad syntax')` silently produced a value instead of throwing.`` |
|        - |   74 | `		 * include/require keep the false return: their parse error is a printed` |
|        - |   75 | `		 * fatal, not an exception. */` |
|        7 |   76 | `		if( pCtx && !bTrueReturn ){` |
|        5 |   77 | `			SyBlob *pErr = &pVm->sCodeGen.sErrBuf;` |
|        5 |   78 | `			ph7_result_bool(pCtx,0);` |
|        5 |   79 | `			if( SyBlobLength(pErr) > 0 ){` |
|        7 |   80 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","%.*s",` |
|        4 |   81 | `					(int)SyBlobLength(pErr),(const char *)SyBlobData(pErr));` |
|        3 |   82 | `			}else{` |
|      ! 0 |   83 | `				rcThrow = PH7_VmThrowException(pCtx,"ParseError","syntax error");` |
|        1 |   84 | `			}` |
|        2 |   85 | `		}else if( pCtx ){` |
|      ! 0 |   86 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |   87 | `		}` |
|        3 |   88 | `	}else{` |
|        - |   89 | `		/* Mount any newly defined classes. Skipped while the VM is still` |
|        - |   90 | `		 * initializing (builtin chunks): mounting evaluates class-constant/` |
|        - |   91 | `		 * static initializers and installs reference-table entries, and the` |
|        - |   92 | `		 * runtime structures those need (apRefObj, the per-exec object pool` |
|        - |   93 | `		 * baseline) do not exist before PH7_VmMakeReady — which mounts every` |
|        - |   94 | `		 * class unconditionally anyway. */` |
|        - |   95 | `		SyHashEntry *pEntry;` |
|        - |   96 | `		ph7_class *pClass;` |
|        - |   97 | `		ph7_value sResult; /* Return value */` |
|        - |   98 | `		sxi32 rc;` |
|    91013 |   99 | `		if( pVm->nMagic != PH7_VM_INIT ){` |
|     9155 |  100 | `			SyHashResetLoopCursor(&pVm->hClass);` |
|  2425860 |  101 | `			while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|  2412137 |  102 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  103 | `				/* Only mount classes that haven't been mounted yet */` |
|  2412137 |  104 | `				if( !pClass->bMounted ){` |
|   242853 |  105 | `					rc = VmMountUserClass(pVm,pClass);` |
|   242853 |  106 | `					if( rc != SXRET_OK ){` |
|        - |  107 | `						/* Mount failure (likely memory error) */` |
|        3 |  108 | `						if( pCtx ){` |
|        3 |  109 | `							ph7_result_bool(pCtx,0);` |
|        1 |  110 | `						}` |
|        3 |  111 | `						goto Cleanup;` |
|        - |  112 | `					}` |
|   121423 |  113 | `				}` |
|        5 |  114 | `			}` |
|     4574 |  115 | `		}` |
|    91011 |  116 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  117 | `			/* Out of memory */` |
|      ! 0 |  118 | `			if( pCtx ){` |
|      ! 0 |  119 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  120 | `			}` |
|      ! 0 |  121 | `			goto Cleanup;` |
|        - |  122 | `		}` |
|    91011 |  123 | `		if( bTrueReturn ){` |
|        - |  124 | `			/* Assume a boolean true return value */` |
|     9056 |  125 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4530 |  126 | `		}else{` |
|        - |  127 | `			/* Assume a null return value */` |
|    81959 |  128 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  129 | `		}` |
|        - |  130 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here` |
|        - |  131 | `		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by` |
|        - |  132 | `		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the` |
|        - |  133 | `		 * native-nesting fatal instead of overflowing the C stack. The PHP` |
|        - |  134 | `		 * call-depth cap is OP_CALL-only (BYTECODE.md stage 5). */` |
|    91011 |  135 | `		VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    91011 |  136 | `		if( pCtx ){` |
|        - |  137 | `			/* Set the execution result */` |
|     9153 |  138 | `			ph7_result_value(pCtx,&sResult);` |
|     4574 |  139 | `		}` |
|    91011 |  140 | `		PH7_MemObjRelease(&sResult);` |
|        - |  141 | `	}` |
|    45506 |  142 | `Cleanup:` |
|        - |  143 | `	/* Cleanup the mess left behind */` |
|    91017 |  144 | `	pVm->pByteContainer = pByteCode;` |
|    91017 |  145 | `	SySetRelease(&aByteCode);` |
|        - |  146 | `	/* Restore caller's namespace state */` |
|    91017 |  147 | `	SyBlobReset(&pVm->sNamespace);` |
|    91017 |  148 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    91017 |  149 | `	SyBlobRelease(&sSavedNs);` |
|        - |  150 | `	/* Restore the outer compile's generator state if this was a nested unit. */` |
|    91017 |  151 | `	if( bNested ){` |
|        5 |  152 | `		PH7_CompilerRestoreState(pVm,&sSavedGen);` |
|        2 |  153 | `	}` |
|        - |  154 | `	/* A ParseError raised above must reach the caller so the VM unwinds the rest` |
|        - |  155 | ``	 * of the statement; returning OK left `eval('bad'); echo 'x';` running the`` |
|        - |  156 | `	 * echo even though php had already thrown. */` |
|    91017 |  157 | `	return rcThrow;` |
|        5 |  158 | `}` |
|        - |  159 | `/*` |
|        - |  160 | ` * Compile an embedded builtin PHP chunk into the VM. Thin exported wrapper` |
|        - |  161 | ` * around the static VmEvalChunk for builtin libraries that live outside` |
|        - |  162 | ` * this file (e.g. the Reflection classes in vm_builtin_reflection.c).` |
|        - |  163 | ` */` |
|    74062 |  164 | `PH7_PRIVATE sxi32 PH7_VmEvalBuiltinChunk(ph7_vm *pVm,const char *zSrc,sxu32 nLen)` |
|        5 |  165 | `{` |
|        - |  166 | `	SyString sChunk;` |
|    74067 |  167 | `	SyStringInitFromBuf(&sChunk,zSrc,nLen);` |
|    74067 |  168 | `	return VmEvalChunk(&(*pVm),0,&sChunk,PH7_PHP_ONLY,FALSE);` |
|        5 |  169 | `}` |
|        - |  170 | `/*` |
|        - |  171 | ` * value eval(string $code)` |
|        - |  172 | ` *   Evaluate a string as PHP code.` |
|        - |  173 | ` * Parameter` |
|        - |  174 | ` *  code: PHP code to evaluate.` |
|        - |  175 | ` * Return` |
|        - |  176 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  177 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  178 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  179 | ` */` |
|      104 |  180 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  181 | `{` |
|        - |  182 | `	SyString sChunk;    /* Chunk to evaluate */` |
|        - |  183 | `	sxi32 rc;` |
|      109 |  184 | `	if( nArg < 1 ){` |
|        - |  185 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  186 | `		ph7_result_null(pCtx);` |
|      ! 0 |  187 | `		return SXRET_OK;` |
|        - |  188 | `	}` |
|        - |  189 | `	/* Chunk to evaluate */` |
|      109 |  190 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|      109 |  191 | `	if( sChunk.nByte < 1 ){` |
|        - |  192 | `		/* php: eval('') compiles nothing and yields FALSE (a whitespace-only` |
|        - |  193 | `		 * chunk still compiles, and yields NULL through the normal path). */` |
|        3 |  194 | `		ph7_result_bool(pCtx,0);` |
|        3 |  195 | `		return SXRET_OK;` |
|        - |  196 | `	}` |
|        - |  197 | `	/* Eval the chunk */` |
|      107 |  198 | `	rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|      107 |  199 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  200 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       71 |  201 | `		return PH7_ABORT;` |
|        - |  202 | `	}` |
|        - |  203 | `	/* Propagate a ParseError from a failed compile (php unwinds; PHL used to` |
|        - |  204 | `	 * keep executing the statement that contained the eval). */` |
|       38 |  205 | `	return rc;` |
|       57 |  206 | `}` |
|        - |  207 | `/*` |
|        - |  208 | ` * Check if a file path is already included.` |
|        - |  209 | ` */` |
|     9064 |  210 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        4 |  211 | `{` |
|        - |  212 | `	SyString *aEntries;` |
|        - |  213 | `	sxu32 n;` |
|     9068 |  214 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  215 | `	/* Perform a linear search */` |
| 20430484 |  216 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 20421432 |  217 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  218 | `			/* Already included */` |
|       16 |  219 | `			return TRUE;` |
|        - |  220 | `		}` |
| 10210710 |  221 | `	}` |
|     9054 |  222 | `	return FALSE;` |
|     4536 |  223 | `}` |
|        - |  224 | `/*` |
|        - |  225 | ` * Push a file path in the appropriate VM container.` |
|        - |  226 | ` */` |
|    12962 |  227 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 |  228 | `{` |
|        - |  229 | `	SyString sPath;` |
|        - |  230 | `	char *zDup;` |
|        - |  231 | `	sxi32 rc;` |
|    12967 |  232 | `	if( nLen < 0 ){` |
|     3903 |  233 | `		nLen = SyStrlen(zPath);` |
|     1949 |  234 | `	}` |
|        - |  235 | `	/* Duplicate the file path first */` |
|    12967 |  236 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    12967 |  237 | `	if( zDup == 0 ){` |
|      ! 0 |  238 | `		return SXERR_MEM;` |
|        - |  239 | `	}` |
|        - |  240 | `#ifdef __UNIXES__` |
|        - |  241 | `	/* php records the REALPATH'd absolute path for __FILE__/__DIR__/getFileName` |
|        - |  242 | `	 * and include-once dedup: realpath() resolves '.', '..' and symlinks (e.g.` |
|        - |  243 | `	 * macOS /tmp -> /private/tmp). It needs an existing file, so on failure keep` |
|        - |  244 | `	 * the raw path (eval'd code, php:///data:// wrappers, a missing include that` |
|        - |  245 | `	 * errors elsewhere). */` |
|        - |  246 | `	{` |
|    12962 |  247 | `		char *zReal = realpath(zDup,0); /* POSIX: malloc'd result */` |
|    12962 |  248 | `		if( zReal ){` |
|    12926 |  249 | `			sxu32 nReal = SyStrlen(zReal);` |
|    12926 |  250 | `			char *zRealDup = SyMemBackendStrDup(&pVm->sAllocator,zReal,nReal);` |
|    12926 |  251 | `			free(zReal);` |
|    12926 |  252 | `			if( zRealDup ){` |
|    12926 |  253 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|    12926 |  254 | `				zDup = zRealDup;` |
|    12926 |  255 | `				nLen = (int)nReal;` |
|     6463 |  256 | `			}` |
|     6463 |  257 | `		}` |
|        - |  258 | `	}` |
|        - |  259 | `#endif` |
|        - |  260 | `#ifdef __WINNT__` |
|        - |  261 | `	/* Windows counterpart of the realpath() above: canonicalize to an absolute` |
|        - |  262 | `	 * path (resolves '.'/'..' , '/' -> '\'), case-preserved to match php — NOT` |
|        - |  263 | `	 * lowercased as the legacy PH7 code did. Like the POSIX branch, keep the raw` |
|        - |  264 | `	 * path when it does not name an existing file (the "Command line code" marker,` |
|        - |  265 | `	 * eval'd code, stream wrappers). _fullpath() is lexical, so pair it with a VFS` |
|        - |  266 | `	 * existence check. */` |
|        - |  267 | `	{` |
|        5 |  268 | `		char *zFull = _fullpath(0,zDup,0); /* MSVC CRT: malloc'd absolute path */` |
|        5 |  269 | `		if( zFull ){` |
|        5 |  270 | `			if( pVm->pEngine->pVfs && pVm->pEngine->pVfs->xFileExists && pVm->pEngine->pVfs->xFileExists(zFull) == PH7_OK ){` |
|        5 |  271 | `				sxu32 nFull = SyStrlen(zFull);` |
|        5 |  272 | `				char *zFullDup = SyMemBackendStrDup(&pVm->sAllocator,zFull,nFull);` |
|        5 |  273 | `				if( zFullDup ){` |
|        5 |  274 | `					SyMemBackendFree(&pVm->sAllocator,zDup);` |
|        5 |  275 | `					zDup = zFullDup;` |
|        5 |  276 | `					nLen = (int)nFull;` |
|        - |  277 | `				}` |
|        - |  278 | `			}` |
|        5 |  279 | `			free(zFull);` |
|        - |  280 | `		}` |
|        - |  281 | `	}` |
|        - |  282 | `#endif` |
|        - |  283 | `	/* Install the file path */` |
|    12967 |  284 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    12967 |  285 | `	if( !bMain ){` |
|     9068 |  286 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  287 | `			/* Already included */` |
|       16 |  288 | `			*pNew = 0;` |
|        9 |  289 | `		}else{` |
|        - |  290 | `			/* Insert in the corresponding container */` |
|     9054 |  291 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|     9054 |  292 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  293 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  294 | `				return rc;` |
|        - |  295 | `			}` |
|     9054 |  296 | `			*pNew = 1;` |
|        - |  297 | `		}` |
|     4532 |  298 | `	}` |
|    12967 |  299 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    12967 |  300 | `	return SXRET_OK;` |
|     6486 |  301 | `}` |
|        - |  302 | `/*` |
|        - |  303 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  304 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  305 | ` * indicates failure.` |
|        - |  306 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  307 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  308 | ` * operations.` |
|        - |  309 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  310 | ` * this function is a no-op.` |
|        - |  311 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  312 | ` * constructs for more information.` |
|        - |  313 | ` */` |
|     9070 |  314 | `static sxi32 VmExecIncludedFile(` |
|        - |  315 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  316 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  317 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  318 | `	 )` |
|        4 |  319 | `{` |
|        - |  320 | `	sxi32 rc;` |
|        - |  321 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  322 | `	const ph7_io_stream *pStream;` |
|        - |  323 | `	SyBlob sContents;` |
|        - |  324 | `	void *pHandle;` |
|        - |  325 | `	ph7_vm *pVm;` |
|        - |  326 | `	int isNew;` |
|        - |  327 | `	/* Initialize fields */` |
|     9074 |  328 | `	pVm = pCtx->pVm;` |
|     9074 |  329 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9074 |  330 | `	isNew = 0;` |
|        - |  331 | `	/* Extract the associated stream */` |
|     9074 |  332 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  333 | `	/*` |
|        - |  334 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  335 | `	 * in a read-only mode.` |
|        - |  336 | `	 */` |
|     9074 |  337 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9074 |  338 | `	if( pHandle == 0 ){` |
|        8 |  339 | `		return SXERR_IO;` |
|        - |  340 | `	}` |
|     9068 |  341 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9068 |  342 | `	if( IncludeOnce && !isNew ){` |
|        - |  343 | `		/* Already included */` |
|       14 |  344 | `		rc = SXERR_EXISTS;` |
|        8 |  345 | `	}else{` |
|        - |  346 | `		/* Read the whole file contents */` |
|     9056 |  347 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9056 |  348 | `		if( rc == SXRET_OK ){` |
|        - |  349 | `			SyString sScript;` |
|        - |  350 | `			/* Compile and execute the script */` |
|     9056 |  351 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9056 |  352 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4526 |  353 | `		}` |
|        - |  354 | `	}` |
|        - |  355 | `	/* Pop from the set of included file */` |
|     9068 |  356 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  357 | `	/* Close the handle */` |
|     9068 |  358 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  359 | `	/* Release the working buffer */` |
|     9068 |  360 | `	SyBlobRelease(&sContents);` |
|        - |  361 | `#else` |
|        - |  362 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  363 | `	SXUNUSED(pPath);` |
|        - |  364 | `	SXUNUSED(IncludeOnce);` |
|        - |  365 | `	rc = SXERR_IO;` |
|        - |  366 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9068 |  367 | `	return rc;` |
|     4539 |  368 | `}` |
|        - |  369 | `/*` |
|        - |  370 | ` * string get_include_path(void)` |
|        - |  371 | ` *  Gets the current include_path configuration option.` |
|        - |  372 | ` * Parameter` |
|        - |  373 | ` *  None` |
|        - |  374 | ` * Return` |
|        - |  375 | ` *  Included paths as a string` |
|        - |  376 | ` */` |
|        8 |  377 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  378 | `{` |
|       10 |  379 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  380 | `	SyString *aEntry;` |
|        - |  381 | `	int dir_sep;` |
|        - |  382 | `	sxu32 n;` |
|        - |  383 | `#ifdef __WINNT__` |
|        2 |  384 | `	dir_sep = ';';` |
|        - |  385 | `#else` |
|        - |  386 | `	/* Assume UNIX path separator */` |
|        8 |  387 | `	dir_sep = ':';` |
|        - |  388 | `#endif` |
|        4 |  389 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 |  390 | `	SXUNUSED(apArg);` |
|        - |  391 | `	/* Point to the list of import paths */` |
|       10 |  392 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       20 |  393 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|       12 |  394 | `		SyString *pEntry = &aEntry[n];` |
|       12 |  395 | `		if( n > 0 ){` |
|        - |  396 | `			/* Append dir seprator */` |
|        2 |  397 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|        1 |  398 | `		}` |
|        - |  399 | `		/* Append path */` |
|       12 |  400 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        7 |  401 | `	}` |
|       10 |  402 | `	return PH7_OK;` |
|        2 |  403 | `}` |
|        - |  404 | `/*` |
|        - |  405 | ` * string\|false set_include_path(string $include_path)` |
|        - |  406 | ` *  Sets the include_path configuration option for the duration of the script,` |
|        - |  407 | ` *  returning the OLD value (php contract).` |
|        - |  408 | ` */` |
|        6 |  409 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  410 | `{` |
|        7 |  411 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  412 | `	SyString *aEntry;` |
|        - |  413 | `	const char *zNew, *z, *zEnd;` |
|        - |  414 | `	int dir_sep, nLen;` |
|        - |  415 | `	sxu32 n;` |
|        - |  416 | `#ifdef __WINNT__` |
|        1 |  417 | `	dir_sep = ';';` |
|        - |  418 | `#else` |
|        6 |  419 | `	dir_sep = ':';` |
|        - |  420 | `#endif` |
|        - |  421 | `	/* Build the OLD include_path first: it is this call's return value */` |
|        7 |  422 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       15 |  423 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        9 |  424 | `		if( n > 0 ){ ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char)); }` |
|        9 |  425 | `		ph7_result_string(pCtx,aEntry[n].zString,(int)aEntry[n].nByte);` |
|        5 |  426 | `	}` |
|        7 |  427 | `	if( nArg < 1 ){` |
|      ! 0 |  428 | `		return PH7_OK;` |
|        - |  429 | `	}` |
|        - |  430 | `	/* Replace the path set with the separated segments of the new value.` |
|        - |  431 | `	 * Segments are duped into the VM allocator (reclaimed at VM teardown) so` |
|        - |  432 | `	 * the SyString entries stay valid, mirroring the config-time literals. */` |
|        7 |  433 | `	zNew = ph7_value_to_string(apArg[0],&nLen);` |
|        7 |  434 | `	SySetReset(&pVm->aPaths);` |
|        7 |  435 | `	z = zNew; zEnd = &zNew[nLen];` |
|       15 |  436 | `	while( z < zEnd ){` |
|        9 |  437 | `		const char *zStart = z;` |
|        - |  438 | `		SyString sPath;` |
|       49 |  439 | `		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }` |
|        9 |  440 | `		if( z > zStart ){` |
|        9 |  441 | `			char *zDup = (char *)SyMemBackendDup(&pVm->sAllocator,zStart,(sxu32)(z-zStart));` |
|        9 |  442 | `			if( zDup ){` |
|        9 |  443 | `				SyStringInitFromBuf(&sPath,zDup,(sxu32)(z-zStart));` |
|        - |  444 | `#ifdef __WINNT__` |
|        1 |  445 | `				SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  446 | `#endif` |
|        9 |  447 | `				SyStringTrimTrailingChar(&sPath,'/');` |
|        9 |  448 | `				SyStringFullTrim(&sPath);` |
|        9 |  449 | `				if( sPath.nByte > 0 ){` |
|        9 |  450 | `					SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|        4 |  451 | `				}` |
|        4 |  452 | `			}` |
|        4 |  453 | `		}` |
|        9 |  454 | `		if( z < zEnd ){ z++; } /* skip the separator */` |
|        1 |  455 | `	}` |
|        7 |  456 | `	return PH7_OK;` |
|        4 |  457 | `}` |
|        - |  458 | `/*` |
|        - |  459 | ` * string get_get_included_files(void)` |
|        - |  460 | ` *  Gets the current include_path configuration option.` |
|        - |  461 | ` * Parameter` |
|        - |  462 | ` *  None` |
|        - |  463 | ` * Return` |
|        - |  464 | ` *  Included paths as a string` |
|        - |  465 | ` */` |
|        2 |  466 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  467 | `{` |
|        3 |  468 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  469 | `	ph7_value *pArray,*pWorker;` |
|        - |  470 | `	SyString *pEntry;` |
|        - |  471 | `	int c,d;` |
|        - |  472 | `	/* Create an array and a working value */` |
|        3 |  473 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  474 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  475 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  476 | `		/* Out of memory,return null */` |
|      ! 0 |  477 | `		ph7_result_null(pCtx);` |
|      ! 0 |  478 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  479 | `		SXUNUSED(apArg);` |
|      ! 0 |  480 | `		return PH7_OK;` |
|        - |  481 | `	}` |
|        3 |  482 | `	c = d = '/';` |
|        - |  483 | `#ifdef __WINNT__` |
|        1 |  484 | `	d = '\\';` |
|        - |  485 | `#endif` |
|        - |  486 | `	/* Iterate throw entries */` |
|        3 |  487 | `	SySetResetCursor(pFiles);` |
|        7 |  488 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  489 | `		const char *zBase,*zEnd;` |
|        - |  490 | `		int iLen;` |
|        - |  491 | `		/* reset the string cursor */` |
|        5 |  492 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  493 | `		/* Extract base name */` |
|        5 |  494 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  495 | `		/* Ignore trailing '/' */` |
|        7 |  496 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  497 | `			zEnd--;` |
|      ! 0 |  498 | `		}` |
|        5 |  499 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|       79 |  500 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|       73 |  501 | `			zEnd--;` |
|        1 |  502 | `		}` |
|        5 |  503 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|        5 |  504 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  505 | `		/* Copy entry name */` |
|        5 |  506 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  507 | `		/* Perform the insertion */` |
|        5 |  508 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  509 | `	}` |
|        - |  510 | `	/* All done,return the created array */` |
|        3 |  511 | `	ph7_result_value(pCtx,pArray);` |
|        - |  512 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  513 | `	 * by the engine as soon we return from this foreign` |
|        - |  514 | `	 * function.` |
|        - |  515 | `	 */` |
|        3 |  516 | `	return PH7_OK;` |
|        2 |  517 | `}` |
|        - |  518 | `/*` |
|        - |  519 | ` * include:` |
|        - |  520 | ` * According to the PHP reference manual.` |
|        - |  521 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  522 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  523 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  524 | ` *  include() will finally check in the calling script's own directory` |
|        - |  525 | ` *  and the current working directory before failing. The include()` |
|        - |  526 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  527 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  528 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  529 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  530 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  531 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  532 | ` *  directory to find the requested file.` |
|        - |  533 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  534 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  535 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  536 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  537 | ` */` |
|     9030 |  538 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  539 | `{` |
|        - |  540 | `	SyString sFile;` |
|        - |  541 | `	sxi32 rc;` |
|     9033 |  542 | `	if( nArg < 1 ){` |
|        - |  543 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  544 | `		ph7_result_null(pCtx);` |
|      ! 0 |  545 | `		return SXRET_OK;` |
|        - |  546 | `	}` |
|        - |  547 | `	/* File to include */` |
|     9033 |  548 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9033 |  549 | `	if( sFile.nByte < 1 ){` |
|        - |  550 | `		/* Empty string,return NULL */` |
|      ! 0 |  551 | `		ph7_result_null(pCtx);` |
|      ! 0 |  552 | `		return SXRET_OK;` |
|        - |  553 | `	}` |
|        - |  554 | `	/* Open,compile and execute the desired script */` |
|     9033 |  555 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9033 |  556 | `	if( rc != SXRET_OK ){` |
|        - |  557 | `		/* Emit a warning and return false */` |
|        3 |  558 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  559 | `		ph7_result_bool(pCtx,0);` |
|        1 |  560 | `	}` |
|     9033 |  561 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  562 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 |  563 | `		return PH7_ABORT;` |
|        - |  564 | `	}` |
|     9028 |  565 | `	return SXRET_OK;` |
|     4518 |  566 | `}` |
|        - |  567 | `/*` |
|        - |  568 | ` * include_once:` |
|        - |  569 | ` *  According to the PHP reference manual.` |
|        - |  570 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  571 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  572 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  573 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  574 | ` *   just once.` |
|        - |  575 | ` */` |
|       16 |  576 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  577 | `{` |
|        - |  578 | `	SyString sFile;` |
|        - |  579 | `	sxi32 rc;` |
|       18 |  580 | `	if( nArg < 1 ){` |
|        - |  581 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  582 | `		ph7_result_null(pCtx);` |
|      ! 0 |  583 | `		return SXRET_OK;` |
|        - |  584 | `	}` |
|        - |  585 | `	/* File to include */` |
|       18 |  586 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       18 |  587 | `	if( sFile.nByte < 1 ){` |
|        - |  588 | `		/* Empty string,return NULL */` |
|      ! 0 |  589 | `		ph7_result_null(pCtx);` |
|      ! 0 |  590 | `		return SXRET_OK;` |
|        - |  591 | `	}` |
|        - |  592 | `	/* Open,compile and execute the desired script */` |
|       18 |  593 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       18 |  594 | `	if( rc == SXERR_EXISTS ){` |
|        - |  595 | `		/* File already included,return TRUE */` |
|       12 |  596 | `		ph7_result_bool(pCtx,1);` |
|       12 |  597 | `		return SXRET_OK;` |
|        - |  598 | `	}` |
|        8 |  599 | `	if( rc != SXRET_OK ){` |
|        - |  600 | `		/* Emit a warning and return false */` |
|      ! 0 |  601 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  602 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  603 | ` 	}` |
|        8 |  604 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  605 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  606 | `		return PH7_ABORT;` |
|        - |  607 | `	}` |
|        8 |  608 | `	return SXRET_OK;` |
|       10 |  609 | `}` |
|        - |  610 | `/*` |
|        - |  611 | ` * require.` |
|        - |  612 | ` *  According to the PHP reference manual.` |
|        - |  613 | ` *   require() is identical to include() except upon failure it will` |
|        - |  614 | ` *   also produce a fatal level error.` |
|        - |  615 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  616 | ` *   emits a warning  which allows the script to continue.` |
|        - |  617 | ` */` |
|       14 |  618 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  619 | `{` |
|        - |  620 | `	SyString sFile;` |
|        - |  621 | `	sxi32 rc;` |
|       17 |  622 | `	if( nArg < 1 ){` |
|        - |  623 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  624 | `		ph7_result_null(pCtx);` |
|      ! 0 |  625 | `		return SXRET_OK;` |
|        - |  626 | `	}` |
|        - |  627 | `	/* File to include */` |
|       17 |  628 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       17 |  629 | `	if( sFile.nByte < 1 ){` |
|        - |  630 | `		/* Empty string,return NULL */` |
|      ! 0 |  631 | `		ph7_result_null(pCtx);` |
|      ! 0 |  632 | `		return SXRET_OK;` |
|        - |  633 | `	}` |
|        - |  634 | `	/* Open,compile and execute the desired script */` |
|       17 |  635 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|       17 |  636 | `	if( rc != SXRET_OK ){` |
|        - |  637 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  638 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  639 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  640 | `		return PH7_ABORT;` |
|        - |  641 | `	}` |
|       17 |  642 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  643 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  644 | `		return PH7_ABORT;` |
|        - |  645 | `	}` |
|       17 |  646 | `	return SXRET_OK;` |
|       10 |  647 | `}` |
|        - |  648 | `/*` |
|        - |  649 | ` * require_once:` |
|        - |  650 | ` *  According to the PHP reference manual.` |
|        - |  651 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  652 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  653 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  654 | ` *   and how it differs from its non _once siblings.` |
|        - |  655 | ` */` |
|        6 |  656 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  657 | `{` |
|        - |  658 | `	SyString sFile;` |
|        - |  659 | `	sxi32 rc;` |
|        8 |  660 | `	if( nArg < 1 ){` |
|        - |  661 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  662 | `		ph7_result_null(pCtx);` |
|      ! 0 |  663 | `		return SXRET_OK;` |
|        - |  664 | `	}` |
|        - |  665 | `	/* File to include */` |
|        8 |  666 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 |  667 | `	if( sFile.nByte < 1 ){` |
|        - |  668 | `		/* Empty string,return NULL */` |
|      ! 0 |  669 | `		ph7_result_null(pCtx);` |
|      ! 0 |  670 | `		return SXRET_OK;` |
|        - |  671 | `	}` |
|        - |  672 | `	/* Open,compile and execute the desired script */` |
|        8 |  673 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        8 |  674 | `	if( rc == SXERR_EXISTS ){` |
|        - |  675 | `		/* File already included,return TRUE */` |
|        3 |  676 | `		ph7_result_bool(pCtx,1);` |
|        3 |  677 | `		return SXRET_OK;` |
|        - |  678 | `	}` |
|        6 |  679 | `	if( rc != SXRET_OK ){` |
|        - |  680 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  681 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  682 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  683 | `		return PH7_ABORT;` |
|        - |  684 | `	}` |
|        6 |  685 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  686 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  687 | `		return PH7_ABORT;` |
|        - |  688 | `	}` |
|        6 |  689 | `	return SXRET_OK;` |
|        5 |  690 | `}` |
|        - |  691 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  692 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  693 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  694 | `/*` |
|        - |  695 | ` * Section:` |
|        - |  696 | ` *  SPL Autoloading functions.` |
|        - |  697 | ` * Status:` |
|        - |  698 | ` *  Stable.` |
|        - |  699 | ` */` |
|        - |  700 | `/*` |
|        - |  701 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - |  702 | ` *  Register given function as __autoload() implementation.` |
|        - |  703 | ` * Parameters` |
|        - |  704 | ` *  callback` |
|        - |  705 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - |  706 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - |  707 | ` *  throw` |
|        - |  708 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - |  709 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - |  710 | ` *  prepend` |
|        - |  711 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - |  712 | ` *   autoload stack instead of appending it.` |
|        - |  713 | ` * Return` |
|        - |  714 | ` *  TRUE on success, FALSE on failure.` |
|        - |  715 | ` */` |
|       38 |  716 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  717 | `{` |
|        - |  718 | `	VmAutoloadCB sEntry;` |
|       43 |  719 | `	ph7_vm *pVm = pCtx->pVm;` |
|       43 |  720 | `	int iPrepend = 0;` |
|        - |  721 | `	sxu32 n;` |
|       43 |  722 | `	if( nArg < 1 ){` |
|        - |  723 | `		/* No callback provided — register default spl_autoload.` |
|        - |  724 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - |  725 | `		/* Check for duplicates first */` |
|        9 |  726 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 |  727 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        4 |  728 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 |  729 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 |  730 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 |  731 | `				ph7_result_bool(pCtx,1);` |
|        5 |  732 | `				return SXRET_OK;` |
|        - |  733 | `			}` |
|      ! 0 |  734 | `		}` |
|        5 |  735 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 |  736 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 |  737 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 |  738 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 |  739 | `		ph7_result_bool(pCtx,1);` |
|        5 |  740 | `		return SXRET_OK;` |
|        - |  741 | `	}` |
|        - |  742 | `	/* Validate that the callback is callable */` |
|       35 |  743 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 |  744 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 |  745 | `		if( nArg >= 2 ){` |
|      ! 0 |  746 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  747 | `		}` |
|      ! 0 |  748 | `		if( iThrow ){` |
|      ! 0 |  749 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - |  750 | `				"Argument is not callable");` |
|      ! 0 |  751 | `		}` |
|      ! 0 |  752 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  753 | `		return SXRET_OK;` |
|        - |  754 | `	}` |
|        - |  755 | `	/* Check for duplicates */` |
|       53 |  756 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 |  757 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 |  758 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  759 | `			/* Already registered */` |
|      ! 0 |  760 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  761 | `			return SXRET_OK;` |
|        - |  762 | `		}` |
|       11 |  763 | `	}` |
|        - |  764 | `	/* Check prepend flag */` |
|       35 |  765 | `	if( nArg >= 3 ){` |
|        3 |  766 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 |  767 | `	}` |
|        - |  768 | `	/* Store the callback */` |
|       35 |  769 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       35 |  770 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       35 |  771 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       36 |  772 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - |  773 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - |  774 | `		 * We do this by appending first, then rotating the array. */` |
|        3 |  775 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - |  776 | `		VmAutoloadCB *aBase;` |
|        3 |  777 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  778 | `		/* Rotate: move last entry to front */` |
|        3 |  779 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 |  780 | `		if( aBase ){` |
|        - |  781 | `			VmAutoloadCB sTemp;` |
|        - |  782 | `			sxu32 i;` |
|        3 |  783 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 |  784 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 |  785 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 |  786 | `			}` |
|        3 |  787 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 |  788 | `		}` |
|        2 |  789 | `	}else{` |
|       33 |  790 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  791 | `	}` |
|       35 |  792 | `	ph7_result_bool(pCtx,1);` |
|       35 |  793 | `	return SXRET_OK;` |
|       24 |  794 | `}` |
|        - |  795 | `/*` |
|        - |  796 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - |  797 | ` *  Unregister a given function as __autoload() implementation.` |
|        - |  798 | ` * Parameters` |
|        - |  799 | ` *  callback` |
|        - |  800 | ` *   The autoload function being unregistered.` |
|        - |  801 | ` * Return` |
|        - |  802 | ` *  TRUE on success, FALSE on failure.` |
|        - |  803 | ` */` |
|       32 |  804 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  805 | `{` |
|       37 |  806 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  807 | `	sxu32 n,nEntry;` |
|       37 |  808 | `	if( nArg < 1 ){` |
|      ! 0 |  809 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  810 | `		return SXRET_OK;` |
|        - |  811 | `	}` |
|       37 |  812 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       41 |  813 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       39 |  814 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       39 |  815 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  816 | `			/* Found — remove by shifting remaining entries down */` |
|       35 |  817 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - |  818 | `			sxu32 i;` |
|       35 |  819 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       49 |  820 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 |  821 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 |  822 | `			}` |
|        - |  823 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       35 |  824 | `			SySetPop(&pVm->aAutoload);` |
|       35 |  825 | `			ph7_result_bool(pCtx,1);` |
|       35 |  826 | `			return SXRET_OK;` |
|        - |  827 | `		}` |
|        3 |  828 | `	}` |
|        3 |  829 | `	ph7_result_bool(pCtx,0);` |
|        3 |  830 | `	return SXRET_OK;` |
|       21 |  831 | `}` |
|        - |  832 | `/*` |
|        - |  833 | ` * array spl_autoload_functions(void)` |
|        - |  834 | ` *  Return all registered __autoload() functions.` |
|        - |  835 | ` * Return` |
|        - |  836 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - |  837 | ` *  an empty array is returned.` |
|        - |  838 | ` */` |
|       20 |  839 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  840 | `{` |
|       21 |  841 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  842 | `	ph7_value *pArray;` |
|        - |  843 | `	sxu32 n,nEntry;` |
|       10 |  844 | `	SXUNUSED(nArg);` |
|       10 |  845 | `	SXUNUSED(apArg);` |
|       21 |  846 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 |  847 | `	if( pArray == 0 ){` |
|      ! 0 |  848 | `		ph7_result_null(pCtx);` |
|      ! 0 |  849 | `		return SXRET_OK;` |
|        - |  850 | `	}` |
|       21 |  851 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 |  852 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 |  853 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 |  854 | `		if( pEntry ){` |
|       15 |  855 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 |  856 | `		}` |
|        8 |  857 | `	}` |
|       21 |  858 | `	ph7_result_value(pCtx,pArray);` |
|       21 |  859 | `	return SXRET_OK;` |
|       11 |  860 | `}` |
|        - |  861 | `/*` |
|        - |  862 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - |  863 | ` *  Default implementation of __autoload().` |
|        - |  864 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - |  865 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - |  866 | ` * Parameters` |
|        - |  867 | ` *  class` |
|        - |  868 | ` *   The class name being searched.` |
|        - |  869 | ` *  file_extensions` |
|        - |  870 | ` *   Comma-separated list of file extensions to try.` |
|        - |  871 | ` */` |
|        2 |  872 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  873 | `{` |
|        - |  874 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - |  875 | `	SyBlob sPath;` |
|        - |  876 | `	int nClass;` |
|        - |  877 | `	sxi32 rc;` |
|        3 |  878 | `	if( nArg < 1 ){` |
|      ! 0 |  879 | `		return SXRET_OK;` |
|        - |  880 | `	}` |
|        3 |  881 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 |  882 | `	if( nClass < 1 ){` |
|      ! 0 |  883 | `		return SXRET_OK;` |
|        - |  884 | `	}` |
|        - |  885 | `	/* Default extensions */` |
|        3 |  886 | `	zExt = ".php,.inc";` |
|        3 |  887 | `	if( nArg >= 2 ){` |
|        - |  888 | `		int nExt;` |
|      ! 0 |  889 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 |  890 | `		if( nExt < 1 ){` |
|      ! 0 |  891 | `			zExt = ".php,.inc";` |
|      ! 0 |  892 | `		}` |
|      ! 0 |  893 | `	}` |
|        3 |  894 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - |  895 | `	/* Iterate over comma-separated extensions */` |
|        3 |  896 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 |  897 | `	zCur = zExt;` |
|        7 |  898 | `	while( zCur < zEnd ){` |
|        - |  899 | `		const char *zComma;` |
|        - |  900 | `		SyString sFile;` |
|        - |  901 | `		int i;` |
|        - |  902 | `		/* Find next comma or end */` |
|        5 |  903 | `		zComma = zCur;` |
|       21 |  904 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 |  905 | `			zComma++;` |
|        1 |  906 | `		}` |
|        - |  907 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 |  908 | `		SyBlobReset(&sPath);` |
|       69 |  909 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 |  910 | `			char c = zClass[i];` |
|       65 |  911 | `			if( c == '\\' ){` |
|      ! 0 |  912 | `				c = '/';` |
|       65 |  913 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 |  914 | `				c = c + ('a' - 'A');` |
|        6 |  915 | `			}` |
|       65 |  916 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 |  917 | `		}` |
|        - |  918 | `		/* Append extension */` |
|        5 |  919 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - |  920 | `		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,` |
|        - |  921 | `		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a` |
|        - |  922 | `		 * terminator, so without this the strlen reads past the buffer (a` |
|        - |  923 | `		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL` |
|        - |  924 | `		 * is not counted in SyBlobLength(), so the SyString length stays correct. */` |
|        5 |  925 | `		SyBlobNullAppend(&sPath);` |
|        - |  926 | `		/* Try to include the file */` |
|        5 |  927 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 |  928 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 |  929 | `		if( rc == SXRET_OK ){` |
|        - |  930 | `			/* File included successfully */` |
|      ! 0 |  931 | `			SyBlobRelease(&sPath);` |
|      ! 0 |  932 | `			return SXRET_OK;` |
|        - |  933 | `		}` |
|        - |  934 | `		/* Move past the comma */` |
|        5 |  935 | `		zCur = zComma;` |
|        5 |  936 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 |  937 | `			zCur++;` |
|        1 |  938 | `		}` |
|        1 |  939 | `	}` |
|        3 |  940 | `	SyBlobRelease(&sPath);` |
|        3 |  941 | `	return SXRET_OK;` |
|        2 |  942 | `}` |
|        - |  943 | `/* Table of built-in VM functions. */` |
|        - |  944 | `/*` |
|        - |  945 | ` * Hidden packing trampoline for __call / __callStatic (band A #3b).` |
|        - |  946 | ` * OP_MEMBER, on a missing method whose class declares the magic handler,` |
|        - |  947 | ` * stashes {receiver, class, original name} on the VM and redirects the` |
|        - |  948 | ` * callee name to this host function; the normal OP_CALL machinery then` |
|        - |  949 | ` * collects the ORIGINAL argument list (incl. spreads) and hands it here,` |
|        - |  950 | ` * which packs it into a php array and invokes` |
|        - |  951 | ` *   $recv->__call($name, $args)   /   Class::__callStatic($name, $args)` |
|        - |  952 | ` * returning the handler's value as the call's result. A throw propagates` |
|        - |  953 | ` * via the returned status (and the boundary rail). Like the __gen_* /` |
|        - |  954 | ` * __reflect_* thunks, this is a PHL-internal global — calling it directly` |
|        - |  955 | ` * yields NULL (documented engine-specific surface).` |
|        - |  956 | ` */` |
|       10 |  957 | `PH7_PRIVATE int vm_builtin_magic_call(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  958 | `{` |
|       11 |  959 | `	ph7_vm *pVm = pCtx->pVm;` |
|       11 |  960 | `	ph7_class_instance *pRecv = pVm->pMagicCallThis;` |
|       11 |  961 | `	ph7_class *pClass = pVm->pMagicCallClass;` |
|       11 |  962 | `	ph7_class_method *pMeth = 0;` |
|        - |  963 | `	ph7_hashmap *pMap;` |
|        - |  964 | `	ph7_value sNameVal,sArgsVal,sResult;` |
|        - |  965 | `	ph7_value *apCall[2];` |
|        - |  966 | `	SyString sMethName;` |
|        - |  967 | `	sxi32 rc;` |
|        - |  968 | `	int i;` |
|        - |  969 | `	/* Consume the pending dispatch (one-shot) */` |
|       11 |  970 | `	pVm->pMagicCallThis = 0;` |
|       11 |  971 | `	pVm->pMagicCallClass = 0;` |
|       11 |  972 | `	if( pClass == 0 ){` |
|        - |  973 | `		/* Not a magic dispatch (direct user invocation): no-op */` |
|      ! 0 |  974 | `		ph7_result_null(pCtx);` |
|      ! 0 |  975 | `		return PH7_OK;` |
|        - |  976 | `	}` |
|       16 |  977 | `	pMeth = PH7_ClassExtractMethod(pClass,` |
|        5 |  978 | `		pRecv ? "__call" : "__callStatic",` |
|        5 |  979 | `		pRecv ? sizeof("__call")-1 : sizeof("__callStatic")-1);` |
|       11 |  980 | `	if( pMeth == 0 ){` |
|        - |  981 | `		/* Unreachable: OP_MEMBER verified the handler exists */` |
|      ! 0 |  982 | `		if( pRecv ){` |
|      ! 0 |  983 | `			PH7_ClassInstanceUnref(pRecv);` |
|      ! 0 |  984 | `		}` |
|      ! 0 |  985 | `		ph7_result_null(pCtx);` |
|      ! 0 |  986 | `		return PH7_OK;` |
|        - |  987 | `	}` |
|       11 |  988 | `	pMap = PH7_NewHashmap(pVm,0,0);` |
|       11 |  989 | `	if( pMap == 0 ){` |
|      ! 0 |  990 | `		if( pRecv ){` |
|      ! 0 |  991 | `			PH7_ClassInstanceUnref(pRecv);` |
|      ! 0 |  992 | `		}` |
|      ! 0 |  993 | `		PH7_VmMemoryError(pVm);` |
|      ! 0 |  994 | `		return PH7_ABORT;` |
|        - |  995 | `	}` |
|       25 |  996 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       15 |  997 | `		PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        8 |  998 | `	}` |
|       11 |  999 | `	SyStringInitFromBuf(&sMethName,SyBlobData(&pVm->sMagicCallName),SyBlobLength(&pVm->sMagicCallName));` |
|       11 | 1000 | `	PH7_MemObjInitFromString(pVm,&sNameVal,&sMethName);` |
|       11 | 1001 | `	sNameVal.nIdx = SXU32_HIGH;` |
|       11 | 1002 | `	PH7_MemObjInitFromArray(pVm,&sArgsVal,pMap);` |
|       11 | 1003 | `	sArgsVal.nIdx = SXU32_HIGH;` |
|       11 | 1004 | `	PH7_MemObjInit(pVm,&sResult);` |
|       11 | 1005 | `	apCall[0] = &sNameVal;` |
|       11 | 1006 | `	apCall[1] = &sArgsVal;` |
|       11 | 1007 | `	rc = PH7_VmCallClassMethod(pVm,pRecv,pMeth,&sResult,2,apCall);` |
|       11 | 1008 | `	if( rc == SXRET_OK ){` |
|        9 | 1009 | `		ph7_result_value(pCtx,&sResult);` |
|        4 | 1010 | `	}` |
|       11 | 1011 | `	PH7_MemObjRelease(&sResult);` |
|       11 | 1012 | `	PH7_MemObjRelease(&sNameVal);` |
|       11 | 1013 | `	PH7_MemObjRelease(&sArgsVal); /* drops the packed array */` |
|       11 | 1014 | `	if( pRecv ){` |
|        9 | 1015 | `		PH7_ClassInstanceUnref(pRecv);` |
|        4 | 1016 | `	}` |
|       11 | 1017 | `	SyBlobReset(&pVm->sMagicCallName);` |
|       11 | 1018 | `	return (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) ? rc : PH7_OK;` |
|        6 | 1019 | `}` |
|        - | 1020 |  |
