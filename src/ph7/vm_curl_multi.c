/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_CURL
#include "curl_int.h"

/*
 * Section:
 *    ext/curl -- the MULTI interface (php's `curl_multi_*`).
 * Status:
 *    The whole multi surface: the handle class, its option setter, the set of
 *    easy handles it holds, and the verbs that drive them. The share half
 *    follows.
 *
 * WHAT A MULTI HANDLE IS HERE. libcurl's multi interface is a SET of easy
 * handles plus a scheduler; php wraps the set in an object and keeps its own
 * ordered list of the CurlHandle OBJECTS beside libcurl's, because two of its
 * answers are about the objects and not about the library: get_handles() hands
 * back the same instances that were added, and the set keeps them ALIVE --
 * `unset($h)` after an add leaves the transfer running.
 *
 * Every answer below is derived by asking php 8.5 and libcurl 8.5 the same
 * question, the way the rest of this binding is.
 */

/* ------------------------------------------------------------------------
 * Lifetime
 * ------------------------------------------------------------------------ */
static phl_curlm * CurlMultiNew(ph7_vm *pVm)
{
	phl_curlm *pMulti = (phl_curlm *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curlm));
	if( pMulti == 0 ){
		return 0;
	}
	SyZero(pMulti,sizeof(phl_curlm));
	pMulti->pVm = pVm;
	pMulti->pMulti = curl_multi_init();
	if( pMulti->pMulti == 0 ){
		SyMemBackendFree(&pVm->sAllocator,pMulti);
		return 0;
	}
	pMulti->pNext = (phl_curlm *)pVm->pCurlMultis;
	pVm->pCurlMultis = pMulti;
	return pMulti;
}
/*
 * Drop one entry: libcurl stops scheduling the transfer, and the reference the
 * set held on the object goes with it.
 */
static void CurlMultiDropEntry(phl_curlm *pMulti,phl_curlm_ent *pEnt)
{
	phl_curl *pCurl = (phl_curl *)PH7_CurlEasyOfInstance(pEnt->pObj);
	if( pMulti->pMulti && pCurl && pCurl->pEasy ){
		curl_multi_remove_handle(pMulti->pMulti,pCurl->pEasy);
	}
	if( pEnt->pVal ){
		ph7_release_value(pMulti->pVm,pEnt->pVal);
	}
	SyMemBackendFree(&pMulti->pVm->sAllocator,pEnt);
}
static void CurlMultiFree(phl_curlm *pMulti)
{
	phl_curlm_ent *pEnt = pMulti->pHandles;
	while( pEnt ){
		phl_curlm_ent *pNext = pEnt->pNext;
		CurlMultiDropEntry(pMulti,pEnt);
		pEnt = pNext;
	}
	pMulti->pHandles = 0;
	if( pMulti->pMulti ){
		/* Every easy handle is out by now: curl_multi_cleanup() on a set that
		 * still holds one leaves that handle's connection in a freed cache. */
		curl_multi_cleanup(pMulti->pMulti);
		pMulti->pMulti = 0;
	}
	if( pMulti->pPushCb ){
		ph7_release_value(pMulti->pVm,pMulti->pPushCb);
		pMulti->pPushCb = 0;
	}
}
/*
 * Free every registered multi. Runs BEFORE the easy-handle sweep (see
 * PH7_CurlVmReset), because a live multi still points at the CURL*s it was
 * given and curl_multi_remove_handle has to reach them.
 */
PH7_PRIVATE void PH7_CurlMultiVmSweep(ph7_vm *pVm)
{
	phl_curlm *pMulti = (phl_curlm *)pVm->pCurlMultis;
	while( pMulti ){
		phl_curlm *pNext = pMulti->pNext;
		PH7_CurlBlankSlot(pMulti->pOwner);
		CurlMultiFree(pMulti);
		SyMemBackendFree(&pVm->sAllocator,pMulti);
		pMulti = pNext;
	}
	pVm->pCurlMultis = 0;
}
/*
 * The object is going away: tear the set down now rather than at VM reset, so
 * a script that drops its last reference releases the sockets there. The shell
 * stays on the registry (the sweep frees it) because the slot is still
 * reachable while the instance is being torn down.
 */
