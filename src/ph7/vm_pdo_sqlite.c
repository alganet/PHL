/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_SQLITE
#include "ph7int.h"
#include "pdo_int.h"

/*
 * ext/pdo_sqlite: the driver.  This unit owns the sqlite3 connection and
 * statement handles and the `Pdo\Sqlite` subclass php 8.4 introduced -- the
 * class PDO::connect("sqlite:...") answers, and the only place the
 * sqlite-specific verbs (createFunction, createCollation, setAuthorizer, ...)
 * are declared.  The class library it extends lives in vm_pdo.c.
 *
 * §10's non-deprecated rule is what splits the constants: php still carries
 * `PDO::SQLITE_OPEN_READONLY` and six siblings, all of them reporting
 * isDeprecated(), and declares the successors here without the prefix.  PHL
 * declares the successors only.  The three OPEN_* values and the three
 * authorizer verdicts are sqlite's own macros rather than copied numbers --
 * they belong to the library, so they are read from its header.
 */

/*
 * Copy sqlite's own view of the last failure onto the connection.  php's
 * driver maps a handful of result codes to their SQL-standard SQLSTATE and
 * leaves everything else at HY000 ("general error"), which is what nearly
 * every sqlite failure reports.
 */
PH7_PRIVATE void PH7_PdoSqliteTakeError(phl_pdo *pConn)
{
	const char *zSqlState = "HY000";
	/* The PRIMARY result code, not the extended one: a UNIQUE violation is 19
	 * (SQLITE_CONSTRAINT) and not 2067 (SQLITE_CONSTRAINT_UNIQUE) unless the
	 * script asked for extended codes, which is what that driver attribute is
	 * for. sqlite reports the extended code from both accessors once they are
	 * enabled on the connection, so the choice is made here. */
	int iCode = pConn->pDb
		? (pConn->bExtendedCodes ? sqlite3_extended_errcode(pConn->pDb)
		                         : sqlite3_errcode(pConn->pDb))
		: SQLITE_ERROR;
	const char *zMsg = pConn->pDb ? sqlite3_errmsg(pConn->pDb) : "unknown error";
	/* the RAW code, not its low byte: with extended result codes on, a UNIQUE
	 * violation reports 1555, which matches none of these and lands on HY000 --
	 * php's own answer, and the reason turning extended codes on changes the
	 * SQLSTATE and not just the number beside it. */
	switch( iCode ){
		case SQLITE_NOTFOUND:   zSqlState = "42S02"; break;
		case SQLITE_INTERRUPT:  zSqlState = "57014"; break;
		case SQLITE_NOLFS:      zSqlState = "HYC00"; break;
		case SQLITE_TOOBIG:     zSqlState = "22001"; break;
		case SQLITE_CONSTRAINT: zSqlState = "23000"; break;
		case SQLITE_ERROR:
		default:                zSqlState = "HY000"; break;
	}
	PH7_PdoSetError(pConn,zSqlState,iCode,zMsg);
}
/*
 * The library version both ATTR_SERVER_VERSION and ATTR_CLIENT_VERSION answer.
 * It is the LINKED library's, so it differs between this engine's platforms
 * (3.45 on a Debian host, whatever vcpkg last shipped on Windows) -- which is
 * why no test may pin it.
 */
PH7_PRIVATE const char * PH7_PdoSqliteLibVersion(void)
{
	return sqlite3_libversion();
}
/*
 * Open one database.  php hands sqlite3_open_v2 the DSN's path verbatim, so
 * every spelling sqlite itself understands is a spelling PDO understands: a
 * relative or absolute path, the empty string (a private temporary database on
 * disk), `:memory:`, and the `file:...?mode=` URI form.
 *
 * A failure here is NOT routed through the error mode: php's constructor
 * always throws PDOException, whatever ATTR_ERRMODE the options asked for, and
 * the exception carries sqlite's own code as its $code (an int, unlike the
 * SQLSTATE string a later failure reports).
 */
