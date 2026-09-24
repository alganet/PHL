/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdlib.h> /* realpath, free */
/*
 * Section:
 *    Dynamic code loading: VmEvalChunk, eval(), the include path
 *    machinery, VmExecIncludedFile and the include/require family, the
 *    spl_autoload group and the __call/__callStatic packing body.
 *    Registration rows stay in vm.c's aVmFunc[].
 * Status:
 *    Stable.
 */
/*
 * Compile and evaluate a PHP chunk at run-time.
 * Refer to the eval() language construct implementation for more
 * information.
 */
PH7_PRIVATE sxi32 VmEvalChunk(
	ph7_vm *pVm,        /* Underlying Virtual Machine */
	ph7_context *pCtx,  /* Call Context */
	SyString *pChunk,   /* PHP chunk to evaluate */
	int iFlags,         /* Compile flag */
	int bTrueReturn     /* TRUE to return execution result */
	)
{
	SySet *pByteCode,aByteCode;
	ProcConsumer xErr = 0;
	void *pErrData = 0;
	ph7_gen_state sSavedGen;
	int bNested;
	sxi32 rcThrow = SXRET_OK; /* a ParseError raised for a failed compile, or a throw the
	                           * evaluated chunk raised — either way the caller must unwind */
	/* Initialize bytecode container */
	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));
	SySetAlloc(&aByteCode,0x20);
	/* Reset the code generator */
	if( bTrueReturn ){
		/* Included file,log compile-time errors */
		xErr = pVm->pEngine->xConf.xErr;
		pErrData = pVm->pEngine->xConf.pErrData;
	}
	/* A non-zero cursor means an OUTER compile is in flight — this eval/include
	 * was reached from inside it (an autoload fired while resolving a base class).
	 * Wiping the generator would corrupt that outer parse, so snapshot it and hand
	 * the nested unit a fresh one; a plain runtime eval/include just resets. */
	bNested = (pVm->sCodeGen.pIn != 0);
	if( bNested ){
		PH7_CompilerSaveState(pVm,&sSavedGen,xErr,pErrData);
	}else{
		PH7_ResetCodeGenerator(pVm,xErr,pErrData);
	}
	/* Swap bytecode container */
	pByteCode = pVm->pByteContainer;
	pVm->pByteContainer = &aByteCode;
	/* Compile the chunk */
	PH7_CompileScript(pVm,pChunk,iFlags);
	/* Record THIS unit's error count where the nested state restore below cannot
	 * wipe it — VmExecDeferredClass reads it to detect a failed re-compile. */
	pVm->nLastEvalErr = pVm->sCodeGen.nErr;
	if( pVm->sCodeGen.nErr > 0 ){
		/* Compilation error. php makes this a CATCHABLE ParseError for eval()
		 * ("syntax error, unexpected ..."), where PHL merely returned false —
		 * so `eval('bad syntax')` silently produced a value instead of throwing.
		 * include/require keep the false return: their parse error is a printed
		 * fatal, not an exception. */
		if( pCtx && !bTrueReturn ){
			SyBlob *pErr = &pVm->sCodeGen.sErrBuf;
			ph7_result_bool(pCtx,0);
			if( SyBlobLength(pErr) > 0 ){
				rcThrow = PH7_VmThrowException(pCtx,"ParseError","%.*s",
					(int)SyBlobLength(pErr),(const char *)SyBlobData(pErr));
			}else{
				rcThrow = PH7_VmThrowException(pCtx,"ParseError","syntax error");
			}
		}else if( pCtx ){
			ph7_result_bool(pCtx,0);
		}
	}else{
		/* Mount any newly defined classes. Skipped while the VM is still
		 * initializing (builtin chunks): mounting evaluates class-constant/
		 * static initializers and installs reference-table entries, and the
		 * runtime structures those need (apRefObj, the per-exec object pool
		 * baseline) do not exist before PH7_VmMakeReady — which mounts every
		 * class unconditionally anyway. */
		SyHashEntry *pEntry;
		ph7_class *pClass;
		ph7_value sResult; /* Return value */
		sxi32 rc;
		if( pVm->nMagic != PH7_VM_INIT ){
			SyHashResetLoopCursor(&pVm->hClass);
			while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){
				pClass = (ph7_class *)pEntry->pUserData;
				/* Only mount classes that haven't been mounted yet */
				if( !pClass->bMounted ){
					rc = VmMountUserClass(pVm,pClass);
					if( rc != SXRET_OK ){
						/* Mount failure (likely memory error) */
						if( pCtx ){
							ph7_result_bool(pCtx,0);
						}
						goto Cleanup;
					}
				}
			}
		}
		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){
			/* Out of memory */
			if( pCtx ){
				ph7_result_bool(pCtx,0);
			}
			goto Cleanup;
		}
		if( bTrueReturn ){
			/* php's include/require answer INT 1 when the file returned nothing
			 * of its own — not `true`. It is the value a script tests, stores
			 * and compares, and `include $f === true` was false on php and true
			 * here. */
			PH7_MemObjInitFromInt(pVm,&sResult,1);
		}else{
			/* Assume a null return value */
			PH7_MemObjInit(pVm,&sResult);
		}
		/* Execute the compiled chunk. eval()/include/require recurse in C here
		 * (VmLocalExec -> VmByteCodeExec) — a native re-entry bounded by
		 * nMaxNativeDepth in the wrapper, so a recursive include/eval hits the
		 * native-nesting fatal instead of overflowing the C stack. The PHP
		 * call-depth cap is OP_CALL-only (BYTECODE.md stage 5).
		 *
		 * The nested exec shares the caller's VM frame, so a throw the CALLER's own
		 * try catches runs that catch IN PLACE and comes back as a status — which was
		 * dropped here, and the statement php abandons then carried on after the catch
		 * had already run (`try { $r = eval('throw new E;'); echo "x"; } catch …`
		 * printed the catch AND the echo). Same rule and same guard as the match-arm /
		 * switch-case / property-default sites: compare the recorded-resume fields
		 * against a pre-exec SNAPSHOT, never against 0. */
		{
			const void *pResumeBefore = (const void *)pVm->pResumeFrame;
			const void *pInlineBefore = (const void *)pVm->pInlineInstr;
			rc = VmLocalExec(pVm,&aByteCode,&sResult,FALSE);
			if( pCtx ){
				/* Set the execution result */
				ph7_result_value(pCtx,&sResult);
			}
			PH7_MemObjRelease(&sResult);
			if( rc != PH7_ABORT && VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){
				rcThrow = PH7_EXCEPTION;
			}
		}
	}