static void CurlMultiInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_curlm *pMulti = (phl_curlm *)PH7_CurlSlotOf(pThis);
	SXUNUSED(pVm);
	if( pMulti == 0 || pMulti->pOwner != pThis ){
		return;
	}
	CurlMultiFree(pMulti);
	pMulti->pOwner = 0;
}
/*
 * The CurlMultiHandle argument of every verb. The signature table has already
 * screened the TYPE, so a miss here means an object the engine tore down.
 */
static phl_curlm * CurlMultiArg(int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	pThis = (nArg > 0 && apArg && ph7_value_is_object(apArg[0])) ?
		(ph7_class_instance *)apArg[0]->x.pOther : 0;
	return (phl_curlm *)PH7_CurlSlotOf(pThis);
}
/* The CurlHandle argument of add/remove, as an object. */
static ph7_class_instance * CurlEasyArgObj(int nArg,ph7_value **apArg)
{
	if( nArg < 2 || apArg == 0 || !ph7_value_is_object(apArg[1]) ){
		return 0;
	}
	return (ph7_class_instance *)apArg[1]->x.pOther;
}

/* ------------------------------------------------------------------------
 * The verbs
 * ------------------------------------------------------------------------ */
/* CurlMultiHandle curl_multi_init() */
static int vm_builtin_curl_multi_init(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis;
	phl_curlm *pMulti;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pMulti = CurlMultiNew(pVm);
	if( pMulti == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pThis = PH7_CurlNewInstance(pVm,"CurlMultiHandle",sizeof("CurlMultiHandle")-1);
	if( pThis == 0 || PH7_CurlSlotAttach(pThis,(void *)pMulti) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pMulti->pOwner = pThis;
	PH7_NativeResultObject(pCtx,pThis);
	return PH7_OK;
}
/*
 * void curl_multi_close(CurlMultiHandle $multi_handle)
 *
 * EMPTIES the set, and does not free it. php 8 turned the resource into an
 * object, so the object's own teardown is what frees the CURLM -- but unlike
 * curl_close(), which is a pure no-op, this one drops every added handle:
 * get_handles() answers the empty array afterwards, and with it go the
 * references the set held. The multi then still works, which is how the two
 * halves are told apart -- add_handle answers CURLM_OK again, and a second
 * close is not an error. Freeing here would make each of those a
 * use-after-free on a script php runs happily.
 */
static int vm_builtin_curl_multi_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);
	if( pMulti ){
		phl_curlm_ent *pEnt = pMulti->pHandles;
		while( pEnt ){
			phl_curlm_ent *pNext = pEnt->pNext;
			CurlMultiDropEntry(pMulti,pEnt);
			pEnt = pNext;
		}
		pMulti->pHandles = 0;
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
/* int curl_multi_errno(CurlMultiHandle $multi_handle) */
static int vm_builtin_curl_multi_errno(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);
	ph7_result_int(pCtx,pMulti ? pMulti->iLastErr : 0);
	return PH7_OK;
}
/* The entry an object already has in this set, or 0. */
static phl_curlm_ent * CurlMultiFind(phl_curlm *pMulti,ph7_class_instance *pObj)
{
	phl_curlm_ent *pEnt = pMulti->pHandles;
	while( pEnt ){
		if( pEnt->pObj == pObj ){
			return pEnt;
		}
		pEnt = pEnt->pNext;
	}
	return 0;
}
/* Append one entry, holding the object's reference through a php value. */
static int CurlMultiAppend(phl_curlm *pMulti,ph7_value *pObjVal,ph7_class_instance *pObj)
{
	ph7_vm *pVm = pMulti->pVm;
	phl_curlm_ent *pEnt,*pTail;
	pEnt = (phl_curlm_ent *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curlm_ent));
	if( pEnt == 0 ){
		return -1;
	}
	SyZero(pEnt,sizeof(phl_curlm_ent));
	pEnt->pObj = pObj;
	pEnt->pVal = ph7_new_scalar(pVm);
	if( pEnt->pVal == 0 ){
		SyMemBackendFree(&pVm->sAllocator,pEnt);
		return -1;
	}
	/* The set OWNS a reference: php's multi keeps the handle alive for as long
	 * as it holds it, so `unset($h)` after an add is not the end of the
	 * transfer. */
	PH7_MemObjStore(pObjVal,pEnt->pVal);
	if( pMulti->pHandles == 0 ){
		pMulti->pHandles = pEnt;
		return 0;
	}
	pTail = pMulti->pHandles;
	while( pTail->pNext ){
		pTail = pTail->pNext;
	}
	pTail->pNext = pEnt;
	return 0;
}
/*
 * int curl_multi_add_handle(CurlMultiHandle $multi_handle, CurlHandle $handle)
 *
 * The CURLMcode comes from libcurl, and the two failures a script can produce
 * are one code: adding a handle this set already holds, and adding one another
 * set holds, are both CURLM_ADDED_ALREADY (7). The set is only extended when
 * the library accepted the handle, so a refused add leaves get_handles()
 * unchanged.
 *
 * The body BUFFER is reset here, which is php's answer and not libcurl's: a
 * handle added for a second transfer answers only what the second one wrote,
 * so curl_multi_getcontent() after the re-add and before the exec is the empty
 * string rather than the previous body.
 */
