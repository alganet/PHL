/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Output buffering, php's model.
 *
 * A buffer holds the RAW bytes written into it and its handler runs on the way
 * OUT — at a flush, a clean, or when the buffer is removed — not on the way in.
 * The engine used to filter at WRITE time, which is a different program in four
 * visible ways: ob_get_contents() answered filtered text php answers raw,
 * a handler saw one call per echo instead of one per operation, the `$phase`
 * argument that tells it WHICH operation was always 0, and $chunk_size (which
 * only means anything to a write-out) was declared and never read.
 *
 * VmObPerform() is that operation: it takes the raw bytes out of the buffer, runs
 * the handler over them once with the phase php would pass, and hands the answer
 * to VmObDeliver(), which is the buffer BELOW or the real output.
 */
static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);
static sxi32 VmObDeliver(ph7_vm *pVm,sxu32 nIdx,const void *pData,sxu32 nLen);
static sxi32 VmObSink(ph7_vm *pVm,sxi32 iIdx,const void *pData,sxu32 nLen);
static ph7_int64 VmObInitSize(VmObEntry *pEntry);
static void VmObGrow(VmObEntry *pEntry,sxu32 nIncoming);
/*
 * TRUE while an output handler's own body is running.
 *
 * php discards everything a handler prints and refuses every ob call that would
 * mutate the stack under it. The test is not merely "a handler is running": when
 * the handler THROWS, this engine runs the enclosing catch IN PLACE, before the
 * dispatch call returns — and that catch is ordinary code in the frame that
 * called ob_flush(), so what IT prints belongs in the buffer and the ob calls it
 * makes are allowed. Comparing the running frame against the one that entered
 * the handler tells the two apart.
 */
