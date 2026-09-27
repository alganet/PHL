/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_SQLITE
#include "ph7int.h"
#include "pdo_int.h"

/*
 * ext/pdo: the driver-INDEPENDENT half -- the PDO, PDOStatement and
 * PDOException class declarations, their constants, and (later slices) the
 * fetch-mode machinery, parameter binding and the SQLSTATE/errmode plumbing
 * every driver shares.  The sqlite backend itself lives in vm_pdo_sqlite.c,
 * which also declares the `Pdo\Sqlite` subclass PDO::connect() answers.
 *
 * Scope is deliberately narrow: the SQLite driver only.  PDO::getAvailableDrivers()
 * therefore answers exactly one name, and a DSN naming any other driver is
 * php's own `could not find driver`.
 *
 * The class surface is DERIVED from the php 8.5 oracle rather than written
 * from the manual, and §10's non-deprecated rule removes part of it: php 8.5
 * still declares seven `PDO::SQLITE_*` constants that report
 * ReflectionClassConstant::isDeprecated(), superseded by the unprefixed
 * `Pdo\Sqlite::` spellings.  PHL declares only the successors.
 */

static void PdoStmtSweep(phl_pdo *pConn);
static void PdoBlankSlot(ph7_class_instance *pOwner);
static phl_pdo * PdoOfInstance(ph7_class_instance *pThis);
static void PdoStmtClearFetchState(phl_pdo_stmt *pSt);
static void PdoStmtLazyClear(phl_pdo_stmt *pSt);
static void PdoBoundColumnsForRow(ph7_vm *pVm,phl_pdo_stmt *pSt);
static int PdoBoundColumnsForIterRow(ph7_vm *pVm,phl_pdo_stmt *pSt);
static sxi32 PdoBoundColumnsRefuse(ph7_context *pCtx,phl_pdo_stmt *pSt,int bWholeSet);
static int PdoBoundColumnsBad(phl_pdo_stmt *pSt);

/* ------------------------------------------------------------------------
 * Connection lifetime
 * ------------------------------------------------------------------------ */
/*
 * A connection is reached from its PDO object through the hidden `__res` slot
 * and is ALSO chained on the per-VM registry, because PH7 resources have no
 * destructor hook: the sweep at VM reset/release is what closes a database a
 * script left open.  `clone` is refused on both classes, so no second object
 * can ever reach one record.
 */
PH7_PRIVATE phl_pdo * PH7_PdoNewConn(ph7_vm *pVm)
{
	phl_pdo *pConn = (phl_pdo *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo));
	if( pConn == 0 ){
		return 0;
	}
	SyZero(pConn,sizeof(phl_pdo));
	pConn->pVm = pVm;
	/* php's defaults for a fresh sqlite handle: exceptions on, both column
	 * shapes, no case folding, no null rewriting, native column types. */
	pConn->iErrMode = PDO_ERRMODE_EXCEPTION;
	pConn->iCase = PDO_CASE_NATURAL;
	pConn->iOracleNulls = PDO_NULL_NATURAL;
	pConn->iDefaultFetch = PDO_FETCH_BOTH;
	pConn->iErrState = PDO_ERR_NONE;
	pConn->pNext = (phl_pdo *)pVm->pPdoConns;
	pVm->pPdoConns = pConn;
	return pConn;
}
PH7_PRIVATE void PH7_PdoFreeConn(phl_pdo *pConn)
{
	/* sqlite refuses to close a database that still has a live statement, so
	 * the cursors go first. */
	PdoStmtSweep(pConn);
	{
		/* the callbacks sqlite still points at; the close is what makes them
		 * unreachable, so they are released after it below */
		phl_pdo_udf *pUdf = pConn->pUdfs;
		while( pUdf ){
			phl_pdo_udf *pNext = pUdf->pNext;
			if( pUdf->pCallback ){
				ph7_release_value(pConn->pVm,pUdf->pCallback);
			}
			if( pUdf->pFinalize ){
				ph7_release_value(pConn->pVm,pUdf->pFinalize);
			}
			if( pUdf->zName ){
				SyMemBackendFree(&pConn->pVm->sAllocator,pUdf->zName);
			}
			SyMemBackendFree(&pConn->pVm->sAllocator,pUdf);
			pUdf = pNext;
		}
		pConn->pUdfs = 0;
	}
	PH7_PdoSqliteClose(pConn);
	if( pConn->zDrvMsg ){
		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);
		pConn->zDrvMsg = 0;
	}
	if( pConn->zStmtClass ){
		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zStmtClass);
		pConn->zStmtClass = 0;
	}
}
/*
 * Free every registered connection.  Called from PH7_PdoVmReset (a reused VM --
 * the -S server's -- must not answer the next request through the previous
 * one's database) and from PH7_PdoVmRelease before the allocator that holds the
 * shells is torn down.
 */
static void PdoVmSweep(ph7_vm *pVm)
{
	phl_pdo *pConn = (phl_pdo *)pVm->pPdoConns;
	while( pConn ){
		phl_pdo *pNext = pConn->pNext;
		PdoBlankSlot(pConn->pOwner);
		PH7_PdoFreeConn(pConn);
		SyMemBackendFree(&pVm->sAllocator,pConn);
		pConn = pNext;
	}
	pVm->pPdoConns = 0;
}
PH7_PRIVATE void PH7_PdoVmReset(ph7_vm *pVm)
{
	PdoVmSweep(&(*pVm));
}
PH7_PRIVATE void PH7_PdoVmRelease(ph7_vm *pVm)
{
	PdoVmSweep(&(*pVm));
}
/*
 * Blank the hidden slot of the object that holds a record we are about to
 * free.  Without this the object outlives its record -- a PDOStatement whose
 * connection was released first, or any handle alive at VM teardown -- and its
 * own release reads freed memory to ask whether it still owns one. (ASan found
 * exactly that; nothing in the ordinary build noticed.)
 */
static void PdoBlankSlot(ph7_class_instance *pOwner)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pOwner == 0 ){
		return;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);
	if( pRes ){
		PH7_MemObjRelease(pRes);
		MemObjSetType(pRes,MEMOBJ_NULL);
	}
}
/* The connection behind a `__res` slot value. */
static phl_pdo * PdoOfValue(ph7_value *pVal)
{
	if( pVal == 0 || !ph7_value_is_resource(pVal) ){
		return 0;
	}
	return (phl_pdo *)ph7_value_to_resource(pVal);
}
PH7_PRIVATE phl_pdo * PH7_PdoConnOfInstance(ph7_class_instance *pThis)
{
	return PdoOfInstance(pThis);
}
static phl_pdo * PdoOfInstance(ph7_class_instance *pThis)
{
	SyString sAttr;
	if( pThis == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	return PdoOfValue(PH7_ClassInstanceFetchAttr(pThis,&sAttr));
}
/* Store one connection in the receiver's hidden slot. */
static int PdoAttach(ph7_class_instance *pThis,phl_pdo *pConn)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return -1;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 ){
		return -1;
	}
	PH7_MemObjRelease(pRes);
	pRes->x.pOther = pConn;
	MemObjSetType(pRes,MEMOBJ_RES);
	pConn->pOwner = pThis;
	return 0;
}
/*
 * The object is going away: close its database now rather than at VM reset, so
 * a script that unsets its last reference releases the file lock there -- which
 * is what php does, and what a test that unlinks the file afterwards needs.
 * The shell stays on the registry (the sweep frees it) because the slot is
 * still reachable while the instance is being torn down.
 */
static void PdoInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_pdo *pConn = PdoOfInstance(pThis);
	SXUNUSED(pVm);
	if( pConn == 0 || pConn->pOwner != pThis ){
		return;
	}
	PdoStmtSweep(pConn);
	PH7_PdoSqliteClose(pConn);
	pConn->pOwner = 0;
}

/* ------------------------------------------------------------------------
 * Statement lifetime
 * ------------------------------------------------------------------------ */
/*
 * A statement is chained on its CONNECTION rather than on the VM: sqlite will
 * not close a database with a live statement on it, so the connection's own
 * close has to finalize them first. The object reaches it through the same
 * hidden slot a connection uses.
 */
PH7_PRIVATE phl_pdo_stmt * PH7_PdoNewStmt(phl_pdo *pConn)
{
	phl_pdo_stmt *pSt = (phl_pdo_stmt *)SyMemBackendAlloc(&pConn->pVm->sAllocator,
		sizeof(phl_pdo_stmt));
	if( pSt == 0 ){
		return 0;
	}
	SyZero(pSt,sizeof(phl_pdo_stmt));
	pSt->pConn = pConn;
	pSt->iFetchMode = pConn->iDefaultFetch;
	pSt->iErrState = PDO_ERR_NONE;
	/* Retain the PDO object. `$db->query(...)` on a temporary connection hands
	 * back a statement that outlives it, and php keeps the connection alive
	 * through exactly this reference -- without it the database closes while
	 * the statement is still being read. */
	pSt->pConnObj = pConn->pOwner;
	if( pSt->pConnObj ){
		pSt->pConnObj->iRef++;
	}
	pSt->pNext = pConn->pStmts;
	pConn->pStmts = pSt;
	return pSt;
}
/* Drop what bindValue()/bindParam() recorded. */
static void PdoBindListFree(ph7_vm *pVm,phl_pdo_bind *pB)
{
	while( pB ){
		phl_pdo_bind *pNext = pB->pNext;
		if( pB->zName ){
			SyMemBackendFree(&pVm->sAllocator,pB->zName);
		}
		if( pB->pVal ){
			ph7_release_value(pVm,pB->pVal);
		}
		SyMemBackendFree(&pVm->sAllocator,pB);
		pB = pNext;
	}
}
static void PdoBindsClear(phl_pdo_stmt *pSt)
{
	PdoBindListFree(pSt->pConn->pVm,pSt->pBinds);
	PdoBindListFree(pSt->pConn->pVm,pSt->pColBinds);
	pSt->pBinds = 0;
	pSt->pColBinds = 0;
}
PH7_PRIVATE void PH7_PdoFreeStmt(phl_pdo_stmt *pSt)
{
	PdoBindsClear(pSt);
	PdoStmtLazyClear(pSt);
	if( pSt->pLazyRow ){
		/* A lazy row RETAINS its statement object, so this cannot run while one
		 * is alive -- except at VM teardown, which releases in no order. Cut the
		 * link from both ends rather than leave the row reading freed memory. */
		PdoBlankSlot(pSt->pLazyRow);
		pSt->pLazyRow = 0;
	}
	PdoStmtClearFetchState(pSt);
	PH7_PdoSqliteFinalize(pSt);
	if( pSt->pConnObj ){
		/* drop the reference taken at creation; the connection may go now */
		ph7_class_instance *pObj = pSt->pConnObj;
		pSt->pConnObj = 0;
		PH7_ClassInstanceUnref(pObj);
	}
}
/* Finalize and free every statement of one connection. */
static void PdoStmtSweep(phl_pdo *pConn)
{
	phl_pdo_stmt *pSt = pConn->pStmts;
	while( pSt ){
		phl_pdo_stmt *pNext = pSt->pNext;
		PdoBlankSlot(pSt->pOwner);
		PH7_PdoFreeStmt(pSt);
		SyMemBackendFree(&pConn->pVm->sAllocator,pSt);
		pSt = pNext;
	}
	pConn->pStmts = 0;
}
static phl_pdo_stmt * PdoStmtOfInstance(ph7_class_instance *pThis)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 || !ph7_value_is_resource(pRes) ){
		return 0;
	}
	return (phl_pdo_stmt *)ph7_value_to_resource(pRes);
}
static int PdoStmtAttach(ph7_class_instance *pThis,phl_pdo_stmt *pSt)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return -1;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 ){
		return -1;
	}
	PH7_MemObjRelease(pRes);
	pRes->x.pOther = pSt;
	MemObjSetType(pRes,MEMOBJ_RES);
	pSt->pOwner = pThis;
	return 0;
}
/* The statement object is going away: release its cursor now, as php does. */
static void PdoStmtInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(pThis);
	SXUNUSED(pVm);
	if( pSt == 0 || pSt->pOwner != pThis ){
		return;
	}
	PH7_PdoSqliteFinalize(pSt);
	pSt->pOwner = 0;
}

/* ------------------------------------------------------------------------
 * The error surface
 * ------------------------------------------------------------------------ */
PH7_PRIVATE void PH7_PdoClearError(phl_pdo *pConn)
{
	pConn->iErrState = PDO_ERR_OK;
	pConn->iDrvCode = 0;
	pConn->bNoDrvDetail = 0;
	SyMemcpy("00000",pConn->zSqlState,sizeof("00000"));
	if( pConn->zDrvMsg ){
		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);
		pConn->zDrvMsg = 0;
	}
}
/*
 * php clears the handle's error at the ENTRY of most verbs -- exec, query,
 * prepare, quote, lastInsertId and both attribute accessors -- so a failure is
 * invisible to errorCode() as soon as any of them is called, even on a handle
 * that has never run anything (NULL becomes "00000"). The verbs that do NOT
 * clear are the two reporters themselves and the transaction quartet.
 */
PH7_PRIVATE void PH7_PdoTouch(phl_pdo *pConn)
{
	PH7_PdoClearError(pConn);
}
PH7_PRIVATE void PH7_PdoSetError(phl_pdo *pConn,const char *zSqlState,int iCode,const char *zMsg)
{
	sxu32 n;
	pConn->iErrState = PDO_ERR_FAILED;
	pConn->bNoDrvDetail = 0;
	pConn->iDrvCode = iCode;
	for( n = 0 ; n < 5 && zSqlState[n] ; ++n ){
		pConn->zSqlState[n] = zSqlState[n];
	}
	pConn->zSqlState[n] = 0;
	if( pConn->zDrvMsg ){
		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);
		pConn->zDrvMsg = 0;
	}
	if( zMsg ){
		n = SyStrlen(zMsg);
		pConn->zDrvMsg = (char *)SyMemBackendAlloc(&pConn->pVm->sAllocator,n + 1);
		if( pConn->zDrvMsg ){
			SyMemcpy(zMsg,pConn->zDrvMsg,n);
			pConn->zDrvMsg[n] = 0;
		}
	}
}
/*
 * Build a PDOException the way php does: the message php prints, the SQLSTATE
 * or driver code in $code, and the raw triple in $errorInfo. The generic
 * exception path cannot do this -- it takes an int code and knows no extra
 * property -- so the object is constructed here and thrown raw.
 */
static sxi32 PdoThrowException(ph7_context *pCtx,const char *zMsg,const char *zSqlState,
	int iCode,int bIntCode,const char *zDrvMsg,int nInfo)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	ph7_value sArg;
	ph7_value *apArg[1];
	SyString sMsgStr;
	sxi32 rc;

	pClass = PH7_VmExtractClass(&(*pVm),"PDOException",sizeof("PDOException")-1,TRUE,0);
	if( pClass == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOException is not available");
	}
	pThis = PH7_NewClassInstance(&(*pVm),pClass);
	if( pThis == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		SyStringInitFromBuf(&sMsgStr,zMsg,SyStrlen(zMsg));
		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	/* php's $code here is the SQLSTATE STRING for a statement failure and the
	 * driver's own INT for a connect failure -- which is why Exception::$code
	 * is redeclared untyped on this class. */
	if( bIntCode ){
		PH7_NativeSetAttrInt(&(*pVm),pThis,"code",(sxi64)iCode);
	}else{
		PH7_NativeSetAttrStr(&(*pVm),pThis,"code",zSqlState,SyStrlen(zSqlState));
	}
	/* A database failure reports all three cells; a layer refusal reports two,
	 * because there is no driver message under it. */
	if( nInfo > 0 ){
		ph7_value *pInfo = ph7_context_new_array(pCtx);
		ph7_value *pCell = ph7_context_new_scalar(pCtx);
		if( pInfo && pCell ){
			ph7_value_string(pCell,zSqlState,(int)SyStrlen(zSqlState));
			ph7_array_add_elem(pInfo,0,pCell);
			ph7_value_int64(pCell,(ph7_int64)iCode);
			ph7_array_add_elem(pInfo,0,pCell);
			if( nInfo > 2 ){
				/* the cell is an int here, so the string write resets it */
				if( zDrvMsg ){
					ph7_value_string(pCell,zDrvMsg,(int)SyStrlen(zDrvMsg));
				}else{
					ph7_value_null(pCell);
				}
				ph7_array_add_elem(pInfo,0,pCell);
			}
			PH7_NativeSetProp(&(*pVm),pThis,"errorInfo",sizeof("errorInfo")-1,pInfo);
		}
	}
	rc = VmThrowException(&(*pVm),pThis);
	PH7_ClassInstanceUnref(pThis);
	if( rc == SXERR_ABORT ){
		pCtx->nThrowRc = PH7_ABORT;
		return PH7_ABORT;
	}
	pCtx->nThrowRc = PH7_EXCEPTION;
	return PH7_EXCEPTION;
}
/*
 * A failed connect.  php never routes this one through the error mode: the
 * constructor throws whatever ATTR_ERRMODE the options carried, and the
 * message is `SQLSTATE[HY000] [14] unable to open database file` -- a shape no
 * other failure uses.
 */
PH7_PRIVATE sxi32 PH7_PdoThrowConstruct(ph7_context *pCtx,const char *zSqlState,int iCode,
	const char *zMsg)
{
	SyBlob sMsg;
	sxi32 rc;
	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
	SyBlobFormat(&sMsg,"SQLSTATE[%s] [%d] %s",zSqlState,iCode,zMsg ? zMsg : "");
	SyBlobAppend(&sMsg,"",1);
	rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),zSqlState,iCode,TRUE,zMsg,3);
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * php's SQLSTATE-to-description table, which is what a failure message says
 * BEFORE the driver's own code and text: a constraint violation reads
 * `SQLSTATE[23000]: Integrity constraint violation: 19 UNIQUE constraint
 * failed: u.v`, not "General error". Only the rows this driver can actually
 * reach are here; anything else falls back to HY000's sentence, which is what
 * php answers for an unlisted state too.
 *
 * HY000, 23000 and IM001 are probe-verified against the oracle. The remaining
 * three are php's own wording for states the sqlite driver maps but cannot
 * reach in practice -- SQLITE_TOOBIG needs a value past the 1 GB limit,
 * SQLITE_INTERRUPT an interrupt this engine never issues -- so no probe can
 * confirm them and none can contradict them either.
 */
static const char * PdoStateDescription(const char *zState)
{
	static const struct { const char *zState; const char *zText; } aState[] = {
		{ "HY000", "General error" },
		{ "23000", "Integrity constraint violation" },
		{ "IM001", "Driver does not support this function" },
		{ "42S02", "Base table or view not found" },
		{ "22001", "String data, right truncated" },
		{ "HYC00", "Optional feature not implemented" },
		{ "57014", "Statement canceled" },
	};
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aState) ; ++n ){
		if( SyStrncmp(zState,aState[n].zState,6) == 0 ){
			return aState[n].zText;
		}
	}
	return "General error";
}
/*
 * A failed OPERATION, routed through ATTR_ERRMODE: silent leaves the answer to
 * errorCode()/errorInfo(), warning adds php's E_WARNING naming the method, and
 * exception throws. The wording is one sentence in all three:
 * `SQLSTATE[HY000]: General error: 1 no such column: bogus`.
 */
PH7_PRIVATE sxi32 PH7_PdoRaise(ph7_context *pCtx,phl_pdo *pConn,const char *zFn)
{
	SyBlob sMsg;
	sxi32 rc = PH7_OK;
	if( pConn->iCallbackExc != 0 ){
		sxi32 rcExc = pConn->iCallbackExc;
		pConn->iCallbackExc = 0;
		return rcExc;
	}
	if( pConn->iErrMode == PDO_ERRMODE_SILENT ){
		return PH7_OK;
	}
	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %d %s",pConn->zSqlState,
		PdoStateDescription(pConn->zSqlState),
		pConn->iDrvCode,pConn->zDrvMsg ? pConn->zDrvMsg : "");
	SyBlobAppend(&sMsg,"",1);
	if( pConn->iErrMode == PDO_ERRMODE_WARNING ){
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));
	}else{
		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),pConn->zSqlState,
			pConn->iDrvCode,FALSE,pConn->zDrvMsg,3);
	}
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * The same two raisers, for a failure that belongs to a STATEMENT. They differ
 * from the connection's only in which object's SQLSTATE the message carries --
 * the driver detail under it is shared -- and in leaving the connection's own
 * state alone, which is what lets `$db->errorCode()` read "00000" while
 * `$stmt->errorCode()` reports the failure.
 */
PH7_PRIVATE sxi32 PH7_PdoRaiseStmt(ph7_context *pCtx,phl_pdo_stmt *pSt,const char *zFn)
{
	SyBlob sMsg;
	sxi32 rc = PH7_OK;
	phl_pdo *pConn = pSt->pConn;
	if( pConn->iCallbackExc != 0 ){
		/* the step did not fail: a userland callback THREW inside it, and what
		 * the script must see is that exception rather than a PDOException
		 * about the statement sqlite stopped */
		sxi32 rcExc = pConn->iCallbackExc;
		pConn->iCallbackExc = 0;
		return rcExc;
	}
	if( pConn->iErrMode == PDO_ERRMODE_SILENT ){
		return PH7_OK;
	}
	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %d %s",pSt->zSqlState,
		PdoStateDescription(pSt->zSqlState),pConn->iDrvCode,
		pConn->zDrvMsg ? pConn->zDrvMsg : "");
	SyBlobAppend(&sMsg,"",1);
	if( pConn->iErrMode == PDO_ERRMODE_WARNING ){
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));
	}else{
		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),pSt->zSqlState,
			pConn->iDrvCode,FALSE,pConn->zDrvMsg,3);
	}
	SyBlobRelease(&sMsg);
	return rc;
}
PH7_PRIVATE sxi32 PH7_PdoRaiseImplStmt(ph7_context *pCtx,phl_pdo_stmt *pSt,const char *zFn,
	const char *zSqlState,const char *zMsg)
{
	SyBlob sMsg;
	sxi32 rc = PH7_OK;
	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %s",zSqlState,
		PdoStateDescription(zSqlState),zMsg);
	SyBlobAppend(&sMsg,"",1);
	if( pSt->pConn->iErrMode == PDO_ERRMODE_EXCEPTION ){
		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),zSqlState,0,FALSE,0,2);
	}else{
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));
	}
	SyBlobRelease(&sMsg);
	return rc;
}
/*
 * php's pdo_raise_impl_error: a refusal by the LAYER rather than the database
 * -- asking a driver for an attribute it does not carry is the whole of it
 * here. It differs from the failure above twice over: the warning fires even
 * in SILENT mode, and the triple's third cell is absent, so errorInfo() is
 * two cells long.
 */