PH7_PRIVATE sxi32 PH7_PdoSqliteOpen(ph7_context *pCtx,phl_pdo *pConn,const char *zPath,
	int nPath,int iFlags)
{
	char *zTerm;
	int rc;
	/* sqlite3_open_v2 wants a C string and the DSN slice is not one. */
	zTerm = (char *)SyMemBackendAlloc(&pConn->pVm->sAllocator,(sxu32)nPath + 1);
	if( zTerm == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( nPath > 0 ){
		SyMemcpy(zPath,zTerm,(sxu32)nPath);
	}
	zTerm[nPath] = 0;
	rc = sqlite3_open_v2(zTerm,&pConn->pDb,iFlags,0);
	SyMemBackendFree(&pConn->pVm->sAllocator,zTerm);
	if( rc != SQLITE_OK ){
		/* sqlite3_open_v2 hands back a handle even on failure so the message can
		 * be read off it; take the message first, then close. */
		const char *zMsg = pConn->pDb ? sqlite3_errmsg(pConn->pDb) : sqlite3_errstr(rc);
		sxi32 rcThrow;
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pConn->pVm->sAllocator);
		SyBlobAppend(&sMsg,zMsg,SyStrlen(zMsg));
		SyBlobAppend(&sMsg,"",1);
		rcThrow = PH7_PdoThrowConstruct(pCtx,"HY000",rc,(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
		PH7_PdoSqliteClose(pConn);
		return rcThrow;
	}
	return PH7_OK;
}
/*
 * Close one database.  Every statement this connection prepared must already
 * be finalized (later slices own that); sqlite3_close_v2 is used so a leaked
 * one defers the close rather than leaking the handle itself.
 */
PH7_PRIVATE void PH7_PdoSqliteClose(phl_pdo *pConn)
{
	if( pConn->pDb ){
		sqlite3_close_v2(pConn->pDb);
		pConn->pDb = 0;
	}
}

/*
 * PDO::exec()'s work: prepare, step and finalize every statement the string
 * holds, one after another.  php runs them ALL -- `INSERT ...; INSERT ...;` is
 * two inserts -- and answers sqlite3_changes(), which is the count from the
 * LAST statement that changed anything and is left ALONE by a SELECT or by a
 * statement that changes nothing. That is why exec() over a SELECT answers
 * whatever the previous write answered rather than 0.
 */
PH7_PRIVATE ph7_int64 PH7_PdoSqliteExec(phl_pdo *pConn,const char *zSql,int nSql)
{
	const char *zTail = zSql;
	const char *zEnd = zSql + nSql;
	if( pConn->pDb == 0 ){
		return -1;
	}
	while( zTail < zEnd ){
		sqlite3_stmt *pStmt = 0;
		const char *zNext = 0;
		int rc = sqlite3_prepare_v2(pConn->pDb,zTail,(int)(zEnd - zTail),&pStmt,&zNext);
		if( rc != SQLITE_OK ){
			PH7_PdoSqliteTakeError(pConn);
			return -1;
		}
		if( pStmt == 0 ){
			/* whitespace or a comment: nothing to run, and php answers the
			 * change count it already had rather than an error */
			if( zNext == 0 || zNext <= zTail ){
				break;
			}
			zTail = zNext;
			continue;
		}
		do{
			rc = sqlite3_step(pStmt);
		}while( rc == SQLITE_ROW );
		if( rc != SQLITE_DONE ){
			PH7_PdoSqliteTakeError(pConn);
			sqlite3_finalize(pStmt);
			return -1;
		}
		sqlite3_finalize(pStmt);
		zTail = zNext ? zNext : zEnd;
	}
	return (ph7_int64)sqlite3_changes(pConn->pDb);
}
PH7_PRIVATE ph7_int64 PH7_PdoSqliteLastInsertId(phl_pdo *pConn)
{
	return pConn->pDb ? (ph7_int64)sqlite3_last_insert_rowid(pConn->pDb) : 0;
}
PH7_PRIVATE ph7_int64 PH7_PdoSqliteChanges(phl_pdo *pConn)
{
	return pConn->pDb ? (ph7_int64)sqlite3_changes(pConn->pDb) : 0;
}
/*
 * sqlite's own autocommit flag rather than a counter of our own: php reads it
 * too, which is why `exec("BEGIN")` makes inTransaction() answer true and
 * beginTransaction() refuse -- the driver has no idea who opened it.
 */
/* Pdo\Sqlite::ATTR_READONLY_STATEMENT / ATTR_BUSY_STATEMENT: sqlite answers
 * both about a compiled statement. */
PH7_PRIVATE int PH7_PdoSqliteStmtReadonly(phl_pdo_stmt *pSt)
{
	return pSt->pStmt ? sqlite3_stmt_readonly(pSt->pStmt) : 0;
}
PH7_PRIVATE int PH7_PdoSqliteStmtBusy(phl_pdo_stmt *pSt)
{
	return pSt->pStmt ? sqlite3_stmt_busy(pSt->pStmt) : 0;
}
/*
 * Pdo\Sqlite::loadExtension().  sqlite keeps extension loading OFF by default,
 * so it is enabled around the one call and turned back off -- php does the
 * same, and leaving it on would let any later SQL load code.
 */
PH7_PRIVATE int PH7_PdoSqliteLoadExtension(phl_pdo *pConn,const char *zName)
{
	int rc;
	char *zErr = 0;
	if( pConn->pDb == 0 ){
		return 0;
	}
	sqlite3_enable_load_extension(pConn->pDb,1);
	rc = sqlite3_load_extension(pConn->pDb,zName,0,&zErr);
	sqlite3_enable_load_extension(pConn->pDb,0);
	if( zErr ){
		sqlite3_free(zErr);
	}
	return rc == SQLITE_OK;
}
PH7_PRIVATE void PH7_PdoSqliteExtendedCodes(phl_pdo *pConn,int bOn)
{
	if( pConn->pDb ){
		sqlite3_extended_result_codes(pConn->pDb,bOn ? 1 : 0);
	}
}
PH7_PRIVATE int PH7_PdoSqliteInTransaction(phl_pdo *pConn)
{
	return pConn->pDb ? (sqlite3_get_autocommit(pConn->pDb) == 0) : 0;
}
/*
 * Prepare ONE statement.  php compiles only the first statement of the string
 * here -- unlike exec(), which runs them all -- and what follows it is simply
 * not executed.
 */
PH7_PRIVATE int PH7_PdoSqlitePrepare(phl_pdo_stmt *pSt,const char *zSql,int nSql)
{
	int rc;
	if( pSt->pConn->pDb == 0 ){
		return 0;
	}
	rc = sqlite3_prepare_v2(pSt->pConn->pDb,zSql,nSql,&pSt->pStmt,0);
	if( rc != SQLITE_OK || pSt->pStmt == 0 ){
		PH7_PdoSqliteTakeError(pSt->pConn);
		if( pSt->pStmt ){
			sqlite3_finalize(pSt->pStmt);
			pSt->pStmt = 0;
		}
		return 0;
	}
	return 1;
}
/*
 * One step of the cursor: 1 when a row is available, 0 when the walk is over,
 * -1 on failure (with the connection's error set).
 */
PH7_PRIVATE int PH7_PdoSqliteStep(phl_pdo_stmt *pSt)
{
	int rc;
	if( pSt->pStmt == 0 ){
		return 0;
	}
	rc = sqlite3_step(pSt->pStmt);
	if( rc == SQLITE_ROW ){
		return 1;
	}
	if( rc == SQLITE_DONE ){
		return 0;
	}
	PH7_PdoSqliteTakeError(pSt->pConn);
	return -1;
}
PH7_PRIVATE void PH7_PdoSqliteFinalize(phl_pdo_stmt *pSt)
{
	if( pSt->pStmt ){
		sqlite3_finalize(pSt->pStmt);
		pSt->pStmt = 0;
	}
}
PH7_PRIVATE int PH7_PdoSqliteColumnCount(phl_pdo_stmt *pSt)
{
	return pSt->pStmt ? sqlite3_column_count(pSt->pStmt) : 0;
}
/*
 * Rewind a statement so it can run again.  The bindings go too: php re-binds
 * everything on every execute(), so a value bound for the previous run must
 * not survive into the next one.
 */
PH7_PRIVATE void PH7_PdoSqliteReset(phl_pdo_stmt *pSt)
{
	if( pSt->pStmt ){
		sqlite3_reset(pSt->pStmt);
		sqlite3_clear_bindings(pSt->pStmt);
	}
}
/*
 * Where a NAMED parameter sits.  sqlite answers 0 for a name the statement
 * does not have, and binding at 0 is SQLITE_RANGE -- which is exactly how php
 * ends up reporting "column index out of range" for a misspelled placeholder
 * rather than something that names it.
 */
PH7_PRIVATE int PH7_PdoSqliteBindIndexOf(phl_pdo_stmt *pSt,const char *zName,int nName)
{
	char zBuf[128];
	int n = 0;
	if( pSt->pStmt == 0 || nName < 1 ){
		return 0;
	}
	/* php accepts a name with or without its colon and sqlite wants it WITH,
	 * so the missing one is supplied here. */
	if( zName[0] != ':' ){
		zBuf[n++] = ':';
	}
	while( n < (int)sizeof(zBuf) - 1 && n - (zName[0] != ':' ? 1 : 0) < nName ){
		zBuf[n] = zName[n - (zName[0] != ':' ? 1 : 0)];
		++n;
	}
	zBuf[n] = 0;
	return sqlite3_bind_parameter_index(pSt->pStmt,zBuf);
}
/*
 * Bind one value at a 1-based position.  php's PARAM_* decides the CAST, not
 * the value's own type: PARAM_INT over the float 1.9 binds 1, PARAM_STR over
 * the same binds "1.5", and PARAM_NULL binds null whatever it was handed. The
 * one type that outranks the declaration is php's own null, which binds as
 * NULL through any of them.
 */
PH7_PRIVATE int PH7_PdoSqliteBindAt(phl_pdo_stmt *pSt,int iPos,int iType,ph7_value *pVal)
{
	int rc;
	if( pSt->pStmt == 0 ){
		return 0;
	}
	iType &= ~PDO_PARAM_FLAGS;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) || iType == PDO_PARAM_NULL ){
		rc = sqlite3_bind_null(pSt->pStmt,iPos);
	}else{
		switch( iType ){
			case PDO_PARAM_INT:
				rc = sqlite3_bind_int64(pSt->pStmt,iPos,(sqlite3_int64)ph7_value_to_int64(pVal));
				break;
			case PDO_PARAM_BOOL:
				rc = sqlite3_bind_int(pSt->pStmt,iPos,ph7_value_to_bool(pVal) ? 1 : 0);
				break;
			case PDO_PARAM_LOB: {
				int nByte = 0;
				const char *zVal = ph7_value_to_string(pVal,&nByte);
				rc = sqlite3_bind_blob(pSt->pStmt,iPos,zVal,nByte,SQLITE_TRANSIENT);
				break;
			}
			default: {
				int nByte = 0;
				const char *zVal = ph7_value_to_string(pVal,&nByte);
				rc = sqlite3_bind_text(pSt->pStmt,iPos,zVal,nByte,SQLITE_TRANSIENT);
				break;
			}
		}
	}
	if( rc != SQLITE_OK ){
		PH7_PdoSqliteTakeError(pSt->pConn);
		return 0;
	}
	return 1;
}
PH7_PRIVATE const char * PH7_PdoSqliteColumnName(phl_pdo_stmt *pSt,int iCol)
{
	const char *zName = pSt->pStmt ? sqlite3_column_name(pSt->pStmt,iCol) : 0;
	return zName ? zName : "";
}
/*
 * The type the SCHEMA declares for a column, which is not the type of the
 * value in it: a column declared TEXT holding NULL reports decl_type "TEXT"
 * and native_type "null". An expression has no declared type at all.
 */
