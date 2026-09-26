/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_SQLITE
#include "ph7int.h"

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

/*
 * Every method body below is a placeholder: this commit lands the DECLARED
 * surface (names, signatures, constants, class flags) and the build plumbing
 * that carries libsqlite3, not the behaviour.  The refusal is loud on purpose
 * -- an unimplemented verb must never look like an answer -- and each
 * following slice replaces one group of them with the real body.
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
		  0, vm_builtin_pdo_stub },
		{ "connect", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",
		  "static", vm_builtin_pdo_stub },
		{ "beginTransaction", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_pdo_stub },
		{ "commit",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_pdo_stub },
		{ "errorCode",        PH7_MOD_PUBLIC, "", "@?string", vm_builtin_pdo_stub },
		{ "errorInfo",        PH7_MOD_PUBLIC, "", "@array", vm_builtin_pdo_stub },
		{ "exec",             PH7_MOD_PUBLIC, "string $statement", "@int|false", vm_builtin_pdo_stub },
		{ "getAttribute",     PH7_MOD_PUBLIC, "int $attribute", "@mixed", vm_builtin_pdo_stub },
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
		  vm_builtin_pdo_stub },
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
	static const PH7_NativeClassSpec aSpec[] = {
		/* Both handles refuse `clone` and `serialize`: php declares neither a
		 * clone handler nor a serializer for them, so the copy would carry the
		 * same sqlite3 pointer in its hidden slot. */
		{ "PDO", 0, 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aPdoMethod, SX_ARRAYSIZE(aPdoMethod),
		  aPdoConst, SX_ARRAYSIZE(aPdoConst),
		  0, 0, 0, 0, 0 },
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
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}

#else
/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */
typedef int vm_pdo_unused;
#endif /* PH7_ENABLE_SQLITE */
