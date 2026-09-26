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
	switch( iCode & 0xff ){
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
		  vm_builtin_pdo_sqlite_stub },
		{ "createCollation", PH7_MOD_PUBLIC, "string $name, callable $callback", "bool",
		  vm_builtin_pdo_sqlite_stub },
		{ "createFunction",  PH7_MOD_PUBLIC,
		  "string $function_name, callable $callback, int $num_args = -1, int $flags = 0", "bool",
		  vm_builtin_pdo_sqlite_stub },
		{ "loadExtension",   PH7_MOD_PUBLIC, "string $name", "void",
		  vm_builtin_pdo_sqlite_stub },
		{ "openBlob",        PH7_MOD_PUBLIC,
		  "string $table, string $column, int $rowid, ?string $dbname = 'main', "
		  "int $flags = Pdo\\Sqlite::OPEN_READONLY", 0, vm_builtin_pdo_sqlite_stub },
		{ "setAuthorizer",   PH7_MOD_PUBLIC, "?callable $callback", "void",
		  vm_builtin_pdo_sqlite_stub },
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