Cleanup:
	/* Cleanup the mess left behind */
	pVm->pByteContainer = pByteCode;
	SySetRelease(&aByteCode);
	/* Restore the outer compile's generator state if this was a nested unit. */
	if( bNested ){
		PH7_CompilerRestoreState(pVm,&sSavedGen);
	}
	/* A ParseError raised above must reach the caller so the VM unwinds the rest
	 * of the statement; returning OK left `eval('bad'); echo 'x';` running the
	 * echo even though php had already thrown. */
	return rcThrow;
}
/*
 * Execute a deferred class declaration (OP_CLASS_DEFER — see the deferral
 * block comment in compile_class.c). At this point the declaration's
 * execution order has been honored: any spl_autoload_register() statement
 * that precedes it in the program has RUN, so each recorded dependency either
 * resolves (possibly by autoload, fired inside PH7_VmExtractClass) or is
 * genuinely missing. A missing one is reported through *ppMissing — the
 * OP_CLASS_DEFER dispatcher throws php's catchable
 * `Class/Interface/Trait "X" not found` Error. Once the dependencies resolve,
 * the captured chunk re-compiles through VmEvalChunk (which also mounts the
 * newly installed classes); a compile failure there (e.g. a body syntax error
 * whose diagnosis was deferred along with the declaration) has already been
 * reported through the engine's error consumer, so it aborts execution.
 * The site is idempotent: bDone short-circuits re-execution (a loop around an
 * anonymous class instantiates the same installed class, php's
 * one-class-per-site semantics).
 */
PH7_PRIVATE sxi32 VmExecDeferredClass(ph7_vm *pVm,VmDeferredClass *pDefer,VmDeferredReq **ppMissing)
{
	VmDeferredReq *aReq;
	sxu32 n;
	*ppMissing = 0;
	if( pDefer->bDone ){
		return SXRET_OK;
	}
	aReq = (VmDeferredReq *)SySetBasePtr(&pDefer->aRequired);
	for( n = 0 ; n < SySetUsed(&pDefer->aRequired) ; ++n ){
		if( PH7_VmExtractClass(&(*pVm),aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0) == 0 ){
			*ppMissing = &aReq[n];
			return SXRET_OK; /* caller throws */
		}
	}
	if( pDefer->sAnonName.nByte > 0 ){
		pVm->sDeferAnonName = pDefer->sAnonName;
	}
	VmEvalChunk(&(*pVm),0,&pDefer->sText,PH7_PHP_ONLY,TRUE);
	pVm->sDeferAnonName.zString = 0;
	pVm->sDeferAnonName.nByte = 0;
	if( pVm->nLastEvalErr > 0
	 || PH7_VmExtractClass(&(*pVm),pDefer->sSelfName.zString,pDefer->sSelfName.nByte,FALSE,0) == 0 ){
		/* The re-compile failed — a deferred-along syntax error or a
		 * redeclaration fatal. It was already reported through the engine's
		 * error consumer; halt like a fatal (php exits 255 for both). */
		pVm->iExitStatus = 255;
		return SXERR_ABORT;
	}
	pDefer->bDone = 1;
	return SXRET_OK;
}
/*
 * value eval(string $code)
 *   Evaluate a string as PHP code.
 * Parameter
 *  code: PHP code to evaluate.
 * Return
 *  eval() returns NULL unless return is called in the evaluated code, in which case
 *  the value passed to return is returned. If there is a parse error in the evaluated
 *  code, eval() returns FALSE and execution of the following code continues normally.
 */