static int vm_builtin_curl_multi_add_handle(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);
	ph7_class_instance *pObj = CurlEasyArgObj(nArg,apArg);
	phl_curl *pCurl = pObj ? (phl_curl *)PH7_CurlEasyOfInstance(pObj) : 0;
	CURLMcode rc;
	if( pMulti == 0 || pMulti->pMulti == 0 || pCurl == 0 || pCurl->pEasy == 0 ){
		ph7_result_int(pCtx,CURLM_BAD_HANDLE);
		return PH7_OK;
	}
	rc = curl_multi_add_handle(pMulti->pMulti,pCurl->pEasy);
	pMulti->iLastErr = (int)rc;
	if( rc == CURLM_OK ){
		PH7_CurlBodyReset(pCurl);
		if( CurlMultiFind(pMulti,pObj) == 0 && CurlMultiAppend(pMulti,apArg[1],pObj) != 0 ){
			/* Out of memory building the entry: undo the add rather than leave
			 * libcurl scheduling a transfer this set cannot report on. */
			curl_multi_remove_handle(pMulti->pMulti,pCurl->pEasy);
			rc = CURLM_OUT_OF_MEMORY;
			pMulti->iLastErr = (int)rc;
		}
	}
	ph7_result_int(pCtx,(int)rc);
	return PH7_OK;
}
/*
 * int curl_multi_remove_handle(CurlMultiHandle $multi_handle, CurlHandle $handle)
 *
 * Removing a handle this set never held is libcurl's CURLM_BAD_EASY_HANDLE (2)
 * when ANOTHER set holds it, and CURLM_OK when no set does -- the library's
 * distinction, not php's, and the reason the answer is read off libcurl rather
 * than off the list here.
 */
static int vm_builtin_curl_multi_remove_handle(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);
	ph7_class_instance *pObj = CurlEasyArgObj(nArg,apArg);
	phl_curl *pCurl = pObj ? (phl_curl *)PH7_CurlEasyOfInstance(pObj) : 0;
	CURLMcode rc;
	if( pMulti == 0 || pMulti->pMulti == 0 || pCurl == 0 || pCurl->pEasy == 0 ){
		ph7_result_int(pCtx,CURLM_BAD_HANDLE);
		return PH7_OK;
	}
	rc = curl_multi_remove_handle(pMulti->pMulti,pCurl->pEasy);
	pMulti->iLastErr = (int)rc;
	if( rc == CURLM_OK ){
		phl_curlm_ent *pEnt = pMulti->pHandles,*pPrev = 0;
		while( pEnt ){
			if( pEnt->pObj == pObj ){
				if( pPrev ){
					pPrev->pNext = pEnt->pNext;
				}else{
					pMulti->pHandles = pEnt->pNext;
				}
				/* Already out of libcurl's set: drop the reference only. */
				if( pEnt->pVal ){
					ph7_release_value(pMulti->pVm,pEnt->pVal);
				}
				SyMemBackendFree(&pMulti->pVm->sAllocator,pEnt);
				break;
			}
			pPrev = pEnt;
			pEnt = pEnt->pNext;
		}
	}
	ph7_result_int(pCtx,(int)rc);
	return PH7_OK;
}
/*
 * array curl_multi_get_handles(CurlMultiHandle $multi_handle)
 *
 * php's own list, not libcurl's: the answer is the same OBJECTS that were
 * added, in the order they were added, re-indexed from 0 -- so a handle
 * removed from the middle leaves no hole, and one removed and added again is
 * last. It does not touch the error state.
 */