PH7_PRIVATE const char * PH7_PdoSqliteColumnDecl(phl_pdo_stmt *pSt,int iCol)
{
	return pSt->pStmt ? sqlite3_column_decltype(pSt->pStmt,iCol) : 0;
}
/*
 * The table a column came from.  sqlite compiles this one only under
 * SQLITE_ENABLE_COLUMN_METADATA, so the SYMBOL's presence is the feature's:
 * verified present in the Debian and vcpkg libraries this engine links, and
 * php reports the same key from the same call. A platform whose sqlite lacks
 * it would fail to LINK rather than answer differently -- and its php would be
 * missing the key too.
 */
PH7_PRIVATE const char * PH7_PdoSqliteColumnTable(phl_pdo_stmt *pSt,int iCol)
{
	return pSt->pStmt ? sqlite3_column_table_name(pSt->pStmt,iCol) : 0;
}
PH7_PRIVATE int PH7_PdoSqliteColumnType(phl_pdo_stmt *pSt,int iCol)
{
	return pSt->pStmt ? sqlite3_column_type(pSt->pStmt,iCol) : SQLITE_NULL;
}
/*
 * What the statement's last step answered.  php reports THIS as the driver
 * code when getColumnMeta() is asked for a column that does not exist, which
 * is how a plain out-of-range index comes back as "100 another row available".
 */
