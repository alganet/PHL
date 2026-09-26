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
	int iCode = pConn->pDb ? sqlite3_extended_errcode(pConn->pDb) : SQLITE_ERROR;
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