PH7_PRIVATE sxi32 PH7_PdoRaiseImpl(ph7_context *pCtx,phl_pdo *pConn,const char *zFn,
	const char *zSqlState,const char *zMsg)
{
	SyBlob sMsg;
	sxi32 rc = PH7_OK;
	if( pConn ){
		PH7_PdoSetError(pConn,zSqlState,0,0);
		pConn->bNoDrvDetail = 1;
	}
	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %s",zSqlState,
		PdoStateDescription(zSqlState),zMsg);
	SyBlobAppend(&sMsg,"",1);
	if( pConn && pConn->iErrMode == PDO_ERRMODE_EXCEPTION ){
		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),zSqlState,0,FALSE,0,2);
	}else{
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));
	}
	SyBlobRelease(&sMsg);
	return rc;
}

/* ------------------------------------------------------------------------
 * Attributes
 * ------------------------------------------------------------------------ */
/* php's PDO::ATTR_* numbering, and the two driver attributes this slice reads. */
#define PDO_ATTR_AUTOCOMMIT           0
#define PDO_ATTR_PREFETCH             1
#define PDO_ATTR_TIMEOUT              2
#define PDO_ATTR_ERRMODE              3
#define PDO_ATTR_SERVER_VERSION       4
#define PDO_ATTR_CLIENT_VERSION       5
#define PDO_ATTR_SERVER_INFO          6
#define PDO_ATTR_CONNECTION_STATUS    7
#define PDO_ATTR_CASE                 8
#define PDO_ATTR_CURSOR_NAME          9
#define PDO_ATTR_CURSOR              10
#define PDO_ATTR_ORACLE_NULLS        11
#define PDO_ATTR_PERSISTENT          12
#define PDO_ATTR_STATEMENT_CLASS     13
#define PDO_ATTR_FETCH_TABLE_NAMES   14
#define PDO_ATTR_FETCH_CATALOG_NAMES 15
#define PDO_ATTR_DRIVER_NAME         16
#define PDO_ATTR_STRINGIFY_FETCHES   17
#define PDO_ATTR_MAX_COLUMN_LEN      18
#define PDO_ATTR_DEFAULT_FETCH_MODE  19
#define PDO_ATTR_EMULATE_PREPARES    20
#define PDO_ATTR_DEFAULT_STR_PARAM   21
#define PDO_SQLITE_ATTR_OPEN_FLAGS            1000
#define PDO_SQLITE_ATTR_READONLY_STATEMENT    1001
#define PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES 1002
#define PDO_SQLITE_ATTR_BUSY_STATEMENT        1003
#define PDO_SQLITE_ATTR_TRANSACTION_MODE 1005

/* One int-keyed element of an array value, or 0 when the key is absent. */
static ph7_value * PdoArrayAtInt(ph7_vm *pVm,ph7_value *pArray,sxi64 iKey)
{
	ph7_hashmap_node *pNode = 0;
	ph7_value sKey;
	sxi32 rc;
	if( pArray == 0 || (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	PH7_MemObjInitFromInt(pVm,&sKey,iKey);
	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&sKey,&pNode);
	PH7_MemObjRelease(&sKey);
	if( rc != SXRET_OK || pNode == 0 ){
		return 0;
	}
	return (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
}
/*
 * ATTR_STATEMENT_CLASS's own validation, which is four refusals deep and in
 * php's own order: the value must be an array, it must carry a class name at
 * index 0, that name must BE a class and must derive from PDOStatement, and
 * anything at index 1 must be an array of constructor arguments. Keys past 1
 * are ignored, and index 1 refuses even a null -- despite the `?array` the
 * message spells, which is php's wording rather than its test. That message
 * also names the type of the WHOLE value rather than the element's, so a
 * string at index 1 reports "array given"; both are reproduced.
 */
static sxi32 PdoSetStatementClass(ph7_context *pCtx,phl_pdo *pConn,ph7_value *pVal)
{
	ph7_value *pName,*pArgs;
	ph7_class *pClass,*pBase;
	const char *zName;
	int nName;
	char zBuf[64];
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS value "
			"must be of type array, %s given",
			VmValueGivenName(pVal,zBuf,sizeof(zBuf)));
	}
	pName = PdoArrayAtInt(pCtx->pVm,pVal,0);
	if( pName == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS value "
			"must be an array with the format array(classname, constructor_args)");
	}
	zName = 0;
	nName = 0;
	if( pName->iFlags & MEMOBJ_STRING ){
		zName = ph7_value_to_string(pName,&nName);
	}
	pClass = (zName && nName > 0)
		? PH7_VmExtractClass(pCtx->pVm,zName,(sxu32)nName,FALSE,0) : 0;
	if( pClass == 0 ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS class "
			"must be a valid class");
	}
	pBase = PH7_VmExtractClass(pCtx->pVm,"PDOStatement",sizeof("PDOStatement")-1,FALSE,0);
	if( pBase == 0 || !PH7_VmInstanceOf(pClass,pBase) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS class "
			"must be derived from PDOStatement");
	}
	pArgs = PdoArrayAtInt(pCtx->pVm,pVal,1);
	if( pArgs != 0 && (pArgs->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS "
			"constructor_args must be of type ?array, array given");
	}
	/* remember it: every later query()/prepare() builds THIS class */
	if( pConn->zStmtClass ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,pConn->zStmtClass);
		pConn->zStmtClass = 0;
		pConn->nStmtClass = 0;
	}
	pConn->zStmtClass = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nName + 1);
	if( pConn->zStmtClass ){
		SyMemcpy(zName,pConn->zStmtClass,(sxu32)nName);
		pConn->zStmtClass[nName] = 0;
		pConn->nStmtClass = nName;
	}
	return PH7_OK;
}
/* php's "the driver has no such attribute" refusal, worded once. */
static sxi32 PdoNoSuchAttr(ph7_context *pCtx,phl_pdo *pConn,const char *zFn)
{
	return PH7_PdoRaiseImpl(pCtx,pConn,zFn,"IM001",
		"driver does not support that attribute");
}
/*
 * PDO::getAttribute(int $attribute): mixed
 *
 * Only the attributes the sqlite driver actually carries answer; every other
 * one -- including the generic PDO::ATTR_* names other drivers implement -- is
 * php's IM001. The two version attributes answer the LINKED library's version,
 * which is why no test may pin them.
 */
static int vm_builtin_PDO_getAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	ph7_int64 iAttr;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	PH7_PdoTouch(pConn);
	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	switch( iAttr ){
		case PDO_ATTR_ERRMODE:            ph7_result_int(pCtx,pConn->iErrMode); break;
		case PDO_ATTR_CASE:               ph7_result_int(pCtx,pConn->iCase); break;
		case PDO_ATTR_ORACLE_NULLS:       ph7_result_int(pCtx,pConn->iOracleNulls); break;
		case PDO_ATTR_DEFAULT_FETCH_MODE: ph7_result_int(pCtx,pConn->iDefaultFetch); break;
		case PDO_ATTR_STRINGIFY_FETCHES:  ph7_result_bool(pCtx,pConn->bStringify); break;
		case PDO_ATTR_PERSISTENT:         ph7_result_bool(pCtx,pConn->bPersistent); break;
		case PDO_SQLITE_ATTR_TRANSACTION_MODE: ph7_result_int(pCtx,pConn->iTxMode); break;
		/* ATTR_EXTENDED_RESULT_CODES is write-ONLY: php refuses to read it back
		 * like any attribute the driver does not carry. */
		case PDO_ATTR_DRIVER_NAME:
			ph7_result_string(pCtx,"sqlite",sizeof("sqlite")-1);
			break;
		case PDO_ATTR_SERVER_VERSION:
		case PDO_ATTR_CLIENT_VERSION: {
			const char *zVer = PH7_PdoSqliteLibVersion();
			ph7_result_string(pCtx,zVer,(int)SyStrlen(zVer));
			break;
		}
		case PDO_ATTR_STATEMENT_CLASS: {
			/* php answers the class name alone until a constructor-argument
			 * array is set beside it. */
			ph7_value *pArray = ph7_context_new_array(pCtx);
			ph7_value *pName = ph7_context_new_scalar(pCtx);
			if( pArray == 0 || pName == 0 ){
				return PH7_ContextMemoryError(pCtx);
			}
			if( pConn->zStmtClass ){
				ph7_value_string(pName,pConn->zStmtClass,pConn->nStmtClass);
			}else{
				ph7_value_string(pName,"PDOStatement",sizeof("PDOStatement")-1);
			}
			ph7_array_add_elem(pArray,0,pName);
			ph7_result_value(pCtx,pArray);
			break;
		}
		default:
			ph7_result_null(pCtx);
			return PdoNoSuchAttr(pCtx,pConn,"PDO::getAttribute");
	}
	return PH7_OK;
}
/*
 * PDO::setAttribute(int $attribute, mixed $value): bool
 *
 * Three outcomes, and which one an attribute takes is php's own table: the
 * five the driver carries return true, the four that describe the CONNECTION
 * rather than configure it (the versions, the driver name, persistence) answer
 * false without a diagnostic -- as does an attribute no driver defines -- and
 * the rest are the same IM001 refusal getAttribute raises.
 */
static int vm_builtin_PDO_setAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	ph7_int64 iAttr;
	ph7_value *pVal;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	PH7_PdoTouch(pConn);
	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	pVal = nArg > 1 ? apArg[1] : 0;
	switch( iAttr ){
		case PDO_ATTR_ERRMODE: {
			ph7_int64 iMode = pVal ? ph7_value_to_int64(pVal) : 0;
			if( iMode != PDO_ERRMODE_SILENT && iMode != PDO_ERRMODE_WARNING
			 && iMode != PDO_ERRMODE_EXCEPTION ){
				return PH7_VmThrowException(pCtx,"ValueError",
					"PDO::setAttribute(): Argument #2 ($value) Error mode must be one of "
					"the PDO::ERRMODE_* constants");
			}
			pConn->iErrMode = (int)iMode;
			break;
		}
		case PDO_ATTR_CASE: {
			ph7_int64 iCase = pVal ? ph7_value_to_int64(pVal) : 0;
			if( iCase != PDO_CASE_NATURAL && iCase != PDO_CASE_UPPER && iCase != PDO_CASE_LOWER ){
				return PH7_VmThrowException(pCtx,"ValueError",
					"PDO::setAttribute(): Argument #2 ($value) Case folding mode must be "
					"one of the PDO::CASE_* constants");
			}
			pConn->iCase = (int)iCase;
			break;
		}
		case PDO_ATTR_ORACLE_NULLS:
			pConn->iOracleNulls = (int)(pVal ? ph7_value_to_int64(pVal) : 0);
			break;
		case PDO_ATTR_DEFAULT_FETCH_MODE:
			pConn->iDefaultFetch = (int)(pVal ? ph7_value_to_int64(pVal) : 0);
			break;
		case PDO_ATTR_STRINGIFY_FETCHES:
			pConn->bStringify = pVal ? ph7_value_to_bool(pVal) : 0;
			break;
		case PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES:
			pConn->bExtendedCodes = pVal ? ph7_value_to_bool(pVal) : 0;
			PH7_PdoSqliteExtendedCodes(pConn,pConn->bExtendedCodes);
			break;
		case PDO_SQLITE_ATTR_TRANSACTION_MODE: {
			/* only php's three modes; anything else answers false in silence */
			ph7_int64 iTx = pVal ? ph7_value_to_int64(pVal) : 0;
			if( iTx < 0 || iTx > 2 ){
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			pConn->iTxMode = (int)iTx;
			break;
		}
		case PDO_ATTR_STATEMENT_CLASS: {
			/* Validated now; the class is USED when a statement is built. */
			sxi32 rcSet = PdoSetStatementClass(pCtx,pConn,pVal);
			if( rcSet != PH7_OK ){
				return rcSet;
			}
			break;
		}
		/* Read-only descriptions of the connection: php answers false and says
		 * nothing at all. An attribute no driver knows lands here too. */
		case PDO_ATTR_SERVER_VERSION:
		case PDO_ATTR_CLIENT_VERSION:
		case PDO_ATTR_DRIVER_NAME:
		case PDO_ATTR_PERSISTENT:
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		/* The generic attributes other drivers carry and this one does not. */
		case PDO_ATTR_AUTOCOMMIT:
		case PDO_ATTR_PREFETCH:
		case PDO_ATTR_TIMEOUT:
		case PDO_ATTR_SERVER_INFO:
		case PDO_ATTR_CONNECTION_STATUS:
		case PDO_ATTR_CURSOR_NAME:
		case PDO_ATTR_CURSOR:
		case PDO_ATTR_FETCH_TABLE_NAMES:
		case PDO_ATTR_FETCH_CATALOG_NAMES:
		case PDO_ATTR_MAX_COLUMN_LEN:
		case PDO_ATTR_EMULATE_PREPARES:
		case PDO_ATTR_DEFAULT_STR_PARAM:
			ph7_result_bool(pCtx,0);
			return PdoNoSuchAttr(pCtx,pConn,"PDO::setAttribute");
		default:
			ph7_result_bool(pCtx,0);
			return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Running statements, and reporting what happened
 * ------------------------------------------------------------------------ */
/*
 * PDO::exec(string $statement): int|false
 *
 * Runs every statement the string holds and answers the number of rows the
 * last one CHANGED. A statement that changes nothing -- a SELECT, a CREATE,
 * whitespace, a comment -- leaves sqlite's counter alone, so exec() answers
 * whatever the previous write did rather than 0; that is php's answer too,
 * because php reads the same counter.
 */
static int vm_builtin_PDO_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	const char *zSql;
	int nSql = 0;   /* the length is only written when the argument IS read */
	ph7_int64 nChange;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	PH7_PdoTouch(pConn);
	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;
	if( zSql == 0 || nSql < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"PDO::exec(): Argument #1 ($statement) must not be empty");
	}
	nChange = PH7_PdoSqliteExec(pConn,zSql,nSql);
	if( nChange < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_PdoRaise(pCtx,pConn,"PDO::exec");
	}
	ph7_result_int64(pCtx,nChange);
	return PH7_OK;
}
/*
 * PDO::errorCode(): ?string
 *
 * Three answers, not two: a handle nothing has run on yet answers NULL, one
 * whose last operation succeeded answers "00000", and a failed one answers the
 * SQLSTATE. errorInfo() splits the same three ways, and its FIRST cell is the
 * empty string -- not null -- in the never-used case.
 */
static int vm_builtin_PDO_errorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	if( pConn->iErrState == PDO_ERR_NONE ){
		ph7_result_null(pCtx);
	}else{
		ph7_result_string(pCtx,pConn->zSqlState,(int)SyStrlen(pConn->zSqlState));
	}
	return PH7_OK;
}
/*
 * The three cells errorInfo() answers, for a connection or a statement alike.
 * The first is the OBJECT's own SQLSTATE; the other two are the DRIVER's last
 * code and message, which are shared and are reported whenever the object's
 * own state is not a success -- so a statement that has never run shows the
 * previous statement's driver detail beside an empty state, exactly as php
 * does.
 */
static int PdoBuildErrorInfo(ph7_context *pCtx,phl_pdo *pConn,int iErrState,
	const char *zSqlState)
{
	ph7_value *pArray = ph7_context_new_array(pCtx);
	ph7_value *pCell = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pCell == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( iErrState == PDO_ERR_NONE ){
		ph7_value_string(pCell,"",0);
	}else{
		ph7_value_string(pCell,zSqlState,(int)SyStrlen(zSqlState));
	}
	ph7_array_add_elem(pArray,0,pCell);
	if( iErrState != PDO_ERR_OK && pConn->iDrvCode != 0 && !pConn->bNoDrvDetail ){
		ph7_value_int64(pCell,(ph7_int64)pConn->iDrvCode);
		ph7_array_add_elem(pArray,0,pCell);
		PH7_MemObjRelease(pCell);
		if( pConn->zDrvMsg ){
			ph7_value_string(pCell,pConn->zDrvMsg,(int)SyStrlen(pConn->zDrvMsg));
		}else{
			ph7_value_null(pCell);
		}
		ph7_array_add_elem(pArray,0,pCell);
	}else{
		ph7_value_null(pCell);
		ph7_array_add_elem(pArray,0,pCell);
		ph7_array_add_elem(pArray,0,pCell);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* A statement's own state, moved to "the last thing succeeded". */
static void PdoStmtOk(phl_pdo_stmt *pSt)
{
	pSt->iErrState = PDO_ERR_OK;
	SyMemcpy("00000",pSt->zSqlState,sizeof("00000"));
}
/* A statement's own state, moved to a failure. The driver detail (if any) is
 * already on the connection, where both objects read it from. */
static void PdoStmtFailed(phl_pdo_stmt *pSt,const char *zSqlState)
{
	sxu32 n;
	pSt->iErrState = PDO_ERR_FAILED;
	for( n = 0 ; n < 5 && zSqlState[n] ; ++n ){
		pSt->zSqlState[n] = zSqlState[n];
	}
	pSt->zSqlState[n] = 0;
}
static int vm_builtin_PDO_errorInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	return PdoBuildErrorInfo(pCtx,pConn,pConn->iErrState,pConn->zSqlState);
}
/*
 * PDO::lastInsertId(?string $name = null): string|false
 *
 * sqlite's rowid of the last insert, as a STRING -- php's portable answer,
 * since another driver's sequence may not fit an int. A handle that has
 * inserted nothing answers "0" rather than false, and the $name a sequence
 * driver would use is accepted and ignored here, as php accepts it.
 */
static int vm_builtin_PDO_lastInsertId(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	char zBuf[32];
	int nBuf;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	PH7_PdoTouch(pConn);
	nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%qd",PH7_PdoSqliteLastInsertId(pConn));
	ph7_result_string(pCtx,zBuf,nBuf);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Statements: running one, and reading its rows
 * ------------------------------------------------------------------------ */
/* php's PDO::FETCH_* values. */
#define PDO_FETCH_DEFAULT 0
#define PDO_FETCH_ASSOC   2
#define PDO_FETCH_NUM     3
#define PDO_FETCH_OBJ     5
#define PDO_FETCH_NAMED  11

/*
 * php's fetch FLAGS, which ride on top of a mode. These are the values the
 * SCRIPT sees (PDO::FETCH_GROUP is 32), not the shifted ones php uses inside
 * its own C -- the modes themselves occupy the low four bits.
 */
#define PDO_FETCH_MODE_MASK   0x0F
#define PDO_FETCH_GROUP       0x20
#define PDO_FETCH_UNIQUE      0x40
#define PDO_FETCH_CLASSTYPE   0x80
#define PDO_FETCH_PROPS_LATE  0x100
#define PDO_FETCH_SERIALIZE   0x200
#define PDO_FETCH_FLAGS       (~PDO_FETCH_MODE_MASK)
#define PDO_FETCH_BOUND        6
#define PDO_FETCH_COLUMN       7
#define PDO_FETCH_CLASS        8
#define PDO_FETCH_FUNC        10
#define PDO_FETCH_KEY_PAIR    12

/*
 * php's three CLASS-only flags refuse to ride on any other mode, and the
 * refusal names all three whichever one was set. Every entry point that takes
 * a mode checks this before it counts arguments.
 */
static sxi32 PdoCheckFetchFlags(ph7_context *pCtx,int iMode,const char *zFn,
	int iArgNo,const char *zParam)
{
	int iFlags = iMode & (PDO_FETCH_CLASSTYPE|PDO_FETCH_SERIALIZE|PDO_FETCH_PROPS_LATE);
	if( iFlags != 0 && (iMode & PDO_FETCH_MODE_MASK) != PDO_FETCH_CLASS ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($%s) cannot use PDO::FETCH_CLASSTYPE, "
			"PDO::FETCH_PROPS_LATE, or PDO::FETCH_SERIALIZE fetch flags with a fetch "
			"mode other than PDO::FETCH_CLASS",zFn,iArgNo,zParam);
	}
	return PH7_OK;
}
#define PDO_FETCH_LAZY         1
#define PDO_FETCH_INTO         9

/* Drop whatever a previous setFetchMode() attached to the statement. */
static void PdoStmtClearFetchState(phl_pdo_stmt *pSt)
{
	ph7_vm *pVm = pSt->pConn->pVm;
	if( pSt->zFetchClass ){
		SyMemBackendFree(&pVm->sAllocator,pSt->zFetchClass);
		pSt->zFetchClass = 0;
		pSt->nFetchClass = 0;
	}
	if( pSt->pFetchArgs ){
		ph7_release_value(pVm,pSt->pFetchArgs);
		pSt->pFetchArgs = 0;
	}
	if( pSt->pFetchInto ){
		ph7_class_instance *pObj = pSt->pFetchInto;
		pSt->pFetchInto = 0;
		PH7_ClassInstanceUnref(pObj);
	}
}

/* Call a constructor with the arguments FETCH_CLASS was given, if any. */
static void PdoCallCtor(ph7_vm *pVm,ph7_class_instance *pObj,ph7_class_method *pCons,
	ph7_value *pArgs)
{
	ph7_value *apArg[16];
	int nArg = 0;
	if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) ){
		ph7_hashmap *pMap = (ph7_hashmap *)pArgs->x.pOther;
		ph7_hashmap_node *pEntry = pMap->pFirst;
		sxu32 n,nCount = pMap->nEntry;
		for( n = 0 ; n < nCount && pEntry && nArg < (int)SX_ARRAYSIZE(apArg) ;
		     ++n, pEntry = pEntry->pPrev ){
			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);
			if( pVal ){
				apArg[nArg++] = pVal;
			}
		}
	}
	PH7_VmCallClassMethod(pVm,pObj,pCons,0,nArg,nArg ? apArg : 0);
}
/*
 * A column NAME as the connection presents it: ATTR_CASE folds it, and php
 * folds the name only -- never a value, and never a positional key.
 */
