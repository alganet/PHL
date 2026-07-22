/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);
/*
 * void ob_clean(void)
 *  This function discards the contents of the output buffer.
 *  This function does not destroy the output buffer like ob_end_clean() does.
 * Parameter
 *  None
 * Return
 *  No value is returned.
 */
PH7_PRIVATE int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pOb;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* Peek the top most OB */
	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);
	if( pOb ){
		SyBlobRelease(&pOb->sOB);
	}
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
 * string ob_get_flush(void)
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
	VmObEntry *pEntry;
	ph7_value sResult;
	/* Peek the top most entry */
	pEntry = (VmObEntry *)SySetPeek(&pVm->aOB);
	if( pEntry == 0 ){
		/* CAN'T HAPPEN */
		return PH7_OK;
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
	}
	if( nDataLen > 0 ){
		/* Redirect the VM output to the internal buffer */
		SyBlobAppend(&pEntry->sOB,pData,nDataLen);
	}
	/* Release */
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
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
	SyBlob *pBlob = &pEntry->sOB;
	sxi32 rc;
	/* Flush contents */
	rc = PH7_OK;
	if( SyBlobLength(pBlob) > 0 ){
		/* Call the VM output consumer */
		rc = pVm->sVmConsumer.xDef(SyBlobData(pBlob),SyBlobLength(pBlob),pVm->sVmConsumer.pDefData);
		/* Increment VM output counter */
		pVm->nOutputLen += SyBlobLength(pBlob);
		if( rc != PH7_ABORT ){
			rc = PH7_OK;
		}
	}
	if( bRelease ){
		VmObRestore(&(*pVm),pEntry);
	}else{
		/* Reset the blob */
		SyBlobReset(pBlob);
	}
	return rc;
}
/*
 * void ob_flush(void)
 * void flush(void)
 *  Flush (send) the output buffer.
 * Parameter
 *  None
 * Return
 *  No return value.
 */
PH7_PRIVATE int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmObEntry *pOb;
	sxi32 rc;
	/* Peek the top most OB entry */
	pOb = (VmObEntry *)SySetPeek(&pVm->aOB);
	if( pOb == 0 ){
		/* Empty stack,return immediately */
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		return PH7_OK;
	}
	/* Flush contents */
	rc = VmObFlush(pVm,pOb,FALSE);
	return rc;
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
	if( SySetUsed(&pVm->aOB) < 1 ){
		/* Empty stack,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
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