static int vm_builtin_curl_multi_get_handles(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);
	ph7_value *pArray = ph7_context_new_array(pCtx);
	phl_curlm_ent *pEnt;
	if( pArray == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	for( pEnt = pMulti ? pMulti->pHandles : 0 ; pEnt ; pEnt = pEnt->pNext ){
		ph7_array_add_elem(pArray,0,pEnt->pVal);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Driving the set
 * ------------------------------------------------------------------------ */
/*
 * int curl_multi_exec(CurlMultiHandle $multi_handle, int &$still_running)
 *
 * One turn of libcurl's scheduler over every handle in the set, and the count
 * of transfers still going written back through the reference. It is the whole
 * loop a program writes: exec, select, exec again until nothing runs.
 *
 * Every handle's handlers are installed before each turn rather than once at
 * add time, because a script may point a handle somewhere else between two
 * turns and nothing else would notice. The CONTEXT they carry is this call's,
 * which is what makes a body with no destination of its own print through the
 * VM's output consumer (so an ob_start() around the loop catches it) and what
 * a callback's throw is raised on.
 *
 * A throw is parked per handle by the callback rail and raised HERE, once the
 * library has unwound -- the transfer itself carries on, so the set's own
 * answer for it is the CURLcode it really ended in.
 */
static int vm_builtin_curl_multi_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);
	phl_curlm_ent *pEnt;
	ph7_value sVal;
	sxi32 rcExc = PH7_OK;
	int nRunning = 0;
	CURLMcode rc = CURLM_BAD_HANDLE;
	if( pMulti && pMulti->pMulti ){
		for( pEnt = pMulti->pHandles ; pEnt ; pEnt = pEnt->pNext ){
			phl_curl *pCurl = (phl_curl *)PH7_CurlEasyOfInstance(pEnt->pObj);
			if( pCurl ){
				PH7_CurlBeginTransfer(pCurl,pCtx);
			}
		}
		rc = curl_multi_perform(pMulti->pMulti,&nRunning);
		for( pEnt = pMulti->pHandles ; pEnt ; pEnt = pEnt->pNext ){
			phl_curl *pCurl = (phl_curl *)PH7_CurlEasyOfInstance(pEnt->pObj);
			if( pCurl == 0 ){
				continue;
			}
			PH7_CurlEndTransfer(pCurl);
			if( pCurl->iCbExc != 0 ){
				/* The first throw of the turn is the one that travels; the rest
				 * are dropped with the same rule a second callback follows. */
				if( rcExc == PH7_OK ){
					rcExc = pCurl->iCbExc;
				}
				pCurl->iCbExc = 0;
			}
		}
		pMulti->iLastErr = (int)rc;
	}
	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,(sxi64)nRunning);
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],&sVal);
	PH7_MemObjRelease(&sVal);
	ph7_result_int(pCtx,(int)rc);
	return rcExc;
}
/*
 * int curl_multi_select(CurlMultiHandle $multi_handle, float $timeout = 1.0)
 *
 * Waits for one of the set's sockets to become readable or writable and
 * answers how many did -- or 0 when the timeout ran out first, which is also
 * the immediate answer for a set with no socket at all: the wait is libcurl's
 * curl_multi_wait(), which does not sleep for a set it has nothing to wait on.
 * An error is -1, and the set's error state is NOT touched either way.
 *
 * The bound on the timeout is php's, not libcurl's: the seconds become an int
 * of MILLISECONDS, so anything past INT_MAX of them is refused up front -- and
 * so is a NaN, which the comparison below rejects by not being >= 0.
 */