static void PdoColumnName(phl_pdo *pConn,const char *zName,SyBlob *pOut)
{
	sxu32 n;
	sxu32 nName = SyStrlen(zName);
	char *zBuf;
	SyBlobReset(pOut);
	SyBlobAppend(pOut,zName,nName);
	/* the array setter takes a C STRING and no length, so the terminator is
	 * part of the buffer and never part of the name */
	SyBlobAppend(pOut,"",1);
	if( pConn->iCase == PDO_CASE_NATURAL || nName < 1 ){
		return;
	}
	zBuf = (char *)SyBlobData(pOut);
	for( n = 0 ; n < nName ; ++n ){
		zBuf[n] = (char)(pConn->iCase == PDO_CASE_UPPER
			? SyToUpper(zBuf[n]) : SyToLower(zBuf[n]));
	}
}
/*
 * The two rewrites php applies to a fetched VALUE, in php's own order.
 *
 * ATTR_STRINGIFY_FETCHES turns everything the driver typed into a string --
 * everything except a null, which stays null. ATTR_ORACLE_NULLS is the empty
 * string and null trading places: NULL_EMPTY_STRING makes an empty string
 * null, NULL_TO_STRING makes a null the empty string. Neither touches a string
 * that merely LOOKS empty, so a single space survives both.
 */
static void PdoApplyValueMods(phl_pdo *pConn,ph7_value *pVal)
{
	if( pConn->bStringify && (pVal->iFlags & MEMOBJ_NULL) == 0 ){
		int nByte = 0;
		const char *zStr = ph7_value_to_string(pVal,&nByte);
		SyBlob sTmp;
		SyBlobInit(&sTmp,&pConn->pVm->sAllocator);
		SyBlobAppend(&sTmp,zStr,(sxu32)nByte);
		PH7_MemObjRelease(pVal);
		ph7_value_string(pVal,(const char *)SyBlobData(&sTmp),(int)SyBlobLength(&sTmp));
		SyBlobRelease(&sTmp);
	}
	if( pConn->iOracleNulls == PDO_NULL_EMPTY_STRING ){
		if( (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) == 0 ){
			PH7_MemObjRelease(pVal);
			ph7_value_null(pVal);
		}
	}else if( pConn->iOracleNulls == PDO_NULL_TO_STRING ){
		if( pVal->iFlags & MEMOBJ_NULL ){
			PH7_MemObjRelease(pVal);
			ph7_value_string(pVal,"",0);
		}
	}
}

/*
 * Drop the captured row a PDO::FETCH_LAZY object reads through. Every column
 * then answers null, which is what php's row does once the walk runs out.
 */
static void PdoStmtLazyClear(phl_pdo_stmt *pSt)
{
	ph7_vm *pVm = pSt->pConn->pVm;
	if( pSt->pLazyVals ){
		ph7_release_value(pVm,pSt->pLazyVals);
		pSt->pLazyVals = 0;
	}
}
/*
 * Capture the row under the cursor for the lazy object: the RAW column values,
 * positionally. The value modifiers are NOT applied -- php's row reads them at
 * property-access time, so a STRINGIFY_FETCHES turned on between two reads
 * shows in the second -- and the NAMES are not captured at all, because they
 * are the statement's and outlive any one row.
 */
static void PdoStmtLazyCapture(phl_pdo_stmt *pSt)
{
	ph7_vm *pVm = pSt->pConn->pVm;
	int nCol,iCol;
	ph7_value *pCell;
	PdoStmtLazyClear(pSt);
	pSt->pLazyVals = ph7_new_array(pVm);
	pCell = ph7_new_scalar(pVm);
	if( pSt->pLazyVals == 0 || pCell == 0 ){
		if( pCell ){ ph7_release_value(pVm,pCell); }
		PdoStmtLazyClear(pSt);
		return;
	}
	nCol = PH7_PdoSqliteColumnCount(pSt);
	for( iCol = 0 ; iCol < nCol ; ++iCol ){
		PH7_PdoSqliteColumnValue(pSt,iCol,pCell);
		ph7_array_add_elem(pSt->pLazyVals,0,pCell);
	}
	ph7_release_value(pVm,pCell);
}
/*
 * Keep the lazy object in step with the cursor: the row about to be handed out
 * becomes what it reads, and a cursor with nothing left clears it. Called from
 * every verb that consumes a row, and only while such an object exists.
 */
static void PdoStmtLazySync(phl_pdo_stmt *pSt)
{
	if( pSt->pLazyRow == 0 ){
		return;
	}
	if( pSt->bRowPending ){
		PdoStmtLazyCapture(pSt);
	}else{
		PdoStmtLazyClear(pSt);
	}
}
/*
 * Does the cursor have a row for the verb about to ask? Answering that is also
 * the moment a lazy object has to be brought in step, because a verb that
 * finds NOTHING is what empties php's row -- every column of it reads null
 * from there on, names and all.
 */
static int PdoStmtHasRow(phl_pdo_stmt *pSt)
{
	PdoStmtLazySync(pSt);
	return pSt->bRowPending;
}
/*
 * Step the cursor once and remember what happened. php's driver does this at
 * execute() so columnCount() has an answer before anything is fetched, and the
 * row it lands on is the one the FIRST fetch() hands back.
 *
 * No diagnostic is raised here: the iterator walks through this too, and its
 * vtable is handed a VM with no call context to raise INTO. The failure is
 * recorded on the connection either way, and the callers that DO have a
 * context route it.
 */
static int PdoStmtStep(phl_pdo_stmt *pSt)
{
	int rc = PH7_PdoSqliteStep(pSt);
	if( rc < 0 ){
		pSt->bDone = 1;
		pSt->bRowPending = 0;
		return -1;
	}
	pSt->bRowPending = (rc == 1);
	pSt->bDone = (rc == 0);
	return rc;
}
/*
 * Build one row in the requested shape.  Answers 0 when the cursor has nothing
 * to hand back, which is what makes fetch() answer false at the end.
 */
static int PdoStmtRowFrom(ph7_vm *pVm,phl_pdo_stmt *pSt,int iMode,ph7_value *pOut,
	int iFirstCol)
{
	int nCol,iCol;
	ph7_value *pCell;
	SyBlob sName;
	/* Whatever shape this row is asked for, it is also the row a lazy object
	 * handed out earlier now reads -- php's is a view of the same cursor. */
	PdoStmtLazySync(pSt);
	if( !pSt->bRowPending ){
		return 0;
	}
	PdoBoundColumnsForRow(pVm,pSt);
	nCol = PH7_PdoSqliteColumnCount(pSt);
	pCell = ph7_new_scalar(pVm);
	if( pCell == 0 ){
		return 0;
	}
	SyBlobInit(&sName,&pVm->sAllocator);
	for( iCol = iFirstCol ; iCol < nCol ; ++iCol ){
		PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sName);
		PH7_PdoSqliteColumnValue(pSt,iCol,pCell);
		PdoApplyValueMods(pSt->pConn,pCell);
		/* FETCH_BOTH is not a third shape: it is both of the other two, so
		 * every column lands twice -- named, then positional. FETCH_OBJ builds
		 * the named shape and is converted below. */
		if( iMode != PDO_FETCH_NUM ){
			const char *zKey = (const char *)SyBlobData(&sName);
			int nKey = (int)SyBlobLength(&sName) - 1;   /* less the terminator */
			if( iMode == PDO_FETCH_NAMED ){
				/* php's answer to two columns of one name: the first stays a
				 * scalar, and a second occurrence turns the entry into a LIST
				 * of every value under that name. Every other named mode keeps
				 * the last one and drops the rest. */
				ph7_value *pPrev = ph7_array_fetch(pOut,zKey,nKey);
				if( pPrev == 0 ){
					ph7_array_add_strkey_elem(pOut,zKey,pCell);
				}else if( pPrev->iFlags & MEMOBJ_HASHMAP ){
					ph7_array_add_elem(pPrev,0,pCell);
				}else{
					ph7_value *pList = ph7_new_array(pVm);
					if( pList ){
						ph7_array_add_elem(pList,0,pPrev);
						ph7_array_add_elem(pList,0,pCell);
						ph7_array_add_strkey_elem(pOut,zKey,pList);
						ph7_release_value(pVm,pList);
					}
				}
			}else{
				ph7_array_add_strkey_elem(pOut,zKey,pCell);
			}
		}
		if( iMode != PDO_FETCH_ASSOC && iMode != PDO_FETCH_OBJ
		 && iMode != PDO_FETCH_NAMED ){
			if( iMode == PDO_FETCH_NUM ){
				/* a NUM row is renumbered from 0 when a leading column was
				 * dropped; a BOTH row keeps the column's original position,
				 * which is php's own asymmetry under FETCH_GROUP */
				ph7_array_add_elem(pOut,0,pCell);
			}else{
				ph7_value sIdx;
				PH7_MemObjInitFromInt(pVm,&sIdx,(sxi64)iCol);
				ph7_array_add_elem(pOut,&sIdx,pCell);
				PH7_MemObjRelease(&sIdx);
			}
		}
	}
	SyBlobRelease(&sName);
	ph7_release_value(pVm,pCell);
	if( iMode == PDO_FETCH_OBJ ){
		/* php's stdClass row is the associative one cast to an object -- one
		 * DYNAMIC property per column, which is what `(object)` builds and what
		 * json_decode() answers for the same reason. */
		PH7_MemObjToObject(pOut);
	}
	/* the row is spent: the next step looks for another */
	pSt->bRowPending = 0;
	return 1;
}
static int PdoStmtRow(ph7_vm *pVm,phl_pdo_stmt *pSt,int iMode,ph7_value *pOut)
{
	return PdoStmtRowFrom(pVm,pSt,iMode,pOut,0);
}
/*
 * Write one column onto an object.  A DECLARED property takes the native
 * setter, whatever its visibility -- php fills a private or protected one
 * named like a column just the same. A class that declares nothing (stdClass,
 * which is what FETCH_CLASS falls back to) gets a dynamic property instead,
 * the same one an `(object)` cast would create.
 */
static void PdoWriteOneProp(ph7_vm *pVm,ph7_class_instance *pObj,const char *zKey,
	int nKey,ph7_value *pVal)
{
	if( PH7_NativeAttr(pObj,zKey) != 0 ){
		PH7_NativeSetProp(pVm,pObj,zKey,(sxu32)nKey,pVal);
		return;
	}
	{
		SyString sName;
		ph7_value *pSlot;
		SyStringInitFromBuf(&sName,zKey,nKey);
		pSlot = PH7_ClassInstanceFetchAttr(pObj,&sName);
		if( pSlot ){
			PH7_MemObjStore(pVal,pSlot);
			return;
		}
		pSlot = PH7_VmCreateDynamicAttr(pVm,pObj,zKey,(sxu32)nKey,0);
		if( pSlot ){
			PH7_MemObjStore(pVal,pSlot);
		}
	}
}
/* Write every column of a row onto an object, visibility ignored -- php fills
 * a private or protected property named like a column just the same. */
static void PdoWriteRowProps(ph7_vm *pVm,ph7_class_instance *pObj,ph7_value *pRow)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	sxu32 n,nCount;
	if( pRow == 0 || (pRow->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return;
	}
	pMap = (ph7_hashmap *)pRow->x.pOther;
	pEntry = pMap->pFirst;
	nCount = pMap->nEntry;
	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){
		ph7_value sKey;
		ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);
		int nKey = 0;
		const char *zKey;
		PH7_MemObjInit(pVm,&sKey);
		PH7_HashmapExtractNodeKey(pEntry,&sKey);
		zKey = ph7_value_to_string(&sKey,&nKey);
		if( pVal && zKey && nKey > 0 ){
			PdoWriteOneProp(pVm,pObj,zKey,nKey,pVal);
		}
		PH7_MemObjRelease(&sKey);
	}
}
/*
 * Build one object for FETCH_CLASS / fetchObject().  php writes the columns as
 * properties and runs the constructor AFTER them, so a constructor that
 * assigns a property wins over the column of the same name -- unless
 * FETCH_PROPS_LATE reverses the order, which is the whole point of that flag.
 * The write ignores visibility: a private or protected property named like a
 * column is filled just the same, which is why this cannot go through the
 * ordinary property-store path.
 */
static int PdoRowIntoObject(ph7_vm *pVm,phl_pdo_stmt *pSt,ph7_class *pClass,
	ph7_value *pArgs,int bPropsLate,int iFirstCol,ph7_value *pResult)
{
	ph7_class_instance *pObj;
	ph7_class_method *pCons;
	ph7_value *pRow;
	int rc = 0;
	pObj = PH7_NewClassInstance(pVm,pClass);
	if( pObj == 0 ){
		return 0;
	}
	/* VM-allocated rather than context-allocated: the foreach ITERATOR builds
	 * objects through here too, and its vtable has no call context. */
	pRow = ph7_new_array(pVm);
	if( pRow == 0 ){
		PH7_ClassInstanceUnref(pObj);
		return 0;
	}
	if( !PdoStmtRowFrom(pVm,pSt,PDO_FETCH_ASSOC,pRow,iFirstCol) ){
		ph7_release_value(pVm,pRow);
		PH7_ClassInstanceUnref(pObj);
		return 0;
	}
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( bPropsLate && pCons ){
		PdoCallCtor(pVm,pObj,pCons,pArgs);
	}
	{
		ph7_hashmap *pMap = (ph7_hashmap *)pRow->x.pOther;
		ph7_hashmap_node *pEntry = pMap->pFirst;
		sxu32 n,nCount = pMap->nEntry;
		for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){
			ph7_value sKey;
			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);
			int nKey = 0;
			const char *zKey;
			PH7_MemObjInit(pVm,&sKey);
			PH7_HashmapExtractNodeKey(pEntry,&sKey);
			zKey = ph7_value_to_string(&sKey,&nKey);
			if( pVal && zKey && nKey > 0 ){
				PdoWriteOneProp(pVm,pObj,zKey,nKey,pVal);
			}
			PH7_MemObjRelease(&sKey);
		}
	}
	if( !bPropsLate && pCons ){
		PdoCallCtor(pVm,pObj,pCons,pArgs);
	}
	ph7_release_value(pVm,pRow);
	PH7_MemObjRelease(pResult);
	pResult->x.pOther = pObj;
	pResult->iFlags = MEMOBJ_OBJ;
	rc = 1;
	return rc;
}
/*
 * The class a FETCH_CLASS or fetchObject() names.  The two verbs word the same
 * refusal differently -- fetchAll() names the ARGUMENT POSITION, fetchObject()
 * names the class it was given -- so the caller supplies the sentence.
 */
static ph7_class * PdoResolveFetchClass(ph7_context *pCtx,ph7_value *pName,int bObjectVerb,
	sxi32 *pRc)
{
	ph7_class *pClass;
	const char *zName;
	int nName = 0;
	*pRc = PH7_OK;
	if( pName == 0 || (pName->iFlags & MEMOBJ_NULL) ){
		return PH7_VmExtractClass(pCtx->pVm,"stdClass",sizeof("stdClass")-1,FALSE,0);
	}
	zName = ph7_value_to_string(pName,&nName);
	/* EXISTENCE, not instantiability: php takes the name of an abstract class
	 * or an interface here and refuses it where the object would be BUILT,
	 * with the ordinary `Cannot instantiate ...` Error. */
	pClass = (zName && nName > 0)
		? PH7_VmExtractClass(pCtx->pVm,zName,(sxu32)nName,FALSE,0) : 0;
	if( pClass == 0 ){
		if( bObjectVerb ){
			*pRc = PH7_VmThrowException(pCtx,"TypeError",
				"PDOStatement::fetchObject(): Argument #1 ($class) must be a valid "
				"class name, %.*s given",nName,zName ? zName : "");
		}else{
			*pRc = PH7_VmThrowException(pCtx,"TypeError",
				"PDOStatement::fetchAll(): Argument #2 must be a valid class");
		}
	}
	return pClass;
}
/*
 * The class a FETCH_CLASSTYPE row names in its first column. php takes what it
 * finds there and falls back to stdClass for anything it cannot use -- a name
 * no class carries, a null, a number -- rather than refusing the row.
 */
static ph7_class * PdoIterClassOf(ph7_vm *pVm,ph7_value *pName)
{
	ph7_class *pClass = 0;
	if( pName && (pName->iFlags & MEMOBJ_NULL) == 0 ){
		int nName = 0;
		const char *zName = ph7_value_to_string(pName,&nName);
		if( zName && nName > 0 ){
			pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);
		}
	}
	return pClass ? pClass
		: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,FALSE,0);
}
static ph7_class * PdoClassTypeClass(ph7_context *pCtx,ph7_value *pName)
{
	return PdoIterClassOf(pCtx->pVm,pName);
}
/*
 * PDOStatement::fetchObject(?string $class = "stdClass", array $ctorArgs = []): object|false
 *
 * FETCH_CLASS for exactly one row, with its own refusal wording.
 */
static int vm_builtin_PDOStatement_fetchObject(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	ph7_class *pClass;
	ph7_value sRes;
	sxi32 rc;
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	pClass = PdoResolveFetchClass(pCtx,nArg > 0 ? apArg[0] : 0,TRUE,&rc);
	if( pClass == 0 ){
		return rc;
	}
	if( !PdoStmtHasRow(pSt) ){
		PdoStmtOk(pSt);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( PdoBoundColumnsBad(pSt) ){
		return PdoBoundColumnsRefuse(pCtx,pSt,0);
	}
	PH7_MemObjInit(pCtx->pVm,&sRes);
	if( !PdoRowIntoObject(pCtx->pVm,pSt,pClass,nArg > 1 ? apArg[1] : 0,FALSE,0,&sRes) ){
		PH7_MemObjRelease(&sRes);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_value(pCtx,&sRes);
	PH7_MemObjRelease(&sRes);
	if( PdoStmtStep(pSt) < 0 ){
		PdoStmtFailed(pSt,pSt->pConn->zSqlState);
		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchObject");
	}
	PdoStmtOk(pSt);
	return PH7_OK;
}
/*
 * One row as an OBJECT: FETCH_CLASS builds a new instance, FETCH_INTO fills
 * the one the script handed setFetchMode(). A fetch(FETCH_INTO) with no such
 * object is php's own "No fetch-into object specified." -- a PDOException with
 * no driver behind it.
 */
static int PdoFetchObjectRow(ph7_context *pCtx,phl_pdo_stmt *pSt,int iMode,
	ph7_value *pClassName,ph7_value *pArgs,const char *zFn)
{
	int iBase = iMode & PDO_FETCH_MODE_MASK;
	int bLate = (iMode & PDO_FETCH_PROPS_LATE) != 0;
	int iFirst = 0;
	ph7_class *pClass = 0;
	ph7_value sRes;
	sxi32 rc;
	SXUNUSED(zFn);
	if( !PdoStmtHasRow(pSt) ){
		PdoStmtOk(pSt);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( PdoBoundColumnsBad(pSt) ){
		return PdoBoundColumnsRefuse(pCtx,pSt,0);
	}
	if( iBase == PDO_FETCH_INTO ){
		ph7_value *pRow;
		if( pSt->pFetchInto == 0 ){
			return PH7_VmThrowException(pCtx,"PDOException",
				"SQLSTATE[HY000]: General error: No fetch-into object specified.");
		}
		pRow = ph7_context_new_array(pCtx);
		if( pRow == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_ASSOC,pRow) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		PdoWriteRowProps(pCtx->pVm,pSt->pFetchInto,pRow);
		PH7_NativeResultObject(pCtx,pSt->pFetchInto);
		pSt->pFetchInto->iRef++;   /* the result took one; the statement keeps its own */
		if( PdoStmtStep(pSt) < 0 ){
			PdoStmtFailed(pSt,pSt->pConn->zSqlState);
			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");
		}
		PdoStmtOk(pSt);
		return PH7_OK;
	}
	/* FETCH_CLASS: the class comes from this call or from setFetchMode() */
	if( pClassName ){
		pClass = PdoResolveFetchClass(pCtx,pClassName,FALSE,&rc);
		if( pClass == 0 ){
			return rc;
		}
	}else if( iMode & PDO_FETCH_CLASSTYPE ){
		/* the FIRST column names the class, and leaves the row */
		ph7_value *pHead = ph7_context_new_array(pCtx);
		ph7_value *pName;
		if( pHead == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pHead,0) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pSt->bRowPending = 1;   /* the cursor has not moved */
		pName = PdoArrayAtInt(pCtx->pVm,pHead,0);
		pClass = PdoClassTypeClass(pCtx,pName);
		iFirst = 1;
	}else if( pSt->zFetchClass ){
		ph7_value sName;
		SyString sStr;
		SyStringInitFromBuf(&sStr,pSt->zFetchClass,pSt->nFetchClass);
		PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);
		pClass = PdoResolveFetchClass(pCtx,&sName,FALSE,&rc);
		PH7_MemObjRelease(&sName);
		if( pClass == 0 ){
			return rc;
		}
		if( pArgs == 0 ){
			pArgs = pSt->pFetchArgs;
		}
	}else{
		/* FETCH_CLASS with no class anywhere -- neither this call's nor a
		 * setFetchMode()'s -- is php's own layer refusal, not a stdClass row. */
		return PH7_VmThrowException(pCtx,"PDOException",
			"SQLSTATE[HY000]: General error: No fetch class specified");
	}
	if( pClass == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	rc = PH7_VmCheckInstantiable(pCtx,pClass);
	if( rc != PH7_OK ){
		return rc;   /* an interface, a trait, an enum or an abstract class */
	}
	PH7_MemObjInit(pCtx->pVm,&sRes);
	if( !PdoRowIntoObject(pCtx->pVm,pSt,pClass,pArgs,bLate,iFirst,&sRes) ){
		PH7_MemObjRelease(&sRes);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_value(pCtx,&sRes);
	PH7_MemObjRelease(&sRes);
	if( PdoStmtStep(pSt) < 0 ){
		PdoStmtFailed(pSt,pSt->pConn->zSqlState);
		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");
	}
	PdoStmtOk(pSt);
	return PH7_OK;
}
/*
 * PDOStatement::bindColumn(string|int $column, mixed &$var, int $type = PDO::PARAM_STR, ...): bool
 *
 * Attach a variable to a column, to be written on every FETCH_BOUND fetch.
 * The column is 1-based like a parameter, and a NAME is resolved NOW against
 * the statement's columns -- a name that is not there warns immediately (in
 * every error mode, the way a layer refusal does) and is simply not bound,
 * while an out-of-range INDEX is accepted here and refused by the fetch.
 */
static int vm_builtin_PDOStatement_bindColumn(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	phl_pdo_bind *pB;
	ph7_value *pKey;
	int iPos = 0,iType;
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	pKey = nArg > 0 ? apArg[0] : 0;
	iType = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : PDO_PARAM_STR;
	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){
		int nName = 0,iCol,nCol;
		const char *zName = ph7_value_to_string(pKey,&nName);
		SyBlob sName;
		iPos = 0;
		nCol = PH7_PdoSqliteColumnCount(pSt);
		SyBlobInit(&sName,&pCtx->pVm->sAllocator);
		for( iCol = 0 ; iCol < nCol ; ++iCol ){
			PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sName);
			if( (int)SyBlobLength(&sName) - 1 == nName
			 && SyMemcmp(SyBlobData(&sName),zName,(sxu32)nName) == 0 ){
				iPos = iCol + 1;
				break;
			}
		}
		SyBlobRelease(&sName);
		if( iPos == 0 ){
			/* php routes this one through the error mode like any layer refusal:
			 * a warning in silent and warning modes, a throw in exception mode --
			 * and the column is simply not bound either way. */
			SyBlob sMsg;
			sxi32 rcWarn;
			ph7_result_bool(pCtx,1);
			SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
			SyBlobFormat(&sMsg,"Did not find column name '%.*s' in the defined "
				"columns; it will not be bound",nName,zName ? zName : "");
			SyBlobAppend(&sMsg,"",1);
			rcWarn = PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::bindColumn","HY000",
				(const char *)SyBlobData(&sMsg));
			SyBlobRelease(&sMsg);
			return rcWarn;
		}
	}else{
		ph7_int64 iWant = pKey ? ph7_value_to_int64(pKey) : 0;
		if( iWant < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"PDOStatement::bindColumn(): Argument #1 ($column) must be greater "
				"than or equal to 1");
		}
		iPos = (int)iWant;
	}
	/* php REPLACES a binding rather than stacking one, and which it considers
	 * the same is not symmetric: a binding made by NAME takes over whatever
	 * already stands for that COLUMN, while one made by NUMBER only replaces
	 * another made by number. So `bindColumn(3,$x)` then `bindColumn('c2',$y)`
	 * is one binding and $x is never written again, while the same pair the
	 * other way round is two and both are. */
	{
		int nName = 0;
		const char *zName = (pKey && (pKey->iFlags & MEMOBJ_STRING))
			? ph7_value_to_string(pKey,&nName) : 0;
		for( pB = pSt->pColBinds ; pB ; pB = pB->pNext ){
			if( zName ? (pB->iPos == iPos
			             || (pB->zName && pB->nName == nName
			                 && SyMemcmp(pB->zName,zName,(sxu32)nName) == 0))
			          : (pB->zName == 0 && pB->iPos == iPos) ){
				break;
			}
		}
		if( pB == 0 ){
			pB = (phl_pdo_bind *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_pdo_bind));
			if( pB == 0 ){
				return PH7_ContextMemoryError(pCtx);
			}
			SyZero(pB,sizeof(phl_pdo_bind));
			pB->pNext = pSt->pColBinds;
			pSt->pColBinds = pB;
		}
		/* The entry remembers HOW it was named, replaced entries included: that
		 * is what a later binding by NUMBER matches against (it takes over a
		 * nameless entry and leaves a named one standing). */
		if( pB->zName ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,pB->zName);
			pB->zName = 0;
			pB->nName = 0;
		}
		if( zName && nName > 0 ){
			pB->zName = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nName + 1);
			if( pB->zName == 0 ){
				return PH7_ContextMemoryError(pCtx);
			}
			SyMemcpy(zName,pB->zName,(sxu32)nName);
			pB->zName[nName] = 0;
			pB->nName = nName;
		}
	}
	pB->iPos = iPos;
	pB->iType = iType;
	pB->nSlot = (nArg > 1 && apArg[1]) ? apArg[1]->nIdx : SXU32_HIGH;
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Write every bound column of the row at the cursor into the variables
 * bindColumn() named.  The value takes the bound TYPE, so an unqualified
 * binding hands back a string where the row itself would have held an int.
 */
