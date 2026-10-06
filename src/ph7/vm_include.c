/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdlib.h> /* realpath, free */
#include <errno.h>
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
 * php prints a compile-time FATAL where it is raised, and it is not catchable.
 * eval() compiles with the generator's logging OFF -- its PARSE errors are a
 * ParseError the caller may catch, and printing them there would double the
 * diagnostic -- so a refusal of E_ERROR severity has to reach the screen from
 * here instead. Formatted exactly as PH7_GenCompileError would have: the bare
 * text, then the file and line of the offence.
 */
static void VmReportCompileFatal(ph7_vm *pVm,SyBlob *pMsg,sxu32 nLine)
{
	SyBlob sOut;
	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( pVm->pEngine->xConf.xErr == 0 || SyBlobLength(pMsg) < 1 ){
		return;
	}
	SyBlobInit(&sOut,&pVm->sAllocator);
	SyBlobAppend(&sOut,SyBlobData(pMsg),SyBlobLength(pMsg));
	if( pFile ){
		SyBlobFormat(&sOut," in %.*s on line %u",(int)pFile->nByte,pFile->zString,nLine);
	}
	PH7_GenAppendFatalTrace(&(*pVm),&sOut,PH7_FATAL_TRACE_COMPILE);
	/* The label, php's two copies, the error_reporting() gate and
	 * error_get_last() are the shared compile-diagnostic emitter's, so eval()'s
	 * fatal cannot drift from the compiler's own. E_COMPILE_ERROR is php's bit
	 * for it, and the BARE sentence is what pMsg still holds. */
	PH7_VmEmitCompileDiagnostic(&(*pVm),64 /* E_COMPILE_ERROR */,"Fatal error",
		(const char *)SyBlobData(&sOut),SyBlobLength(&sOut),
		(const char *)SyBlobData(pMsg),SyBlobLength(pMsg),nLine);
	SyBlobRelease(&sOut);
}
/*
 * Record an ACTIVE include/require/eval so a backtrace can show it: php gives each
 * one a frame of its own between the loaded unit's frames and the caller's. Nothing
 * else in this engine records it -- an include shares its caller's variable scope,
 * so it pushes no VmFrame.
 *
 * pPath is the unit being loaded (php's single argument for the frame) and is empty
 * for eval(), which php shows argument-less. The call SITE is read here rather than
 * later for the same reason a frame's is (see VmEnterFrame): the include stack moves
 * on, and a `try` block's frame carries no function to read a file from.
 */