PH7_PRIVATE int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sChunk;    /* Chunk to evaluate */
	sxi32 rc;
	if( nArg < 1 ){
		/* Nothing to evaluate,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Chunk to evaluate. Coerced USER-VISIBLY like every other string argument
	 * spelled as a language construct: an object with no __toString() is php's
	 * Error, where PHL used to hand the parser the literal "Object" and answer a
	 * ParseError. */
	{
		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sChunk.zString,(int *)&sChunk.nByte);
		if( rcSv != SXRET_OK ){
			return rcSv;
		}
	}
	if( sChunk.nByte < 1 ){
		/* php: eval('') compiles nothing and yields FALSE (a whitespace-only
		 * chunk still compiles, and yields NULL through the normal path). */
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Eval the chunk */
	rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);
	if( pCtx->pVm->bHaltRequested ){
		/* exit/die inside the evaluated chunk: cascade the halt */
		return PH7_ABORT;
	}
	/* Propagate a ParseError from a failed compile (php unwinds; PHL used to
	 * keep executing the statement that contained the eval). */
	return rc;
}
/*
 * Check if a file path is already included.
 */
static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)
{
	SyString *aEntries;
	sxu32 n;
	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);
	/* Perform a linear search */
	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){
		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){
			/* Already included */
			return TRUE;
		}
	}
	return FALSE;
}
/*
 * Push a file path in the appropriate VM container.
 */
PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)
{
	SyString sPath;
	char *zDup;
	sxi32 rc;
	if( nLen < 0 ){
		nLen = SyStrlen(zPath);
	}
	/* Duplicate the file path first */
	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);
	if( zDup == 0 ){
		return SXERR_MEM;
	}
#ifdef __UNIXES__
	/* php records the REALPATH'd absolute path for __FILE__/__DIR__/getFileName
	 * and include-once dedup: realpath() resolves '.', '..' and symlinks (e.g.
	 * macOS /tmp -> /private/tmp). It needs an existing file, so on failure keep
	 * the raw path (eval'd code, php:///data:// wrappers, a missing include that
	 * errors elsewhere). */
	{
		char *zReal = realpath(zDup,0); /* POSIX: malloc'd result */
		if( zReal ){
			sxu32 nReal = SyStrlen(zReal);
			char *zRealDup = SyMemBackendStrDup(&pVm->sAllocator,zReal,nReal);
			free(zReal);
			if( zRealDup ){
				SyMemBackendFree(&pVm->sAllocator,zDup);
				zDup = zRealDup;
				nLen = (int)nReal;
			}
		}
	}
#endif
#ifdef __WINNT__
	/* Windows counterpart of the realpath() above: canonicalize to an absolute
	 * path (resolves '.'/'..' , '/' -> '\'), case-preserved to match php — NOT
	 * lowercased as the legacy PH7 code did. Like the POSIX branch, keep the raw
	 * path when it does not name an existing file (the "Command line code" marker,
	 * eval'd code, stream wrappers). _fullpath() is lexical, so pair it with a VFS
	 * existence check. */
	{
		char *zFull = _fullpath(0,zDup,0); /* MSVC CRT: malloc'd absolute path */
		if( zFull ){
			if( pVm->pEngine->pVfs && pVm->pEngine->pVfs->xFileExists && pVm->pEngine->pVfs->xFileExists(zFull) == PH7_OK ){
				sxu32 nFull = SyStrlen(zFull);
				char *zFullDup = SyMemBackendStrDup(&pVm->sAllocator,zFull,nFull);
				if( zFullDup ){
					SyMemBackendFree(&pVm->sAllocator,zDup);
					zDup = zFullDup;
					nLen = (int)nFull;
				}
			}
			free(zFull);
		}
	}
#endif
	/* Install the file path */
	SyStringInitFromBuf(&sPath,zDup,nLen);
	if( !bMain ){
		if( VmIsIncludedFile(&(*pVm),&sPath) ){
			/* Already included */
			*pNew = 0;
		}else{
			/* Insert in the corresponding container */
			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);
			if( rc != SXRET_OK ){
				SyMemBackendFree(&pVm->sAllocator,zDup);
				return rc;
			}
			*pNew = 1;
		}
	}
	SySetPut(&pVm->aFiles,(const void *)&sPath);
	return SXRET_OK;
}
/*
 * Compile and Execute a PHP script at run-time.
 * SXRET_OK is returned on sucessful evaluation.Any other return values
 * indicates failure.
 * Note that the PHP script to evaluate can be a local or remote file.In
 * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying
 * operations.
 * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then
 * this function is a no-op.
 * Refer to the implementation of the include(),include_once() language
 * constructs for more information.
 */