static int VmObInHandler(ph7_vm *pVm)
{
	VmFrame *pCur;
	if( pVm->nObDepth < 1 ){
		return 0;
	}
	if( (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){
		/* The handler's own body — a php function running in its own frame, or a C
		 * builtin handler running in the caller's, which pushes none at all. */
		return 1;
	}
	/* A CATCH body, which runs in an exception frame. This engine runs the catch
	 * for a throw inside the handler IN PLACE, before the dispatch call returns, so
	 * a catch is only "inside the handler" when it belongs to a frame BELOW the one
	 * that entered it — the handler catching its own throw. A catch in that frame,
	 * or in any frame above it, is ordinary code: what it prints belongs in the
	 * buffer and the ob calls it makes are allowed. */
	pCur = VmSkipExceptionFrames(pVm->pFrame);
	if( pCur == pVm->pObFrame ){
		return 0;
	}
	while( pCur ){
		if( pCur == pVm->pObFrame ){
			return 1;
		}
		pCur = pCur->pParent;
	}
	return 0;
}
/*
 * How many buffers the ob functions can SEE right now. php truncates the stack at
 * the buffer whose handler is running: from inside one, ob_get_level() answers
 * that buffer's level and ob_get_contents() its bytes, not those of whatever was
 * stacked on top of it (reachable when a chunked buffer writes out while an inner
 * buffer is open).
 */
static sxu32 VmObVisible(ph7_vm *pVm)
{
	sxu32 nUsed = SySetUsed(&pVm->aOB);
	if( VmObInHandler(pVm) && pVm->nObActive > 0 && pVm->nObActive < nUsed ){
		return pVm->nObActive;
	}
	return nUsed;
}
static sxi32 VmObPerform(ph7_vm *pVm,sxu32 nIdx,int iOp,SyBlob *pRaw);
/*
 * Perform one output-buffer operation on the buffer at index nIdx.
 *
 *   iOp   one of PH7_OB_WRITE / PH7_OB_FLUSH / PH7_OB_CLEAN, optionally with
 *         PH7_OB_FINAL (the buffer is going away). PH7_OB_START is added here
 *         while the handler has not run yet, exactly as php does.
 *   pRaw  when non-NULL, receives a copy of the buffer as it was BEFORE the
 *         handler ran — ob_get_clean()/ob_get_flush() answer that, not the
 *         handler's output.
 *
 * The buffer is left empty; removing it is the caller's job. A CLEAN still runs
 * the handler (php gives it the chance to reset its own state) and then throws
 * the answer away.
 */
static sxi32 VmObPerform(ph7_vm *pVm,sxu32 nIdx,int iOp,SyBlob *pRaw)
{
	VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);
	SyBlob sData;
	sxi32 rc = PH7_OK;
	sxu32 nRawLen;
	int bDrop = 0;
	if( pEntry == 0 ){
		return PH7_OK;
	}
	/* Take the bytes OUT of the entry before anything else runs: the handler is
	 * php code, and php code reaching back into the buffer stack reallocates it —
	 * every pointer into the set, this entry's own blob included, dies with it. */
	SyBlobInit(&sData,&pVm->sAllocator);
	if( SyBlobLength(&pEntry->sOB) > 0 ){
		SyBlobDup(&pEntry->sOB,&sData);
		if( pRaw ){
			SyBlobDup(&pEntry->sOB,pRaw);
		}
	}
	nRawLen = SyBlobLength(&sData);
	if( !ph7_value_is_callable(&pEntry->sCallback) ){
		/* No handler: php's internal one, which "runs" for every operation and is
		 * always taken to have produced its output. ob_get_status() reports both
		 * bits from the first operation on, empty buffer included. */
		pEntry->iFlags |= PH7_OB_STARTED | PH7_OB_PROCESSED;
	}
	if( ph7_value_is_callable(&pEntry->sCallback)
		&& (pEntry->iFlags & PH7_OB_DISABLED) == 0 ){
		ph7_value sArg,sPhase,sResult,*apArg[2];
		int iPhase = iOp | ((pEntry->iFlags & PH7_OB_STARTED) ? 0 : PH7_OB_START);
		int bRefused = 0;
		ph7_value sCallback;
		sxi32 rcCall;
		/* The callback is copied out for the same reason the bytes are. */
		PH7_MemObjInit(pVm,&sCallback);
		PH7_MemObjStore(&pEntry->sCallback,&sCallback);
		/* Marked failed for the DURATION of the call, and cleared again when it comes
		 * back with an answer. A disabled buffer is transparent, and that is exactly
		 * what this buffer is while its handler runs: whatever the in-place catch for
		 * a throwing handler prints belongs to the level BELOW, which is where php —
		 * whose catch runs after the operation finished — puts it too. */
		pEntry->iFlags |= PH7_OB_DISABLED;
		PH7_MemObjInitFromString(pVm,&sArg,0);
		PH7_MemObjStringAppend(&sArg,(const char *)SyBlobData(&sData),SyBlobLength(&sData));
		/* php calls the handler as ($buffer, int $phase) — a handler declaring
		 * both as required must not trip the arity check. */
		PH7_MemObjInitFromInt(pVm,&sPhase,iPhase);
		apArg[0] = &sArg;
		apArg[1] = &sPhase;
		PH7_MemObjInit(pVm,&sResult);
		/* Everything the handler prints is DISCARDED (php has no buffer to put it
		 * in — this one is mid-operation), and it may not open one either.
		 * Through the callback dispatcher, not PH7_VmCallUserFunction: php builds
		 * both arguments itself and passes them BY VALUE, so a handler declaring
		 * `&$buffer` gets php's warning and a copy rather than the fatal a direct
		 * call raises — and a throw from inside reaches the enclosing catch. */
		{
			VmFrame *pSaveFrame = pVm->pObFrame;
			sxu32 nSaveActive = pVm->nObActive;
			int bSaveRefused = pVm->bObRefused;
			pVm->pObFrame = pVm->pFrame;
			pVm->nObActive = nIdx + 1;
			pVm->bObRefused = 0;
			pVm->nObDepth++;
			rcCall = PH7_VmCallCallbackByValue(pVm,&sCallback,2,apArg,&sResult,0);
			pVm->nObDepth--;
			bRefused = pVm->bObRefused;
			pVm->bObRefused = bSaveRefused;
			pVm->nObActive = nSaveActive;
			pVm->pObFrame = pSaveFrame;
		}
		/* php code ran: the slot may have moved, or gone. */
		pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);
		if( pEntry ){
			/* Set AFTER the call: php's own STARTED is not visible to the first
			 * invocation, only to the ones that follow it. */
			pEntry->iFlags |= PH7_OB_STARTED;
		}
		if( PH7_CALLBACK_UNWOUND(rcCall)
			|| (ph7_value_is_bool(&sResult) && !ph7_value_to_bool(&sResult)) ){
			/* php's two FAILURE shapes — the handler answered FALSE, or it threw
			 * (or exited) and never answered at all. Both send the ORIGINAL bytes,
			 * so `sData` is left exactly as it was, and both leave the handler
			 * DISABLED: it is not called again (which is what keeps a throwing
			 * handler from throwing a second time out of the shutdown flush) and the
			 * buffer stops buffering. */
		}else if( ph7_value_is_bool(&sResult) ){
			/* TRUE: "no data" — the operation produces nothing at all, and php
			 * counts that as the handler having processed the buffer. */
			if( pEntry ){
				pEntry->iFlags &= ~PH7_OB_DISABLED;
				pEntry->iFlags |= PH7_OB_PROCESSED;
			}
			SyBlobReset(&sData);
		}else{
			/* Anything else is cast to a string, NULL included — php's own
			 * user-visible conversion, so an ARRAY comes out as "Array" WITH the
			 * warning and an object with no __toString() throws. */
			const char *zOut;
			int nOut;
			PH7_MemObjToStringUV(&sResult);
			zOut = ph7_value_to_string(&sResult,&nOut);
			SyBlobReset(&sData);
			if( nOut > 0 ){
				SyBlobAppend(&sData,zOut,(sxu32)nOut);
			}
			if( pEntry ){
				pEntry->iFlags &= ~PH7_OB_DISABLED;
				pEntry->iFlags |= PH7_OB_PROCESSED;
			}
		}
		PH7_MemObjRelease(&sArg);
		PH7_MemObjRelease(&sPhase);
		PH7_MemObjRelease(&sResult);
		PH7_MemObjRelease(&sCallback);
		/* A throw or an exit() inside the handler is the CALLER's to act on: the
		 * enclosing catch runs, or the program ends. */
		if( PH7_CALLBACK_UNWOUND(rcCall) ){
			rc = rcCall;
		}
		/* An ob call the handler was not allowed to make ends the request, and php
		 * delivers nothing more. An ordinary exit() from inside one is NOT that:
		 * php still sends what the buffer held. */
		if( bRefused ){
			bDrop = 1;
		}
	}
	/* The bytes this operation took are gone from the buffer now — but only those:
	 * an in-place catch for a throw inside the handler runs before the dispatch
	 * returns and may have written MORE into this still-open buffer, and that tail
	 * is the caller's output, not this operation's. Re-resolved because php code
	 * may have moved the set. */
	pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);
	if( pEntry ){
		sxu32 nHave = SyBlobLength(&pEntry->sOB);
		if( nHave > nRawLen ){
			SyBlob sTail;
			SyBlobInit(&sTail,&pVm->sAllocator);
			SyBlobAppend(&sTail,(const char *)SyBlobData(&pEntry->sOB) + nRawLen,nHave - nRawLen);
			SyBlobReset(&pEntry->sOB);
			SyBlobAppend(&pEntry->sOB,SyBlobData(&sTail),SyBlobLength(&sTail));
			SyBlobRelease(&sTail);
		}else{
			SyBlobReset(&pEntry->sOB);
		}
	}
	if( (iOp & PH7_OB_CLEAN) == 0 && SyBlobLength(&sData) > 0 && !bDrop ){
		sxi32 rcOut = VmObDeliver(pVm,nIdx,SyBlobData(&sData),SyBlobLength(&sData));
		if( rc == PH7_OK ){
			rc = rcOut;
		}
	}
	SyBlobRelease(&sData);
	return rc;
}
/*
 * Hand nLen bytes to the buffer at index iIdx, or to whatever is under it.
 *
 * php's stack is a stack — a nested flush lands in the enclosing buffer, not on
 * stdout — and a DISABLED buffer is transparent: once a handler has failed php
 * stops buffering through it (the level is still there and still counts, but
 * everything written to it passes straight down), which is why the catch that
 * follows a throwing handler prints immediately and ob_get_contents() answers "".
 */