static int PdoWriteBoundColumns(ph7_vm *pVm,phl_pdo_stmt *pSt)
{
	phl_pdo_bind *pB;
	int nCol = PH7_PdoSqliteColumnCount(pSt);
	for( pB = pSt->pColBinds ; pB ; pB = pB->pNext ){
		ph7_value *pSlot;
		ph7_value sVal;
		if( pB->nSlot == SXU32_HIGH ){
			continue;
		}
		pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pB->nSlot);
		if( pSlot == 0 ){
			continue;
		}
		if( pB->iPos < 1 || pB->iPos > nCol ){
			/* A column the statement does not have writes a NULL into its
			 * variable and only THEN refuses (the refusal is the caller's). */
			PH7_MemObjInit(pVm,&sVal);
			PH7_MemObjStore(&sVal,pSlot);
			PH7_MemObjRelease(&sVal);
			continue;
		}
		PH7_MemObjInit(pVm,&sVal);
		PH7_PdoSqliteColumnValue(pSt,pB->iPos - 1,&sVal);
		PdoApplyValueMods(pSt->pConn,&sVal);
		/* php switches on the type it was GIVEN, flags and all: PARAM_NULL
		 * writes a null whatever the column holds, INT/STR/BOOL convert, and
		 * everything else -- PARAM_LOB, PARAM_STMT, a number no constant names,
		 * or any of these with PARAM_INPUT_OUTPUT ored on -- writes the driver's
		 * own value untouched. A column holding NULL stays null throughout. */
		if( pB->iType == PDO_PARAM_NULL ){
			PH7_MemObjRelease(&sVal);
			PH7_MemObjInit(pVm,&sVal);
		}else if( (sVal.iFlags & MEMOBJ_NULL) == 0 ){
			switch( pB->iType ){
				case PDO_PARAM_INT:  PH7_MemObjToInteger(&sVal); break;
				case PDO_PARAM_BOOL: PH7_MemObjToBool(&sVal); break;
				case PDO_PARAM_STR:  PH7_MemObjToString(&sVal); break;
				default:             break;   /* the value as the driver typed it */
			}
		}
		PH7_MemObjStore(&sVal,pSlot);
		PH7_MemObjRelease(&sVal);
	}
	return 1;
}
/*
 * Does every binding name a column this statement HAS? The width is the
 * statement's and does not change while it is walked, so this is asked ONCE by
 * the verb rather than per row -- which is also what keeps php's
 * `Invalid column index` out of the middle of fetchAll()'s loop, where a raise
 * has no frame to leave.
 */
static int PdoBoundColumnsInRange(phl_pdo_stmt *pSt)
{
	phl_pdo_bind *pB;
	int nCol = PH7_PdoSqliteColumnCount(pSt);
	for( pB = pSt->pColBinds ; pB ; pB = pB->pNext ){
		if( pB->iPos < 1 || pB->iPos > nCol ){
			return 0;
		}
	}
	return 1;
}
/*
 * The bound columns of the row a verb is about to hand out. php writes them on
 * EVERY fetch, whatever the mode -- FETCH_BOUND is only the mode that answers
 * `true` INSTEAD of a row, not the one that does the writing -- so this runs
 * wherever a row is read. A binding OUT of range is not its business: the verb
 * screens for one first and refuses through PdoBoundColumnsRefuse.
 */
static void PdoBoundColumnsForRow(ph7_vm *pVm,phl_pdo_stmt *pSt)
{
	if( pSt->pColBinds == 0 || !pSt->bRowPending || !PdoBoundColumnsInRange(pSt) ){
		/* php looks at the bindings only when there is a ROW to write from, so a
		 * cursor with nothing left answers false rather than refusing; a binding
		 * OUT of range was screened by the verb before the row was read. */
		return;
	}
	PdoWriteBoundColumns(pVm,pSt);
}
/* Has the verb about to read a row a binding it cannot honour? */
static int PdoBoundColumnsBad(phl_pdo_stmt *pSt)
{
	return pSt->pColBinds != 0 && pSt->bRowPending && !PdoBoundColumnsInRange(pSt);
}
/*
 * php's refusal for a binding naming a column the statement does not have. It
 * reads the row and moves the cursor ON before the refusal surfaces, so three
 * fetches over three rows refuse one by one and the fourth answers false --
 * and fetchAll(), which would have walked the whole set, exhausts it and
 * refuses once.
 */
static sxi32 PdoBoundColumnsRefuse(ph7_context *pCtx,phl_pdo_stmt *pSt,int bWholeSet)
{
	/* The bindings it CAN honour are written all the same -- the one it cannot
	 * reach does not stop the rest -- and for the whole-set verb they are
	 * written from EVERY row it walks past, so the caller is left holding the
	 * last row's values exactly as a successful fetchAll() would leave them. */
	do{
		PdoWriteBoundColumns(pCtx->pVm,pSt);
		pSt->bRowPending = 0;
		PdoStmtStep(pSt);
	}while( bWholeSet && pSt->bRowPending );
	PdoStmtOk(pSt);
	return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");
}
/*
 * The same, for the foreach ITERATOR: its vtable is handed a VM with no call
 * context, so the refusal is raised on the VM directly -- the walk is inside
 * the foreach opcode, which is the frame php raises it out of too.
 */
static int PdoBoundColumnsForIterRow(ph7_vm *pVm,phl_pdo_stmt *pSt)
{
	if( pSt->pColBinds == 0 || !pSt->bRowPending || PdoBoundColumnsInRange(pSt) ){
		return 1;   /* the row read below writes them, through the same routine */
	}
	/* Same as the verbs' refusal -- the honourable bindings are written and the
	 * row is consumed before it surfaces -- but raised on the VM directly: the
	 * walk is inside the foreach opcode, which is the frame php raises out of,
	 * and the iterator's vtable has no call context to throw into. */
	PdoWriteBoundColumns(pVm,pSt);
	pSt->bRowPending = 0;
	PdoStmtStep(pSt);
	VmThrowFromVm(pVm,"ValueError","Invalid column index",
		sizeof("Invalid column index")-1);
	return 0;
}
/* ------------------------------------------------------------------------
 * PDORow: what PDO::FETCH_LAZY answers
 * ------------------------------------------------------------------------ */
/*
 * php's PDORow is a fully VIRTUAL object over a statement's CURRENT row: it
 * declares one property (`queryString`) and holds NONE, every column is read
 * through its property and dimension handlers, and every write is refused.
 * That split is what makes `get_object_vars()` empty beside a `$row->id` that
 * works, `var_dump()` show the columns anyway (its get_debug_info handler) and
 * `(array)`/`var_export()`/`json_encode()` show nothing at all.
 *
 * One object per statement, handed back by every lazy fetch, so two fetches
 * answer the same object and the FIRST one moves on to the second row. It
 * RETAINS the statement object -- a row outliving the variable that fetched it
 * still reads (and still keeps the database open under it) -- and the statement
 * points back at it without a reference, which the row's own release clears.
 */
/* The row's hidden slots: the statement it reads, and the object that owns it. */
#define PDOROW_RES  "__res"
#define PDOROW_STMT "__stmt"

static phl_pdo_stmt * PdoRowStmt(ph7_class_instance *pThis)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr,PDOROW_RES,sizeof(PDOROW_RES)-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 || !ph7_value_is_resource(pRes) ){
		return 0;
	}
	return (phl_pdo_stmt *)ph7_value_to_resource(pRes);
}
/*
 * php reads a property or offset NAME as a column NUMBER when it is an integer
 * string -- `$row->{'0'}` and `$row['1']` are columns, not names -- and as a
 * column NAME otherwise. The grammar is php's is_numeric_string answering
 * IS_LONG: whitespace, a sign, digits, whitespace, and a value an int64 holds
 * (an overflow reads as a double there, so it falls through to the name lookup
 * and misses).
 */
static int PdoRowIntName(const char *z,int n,ph7_int64 *pOut)
{
	const char *zEnd = z + n;
	int bNeg = 0, nDigit = 0;
	ph7_int64 iVal = 0;
	while( z < zEnd && SyisSpace(z[0]) ){ z++; }
	if( z < zEnd && (z[0] == '+' || z[0] == '-') ){
		bNeg = (z[0] == '-');
		z++;
	}
	while( z < zEnd && SyisDigit(z[0]) ){
		int iDigit = z[0] - '0';
		if( iVal > (SXI64_HIGH - iDigit) / 10 ){
			return 0;
		}
		iVal = iVal * 10 + iDigit;
		z++;
		nDigit++;
	}
	while( z < zEnd && SyisSpace(z[0]) ){ z++; }
	if( nDigit == 0 || z != zEnd ){
		return 0;
	}
	*pOut = bNeg ? -iVal : iVal;
	return 1;
}
/* php's read handlers answer `queryString` from the STATEMENT before they look
 * at any column, so a query selecting a column of that name cannot shadow it --
 * while has_property/has_dimension do not know the name at all. */
static int PdoRowIsQueryString(const char *zName,int nName)
{
	return nName == sizeof("queryString")-1
		&& SyMemcmp(zName,"queryString",sizeof("queryString")-1) == 0;
}
static void PdoRowQueryString(phl_pdo_stmt *pSt,ph7_value *pOut)
{
	ph7_value *pQs = pSt->pOwner ? PH7_NativeAttr(pSt->pOwner,"queryString") : 0;
	if( pQs ){
		PH7_MemObjStore(pQs,pOut);
	}
}
/*
 * How many columns the row HAS, which is the statement's own count and not the
 * capture's: php describes a statement once and answers for those columns for
 * as long as the cursor exists, so a walk that has run out still knows every
 * name and reads each as a null the connection's modifiers may still reshape.
 */
static int PdoRowColumnCount(phl_pdo_stmt *pSt)
{
	return pSt ? PH7_PdoSqliteColumnCount(pSt) : 0;
}
/*
 * The column a name selects, or -1. php looks a NUMBER up positionally and
 * misses outright when it is out of range -- it never falls back to a name of
 * the same spelling -- and matches a NAME byte for byte, first occurrence
 * winning when a query selects one twice.
 */
static int PdoRowColumnOf(phl_pdo_stmt *pSt,const char *zName,int nName)
{
	int nCol = PdoRowColumnCount(pSt);
	ph7_int64 iPos;
	int iCol;
	if( nCol < 1 ){
		return -1;
	}
	if( PdoRowIntName(zName,nName,&iPos) ){
		return (iPos >= 0 && iPos < (ph7_int64)nCol) ? (int)iPos : -1;
	}
	for( iCol = 0 ; iCol < nCol ; ++iCol ){
		SyBlob sHave;
		int nHave, bHit;
		SyBlobInit(&sHave,&pSt->pConn->pVm->sAllocator);
		PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sHave);
		nHave = (int)SyBlobLength(&sHave) - 1;   /* less the terminator */
		bHit = nHave == nName
			&& SyMemcmp((const char *)SyBlobData(&sHave),zName,(sxu32)nName) == 0;
		SyBlobRelease(&sHave);
		if( bHit ){
			return iCol;
		}
	}
	return -1;
}
/*
 * One column's value as the SCRIPT sees it: the captured raw value with the
 * connection's presentation modifiers applied now rather than at capture, so a
 * STRINGIFY_FETCHES or ORACLE_NULLS changed between two reads shows in the
 * second -- which is what php's read-through row does.
 */
static void PdoRowColumnValue(phl_pdo_stmt *pSt,int iCol,ph7_value *pOut)
{
	ph7_value *pRaw = pSt->pLazyVals
		? PdoArrayAtInt(pSt->pConn->pVm,pSt->pLazyVals,(sxi64)iCol) : 0;
	if( pRaw ){
		PH7_MemObjStore(pRaw,pOut);
	}
	/* A column with no captured value is php's null -- and ORACLE_NULLS still
	 * has its say over that null, which is why an exhausted row under
	 * NULL_TO_STRING reads the empty string rather than null. */
	PdoApplyValueMods(pSt->pConn,pOut);
}
/*
 * php's read_property / has_property / write_property / unset_property for the
 * row. A name that is no column at all is NULL to a read and false to an
 * isset(), never a warning -- and `queryString` is answered from the STATEMENT
 * BEFORE any column is looked at, so a query selecting a column of that name
 * cannot shadow it. The has side does NOT know the name at all, which is why
 * `isset($row->queryString)` is false while reading it works.
 */
static void PdoRowProp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)
{
	phl_pdo_stmt *pSt = PdoRowStmt(pThis);
	const char *zName = SyStringData(pCtx->pName);
	int nName = (int)SyStringLength(pCtx->pName);
	int iCol;
	SXUNUSED(pVm);
	if( pCtx->iMode == PH7_NATIVE_PROP_WRITE ){
		pCtx->zThrowClass = "Error";
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Cannot write to PDORow property");
		return;
	}
	if( pCtx->iMode == PH7_NATIVE_PROP_UNSET ){
		pCtx->zThrowClass = "Error";
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Cannot unset PDORow property");
		return;
	}
	pCtx->bAnswered = 1;
	if( pSt == 0 ){
		return;   /* the statement is gone: every name reads null */
	}
	if( (pCtx->iMode == PH7_NATIVE_PROP_READ) && PdoRowIsQueryString(zName,nName) ){
		PdoRowQueryString(pSt,pCtx->pResult);
		return;
	}
	iCol = PdoRowColumnOf(pSt,zName,nName);
	if( iCol >= 0 ){
		PdoRowColumnValue(pSt,iCol,pCtx->pResult);
	}
	if( pCtx->iMode != PH7_NATIVE_PROP_READ ){
		/* php's has_property fetches the value and judges it -- by NULL-ness for
		 * isset() and by TRUTH for property_exists(), which asks the same handler
		 * with a non-zero check_empty. Either way it does NOT know the name
		 * `queryString`, which is why reading one works where isset() on it is
		 * false. */
		int bSet;
		if( pCtx->iMode == PH7_NATIVE_PROP_EXISTS ){
			bSet = iCol >= 0 && ph7_value_to_bool(pCtx->pResult);
		}else{
			bSet = iCol >= 0 && (pCtx->pResult->iFlags & MEMOBJ_NULL) == 0;
		}
		PH7_MemObjRelease(pCtx->pResult);
		ph7_value_bool(pCtx->pResult,bSet);
	}
}
/*
 * php's read_dimension / has_dimension for the row, and the three refusals its
 * write side gives. The offset is the property NAME spelled as a value: an
 * integer is a column number outright, and everything else is converted to a
 * string first -- which is where an object offset raises php's
 * "could not be converted to string" Error and an array warns.
 */
static void PdoRowDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)
{
	phl_pdo_stmt *pSt = PdoRowStmt(pThis);
	ph7_value sKey;
	const char *zName;
	int nName, iCol;
	if( pCtx->iMode == PH7_NATIVE_DIM_WRITE || pCtx->iMode == PH7_NATIVE_DIM_APPEND
	 || pCtx->iMode == PH7_NATIVE_DIM_UNSET ){
		pCtx->zThrowClass = "Error";
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),"Cannot %s PDORow offset",
			pCtx->iMode == PH7_NATIVE_DIM_WRITE ? "write to"
			: (pCtx->iMode == PH7_NATIVE_DIM_APPEND ? "append to" : "unset"));
		return;
	}
	if( pCtx->pOffset == 0 || pSt == 0 ){
		return;   /* `$row[]` read, or a statement that is gone: null */
	}
	if( pCtx->pOffset->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pObj = (ph7_class_instance *)pCtx->pOffset->x.pOther;
		pCtx->zThrowClass = "Error";
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Object of class %.*s could not be converted to string",
			(int)pObj->pClass->sName.nByte,pObj->pClass->sName.zString);
		return;
	}
	PH7_MemObjInit(pVm,&sKey);
	PH7_MemObjStore(pCtx->pOffset,&sKey);
	if( sKey.iFlags & MEMOBJ_HASHMAP ){
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,"Array to string conversion");
	}
	PH7_MemObjToString(&sKey);
	zName = (const char *)SyBlobData(&sKey.sBlob);
	nName = (int)SyBlobLength(&sKey.sBlob);
	if( pCtx->iMode == PH7_NATIVE_DIM_READ && zName && PdoRowIsQueryString(zName,nName) ){
		PdoRowQueryString(pSt,pCtx->pResult);
		PH7_MemObjRelease(&sKey);
		return;
	}
	iCol = PdoRowColumnOf(pSt,zName ? zName : "",zName ? nName : 0);
	if( iCol >= 0 ){
		PdoRowColumnValue(pSt,iCol,pCtx->pResult);
	}
	if( pCtx->iMode == PH7_NATIVE_DIM_ISSET ){
		int bSet = iCol >= 0 && (pCtx->pResult->iFlags & MEMOBJ_NULL) == 0;
		PH7_MemObjRelease(pCtx->pResult);
		ph7_value_bool(pCtx->pResult,bSet);
	}
	PH7_MemObjRelease(&sKey);
}
/*
 * php's get_debug_info for the row: `queryString` and then every column of the
 * row it is sitting on, which is why var_dump() shows what get_object_vars()
 * does not. The get_properties half shows nothing at all, so (array), var_export
 * and json_encode answer empty.
 */
static sxi32 PdoRowPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	phl_pdo_stmt *pSt = PdoRowStmt(pThis);
	ph7_value sKey,sVal;
	int nCol,iCol;
	if( !bDebug || pSt == 0 ){
		return SXRET_OK;
	}
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjInit(pVm,&sVal);
	PH7_MemObjStringAppend(&sKey,"queryString",sizeof("queryString")-1);
	if( pSt->pOwner ){
		ph7_value *pQs = PH7_NativeAttr(pSt->pOwner,"queryString");
		if( pQs ){
			PH7_MemObjStore(pQs,&sVal);
		}
	}
	ph7_array_add_elem(pOut,&sKey,&sVal);
	/* The COLUMNS come from the statement rather than from the capture: php
	 * describes them once and shows them for as long as the cursor exists, so a
	 * row whose walk has run out (or whose cursor was closed) still prints every
	 * name, each holding null. */
	nCol = PdoRowColumnCount(pSt);
	for( iCol = 0 ; iCol < nCol ; ++iCol ){
		SyBlob sColName;
		int nName;
		const char *zName;
		SyBlobInit(&sColName,&pVm->sAllocator);
		PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sColName);
		zName = (const char *)SyBlobData(&sColName);
		nName = (int)SyBlobLength(&sColName) - 1;   /* less the terminator */
		if( zName == 0 || nName < 0 || PdoRowIsQueryString(zName,nName) ){
			/* php builds the columns as a table of their own and merges it
			 * BEHIND the queryString entry, so a column of that name is the one
			 * that loses -- while two columns sharing any other name collapse
			 * to the LAST of them, which the update below does. */
			SyBlobRelease(&sColName);
			continue;
		}
		PH7_MemObjRelease(&sKey);
		PH7_MemObjInitFromString(pVm,&sKey,0);
		PH7_MemObjStringAppend(&sKey,zName,(sxu32)nName);
		PH7_MemObjRelease(&sVal);
		PH7_MemObjInit(pVm,&sVal);
		PdoRowColumnValue(pSt,iCol,&sVal);
		ph7_array_add_elem(pOut,&sKey,&sVal);
		SyBlobRelease(&sColName);
	}
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	return SXRET_OK;
}
/*
 * php gives the row `zend_objects_not_comparable`: no two PDORows are ever
 * equal, `<=>` answers the uncomparable 1 from either side, and every
 * relational spelling is false -- `$row == $row` alone is true, and that is the
 * engine's identity shortcut answering before any handler. A BOOL partner is
 * not this handler's business in php either: that comparison converts both
 * sides, which is why `$row == true` is true.
 */