static sxi32 VmExecIncludedFile(
	 ph7_context *pCtx, /* Call Context */
	 SyString *pPath,   /* Script path or URL*/
	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */
	 )
{
	sxi32 rc;
#ifndef PH7_DISABLE_BUILTIN_FUNC
	const ph7_io_stream *pStream;
	SyBlob sContents;
	void *pHandle;
	ph7_vm *pVm;
	int isNew;
	/* Initialize fields */
	pVm = pCtx->pVm;
	SyBlobInit(&sContents,&pVm->sAllocator);
	isNew = 0;
	/* Extract the associated stream */
	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);
	/*
	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]
	 * in a read-only mode.
	 */
	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew,ph7_function_name(pCtx));
	if( pHandle == 0 ){
		return SXERR_IO;
	}
	rc = SXRET_OK; /* Stupid cc warning */
	if( IncludeOnce && !isNew ){
		/* Already included */
		rc = SXERR_EXISTS;
	}else{
		/* Read the whole file contents */
		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);
		if( rc == SXRET_OK ){
			SyString sScript;
			/* Compile and execute the script. A throw the included file raised — and the
			 * INCLUDING statement's own try caught in place — comes back as PH7_EXCEPTION
			 * and travels out to the include builtin's caller, which unwinds the rest of
			 * the statement instead of resuming it. It is not an IO failure, so the
			 * callers must tell the two apart before warning. */
			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));
			rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);
			if( rc != PH7_EXCEPTION ){
				rc = SXRET_OK;
			}
		}
	}
	/* Pop from the set of included file */
	(void)SySetPop(&pVm->aFiles);
	/* Close the handle */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Release the working buffer */
	SyBlobRelease(&sContents);
#else
	SXUNUSED(pCtx); /* cc warning */
	SXUNUSED(pPath);
	SXUNUSED(IncludeOnce);
	rc = SXERR_IO;
#endif /* PH7_DISABLE_BUILTIN_FUNC */
	return rc;
}
/*
 * string get_include_path(void)
 *  Gets the current include_path configuration option.
 * Parameter
 *  None
 * Return
 *  Included paths as a string
 */
PH7_PRIVATE int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyString *aEntry;
	int dir_sep;
	sxu32 n;
#ifdef __WINNT__
	dir_sep = ';';
#else
	/* Assume UNIX path separator */
	dir_sep = ':';
#endif
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* Point to the list of import paths */
	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);
	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){
		SyString *pEntry = &aEntry[n];
		if( n > 0 ){
			/* Append dir seprator */
			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));
		}
		/* Append path */
		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);
	}
	return PH7_OK;
}
/*
 * string|false set_include_path(string $include_path)
 *  Sets the include_path configuration option for the duration of the script,
 *  returning the OLD value (php contract).
 */
PH7_PRIVATE int vm_builtin_set_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SyString *aEntry;
	const char *zNew, *z, *zEnd;
	int dir_sep, nLen;
	sxu32 n;
#ifdef __WINNT__
	dir_sep = ';';
#else
	dir_sep = ':';
#endif
	/* Build the OLD include_path first: it is this call's return value */
	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);
	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){
		if( n > 0 ){ ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char)); }
		ph7_result_string(pCtx,aEntry[n].zString,(int)aEntry[n].nByte);
	}
	if( nArg < 1 ){
		return PH7_OK;
	}
	/* Replace the path set with the separated segments of the new value.
	 * Segments are duped into the VM allocator (reclaimed at VM teardown) so
	 * the SyString entries stay valid, mirroring the config-time literals. */
	zNew = ph7_value_to_string(apArg[0],&nLen);
	SySetReset(&pVm->aPaths);
	z = zNew; zEnd = &zNew[nLen];
	while( z < zEnd ){
		const char *zStart = z;
		SyString sPath;
		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }
		if( z > zStart ){
			char *zDup = (char *)SyMemBackendDup(&pVm->sAllocator,zStart,(sxu32)(z-zStart));
			if( zDup ){
				SyStringInitFromBuf(&sPath,zDup,(sxu32)(z-zStart));
#ifdef __WINNT__
				SyStringTrimTrailingChar(&sPath,'\\');
#endif
				SyStringTrimTrailingChar(&sPath,'/');
				SyStringFullTrim(&sPath);
				if( sPath.nByte > 0 ){
					SySetPut(&pVm->aPaths,(const void *)&sPath);
				}
			}
		}
		if( z < zEnd ){ z++; } /* skip the separator */
	}
	return PH7_OK;
}
/*
 * string get_get_included_files(void)
 *  Gets the current include_path configuration option.
 * Parameter
 *  None
 * Return
 *  Included paths as a string
 */