PH7_PRIVATE int PH7_PdoSqliteLastStepCode(phl_pdo_stmt *pSt)
{
	return pSt->bRowPending ? SQLITE_ROW : SQLITE_DONE;
}
/*
 * One column of the row at the cursor, in sqlite's OWN type: an INTEGER comes
 * back as an int and a REAL as a float, which is why a fetch from this driver
 * is not all-strings the way a stringifying one is. A BLOB is a php string of
 * those bytes, NUL bytes included, so the length has to come from sqlite
 * rather than from the pointer.
 */
PH7_PRIVATE void PH7_PdoSqliteColumnValue(phl_pdo_stmt *pSt,int iCol,ph7_value *pOut)
{
	if( pSt->pStmt == 0 ){
		ph7_value_null(pOut);
		return;
	}
	switch( sqlite3_column_type(pSt->pStmt,iCol) ){
		case SQLITE_INTEGER:
			ph7_value_int64(pOut,(ph7_int64)sqlite3_column_int64(pSt->pStmt,iCol));
			break;
#ifndef PH7_OMIT_FLOATING_POINT
		case SQLITE_FLOAT:
			ph7_value_double(pOut,(ph7_real)sqlite3_column_double(pSt->pStmt,iCol));
			break;
#endif
		case SQLITE_BLOB: {
			const void *pBlob = sqlite3_column_blob(pSt->pStmt,iCol);
			int nByte = sqlite3_column_bytes(pSt->pStmt,iCol);
			/* the release is what CLEARS it: ph7_value_string APPENDS to a value
			 * that is already a string, so writing into a reused cell without
			 * this carries the previous column's bytes along */
			PH7_MemObjRelease(pOut);
			ph7_value_string(pOut,(const char *)pBlob,pBlob ? nByte : 0);
			break;
		}
		case SQLITE_NULL:
			ph7_value_null(pOut);
			break;
		default: {
			const char *zText = (const char *)sqlite3_column_text(pSt->pStmt,iCol);
			int nByte = sqlite3_column_bytes(pSt->pStmt,iCol);
			PH7_MemObjRelease(pOut);
			ph7_value_string(pOut,zText,zText ? nByte : 0);
			break;
		}
	}
}


/* ------------------------------------------------------------------------
 * Userland callbacks, called from inside sqlite's own loop
 * ------------------------------------------------------------------------ */
/*
 * The rule for every callback here (and the reason they share one shape):
 * sqlite is in the middle of a step when the engine re-enters PHP, and a throw
 * out of that PHP cannot travel through sqlite's C frames. So the status is
 * PARKED on the connection, sqlite is told to stop with an error, and the verb
 * that started the step raises exactly the parked status once the library has
 * unwound. A second callback while one is parked does not re-enter PHP at all.
 */
static int PdoUdfParked(phl_pdo_udf *pUdf)
{
	return pUdf->pConn->iCallbackExc != 0;
}
static void PdoUdfPark(phl_pdo_udf *pUdf,sxi32 rc,sqlite3_context *pCtx)
{
	pUdf->pConn->iCallbackExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;
	if( pCtx ){
		sqlite3_result_error(pCtx,"PHL: callback raised",-1);
	}
}
/* One sqlite value as a php value, in sqlite's own types. */
static void PdoUdfArgValue(ph7_vm *pVm,sqlite3_value *pIn,ph7_value *pOut)
{
	PH7_MemObjInit(pVm,pOut);
	switch( sqlite3_value_type(pIn) ){
		case SQLITE_INTEGER:
			ph7_value_int64(pOut,(ph7_int64)sqlite3_value_int64(pIn));
			break;
#ifndef PH7_OMIT_FLOATING_POINT
		case SQLITE_FLOAT:
			ph7_value_double(pOut,(ph7_real)sqlite3_value_double(pIn));
			break;
#endif
		case SQLITE_NULL:
			ph7_value_null(pOut);
			break;
		case SQLITE_BLOB:
			ph7_value_string(pOut,(const char *)sqlite3_value_blob(pIn),
				sqlite3_value_bytes(pIn));
			break;
		default:
			ph7_value_string(pOut,(const char *)sqlite3_value_text(pIn),
				sqlite3_value_bytes(pIn));
			break;
	}
}
/*
 * What a callback RETURNED, as a sqlite result.  php maps null, int and float
 * straight through and puts everything else through a STRING cast -- which is
 * why a bool comes back as "1" and "" and an array comes back as "Array" with
 * php's own conversion warning behind it.
 */