static void PdoRowCmp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)
{
	SXUNUSED(pVm);
	SXUNUSED(pThis);
	if( pCtx->pOtherValue && (pCtx->pOtherValue->iFlags & MEMOBJ_BOOL) ){
		return;   /* declined: php's cast rule decides an object against a bool */
	}
	pCtx->bAnswered = 1;
	pCtx->iResult = 1;   /* php's ZEND_UNCOMPARABLE, the same from both directions */
}
/* The row is going away: the statement must stop pointing at it. */
static void PdoRowInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_pdo_stmt *pSt = PdoRowStmt(pThis);
	SXUNUSED(pVm);
	if( pSt && pSt->pLazyRow == pThis ){
		pSt->pLazyRow = 0;
		PdoStmtLazyClear(pSt);
	}
}
/*
 * The statement's row object, built on first use and CAPTURING the row under
 * the cursor, which it also marks as spent. Answers the object with a
 * reference of the caller's own, or 0 when it could not be made. Shared by
 * fetch() and the foreach iterator: php answers both from one lazy row.
 */
static ph7_class_instance * PdoLazyRowFor(ph7_vm *pVm,phl_pdo_stmt *pSt)
{
	ph7_class_instance *pRow = pSt->pLazyRow;
	PdoBoundColumnsForRow(pVm,pSt);
	PdoStmtLazyCapture(pSt);
	if( pRow == 0 ){
		ph7_class *pClass = PH7_VmExtractClass(pVm,"PDORow",sizeof("PDORow")-1,FALSE,0);
		SyString sAttr;
		ph7_value *pSlot;
		pRow = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
		if( pRow == 0 ){
			return 0;
		}
		SyStringInitFromBuf(&sAttr,PDOROW_RES,sizeof(PDOROW_RES)-1);
		pSlot = PH7_ClassInstanceFetchAttr(pRow,&sAttr);
		if( pSlot == 0 ){
			PH7_ClassInstanceUnref(pRow);
			return 0;
		}
		PH7_MemObjRelease(pSlot);
		pSlot->x.pOther = pSt;
		MemObjSetType(pSlot,MEMOBJ_RES);
		/* Retain the STATEMENT object through a slot of the row's own: php's
		 * row keeps its statement alive, so `unset($stmt)` leaves the row
		 * reading and the database open. The statement's pointer back here is
		 * deliberately NOT a reference -- that pair would be a cycle no
		 * refcount can break. */
		SyStringInitFromBuf(&sAttr,PDOROW_STMT,sizeof(PDOROW_STMT)-1);
		pSlot = PH7_ClassInstanceFetchAttr(pRow,&sAttr);
		if( pSlot && pSt->pOwner ){
			PH7_MemObjRelease(pSlot);
			pSt->pOwner->iRef++;
			pSlot->x.pOther = pSt->pOwner;
			MemObjSetType(pSlot,MEMOBJ_OBJ);
		}
		pSt->pLazyRow = pRow;
	}else{
		pRow->iRef++;   /* the caller's own reference */
	}
	pSt->bRowPending = 0;
	return pRow;
}
/*
 * PDO::FETCH_LAZY: hand the row object back and move the cursor on. The row
 * carries no values of its own -- the capture on the statement is what it
 * reads -- so a second lazy fetch answers the SAME object showing the next
 * row, which is php.
 */
static int PdoFetchLazyRow(ph7_context *pCtx,phl_pdo_stmt *pSt)
{
	ph7_class_instance *pRow;
	if( !PdoStmtHasRow(pSt) ){
		PdoStmtOk(pSt);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRow = PdoLazyRowFor(pCtx->pVm,pSt);
	if( pRow == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( PdoStmtStep(pSt) < 0 ){
		PdoStmtFailed(pSt,pSt->pConn->zSqlState);
		PH7_ClassInstanceUnref(pRow);
		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");
	}
	PdoStmtOk(pSt);
	PH7_NativeResultObject(pCtx,pRow);
	return PH7_OK;
}
/*
 * PDOStatement::fetch(int $mode = PDO::FETCH_DEFAULT, ...): mixed
 *
 * FETCH_DEFAULT means the connection's ATTR_DEFAULT_FETCH_MODE, which is
 * FETCH_BOTH unless the script changed it -- so a bare fetch() answers every
 * column twice, once under its name and once under its position.
 */
static int vm_builtin_PDOStatement_fetch(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	ph7_value *pRow;
	int iMode;
	sxi32 rcFlags;
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;
	rcFlags = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::fetch",1,"mode");
	if( rcFlags != PH7_OK ){
		return rcFlags;
	}
	if( iMode == PDO_FETCH_DEFAULT ){
		iMode = pSt->iFetchMode;
	}
	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_DEFAULT ){
		/* A statement whose own mode is FETCH_DEFAULT -- which only a
		 * connection whose ATTR_DEFAULT_FETCH_MODE is 0 leaves it on -- has no
		 * mode to fall back to, and php says so at the fetch. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"PDOStatement::fetch(): Argument #1 ($mode) must be a bitmask of "
			"PDO::FETCH_* constants");
	}
	if( PdoBoundColumnsBad(pSt) ){
		return PdoBoundColumnsRefuse(pCtx,pSt,0);
	}
	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_LAZY ){
		return PdoFetchLazyRow(pCtx,pSt);
	}
	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_KEY_PAIR ){
		/* php's own fetch() cannot do this mode: it builds a value var_dump
		 * crashes on and json_encode refuses, and one spelling of the same call
		 * aborts the process (§10 -- a php defect PHL does not reproduce). The
		 * honest answer is the one the mode NAMES and fetchAll() builds: the
		 * row as a single key => value pair. */
		ph7_value *pPair,*pRowVals,*pKey,*pVal;
		if( PH7_PdoSqliteColumnCount(pSt) != 2 ){
			return PH7_VmThrowException(pCtx,"PDOException",
				"SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires "
				"the result set to contain exactly 2 columns.");
		}
		if( !PdoStmtHasRow(pSt) ){
			PdoStmtOk(pSt);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pPair    = ph7_context_new_array(pCtx);
		pRowVals = ph7_context_new_array(pCtx);
		if( pPair == 0 || pRowVals == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pRowVals) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pKey = PdoArrayAtInt(pCtx->pVm,pRowVals,0);
		pVal = PdoArrayAtInt(pCtx->pVm,pRowVals,1);
		ph7_array_add_elem(pPair,pKey,pVal);
		ph7_result_value(pCtx,pPair);
		if( PdoStmtStep(pSt) < 0 ){
			PdoStmtFailed(pSt,pSt->pConn->zSqlState);
			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");
		}
		PdoStmtOk(pSt);
		return PH7_OK;
	}
	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_COLUMN ){
		/* a statement told to fetch one COLUMN answers that column from here
		 * on, whichever verb asks for the row */
		ph7_value *pOneRow,*pOne;
		if( !PdoStmtHasRow(pSt) ){
			PdoStmtOk(pSt);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		if( pSt->iFetchColumn >= PH7_PdoSqliteColumnCount(pSt) ){
			/* php checks the width only once it has a ROW to read it from, so a
			 * cursor with nothing left answers false rather than refusing. */
			return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");
		}
		pOneRow = ph7_context_new_array(pCtx);
		if( pOneRow == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pOneRow) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pOne = PdoArrayAtInt(pCtx->pVm,pOneRow,(sxi64)pSt->iFetchColumn);
		if( pOne ){
			ph7_result_value(pCtx,pOne);
		}else{
			ph7_result_null(pCtx);
		}
		if( PdoStmtStep(pSt) < 0 ){
			PdoStmtFailed(pSt,pSt->pConn->zSqlState);
			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");
		}
		PdoStmtOk(pSt);
		return PH7_OK;
	}
	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_BOUND ){
		if( !PdoStmtHasRow(pSt) ){
			PdoStmtOk(pSt);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		PdoBoundColumnsForRow(pCtx->pVm,pSt);
		pSt->bRowPending = 0;
		ph7_result_bool(pCtx,1);
		if( PdoStmtStep(pSt) < 0 ){
			PdoStmtFailed(pSt,pSt->pConn->zSqlState);
			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");
		}
		PdoStmtOk(pSt);
		return PH7_OK;
	}
	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_CLASS
	 || (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_INTO ){
		return PdoFetchObjectRow(pCtx,pSt,iMode,0,0,"PDOStatement::fetch");
	}
	if( !PdoStmtHasRow(pSt) ){
		PdoStmtOk(pSt);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRow = ph7_context_new_array(pCtx);
	if( pRow == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !PdoStmtRow(pCtx->pVm,pSt,iMode & PDO_FETCH_MODE_MASK,pRow) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_value(pCtx,pRow);
	/* step ahead so the next call knows whether a row is waiting without
	 * having to ask twice */
	if( PdoStmtStep(pSt) < 0 ){
		PdoStmtFailed(pSt,pSt->pConn->zSqlState);
		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");
	}
	PdoStmtOk(pSt);
	return PH7_OK;
}
/*
 * PDOStatement::getColumnMeta(int $column): array|false
 *
 * php's eight keys. Two of them describe different things and are routinely
 * confused: `sqlite:decl_type` is what the SCHEMA declares, and `native_type`
 * is the type of the value in the CURRENT row -- so a TEXT column holding NULL
 * reports "TEXT" and "null" at once, and an exhausted cursor reports "null"
 * for every column.
 *
 * A column that does not exist answers false, and php reports the last STEP's
 * result code as the driver error while doing so: that is why asking for
 * column 99 while a row is up comes back as "100 another row available"
 * instead of anything about the index.
 */
static int vm_builtin_PDOStatement_getColumnMeta(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	ph7_value *pMeta,*pCell,*pFlags;
	ph7_int64 iCol;
	const char *zDecl,*zTable,*zName;
	int iType,iPdoType;
	const char *zNative;
	SyBlob sName;
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	iCol = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	if( iCol < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"PDOStatement::getColumnMeta(): Argument #1 ($column) must be greater than "
			"or equal to 0");
	}
	if( iCol >= (ph7_int64)PH7_PdoSqliteColumnCount(pSt) ){
		int iStep = PH7_PdoSqliteLastStepCode(pSt);
		ph7_result_bool(pCtx,0);
		/* php reports the last STEP's code as the driver detail here, which is
		 * why an out-of-range index talks about a row being available */
		PH7_PdoSetError(pSt->pConn,"HY000",iStep,
			iStep == 100 ? "another row available" : "no more rows available");
		pSt->pConn->iErrState = PDO_ERR_OK;   /* the CONNECTION did not fail */
		SyMemcpy("00000",pSt->pConn->zSqlState,sizeof("00000"));
		PdoStmtFailed(pSt,"HY000");
		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::getColumnMeta");
	}
	pMeta = ph7_context_new_array(pCtx);
	pCell = ph7_context_new_scalar(pCtx);
	pFlags = ph7_context_new_array(pCtx);
	if( pMeta == 0 || pCell == 0 || pFlags == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	iType = pSt->bRowPending ? PH7_PdoSqliteColumnType(pSt,(int)iCol) : SQLITE_NULL;
	switch( iType ){
		case SQLITE_INTEGER: zNative = "integer"; iPdoType = PDO_PARAM_INT; break;
		case SQLITE_FLOAT:   zNative = "double";  iPdoType = PDO_PARAM_STR; break;
		case SQLITE_BLOB:    zNative = "blob";    iPdoType = PDO_PARAM_LOB; break;
		case SQLITE_NULL:    zNative = "null";    iPdoType = PDO_PARAM_NULL; break;
		default:             zNative = "string";  iPdoType = PDO_PARAM_STR; break;
	}
	PH7_MemObjRelease(pCell);
	ph7_value_string(pCell,zNative,(int)SyStrlen(zNative));
	ph7_array_add_strkey_elem(pMeta,"native_type",pCell);
	ph7_value_int(pCell,iPdoType);
	ph7_array_add_strkey_elem(pMeta,"pdo_type",pCell);
	zDecl = PH7_PdoSqliteColumnDecl(pSt,(int)iCol);
	if( zDecl ){
		PH7_MemObjRelease(pCell);
		ph7_value_string(pCell,zDecl,(int)SyStrlen(zDecl));
		ph7_array_add_strkey_elem(pMeta,"sqlite:decl_type",pCell);
	}
	zTable = PH7_PdoSqliteColumnTable(pSt,(int)iCol);
	if( zTable ){
		PH7_MemObjRelease(pCell);
		ph7_value_string(pCell,zTable,(int)SyStrlen(zTable));
		ph7_array_add_strkey_elem(pMeta,"table",pCell);
	}
	ph7_array_add_strkey_elem(pMeta,"flags",pFlags);
	SyBlobInit(&sName,&pCtx->pVm->sAllocator);
	zName = PH7_PdoSqliteColumnName(pSt,(int)iCol);
	PdoColumnName(pSt->pConn,zName,&sName);
	PH7_MemObjRelease(pCell);
	ph7_value_string(pCell,(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName) - 1);
	ph7_array_add_strkey_elem(pMeta,"name",pCell);
	SyBlobRelease(&sName);
	ph7_value_int(pCell,-1);
	ph7_array_add_strkey_elem(pMeta,"len",pCell);
	ph7_value_int(pCell,0);
	ph7_array_add_strkey_elem(pMeta,"precision",pCell);
	ph7_result_value(pCtx,pMeta);
	return PH7_OK;
}
/*
 * PDOStatement::nextRowset(): bool
 *
 * sqlite has no second result set to move to, so this is the layer refusal --
 * false, and IM001 with php's own "driver does not support multiple rowsets".
 * It leaves the CURSOR alone: a fetch after it still answers the row that was
 * waiting.
 */
static int vm_builtin_PDOStatement_nextRowset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	ph7_result_bool(pCtx,0);
	PdoStmtFailed(pSt,"IM001");
	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::nextRowset","IM001",
		"driver does not support multiple rowsets");
}
/*
 * PDOStatement::fetchColumn(int $column = 0): mixed
 *
 * One column of the next row, by position. An index outside the RESULT SET is
 * a ValueError rather than a null, and its two refusals are worded unlike
 * fetchAll()'s -- php's own inconsistency, reproduced.
 */
static int vm_builtin_PDOStatement_fetchColumn(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	ph7_int64 iCol;
	ph7_value *pRow,*pCell;
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	iCol = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	if( iCol < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Column index must be greater than or equal to 0");
	}
	if( iCol >= (ph7_int64)PH7_PdoSqliteColumnCount(pSt) ){
		return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");
	}
	if( !PdoStmtHasRow(pSt) ){
		PdoStmtOk(pSt);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( PdoBoundColumnsBad(pSt) ){
		return PdoBoundColumnsRefuse(pCtx,pSt,0);
	}
	pRow = ph7_context_new_array(pCtx);
	if( pRow == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pRow) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pCell = PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)iCol);
	if( pCell ){
		ph7_result_value(pCtx,pCell);
	}else{
		ph7_result_null(pCtx);
	}
	if( PdoStmtStep(pSt) < 0 ){
		PdoStmtFailed(pSt,pSt->pConn->zSqlState);
		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchColumn");
	}
	PdoStmtOk(pSt);
	return PH7_OK;
}
/*
 * php's `pdo_stmt_setup_fetch_mode`: the mode a statement will use from here
 * on, and the whole screen over it. Two verbs give one: setFetchMode()'s first
 * argument and query()'s SECOND, so every diagnostic counts arguments the way
 * the verb that took them does -- `iModeArg` is the mode's own 1-based
 * position, and the counts php reports are that position plus what the mode
 * needs beside it.
 *
 * The rules are php's, per mode: FETCH_COLUMN wants a column NUMBER and
 * FETCH_INTO an OBJECT, both exactly one; FETCH_CLASS wants a class NAME and
 * accepts constructor arguments behind it -- unless FETCH_CLASSTYPE rides on
 * it, which takes the class from the first column and therefore wants nothing;
 * FETCH_FUNC belongs to fetchAll() alone; and every other mode takes the mode
 * and nothing else. A base outside php's own enum is `must be a bitmask of
 * PDO::FETCH_* constants`. FETCH_DEFAULT itself names the connection's
 * ATTR_DEFAULT_FETCH_MODE and leaves the statement on it.
 */
static sxi32 PdoSetupFetchMode(ph7_context *pCtx,phl_pdo_stmt *pSt,int nArg,ph7_value **apArg,
	int iModeArg,const char *zFn,const char *zModeParam)
{
	ph7_value *pMode = nArg >= iModeArg ? apArg[iModeArg-1] : 0;
	int iMode = pMode ? (int)ph7_value_to_int64(pMode) : PDO_FETCH_DEFAULT;
	int iBase = iMode & PDO_FETCH_MODE_MASK;
	int nExtra = nArg - iModeArg;          /* arguments given BEHIND the mode */
	char zBuf[64];
	sxi32 rc;
	/* php clears the statement's mode BEFORE it judges the new one, and clears
	 * it to the CONNECTION's default rather than to what the statement was
	 * carrying -- so a REFUSED setFetchMode() leaves a statement that was
	 * fetching NUM answering whatever ATTR_DEFAULT_FETCH_MODE says. */
	PdoStmtClearFetchState(pSt);
	pSt->iFetchMode = pSt->pConn->iDefaultFetch;
	pSt->iFetchColumn = 0;
	if( iBase > PDO_FETCH_KEY_PAIR ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($%s) must be a bitmask of PDO::FETCH_* constants",
			zFn,iModeArg,zModeParam);
	}
	rc = PdoCheckFetchFlags(pCtx,iMode,zFn,iModeArg,zModeParam);
	if( rc != PH7_OK ){
		return rc;
	}
	if( iBase == PDO_FETCH_FUNC ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($%s) PDO::FETCH_FUNC can only be used with "
			"PDOStatement::fetchAll()",zFn,iModeArg,zModeParam);
	}
	if( iBase == PDO_FETCH_CLASS && (iMode & PDO_FETCH_CLASSTYPE) == 0 ){
		/* The class NAME, then optional constructor arguments. php checks the
		 * TYPE of what it was handed before it counts, so a wrong second
		 * argument is a TypeError even when a fourth is there too. */
		if( nExtra < 1 ){
			return PH7_VmThrowException(pCtx,"ArgumentCountError",
				"%s() expects at least %d arguments for the fetch mode provided, %d given",
				zFn,iModeArg+1,nArg);
		}
		if( (apArg[iModeArg]->iFlags & MEMOBJ_STRING) == 0 ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #%d must be of type string, %s given",
				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));
		}
		{
			/* php resolves the name HERE -- before it looks at the constructor
			 * arguments behind it -- so a class that does not exist is refused
			 * where it was named rather than at the first fetch, and one that
			 * merely cannot be instantiated is accepted here and refused there. */
			int nCls = 0;
			const char *zCls = ph7_value_to_string(apArg[iModeArg],&nCls);
			if( zCls == 0 || nCls < 1
			 || PH7_VmExtractClass(pCtx->pVm,zCls,(sxu32)nCls,FALSE,0) == 0 ){
				return PH7_VmThrowException(pCtx,"TypeError",
					"%s(): Argument #%d must be a valid class",zFn,iModeArg+1);
			}
		}
		if( nExtra > 1 && (apArg[iModeArg+1]->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_NULL)) == 0 ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #%d must be of type ?array, %s given",
				zFn,iModeArg+2,VmValueGivenName(apArg[iModeArg+1],zBuf,sizeof(zBuf)));
		}
		if( nExtra > 2 ){
			return PH7_VmThrowException(pCtx,"ArgumentCountError",
				"%s() expects at most %d arguments for the fetch mode provided, %d given",
				zFn,iModeArg+2,nArg);
		}
	}else if( iBase == PDO_FETCH_COLUMN || iBase == PDO_FETCH_INTO ){
		if( nExtra != 1 ){
			return PH7_VmThrowException(pCtx,"ArgumentCountError",
				"%s() expects exactly %d arguments for the fetch mode provided, %d given",
				zFn,iModeArg+1,nArg);
		}
		/* php's screen is the zval's TYPE: only a real int passes, and a float
		 * whose value happens to be integral does not (the slot may carry the
		 * int flag beside the real one once something has read it as a number,
		 * so the REAL bit is what decides). */
		if( iBase == PDO_FETCH_COLUMN
		 && ((apArg[iModeArg]->iFlags & MEMOBJ_INT) == 0
		  || (apArg[iModeArg]->iFlags & MEMOBJ_REAL) != 0) ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #%d must be of type int, %s given",
				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));
		}
		if( iBase == PDO_FETCH_INTO && (apArg[iModeArg]->iFlags & MEMOBJ_OBJ) == 0 ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #%d must be of type object, %s given",
				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));
		}
		if( iBase == PDO_FETCH_COLUMN && ph7_value_to_int64(apArg[iModeArg]) < 0 ){
			/* A NEGATIVE column is refused where it is given; one merely past
			 * the last column is not, and answers php's `Invalid column index`
			 * at the fetch -- the statement's width is not this screen's
			 * business. */
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #%d must be greater than or equal to 0",zFn,iModeArg+1);
		}
	}else if( nExtra > 0 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"%s() expects exactly %d arguments for the fetch mode provided, %d given",
			zFn,iModeArg,nArg);
	}
	if( iBase == PDO_FETCH_CLASS && (iMode & PDO_FETCH_CLASSTYPE) == 0 ){
		int nName = 0;
		const char *zName = ph7_value_to_string(apArg[iModeArg],&nName);
		if( zName && nName > 0 ){
			pSt->zFetchClass = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
				(sxu32)nName + 1);
			if( pSt->zFetchClass ){
				SyMemcpy(zName,pSt->zFetchClass,(sxu32)nName);
				pSt->zFetchClass[nName] = 0;
				pSt->nFetchClass = nName;
			}
		}
		if( nExtra > 1 && (apArg[iModeArg+1]->iFlags & MEMOBJ_HASHMAP) ){
			pSt->pFetchArgs = ph7_new_array(pCtx->pVm);
			if( pSt->pFetchArgs ){
				PH7_MemObjStore(apArg[iModeArg+1],pSt->pFetchArgs);
			}
		}
	}else if( iBase == PDO_FETCH_INTO ){
		pSt->pFetchInto = (ph7_class_instance *)apArg[iModeArg]->x.pOther;
		pSt->pFetchInto->iRef++;   /* the statement writes into it for as long as it lives */
	}
	/* FETCH_DEFAULT is not a mode to keep: the statement stays on the
	 * connection's default it was just cleared to (php 8.5.11, GH-20214). */
	if( iBase != PDO_FETCH_DEFAULT ){
		pSt->iFetchMode = iMode;
	}
	pSt->iFetchColumn = iBase == PDO_FETCH_COLUMN
		? (int)ph7_value_to_int64(apArg[iModeArg]) : 0;
	return PH7_OK;
}
/*
 * PDOStatement::setFetchMode(int $mode, mixed ...$args): true
 *
 * The mode a bare fetch()/fetchAll() will use from here on -- one spelling of
 * the screen above, the other being PDO::query()'s second argument.
 */