static sxi32 VmObSink(ph7_vm *pVm,sxi32 iIdx,const void *pData,sxu32 nLen)
{
	sxi32 rc;
	while( iIdx >= 0 ){
		VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,(sxu32)iIdx);
		if( pEntry == 0 ){
			break; /* the buffer went away underneath: fall through to the output */
		}
		if( (pEntry->iFlags & PH7_OB_DISABLED) == 0 ){
			VmObGrow(pEntry,nLen);
			SyBlobAppend(&pEntry->sOB,pData,nLen);
			/* A buffer with a chunk size writes out as soon as it holds one. */
			if( pEntry->nChunk > 0 && SyBlobLength(&pEntry->sOB) >= pEntry->nChunk ){
				return VmObPerform(pVm,(sxu32)iIdx,PH7_OB_WRITE,0);
			}
			return PH7_OK;
		}
		iIdx--;
	}
	/* Call the VM output consumer */
	rc = pVm->sVmConsumer.xDef(pData,(unsigned int)nLen,pVm->sVmConsumer.pDefData);
	/* Increment VM output counter */
	pVm->nOutputLen += nLen;
	if( rc != PH7_ABORT ){
		rc = PH7_OK;
	}
	return rc;
}
/*
 * Deliver what is leaving the buffer at nIdx to whatever is under it.
 */
static sxi32 VmObDeliver(ph7_vm *pVm,sxu32 nIdx,const void *pData,sxu32 nLen)
{
	return VmObSink(pVm,(sxi32)nIdx - 1,pData,nLen);
}
/*
 * Output Buffer(OB) default VM consumer routine.All VM output is now redirected
 * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].
 * Refer to the implementation of [ob_start()] for more information.
 */