PH7_PRIVATE void PH7_VmIncFramePush(ph7_vm *pVm,const char *zName,SyString *pPath)
{
	VmIncFrame sInc;
	VmFrame *pCaller = pVm->pFrame;
	SyString *pFile;
	SyZero(&sInc,sizeof(sInc));
	while( pCaller && pCaller->pParent
	    && (pCaller->iFlags & (VM_FRAME_EXCEPTION|VM_FRAME_CATCH)) ){
		pCaller = pCaller->pParent;
	}
	sInc.pFrame = (void *)pCaller;
	sInc.nLine = pVm->nCurLine;
	sInc.zName = zName;
	/* The unit this construct is LOADING is already on the include stack by the
	 * time we get here, so it must not be mistaken for the one the construct is
	 * WRITTEN in: pop it for the question and put it back. */
	if( pPath ){
		SyString sTop = *(SyString *)SySetPeek(&pVm->aFiles);
		(void)SySetPop(&pVm->aFiles);
		pFile = PH7_VmExecutingUnitFile(&(*pVm));
		if( pFile ){
			sInc.sFile = *pFile;
		}
		SySetPut(&pVm->aFiles,(const void *)&sTop);
	}else{
		pFile = PH7_VmExecutingUnitFile(&(*pVm));
		if( pFile ){
			sInc.sFile = *pFile;
		}
	}
	if( pPath ){
		sInc.sPath = *pPath;
	}
	SySetPut(&pVm->aIncFrame,(const void *)&sInc);
}
PH7_PRIVATE void PH7_VmIncFramePop(ph7_vm *pVm)
{
	(void)SySetPop(&pVm->aIncFrame);
}
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
	int bSavedUnitDecl = pVm->bUnitDecl;
	sxu32 nUnitDeclMark = SySetUsed(&pVm->aUnitDecl);
	sxu32 nHiddenMark = SySetUsed(&pVm->aHiddenClass);
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
	/* An include/require's PARSE error is php's catchable ParseError, thrown from
	 * the include and printed only if it goes uncaught -- so the generator must not
	 * print it. A refusal of E_ERROR severity is php's uncatchable compile fatal and
	 * still prints where it is raised. */
	pVm->sCodeGen.bParseThrows = (bTrueReturn && pCtx) ? 1 : 0;
	/* Swap bytecode container */
	pByteCode = pVm->pByteContainer;
	pVm->pByteContainer = &aByteCode;
	/* Compile the chunk, logging what it declares. The log is OFF while the chunk
	 * runs: what its statements declare at run time is theirs to keep, even when an
	 * outer unit this one was autoloaded from goes on to fail. */
	pVm->bUnitDecl = 1;
	PH7_CompileScript(pVm,pChunk,iFlags);
	pVm->bUnitDecl = 0;
	PH7_VmUnitDeclEnd(pVm,nUnitDeclMark,pVm->sCodeGen.nErr > 0);
	if( pVm->sCodeGen.nErr > 0 ){
		SySetTruncate(&pVm->aHiddenClass,nHiddenMark);
	}else{
		/* What php does not early-bind waits for its statement. */
		PH7_VmHideClasses(pVm,nHiddenMark);
	}
	/* Record THIS unit's error count where the nested state restore below cannot
	 * wipe it — VmExecDeferredClass reads it to detect a failed re-compile. */
	pVm->nLastEvalErr = pVm->sCodeGen.nErr;
	if( pVm->sCodeGen.nErr > 0 ){
		/* Compilation error. php makes this a CATCHABLE ParseError -- for eval()
		 * and for include/require alike -- where PHL merely returned false, so
		 * `eval('bad syntax')` silently produced a value and a broken include
		 * printed its diagnostic and then CARRIED ON, leaving a half-compiled unit
		 * behind for later code to trip over. A refusal of E_ERROR severity is
		 * php's uncatchable compile fatal instead: it has already printed itself,
		 * and the program stops here. */
		SyBlob *pErr = &pVm->sCodeGen.sErrBuf;
		sxu32 nErrLine = pVm->sCodeGen.nFirstErrLine;
		if( SyBlobLength(&pVm->sCodeGen.sFirstErr) > 0 ){
			pErr = &pVm->sCodeGen.sFirstErr;
		}
		if( pVm->sCodeGen.nFatal > 0 ){
			if( pCtx ){
				ph7_result_bool(pCtx,0);
			}
			if( !bTrueReturn ){
				/* eval(): the generator had no consumer, so say it here. */
				VmReportCompileFatal(pVm,&pVm->sCodeGen.sFirstErr,nErrLine);
			}
			/* php exits 255 and runs nothing else. The include builtins cascade a
			 * requested halt exactly as they do for an exit() inside the file. */
			pVm->iExitStatus = 255;
			pVm->bHaltRequested = 1;
			rcThrow = PH7_ABORT;
		}else if( pCtx ){
			/* php's ParseError names the OFFENDING file and line, not the line the
			 * include was written on -- the throw is stamped from nCurLine, so aim
			 * it at the refusal and put the caller's line back afterwards. */
			sxu32 nSaveLine = pVm->nCurLine;
			ph7_result_bool(pCtx,0);
			if( nErrLine > 0 ){
				pVm->nCurLine = nErrLine;
			}
			if( SyBlobLength(pErr) > 0 ){
				rcThrow = PH7_VmThrowException(pCtx,"ParseError","%.*s",
					(int)SyBlobLength(pErr),(const char *)SyBlobData(pErr));
			}else{
				rcThrow = PH7_VmThrowException(pCtx,"ParseError","syntax error");
			}
			pVm->nCurLine = nSaveLine;
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
		 * call-depth cap is OP_CALL-only.
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
	/* Hand back the call-site cache records this chunk's OP_CALLs claimed, BEFORE the
	 * instructions holding their indices disappear. */
	PH7_VmCallSiteReleaseChunk(pVm,&aByteCode);
	SySetRelease(&aByteCode);
	/* Restore the outer compile's generator state if this was a nested unit. */
	if( bNested ){
		PH7_CompilerRestoreState(pVm,&sSavedGen);
	}
	pVm->bUnitDecl = bSavedUnitDecl;
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
	sxu8 bHaltIn;
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
	pVm->bDeclQuietNext = pDefer->bChecked;
	bHaltIn = pVm->bHaltRequested;
	VmEvalChunk(&(*pVm),0,&pDefer->sText,PH7_PHP_ONLY,TRUE);
	pVm->bDeclQuietNext = 0;
	pVm->sDeferAnonName.zString = 0;
	pVm->sDeferAnonName.nByte = 0;
	if( pVm->bHaltRequested && !bHaltIn ){
		/* The re-compiled declaration RAN and halted -- a variance pair refused
		 * where it settles, a loader's throw made fatal there, an exit() in a
		 * loader. The statement after the declaration must not run. */
		return SXERR_ABORT;
	}
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
 * php names an eval()'d compilation unit after the SITE that evaluated it:
 * `<file>(<line>) : eval()'d code`, where <file> is the unit the eval() was
 * written in -- itself such a name when the eval is nested -- and <line> the line
 * it sits on. That name is not a decoration on one message: it is the unit's
 * identity, so `__FILE__`, every diagnostic's location, a Throwable's getFile(),
 * and ReflectionFunction/ReflectionClass::getFileName() for anything the chunk
 * declares all read it. PHL left the chunk sharing its caller's name, so a
 * template engine that compiles to PHP and evals it (twig, and every cache-less
 * renderer of that shape) reported positions in the COMPILER's file -- and the
 * caller could not map them back, because its own class had the compiler's name
 * on it too.
 *
 * Interned per site rather than per call: the name is copied by value into every
 * function and class record compiled out of the chunk, so it must outlive the
 * eval, and an eval inside a loop repeats one site.
 */
static sxi32 VmEvalUnitName(ph7_vm *pVm,SyString *pOut)
{
	SyString *pHost = PH7_VmExecutingUnitFile(&(*pVm));
	SyString *aName;
	SyString sKey;
	SyBlob sName;
	char *zDup;
	sxu32 n;
	SyBlobInit(&sName,&pVm->sAllocator);
	if( pHost && pHost->nByte > 0 ){
		SyBlobAppend(&sName,pHost->zString,pHost->nByte);
	}
	SyBlobFormat(&sName,"(%u) : eval()'d code",pVm->nCurLine);
	if( SyBlobLength(&sName) < 1 ){
		SyBlobRelease(&sName);
		return SXERR_MEM;
	}
	SyStringInitFromBuf(&sKey,SyBlobData(&sName),SyBlobLength(&sName));
	aName = (SyString *)SySetBasePtr(&pVm->aEvalFile);
	for( n = 0 ; n < SySetUsed(&pVm->aEvalFile) ; ++n ){
		if( SyStringCmp(&sKey,&aName[n],SyMemcmp) == 0 ){
			*pOut = aName[n];
			SyBlobRelease(&sName);
			return SXRET_OK;
		}
	}
	zDup = SyMemBackendStrDup(&pVm->sAllocator,sKey.zString,sKey.nByte);
	n = sKey.nByte;
	SyBlobRelease(&sName);
	if( zDup == 0 ){
		return SXERR_MEM;
	}
	SyStringInitFromBuf(&sKey,zDup,n);
	if( SySetPut(&pVm->aEvalFile,(const void *)&sKey) != SXRET_OK ){
		SyMemBackendFree(&pVm->sAllocator,zDup);
		return SXERR_MEM;
	}
	*pOut = sKey;
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
	/* Eval the chunk.
	 *
	 * php compiles it as though it began right after a `<?php`, which means a `?>`
	 * inside it LEAVES php mode: the text after it is echoed and a later `<?php`
	 * re-enters. `eval('?>' . file_get_contents($f))` is the ordinary way to run a
	 * php FILE's bytes, and composer reloads its own `vendor/composer/installed.php`
	 * exactly that way. PHL handed the whole chunk to the compiler as ONE php token
	 * (PH7_PHP_ONLY), so the `?>` was a syntax error and composer could not boot.
	 *
	 * Prepending the opening tag and letting the ordinary raw tokenizer split it is
	 * php's rule itself, with no second implementation of it. The prefix carries no
	 * newline, so every line still reports the number the caller wrote it on. */
	{
		SyBlob sTagged;
		SyString sTaggedStr;
		SyString sUnit;
		int bUnit;
		SyBlobInit(&sTagged,&pCtx->pVm->sAllocator);
		if( SyBlobAppend(&sTagged,"<?php ",sizeof("<?php ")-1) != SXRET_OK
		 || SyBlobAppend(&sTagged,sChunk.zString,sChunk.nByte) != SXRET_OK ){
			SyBlobRelease(&sTagged);
			return PH7_ContextMemoryError(pCtx);
		}
		SyStringInitFromBuf(&sTaggedStr,SyBlobData(&sTagged),SyBlobLength(&sTagged));
		/* php gives eval() a trace frame of its own, exactly as it does an include --
		 * argument-less, where an include names the unit it loaded. */
		/* Name the chunk BEFORE the trace frame goes on: PH7_VmIncFramePush records
		 * an entry whose pFrame is the CURRENT frame, and PH7_VmExecutingUnitFile
		 * answers such a frame from the include-stack TOP -- so asking after the push
		 * names the unit being loaded rather than the one holding the eval(), and a
		 * template engine's compiled chunk came out named after the SCRIPT instead of
		 * the library method that evaluated it. */
		bUnit = (VmEvalUnitName(pCtx->pVm,&sUnit) == SXRET_OK);
		PH7_VmIncFramePush(pCtx->pVm,"eval",0);
		/* Now push the chunk's own unit name. The include-frame entry is what makes
		 * the include stack answer for code running at the chunk's TOP level -- which
		 * pushes no VmFrame of its own -- so the two go together. */
		bUnit = bUnit && (SySetPut(&pCtx->pVm->aFiles,(const void *)&sUnit) == SXRET_OK);
		rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sTaggedStr,0,FALSE);
		if( bUnit ){
			(void)SySetPop(&pCtx->pVm->aFiles);
		}
		PH7_VmIncFramePop(pCtx->pVm);
		SyBlobRelease(&sTagged);
	}
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
	/* php's lint mode (`phl -l`) hands the compiler the file handle it opened
	 * under the name it was GIVEN -- it never runs the unit, so nothing needs the
	 * canonical name -- where a run expands the main script's path first. That is
	 * why a parse error names `./x.php` under `-l` and `/abs/dir/x.php` under a
	 * run, and it is the file name a lint diagnostic is matched against.
	 * Only the MAIN unit: an include compiled from a checked file would still
	 * want the canonical name, and lint mode compiles no includes anyway. */
	if( bMain && pVm->bSyntaxCheck ){
		goto Install;
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
	/* A `phar://` url has no realpath() to ask, so its own canonical form is
	 * built here: the archive half as the url spells it, the ENTRY half with its
	 * `.` and `..` segments collapsed the way the archive's reader already
	 * resolves them. Without it two spellings of one entry are two rows in the
	 * once-registry.
	 *
	 * Behind the same guard as ext/phar itself: vm_phar.c's whole body, and the
	 * prototype block this calls into, are `#ifndef PH7_DISABLE_BUILTIN_FUNC`, so
	 * the tiny build has no archive reader to ask and no phar:// url to ask about. */
#ifndef PH7_DISABLE_BUILTIN_FUNC
	{
		SyBlob sPhar;
		SyBlobInit(&sPhar,&pVm->sAllocator);
		if( PH7_PharCanonicalUrl(pVm,zDup,nLen,&sPhar) ){
			char *zPharDup = SyMemBackendStrDup(&pVm->sAllocator,
				(const char *)SyBlobData(&sPhar),SyBlobLength(&sPhar));
			if( zPharDup ){
				SyMemBackendFree(&pVm->sAllocator,zDup);
				zDup = zPharDup;
				nLen = (int)SyBlobLength(&sPhar);
			}
		}
		SyBlobRelease(&sPhar);
	}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
Install:
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
	 ph7_context *pCtx,   /* Call Context */
	 SyString *pPath,     /* Script path or URL*/
	 int IncludeOnce,     /* TRUE if called from include_once() or require_once() */
	 const char **pzWhy   /* OUT: php's reason for a failed open, for the caller's warning */
	 )
{
	sxi32 rc;
	if( pzWhy ){
		*pzWhy = "operation failed";
	}
#ifndef PH7_DISABLE_BUILTIN_FUNC
	const ph7_io_stream *pStream;
	SyBlob sContents;
	void *pHandle;
	ph7_vm *pVm;
	int isNew;
	int bFellBack;
	/* Initialize fields */
	pVm = pCtx->pVm;
	SyBlobInit(&sContents,&pVm->sAllocator);
	isNew = 0;
	bFellBack = 0;
	/* Extract the associated stream. The lookup ADVANCES the pointer past the
	 * scheme, so it walks a copy: advancing the caller's SyString left its nByte
	 * describing the whole url and its zString seven bytes in, and the failure
	 * message below then printed that many bytes from there -- past the end of the
	 * string, so `include 'file:///nope'` reported whatever followed it in memory. */
	{
		const char *zOpen = pPath->zString;
		pStream = PH7_VmGetStreamDevice(pVm,&zOpen,(int)pPath->nByte);
		if( pStream == 0 ){
			/* A scheme NOBODY is registered under is not a refusal in php: its
			 * lookup warns, forgets the protocol, and hands the WHOLE uri --
			 * scheme and all -- to the plain-files wrapper, which resolves it
			 * against the include_path like any other relative name. So
			 * `include 'zzz://hit.php'` warns once and then RUNS ./zzz:/hit.php,
			 * where this engine reported `Invalid argument` and included
			 * nothing. The two cases php words differently are excluded here and
			 * keep the failing path below: a `file://` with an authority php
			 * will not reach, and a `file://` the configuration switched off. */
			int nScheme = 0;
			if( PH7_VmStreamDeviceIsRemoteHost(pPath->zString,(int)pPath->nByte,&nScheme) ){
				/* `file://host/path`. A wrapper WAS found and declined the name,
				 * so php says so in its own sentence and gives the open a reason
				 * of the lookup's rather than an errno nothing set. The failed
				 * open's own line is the caller's, which is why only the first
				 * half is raised here. */
				PH7_VmThrowWarningFmt(pVm,"%s(): Remote host file access not supported, %.*s",
					ph7_function_name(pCtx),(int)pPath->nByte,pPath->zString);
				if( pzWhy ){
					*pzWhy = "no suitable wrapper could be found";
				}
				return SXERR_IO;
			}
			if( nScheme > 0
			 && !PH7_VmStreamSchemeDisabled(pVm,"file",(int)sizeof("file")-1) ){
				/* One sentence per wrapper lookup that still SEES the scheme.
				 * php resolves the path before it opens it -- and the _once
				 * forms resolve it once more, to answer whether it has already
				 * been included -- so a miss costs two sentences and a _once
				 * miss three. A resolve that succeeds hands an absolute plain
				 * path to everything after it, so a hit costs exactly one
				 * whichever construct asked. */
				VfsThrowUnknownWrapperWarning(pCtx,pPath->zString);
				pStream = PH7_VmFindStreamDevice(pVm,"file",(int)sizeof("file")-1);
				zOpen = pPath->zString;
				bFellBack = 1;
			}
		}
		/*
		 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]
		 * in a read-only mode.
		 */
		pHandle = PH7_StreamOpenHandle(pVm,pStream,zOpen,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew,ph7_function_name(pCtx));
	}
	if( pHandle == 0 ){
		/* The reason belongs to THIS open and nothing else: read it before any
		 * other stream operation can re-arm it. A wrapper that logged one of its
		 * own wins; the plain-file wrapper logs none and reports its errno. */
		if( bFellBack ){
			/* The lookups the failed resolve above did not spare: the open's
			 * own, plus the _once forms' extra resolve. And the reason is the
			 * plain-files wrapper's, not the scheme's -- php has stopped talking
			 * about the wrapper by the time it words the failure. */
			VfsThrowUnknownWrapperWarning(pCtx,pPath->zString);
			if( IncludeOnce ){
				VfsThrowUnknownWrapperWarning(pCtx,pPath->zString);
			}
			if( pzWhy ){
				*pzWhy = PH7_VfsOpenStrerror(ENOENT);
			}
			return SXERR_IO;
		}
		if( pzWhy ){
			*pzWhy = pVm->zOpenErr ? pVm->zOpenErr : PH7_VfsOpenStrerror(errno);
		}
		return SXERR_IO;
	}
	rc = SXRET_OK; /* Stupid cc warning */
	if( IncludeOnce && !isNew ){
		/* Already included */
		rc = SXERR_EXISTS;
	}else{
		/* Read the whole file contents */
		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);
		if( rc != SXRET_OK ){
			/* php refuses a target that is not a REGULAR file -- a directory is
			 * the one every script meets -- inside the open itself, so what a
			 * script reads is the open's generic reason and not the read's
			 * errno. `include '/etc'` says "No such file or directory" there
			 * and "Is a directory" here. */
			if( pzWhy ){
				*pzWhy = PH7_VfsOpenStrerror(ENOENT);
			}
		}
		if( rc == SXRET_OK ){
			SyString sScript;
			/* Compile and execute the script. A throw the included file raised — and the
			 * INCLUDING statement's own try caught in place — comes back as PH7_EXCEPTION
			 * and travels out to the include builtin's caller, which unwinds the rest of
			 * the statement instead of resuming it. It is not an IO failure, so the
			 * callers must tell the two apart before warning. */
			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));
			/* php shows the construct itself as a trace frame; the path it names is
			 * the RESOLVED one, which is what aFiles was just given. */
			PH7_VmIncFramePush(pVm,ph7_function_name(pCtx),
				(SyString *)SySetPeek(&pVm->aFiles));
			if( pCtx->pFunc && !pCtx->pFunc->bConstruct && pVm->pNativeCall
			 && pVm->pNativeCall->pName == &pCtx->pFunc->sName ){
				/* A builtin loading the unit itself (spl_autoload()) is a call, and
				 * its running record is the frame php shows -- not an include. */
				((VmIncFrame *)SySetPeek(&pVm->aIncFrame))->pNat = pVm->pNativeCall;
			}
			rc = VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);
			PH7_VmIncFramePop(pVm);
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
 * php keeps include_path in ONE place -- the INI table -- and get_include_path(),
 * ini_get('include_path'), ini_get_all() and the resolver all read that one string.
 * PHL had TWO: pVm->aPaths, which is what the include walk actually uses and which
 * only set_include_path() ever wrote, and the `include_path` INI slot, which is what
 * ini_get() answers and which only ini_set()/-d ever wrote. Neither told the other,
 * so `ini_set('include_path', $dir)` -- the ordinary way a bootstrap file points the
 * engine at a library -- moved NOTHING, and `set_include_path($dir)` left ini_get()
 * naming the value that was no longer in force. The set below is the single store;
 * the INI slot is a view of it (vm_builtin_ini.c).
 */
PH7_PRIVATE int PH7_VmIncludePathSep(void)
{
#ifdef __WINNT__
	return ';';
#else
	/* Assume UNIX path separator */
	return ':';
#endif
}
/*
 * Split an include_path STRING into its segments and make them the VM's set.
 *
 * Segments are kept VERBATIM. php hands back the string it was given, so a
 * trailing slash, a leading space and an EMPTY segment all survive the round trip
 * -- and the walk means something for each of them: php builds "<segment>/<file>"
 * from every entry, which is how an empty segment comes to try "/file".
 */
PH7_PRIVATE void PH7_VmSetIncludePath(ph7_vm *pVm,const char *zPath,sxu32 nByte)
{
	const char *z,*zEnd,*zDup;
	int dir_sep = PH7_VmIncludePathSep();
	SySetReset(&pVm->aPaths);
	if( nByte < 1 ){
		return;
	}
	/* ONE VM-lifetime copy backs every segment (the SyString entries alias it),
	 * where the old splitter duped each segment separately on every call. */
	zDup = (const char *)SyMemBackendDup(&pVm->sAllocator,zPath,nByte);
	if( zDup == 0 ){
		return;
	}
	z = zDup;
	zEnd = &zDup[nByte];
	for(;;){
		const char *zStart = z;
		SyString sPath;
		while( z < zEnd && (int)z[0] != dir_sep ){ z++; }
		SyStringInitFromBuf(&sPath,zStart,(sxu32)(z-zStart));
		SySetPut(&pVm->aPaths,(const void *)&sPath);
		if( z >= zEnd ){
			break;
		}
		z++; /* skip the separator */
	}
}
/*
 * php's LAST RESORT for a relative name the include_path did not answer: the
 * directory of the file that is EXECUTING, not the process's cwd. It is why
 * `include 'helper.php'` next to the script keeps working when the script was
 * started from somewhere else, and why a library's own relative includes
 * resolve at all. PHL had nothing of the kind, so
 *
 *   cd / && phl /srv/app/main.php   with   include 'lib.php'   next to main.php
 *
 * failed on this engine and ran on php.
 *
 * WHICH file is executing is the whole question, and the include-nesting stack
 * is the wrong answer to it. php reads the running op array's own filename
 * (`zend_get_executed_filename_ex()`), which is the file the include statement
 * is WRITTEN in -- so a method that says `require 'A.php'` looks beside the file
 * that declared the method. The include stack only agrees with that while a
 * unit's top-level code is running: it is popped as soon as an include returns,
 * so by the time the library's own function is CALLED the stack's top is the
 * entry script, and the fallback searched the caller's directory instead of the
 * library's. That is exactly the shape a composer package has -- one bootstrap
 * file, functions that pull siblings in by bare name -- and it is why a fixture
 * requiring its own sibling failed here. PH7_VmExecutingUnitFile is the engine's
 * answer to php's question, and every diagnostic's location already uses it.
 *
 * Answers 0 when that name has no directory part at all (`-r`'s "Command line
 * code"). php's own arithmetic degenerates to the bare, cwd-relative name
 * there, which the default include_path's "." entry already tries.
 */
PH7_PRIVATE int PH7_VmExecutingDir(ph7_vm *pVm,SyString *pOut)
{
	SyString *pFile = PH7_VmExecutingUnitFile(&(*pVm));
	sxu32 n;
	if( pFile == 0 || pFile->nByte < 2 ){
		return 0;
	}
	n = pFile->nByte;
	while( n > 0 ){
		int c = pFile->zString[n-1];
		if( c == '/'
#ifdef __WINNT__
		 || c == '\\'
#endif
		){
			break;
		}
		n--;
	}
	if( n < 2 ){
		/* No separator, or one sitting at index 0 -- php skips the fallback for
		 * a root-level script exactly the same way. */
		return 0;
	}
	SyStringInitFromBuf(pOut,pFile->zString,n-1); /* without the separator */
	return 1;
}
/*
 * Join the set back into php's one string. Splitting on the separator and
 * joining with it round-trips exactly, which is what ini_get() promises.
 */
PH7_PRIVATE void PH7_VmGetIncludePath(ph7_vm *pVm,SyBlob *pOut)
{
	SyString *aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);
	char cSep = (char)PH7_VmIncludePathSep();
	sxu32 n;
	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){
		if( n > 0 ){
			SyBlobAppend(pOut,(const void *)&cSep,sizeof(char));
		}
		SyBlobAppend(pOut,aEntry[n].zString,aEntry[n].nByte);
	}
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
	SyBlob sOut;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	PH7_VmGetIncludePath(pCtx->pVm,&sOut);
	/* php's answer is always a STRING: an empty set is "", never NULL, which is
	 * what this used to leave behind when nothing had ever been appended. */
	if( SyBlobLength(&sOut) < 1 ){
		ph7_result_string(pCtx,"",0);
	}else{
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	}
	SyBlobRelease(&sOut);
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
	const char *zNew;
	int nLen = 0;
	SyBlob sOld;
	if( nArg < 1 ){
		return PH7_OK;
	}
	/* The OLD value php answers is the effective one, read before the write. */
	SyBlobInit(&sOld,&pVm->sAllocator);
	PH7_VmGetIncludePath(pVm,&sOld);
	zNew = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* php registers include_path with OnUpdateStringUnempty, so the EMPTY
		 * string is refused outright: the directive keeps the value it had and
		 * the call answers FALSE. PHL used to accept it, wiping the set and
		 * leaving get_include_path() with nothing to answer. */
		SyBlobRelease(&sOld);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_VmSetIncludePath(pVm,zNew,(sxu32)nLen);
	if( SyBlobLength(&sOld) < 1 ){
		ph7_result_string(pCtx,"",0);
	}else{
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));
	}
	SyBlobRelease(&sOld);
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
 * php raises TWO diagnostics for an include it could not open, and this engine
 * raised one sentence of its own -- `include(): IO error while importing: 'x'`
 * -- which names neither the reason the open failed nor the include_path that
 * was searched, and which no handler written against php can recognise:
 *
 *   Warning: include(x.php): Failed to open stream: No such file or directory
 *   Warning: include(): Failed opening 'x.php' for inclusion (include_path='.')
 *
 * The first is the stream layer's, and is the same sentence fopen() raises for
 * the same failure; the second is the language construct's, and is the only one
 * that says where it looked. Both name the path AS WRITTEN.
 *
 * require's second diagnostic is not a warning at all. php 8 THROWS an Error --
 * `Failed opening required 'x.php' (include_path='.')`, no function prefix, no
 * "for inclusion" -- so a script may catch a missing dependency and carry on.
 * PHL reported it through the native fatal path, which ended the run and could
 * not be caught at all.
 */