static int vm_builtin_curl_multi_select(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);
	double rTimeout = nArg > 1 ? (double)ph7_value_to_double(apArg[1]) : 1.0;
	int nFds = 0;
	CURLMcode rc;
	if( !(rTimeout >= 0.0) || rTimeout > (double)SXI32_HIGH / 1000.0 ){
		ph7_result_bool(pCtx,0);
		return PH7_VmThrowException(pCtx,"ValueError",
			"curl_multi_select(): Argument #2 ($timeout) must be between 0 and %f",
			(double)SXI32_HIGH / 1000.0);
	}
	if( pMulti == 0 || pMulti->pMulti == 0 ){
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	rc = curl_multi_wait(pMulti->pMulti,0,0,(int)(rTimeout * 1000.0),&nFds);
	ph7_result_int(pCtx,rc == CURLM_OK ? nFds : -1);
	return PH7_OK;
}
/*
 * array|false curl_multi_info_read(CurlMultiHandle $multi_handle, int &$queued_messages = null)
 *
 * One message off libcurl's queue -- always a CURLMSG_DONE -- as php's three
 * keys in php's order, with the CurlHandle OBJECT that was added rather than
 * the CURL* the message names. The count still queued is written back only
 * when there WAS a message: an empty queue answers false and leaves the
 * reference exactly as the caller left it.
 *
 * This is also the verb that gives the easy handle its error state. Until the
 * message is read, curl_errno() on a handle whose multi transfer already
 * finished still answers 0 -- the transfer reported to the SET, and reading the
 * message is what moves the result onto the handle. libcurl wrote the detailed
 * text into the handle's error buffer during the transfer; a code with no
 * detail falls back to its own sentence, the same as the easy rail.
 */