static int vm_builtin_PDOStatement_setFetchMode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	sxi32 rc;
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	rc = PdoSetupFetchMode(pCtx,pSt,nArg,apArg,1,"PDOStatement::setFetchMode","mode");
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* Add one row under a key, collecting repeats into a list (FETCH_GROUP). */
static void PdoGroupAppend(ph7_context *pCtx,ph7_value *pOut,ph7_value *pKey,ph7_value *pRow)
{
	ph7_value *pList = 0;
	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){
		int nKey = 0;
		const char *zKey = ph7_value_to_string(pKey,&nKey);
		pList = ph7_array_fetch(pOut,zKey,nKey);
	}else if( pKey ){
		SyBlob sKey;
		SyBlobInit(&sKey,&pCtx->pVm->sAllocator);
		SyBlobFormat(&sKey,"%qd",ph7_value_to_int64(pKey));
		SyBlobAppend(&sKey,"",1);
		pList = ph7_array_fetch(pOut,(const char *)SyBlobData(&sKey),
			(int)SyBlobLength(&sKey) - 1);
		SyBlobRelease(&sKey);
	}
	if( pList && (pList->iFlags & MEMOBJ_HASHMAP) ){
		ph7_array_add_elem(pList,0,pRow);
		return;
	}
	pList = ph7_context_new_array(pCtx);
	if( pList == 0 ){
		return;
	}
	ph7_array_add_elem(pList,0,pRow);
	ph7_array_add_elem(pOut,pKey,pList);
}
/*
 * PDOStatement::fetchAll(int $mode = PDO::FETCH_DEFAULT, mixed ...$args): array
 *
 * Every remaining row in one array. Four of the modes change the shape of that
 * ARRAY rather than the shape of a row: FETCH_COLUMN reduces each row to one
 * value, FETCH_KEY_PAIR to a key and a value (and refuses a result set that is
 * not exactly two columns wide), FETCH_FUNC replaces it with whatever a
 * callable answers, and GROUP/UNIQUE take the first column as a key -- GROUP
 * collecting every row under it, UNIQUE keeping the last.
 *
 * php counts arguments per mode here too, and its FETCH_FUNC wording is
 * singular ("expects exactly 2 argument"); both are reproduced as they stand.
 */
static int vm_builtin_PDOStatement_fetchAll(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	ph7_value *pOut,*pRow;
	int iMode,iBase,iCol = 0;
	int bGroup,bUnique;
	sxi32 rc;
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;
	rc = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::fetchAll",1,"mode");
	if( rc != PH7_OK ){
		return rc;
	}
	bGroup = (iMode & PDO_FETCH_GROUP) == PDO_FETCH_GROUP;
	bUnique = (iMode & PDO_FETCH_UNIQUE) == PDO_FETCH_UNIQUE;
	iBase = iMode & PDO_FETCH_MODE_MASK;
	if( iBase == PDO_FETCH_DEFAULT ){
		iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;
		bGroup = bGroup || (pSt->iFetchMode & PDO_FETCH_GROUP) == PDO_FETCH_GROUP;
		bUnique = bUnique || (pSt->iFetchMode & PDO_FETCH_UNIQUE) == PDO_FETCH_UNIQUE;
		iCol = pSt->iFetchColumn;
	}
	if( iBase == PDO_FETCH_DEFAULT ){
		iBase = pSt->pConn->iDefaultFetch;
	}
	if( iBase == PDO_FETCH_COLUMN ){
		if( nArg > 2 ){
			return PH7_VmThrowException(pCtx,"ArgumentCountError",
				"PDOStatement::fetchAll() expects exactly 2 arguments for the fetch "
				"mode provided, %d given",nArg);
		}
		if( nArg > 1 ){
			ph7_int64 iWant = ph7_value_to_int64(apArg[1]);
			if( iWant < 0 ){
				return PH7_VmThrowException(pCtx,"ValueError",
					"PDOStatement::fetchAll(): Argument #2 must be greater than or "
					"equal to 0");
			}
			iCol = (int)iWant;
		}
		if( iCol >= PH7_PdoSqliteColumnCount(pSt) ){
			return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");
		}
	}else if( iBase == PDO_FETCH_CLASS ){
		if( nArg > 3 ){
			return PH7_VmThrowException(pCtx,"ArgumentCountError",
				"PDOStatement::fetchAll() expects at most 3 arguments for the fetch "
				"mode provided, %d given",nArg);
		}
	}else if( iBase == PDO_FETCH_LAZY ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"PDOStatement::fetchAll(): Argument #1 ($mode) PDO::FETCH_LAZY cannot be "
			"used with PDOStatement::fetchAll()");
	}else if( iBase == PDO_FETCH_FUNC ){
		if( nArg != 2 ){
			return PH7_VmThrowException(pCtx,"ArgumentCountError",
				"PDOStatement::fetchAll() expects exactly 2 argument for "
				"PDO::FETCH_FUNC, %d given",nArg);
		}
		if( !ph7_value_is_callable(apArg[1]) ){
			/* php checks the callable BEFORE the first row, so an unusable one
			 * is a TypeError from PDO and never the engine's own
			 * "Call to undefined function" from inside the walk */
			int nName = 0;
			const char *zName = ph7_value_to_string(apArg[1],&nName);
			return PH7_VmThrowException(pCtx,"TypeError",
				"function \"%.*s\" not found or invalid function name",nName,zName);
		}
	}else if( nArg > 1 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"PDOStatement::fetchAll() expects exactly 1 argument for the fetch mode "
			"provided, %d given",nArg);
	}
	if( iBase == PDO_FETCH_KEY_PAIR && PH7_PdoSqliteColumnCount(pSt) != 2 ){
		return PH7_VmThrowException(pCtx,"PDOException",
			"SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires "
			"the result set to contain exactly 2 columns.");
	}
	pOut = ph7_context_new_array(pCtx);
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( PdoBoundColumnsBad(pSt) ){
		return PdoBoundColumnsRefuse(pCtx,pSt,1);
	}
	while( PdoStmtHasRow(pSt) ){
		int iRowMode = iBase;
		int iFirst = (bGroup || bUnique) ? 1 : 0;
		if( iBase == PDO_FETCH_COLUMN || iBase == PDO_FETCH_KEY_PAIR
		 || iBase == PDO_FETCH_FUNC || iBase == PDO_FETCH_BOUND ){
			iRowMode = PDO_FETCH_NUM;
			iFirst = 0;
		}else if( iBase == PDO_FETCH_CLASS ){
			iFirst = 0;
		}
		pRow = ph7_context_new_array(pCtx);
		if( pRow == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		if( iFirst ){
			/* the FIRST column is the key and never joins the row */
			ph7_value *pKeyRow = ph7_context_new_array(pCtx);
			ph7_value *pKey;
			if( pKeyRow == 0 ){
				return PH7_ContextMemoryError(pCtx);
			}
			if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pKeyRow,0) ){
				break;
			}
			pKey = PdoArrayAtInt(pCtx->pVm,pKeyRow,0);
			/* the row itself is rebuilt from the SECOND column on; the cursor
			 * has not moved, so this reads the same sqlite row again */
			pSt->bRowPending = 1;
			if( !PdoStmtRowFrom(pCtx->pVm,pSt,iRowMode,pRow,1) ){
				break;
			}
			if( bUnique ){
				ph7_array_add_elem(pOut,pKey,pRow);
			}else{
				PdoGroupAppend(pCtx,pOut,pKey,pRow);
			}
		}else if( iBase == PDO_FETCH_CLASS ){
			/* every row is its own instance; the class and its constructor
			 * arguments are the same for all of them */
			ph7_class *pClass;
			ph7_value sObj;
			sxi32 rcCls;
			int iFirstCol = 0;
			if( nArg > 1 ){
				pClass = PdoResolveFetchClass(pCtx,apArg[1],FALSE,&rcCls);
			}else if( iMode & PDO_FETCH_CLASSTYPE ){
				ph7_value *pHead = ph7_context_new_array(pCtx);
				if( pHead == 0 ){
					return PH7_ContextMemoryError(pCtx);
				}
				if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pHead,0) ){
					break;
				}
				pSt->bRowPending = 1;
				pClass = PdoClassTypeClass(pCtx,PdoArrayAtInt(pCtx->pVm,pHead,0));
				rcCls = PH7_OK;
				iFirstCol = 1;
			}else if( pSt->zFetchClass ){
				ph7_value sName;
				SyString sStr;
				SyStringInitFromBuf(&sStr,pSt->zFetchClass,pSt->nFetchClass);
				PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);
				pClass = PdoResolveFetchClass(pCtx,&sName,FALSE,&rcCls);
				PH7_MemObjRelease(&sName);
			}else{
				pClass = PH7_VmExtractClass(pCtx->pVm,"stdClass",sizeof("stdClass")-1,
					FALSE,0);
				rcCls = PH7_OK;
			}
			if( pClass == 0 ){
				return rcCls;
			}
			rcCls = PH7_VmCheckInstantiable(pCtx,pClass);
			if( rcCls != PH7_OK ){
				return rcCls;
			}
			PH7_MemObjInit(pCtx->pVm,&sObj);
			if( !PdoRowIntoObject(pCtx->pVm,pSt,pClass,nArg > 2 ? apArg[2] : 0,
				(iMode & PDO_FETCH_PROPS_LATE) != 0,iFirstCol,&sObj) ){
				PH7_MemObjRelease(&sObj);
				break;
			}
			ph7_array_add_elem(pOut,0,&sObj);
			PH7_MemObjRelease(&sObj);
		}else if( iBase == PDO_FETCH_BOUND ){
			/* the row goes into the BOUND VARIABLES, not into the result: the
			 * array collects one true per row and the caller reads the last
			 * row's values out of its own variables */
			ph7_value *pTrue;
			PdoBoundColumnsForRow(pCtx->pVm,pSt);
			pSt->bRowPending = 0;
			pTrue = ph7_context_new_scalar(pCtx);
			if( pTrue ){
				ph7_value_bool(pTrue,1);
				ph7_array_add_elem(pOut,0,pTrue);
			}
		}else if( !PdoStmtRow(pCtx->pVm,pSt,iRowMode,pRow) ){
			break;
		}else if( iBase == PDO_FETCH_COLUMN ){
			ph7_array_add_elem(pOut,0,PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)iCol));
		}else if( iBase == PDO_FETCH_KEY_PAIR ){
			ph7_array_add_elem(pOut,PdoArrayAtInt(pCtx->pVm,pRow,0),
				PdoArrayAtInt(pCtx->pVm,pRow,1));
		}else if( iBase == PDO_FETCH_FUNC ){
			ph7_value sRes;
			ph7_value *apCall[32];
			int n,nCall = PH7_PdoSqliteColumnCount(pSt);
			if( nCall > (int)SX_ARRAYSIZE(apCall) ){
				nCall = (int)SX_ARRAYSIZE(apCall);
			}
			for( n = 0 ; n < nCall ; ++n ){
				apCall[n] = PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)n);
			}
			PH7_MemObjInit(pCtx->pVm,&sRes);
			if( PH7_VmCallUserFunction(pCtx->pVm,apArg[1],nCall,apCall,&sRes) != SXRET_OK ){
				PH7_MemObjRelease(&sRes);
				return PH7_OK;   /* whatever the callable raised is already in flight */
			}
			ph7_array_add_elem(pOut,0,&sRes);
			PH7_MemObjRelease(&sRes);
		}else{
			ph7_array_add_elem(pOut,0,pRow);
		}
		if( PdoStmtStep(pSt) < 0 ){
			PdoStmtFailed(pSt,pSt->pConn->zSqlState);
			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchAll");
		}
	}
	PdoStmtOk(pSt);
	ph7_result_value(pCtx,pOut);
	return PH7_OK;
}
/*
 * PDOStatement::columnCount(): int
 *
 * 0 for a statement that returns no rows -- and also for one that has been
 * PREPARED but not yet run, even though sqlite already knows the count from
 * the compile. php only publishes it once the statement has executed, so
 * `prepare('SELECT 1')->columnCount()` is 0 and not 1.
 */
static int vm_builtin_PDOStatement_columnCount(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	ph7_result_int(pCtx,pSt->bExecuted ? PH7_PdoSqliteColumnCount(pSt) : 0);
	return PH7_OK;
}
/*
 * PDOStatement::debugDumpParams(): ?bool
 *
 * php's own diagnostic dump, printed rather than returned (it answers null).
 * The bindings appear in the order they were MADE, and the two kinds report
 * differently: a positional one carries its 0-based paramno and an empty name,
 * a named one carries paramno -1 and the name WITH its colon. Both lengths are
 * printed in brackets, php's `[%d]` shape.
 */
static int vm_builtin_PDOStatement_debugDumpParams(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	phl_pdo_bind *pB;
	const char *zSql = "";
	int nSql = 0,nBind = 0;
	ph7_value *pQuery;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	pQuery = pSt->pOwner ? PH7_NativeAttr(pSt->pOwner,"queryString") : 0;
	if( pQuery ){
		zSql = ph7_value_to_string(pQuery,&nSql);
	}
	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){
		++nBind;
	}
	ph7_context_output_format(pCtx,"SQL: [%d] %.*s\n",nSql,nSql,zSql);
	ph7_context_output_format(pCtx,"Params:  %d\n",nBind);
	/* the list is built by prepending, so walking it backwards is what puts
	 * the bindings back in the order the script made them */
	{
		phl_pdo_bind *apBind[64];
		int n = 0,i;
		for( pB = pSt->pBinds ; pB && n < (int)SX_ARRAYSIZE(apBind) ; pB = pB->pNext ){
			apBind[n++] = pB;
		}
		for( i = n - 1 ; i >= 0 ; --i ){
			pB = apBind[i];
			if( pB->zName ){
				ph7_context_output_format(pCtx,"Key: Name: [%d] %.*s\n",
					pB->nName,pB->nName,pB->zName);
				ph7_context_output_format(pCtx,"paramno=-1\n");
				ph7_context_output_format(pCtx,"name=[%d] \"%.*s\"\n",
					pB->nName,pB->nName,pB->zName);
			}else{
				ph7_context_output_format(pCtx,"Key: Position #%d:\n",pB->iPos - 1);
				ph7_context_output_format(pCtx,"paramno=%d\n",pB->iPos - 1);
				ph7_context_output_format(pCtx,"name=[0] \"\"\n");
			}
			ph7_context_output_format(pCtx,"is_param=1\n");
			ph7_context_output_format(pCtx,"param_type=%d\n",pB->iType & ~PDO_PARAM_FLAGS);
		}
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * PDOStatement::getAttribute(int $name): mixed
 *
 * Two of the driver's attributes describe a STATEMENT rather than the
 * connection -- whether it only reads, and whether it is mid-walk -- and both
 * are sqlite's own answers about the compiled statement. Everything else is
 * the same IM001 refusal the connection gives.
 */
static int vm_builtin_PDOStatement_getAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	ph7_int64 iAttr;
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	if( iAttr == PDO_SQLITE_ATTR_READONLY_STATEMENT ){
		ph7_result_bool(pCtx,PH7_PdoSqliteStmtReadonly(pSt));
		return PH7_OK;
	}
	if( iAttr == PDO_SQLITE_ATTR_BUSY_STATEMENT ){
		ph7_result_bool(pCtx,PH7_PdoSqliteStmtBusy(pSt));
		return PH7_OK;
	}
	ph7_result_null(pCtx);
	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::getAttribute","IM001",
		"driver does not support that attribute");
}
/*
 * PDOStatement::setAttribute(int $attribute, mixed $value): bool
 *
 * This driver carries no SETTABLE statement attribute at all, so every one of
 * them is the same IM001 refusal.
 */
static int vm_builtin_PDOStatement_setAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	ph7_result_bool(pCtx,0);
	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::setAttribute","IM001",
		"driver does not support that attribute");
}
/*
 * PDOStatement::rowCount(): int
 *
 * The number of rows a WRITE changed. It is not the size of a result set --
 * sqlite cannot know that without walking it -- so a SELECT answers 0, which
 * is php's answer and the reason its manual warns against this method.
 */
static int vm_builtin_PDOStatement_rowCount(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	ph7_result_int64(pCtx,pSt->nChanges);
	return PH7_OK;
}
/*
 * PDOStatement::closeCursor(): bool
 *
 * Frees the rows a statement is still holding without discarding the statement
 * itself: php answers true and leaves the object reusable, and a fetch after
 * it answers false.
 */
static int vm_builtin_PDOStatement_closeCursor(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	if( pSt->pStmt ){
		sqlite3_reset(pSt->pStmt);
	}
	pSt->bRowPending = 0;
	pSt->bDone = 1;
	/* php frees the row's columns with the cursor, so a lazy object still in a
	 * variable answers null from here on. */
	PdoStmtLazyClear(pSt);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* A statement reports its CONNECTION's error state; php keeps one per
 * statement, and every path that sets one sets both. */
static int vm_builtin_PDOStatement_errorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	if( pSt->iErrState == PDO_ERR_NONE ){
		ph7_result_null(pCtx);
	}else{
		ph7_result_string(pCtx,pSt->zSqlState,(int)SyStrlen(pSt->zSqlState));
	}
	return PH7_OK;
}
static int vm_builtin_PDOStatement_errorInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	return PdoBuildErrorInfo(pCtx,pSt->pConn,pSt->iErrState,pSt->zSqlState);
}
/*
 * The InternalIterator a foreach over a statement walks.  A statement is a
 * forward cursor, so REWIND does not rewind: it settles on whatever row is
 * pending, which is why a second foreach over the same statement walks nothing
 * at all rather than repeating the set.
 */
static void PdoStmtIterSettle(ph7_vm *pVm,ph7_class_instance *pIt)
{
	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	phl_pdo_stmt *pSt = PdoStmtOfInstance(pSrc);
	ph7_value *pRow;
	if( pSt == 0 || !PdoStmtHasRow(pSt) ){
		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
		return;
	}
	if( !PdoBoundColumnsForIterRow(pVm,pSt) ){
		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
		return;
	}
	{
		/* A foreach honours the statement's mode, the whole of it: php walks a
		 * LAZY statement with its one row object, a CLASS or INTO one with the
		 * objects those modes build, a COLUMN one with that column's value and
		 * a BOUND one with `true` per row (the values having gone to the bound
		 * variables). Only the four row SHAPES are what PdoStmtRow answers. */
		int iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;
		ph7_value sCur;
		int bHave = 0;
		PH7_MemObjInit(pVm,&sCur);
		if( iBase == PDO_FETCH_LAZY ){
			ph7_class_instance *pLazy = PdoLazyRowFor(pVm,pSt);
			if( pLazy ){
				sCur.x.pOther = pLazy;
				MemObjSetType(&sCur,MEMOBJ_OBJ);
				bHave = 1;   /* the reference PdoLazyRowFor took is this value's */
			}
		}else if( iBase == PDO_FETCH_BOUND ){
			/* the row IS the bound variables: nothing else reads it, so the
			 * write happens here rather than inside a row build */
			PdoBoundColumnsForRow(pVm,pSt);
			pSt->bRowPending = 0;
			ph7_value_bool(&sCur,1);
			bHave = 1;
		}else if( iBase == PDO_FETCH_COLUMN ){
			ph7_value *pNumRow = ph7_new_array(pVm);
			if( pNumRow ){
				if( PdoStmtRowFrom(pVm,pSt,PDO_FETCH_NUM,pNumRow,0) ){
					ph7_value *pOne = PdoArrayAtInt(pVm,pNumRow,(sxi64)pSt->iFetchColumn);
					if( pOne ){
						PH7_MemObjStore(pOne,&sCur);
					}
					bHave = 1;
				}
				ph7_release_value(pVm,pNumRow);
			}
		}else if( iBase == PDO_FETCH_CLASS || iBase == PDO_FETCH_INTO ){
			ph7_class *pClass = 0;
			int iFirst = 0;
			if( iBase == PDO_FETCH_INTO ){
				if( pSt->pFetchInto ){
					ph7_value *pRowVals = ph7_new_array(pVm);
					if( pRowVals && PdoStmtRowFrom(pVm,pSt,PDO_FETCH_ASSOC,pRowVals,0) ){
						PdoWriteRowProps(pVm,pSt->pFetchInto,pRowVals);
						pSt->pFetchInto->iRef++;
						sCur.x.pOther = pSt->pFetchInto;
						MemObjSetType(&sCur,MEMOBJ_OBJ);
						bHave = 1;
					}
					if( pRowVals ){
						ph7_release_value(pVm,pRowVals);
					}
				}
			}else{
				if( pSt->iFetchMode & PDO_FETCH_CLASSTYPE ){
					/* the FIRST column names the class and leaves the row */
					ph7_value *pHead = ph7_new_array(pVm);
					if( pHead && PdoStmtRowFrom(pVm,pSt,PDO_FETCH_NUM,pHead,0) ){
						pSt->bRowPending = 1;   /* the cursor has not moved */
						pClass = PdoIterClassOf(pVm,PdoArrayAtInt(pVm,pHead,0));
						iFirst = 1;
					}
					if( pHead ){
						ph7_release_value(pVm,pHead);
					}
				}else if( pSt->zFetchClass ){
					pClass = PH7_VmExtractClass(pVm,pSt->zFetchClass,
						(sxu32)pSt->nFetchClass,FALSE,0);
				}
				if( pClass && PdoRowIntoObject(pVm,pSt,pClass,pSt->pFetchArgs,
						(pSt->iFetchMode & PDO_FETCH_PROPS_LATE) != 0,iFirst,&sCur) ){
					bHave = 1;
				}
			}
		}
		if( bHave ){
			PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,
				(int)SyStrlen(PH7_NATIVE_IT_CUR),&sCur);
			PH7_MemObjRelease(&sCur);
			PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,
				PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));
			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);
			PdoStmtStep(pSt);
			return;
		}
		PH7_MemObjRelease(&sCur);
		if( iBase != PDO_FETCH_ASSOC && iBase != PDO_FETCH_NUM && iBase != PDO_FETCH_BOTH
		 && iBase != PDO_FETCH_OBJ && iBase != PDO_FETCH_NAMED ){
			/* a mode with nothing to hand out ends the walk */
			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
			return;
		}
	}
	pRow = ph7_new_array(pVm);
	if( pRow == 0 || !PdoStmtRow(pVm,pSt,pSt->iFetchMode,pRow) ){
		if( pRow ){
			ph7_release_value(pVm,pRow);
		}
		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
		return;
	}
	PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,(int)SyStrlen(PH7_NATIVE_IT_CUR),pRow);
	ph7_release_value(pVm,pRow);
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,
		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));
	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);
	PdoStmtStep(pSt);
}
/*
 * A rewind that does NOT rewind, and must not even re-read: the iterator is
 * built already positioned and `foreach` rewinds it again, so a settle here
 * would swallow the first row. The AUX slot records that the first row has
 * been taken; every later rewind is a no-op, which is also what makes a SECOND
 * foreach over the same statement walk nothing at all -- php's answer, because
 * the cursor is forward-only and has nowhere to go back to.
 */