PH7_PRIVATE int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SySet *pFiles = &pCtx->pVm->aFiles;
	ph7_value *pArray,*pWorker;
	SyString *pEntry;
	int c,d;
	/* Create an array and a working value */
	pArray  = ph7_context_new_array(pCtx);
	pWorker = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pWorker == 0 ){
		/* Out of memory,return null */
		ph7_result_null(pCtx);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	c = d = '/';
#ifdef __WINNT__
	d = '\\';
#endif
	/* Iterate throw entries */
	SySetResetCursor(pFiles);
	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){
		const char *zBase,*zEnd;
		int iLen;
		/* reset the string cursor */
		ph7_value_reset_string_cursor(pWorker);
		/* Extract base name */
		zEnd = &pEntry->zString[pEntry->nByte - 1];
		/* Ignore trailing '/' */
		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c || (int)zEnd[0] == d ) ){
			zEnd--;
		}
		iLen = (int)(&zEnd[1]-pEntry->zString);
		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){
			zEnd--;
		}
		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;
		zEnd = &pEntry->zString[iLen];
		/* Copy entry name */
		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));
		/* Perform the insertion */
		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */
	}
	/* All done,return the created array */
	ph7_result_value(pCtx,pArray);
	/* Note that 'pWorker' will be automatically destroyed
	 * by the engine as soon we return from this foreign
	 * function.
	 */
	return PH7_OK;
}
/*
 * include:
 * According to the PHP reference manual.
 *  The include() function includes and evaluates the specified file.
 *  Files are included based on the file path given or, if none is given
 *  the include_path specified.If the file isn't found in the include_path
 *  include() will finally check in the calling script's own directory
 *  and the current working directory before failing. The include()
 *  construct will emit a warning if it cannot find a file; this is different
 *  behavior from require(), which will emit a fatal error.
 *  If a path is defined � whether absolute (starting with a drive letter
 *  or \ on Windows, or / on Unix/Linux systems) or relative to the current
 *  directory (starting with . or ..) � the include_path will be ignored altogether.
 *  For example, if a filename begins with ../, the parser will look in the parent
 *  directory to find the requested file.
 *  When a file is included, the code it contains inherits the variable scope
 *  of the line on which the include occurs. Any variables available at that line
 *  in the calling file will be available within the called file, from that point forward.
 *  However, all functions and classes defined in the included file have the global scope.
 */
PH7_PRIVATE int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sFile;
	sxi32 rc;
	if( nArg < 1 ){
		/* Nothing to evaluate,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* File to include. php coerces the path USER-VISIBLY, so an object with no
	 * __toString() is the catchable "could not be converted to string" Error --
	 * PHL used to include the literal path "Object" and warn about the IO error. */
	{
		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);
		if( rcSv != SXRET_OK ){
			return rcSv;
		}
	}
	if( sFile.nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);
	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){
		/* The included file THREW and the including statement's own try caught it in
		 * place: unwind the rest of that statement instead of resuming it, and say
		 * nothing — this is not an IO failure. A halt (exit/die in the file) still
		 * wins, so it is tested first. */
		return rc;
	}
	if( rc != SXRET_OK ){
		/* Emit a warning and return false */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);
		ph7_result_bool(pCtx,0);
	}
	if( pCtx->pVm->bHaltRequested ){
		/* exit/die inside the included file: cascade the halt */
		return PH7_ABORT;
	}
	return SXRET_OK;
}
/*
 * include_once:
 *  According to the PHP reference manual.
 *   The include_once() statement includes and evaluates the specified file during
 *   the execution of the script. This is a behavior similar to the include()
 *   statement, with the only difference being that if the code from a file has already
 *   been included, it will not be included again. As the name suggests, it will be included
 *   just once.
 */
PH7_PRIVATE int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sFile;
	sxi32 rc;
	if( nArg < 1 ){
		/* Nothing to evaluate,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* File to include. php coerces the path USER-VISIBLY, so an object with no
	 * __toString() is the catchable "could not be converted to string" Error --
	 * PHL used to include the literal path "Object" and warn about the IO error. */
	{
		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);
		if( rcSv != SXRET_OK ){
			return rcSv;
		}
	}
	if( sFile.nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);
	if( rc == SXERR_EXISTS ){
		/* File already included,return TRUE */
		ph7_result_bool(pCtx,1);
		return SXRET_OK;
	}
	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){
		/* The included file THREW and the including statement's own try caught it in
		 * place: unwind the rest of that statement instead of resuming it, and say
		 * nothing — this is not an IO failure. A halt (exit/die in the file) still
		 * wins, so it is tested first. */
		return rc;
	}
	if( rc != SXRET_OK ){
		/* Emit a warning and return false */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);
		ph7_result_bool(pCtx,0);
 	}
	if( pCtx->pVm->bHaltRequested ){
		/* exit/die inside the included file: cascade the halt */
		return PH7_ABORT;
	}
	return SXRET_OK;
}
/*
 * require.
 *  According to the PHP reference manual.
 *   require() is identical to include() except upon failure it will
 *   also produce a fatal level error.
 *   In other words, it will halt the script whereas include() only
 *   emits a warning  which allows the script to continue.
 */
