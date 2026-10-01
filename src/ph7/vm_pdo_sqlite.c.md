# src/ph7/vm_pdo_sqlite.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 625/713 lines (87.66%)

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
|   46 |   30 | `PH7_PRIVATE void PH7_PdoSqliteTakeError(phl_pdo *pConn)` |
|    1 |   31 | `{` |
|   47 |   32 | `	const char *zSqlState = "HY000";` |
|    - |   33 | `	/* The PRIMARY result code, not the extended one: a UNIQUE violation is 19` |
|    - |   34 | `	 * (SQLITE_CONSTRAINT) and not 2067 (SQLITE_CONSTRAINT_UNIQUE) unless the` |
|    - |   35 | `	 * script asked for extended codes, which is what that driver attribute is` |
|    - |   36 | `	 * for. sqlite reports the extended code from both accessors once they are` |
|    - |   37 | `	 * enabled on the connection, so the choice is made here. */` |
|   93 |   38 | `	int iCode = pConn->pDb` |
|   24 |   39 | `		? (pConn->bExtendedCodes ? sqlite3_extended_errcode(pConn->pDb)` |
|   45 |   40 | `		                         : sqlite3_errcode(pConn->pDb))` |
|   46 |   41 | `		: SQLITE_ERROR;` |
|   47 |   42 | `	const char *zMsg = pConn->pDb ? sqlite3_errmsg(pConn->pDb) : "unknown error";` |
|    - |   43 | `	/* the RAW code, not its low byte: with extended result codes on, a UNIQUE` |
|    - |   44 | `	 * violation reports 1555, which matches none of these and lands on HY000 --` |
|    - |   45 | `	 * php's own answer, and the reason turning extended codes on changes the` |
|    - |   46 | `	 * SQLSTATE and not just the number beside it. */` |
|   47 |   47 | `	switch( iCode ){` |
|  ! 0 |   48 | `		case SQLITE_NOTFOUND:   zSqlState = "42S02"; break;` |
|  ! 0 |   49 | `		case SQLITE_INTERRUPT:  zSqlState = "57014"; break;` |
|  ! 0 |   50 | `		case SQLITE_NOLFS:      zSqlState = "HYC00"; break;` |
|  ! 0 |   51 | `		case SQLITE_TOOBIG:     zSqlState = "22001"; break;` |
|    7 |   52 | `		case SQLITE_CONSTRAINT: zSqlState = "23000"; break;` |
|   34 |   53 | `		case SQLITE_ERROR:` |
|   41 |   54 | `		default:                zSqlState = "HY000"; break;` |
|    - |   55 | `	}` |
|   47 |   56 | `	PH7_PdoSetError(pConn,zSqlState,iCode,zMsg);` |
|   47 |   57 | `}` |
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
|  162 |   79 | `PH7_PRIVATE sxi32 PH7_PdoSqliteOpen(ph7_context *pCtx,phl_pdo *pConn,const char *zPath,` |
|    - |   80 | `	int nPath,int iFlags)` |
|    5 |   81 | `{` |
|    - |   82 | `	char *zTerm;` |
|    - |   83 | `	int rc;` |
|    - |   84 | `	/* sqlite3_open_v2 wants a C string and the DSN slice is not one. */` |
|  167 |   85 | `	zTerm = (char *)SyMemBackendAlloc(&pConn->pVm->sAllocator,(sxu32)nPath + 1);` |
|  167 |   86 | `	if( zTerm == 0 ){` |
|  ! 0 |   87 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |   88 | `	}` |
|  167 |   89 | `	if( nPath > 0 ){` |
|  165 |   90 | `		SyMemcpy(zPath,zTerm,(sxu32)nPath);` |
|   80 |   91 | `	}` |
|  167 |   92 | `	zTerm[nPath] = 0;` |
|  167 |   93 | `	rc = sqlite3_open_v2(zTerm,&pConn->pDb,iFlags,0);` |
|  167 |   94 | `	SyMemBackendFree(&pConn->pVm->sAllocator,zTerm);` |
|  167 |   95 | `	if( rc != SQLITE_OK ){` |
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
|  159 |  109 | `	return PH7_OK;` |
|   86 |  110 | `}` |
|    - |  111 | `/*` |
|    - |  112 | ` * Close one database.  Every statement this connection prepared must already` |
|    - |  113 | ` * be finalized (later slices own that); sqlite3_close_v2 is used so a leaked` |
|    - |  114 | ` * one defers the close rather than leaking the handle itself.` |
|    - |  115 | ` */` |
|  286 |  116 | `PH7_PRIVATE void PH7_PdoSqliteClose(phl_pdo *pConn)` |
|    5 |  117 | `{` |
|  291 |  118 | `	if( pConn->pDb ){` |
|  167 |  119 | `		sqlite3_close_v2(pConn->pDb);` |
|  167 |  120 | `		pConn->pDb = 0;` |
|   81 |  121 | `	}` |
|  291 |  122 | `}` |
|    - |  123 |  |
|    - |  124 | `/*` |
|    - |  125 | ` * PDO::exec()'s work: prepare, step and finalize every statement the string` |
|    - |  126 | `` * holds, one after another.  php runs them ALL -- `INSERT ...; INSERT ...;` is`` |
|    - |  127 | ` * two inserts -- and answers sqlite3_changes(), which is the count from the` |
|    - |  128 | ` * LAST statement that changed anything and is left ALONE by a SELECT or by a` |
|    - |  129 | ` * statement that changes nothing. That is why exec() over a SELECT answers` |
|    - |  130 | ` * whatever the previous write answered rather than 0.` |
|    - |  131 | ` */` |
|  266 |  132 | `PH7_PRIVATE ph7_int64 PH7_PdoSqliteExec(phl_pdo *pConn,const char *zSql,int nSql)` |
|    5 |  133 | `{` |
|  271 |  134 | `	const char *zTail = zSql;` |
|  271 |  135 | `	const char *zEnd = zSql + nSql;` |
|  271 |  136 | `	if( pConn->pDb == 0 ){` |
|  ! 0 |  137 | `		return -1;` |
|    - |  138 | `	}` |
|  523 |  139 | `	while( zTail < zEnd ){` |
|  273 |  140 | `		sqlite3_stmt *pStmt = 0;` |
|  273 |  141 | `		const char *zNext = 0;` |
|  273 |  142 | `		int rc = sqlite3_prepare_v2(pConn->pDb,zTail,(int)(zEnd - zTail),&pStmt,&zNext);` |
|  273 |  143 | `		if( rc != SQLITE_OK ){` |
|   11 |  144 | `			PH7_PdoSqliteTakeError(pConn);` |
|   14 |  145 | `			return -1;` |
|    - |  146 | `		}` |
|  263 |  147 | `		if( pStmt == 0 ){` |
|    - |  148 | `			/* whitespace or a comment: nothing to run, and php answers the` |
|    - |  149 | `			 * change count it already had rather than an error */` |
|    5 |  150 | `			if( zNext == 0 \|\| zNext <= zTail ){` |
|  ! 0 |  151 | `				break;` |
|    - |  152 | `			}` |
|    5 |  153 | `			zTail = zNext;` |
|    5 |  154 | `			continue;` |
|    - |  155 | `		}` |
|  127 |  156 | `		do{` |
|  265 |  157 | `			rc = sqlite3_step(pStmt);` |
|  265 |  158 | `		}while( rc == SQLITE_ROW );` |
|  259 |  159 | `		if( rc != SQLITE_DONE ){` |
|    7 |  160 | `			PH7_PdoSqliteTakeError(pConn);` |
|    7 |  161 | `			sqlite3_finalize(pStmt);` |
|    7 |  162 | `			return -1;` |
|    - |  163 | `		}` |
|  253 |  164 | `		sqlite3_finalize(pStmt);` |
|  253 |  165 | `		zTail = zNext ? zNext : zEnd;` |
|    5 |  166 | `	}` |
|  255 |  167 | `	return (ph7_int64)sqlite3_changes(pConn->pDb);` |
|  138 |  168 | `}` |
|   10 |  169 | `PH7_PRIVATE ph7_int64 PH7_PdoSqliteLastInsertId(phl_pdo *pConn)` |
|    1 |  170 | `{` |
|   11 |  171 | `	return pConn->pDb ? (ph7_int64)sqlite3_last_insert_rowid(pConn->pDb) : 0;` |
|    1 |  172 | `}` |
|   10 |  173 | `PH7_PRIVATE ph7_int64 PH7_PdoSqliteChanges(phl_pdo *pConn)` |
|    1 |  174 | `{` |
|   11 |  175 | `	return pConn->pDb ? (ph7_int64)sqlite3_changes(pConn->pDb) : 0;` |
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
|    - |  212 | `/*` |
|    - |  213 | ` * sqlite3_blob_open, with php's own argument order behind it. The failure is` |
|    - |  214 | `` * left on the connection: php reports it as `Unable to open blob: <sqlite's`` |
|    - |  215 | `` * message>` -- a WARNING whatever the error mode, since a blob is a stream and`` |
|    - |  216 | ` * not a statement.` |
|    - |  217 | ` */` |
|   16 |  218 | `PH7_PRIVATE phl_pdo_blob * PH7_PdoSqliteBlobOpen(phl_pdo *pConn,const char *zDb,` |
|    - |  219 | `	const char *zTable,const char *zColumn,ph7_int64 iRow,int bWrite)` |
|    1 |  220 | `{` |
|   17 |  221 | `	sqlite3_blob *pBlob = 0;` |
|    - |  222 | `	phl_pdo_blob *pBl;` |
|   17 |  223 | `	if( pConn->pDb == 0 ){` |
|  ! 0 |  224 | `		return 0;` |
|    - |  225 | `	}` |
|   24 |  226 | `	if( sqlite3_blob_open(pConn->pDb,zDb,zTable,zColumn,(sqlite3_int64)iRow,` |
|   20 |  227 | `			bWrite ? 1 : 0,&pBlob) != SQLITE_OK \|\| pBlob == 0 ){` |
|   11 |  228 | `		PH7_PdoSqliteTakeError(pConn);` |
|   11 |  229 | `		return 0;` |
|    - |  230 | `	}` |
|    7 |  231 | `	pBl = (phl_pdo_blob *)SyMemBackendAlloc(&pConn->pVm->sAllocator,sizeof(phl_pdo_blob));` |
|    7 |  232 | `	if( pBl == 0 ){` |
|  ! 0 |  233 | `		sqlite3_blob_close(pBlob);` |
|  ! 0 |  234 | `		return 0;` |
|    - |  235 | `	}` |
|    7 |  236 | `	SyZero(pBl,sizeof(phl_pdo_blob));` |
|    7 |  237 | `	pBl->pBlob = pBlob;` |
|    7 |  238 | `	pBl->pVm = pConn->pVm;` |
|    7 |  239 | `	pBl->pConn = pConn;` |
|    7 |  240 | `	pBl->bWrite = bWrite;` |
|    7 |  241 | `	pBl->nSize = (ph7_int64)sqlite3_blob_bytes(pBlob);` |
|    7 |  242 | `	pBl->pNext = pConn->pBlobs;` |
|    7 |  243 | `	pConn->pBlobs = pBl;` |
|    7 |  244 | `	return pBl;` |
|    9 |  245 | `}` |
|    - |  246 | `/* One read or write at the handle's own cursor. Answers the byte count, or -1. */` |
|   18 |  247 | `PH7_PRIVATE int PH7_PdoSqliteBlobIo(phl_pdo_blob *pBl,void *pBuf,int nByte,int bWrite)` |
|    1 |  248 | `{` |
|    - |  249 | `	int rc;` |
|   19 |  250 | `	if( pBl->pBlob == 0 \|\| nByte < 1 \|\| pBl->bBadPos ){` |
|    3 |  251 | `		return 0;` |
|    - |  252 | `	}` |
|   17 |  253 | `	if( pBl->iOfft >= pBl->nSize ){` |
|    5 |  254 | `		return 0;` |
|    - |  255 | `	}` |
|   13 |  256 | `	if( pBl->iOfft + nByte > pBl->nSize ){` |
|    5 |  257 | `		nByte = (int)(pBl->nSize - pBl->iOfft);` |
|    2 |  258 | `	}` |
|   13 |  259 | `	rc = bWrite` |
|    2 |  260 | `		? sqlite3_blob_write(pBl->pBlob,pBuf,nByte,(int)pBl->iOfft)` |
|   11 |  261 | `		: sqlite3_blob_read(pBl->pBlob,pBuf,nByte,(int)pBl->iOfft);` |
|   13 |  262 | `	if( rc != SQLITE_OK ){` |
|  ! 0 |  263 | `		if( pBl->pConn ){` |
|  ! 0 |  264 | `			PH7_PdoSqliteTakeError(pBl->pConn);` |
|  ! 0 |  265 | `		}` |
|  ! 0 |  266 | `		return -1;` |
|    - |  267 | `	}` |
|   13 |  268 | `	pBl->iOfft += nByte;` |
|   13 |  269 | `	return nByte;` |
|   10 |  270 | `}` |
|    - |  271 | `/* Close one handle and take it off its connection's chain. */` |
|    4 |  272 | `PH7_PRIVATE void PH7_PdoSqliteBlobClose(phl_pdo_blob *pBl)` |
|    1 |  273 | `{` |
|    5 |  274 | `	ph7_vm *pVm = pBl->pVm;` |
|    5 |  275 | `	if( pBl->pBlob ){` |
|    5 |  276 | `		sqlite3_blob_close(pBl->pBlob);` |
|    5 |  277 | `		pBl->pBlob = 0;` |
|    2 |  278 | `	}` |
|    5 |  279 | `	if( pBl->pConn ){` |
|    - |  280 | `		phl_pdo_blob **ppSlot;` |
|    5 |  281 | `		for( ppSlot = &pBl->pConn->pBlobs ; *ppSlot ; ppSlot = &(*ppSlot)->pNext ){` |
|    5 |  282 | `			if( *ppSlot == pBl ){` |
|    5 |  283 | `				*ppSlot = pBl->pNext;` |
|    5 |  284 | `				break;` |
|    - |  285 | `			}` |
|  ! 0 |  286 | `		}` |
|    2 |  287 | `	}` |
|    5 |  288 | `	SyMemBackendFree(&pVm->sAllocator,pBl);` |
|    5 |  289 | `}` |
|    - |  290 | `/*` |
|    - |  291 | ` * The connection is going: sqlite will not close a database while a blob` |
|    - |  292 | ` * handle is open, so every handle a script left behind is closed here and cut` |
|    - |  293 | ` * loose. The STREAMS over them survive -- their own close frees the record --` |
|    - |  294 | ` * and answer empty from now on.` |
|    - |  295 | ` */` |
|  278 |  296 | `PH7_PRIVATE void PH7_PdoSqliteBlobSweep(phl_pdo *pConn)` |
|    5 |  297 | `{` |
|  283 |  298 | `	phl_pdo_blob *pBl = pConn->pBlobs;` |
|  285 |  299 | `	while( pBl ){` |
|    3 |  300 | `		phl_pdo_blob *pNext = pBl->pNext;` |
|    3 |  301 | `		if( pBl->pBlob ){` |
|    3 |  302 | `			sqlite3_blob_close(pBl->pBlob);` |
|    3 |  303 | `			pBl->pBlob = 0;` |
|    1 |  304 | `		}` |
|    3 |  305 | `		pBl->pConn = 0;` |
|    3 |  306 | `		pBl->pNext = 0;` |
|    3 |  307 | `		pBl = pNext;` |
|    1 |  308 | `	}` |
|  283 |  309 | `	pConn->pBlobs = 0;` |
|  283 |  310 | `}` |
|    2 |  311 | `PH7_PRIVATE void PH7_PdoSqliteExtendedCodes(phl_pdo *pConn,int bOn)` |
|    1 |  312 | `{` |
|    3 |  313 | `	if( pConn->pDb ){` |
|    3 |  314 | `		sqlite3_extended_result_codes(pConn->pDb,bOn ? 1 : 0);` |
|    1 |  315 | `	}` |
|    3 |  316 | `}` |
|   44 |  317 | `PH7_PRIVATE int PH7_PdoSqliteInTransaction(phl_pdo *pConn)` |
|    1 |  318 | `{` |
|   45 |  319 | `	return pConn->pDb ? (sqlite3_get_autocommit(pConn->pDb) == 0) : 0;` |
|    1 |  320 | `}` |
|    - |  321 | `/*` |
|    - |  322 | ` * Prepare ONE statement.  php compiles only the first statement of the string` |
|    - |  323 | ` * here -- unlike exec(), which runs them all -- and what follows it is simply` |
|    - |  324 | ` * not executed.` |
|    - |  325 | ` */` |
|  930 |  326 | `PH7_PRIVATE int PH7_PdoSqlitePrepare(phl_pdo_stmt *pSt,const char *zSql,int nSql)` |
|    4 |  327 | `{` |
|    - |  328 | `	int rc;` |
|  934 |  329 | `	if( pSt->pConn->pDb == 0 ){` |
|  ! 0 |  330 | `		return 0;` |
|    - |  331 | `	}` |
|  934 |  332 | `	rc = sqlite3_prepare_v2(pSt->pConn->pDb,zSql,nSql,&pSt->pStmt,0);` |
|  934 |  333 | `	if( rc != SQLITE_OK \|\| pSt->pStmt == 0 ){` |
|    9 |  334 | `		PH7_PdoSqliteTakeError(pSt->pConn);` |
|    9 |  335 | `		if( pSt->pStmt ){` |
|  ! 0 |  336 | `			sqlite3_finalize(pSt->pStmt);` |
|  ! 0 |  337 | `			pSt->pStmt = 0;` |
|  ! 0 |  338 | `		}` |
|    9 |  339 | `		return 0;` |
|    - |  340 | `	}` |
|  926 |  341 | `	return 1;` |
|  469 |  342 | `}` |
|    - |  343 | `/*` |
|    - |  344 | ` * One step of the cursor: 1 when a row is available, 0 when the walk is over,` |
|    - |  345 | ` * -1 on failure (with the connection's error set).` |
|    - |  346 | ` */` |
| 1478 |  347 | `PH7_PRIVATE int PH7_PdoSqliteStep(phl_pdo_stmt *pSt)` |
|    4 |  348 | `{` |
|    - |  349 | `	int rc;` |
| 1482 |  350 | `	if( pSt->pStmt == 0 ){` |
|  ! 0 |  351 | `		return 0;` |
|    - |  352 | `	}` |
| 1482 |  353 | `	rc = sqlite3_step(pSt->pStmt);` |
| 1482 |  354 | `	if( rc == SQLITE_ROW ){` |
| 1210 |  355 | `		return 1;` |
|    - |  356 | `	}` |
|  274 |  357 | `	if( rc == SQLITE_DONE ){` |
|  270 |  358 | `		return 0;` |
|    - |  359 | `	}` |
|    5 |  360 | `	PH7_PdoSqliteTakeError(pSt->pConn);` |
|    5 |  361 | `	return -1;` |
|  743 |  362 | `}` |
| 1846 |  363 | `PH7_PRIVATE void PH7_PdoSqliteFinalize(phl_pdo_stmt *pSt)` |
|    4 |  364 | `{` |
| 1850 |  365 | `	if( pSt->pStmt ){` |
|  926 |  366 | `		sqlite3_finalize(pSt->pStmt);` |
|  926 |  367 | `		pSt->pStmt = 0;` |
|  461 |  368 | `	}` |
| 1850 |  369 | `}` |
| 2048 |  370 | `PH7_PRIVATE int PH7_PdoSqliteColumnCount(phl_pdo_stmt *pSt)` |
|    4 |  371 | `{` |
| 2052 |  372 | `	return pSt->pStmt ? sqlite3_column_count(pSt->pStmt) : 0;` |
|    4 |  373 | `}` |
|    - |  374 | `/*` |
|    - |  375 | ` * Rewind a statement so it can run again.  The bindings go too: php re-binds` |
|    - |  376 | ` * everything on every execute(), so a value bound for the previous run must` |
|    - |  377 | ` * not survive into the next one.` |
|    - |  378 | ` */` |
|   52 |  379 | `PH7_PRIVATE void PH7_PdoSqliteReset(phl_pdo_stmt *pSt)` |
|    2 |  380 | `{` |
|   54 |  381 | `	if( pSt->pStmt ){` |
|   54 |  382 | `		sqlite3_reset(pSt->pStmt);` |
|   54 |  383 | `		sqlite3_clear_bindings(pSt->pStmt);` |
|   26 |  384 | `	}` |
|   54 |  385 | `}` |
|    - |  386 | `/*` |
|    - |  387 | ` * Where a NAMED parameter sits.  sqlite answers 0 for a name the statement` |
|    - |  388 | ` * does not have, and binding at 0 is SQLITE_RANGE -- which is exactly how php` |
|    - |  389 | ` * ends up reporting "column index out of range" for a misspelled placeholder` |
|    - |  390 | ` * rather than something that names it.` |
|    - |  391 | ` */` |
|   16 |  392 | `PH7_PRIVATE int PH7_PdoSqliteBindIndexOf(phl_pdo_stmt *pSt,const char *zName,int nName)` |
|    1 |  393 | `{` |
|    - |  394 | `	char zBuf[128];` |
|   17 |  395 | `	int n = 0;` |
|   17 |  396 | `	if( pSt->pStmt == 0 \|\| nName < 1 ){` |
|  ! 0 |  397 | `		return 0;` |
|    - |  398 | `	}` |
|    - |  399 | `	/* php accepts a name with or without its colon and sqlite wants it WITH,` |
|    - |  400 | `	 * so the missing one is supplied here. */` |
|   17 |  401 | `	if( zName[0] != ':' ){` |
|    5 |  402 | `		zBuf[n++] = ':';` |
|    2 |  403 | `	}` |
|   59 |  404 | `	while( n < (int)sizeof(zBuf) - 1 && n - (zName[0] != ':' ? 1 : 0) < nName ){` |
|   43 |  405 | `		zBuf[n] = zName[n - (zName[0] != ':' ? 1 : 0)];` |
|   43 |  406 | `		++n;` |
|    1 |  407 | `	}` |
|   17 |  408 | `	zBuf[n] = 0;` |
|   17 |  409 | `	return sqlite3_bind_parameter_index(pSt->pStmt,zBuf);` |
|    9 |  410 | `}` |
|    - |  411 | `/*` |
|    - |  412 | ` * Bind one value at a 1-based position.  php's PARAM_* decides the CAST, not` |
|    - |  413 | ` * the value's own type: PARAM_INT over the float 1.9 binds 1, PARAM_STR over` |
|    - |  414 | ` * the same binds "1.5", and PARAM_NULL binds null whatever it was handed. The` |
|    - |  415 | ` * one type that outranks the declaration is php's own null, which binds as` |
|    - |  416 | ` * NULL through any of them.` |
|    - |  417 | ` */` |
|   62 |  418 | `PH7_PRIVATE int PH7_PdoSqliteBindAt(phl_pdo_stmt *pSt,int iPos,int iType,ph7_value *pVal)` |
|    1 |  419 | `{` |
|    - |  420 | `	int rc;` |
|   63 |  421 | `	if( pSt->pStmt == 0 ){` |
|  ! 0 |  422 | `		return 0;` |
|    - |  423 | `	}` |
|   63 |  424 | `	iType &= ~PDO_PARAM_FLAGS;` |
|   63 |  425 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) \|\| iType == PDO_PARAM_NULL ){` |
|    7 |  426 | `		rc = sqlite3_bind_null(pSt->pStmt,iPos);` |
|    4 |  427 | `	}else{` |
|   57 |  428 | `		switch( iType ){` |
|    3 |  429 | `			case PDO_PARAM_INT:` |
|    7 |  430 | `				rc = sqlite3_bind_int64(pSt->pStmt,iPos,(sqlite3_int64)ph7_value_to_int64(pVal));` |
|    7 |  431 | `				break;` |
|    2 |  432 | `			case PDO_PARAM_BOOL:` |
|    5 |  433 | `				rc = sqlite3_bind_int(pSt->pStmt,iPos,ph7_value_to_bool(pVal) ? 1 : 0);` |
|    5 |  434 | `				break;` |
|    1 |  435 | `			case PDO_PARAM_LOB: {` |
|    3 |  436 | `				int nByte = 0;` |
|    3 |  437 | `				const char *zVal = ph7_value_to_string(pVal,&nByte);` |
|    3 |  438 | `				rc = sqlite3_bind_blob(pSt->pStmt,iPos,zVal,nByte,SQLITE_TRANSIENT);` |
|    3 |  439 | `				break;` |
|    - |  440 | `			}` |
|   22 |  441 | `			default: {` |
|   45 |  442 | `				int nByte = 0;` |
|   45 |  443 | `				const char *zVal = ph7_value_to_string(pVal,&nByte);` |
|   45 |  444 | `				rc = sqlite3_bind_text(pSt->pStmt,iPos,zVal,nByte,SQLITE_TRANSIENT);` |
|   44 |  445 | `				break;` |
|    - |  446 | `			}` |
|    - |  447 | `		}` |
|    - |  448 | `	}` |
|   63 |  449 | `	if( rc != SQLITE_OK ){` |
|    9 |  450 | `		PH7_PdoSqliteTakeError(pSt->pConn);` |
|    9 |  451 | `		return 0;` |
|    - |  452 | `	}` |
|   55 |  453 | `	return 1;` |
|   32 |  454 | `}` |
| 1374 |  455 | `PH7_PRIVATE const char * PH7_PdoSqliteColumnName(phl_pdo_stmt *pSt,int iCol)` |
|    2 |  456 | `{` |
| 1376 |  457 | `	const char *zName = pSt->pStmt ? sqlite3_column_name(pSt->pStmt,iCol) : 0;` |
| 1376 |  458 | `	return zName ? zName : "";` |
|    2 |  459 | `}` |
|    - |  460 | `/*` |
|    - |  461 | ` * The type the SCHEMA declares for a column, which is not the type of the` |
|    - |  462 | ` * value in it: a column declared TEXT holding NULL reports decl_type "TEXT"` |
|    - |  463 | ` * and native_type "null". An expression has no declared type at all.` |
|    - |  464 | ` */` |
|    4 |  465 | `PH7_PRIVATE const char * PH7_PdoSqliteColumnDecl(phl_pdo_stmt *pSt,int iCol)` |
|    1 |  466 | `{` |
|    5 |  467 | `	return pSt->pStmt ? sqlite3_column_decltype(pSt->pStmt,iCol) : 0;` |
|    1 |  468 | `}` |
|    - |  469 | `/*` |
|    - |  470 | ` * The table a column came from.  sqlite compiles this one only under` |
|    - |  471 | ` * SQLITE_ENABLE_COLUMN_METADATA, so the SYMBOL's presence is the feature's:` |
|    - |  472 | ` * verified present in the Debian and vcpkg libraries this engine links, and` |
|    - |  473 | ` * php reports the same key from the same call. A platform whose sqlite lacks` |
|    - |  474 | ` * it would fail to LINK rather than answer differently -- and its php would be` |
|    - |  475 | ` * missing the key too.` |
|    - |  476 | ` */` |
|    4 |  477 | `PH7_PRIVATE const char * PH7_PdoSqliteColumnTable(phl_pdo_stmt *pSt,int iCol)` |
|    1 |  478 | `{` |
|    5 |  479 | `	return pSt->pStmt ? sqlite3_column_table_name(pSt->pStmt,iCol) : 0;` |
|    1 |  480 | `}` |
|    4 |  481 | `PH7_PRIVATE int PH7_PdoSqliteColumnType(phl_pdo_stmt *pSt,int iCol)` |
|    1 |  482 | `{` |
|    5 |  483 | `	return pSt->pStmt ? sqlite3_column_type(pSt->pStmt,iCol) : SQLITE_NULL;` |
|    1 |  484 | `}` |
|    - |  485 | `/*` |
|    - |  486 | ` * What the statement's last step answered.  php reports THIS as the driver` |
|    - |  487 | ` * code when getColumnMeta() is asked for a column that does not exist, which` |
|    - |  488 | ` * is how a plain out-of-range index comes back as "100 another row available".` |
|    - |  489 | ` */` |
|    2 |  490 | `PH7_PRIVATE int PH7_PdoSqliteLastStepCode(phl_pdo_stmt *pSt)` |
|    1 |  491 | `{` |
|    3 |  492 | `	return pSt->bRowPending ? SQLITE_ROW : SQLITE_DONE;` |
|    1 |  493 | `}` |
|    - |  494 | `/*` |
|    - |  495 | ` * One column of the row at the cursor, in sqlite's OWN type: an INTEGER comes` |
|    - |  496 | ` * back as an int and a REAL as a float, which is why a fetch from this driver` |
|    - |  497 | ` * is not all-strings the way a stringifying one is. A BLOB is a php string of` |
|    - |  498 | ` * those bytes, NUL bytes included, so the length has to come from sqlite` |
|    - |  499 | ` * rather than from the pointer.` |
|    - |  500 | ` */` |
| 1244 |  501 | `PH7_PRIVATE void PH7_PdoSqliteColumnValue(phl_pdo_stmt *pSt,int iCol,ph7_value *pOut)` |
|    2 |  502 | `{` |
| 1246 |  503 | `	if( pSt->pStmt == 0 ){` |
|  ! 0 |  504 | `		ph7_value_null(pOut);` |
|  ! 0 |  505 | `		return;` |
|    - |  506 | `	}` |
| 1246 |  507 | `	switch( sqlite3_column_type(pSt->pStmt,iCol) ){` |
|  144 |  508 | `		case SQLITE_INTEGER:` |
|  289 |  509 | `			ph7_value_int64(pOut,(ph7_int64)sqlite3_column_int64(pSt->pStmt,iCol));` |
|  289 |  510 | `			break;` |
|    - |  511 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|   27 |  512 | `		case SQLITE_FLOAT:` |
|   55 |  513 | `			ph7_value_double(pOut,(ph7_real)sqlite3_column_double(pSt->pStmt,iCol));` |
|   55 |  514 | `			break;` |
|    - |  515 | `#endif` |
|    3 |  516 | `		case SQLITE_BLOB: {` |
|    7 |  517 | `			const void *pBlob = sqlite3_column_blob(pSt->pStmt,iCol);` |
|    7 |  518 | `			int nByte = sqlite3_column_bytes(pSt->pStmt,iCol);` |
|    - |  519 | `			/* the release is what CLEARS it: ph7_value_string APPENDS to a value` |
|    - |  520 | `			 * that is already a string, so writing into a reused cell without` |
|    - |  521 | `			 * this carries the previous column's bytes along */` |
|    7 |  522 | `			PH7_MemObjRelease(pOut);` |
|    7 |  523 | `			ph7_value_string(pOut,(const char *)pBlob,pBlob ? nByte : 0);` |
|    7 |  524 | `			break;` |
|    - |  525 | `		}` |
|   51 |  526 | `		case SQLITE_NULL:` |
|  103 |  527 | `			ph7_value_null(pOut);` |
|  103 |  528 | `			break;` |
|  397 |  529 | `		default: {` |
|  796 |  530 | `			const char *zText = (const char *)sqlite3_column_text(pSt->pStmt,iCol);` |
|  796 |  531 | `			int nByte = sqlite3_column_bytes(pSt->pStmt,iCol);` |
|  796 |  532 | `			PH7_MemObjRelease(pOut);` |
|  796 |  533 | `			ph7_value_string(pOut,zText,zText ? nByte : 0);` |
|  794 |  534 | `			break;` |
|    - |  535 | `		}` |
|    - |  536 | `	}` |
|  624 |  537 | `}` |
|    - |  538 |  |
|    - |  539 |  |
|    - |  540 | `/* ------------------------------------------------------------------------` |
|    - |  541 | ` * Userland callbacks, called from inside sqlite's own loop` |
|    - |  542 | ` * ------------------------------------------------------------------------ */` |
|    - |  543 | `/*` |
|    - |  544 | ` * The rule for every callback here (and the reason they share one shape):` |
|    - |  545 | ` * sqlite is in the middle of a step when the engine re-enters PHP, and a throw` |
|    - |  546 | ` * out of that PHP cannot travel through sqlite's C frames. So the status is` |
|    - |  547 | ` * PARKED on the connection, sqlite is told to stop with an error, and the verb` |
|    - |  548 | ` * that started the step raises exactly the parked status once the library has` |
|    - |  549 | ` * unwound. A second callback while one is parked does not re-enter PHP at all.` |
|    - |  550 | ` */` |
|   52 |  551 | `static int PdoUdfParked(phl_pdo_udf *pUdf)` |
|    1 |  552 | `{` |
|   53 |  553 | `	return pUdf->pConn->iCallbackExc != 0;` |
|    1 |  554 | `}` |
|    2 |  555 | `static void PdoUdfPark(phl_pdo_udf *pUdf,sxi32 rc,sqlite3_context *pCtx)` |
|    1 |  556 | `{` |
|    3 |  557 | `	pUdf->pConn->iCallbackExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;` |
|    3 |  558 | `	if( pCtx ){` |
|    3 |  559 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|    1 |  560 | `	}` |
|    3 |  561 | `}` |
|    - |  562 | `/* One sqlite value as a php value, in sqlite's own types. */` |
|   30 |  563 | `static void PdoUdfArgValue(ph7_vm *pVm,sqlite3_value *pIn,ph7_value *pOut)` |
|    1 |  564 | `{` |
|   31 |  565 | `	PH7_MemObjInit(pVm,pOut);` |
|   31 |  566 | `	switch( sqlite3_value_type(pIn) ){` |
|   12 |  567 | `		case SQLITE_INTEGER:` |
|   25 |  568 | `			ph7_value_int64(pOut,(ph7_int64)sqlite3_value_int64(pIn));` |
|   25 |  569 | `			break;` |
|    - |  570 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|  ! 0 |  571 | `		case SQLITE_FLOAT:` |
|  ! 0 |  572 | `			ph7_value_double(pOut,(ph7_real)sqlite3_value_double(pIn));` |
|  ! 0 |  573 | `			break;` |
|    - |  574 | `#endif` |
|  ! 0 |  575 | `		case SQLITE_NULL:` |
|  ! 0 |  576 | `			ph7_value_null(pOut);` |
|  ! 0 |  577 | `			break;` |
|  ! 0 |  578 | `		case SQLITE_BLOB:` |
|  ! 0 |  579 | `			ph7_value_string(pOut,(const char *)sqlite3_value_blob(pIn),` |
|  ! 0 |  580 | `				sqlite3_value_bytes(pIn));` |
|  ! 0 |  581 | `			break;` |
|    3 |  582 | `		default:` |
|   10 |  583 | `			ph7_value_string(pOut,(const char *)sqlite3_value_text(pIn),` |
|    3 |  584 | `				sqlite3_value_bytes(pIn));` |
|    6 |  585 | `			break;` |
|    - |  586 | `	}` |
|   31 |  587 | `}` |
|    - |  588 | `/*` |
|    - |  589 | ` * What a callback RETURNED, as a sqlite result.  php maps null, int and float` |
|    - |  590 | ` * straight through and puts everything else through a STRING cast -- which is` |
|    - |  591 | ` * why a bool comes back as "1" and "" and an array comes back as "Array" with` |
|    - |  592 | ` * php's own conversion warning behind it.` |
|    - |  593 | ` */` |
|   28 |  594 | `static void PdoUdfResult(sqlite3_context *pCtx,ph7_value *pVal)` |
|    1 |  595 | `{` |
|   29 |  596 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|    3 |  597 | `		sqlite3_result_null(pCtx);` |
|   28 |  598 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|   13 |  599 | `		sqlite3_result_int64(pCtx,(sqlite3_int64)ph7_value_to_int64(pVal));` |
|    - |  600 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|   21 |  601 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|    3 |  602 | `		sqlite3_result_double(pCtx,(double)ph7_value_to_double(pVal));` |
|    - |  603 | `#endif` |
|    2 |  604 | `	}else{` |
|    - |  605 | `		/* php's string CAST, not a quiet stringification: an array coming back` |
|    - |  606 | `		 * from a callback is "Array" with php's own conversion warning behind` |
|    - |  607 | `		 * it, and a __toString() that throws travels the callback rail. */` |
|   13 |  608 | `		int nByte = 0;` |
|    - |  609 | `		const char *zStr;` |
|   13 |  610 | `		PH7_MemObjToStringUV(pVal);` |
|   13 |  611 | `		zStr = ph7_value_to_string(pVal,&nByte);` |
|   13 |  612 | `		sqlite3_result_text(pCtx,zStr,nByte,SQLITE_TRANSIENT);` |
|    - |  613 | `	}` |
|   29 |  614 | `}` |
|    - |  615 | `/* A scalar function's body: build the arguments, call, convert the answer. */` |
|   26 |  616 | `static void PdoUdfScalar(sqlite3_context *pCtx,int nArg,sqlite3_value **apVal)` |
|    1 |  617 | `{` |
|   27 |  618 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)sqlite3_user_data(pCtx);` |
|   27 |  619 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|    - |  620 | `	ph7_value *apArg[16];` |
|    - |  621 | `	ph7_value sArgs[16];` |
|    - |  622 | `	ph7_value sRes;` |
|   27 |  623 | `	int n,nCall = nArg;` |
|    - |  624 | `	sxi32 rc;` |
|   27 |  625 | `	if( PdoUdfParked(pUdf) ){` |
|  ! 0 |  626 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|  ! 0 |  627 | `		return;` |
|    - |  628 | `	}` |
|   27 |  629 | `	if( nCall > (int)SX_ARRAYSIZE(sArgs) ){` |
|  ! 0 |  630 | `		nCall = (int)SX_ARRAYSIZE(sArgs);` |
|  ! 0 |  631 | `	}` |
|   51 |  632 | `	for( n = 0 ; n < nCall ; ++n ){` |
|   25 |  633 | `		PdoUdfArgValue(pVm,apVal[n],&sArgs[n]);` |
|   25 |  634 | `		apArg[n] = &sArgs[n];` |
|   13 |  635 | `	}` |
|   27 |  636 | `	PH7_MemObjInit(pVm,&sRes);` |
|   27 |  637 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,nCall,nCall ? apArg : 0,&sRes);` |
|   27 |  638 | `	if( rc != SXRET_OK ){` |
|    3 |  639 | `		PdoUdfPark(pUdf,rc,pCtx);` |
|    2 |  640 | `	}else{` |
|   25 |  641 | `		PdoUdfResult(pCtx,&sRes);` |
|    - |  642 | `	}` |
|   27 |  643 | `	PH7_MemObjRelease(&sRes);` |
|   51 |  644 | `	for( n = 0 ; n < nCall ; ++n ){` |
|   25 |  645 | `		PH7_MemObjRelease(&sArgs[n]);` |
|   13 |  646 | `	}` |
|   14 |  647 | `}` |
|    - |  648 | `/* A collation: two strings in, an ordering out. */` |
|    6 |  649 | `static int PdoUdfCollate(void *pUser,int nLeft,const void *pLeft,int nRight,const void *pRight)` |
|    1 |  650 | `{` |
|    7 |  651 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)pUser;` |
|    7 |  652 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|    - |  653 | `	ph7_value sL,sR,sRes;` |
|    - |  654 | `	ph7_value *apArg[2];` |
|    - |  655 | `	sxi32 rc;` |
|    7 |  656 | `	int iCmp = 0;` |
|    7 |  657 | `	if( PdoUdfParked(pUdf) ){` |
|  ! 0 |  658 | `		return 0;` |
|    - |  659 | `	}` |
|    7 |  660 | `	PH7_MemObjInit(pVm,&sL);` |
|    7 |  661 | `	PH7_MemObjInit(pVm,&sR);` |
|    7 |  662 | `	ph7_value_string(&sL,(const char *)pLeft,nLeft);` |
|    7 |  663 | `	ph7_value_string(&sR,(const char *)pRight,nRight);` |
|    7 |  664 | `	apArg[0] = &sL;` |
|    7 |  665 | `	apArg[1] = &sR;` |
|    7 |  666 | `	PH7_MemObjInit(pVm,&sRes);` |
|    7 |  667 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,2,apArg,&sRes);` |
|    7 |  668 | `	if( rc != SXRET_OK ){` |
|    - |  669 | `		/* no sqlite3_context here to fail through: park it and order the pair` |
|    - |  670 | `		 * as equal, which leaves the walk to end on the parked status */` |
|  ! 0 |  671 | `		PdoUdfPark(pUdf,rc,0);` |
|  ! 0 |  672 | `	}else{` |
|    7 |  673 | `		ph7_int64 iVal = ph7_value_to_int64(&sRes);` |
|    7 |  674 | `		iCmp = iVal < 0 ? -1 : (iVal > 0 ? 1 : 0);` |
|    - |  675 | `	}` |
|    7 |  676 | `	PH7_MemObjRelease(&sRes);` |
|    7 |  677 | `	PH7_MemObjRelease(&sL);` |
|    7 |  678 | `	PH7_MemObjRelease(&sR);` |
|    7 |  679 | `	return iCmp;` |
|    4 |  680 | `}` |
|    - |  681 | `/*` |
|    - |  682 | ` * An aggregate's step and finalize halves.  php gives both callbacks the` |
|    - |  683 | ` * running CONTEXT and the ROW COUNT as their first two arguments, and whatever` |
|    - |  684 | ` * step returns becomes the context of the next one. The count php reports to` |
|    - |  685 | ` * the finalizer is one past the rows it stepped over -- including for an empty` |
|    - |  686 | ` * group, where step never runs at all and the finalizer still sees 1.` |
|    - |  687 | ` */` |
|    6 |  688 | `static void PdoUdfStep(sqlite3_context *pCtx,int nArg,sqlite3_value **apVal)` |
|    1 |  689 | `{` |
|    7 |  690 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)sqlite3_user_data(pCtx);` |
|    7 |  691 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|    - |  692 | `	phl_pdo_agg *pAgg;` |
|    - |  693 | `	ph7_value sArgs[16];` |
|    - |  694 | `	ph7_value *apArg[18];` |
|    - |  695 | `	ph7_value sCount,sRes;` |
|    7 |  696 | `	int n,nCall = nArg;` |
|    - |  697 | `	sxi32 rc;` |
|    7 |  698 | `	if( PdoUdfParked(pUdf) ){` |
|  ! 0 |  699 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|  ! 0 |  700 | `		return;` |
|    - |  701 | `	}` |
|    7 |  702 | `	pAgg = (phl_pdo_agg *)sqlite3_aggregate_context(pCtx,(int)sizeof(phl_pdo_agg));` |
|    7 |  703 | `	if( pAgg == 0 ){` |
|  ! 0 |  704 | `		return;` |
|    - |  705 | `	}` |
|    7 |  706 | `	if( pAgg->pCtx == 0 ){` |
|    3 |  707 | `		pAgg->pCtx = ph7_new_scalar(pVm);` |
|    3 |  708 | `		pAgg->nRow = 0;` |
|    1 |  709 | `	}` |
|    7 |  710 | `	pAgg->nRow++;` |
|    7 |  711 | `	if( nCall > (int)SX_ARRAYSIZE(sArgs) ){` |
|  ! 0 |  712 | `		nCall = (int)SX_ARRAYSIZE(sArgs);` |
|  ! 0 |  713 | `	}` |
|    7 |  714 | `	PH7_MemObjInit(pVm,&sCount);` |
|    7 |  715 | `	ph7_value_int(&sCount,pAgg->nRow);` |
|    7 |  716 | `	apArg[0] = pAgg->pCtx;` |
|    7 |  717 | `	apArg[1] = &sCount;` |
|   13 |  718 | `	for( n = 0 ; n < nCall ; ++n ){` |
|    7 |  719 | `		PdoUdfArgValue(pVm,apVal[n],&sArgs[n]);` |
|    7 |  720 | `		apArg[n + 2] = &sArgs[n];` |
|    4 |  721 | `	}` |
|    7 |  722 | `	PH7_MemObjInit(pVm,&sRes);` |
|    7 |  723 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,nCall + 2,apArg,&sRes);` |
|    7 |  724 | `	if( rc != SXRET_OK ){` |
|  ! 0 |  725 | `		PdoUdfPark(pUdf,rc,pCtx);` |
|    7 |  726 | `	}else if( pAgg->pCtx ){` |
|    7 |  727 | `		PH7_MemObjStore(&sRes,pAgg->pCtx);` |
|    3 |  728 | `	}` |
|    7 |  729 | `	PH7_MemObjRelease(&sRes);` |
|    7 |  730 | `	PH7_MemObjRelease(&sCount);` |
|   13 |  731 | `	for( n = 0 ; n < nCall ; ++n ){` |
|    7 |  732 | `		PH7_MemObjRelease(&sArgs[n]);` |
|    4 |  733 | `	}` |
|    4 |  734 | `}` |
|    4 |  735 | `static void PdoUdfFinal(sqlite3_context *pCtx)` |
|    1 |  736 | `{` |
|    5 |  737 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)sqlite3_user_data(pCtx);` |
|    5 |  738 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|    5 |  739 | `	phl_pdo_agg *pAgg = (phl_pdo_agg *)sqlite3_aggregate_context(pCtx,0);` |
|    - |  740 | `	ph7_value sCtx,sCount,sRes;` |
|    - |  741 | `	ph7_value *apArg[2];` |
|    - |  742 | `	sxi32 rc;` |
|    5 |  743 | `	if( PdoUdfParked(pUdf) ){` |
|  ! 0 |  744 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|  ! 0 |  745 | `		return;` |
|    - |  746 | `	}` |
|    5 |  747 | `	PH7_MemObjInit(pVm,&sCtx);` |
|    5 |  748 | `	PH7_MemObjInit(pVm,&sCount);` |
|    5 |  749 | `	if( pAgg && pAgg->pCtx ){` |
|    3 |  750 | `		PH7_MemObjStore(pAgg->pCtx,&sCtx);` |
|    1 |  751 | `	}` |
|    5 |  752 | `	ph7_value_int(&sCount,(pAgg ? pAgg->nRow : 0) + 1);` |
|    5 |  753 | `	apArg[0] = &sCtx;` |
|    5 |  754 | `	apArg[1] = &sCount;` |
|    5 |  755 | `	PH7_MemObjInit(pVm,&sRes);` |
|    5 |  756 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pFinalize ? pUdf->pFinalize : pUdf->pCallback,` |
|    2 |  757 | `		2,apArg,&sRes);` |
|    5 |  758 | `	if( rc != SXRET_OK ){` |
|  ! 0 |  759 | `		PdoUdfPark(pUdf,rc,pCtx);` |
|  ! 0 |  760 | `	}else{` |
|    5 |  761 | `		PdoUdfResult(pCtx,&sRes);` |
|    - |  762 | `	}` |
|    5 |  763 | `	if( pAgg && pAgg->pCtx ){` |
|    3 |  764 | `		ph7_release_value(pVm,pAgg->pCtx);` |
|    3 |  765 | `		pAgg->pCtx = 0;` |
|    1 |  766 | `	}` |
|    5 |  767 | `	PH7_MemObjRelease(&sRes);` |
|    5 |  768 | `	PH7_MemObjRelease(&sCtx);` |
|    5 |  769 | `	PH7_MemObjRelease(&sCount);` |
|    3 |  770 | `}` |
|    2 |  771 | `PH7_PRIVATE int PH7_PdoSqliteAddAggregate(phl_pdo_udf *pUdf,const char *zName,int nArg)` |
|    1 |  772 | `{` |
|    - |  773 | `	int rc;` |
|    3 |  774 | `	if( pUdf->pConn->pDb == 0 ){` |
|  ! 0 |  775 | `		return 0;` |
|    - |  776 | `	}` |
|    3 |  777 | `	rc = sqlite3_create_function_v2(pUdf->pConn->pDb,zName,nArg,SQLITE_UTF8,pUdf,` |
|    - |  778 | `		0,PdoUdfStep,PdoUdfFinal,0);` |
|    3 |  779 | `	if( rc != SQLITE_OK ){` |
|  ! 0 |  780 | `		PH7_PdoSqliteTakeError(pUdf->pConn);` |
|  ! 0 |  781 | `		return 0;` |
|    - |  782 | `	}` |
|    3 |  783 | `	return 1;` |
|    2 |  784 | `}` |
|    - |  785 | `/*` |
|    - |  786 | ` * The authorizer: sqlite asks before it COMPILES each action, so a refusal` |
|    - |  787 | ` * here stops a prepare rather than a step. Its verdict is one of three ints,` |
|    - |  788 | ` * and anything else (including a callback that throws, whose status is parked` |
|    - |  789 | ` * the usual way) denies.` |
|    - |  790 | ` */` |
|   10 |  791 | `static int PdoUdfAuthorize(void *pUser,int iAction,const char *z1,const char *z2,` |
|    - |  792 | `	const char *z3,const char *z4)` |
|    1 |  793 | `{` |
|   11 |  794 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)pUser;` |
|    - |  795 | `	ph7_vm *pVm;` |
|    - |  796 | `	ph7_value sArgs[6],sRes;` |
|    - |  797 | `	ph7_value *apArg[6];` |
|    - |  798 | `	const char *azIn[4];` |
|   11 |  799 | `	int n,iVerdict = SQLITE_OK;` |
|    - |  800 | `	sxi32 rc;` |
|   11 |  801 | `	if( pUdf == 0 \|\| PdoUdfParked(pUdf) ){` |
|  ! 0 |  802 | `		return SQLITE_DENY;` |
|    - |  803 | `	}` |
|   11 |  804 | `	pVm = pUdf->pConn->pVm;` |
|   11 |  805 | `	azIn[0] = z1; azIn[1] = z2; azIn[2] = z3; azIn[3] = z4;` |
|   11 |  806 | `	PH7_MemObjInit(pVm,&sArgs[0]);` |
|   11 |  807 | `	ph7_value_int(&sArgs[0],iAction);` |
|   11 |  808 | `	apArg[0] = &sArgs[0];` |
|   51 |  809 | `	for( n = 0 ; n < 4 ; ++n ){` |
|   41 |  810 | `		PH7_MemObjInit(pVm,&sArgs[n + 1]);` |
|   41 |  811 | `		if( azIn[n] ){` |
|    7 |  812 | `			ph7_value_string(&sArgs[n + 1],azIn[n],(int)SyStrlen(azIn[n]));` |
|    4 |  813 | `		}else{` |
|   35 |  814 | `			ph7_value_null(&sArgs[n + 1]);` |
|    - |  815 | `		}` |
|   41 |  816 | `		apArg[n + 1] = &sArgs[n + 1];` |
|   21 |  817 | `	}` |
|   11 |  818 | `	PH7_MemObjInit(pVm,&sRes);` |
|   11 |  819 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,5,apArg,&sRes);` |
|   11 |  820 | `	if( rc != SXRET_OK ){` |
|  ! 0 |  821 | `		PdoUdfPark(pUdf,rc,0);` |
|  ! 0 |  822 | `		iVerdict = SQLITE_DENY;` |
|  ! 0 |  823 | `	}else{` |
|   11 |  824 | `		ph7_int64 iVal = ph7_value_to_int64(&sRes);` |
|   11 |  825 | `		iVerdict = (iVal == SQLITE_IGNORE) ? SQLITE_IGNORE` |
|    9 |  826 | `			: ((iVal == SQLITE_OK) ? SQLITE_OK : SQLITE_DENY);` |
|    - |  827 | `	}` |
|   11 |  828 | `	PH7_MemObjRelease(&sRes);` |
|   61 |  829 | `	for( n = 0 ; n < 5 ; ++n ){` |
|   51 |  830 | `		PH7_MemObjRelease(&sArgs[n]);` |
|   26 |  831 | `	}` |
|   11 |  832 | `	return iVerdict;` |
|    6 |  833 | `}` |
|    8 |  834 | `PH7_PRIVATE void PH7_PdoSqliteSetAuthorizer(phl_pdo *pConn,phl_pdo_udf *pUdf)` |
|    1 |  835 | `{` |
|    9 |  836 | `	if( pConn->pDb == 0 ){` |
|  ! 0 |  837 | `		return;` |
|    - |  838 | `	}` |
|    9 |  839 | `	if( pUdf ){` |
|    7 |  840 | `		sqlite3_set_authorizer(pConn->pDb,PdoUdfAuthorize,pUdf);` |
|    4 |  841 | `	}else{` |
|    3 |  842 | `		sqlite3_set_authorizer(pConn->pDb,0,0);` |
|    - |  843 | `	}` |
|    5 |  844 | `}` |
|   16 |  845 | `PH7_PRIVATE int PH7_PdoSqliteAddFunction(phl_pdo_udf *pUdf,const char *zName,int nArg,int iFlags)` |
|    1 |  846 | `{` |
|    - |  847 | `	int rc;` |
|   17 |  848 | `	if( pUdf->pConn->pDb == 0 ){` |
|  ! 0 |  849 | `		return 0;` |
|    - |  850 | `	}` |
|   25 |  851 | `	rc = sqlite3_create_function_v2(pUdf->pConn->pDb,zName,nArg,` |
|   16 |  852 | `		SQLITE_UTF8 \| (iFlags & SQLITE_DETERMINISTIC),pUdf,PdoUdfScalar,0,0,0);` |
|   17 |  853 | `	if( rc != SQLITE_OK ){` |
|  ! 0 |  854 | `		PH7_PdoSqliteTakeError(pUdf->pConn);` |
|  ! 0 |  855 | `		return 0;` |
|    - |  856 | `	}` |
|   17 |  857 | `	return 1;` |
|    9 |  858 | `}` |
|    2 |  859 | `PH7_PRIVATE int PH7_PdoSqliteAddCollation(phl_pdo_udf *pUdf,const char *zName)` |
|    1 |  860 | `{` |
|    - |  861 | `	int rc;` |
|    3 |  862 | `	if( pUdf->pConn->pDb == 0 ){` |
|  ! 0 |  863 | `		return 0;` |
|    - |  864 | `	}` |
|    3 |  865 | `	rc = sqlite3_create_collation_v2(pUdf->pConn->pDb,zName,SQLITE_UTF8,pUdf,` |
|    - |  866 | `		PdoUdfCollate,0);` |
|    3 |  867 | `	if( rc != SQLITE_OK ){` |
|  ! 0 |  868 | `		PH7_PdoSqliteTakeError(pUdf->pConn);` |
|  ! 0 |  869 | `		return 0;` |
|    - |  870 | `	}` |
|    3 |  871 | `	return 1;` |
|    2 |  872 | `}` |
|    - |  873 |  |
|    - |  874 | `/*` |
|    - |  875 | ` * The connection a Pdo\Sqlite verb was called on.  These methods are declared` |
|    - |  876 | ` * on the subclass, so the receiver always carries one.` |
|    - |  877 | ` */` |
|   34 |  878 | `static phl_pdo * PdoSqliteThis(ph7_context *pCtx)` |
|    1 |  879 | `{` |
|   35 |  880 | `	return PH7_PdoConnOfInstance(PH7_ContextThis(pCtx));` |
|    1 |  881 | `}` |
|    - |  882 | `/* Record one callback on the connection, so it outlives the registering call. */` |
|   26 |  883 | `static phl_pdo_udf * PdoUdfNew(phl_pdo *pConn,const char *zName,int nName,` |
|    - |  884 | `	ph7_value *pCallback,ph7_value *pFinalize)` |
|    1 |  885 | `{` |
|   27 |  886 | `	ph7_vm *pVm = pConn->pVm;` |
|   27 |  887 | `	phl_pdo_udf *pUdf = (phl_pdo_udf *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo_udf));` |
|   27 |  888 | `	if( pUdf == 0 ){` |
|  ! 0 |  889 | `		return 0;` |
|    - |  890 | `	}` |
|   27 |  891 | `	SyZero(pUdf,sizeof(phl_pdo_udf));` |
|   27 |  892 | `	pUdf->pConn = pConn;` |
|   27 |  893 | `	pUdf->zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nName + 1);` |
|   27 |  894 | `	if( pUdf->zName ){` |
|   27 |  895 | `		SyMemcpy(zName,pUdf->zName,(sxu32)nName);` |
|   27 |  896 | `		pUdf->zName[nName] = 0;` |
|   13 |  897 | `	}` |
|   27 |  898 | `	pUdf->pCallback = ph7_new_scalar(pVm);` |
|   27 |  899 | `	if( pUdf->pCallback ){` |
|   27 |  900 | `		PH7_MemObjStore(pCallback,pUdf->pCallback);` |
|   13 |  901 | `	}` |
|   27 |  902 | `	if( pFinalize ){` |
|    3 |  903 | `		pUdf->pFinalize = ph7_new_scalar(pVm);` |
|    3 |  904 | `		if( pUdf->pFinalize ){` |
|    3 |  905 | `			PH7_MemObjStore(pFinalize,pUdf->pFinalize);` |
|    1 |  906 | `		}` |
|    1 |  907 | `	}` |
|   27 |  908 | `	pUdf->pNext = pConn->pUdfs;` |
|   27 |  909 | `	pConn->pUdfs = pUdf;` |
|   27 |  910 | `	return pUdf;` |
|   14 |  911 | `}` |
|    - |  912 | `/* php's refusal for a callback it cannot call, worded per verb. */` |
|    4 |  913 | `static sxi32 PdoUdfBadCallable(ph7_context *pCtx,const char *zFn,int iArg,const char *zParam,` |
|    - |  914 | `	ph7_value *pVal)` |
|    1 |  915 | `{` |
|    5 |  916 | `	int nName = 0;` |
|    5 |  917 | `	const char *zName = pVal ? ph7_value_to_string(pVal,&nName) : "";` |
|    7 |  918 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  919 | `		"%s(): Argument #%d ($%s) must be a valid callback, function \"%.*s\" not found "` |
|    2 |  920 | `		"or invalid function name",zFn,iArg,zParam,nName,zName);` |
|    1 |  921 | `}` |
|    - |  922 | `/*` |
|    - |  923 | ` * Pdo\Sqlite::createFunction(string $name, callable $callback,` |
|    - |  924 | ` *                            int $numArgs = -1, int $flags = 0): bool` |
|    - |  925 | ` *` |
|    - |  926 | ` * A SQL function whose body is PHP. -1 arguments means "any arity", which is` |
|    - |  927 | ` * sqlite's own convention, and DETERMINISTIC is the one flag sqlite takes here.` |
|    - |  928 | ` * Registering the same name twice REPLACES the previous body.` |
|    - |  929 | ` */` |
|   18 |  930 | `static int vm_builtin_PdoSqlite_createFunction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  931 | `{` |
|   19 |  932 | `	phl_pdo *pConn = PdoSqliteThis(pCtx);` |
|    - |  933 | `	phl_pdo_udf *pUdf;` |
|    - |  934 | `	const char *zName;` |
|   19 |  935 | `	int nName = 0,nWant,iFlags;` |
|   19 |  936 | `	if( pConn == 0 ){` |
|  ! 0 |  937 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  938 | `	}` |
|   19 |  939 | `	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|   19 |  940 | `	if( nArg < 2 \|\| !ph7_value_is_callable(apArg[1]) ){` |
|    4 |  941 | `		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createFunction",2,"callback",` |
|    1 |  942 | `			nArg > 1 ? apArg[1] : 0);` |
|    - |  943 | `	}` |
|   17 |  944 | `	nWant = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : -1;` |
|   17 |  945 | `	iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3]) : 0;` |
|   17 |  946 | `	pUdf = PdoUdfNew(pConn,zName,nName,apArg[1],0);` |
|   17 |  947 | `	if( pUdf == 0 ){` |
|  ! 0 |  948 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  949 | `	}` |
|   17 |  950 | `	if( !PH7_PdoSqliteAddFunction(pUdf,pUdf->zName ? pUdf->zName : "",nWant,iFlags) ){` |
|  ! 0 |  951 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  952 | `		return PH7_PdoRaise(pCtx,pConn,"Pdo\\Sqlite::createFunction");` |
|    - |  953 | `	}` |
|   17 |  954 | `	ph7_result_bool(pCtx,1);` |
|   17 |  955 | `	return PH7_OK;` |
|   10 |  956 | `}` |
|    - |  957 | `/*` |
|    - |  958 | ` * Pdo\Sqlite::createCollation(string $name, callable $callback): bool` |
|    - |  959 | ` *` |
|    - |  960 | ` * An ORDER BY ... COLLATE whose comparison is PHP. The callback answers the` |
|    - |  961 | ` * usual negative/zero/positive.` |
|    - |  962 | ` */` |
|    4 |  963 | `static int vm_builtin_PdoSqlite_createCollation(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  964 | `{` |
|    5 |  965 | `	phl_pdo *pConn = PdoSqliteThis(pCtx);` |
|    - |  966 | `	phl_pdo_udf *pUdf;` |
|    - |  967 | `	const char *zName;` |
|    5 |  968 | `	int nName = 0;` |
|    5 |  969 | `	if( pConn == 0 ){` |
|  ! 0 |  970 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  971 | `	}` |
|    5 |  972 | `	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|    5 |  973 | `	if( nArg < 2 \|\| !ph7_value_is_callable(apArg[1]) ){` |
|    4 |  974 | `		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createCollation",2,"callback",` |
|    1 |  975 | `			nArg > 1 ? apArg[1] : 0);` |
|    - |  976 | `	}` |
|    3 |  977 | `	pUdf = PdoUdfNew(pConn,zName,nName,apArg[1],0);` |
|    3 |  978 | `	if( pUdf == 0 ){` |
|  ! 0 |  979 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  980 | `	}` |
|    3 |  981 | `	if( !PH7_PdoSqliteAddCollation(pUdf,pUdf->zName ? pUdf->zName : "") ){` |
|  ! 0 |  982 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  983 | `		return PH7_PdoRaise(pCtx,pConn,"Pdo\\Sqlite::createCollation");` |
|    - |  984 | `	}` |
|    3 |  985 | `	ph7_result_bool(pCtx,1);` |
|    3 |  986 | `	return PH7_OK;` |
|    3 |  987 | `}` |
|    - |  988 | `/*` |
|    - |  989 | ` * Pdo\Sqlite::createAggregate(string $name, callable $step, callable $finalize,` |
|    - |  990 | ` *                             int $numArgs = -1): bool` |
|    - |  991 | ` */` |
|    2 |  992 | `static int vm_builtin_PdoSqlite_createAggregate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  993 | `{` |
|    3 |  994 | `	phl_pdo *pConn = PdoSqliteThis(pCtx);` |
|    - |  995 | `	phl_pdo_udf *pUdf;` |
|    - |  996 | `	const char *zName;` |
|    3 |  997 | `	int nName = 0,nWant;` |
|    3 |  998 | `	if( pConn == 0 ){` |
|  ! 0 |  999 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 1000 | `	}` |
|    3 | 1001 | `	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|    3 | 1002 | `	if( nArg < 2 \|\| !ph7_value_is_callable(apArg[1]) ){` |
|  ! 0 | 1003 | `		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createAggregate",2,"step",` |
|  ! 0 | 1004 | `			nArg > 1 ? apArg[1] : 0);` |
|    - | 1005 | `	}` |
|    3 | 1006 | `	if( nArg < 3 \|\| !ph7_value_is_callable(apArg[2]) ){` |
|  ! 0 | 1007 | `		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::createAggregate",3,"finalize",` |
|  ! 0 | 1008 | `			nArg > 2 ? apArg[2] : 0);` |
|    - | 1009 | `	}` |
|    3 | 1010 | `	nWant = nArg > 3 ? (int)ph7_value_to_int64(apArg[3]) : -1;` |
|    3 | 1011 | `	pUdf = PdoUdfNew(pConn,zName,nName,apArg[1],apArg[2]);` |
|    3 | 1012 | `	if( pUdf == 0 ){` |
|  ! 0 | 1013 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1014 | `	}` |
|    3 | 1015 | `	if( !PH7_PdoSqliteAddAggregate(pUdf,pUdf->zName ? pUdf->zName : "",nWant) ){` |
|  ! 0 | 1016 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1017 | `		return PH7_PdoRaise(pCtx,pConn,"Pdo\\Sqlite::createAggregate");` |
|    - | 1018 | `	}` |
|    3 | 1019 | `	ph7_result_bool(pCtx,1);` |
|    3 | 1020 | `	return PH7_OK;` |
|    2 | 1021 | `}` |
|    - | 1022 | `/*` |
|    - | 1023 | ` * Pdo\Sqlite::setAuthorizer(?callable $callback): void` |
|    - | 1024 | ` *` |
|    - | 1025 | ` * sqlite asks the authorizer while it COMPILES, so a refusal stops a prepare` |
|    - | 1026 | ` * rather than a step -- which is why a denied SELECT fails with "not` |
|    - | 1027 | ` * authorized" from query() and never reaches a fetch. null removes it.` |
|    - | 1028 | ` */` |
|    8 | 1029 | `static int vm_builtin_PdoSqlite_setAuthorizer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1030 | `{` |
|    9 | 1031 | `	phl_pdo *pConn = PdoSqliteThis(pCtx);` |
|    - | 1032 | `	phl_pdo_udf *pUdf;` |
|    9 | 1033 | `	if( pConn == 0 ){` |
|  ! 0 | 1034 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 1035 | `	}` |
|    9 | 1036 | `	if( nArg < 1 \|\| apArg[0] == 0 \|\| (apArg[0]->iFlags & MEMOBJ_NULL) ){` |
|    3 | 1037 | `		PH7_PdoSqliteSetAuthorizer(pConn,0);` |
|    3 | 1038 | `		return PH7_OK;` |
|    - | 1039 | `	}` |
|    7 | 1040 | `	if( !ph7_value_is_callable(apArg[0]) ){` |
|  ! 0 | 1041 | `		return PdoUdfBadCallable(pCtx,"Pdo\\Sqlite::setAuthorizer",1,"callback",apArg[0]);` |
|    - | 1042 | `	}` |
|    7 | 1043 | `	pUdf = PdoUdfNew(pConn,"",0,apArg[0],0);` |
|    7 | 1044 | `	if( pUdf == 0 ){` |
|  ! 0 | 1045 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1046 | `	}` |
|    7 | 1047 | `	PH7_PdoSqliteSetAuthorizer(pConn,pUdf);` |
|    7 | 1048 | `	return PH7_OK;` |
|    5 | 1049 | `}` |
|    - | 1050 | `/*` |
|    - | 1051 | ` * Pdo\Sqlite::loadExtension(string $name): void` |
|    - | 1052 | ` *` |
|    - | 1053 | ` * A refusal here is a bare PDOException naming the extension, with no SQLSTATE` |
|    - | 1054 | ` * in front of it -- the same shape the transaction refusals use.` |
|    - | 1055 | ` */` |
|    2 | 1056 | `static int vm_builtin_PdoSqlite_loadExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1057 | `{` |
|    3 | 1058 | `	phl_pdo *pConn = PdoSqliteThis(pCtx);` |
|    - | 1059 | `	const char *zName;` |
|    3 | 1060 | `	int nName = 0;` |
|    - | 1061 | `	SyBlob sName;` |
|    - | 1062 | `	int bOk;` |
|    3 | 1063 | `	if( pConn == 0 ){` |
|  ! 0 | 1064 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 1065 | `	}` |
|    3 | 1066 | `	zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|    3 | 1067 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    3 | 1068 | `	SyBlobAppend(&sName,zName,(sxu32)nName);` |
|    3 | 1069 | `	SyBlobAppend(&sName,"",1);` |
|    3 | 1070 | `	bOk = PH7_PdoSqliteLoadExtension(pConn,(const char *)SyBlobData(&sName));` |
|    3 | 1071 | `	SyBlobRelease(&sName);` |
|    3 | 1072 | `	if( !bOk ){` |
|    4 | 1073 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    1 | 1074 | `			"Unable to load extension \"%.*s\"",nName,zName);` |
|    - | 1075 | `	}` |
|  ! 0 | 1076 | `	return PH7_OK;` |
|    2 | 1077 | `}` |
|    - | 1078 |  |
|    - | 1079 | `/*` |
|    - | 1080 | ` * Install the sqlite driver's class surface.  Called from PH7_VmInit right` |
|    - | 1081 | `` * after PH7_VmInstallPdo -- `Pdo\Sqlite` extends PDO, so the parent must`` |
|    - | 1082 | ` * already be mounted.` |
|    - | 1083 | ` */` |
| 6721 | 1084 | `PH7_PRIVATE sxi32 PH7_VmInstallPdoSqlite(ph7_vm *pVm)` |
|    5 | 1085 | `{` |
|    - | 1086 | `#define PDO_SQLITE_INT_CONST(NAME,VALUE) \` |
|    - | 1087 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (ph7_int64)(VALUE), 0, 0.0 }` |
|    - | 1088 | `	static const PH7_NativeConstDef aConst[] = {` |
|    - | 1089 | `		/* php's own PDO_SQLITE_ATTR_* numbering, which starts past the generic` |
|    - | 1090 | `		 * PDO::ATTR_* block so a driver attribute can never collide with one. */` |
|    - | 1091 | `		PDO_SQLITE_INT_CONST("ATTR_OPEN_FLAGS",            1000),` |
|    - | 1092 | `		PDO_SQLITE_INT_CONST("ATTR_READONLY_STATEMENT",    1001),` |
|    - | 1093 | `		PDO_SQLITE_INT_CONST("ATTR_EXTENDED_RESULT_CODES", 1002),` |
|    - | 1094 | `		PDO_SQLITE_INT_CONST("ATTR_BUSY_STATEMENT",        1003),` |
|    - | 1095 | `		PDO_SQLITE_INT_CONST("ATTR_EXPLAIN_STATEMENT",     1004),` |
|    - | 1096 | `		PDO_SQLITE_INT_CONST("ATTR_TRANSACTION_MODE",      1005),` |
|    - | 1097 | `		/* sqlite's own flags, read from its header rather than copied. */` |
|    - | 1098 | `		PDO_SQLITE_INT_CONST("DETERMINISTIC",   SQLITE_DETERMINISTIC),` |
|    - | 1099 | `		PDO_SQLITE_INT_CONST("OPEN_READONLY",   SQLITE_OPEN_READONLY),` |
|    - | 1100 | `		PDO_SQLITE_INT_CONST("OPEN_READWRITE",  SQLITE_OPEN_READWRITE),` |
|    - | 1101 | `		PDO_SQLITE_INT_CONST("OPEN_CREATE",     SQLITE_OPEN_CREATE),` |
|    - | 1102 | `		/* An authorizer callback's three verdicts. */` |
|    - | 1103 | `		PDO_SQLITE_INT_CONST("OK",     SQLITE_OK),` |
|    - | 1104 | `		PDO_SQLITE_INT_CONST("DENY",   SQLITE_DENY),` |
|    - | 1105 | `		PDO_SQLITE_INT_CONST("IGNORE", SQLITE_IGNORE),` |
|    - | 1106 | `		/* php's own: which BEGIN a beginTransaction() emits, and what an` |
|    - | 1107 | `		 * ATTR_EXPLAIN_STATEMENT prepare explains. */` |
|    - | 1108 | `		PDO_SQLITE_INT_CONST("TRANSACTION_MODE_DEFERRED",  0),` |
|    - | 1109 | `		PDO_SQLITE_INT_CONST("TRANSACTION_MODE_IMMEDIATE", 1),` |
|    - | 1110 | `		PDO_SQLITE_INT_CONST("TRANSACTION_MODE_EXCLUSIVE", 2),` |
|    - | 1111 | `		PDO_SQLITE_INT_CONST("EXPLAIN_MODE_PREPARED",            0),` |
|    - | 1112 | `		PDO_SQLITE_INT_CONST("EXPLAIN_MODE_EXPLAIN",             1),` |
|    - | 1113 | `		PDO_SQLITE_INT_CONST("EXPLAIN_MODE_EXPLAIN_QUERY_PLAN",  2),` |
|    - | 1114 | `	};` |
|    - | 1115 | `	/* Unlike the base class's, these return types are DECLARED, not tentative` |
|    - | 1116 | `	 * -- php wrote this stub after tentative types existed. openBlob is the one` |
|    - | 1117 | `	 * exception: it answers a stream resource, which php's stubs cannot spell. */` |
|    - | 1118 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|    - | 1119 | `		{ "createAggregate", PH7_MOD_PUBLIC,` |
|    - | 1120 | `		  "string $name, callable $step, callable $finalize, int $numArgs = -1", "bool",` |
|    - | 1121 | `		  vm_builtin_PdoSqlite_createAggregate },` |
|    - | 1122 | `		{ "createCollation", PH7_MOD_PUBLIC, "string $name, callable $callback", "bool",` |
|    - | 1123 | `		  vm_builtin_PdoSqlite_createCollation },` |
|    - | 1124 | `		{ "createFunction",  PH7_MOD_PUBLIC,` |
|    - | 1125 | `		  "string $function_name, callable $callback, int $num_args = -1, int $flags = 0", "bool",` |
|    - | 1126 | `		  vm_builtin_PdoSqlite_createFunction },` |
|    - | 1127 | `		{ "loadExtension",   PH7_MOD_PUBLIC, "string $name", "void",` |
|    - | 1128 | `		  vm_builtin_PdoSqlite_loadExtension },` |
|    - | 1129 | `		{ "openBlob",        PH7_MOD_PUBLIC,` |
|    - | 1130 | `		  "string $table, string $column, int $rowid, ?string $dbname = 'main', "` |
|    - | 1131 | `		  "int $flags = Pdo\\Sqlite::OPEN_READONLY", 0, PH7_PdoSqliteOpenBlobMethod },` |
|    - | 1132 | `		{ "setAuthorizer",   PH7_MOD_PUBLIC, "?callable $callback", "void",` |
|    - | 1133 | `		  vm_builtin_PdoSqlite_setAuthorizer },` |
|    - | 1134 | `	};` |
|    - | 1135 | `	static const PH7_NativeClassSpec sSpec = {` |
|    - | 1136 | `		"Pdo\\Sqlite", "PDO", 0, 0,` |
|    - | 1137 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|    - | 1138 | `		aConst, SX_ARRAYSIZE(aConst),` |
|    - | 1139 | `		0, 0,` |
|    - | 1140 | `		0, 0, 0` |
|    - | 1141 | `	};` |
|    - | 1142 | `#undef PDO_SQLITE_INT_CONST` |
| 6726 | 1143 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    5 | 1144 | `}` |
|    - | 1145 |  |
|    - | 1146 | `#else` |
|    - | 1147 | `/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */` |
|    - | 1148 | `typedef int vm_pdo_sqlite_unused;` |
|    - | 1149 | `#endif /* PH7_ENABLE_SQLITE */` |
|    - | 1150 |  |
