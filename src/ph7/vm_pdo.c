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
	PH7_PdoSqliteClose(pConn);
	if( pConn->zDrvMsg ){
		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);
		pConn->zDrvMsg = 0;
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
/* The connection behind a `__res` slot value. */
static phl_pdo * PdoOfValue(ph7_value *pVal)
{
	if( pVal == 0 || !ph7_value_is_resource(pVal) ){
		return 0;
	}
	return (phl_pdo *)ph7_value_to_resource(pVal);
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
	PH7_PdoSqliteClose(pConn);
	pConn->pOwner = 0;
}

/* ------------------------------------------------------------------------
 * The error surface
 * ------------------------------------------------------------------------ */
PH7_PRIVATE void PH7_PdoClearError(phl_pdo *pConn)
{
	pConn->iErrState = PDO_ERR_OK;
	pConn->iDrvCode = 0;
	SyMemcpy("00000",pConn->zSqlState,sizeof("00000"));
	if( pConn->zDrvMsg ){
		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);
		pConn->zDrvMsg = 0;
	}
}
PH7_PRIVATE void PH7_PdoSetError(phl_pdo *pConn,const char *zSqlState,int iCode,const char *zMsg)
{
	sxu32 n;
	pConn->iErrState = PDO_ERR_FAILED;
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
 * A failed OPERATION, routed through ATTR_ERRMODE: silent leaves the answer to
 * errorCode()/errorInfo(), warning adds php's E_WARNING naming the method, and
 * exception throws. The wording is one sentence in all three:
 * `SQLSTATE[HY000]: General error: 1 no such column: bogus`.
 */
PH7_PRIVATE sxi32 PH7_PdoRaise(ph7_context *pCtx,phl_pdo *pConn,const char *zFn)
{
	SyBlob sMsg;
	sxi32 rc = PH7_OK;
	if( pConn->iErrMode == PDO_ERRMODE_SILENT ){
		return PH7_OK;
	}
	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
	SyBlobFormat(&sMsg,"SQLSTATE[%s]: General error: %d %s",pConn->zSqlState,
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
	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
	SyBlobFormat(&sMsg,"SQLSTATE[%s]: Driver does not support this function: %s",
		zSqlState,zMsg);
	SyBlobAppend(&sMsg,"",1);
	if( pConn && pConn->iErrMode == PDO_ERRMODE_EXCEPTION ){
		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),zSqlState,0,FALSE,0,2);
	}else{
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));
	}
	SyBlobRelease(&sMsg);
	return rc;
}

/*
 * Every method body below is a placeholder: the DECLARED surface is in place
 * and each slice replaces one group of them with the real body.  The refusal
 * is loud on purpose -- an unimplemented verb must never look like an answer.
 */