PH7_PRIVATE int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sFile;
	sxi32 rc;
	if( nArg < 1 ){
		/* Nothing to evaluate,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* File to include. php coerces the path USER-VISIBLY, so an object with no
	 * __toString() is the catchable "could not be converted to string" Error --
	 * PHL used to include the literal path "Object" and warn about the IO error. */
	{
		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);
		if( rcSv != SXRET_OK ){
			return rcSv;
		}
	}
	if( sFile.nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);
	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){
		/* The included file THREW and the including statement's own try caught it in
		 * place: unwind the rest of that statement instead of resuming it, and say
		 * nothing — this is not an IO failure. A halt (exit/die in the file) still
		 * wins, so it is tested first. */
		return rc;
	}
	if( rc != SXRET_OK ){
		/* Fatal,abort VM execution immediately */
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);
		ph7_result_bool(pCtx,0);
		return PH7_ABORT;
	}
	if( pCtx->pVm->bHaltRequested ){
		/* exit/die inside the included file: cascade the halt */
		return PH7_ABORT;
	}
	return SXRET_OK;
}
/*
 * require_once:
 *  According to the PHP reference manual.
 *   The require_once() statement is identical to require() except PHP will check
 *   if the file has already been included, and if so, not include (require) it again.
 *   See the include_once() documentation for information about the _once behaviour
 *   and how it differs from its non _once siblings.
 */
PH7_PRIVATE int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sFile;
	sxi32 rc;
	if( nArg < 1 ){
		/* Nothing to evaluate,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* File to include. php coerces the path USER-VISIBLY, so an object with no
	 * __toString() is the catchable "could not be converted to string" Error --
	 * PHL used to include the literal path "Object" and warn about the IO error. */
	{
		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&sFile.zString,(int *)&sFile.nByte);
		if( rcSv != SXRET_OK ){
			return rcSv;
		}
	}
	if( sFile.nByte < 1 ){
		/* Empty string,return NULL */
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);
	if( rc == SXERR_EXISTS ){
		/* File already included,return TRUE */
		ph7_result_bool(pCtx,1);
		return SXRET_OK;
	}
	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){
		/* The included file THREW and the including statement's own try caught it in
		 * place: unwind the rest of that statement instead of resuming it, and say
		 * nothing — this is not an IO failure. A halt (exit/die in the file) still
		 * wins, so it is tested first. */
		return rc;
	}
	if( rc != SXRET_OK ){
		/* Fatal,abort VM execution immediately */
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);
		ph7_result_bool(pCtx,0);
		return PH7_ABORT;
	}
	if( pCtx->pVm->bHaltRequested ){
		/* exit/die inside the included file: cascade the halt */
		return PH7_ABORT;
	}
	return SXRET_OK;
}
/* Getopt builtins moved to vm_builtin_getopt.c */
/* JSON encoding/decoding routines moved to vm_json.c */
/* XML processing and UTF-8 routines moved to vm_xml.c */
/*
 * Section:
 *  SPL Autoloading functions.
 * Status:
 *  Stable.
 */
