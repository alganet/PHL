/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internals shared by ext/pdo's two units: vm_pdo.c (the class library, the
 * attribute and error machinery) and vm_pdo_sqlite.c (every call into
 * libsqlite3).  The compile_int.h pattern -- one header, no public surface.
 *
 * php splits this boundary with a driver vtable because ext/pdo carries many
 * drivers; §10 scopes PHL to one, so the connection record is a single struct
 * both units see and the "driver" is just the half of the code that talks to
 * sqlite3.
 */
#ifndef PH7_PDO_INT_H
#define PH7_PDO_INT_H
#ifdef PH7_ENABLE_SQLITE
#include <sqlite3.h>

/*
 * What errorCode()/errorInfo() answer depends on whether anything has run yet:
 * a fresh handle answers NULL and ["", null, null], a handle whose last
 * operation succeeded answers "00000", and a failed one answers the driver's
 * own triple.  php keeps this as an error_code string on the dbh; the tri-state
 * is what the empty-string-versus-null distinction needs.
 */
#define PDO_ERR_NONE   0   /* nothing has run on this handle yet */
#define PDO_ERR_OK     1   /* the last operation succeeded ("00000") */
#define PDO_ERR_FAILED 2   /* zSqlState/iDrvCode/zDrvMsg describe it */

/* php's PDO::ERRMODE_* / CASE_* / NULL_* / FETCH_* -- spelled here so the C
 * bodies read as the constants the script uses. */
#define PDO_ERRMODE_SILENT     0
#define PDO_ERRMODE_WARNING    1
#define PDO_ERRMODE_EXCEPTION  2
#define PDO_CASE_NATURAL       0
#define PDO_CASE_UPPER         1
#define PDO_CASE_LOWER         2
#define PDO_NULL_NATURAL       0
#define PDO_NULL_EMPTY_STRING  1
#define PDO_NULL_TO_STRING     2
#define PDO_FETCH_BOTH         4

/*
 * One connection.  Lives on the per-VM chain (pVm->pPdoConns) and is reached
 * from its PDO object through the hidden `__res` slot -- the XMLWriter model,
 * which is safe here for the same reason: `clone` is refused, so no second
 * object can ever share the handle.
 */
typedef struct phl_pdo phl_pdo;
struct phl_pdo {
	sqlite3 *pDb;                 /* 0 once closed */
	ph7_vm *pVm;
	ph7_class_instance *pOwner;   /* the object whose slot holds it */
	/* Attributes the driver actually carries.  Everything else is php's
	 * "driver does not support that attribute" (see PdoAttrTable in vm_pdo.c). */
	int iErrMode;                 /* PDO_ERRMODE_* */
	int iCase;                    /* PDO_CASE_* */
	int iOracleNulls;             /* PDO_NULL_* */
	int bStringify;               /* ATTR_STRINGIFY_FETCHES */
	int iDefaultFetch;            /* ATTR_DEFAULT_FETCH_MODE */
	int bPersistent;              /* what ATTR_PERSISTENT reports back */
	int iTxMode;                  /* Pdo\Sqlite::ATTR_TRANSACTION_MODE */
	/* The last operation's outcome, as errorCode()/errorInfo() present it. */
	int bExtendedCodes;           /* Pdo\Sqlite::ATTR_EXTENDED_RESULT_CODES */
	int iErrState;                /* PDO_ERR_* */
	char zSqlState[6];            /* "HY000" and friends; always NUL-terminated */
	int iDrvCode;                 /* sqlite's own result code */
	char *zDrvMsg;                /* sqlite's own message, VM-allocated, or 0 */
	int bNoDrvDetail;             /* the failure came from the LAYER, not the database,
	                               * so errorInfo() reports [state, null, null] */
	phl_pdo *pNext;
};

/* vm_pdo.c -- the class library and the shared machinery. */
PH7_PRIVATE phl_pdo * PH7_PdoNewConn(ph7_vm *pVm);
PH7_PRIVATE void PH7_PdoFreeConn(phl_pdo *pConn);
PH7_PRIVATE void PH7_PdoSetError(phl_pdo *pConn,const char *zSqlState,int iCode,const char *zMsg);
PH7_PRIVATE void PH7_PdoClearError(phl_pdo *pConn);
/* php clears the handle's error at the ENTRY of most verbs, which is why a
 * failure is invisible to errorCode() after the next successful call. */
PH7_PRIVATE void PH7_PdoTouch(phl_pdo *pConn);
PH7_PRIVATE sxi32 PH7_PdoRaise(ph7_context *pCtx,phl_pdo *pConn,const char *zFn);
PH7_PRIVATE sxi32 PH7_PdoRaiseImpl(ph7_context *pCtx,phl_pdo *pConn,const char *zFn,
	const char *zSqlState,const char *zMsg);
PH7_PRIVATE sxi32 PH7_PdoThrowConstruct(ph7_context *pCtx,const char *zSqlState,int iCode,
	const char *zMsg);

/* vm_pdo_sqlite.c -- everything that touches libsqlite3. */
PH7_PRIVATE sxi32 PH7_PdoSqliteOpen(ph7_context *pCtx,phl_pdo *pConn,const char *zPath,
	int nPath,int iFlags);
PH7_PRIVATE void PH7_PdoSqliteClose(phl_pdo *pConn);
PH7_PRIVATE const char * PH7_PdoSqliteLibVersion(void);
PH7_PRIVATE void PH7_PdoSqliteTakeError(phl_pdo *pConn);
/* Run every statement in one string; answers the change count or -1 on failure
 * (with the connection's error already set). */
PH7_PRIVATE ph7_int64 PH7_PdoSqliteExec(phl_pdo *pConn,const char *zSql,int nSql);
PH7_PRIVATE ph7_int64 PH7_PdoSqliteLastInsertId(phl_pdo *pConn);

#endif /* PH7_ENABLE_SQLITE */
#endif /* PH7_PDO_INT_H */