static sxi32 VmIncludeFailure(ph7_context *pCtx,SyString *pFile,const char *zWhy,int bRequire)
{
	ph7_vm *pVm = pCtx->pVm;
	SyString sPath;
	SyBlob sIncPath;
	sxi32 rc = PH7_OK;
	/* The stream layer's sentence. Raised through the VM rather than the call
	 * context, because the context's own prefix is `name(): ` and php's here
	 * carries the path inside the parentheses. */
	PH7_VmThrowWarningFmt(pVm,"%s(%z): Failed to open stream: %s",
		ph7_function_name(pCtx),pFile,zWhy ? zWhy : "operation failed");
	SyBlobInit(&sIncPath,&pVm->sAllocator);
	PH7_VmGetIncludePath(pVm,&sIncPath);
	SyStringInitFromBuf(&sPath,(const char *)SyBlobData(&sIncPath),SyBlobLength(&sIncPath));
	if( bRequire ){
		rc = PH7_VmThrowException(pCtx,"Error",
			"Failed opening required '%z' (include_path='%z')",pFile,&sPath);
	}else{
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Failed opening '%z' for inclusion (include_path='%z')",pFile,&sPath);
	}
	SyBlobRelease(&sIncPath);
	return rc;
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
	const char *zWhy = 0;
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
	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){
		/* php's stream layer refuses an empty path wherever it is opened, and
		 * an include is an open: the ValueError is what a script catches, where
		 * this used to answer NULL and carry on. */
		return PH7_EXCEPTION;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE,&zWhy);
	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){
		/* The included file THREW and the including statement's own try caught it in
		 * place: unwind the rest of that statement instead of resuming it, and say
		 * nothing — this is not an IO failure. A halt (exit/die in the file) still
		 * wins, so it is tested first. */
		return rc;
	}
	if( rc != SXRET_OK ){
		VmIncludeFailure(pCtx,&sFile,zWhy,FALSE);
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
	const char *zWhy = 0;
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
	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){
		/* php's stream layer refuses an empty path wherever it is opened, and
		 * an include is an open: the ValueError is what a script catches, where
		 * this used to answer NULL and carry on. */
		return PH7_EXCEPTION;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE,&zWhy);
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
		VmIncludeFailure(pCtx,&sFile,zWhy,FALSE);
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
	const char *zWhy = 0;
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
	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){
		/* php's stream layer refuses an empty path wherever it is opened, and
		 * an include is an open: the ValueError is what a script catches, where
		 * this used to answer NULL and carry on. */
		return PH7_EXCEPTION;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE,&zWhy);
	if( rc == PH7_EXCEPTION && !pCtx->pVm->bHaltRequested ){
		/* The included file THREW and the including statement's own try caught it in
		 * place: unwind the rest of that statement instead of resuming it, and say
		 * nothing — this is not an IO failure. A halt (exit/die in the file) still
		 * wins, so it is tested first. */
		return rc;
	}
	if( rc != SXRET_OK ){
		/* php THROWS: the include is over, but the SCRIPT need not be. */
		sxi32 rcThrow = VmIncludeFailure(pCtx,&sFile,zWhy,TRUE);
		ph7_result_bool(pCtx,0);
		return rcThrow == PH7_OK ? PH7_EXCEPTION : rcThrow;
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
	const char *zWhy = 0;
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
	if( PH7_VfsEmptyPathRefused(pCtx,(int)sFile.nByte) ){
		/* php's stream layer refuses an empty path wherever it is opened, and
		 * an include is an open: the ValueError is what a script catches, where
		 * this used to answer NULL and carry on. */
		return PH7_EXCEPTION;
	}
	/* Open,compile and execute the desired script */
	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE,&zWhy);
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
		/* php THROWS: the include is over, but the SCRIPT need not be. */
		sxi32 rcThrow = VmIncludeFailure(pCtx,&sFile,zWhy,TRUE);
		ph7_result_bool(pCtx,0);
		return rcThrow == PH7_OK ? PH7_EXCEPTION : rcThrow;
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
 *   exceptions on error. Ignored, as in php 8: an uncallable callback is
 *   always a TypeError, and false only raises a notice saying so.
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
	ph7_value sDefault,*pCb;
	int iPrepend = 0;
	sxu32 n;
	PH7_MemObjInit(pVm,&sDefault);
	if( nArg < 1 || ph7_value_is_null(apArg[0]) ){
		/* No callback (or null): the default spl_autoload() implementation */
		PH7_MemObjStringAppend(&sDefault,"spl_autoload",sizeof("spl_autoload")-1);
		pCb = &sDefault;
	}else{
		/* php refuses an uncallable callback with a TypeError, whatever $throw says */
		sxi32 rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);
		if( rc != PH7_OK ){
			return rc;
		}
		pCb = apArg[0];
	}
	if( nArg >= 2 && !ph7_value_to_bool(apArg[1]) ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Argument #2 ($do_throw) has been ignored, spl_autoload_register() will always throw");
	}
	/* Check for duplicates */
	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){
		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,pCb,TRUE,0) == 0 ){
			/* Already registered */
			PH7_MemObjRelease(&sDefault);
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
	PH7_MemObjInit(pVm,&sEntry.sInvoke);
	PH7_MemObjStore(pCb,&sEntry.sCallback);
	PH7_VmBindCallbackScope(pVm,pCb,&sEntry.sInvoke);
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
	PH7_MemObjRelease(&sDefault);
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
	sxi32 rc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return SXRET_OK;
	}
	/* php refuses an uncallable argument before it looks the stack up */
	rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);
	if( rc != PH7_OK ){
		return rc;
	}
	nEntry = SySetUsed(&pVm->aAutoload);
	for( n = 0 ; n < nEntry ; ++n ){
		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);
		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){
			/* Found — remove by shifting remaining entries down */
			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);
			sxu32 i;
			PH7_MemObjRelease(&pEntry->sCallback);
			PH7_MemObjRelease(&pEntry->sInvoke);
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
 * string spl_autoload_extensions([?string $file_extensions = null])
 *  Register and return default file extensions for spl_autoload().
 * Return
 *  The list in force AFTER the call, which is the new one when a string was
 *  handed over and the standing one otherwise.
 *
 * The list is per-VM state rather than an ini directive (php keeps it in SPL's
 * own globals), and `null` means READ: it is the one argument value that does
 * not write. Every other value writes what it stringifies to, so `false`
 * empties the list -- and an empty list is a real setting, not a reset.
 */