PH7_PRIVATE int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	sxu32 nUsed = SySetUsed(&pVm->aOB);
	if( nUsed < 1 ){
		/* CAN'T HAPPEN */
		return PH7_OK;
	}
	if( VmObInHandler(pVm) ){
		/* Inside a handler: php has nowhere to put this and drops it. */
		return PH7_OK;
	}
	return VmObSink(pVm,(sxi32)nUsed - 1,pData,nDataLen);
}
/*
 * Pop the topmost buffer and release it, restoring the default consumer when the
 * stack empties out.
 */
static void VmObPop(ph7_vm *pVm)
{
	VmObEntry *pEntry = (VmObEntry *)SySetPop(&pVm->aOB);
	if( pEntry ){
		VmObRestore(pVm,pEntry);
	}
}
/*
 * Restore the default consumer.
 * Refer to the implementation of [ob_end_clean()] for more
 * information.
 */
static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)
{
	ph7_output_consumer *pCons = &pVm->sVmConsumer;
	if( SySetUsed(&pVm->aOB) < 1 ){
		/* No more stackable OB */
		pCons->xConsumer = pCons->xDef;
		pCons->pUserData = pCons->pDefData;
	}
	/* Release OB data */
	PH7_MemObjRelease(&pEntry->sCallback);
	SyBlobRelease(&pEntry->sOB);
}
/*
 * php ends and FLUSHES every still-open buffer at shutdown — innermost first, so
 * an inner handler's answer is what the outer one is handed. A script that never
 * called ob_end_flush() (PHPUnit, which buffers its summary and then exit()s with
 * a non-zero status) would otherwise lose that output entirely.
 */
PH7_PRIVATE void PH7_VmObFlushAll(ph7_vm *pVm)
{
	while( SySetUsed(&pVm->aOB) > 0 ){
		VmObPerform(pVm,SySetUsed(&pVm->aOB) - 1,PH7_OB_FINAL,0);
		VmObPop(pVm);
	}
	pVm->nObDepth = 0;
}
/*
 * php refuses every ob call that MUTATES the stack while a handler is running —
 * the handler IS an operation on that stack, so cleaning, flushing, removing or
 * pushing under it has nowhere sane to land. The read-only members
 * (ob_get_contents/ob_get_length/ob_get_level/ob_list_handlers) are allowed and
 * still answer for the buffer being processed.
 *
 * Returns TRUE when the call was refused; the caller returns PH7_ABORT.
 */
static int VmObRefuseInHandler(ph7_context *pCtx)
{
	ph7_vm *pVm = pCtx->pVm;
	if( !VmObInHandler(pVm) ){
		return 0;
	}
	/* The context prefixes "name(): " itself, so each member names itself. */
	ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,
		"Cannot use output buffering in output buffering display handlers");
	ph7_result_bool(pCtx,0);
	pVm->iExitStatus = 255;
	pVm->bHaltRequested = 1;
	pVm->bObRefused = 1;
	return 1;
}
/*
 * php's name for one buffer's handler: the callable's own display name (a plain
 * function name, `Class::method`, `{closure:file:line}`) or, with no handler at
 * all, the literal "default output handler". It reaches ob_list_handlers(),
 * ob_get_status() and the refusal notices below — where PHL used to answer
 * "Class Method" for every array callback and "default output handler" for every
 * CLOSURE, so a closure handler was indistinguishable from none.
 */
static void VmObHandlerName(ph7_vm *pVm,VmObEntry *pEntry,SyBlob *pOut)
{
	if( ph7_value_is_callable(&pEntry->sCallback) ){
		PH7_VmCallableName(pVm,&pEntry->sCallback,pOut);
		if( SyBlobLength(pOut) > 0 ){
			return;
		}
	}
	SyBlobAppend(pOut,"default output handler",sizeof("default output handler")-1);
}
/*
 * Copy one buffer's bytes out. Anything that can run php code — a notice reaching
 * a user error handler included — may realloc the buffer stack, so nothing holds
 * a VmObEntry pointer across it.
 */
static void VmObSnapshot(ph7_vm *pVm,sxu32 nIdx,SyBlob *pOut)
{
	VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);
	if( pEntry && SyBlobLength(&pEntry->sOB) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&pEntry->sOB),SyBlobLength(&pEntry->sOB));
	}
}
/*
 * ob_start()'s $flags decide what may be done to the buffer afterwards, and every
 * member tests its own bit before it touches anything: CLEANABLE for ob_clean(),
 * FLUSHABLE for ob_flush(), REMOVABLE for the four that take the buffer away. A
 * refused operation does NOT run the handler and leaves the buffer exactly as it
 * was. The argument was declared in `aBuiltinSig[]` and read by nothing, so a
 * buffer opened as un-removable — the standard way a framework pins its own
 * output layer in place — could be torn out by any library that called
 * ob_end_clean().
 *
 * `zWhat` is php's verb for this member ("delete"/"flush"/"discard"/"send").
 * Returns TRUE when the operation may proceed.
 */
