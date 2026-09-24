/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);
static void VmObWrite(ph7_vm *pVm,sxu32 nIdx,const void *pData,unsigned int nDataLen);
static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease);
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
	VmObEntry *pOb;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* Peek the top most OB */
	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);
	if( pOb == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Failed to delete buffer. No buffer to delete");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobRelease(&pOb->sOB);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
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
	VmObEntry *pOb;
	/* Pop the top most OB */
	pOb = (VmObEntry *)SySetPop(&pVm->aOB);
	if( pOb == 0){
		/* No such OB,return FALSE */
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Failed to delete buffer. No buffer to delete");
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
	}else{
		/* Release */
		VmObRestore(pVm,pOb);
		/* Return true */
		ph7_result_bool(pCtx,1);
	}
	return PH7_OK;
}
/*
 * string ob_get_contents(void)
 *  Gets the contents of the output buffer without clearing it.
 * Parameter
 *  None
 * Return
 *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.
 */
PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pOb;
	/* Peek the top most OB */
	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);
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
	VmObEntry *pOb;
	/* Pop the top most OB */
	pOb = (VmObEntry *)SySetPop(&pVm->aOB);
	if( pOb == 0 ){
		/* No active OB,return FALSE */
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
	}else{
		/* Return contents */
		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB)); /* Will make it's own copy */
		/* Release */
		VmObRestore(pVm,pOb);
	}
	return PH7_OK;
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
	VmObEntry *pOb;
	sxi32 rc;
	/* Pop the top most OB */
	pOb = (VmObEntry *)SySetPop(&pVm->aOB);
	if( pOb == 0 ){
		/* No active OB,return FALSE. php's silent member here is ob_get_CLEAN;
		 * this one reports, and the two wordings are php's own. */
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Failed to delete and flush buffer. No buffer to delete or flush");
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	/* Return contents (copied out before the flush releases the blob) */
	ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));
	/* Send them on and turn this buffer off */
	rc = VmObFlush(pVm,pOb,TRUE);
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
	VmObEntry *pOb;
	/* Peek the top most OB */
	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);
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
	iNest = (int)SySetUsed(&pVm->aOB);
	/* Return the nesting value */
	ph7_result_int(pCtx,iNest);
	return PH7_OK;
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
	VmObWrite(pVm,nUsed - 1,pData,nDataLen);
	return PH7_OK;
}
/*
 * Write output INTO one specific buffer, running that buffer's own handler over it.
 *
 * The top-of-stack case is the VM consumer above; the other one is a FLUSH, which
 * php delivers to the buffer BELOW the flushing one rather than to the real output
 * (`ob_start(); ob_start(); echo "x"; ob_end_flush();` leaves "x" in the outer
 * buffer, where PHL used to write it straight to stdout — so buffered output
 * escaped its buffer and printed out of order, ahead of everything the outer
 * buffer was still holding).
 *
 * The buffer is named by its INDEX, never by a pointer: the handler is php code
 * and may ob_start() (which reallocs the stack out from under every pointer into
 * it) or close buffers, so the slot is re-resolved after the call and the write
 * is dropped if the handler took it away.
 */
static void VmObWrite(ph7_vm *pVm,sxu32 nIdx,const void *pData,unsigned int nDataLen)
{
	VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);
	ph7_value sResult;
	if( pEntry == 0 ){
		return;
	}
	PH7_MemObjInit(pVm,&sResult);
	if( ph7_value_is_callable(&pEntry->sCallback) && pVm->nObDepth < 15 ){
		ph7_value sArg,sPhase,*apArg[2];
		/* Fill the first argument */
		PH7_MemObjInitFromString(pVm,&sArg,0);
		PH7_MemObjStringAppend(&sArg,(const char *)pData,nDataLen);
		apArg[0] = &sArg;
		/* php calls the handler as ($buffer, int $phase) — a handler
		 * declaring both as required must not trip the arity check. The
		 * phase is PHP_OUTPUT_HANDLER_WRITE (0); the per-write phase
		 * bitmask semantics (START/FINAL) are not modeled. */
		PH7_MemObjInitFromInt(pVm,&sPhase,0);
		apArg[1] = &sPhase;
		/* Call the 'filter' callback */
		pVm->nObDepth++;
		PH7_VmCallUserFunction(pVm,&pEntry->sCallback,2,apArg,&sResult);
		pVm->nObDepth--;
		if( sResult.iFlags & MEMOBJ_STRING ){
			/* Extract the function result */
			pData = SyBlobData(&sResult.sBlob);
			nDataLen = SyBlobLength(&sResult.sBlob);
		}
		PH7_MemObjRelease(&sArg);
		PH7_MemObjRelease(&sPhase);
		/* The handler ran php code: the slot may have moved, or gone. */
		pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);
	}
	if( nDataLen > 0 && pEntry ){
		/* Redirect the VM output to the internal buffer */
		SyBlobAppend(&pEntry->sOB,pData,nDataLen);
	}
	/* Release */
	PH7_MemObjRelease(&sResult);
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
 * bool ob_start([ callback $output_callback] )
 * This function will turn output buffering on. While output buffering is active no output
 *  is sent from the script (other than headers), instead the output is stored in an internal
 *  buffer.
 * Parameter
 *  $output_callback
 *   An optional output_callback function may be specified. This function takes a string
 *   as a parameter and should return a string. The function will be called when the output
 *   buffer is flushed (sent) or cleaned (with ob_flush(), ob_clean() or similar function)
 *   or when the output buffer is flushed to the browser at the end of the request.
 *   When output_callback is called, it will receive the contents of the output buffer
 *   as its parameter and is expected to return a new output buffer as a result, which will
 *   be sent to the browser. If the output_callback is not a callable function, this function
 *   will return FALSE.
 *   If the callback function has two parameters, the second parameter is filled with
 *   a bit-field consisting of PHP_OUTPUT_HANDLER_START, PHP_OUTPUT_HANDLER_CONT
 *   and PHP_OUTPUT_HANDLER_END.
 *   If output_callback returns FALSE original input is sent to the browser.
 *   The output_callback parameter may be bypassed by passing a NULL value.
 * Return
 *   Returns TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry sOb;
	sxi32 rc;
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
	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ)) ){
		/* Save the callback name for later invocation (MEMOBJ_OBJ = a Closure callback). */
		PH7_MemObjStore(apArg[0],&sOb.sCallback);
	}
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
 * Flush Output buffer to the default VM output consumer.
 * Refer to the implementation of [ob_flush()] for more
 * information.
 */