PH7_PRIVATE int vm_builtin_spl_autoload_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		const char *zExt;
		int nExt;
		zExt = ph7_value_to_string(apArg[0],&nExt);
		SyBlobReset(&pVm->sAutoloadExt);
		if( nExt > 0 ){
			SyBlobAppend(&pVm->sAutoloadExt,zExt,(sxu32)nExt);
		}
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&pVm->sAutoloadExt),
		(int)SyBlobLength(&pVm->sAutoloadExt));
	return SXRET_OK;
}
/*
 * void spl_autoload_call(string $class)
 *  Try all registered autoloaders to load the requested class.
 *
 * php runs the stack whether or not the class is already declared -- the
 * verb is "call the autoloaders", not "load if missing" -- and stops as soon
 * as one of them declares it. The answer is always NULL.
 */
PH7_PRIVATE int vm_builtin_spl_autoload_call(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zClass;
	int nClass;
	if( nArg < 1 ){
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	zClass = ph7_value_to_string(apArg[0],&nClass);
	{
		/* php runs the stack for the EMPTY name too -- the autoloaders see a
		 * "" they have to answer for, rather than a call that never happened. */
		sxu32 nByte = nClass > 0 ? (sxu32)nClass : 0;
		if( zClass == 0 ){
			zClass = "";
		}
		PH7_VmClassNameAnchor(&zClass,&nByte);
		PH7_VmTriggerAutoload(pCtx->pVm,zClass,nByte,0);
	}
	ph7_result_null(pCtx);
	return SXRET_OK;
}
/*
 * array spl_classes()
 *  Return an array with the name of every class and interface SPL declares.
 *
 * php answers a map whose key and value are both the name, in the order its
 * own list is written -- which is alphabetical. The set is SPL's own and not
 * "everything iterable": Traversable, Countable and the exception BASE are
 * php's Core, so none of the three is here, while the five SPL INTERFACES
 * (OuterIterator, RecursiveIterator, SeekableIterator, SplObserver,
 * SplSubject) are.
 */
PH7_PRIVATE int vm_builtin_spl_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const char * const azSpl[] = {
		"AppendIterator", "ArrayIterator", "ArrayObject", "BadFunctionCallException",
		"BadMethodCallException", "CachingIterator", "CallbackFilterIterator",
		"DirectoryIterator", "DomainException", "EmptyIterator", "FilesystemIterator",
		"FilterIterator", "GlobIterator", "InfiniteIterator", "InvalidArgumentException",
		"IteratorIterator", "LengthException", "LimitIterator", "LogicException",
		"MultipleIterator", "NoRewindIterator", "OuterIterator", "OutOfBoundsException",
		"OutOfRangeException", "OverflowException", "ParentIterator", "RangeException",
		"RecursiveArrayIterator", "RecursiveCachingIterator",
		"RecursiveCallbackFilterIterator", "RecursiveDirectoryIterator",
		"RecursiveFilterIterator", "RecursiveIterator", "RecursiveIteratorIterator",
		"RecursiveRegexIterator", "RecursiveTreeIterator", "RegexIterator",
		"RuntimeException", "SeekableIterator", "SplDoublyLinkedList", "SplFileInfo",
		"SplFileObject", "SplFixedArray", "SplHeap", "SplMinHeap", "SplMaxHeap",
		"SplObjectStorage", "SplObserver", "SplPriorityQueue", "SplQueue", "SplStack",
		"SplSubject", "SplTempFileObject", "UnderflowException", "UnexpectedValueException"
	};
	ph7_value *pArray,*pVal;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_null(pCtx);
		return SXRET_OK;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(azSpl) ; ++n ){
		ph7_value_string(pVal,azSpl[n],-1);
		ph7_array_add_strkey_elem(pArray,azSpl[n],pVal);
		ph7_value_reset_string_cursor(pVal);
	}
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * void spl_autoload(string $class [, ?string $file_extensions = null ])
 *  Default implementation of __autoload().
 *  Converts namespace separators to directory separators, lowercases the class
 *  name, and tries to include a file with each of the given extensions.
 * Parameters
 *  class
 *   The class name being searched.
 *  file_extensions
 *   Comma-separated list of file extensions to try. When none is handed over,
 *   the list is whatever spl_autoload_extensions() holds -- which is where the
 *   ORDER comes from, and the order decides the answer: php's default tries
 *   `.inc` BEFORE `.php`, and this engine had the pair hardcoded the other way
 *   round, so a directory carrying both `foo.inc` and `foo.php` loaded the one
 *   php does not.
 */