static void PdoUdfResult(sqlite3_context *pCtx,ph7_value *pVal)
{
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) ){
		sqlite3_result_null(pCtx);
	}else if( pVal->iFlags & MEMOBJ_INT ){
		sqlite3_result_int64(pCtx,(sqlite3_int64)ph7_value_to_int64(pVal));
#ifndef PH7_OMIT_FLOATING_POINT
	}else if( pVal->iFlags & MEMOBJ_REAL ){
		sqlite3_result_double(pCtx,(double)ph7_value_to_double(pVal));
#endif
	}else{
		/* php's string CAST, not a quiet stringification: an array coming back
		 * from a callback is "Array" with php's own conversion warning behind
		 * it, and a __toString() that throws travels the callback rail. */
		int nByte = 0;
		const char *zStr;
		PH7_MemObjToStringUV(pVal);
		zStr = ph7_value_to_string(pVal,&nByte);
		sqlite3_result_text(pCtx,zStr,nByte,SQLITE_TRANSIENT);
	}
}
/* A scalar function's body: build the arguments, call, convert the answer. */
static void PdoUdfScalar(sqlite3_context *pCtx,int nArg,sqlite3_value **apVal)
{
	phl_pdo_udf *pUdf = (phl_pdo_udf *)sqlite3_user_data(pCtx);
	ph7_vm *pVm = pUdf->pConn->pVm;
	ph7_value *apArg[16];
	ph7_value sArgs[16];
	ph7_value sRes;
	int n,nCall = nArg;
	sxi32 rc;
	if( PdoUdfParked(pUdf) ){
		sqlite3_result_error(pCtx,"PHL: callback raised",-1);
		return;
	}
	if( nCall > (int)SX_ARRAYSIZE(sArgs) ){
		nCall = (int)SX_ARRAYSIZE(sArgs);
	}
	for( n = 0 ; n < nCall ; ++n ){
		PdoUdfArgValue(pVm,apVal[n],&sArgs[n]);
		apArg[n] = &sArgs[n];
	}
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,nCall,nCall ? apArg : 0,&sRes);
	if( rc != SXRET_OK ){
		PdoUdfPark(pUdf,rc,pCtx);
	}else{
		PdoUdfResult(pCtx,&sRes);
	}
	PH7_MemObjRelease(&sRes);
	for( n = 0 ; n < nCall ; ++n ){
		PH7_MemObjRelease(&sArgs[n]);
	}
}
/* A collation: two strings in, an ordering out. */
static int PdoUdfCollate(void *pUser,int nLeft,const void *pLeft,int nRight,const void *pRight)
{
	phl_pdo_udf *pUdf = (phl_pdo_udf *)pUser;
	ph7_vm *pVm = pUdf->pConn->pVm;
	ph7_value sL,sR,sRes;
	ph7_value *apArg[2];
	sxi32 rc;
	int iCmp = 0;
	if( PdoUdfParked(pUdf) ){
		return 0;
	}
	PH7_MemObjInit(pVm,&sL);
	PH7_MemObjInit(pVm,&sR);
	ph7_value_string(&sL,(const char *)pLeft,nLeft);
	ph7_value_string(&sR,(const char *)pRight,nRight);
	apArg[0] = &sL;
	apArg[1] = &sR;
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,2,apArg,&sRes);
	if( rc != SXRET_OK ){
		/* no sqlite3_context here to fail through: park it and order the pair
		 * as equal, which leaves the walk to end on the parked status */
		PdoUdfPark(pUdf,rc,0);
	}else{
		ph7_int64 iVal = ph7_value_to_int64(&sRes);
		iCmp = iVal < 0 ? -1 : (iVal > 0 ? 1 : 0);
	}
	PH7_MemObjRelease(&sRes);
	PH7_MemObjRelease(&sL);
	PH7_MemObjRelease(&sR);
	return iCmp;
}
/*
 * An aggregate's step and finalize halves.  php gives both callbacks the
 * running CONTEXT and the ROW COUNT as their first two arguments, and whatever
 * step returns becomes the context of the next one. The count php reports to
 * the finalizer is one past the rows it stepped over -- including for an empty
 * group, where step never runs at all and the finalizer still sees 1.
 */
static void PdoUdfStep(sqlite3_context *pCtx,int nArg,sqlite3_value **apVal)
{
	phl_pdo_udf *pUdf = (phl_pdo_udf *)sqlite3_user_data(pCtx);
	ph7_vm *pVm = pUdf->pConn->pVm;
	phl_pdo_agg *pAgg;
	ph7_value sArgs[16];
	ph7_value *apArg[18];
	ph7_value sCount,sRes;
	int n,nCall = nArg;
	sxi32 rc;
	if( PdoUdfParked(pUdf) ){
		sqlite3_result_error(pCtx,"PHL: callback raised",-1);
		return;
	}
	pAgg = (phl_pdo_agg *)sqlite3_aggregate_context(pCtx,(int)sizeof(phl_pdo_agg));
	if( pAgg == 0 ){
		return;
	}
	if( pAgg->pCtx == 0 ){
		pAgg->pCtx = ph7_new_scalar(pVm);
		pAgg->nRow = 0;
	}
	pAgg->nRow++;
	if( nCall > (int)SX_ARRAYSIZE(sArgs) ){
		nCall = (int)SX_ARRAYSIZE(sArgs);
	}
	PH7_MemObjInit(pVm,&sCount);
	ph7_value_int(&sCount,pAgg->nRow);
	apArg[0] = pAgg->pCtx;
	apArg[1] = &sCount;
	for( n = 0 ; n < nCall ; ++n ){
		PdoUdfArgValue(pVm,apVal[n],&sArgs[n]);
		apArg[n + 2] = &sArgs[n];
	}
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,nCall + 2,apArg,&sRes);
	if( rc != SXRET_OK ){
		PdoUdfPark(pUdf,rc,pCtx);
	}else if( pAgg->pCtx ){
		PH7_MemObjStore(&sRes,pAgg->pCtx);
	}
	PH7_MemObjRelease(&sRes);
	PH7_MemObjRelease(&sCount);
	for( n = 0 ; n < nCall ; ++n ){
		PH7_MemObjRelease(&sArgs[n]);
	}
}
static void PdoUdfFinal(sqlite3_context *pCtx)
{
	phl_pdo_udf *pUdf = (phl_pdo_udf *)sqlite3_user_data(pCtx);
	ph7_vm *pVm = pUdf->pConn->pVm;
	phl_pdo_agg *pAgg = (phl_pdo_agg *)sqlite3_aggregate_context(pCtx,0);
	ph7_value sCtx,sCount,sRes;
	ph7_value *apArg[2];
	sxi32 rc;
	if( PdoUdfParked(pUdf) ){
		sqlite3_result_error(pCtx,"PHL: callback raised",-1);
		return;
	}
	PH7_MemObjInit(pVm,&sCtx);
	PH7_MemObjInit(pVm,&sCount);
	if( pAgg && pAgg->pCtx ){
		PH7_MemObjStore(pAgg->pCtx,&sCtx);
	}
	ph7_value_int(&sCount,(pAgg ? pAgg->nRow : 0) + 1);
	apArg[0] = &sCtx;
	apArg[1] = &sCount;
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pUdf->pFinalize ? pUdf->pFinalize : pUdf->pCallback,
		2,apArg,&sRes);
	if( rc != SXRET_OK ){
		PdoUdfPark(pUdf,rc,pCtx);
	}else{
		PdoUdfResult(pCtx,&sRes);
	}
	if( pAgg && pAgg->pCtx ){
		ph7_release_value(pVm,pAgg->pCtx);
		pAgg->pCtx = 0;
	}
	PH7_MemObjRelease(&sRes);
	PH7_MemObjRelease(&sCtx);
	PH7_MemObjRelease(&sCount);
}
PH7_PRIVATE int PH7_PdoSqliteAddAggregate(phl_pdo_udf *pUdf,const char *zName,int nArg)
{
	int rc;
	if( pUdf->pConn->pDb == 0 ){
		return 0;
	}
	rc = sqlite3_create_function_v2(pUdf->pConn->pDb,zName,nArg,SQLITE_UTF8,pUdf,
		0,PdoUdfStep,PdoUdfFinal,0);
	if( rc != SQLITE_OK ){
		PH7_PdoSqliteTakeError(pUdf->pConn);
		return 0;
	}
	return 1;
}
/*
 * The authorizer: sqlite asks before it COMPILES each action, so a refusal
 * here stops a prepare rather than a step. Its verdict is one of three ints,
 * and anything else (including a callback that throws, whose status is parked
 * the usual way) denies.
 */
