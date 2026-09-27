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
	int iOpenFlags;               /* Pdo\Sqlite::ATTR_OPEN_FLAGS, read at open time */
	char *zStmtClass;             /* ATTR_STATEMENT_CLASS: what query()/prepare() build */
	int nStmtClass;
	ph7_value *pStmtArgs;         /* ...and the CONSTRUCTOR arguments it builds them with,
	                               * which php passes to the class's own (non-public)
	                               * constructor at every query()/prepare(). 0 when the
	                               * attribute was set without them. */
	int iErrState;                /* PDO_ERR_* */
	char zSqlState[6];            /* "HY000" and friends; always NUL-terminated */
	int iDrvCode;                 /* sqlite's own result code */
	char *zDrvMsg;                /* sqlite's own message, VM-allocated, or 0 */
	int bNoDrvDetail;             /* the failure came from the LAYER, not the database,
	                               * so errorInfo() reports [state, null, null] */
	struct phl_pdo_stmt *pStmts;  /* statements prepared on this connection: they must be
	                               * finalized before its handle can close */
	struct phl_pdo_udf *pUdfs;    /* createFunction/createCollation callbacks, kept alive
	                               * for as long as sqlite may call them */
	sxi32 iCallbackExc;           /* the status a callback threw with, PARKED: sqlite has to
	                               * finish unwinding before the engine may raise it, and
	                               * the verb that started the step answers exactly this */
	phl_pdo *pNext;
};

/* php's PDO::PARAM_* -- the ones a value can be bound AS. */
#define PDO_PARAM_NULL   0
#define PDO_PARAM_INT    1
#define PDO_PARAM_STR    2
#define PDO_PARAM_LOB    3
#define PDO_PARAM_STMT   4
#define PDO_PARAM_BOOL   5
/* The two flag bits php ORs into a type; neither changes how sqlite binds. */
#define PDO_PARAM_FLAGS  0xFFFF0000

/*
 * One parameter a script bound BEFORE execute().  bindValue() copies the value
 * here; bindParam() records the caller's variable SLOT instead and reads it at
 * execute time, which is what makes a later write to that variable the one the
 * statement runs with.
 */
typedef struct phl_pdo_bind phl_pdo_bind;
struct phl_pdo_bind {
	char *zName;                  /* ":id" as the script spelled it, or 0 for positional */
	int nName;
	int iPos;                     /* 1-based position; 0 when named */
	int iType;                    /* PDO_PARAM_* */
	sxu32 nSlot;                  /* bindParam: the caller's memobj index (SXU32_HIGH = none) */
	ph7_value *pVal;              /* bindValue: this statement's own copy */
	phl_pdo_bind *pNext;
};
/* debugDumpParams() prints the bindings in the order they were MADE, so the
 * list is walked backwards -- it is built by prepending. */

/*
 * One statement.  It is a FORWARD cursor and nothing more: php's sqlite driver
 * steps once at execute() so columnCount() can answer, holds that row for the
 * first fetch(), and steps again per fetch after that. Nothing rewinds -- a
 * second foreach over the same statement walks nothing, which is php.
 */