PH7_PRIVATE int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zClass,*zExt,*zEnd,*zCur;
	SyBlob sPath;
	int nClass,nExt,iByte;
	sxi32 rc;
	if( nArg < 1 ){
		return SXRET_OK;
	}
	zClass = ph7_value_to_string(apArg[0],&nClass);
	if( nClass < 1 ){
		return SXRET_OK;
	}
	/* The list to try: the argument when there is one, and the standing
	 * spl_autoload_extensions() setting otherwise. */
	if( nArg >= 2 && !ph7_value_is_null(apArg[1]) ){
		/* An argument that is EMPTY is an empty list and not a request for the
		 * default one, so `spl_autoload($c,"")` searches nothing at all. */
		zExt = ph7_value_to_string(apArg[1],&nExt);
	}else{
		zExt = (const char *)SyBlobData(&pCtx->pVm->sAutoloadExt);
		nExt = (int)SyBlobLength(&pCtx->pVm->sAutoloadExt);
	}
	/* php walks the list as a C string, so a NUL byte in it ends the search. */
	for( iByte = 0 ; iByte < nExt ; ++iByte ){
		if( zExt[iByte] == 0 ){
			nExt = iByte;
			break;
		}
	}
	if( nExt < 1 ){
		return SXRET_OK;
	}
	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);
	/* Iterate over comma-separated extensions */
	zEnd = zExt + nExt;
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
		rc = VmExecIncludedFile(pCtx,&sFile,FALSE,0);
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
	ph7_class *pLsb = pVm->pMagicCallLsb;
	ph7_value sResult;
	SyString sMethName;
	sxi32 rc;
	/* Consume the pending dispatch (one-shot) */
	pVm->pMagicCallThis = 0;
	pVm->pMagicCallClass = 0;
	pVm->pMagicCallLsb = 0;
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
	rc = PH7_VmDispatchMagicCall(pVm,pClass,pLsb,pRecv,SyStringData(&sMethName),
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
