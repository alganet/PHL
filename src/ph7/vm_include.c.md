# src/ph7/vm_include.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 435/517 lines (84.14%)

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
|    90686 |   22 | `PH7_PRIVATE sxi32 VmEvalChunk(` |
|        - |   23 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |   24 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |   25 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |   26 | `	int iFlags,         /* Compile flag */` |
|        - |   27 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |   28 | `	)` |
|        5 |   29 | `{` |
|        - |   30 | `	SySet *pByteCode,aByteCode;` |
|        - |   31 | `	SyBlob sSavedNs;` |
|    90691 |   32 | `	ProcConsumer xErr = 0;` |
|    90691 |   33 | `	void *pErrData = 0;` |
|        - |   34 | `	ph7_gen_state sSavedGen;` |
|        - |   35 | `	int bNested;` |
|        - |   36 | `	/* Initialize bytecode container */` |
|    90691 |   37 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    90691 |   38 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |   39 | `	/* Reset the code generator */` |
|    90691 |   40 | `	if( bTrueReturn ){` |
|        - |   41 | `		/* Included file,log compile-time errors */` |
|     9155 |   42 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9155 |   43 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4576 |   44 | `	}` |
|        - |   45 | `	/* A non-zero cursor means an OUTER compile is in flight — this eval/include` |
|        - |   46 | `	 * was reached from inside it (an autoload fired while resolving a base class).` |
|        - |   47 | `	 * Wiping the generator would corrupt that outer parse, so snapshot it and hand` |
|        - |   48 | `	 * the nested unit a fresh one; a plain runtime eval/include just resets. */` |
|    90691 |   49 | `	bNested = (pVm->sCodeGen.pIn != 0);` |
|    90691 |   50 | `	if( bNested ){` |
|        5 |   51 | `		PH7_CompilerSaveState(pVm,&sSavedGen,xErr,pErrData);` |
|        3 |   52 | `	}else{` |
|    90687 |   53 | `		PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |   54 | `	}` |
|        - |   55 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |   56 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |   57 | `	 * the caller's namespace is restored. */` |
|    90691 |   58 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    90691 |   59 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    90691 |   60 | `	if( bTrueReturn ){` |
|        - |   61 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9155 |   62 | `		SyBlobReset(&pVm->sNamespace);` |
|     4576 |   63 | `	}` |
|        - |   64 | `	/* Swap bytecode container */` |
|    90691 |   65 | `	pByteCode = pVm->pByteContainer;` |
|    90691 |   66 | `	pVm->pByteContainer = &aByteCode;` |
|        - |   67 | `	/* Compile the chunk */` |
|    90691 |   68 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|   136033 |   69 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |   70 | `		/* Compilation error,return false */` |
|        3 |   71 | `		if( pCtx ){` |
|        3 |   72 | `			ph7_result_bool(pCtx,0);` |
|        1 |   73 | `		}` |
|        2 |   74 | `	}else{` |
|        - |   75 | `		/* Mount any newly defined classes. Skipped while the VM is still` |
|        - |   76 | `		 * initializing (builtin chunks): mounting evaluates class-constant/` |
|        - |   77 | `		 * static initializers and installs reference-table entries, and the` |
|        - |   78 | `		 * runtime structures those need (apRefObj, the per-exec object pool` |
|        - |   79 | `		 * baseline) do not exist before PH7_VmMakeReady — which mounts every` |
|        - |   80 | `		 * class unconditionally anyway. */` |
|        - |   81 | `		SyHashEntry *pEntry;` |
|        - |   82 | `		ph7_class *pClass;` |
|        - |   83 | `		ph7_value sResult; /* Return value */` |
|        - |   84 | `		sxi32 rc;` |
|    90689 |   85 | `		if( pVm->nMagic != PH7_VM_INIT ){` |
|     9251 |   86 | `			SyHashResetLoopCursor(&pVm->hClass);` |
|  2420634 |   87 | `			while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|  2406767 |   88 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|        - |   89 | `				/* Only mount classes that haven't been mounted yet */` |
|  2406767 |   90 | `				if( !pClass->bMounted ){` |
|   245495 |   91 | `					rc = VmMountUserClass(pVm,pClass);` |
|   245495 |   92 | `					if( rc != SXRET_OK ){` |
|        - |   93 | `						/* Mount failure (likely memory error) */` |
|        3 |   94 | `						if( pCtx ){` |
|        3 |   95 | `							ph7_result_bool(pCtx,0);` |
|        1 |   96 | `						}` |
|        3 |   97 | `						goto Cleanup;` |
|        - |   98 | `					}` |
|   122744 |   99 | `				}` |
|        5 |  100 | `			}` |
|     4622 |  101 | `		}` |
|    90687 |  102 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  103 | `			/* Out of memory */` |
|      ! 0 |  104 | `			if( pCtx ){` |
|      ! 0 |  105 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  106 | `			}` |
|      ! 0 |  107 | `			goto Cleanup;` |
|        - |  108 | `		}` |
|    90687 |  109 | `		if( bTrueReturn ){` |
|        - |  110 | `			/* Assume a boolean true return value */` |
|     9155 |  111 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4579 |  112 | `		}else{` |
|        - |  113 | `			/* Assume a null return value */` |
|    81535 |  114 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  115 | `		}` |
|        - |  116 | `		/* Execute the compiled chunk. eval()/include/require recurse in C here` |
|        - |  117 | `		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by` |
|        - |  118 | `		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the` |
|        - |  119 | `		 * native-nesting fatal instead of overflowing the C stack. The PHP` |
|        - |  120 | `		 * call-depth cap is OP_CALL-only (BYTECODE.md stage 5). */` |
|    90687 |  121 | `		VmLocalExec(pVm,&aByteCode,&sResult,FALSE);` |
|    90687 |  122 | `		if( pCtx ){` |
|        - |  123 | `			/* Set the execution result */` |
|     9249 |  124 | `			ph7_result_value(pCtx,&sResult);` |
|     4622 |  125 | `		}` |
|    90687 |  126 | `		PH7_MemObjRelease(&sResult);` |
|        - |  127 | `	}` |
|    45343 |  128 | `Cleanup:` |
|        - |  129 | `	/* Cleanup the mess left behind */` |
|    90691 |  130 | `	pVm->pByteContainer = pByteCode;` |
|    90691 |  131 | `	SySetRelease(&aByteCode);` |
|        - |  132 | `	/* Restore caller's namespace state */` |
|    90691 |  133 | `	SyBlobReset(&pVm->sNamespace);` |
|    90691 |  134 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    90691 |  135 | `	SyBlobRelease(&sSavedNs);` |
|        - |  136 | `	/* Restore the outer compile's generator state if this was a nested unit. */` |
|    90691 |  137 | `	if( bNested ){` |
|        5 |  138 | `		PH7_CompilerRestoreState(pVm,&sSavedGen);` |
|        2 |  139 | `	}` |
|    90691 |  140 | `	return SXRET_OK;` |
|        5 |  141 | `}` |
|        - |  142 | `/*` |
|        - |  143 | ` * Compile an embedded builtin PHP chunk into the VM. Thin exported wrapper` |
|        - |  144 | ` * around the static VmEvalChunk for builtin libraries that live outside` |
|        - |  145 | ` * this file (e.g. the Reflection classes in vm_builtin_reflection.c).` |
|        - |  146 | ` */` |
|    73682 |  147 | `PH7_PRIVATE sxi32 PH7_VmEvalBuiltinChunk(ph7_vm *pVm,const char *zSrc,sxu32 nLen)` |
|        5 |  148 | `{` |
|        - |  149 | `	SyString sChunk;` |
|    73687 |  150 | `	SyStringInitFromBuf(&sChunk,zSrc,nLen);` |
|    73687 |  151 | `	return VmEvalChunk(&(*pVm),0,&sChunk,PH7_PHP_ONLY,FALSE);` |
|        5 |  152 | `}` |
|        - |  153 | `/*` |
|        - |  154 | ` * value eval(string $code)` |
|        - |  155 | ` *   Evaluate a string as PHP code.` |
|        - |  156 | ` * Parameter` |
|        - |  157 | ` *  code: PHP code to evaluate.` |
|        - |  158 | ` * Return` |
|        - |  159 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  160 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  161 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  162 | ` */` |
|       98 |  163 | `PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  164 | `{` |
|        - |  165 | `	SyString sChunk;    /* Chunk to evaluate */` |
|      103 |  166 | `	if( nArg < 1 ){` |
|        - |  167 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  168 | `		ph7_result_null(pCtx);` |
|      ! 0 |  169 | `		return SXRET_OK;` |
|        - |  170 | `	}` |
|        - |  171 | `	/* Chunk to evaluate */` |
|      103 |  172 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|      103 |  173 | `	if( sChunk.nByte < 1 ){` |
|        - |  174 | `		/* Empty string,return NULL */` |
|        3 |  175 | `		ph7_result_null(pCtx);` |
|        3 |  176 | `		return SXRET_OK;` |
|        - |  177 | `	}` |
|        - |  178 | `	/* Eval the chunk */` |
|      101 |  179 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|      101 |  180 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  181 | `		/* exit/die inside the evaluated chunk: cascade the halt */` |
|       70 |  182 | `		return PH7_ABORT;` |
|        - |  183 | `	}` |
|       31 |  184 | `	return SXRET_OK;` |
|       54 |  185 | `}` |
|        - |  186 | `/*` |
|        - |  187 | ` * Check if a file path is already included.` |
|        - |  188 | ` */` |
|     9164 |  189 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        3 |  190 | `{` |
|        - |  191 | `	SyString *aEntries;` |
|        - |  192 | `	sxu32 n;` |
|     9167 |  193 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  194 | `	/* Perform a linear search */` |
| 20884981 |  195 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 20875830 |  196 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  197 | `			/* Already included */` |
|       16 |  198 | `			return TRUE;` |
|        - |  199 | `		}` |
| 10437909 |  200 | `	}` |
|     9153 |  201 | `	return FALSE;` |
|     4585 |  202 | `}` |
|        - |  203 | `/*` |
|        - |  204 | ` * Push a file path in the appropriate VM container.` |
|        - |  205 | ` */` |
|    13042 |  206 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        5 |  207 | `{` |
|        - |  208 | `	SyString sPath;` |
|        - |  209 | `	char *zDup;` |
|        - |  210 | `	sxi32 rc;` |
|    13047 |  211 | `	if( nLen < 0 ){` |
|     3883 |  212 | `		nLen = SyStrlen(zPath);` |
|     1939 |  213 | `	}` |
|        - |  214 | `	/* Duplicate the file path first */` |
|    13047 |  215 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    13047 |  216 | `	if( zDup == 0 ){` |
|      ! 0 |  217 | `		return SXERR_MEM;` |
|        - |  218 | `	}` |
|        - |  219 | `#ifdef __UNIXES__` |
|        - |  220 | `	/* php records the REALPATH'd absolute path for __FILE__/__DIR__/getFileName` |
|        - |  221 | `	 * and include-once dedup: realpath() resolves '.', '..' and symlinks (e.g.` |
|        - |  222 | `	 * macOS /tmp -> /private/tmp). It needs an existing file, so on failure keep` |
|        - |  223 | `	 * the raw path (eval'd code, php:///data:// wrappers, a missing include that` |
|        - |  224 | `	 * errors elsewhere). */` |
|        - |  225 | `	{` |
|    13042 |  226 | `		char *zReal = realpath(zDup,0); /* POSIX: malloc'd result */` |
|    13042 |  227 | `		if( zReal ){` |
|    13006 |  228 | `			sxu32 nReal = SyStrlen(zReal);` |
|    13006 |  229 | `			char *zRealDup = SyMemBackendStrDup(&pVm->sAllocator,zReal,nReal);` |
|    13006 |  230 | `			free(zReal);` |
|    13006 |  231 | `			if( zRealDup ){` |
|    13006 |  232 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|    13006 |  233 | `				zDup = zRealDup;` |
|    13006 |  234 | `				nLen = (int)nReal;` |
|     6503 |  235 | `			}` |
|     6503 |  236 | `		}` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|        - |  239 | `#ifdef __WINNT__` |
|        - |  240 | `	/* Windows counterpart of the realpath() above: canonicalize to an absolute` |
|        - |  241 | `	 * path (resolves '.'/'..' , '/' -> '\'), case-preserved to match php — NOT` |
|        - |  242 | `	 * lowercased as the legacy PH7 code did. Like the POSIX branch, keep the raw` |
|        - |  243 | `	 * path when it does not name an existing file (the "Command line code" marker,` |
|        - |  244 | `	 * eval'd code, stream wrappers). _fullpath() is lexical, so pair it with a VFS` |
|        - |  245 | `	 * existence check. */` |
|        - |  246 | `	{` |
|        5 |  247 | `		char *zFull = _fullpath(0,zDup,0); /* MSVC CRT: malloc'd absolute path */` |
|        5 |  248 | `		if( zFull ){` |
|        5 |  249 | `			if( pVm->pEngine->pVfs && pVm->pEngine->pVfs->xFileExists && pVm->pEngine->pVfs->xFileExists(zFull) == PH7_OK ){` |
|        5 |  250 | `				sxu32 nFull = SyStrlen(zFull);` |
|        5 |  251 | `				char *zFullDup = SyMemBackendStrDup(&pVm->sAllocator,zFull,nFull);` |
|        5 |  252 | `				if( zFullDup ){` |
|        5 |  253 | `					SyMemBackendFree(&pVm->sAllocator,zDup);` |
|        5 |  254 | `					zDup = zFullDup;` |
|        5 |  255 | `					nLen = (int)nFull;` |
|        - |  256 | `				}` |
|        - |  257 | `			}` |
|        5 |  258 | `			free(zFull);` |
|        - |  259 | `		}` |
|        - |  260 | `	}` |
|        - |  261 | `#endif` |
|        - |  262 | `	/* Install the file path */` |
|    13047 |  263 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    13047 |  264 | `	if( !bMain ){` |
|     9167 |  265 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  266 | `			/* Already included */` |
|       16 |  267 | `			*pNew = 0;` |
|        9 |  268 | `		}else{` |
|        - |  269 | `			/* Insert in the corresponding container */` |
|     9153 |  270 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|     9153 |  271 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  272 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  273 | `				return rc;` |
|        - |  274 | `			}` |
|     9153 |  275 | `			*pNew = 1;` |
|        - |  276 | `		}` |
|     4582 |  277 | `	}` |
|    13047 |  278 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    13047 |  279 | `	return SXRET_OK;` |
|     6526 |  280 | `}` |
|        - |  281 | `/*` |
|        - |  282 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  283 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  284 | ` * indicates failure.` |
|        - |  285 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  286 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  287 | ` * operations.` |
|        - |  288 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  289 | ` * this function is a no-op.` |
|        - |  290 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  291 | ` * constructs for more information.` |
|        - |  292 | ` */` |
|     9170 |  293 | `static sxi32 VmExecIncludedFile(` |
|        - |  294 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  295 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  296 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  297 | `	 )` |
|        3 |  298 | `{` |
|        - |  299 | `	sxi32 rc;` |
|        - |  300 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  301 | `	const ph7_io_stream *pStream;` |
|        - |  302 | `	SyBlob sContents;` |
|        - |  303 | `	void *pHandle;` |
|        - |  304 | `	ph7_vm *pVm;` |
|        - |  305 | `	int isNew;` |
|        - |  306 | `	/* Initialize fields */` |
|     9173 |  307 | `	pVm = pCtx->pVm;` |
|     9173 |  308 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9173 |  309 | `	isNew = 0;` |
|        - |  310 | `	/* Extract the associated stream */` |
|     9173 |  311 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  312 | `	/*` |
|        - |  313 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  314 | `	 * in a read-only mode.` |
|        - |  315 | `	 */` |
|     9173 |  316 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9173 |  317 | `	if( pHandle == 0 ){` |
|        8 |  318 | `		return SXERR_IO;` |
|        - |  319 | `	}` |
|     9167 |  320 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9167 |  321 | `	if( IncludeOnce && !isNew ){` |
|        - |  322 | `		/* Already included */` |
|       14 |  323 | `		rc = SXERR_EXISTS;` |
|        8 |  324 | `	}else{` |
|        - |  325 | `		/* Read the whole file contents */` |
|     9155 |  326 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9155 |  327 | `		if( rc == SXRET_OK ){` |
|        - |  328 | `			SyString sScript;` |
|        - |  329 | `			/* Compile and execute the script */` |
|     9155 |  330 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9155 |  331 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4576 |  332 | `		}` |
|        - |  333 | `	}` |
|        - |  334 | `	/* Pop from the set of included file */` |
|     9167 |  335 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  336 | `	/* Close the handle */` |
|     9167 |  337 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  338 | `	/* Release the working buffer */` |
|     9167 |  339 | `	SyBlobRelease(&sContents);` |
|        - |  340 | `#else` |
|        - |  341 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  342 | `	SXUNUSED(pPath);` |
|        - |  343 | `	SXUNUSED(IncludeOnce);` |
|        - |  344 | `	rc = SXERR_IO;` |
|        - |  345 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9167 |  346 | `	return rc;` |
|     4588 |  347 | `}` |
|        - |  348 | `/*` |
|        - |  349 | ` * string get_include_path(void)` |
|        - |  350 | ` *  Gets the current include_path configuration option.` |
|        - |  351 | ` * Parameter` |
|        - |  352 | ` *  None` |
|        - |  353 | ` * Return` |
|        - |  354 | ` *  Included paths as a string` |
|        - |  355 | ` */` |
|        8 |  356 | `PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  357 | `{` |
|       10 |  358 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  359 | `	SyString *aEntry;` |
|        - |  360 | `	int dir_sep;` |
|        - |  361 | `	sxu32 n;` |
|        - |  362 | `#ifdef __WINNT__` |
|        2 |  363 | `	dir_sep = ';';` |
|        - |  364 | `#else` |
|        - |  365 | `	/* Assume UNIX path separator */` |
|        8 |  366 | `	dir_sep = ':';` |
|        - |  367 | `#endif` |
|        4 |  368 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 |  369 | `	SXUNUSED(apArg);` |
|        - |  370 | `	/* Point to the list of import paths */` |
|       10 |  371 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       20 |  372 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|       12 |  373 | `		SyString *pEntry = &aEntry[n];` |
|       12 |  374 | `		if( n > 0 ){` |
|        - |  375 | `			/* Append dir seprator */` |
|        2 |  376 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|        1 |  377 | `		}` |
|        - |  378 | `		/* Append path */` |
|       12 |  379 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        7 |  380 | `	}` |
|       10 |  381 | `	return PH7_OK;` |
|        2 |  382 | `}` |
|        - |  383 | `/*` |
|        - |  384 | ` * string\|false set_include_path(string $include_path)` |
|        - |  385 | ` *  Sets the include_path configuration option for the duration of the script,` |
|        - |  386 | ` *  returning the OLD value (php contract).` |
|        - |  387 | ` */` |
|        6 |  388 | `PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  389 | `{` |
|        7 |  390 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  391 | `	SyString *aEntry;` |
|        - |  392 | `	const char *zNew, *z, *zEnd;` |
|        - |  393 | `	int dir_sep, nLen;` |
|        - |  394 | `	sxu32 n;` |
|        - |  395 | `#ifdef __WINNT__` |
|        1 |  396 | `	dir_sep = ';';` |
|        - |  397 | `#else` |
|        6 |  398 | `	dir_sep = ':';` |
|        - |  399 | `#endif` |
|        - |  400 | `	/* Build the OLD include_path first: it is this call's return value */` |
|        7 |  401 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|       15 |  402 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        9 |  403 | `		if( n > 0 ){ ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char)); }` |
|        9 |  404 | `		ph7_result_string(pCtx,aEntry[n].zString,(int)aEntry[n].nByte);` |
|        5 |  405 | `	}` |
|        7 |  406 | `	if( nArg < 1 ){` |
|      ! 0 |  407 | `		return PH7_OK;` |
|        - |  408 | `	}` |
|        - |  409 | `	/* Replace the path set with the separated segments of the new value.` |
|        - |  410 | `	 * Segments are duped into the VM allocator (reclaimed at VM teardown) so` |
|        - |  411 | `	 * the SyString entries stay valid, mirroring the config-time literals. */` |
|        7 |  412 | `	zNew = ph7_value_to_string(apArg[0],&nLen);` |
|        7 |  413 | `	SySetReset(&pVm->aPaths);` |
|        7 |  414 | `	z = zNew; zEnd = &zNew[nLen];` |
|       15 |  415 | `	while( z < zEnd ){` |
|        9 |  416 | `		const char *zStart = z;` |
|        - |  417 | `		SyString sPath;` |
|       49 |  418 | `		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }` |
|        9 |  419 | `		if( z > zStart ){` |
|        9 |  420 | `			char *zDup = (char *)SyMemBackendDup(&pVm->sAllocator,zStart,(sxu32)(z-zStart));` |
|        9 |  421 | `			if( zDup ){` |
|        9 |  422 | `				SyStringInitFromBuf(&sPath,zDup,(sxu32)(z-zStart));` |
|        - |  423 | `#ifdef __WINNT__` |
|        1 |  424 | `				SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  425 | `#endif` |
|        9 |  426 | `				SyStringTrimTrailingChar(&sPath,'/');` |
|        9 |  427 | `				SyStringFullTrim(&sPath);` |
|        9 |  428 | `				if( sPath.nByte > 0 ){` |
|        9 |  429 | `					SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|        4 |  430 | `				}` |
|        4 |  431 | `			}` |
|        4 |  432 | `		}` |
|        9 |  433 | `		if( z < zEnd ){ z++; } /* skip the separator */` |
|        1 |  434 | `	}` |
|        7 |  435 | `	return PH7_OK;` |
|        4 |  436 | `}` |
|        - |  437 | `/*` |
|        - |  438 | ` * string get_get_included_files(void)` |
|        - |  439 | ` *  Gets the current include_path configuration option.` |
|        - |  440 | ` * Parameter` |
|        - |  441 | ` *  None` |
|        - |  442 | ` * Return` |
|        - |  443 | ` *  Included paths as a string` |
|        - |  444 | ` */` |
|        2 |  445 | `PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  446 | `{` |
|        3 |  447 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  448 | `	ph7_value *pArray,*pWorker;` |
|        - |  449 | `	SyString *pEntry;` |
|        - |  450 | `	int c,d;` |
|        - |  451 | `	/* Create an array and a working value */` |
|        3 |  452 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  453 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  454 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  455 | `		/* Out of memory,return null */` |
|      ! 0 |  456 | `		ph7_result_null(pCtx);` |
|      ! 0 |  457 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  458 | `		SXUNUSED(apArg);` |
|      ! 0 |  459 | `		return PH7_OK;` |
|        - |  460 | `	}` |
|        3 |  461 | `	c = d = '/';` |
|        - |  462 | `#ifdef __WINNT__` |
|        1 |  463 | `	d = '\\';` |
|        - |  464 | `#endif` |
|        - |  465 | `	/* Iterate throw entries */` |
|        3 |  466 | `	SySetResetCursor(pFiles);` |
|        7 |  467 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  468 | `		const char *zBase,*zEnd;` |
|        - |  469 | `		int iLen;` |
|        - |  470 | `		/* reset the string cursor */` |
|        5 |  471 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  472 | `		/* Extract base name */` |
|        5 |  473 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  474 | `		/* Ignore trailing '/' */` |
|        7 |  475 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  476 | `			zEnd--;` |
|      ! 0 |  477 | `		}` |
|        5 |  478 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|       79 |  479 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|       73 |  480 | `			zEnd--;` |
|        1 |  481 | `		}` |
|        5 |  482 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|        5 |  483 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  484 | `		/* Copy entry name */` |
|        5 |  485 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  486 | `		/* Perform the insertion */` |
|        5 |  487 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  488 | `	}` |
|        - |  489 | `	/* All done,return the created array */` |
|        3 |  490 | `	ph7_result_value(pCtx,pArray);` |
|        - |  491 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  492 | `	 * by the engine as soon we return from this foreign` |
|        - |  493 | `	 * function.` |
|        - |  494 | `	 */` |
|        3 |  495 | `	return PH7_OK;` |
|        2 |  496 | `}` |
|        - |  497 | `/*` |
|        - |  498 | ` * include:` |
|        - |  499 | ` * According to the PHP reference manual.` |
|        - |  500 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  501 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  502 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  503 | ` *  include() will finally check in the calling script's own directory` |
|        - |  504 | ` *  and the current working directory before failing. The include()` |
|        - |  505 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  506 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  507 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  508 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  509 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  510 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  511 | ` *  directory to find the requested file.` |
|        - |  512 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  513 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  514 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  515 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  516 | ` */` |
|     9130 |  517 | `PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  518 | `{` |
|        - |  519 | `	SyString sFile;` |
|        - |  520 | `	sxi32 rc;` |
|     9133 |  521 | `	if( nArg < 1 ){` |
|        - |  522 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  523 | `		ph7_result_null(pCtx);` |
|      ! 0 |  524 | `		return SXRET_OK;` |
|        - |  525 | `	}` |
|        - |  526 | `	/* File to include */` |
|     9133 |  527 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9133 |  528 | `	if( sFile.nByte < 1 ){` |
|        - |  529 | `		/* Empty string,return NULL */` |
|      ! 0 |  530 | `		ph7_result_null(pCtx);` |
|      ! 0 |  531 | `		return SXRET_OK;` |
|        - |  532 | `	}` |
|        - |  533 | `	/* Open,compile and execute the desired script */` |
|     9133 |  534 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9133 |  535 | `	if( rc != SXRET_OK ){` |
|        - |  536 | `		/* Emit a warning and return false */` |
|        3 |  537 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  538 | `		ph7_result_bool(pCtx,0);` |
|        1 |  539 | `	}` |
|     9133 |  540 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  541 | `		/* exit/die inside the included file: cascade the halt */` |
|        6 |  542 | `		return PH7_ABORT;` |
|        - |  543 | `	}` |
|     9128 |  544 | `	return SXRET_OK;` |
|     4568 |  545 | `}` |
|        - |  546 | `/*` |
|        - |  547 | ` * include_once:` |
|        - |  548 | ` *  According to the PHP reference manual.` |
|        - |  549 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  550 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  551 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  552 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  553 | ` *   just once.` |
|        - |  554 | ` */` |
|       16 |  555 | `PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  556 | `{` |
|        - |  557 | `	SyString sFile;` |
|        - |  558 | `	sxi32 rc;` |
|       18 |  559 | `	if( nArg < 1 ){` |
|        - |  560 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  561 | `		ph7_result_null(pCtx);` |
|      ! 0 |  562 | `		return SXRET_OK;` |
|        - |  563 | `	}` |
|        - |  564 | `	/* File to include */` |
|       18 |  565 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       18 |  566 | `	if( sFile.nByte < 1 ){` |
|        - |  567 | `		/* Empty string,return NULL */` |
|      ! 0 |  568 | `		ph7_result_null(pCtx);` |
|      ! 0 |  569 | `		return SXRET_OK;` |
|        - |  570 | `	}` |
|        - |  571 | `	/* Open,compile and execute the desired script */` |
|       18 |  572 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|       18 |  573 | `	if( rc == SXERR_EXISTS ){` |
|        - |  574 | `		/* File already included,return TRUE */` |
|       12 |  575 | `		ph7_result_bool(pCtx,1);` |
|       12 |  576 | `		return SXRET_OK;` |
|        - |  577 | `	}` |
|        8 |  578 | `	if( rc != SXRET_OK ){` |
|        - |  579 | `		/* Emit a warning and return false */` |
|      ! 0 |  580 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  581 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  582 | ` 	}` |
|        8 |  583 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  584 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  585 | `		return PH7_ABORT;` |
|        - |  586 | `	}` |
|        8 |  587 | `	return SXRET_OK;` |
|       10 |  588 | `}` |
|        - |  589 | `/*` |
|        - |  590 | ` * require.` |
|        - |  591 | ` *  According to the PHP reference manual.` |
|        - |  592 | ` *   require() is identical to include() except upon failure it will` |
|        - |  593 | ` *   also produce a fatal level error.` |
|        - |  594 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  595 | ` *   emits a warning  which allows the script to continue.` |
|        - |  596 | ` */` |
|       14 |  597 | `PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  598 | `{` |
|        - |  599 | `	SyString sFile;` |
|        - |  600 | `	sxi32 rc;` |
|       17 |  601 | `	if( nArg < 1 ){` |
|        - |  602 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  603 | `		ph7_result_null(pCtx);` |
|      ! 0 |  604 | `		return SXRET_OK;` |
|        - |  605 | `	}` |
|        - |  606 | `	/* File to include */` |
|       17 |  607 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|       17 |  608 | `	if( sFile.nByte < 1 ){` |
|        - |  609 | `		/* Empty string,return NULL */` |
|      ! 0 |  610 | `		ph7_result_null(pCtx);` |
|      ! 0 |  611 | `		return SXRET_OK;` |
|        - |  612 | `	}` |
|        - |  613 | `	/* Open,compile and execute the desired script */` |
|       17 |  614 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|       17 |  615 | `	if( rc != SXRET_OK ){` |
|        - |  616 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  617 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  618 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  619 | `		return PH7_ABORT;` |
|        - |  620 | `	}` |
|       17 |  621 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  622 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  623 | `		return PH7_ABORT;` |
|        - |  624 | `	}` |
|       17 |  625 | `	return SXRET_OK;` |
|       10 |  626 | `}` |
|        - |  627 | `/*` |
|        - |  628 | ` * require_once:` |
|        - |  629 | ` *  According to the PHP reference manual.` |
|        - |  630 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  631 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  632 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  633 | ` *   and how it differs from its non _once siblings.` |
|        - |  634 | ` */` |
|        6 |  635 | `PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  636 | `{` |
|        - |  637 | `	SyString sFile;` |
|        - |  638 | `	sxi32 rc;` |
|        8 |  639 | `	if( nArg < 1 ){` |
|        - |  640 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  641 | `		ph7_result_null(pCtx);` |
|      ! 0 |  642 | `		return SXRET_OK;` |
|        - |  643 | `	}` |
|        - |  644 | `	/* File to include */` |
|        8 |  645 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 |  646 | `	if( sFile.nByte < 1 ){` |
|        - |  647 | `		/* Empty string,return NULL */` |
|      ! 0 |  648 | `		ph7_result_null(pCtx);` |
|      ! 0 |  649 | `		return SXRET_OK;` |
|        - |  650 | `	}` |
|        - |  651 | `	/* Open,compile and execute the desired script */` |
|        8 |  652 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        8 |  653 | `	if( rc == SXERR_EXISTS ){` |
|        - |  654 | `		/* File already included,return TRUE */` |
|        3 |  655 | `		ph7_result_bool(pCtx,1);` |
|        3 |  656 | `		return SXRET_OK;` |
|        - |  657 | `	}` |
|        6 |  658 | `	if( rc != SXRET_OK ){` |
|        - |  659 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  660 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  661 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  662 | `		return PH7_ABORT;` |
|        - |  663 | `	}` |
|        6 |  664 | `	if( pCtx->pVm->bHaltRequested ){` |
|        - |  665 | `		/* exit/die inside the included file: cascade the halt */` |
|      ! 0 |  666 | `		return PH7_ABORT;` |
|        - |  667 | `	}` |
|        6 |  668 | `	return SXRET_OK;` |
|        5 |  669 | `}` |
|        - |  670 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  671 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  672 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  673 | `/*` |
|        - |  674 | ` * Section:` |
|        - |  675 | ` *  SPL Autoloading functions.` |
|        - |  676 | ` * Status:` |
|        - |  677 | ` *  Stable.` |
|        - |  678 | ` */` |
|        - |  679 | `/*` |
|        - |  680 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - |  681 | ` *  Register given function as __autoload() implementation.` |
|        - |  682 | ` * Parameters` |
|        - |  683 | ` *  callback` |
|        - |  684 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - |  685 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - |  686 | ` *  throw` |
|        - |  687 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - |  688 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - |  689 | ` *  prepend` |
|        - |  690 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - |  691 | ` *   autoload stack instead of appending it.` |
|        - |  692 | ` * Return` |
|        - |  693 | ` *  TRUE on success, FALSE on failure.` |
|        - |  694 | ` */` |
|       38 |  695 | `PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  696 | `{` |
|        - |  697 | `	VmAutoloadCB sEntry;` |
|       43 |  698 | `	ph7_vm *pVm = pCtx->pVm;` |
|       43 |  699 | `	int iPrepend = 0;` |
|        - |  700 | `	sxu32 n;` |
|       43 |  701 | `	if( nArg < 1 ){` |
|        - |  702 | `		/* No callback provided — register default spl_autoload.` |
|        - |  703 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - |  704 | `		/* Check for duplicates first */` |
|        9 |  705 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 |  706 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        4 |  707 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 |  708 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 |  709 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 |  710 | `				ph7_result_bool(pCtx,1);` |
|        5 |  711 | `				return SXRET_OK;` |
|        - |  712 | `			}` |
|      ! 0 |  713 | `		}` |
|        5 |  714 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 |  715 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 |  716 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 |  717 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 |  718 | `		ph7_result_bool(pCtx,1);` |
|        5 |  719 | `		return SXRET_OK;` |
|        - |  720 | `	}` |
|        - |  721 | `	/* Validate that the callback is callable */` |
|       35 |  722 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 |  723 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 |  724 | `		if( nArg >= 2 ){` |
|      ! 0 |  725 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  726 | `		}` |
|      ! 0 |  727 | `		if( iThrow ){` |
|      ! 0 |  728 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - |  729 | `				"Argument is not callable");` |
|      ! 0 |  730 | `		}` |
|      ! 0 |  731 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  732 | `		return SXRET_OK;` |
|        - |  733 | `	}` |
|        - |  734 | `	/* Check for duplicates */` |
|       53 |  735 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 |  736 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 |  737 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  738 | `			/* Already registered */` |
|      ! 0 |  739 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  740 | `			return SXRET_OK;` |
|        - |  741 | `		}` |
|       11 |  742 | `	}` |
|        - |  743 | `	/* Check prepend flag */` |
|       35 |  744 | `	if( nArg >= 3 ){` |
|        3 |  745 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 |  746 | `	}` |
|        - |  747 | `	/* Store the callback */` |
|       35 |  748 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       35 |  749 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       35 |  750 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       36 |  751 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - |  752 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - |  753 | `		 * We do this by appending first, then rotating the array. */` |
|        3 |  754 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - |  755 | `		VmAutoloadCB *aBase;` |
|        3 |  756 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  757 | `		/* Rotate: move last entry to front */` |
|        3 |  758 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 |  759 | `		if( aBase ){` |
|        - |  760 | `			VmAutoloadCB sTemp;` |
|        - |  761 | `			sxu32 i;` |
|        3 |  762 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 |  763 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 |  764 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 |  765 | `			}` |
|        3 |  766 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 |  767 | `		}` |
|        2 |  768 | `	}else{` |
|       33 |  769 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - |  770 | `	}` |
|       35 |  771 | `	ph7_result_bool(pCtx,1);` |
|       35 |  772 | `	return SXRET_OK;` |
|       24 |  773 | `}` |
|        - |  774 | `/*` |
|        - |  775 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - |  776 | ` *  Unregister a given function as __autoload() implementation.` |
|        - |  777 | ` * Parameters` |
|        - |  778 | ` *  callback` |
|        - |  779 | ` *   The autoload function being unregistered.` |
|        - |  780 | ` * Return` |
|        - |  781 | ` *  TRUE on success, FALSE on failure.` |
|        - |  782 | ` */` |
|       32 |  783 | `PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  784 | `{` |
|       37 |  785 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  786 | `	sxu32 n,nEntry;` |
|       37 |  787 | `	if( nArg < 1 ){` |
|      ! 0 |  788 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  789 | `		return SXRET_OK;` |
|        - |  790 | `	}` |
|       37 |  791 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       41 |  792 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       39 |  793 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       39 |  794 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - |  795 | `			/* Found — remove by shifting remaining entries down */` |
|       35 |  796 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - |  797 | `			sxu32 i;` |
|       35 |  798 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       49 |  799 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 |  800 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 |  801 | `			}` |
|        - |  802 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       35 |  803 | `			SySetPop(&pVm->aAutoload);` |
|       35 |  804 | `			ph7_result_bool(pCtx,1);` |
|       35 |  805 | `			return SXRET_OK;` |
|        - |  806 | `		}` |
|        3 |  807 | `	}` |
|        3 |  808 | `	ph7_result_bool(pCtx,0);` |
|        3 |  809 | `	return SXRET_OK;` |
|       21 |  810 | `}` |
|        - |  811 | `/*` |
|        - |  812 | ` * array spl_autoload_functions(void)` |
|        - |  813 | ` *  Return all registered __autoload() functions.` |
|        - |  814 | ` * Return` |
|        - |  815 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - |  816 | ` *  an empty array is returned.` |
|        - |  817 | ` */` |
|       20 |  818 | `PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  819 | `{` |
|       21 |  820 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  821 | `	ph7_value *pArray;` |
|        - |  822 | `	sxu32 n,nEntry;` |
|       10 |  823 | `	SXUNUSED(nArg);` |
|       10 |  824 | `	SXUNUSED(apArg);` |
|       21 |  825 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 |  826 | `	if( pArray == 0 ){` |
|      ! 0 |  827 | `		ph7_result_null(pCtx);` |
|      ! 0 |  828 | `		return SXRET_OK;` |
|        - |  829 | `	}` |
|       21 |  830 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 |  831 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 |  832 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 |  833 | `		if( pEntry ){` |
|       15 |  834 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 |  835 | `		}` |
|        8 |  836 | `	}` |
|       21 |  837 | `	ph7_result_value(pCtx,pArray);` |
|       21 |  838 | `	return SXRET_OK;` |
|       11 |  839 | `}` |
|        - |  840 | `/*` |
|        - |  841 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - |  842 | ` *  Default implementation of __autoload().` |
|        - |  843 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - |  844 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - |  845 | ` * Parameters` |
|        - |  846 | ` *  class` |
|        - |  847 | ` *   The class name being searched.` |
|        - |  848 | ` *  file_extensions` |
|        - |  849 | ` *   Comma-separated list of file extensions to try.` |
|        - |  850 | ` */` |
|        2 |  851 | `PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  852 | `{` |
|        - |  853 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - |  854 | `	SyBlob sPath;` |
|        - |  855 | `	int nClass;` |
|        - |  856 | `	sxi32 rc;` |
|        3 |  857 | `	if( nArg < 1 ){` |
|      ! 0 |  858 | `		return SXRET_OK;` |
|        - |  859 | `	}` |
|        3 |  860 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 |  861 | `	if( nClass < 1 ){` |
|      ! 0 |  862 | `		return SXRET_OK;` |
|        - |  863 | `	}` |
|        - |  864 | `	/* Default extensions */` |
|        3 |  865 | `	zExt = ".php,.inc";` |
|        3 |  866 | `	if( nArg >= 2 ){` |
|        - |  867 | `		int nExt;` |
|      ! 0 |  868 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 |  869 | `		if( nExt < 1 ){` |
|      ! 0 |  870 | `			zExt = ".php,.inc";` |
|      ! 0 |  871 | `		}` |
|      ! 0 |  872 | `	}` |
|        3 |  873 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - |  874 | `	/* Iterate over comma-separated extensions */` |
|        3 |  875 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 |  876 | `	zCur = zExt;` |
|        7 |  877 | `	while( zCur < zEnd ){` |
|        - |  878 | `		const char *zComma;` |
|        - |  879 | `		SyString sFile;` |
|        - |  880 | `		int i;` |
|        - |  881 | `		/* Find next comma or end */` |
|        5 |  882 | `		zComma = zCur;` |
|       21 |  883 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 |  884 | `			zComma++;` |
|        1 |  885 | `		}` |
|        - |  886 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 |  887 | `		SyBlobReset(&sPath);` |
|       69 |  888 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 |  889 | `			char c = zClass[i];` |
|       65 |  890 | `			if( c == '\\' ){` |
|      ! 0 |  891 | `				c = '/';` |
|       65 |  892 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 |  893 | `				c = c + ('a' - 'A');` |
|        6 |  894 | `			}` |
|       65 |  895 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 |  896 | `		}` |
|        - |  897 | `		/* Append extension */` |
|        5 |  898 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - |  899 | `		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,` |
|        - |  900 | `		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a` |
|        - |  901 | `		 * terminator, so without this the strlen reads past the buffer (a` |
|        - |  902 | `		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL` |
|        - |  903 | `		 * is not counted in SyBlobLength(), so the SyString length stays correct. */` |
|        5 |  904 | `		SyBlobNullAppend(&sPath);` |
|        - |  905 | `		/* Try to include the file */` |
|        5 |  906 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 |  907 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 |  908 | `		if( rc == SXRET_OK ){` |
|        - |  909 | `			/* File included successfully */` |
|      ! 0 |  910 | `			SyBlobRelease(&sPath);` |
|      ! 0 |  911 | `			return SXRET_OK;` |
|        - |  912 | `		}` |
|        - |  913 | `		/* Move past the comma */` |
|        5 |  914 | `		zCur = zComma;` |
|        5 |  915 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 |  916 | `			zCur++;` |
|        1 |  917 | `		}` |
|        1 |  918 | `	}` |
|        3 |  919 | `	SyBlobRelease(&sPath);` |
|        3 |  920 | `	return SXRET_OK;` |
|        2 |  921 | `}` |
|        - |  922 | `/* Table of built-in VM functions. */` |
|        - |  923 | `/*` |
|        - |  924 | ` * Hidden packing trampoline for __call / __callStatic (band A #3b).` |
|        - |  925 | ` * OP_MEMBER, on a missing method whose class declares the magic handler,` |
|        - |  926 | ` * stashes {receiver, class, original name} on the VM and redirects the` |
|        - |  927 | ` * callee name to this host function; the normal OP_CALL machinery then` |
|        - |  928 | ` * collects the ORIGINAL argument list (incl. spreads) and hands it here,` |
|        - |  929 | ` * which packs it into a php array and invokes` |
|        - |  930 | ` *   $recv->__call($name, $args)   /   Class::__callStatic($name, $args)` |
|        - |  931 | ` * returning the handler's value as the call's result. A throw propagates` |
|        - |  932 | ` * via the returned status (and the boundary rail). Like the __gen_* /` |
|        - |  933 | ` * __reflect_* thunks, this is a PHL-internal global — calling it directly` |
|        - |  934 | ` * yields NULL (documented engine-specific surface).` |
|        - |  935 | ` */` |
|       10 |  936 | `PH7_PRIVATE int vm_builtin_magic_call(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  937 | `{` |
|       11 |  938 | `	ph7_vm *pVm = pCtx->pVm;` |
|       11 |  939 | `	ph7_class_instance *pRecv = pVm->pMagicCallThis;` |
|       11 |  940 | `	ph7_class *pClass = pVm->pMagicCallClass;` |
|       11 |  941 | `	ph7_class_method *pMeth = 0;` |
|        - |  942 | `	ph7_hashmap *pMap;` |
|        - |  943 | `	ph7_value sNameVal,sArgsVal,sResult;` |
|        - |  944 | `	ph7_value *apCall[2];` |
|        - |  945 | `	SyString sMethName;` |
|        - |  946 | `	sxi32 rc;` |
|        - |  947 | `	int i;` |
|        - |  948 | `	/* Consume the pending dispatch (one-shot) */` |
|       11 |  949 | `	pVm->pMagicCallThis = 0;` |
|       11 |  950 | `	pVm->pMagicCallClass = 0;` |
|       11 |  951 | `	if( pClass == 0 ){` |
|        - |  952 | `		/* Not a magic dispatch (direct user invocation): no-op */` |
|      ! 0 |  953 | `		ph7_result_null(pCtx);` |
|      ! 0 |  954 | `		return PH7_OK;` |
|        - |  955 | `	}` |
|       16 |  956 | `	pMeth = PH7_ClassExtractMethod(pClass,` |
|        5 |  957 | `		pRecv ? "__call" : "__callStatic",` |
|        5 |  958 | `		pRecv ? sizeof("__call")-1 : sizeof("__callStatic")-1);` |
|       11 |  959 | `	if( pMeth == 0 ){` |
|        - |  960 | `		/* Unreachable: OP_MEMBER verified the handler exists */` |
|      ! 0 |  961 | `		if( pRecv ){` |
|      ! 0 |  962 | `			PH7_ClassInstanceUnref(pRecv);` |
|      ! 0 |  963 | `		}` |
|      ! 0 |  964 | `		ph7_result_null(pCtx);` |
|      ! 0 |  965 | `		return PH7_OK;` |
|        - |  966 | `	}` |
|       11 |  967 | `	pMap = PH7_NewHashmap(pVm,0,0);` |
|       11 |  968 | `	if( pMap == 0 ){` |
|      ! 0 |  969 | `		if( pRecv ){` |
|      ! 0 |  970 | `			PH7_ClassInstanceUnref(pRecv);` |
|      ! 0 |  971 | `		}` |
|      ! 0 |  972 | `		PH7_VmMemoryError(pVm);` |
|      ! 0 |  973 | `		return PH7_ABORT;` |
|        - |  974 | `	}` |
|       25 |  975 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       15 |  976 | `		PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        8 |  977 | `	}` |
|       11 |  978 | `	SyStringInitFromBuf(&sMethName,SyBlobData(&pVm->sMagicCallName),SyBlobLength(&pVm->sMagicCallName));` |
|       11 |  979 | `	PH7_MemObjInitFromString(pVm,&sNameVal,&sMethName);` |
|       11 |  980 | `	sNameVal.nIdx = SXU32_HIGH;` |
|       11 |  981 | `	PH7_MemObjInitFromArray(pVm,&sArgsVal,pMap);` |
|       11 |  982 | `	sArgsVal.nIdx = SXU32_HIGH;` |
|       11 |  983 | `	PH7_MemObjInit(pVm,&sResult);` |
|       11 |  984 | `	apCall[0] = &sNameVal;` |
|       11 |  985 | `	apCall[1] = &sArgsVal;` |
|       11 |  986 | `	rc = PH7_VmCallClassMethod(pVm,pRecv,pMeth,&sResult,2,apCall);` |
|       11 |  987 | `	if( rc == SXRET_OK ){` |
|        9 |  988 | `		ph7_result_value(pCtx,&sResult);` |
|        4 |  989 | `	}` |
|       11 |  990 | `	PH7_MemObjRelease(&sResult);` |
|       11 |  991 | `	PH7_MemObjRelease(&sNameVal);` |
|       11 |  992 | `	PH7_MemObjRelease(&sArgsVal); /* drops the packed array */` |
|       11 |  993 | `	if( pRecv ){` |
|        9 |  994 | `		PH7_ClassInstanceUnref(pRecv);` |
|        4 |  995 | `	}` |
|       11 |  996 | `	SyBlobReset(&pVm->sMagicCallName);` |
|       11 |  997 | `	return (rc == PH7_EXCEPTION \|\| rc == PH7_ABORT) ? rc : PH7_OK;` |
|        6 |  998 | `}` |
|        - |  999 |  |