static sxi32 VmObFlush(ph7_vm *pVm,VmObEntry *pEntry,int bRelease)
{
	SyBlob sPayload;
	sxu32 nBelow;
	sxi32 rc = PH7_OK;
	/* php delivers a flush to the buffer BELOW this one, not to the real output.
	 * `bRelease` is set by the ob_end_* callers, which have already POPPED the
	 * entry — so the enclosing buffer is the new top there, and the one under the
	 * top otherwise. */
	{
		sxu32 nUsed = SySetUsed(&pVm->aOB);
		nBelow = bRelease ? nUsed : (nUsed >= 1 ? nUsed - 1 : 0);
	}
	/* Take the payload OUT of the entry before anything below can run php: a
	 * handler down there may ob_start(), and that reallocs the buffer stack —
	 * every pointer into it, this entry's own blob included, dies with it. */
	SyBlobInit(&sPayload,&pVm->sAllocator);
	if( SyBlobLength(&pEntry->sOB) > 0 ){
		SyBlobDup(&pEntry->sOB,&sPayload);
	}
	/* Retire this buffer first, for the same reason. */
	if( bRelease ){
		VmObRestore(&(*pVm),pEntry);
	}else{
		/* Reset the blob */
		SyBlobReset(&pEntry->sOB);
	}
	if( SyBlobLength(&sPayload) > 0 ){
		if( nBelow > 0 ){
			VmObWrite(pVm,nBelow - 1,SyBlobData(&sPayload),SyBlobLength(&sPayload));
		}else{
			/* Call the VM output consumer */
			rc = pVm->sVmConsumer.xDef(SyBlobData(&sPayload),SyBlobLength(&sPayload),pVm->sVmConsumer.pDefData);
			/* Increment VM output counter */
			pVm->nOutputLen += SyBlobLength(&sPayload);
			if( rc != PH7_ABORT ){
				rc = PH7_OK;
			}
		}
	}
	SyBlobRelease(&sPayload);
	return rc;
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
	VmObEntry *pOb;
	sxi32 rc;
	/* Peek the top most OB entry */
	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);
	if( pOb == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Failed to flush buffer. No buffer to flush");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Flush contents */
	rc = VmObFlush(pVm,pOb,FALSE);
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
	VmObEntry *pOb;
	sxi32 rc;
	/* Pop the top most OB entry */
	pOb = (VmObEntry *)SySetPop(&pVm->aOB);
	if( pOb == 0 ){
		/* Empty stack,return FALSE */
		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
			"Failed to delete and flush buffer. No buffer to delete or flush");
		ph7_result_bool(pCtx,0);
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	/* Flush contents */
	rc = VmObFlush(pVm,pOb,TRUE);
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
	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; n++ ){
		VmObEntry *pEntry = &aEntry[n];
		/* Extract handler name */
		SyBlobReset(&sVal.sBlob);
		if( pEntry->sCallback.iFlags & MEMOBJ_STRING ){
			/* Callback,dup it's name */
			SyBlobDup(&pEntry->sCallback.sBlob,&sVal.sBlob);
		}else if( pEntry->sCallback.iFlags & MEMOBJ_HASHMAP ){
			SyBlobAppend(&sVal.sBlob,"Class Method",sizeof("Class Method")-1);
		}else{
			SyBlobAppend(&sVal.sBlob,"default output handler",sizeof("default output handler")-1);
		}
		sVal.iFlags = MEMOBJ_STRING;
		/* Perform the insertion */
		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);
	}
	PH7_MemObjRelease(&sVal);
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