static int VmObAllows(ph7_context *pCtx,sxu32 nIdx,int iNeed,const char *zWhat)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);
	SyBlob sName;
	if( pEntry == 0 || (pEntry->iFlags & iNeed) != 0 ){
		return 1;
	}
	SyBlobInit(&sName,&pVm->sAllocator);
	VmObHandlerName(pVm,pEntry,&sName);
	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
		"Failed to %s buffer of %.*s (%u)",zWhat,
		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),nIdx);
	SyBlobRelease(&sName);
	return 0;
}
/*
 * bool ob_clean(void)
 *  This function discards the contents of the output buffer.
 *  This function does not destroy the output buffer like ob_end_clean() does.
 * Parameter
 *  None
 * Return
 *  TRUE on success, FALSE (with a notice) when no buffer is active. This used to
 *  return NOTHING, so `if (!ob_clean())` fired on the successful call.
 */
PH7_PRIVATE int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 nUsed = SySetUsed(&pVm->aOB);
	sxi32 rc;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	if( VmObRefuseInHandler(pCtx) ){
		return PH7_ABORT;
	}
	if( nUsed < 1 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Failed to delete buffer. No buffer to delete");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_CLEANABLE,"delete") ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_CLEAN,0);
	ph7_result_bool(pCtx,1);
	return rc;
}
/*
 * bool ob_end_clean(void)
 *  Clean (erase) the output buffer and turn off output buffering
 *  This function discards the contents of the topmost output buffer and turns
 *  off this output buffering. If you want to further process the buffer's contents
 *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents
 *  are discarded when ob_end_clean() is called.
 * Parameter
 *  None
 * Return
 *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called
 *  the function without an active buffer or that for some reason a buffer could not be deleted
 * (possible for special buffer)
 */
PH7_PRIVATE int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 nUsed = SySetUsed(&pVm->aOB);
	sxi32 rc;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	if( VmObRefuseInHandler(pCtx) ){
		return PH7_ABORT;
	}
	if( nUsed < 1 ){
		/* No such OB,return FALSE */
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Failed to delete buffer. No buffer to delete");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"discard") ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_CLEAN|PH7_OB_FINAL,0);
	VmObPop(pVm);
	ph7_result_bool(pCtx,1);
	return rc;
}
/*
 * string ob_get_contents(void)
 *  Gets the contents of the output buffer without clearing it.
 * Parameter
 *  None
 * Return
 *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.
 *  The bytes are the RAW ones: php runs the handler on the way out, so what a
 *  handler will make of them has not happened yet.
 */
PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 nSeen = VmObVisible(pVm);
	VmObEntry *pOb = nSeen > 0 ? (VmObEntry *)SySetAt(&pVm->aOB,nSeen - 1) : 0;
	if( pOb == 0 ){
		/* No active OB,return FALSE */
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
	}else{
		/* Return contents */
		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));
	}
	return PH7_OK;
}
/*
 * string ob_get_clean(void)
 *  Get current buffer contents and delete current output buffer.
 * Parameter
 *  None
 * Return
 *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.
 */
PH7_PRIVATE int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 nUsed = SySetUsed(&pVm->aOB);
	SyBlob sRaw;
	sxi32 rc;
	if( VmObRefuseInHandler(pCtx) ){
		return PH7_ABORT;
	}
	if( nUsed < 1 ){
		/* No active OB,return FALSE. php reports every other empty-stack call and
		 * stays silent for this one. */
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	SyBlobInit(&sRaw,&pVm->sAllocator);
	/* Snapshot BEFORE the refusal is even tested: the notices below reach a user
	 * error handler, which is php code that may print into this very buffer, and
	 * php answers the contents as they were when the call was made. */
	VmObSnapshot(pVm,nUsed - 1,&sRaw);
	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"discard") ){
		/* php reports the CLEAN and the REMOVAL separately, and still answers the
		 * contents it could not take away — as they were BEFORE those reports,
		 * which may run a user error handler that writes into this very buffer. */
		VmObAllows(pCtx,nUsed - 1,0,"delete");
		ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw));
		SyBlobRelease(&sRaw);
		return PH7_OK;
	}
	SyBlobReset(&sRaw);
	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_CLEAN|PH7_OB_FINAL,&sRaw);
	VmObPop(pVm);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw)); /* Will make it's own copy */
	SyBlobRelease(&sRaw);
	return rc;
}
/*
 * string ob_get_flush(void)
 *  Flush the output buffer, return it as a string and turn off output buffering.
 * Parameter
 *  None
 * Return
 *  The contents of the output buffer, or FALSE if output buffering isn't active.
 * Note
 *  This used to be registered as ob_get_clean(), which DISCARDS the buffer: the
 *  string came back correctly and the output it names never reached the terminal.
 */