static int vm_builtin_curl_multi_info_read(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);
	ph7_value *pArray,*pVal;
	phl_curlm_ent *pEnt;
	CURLMsg *pMsg;
	int nQueued = 0;
	if( pMulti == 0 || pMulti->pMulti == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pMsg = curl_multi_info_read(pMulti->pMulti,&nQueued);
	if( pMsg == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_value_int64(pVal,(sxi64)pMsg->msg);
	ph7_array_add_strkey_elem(pArray,"msg",pVal);
	ph7_value_int64(pVal,(sxi64)pMsg->data.result);
	ph7_array_add_strkey_elem(pArray,"result",pVal);
	for( pEnt = pMulti->pHandles ; pEnt ; pEnt = pEnt->pNext ){
		phl_curl *pCurl = (phl_curl *)PH7_CurlEasyOfInstance(pEnt->pObj);
		if( pCurl == 0 || pCurl->pEasy != pMsg->easy_handle ){
			continue;
		}
		PH7_CurlRecordResult(pCurl,(int)pMsg->data.result);
		ph7_array_add_strkey_elem(pArray,"handle",pEnt->pVal);
		break;
	}
	if( nArg > 1 ){
		ph7_value_int64(pVal,(sxi64)nQueued);
		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pVal);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * ?string curl_multi_getcontent(CurlHandle $handle)
 *
 * The body a RETURNTRANSFER handle collected -- the only way to reach it after
 * a multi transfer, which has no call to answer it. It reads the DESTINATION
 * that stands now rather than what the last transfer did, so a handle whose
 * body went to the output, to a callback or to a stream answers null, and one
 * that was reset after collecting a body answers null too. The verb belongs to
 * the easy handle: it is the same buffer a plain curl_exec() answers, and
 * asking twice answers twice.
 */
static int vm_builtin_curl_multi_getcontent(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = (nArg > 0 && apArg && ph7_value_is_object(apArg[0])) ?
		(ph7_class_instance *)apArg[0]->x.pOther : 0;
	phl_curl *pCurl = pThis ? (phl_curl *)PH7_CurlEasyOfInstance(pThis) : 0;
	if( pCurl == 0 || pCurl->iWriteDest != PHL_CURL_DEST_RETURN ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_CurlResultBody(pCtx,pCurl);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * curl_multi_setopt()
 * ------------------------------------------------------------------------ */
/*
 * php's nine multi options, in two kinds. Everything but the push callback is a
 * long libcurl reads, and php converts whatever it was given -- a string, an
 * array, null -- with the ordinary int cast rather than refusing it.
 *
 * An option php does not know is a ValueError, and the handle's error state
 * moves with it: curl_multi_errno() answers CURLM_UNKNOWN_OPTION (6) after the
 * throw, a code php declares no constant for.
 */
#define PHL_CURLM_OPT_LONG 0
#define PHL_CURLM_OPT_PUSH 1
static const struct CurlMultiOptDef {
	int iOpt;
	int iKind;
	const char *zName;
} aCurlMultiOpt[] = {
	{ CURLMOPT_PIPELINING,                  PHL_CURLM_OPT_LONG, "CURLMOPT_PIPELINING" },
	{ CURLMOPT_MAXCONNECTS,                 PHL_CURLM_OPT_LONG, "CURLMOPT_MAXCONNECTS" },
	{ CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE,   PHL_CURLM_OPT_LONG, "CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE" },
	{ CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE, PHL_CURLM_OPT_LONG, "CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE" },
	{ CURLMOPT_MAX_HOST_CONNECTIONS,        PHL_CURLM_OPT_LONG, "CURLMOPT_MAX_HOST_CONNECTIONS" },
	{ CURLMOPT_MAX_PIPELINE_LENGTH,         PHL_CURLM_OPT_LONG, "CURLMOPT_MAX_PIPELINE_LENGTH" },
	{ CURLMOPT_MAX_TOTAL_CONNECTIONS,       PHL_CURLM_OPT_LONG, "CURLMOPT_MAX_TOTAL_CONNECTIONS" },
	{ CURLMOPT_MAX_CONCURRENT_STREAMS,      PHL_CURLM_OPT_LONG, "CURLMOPT_MAX_CONCURRENT_STREAMS" },
	{ CURLMOPT_PUSHFUNCTION,                PHL_CURLM_OPT_PUSH, "CURLMOPT_PUSHFUNCTION" }
};
static const struct CurlMultiOptDef * CurlMultiOptFind(sxi64 iOpt)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aCurlMultiOpt) ; ++n ){
		if( (sxi64)aCurlMultiOpt[n].iOpt == iOpt ){
			return &aCurlMultiOpt[n];
		}
	}
	return 0;
}
/*
 * bool curl_multi_setopt(CurlMultiHandle $multi_handle, int $option, mixed $value)
 *
 * CURLMOPT_PIPELINING carries a diagnostic of php's own: libcurl dropped HTTP/1
 * pipelining, so CURLPIPE_HTTP1 (which is what a plain `true` casts to) is a
 * WARNING naming the constant, and the call still answers true.
 */
static int vm_builtin_curl_multi_setopt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curlm *pMulti = CurlMultiArg(nArg,apArg);
	const struct CurlMultiOptDef *pDef;
	sxi64 iOpt;
	CURLMcode rc;
	if( pMulti == 0 || pMulti->pMulti == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iOpt = ph7_value_to_int64(apArg[1]);
	pDef = CurlMultiOptFind(iOpt);
	if( pDef == 0 ){
		pMulti->iLastErr = (int)CURLM_UNKNOWN_OPTION;
		ph7_result_bool(pCtx,0);
		return PH7_VmThrowException(pCtx,"ValueError",
			"curl_multi_setopt(): Argument #2 ($option) is not a valid cURL multi option");
	}
	if( pDef->iKind == PHL_CURLM_OPT_PUSH ){
		sxi32 rcThrow = PH7_OK;
		int rcCb = PH7_CurlSetCallback(pCtx,pMulti->pVm,&pMulti->pPushCb,apArg[2],
			"curl_multi_setopt","#2 ($option)",pDef->zName,FALSE,&rcThrow);
		if( rcCb < 0 ){
			ph7_result_bool(pCtx,0);
			return rcThrow;
		}
		if( rcCb == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/*
		 * The callable is validated and retained -- php's whole setopt answer
		 * for this option, TypeErrors included -- and no push callback is
		 * installed on the library. libcurl's default with none is to DENY
		 * every pushed stream, which is what a php callback answering
		 * CURL_PUSH_DENY produces, so the only script this differs for is one
		 * whose callback would have said CURL_PUSH_OK. Calling it needs a php
		 * CurlHandle over an easy handle LIBCURL owns and hands out mid-push,
		 * and only an HTTP/2 peer that actually pushes can derive what that
		 * ownership is -- no corpus here has one. §7.4 carries it.
		 */
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( iOpt == CURLMOPT_PIPELINING && ph7_value_to_int64(apArg[2]) == CURLPIPE_HTTP1 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,
			"CURLPIPE_HTTP1 is no longer supported");
	}
	rc = curl_multi_setopt(pMulti->pMulti,(CURLMoption)pDef->iOpt,
		(long)ph7_value_to_int64(apArg[2]));
	pMulti->iLastErr = (int)rc;
	ph7_result_bool(pCtx,rc == CURLM_OK);
	return PH7_OK;
}

/* ===== Installation ===== */

PH7_PRIVATE sxi32 PH7_VmInstallCurlMulti(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "curl_multi_init",          vm_builtin_curl_multi_init          },
		{ "curl_multi_close",         vm_builtin_curl_multi_close         },
		{ "curl_multi_errno",         vm_builtin_curl_multi_errno         },
		{ "curl_multi_setopt",        vm_builtin_curl_multi_setopt        },
		{ "curl_multi_add_handle",    vm_builtin_curl_multi_add_handle    },
		{ "curl_multi_remove_handle", vm_builtin_curl_multi_remove_handle },
		{ "curl_multi_get_handles",   vm_builtin_curl_multi_get_handles   },
		{ "curl_multi_exec",          vm_builtin_curl_multi_exec          },
		{ "curl_multi_select",        vm_builtin_curl_multi_select        },
		{ "curl_multi_info_read",     vm_builtin_curl_multi_info_read     },
		{ "curl_multi_getcontent",    vm_builtin_curl_multi_getcontent    }
	};
	/*
	 * The set, and nothing else: php's CurlMultiHandle declares no method, no
	 * constant and no property, and prints as an empty object on every
	 * presentation surface. The one slot here is engine storage, hidden so it
	 * appears on none of them.
	 */
	static const PH7_NativePropDef aProp[] = {
		{ "__res", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }
	};
	/*
	 * FINAL, NOINSTANTIATE with php's own per-class sentence, NOSERIALIZE and
	 * -- unlike CurlHandle -- NOCLONE: libcurl has no curl_multi_duphandle, so
	 * php's `clone $mh` is "Trying to clone an uncloneable object".
	 */
	static const PH7_NativeClassSpec aSpec[] = {
		{ "CurlMultiHandle", 0, 0,
		  PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOSERIALIZE|PH7_CLASS_NOCLONE,
		  0, 0, 0, 0,
		  aProp, SX_ARRAYSIZE(aProp),
		  CurlMultiInstanceRelease, 0, 0 }
	};
	sxu32 n;
	sxi32 rc;
	pVm->pCurlMultis = 0;
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; ++n ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc == SXRET_OK ){
		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"CurlMultiHandle",
			sizeof("CurlMultiHandle")-1,FALSE,0);
		if( pClass ){
			pClass->zNewRefusal =
				"Cannot directly construct CurlMultiHandle, use curl_multi_init() instead";
		}
	}
	return rc;
}

#else
/* Ensure non-empty translation unit when curl is disabled (MSVC C4206) */
typedef int vm_curl_multi_unused;
#endif /* PH7_ENABLE_CURL */