static int PdoUdfAuthorize(void *pUser,int iAction,const char *z1,const char *z2,
	const char *z3,const char *z4)
{
	phl_pdo_udf *pUdf = (phl_pdo_udf *)pUser;
	ph7_vm *pVm;
	ph7_value sArgs[6],sRes;
	ph7_value *apArg[6];
	const char *azIn[4];
	int n,iVerdict = SQLITE_OK;
	sxi32 rc;
	if( pUdf == 0 || PdoUdfParked(pUdf) ){
		return SQLITE_DENY;
	}
	pVm = pUdf->pConn->pVm;
	azIn[0] = z1; azIn[1] = z2; azIn[2] = z3; azIn[3] = z4;
	PH7_MemObjInit(pVm,&sArgs[0]);
	ph7_value_int(&sArgs[0],iAction);
	apArg[0] = &sArgs[0];
	for( n = 0 ; n < 4 ; ++n ){
		PH7_MemObjInit(pVm,&sArgs[n + 1]);
		if( azIn[n] ){
			ph7_value_string(&sArgs[n + 1],azIn[n],(int)SyStrlen(azIn[n]));
		}else{
			ph7_value_null(&sArgs[n + 1]);
		}
		apArg[n + 1] = &sArgs[n + 1];
	}
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,5,apArg,&sRes);
	if( rc != SXRET_OK ){
		PdoUdfPark(pUdf,rc,0);
		iVerdict = SQLITE_DENY;
	}else{
		ph7_int64 iVal = ph7_value_to_int64(&sRes);
		iVerdict = (iVal == SQLITE_IGNORE) ? SQLITE_IGNORE
			: ((iVal == SQLITE_OK) ? SQLITE_OK : SQLITE_DENY);
	}
	PH7_MemObjRelease(&sRes);
	for( n = 0 ; n < 5 ; ++n ){
		PH7_MemObjRelease(&sArgs[n]);
	}
	return iVerdict;
}
PH7_PRIVATE void PH7_PdoSqliteSetAuthorizer(phl_pdo *pConn,phl_pdo_udf *pUdf)
{
	if( pConn->pDb == 0 ){
		return;
	}
	if( pUdf ){
		sqlite3_set_authorizer(pConn->pDb,PdoUdfAuthorize,pUdf);
	}else{
		sqlite3_set_authorizer(pConn->pDb,0,0);
	}
}
PH7_PRIVATE int PH7_PdoSqliteAddFunction(phl_pdo_udf *pUdf,const char *zName,int nArg,int iFlags)
{
	int rc;
	if( pUdf->pConn->pDb == 0 ){
		return 0;
	}
	rc = sqlite3_create_function_v2(pUdf->pConn->pDb,zName,nArg,
		SQLITE_UTF8 | (iFlags & SQLITE_DETERMINISTIC),pUdf,PdoUdfScalar,0,0,0);
	if( rc != SQLITE_OK ){
		PH7_PdoSqliteTakeError(pUdf->pConn);
		return 0;
	}
	return 1;
}
PH7_PRIVATE int PH7_PdoSqliteAddCollation(phl_pdo_udf *pUdf,const char *zName)
{
	int rc;
	if( pUdf->pConn->pDb == 0 ){
		return 0;
	}
	rc = sqlite3_create_collation_v2(pUdf->pConn->pDb,zName,SQLITE_UTF8,pUdf,
		PdoUdfCollate,0);
	if( rc != SQLITE_OK ){
		PH7_PdoSqliteTakeError(pUdf->pConn);
		return 0;
	}
	return 1;
}

/*
 * The connection a Pdo\Sqlite verb was called on.  These methods are declared
 * on the subclass, so the receiver always carries one.
 */