/*
 * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])
 *  Register given function as __autoload() implementation.
 * Parameters
 *  callback
 *   The autoload function being registered. If no parameter is provided,
 *   then the default implementation of spl_autoload() will be registered.
 *  throw
 *   This parameter specifies whether spl_autoload_register() should throw
 *   exceptions on error. (Ignored in this implementation — always succeeds.)
 *  prepend
 *   If true, spl_autoload_register() will prepend the autoloader on the
 *   autoload stack instead of appending it.
 * Return
 *  TRUE on success, FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	VmAutoloadCB sEntry;
	ph7_vm *pVm = pCtx->pVm;
	int iPrepend = 0;
	sxu32 n;
	if( nArg < 1 ){
		/* No callback provided — register default spl_autoload.
		 * Store the string "spl_autoload" as the callback. */
		/* Check for duplicates first */
		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){
			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)
				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1
				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){
				ph7_result_bool(pCtx,1);
				return SXRET_OK;
			}
		}
		SyZero(&sEntry,sizeof(VmAutoloadCB));
		PH7_MemObjInit(pVm,&sEntry.sCallback);
		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);
		SySetPut(&pVm->aAutoload,(const void *)&sEntry);
		ph7_result_bool(pCtx,1);
		return SXRET_OK;
	}
	/* Validate that the callback is callable */
	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){
		int iThrow = 1; /* Default: throw on error */
		if( nArg >= 2 ){
			iThrow = ph7_value_to_bool(apArg[1]);
		}
		if( iThrow ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Argument is not callable");
		}
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* Check for duplicates */
	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){
		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){
			/* Already registered */
			ph7_result_bool(pCtx,1);
			return SXRET_OK;
		}
	}
	/* Check prepend flag */
	if( nArg >= 3 ){
		iPrepend = ph7_value_to_bool(apArg[2]);
	}
	/* Store the callback */
	SyZero(&sEntry,sizeof(VmAutoloadCB));
	PH7_MemObjInit(pVm,&sEntry.sCallback);
	PH7_MemObjStore(apArg[0],&sEntry.sCallback);
	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){
		/* Prepend: shift existing entries and insert at position 0.
		 * We do this by appending first, then rotating the array. */
		sxu32 nTotal = SySetUsed(&pVm->aAutoload);
		VmAutoloadCB *aBase;
		SySetPut(&pVm->aAutoload,(const void *)&sEntry);
		/* Rotate: move last entry to front */
		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);
		if( aBase ){
			VmAutoloadCB sTemp;
			sxu32 i;
			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));
			for( i = nTotal ; i > 0 ; i-- ){
				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));
			}
			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));
		}
	}else{
		SySetPut(&pVm->aAutoload,(const void *)&sEntry);
	}
	ph7_result_bool(pCtx,1);
	return SXRET_OK;
}
/*
 * bool spl_autoload_unregister(callable $callback)
 *  Unregister a given function as __autoload() implementation.
 * Parameters
 *  callback
 *   The autoload function being unregistered.
 * Return
 *  TRUE on success, FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 n,nEntry;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	nEntry = SySetUsed(&pVm->aAutoload);
	for( n = 0 ; n < nEntry ; ++n ){
		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){
			/* Found — remove by shifting remaining entries down */
			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);
			sxu32 i;
			PH7_MemObjRelease(&pEntry->sCallback);
			for( i = n ; i + 1 < nEntry ; i++ ){
				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));
			}
			/* Pop the now-duplicate tail entry via the SySet API */
			SySetPop(&pVm->aAutoload);
			ph7_result_bool(pCtx,1);
			return SXRET_OK;
		}
	}
	ph7_result_bool(pCtx,0);
	return SXRET_OK;
}
/*
 * array spl_autoload_functions(void)
 *  Return all registered __autoload() functions.
 * Return
 *  An array of all registered autoload functions. If no function is registered,
 *  an empty array is returned.
 */
PH7_PRIVATE int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pArray;
	sxu32 n,nEntry;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	nEntry = SySetUsed(&pVm->aAutoload);
	for( n = 0 ; n < nEntry ; ++n ){
		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
		if( pEntry ){
			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);
		}
	}
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])
 *  Default implementation of __autoload().
 *  Converts namespace separators to directory separators, lowercases the class
 *  name, and tries to include a file with each of the given extensions.
 * Parameters
 *  class
 *   The class name being searched.
 *  file_extensions
 *   Comma-separated list of file extensions to try.
 */
PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zClass,*zExt,*zEnd,*zCur;
	SyBlob sPath;
	int nClass;
	sxi32 rc;
	if( nArg < 1 ){
		return SXRET_OK;
	}
	zClass = ph7_value_to_string(apArg[0],&nClass);
	if( nClass < 1 ){
		return SXRET_OK;
	}
	/* Default extensions */
	zExt = ".php,.inc";
	if( nArg >= 2 ){
		int nExt;
		zExt = ph7_value_to_string(apArg[1],&nExt);
		if( nExt < 1 ){
			zExt = ".php,.inc";
		}
	}
	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);
	/* Iterate over comma-separated extensions */
	zEnd = zExt + SyStrlen(zExt);
	zCur = zExt;
	while( zCur < zEnd ){
		const char *zComma;
		SyString sFile;
		int i;
		/* Find next comma or end */
		zComma = zCur;
		while( zComma < zEnd && *zComma != ',' ){
			zComma++;
		}
		/* Build path: lowercase class name with \ -> / , then append extension */
		SyBlobReset(&sPath);
		for( i = 0 ; i < nClass ; i++ ){
			char c = zClass[i];
			if( c == '\\' ){
				c = '/';
			}else if( c >= 'A' && c <= 'Z' ){
				c = c + ('a' - 'A');
			}
			SyBlobAppend(&sPath,(const void *)&c,1);
		}
		/* Append extension */
		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));
		/* NUL-terminate: the include path flows down to PH7_StreamOpenHandle,
		 * which does SyStrlen() on it as a C string. SyBlobAppend does not add a
		 * terminator, so without this the strlen reads past the buffer (a
		 * heap-buffer-overflow whose visibility depends on heap layout). The NUL
		 * is not counted in SyBlobLength(), so the SyString length stays correct. */
		SyBlobNullAppend(&sPath);
		/* Try to include the file */
		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));
		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);
		if( rc == SXRET_OK || rc == PH7_EXCEPTION ){
			/* Included — or it threw, which ends the search too: the remaining
			 * extensions are not tried after a file has already run. */
			SyBlobRelease(&sPath);
			return rc == PH7_EXCEPTION ? rc : SXRET_OK;
		}
		/* Move past the comma */
		zCur = zComma;
		if( zCur < zEnd && *zCur == ',' ){
			zCur++;
		}
	}
	SyBlobRelease(&sPath);
	return SXRET_OK;
}
/* Table of built-in VM functions. */
/*
 * Packing body for __call / __callStatic (band A #3b).
 * OP_MEMBER, on a missing or inaccessible method whose class declares the magic
 * handler, stashes {receiver, class, original name} on the VM and marks the callee
 * slot MEMOBJ_AUX_MAGICCALL; the normal OP_CALL machinery then collects the
 * ORIGINAL argument list (incl. spreads) and hands it here, which packs it into a
 * php array and invokes
 *   $recv->__call($name, $args)   /   Class::__callStatic($name, $args)
 * returning the handler's value as the call's result. A throw propagates via the
 * returned status (and the boundary rail).
 *
 * This used to be a REGISTERED host function named `__phl_magic_call` whose name
 * the four OP_MEMBER sites wrote into the callee slot — so the engine's own
 * dispatch was spelled as a global PHP function that function_exists() and
 * get_defined_functions() both reported, and that any script could call. It is
 * reached through the VM's own function record now (PH7_VmMagicCallFunc); the
 * mark on the slot is the only thing that selects it, and there is no name.
 */