PH7_PRIVATE int vm_builtin_ob_get_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 nUsed = SySetUsed(&pVm->aOB);
	SyBlob sRaw;
	sxi32 rc;
	if( VmObRefuseInHandler(pCtx) ){
		return PH7_ABORT;
	}
	if( nUsed < 1 ){
		/* No active OB,return FALSE. php's silent member here is ob_get_CLEAN;
		 * this one reports, and the two wordings are php's own. */
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Failed to delete and flush buffer. No buffer to delete or flush");
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	SyBlobInit(&sRaw,&pVm->sAllocator);
	/* Snapshot BEFORE the refusal is even tested: the notices below reach a user
	 * error handler, which is php code that may print into this very buffer, and
	 * php answers the contents as they were when the call was made. */
	VmObSnapshot(pVm,nUsed - 1,&sRaw);
	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"send") ){
		VmObAllows(pCtx,nUsed - 1,0,"delete");
		ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw));
		SyBlobRelease(&sRaw);
		return PH7_OK;
	}
	SyBlobReset(&sRaw);
	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_FINAL,&sRaw);
	VmObPop(pVm);
	/* The answer is the RAW buffer, not what the handler made of it */
	ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw));
	SyBlobRelease(&sRaw);
	return rc;
}
/*
 * int ob_get_length(void)
 *  Return the length of the output buffer.
 * Parameter
 *  None
 * Return
 *  Returns the length of the output buffer contents or FALSE if no buffering is active.
 */
PH7_PRIVATE int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 nSeen = VmObVisible(pVm);
	VmObEntry *pOb = nSeen > 0 ? (VmObEntry *)SySetAt(&pVm->aOB,nSeen - 1) : 0;
	if( pOb == 0 ){
		/* No active OB,return FALSE */
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
	}else{
		/* Return OB length */
		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));
	}
	return PH7_OK;
}
/*
 * int ob_get_level(void)
 *  Returns the nesting level of the output buffering mechanism.
 * Parameter
 *  None
 * Return
 *  Returns the level of nested output buffering handlers or zero if output buffering is not active.
 */
PH7_PRIVATE int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	int iNest;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* Nesting level */
	iNest = (int)VmObVisible(pVm);
	/* Return the nesting value */
	ph7_result_int(pCtx,iNest);
	return PH7_OK;
}
/*
 * bool ob_start([ callback $output_callback[, int $chunk_size = 0[, int $flags]]] )
 * This function will turn output buffering on. While output buffering is active no output
 *  is sent from the script (other than headers), instead the output is stored in an internal
 *  buffer.
 * Parameter
 *  $output_callback
 *   An optional output_callback function may be specified. This function takes a string
 *   as a parameter and should return a string. The function is called when the buffer is
 *   flushed, cleaned or removed, and its second argument says WHICH of those it is (the
 *   PHP_OUTPUT_HANDLER_* phase bits). Returning FALSE sends the original bytes and
 *   disables the handler for good; returning TRUE sends nothing.
 *  $chunk_size
 *   Write out as soon as the buffer holds this many bytes.
 * Return
 *   Returns TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry sOb;
	sxi32 rc;
	/* php has nowhere to put a buffer opened from inside a handler — that handler
	 * is mid-operation on the stack this would push onto — and refuses outright. */
	if( VmObRefuseInHandler(pCtx) ){
		return PH7_ABORT;
	}
	/* php screens the handler BEFORE it opens the buffer, and a handler it cannot
	 * call is a refusal rather than a silent downgrade to the default one: the
	 * warning names why (zend's own callable reason) and the notice says no buffer
	 * was created. This used to answer TRUE with a buffer whose handler never ran,
	 * so `if (!ob_start('my_filter'))` never fired on a misspelled name and the
	 * output came out unfiltered. */
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){
		if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){
			char zBuf[256];
			const char *zReason = PH7_VmCallableReason(pVm,apArg[0],zBuf,(int)sizeof(zBuf));
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zReason);
			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Failed to create buffer");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	/* Initialize the OB entry */
	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);
	SyBlobInit(&sOb.sOB,&pVm->sAllocator);
	/* php keeps whatever it is given except the two nibbles it reserves for
	 * itself — the phase bits and the STARTED/DISABLED/PROCESSED state — and
	 * reports the rest back verbatim, sign included. */
	sOb.iFlags = nArg > 2 ? (ph7_value_to_int64(apArg[2]) & PH7_OB_FLAGMASK) : PH7_OB_STDFLAGS;
	sOb.nChunk = 0;
	sOb.nSize = 0;
	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ)) ){
		/* Save the callback name for later invocation (MEMOBJ_OBJ = a Closure callback). */
		PH7_MemObjStore(apArg[0],&sOb.sCallback);
		sOb.iFlags |= PH7_OB_USER;
	}
	if( nArg > 1 ){
		ph7_int64 nChunk = ph7_value_to_int64(apArg[1]);
		/* A negative chunk size is no chunk size at all, which is what php
		 * reports back for one. */
		if( nChunk > 0 ){
			sOb.nChunk = nChunk;
		}
	}
	sOb.nSize = VmObInitSize(&sOb);
	/* Push in the stack */
	rc = SySetPut(&pVm->aOB,(const void *)&sOb);
	if( rc != SXRET_OK ){
		PH7_MemObjRelease(&sOb.sCallback);
	}else{
		ph7_output_consumer *pCons = &pVm->sVmConsumer;
		/* Substitute the default VM consumer */
		if( pCons->xConsumer != VmObConsumer ){
			pCons->xDef = pCons->xConsumer;
			pCons->pDefData = pCons->pUserData;
			/* Install the new consumer */
			pCons->xConsumer = VmObConsumer;
			pCons->pUserData = pVm;
		}
	}
	ph7_result_bool(pCtx,rc == SXRET_OK);
	return PH7_OK;
}
/*
 * bool ob_flush(void)
 *  Flush (send) the output buffer.
 * Parameter
 *  None
 * Return
 *  TRUE on success, FALSE (with a notice) when no buffer is active.
 */