static phl_pdo * PdoSqliteThis(ph7_context *pCtx)
{
	return PH7_PdoConnOfInstance(PH7_ContextThis(pCtx));
}
/* Record one callback on the connection, so it outlives the registering call. */
static phl_pdo_udf * PdoUdfNew(phl_pdo *pConn,const char *zName,int nName,
	ph7_value *pCallback,ph7_value *pFinalize)
{
	ph7_vm *pVm = pConn->pVm;
	phl_pdo_udf *pUdf = (phl_pdo_udf *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo_udf));
	if( pUdf == 0 ){
		return 0;
	}
	SyZero(pUdf,sizeof(phl_pdo_udf));
	pUdf->pConn = pConn;
	pUdf->zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nName + 1);
	if( pUdf->zName ){
		SyMemcpy(zName,pUdf->zName,(sxu32)nName);
		pUdf->zName[nName] = 0;
	}
	pUdf->pCallback = ph7_new_scalar(pVm);
	if( pUdf->pCallback ){
		PH7_MemObjStore(pCallback,pUdf->pCallback);
	}
	if( pFinalize ){
		pUdf->pFinalize = ph7_new_scalar(pVm);
		if( pUdf->pFinalize ){
			PH7_MemObjStore(pFinalize,pUdf->pFinalize);
		}
	}
	pUdf->pNext = pConn->pUdfs;
	pConn->pUdfs = pUdf;
	return pUdf;
}
/* php's refusal for a callback it cannot call, worded per verb. */
static sxi32 PdoUdfBadCallable(ph7_context *pCtx,const char *zFn,int iArg,const char *zParam,
	ph7_value *pVal)
{
	int nName = 0;
	const char *zName = pVal ? ph7_value_to_string(pVal,&nName) : "";
	return PH7_VmThrowException(pCtx,"TypeError",
		"%s(): Argument #%d ($%s) must be a valid callback, function \"%.*s\" not found "
		"or invalid function name",zFn,iArg,zParam,nName,zName);
}
/*
 * Pdo\Sqlite::createFunction(string $name, callable $callback,
 *                            int $numArgs = -1, int $flags = 0): bool
 *
 * A SQL function whose body is PHP. -1 arguments means "any arity", which is
 * sqlite's own convention, and DETERMINISTIC is the one flag sqlite takes here.
 * Registering the same name twice REPLACES the previous body.
 */