static void PdoStmtIterRewind(ph7_vm *pVm,ph7_class_instance *pIt)
{
	if( PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_AUX) != 0 ){
		return;
	}
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_AUX,1);
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);
	PdoStmtIterSettle(&(*pVm),pIt);
}
static void PdoStmtIterNext(ph7_vm *pVm,ph7_class_instance *pIt)
{
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,
		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);
	PdoStmtIterSettle(&(*pVm),pIt);
}
static const PH7_NativeIterVtab sPdoStmtIterVtab = { PdoStmtIterRewind, PdoStmtIterNext, 0, 0 };
static int vm_builtin_PDOStatement_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pIt;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement::getIterator() needs a receiver");
	}
	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);
	if( pIt == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pIt);
	return PH7_OK;
}
/*
 * Record one binding.  A name is kept as the script spelled it -- with or
 * without its colon -- because the resolution happens at execute(), when the
 * statement that knows the names exists.
 */
static phl_pdo_bind * PdoBindAdd(phl_pdo_stmt *pSt,const char *zName,int nName,int iPos,
	int iType)
{
	ph7_vm *pVm = pSt->pConn->pVm;
	phl_pdo_bind *pB;
	/* php REPLACES a binding for the same parameter rather than stacking one */
	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){
		if( zName ? (pB->zName && pB->nName == nName
		             && SyMemcmp(pB->zName,zName,(sxu32)nName) == 0)
		          : (pB->zName == 0 && pB->iPos == iPos) ){
			if( pB->pVal ){
				ph7_release_value(pVm,pB->pVal);
				pB->pVal = 0;
			}
			pB->iType = iType;
			pB->nSlot = SXU32_HIGH;
			return pB;
		}
	}
	pB = (phl_pdo_bind *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo_bind));
	if( pB == 0 ){
		return 0;
	}
	SyZero(pB,sizeof(phl_pdo_bind));
	pB->iPos = iPos;
	pB->iType = iType;
	pB->nSlot = SXU32_HIGH;
	if( zName && nName > 0 ){
		pB->zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nName + 1);
		if( pB->zName == 0 ){
			SyMemBackendFree(&pVm->sAllocator,pB);
			return 0;
		}
		SyMemcpy(zName,pB->zName,(sxu32)nName);
		pB->zName[nName] = 0;
		pB->nName = nName;
	}
	pB->pNext = pSt->pBinds;
	pSt->pBinds = pB;
	return pB;
}
/*
 * The shared body of bindValue() and bindParam(): they differ only in WHEN the
 * value is read. Argument #1 is a name or a 1-based position, and php refuses
 * position 0 by ValueError before the statement is consulted at all.
 */
static int PdoBindArgument(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFn,
	int bByRef)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	phl_pdo_bind *pB;
	ph7_value *pKey;
	int iType;
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	pKey = nArg > 0 ? apArg[0] : 0;
	iType = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : PDO_PARAM_STR;
	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){
		int nName = 0;
		const char *zName = ph7_value_to_string(pKey,&nName);
		pB = PdoBindAdd(pSt,zName,nName,0,iType);
	}else{
		ph7_int64 iPos = pKey ? ph7_value_to_int64(pKey) : 0;
		if( iPos < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #1 ($param) must be greater than or equal to 1",zFn);
		}
		pB = PdoBindAdd(pSt,0,0,(int)iPos,iType);
	}
	if( pB == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( bByRef ){
		/* bindParam(): remember the caller's SLOT, so a write to that variable
		 * after this call is the value execute() runs with. The engine hands a
		 * by-reference argument as the caller's own memobj, and its index is
		 * how every other deferred read here finds it again. */
		pB->nSlot = (nArg > 1 && apArg[1]) ? apArg[1]->nIdx : SXU32_HIGH;
	}else if( nArg > 1 ){
		/* bindValue(): the statement takes its own copy now */
		pB->pVal = ph7_new_scalar(pCtx->pVm);
		if( pB->pVal == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		PH7_MemObjStore(apArg[1],pB->pVal);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_PDOStatement_bindValue(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return PdoBindArgument(pCtx,nArg,apArg,"PDOStatement::bindValue",FALSE);
}
static int vm_builtin_PDOStatement_bindParam(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return PdoBindArgument(pCtx,nArg,apArg,"PDOStatement::bindParam",TRUE);
}
/* Bind one recorded parameter, resolving a name against the live statement. */
static int PdoBindApply(ph7_vm *pVm,phl_pdo_stmt *pSt,phl_pdo_bind *pB)
{
	ph7_value *pVal = pB->pVal;
	int iPos = pB->iPos;
	if( pB->zName ){
		iPos = PH7_PdoSqliteBindIndexOf(pSt,pB->zName,pB->nName);
	}
	if( pB->nSlot != SXU32_HIGH ){
		/* bindParam(): read the caller's variable NOW */
		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pB->nSlot);
	}
	return PH7_PdoSqliteBindAt(pSt,iPos,pB->iType,pVal);
}
/*
 * execute()'s `?array $params`: php binds the array INSTEAD of whatever was
 * recorded, an integer key naming a 1-based position (so element 0 is
 * parameter 1) and a string key naming a placeholder.
 */
static int PdoBindFromArray(ph7_vm *pVm,phl_pdo_stmt *pSt,ph7_value *pArray)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	sxu32 n,nCount;
	int rc = 1;
	if( pArray == 0 || (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 1;
	}
	pMap = (ph7_hashmap *)pArray->x.pOther;
	nCount = pMap->nEntry;
	pEntry = pMap->pFirst;
	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){
		ph7_value sKey;
		ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);
		int iPos;
		PH7_MemObjInit(pVm,&sKey);
		PH7_HashmapExtractNodeKey(pEntry,&sKey);
		if( pEntry->iType == HASHMAP_INT_NODE ){
			iPos = (int)sKey.x.iVal + 1;
		}else{
			int nName = 0;
			const char *zName = ph7_value_to_string(&sKey,&nName);
			iPos = PH7_PdoSqliteBindIndexOf(pSt,zName,nName);
		}
		PH7_MemObjRelease(&sKey);
		/* php binds every element as a STRING unless the script said otherwise
		 * through bindValue(); a php null still binds as NULL. */
		if( !PH7_PdoSqliteBindAt(pSt,iPos,PDO_PARAM_STR,pVal) ){
			rc = 0;
			break;
		}
	}
	return rc;
}
/*
 * PDOStatement::execute(?array $params = null): bool
 *
 * Runs the statement from the start: the cursor is rewound, the previous run's
 * values are dropped, the parameters are bound and one step is taken -- the
 * same first step query() takes, so columnCount() and the first fetch() behave
 * identically whichever verb produced the statement.
 */
static int vm_builtin_PDOStatement_execute(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));
	int bOk = 1;
	if( pSt == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");
	}
	PH7_PdoTouch(pSt->pConn);
	PH7_PdoSqliteReset(pSt);
	pSt->bRowPending = 0;
	pSt->bDone = 0;
	if( nArg > 0 && apArg[0] && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){
		bOk = PdoBindFromArray(pCtx->pVm,pSt,apArg[0]);
	}else{
		phl_pdo_bind *pB;
		for( pB = pSt->pBinds ; pB && bOk ; pB = pB->pNext ){
			bOk = PdoBindApply(pCtx->pVm,pSt,pB);
		}
	}
	if( !bOk ){
		ph7_result_bool(pCtx,0);
		PdoStmtFailed(pSt,pSt->pConn->zSqlState);
		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::execute");
	}
	pSt->bExecuted = 1;
	if( PdoStmtStep(pSt) < 0 ){
		ph7_result_bool(pCtx,0);
		PdoStmtFailed(pSt,pSt->pConn->zSqlState);
		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::execute");
	}
	PdoStmtOk(pSt);
	pSt->nChanges = PH7_PdoSqliteColumnCount(pSt) > 0
		? 0 : PH7_PdoSqliteChanges(pSt->pConn);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * The class query()/prepare() builds.  ATTR_STATEMENT_CLASS replaces
 * PDOStatement with a subclass of the script's own, and php builds THAT for
 * every statement the connection makes from then on.
 */
static ph7_class * PdoStatementClass(ph7_context *pCtx,phl_pdo *pConn)
{
	if( pConn->zStmtClass ){
		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,pConn->zStmtClass,
			(sxu32)pConn->nStmtClass,FALSE,0);
		if( pClass ){
			return pClass;
		}
	}
	return PH7_VmExtractClass(pCtx->pVm,"PDOStatement",sizeof("PDOStatement")-1,FALSE,0);
}
/*
 * PDO::prepare(string $query, array $options = []): PDOStatement|false
 *
 * Compiles without running. The options array is php's per-statement
 * attribute set; the sqlite driver carries none of the ones a script can put
 * there, and php ignores an unusable one rather than refusing the call.
 */
static int vm_builtin_PDO_prepare(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	phl_pdo_stmt *pSt;
	ph7_class *pClass;
	ph7_class_instance *pObj;
	const char *zSql;
	int nSql = 0;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	PH7_PdoTouch(pConn);
	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;
	if( zSql == 0 || nSql < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"PDO::prepare(): Argument #1 ($query) must not be empty");
	}
	pSt = PH7_PdoNewStmt(pConn);
	if( pSt == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !PH7_PdoSqlitePrepare(pSt,zSql,nSql) ){
		ph7_result_bool(pCtx,0);
		return PH7_PdoRaise(pCtx,pConn,"PDO::prepare");
	}
	pClass = PdoStatementClass(pCtx,pConn);
	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( PdoStmtAttach(pObj,pSt) != 0 ){
		PH7_ClassInstanceUnref(pObj);
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeSetAttrStr(pCtx->pVm,pObj,"queryString",zSql,nSql);
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/*
 * PDO::quote(string $string, int $type = PDO::PARAM_STR): string|false
 *
 * sqlite's own quoting: single quotes around it, each embedded quote doubled.
 * A NUL byte has no spelling inside a sqlite literal at all, so php refuses
 * one -- with a bare sentence carrying no SQLSTATE, unlike every other
 * PDOException this driver raises.
 */
static int vm_builtin_PDO_quote(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	const char *zIn;
	int nIn = 0,i;
	SyBlob sOut;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	PH7_PdoTouch(pConn);
	zIn = nArg > 0 ? ph7_value_to_string(apArg[0],&nIn) : "";
	for( i = 0 ; i < nIn ; ++i ){
		if( zIn[i] == 0 ){
			return PH7_VmThrowException(pCtx,"PDOException",
				"SQLite PDO::quote does not support null bytes");
		}
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sOut,"'",1);
	for( i = 0 ; i < nIn ; ++i ){
		if( zIn[i] == '\'' ){
			SyBlobAppend(&sOut,"'",1);
		}
		SyBlobAppend(&sOut,&zIn[i],1);
	}
	SyBlobAppend(&sOut,"'",1);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/*
 * PDO::query(string $query, ...): PDOStatement|false
 *
 * Prepares and runs ONE statement -- what follows a `;` is compiled but never
 * executed, unlike exec(), which runs them all.
 */
static int vm_builtin_PDO_query(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	phl_pdo_stmt *pSt;
	ph7_class *pClass;
	ph7_class_instance *pObj;
	const char *zSql;
	int nSql = 0;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	PH7_PdoTouch(pConn);
	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;
	if( zSql == 0 || nSql < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"PDO::query(): Argument #1 ($query) must not be empty");
	}
	pSt = PH7_PdoNewStmt(pConn);
	if( pSt == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !PH7_PdoSqlitePrepare(pSt,zSql,nSql) ){
		ph7_result_bool(pCtx,0);
		return PH7_PdoRaise(pCtx,pConn,"PDO::query");
	}
	pSt->bExecuted = 1;
	if( PdoStmtStep(pSt) < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_PdoRaise(pCtx,pConn,"PDO::query");
	}
	/* the change count is read once, here: a later statement on the same
	 * connection would otherwise move what this one reports */
	pSt->nChanges = PH7_PdoSqliteColumnCount(pSt) > 0
		? 0 : PH7_PdoSqliteChanges(pConn);
	pClass = PdoStatementClass(pCtx,pConn);
	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( PdoStmtAttach(pObj,pSt) != 0 ){
		PH7_ClassInstanceUnref(pObj);
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeSetAttrStr(pCtx->pVm,pObj,"queryString",zSql,nSql);
	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){
		/* php's second argument IS setFetchMode(), run on the statement this
		 * call just built -- same screen, same per-mode arity, and diagnostics
		 * that count from PDO::query()'s own signature. A refusal leaves the
		 * statement behind (php's does too), so it is raised after the object
		 * exists rather than before the query runs. */
		sxi32 rcMode = PdoSetupFetchMode(pCtx,pSt,nArg,apArg,2,"PDO::query","fetchMode");
		if( rcMode != PH7_OK ){
			PH7_ClassInstanceUnref(pObj);
			return rcMode;
		}
	}
	PdoStmtOk(pSt);   /* it ran, and it ran cleanly */
	/* PH7_NativeResultObject takes the reference this call made: unref'ing
	 * again here frees the object the result slot is still holding. */
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Transactions
 * ------------------------------------------------------------------------ */
/*
 * PDO::beginTransaction(): bool / commit() / rollBack() / inTransaction()
 *
 * Whether a transaction is open is sqlite's own autocommit flag and not a
 * count this driver keeps, so a BEGIN the script sent through exec() is
 * indistinguishable from beginTransaction() -- inTransaction() answers true
 * for it and a second beginTransaction() refuses.
 *
 * The three refusals are bare sentences with no SQLSTATE in front of them,
 * which is unlike every other PDOException the driver raises; and the four
 * verbs are the ones that do NOT clear the handle's error on entry.
 */
static int PdoTxRun(ph7_context *pCtx,const char *zSql,const char *zFn)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	if( PH7_PdoSqliteExec(pConn,zSql,(int)SyStrlen(zSql)) < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_PdoRaise(pCtx,pConn,zFn);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_PDO_beginTransaction(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	const char *zBegin = "BEGIN";
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	if( PH7_PdoSqliteInTransaction(pConn) ){
		return PH7_VmThrowException(pCtx,"PDOException",
			"There is already an active transaction");
	}
	/* which BEGIN, per Pdo\Sqlite::ATTR_TRANSACTION_MODE */
	if( pConn->iTxMode == 1 ){
		zBegin = "BEGIN IMMEDIATE";
	}else if( pConn->iTxMode == 2 ){
		zBegin = "BEGIN EXCLUSIVE";
	}
	return PdoTxRun(pCtx,zBegin,"PDO::beginTransaction");
}
static int vm_builtin_PDO_commit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	if( !PH7_PdoSqliteInTransaction(pConn) ){
		return PH7_VmThrowException(pCtx,"PDOException","There is no active transaction");
	}
	return PdoTxRun(pCtx,"COMMIT","PDO::commit");
}
static int vm_builtin_PDO_rollBack(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	if( !PH7_PdoSqliteInTransaction(pConn) ){
		return PH7_VmThrowException(pCtx,"PDOException","There is no active transaction");
	}
	return PdoTxRun(pCtx,"ROLLBACK","PDO::rollBack");
}
static int vm_builtin_PDO_inTransaction(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	ph7_result_bool(pCtx,PH7_PdoSqliteInTransaction(pConn));
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Connecting
 * ------------------------------------------------------------------------ */
/*
 * Apply the constructor's `?array $options`.  php walks it before the driver
 * sees the handle, so an ATTR_ERRMODE in there is already in force when a
 * later failure is routed -- and a key no driver knows is ignored in silence.
 */
static void PdoApplyOptions(phl_pdo *pConn,ph7_value *pOptions)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	sxu32 n,nCount;
	if( pOptions == 0 || (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return;
	}
	pMap = (ph7_hashmap *)pOptions->x.pOther;
	nCount = pMap->nEntry;
	pEntry = pMap->pFirst;
	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){
		ph7_value sKey;
		ph7_value *pVal;
		ph7_int64 iKey;
		if( pEntry->iType != HASHMAP_INT_NODE ){
			continue;  /* php ignores a string key here */
		}
		PH7_MemObjInit(pConn->pVm,&sKey);
		PH7_HashmapExtractNodeKey(pEntry,&sKey);
		iKey = sKey.x.iVal;
		PH7_MemObjRelease(&sKey);
		pVal = (ph7_value *)SySetAt(&pConn->pVm->aMemObj,pEntry->nValIdx);
		if( pVal == 0 ){
			continue;
		}
		switch( iKey ){
			case PDO_ATTR_ERRMODE:            pConn->iErrMode = (int)ph7_value_to_int64(pVal); break;
			case PDO_ATTR_CASE:               pConn->iCase = (int)ph7_value_to_int64(pVal); break;
			case PDO_ATTR_ORACLE_NULLS:       pConn->iOracleNulls = (int)ph7_value_to_int64(pVal); break;
			case PDO_ATTR_DEFAULT_FETCH_MODE: pConn->iDefaultFetch = (int)ph7_value_to_int64(pVal); break;
			case PDO_ATTR_STRINGIFY_FETCHES:  pConn->bStringify = ph7_value_to_bool(pVal); break;
			/* php remembers this one and reports it back, though a CLI process
			 * has no pool to keep the handle in. */
			case PDO_ATTR_PERSISTENT:         pConn->bPersistent = ph7_value_to_bool(pVal); break;
			case PDO_SQLITE_ATTR_TRANSACTION_MODE: pConn->iTxMode = (int)ph7_value_to_int64(pVal); break;
			/* the OPEN flags are read here and used by the open itself, which is
			 * the only moment they mean anything */
			case PDO_SQLITE_ATTR_OPEN_FLAGS: pConn->iOpenFlags = (int)ph7_value_to_int64(pVal); break;
			case PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES: pConn->bExtendedCodes = ph7_value_to_bool(pVal); break;
			default: break;
		}
	}
}
/*
 * php's `uri:` DSN: the real DSN is the FIRST LINE of what that URI names, read
 * through the ordinary stream layer -- so a php:// wrapper or a userland one is
 * a source too. The line is used verbatim, which is why one with leading
 * whitespace, or a second `uri:`, comes back as "could not find driver" rather
 * than anything more specific: it is simply parsed as a driver name.
 *
 * Returns 0 when the URI could not be read (the caller words the refusal); the
 * open warning underneath it is the stream layer's own, attributed to
 * PDO::__construct the way php attributes it.
 */
static int PdoResolveUriDsn(ph7_context *pCtx,const char *zUri,int nUri,SyBlob *pOut)
{
	const ph7_io_stream *pStream;
	const char *zFile,*zWhole;
	void *pHandle;
	SyBlob sRaw;
	const char *zRaw;
	sxu32 n,nRaw;
	int rc = 0;
	if( nUri < 1 ){
		return 0;
	}
	/* the slice is not a C string, and the stream layer wants one */
	SyBlobInit(&sRaw,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sRaw,zUri,(sxu32)nUri);
	SyBlobAppend(&sRaw,"",1);
	zFile = zWhole = (const char *)SyBlobData(&sRaw);
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nUri);
	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,
		FALSE,0,FALSE,0,"PDO::__construct") : 0;
	if( pHandle == 0 ){
		/* the device lookup advanced zFile past the wrapper prefix; php's warning
		 * names the URI the SCRIPT wrote, `file:///nope` and not `/nope` */
		PH7_VmThrowWarningFmt(pCtx->pVm,"PDO::__construct(%s): Failed to open stream: %s",
			zWhole,"No such file or directory");
		SyBlobRelease(&sRaw);
		return 0;
	}
	SyBlobReset(&sRaw);
	if( PH7_StreamReadWholeFile(pHandle,pStream,&sRaw) == SXRET_OK ){
		zRaw = (const char *)SyBlobData(&sRaw);
		nRaw = SyBlobLength(&sRaw);
		/* php reads ONE line and keeps its terminator, so a file written with a
		 * trailing newline yields a DSN that ends in one -- which reaches sqlite
		 * as part of the PATH. That is why `sqlite::memory:\n` opens a FILE of
		 * that name rather than a memory database; the newline is not noise the
		 * driver trims, and trimming it here would answer differently. */
		for( n = 0 ; n < nRaw && zRaw[n] != '\n' ; ++n ){}
		if( n < nRaw ){
			++n;  /* the newline belongs to the line */
		}
		if( n > 0 ){
			SyBlobAppend(pOut,zRaw,n);
			rc = 1;
		}
	}
	PH7_StreamCloseHandle(pStream,pHandle);
	SyBlobRelease(&sRaw);
	return rc;
}
/*
 * The driver split, over a DSN that is already resolved.  php reads up to the
 * first `:` as the driver name and hands the rest to that driver; the name is
 * matched case-SENSITIVELY, so `SQLITE:` is "could not find driver" rather
 * than a connection, and a DSN with no colon at all is refused before any
 * driver is looked for.
 */