static int PdoUnimplemented(ph7_context *pCtx)
{
	SyBlob sFn;
	SyBlobInit(&sFn,&pCtx->pVm->sAllocator);
	PH7_VmActiveFuncName(pCtx->pVm,&sFn);
	PH7_VmThrowException(pCtx,"Error","%.*s is not implemented yet",
		(int)SyBlobLength(&sFn),(const char *)SyBlobData(&sFn));
	SyBlobRelease(&sFn);
	return PH7_OK;
}
static int vm_builtin_pdo_stub(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PdoUnimplemented(pCtx);
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
	SXUNUSED(pConn);
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
	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	switch( iAttr ){
		case PDO_ATTR_ERRMODE:            ph7_result_int(pCtx,pConn->iErrMode); break;
		case PDO_ATTR_CASE:               ph7_result_int(pCtx,pConn->iCase); break;
		case PDO_ATTR_ORACLE_NULLS:       ph7_result_int(pCtx,pConn->iOracleNulls); break;
		case PDO_ATTR_DEFAULT_FETCH_MODE: ph7_result_int(pCtx,pConn->iDefaultFetch); break;
		case PDO_ATTR_STRINGIFY_FETCHES:  ph7_result_bool(pCtx,pConn->bStringify); break;
		case PDO_ATTR_PERSISTENT:         ph7_result_bool(pCtx,pConn->bPersistent); break;
		case PDO_SQLITE_ATTR_TRANSACTION_MODE: ph7_result_int(pCtx,pConn->iTxMode); break;
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
			ph7_value_string(pName,"PDOStatement",sizeof("PDOStatement")-1);
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
		case PDO_SQLITE_ATTR_TRANSACTION_MODE:
			pConn->iTxMode = (int)(pVal ? ph7_value_to_int64(pVal) : 0);
			break;
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
	return PH7_PdoSqliteOpen(pCtx,pConn,zDsn + nDriver + 1,nDsn - nDriver - 1,
		SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_URI);
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
 * PDO::getAvailableDrivers(): the one name this build carries.  php answers
 * the list of drivers its ext/pdo actually loaded, which is why an engine
 * with no driver at all answers [] -- here it is always ["sqlite"].
 */
static int vm_builtin_PDO_getAvailableDrivers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray, *pName;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
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
		{ "beginTransaction", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_pdo_stub },
		{ "commit",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_pdo_stub },
		{ "errorCode",        PH7_MOD_PUBLIC, "", "@?string", vm_builtin_pdo_stub },
		{ "errorInfo",        PH7_MOD_PUBLIC, "", "@array", vm_builtin_pdo_stub },
		{ "exec",             PH7_MOD_PUBLIC, "string $statement", "@int|false", vm_builtin_pdo_stub },
		{ "getAttribute",     PH7_MOD_PUBLIC, "int $attribute", "@mixed",
		  vm_builtin_PDO_getAttribute },
		{ "getAvailableDrivers", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "", "@array",
		  vm_builtin_PDO_getAvailableDrivers },
		{ "inTransaction",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_pdo_stub },
		{ "lastInsertId",     PH7_MOD_PUBLIC, "?string $name = null", "@string|false",
		  vm_builtin_pdo_stub },
		{ "prepare",          PH7_MOD_PUBLIC, "string $query, array $options = []",
		  "@PDOStatement|false", vm_builtin_pdo_stub },
		{ "query",            PH7_MOD_PUBLIC,
		  "string $query, ?int $fetchMode = null, mixed ...$fetchModeArgs",
		  "@PDOStatement|false", vm_builtin_pdo_stub },
		{ "quote",            PH7_MOD_PUBLIC, "string $string, int $type = PDO::PARAM_STR",
		  "@string|false", vm_builtin_pdo_stub },
		{ "rollBack",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_pdo_stub },
		{ "setAttribute",     PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",
		  vm_builtin_PDO_setAttribute },
	};
	static const PH7_NativeMethodDef aStmtMethod[] = {
		{ "bindColumn",   PH7_MOD_PUBLIC,
		  "string|int $column, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "
		  "mixed $driverOptions = null", "@bool", vm_builtin_pdo_stub },
		{ "bindParam",    PH7_MOD_PUBLIC,
		  "string|int $param, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "
		  "mixed $driverOptions = null", "@bool", vm_builtin_pdo_stub },
		{ "bindValue",    PH7_MOD_PUBLIC,
		  "string|int $param, mixed $value, int $type = PDO::PARAM_STR", "@bool",
		  vm_builtin_pdo_stub },
		{ "closeCursor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_pdo_stub },
		{ "columnCount",  PH7_MOD_PUBLIC, "", "@int", vm_builtin_pdo_stub },
		{ "debugDumpParams", PH7_MOD_PUBLIC, "", "@?bool", vm_builtin_pdo_stub },
		{ "errorCode",    PH7_MOD_PUBLIC, "", "@?string", vm_builtin_pdo_stub },
		{ "errorInfo",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_pdo_stub },
		{ "execute",      PH7_MOD_PUBLIC, "?array $params = null", "@bool", vm_builtin_pdo_stub },
		{ "fetch",        PH7_MOD_PUBLIC,
		  "int $mode = PDO::FETCH_DEFAULT, int $cursorOrientation = PDO::FETCH_ORI_NEXT, "
		  "int $cursorOffset = 0", "@mixed", vm_builtin_pdo_stub },
		{ "fetchAll",     PH7_MOD_PUBLIC, "int $mode = PDO::FETCH_DEFAULT, mixed ...$args",
		  "@array", vm_builtin_pdo_stub },
		{ "fetchColumn",  PH7_MOD_PUBLIC, "int $column = 0", "@mixed", vm_builtin_pdo_stub },
		{ "fetchObject",  PH7_MOD_PUBLIC,
		  "?string $class = 'stdClass', array $constructorArgs = []", "@object|false",
		  vm_builtin_pdo_stub },
		{ "getAttribute", PH7_MOD_PUBLIC, "int $name", "@mixed", vm_builtin_pdo_stub },
		{ "getColumnMeta",PH7_MOD_PUBLIC, "int $column", "@array|false", vm_builtin_pdo_stub },
		{ "nextRowset",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_pdo_stub },
		{ "rowCount",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_pdo_stub },
		{ "setAttribute", PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",
		  vm_builtin_pdo_stub },
		{ "setFetchMode", PH7_MOD_PUBLIC, "int $mode, mixed ...$args", "@true",
		  vm_builtin_pdo_stub },
		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_pdo_stub },
	};
	/* The one property php PRESENTS on a statement: var_dump of a PDOStatement
	 * shows `queryString` and nothing else. It is typed and has no default --
	 * `new PDOStatement()` (which php allows) leaves it uninitialized. */
	static const PH7_NativePropDef aStmtProp[] = {
		{ "queryString", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
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
		  0, 0, 0 },
		{ "PDOException", "RuntimeException", 0, 0,
		  0, 0, 0, 0,
		  aExcProp, SX_ARRAYSIZE(aExcProp),
		  0, 0, 0 },
	};
#undef PDO_INT_CONST
	pVm->pPdoConns = 0;
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}

#else
/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */
typedef int vm_pdo_unused;
#endif /* PH7_ENABLE_SQLITE */