typedef struct phl_pdo_stmt phl_pdo_stmt;
struct phl_pdo_stmt {
	sqlite3_stmt *pStmt;          /* 0 once finalized */
	phl_pdo *pConn;               /* the connection it was prepared on */
	ph7_class_instance *pOwner;
	int bExecuted;                /* execute() has run at least once */
	int bRowPending;              /* a stepped row is waiting for the next fetch */
	int bDone;                    /* the cursor is past the last row */
	ph7_int64 nChanges;           /* what rowCount() answers: the WRITE's row count */
	int iFetchMode;               /* PDO::FETCH_* for a fetch() given none */
	int iFetchColumn;             /* setFetchMode(FETCH_COLUMN, n)'s column */
	char *zFetchClass;            /* setFetchMode(FETCH_CLASS, name)'s class */
	int nFetchClass;
	ph7_value *pFetchArgs;        /* its constructor arguments, or 0 */
	ph7_class_instance *pFetchInto; /* setFetchMode(FETCH_INTO, $obj)'s object */
	/* A statement carries its OWN SQLSTATE. php keeps one per object and shares
	 * only the DRIVER's code and message (which live on the connection), so a
	 * failed statement leaves the connection reading "00000" while its own
	 * errorInfo() reports the failure. */
	int iErrState;                /* PDO_ERR_* */
	char zSqlState[6];
	ph7_class_instance *pConnObj; /* the PDO object, retained: a statement outliving
	                               * its connection's last reference must not lose
	                               * the database under it */
	/* PDO::FETCH_LAZY's one row object, and the row it reads.
	 *
	 * php's lazy row is a VIEW of the statement's current cursor position: one
	 * object per statement, handed back by every lazy fetch, whose columns are
	 * read when the SCRIPT asks for them and therefore move with the walk.  This
	 * driver steps AHEAD (a row is stepped at execute() so columnCount() can
	 * answer, and again after each fetch), so the cursor is already past the row
	 * the script is holding -- the values are captured instead, RAW, at every
	 * fetch that hands a row out, and php's presentation rules (ATTR_CASE is
	 * already in the names, STRINGIFY_FETCHES and ORACLE_NULLS are not) are
	 * applied when the property is read.  That is what keeps an attribute
	 * changed between two reads visible in the second, as php's is.
	 */
	ph7_class_instance *pLazyRow;  /* the object, NOT retained: it retains the STATEMENT,
	                                * and its own release clears this pointer */
	ph7_value *pLazyVals;          /* the captured row, positionally; 0 when the cursor
	                                * has produced none (every column then reads null).
	                                * The NAMES are not captured beside it: they belong
	                                * to the prepared statement, which the row keeps
	                                * alive, and php answers them for as long as it
	                                * exists -- a walk that has run out still prints
	                                * every column, each holding null. */
	phl_pdo_bind *pBinds;         /* what bindValue()/bindParam() recorded */
	phl_pdo_bind *pColBinds;      /* what bindColumn() recorded: the same record, read the
	                               * other way -- a COLUMN and the variable it writes to */
	phl_pdo_stmt *pNext;
};

/*
 * One userland callback registered on a connection: a scalar function, an
 * aggregate's step/finalize pair, or a collation.  The connection owns the
 * ph7_value holding the callable, because sqlite will call it long after the
 * registering call has returned.
 */
typedef struct phl_pdo_udf phl_pdo_udf;
struct phl_pdo_udf {
	phl_pdo *pConn;
	ph7_value *pCallback;         /* the callable itself */
	ph7_value *pFinalize;         /* an aggregate's second half, or 0 */
	char *zName;                  /* the SQL name, for the registry's own bookkeeping */
	phl_pdo_udf *pNext;
};
/*
 * One aggregate in progress.  sqlite hands the same scratch buffer to every
 * step of one group and then to the finalizer, which is where php keeps its
 * running context and row count -- both visible to the callbacks as their
 * first two arguments.
 */
typedef struct phl_pdo_agg phl_pdo_agg;
struct phl_pdo_agg {
	ph7_value *pCtx;              /* whatever the last step returned */
	int nRow;                     /* rows seen so far */
};

/* vm_pdo.c -- the class library and the shared machinery. */
PH7_PRIVATE phl_pdo * PH7_PdoNewConn(ph7_vm *pVm);
/* The connection behind a PDO object's hidden slot (the driver unit needs it too). */
PH7_PRIVATE phl_pdo * PH7_PdoConnOfInstance(ph7_class_instance *pThis);
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
/* The statement-side twins: same routing, the statement's own SQLSTATE. */
PH7_PRIVATE sxi32 PH7_PdoRaiseStmt(ph7_context *pCtx,phl_pdo_stmt *pSt,const char *zFn);
PH7_PRIVATE sxi32 PH7_PdoRaiseImplStmt(ph7_context *pCtx,phl_pdo_stmt *pSt,const char *zFn,
	const char *zSqlState,const char *zMsg);

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
PH7_PRIVATE ph7_int64 PH7_PdoSqliteChanges(phl_pdo *pConn);
/* Whether a transaction is open: sqlite's own autocommit flag, so a BEGIN the
 * script sent through exec() counts exactly as beginTransaction() does. */