PH7_PRIVATE int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 nUsed = SySetUsed(&pVm->aOB);
	sxi32 rc;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	if( VmObRefuseInHandler(pCtx) ){
		return PH7_ABORT;
	}
	if( nUsed < 1 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Failed to flush buffer. No buffer to flush");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_FLUSHABLE,"flush") ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_FLUSH,0);
	ph7_result_bool(pCtx,1);
	return rc;
}
/*
 * void flush(void)
 *  Flush the output layer to the SAPI. It does NOT touch the userland output
 *  buffers: `ob_start(); echo "a"; flush();` leaves "a" in the buffer under php,
 *  where this used to be registered as ob_flush() and SENT it — so a progress-bar
 *  idiom (echo, flush(), keep working) emptied a buffer the script meant to read
 *  back later, and ob_get_contents() answered "".
 *  This engine's own consumer writes with an unbuffered write(2)/WriteFile(), so
 *  there is nothing left to push and the call is a no-op that answers nothing,
 *  which is php's `void` return.
 * Parameter
 *  None
 * Return
 *  No value is returned.
 */
PH7_PRIVATE int vm_builtin_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(pCtx);
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return PH7_OK;
}
/*
 * bool ob_end_flush(void)
 *  Flush (send) the output buffer and turn off output buffering.
 * Parameter
 *  None
 * Return
 *  Returns TRUE on success or FALSE on failure. Reasons for failure are first
 *  that you called the function without an active buffer or that for some reason
 *  a buffer could not be deleted (possible for special buffer).
 */
PH7_PRIVATE int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 nUsed = SySetUsed(&pVm->aOB);
	sxi32 rc;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	if( VmObRefuseInHandler(pCtx) ){
		return PH7_ABORT;
	}
	if( nUsed < 1 ){
		/* Empty stack,return FALSE */
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Failed to delete and flush buffer. No buffer to delete or flush");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"send") ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_FINAL,0);
	VmObPop(pVm);
	/* Return true */
	ph7_result_bool(pCtx,1);
	return rc;
}
/*
 * void ob_implicit_flush([int $flag = true ])
 *  ob_implicit_flush() will turn implicit flushing on or off.
 *  Implicit flushing will result in a flush operation after every
 *  output call, so that explicit calls to flush() will no longer be needed.
 * Parameter
 *  $flag
 *   TRUE to turn implicit flushing on, FALSE otherwise.
 * Return
 *   Nothing
 */
PH7_PRIVATE int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/* NOTE: As of this version,this function is a no-op.
	 * PH7 is smart enough to flush it's internal buffer when appropriate.
	 */
	SXUNUSED(pCtx);
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	return PH7_OK;
}
/*
 * php's buffer SIZE bookkeeping, reproduced so ob_get_status() answers php's
 * number rather than this engine's blob capacity. The initial allocation is
 * 16 KB, or the chunk size rounded up to a 4 KB boundary when one was asked for;
 * a write that would not fit grows it by whichever is larger of that initial size
 * and the shortfall rounded the same way.
 */
#define PH7_OB_ALIGN(n)   ((((ph7_int64)(n)) + 0xFFF) & ~(ph7_int64)0xFFF)
#define PH7_OB_DEFSIZE    0x4000
static ph7_int64 VmObInitSize(VmObEntry *pEntry)
{
	return pEntry->nChunk > 0 ? PH7_OB_ALIGN(pEntry->nChunk) : PH7_OB_DEFSIZE;
}
static void VmObGrow(VmObEntry *pEntry,sxu32 nIncoming)
{
	ph7_int64 nUsed = (ph7_int64)SyBlobLength(&pEntry->sOB);
	ph7_int64 nFree = pEntry->nSize > nUsed ? pEntry->nSize - nUsed : 0;
	if( nFree <= (ph7_int64)nIncoming ){
		ph7_int64 nInit = VmObInitSize(pEntry);
		ph7_int64 nGrow = PH7_OB_ALIGN((ph7_int64)nIncoming - nFree);
		pEntry->nSize += nGrow > nInit ? nGrow : nInit;
	}
}
/*
 * Describe one buffer the way ob_get_status() does.
 */