static int VmMagicCallDispatch(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pRecv = pVm->pMagicCallThis;
	ph7_class *pClass = pVm->pMagicCallClass;
	ph7_value sResult;
	SyString sMethName;
	sxi32 rc;
	/* Consume the pending dispatch (one-shot) */
	pVm->pMagicCallThis = 0;
	pVm->pMagicCallClass = 0;
	if( pClass == 0 ){
		/* Defensive: the mark and the latch are set together by OP_MEMBER, so a
		 * classless arrival is unreachable. It was reachable while this body wore a
		 * function NAME — anyone could call `__phl_magic_call()` — which is exactly
		 * what the mark retired. */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	SyStringInitFromBuf(&sMethName,SyBlobData(&pVm->sMagicCallName),SyBlobLength(&pVm->sMagicCallName));
	PH7_MemObjInit(pVm,&sResult);
	/* The one packing site, shared with every CALLABLE spelling of the same call
	 * (PH7_VmDispatchMagicCall, vm_builtin_call.c): the handler lookup, the $args array
	 * — named arguments keyed by name, as php keys them — and the engine-dispatch
	 * visibility rule all live there. Two copies of this is how the syntax path and the
	 * callable path came to disagree about the argument names in the first place.
	 * SXERR_NOTFOUND is unreachable: OP_MEMBER verified the handler exists. */
	rc = PH7_VmDispatchMagicCall(pVm,pClass,pRecv,SyStringData(&sMethName),
		SyStringLength(&sMethName),&sResult,nArg,apArg,pCtx->pArgMap);
	if( rc == SXRET_OK ){
		ph7_result_value(pCtx,&sResult);
	}
	PH7_MemObjRelease(&sResult);
	if( pRecv ){
		PH7_ClassInstanceUnref(pRecv);
	}
	SyBlobReset(&pVm->sMagicCallName);
	return (rc == PH7_EXCEPTION || rc == PH7_ABORT) ? rc : PH7_OK;
}
/*
 * The function record OP_CALL dispatches a MEMOBJ_AUX_MAGICCALL callee slot through,
 * built on first use and owned by the VM (the allocator frees it with everything else).
 *
 * It is deliberately NOT installed in pVm->hHostFunction: the same property that makes
 * a native class method unreachable except by dispatching the method (see
 * PH7_NativeClassInstallMethod) makes this body unreachable except by the engine's own
 * __call routing. It carries no signature and no arity bounds, so the OP_CALL choke
 * point's ZPP and ArgumentCountError screens are inert for it — the handler's OWN
 * declared parameters are what php enforces, and it is a PHP method with a frame of its
 * own. sName is a diagnostic label only; nothing that reads it can be reached from here.
 */
PH7_PRIVATE ph7_user_func * PH7_VmMagicCallFunc(ph7_vm *pVm)
{
	SyString sName;
	if( pVm->pMagicCallFunc == 0 ){
		SyStringInitFromBuf(&sName,"__call",sizeof("__call")-1);
		if( PH7_NewForeignFunction(&(*pVm),&sName,VmMagicCallDispatch,0,
			&pVm->pMagicCallFunc) != SXRET_OK ){
			pVm->pMagicCallFunc = 0;
		}
	}
	return pVm->pMagicCallFunc;
}
