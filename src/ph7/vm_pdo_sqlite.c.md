# src/ph7/vm_pdo_sqlite.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 559/650 lines (86.00%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#ifdef PH7_ENABLE_SQLITE` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#include "pdo_int.h"` |
|    - |    8 |  |
|    - |    9 | `/*` |
|    - |   10 | ` * ext/pdo_sqlite: the driver.  This unit owns the sqlite3 connection and` |
|    - |   11 | `` * statement handles and the `Pdo\Sqlite` subclass php 8.4 introduced -- the`` |
|    - |   12 | ` * class PDO::connect("sqlite:...") answers, and the only place the` |
|    - |   13 | ` * sqlite-specific verbs (createFunction, createCollation, setAuthorizer, ...)` |
|    - |   14 | ` * are declared.  The class library it extends lives in vm_pdo.c.` |
|    - |   15 | ` *` |
|    - |   16 | ` * §10's non-deprecated rule is what splits the constants: php still carries` |
|    - |   17 | `` * `PDO::SQLITE_OPEN_READONLY` and six siblings, all of them reporting`` |
|    - |   18 | ` * isDeprecated(), and declares the successors here without the prefix.  PHL` |
|    - |   19 | ` * declares the successors only.  The three OPEN_* values and the three` |
|    - |   20 | ` * authorizer verdicts are sqlite's own macros rather than copied numbers --` |
|    - |   21 | ` * they belong to the library, so they are read from its header.` |
|    - |   22 | ` */` |
|    - |   23 |  |
|    - |   24 | `/*` |
|    - |   25 | ` * Copy sqlite's own view of the last failure onto the connection.  php's` |
|    - |   26 | ` * driver maps a handful of result codes to their SQL-standard SQLSTATE and` |
|    - |   27 | ` * leaves everything else at HY000 ("general error"), which is what nearly` |
|    - |   28 | ` * every sqlite failure reports.` |
|    - |   29 | ` */` |
|   36 |   30 | `PH7_PRIVATE void PH7_PdoSqliteTakeError(phl_pdo *pConn)` |
|    1 |   31 | `{` |
|   37 |   32 | `	const char *zSqlState = "HY000";` |
|    - |   33 | `	/* The PRIMARY result code, not the extended one: a UNIQUE violation is 19` |
|    - |   34 | `	 * (SQLITE_CONSTRAINT) and not 2067 (SQLITE_CONSTRAINT_UNIQUE) unless the` |
|    - |   35 | `	 * script asked for extended codes, which is what that driver attribute is` |
|    - |   36 | `	 * for. sqlite reports the extended code from both accessors once they are` |
|    - |   37 | `	 * enabled on the connection, so the choice is made here. */` |
|   73 |   38 | `	int iCode = pConn->pDb` |
|   19 |   39 | `		? (pConn->bExtendedCodes ? sqlite3_extended_errcode(pConn->pDb)` |
|   35 |   40 | `		                         : sqlite3_errcode(pConn->pDb))` |
|   36 |   41 | `		: SQLITE_ERROR;` |
|   37 |   42 | `	const char *zMsg = pConn->pDb ? sqlite3_errmsg(pConn->pDb) : "unknown error";` |
|    - |   43 | `	/* the RAW code, not its low byte: with extended result codes on, a UNIQUE` |
|    - |   44 | `	 * violation reports 1555, which matches none of these and lands on HY000 --` |
|    - |   45 | `	 * php's own answer, and the reason turning extended codes on changes the` |
|    - |   46 | `	 * SQLSTATE and not just the number beside it. */` |
|   37 |   47 | `	switch( iCode ){` |
|  ! 0 |   48 | `		case SQLITE_NOTFOUND:   zSqlState = "42S02"; break;` |
|  ! 0 |   49 | `		case SQLITE_INTERRUPT:  zSqlState = "57014"; break;` |
|  ! 0 |   50 | `		case SQLITE_NOLFS:      zSqlState = "HYC00"; break;` |
|  ! 0 |   51 | `		case SQLITE_TOOBIG:     zSqlState = "22001"; break;` |
|    7 |   52 | `		case SQLITE_CONSTRAINT: zSqlState = "23000"; break;` |
|   24 |   53 | `		case SQLITE_ERROR:` |
|   31 |   54 | `		default:                zSqlState = "HY000"; break;` |
|    - |   55 | `	}` |
|   37 |   56 | `	PH7_PdoSetError(pConn,zSqlState,iCode,zMsg);` |
|   37 |   57 | `}` |
|    - |   58 | `/*` |
|    - |   59 | ` * The library version both ATTR_SERVER_VERSION and ATTR_CLIENT_VERSION answer.` |
|    - |   60 | ` * It is the LINKED library's, so it differs between this engine's platforms` |
|    - |   61 | ` * (3.45 on a Debian host, whatever vcpkg last shipped on Windows) -- which is` |
|    - |   62 | ` * why no test may pin it.` |
|    - |   63 | ` */` |
|    6 |   64 | `PH7_PRIVATE const char * PH7_PdoSqliteLibVersion(void)` |
|    1 |   65 | `{` |
|    7 |   66 | `	return sqlite3_libversion();` |
|    1 |   67 | `}` |
|    - |   68 | `/*` |
|    - |   69 | ` * Open one database.  php hands sqlite3_open_v2 the DSN's path verbatim, so` |
|    - |   70 | ` * every spelling sqlite itself understands is a spelling PDO understands: a` |
|    - |   71 | ` * relative or absolute path, the empty string (a private temporary database on` |
|    - |   72 | `` * disk), `:memory:`, and the `file:...?mode=` URI form.`` |
|    - |   73 | ` *` |
|    - |   74 | ` * A failure here is NOT routed through the error mode: php's constructor` |
|    - |   75 | ` * always throws PDOException, whatever ATTR_ERRMODE the options asked for, and` |
|    - |   76 | ` * the exception carries sqlite's own code as its $code (an int, unlike the` |
|    - |   77 | ` * SQLSTATE string a later failure reports).` |
|    - |   78 | ` */` |
|  128 |   79 | `PH7_PRIVATE sxi32 PH7_PdoSqliteOpen(ph7_context *pCtx,phl_pdo *pConn,const char *zPath,` |
|    - |   80 | `	int nPath,int iFlags)` |
|    3 |   81 | `{` |
|    - |   82 | `	char *zTerm;` |
|    - |   83 | `	int rc;` |
|    - |   84 | `	/* sqlite3_open_v2 wants a C string and the DSN slice is not one. */` |
|  131 |   85 | `	zTerm = (char *)SyMemBackendAlloc(&pConn->pVm->sAllocator,(sxu32)nPath + 1);` |
|  131 |   86 | `	if( zTerm == 0 ){` |
|  ! 0 |   87 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |   88 | `	}` |
|  131 |   89 | `	if( nPath > 0 ){` |
|  129 |   90 | `		SyMemcpy(zPath,zTerm,(sxu32)nPath);` |
|   63 |   91 | `	}` |
|  131 |   92 | `	zTerm[nPath] = 0;` |
|  131 |   93 | `	rc = sqlite3_open_v2(zTerm,&pConn->pDb,iFlags,0);` |
|  131 |   94 | `	SyMemBackendFree(&pConn->pVm->sAllocator,zTerm);` |
|  131 |   95 | `	if( rc != SQLITE_OK ){` |
|    - |   96 | `		/* sqlite3_open_v2 hands back a handle even on failure so the message can` |
|    - |   97 | `		 * be read off it; take the message first, then close. */` |
|    9 |   98 | `		const char *zMsg = pConn->pDb ? sqlite3_errmsg(pConn->pDb) : sqlite3_errstr(rc);` |
|    - |   99 | `		sxi32 rcThrow;` |
|    - |  100 | `		SyBlob sMsg;` |
|    9 |  101 | `		SyBlobInit(&sMsg,&pConn->pVm->sAllocator);` |
|    9 |  102 | `		SyBlobAppend(&sMsg,zMsg,SyStrlen(zMsg));` |
|    9 |  103 | `		SyBlobAppend(&sMsg,"",1);` |
|    9 |  104 | `		rcThrow = PH7_PdoThrowConstruct(pCtx,"HY000",rc,(const char *)SyBlobData(&sMsg));` |
|    9 |  105 | `		SyBlobRelease(&sMsg);` |
|    9 |  106 | `		PH7_PdoSqliteClose(pConn);` |
|    9 |  107 | `		return rcThrow;` |
|    - |  108 | `	}` |
|  123 |  109 | `	return PH7_OK;` |
|   67 |  110 | `}` |
|    - |  111 | `/*` |
|    - |  112 | ` * Close one database.  Every statement this connection prepared must already` |
|    - |  113 | ` * be finalized (later slices own that); sqlite3_close_v2 is used so a leaked` |
|    - |  114 | ` * one defers the close rather than leaking the handle itself.` |
|    - |  115 | ` */` |
|  246 |  116 | `PH7_PRIVATE void PH7_PdoSqliteClose(phl_pdo *pConn)` |
|    3 |  117 | `{` |
|  249 |  118 | `	if( pConn->pDb ){` |
|  131 |  119 | `		sqlite3_close_v2(pConn->pDb);` |
|  131 |  120 | `		pConn->pDb = 0;` |
|   64 |  121 | `	}` |
|  249 |  122 | `}` |
|    - |  123 |  |
|    - |  124 | `/*` |
|    - |  125 | ` * PDO::exec()'s work: prepare, step and finalize every statement the string` |
|    - |  126 | `` * holds, one after another.  php runs them ALL -- `INSERT ...; INSERT ...;` is`` |
|    - |  127 | ` * two inserts -- and answers sqlite3_changes(), which is the count from the` |
|    - |  128 | ` * LAST statement that changed anything and is left ALONE by a SELECT or by a` |
|    - |  129 | ` * statement that changes nothing. That is why exec() over a SELECT answers` |
|    - |  130 | ` * whatever the previous write answered rather than 0.` |
|    - |  131 | ` */` |
|  196 |  132 | `PH7_PRIVATE ph7_int64 PH7_PdoSqliteExec(phl_pdo *pConn,const char *zSql,int nSql)` |
|    3 |  133 | `{` |
|  199 |  134 | `	const char *zTail = zSql;` |
|  199 |  135 | `	const char *zEnd = zSql + nSql;` |
|  199 |  136 | `	if( pConn->pDb == 0 ){` |
|  ! 0 |  137 | `		return -1;` |
|    - |  138 | `	}` |
|  381 |  139 | `	while( zTail < zEnd ){` |
|  201 |  140 | `		sqlite3_stmt *pStmt = 0;` |
|  201 |  141 | `		const char *zNext = 0;` |
|  201 |  142 | `		int rc = sqlite3_prepare_v2(pConn->pDb,zTail,(int)(zEnd - zTail),&pStmt,&zNext);` |
|  201 |  143 | `		if( rc != SQLITE_OK ){` |
|   11 |  144 | `			PH7_PdoSqliteTakeError(pConn);` |
|   14 |  145 | `			return -1;` |
|    - |  146 | `		}` |
|  191 |  147 | `		if( pStmt == 0 ){` |
|    - |  148 | `			/* whitespace or a comment: nothing to run, and php answers the` |
|    - |  149 | `			 * change count it already had rather than an error */` |
|    5 |  150 | `			if( zNext == 0 \|\| zNext <= zTail ){` |
|  ! 0 |  151 | `				break;` |
|    - |  152 | `			}` |
|    5 |  153 | `			zTail = zNext;` |
|    5 |  154 | `			continue;` |
|    - |  155 | `		}` |
|   92 |  156 | `		do{` |
|  193 |  157 | `			rc = sqlite3_step(pStmt);` |
|  193 |  158 | `		}while( rc == SQLITE_ROW );` |
|  187 |  159 | `		if( rc != SQLITE_DONE ){` |
|    7 |  160 | `			PH7_PdoSqliteTakeError(pConn);` |
|    7 |  161 | `			sqlite3_finalize(pStmt);` |
|    7 |  162 | `			return -1;` |
|    - |  163 | `		}` |
|  181 |  164 | `		sqlite3_finalize(pStmt);` |
|  181 |  165 | `		zTail = zNext ? zNext : zEnd;` |
|    3 |  166 | `	}` |
|  183 |  167 | `	return (ph7_int64)sqlite3_changes(pConn->pDb);` |
|  101 |  168 | `}` |
|   10 |  169 | `PH7_PRIVATE ph7_int64 PH7_PdoSqliteLastInsertId(phl_pdo *pConn)` |
|    1 |  170 | `{` |
|   11 |  171 | `	return pConn->pDb ? (ph7_int64)sqlite3_last_insert_rowid(pConn->pDb) : 0;` |
|    1 |  172 | `}` |
|    8 |  173 | `PH7_PRIVATE ph7_int64 PH7_PdoSqliteChanges(phl_pdo *pConn)` |
|    1 |  174 | `{` |
|    9 |  175 | `	return pConn->pDb ? (ph7_int64)sqlite3_changes(pConn->pDb) : 0;` |
|    1 |  176 | `}` |
|    - |  177 | `/*` |
|    - |  178 | ` * sqlite's own autocommit flag rather than a counter of our own: php reads it` |
|    - |  179 | `` * too, which is why `exec("BEGIN")` makes inTransaction() answer true and`` |
|    - |  180 | ` * beginTransaction() refuse -- the driver has no idea who opened it.` |
|    - |  181 | ` */` |
|    - |  182 | `/* Pdo\Sqlite::ATTR_READONLY_STATEMENT / ATTR_BUSY_STATEMENT: sqlite answers` |
|    - |  183 | ` * both about a compiled statement. */` |
|    2 |  184 | `PH7_PRIVATE int PH7_PdoSqliteStmtReadonly(phl_pdo_stmt *pSt)` |
|    1 |  185 | `{` |
|    3 |  186 | `	return pSt->pStmt ? sqlite3_stmt_readonly(pSt->pStmt) : 0;` |
|    1 |  187 | `}` |
|    2 |  188 | `PH7_PRIVATE int PH7_PdoSqliteStmtBusy(phl_pdo_stmt *pSt)` |
|    1 |  189 | `{` |
|    3 |  190 | `	return pSt->pStmt ? sqlite3_stmt_busy(pSt->pStmt) : 0;` |
|    1 |  191 | `}` |
|    - |  192 | `/*` |
|    - |  193 | ` * Pdo\Sqlite::loadExtension().  sqlite keeps extension loading OFF by default,` |
|    - |  194 | ` * so it is enabled around the one call and turned back off -- php does the` |
|    - |  195 | ` * same, and leaving it on would let any later SQL load code.` |
|    - |  196 | ` */` |
|    2 |  197 | `PH7_PRIVATE int PH7_PdoSqliteLoadExtension(phl_pdo *pConn,const char *zName)` |
|    1 |  198 | `{` |
|    - |  199 | `	int rc;` |
|    3 |  200 | `	char *zErr = 0;` |
|    3 |  201 | `	if( pConn->pDb == 0 ){` |
|  ! 0 |  202 | `		return 0;` |
|    - |  203 | `	}` |
|    3 |  204 | `	sqlite3_enable_load_extension(pConn->pDb,1);` |
|    3 |  205 | `	rc = sqlite3_load_extension(pConn->pDb,zName,0,&zErr);` |
|    3 |  206 | `	sqlite3_enable_load_extension(pConn->pDb,0);` |
|    3 |  207 | `	if( zErr ){` |
|    3 |  208 | `		sqlite3_free(zErr);` |
|    1 |  209 | `	}` |
|    3 |  210 | `	return rc == SQLITE_OK;` |
|    2 |  211 | `}` |
|    2 |  212 | `PH7_PRIVATE void PH7_PdoSqliteExtendedCodes(phl_pdo *pConn,int bOn)` |
|    1 |  213 | `{` |
|    3 |  214 | `	if( pConn->pDb ){` |
|    3 |  215 | `		sqlite3_extended_result_codes(pConn->pDb,bOn ? 1 : 0);` |
|    1 |  216 | `	}` |
|    3 |  217 | `}` |
|   44 |  218 | `PH7_PRIVATE int PH7_PdoSqliteInTransaction(phl_pdo *pConn)` |
|    1 |  219 | `{` |
|   45 |  220 | `	return pConn->pDb ? (sqlite3_get_autocommit(pConn->pDb) == 0) : 0;` |
|    1 |  221 | `}` |
|    - |  222 | `/*` |
|    - |  223 | ` * Prepare ONE statement.  php compiles only the first statement of the string` |
|    - |  224 | ` * here -- unlike exec(), which runs them all -- and what follows it is simply` |
|    - |  225 | ` * not executed.` |
|    - |  226 | ` */` |
|  282 |  227 | `PH7_PRIVATE int PH7_PdoSqlitePrepare(phl_pdo_stmt *pSt,const char *zSql,int nSql)` |
|    2 |  228 | `{` |
|    - |  229 | `	int rc;` |
|  284 |  230 | `	if( pSt->pConn->pDb == 0 ){` |
|  ! 0 |  231 | `		return 0;` |
|    - |  232 | `	}` |
|  284 |  233 | `	rc = sqlite3_prepare_v2(pSt->pConn->pDb,zSql,nSql,&pSt->pStmt,0);` |
|  284 |  234 | `	if( rc != SQLITE_OK \|\| pSt->pStmt == 0 ){` |
|    9 |  235 | `		PH7_PdoSqliteTakeError(pSt->pConn);` |
|    9 |  236 | `		if( pSt->pStmt ){` |
|  ! 0 |  237 | `			sqlite3_finalize(pSt->pStmt);` |
|  ! 0 |  238 | `			pSt->pStmt = 0;` |
|  ! 0 |  239 | `		}` |
|    9 |  240 | `		return 0;` |
|    - |  241 | `	}` |
|  276 |  242 | `	return 1;` |
|  143 |  243 | `}` |
|    - |  244 | `/*` |
|    - |  245 | ` * One step of the cursor: 1 when a row is available, 0 when the walk is over,` |
|    - |  246 | ` * -1 on failure (with the connection's error set).` |
|    - |  247 | ` */` |
|  564 |  248 | `PH7_PRIVATE int PH7_PdoSqliteStep(phl_pdo_stmt *pSt)` |
|    2 |  249 | `{` |
|    - |  250 | `	int rc;` |
|  566 |  251 | `	if( pSt->pStmt == 0 ){` |
|  ! 0 |  252 | `		return 0;` |
|    - |  253 | `	}` |
|  566 |  254 | `	rc = sqlite3_step(pSt->pStmt);` |
|  566 |  255 | `	if( rc == SQLITE_ROW ){` |
|  386 |  256 | `		return 1;` |
|    - |  257 | `	}` |
|  181 |  258 | `	if( rc == SQLITE_DONE ){` |
|  177 |  259 | `		return 0;` |
|    - |  260 | `	}` |
|    5 |  261 | `	PH7_PdoSqliteTakeError(pSt->pConn);` |
|    5 |  262 | `	return -1;` |
|  284 |  263 | `}` |
|  526 |  264 | `PH7_PRIVATE void PH7_PdoSqliteFinalize(phl_pdo_stmt *pSt)` |
|    2 |  265 | `{` |
|  528 |  266 | `	if( pSt->pStmt ){` |
|  276 |  267 | `		sqlite3_finalize(pSt->pStmt);` |
|  276 |  268 | `		pSt->pStmt = 0;` |
|  137 |  269 | `	}` |
|  528 |  270 | `}` |
|  652 |  271 | `PH7_PRIVATE int PH7_PdoSqliteColumnCount(phl_pdo_stmt *pSt)` |
|    2 |  272 | `{` |
|  654 |  273 | `	return pSt->pStmt ? sqlite3_column_count(pSt->pStmt) : 0;` |
|    2 |  274 | `}` |
|    - |  275 | `/*` |
|    - |  276 | ` * Rewind a statement so it can run again.  The bindings go too: php re-binds` |
|    - |  277 | ` * everything on every execute(), so a value bound for the previous run must` |
|    - |  278 | ` * not survive into the next one.` |
|    - |  279 | ` */` |
|   50 |  280 | `PH7_PRIVATE void PH7_PdoSqliteReset(phl_pdo_stmt *pSt)` |
|    2 |  281 | `{` |
|   52 |  282 | `	if( pSt->pStmt ){` |
|   52 |  283 | `		sqlite3_reset(pSt->pStmt);` |
|   52 |  284 | `		sqlite3_clear_bindings(pSt->pStmt);` |
|   25 |  285 | `	}` |
|   52 |  286 | `}` |
|    - |  287 | `/*` |
|    - |  288 | ` * Where a NAMED parameter sits.  sqlite answers 0 for a name the statement` |
|    - |  289 | ` * does not have, and binding at 0 is SQLITE_RANGE -- which is exactly how php` |
|    - |  290 | ` * ends up reporting "column index out of range" for a misspelled placeholder` |
|    - |  291 | ` * rather than something that names it.` |
|    - |  292 | ` */` |
|   16 |  293 | `PH7_PRIVATE int PH7_PdoSqliteBindIndexOf(phl_pdo_stmt *pSt,const char *zName,int nName)` |
|    1 |  294 | `{` |
|    - |  295 | `	char zBuf[128];` |
|   17 |  296 | `	int n = 0;` |
|   17 |  297 | `	if( pSt->pStmt == 0 \|\| nName < 1 ){` |
|  ! 0 |  298 | `		return 0;` |
|    - |  299 | `	}` |
|    - |  300 | `	/* php accepts a name with or without its colon and sqlite wants it WITH,` |
|    - |  301 | `	 * so the missing one is supplied here. */` |
|   17 |  302 | `	if( zName[0] != ':' ){` |
|    5 |  303 | `		zBuf[n++] = ':';` |
|    2 |  304 | `	}` |
|   59 |  305 | `	while( n < (int)sizeof(zBuf) - 1 && n - (zName[0] != ':' ? 1 : 0) < nName ){` |
|   43 |  306 | `		zBuf[n] = zName[n - (zName[0] != ':' ? 1 : 0)];` |
|   43 |  307 | `		++n;` |
|    1 |  308 | `	}` |
|   17 |  309 | `	zBuf[n] = 0;` |
|   17 |  310 | `	return sqlite3_bind_parameter_index(pSt->pStmt,zBuf);` |
|    9 |  311 | `}` |
|    - |  312 | `/*` |
|    - |  313 | ` * Bind one value at a 1-based position.  php's PARAM_* decides the CAST, not` |
|    - |  314 | ` * the value's own type: PARAM_INT over the float 1.9 binds 1, PARAM_STR over` |
|    - |  315 | ` * the same binds "1.5", and PARAM_NULL binds null whatever it was handed. The` |
|    - |  316 | ` * one type that outranks the declaration is php's own null, which binds as` |
|    - |  317 | ` * NULL through any of them.` |
|    - |  318 | ` */` |
|   62 |  319 | `PH7_PRIVATE int PH7_PdoSqliteBindAt(phl_pdo_stmt *pSt,int iPos,int iType,ph7_value *pVal)` |
|    1 |  320 | `{` |
|    - |  321 | `	int rc;` |
|   63 |  322 | `	if( pSt->pStmt == 0 ){` |
|  ! 0 |  323 | `		return 0;` |
|    - |  324 | `	}` |
|   63 |  325 | `	iType &= ~PDO_PARAM_FLAGS;` |
|   63 |  326 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) \|\| iType == PDO_PARAM_NULL ){` |
|    7 |  327 | `		rc = sqlite3_bind_null(pSt->pStmt,iPos);` |
|    4 |  328 | `	}else{` |
|   57 |  329 | `		switch( iType ){` |
|    3 |  330 | `			case PDO_PARAM_INT:` |
|    7 |  331 | `				rc = sqlite3_bind_int64(pSt->pStmt,iPos,(sqlite3_int64)ph7_value_to_int64(pVal));` |
|    7 |  332 | `				break;` |
|    2 |  333 | `			case PDO_PARAM_BOOL:` |
|    5 |  334 | `				rc = sqlite3_bind_int(pSt->pStmt,iPos,ph7_value_to_bool(pVal) ? 1 : 0);` |
|    5 |  335 | `				break;` |
|    1 |  336 | `			case PDO_PARAM_LOB: {` |
|    3 |  337 | `				int nByte = 0;` |
|    3 |  338 | `				const char *zVal = ph7_value_to_string(pVal,&nByte);` |
|    3 |  339 | `				rc = sqlite3_bind_blob(pSt->pStmt,iPos,zVal,nByte,SQLITE_TRANSIENT);` |
|    3 |  340 | `				break;` |
|    - |  341 | `			}` |
|   22 |  342 | `			default: {` |
|   45 |  343 | `				int nByte = 0;` |
|   45 |  344 | `				const char *zVal = ph7_value_to_string(pVal,&nByte);` |
|   45 |  345 | `				rc = sqlite3_bind_text(pSt->pStmt,iPos,zVal,nByte,SQLITE_TRANSIENT);` |
|   44 |  346 | `				break;` |
|    - |  347 | `			}` |
|    - |  348 | `		}` |
|    - |  349 | `	}` |
|   63 |  350 | `	if( rc != SQLITE_OK ){` |
|    9 |  351 | `		PH7_PdoSqliteTakeError(pSt->pConn);` |
|    9 |  352 | `		return 0;` |
|    - |  353 | `	}` |
|   55 |  354 | `	return 1;` |
|   32 |  355 | `}` |
|  596 |  356 | `PH7_PRIVATE const char * PH7_PdoSqliteColumnName(phl_pdo_stmt *pSt,int iCol)` |
|    1 |  357 | `{` |
|  597 |  358 | `	const char *zName = pSt->pStmt ? sqlite3_column_name(pSt->pStmt,iCol) : 0;` |
|  597 |  359 | `	return zName ? zName : "";` |
|    1 |  360 | `}` |
|    - |  361 | `/*` |
|    - |  362 | ` * The type the SCHEMA declares for a column, which is not the type of the` |
|    - |  363 | ` * value in it: a column declared TEXT holding NULL reports decl_type "TEXT"` |
|    - |  364 | ` * and native_type "null". An expression has no declared type at all.` |
|    - |  365 | ` */` |
|    4 |  366 | `PH7_PRIVATE const char * PH7_PdoSqliteColumnDecl(phl_pdo_stmt *pSt,int iCol)` |
|    1 |  367 | `{` |
|    5 |  368 | `	return pSt->pStmt ? sqlite3_column_decltype(pSt->pStmt,iCol) : 0;` |
|    1 |  369 | `}` |
|    - |  370 | `/*` |
|    - |  371 | ` * The table a column came from.  sqlite compiles this one only under` |
|    - |  372 | ` * SQLITE_ENABLE_COLUMN_METADATA, so the SYMBOL's presence is the feature's:` |
|    - |  373 | ` * verified present in the Debian and vcpkg libraries this engine links, and` |
|    - |  374 | ` * php reports the same key from the same call. A platform whose sqlite lacks` |
|    - |  375 | ` * it would fail to LINK rather than answer differently -- and its php would be` |
|    - |  376 | ` * missing the key too.` |
|    - |  377 | ` */` |
|    4 |  378 | `PH7_PRIVATE const char * PH7_PdoSqliteColumnTable(phl_pdo_stmt *pSt,int iCol)` |
|    1 |  379 | `{` |
|    5 |  380 | `	return pSt->pStmt ? sqlite3_column_table_name(pSt->pStmt,iCol) : 0;` |
|    1 |  381 | `}` |
|    4 |  382 | `PH7_PRIVATE int PH7_PdoSqliteColumnType(phl_pdo_stmt *pSt,int iCol)` |
|    1 |  383 | `{` |
|    5 |  384 | `	return pSt->pStmt ? sqlite3_column_type(pSt->pStmt,iCol) : SQLITE_NULL;` |
|    1 |  385 | `}` |
|    - |  386 | `/*` |
|    - |  387 | ` * What the statement's last step answered.  php reports THIS as the driver` |
|    - |  388 | ` * code when getColumnMeta() is asked for a column that does not exist, which` |
|    - |  389 | ` * is how a plain out-of-range index comes back as "100 another row available".` |
|    - |  390 | ` */` |
|    2 |  391 | `PH7_PRIVATE int PH7_PdoSqliteLastStepCode(phl_pdo_stmt *pSt)` |
|    1 |  392 | `{` |
|    3 |  393 | `	return pSt->bRowPending ? SQLITE_ROW : SQLITE_DONE;` |
|    1 |  394 | `}` |
|    - |  395 | `/*` |
|    - |  396 | ` * One column of the row at the cursor, in sqlite's OWN type: an INTEGER comes` |
|    - |  397 | ` * back as an int and a REAL as a float, which is why a fetch from this driver` |
|    - |  398 | ` * is not all-strings the way a stringifying one is. A BLOB is a php string of` |
|    - |  399 | ` * those bytes, NUL bytes included, so the length has to come from sqlite` |
|    - |  400 | ` * rather than from the pointer.` |
|    - |  401 | ` */` |
|  598 |  402 | `PH7_PRIVATE void PH7_PdoSqliteColumnValue(phl_pdo_stmt *pSt,int iCol,ph7_value *pOut)` |
|    1 |  403 | `{` |
|  599 |  404 | `	if( pSt->pStmt == 0 ){` |
|  ! 0 |  405 | `		ph7_value_null(pOut);` |
|  ! 0 |  406 | `		return;` |
|    - |  407 | `	}` |
|  599 |  408 | `	switch( sqlite3_column_type(pSt->pStmt,iCol) ){` |
|  120 |  409 | `		case SQLITE_INTEGER:` |
|  241 |  410 | `			ph7_value_int64(pOut,(ph7_int64)sqlite3_column_int64(pSt->pStmt,iCol));` |
|  241 |  411 | `			break;` |
|    - |  412 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|   13 |  413 | `		case SQLITE_FLOAT:` |
|   27 |  414 | `			ph7_value_double(pOut,(ph7_real)sqlite3_column_double(pSt->pStmt,iCol));` |
|   27 |  415 | `			break;` |
|    - |  416 | `#endif` |
|    2 |  417 | `		case SQLITE_BLOB: {` |
|    5 |  418 | `			const void *pBlob = sqlite3_column_blob(pSt->pStmt,iCol);` |
|    5 |  419 | `			int nByte = sqlite3_column_bytes(pSt->pStmt,iCol);` |
|    - |  420 | `			/* the release is what CLEARS it: ph7_value_string APPENDS to a value` |
|    - |  421 | `			 * that is already a string, so writing into a reused cell without` |
|    - |  422 | `			 * this carries the previous column's bytes along */` |
|    5 |  423 | `			PH7_MemObjRelease(pOut);` |
|    5 |  424 | `			ph7_value_string(pOut,(const char *)pBlob,pBlob ? nByte : 0);` |
|    5 |  425 | `			break;` |
|    - |  426 | `		}` |
|   18 |  427 | `		case SQLITE_NULL:` |
|   37 |  428 | `			ph7_value_null(pOut);` |
|   37 |  429 | `			break;` |
|  146 |  430 | `		default: {` |
|  293 |  431 | `			const char *zText = (const char *)sqlite3_column_text(pSt->pStmt,iCol);` |
|  293 |  432 | `			int nByte = sqlite3_column_bytes(pSt->pStmt,iCol);` |
|  293 |  433 | `			PH7_MemObjRelease(pOut);` |
|  293 |  434 | `			ph7_value_string(pOut,zText,zText ? nByte : 0);` |
|  292 |  435 | `			break;` |
|    - |  436 | `		}` |
|    - |  437 | `	}` |
|  300 |  438 | `}` |
|    - |  439 |  |
|    - |  440 |  |
|    - |  441 | `/* ------------------------------------------------------------------------` |
|    - |  442 | ` * Userland callbacks, called from inside sqlite's own loop` |
|    - |  443 | ` * ------------------------------------------------------------------------ */` |
|    - |  444 | `/*` |
|    - |  445 | ` * The rule for every callback here (and the reason they share one shape):` |
|    - |  446 | ` * sqlite is in the middle of a step when the engine re-enters PHP, and a throw` |
|    - |  447 | ` * out of that PHP cannot travel through sqlite's C frames. So the status is` |
|    - |  448 | ` * PARKED on the connection, sqlite is told to stop with an error, and the verb` |
|    - |  449 | ` * that started the step raises exactly the parked status once the library has` |
|    - |  450 | ` * unwound. A second callback while one is parked does not re-enter PHP at all.` |
|    - |  451 | ` */` |
|   52 |  452 | `static int PdoUdfParked(phl_pdo_udf *pUdf)` |
|    1 |  453 | `{` |
|   53 |  454 | `	return pUdf->pConn->iCallbackExc != 0;` |
|    1 |  455 | `}` |
|    2 |  456 | `static void PdoUdfPark(phl_pdo_udf *pUdf,sxi32 rc,sqlite3_context *pCtx)` |
|    1 |  457 | `{` |
|    3 |  458 | `	pUdf->pConn->iCallbackExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;` |
|    3 |  459 | `	if( pCtx ){` |
|    3 |  460 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|    1 |  461 | `	}` |
|    3 |  462 | `}` |
|    - |  463 | `/* One sqlite value as a php value, in sqlite's own types. */` |
|   30 |  464 | `static void PdoUdfArgValue(ph7_vm *pVm,sqlite3_value *pIn,ph7_value *pOut)` |
|    1 |  465 | `{` |
|   31 |  466 | `	PH7_MemObjInit(pVm,pOut);` |
|   31 |  467 | `	switch( sqlite3_value_type(pIn) ){` |
|   12 |  468 | `		case SQLITE_INTEGER:` |
|   25 |  469 | `			ph7_value_int64(pOut,(ph7_int64)sqlite3_value_int64(pIn));` |
|   25 |  470 | `			break;` |
|    - |  471 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|  ! 0 |  472 | `		case SQLITE_FLOAT:` |
|  ! 0 |  473 | `			ph7_value_double(pOut,(ph7_real)sqlite3_value_double(pIn));` |
|  ! 0 |  474 | `			break;` |
|    - |  475 | `#endif` |
|  ! 0 |  476 | `		case SQLITE_NULL:` |
|  ! 0 |  477 | `			ph7_value_null(pOut);` |
|  ! 0 |  478 | `			break;` |
|  ! 0 |  479 | `		case SQLITE_BLOB:` |
|  ! 0 |  480 | `			ph7_value_string(pOut,(const char *)sqlite3_value_blob(pIn),` |
|  ! 0 |  481 | `				sqlite3_value_bytes(pIn));` |
|  ! 0 |  482 | `			break;` |
|    3 |  483 | `		default:` |
|   10 |  484 | `			ph7_value_string(pOut,(const char *)sqlite3_value_text(pIn),` |
|    3 |  485 | `				sqlite3_value_bytes(pIn));` |
|    6 |  486 | `			break;` |
|    - |  487 | `	}` |
|   31 |  488 | `}` |
|    - |  489 | `/*` |
|    - |  490 | ` * What a callback RETURNED, as a sqlite result.  php maps null, int and float` |
|    - |  491 | ` * straight through and puts everything else through a STRING cast -- which is` |
|    - |  492 | ` * why a bool comes back as "1" and "" and an array comes back as "Array" with` |
|    - |  493 | ` * php's own conversion warning behind it.` |
|    - |  494 | ` */` |
|   28 |  495 | `static void PdoUdfResult(sqlite3_context *pCtx,ph7_value *pVal)` |
|    1 |  496 | `{` |
|   29 |  497 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|    3 |  498 | `		sqlite3_result_null(pCtx);` |
|   28 |  499 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|   13 |  500 | `		sqlite3_result_int64(pCtx,(sqlite3_int64)ph7_value_to_int64(pVal));` |
|    - |  501 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|   21 |  502 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|    3 |  503 | `		sqlite3_result_double(pCtx,(double)ph7_value_to_double(pVal));` |
|    - |  504 | `#endif` |
|    2 |  505 | `	}else{` |
|    - |  506 | `		/* php's string CAST, not a quiet stringification: an array coming back` |
|    - |  507 | `		 * from a callback is "Array" with php's own conversion warning behind` |
|    - |  508 | `		 * it, and a __toString() that throws travels the callback rail. */` |
|   13 |  509 | `		int nByte = 0;` |
|    - |  510 | `		const char *zStr;` |
|   13 |  511 | `		PH7_MemObjToStringUV(pVal);` |
|   13 |  512 | `		zStr = ph7_value_to_string(pVal,&nByte);` |
|   13 |  513 | `		sqlite3_result_text(pCtx,zStr,nByte,SQLITE_TRANSIENT);` |
|    - |  514 | `	}` |
|   29 |  515 | `}` |
|    - |  516 | `/* A scalar function's body: build the arguments, call, convert the answer. */` |
|   26 |  517 | `static void PdoUdfScalar(sqlite3_context *pCtx,int nArg,sqlite3_value **apVal)` |
|    1 |  518 | `{` |
|   27 |  519 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)sqlite3_user_data(pCtx);` |
|   27 |  520 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|    - |  521 | `	ph7_value *apArg[16];` |
|    - |  522 | `	ph7_value sArgs[16];` |
|    - |  523 | `	ph7_value sRes;` |
|   27 |  524 | `	int n,nCall = nArg;` |
|    - |  525 | `	sxi32 rc;` |
|   27 |  526 | `	if( PdoUdfParked(pUdf) ){` |
|  ! 0 |  527 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|  ! 0 |  528 | `		return;` |
|    - |  529 | `	}` |
|   27 |  530 | `	if( nCall > (int)SX_ARRAYSIZE(sArgs) ){` |
|  ! 0 |  531 | `		nCall = (int)SX_ARRAYSIZE(sArgs);` |
|  ! 0 |  532 | `	}` |
|   51 |  533 | `	for( n = 0 ; n < nCall ; ++n ){` |
|   25 |  534 | `		PdoUdfArgValue(pVm,apVal[n],&sArgs[n]);` |
|   25 |  535 | `		apArg[n] = &sArgs[n];` |
|   13 |  536 | `	}` |
|   27 |  537 | `	PH7_MemObjInit(pVm,&sRes);` |
|   27 |  538 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,nCall,nCall ? apArg : 0,&sRes);` |
|   27 |  539 | `	if( rc != SXRET_OK ){` |
|    3 |  540 | `		PdoUdfPark(pUdf,rc,pCtx);` |
|    2 |  541 | `	}else{` |
|   25 |  542 | `		PdoUdfResult(pCtx,&sRes);` |
|    - |  543 | `	}` |
|   27 |  544 | `	PH7_MemObjRelease(&sRes);` |
|   51 |  545 | `	for( n = 0 ; n < nCall ; ++n ){` |
|   25 |  546 | `		PH7_MemObjRelease(&sArgs[n]);` |
|   13 |  547 | `	}` |
|   14 |  548 | `}` |
|    - |  549 | `/* A collation: two strings in, an ordering out. */` |
|    6 |  550 | `static int PdoUdfCollate(void *pUser,int nLeft,const void *pLeft,int nRight,const void *pRight)` |
|    1 |  551 | `{` |
|    7 |  552 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)pUser;` |
|    7 |  553 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|    - |  554 | `	ph7_value sL,sR,sRes;` |
|    - |  555 | `	ph7_value *apArg[2];` |
|    - |  556 | `	sxi32 rc;` |
|    7 |  557 | `	int iCmp = 0;` |
|    7 |  558 | `	if( PdoUdfParked(pUdf) ){` |
|  ! 0 |  559 | `		return 0;` |
|    - |  560 | `	}` |
|    7 |  561 | `	PH7_MemObjInit(pVm,&sL);` |
|    7 |  562 | `	PH7_MemObjInit(pVm,&sR);` |
|    7 |  563 | `	ph7_value_string(&sL,(const char *)pLeft,nLeft);` |
|    7 |  564 | `	ph7_value_string(&sR,(const char *)pRight,nRight);` |
|    7 |  565 | `	apArg[0] = &sL;` |
|    7 |  566 | `	apArg[1] = &sR;` |
|    7 |  567 | `	PH7_MemObjInit(pVm,&sRes);` |
|    7 |  568 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,2,apArg,&sRes);` |
|    7 |  569 | `	if( rc != SXRET_OK ){` |
|    - |  570 | `		/* no sqlite3_context here to fail through: park it and order the pair` |
|    - |  571 | `		 * as equal, which leaves the walk to end on the parked status */` |
|  ! 0 |  572 | `		PdoUdfPark(pUdf,rc,0);` |
|  ! 0 |  573 | `	}else{` |
|    7 |  574 | `		ph7_int64 iVal = ph7_value_to_int64(&sRes);` |
|    7 |  575 | `		iCmp = iVal < 0 ? -1 : (iVal > 0 ? 1 : 0);` |
|    - |  576 | `	}` |
|    7 |  577 | `	PH7_MemObjRelease(&sRes);` |
|    7 |  578 | `	PH7_MemObjRelease(&sL);` |
|    7 |  579 | `	PH7_MemObjRelease(&sR);` |
|    7 |  580 | `	return iCmp;` |
|    4 |  581 | `}` |
|    - |  582 | `/*` |
|    - |  583 | ` * An aggregate's step and finalize halves.  php gives both callbacks the` |
|    - |  584 | ` * running CONTEXT and the ROW COUNT as their first two arguments, and whatever` |
|    - |  585 | ` * step returns becomes the context of the next one. The count php reports to` |
|    - |  586 | ` * the finalizer is one past the rows it stepped over -- including for an empty` |
|    - |  587 | ` * group, where step never runs at all and the finalizer still sees 1.` |
|    - |  588 | ` */` |
|    6 |  589 | `static void PdoUdfStep(sqlite3_context *pCtx,int nArg,sqlite3_value **apVal)` |
|    1 |  590 | `{` |
|    7 |  591 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)sqlite3_user_data(pCtx);` |
|    7 |  592 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|    - |  593 | `	phl_pdo_agg *pAgg;` |
|    - |  594 | `	ph7_value sArgs[16];` |
|    - |  595 | `	ph7_value *apArg[18];` |
|    - |  596 | `	ph7_value sCount,sRes;` |
|    7 |  597 | `	int n,nCall = nArg;` |
|    - |  598 | `	sxi32 rc;` |
|    7 |  599 | `	if( PdoUdfParked(pUdf) ){` |
|  ! 0 |  600 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|  ! 0 |  601 | `		return;` |
|    - |  602 | `	}` |
|    7 |  603 | `	pAgg = (phl_pdo_agg *)sqlite3_aggregate_context(pCtx,(int)sizeof(phl_pdo_agg));` |
|    7 |  604 | `	if( pAgg == 0 ){` |
|  ! 0 |  605 | `		return;` |
|    - |  606 | `	}` |
|    7 |  607 | `	if( pAgg->pCtx == 0 ){` |
|    3 |  608 | `		pAgg->pCtx = ph7_new_scalar(pVm);` |
|    3 |  609 | `		pAgg->nRow = 0;` |
|    1 |  610 | `	}` |
|    7 |  611 | `	pAgg->nRow++;` |
|    7 |  612 | `	if( nCall > (int)SX_ARRAYSIZE(sArgs) ){` |
|  ! 0 |  613 | `		nCall = (int)SX_ARRAYSIZE(sArgs);` |
|  ! 0 |  614 | `	}` |
|    7 |  615 | `	PH7_MemObjInit(pVm,&sCount);` |
|    7 |  616 | `	ph7_value_int(&sCount,pAgg->nRow);` |
|    7 |  617 | `	apArg[0] = pAgg->pCtx;` |
|    7 |  618 | `	apArg[1] = &sCount;` |
|   13 |  619 | `	for( n = 0 ; n < nCall ; ++n ){` |
|    7 |  620 | `		PdoUdfArgValue(pVm,apVal[n],&sArgs[n]);` |
|    7 |  621 | `		apArg[n + 2] = &sArgs[n];` |
|    4 |  622 | `	}` |
|    7 |  623 | `	PH7_MemObjInit(pVm,&sRes);` |
|    7 |  624 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,nCall + 2,apArg,&sRes);` |
|    7 |  625 | `	if( rc != SXRET_OK ){` |
|  ! 0 |  626 | `		PdoUdfPark(pUdf,rc,pCtx);` |
|    7 |  627 | `	}else if( pAgg->pCtx ){` |
|    7 |  628 | `		PH7_MemObjStore(&sRes,pAgg->pCtx);` |
|    3 |  629 | `	}` |
|    7 |  630 | `	PH7_MemObjRelease(&sRes);` |
|    7 |  631 | `	PH7_MemObjRelease(&sCount);` |
|   13 |  632 | `	for( n = 0 ; n < nCall ; ++n ){` |
|    7 |  633 | `		PH7_MemObjRelease(&sArgs[n]);` |
|    4 |  634 | `	}` |
|    4 |  635 | `}` |
|    4 |  636 | `static void PdoUdfFinal(sqlite3_context *pCtx)` |
|    1 |  637 | `{` |
|    5 |  638 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)sqlite3_user_data(pCtx);` |
|    5 |  639 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|    5 |  640 | `	phl_pdo_agg *pAgg = (phl_pdo_agg *)sqlite3_aggregate_context(pCtx,0);` |
|    - |  641 | `	ph7_value sCtx,sCount,sRes;` |
|    - |  642 | `	ph7_value *apArg[2];` |
|    - |  643 | `	sxi32 rc;` |
|    5 |  644 | `	if( PdoUdfParked(pUdf) ){` |
|  ! 0 |  645 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|  ! 0 |  646 | `		return;` |
|    - |  647 | `	}` |
|    5 |  648 | `	PH7_MemObjInit(pVm,&sCtx);` |
|    5 |  649 | `	PH7_MemObjInit(pVm,&sCount);` |
|    5 |  650 | `	if( pAgg && pAgg->pCtx ){` |
|    3 |  651 | `		PH7_MemObjStore(pAgg->pCtx,&sCtx);` |
|    1 |  652 | `	}` |
|    5 |  653 | `	ph7_value_int(&sCount,(pAgg ? pAgg->nRow : 0) + 1);` |
|    5 |  654 | `	apArg[0] = &sCtx;` |
|    5 |  655 | `	apArg[1] = &sCount;` |
|    5 |  656 | `	PH7_MemObjInit(pVm,&sRes);` |
|    5 |  657 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pFinalize ? pUdf->pFinalize : pUdf->pCallback,` |
|    2 |  658 | `		2,apArg,&sRes);` |
|    5 |  659 | `	if( rc != SXRET_OK ){` |
|  ! 0 |  660 | `		PdoUdfPark(pUdf,rc,pCtx);` |
|  ! 0 |  661 | `	}else{` |
|    5 |  662 | `		PdoUdfResult(pCtx,&sRes);` |
|    - |  663 | `	}` |
|    5 |  664 | `	if( pAgg && pAgg->pCtx ){` |
|    3 |  665 | `		ph7_release_value(pVm,pAgg->pCtx);` |
|    3 |  666 | `		pAgg->pCtx = 0;` |
|    1 |  667 | `	}` |
|    5 |  668 | `	PH7_MemObjRelease(&sRes);` |
|    5 |  669 | `	PH7_MemObjRelease(&sCtx);` |
|    5 |  670 | `	PH7_MemObjRelease(&sCount);` |
|    3 |  671 | `}` |
|    2 |  672 | `PH7_PRIVATE int PH7_PdoSqliteAddAggregate(phl_pdo_udf *pUdf,const char *zName,int nArg)` |
|    1 |  673 | `{` |
|    - |  674 | `	int rc;` |
|    3 |  675 | `	if( pUdf->pConn->pDb == 0 ){` |
|  ! 0 |  676 | `		return 0;` |
|    - |  677 | `	}` |
|    3 |  678 | `	rc = sqlite3_create_function_v2(pUdf->pConn->pDb,zName,nArg,SQLITE_UTF8,pUdf,` |
|    - |  679 | `		0,PdoUdfStep,PdoUdfFinal,0);` |
|    3 |  680 | `	if( rc != SQLITE_OK ){` |
|  ! 0 |  681 | `		PH7_PdoSqliteTakeError(pUdf->pConn);` |
|  ! 0 |  682 | `		return 0;` |
|    - |  683 | `	}` |
|    3 |  684 | `	return 1;` |
|    2 |  685 | `}` |
|    - |  686 | `/*` |
|    - |  687 | ` * The authorizer: sqlite asks before it COMPILES each action, so a refusal` |
|    - |  688 | ` * here stops a prepare rather than a step. Its verdict is one of three ints,` |
|    - |  689 | ` * and anything else (including a callback that throws, whose status is parked` |
|    - |  690 | ` * the usual way) denies.` |
|    - |  691 | ` */` |
|   10 |  692 | `static int PdoUdfAuthorize(void *pUser,int iAction,const char *z1,const char *z2,` |
|    - |  693 | `	const char *z3,const char *z4)` |
|    1 |  694 | `{` |
|   11 |  695 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)pUser;` |
|    - |  696 | `	ph7_vm *pVm;` |
|    - |  697 | `	ph7_value sArgs[6],sRes;` |
|    - |  698 | `	ph7_value *apArg[6];` |
|    - |  699 | `	const char *azIn[4];` |
|   11 |  700 | `	int n,iVerdict = SQLITE_OK;` |
|    - |  701 | `	sxi32 rc;` |
|   11 |  702 | `	if( pUdf == 0 \|\| PdoUdfParked(pUdf) ){` |
|  ! 0 |  703 | `		return SQLITE_DENY;` |
|    - |  704 | `	}` |
|   11 |  705 | `	pVm = pUdf->pConn->pVm;` |
|   11 |  706 | `	azIn[0] = z1; azIn[1] = z2; azIn[2] = z3; azIn[3] = z4;` |
|   11 |  707 | `	PH7_MemObjInit(pVm,&sArgs[0]);` |
|   11 |  708 | `	ph7_value_int(&sArgs[0],iAction);` |
|   11 |  709 | `	apArg[0] = &sArgs[0];` |
|   51 |  710 | `	for( n = 0 ; n < 4 ; ++n ){` |
|   41 |  711 | `		PH7_MemObjInit(pVm,&sArgs[n + 1]);` |
|   41 |  712 | `		if( azIn[n] ){` |
|    7 |  713 | `			ph7_value_string(&sArgs[n + 1],azIn[n],(int)SyStrlen(azIn[n]));` |
|    4 |  714 | `		}else{` |
|   35 |  715 | `			ph7_value_null(&sArgs[n + 1]);` |
|    - |  716 | `		}` |
|   41 |  717 | `		apArg[n + 1] = &sArgs[n + 1];` |
|   21 |  718 | `	}` |
|   11 |  719 | `	PH7_MemObjInit(pVm,&sRes);` |
|   11 |  720 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,5,apArg,&sRes);` |
|   11 |  721 | `	if( rc != SXRET_OK ){` |
|  ! 0 |  722 | `		PdoUdfPark(pUdf,rc,0);` |
|  ! 0 |  723 | `		iVerdict = SQLITE_DENY;` |
|  ! 0 |  724 | `	}else{` |
|   11 |  725 | `		ph7_int64 iVal = ph7_value_to_int64(&sRes);` |
|   11 |  726 | `		iVerdict = (iVal == SQLITE_IGNORE) ? SQLITE_IGNORE` |
|    9 |  727 | `			: ((iVal == SQLITE_OK) ? SQLITE_OK : SQLITE_DENY);` |
|    - |  728 | `	}` |
|   11 |  729 | `	PH7_MemObjRelease(&sRes);` |
|   61 |  730 | `	for( n = 0 ; n < 5 ; ++n ){` |
|   51 |  731 | `		PH7_MemObjRelease(&sArgs[n]);` |
|   26 |  732 | `	}` |
|   11 |  733 | `	return iVerdict;` |
|    6 |  734 | `}` |
|    8 |  735 | `PH7_PRIVATE void PH7_PdoSqliteSetAuthorizer(phl_pdo *pConn,phl_pdo_udf *pUdf)` |
|    1 |  736 | `{` |
|    9 |  737 | `	if( pConn->pDb == 0 ){` |
|  ! 0 |  738 | `		return;` |
|    - |  739 | `	}` |
|    9 |  740 | `	if( pUdf ){` |
|    7 |  741 | `		sqlite3_set_authorizer(pConn->pDb,PdoUdfAuthorize,pUdf);` |
|    4 |  742 | `	}else{` |
|    3 |  743 | `		sqlite3_set_authorizer(pConn->pDb,0,0);` |
|    - |  744 | `	}` |
|    5 |  745 | `}` |
|   16 |  746 | `PH7_PRIVATE int PH7_PdoSqliteAddFunction(phl_pdo_udf *pUdf,const char *zName,int nArg,int iFlags)` |
|    1 |  747 | `{` |
|    - |  748 | `	int rc;` |
|   17 |  749 | `	if( pUdf->pConn->pDb == 0 ){` |
|  ! 0 |  750 | `		return 0;` |
|    - |  751 | `	}` |
|   25 |  752 | `	rc = sqlite3_create_function_v2(pUdf->pConn->pDb,zName,nArg,` |
|   16 |  753 | `		SQLITE_UTF8 \| (iFlags & SQLITE_DETERMINISTIC),pUdf,PdoUdfScalar,0,0,0);` |
|   17 |  754 | `	if( rc != SQLITE_OK ){` |
|  ! 0 |  755 | `		PH7_PdoSqliteTakeError(pUdf->pConn);` |
|  ! 0 |  756 | `		return 0;` |
|    - |  757 | `	}` |
|   17 |  758 | `	return 1;` |
|    9 |  759 | `}` |
|    2 |  760 | `PH7_PRIVATE int PH7_PdoSqliteAddCollation(phl_pdo_udf *pUdf,const char *zName)` |
|    1 |  761 | `{` |
|    - |  762 | `	int rc;` |
|    3 |  763 | `	if( pUdf->pConn->pDb == 0 ){` |
|  ! 0 |  764 | `		return 0;` |
|    - |  765 | `	}` |
|    3 |  766 | `	rc = sqlite3_create_collation_v2(pUdf->pConn->pDb,zName,SQLITE_UTF8,pUdf,` |
|    - |  767 | `		PdoUdfCollate,0);` |
|    3 |  768 | `	if( rc != SQLITE_OK ){` |
|  ! 0 |  769 | `		PH7_PdoSqliteTakeError(pUdf->pConn);` |
|  ! 0 |  770 | `		return 0;` |
|    - |  771 | `	}` |
|    3 |  772 | `	return 1;` |
|    2 |  773 | `}` |
|    - |  774 |  |
|    - |  775 | `/*` |
|    - |  776 | ` * The connection a Pdo\Sqlite verb was called on.  These methods are declared` |
|    - |  777 | ` * on the subclass, so the receiver always carries one.` |
|    - |  778 | ` */` |
|   34 |  779 | `static phl_pdo * PdoSqliteThis(ph7_context *pCtx)` |
|    1 |  780 | `{` |
|   35 |  781 | `	return PH7_PdoConnOfInstance(PH7_ContextThis(pCtx));` |
|    1 |  782 | `}` |
|    - |  783 | `/* Record one callback on the connection, so it outlives the registering call. */` |
|   26 |  784 | `static phl_pdo_udf * PdoUdfNew(phl_pdo *pConn,const char *zName,int nName,` |
|    - |  785 | `	ph7_value *pCallback,ph7_value *pFinalize)` |
|    1 |  786 | `{` |
|   27 |  787 | `	ph7_vm *pVm = pConn->pVm;` |
|   27 |  788 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo_udf));` |
|   27 |  789 | `	if( pUdf == 0 ){` |
|  ! 0 |  790 | `		return 0;` |
|    - |  791 | `	}` |
|   27 |  792 | `	SyZero(pUdf,sizeof(phl_pdo_udf));` |
|   27 |  793 | `	pUdf->pConn = pConn;` |
|   27 |  794 | `	pUdf->zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nName + 1);` |
|   27 |  795 | `	if( pUdf->zName ){` |
|   27 |  796 | `		SyMemcpy(zName,pUdf->zName,(sxu32)nName);` |
|   27 |  797 | `		pUdf->zName[nName] = 0;` |
|   13 |  798 | `	}` |
|   27 |  799 | `	pUdf->pCallback = ph7_new_scalar(pVm);` |
|   27 |  800 | `	if( pUdf->pCallback ){` |
|   27 |  801 | `		PH7_MemObjStore(pCallback,pUdf->pCallback);` |
|   13 |  802 | `	}` |
|   27 |  803 | `	if( pFinalize ){` |
|    3 |  804 | `		pUdf->pFinalize = ph7_new_scalar(pVm);` |
|    3 |  805 | `		if( pUdf->pFinalize ){` |
|    3 |  806 | `			PH7_MemObjStore(pFinalize,pUdf->pFinalize);` |
|    1 |  807 | `		}` |
|    1 |  808 | `	}` |
|   27 |  809 | `	pUdf->pNext = pConn->pUdfs;` |
|   27 |  810 | `	pConn->pUdfs = pUdf;` |
|   27 |  811 | `	return pUdf;` |
|   14 |  812 | `}` |
|    - |  813 | `/* php's refusal for a callback it cannot call, worded per verb. */` |
|    4 |  814 | `static sxi32 PdoUdfBadCallable(ph7_context *pCtx,const char *zFn,int iArg,const char *zParam,` |
|    - |  815 | `	ph7_value *pVal)` |
|    1 |  816 | `{` |
|    5 |  817 | `	int nName = 0;` |
|    5 |  818 | `	const char *zName = pVal ? ph7_value_to_string(pVal,&nName) : "";` |
|    7 |  819 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  820 | `		"%s(): Argument #%d ($%s) must be a valid callback, function \"%.*s\" not found "` |
|    2 |  821 | `		"or invalid function name",zFn,iArg,zParam,nName,zName);` |
|    1 |  822 | `}` |
|    - |  823 | `/*` |
|    - |  824 | ` * Pdo\Sqlite::createFunction(string $name, callable $callback,` |
|    - |  825 | ` *                            int $numArgs = -1, int $flags = 0): bool` |
|    - |  826 | ` *` |
|    - |  827 | ` * A SQL function whose body is PHP. -1 arguments means "any arity", which is` |
|    - |  828 | ` * sqlite's own convention, and DETERMINISTIC is the one flag sqlite takes here.` |
|    - |  829 | ` * Registering the same name twice REPLACES the previous body.` |
|    - |  830 | ` */` |
|   18 |  831 | `static int vm_builtin_PdoSqlite_createFunction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  832 | `{` |
|   19 |  833 | `	phl_pdo *pConn = PdoSqliteThis(pCtx);` |
|    - |  834 | `	phl_pdo_udf *pUdf;` |
|    - |  835 | `	const char *zName;` |
|   19 |  836 | `	int nName = 0,nWant,iFlags;` |
|   19 |  837 | `	if( pConn == 0 ){` |
|  ! 0 |  838 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  839 | `	}` |
|   19 |  840 | `	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|   19 |  841 | `	if( nArg < 2 \|\| !ph7_value_is_callable(apArg[1]) ){` |
|    4 |  842 | `		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createFunction",2,"callback",` |
|    1 |  843 | `			nArg > 1 ? apArg[1] : 0);` |
|    - |  844 | `	}` |
|   17 |  845 | `	nWant = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : -1;` |
|   17 |  846 | `	iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3]) : 0;` |
|   17 |  847 | `	pUdf = PdoUdfNew(pConn,zName,nName,apArg[1],0);` |
|   17 |  848 | `	if( pUdf == 0 ){` |
|  ! 0 |  849 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  850 | `	}` |
|   17 |  851 | `	if( !PH7_PdoSqliteAddFunction(pUdf,pUdf->zName ? pUdf->zName : "",nWant,iFlags) ){` |
|  ! 0 |  852 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  853 | `		return PH7_PdoRaise(pCtx,pConn,"Pdo\\Sqlite::createFunction");` |
|    - |  854 | `	}` |
|   17 |  855 | `	ph7_result_bool(pCtx,1);` |
|   17 |  856 | `	return PH7_OK;` |
|   10 |  857 | `}` |
|    - |  858 | `/*` |
|    - |  859 | ` * Pdo\Sqlite::createCollation(string $name, callable $callback): bool` |
|    - |  860 | ` *` |
|    - |  861 | ` * An ORDER BY ... COLLATE whose comparison is PHP. The callback answers the` |
|    - |  862 | ` * usual negative/zero/positive.` |
|    - |  863 | ` */` |
|    4 |  864 | `static int vm_builtin_PdoSqlite_createCollation(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  865 | `{` |
|    5 |  866 | `	phl_pdo *pConn = PdoSqliteThis(pCtx);` |
|    - |  867 | `	phl_pdo_udf *pUdf;` |
|    - |  868 | `	const char *zName;` |
|    5 |  869 | `	int nName = 0;` |
|    5 |  870 | `	if( pConn == 0 ){` |
|  ! 0 |  871 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  872 | `	}` |
|    5 |  873 | `	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|    5 |  874 | `	if( nArg < 2 \|\| !ph7_value_is_callable(apArg[1]) ){` |
|    4 |  875 | `		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createCollation",2,"callback",` |
|    1 |  876 | `			nArg > 1 ? apArg[1] : 0);` |
|    - |  877 | `	}` |
|    3 |  878 | `	pUdf = PdoUdfNew(pConn,zName,nName,apArg[1],0);` |
|    3 |  879 | `	if( pUdf == 0 ){` |
|  ! 0 |  880 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  881 | `	}` |
|    3 |  882 | `	if( !PH7_PdoSqliteAddCollation(pUdf,pUdf->zName ? pUdf->zName : "") ){` |
|  ! 0 |  883 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  884 | `		return PH7_PdoRaise(pCtx,pConn,"Pdo\\Sqlite::createCollation");` |
|    - |  885 | `	}` |
|    3 |  886 | `	ph7_result_bool(pCtx,1);` |
|    3 |  887 | `	return PH7_OK;` |
|    3 |  888 | `}` |
|    - |  889 | `/*` |
|    - |  890 | ` * Pdo\Sqlite::createAggregate(string $name, callable $step, callable $finalize,` |
|    - |  891 | ` *                             int $numArgs = -1): bool` |
|    - |  892 | ` */` |
|    2 |  893 | `static int vm_builtin_PdoSqlite_createAggregate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  894 | `{` |
|    3 |  895 | `	phl_pdo *pConn = PdoSqliteThis(pCtx);` |
|    - |  896 | `	phl_pdo_udf *pUdf;` |
|    - |  897 | `	const char *zName;` |
|    3 |  898 | `	int nName = 0,nWant;` |
|    3 |  899 | `	if( pConn == 0 ){` |
|  ! 0 |  900 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  901 | `	}` |
|    3 |  902 | `	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|    3 |  903 | `	if( nArg < 2 \|\| !ph7_value_is_callable(apArg[1]) ){` |
|  ! 0 |  904 | `		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createAggregate",2,"step",` |
|  ! 0 |  905 | `			nArg > 1 ? apArg[1] : 0);` |
|    - |  906 | `	}` |
|    3 |  907 | `	if( nArg < 3 \|\| !ph7_value_is_callable(apArg[2]) ){` |
|  ! 0 |  908 | `		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createAggregate",3,"finalize",` |
|  ! 0 |  909 | `			nArg > 2 ? apArg[2] : 0);` |
|    - |  910 | `	}` |
|    3 |  911 | `	nWant = nArg > 3 ? (int)ph7_value_to_int64(apArg[3]) : -1;` |
|    3 |  912 | `	pUdf = PdoUdfNew(pConn,zName,nName,apArg[1],apArg[2]);` |
|    3 |  913 | `	if( pUdf == 0 ){` |
|  ! 0 |  914 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  915 | `	}` |
|    3 |  916 | `	if( !PH7_PdoSqliteAddAggregate(pUdf,pUdf->zName ? pUdf->zName : "",nWant) ){` |
|  ! 0 |  917 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  918 | `		return PH7_PdoRaise(pCtx,pConn,"Pdo\\Sqlite::createAggregate");` |
|    - |  919 | `	}` |
|    3 |  920 | `	ph7_result_bool(pCtx,1);` |
|    3 |  921 | `	return PH7_OK;` |
|    2 |  922 | `}` |
|    - |  923 | `/*` |
|    - |  924 | ` * Pdo\Sqlite::setAuthorizer(?callable $callback): void` |
|    - |  925 | ` *` |
|    - |  926 | ` * sqlite asks the authorizer while it COMPILES, so a refusal stops a prepare` |
|    - |  927 | ` * rather than a step -- which is why a denied SELECT fails with "not` |
|    - |  928 | ` * authorized" from query() and never reaches a fetch. null removes it.` |
|    - |  929 | ` */` |
|    8 |  930 | `static int vm_builtin_PdoSqlite_setAuthorizer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  931 | `{` |
|    9 |  932 | `	phl_pdo *pConn = PdoSqliteThis(pCtx);` |
|    - |  933 | `	phl_pdo_udf *pUdf;` |
|    9 |  934 | `	if( pConn == 0 ){` |
|  ! 0 |  935 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  936 | `	}` |
|    9 |  937 | `	if( nArg < 1 \|\| apArg[0] == 0 \|\| (apArg[0]->iFlags & MEMOBJ_NULL) ){` |
|    3 |  938 | `		PH7_PdoSqliteSetAuthorizer(pConn,0);` |
|    3 |  939 | `		return PH7_OK;` |
|    - |  940 | `	}` |
|    7 |  941 | `	if( !ph7_value_is_callable(apArg[0]) ){` |
|  ! 0 |  942 | `		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::setAuthorizer",1,"callback",apArg[0]);` |
|    - |  943 | `	}` |
|    7 |  944 | `	pUdf = PdoUdfNew(pConn,"",0,apArg[0],0);` |
|    7 |  945 | `	if( pUdf == 0 ){` |
|  ! 0 |  946 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  947 | `	}` |
|    7 |  948 | `	PH7_PdoSqliteSetAuthorizer(pConn,pUdf);` |
|    7 |  949 | `	return PH7_OK;` |
|    5 |  950 | `}` |
|    - |  951 | `/*` |
|    - |  952 | ` * Pdo\Sqlite::loadExtension(string $name): void` |
|    - |  953 | ` *` |
|    - |  954 | ` * A refusal here is a bare PDOException naming the extension, with no SQLSTATE` |
|    - |  955 | ` * in front of it -- the same shape the transaction refusals use.` |
|    - |  956 | ` */` |
|    2 |  957 | `static int vm_builtin_PdoSqlite_loadExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  958 | `{` |
|    3 |  959 | `	phl_pdo *pConn = PdoSqliteThis(pCtx);` |
|    - |  960 | `	const char *zName;` |
|    3 |  961 | `	int nName = 0;` |
|    - |  962 | `	SyBlob sName;` |
|    - |  963 | `	int bOk;` |
|    3 |  964 | `	if( pConn == 0 ){` |
|  ! 0 |  965 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  966 | `	}` |
|    3 |  967 | `	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|    3 |  968 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    3 |  969 | `	SyBlobAppend(&sName,zName,(sxu32)nName);` |
|    3 |  970 | `	SyBlobAppend(&sName,"",1);` |
|    3 |  971 | `	bOk = PH7_PdoSqliteLoadExtension(pConn,(const char *)SyBlobData(&sName));` |
|    3 |  972 | `	SyBlobRelease(&sName);` |
|    3 |  973 | `	if( !bOk ){` |
|    4 |  974 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    1 |  975 | `			"Unable to load extension \"%.*s\"",nName,zName);` |
|    - |  976 | `	}` |
|  ! 0 |  977 | `	return PH7_OK;` |
|    2 |  978 | `}` |
|  ! 0 |  979 | `static int vm_builtin_pdo_sqlite_stub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 |  980 | `{` |
|    - |  981 | `	SyBlob sFn;` |
|  ! 0 |  982 | `	SXUNUSED(nArg);` |
|  ! 0 |  983 | `	SXUNUSED(apArg);` |
|  ! 0 |  984 | `	SyBlobInit(&sFn,&pCtx->pVm->sAllocator);` |
|  ! 0 |  985 | `	PH7_VmActiveFuncName(pCtx->pVm,&sFn);` |
|  ! 0 |  986 | `	PH7_VmThrowException(pCtx,"Error","%.*s is not implemented yet",` |
|  ! 0 |  987 | `		(int)SyBlobLength(&sFn),(const char *)SyBlobData(&sFn));` |
|  ! 0 |  988 | `	SyBlobRelease(&sFn);` |
|  ! 0 |  989 | `	return PH7_OK;` |
|  ! 0 |  990 | `}` |
|    - |  991 |  |
|    - |  992 | `/*` |
|    - |  993 | ` * Install the sqlite driver's class surface.  Called from PH7_VmInit right` |
|    - |  994 | `` * after PH7_VmInstallPdo -- `Pdo\Sqlite` extends PDO, so the parent must`` |
|    - |  995 | ` * already be mounted.` |
|    - |  996 | ` */` |
| 5254 |  997 | `PH7_PRIVATE sxi32 PH7_VmInstallPdoSqlite(ph7_vm *pVm)` |
|    5 |  998 | `{` |
|    - |  999 | `#define PDO_SQLITE_INT_CONST(NAME,VALUE) \` |
|    - | 1000 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (ph7_int64)(VALUE), 0, 0.0 }` |
|    - | 1001 | `	static const PH7_NativeConstDef aConst[] = {` |
|    - | 1002 | `		/* php's own PDO_SQLITE_ATTR_* numbering, which starts past the generic` |
|    - | 1003 | `		 * PDO::ATTR_* block so a driver attribute can never collide with one. */` |
|    - | 1004 | `		PDO_SQLITE_INT_CONST("ATTR_OPEN_FLAGS",            1000),` |
|    - | 1005 | `		PDO_SQLITE_INT_CONST("ATTR_READONLY_STATEMENT",    1001),` |
|    - | 1006 | `		PDO_SQLITE_INT_CONST("ATTR_EXTENDED_RESULT_CODES", 1002),` |
|    - | 1007 | `		PDO_SQLITE_INT_CONST("ATTR_BUSY_STATEMENT",        1003),` |
|    - | 1008 | `		PDO_SQLITE_INT_CONST("ATTR_EXPLAIN_STATEMENT",     1004),` |
|    - | 1009 | `		PDO_SQLITE_INT_CONST("ATTR_TRANSACTION_MODE",      1005),` |
|    - | 1010 | `		/* sqlite's own flags, read from its header rather than copied. */` |
|    - | 1011 | `		PDO_SQLITE_INT_CONST("DETERMINISTIC",   SQLITE_DETERMINISTIC),` |
|    - | 1012 | `		PDO_SQLITE_INT_CONST("OPEN_READONLY",   SQLITE_OPEN_READONLY),` |
|    - | 1013 | `		PDO_SQLITE_INT_CONST("OPEN_READWRITE",  SQLITE_OPEN_READWRITE),` |
|    - | 1014 | `		PDO_SQLITE_INT_CONST("OPEN_CREATE",     SQLITE_OPEN_CREATE),` |
|    - | 1015 | `		/* An authorizer callback's three verdicts. */` |
|    - | 1016 | `		PDO_SQLITE_INT_CONST("OK",     SQLITE_OK),` |
|    - | 1017 | `		PDO_SQLITE_INT_CONST("DENY",   SQLITE_DENY),` |
|    - | 1018 | `		PDO_SQLITE_INT_CONST("IGNORE", SQLITE_IGNORE),` |
|    - | 1019 | `		/* php's own: which BEGIN a beginTransaction() emits, and what an` |
|    - | 1020 | `		 * ATTR_EXPLAIN_STATEMENT prepare explains. */` |
|    - | 1021 | `		PDO_SQLITE_INT_CONST("TRANSACTION_MODE_DEFERRED",  0),` |
|    - | 1022 | `		PDO_SQLITE_INT_CONST("TRANSACTION_MODE_IMMEDIATE", 1),` |
|    - | 1023 | `		PDO_SQLITE_INT_CONST("TRANSACTION_MODE_EXCLUSIVE", 2),` |
|    - | 1024 | `		PDO_SQLITE_INT_CONST("EXPLAIN_MODE_PREPARED",            0),` |
|    - | 1025 | `		PDO_SQLITE_INT_CONST("EXPLAIN_MODE_EXPLAIN",             1),` |
|    - | 1026 | `		PDO_SQLITE_INT_CONST("EXPLAIN_MODE_EXPLAIN_QUERY_PLAN",  2),` |
|    - | 1027 | `	};` |
|    - | 1028 | `	/* Unlike the base class's, these return types are DECLARED, not tentative` |
|    - | 1029 | `	 * -- php wrote this stub after tentative types existed. openBlob is the one` |
|    - | 1030 | `	 * exception: it answers a stream resource, which php's stubs cannot spell. */` |
|    - | 1031 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|    - | 1032 | `		{ "createAggregate", PH7_MOD_PUBLIC,` |
|    - | 1033 | `		  "string $name, callable $step, callable $finalize, int $numArgs = -1", "bool",` |
|    - | 1034 | `		  vm_builtin_PdoSqlite_createAggregate },` |
|    - | 1035 | `		{ "createCollation", PH7_MOD_PUBLIC, "string $name, callable $callback", "bool",` |
|    - | 1036 | `		  vm_builtin_PdoSqlite_createCollation },` |
|    - | 1037 | `		{ "createFunction",  PH7_MOD_PUBLIC,` |
|    - | 1038 | `		  "string $function_name, callable $callback, int $num_args = -1, int $flags = 0", "bool",` |
|    - | 1039 | `		  vm_builtin_PdoSqlite_createFunction },` |
|    - | 1040 | `		{ "loadExtension",   PH7_MOD_PUBLIC, "string $name", "void",` |
|    - | 1041 | `		  vm_builtin_PdoSqlite_loadExtension },` |
|    - | 1042 | `		{ "openBlob",        PH7_MOD_PUBLIC,` |
|    - | 1043 | `		  "string $table, string $column, int $rowid, ?string $dbname = 'main', "` |
|    - | 1044 | `		  "int $flags = Pdo\\Sqlite::OPEN_READONLY", 0, vm_builtin_pdo_sqlite_stub },` |
|    - | 1045 | `		{ "setAuthorizer",   PH7_MOD_PUBLIC, "?callable $callback", "void",` |
|    - | 1046 | `		  vm_builtin_PdoSqlite_setAuthorizer },` |
|    - | 1047 | `	};` |
|    - | 1048 | `	static const PH7_NativeClassSpec sSpec = {` |
|    - | 1049 | `		"Pdo\\Sqlite", "PDO", 0, 0,` |
|    - | 1050 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|    - | 1051 | `		aConst, SX_ARRAYSIZE(aConst),` |
|    - | 1052 | `		0, 0,` |
|    - | 1053 | `		0, 0, 0` |
|    - | 1054 | `	};` |
|    - | 1055 | `#undef PDO_SQLITE_INT_CONST` |
| 5259 | 1056 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    5 | 1057 | `}` |
|    - | 1058 |  |
|    - | 1059 | `#else` |
|    - | 1060 | `/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */` |
|    - | 1061 | `typedef int vm_pdo_sqlite_unused;` |
|    - | 1062 | `#endif /* PH7_ENABLE_SQLITE */` |
|    - | 1063 |  |