PH7_PRIVATE int PH7_PdoSqliteInTransaction(phl_pdo *pConn);
/* Register a userland scalar function or collation on the connection. */
PH7_PRIVATE int PH7_PdoSqliteAddFunction(phl_pdo_udf *pUdf,const char *zName,int nArg,int iFlags);
PH7_PRIVATE int PH7_PdoSqliteAddCollation(phl_pdo_udf *pUdf,const char *zName);
PH7_PRIVATE int PH7_PdoSqliteAddAggregate(phl_pdo_udf *pUdf,const char *zName,int nArg);
PH7_PRIVATE void PH7_PdoSqliteSetAuthorizer(phl_pdo *pConn,phl_pdo_udf *pUdf);
PH7_PRIVATE int PH7_PdoSqliteStmtReadonly(phl_pdo_stmt *pSt);
PH7_PRIVATE int PH7_PdoSqliteStmtBusy(phl_pdo_stmt *pSt);
PH7_PRIVATE int PH7_PdoSqliteLoadExtension(phl_pdo *pConn,const char *zName);
PH7_PRIVATE void PH7_PdoSqliteExtendedCodes(phl_pdo *pConn,int bOn);
/* Statement plumbing. Prepare answers 0 on failure with the connection's error
 * set; step answers 1 (a row), 0 (finished) or -1 (failed). */
PH7_PRIVATE int PH7_PdoSqlitePrepare(phl_pdo_stmt *pSt,const char *zSql,int nSql);
PH7_PRIVATE int PH7_PdoSqliteStep(phl_pdo_stmt *pSt);
PH7_PRIVATE void PH7_PdoSqliteFinalize(phl_pdo_stmt *pSt);
PH7_PRIVATE int PH7_PdoSqliteColumnCount(phl_pdo_stmt *pSt);
/* Rewind a statement so it can run again, dropping the previous run's values. */
PH7_PRIVATE void PH7_PdoSqliteReset(phl_pdo_stmt *pSt);
/* Bind one value. iPos is 1-based; a named parameter resolves through
 * sqlite3_bind_parameter_index first, and an unknown name lands on index 0 --
 * which is what makes php answer "column index out of range". */
PH7_PRIVATE int PH7_PdoSqliteBindAt(phl_pdo_stmt *pSt,int iPos,int iType,ph7_value *pVal);
PH7_PRIVATE int PH7_PdoSqliteBindIndexOf(phl_pdo_stmt *pSt,const char *zName,int nName);
PH7_PRIVATE const char * PH7_PdoSqliteColumnName(phl_pdo_stmt *pSt,int iCol);
/* getColumnMeta()'s two driver-side answers: the DECLARED type of a column
 * (sqlite's own "sqlite:decl_type") and the table it came from. */
PH7_PRIVATE const char * PH7_PdoSqliteColumnDecl(phl_pdo_stmt *pSt,int iCol);
PH7_PRIVATE const char * PH7_PdoSqliteColumnTable(phl_pdo_stmt *pSt,int iCol);
/* The runtime type of the column in the row at the cursor. */
PH7_PRIVATE int PH7_PdoSqliteColumnType(phl_pdo_stmt *pSt,int iCol);
/* sqlite's result code from the statement's last step (100 = a row is up). */
PH7_PRIVATE int PH7_PdoSqliteLastStepCode(phl_pdo_stmt *pSt);
/* Write column iCol of the row at the cursor into pOut, with sqlite's own type. */
PH7_PRIVATE void PH7_PdoSqliteColumnValue(phl_pdo_stmt *pSt,int iCol,ph7_value *pOut);

/* vm_pdo.c -- statement lifetime, shared by both units. */
PH7_PRIVATE phl_pdo_stmt * PH7_PdoNewStmt(phl_pdo *pConn);
PH7_PRIVATE void PH7_PdoFreeStmt(phl_pdo_stmt *pSt);

#endif /* PH7_ENABLE_SQLITE */
#endif /* PH7_PDO_INT_H */