static int PdoOpenParsed(ph7_context *pCtx,phl_pdo *pConn,const char *zDsn,int nDsn)
{
	int nDriver;
	for( nDriver = 0 ; nDriver < nDsn && zDsn[nDriver] != ':' ; ++nDriver ){}
	if( nDriver >= nDsn ){
		/* no colon: php refuses the ARGUMENT, not the driver */
		return PH7_VmThrowException(pCtx,"PDOException",
			"PDO::__construct(): Argument #1 ($dsn) must be a valid data source name");
	}
	if( nDriver != (int)sizeof("sqlite")-1
	 || SyMemcmp(zDsn,"sqlite",sizeof("sqlite")-1) != 0 ){
		/* §10 scopes this build to one driver, so every other name -- and every
		 * other SPELLING of this one -- is what a php without that driver says. */
		return PH7_VmThrowException(pCtx,"PDOException","could not find driver");
	}
	/* SQLITE_OPEN_URI is passed EXPLICITLY rather than left to the linked
	 * library's compile-time default: a Debian libsqlite3 is built with URI
	 * filenames on and a vcpkg one is not, so `sqlite:file::memory:?cache=shared`
	 * opened a memory database on one platform and created a FILE of that name on
	 * the other. php's own sqlite has them on, so on is the answer everywhere. */
	{
		/* the script's own ATTR_OPEN_FLAGS replace the read-write default */
		int iFlags = pConn->iOpenFlags
			? pConn->iOpenFlags
			: (SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
		return PH7_PdoSqliteOpen(pCtx,pConn,zDsn + nDriver + 1,nDsn - nDriver - 1,
			iFlags|SQLITE_OPEN_URI);
	}
}
/*
 * One DSN, resolved then split: php's `uri:` form is read first, and what it
 * names replaces the DSN whole.
 */
static int PdoOpenFromDsn(ph7_context *pCtx,phl_pdo *pConn,ph7_value *pDsn)
{
	const char *zDsn;
	int nDsn,rc;
	SyBlob sResolved;
	if( pDsn == 0 ){
		nDsn = 0;
		zDsn = "";
	}else{
		zDsn = ph7_value_to_string(pDsn,&nDsn);
	}
	SyBlobInit(&sResolved,&pCtx->pVm->sAllocator);
	if( nDsn >= (int)sizeof("uri:")-1 && SyMemcmp(zDsn,"uri:",sizeof("uri:")-1) == 0 ){
		if( !PdoResolveUriDsn(pCtx,zDsn + sizeof("uri:")-1,nDsn - ((int)sizeof("uri:")-1),
			&sResolved) ){
			SyBlobRelease(&sResolved);
			return PH7_VmThrowException(pCtx,"PDOException",
				"PDO::__construct(): Argument #1 ($dsn) must be a valid data source URI");
		}
		/* the line replaces the DSN whole, and is NOT resolved again: a nested
		 * `uri:` is read as a driver name, exactly as php reads it */
		zDsn = (const char *)SyBlobData(&sResolved);
		nDsn = (int)SyBlobLength(&sResolved);
	}
	rc = PdoOpenParsed(pCtx,pConn,zDsn,nDsn);
	SyBlobRelease(&sResolved);
	return rc;
}
/*
 * PDO::__construct(string $dsn, ?string $username = null, ?string $password = null,
 *                  ?array $options = null)
 *
 * The two credential arguments are the generic surface: sqlite has no user to
 * be, so php accepts and ignores them rather than refusing a portable call.
 */
static int vm_builtin_PDO___construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_pdo *pConn;
	int rc;
	if( pThis == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO::__construct() needs a receiver");
	}
	pConn = PH7_PdoNewConn(pCtx->pVm);
	if( pConn == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( PdoAttach(pThis,pConn) != 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* the options are read BEFORE the open, so an ATTR_ERRMODE they carry is
	 * already in force for everything that follows */
	if( nArg > 3 ){
		PdoApplyOptions(pConn,apArg[3]);
	}
	rc = PdoOpenFromDsn(pCtx,pConn,nArg > 0 ? apArg[0] : 0);
	return rc;
}
/*
 * static PDO::connect(string $dsn, ...): static
 *
 * php 8.4's replacement for `new PDO(...)`: same arguments, but the object it
 * answers is the DRIVER's subclass -- `Pdo\Sqlite` here -- so the
 * sqlite-specific verbs are callable on it without a cast. Called on a
 * subclass it answers that subclass, which is what `static` means.
 */
static int vm_builtin_PDO_connect(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_ContextCalledClass(pCtx);
	ph7_class_instance *pObj;
	phl_pdo *pConn;
	int rc;
	if( pClass == 0 || SyStrncmp(pClass->sName.zString,"PDO",sizeof("PDO")-1) == 0 ){
		/* PDO::connect() itself answers the driver's class, not PDO */
		ph7_class *pDrv = PH7_VmExtractClass(&(*pVm),"Pdo\\Sqlite",
			sizeof("Pdo\\Sqlite")-1,FALSE,0);
		if( pDrv ){
			pClass = pDrv;
		}
	}
	if( pClass == 0 ){
		return PH7_VmThrowException(pCtx,"Error","Pdo\\Sqlite is not available");
	}
	pObj = PH7_NewClassInstance(&(*pVm),pClass);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pConn = PH7_PdoNewConn(&(*pVm));
	if( pConn == 0 || PdoAttach(pObj,pConn) != 0 ){
		PH7_ClassInstanceUnref(pObj);
		return PH7_ContextMemoryError(pCtx);
	}
	if( nArg > 3 ){
		PdoApplyOptions(pConn,apArg[3]);
	}
	rc = PdoOpenFromDsn(pCtx,pConn,nArg > 0 ? apArg[0] : 0);
	if( rc != PH7_OK ){
		PH7_ClassInstanceUnref(pObj);
		return rc;
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/*
 * The loaded-driver list, the one answer php's two spellings share.  php
 * answers the drivers its ext/pdo actually loaded, which is why an engine
 * with no driver at all answers [] -- here it is always ["sqlite"].
 */
static int PdoDriverList(ph7_context *pCtx)
{
	ph7_value *pArray, *pName;
	pArray = ph7_context_new_array(pCtx);
	pName  = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pName == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_value_string(pName,"sqlite",sizeof("sqlite")-1);
	ph7_array_add_elem(pArray,0,pName);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* PDO::getAvailableDrivers(): the static method spelling. */
static int vm_builtin_PDO_getAvailableDrivers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PdoDriverList(pCtx);
}
/*
 * pdo_drivers(): the PROCEDURAL spelling of the same list, and the only
 * FUNCTION ext/pdo declares.  php's two answers are the same array built by
 * the same C routine, so they are `===` to each other.
 */
static int vm_builtin_pdo_drivers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PdoDriverList(pCtx);
}

/*
 * Install the PDO class library.  Called from PH7_VmInit inside the
 * bCompilingBuiltin window; vm_pdo_sqlite.c's installer runs right after and
 * needs PDO to already be mounted (it is the parent of `Pdo\Sqlite`).
 */
PH7_PRIVATE sxi32 PH7_VmInstallPdo(ph7_vm *pVm)
{
	/* php's own constant values, in its own declaration order. The seven
	 * deprecated PDO::SQLITE_* rows php still carries are absent by §10; their
	 * successors are declared on Pdo\Sqlite (vm_pdo_sqlite.c). */
#define PDO_INT_CONST(NAME,VALUE) \
	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (ph7_int64)(VALUE), 0, 0.0 }
	static const PH7_NativeConstDef aPdoConst[] = {
		PDO_INT_CONST("PARAM_NULL",             0),
		PDO_INT_CONST("PARAM_BOOL",             5),
		PDO_INT_CONST("PARAM_INT",              1),
		PDO_INT_CONST("PARAM_STR",              2),
		PDO_INT_CONST("PARAM_LOB",              3),
		PDO_INT_CONST("PARAM_STMT",             4),
		PDO_INT_CONST("PARAM_INPUT_OUTPUT",     2147483648LL),
		PDO_INT_CONST("PARAM_STR_NATL",         1073741824LL),
		PDO_INT_CONST("PARAM_STR_CHAR",         536870912LL),
		PDO_INT_CONST("PARAM_EVT_ALLOC",        0),
		PDO_INT_CONST("PARAM_EVT_FREE",         1),
		PDO_INT_CONST("PARAM_EVT_EXEC_PRE",     2),
		PDO_INT_CONST("PARAM_EVT_EXEC_POST",    3),
		PDO_INT_CONST("PARAM_EVT_FETCH_PRE",    4),
		PDO_INT_CONST("PARAM_EVT_FETCH_POST",   5),
		PDO_INT_CONST("PARAM_EVT_NORMALIZE",    6),
		PDO_INT_CONST("FETCH_DEFAULT",          0),
		PDO_INT_CONST("FETCH_LAZY",             1),
		PDO_INT_CONST("FETCH_ASSOC",            2),
		PDO_INT_CONST("FETCH_NUM",              3),
		PDO_INT_CONST("FETCH_BOTH",             4),
		PDO_INT_CONST("FETCH_OBJ",              5),
		PDO_INT_CONST("FETCH_BOUND",            6),
		PDO_INT_CONST("FETCH_COLUMN",           7),
		PDO_INT_CONST("FETCH_CLASS",            8),
		PDO_INT_CONST("FETCH_INTO",             9),
		PDO_INT_CONST("FETCH_FUNC",            10),
		PDO_INT_CONST("FETCH_GROUP",           32),
		PDO_INT_CONST("FETCH_UNIQUE",          64),
		PDO_INT_CONST("FETCH_KEY_PAIR",        12),
		PDO_INT_CONST("FETCH_CLASSTYPE",      128),
		PDO_INT_CONST("FETCH_SERIALIZE",      512),
		PDO_INT_CONST("FETCH_PROPS_LATE",     256),
		PDO_INT_CONST("FETCH_NAMED",           11),
		PDO_INT_CONST("ATTR_AUTOCOMMIT",        0),
		PDO_INT_CONST("ATTR_PREFETCH",          1),
		PDO_INT_CONST("ATTR_TIMEOUT",           2),
		PDO_INT_CONST("ATTR_ERRMODE",           3),
		PDO_INT_CONST("ATTR_SERVER_VERSION",    4),
		PDO_INT_CONST("ATTR_CLIENT_VERSION",    5),
		PDO_INT_CONST("ATTR_SERVER_INFO",       6),
		PDO_INT_CONST("ATTR_CONNECTION_STATUS", 7),
		PDO_INT_CONST("ATTR_CASE",              8),
		PDO_INT_CONST("ATTR_CURSOR_NAME",       9),
		PDO_INT_CONST("ATTR_CURSOR",           10),
		PDO_INT_CONST("ATTR_ORACLE_NULLS",     11),
		PDO_INT_CONST("ATTR_PERSISTENT",       12),
		PDO_INT_CONST("ATTR_STATEMENT_CLASS",  13),
		PDO_INT_CONST("ATTR_FETCH_TABLE_NAMES",14),
		PDO_INT_CONST("ATTR_FETCH_CATALOG_NAMES",15),
		PDO_INT_CONST("ATTR_DRIVER_NAME",      16),
		PDO_INT_CONST("ATTR_STRINGIFY_FETCHES",17),
		PDO_INT_CONST("ATTR_MAX_COLUMN_LEN",   18),
		PDO_INT_CONST("ATTR_EMULATE_PREPARES", 20),
		PDO_INT_CONST("ATTR_DEFAULT_FETCH_MODE",19),
		PDO_INT_CONST("ATTR_DEFAULT_STR_PARAM",21),
		PDO_INT_CONST("ERRMODE_SILENT",         0),
		PDO_INT_CONST("ERRMODE_WARNING",        1),
		PDO_INT_CONST("ERRMODE_EXCEPTION",      2),
		PDO_INT_CONST("CASE_NATURAL",           0),
		PDO_INT_CONST("CASE_LOWER",             2),
		PDO_INT_CONST("CASE_UPPER",             1),
		PDO_INT_CONST("NULL_NATURAL",           0),
		PDO_INT_CONST("NULL_EMPTY_STRING",      1),
		PDO_INT_CONST("NULL_TO_STRING",         2),
		{ "ERR_NONE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, "00000", 0.0 },
		PDO_INT_CONST("FETCH_ORI_NEXT",         0),
		PDO_INT_CONST("FETCH_ORI_PRIOR",        1),
		PDO_INT_CONST("FETCH_ORI_FIRST",        2),
		PDO_INT_CONST("FETCH_ORI_LAST",         3),
		PDO_INT_CONST("FETCH_ORI_ABS",          4),
		PDO_INT_CONST("FETCH_ORI_REL",          5),
		PDO_INT_CONST("CURSOR_FWDONLY",         0),
		PDO_INT_CONST("CURSOR_SCROLL",          1),
	};
	/* php's own signatures and its own stub ORDER: Reflection and
	 * get_class_methods() both answer declaration order, so the two engines
	 * must list one surface. Nearly every return type is TENTATIVE in php's
	 * stubs (the leading `@`), which is a php-visible difference from a
	 * declared one -- getReturnType() answers null for a tentative type. */
	static const PH7_NativeMethodDef aPdoMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC,
		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",
		  0, vm_builtin_PDO___construct },
		{ "connect", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",
		  "static", vm_builtin_PDO_connect },
		{ "beginTransaction", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_beginTransaction },
		{ "commit",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_commit },
		{ "errorCode",        PH7_MOD_PUBLIC, "", "@?string", vm_builtin_PDO_errorCode },
		{ "errorInfo",        PH7_MOD_PUBLIC, "", "@array", vm_builtin_PDO_errorInfo },
		{ "exec",             PH7_MOD_PUBLIC, "string $statement", "@int|false",
		  vm_builtin_PDO_exec },
		{ "getAttribute",     PH7_MOD_PUBLIC, "int $attribute", "@mixed",
		  vm_builtin_PDO_getAttribute },
		{ "getAvailableDrivers", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "", "@array",
		  vm_builtin_PDO_getAvailableDrivers },
		{ "inTransaction",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_inTransaction },
		{ "lastInsertId",     PH7_MOD_PUBLIC, "?string $name = null", "@string|false",
		  vm_builtin_PDO_lastInsertId },
		{ "prepare",          PH7_MOD_PUBLIC, "string $query, array $options = []",
		  "@PDOStatement|false", vm_builtin_PDO_prepare },
		{ "query",            PH7_MOD_PUBLIC,
		  "string $query, ?int $fetchMode = null, mixed ...$fetchModeArgs",
		  "@PDOStatement|false", vm_builtin_PDO_query },
		{ "quote",            PH7_MOD_PUBLIC, "string $string, int $type = PDO::PARAM_STR",
		  "@string|false", vm_builtin_PDO_quote },
		{ "rollBack",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_rollBack },
		{ "setAttribute",     PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",
		  vm_builtin_PDO_setAttribute },
	};
	static const PH7_NativeMethodDef aStmtMethod[] = {
		{ "bindColumn",   PH7_MOD_PUBLIC,
		  "string|int $column, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "
		  "mixed $driverOptions = null", "@bool", vm_builtin_PDOStatement_bindColumn },
		{ "bindParam",    PH7_MOD_PUBLIC,
		  "string|int $param, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "
		  "mixed $driverOptions = null", "@bool", vm_builtin_PDOStatement_bindParam },
		{ "bindValue",    PH7_MOD_PUBLIC,
		  "string|int $param, mixed $value, int $type = PDO::PARAM_STR", "@bool",
		  vm_builtin_PDOStatement_bindValue },
		{ "closeCursor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDOStatement_closeCursor },
		{ "columnCount",  PH7_MOD_PUBLIC, "", "@int", vm_builtin_PDOStatement_columnCount },
		{ "debugDumpParams", PH7_MOD_PUBLIC, "", "@?bool",
		  vm_builtin_PDOStatement_debugDumpParams },
		{ "errorCode",    PH7_MOD_PUBLIC, "", "@?string", vm_builtin_PDOStatement_errorCode },
		{ "errorInfo",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_PDOStatement_errorInfo },
		{ "execute",      PH7_MOD_PUBLIC, "?array $params = null", "@bool",
		  vm_builtin_PDOStatement_execute },
		{ "fetch",        PH7_MOD_PUBLIC,
		  "int $mode = PDO::FETCH_DEFAULT, int $cursorOrientation = PDO::FETCH_ORI_NEXT, "
		  "int $cursorOffset = 0", "@mixed", vm_builtin_PDOStatement_fetch },
		{ "fetchAll",     PH7_MOD_PUBLIC, "int $mode = PDO::FETCH_DEFAULT, mixed ...$args",
		  "@array", vm_builtin_PDOStatement_fetchAll },
		{ "fetchColumn",  PH7_MOD_PUBLIC, "int $column = 0", "@mixed",
		  vm_builtin_PDOStatement_fetchColumn },
		{ "fetchObject",  PH7_MOD_PUBLIC,
		  "?string $class = 'stdClass', array $constructorArgs = []", "@object|false",
		  vm_builtin_PDOStatement_fetchObject },
		{ "getAttribute", PH7_MOD_PUBLIC, "int $name", "@mixed",
		  vm_builtin_PDOStatement_getAttribute },
		{ "getColumnMeta",PH7_MOD_PUBLIC, "int $column", "@array|false",
		  vm_builtin_PDOStatement_getColumnMeta },
		{ "nextRowset",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDOStatement_nextRowset },
		{ "rowCount",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_PDOStatement_rowCount },
		{ "setAttribute", PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",
		  vm_builtin_PDOStatement_setAttribute },
		{ "setFetchMode", PH7_MOD_PUBLIC, "int $mode, mixed ...$args", "@true",
		  vm_builtin_PDOStatement_setFetchMode },
		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_PDOStatement_getIterator },
	};
	/* The one property php PRESENTS on a statement: var_dump of a PDOStatement
	 * shows `queryString` and nothing else. It is typed and has no default --
	 * `new PDOStatement()` (which php allows) leaves it uninitialized. */
	static const PH7_NativePropDef aStmtProp[] = {
		{ "queryString", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		/* the cursor, hidden the way the connection's handle is */
		{ "__res", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	/*
	 * PDORow declares `public string $queryString;` and holds NO property at
	 * all: the object's whole surface is its handlers (PdoRowProp/PdoRowDim),
	 * so the declaration is marked LAZY below and nothing ever materializes it.
	 * The two engine slots beside it are hidden the way every other handle is.
	 */
	static const PH7_NativePropDef aRowProp[] = {
		{ "queryString", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		{ PDOROW_RES,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ PDOROW_STMT, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }
	};
	/* php redeclares Exception::$code UNTYPED here so a SQLSTATE -- a string
	 * like 'HY000' -- can live in it, and adds the driver's raw error triple. */
	static const PH7_NativePropDef aExcProp[] = {
		{ "code",      PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ "errorInfo", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?array" },
	};
	/* The connection handle: storage the class owns and NEVER presents -- php
	 * shows no property at all on a PDO, so the slot is hidden (which is what
	 * keeps it out of var_dump, (array), get_object_vars and Reflection). */
	static const PH7_NativePropDef aPdoProp[] = {
		{ "__res", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		/* Both handles refuse `clone` and `serialize`: php declares neither a
		 * clone handler nor a serializer for them, so the copy would carry the
		 * same sqlite3 pointer in its hidden slot. */
		{ "PDO", 0, 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aPdoMethod, SX_ARRAYSIZE(aPdoMethod),
		  aPdoConst, SX_ARRAYSIZE(aPdoConst),
		  aPdoProp, SX_ARRAYSIZE(aPdoProp),
		  PdoInstanceRelease, 0, 0 },
		{ "PDOStatement", 0, "IteratorAggregate", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aStmtMethod, SX_ARRAYSIZE(aStmtMethod),
		  0, 0,
		  aStmtProp, SX_ARRAYSIZE(aStmtProp),
		  PdoStmtInstanceRelease, &sPdoStmtIterVtab, 0 },
		{ "PDOException", "RuntimeException", 0, 0,
		  0, 0, 0, 0,
		  aExcProp, SX_ARRAYSIZE(aExcProp),
		  0, 0, 0 },
		/* FINAL, uncloneable, unserializable, and refusing `new` with php's own
		 * sentence -- which is a PDOException here and an Error everywhere else,
		 * so the class carries the exception name beside the text. */
		{ "PDORow", 0, 0,
		  PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  0, 0, 0, 0,
		  aRowProp, SX_ARRAYSIZE(aRowProp),
		  PdoRowInstanceRelease, 0, PdoRowPresent }
	};
#undef PDO_INT_CONST
	sxi32 rc;
	pVm->pPdoConns = 0;
	/* ext/pdo declares exactly one function beside its classes. */
	ph7_create_function(&(*pVm),"pdo_drivers",vm_builtin_pdo_drivers,0);
	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc == SXRET_OK ){
		ph7_class *pRow = PH7_VmExtractClass(&(*pVm),"PDORow",sizeof("PDORow")-1,FALSE,0);
		if( pRow ){
			pRow->zNewRefusal = "You may not create a PDORow manually";
			pRow->zNewRefusalClass = "PDOException";
			pRow->xDim = PdoRowDim;
			pRow->xCmp = PdoRowCmp;
		}
		/* The declaration php makes and the object never holds: marked LAZY, and
		 * nothing materializes it -- every write to this class is refused. */
		PH7_NativeClassMarkLazyProps(&(*pVm),"PDORow",0);
		PH7_NativeClassInstallPropHook(&(*pVm),"PDORow",PdoRowProp);
	}
	return rc;
}

#else
/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */
typedef int vm_pdo_unused;
#endif /* PH7_ENABLE_SQLITE */