static void VmObStatusEntry(ph7_context *pCtx,VmObEntry *pEntry,sxu32 nIdx,ph7_value *pOut)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	ph7_int64 iFlags = pEntry->iFlags;
	SyBlob sName;
	if( pVal == 0 ){
		return;
	}
	if( VmObInHandler(pVm) && pVm->nObActive == nIdx + 1 ){
		/* Asking from inside this buffer's own handler: the DISABLED mark it wears
		 * for the duration of the call is bookkeeping, not an answer. */
		iFlags &= ~PH7_OB_DISABLED;
	}
	SyBlobInit(&sName,&pVm->sAllocator);
	VmObHandlerName(pVm,pEntry,&sName);
	ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));
	ph7_array_add_strkey_elem(pOut,"name",pVal);
	SyBlobRelease(&sName);
	ph7_value_int(pVal,(iFlags & PH7_OB_USER) ? 1 : 0);
	ph7_array_add_strkey_elem(pOut,"type",pVal);
	ph7_value_int64(pVal,iFlags);
	ph7_array_add_strkey_elem(pOut,"flags",pVal);
	ph7_value_int64(pVal,(ph7_int64)nIdx);
	ph7_array_add_strkey_elem(pOut,"level",pVal);
	ph7_value_int64(pVal,pEntry->nChunk);
	ph7_array_add_strkey_elem(pOut,"chunk_size",pVal);
	/* A buffer whose handler failed is not buffering at all, and php reports the
	 * allocation it dropped: 0. */
	ph7_value_int64(pVal,(iFlags & PH7_OB_DISABLED) ? 0 : pEntry->nSize);
	ph7_array_add_strkey_elem(pOut,"buffer_size",pVal);
	ph7_value_int64(pVal,(ph7_int64)SyBlobLength(&pEntry->sOB));
	ph7_array_add_strkey_elem(pOut,"buffer_used",pVal);
	ph7_context_release_value(pCtx,pVal);
}
/*
 * array ob_get_status([bool $full_status = false])
 *  Describe the active output buffers: the TOPMOST one by default (an empty array
 *  when nothing is buffering), or every one of them, outermost first, when asked
 *  for the full status.
 * Note
 *  This function did not exist here at all, so the standard way to ask what an
 *  output handler is and what it may do — `ob_get_status()['flags']` — was an
 *  undefined-function fatal, and so was every framework probe that guards on it.
 */
PH7_PRIVATE int vm_builtin_ob_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	sxu32 nSeen = VmObVisible(pVm);
	int bFull = nArg > 0 && ph7_value_to_bool(apArg[0]);
	ph7_value *pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( bFull ){
		sxu32 n;
		for( n = 0 ; n < nSeen ; ++n ){
			VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,n);
			ph7_value *pOne = ph7_context_new_array(pCtx);
			if( pEntry == 0 || pOne == 0 ){
				continue;
			}
			VmObStatusEntry(pCtx,pEntry,n,pOne);
			ph7_array_add_elem(pArray,0,pOne);
			ph7_context_release_value(pCtx,pOne);
		}
	}else if( nSeen > 0 ){
		VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nSeen - 1);
		if( pEntry ){
			VmObStatusEntry(pCtx,pEntry,nSeen - 1,pArray);
		}
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array ob_list_handlers(void)
 *  Lists all output handlers in use.
 * Parameter
 *  None
 * Return
 *  This will return an array with the output handlers in use (if any).
 */
PH7_PRIVATE int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pArray;
	VmObEntry *aEntry;
	ph7_value sVal;
	sxu32 n;
	/* Create a new array. php answers an EMPTY array when nothing is buffering
	 * (`foreach (ob_list_handlers() as ...)` over the NULL this used to answer is
	 * a TypeError), so the array is built before the stack is looked at. */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Out of memory,return NULL */
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sVal);
	/* Point to the installed OB entries */
	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);
	/* Perform the requested operation */
	for( n = 0 ; n < VmObVisible(pVm) ; n++ ){
		VmObEntry *pEntry = &aEntry[n];
		/* Extract handler name */
		SyBlobReset(&sVal.sBlob);
		VmObHandlerName(pVm,pEntry,&sVal.sBlob);
		sVal.iFlags = MEMOBJ_STRING;
		/* Perform the insertion */
		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);
	}
	PH7_MemObjRelease(&sVal);
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