static int vm_builtin_PdoSqlite_createFunction(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoSqliteThis(pCtx);
	phl_pdo_udf *pUdf;
	const char *zName;
	int nName = 0,nWant,iFlags;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";
	if( nArg < 2 || !ph7_value_is_callable(apArg[1]) ){
		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createFunction",2,"callback",
			nArg > 1 ? apArg[1] : 0);
	}
	nWant = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : -1;
	iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3]) : 0;
	pUdf = PdoUdfNew(pConn,zName,nName,apArg[1],0);
	if( pUdf == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !PH7_PdoSqliteAddFunction(pUdf,pUdf->zName ? pUdf->zName : "",nWant,iFlags) ){
		ph7_result_bool(pCtx,0);
		return PH7_PdoRaise(pCtx,pConn,"Pdo\\Sqlite::createFunction");
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Pdo\Sqlite::createCollation(string $name, callable $callback): bool
 *
 * An ORDER BY ... COLLATE whose comparison is PHP. The callback answers the
 * usual negative/zero/positive.
 */
static int vm_builtin_PdoSqlite_createCollation(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoSqliteThis(pCtx);
	phl_pdo_udf *pUdf;
	const char *zName;
	int nName = 0;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";
	if( nArg < 2 || !ph7_value_is_callable(apArg[1]) ){
		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createCollation",2,"callback",
			nArg > 1 ? apArg[1] : 0);
	}
	pUdf = PdoUdfNew(pConn,zName,nName,apArg[1],0);
	if( pUdf == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !PH7_PdoSqliteAddCollation(pUdf,pUdf->zName ? pUdf->zName : "") ){
		ph7_result_bool(pCtx,0);
		return PH7_PdoRaise(pCtx,pConn,"Pdo\\Sqlite::createCollation");
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Pdo\Sqlite::createAggregate(string $name, callable $step, callable $finalize,
 *                             int $numArgs = -1): bool
 */
static int vm_builtin_PdoSqlite_createAggregate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoSqliteThis(pCtx);
	phl_pdo_udf *pUdf;
	const char *zName;
	int nName = 0,nWant;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";
	if( nArg < 2 || !ph7_value_is_callable(apArg[1]) ){
		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createAggregate",2,"step",
			nArg > 1 ? apArg[1] : 0);
	}
	if( nArg < 3 || !ph7_value_is_callable(apArg[2]) ){
		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createAggregate",3,"finalize",
			nArg > 2 ? apArg[2] : 0);
	}
	nWant = nArg > 3 ? (int)ph7_value_to_int64(apArg[3]) : -1;
	pUdf = PdoUdfNew(pConn,zName,nName,apArg[1],apArg[2]);
	if( pUdf == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !PH7_PdoSqliteAddAggregate(pUdf,pUdf->zName ? pUdf->zName : "",nWant) ){
		ph7_result_bool(pCtx,0);
		return PH7_PdoRaise(pCtx,pConn,"Pdo\\Sqlite::createAggregate");
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Pdo\Sqlite::setAuthorizer(?callable $callback): void
 *
 * sqlite asks the authorizer while it COMPILES, so a refusal stops a prepare
 * rather than a step -- which is why a denied SELECT fails with "not
 * authorized" from query() and never reaches a fetch. null removes it.
 */
static int vm_builtin_PdoSqlite_setAuthorizer(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoSqliteThis(pCtx);
	phl_pdo_udf *pUdf;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	if( nArg < 1 || apArg[0] == 0 || (apArg[0]->iFlags & MEMOBJ_NULL) ){
		PH7_PdoSqliteSetAuthorizer(pConn,0);
		return PH7_OK;
	}
	if( !ph7_value_is_callable(apArg[0]) ){
		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::setAuthorizer",1,"callback",apArg[0]);
	}
	pUdf = PdoUdfNew(pConn,"",0,apArg[0],0);
	if( pUdf == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_PdoSqliteSetAuthorizer(pConn,pUdf);
	return PH7_OK;
}
/*
 * Pdo\Sqlite::loadExtension(string $name): void
 *
 * A refusal here is a bare PDOException naming the extension, with no SQLSTATE
 * in front of it -- the same shape the transaction refusals use.
 */
static int vm_builtin_PdoSqlite_loadExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_pdo *pConn = PdoSqliteThis(pCtx);
	const char *zName;
	int nName = 0;
	SyBlob sName;
	int bOk;
	if( pConn == 0 ){
		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");
	}
	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";
	SyBlobInit(&sName,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sName,zName,(sxu32)nName);
	SyBlobAppend(&sName,"",1);
	bOk = PH7_PdoSqliteLoadExtension(pConn,(const char *)SyBlobData(&sName));
	SyBlobRelease(&sName);
	if( !bOk ){
		return PH7_VmThrowException(pCtx,"PDOException",
			"Unable to load extension \"%.*s\"",nName,zName);
	}
	return PH7_OK;
}
static int vm_builtin_pdo_sqlite_stub(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyBlob sFn;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SyBlobInit(&sFn,&pCtx->pVm->sAllocator);
	PH7_VmActiveFuncName(pCtx->pVm,&sFn);
	PH7_VmThrowException(pCtx,"Error","%.*s is not implemented yet",
		(int)SyBlobLength(&sFn),(const char *)SyBlobData(&sFn));
	SyBlobRelease(&sFn);
	return PH7_OK;
}

/*
 * Install the sqlite driver's class surface.  Called from PH7_VmInit right
 * after PH7_VmInstallPdo -- `Pdo\Sqlite` extends PDO, so the parent must
 * already be mounted.
 */
PH7_PRIVATE sxi32 PH7_VmInstallPdoSqlite(ph7_vm *pVm)
{
#define PDO_SQLITE_INT_CONST(NAME,VALUE) \
	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (ph7_int64)(VALUE), 0, 0.0 }
	static const PH7_NativeConstDef aConst[] = {
		/* php's own PDO_SQLITE_ATTR_* numbering, which starts past the generic
		 * PDO::ATTR_* block so a driver attribute can never collide with one. */
		PDO_SQLITE_INT_CONST("ATTR_OPEN_FLAGS",            1000),
		PDO_SQLITE_INT_CONST("ATTR_READONLY_STATEMENT",    1001),
		PDO_SQLITE_INT_CONST("ATTR_EXTENDED_RESULT_CODES", 1002),
		PDO_SQLITE_INT_CONST("ATTR_BUSY_STATEMENT",        1003),
		PDO_SQLITE_INT_CONST("ATTR_EXPLAIN_STATEMENT",     1004),
		PDO_SQLITE_INT_CONST("ATTR_TRANSACTION_MODE",      1005),
		/* sqlite's own flags, read from its header rather than copied. */
		PDO_SQLITE_INT_CONST("DETERMINISTIC",   SQLITE_DETERMINISTIC),
		PDO_SQLITE_INT_CONST("OPEN_READONLY",   SQLITE_OPEN_READONLY),
		PDO_SQLITE_INT_CONST("OPEN_READWRITE",  SQLITE_OPEN_READWRITE),
		PDO_SQLITE_INT_CONST("OPEN_CREATE",     SQLITE_OPEN_CREATE),
		/* An authorizer callback's three verdicts. */
		PDO_SQLITE_INT_CONST("OK",     SQLITE_OK),
		PDO_SQLITE_INT_CONST("DENY",   SQLITE_DENY),
		PDO_SQLITE_INT_CONST("IGNORE", SQLITE_IGNORE),
		/* php's own: which BEGIN a beginTransaction() emits, and what an
		 * ATTR_EXPLAIN_STATEMENT prepare explains. */
		PDO_SQLITE_INT_CONST("TRANSACTION_MODE_DEFERRED",  0),
		PDO_SQLITE_INT_CONST("TRANSACTION_MODE_IMMEDIATE", 1),
		PDO_SQLITE_INT_CONST("TRANSACTION_MODE_EXCLUSIVE", 2),
		PDO_SQLITE_INT_CONST("EXPLAIN_MODE_PREPARED",            0),
		PDO_SQLITE_INT_CONST("EXPLAIN_MODE_EXPLAIN",             1),
		PDO_SQLITE_INT_CONST("EXPLAIN_MODE_EXPLAIN_QUERY_PLAN",  2),
	};
	/* Unlike the base class's, these return types are DECLARED, not tentative
	 * -- php wrote this stub after tentative types existed. openBlob is the one
	 * exception: it answers a stream resource, which php's stubs cannot spell. */
	static const PH7_NativeMethodDef aMethod[] = {
		{ "createAggregate", PH7_MOD_PUBLIC,
		  "string $name, callable $step, callable $finalize, int $numArgs = -1", "bool",
		  vm_builtin_PdoSqlite_createAggregate },
		{ "createCollation", PH7_MOD_PUBLIC, "string $name, callable $callback", "bool",
		  vm_builtin_PdoSqlite_createCollation },
		{ "createFunction",  PH7_MOD_PUBLIC,
		  "string $function_name, callable $callback, int $num_args = -1, int $flags = 0", "bool",
		  vm_builtin_PdoSqlite_createFunction },
		{ "loadExtension",   PH7_MOD_PUBLIC, "string $name", "void",
		  vm_builtin_PdoSqlite_loadExtension },
		{ "openBlob",        PH7_MOD_PUBLIC,
		  "string $table, string $column, int $rowid, ?string $dbname = 'main', "
		  "int $flags = Pdo\\Sqlite::OPEN_READONLY", 0, vm_builtin_pdo_sqlite_stub },
		{ "setAuthorizer",   PH7_MOD_PUBLIC, "?callable $callback", "void",
		  vm_builtin_PdoSqlite_setAuthorizer },
	};
	static const PH7_NativeClassSpec sSpec = {
		"Pdo\\Sqlite", "PDO", 0, 0,
		aMethod, SX_ARRAYSIZE(aMethod),
		aConst, SX_ARRAYSIZE(aConst),
		0, 0,
		0, 0, 0
	};
#undef PDO_SQLITE_INT_CONST
	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
}

#else
/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */
typedef int vm_pdo_sqlite_unused;
#endif /* PH7_ENABLE_SQLITE */
