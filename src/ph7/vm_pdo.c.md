# src/ph7/vm_pdo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2612/2887 lines (90.47%)

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
|    - |   10 | ` * ext/pdo: the driver-INDEPENDENT half -- the PDO, PDOStatement and` |
|    - |   11 | ` * PDOException class declarations, their constants, and (later slices) the` |
|    - |   12 | ` * fetch-mode machinery, parameter binding and the SQLSTATE/errmode plumbing` |
|    - |   13 | ` * every driver shares.  The sqlite backend itself lives in vm_pdo_sqlite.c,` |
|    - |   14 | `` * which also declares the `Pdo\Sqlite` subclass PDO::connect() answers.`` |
|    - |   15 | ` *` |
|    - |   16 | ` * Scope is deliberately narrow: the SQLite driver only.  PDO::getAvailableDrivers()` |
|    - |   17 | ` * therefore answers exactly one name, and a DSN naming any other driver is` |
|    - |   18 | `` * php's own `could not find driver`.`` |
|    - |   19 | ` *` |
|    - |   20 | ` * The class surface is DERIVED from the php 8.5 oracle rather than written` |
|    - |   21 | ` * from the manual, and the non-deprecated rule removes part of it: php 8.5` |
|    - |   22 | `` * still declares seven `PDO::SQLITE_*` constants that report`` |
|    - |   23 | ` * ReflectionClassConstant::isDeprecated(), superseded by the unprefixed` |
|    - |   24 | `` * `Pdo\Sqlite::` spellings.  PHL declares only the successors.`` |
|    - |   25 | ` */` |
|    - |   26 |  |
|    - |   27 | `static void PdoStmtSweep(phl_pdo *pConn);` |
|    - |   28 | `static void PdoBlankSlot(ph7_class_instance *pOwner);` |
|    - |   29 | `static phl_pdo * PdoOfInstance(ph7_class_instance *pThis);` |
|    - |   30 | `static void PdoStmtClearFetchState(phl_pdo_stmt *pSt);` |
|    - |   31 | `static void PdoStmtLazyClear(phl_pdo_stmt *pSt);` |
|    - |   32 | `static io_private * PdoLobStreamNew(ph7_vm *pVm,const char *zData,int nData);` |
|    - |   33 | `static sxi32 PdoStatementCtor(ph7_context *pCtx,phl_pdo *pConn,ph7_class *pClass,` |
|    - |   34 | `	ph7_class_instance *pObj);` |
|    - |   35 | `static void PdoBoundColumnsForRow(ph7_vm *pVm,phl_pdo_stmt *pSt);` |
|    - |   36 | `static sxi32 PdoBoundColumnsRefuse(ph7_context *pCtx,phl_pdo_stmt *pSt,int bWholeSet);` |
|    - |   37 | `static int PdoBoundColumnsBad(phl_pdo_stmt *pSt);` |
|    - |   38 |  |
|    - |   39 | `/* ------------------------------------------------------------------------` |
|    - |   40 | ` * Connection lifetime` |
|    - |   41 | ` * ------------------------------------------------------------------------ */` |
|    - |   42 | `/*` |
|    - |   43 | `` * A connection is reached from its PDO object through the hidden `__res` slot`` |
|    - |   44 | ` * and is ALSO chained on the per-VM registry, because PH7 resources have no` |
|    - |   45 | ` * destructor hook: the sweep at VM reset/release is what closes a database a` |
|    - |   46 | `` * script left open.  `clone` is refused on both classes, so no second object`` |
|    - |   47 | ` * can ever reach one record.` |
|    - |   48 | ` */` |
|  180 |   49 | `PH7_PRIVATE phl_pdo * PH7_PdoNewConn(ph7_vm *pVm)` |
|    4 |   50 | `{` |
|  184 |   51 | `	phl_pdo *pConn = (phl_pdo *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo));` |
|  184 |   52 | `	if( pConn == 0 ){` |
|  ! 0 |   53 | `		return 0;` |
|    - |   54 | `	}` |
|  184 |   55 | `	SyZero(pConn,sizeof(phl_pdo));` |
|  184 |   56 | `	pConn->pVm = pVm;` |
|    - |   57 | `	/* php's defaults for a fresh sqlite handle: exceptions on, both column` |
|    - |   58 | `	 * shapes, no case folding, no null rewriting, native column types. */` |
|  184 |   59 | `	pConn->iErrMode = PDO_ERRMODE_EXCEPTION;` |
|  184 |   60 | `	pConn->iCase = PDO_CASE_NATURAL;` |
|  184 |   61 | `	pConn->iOracleNulls = PDO_NULL_NATURAL;` |
|  184 |   62 | `	pConn->iDefaultFetch = PDO_FETCH_BOTH;` |
|  184 |   63 | `	pConn->iErrState = PDO_ERR_NONE;` |
|  184 |   64 | `	pConn->pNext = (phl_pdo *)pVm->pPdoConns;` |
|  184 |   65 | `	pVm->pPdoConns = pConn;` |
|  184 |   66 | `	return pConn;` |
|   94 |   67 | `}` |
|  180 |   68 | `PH7_PRIVATE void PH7_PdoFreeConn(phl_pdo *pConn)` |
|    4 |   69 | `{` |
|    - |   70 | `	/* sqlite refuses to close a database that still has a live statement or an` |
|    - |   71 | `	 * open blob handle, so the cursors go first and the blobs beside them. */` |
|  184 |   72 | `	PdoStmtSweep(pConn);` |
|  184 |   73 | `	PH7_PdoSqliteBlobSweep(pConn);` |
|    - |   74 | `	{` |
|    - |   75 | `		/* the callbacks sqlite still points at; the close is what makes them` |
|    - |   76 | `		 * unreachable, so they are released after it below */` |
|  184 |   77 | `		phl_pdo_udf *pUdf = pConn->pUdfs;` |
|  210 |   78 | `		while( pUdf ){` |
|   27 |   79 | `			phl_pdo_udf *pNext = pUdf->pNext;` |
|   27 |   80 | `			if( pUdf->pCallback ){` |
|   27 |   81 | `				ph7_release_value(pConn->pVm,pUdf->pCallback);` |
|   13 |   82 | `			}` |
|   27 |   83 | `			if( pUdf->pFinalize ){` |
|    3 |   84 | `				ph7_release_value(pConn->pVm,pUdf->pFinalize);` |
|    1 |   85 | `			}` |
|   27 |   86 | `			if( pUdf->zName ){` |
|   27 |   87 | `				SyMemBackendFree(&pConn->pVm->sAllocator,pUdf->zName);` |
|   13 |   88 | `			}` |
|   27 |   89 | `			SyMemBackendFree(&pConn->pVm->sAllocator,pUdf);` |
|   27 |   90 | `			pUdf = pNext;` |
|    1 |   91 | `		}` |
|  184 |   92 | `		pConn->pUdfs = 0;` |
|    - |   93 | `	}` |
|  184 |   94 | `	PH7_PdoSqliteClose(pConn);` |
|  184 |   95 | `	if( pConn->zDrvMsg ){` |
|   15 |   96 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);` |
|   15 |   97 | `		pConn->zDrvMsg = 0;` |
|    7 |   98 | `	}` |
|  184 |   99 | `	if( pConn->pStmtArgs ){` |
|    3 |  100 | `		ph7_release_value(pConn->pVm,pConn->pStmtArgs);` |
|    3 |  101 | `		pConn->pStmtArgs = 0;` |
|    1 |  102 | `	}` |
|  184 |  103 | `	if( pConn->zStmtClass ){` |
|    8 |  104 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zStmtClass);` |
|    8 |  105 | `		pConn->zStmtClass = 0;` |
|    3 |  106 | `	}` |
|  184 |  107 | `}` |
|    - |  108 | `/*` |
|    - |  109 | ` * Free every registered connection.  Called from PH7_PdoVmReset (a reused VM --` |
|    - |  110 | ` * the -S server's -- must not answer the next request through the previous` |
|    - |  111 | ` * one's database) and from PH7_PdoVmRelease before the allocator that holds the` |
|    - |  112 | ` * shells is torn down.` |
|    - |  113 | ` */` |
| 6717 |  114 | `static void PdoVmSweep(ph7_vm *pVm)` |
|    5 |  115 | `{` |
| 6722 |  116 | `	phl_pdo *pConn = (phl_pdo *)pVm->pPdoConns;` |
| 6902 |  117 | `	while( pConn ){` |
|  184 |  118 | `		phl_pdo *pNext = pConn->pNext;` |
|  184 |  119 | `		PdoBlankSlot(pConn->pOwner);` |
|  184 |  120 | `		PH7_PdoFreeConn(pConn);` |
|  184 |  121 | `		SyMemBackendFree(&pVm->sAllocator,pConn);` |
|  184 |  122 | `		pConn = pNext;` |
|    4 |  123 | `	}` |
| 6722 |  124 | `	pVm->pPdoConns = 0;` |
| 6722 |  125 | `}` |
|   16 |  126 | `PH7_PRIVATE void PH7_PdoVmReset(ph7_vm *pVm)` |
|  ! 0 |  127 | `{` |
|   16 |  128 | `	PdoVmSweep(&(*pVm));` |
|   16 |  129 | `}` |
| 6701 |  130 | `PH7_PRIVATE void PH7_PdoVmRelease(ph7_vm *pVm)` |
|    5 |  131 | `{` |
| 6706 |  132 | `	PdoVmSweep(&(*pVm));` |
| 6706 |  133 | `}` |
|    - |  134 | `/*` |
|    - |  135 | ` * Blank the hidden slot of the object that holds a record we are about to` |
|    - |  136 | ` * free.  Without this the object outlives its record -- a PDOStatement whose` |
|    - |  137 | ` * connection was released first, or any handle alive at VM teardown -- and its` |
|    - |  138 | ` * own release reads freed memory to ask whether it still owns one. (ASan found` |
|    - |  139 | ` * exactly that; nothing in the ordinary build noticed.)` |
|    - |  140 | ` */` |
| 1112 |  141 | `static void PdoBlankSlot(ph7_class_instance *pOwner)` |
|    4 |  142 | `{` |
|    - |  143 | `	SyString sAttr;` |
|    - |  144 | `	ph7_value *pRes;` |
| 1116 |  145 | `	if( pOwner == 0 ){` |
| 1028 |  146 | `		return;` |
|    - |  147 | `	}` |
|   91 |  148 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|   91 |  149 | `	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);` |
|   91 |  150 | `	if( pRes ){` |
|   91 |  151 | `		PH7_MemObjRelease(pRes);` |
|   91 |  152 | `		MemObjSetType(pRes,MEMOBJ_NULL);` |
|   44 |  153 | `	}` |
|  560 |  154 | `}` |
|    - |  155 | ``/* The connection behind a `__res` slot value. */`` |
| 1678 |  156 | `static phl_pdo * PdoOfValue(ph7_value *pVal)` |
|    4 |  157 | `{` |
| 1682 |  158 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   45 |  159 | `		return 0;` |
|    - |  160 | `	}` |
| 1638 |  161 | `	return (phl_pdo *)ph7_value_to_resource(pVal);` |
|  843 |  162 | `}` |
|   50 |  163 | `PH7_PRIVATE phl_pdo * PH7_PdoConnOfInstance(ph7_class_instance *pThis)` |
|    1 |  164 | `{` |
|   51 |  165 | `	return PdoOfInstance(pThis);` |
|    1 |  166 | `}` |
| 1678 |  167 | `static phl_pdo * PdoOfInstance(ph7_class_instance *pThis)` |
|    4 |  168 | `{` |
|    - |  169 | `	SyString sAttr;` |
| 1682 |  170 | `	if( pThis == 0 ){` |
|  ! 0 |  171 | `		return 0;` |
|    - |  172 | `	}` |
| 1682 |  173 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
| 1682 |  174 | `	return PdoOfValue(PH7_ClassInstanceFetchAttr(pThis,&sAttr));` |
|  843 |  175 | `}` |
|    - |  176 | `/* Store one connection in the receiver's hidden slot. */` |
|  180 |  177 | `static int PdoAttach(ph7_class_instance *pThis,phl_pdo *pConn)` |
|    4 |  178 | `{` |
|    - |  179 | `	SyString sAttr;` |
|    - |  180 | `	ph7_value *pRes;` |
|  184 |  181 | `	if( pThis == 0 ){` |
|  ! 0 |  182 | `		return -1;` |
|    - |  183 | `	}` |
|  184 |  184 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  184 |  185 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  184 |  186 | `	if( pRes == 0 ){` |
|  ! 0 |  187 | `		return -1;` |
|    - |  188 | `	}` |
|  184 |  189 | `	PH7_MemObjRelease(pRes);` |
|  184 |  190 | `	pRes->x.pOther = pConn;` |
|  184 |  191 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|  184 |  192 | `	pConn->pOwner = pThis;` |
|  184 |  193 | `	return 0;` |
|   94 |  194 | `}` |
|    - |  195 | `/*` |
|    - |  196 | ` * The object is going away: close its database now rather than at VM reset, so` |
|    - |  197 | ` * a script that unsets its last reference releases the file lock there -- which` |
|    - |  198 | ` * is what php does, and what a test that unlinks the file afterwards needs.` |
|    - |  199 | ` * The shell stays on the registry (the sweep frees it) because the slot is` |
|    - |  200 | ` * still reachable while the instance is being torn down.` |
|    - |  201 | ` */` |
|  142 |  202 | `static void PdoInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    4 |  203 | `{` |
|  146 |  204 | `	phl_pdo *pConn = PdoOfInstance(pThis);` |
|   71 |  205 | `	SXUNUSED(pVm);` |
|  146 |  206 | `	if( pConn == 0 \|\| pConn->pOwner != pThis ){` |
|   45 |  207 | `		return;` |
|    - |  208 | `	}` |
|  102 |  209 | `	PdoStmtSweep(pConn);` |
|  102 |  210 | `	PH7_PdoSqliteBlobSweep(pConn);` |
|  102 |  211 | `	PH7_PdoSqliteClose(pConn);` |
|  102 |  212 | `	pConn->pOwner = 0;` |
|   75 |  213 | `}` |
|    - |  214 |  |
|    - |  215 | `/* ------------------------------------------------------------------------` |
|    - |  216 | ` * Statement lifetime` |
|    - |  217 | ` * ------------------------------------------------------------------------ */` |
|    - |  218 | `/*` |
|    - |  219 | ` * A statement is chained on its CONNECTION rather than on the VM: sqlite will` |
|    - |  220 | ` * not close a database with a live statement on it, so the connection's own` |
|    - |  221 | ` * close has to finalize them first. The object reaches it through the same` |
|    - |  222 | ` * hidden slot a connection uses.` |
|    - |  223 | ` */` |
|  930 |  224 | `PH7_PRIVATE phl_pdo_stmt * PH7_PdoNewStmt(phl_pdo *pConn)` |
|    3 |  225 | `{` |
|  933 |  226 | `	phl_pdo_stmt *pSt = (phl_pdo_stmt *)SyMemBackendAlloc(&pConn->pVm->sAllocator,` |
|    - |  227 | `		sizeof(phl_pdo_stmt));` |
|  933 |  228 | `	if( pSt == 0 ){` |
|  ! 0 |  229 | `		return 0;` |
|    - |  230 | `	}` |
|  933 |  231 | `	SyZero(pSt,sizeof(phl_pdo_stmt));` |
|  933 |  232 | `	pSt->pConn = pConn;` |
|  933 |  233 | `	pSt->iFetchMode = pConn->iDefaultFetch;` |
|  933 |  234 | `	pSt->iErrState = PDO_ERR_NONE;` |
|    - |  235 | ``	/* Retain the PDO object. `$db->query(...)` on a temporary connection hands`` |
|    - |  236 | `	 * back a statement that outlives it, and php keeps the connection alive` |
|    - |  237 | `	 * through exactly this reference -- without it the database closes while` |
|    - |  238 | `	 * the statement is still being read. */` |
|  933 |  239 | `	pSt->pConnObj = pConn->pOwner;` |
|  933 |  240 | `	if( pSt->pConnObj ){` |
|  933 |  241 | `		pSt->pConnObj->iRef++;` |
|  465 |  242 | `	}` |
|  933 |  243 | `	pSt->pNext = pConn->pStmts;` |
|  933 |  244 | `	pConn->pStmts = pSt;` |
|  933 |  245 | `	return pSt;` |
|  468 |  246 | `}` |
|    - |  247 | `/* Drop what bindValue()/bindParam() recorded. */` |
| 1860 |  248 | `static void PdoBindListFree(ph7_vm *pVm,phl_pdo_bind *pB)` |
|    3 |  249 | `{` |
| 1997 |  250 | `	while( pB ){` |
|  136 |  251 | `		phl_pdo_bind *pNext = pB->pNext;` |
|  136 |  252 | `		if( pB->zName ){` |
|   18 |  253 | `			SyMemBackendFree(&pVm->sAllocator,pB->zName);` |
|    8 |  254 | `		}` |
|  136 |  255 | `		if( pB->pVal ){` |
|   32 |  256 | `			ph7_release_value(pVm,pB->pVal);` |
|   15 |  257 | `		}` |
|  136 |  258 | `		SyMemBackendFree(&pVm->sAllocator,pB);` |
|  136 |  259 | `		pB = pNext;` |
|    2 |  260 | `	}` |
| 1863 |  261 | `}` |
|  930 |  262 | `static void PdoBindsClear(phl_pdo_stmt *pSt)` |
|    3 |  263 | `{` |
|  933 |  264 | `	PdoBindListFree(pSt->pConn->pVm,pSt->pBinds);` |
|  933 |  265 | `	PdoBindListFree(pSt->pConn->pVm,pSt->pColBinds);` |
|  933 |  266 | `	pSt->pBinds = 0;` |
|  933 |  267 | `	pSt->pColBinds = 0;` |
|  933 |  268 | `}` |
|  930 |  269 | `PH7_PRIVATE void PH7_PdoFreeStmt(phl_pdo_stmt *pSt)` |
|    3 |  270 | `{` |
|  933 |  271 | `	PdoBindsClear(pSt);` |
|  933 |  272 | `	PdoStmtLazyClear(pSt);` |
|  933 |  273 | `	if( pSt->pLazyRow ){` |
|    - |  274 | `		/* A lazy row RETAINS its statement object, so this cannot run while one` |
|    - |  275 | `		 * is alive -- except at VM teardown, which releases in no order. Cut the` |
|    - |  276 | `		 * link from both ends rather than leave the row reading freed memory. */` |
|    3 |  277 | `		PdoBlankSlot(pSt->pLazyRow);` |
|    3 |  278 | `		pSt->pLazyRow = 0;` |
|    1 |  279 | `	}` |
|  933 |  280 | `	PdoStmtClearFetchState(pSt);` |
|  933 |  281 | `	PH7_PdoSqliteFinalize(pSt);` |
|  933 |  282 | `	if( pSt->pConnObj ){` |
|    - |  283 | `		/* drop the reference taken at creation; the connection may go now */` |
|  933 |  284 | `		ph7_class_instance *pObj = pSt->pConnObj;` |
|  933 |  285 | `		pSt->pConnObj = 0;` |
|  933 |  286 | `		PH7_ClassInstanceUnref(pObj);` |
|  465 |  287 | `	}` |
|  933 |  288 | `}` |
|    - |  289 | `/* Finalize and free every statement of one connection. */` |
|  278 |  290 | `static void PdoStmtSweep(phl_pdo *pConn)` |
|    4 |  291 | `{` |
|  282 |  292 | `	phl_pdo_stmt *pSt = pConn->pStmts;` |
| 1212 |  293 | `	while( pSt ){` |
|  933 |  294 | `		phl_pdo_stmt *pNext = pSt->pNext;` |
|  933 |  295 | `		PdoBlankSlot(pSt->pOwner);` |
|  933 |  296 | `		PH7_PdoFreeStmt(pSt);` |
|  933 |  297 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pSt);` |
|  933 |  298 | `		pSt = pNext;` |
|    3 |  299 | `	}` |
|  282 |  300 | `	pConn->pStmts = 0;` |
|  282 |  301 | `}` |
| 2528 |  302 | `static phl_pdo_stmt * PdoStmtOfInstance(ph7_class_instance *pThis)` |
|    3 |  303 | `{` |
|    - |  304 | `	SyString sAttr;` |
|    - |  305 | `	ph7_value *pRes;` |
| 2531 |  306 | `	if( pThis == 0 ){` |
|  ! 0 |  307 | `		return 0;` |
|    - |  308 | `	}` |
| 2531 |  309 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
| 2531 |  310 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
| 2531 |  311 | `	if( pRes == 0 \|\| !ph7_value_is_resource(pRes) ){` |
|   11 |  312 | `		return 0;` |
|    - |  313 | `	}` |
| 2521 |  314 | `	return (phl_pdo_stmt *)ph7_value_to_resource(pRes);` |
| 1267 |  315 | `}` |
|  920 |  316 | `static int PdoStmtAttach(ph7_class_instance *pThis,phl_pdo_stmt *pSt)` |
|    3 |  317 | `{` |
|    - |  318 | `	SyString sAttr;` |
|    - |  319 | `	ph7_value *pRes;` |
|  923 |  320 | `	if( pThis == 0 ){` |
|  ! 0 |  321 | `		return -1;` |
|    - |  322 | `	}` |
|  923 |  323 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  923 |  324 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  923 |  325 | `	if( pRes == 0 ){` |
|  ! 0 |  326 | `		return -1;` |
|    - |  327 | `	}` |
|  923 |  328 | `	PH7_MemObjRelease(pRes);` |
|  923 |  329 | `	pRes->x.pOther = pSt;` |
|  923 |  330 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|  923 |  331 | `	pSt->pOwner = pThis;` |
|    - |  332 | ``	/* php's write_property refuses a store to `queryString` on a statement a`` |
|    - |  333 | ``	 * driver built -- and takes one on a `new PDOStatement()`, which has no`` |
|    - |  334 | `	 * cursor for it to describe. */` |
|  923 |  335 | `	PH7_NativeMarkAttrReadOnly(pThis,"queryString");` |
|  923 |  336 | `	return 0;` |
|  463 |  337 | `}` |
|    - |  338 | `/* The statement object is going away: release its cursor now, as php does. */` |
|  920 |  339 | `static void PdoStmtInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    3 |  340 | `{` |
|  923 |  341 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(pThis);` |
|  460 |  342 | `	SXUNUSED(pVm);` |
|  923 |  343 | `	if( pSt == 0 \|\| pSt->pOwner != pThis ){` |
|    5 |  344 | `		return;` |
|    - |  345 | `	}` |
|  919 |  346 | `	PH7_PdoSqliteFinalize(pSt);` |
|  919 |  347 | `	pSt->pOwner = 0;` |
|  463 |  348 | `}` |
|    - |  349 |  |
|    - |  350 | `/* ------------------------------------------------------------------------` |
|    - |  351 | ` * The error surface` |
|    - |  352 | ` * ------------------------------------------------------------------------ */` |
| 1430 |  353 | `PH7_PRIVATE void PH7_PdoClearError(phl_pdo *pConn)` |
|    4 |  354 | `{` |
| 1434 |  355 | `	pConn->iErrState = PDO_ERR_OK;` |
| 1434 |  356 | `	pConn->iDrvCode = 0;` |
| 1434 |  357 | `	pConn->bNoDrvDetail = 0;` |
| 1434 |  358 | `	SyMemcpy("00000",pConn->zSqlState,sizeof("00000"));` |
| 1434 |  359 | `	if( pConn->zDrvMsg ){` |
|   27 |  360 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);` |
|   27 |  361 | `		pConn->zDrvMsg = 0;` |
|   13 |  362 | `	}` |
| 1434 |  363 | `}` |
|    - |  364 | `/*` |
|    - |  365 | ` * php clears the handle's error at the ENTRY of most verbs -- exec, query,` |
|    - |  366 | ` * prepare, quote, lastInsertId and both attribute accessors -- so a failure is` |
|    - |  367 | ` * invisible to errorCode() as soon as any of them is called, even on a handle` |
|    - |  368 | ` * that has never run anything (NULL becomes "00000"). The verbs that do NOT` |
|    - |  369 | ` * clear are the two reporters themselves and the transaction quartet.` |
|    - |  370 | ` */` |
| 1430 |  371 | `PH7_PRIVATE void PH7_PdoTouch(phl_pdo *pConn)` |
|    4 |  372 | `{` |
| 1434 |  373 | `	PH7_PdoClearError(pConn);` |
| 1434 |  374 | `}` |
|   76 |  375 | `PH7_PRIVATE void PH7_PdoSetError(phl_pdo *pConn,const char *zSqlState,int iCode,const char *zMsg)` |
|    1 |  376 | `{` |
|    - |  377 | `	sxu32 n;` |
|   77 |  378 | `	pConn->iErrState = PDO_ERR_FAILED;` |
|   77 |  379 | `	pConn->bNoDrvDetail = 0;` |
|   77 |  380 | `	pConn->iDrvCode = iCode;` |
|  457 |  381 | `	for( n = 0 ; n < 5 && zSqlState[n] ; ++n ){` |
|  381 |  382 | `		pConn->zSqlState[n] = zSqlState[n];` |
|  191 |  383 | `	}` |
|   77 |  384 | `	pConn->zSqlState[n] = 0;` |
|   77 |  385 | `	if( pConn->zDrvMsg ){` |
|    9 |  386 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);` |
|    9 |  387 | `		pConn->zDrvMsg = 0;` |
|    4 |  388 | `	}` |
|   77 |  389 | `	if( zMsg ){` |
|   49 |  390 | `		n = SyStrlen(zMsg);` |
|   49 |  391 | `		pConn->zDrvMsg = (char *)SyMemBackendAlloc(&pConn->pVm->sAllocator,n + 1);` |
|   49 |  392 | `		if( pConn->zDrvMsg ){` |
|   49 |  393 | `			SyMemcpy(zMsg,pConn->zDrvMsg,n);` |
|   49 |  394 | `			pConn->zDrvMsg[n] = 0;` |
|   24 |  395 | `		}` |
|   24 |  396 | `	}` |
|   77 |  397 | `}` |
|    - |  398 | `/*` |
|    - |  399 | ` * Build a PDOException the way php does: the message php prints, the SQLSTATE` |
|    - |  400 | ` * or driver code in $code, and the raw triple in $errorInfo. The generic` |
|    - |  401 | ` * exception path cannot do this -- it takes an int code and knows no extra` |
|    - |  402 | ` * property -- so the object is constructed here and thrown raw.` |
|    - |  403 | ` */` |
|   68 |  404 | `static sxi32 PdoThrowException(ph7_context *pCtx,const char *zMsg,const char *zSqlState,` |
|    - |  405 | `	int iCode,int bIntCode,const char *zDrvMsg,int nInfo)` |
|    2 |  406 | `{` |
|   70 |  407 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  408 | `	ph7_class *pClass;` |
|    - |  409 | `	ph7_class_instance *pThis;` |
|    - |  410 | `	ph7_class_method *pCons;` |
|    - |  411 | `	ph7_value sArg;` |
|    - |  412 | `	ph7_value *apArg[1];` |
|    - |  413 | `	SyString sMsgStr;` |
|    - |  414 | `	sxi32 rc;` |
|    - |  415 |  |
|   70 |  416 | `	pClass = PH7_VmExtractClass(&(*pVm),"PDOException",sizeof("PDOException")-1,TRUE,0);` |
|   70 |  417 | `	if( pClass == 0 ){` |
|  ! 0 |  418 | `		return PH7_VmThrowException(pCtx,"Error","PDOException is not available");` |
|    - |  419 | `	}` |
|   70 |  420 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|   70 |  421 | `	if( pThis == 0 ){` |
|  ! 0 |  422 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  423 | `	}` |
|   70 |  424 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   70 |  425 | `	if( pCons ){` |
|   70 |  426 | `		SyStringInitFromBuf(&sMsgStr,zMsg,SyStrlen(zMsg));` |
|   70 |  427 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|   70 |  428 | `		apArg[0] = &sArg;` |
|   70 |  429 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|   70 |  430 | `		PH7_MemObjRelease(&sArg);` |
|   34 |  431 | `	}` |
|    - |  432 | `	/* php's $code here is the SQLSTATE STRING for a statement failure and the` |
|    - |  433 | `	 * driver's own INT for a connect failure -- which is why Exception::$code` |
|    - |  434 | `	 * is redeclared untyped on this class. */` |
|   70 |  435 | `	if( bIntCode ){` |
|    9 |  436 | `		PH7_NativeSetAttrInt(&(*pVm),pThis,"code",(sxi64)iCode);` |
|    5 |  437 | `	}else{` |
|   61 |  438 | `		PH7_NativeSetAttrStr(&(*pVm),pThis,"code",zSqlState,SyStrlen(zSqlState));` |
|    - |  439 | `	}` |
|    - |  440 | `	/* A database failure reports all three cells; a layer refusal reports two,` |
|    - |  441 | `	 * because there is no driver message under it. */` |
|   70 |  442 | `	if( nInfo > 0 ){` |
|   70 |  443 | `		ph7_value *pInfo = ph7_context_new_array(pCtx);` |
|   70 |  444 | `		ph7_value *pCell = ph7_context_new_scalar(pCtx);` |
|   70 |  445 | `		if( pInfo && pCell ){` |
|   70 |  446 | `			ph7_value_string(pCell,zSqlState,(int)SyStrlen(zSqlState));` |
|   70 |  447 | `			ph7_array_add_elem(pInfo,0,pCell);` |
|   70 |  448 | `			ph7_value_int64(pCell,(ph7_int64)iCode);` |
|   70 |  449 | `			ph7_array_add_elem(pInfo,0,pCell);` |
|   70 |  450 | `			if( nInfo > 2 ){` |
|    - |  451 | `				/* the cell is an int here, so the string write resets it */` |
|   36 |  452 | `				if( zDrvMsg ){` |
|   36 |  453 | `					ph7_value_string(pCell,zDrvMsg,(int)SyStrlen(zDrvMsg));` |
|   19 |  454 | `				}else{` |
|  ! 0 |  455 | `					ph7_value_null(pCell);` |
|    - |  456 | `				}` |
|   36 |  457 | `				ph7_array_add_elem(pInfo,0,pCell);` |
|   17 |  458 | `			}` |
|   70 |  459 | `			PH7_NativeSetProp(&(*pVm),pThis,"errorInfo",sizeof("errorInfo")-1,pInfo);` |
|   34 |  460 | `		}` |
|   34 |  461 | `	}` |
|   70 |  462 | `	rc = VmThrowException(&(*pVm),pThis);` |
|   70 |  463 | `	PH7_ClassInstanceUnref(pThis);` |
|   70 |  464 | `	if( rc == SXERR_ABORT ){` |
|  ! 0 |  465 | `		pCtx->nThrowRc = PH7_ABORT;` |
|  ! 0 |  466 | `		return PH7_ABORT;` |
|    - |  467 | `	}` |
|   70 |  468 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|   70 |  469 | `	return PH7_EXCEPTION;` |
|   36 |  470 | `}` |
|    - |  471 | `/*` |
|    - |  472 | ` * A failed connect.  php never routes this one through the error mode: the` |
|    - |  473 | ` * constructor throws whatever ATTR_ERRMODE the options carried, and the` |
|    - |  474 | `` * message is `SQLSTATE[HY000] [14] unable to open database file` -- a shape no`` |
|    - |  475 | ` * other failure uses.` |
|    - |  476 | ` */` |
|    8 |  477 | `PH7_PRIVATE sxi32 PH7_PdoThrowConstruct(ph7_context *pCtx,const char *zSqlState,int iCode,` |
|    - |  478 | `	const char *zMsg)` |
|    1 |  479 | `{` |
|    - |  480 | `	SyBlob sMsg;` |
|    - |  481 | `	sxi32 rc;` |
|    9 |  482 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    9 |  483 | `	SyBlobFormat(&sMsg,"SQLSTATE[%s] [%d] %s",zSqlState,iCode,zMsg ? zMsg : "");` |
|    9 |  484 | `	SyBlobAppend(&sMsg,"",1);` |
|    9 |  485 | `	rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),zSqlState,iCode,TRUE,zMsg,3);` |
|    9 |  486 | `	SyBlobRelease(&sMsg);` |
|    9 |  487 | `	return rc;` |
|    1 |  488 | `}` |
|    - |  489 | `/*` |
|    - |  490 | ` * php's SQLSTATE-to-description table, which is what a failure message says` |
|    - |  491 | ` * BEFORE the driver's own code and text: a constraint violation reads` |
|    - |  492 | `` * `SQLSTATE[23000]: Integrity constraint violation: 19 UNIQUE constraint`` |
|    - |  493 | `` * failed: u.v`, not "General error". Only the rows this driver can actually`` |
|    - |  494 | ` * reach are here; anything else falls back to HY000's sentence, which is what` |
|    - |  495 | ` * php answers for an unlisted state too.` |
|    - |  496 | ` *` |
|    - |  497 | ` * HY000, 23000 and IM001 are probe-verified against the oracle. The remaining` |
|    - |  498 | ` * three are php's own wording for states the sqlite driver maps but cannot` |
|    - |  499 | ` * reach in practice -- SQLITE_TOOBIG needs a value past the 1 GB limit,` |
|    - |  500 | ` * SQLITE_INTERRUPT an interrupt this engine never issues -- so no probe can` |
|    - |  501 | ` * confirm them and none can contradict them either.` |
|    - |  502 | ` */` |
|   66 |  503 | `static const char * PdoStateDescription(const char *zState)` |
|    1 |  504 | `{` |
|    - |  505 | `	static const struct { const char *zState; const char *zText; } aState[] = {` |
|    - |  506 | `		{ "HY000", "General error" },` |
|    - |  507 | `		{ "23000", "Integrity constraint violation" },` |
|    - |  508 | `		{ "IM001", "Driver does not support this function" },` |
|    - |  509 | `		{ "42S02", "Base table or view not found" },` |
|    - |  510 | `		{ "22001", "String data, right truncated" },` |
|    - |  511 | `		{ "HYC00", "Optional feature not implemented" },` |
|    - |  512 | `		{ "57014", "Statement canceled" },` |
|    - |  513 | `	};` |
|    - |  514 | `	sxu32 n;` |
|  135 |  515 | `	for( n = 0 ; n < SX_ARRAYSIZE(aState) ; ++n ){` |
|  135 |  516 | `		if( SyStrncmp(zState,aState[n].zState,6) == 0 ){` |
|   67 |  517 | `			return aState[n].zText;` |
|    - |  518 | `		}` |
|   35 |  519 | `	}` |
|  ! 0 |  520 | `	return "General error";` |
|   34 |  521 | `}` |
|    - |  522 | `/*` |
|    - |  523 | ` * A failed OPERATION, routed through ATTR_ERRMODE: silent leaves the answer to` |
|    - |  524 | ` * errorCode()/errorInfo(), warning adds php's E_WARNING naming the method, and` |
|    - |  525 | ` * exception throws. The wording is one sentence in all three:` |
|    - |  526 | `` * `SQLSTATE[HY000]: General error: 1 no such column: bogus`.`` |
|    - |  527 | ` */` |
|   26 |  528 | `PH7_PRIVATE sxi32 PH7_PdoRaise(ph7_context *pCtx,phl_pdo *pConn,const char *zFn)` |
|    1 |  529 | `{` |
|    - |  530 | `	SyBlob sMsg;` |
|   27 |  531 | `	sxi32 rc = PH7_OK;` |
|   27 |  532 | `	if( pConn->iCallbackExc != 0 ){` |
|    3 |  533 | `		sxi32 rcExc = pConn->iCallbackExc;` |
|    3 |  534 | `		pConn->iCallbackExc = 0;` |
|    3 |  535 | `		return rcExc;` |
|    - |  536 | `	}` |
|   25 |  537 | `	if( pConn->iErrMode == PDO_ERRMODE_SILENT ){` |
|    9 |  538 | `		return PH7_OK;` |
|    - |  539 | `	}` |
|   17 |  540 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|   17 |  541 | `	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %d %s",pConn->zSqlState,` |
|   16 |  542 | `		PdoStateDescription(pConn->zSqlState),` |
|   16 |  543 | `		pConn->iDrvCode,pConn->zDrvMsg ? pConn->zDrvMsg : "");` |
|   17 |  544 | `	SyBlobAppend(&sMsg,"",1);` |
|   17 |  545 | `	if( pConn->iErrMode == PDO_ERRMODE_WARNING ){` |
|    3 |  546 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));` |
|    2 |  547 | `	}else{` |
|   22 |  548 | `		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),pConn->zSqlState,` |
|   14 |  549 | `			pConn->iDrvCode,FALSE,pConn->zDrvMsg,3);` |
|    - |  550 | `	}` |
|   17 |  551 | `	SyBlobRelease(&sMsg);` |
|   17 |  552 | `	return rc;` |
|   14 |  553 | `}` |
|    - |  554 | `/*` |
|    - |  555 | ` * The same two raisers, for a failure that belongs to a STATEMENT. They differ` |
|    - |  556 | ` * from the connection's only in which object's SQLSTATE the message carries --` |
|    - |  557 | ` * the driver detail under it is shared -- and in leaving the connection's own` |
|    - |  558 | `` * state alone, which is what lets `$db->errorCode()` read "00000" while`` |
|    - |  559 | `` * `$stmt->errorCode()` reports the failure.`` |
|    - |  560 | ` */` |
|   12 |  561 | `PH7_PRIVATE sxi32 PH7_PdoRaiseStmt(ph7_context *pCtx,phl_pdo_stmt *pSt,const char *zFn)` |
|    1 |  562 | `{` |
|    - |  563 | `	SyBlob sMsg;` |
|   13 |  564 | `	sxi32 rc = PH7_OK;` |
|   13 |  565 | `	phl_pdo *pConn = pSt->pConn;` |
|   13 |  566 | `	if( pConn->iCallbackExc != 0 ){` |
|    - |  567 | `		/* the step did not fail: a userland callback THREW inside it, and what` |
|    - |  568 | `		 * the script must see is that exception rather than a PDOException` |
|    - |  569 | `		 * about the statement sqlite stopped */` |
|  ! 0 |  570 | `		sxi32 rcExc = pConn->iCallbackExc;` |
|  ! 0 |  571 | `		pConn->iCallbackExc = 0;` |
|  ! 0 |  572 | `		return rcExc;` |
|    - |  573 | `	}` |
|   13 |  574 | `	if( pConn->iErrMode == PDO_ERRMODE_SILENT ){` |
|  ! 0 |  575 | `		return PH7_OK;` |
|    - |  576 | `	}` |
|   13 |  577 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|   13 |  578 | `	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %d %s",pSt->zSqlState,` |
|   12 |  579 | `		PdoStateDescription(pSt->zSqlState),pConn->iDrvCode,` |
|   12 |  580 | `		pConn->zDrvMsg ? pConn->zDrvMsg : "");` |
|   13 |  581 | `	SyBlobAppend(&sMsg,"",1);` |
|   13 |  582 | `	if( pConn->iErrMode == PDO_ERRMODE_WARNING ){` |
|  ! 0 |  583 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));` |
|  ! 0 |  584 | `	}else{` |
|   19 |  585 | `		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),pSt->zSqlState,` |
|   12 |  586 | `			pConn->iDrvCode,FALSE,pConn->zDrvMsg,3);` |
|    - |  587 | `	}` |
|   13 |  588 | `	SyBlobRelease(&sMsg);` |
|   13 |  589 | `	return rc;` |
|    7 |  590 | `}` |
|   10 |  591 | `PH7_PRIVATE sxi32 PH7_PdoRaiseImplStmt(ph7_context *pCtx,phl_pdo_stmt *pSt,const char *zFn,` |
|    - |  592 | `	const char *zSqlState,const char *zMsg)` |
|    1 |  593 | `{` |
|    - |  594 | `	SyBlob sMsg;` |
|   11 |  595 | `	sxi32 rc = PH7_OK;` |
|   11 |  596 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|   16 |  597 | `	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %s",zSqlState,` |
|    5 |  598 | `		PdoStateDescription(zSqlState),zMsg);` |
|   11 |  599 | `	SyBlobAppend(&sMsg,"",1);` |
|   11 |  600 | `	if( pSt->pConn->iErrMode == PDO_ERRMODE_EXCEPTION ){` |
|    7 |  601 | `		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),zSqlState,0,FALSE,0,2);` |
|    4 |  602 | `	}else{` |
|    5 |  603 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));` |
|    - |  604 | `	}` |
|   11 |  605 | `	SyBlobRelease(&sMsg);` |
|   11 |  606 | `	return rc;` |
|    1 |  607 | `}` |
|    - |  608 | `/*` |
|    - |  609 | ` * php's pdo_raise_impl_error: a refusal by the LAYER rather than the database` |
|    - |  610 | ` * -- asking a driver for an attribute it does not carry is the whole of it` |
|    - |  611 | ` * here. It differs from the failure above twice over: the warning fires even` |
|    - |  612 | ` * in SILENT mode, and the triple's third cell is absent, so errorInfo() is` |
|    - |  613 | ` * two cells long.` |
|    - |  614 | ` */` |
|   28 |  615 | `PH7_PRIVATE sxi32 PH7_PdoRaiseImpl(ph7_context *pCtx,phl_pdo *pConn,const char *zFn,` |
|    - |  616 | `	const char *zSqlState,const char *zMsg)` |
|    1 |  617 | `{` |
|    - |  618 | `	SyBlob sMsg;` |
|   29 |  619 | `	sxi32 rc = PH7_OK;` |
|   29 |  620 | `	if( pConn ){` |
|   29 |  621 | `		PH7_PdoSetError(pConn,zSqlState,0,0);` |
|   29 |  622 | `		pConn->bNoDrvDetail = 1;` |
|   14 |  623 | `	}` |
|   29 |  624 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|   43 |  625 | `	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %s",zSqlState,` |
|   14 |  626 | `		PdoStateDescription(zSqlState),zMsg);` |
|   29 |  627 | `	SyBlobAppend(&sMsg,"",1);` |
|   29 |  628 | `	if( pConn && pConn->iErrMode == PDO_ERRMODE_EXCEPTION ){` |
|   29 |  629 | `		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),zSqlState,0,FALSE,0,2);` |
|   15 |  630 | `	}else{` |
|  ! 0 |  631 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));` |
|    - |  632 | `	}` |
|   29 |  633 | `	SyBlobRelease(&sMsg);` |
|   29 |  634 | `	return rc;` |
|    1 |  635 | `}` |
|    - |  636 |  |
|    - |  637 | `/* ------------------------------------------------------------------------` |
|    - |  638 | ` * Attributes` |
|    - |  639 | ` * ------------------------------------------------------------------------ */` |
|    - |  640 | `/* php's PDO::ATTR_* numbering, and the two driver attributes this slice reads. */` |
|    - |  641 | `#define PDO_ATTR_AUTOCOMMIT           0` |
|    - |  642 | `#define PDO_ATTR_PREFETCH             1` |
|    - |  643 | `#define PDO_ATTR_TIMEOUT              2` |
|    - |  644 | `#define PDO_ATTR_ERRMODE              3` |
|    - |  645 | `#define PDO_ATTR_SERVER_VERSION       4` |
|    - |  646 | `#define PDO_ATTR_CLIENT_VERSION       5` |
|    - |  647 | `#define PDO_ATTR_SERVER_INFO          6` |
|    - |  648 | `#define PDO_ATTR_CONNECTION_STATUS    7` |
|    - |  649 | `#define PDO_ATTR_CASE                 8` |
|    - |  650 | `#define PDO_ATTR_CURSOR_NAME          9` |
|    - |  651 | `#define PDO_ATTR_CURSOR              10` |
|    - |  652 | `#define PDO_ATTR_ORACLE_NULLS        11` |
|    - |  653 | `#define PDO_ATTR_PERSISTENT          12` |
|    - |  654 | `#define PDO_ATTR_STATEMENT_CLASS     13` |
|    - |  655 | `#define PDO_ATTR_FETCH_TABLE_NAMES   14` |
|    - |  656 | `#define PDO_ATTR_FETCH_CATALOG_NAMES 15` |
|    - |  657 | `#define PDO_ATTR_DRIVER_NAME         16` |
|    - |  658 | `#define PDO_ATTR_STRINGIFY_FETCHES   17` |
|    - |  659 | `#define PDO_ATTR_MAX_COLUMN_LEN      18` |
|    - |  660 | `#define PDO_ATTR_DEFAULT_FETCH_MODE  19` |
|    - |  661 | `#define PDO_ATTR_EMULATE_PREPARES    20` |
|    - |  662 | `#define PDO_ATTR_DEFAULT_STR_PARAM   21` |
|    - |  663 | `#define PDO_SQLITE_ATTR_OPEN_FLAGS            1000` |
|    - |  664 | `#define PDO_SQLITE_ATTR_READONLY_STATEMENT    1001` |
|    - |  665 | `#define PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES 1002` |
|    - |  666 | `#define PDO_SQLITE_ATTR_BUSY_STATEMENT        1003` |
|    - |  667 | `#define PDO_SQLITE_ATTR_TRANSACTION_MODE 1005` |
|    - |  668 |  |
|    - |  669 | `/* One int-keyed element of an array value, or 0 when the key is absent. */` |
|  358 |  670 | `static ph7_value * PdoArrayAtInt(ph7_vm *pVm,ph7_value *pArray,sxi64 iKey)` |
|    3 |  671 | `{` |
|  361 |  672 | `	ph7_hashmap_node *pNode = 0;` |
|    - |  673 | `	ph7_value sKey;` |
|    - |  674 | `	sxi32 rc;` |
|  361 |  675 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 |  676 | `		return 0;` |
|    - |  677 | `	}` |
|  361 |  678 | `	PH7_MemObjInitFromInt(pVm,&sKey,iKey);` |
|  361 |  679 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&sKey,&pNode);` |
|  361 |  680 | `	PH7_MemObjRelease(&sKey);` |
|  361 |  681 | `	if( rc != SXRET_OK \|\| pNode == 0 ){` |
|   14 |  682 | `		return 0;` |
|    - |  683 | `	}` |
|  349 |  684 | `	return (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|  182 |  685 | `}` |
|    - |  686 | `/*` |
|    - |  687 | ` * ATTR_STATEMENT_CLASS's own validation, which is four refusals deep and in` |
|    - |  688 | ` * php's own order: the value must be an array, it must carry a class name at` |
|    - |  689 | ` * index 0, that name must BE a class and must derive from PDOStatement, and` |
|    - |  690 | ` * anything at index 1 must be an array of constructor arguments. Keys past 1` |
|    - |  691 | `` * are ignored, and index 1 refuses even a null -- despite the `?array` the`` |
|    - |  692 | ` * message spells, which is php's wording rather than its test. That message` |
|    - |  693 | ` * also names the type of the WHOLE value rather than the element's, so a` |
|    - |  694 | ` * string at index 1 reports "array given"; both are reproduced.` |
|    - |  695 | ` */` |
|   36 |  696 | `static sxi32 PdoSetStatementClass(ph7_context *pCtx,phl_pdo *pConn,ph7_value *pVal)` |
|    2 |  697 | `{` |
|    - |  698 | `	ph7_value *pName,*pArgs;` |
|    - |  699 | `	ph7_class *pClass,*pBase;` |
|    - |  700 | `	const char *zName;` |
|    - |  701 | `	int nName;` |
|    - |  702 | `	char zBuf[64];` |
|   38 |  703 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    7 |  704 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  705 | `			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS value "` |
|    - |  706 | `			"must be of type array, %s given",` |
|    2 |  707 | `			VmValueGivenName(pVal,zBuf,sizeof(zBuf)));` |
|    - |  708 | `	}` |
|   34 |  709 | `	pName = PdoArrayAtInt(pCtx->pVm,pVal,0);` |
|   34 |  710 | `	if( pName == 0 ){` |
|    5 |  711 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  712 | `			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS value "` |
|    - |  713 | `			"must be an array with the format array(classname, constructor_args)");` |
|    - |  714 | `	}` |
|   30 |  715 | `	zName = 0;` |
|   30 |  716 | `	nName = 0;` |
|   30 |  717 | `	if( pName->iFlags & MEMOBJ_STRING ){` |
|   30 |  718 | `		zName = ph7_value_to_string(pName,&nName);` |
|   14 |  719 | `	}` |
|   30 |  720 | `	pClass = (zName && nName > 0)` |
|   42 |  721 | `		? PH7_VmExtractClass(pCtx->pVm,zName,(sxu32)nName,FALSE,0) : 0;` |
|   30 |  722 | `	if( pClass == 0 ){` |
|    3 |  723 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  724 | `			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS class "` |
|    - |  725 | `			"must be a valid class");` |
|    - |  726 | `	}` |
|   28 |  727 | `	pBase = PH7_VmExtractClass(pCtx->pVm,"PDOStatement",sizeof("PDOStatement")-1,FALSE,0);` |
|   28 |  728 | `	if( pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    5 |  729 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  730 | `			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS class "` |
|    - |  731 | `			"must be derived from PDOStatement");` |
|    - |  732 | `	}` |
|    - |  733 | `	{` |
|    - |  734 | `		/* php builds the statement OBJECT itself and then calls the class's own` |
|    - |  735 | `		 * constructor, so that constructor may not be one a script could call:` |
|    - |  736 | `		 * a PUBLIC one is refused here, and a protected or private one is what` |
|    - |  737 | `		 * the documented subclass declares. */` |
|   24 |  738 | `		ph7_class_method *pCons = PH7_ClassExtractMethod(pClass,"__construct",` |
|    - |  739 | `			sizeof("__construct")-1);` |
|   24 |  740 | `		if( pCons && pCons->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|    3 |  741 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  742 | `				"PDO::setAttribute(): Argument #2 ($value) User-supplied statement "` |
|    - |  743 | `				"class cannot have a public constructor");` |
|    - |  744 | `		}` |
|    - |  745 | `	}` |
|    - |  746 | `	/* Remember it: every later query()/prepare() builds THIS class. The CLASS is` |
|    - |  747 | `	 * stored before the constructor arguments are judged, because php stores it` |
|    - |  748 | ``	 * there too -- a bad `constructor_args` refuses with the new class already`` |
|    - |  749 | `	 * standing and the old arguments already dropped (php acts, then throws). */` |
|   22 |  750 | `	if( pConn->zStmtClass ){` |
|   15 |  751 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pConn->zStmtClass);` |
|   15 |  752 | `		pConn->zStmtClass = 0;` |
|   15 |  753 | `		pConn->nStmtClass = 0;` |
|    7 |  754 | `	}` |
|   22 |  755 | `	if( pConn->pStmtArgs ){` |
|    9 |  756 | `		ph7_release_value(pCtx->pVm,pConn->pStmtArgs);` |
|    9 |  757 | `		pConn->pStmtArgs = 0;` |
|    4 |  758 | `	}` |
|   22 |  759 | `	pConn->zStmtClass = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nName + 1);` |
|   22 |  760 | `	if( pConn->zStmtClass ){` |
|   22 |  761 | `		SyMemcpy(zName,pConn->zStmtClass,(sxu32)nName);` |
|   22 |  762 | `		pConn->zStmtClass[nName] = 0;` |
|   22 |  763 | `		pConn->nStmtClass = nName;` |
|   10 |  764 | `	}` |
|   22 |  765 | `	pArgs = PdoArrayAtInt(pCtx->pVm,pVal,1);` |
|   22 |  766 | `	if( pArgs != 0 && (pArgs->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    3 |  767 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  768 | `			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS "` |
|    - |  769 | `			"constructor_args must be of type ?array, array given");` |
|    - |  770 | `	}` |
|   20 |  771 | `	if( pArgs ){` |
|   11 |  772 | `		pConn->pStmtArgs = ph7_new_array(pCtx->pVm);` |
|   11 |  773 | `		if( pConn->pStmtArgs ){` |
|   11 |  774 | `			PH7_MemObjStore(pArgs,pConn->pStmtArgs);` |
|    5 |  775 | `		}` |
|    5 |  776 | `	}` |
|   20 |  777 | `	return PH7_OK;` |
|   20 |  778 | `}` |
|    - |  779 | `/* php's "the driver has no such attribute" refusal, worded once. */` |
|   28 |  780 | `static sxi32 PdoNoSuchAttr(ph7_context *pCtx,phl_pdo *pConn,const char *zFn)` |
|    1 |  781 | `{` |
|   29 |  782 | `	return PH7_PdoRaiseImpl(pCtx,pConn,zFn,"IM001",` |
|    - |  783 | `		"driver does not support that attribute");` |
|    1 |  784 | `}` |
|    - |  785 | `/*` |
|    - |  786 | ` * PDO::getAttribute(int $attribute): mixed` |
|    - |  787 | ` *` |
|    - |  788 | ` * Only the attributes the sqlite driver actually carries answer; every other` |
|    - |  789 | ` * one -- including the generic PDO::ATTR_* names other drivers implement -- is` |
|    - |  790 | ` * php's IM001. The two version attributes answer the LINKED library's version,` |
|    - |  791 | ` * which is why no test may pin them.` |
|    - |  792 | ` */` |
|  104 |  793 | `static int vm_builtin_PDO_getAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  794 | `{` |
|  106 |  795 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - |  796 | `	ph7_int64 iAttr;` |
|  106 |  797 | `	if( pConn == 0 ){` |
|  ! 0 |  798 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  799 | `	}` |
|  106 |  800 | `	PH7_PdoTouch(pConn);` |
|  106 |  801 | `	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|  106 |  802 | `	switch( iAttr ){` |
|    8 |  803 | `		case PDO_ATTR_ERRMODE:            ph7_result_int(pCtx,pConn->iErrMode); break;` |
|    5 |  804 | `		case PDO_ATTR_CASE:               ph7_result_int(pCtx,pConn->iCase); break;` |
|    5 |  805 | `		case PDO_ATTR_ORACLE_NULLS:       ph7_result_int(pCtx,pConn->iOracleNulls); break;` |
|    5 |  806 | `		case PDO_ATTR_DEFAULT_FETCH_MODE: ph7_result_int(pCtx,pConn->iDefaultFetch); break;` |
|    5 |  807 | `		case PDO_ATTR_STRINGIFY_FETCHES:  ph7_result_bool(pCtx,pConn->bStringify); break;` |
|    6 |  808 | `		case PDO_ATTR_PERSISTENT:         ph7_result_bool(pCtx,pConn->bPersistent); break;` |
|   13 |  809 | `		case PDO_SQLITE_ATTR_TRANSACTION_MODE: ph7_result_int(pCtx,pConn->iTxMode); break;` |
|    - |  810 | `		/* ATTR_EXTENDED_RESULT_CODES is write-ONLY: php refuses to read it back` |
|    - |  811 | `		 * like any attribute the driver does not carry. */` |
|    1 |  812 | `		case PDO_ATTR_DRIVER_NAME:` |
|    3 |  813 | `			ph7_result_string(pCtx,"sqlite",sizeof("sqlite")-1);` |
|    3 |  814 | `			break;` |
|    3 |  815 | `		case PDO_ATTR_SERVER_VERSION:` |
|    - |  816 | `		case PDO_ATTR_CLIENT_VERSION: {` |
|    7 |  817 | `			const char *zVer = PH7_PdoSqliteLibVersion();` |
|    7 |  818 | `			ph7_result_string(pCtx,zVer,(int)SyStrlen(zVer));` |
|    7 |  819 | `			break;` |
|    - |  820 | `		}` |
|   15 |  821 | `		case PDO_ATTR_STATEMENT_CLASS: {` |
|    - |  822 | `			/* php answers the class name alone until a constructor-argument` |
|    - |  823 | `			 * array is set beside it. */` |
|   32 |  824 | `			ph7_value *pArray = ph7_context_new_array(pCtx);` |
|   32 |  825 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|   32 |  826 | `			if( pArray == 0 \|\| pName == 0 ){` |
|  ! 0 |  827 | `				return PH7_ContextMemoryError(pCtx);` |
|    - |  828 | `			}` |
|   32 |  829 | `			if( pConn->zStmtClass ){` |
|   24 |  830 | `				ph7_value_string(pName,pConn->zStmtClass,pConn->nStmtClass);` |
|   13 |  831 | `			}else{` |
|    9 |  832 | `				ph7_value_string(pName,"PDOStatement",sizeof("PDOStatement")-1);` |
|    - |  833 | `			}` |
|   32 |  834 | `			ph7_array_add_elem(pArray,0,pName);` |
|   32 |  835 | `			if( pConn->pStmtArgs ){` |
|    9 |  836 | `				ph7_array_add_elem(pArray,0,pConn->pStmtArgs);` |
|    4 |  837 | `			}` |
|   32 |  838 | `			ph7_result_value(pCtx,pArray);` |
|   32 |  839 | `			break;` |
|    - |  840 | `		}` |
|   14 |  841 | `		default:` |
|   29 |  842 | `			ph7_result_null(pCtx);` |
|   29 |  843 | `			return PdoNoSuchAttr(pCtx,pConn,"PDO::getAttribute");` |
|    - |  844 | `	}` |
|   78 |  845 | `	return PH7_OK;` |
|   54 |  846 | `}` |
|    - |  847 | `/*` |
|    - |  848 | ` * PDO::setAttribute(int $attribute, mixed $value): bool` |
|    - |  849 | ` *` |
|    - |  850 | ` * Three outcomes, and which one an attribute takes is php's own table: the` |
|    - |  851 | ` * five the driver carries return true, the four that describe the CONNECTION` |
|    - |  852 | ` * rather than configure it (the versions, the driver name, persistence) answer` |
|    - |  853 | ` * false without a diagnostic -- as does an attribute no driver defines -- and` |
|    - |  854 | ` * the rest are the same IM001 refusal getAttribute raises.` |
|    - |  855 | ` */` |
|   80 |  856 | `static int vm_builtin_PDO_setAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  857 | `{` |
|   82 |  858 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - |  859 | `	ph7_int64 iAttr;` |
|    - |  860 | `	ph7_value *pVal;` |
|   82 |  861 | `	if( pConn == 0 ){` |
|  ! 0 |  862 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  863 | `	}` |
|   82 |  864 | `	PH7_PdoTouch(pConn);` |
|   82 |  865 | `	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|   82 |  866 | `	pVal = nArg > 1 ? apArg[1] : 0;` |
|   82 |  867 | `	switch( iAttr ){` |
|    4 |  868 | `		case PDO_ATTR_ERRMODE: {` |
|    9 |  869 | `			ph7_int64 iMode = pVal ? ph7_value_to_int64(pVal) : 0;` |
|    8 |  870 | `			if( iMode != PDO_ERRMODE_SILENT && iMode != PDO_ERRMODE_WARNING` |
|    6 |  871 | `			 && iMode != PDO_ERRMODE_EXCEPTION ){` |
|    3 |  872 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  873 | `					"PDO::setAttribute(): Argument #2 ($value) Error mode must be one of "` |
|    - |  874 | `					"the PDO::ERRMODE_* constants");` |
|    - |  875 | `			}` |
|    7 |  876 | `			pConn->iErrMode = (int)iMode;` |
|    7 |  877 | `			break;` |
|    - |  878 | `		}` |
|    2 |  879 | `		case PDO_ATTR_CASE: {` |
|    5 |  880 | `			ph7_int64 iCase = pVal ? ph7_value_to_int64(pVal) : 0;` |
|    5 |  881 | `			if( iCase != PDO_CASE_NATURAL && iCase != PDO_CASE_UPPER && iCase != PDO_CASE_LOWER ){` |
|    3 |  882 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  883 | `					"PDO::setAttribute(): Argument #2 ($value) Case folding mode must be "` |
|    - |  884 | `					"one of the PDO::CASE_* constants");` |
|    - |  885 | `			}` |
|    3 |  886 | `			pConn->iCase = (int)iCase;` |
|    3 |  887 | `			break;` |
|    - |  888 | `		}` |
|    1 |  889 | `		case PDO_ATTR_ORACLE_NULLS:` |
|    3 |  890 | `			pConn->iOracleNulls = (int)(pVal ? ph7_value_to_int64(pVal) : 0);` |
|    3 |  891 | `			break;` |
|    3 |  892 | `		case PDO_ATTR_DEFAULT_FETCH_MODE:` |
|    7 |  893 | `			pConn->iDefaultFetch = (int)(pVal ? ph7_value_to_int64(pVal) : 0);` |
|    7 |  894 | `			break;` |
|    2 |  895 | `		case PDO_ATTR_STRINGIFY_FETCHES:` |
|    5 |  896 | `			pConn->bStringify = pVal ? ph7_value_to_bool(pVal) : 0;` |
|    5 |  897 | `			break;` |
|    1 |  898 | `		case PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES:` |
|    3 |  899 | `			pConn->bExtendedCodes = pVal ? ph7_value_to_bool(pVal) : 0;` |
|    3 |  900 | `			PH7_PdoSqliteExtendedCodes(pConn,pConn->bExtendedCodes);` |
|    3 |  901 | `			break;` |
|    4 |  902 | `		case PDO_SQLITE_ATTR_TRANSACTION_MODE: {` |
|    - |  903 | `			/* only php's three modes; anything else answers false in silence */` |
|    9 |  904 | `			ph7_int64 iTx = pVal ? ph7_value_to_int64(pVal) : 0;` |
|    9 |  905 | `			if( iTx < 0 \|\| iTx > 2 ){` |
|    3 |  906 | `				ph7_result_bool(pCtx,0);` |
|    3 |  907 | `				return PH7_OK;` |
|    - |  908 | `			}` |
|    7 |  909 | `			pConn->iTxMode = (int)iTx;` |
|    7 |  910 | `			break;` |
|    - |  911 | `		}` |
|   18 |  912 | `		case PDO_ATTR_STATEMENT_CLASS: {` |
|    - |  913 | `			/* Validated now; the class is USED when a statement is built. */` |
|   38 |  914 | `			sxi32 rcSet = PdoSetStatementClass(pCtx,pConn,pVal);` |
|   38 |  915 | `			if( rcSet != PH7_OK ){` |
|   19 |  916 | `				return rcSet;` |
|    - |  917 | `			}` |
|   20 |  918 | `			break;` |
|    - |  919 | `		}` |
|    - |  920 | `		/* Read-only descriptions of the connection: php answers false and says` |
|    - |  921 | `		 * nothing at all. An attribute no driver knows lands here too. */` |
|    4 |  922 | `		case PDO_ATTR_SERVER_VERSION:` |
|    - |  923 | `		case PDO_ATTR_CLIENT_VERSION:` |
|    - |  924 | `		case PDO_ATTR_DRIVER_NAME:` |
|    - |  925 | `		case PDO_ATTR_PERSISTENT:` |
|    9 |  926 | `			ph7_result_bool(pCtx,0);` |
|    9 |  927 | `			return PH7_OK;` |
|    - |  928 | `		/* The generic attributes other drivers carry and this one does not. */` |
|  ! 0 |  929 | `		case PDO_ATTR_AUTOCOMMIT:` |
|    - |  930 | `		case PDO_ATTR_PREFETCH:` |
|    - |  931 | `		case PDO_ATTR_TIMEOUT:` |
|    - |  932 | `		case PDO_ATTR_SERVER_INFO:` |
|    - |  933 | `		case PDO_ATTR_CONNECTION_STATUS:` |
|    - |  934 | `		case PDO_ATTR_CURSOR_NAME:` |
|    - |  935 | `		case PDO_ATTR_CURSOR:` |
|    - |  936 | `		case PDO_ATTR_FETCH_TABLE_NAMES:` |
|    - |  937 | `		case PDO_ATTR_FETCH_CATALOG_NAMES:` |
|    - |  938 | `		case PDO_ATTR_MAX_COLUMN_LEN:` |
|    - |  939 | `		case PDO_ATTR_EMULATE_PREPARES:` |
|    - |  940 | `		case PDO_ATTR_DEFAULT_STR_PARAM:` |
|  ! 0 |  941 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  942 | `			return PdoNoSuchAttr(pCtx,pConn,"PDO::setAttribute");` |
|    1 |  943 | `		default:` |
|    3 |  944 | `			ph7_result_bool(pCtx,0);` |
|    3 |  945 | `			return PH7_OK;` |
|    - |  946 | `	}` |
|   48 |  947 | `	ph7_result_bool(pCtx,1);` |
|   48 |  948 | `	return PH7_OK;` |
|   42 |  949 | `}` |
|    - |  950 |  |
|    - |  951 | `/* ------------------------------------------------------------------------` |
|    - |  952 | ` * Running statements, and reporting what happened` |
|    - |  953 | ` * ------------------------------------------------------------------------ */` |
|    - |  954 | `/*` |
|    - |  955 | ` * PDO::exec(string $statement): int\|false` |
|    - |  956 | ` *` |
|    - |  957 | ` * Runs every statement the string holds and answers the number of rows the` |
|    - |  958 | ` * last one CHANGED. A statement that changes nothing -- a SELECT, a CREATE,` |
|    - |  959 | ` * whitespace, a comment -- leaves sqlite's counter alone, so exec() answers` |
|    - |  960 | ` * whatever the previous write did rather than 0; that is php's answer too,` |
|    - |  961 | ` * because php reads the same counter.` |
|    - |  962 | ` */` |
|  240 |  963 | `static int vm_builtin_PDO_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  964 | `{` |
|  244 |  965 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - |  966 | `	const char *zSql;` |
|  244 |  967 | `	int nSql = 0;   /* the length is only written when the argument IS read */` |
|    - |  968 | `	ph7_int64 nChange;` |
|  244 |  969 | `	if( pConn == 0 ){` |
|  ! 0 |  970 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  971 | `	}` |
|  244 |  972 | `	PH7_PdoTouch(pConn);` |
|  244 |  973 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|  244 |  974 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 |  975 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  976 | `			"PDO::exec(): Argument #1 ($statement) must not be empty");` |
|    - |  977 | `	}` |
|  242 |  978 | `	nChange = PH7_PdoSqliteExec(pConn,zSql,nSql);` |
|  242 |  979 | `	if( nChange < 0 ){` |
|   17 |  980 | `		ph7_result_bool(pCtx,0);` |
|   17 |  981 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::exec");` |
|    - |  982 | `	}` |
|  226 |  983 | `	ph7_result_int64(pCtx,nChange);` |
|  226 |  984 | `	return PH7_OK;` |
|  124 |  985 | `}` |
|    - |  986 | `/*` |
|    - |  987 | ` * PDO::errorCode(): ?string` |
|    - |  988 | ` *` |
|    - |  989 | ` * Three answers, not two: a handle nothing has run on yet answers NULL, one` |
|    - |  990 | ` * whose last operation succeeded answers "00000", and a failed one answers the` |
|    - |  991 | ` * SQLSTATE. errorInfo() splits the same three ways, and its FIRST cell is the` |
|    - |  992 | ` * empty string -- not null -- in the never-used case.` |
|    - |  993 | ` */` |
|   20 |  994 | `static int vm_builtin_PDO_errorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  995 | `{` |
|   21 |  996 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|   10 |  997 | `	SXUNUSED(nArg);` |
|   10 |  998 | `	SXUNUSED(apArg);` |
|   21 |  999 | `	if( pConn == 0 ){` |
|  ! 0 | 1000 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 1001 | `	}` |
|   21 | 1002 | `	if( pConn->iErrState == PDO_ERR_NONE ){` |
|    3 | 1003 | `		ph7_result_null(pCtx);` |
|    2 | 1004 | `	}else{` |
|   19 | 1005 | `		ph7_result_string(pCtx,pConn->zSqlState,(int)SyStrlen(pConn->zSqlState));` |
|    - | 1006 | `	}` |
|   21 | 1007 | `	return PH7_OK;` |
|   11 | 1008 | `}` |
|    - | 1009 | `/*` |
|    - | 1010 | ` * The three cells errorInfo() answers, for a connection or a statement alike.` |
|    - | 1011 | ` * The first is the OBJECT's own SQLSTATE; the other two are the DRIVER's last` |
|    - | 1012 | ` * code and message, which are shared and are reported whenever the object's` |
|    - | 1013 | ` * own state is not a success -- so a statement that has never run shows the` |
|    - | 1014 | ` * previous statement's driver detail beside an empty state, exactly as php` |
|    - | 1015 | ` * does.` |
|    - | 1016 | ` */` |
|   18 | 1017 | `static int PdoBuildErrorInfo(ph7_context *pCtx,phl_pdo *pConn,int iErrState,` |
|    - | 1018 | `	const char *zSqlState)` |
|    1 | 1019 | `{` |
|   19 | 1020 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|   19 | 1021 | `	ph7_value *pCell = ph7_context_new_scalar(pCtx);` |
|   19 | 1022 | `	if( pArray == 0 \|\| pCell == 0 ){` |
|  ! 0 | 1023 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1024 | `	}` |
|   19 | 1025 | `	if( iErrState == PDO_ERR_NONE ){` |
|    3 | 1026 | `		ph7_value_string(pCell,"",0);` |
|    2 | 1027 | `	}else{` |
|   17 | 1028 | `		ph7_value_string(pCell,zSqlState,(int)SyStrlen(zSqlState));` |
|    - | 1029 | `	}` |
|   19 | 1030 | `	ph7_array_add_elem(pArray,0,pCell);` |
|   19 | 1031 | `	if( iErrState != PDO_ERR_OK && pConn->iDrvCode != 0 && !pConn->bNoDrvDetail ){` |
|   11 | 1032 | `		ph7_value_int64(pCell,(ph7_int64)pConn->iDrvCode);` |
|   11 | 1033 | `		ph7_array_add_elem(pArray,0,pCell);` |
|   11 | 1034 | `		PH7_MemObjRelease(pCell);` |
|   11 | 1035 | `		if( pConn->zDrvMsg ){` |
|   11 | 1036 | `			ph7_value_string(pCell,pConn->zDrvMsg,(int)SyStrlen(pConn->zDrvMsg));` |
|    6 | 1037 | `		}else{` |
|  ! 0 | 1038 | `			ph7_value_null(pCell);` |
|    - | 1039 | `		}` |
|   11 | 1040 | `		ph7_array_add_elem(pArray,0,pCell);` |
|    6 | 1041 | `	}else{` |
|    9 | 1042 | `		ph7_value_null(pCell);` |
|    9 | 1043 | `		ph7_array_add_elem(pArray,0,pCell);` |
|    9 | 1044 | `		ph7_array_add_elem(pArray,0,pCell);` |
|    - | 1045 | `	}` |
|   19 | 1046 | `	ph7_result_value(pCtx,pArray);` |
|   19 | 1047 | `	return PH7_OK;` |
|   10 | 1048 | `}` |
|    - | 1049 | `/* A statement's own state, moved to "the last thing succeeded". */` |
| 1132 | 1050 | `static void PdoStmtOk(phl_pdo_stmt *pSt)` |
|    3 | 1051 | `{` |
| 1135 | 1052 | `	pSt->iErrState = PDO_ERR_OK;` |
| 1135 | 1053 | `	SyMemcpy("00000",pSt->zSqlState,sizeof("00000"));` |
| 1135 | 1054 | `}` |
|    - | 1055 | `/* A statement's own state, moved to a failure. The driver detail (if any) is` |
|    - | 1056 | ` * already on the connection, where both objects read it from. */` |
|   16 | 1057 | `static void PdoStmtFailed(phl_pdo_stmt *pSt,const char *zSqlState)` |
|    1 | 1058 | `{` |
|    - | 1059 | `	sxu32 n;` |
|   17 | 1060 | `	pSt->iErrState = PDO_ERR_FAILED;` |
|   97 | 1061 | `	for( n = 0 ; n < 5 && zSqlState[n] ; ++n ){` |
|   81 | 1062 | `		pSt->zSqlState[n] = zSqlState[n];` |
|   41 | 1063 | `	}` |
|   17 | 1064 | `	pSt->zSqlState[n] = 0;` |
|   17 | 1065 | `}` |
|   16 | 1066 | `static int vm_builtin_PDO_errorInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1067 | `{` |
|   17 | 1068 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    8 | 1069 | `	SXUNUSED(nArg);` |
|    8 | 1070 | `	SXUNUSED(apArg);` |
|   17 | 1071 | `	if( pConn == 0 ){` |
|  ! 0 | 1072 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 1073 | `	}` |
|   17 | 1074 | `	return PdoBuildErrorInfo(pCtx,pConn,pConn->iErrState,pConn->zSqlState);` |
|    9 | 1075 | `}` |
|    - | 1076 | `/*` |
|    - | 1077 | ` * PDO::lastInsertId(?string $name = null): string\|false` |
|    - | 1078 | ` *` |
|    - | 1079 | ` * sqlite's rowid of the last insert, as a STRING -- php's portable answer,` |
|    - | 1080 | ` * since another driver's sequence may not fit an int. A handle that has` |
|    - | 1081 | ` * inserted nothing answers "0" rather than false, and the $name a sequence` |
|    - | 1082 | ` * driver would use is accepted and ignored here, as php accepts it.` |
|    - | 1083 | ` */` |
|   10 | 1084 | `static int vm_builtin_PDO_lastInsertId(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1085 | `{` |
|   11 | 1086 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 1087 | `	char zBuf[32];` |
|    - | 1088 | `	int nBuf;` |
|    5 | 1089 | `	SXUNUSED(nArg);` |
|    5 | 1090 | `	SXUNUSED(apArg);` |
|   11 | 1091 | `	if( pConn == 0 ){` |
|  ! 0 | 1092 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 1093 | `	}` |
|   11 | 1094 | `	PH7_PdoTouch(pConn);` |
|   11 | 1095 | `	nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%qd",PH7_PdoSqliteLastInsertId(pConn));` |
|   11 | 1096 | `	ph7_result_string(pCtx,zBuf,nBuf);` |
|   11 | 1097 | `	return PH7_OK;` |
|    6 | 1098 | `}` |
|    - | 1099 |  |
|    - | 1100 | `/* ------------------------------------------------------------------------` |
|    - | 1101 | ` * Statements: running one, and reading its rows` |
|    - | 1102 | ` * ------------------------------------------------------------------------ */` |
|    - | 1103 | `/* php's PDO::FETCH_* values. */` |
|    - | 1104 | `#define PDO_FETCH_DEFAULT 0` |
|    - | 1105 | `#define PDO_FETCH_ASSOC   2` |
|    - | 1106 | `#define PDO_FETCH_NUM     3` |
|    - | 1107 | `#define PDO_FETCH_OBJ     5` |
|    - | 1108 | `#define PDO_FETCH_NAMED  11` |
|    - | 1109 |  |
|    - | 1110 | `/*` |
|    - | 1111 | ` * php's fetch FLAGS, which ride on top of a mode. These are the values the` |
|    - | 1112 | ` * SCRIPT sees (PDO::FETCH_GROUP is 32), not the shifted ones php uses inside` |
|    - | 1113 | ` * its own C -- the modes themselves occupy the low four bits.` |
|    - | 1114 | ` */` |
|    - | 1115 | `#define PDO_FETCH_MODE_MASK   0x0F` |
|    - | 1116 | `#define PDO_FETCH_GROUP       0x20` |
|    - | 1117 | `#define PDO_FETCH_UNIQUE      0x40` |
|    - | 1118 | `#define PDO_FETCH_CLASSTYPE   0x80` |
|    - | 1119 | `#define PDO_FETCH_PROPS_LATE  0x100` |
|    - | 1120 | `#define PDO_FETCH_SERIALIZE   0x200` |
|    - | 1121 | `#define PDO_FETCH_FLAGS       (~PDO_FETCH_MODE_MASK)` |
|    - | 1122 | `#define PDO_FETCH_BOUND        6` |
|    - | 1123 | `#define PDO_FETCH_COLUMN       7` |
|    - | 1124 | `#define PDO_FETCH_CLASS        8` |
|    - | 1125 | `#define PDO_FETCH_FUNC        10` |
|    - | 1126 | `#define PDO_FETCH_KEY_PAIR    12` |
|    - | 1127 |  |
|    - | 1128 | `/*` |
|    - | 1129 | ` * php's three CLASS-only flags refuse to ride on any other mode, and the` |
|    - | 1130 | ` * refusal names all three whichever one was set. Every entry point that takes` |
|    - | 1131 | ` * a mode checks this before it counts arguments.` |
|    - | 1132 | ` */` |
|  954 | 1133 | `static sxi32 PdoCheckFetchFlags(ph7_context *pCtx,int iMode,const char *zFn,` |
|    - | 1134 | `	int iArgNo,const char *zParam)` |
|    2 | 1135 | `{` |
|  956 | 1136 | `	int iFlags = iMode & (PDO_FETCH_CLASSTYPE\|PDO_FETCH_SERIALIZE\|PDO_FETCH_PROPS_LATE);` |
|  956 | 1137 | `	if( iFlags != 0 && (iMode & PDO_FETCH_MODE_MASK) != PDO_FETCH_CLASS ){` |
|    7 | 1138 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1139 | `			"%s(): Argument #%d ($%s) cannot use PDO::FETCH_CLASSTYPE, "` |
|    - | 1140 | `			"PDO::FETCH_PROPS_LATE, or PDO::FETCH_SERIALIZE fetch flags with a fetch "` |
|    2 | 1141 | `			"mode other than PDO::FETCH_CLASS",zFn,iArgNo,zParam);` |
|    - | 1142 | `	}` |
|  952 | 1143 | `	return PH7_OK;` |
|  479 | 1144 | `}` |
|    - | 1145 | `#define PDO_FETCH_LAZY         1` |
|    - | 1146 | `#define PDO_FETCH_INTO         9` |
|    - | 1147 |  |
|    - | 1148 | `/* Drop whatever a previous setFetchMode() attached to the statement. */` |
| 1474 | 1149 | `static void PdoStmtClearFetchState(phl_pdo_stmt *pSt)` |
|    3 | 1150 | `{` |
| 1477 | 1151 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
| 1477 | 1152 | `	if( pSt->zFetchClass ){` |
|   25 | 1153 | `		SyMemBackendFree(&pVm->sAllocator,pSt->zFetchClass);` |
|   25 | 1154 | `		pSt->zFetchClass = 0;` |
|   25 | 1155 | `		pSt->nFetchClass = 0;` |
|   12 | 1156 | `	}` |
| 1477 | 1157 | `	if( pSt->pFetchArgs ){` |
|    9 | 1158 | `		ph7_release_value(pVm,pSt->pFetchArgs);` |
|    9 | 1159 | `		pSt->pFetchArgs = 0;` |
|    4 | 1160 | `	}` |
| 1477 | 1161 | `	if( pSt->pFetchInto ){` |
|   11 | 1162 | `		ph7_class_instance *pObj = pSt->pFetchInto;` |
|   11 | 1163 | `		pSt->pFetchInto = 0;` |
|   11 | 1164 | `		PH7_ClassInstanceUnref(pObj);` |
|    5 | 1165 | `	}` |
| 1477 | 1166 | `}` |
|    - | 1167 |  |
|    - | 1168 | `/* Call a constructor with the arguments FETCH_CLASS was given, if any. */` |
|   56 | 1169 | `static void PdoCallCtor(ph7_vm *pVm,ph7_class_instance *pObj,ph7_class_method *pCons,` |
|    - | 1170 | `	ph7_value *pArgs)` |
|    1 | 1171 | `{` |
|    - | 1172 | `	ph7_value *apArg[16];` |
|   57 | 1173 | `	int nArg = 0;` |
|   57 | 1174 | `	if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) ){` |
|   25 | 1175 | `		ph7_hashmap *pMap = (ph7_hashmap *)pArgs->x.pOther;` |
|   25 | 1176 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|   25 | 1177 | `		sxu32 n,nCount = pMap->nEntry;` |
|   45 | 1178 | `		for( n = 0 ; n < nCount && pEntry && nArg < (int)SX_ARRAYSIZE(apArg) ;` |
|   21 | 1179 | `		     ++n, pEntry = pEntry->pPrev ){` |
|   21 | 1180 | `			ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|   21 | 1181 | `			if( pVal ){` |
|   21 | 1182 | `				apArg[nArg++] = pVal;` |
|   10 | 1183 | `			}` |
|   11 | 1184 | `		}` |
|   12 | 1185 | `	}` |
|    - | 1186 | `	/* php's ENGINE builds the object and then calls its constructor, so the` |
|    - | 1187 | ``	 * scope check a script's `new` would take does not apply: a FETCH_CLASS`` |
|    - | 1188 | `	 * class and a statement class alike may declare a private or protected` |
|    - | 1189 | `	 * constructor -- the statement one MUST (a public one is refused where the` |
|    - | 1190 | `	 * attribute is set). */` |
|   57 | 1191 | `	PH7_VmCallMethodUnchecked(pVm,pObj,pCons,0,nArg,nArg ? apArg : 0);` |
|   57 | 1192 | `}` |
|    - | 1193 | `/*` |
|    - | 1194 | ` * A column NAME as the connection presents it: ATTR_CASE folds it, and php` |
|    - | 1195 | ` * folds the name only -- never a value, and never a positional key.` |
|    - | 1196 | ` */` |
| 1374 | 1197 | `static void PdoColumnName(phl_pdo *pConn,const char *zName,SyBlob *pOut)` |
|    2 | 1198 | `{` |
|    - | 1199 | `	sxu32 n;` |
| 1376 | 1200 | `	sxu32 nName = SyStrlen(zName);` |
|    - | 1201 | `	char *zBuf;` |
| 1376 | 1202 | `	SyBlobReset(pOut);` |
| 1376 | 1203 | `	SyBlobAppend(pOut,zName,nName);` |
|    - | 1204 | `	/* the array setter takes a C STRING and no length, so the terminator is` |
|    - | 1205 | `	 * part of the buffer and never part of the name */` |
| 1376 | 1206 | `	SyBlobAppend(pOut,"",1);` |
| 1376 | 1207 | `	if( pConn->iCase == PDO_CASE_NATURAL \|\| nName < 1 ){` |
| 1322 | 1208 | `		return;` |
|    - | 1209 | `	}` |
|   55 | 1210 | `	zBuf = (char *)SyBlobData(pOut);` |
|  247 | 1211 | `	for( n = 0 ; n < nName ; ++n ){` |
|  385 | 1212 | `		zBuf[n] = (char)(pConn->iCase == PDO_CASE_UPPER` |
|  192 | 1213 | `			? SyToUpper(zBuf[n]) : SyToLower(zBuf[n]));` |
|   97 | 1214 | `	}` |
|  689 | 1215 | `}` |
|    - | 1216 | `/*` |
|    - | 1217 | ` * The two rewrites php applies to a fetched VALUE, in php's own order.` |
|    - | 1218 | ` *` |
|    - | 1219 | ` * ATTR_STRINGIFY_FETCHES turns everything the driver typed into a string --` |
|    - | 1220 | ` * everything except a null, which stays null. ATTR_ORACLE_NULLS is the empty` |
|    - | 1221 | ` * string and null trading places: NULL_EMPTY_STRING makes an empty string` |
|    - | 1222 | ` * null, NULL_TO_STRING makes a null the empty string. Neither touches a string` |
|    - | 1223 | ` * that merely LOOKS empty, so a single space survives both.` |
|    - | 1224 | ` */` |
| 1266 | 1225 | `static void PdoApplyValueMods(phl_pdo *pConn,ph7_value *pVal)` |
|    2 | 1226 | `{` |
| 1268 | 1227 | `	if( pConn->bStringify && (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|    9 | 1228 | `		int nByte = 0;` |
|    9 | 1229 | `		const char *zStr = ph7_value_to_string(pVal,&nByte);` |
|    - | 1230 | `		SyBlob sTmp;` |
|    9 | 1231 | `		SyBlobInit(&sTmp,&pConn->pVm->sAllocator);` |
|    9 | 1232 | `		SyBlobAppend(&sTmp,zStr,(sxu32)nByte);` |
|    9 | 1233 | `		PH7_MemObjRelease(pVal);` |
|    9 | 1234 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sTmp),(int)SyBlobLength(&sTmp));` |
|    9 | 1235 | `		SyBlobRelease(&sTmp);` |
|    4 | 1236 | `	}` |
| 1268 | 1237 | `	if( pConn->iOracleNulls == PDO_NULL_EMPTY_STRING ){` |
|    9 | 1238 | `		if( (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) == 0 ){` |
|    3 | 1239 | `			PH7_MemObjRelease(pVal);` |
|    3 | 1240 | `			ph7_value_null(pVal);` |
|    2 | 1241 | `		}` |
| 1264 | 1242 | `	}else if( pConn->iOracleNulls == PDO_NULL_TO_STRING ){` |
|   17 | 1243 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|    9 | 1244 | `			PH7_MemObjRelease(pVal);` |
|    9 | 1245 | `			ph7_value_string(pVal,"",0);` |
|    4 | 1246 | `		}` |
|    8 | 1247 | `	}` |
| 1268 | 1248 | `}` |
|    - | 1249 |  |
|    - | 1250 | `/*` |
|    - | 1251 | ` * Drop the captured row a PDO::FETCH_LAZY object reads through. Every column` |
|    - | 1252 | ` * then answers null, which is what php's row does once the walk runs out.` |
|    - | 1253 | ` */` |
| 1038 | 1254 | `static void PdoStmtLazyClear(phl_pdo_stmt *pSt)` |
|    3 | 1255 | `{` |
| 1041 | 1256 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
| 1041 | 1257 | `	if( pSt->pLazyVals ){` |
|   61 | 1258 | `		ph7_release_value(pVm,pSt->pLazyVals);` |
|   61 | 1259 | `		pSt->pLazyVals = 0;` |
|   30 | 1260 | `	}` |
| 1041 | 1261 | `}` |
|    - | 1262 | `/*` |
|    - | 1263 | ` * Capture the row under the cursor for the lazy object: the RAW column values,` |
|    - | 1264 | ` * positionally. The value modifiers are NOT applied -- php's row reads them at` |
|    - | 1265 | ` * property-access time, so a STRINGIFY_FETCHES turned on between two reads` |
|    - | 1266 | ` * shows in the second -- and the NAMES are not captured at all, because they` |
|    - | 1267 | ` * are the statement's and outlive any one row.` |
|    - | 1268 | ` */` |
|   60 | 1269 | `static void PdoStmtLazyCapture(phl_pdo_stmt *pSt)` |
|    1 | 1270 | `{` |
|   61 | 1271 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
|    - | 1272 | `	int nCol,iCol;` |
|    - | 1273 | `	ph7_value *pCell;` |
|   61 | 1274 | `	PdoStmtLazyClear(pSt);` |
|   61 | 1275 | `	pSt->pLazyVals = ph7_new_array(pVm);` |
|   61 | 1276 | `	pCell = ph7_new_scalar(pVm);` |
|   61 | 1277 | `	if( pSt->pLazyVals == 0 \|\| pCell == 0 ){` |
|  ! 0 | 1278 | `		if( pCell ){ ph7_release_value(pVm,pCell); }` |
|  ! 0 | 1279 | `		PdoStmtLazyClear(pSt);` |
|  ! 0 | 1280 | `		return;` |
|    - | 1281 | `	}` |
|   61 | 1282 | `	nCol = PH7_PdoSqliteColumnCount(pSt);` |
|  207 | 1283 | `	for( iCol = 0 ; iCol < nCol ; ++iCol ){` |
|  147 | 1284 | `		PH7_PdoSqliteColumnValue(pSt,iCol,pCell);` |
|  147 | 1285 | `		ph7_array_add_elem(pSt->pLazyVals,0,pCell);` |
|   74 | 1286 | `	}` |
|   61 | 1287 | `	ph7_release_value(pVm,pCell);` |
|   31 | 1288 | `}` |
|    - | 1289 | `/*` |
|    - | 1290 | ` * Keep the lazy object in step with the cursor: the row about to be handed out` |
|    - | 1291 | ` * becomes what it reads, and a cursor with nothing left clears it. Called from` |
|    - | 1292 | ` * every verb that consumes a row, and only while such an object exists.` |
|    - | 1293 | ` */` |
| 1250 | 1294 | `static void PdoStmtLazySync(phl_pdo_stmt *pSt)` |
|    2 | 1295 | `{` |
| 1252 | 1296 | `	if( pSt->pLazyRow == 0 ){` |
| 1228 | 1297 | `		return;` |
|    - | 1298 | `	}` |
|   25 | 1299 | `	if( pSt->bRowPending ){` |
|   15 | 1300 | `		PdoStmtLazyCapture(pSt);` |
|    8 | 1301 | `	}else{` |
|   11 | 1302 | `		PdoStmtLazyClear(pSt);` |
|    - | 1303 | `	}` |
|  627 | 1304 | `}` |
|    - | 1305 | `/*` |
|    - | 1306 | ` * Does the cursor have a row for the verb about to ask? Answering that is also` |
|    - | 1307 | ` * the moment a lazy object has to be brought in step, because a verb that` |
|    - | 1308 | ` * finds NOTHING is what empties php's row -- every column of it reads null` |
|    - | 1309 | ` * from there on, names and all.` |
|    - | 1310 | ` */` |
|  724 | 1311 | `static int PdoStmtHasRow(phl_pdo_stmt *pSt)` |
|    2 | 1312 | `{` |
|  726 | 1313 | `	PdoStmtLazySync(pSt);` |
|  726 | 1314 | `	return pSt->bRowPending;` |
|    2 | 1315 | `}` |
|    - | 1316 | `/*` |
|    - | 1317 | ` * Step the cursor once and remember what happened. php's driver does this at` |
|    - | 1318 | ` * execute() so columnCount() has an answer before anything is fetched, and the` |
|    - | 1319 | ` * row it lands on is the one the FIRST fetch() hands back.` |
|    - | 1320 | ` *` |
|    - | 1321 | ` * No diagnostic is raised here: the iterator walks through this too, and its` |
|    - | 1322 | ` * vtable is handed a VM with no call context to raise INTO. The failure is` |
|    - | 1323 | ` * recorded on the connection either way, and the callers that DO have a` |
|    - | 1324 | ` * context route it.` |
|    - | 1325 | ` */` |
| 1478 | 1326 | `static int PdoStmtStep(phl_pdo_stmt *pSt)` |
|    3 | 1327 | `{` |
| 1481 | 1328 | `	int rc = PH7_PdoSqliteStep(pSt);` |
| 1481 | 1329 | `	if( rc < 0 ){` |
|    5 | 1330 | `		pSt->bDone = 1;` |
|    5 | 1331 | `		pSt->bRowPending = 0;` |
|    5 | 1332 | `		return -1;` |
|    - | 1333 | `	}` |
| 1477 | 1334 | `	pSt->bRowPending = (rc == 1);` |
| 1477 | 1335 | `	pSt->bDone = (rc == 0);` |
| 1477 | 1336 | `	return rc;` |
|  742 | 1337 | `}` |
|    - | 1338 | `/*` |
|    - | 1339 | ` * Build one row in the requested shape.  Answers 0 when the cursor has nothing` |
|    - | 1340 | ` * to hand back, which is what makes fetch() answer false at the end.` |
|    - | 1341 | ` */` |
|  526 | 1342 | `static int PdoStmtRowFrom(ph7_vm *pVm,phl_pdo_stmt *pSt,int iMode,ph7_value *pOut,` |
|    - | 1343 | `	int iFirstCol)` |
|    2 | 1344 | `{` |
|    - | 1345 | `	int nCol,iCol;` |
|    - | 1346 | `	ph7_value *pCell;` |
|    - | 1347 | `	SyBlob sName;` |
|    - | 1348 | `	/* Whatever shape this row is asked for, it is also the row a lazy object` |
|    - | 1349 | `	 * handed out earlier now reads -- php's is a view of the same cursor. */` |
|  528 | 1350 | `	PdoStmtLazySync(pSt);` |
|  528 | 1351 | `	if( !pSt->bRowPending ){` |
|  ! 0 | 1352 | `		return 0;` |
|    - | 1353 | `	}` |
|  528 | 1354 | `	PdoBoundColumnsForRow(pVm,pSt);` |
|  528 | 1355 | `	nCol = PH7_PdoSqliteColumnCount(pSt);` |
|  528 | 1356 | `	pCell = ph7_new_scalar(pVm);` |
|  528 | 1357 | `	if( pCell == 0 ){` |
|  ! 0 | 1358 | `		return 0;` |
|    - | 1359 | `	}` |
|  528 | 1360 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
| 1514 | 1361 | `	for( iCol = iFirstCol ; iCol < nCol ; ++iCol ){` |
|  988 | 1362 | `		PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sName);` |
|  988 | 1363 | `		PH7_PdoSqliteColumnValue(pSt,iCol,pCell);` |
|  988 | 1364 | `		PdoApplyValueMods(pSt->pConn,pCell);` |
|    - | 1365 | `		/* FETCH_BOTH is not a third shape: it is both of the other two, so` |
|    - | 1366 | `		 * every column lands twice -- named, then positional. FETCH_OBJ builds` |
|    - | 1367 | `		 * the named shape and is converted below. */` |
|  988 | 1368 | `		if( iMode != PDO_FETCH_NUM ){` |
|  591 | 1369 | `			const char *zKey = (const char *)SyBlobData(&sName);` |
|  591 | 1370 | `			int nKey = (int)SyBlobLength(&sName) - 1;   /* less the terminator */` |
|  591 | 1371 | `			if( iMode == PDO_FETCH_NAMED ){` |
|    - | 1372 | `				/* php's answer to two columns of one name: the first stays a` |
|    - | 1373 | `				 * scalar, and a second occurrence turns the entry into a LIST` |
|    - | 1374 | `				 * of every value under that name. Every other named mode keeps` |
|    - | 1375 | `				 * the last one and drops the rest. */` |
|   27 | 1376 | `				ph7_value *pPrev = ph7_array_fetch(pOut,zKey,nKey);` |
|   27 | 1377 | `				if( pPrev == 0 ){` |
|   23 | 1378 | `					ph7_array_add_strkey_elem(pOut,zKey,pCell);` |
|   16 | 1379 | `				}else if( pPrev->iFlags & MEMOBJ_HASHMAP ){` |
|    3 | 1380 | `					ph7_array_add_elem(pPrev,0,pCell);` |
|    2 | 1381 | `				}else{` |
|    3 | 1382 | `					ph7_value *pList = ph7_new_array(pVm);` |
|    3 | 1383 | `					if( pList ){` |
|    3 | 1384 | `						ph7_array_add_elem(pList,0,pPrev);` |
|    3 | 1385 | `						ph7_array_add_elem(pList,0,pCell);` |
|    3 | 1386 | `						ph7_array_add_strkey_elem(pOut,zKey,pList);` |
|    3 | 1387 | `						ph7_release_value(pVm,pList);` |
|    1 | 1388 | `					}` |
|    - | 1389 | `				}` |
|   14 | 1390 | `			}else{` |
|  565 | 1391 | `				ph7_array_add_strkey_elem(pOut,zKey,pCell);` |
|    - | 1392 | `			}` |
|  295 | 1393 | `		}` |
|  986 | 1394 | `		if( iMode != PDO_FETCH_ASSOC && iMode != PDO_FETCH_OBJ` |
|  648 | 1395 | `		 && iMode != PDO_FETCH_NAMED ){` |
|  586 | 1396 | `			if( iMode == PDO_FETCH_NUM ){` |
|    - | 1397 | `				/* a NUM row is renumbered from 0 when a leading column was` |
|    - | 1398 | `				 * dropped; a BOTH row keeps the column's original position,` |
|    - | 1399 | `				 * which is php's own asymmetry under FETCH_GROUP */` |
|  398 | 1400 | `				ph7_array_add_elem(pOut,0,pCell);` |
|  200 | 1401 | `			}else{` |
|    - | 1402 | `				ph7_value sIdx;` |
|  189 | 1403 | `				PH7_MemObjInitFromInt(pVm,&sIdx,(sxi64)iCol);` |
|  189 | 1404 | `				ph7_array_add_elem(pOut,&sIdx,pCell);` |
|  189 | 1405 | `				PH7_MemObjRelease(&sIdx);` |
|    - | 1406 | `			}` |
|  292 | 1407 | `		}` |
|  495 | 1408 | `	}` |
|  528 | 1409 | `	SyBlobRelease(&sName);` |
|  528 | 1410 | `	ph7_release_value(pVm,pCell);` |
|  528 | 1411 | `	if( iMode == PDO_FETCH_OBJ ){` |
|    - | 1412 | `		/* php's stdClass row is the associative one cast to an object -- one` |
|    - | 1413 | ``		 * DYNAMIC property per column, which is what `(object)` builds and what`` |
|    - | 1414 | `		 * json_decode() answers for the same reason. */` |
|   31 | 1415 | `		PH7_MemObjToObject(pOut);` |
|   15 | 1416 | `	}` |
|    - | 1417 | `	/* the row is spent: the next step looks for another */` |
|  528 | 1418 | `	pSt->bRowPending = 0;` |
|  528 | 1419 | `	return 1;` |
|  265 | 1420 | `}` |
|  362 | 1421 | `static int PdoStmtRow(ph7_vm *pVm,phl_pdo_stmt *pSt,int iMode,ph7_value *pOut)` |
|    2 | 1422 | `{` |
|  364 | 1423 | `	return PdoStmtRowFrom(pVm,pSt,iMode,pOut,0);` |
|    2 | 1424 | `}` |
|    - | 1425 | `/*` |
|    - | 1426 | ` * Write one column onto an object.  A DECLARED property takes the native` |
|    - | 1427 | ` * setter, whatever its visibility -- php fills a private or protected one` |
|    - | 1428 | ` * named like a column just the same. A class that declares nothing (stdClass,` |
|    - | 1429 | ` * which is what FETCH_CLASS falls back to) gets a dynamic property instead,` |
|    - | 1430 | `` * the same one an `(object)` cast would create.`` |
|    - | 1431 | ` */` |
|  162 | 1432 | `static void PdoWriteOneProp(ph7_vm *pVm,ph7_class_instance *pObj,const char *zKey,` |
|    - | 1433 | `	int nKey,ph7_value *pVal)` |
|    1 | 1434 | `{` |
|  163 | 1435 | `	if( PH7_NativeAttr(pObj,zKey) != 0 ){` |
|  131 | 1436 | `		PH7_NativeSetProp(pVm,pObj,zKey,(sxu32)nKey,pVal);` |
|  131 | 1437 | `		return;` |
|    - | 1438 | `	}` |
|    - | 1439 | `	{` |
|    - | 1440 | `		SyString sName;` |
|    - | 1441 | `		ph7_value *pSlot;` |
|   33 | 1442 | `		SyStringInitFromBuf(&sName,zKey,nKey);` |
|   33 | 1443 | `		pSlot = PH7_ClassInstanceFetchAttr(pObj,&sName);` |
|   33 | 1444 | `		if( pSlot ){` |
|  ! 0 | 1445 | `			PH7_MemObjStore(pVal,pSlot);` |
|  ! 0 | 1446 | `			return;` |
|    - | 1447 | `		}` |
|   33 | 1448 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pObj,zKey,(sxu32)nKey,0);` |
|   33 | 1449 | `		if( pSlot ){` |
|   33 | 1450 | `			PH7_MemObjStore(pVal,pSlot);` |
|   16 | 1451 | `		}` |
|    - | 1452 | `	}` |
|   82 | 1453 | `}` |
|    - | 1454 | `/* Write every column of a row onto an object, visibility ignored -- php fills` |
|    - | 1455 | ` * a private or protected property named like a column just the same. */` |
|   14 | 1456 | `static void PdoWriteRowProps(ph7_vm *pVm,ph7_class_instance *pObj,ph7_value *pRow)` |
|    1 | 1457 | `{` |
|    - | 1458 | `	ph7_hashmap *pMap;` |
|    - | 1459 | `	ph7_hashmap_node *pEntry;` |
|    - | 1460 | `	sxu32 n,nCount;` |
|   15 | 1461 | `	if( pRow == 0 \|\| (pRow->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 1462 | `		return;` |
|    - | 1463 | `	}` |
|   15 | 1464 | `	pMap = (ph7_hashmap *)pRow->x.pOther;` |
|   15 | 1465 | `	pEntry = pMap->pFirst;` |
|   15 | 1466 | `	nCount = pMap->nEntry;` |
|   43 | 1467 | `	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 1468 | `		ph7_value sKey;` |
|   29 | 1469 | `		ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|   29 | 1470 | `		int nKey = 0;` |
|    - | 1471 | `		const char *zKey;` |
|   29 | 1472 | `		PH7_MemObjInit(pVm,&sKey);` |
|   29 | 1473 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   29 | 1474 | `		zKey = ph7_value_to_string(&sKey,&nKey);` |
|   29 | 1475 | `		if( pVal && zKey && nKey > 0 ){` |
|   29 | 1476 | `			PdoWriteOneProp(pVm,pObj,zKey,nKey,pVal);` |
|   14 | 1477 | `		}` |
|   29 | 1478 | `		PH7_MemObjRelease(&sKey);` |
|   15 | 1479 | `	}` |
|    8 | 1480 | `}` |
|    - | 1481 | `/*` |
|    - | 1482 | ` * Build one object for FETCH_CLASS / fetchObject().  php writes the columns as` |
|    - | 1483 | ` * properties and runs the constructor AFTER them, so a constructor that` |
|    - | 1484 | ` * assigns a property wins over the column of the same name -- unless` |
|    - | 1485 | ` * FETCH_PROPS_LATE reverses the order, which is the whole point of that flag.` |
|    - | 1486 | ` * The write ignores visibility: a private or protected property named like a` |
|    - | 1487 | ` * column is filled just the same, which is why this cannot go through the` |
|    - | 1488 | ` * ordinary property-store path.` |
|    - | 1489 | ` */` |
|   76 | 1490 | `static int PdoRowIntoObject(ph7_vm *pVm,phl_pdo_stmt *pSt,ph7_class *pClass,` |
|    - | 1491 | `	ph7_value *pArgs,int bPropsLate,int iFirstCol,ph7_value *pResult)` |
|    1 | 1492 | `{` |
|    - | 1493 | `	ph7_class_instance *pObj;` |
|    - | 1494 | `	ph7_class_method *pCons;` |
|    - | 1495 | `	ph7_value *pRow;` |
|   77 | 1496 | `	int rc = 0;` |
|   77 | 1497 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|   77 | 1498 | `	if( pObj == 0 ){` |
|  ! 0 | 1499 | `		return 0;` |
|    - | 1500 | `	}` |
|    - | 1501 | `	/* VM-allocated rather than context-allocated: the foreach ITERATOR builds` |
|    - | 1502 | `	 * objects through here too, and its vtable has no call context. */` |
|   77 | 1503 | `	pRow = ph7_new_array(pVm);` |
|   77 | 1504 | `	if( pRow == 0 ){` |
|  ! 0 | 1505 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 1506 | `		return 0;` |
|    - | 1507 | `	}` |
|   77 | 1508 | `	if( !PdoStmtRowFrom(pVm,pSt,PDO_FETCH_ASSOC,pRow,iFirstCol) ){` |
|  ! 0 | 1509 | `		ph7_release_value(pVm,pRow);` |
|  ! 0 | 1510 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 1511 | `		return 0;` |
|    - | 1512 | `	}` |
|   77 | 1513 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   77 | 1514 | `	if( bPropsLate && pCons ){` |
|    5 | 1515 | `		PdoCallCtor(pVm,pObj,pCons,pArgs);` |
|    2 | 1516 | `	}` |
|    - | 1517 | `	{` |
|   77 | 1518 | `		ph7_hashmap *pMap = (ph7_hashmap *)pRow->x.pOther;` |
|   77 | 1519 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|   77 | 1520 | `		sxu32 n,nCount = pMap->nEntry;` |
|  211 | 1521 | `		for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 1522 | `			ph7_value sKey;` |
|  135 | 1523 | `			ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|  135 | 1524 | `			int nKey = 0;` |
|    - | 1525 | `			const char *zKey;` |
|  135 | 1526 | `			PH7_MemObjInit(pVm,&sKey);` |
|  135 | 1527 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  135 | 1528 | `			zKey = ph7_value_to_string(&sKey,&nKey);` |
|  135 | 1529 | `			if( pVal && zKey && nKey > 0 ){` |
|  135 | 1530 | `				PdoWriteOneProp(pVm,pObj,zKey,nKey,pVal);` |
|   67 | 1531 | `			}` |
|  135 | 1532 | `			PH7_MemObjRelease(&sKey);` |
|   68 | 1533 | `		}` |
|    - | 1534 | `	}` |
|   77 | 1535 | `	if( !bPropsLate && pCons ){` |
|   45 | 1536 | `		PdoCallCtor(pVm,pObj,pCons,pArgs);` |
|   22 | 1537 | `	}` |
|   77 | 1538 | `	ph7_release_value(pVm,pRow);` |
|   77 | 1539 | `	PH7_MemObjRelease(pResult);` |
|   77 | 1540 | `	pResult->x.pOther = pObj;` |
|   77 | 1541 | `	pResult->iFlags = MEMOBJ_OBJ;` |
|   77 | 1542 | `	rc = 1;` |
|   77 | 1543 | `	return rc;` |
|   39 | 1544 | `}` |
|    - | 1545 | `/*` |
|    - | 1546 | ` * The class a FETCH_CLASS or fetchObject() names.  The two verbs word the same` |
|    - | 1547 | ` * refusal differently -- fetchAll() names the ARGUMENT POSITION, fetchObject()` |
|    - | 1548 | ` * names the class it was given -- so the caller supplies the sentence.` |
|    - | 1549 | ` */` |
|   58 | 1550 | `static ph7_class * PdoResolveFetchClass(ph7_context *pCtx,ph7_value *pName,int bObjectVerb,` |
|    - | 1551 | `	sxi32 *pRc)` |
|    1 | 1552 | `{` |
|    - | 1553 | `	ph7_class *pClass;` |
|    - | 1554 | `	const char *zName;` |
|   59 | 1555 | `	int nName = 0;` |
|   59 | 1556 | `	*pRc = PH7_OK;` |
|   59 | 1557 | `	if( pName == 0 \|\| (pName->iFlags & MEMOBJ_NULL) ){` |
|    7 | 1558 | `		return PH7_VmExtractClass(pCtx->pVm,"stdClass",sizeof("stdClass")-1,FALSE,0);` |
|    - | 1559 | `	}` |
|   53 | 1560 | `	zName = ph7_value_to_string(pName,&nName);` |
|    - | 1561 | `	/* EXISTENCE, not instantiability: php takes the name of an abstract class` |
|    - | 1562 | `	 * or an interface here and refuses it where the object would be BUILT,` |
|    - | 1563 | ``	 * with the ordinary `Cannot instantiate ...` Error. */`` |
|   53 | 1564 | `	pClass = (zName && nName > 0)` |
|   78 | 1565 | `		? PH7_VmExtractClass(pCtx->pVm,zName,(sxu32)nName,FALSE,0) : 0;` |
|   53 | 1566 | `	if( pClass == 0 ){` |
|    5 | 1567 | `		if( bObjectVerb ){` |
|    4 | 1568 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1569 | `				"PDOStatement::fetchObject(): Argument #1 ($class) must be a valid "` |
|    1 | 1570 | `				"class name, %.*s given",nName,zName ? zName : "");` |
|    2 | 1571 | `		}else{` |
|    3 | 1572 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1573 | `				"PDOStatement::fetchAll(): Argument #2 must be a valid class");` |
|    - | 1574 | `		}` |
|    2 | 1575 | `	}` |
|   53 | 1576 | `	return pClass;` |
|   30 | 1577 | `}` |
|    - | 1578 | `/*` |
|    - | 1579 | ` * The class a FETCH_CLASSTYPE row names in its first column. php takes what it` |
|    - | 1580 | ` * finds there and falls back to stdClass for anything it cannot use -- a name` |
|    - | 1581 | ` * no class carries, a null, a number -- rather than refusing the row.` |
|    - | 1582 | ` */` |
|   20 | 1583 | `static ph7_class * PdoIterClassOf(ph7_vm *pVm,ph7_value *pName)` |
|    1 | 1584 | `{` |
|   21 | 1585 | `	ph7_class *pClass = 0;` |
|   21 | 1586 | `	if( pName && (pName->iFlags & MEMOBJ_NULL) == 0 ){` |
|   19 | 1587 | `		int nName = 0;` |
|   19 | 1588 | `		const char *zName = ph7_value_to_string(pName,&nName);` |
|   19 | 1589 | `		if( zName && nName > 0 ){` |
|   19 | 1590 | `			pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);` |
|    9 | 1591 | `		}` |
|    9 | 1592 | `	}` |
|   11 | 1593 | `	return pClass ? pClass` |
|   14 | 1594 | `		: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,FALSE,0);` |
|    1 | 1595 | `}` |
|   16 | 1596 | `static ph7_class * PdoClassTypeClass(ph7_context *pCtx,ph7_value *pName)` |
|    1 | 1597 | `{` |
|   17 | 1598 | `	return PdoIterClassOf(pCtx->pVm,pName);` |
|    1 | 1599 | `}` |
|    - | 1600 | `/*` |
|    - | 1601 | ` * PDOStatement::fetchObject(?string $class = "stdClass", array $ctorArgs = []): object\|false` |
|    - | 1602 | ` *` |
|    - | 1603 | ` * FETCH_CLASS for exactly one row, with its own refusal wording.` |
|    - | 1604 | ` */` |
|   12 | 1605 | `static int vm_builtin_PDOStatement_fetchObject(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1606 | `{` |
|   13 | 1607 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 1608 | `	ph7_class *pClass;` |
|    - | 1609 | `	ph7_value sRes;` |
|    - | 1610 | `	sxi32 rc;` |
|   13 | 1611 | `	if( pSt == 0 ){` |
|  ! 0 | 1612 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 1613 | `	}` |
|   13 | 1614 | `	pClass = PdoResolveFetchClass(pCtx,nArg > 0 ? apArg[0] : 0,TRUE,&rc);` |
|   13 | 1615 | `	if( pClass == 0 ){` |
|    3 | 1616 | `		return rc;` |
|    - | 1617 | `	}` |
|   11 | 1618 | `	if( !PdoStmtHasRow(pSt) ){` |
|    3 | 1619 | `		PdoStmtOk(pSt);` |
|    3 | 1620 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1621 | `		return PH7_OK;` |
|    - | 1622 | `	}` |
|    9 | 1623 | `	if( PdoBoundColumnsBad(pSt) ){` |
|  ! 0 | 1624 | `		return PdoBoundColumnsRefuse(pCtx,pSt,0);` |
|    - | 1625 | `	}` |
|    9 | 1626 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    9 | 1627 | `	if( !PdoRowIntoObject(pCtx->pVm,pSt,pClass,nArg > 1 ? apArg[1] : 0,FALSE,0,&sRes) ){` |
|  ! 0 | 1628 | `		PH7_MemObjRelease(&sRes);` |
|  ! 0 | 1629 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1630 | `		return PH7_OK;` |
|    - | 1631 | `	}` |
|    9 | 1632 | `	ph7_result_value(pCtx,&sRes);` |
|    9 | 1633 | `	PH7_MemObjRelease(&sRes);` |
|    9 | 1634 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 1635 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 1636 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchObject");` |
|    - | 1637 | `	}` |
|    9 | 1638 | `	PdoStmtOk(pSt);` |
|    9 | 1639 | `	return PH7_OK;` |
|    7 | 1640 | `}` |
|    - | 1641 | `/*` |
|    - | 1642 | ` * One row as an OBJECT: FETCH_CLASS builds a new instance, FETCH_INTO fills` |
|    - | 1643 | ` * the one the script handed setFetchMode(). A fetch(FETCH_INTO) with no such` |
|    - | 1644 | ` * object is php's own "No fetch-into object specified." -- a PDOException with` |
|    - | 1645 | ` * no driver behind it.` |
|    - | 1646 | ` */` |
|   38 | 1647 | `static int PdoFetchObjectRow(ph7_context *pCtx,phl_pdo_stmt *pSt,int iMode,` |
|    - | 1648 | `	ph7_value *pClassName,ph7_value *pArgs,const char *zFn)` |
|    1 | 1649 | `{` |
|   39 | 1650 | `	int iBase = iMode & PDO_FETCH_MODE_MASK;` |
|   39 | 1651 | `	int bLate = (iMode & PDO_FETCH_PROPS_LATE) != 0;` |
|   39 | 1652 | `	int iFirst = 0;` |
|   39 | 1653 | `	ph7_class *pClass = 0;` |
|    - | 1654 | `	ph7_value sRes;` |
|    - | 1655 | `	sxi32 rc;` |
|   19 | 1656 | `	SXUNUSED(zFn);` |
|   39 | 1657 | `	if( !PdoStmtHasRow(pSt) ){` |
|  ! 0 | 1658 | `		PdoStmtOk(pSt);` |
|  ! 0 | 1659 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1660 | `		return PH7_OK;` |
|    - | 1661 | `	}` |
|   39 | 1662 | `	if( PdoBoundColumnsBad(pSt) ){` |
|  ! 0 | 1663 | `		return PdoBoundColumnsRefuse(pCtx,pSt,0);` |
|    - | 1664 | `	}` |
|   39 | 1665 | `	if( iBase == PDO_FETCH_INTO ){` |
|    - | 1666 | `		ph7_value *pRow;` |
|   13 | 1667 | `		if( pSt->pFetchInto == 0 ){` |
|    3 | 1668 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 1669 | `				"SQLSTATE[HY000]: General error: No fetch-into object specified.");` |
|    - | 1670 | `		}` |
|   11 | 1671 | `		pRow = ph7_context_new_array(pCtx);` |
|   11 | 1672 | `		if( pRow == 0 ){` |
|  ! 0 | 1673 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 1674 | `		}` |
|   11 | 1675 | `		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_ASSOC,pRow) ){` |
|  ! 0 | 1676 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 1677 | `			return PH7_OK;` |
|    - | 1678 | `		}` |
|   11 | 1679 | `		PdoWriteRowProps(pCtx->pVm,pSt->pFetchInto,pRow);` |
|   11 | 1680 | `		PH7_NativeResultObject(pCtx,pSt->pFetchInto);` |
|   11 | 1681 | `		pSt->pFetchInto->iRef++;   /* the result took one; the statement keeps its own */` |
|   11 | 1682 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 1683 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 1684 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 1685 | `		}` |
|   11 | 1686 | `		PdoStmtOk(pSt);` |
|   11 | 1687 | `		return PH7_OK;` |
|    - | 1688 | `	}` |
|    - | 1689 | `	/* FETCH_CLASS: the class comes from this call or from setFetchMode() */` |
|   27 | 1690 | `	if( pClassName ){` |
|  ! 0 | 1691 | `		pClass = PdoResolveFetchClass(pCtx,pClassName,FALSE,&rc);` |
|  ! 0 | 1692 | `		if( pClass == 0 ){` |
|  ! 0 | 1693 | `			return rc;` |
|  ! 0 | 1694 | `		}` |
|   27 | 1695 | `	}else if( iMode & PDO_FETCH_CLASSTYPE ){` |
|    - | 1696 | `		/* the FIRST column names the class, and leaves the row */` |
|    5 | 1697 | `		ph7_value *pHead = ph7_context_new_array(pCtx);` |
|    - | 1698 | `		ph7_value *pName;` |
|    5 | 1699 | `		if( pHead == 0 ){` |
|  ! 0 | 1700 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 1701 | `		}` |
|    5 | 1702 | `		if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pHead,0) ){` |
|  ! 0 | 1703 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 1704 | `			return PH7_OK;` |
|    - | 1705 | `		}` |
|    5 | 1706 | `		pSt->bRowPending = 1;   /* the cursor has not moved */` |
|    5 | 1707 | `		pName = PdoArrayAtInt(pCtx->pVm,pHead,0);` |
|    5 | 1708 | `		pClass = PdoClassTypeClass(pCtx,pName);` |
|    5 | 1709 | `		iFirst = 1;` |
|   25 | 1710 | `	}else if( pSt->zFetchClass ){` |
|    - | 1711 | `		ph7_value sName;` |
|    - | 1712 | `		SyString sStr;` |
|   21 | 1713 | `		SyStringInitFromBuf(&sStr,pSt->zFetchClass,pSt->nFetchClass);` |
|   21 | 1714 | `		PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|   21 | 1715 | `		pClass = PdoResolveFetchClass(pCtx,&sName,FALSE,&rc);` |
|   21 | 1716 | `		PH7_MemObjRelease(&sName);` |
|   21 | 1717 | `		if( pClass == 0 ){` |
|  ! 0 | 1718 | `			return rc;` |
|    - | 1719 | `		}` |
|   21 | 1720 | `		if( pArgs == 0 ){` |
|   21 | 1721 | `			pArgs = pSt->pFetchArgs;` |
|   10 | 1722 | `		}` |
|   11 | 1723 | `	}else{` |
|    - | 1724 | `		/* FETCH_CLASS with no class anywhere -- neither this call's nor a` |
|    - | 1725 | `		 * setFetchMode()'s -- is php's own layer refusal, not a stdClass row. */` |
|    3 | 1726 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 1727 | `			"SQLSTATE[HY000]: General error: No fetch class specified");` |
|    - | 1728 | `	}` |
|   25 | 1729 | `	if( pClass == 0 ){` |
|  ! 0 | 1730 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1731 | `	}` |
|   25 | 1732 | `	rc = PH7_VmCheckInstantiable(pCtx,pClass);` |
|   25 | 1733 | `	if( rc != PH7_OK ){` |
|    9 | 1734 | `		return rc;   /* an interface, a trait, an enum or an abstract class */` |
|    - | 1735 | `	}` |
|   17 | 1736 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|   17 | 1737 | `	if( !PdoRowIntoObject(pCtx->pVm,pSt,pClass,pArgs,bLate,iFirst,&sRes) ){` |
|  ! 0 | 1738 | `		PH7_MemObjRelease(&sRes);` |
|  ! 0 | 1739 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1740 | `		return PH7_OK;` |
|    - | 1741 | `	}` |
|   17 | 1742 | `	ph7_result_value(pCtx,&sRes);` |
|   17 | 1743 | `	PH7_MemObjRelease(&sRes);` |
|   17 | 1744 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 1745 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 1746 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 1747 | `	}` |
|   17 | 1748 | `	PdoStmtOk(pSt);` |
|   17 | 1749 | `	return PH7_OK;` |
|   20 | 1750 | `}` |
|    - | 1751 | `/*` |
|    - | 1752 | ` * PDOStatement::bindColumn(string\|int $column, mixed &$var, int $type = PDO::PARAM_STR, ...): bool` |
|    - | 1753 | ` *` |
|    - | 1754 | ` * Attach a variable to a column, to be written on every FETCH_BOUND fetch.` |
|    - | 1755 | ` * The column is 1-based like a parameter, and a NAME is resolved NOW against` |
|    - | 1756 | ` * the statement's columns -- a name that is not there warns immediately (in` |
|    - | 1757 | ` * every error mode, the way a layer refusal does) and is simply not bound,` |
|    - | 1758 | ` * while an out-of-range INDEX is accepted here and refused by the fetch.` |
|    - | 1759 | ` */` |
|  118 | 1760 | `static int vm_builtin_PDOStatement_bindColumn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1761 | `{` |
|  119 | 1762 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 1763 | `	phl_pdo_bind *pB;` |
|    - | 1764 | `	ph7_value *pKey;` |
|  119 | 1765 | `	int iPos = 0,iType;` |
|  119 | 1766 | `	if( pSt == 0 ){` |
|  ! 0 | 1767 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 1768 | `	}` |
|  119 | 1769 | `	pKey = nArg > 0 ? apArg[0] : 0;` |
|  119 | 1770 | `	iType = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : PDO_PARAM_STR;` |
|  126 | 1771 | `	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){` |
|   21 | 1772 | `		int nName = 0,iCol,nCol;` |
|   21 | 1773 | `		const char *zName = ph7_value_to_string(pKey,&nName);` |
|    - | 1774 | `		SyBlob sName;` |
|   21 | 1775 | `		iPos = 0;` |
|   21 | 1776 | `		nCol = PH7_PdoSqliteColumnCount(pSt);` |
|   21 | 1777 | `		SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|   35 | 1778 | `		for( iCol = 0 ; iCol < nCol ; ++iCol ){` |
|   29 | 1779 | `			PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sName);` |
|   28 | 1780 | `			if( (int)SyBlobLength(&sName) - 1 == nName` |
|   24 | 1781 | `			 && SyMemcmp(SyBlobData(&sName),zName,(sxu32)nName) == 0 ){` |
|   15 | 1782 | `				iPos = iCol + 1;` |
|   15 | 1783 | `				break;` |
|    - | 1784 | `			}` |
|    8 | 1785 | `		}` |
|   21 | 1786 | `		SyBlobRelease(&sName);` |
|   21 | 1787 | `		if( iPos == 0 ){` |
|    - | 1788 | `			/* php routes this one through the error mode like any layer refusal:` |
|    - | 1789 | `			 * a warning in silent and warning modes, a throw in exception mode --` |
|    - | 1790 | `			 * and the column is simply not bound either way. */` |
|    - | 1791 | `			SyBlob sMsg;` |
|    - | 1792 | `			sxi32 rcWarn;` |
|    7 | 1793 | `			ph7_result_bool(pCtx,1);` |
|    7 | 1794 | `			SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    7 | 1795 | `			SyBlobFormat(&sMsg,"Did not find column name '%.*s' in the defined "` |
|    3 | 1796 | `				"columns; it will not be bound",nName,zName ? zName : "");` |
|    7 | 1797 | `			SyBlobAppend(&sMsg,"",1);` |
|   10 | 1798 | `			rcWarn = PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::bindColumn","HY000",` |
|    6 | 1799 | `				(const char *)SyBlobData(&sMsg));` |
|    7 | 1800 | `			SyBlobRelease(&sMsg);` |
|    7 | 1801 | `			return rcWarn;` |
|    - | 1802 | `		}` |
|    8 | 1803 | `	}else{` |
|   99 | 1804 | `		ph7_int64 iWant = pKey ? ph7_value_to_int64(pKey) : 0;` |
|   99 | 1805 | `		if( iWant < 1 ){` |
|    5 | 1806 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1807 | `				"PDOStatement::bindColumn(): Argument #1 ($column) must be greater "` |
|    - | 1808 | `				"than or equal to 1");` |
|    - | 1809 | `		}` |
|   95 | 1810 | `		iPos = (int)iWant;` |
|    - | 1811 | `	}` |
|    - | 1812 | `	/* php REPLACES a binding rather than stacking one, and which it considers` |
|    - | 1813 | `	 * the same is not symmetric: a binding made by NAME takes over whatever` |
|    - | 1814 | `	 * already stands for that COLUMN, while one made by NUMBER only replaces` |
|    - | 1815 | `` 	 * another made by number. So `bindColumn(3,$x)` then `bindColumn('c2',$y)` `` |
|    - | 1816 | `	 * is one binding and $x is never written again, while the same pair the` |
|    - | 1817 | `	 * other way round is two and both are. */` |
|    - | 1818 | `	{` |
|  109 | 1819 | `		int nName = 0;` |
|  109 | 1820 | `		const char *zName = (pKey && (pKey->iFlags & MEMOBJ_STRING))` |
|  115 | 1821 | `			? ph7_value_to_string(pKey,&nName) : 0;` |
|  139 | 1822 | `		for( pB = pSt->pColBinds ; pB ; pB = pB->pNext ){` |
|   70 | 1823 | `			if( zName ? (pB->iPos == iPos` |
|    7 | 1824 | `			             \|\| (pB->zName && pB->nName == nName` |
|    2 | 1825 | `			                 && SyMemcmp(pB->zName,zName,(sxu32)nName) == 0))` |
|   30 | 1826 | `			          : (pB->zName == 0 && pB->iPos == iPos) ){` |
|    7 | 1827 | `				break;` |
|    - | 1828 | `			}` |
|   16 | 1829 | `		}` |
|  109 | 1830 | `		if( pB == 0 ){` |
|  103 | 1831 | `			pB = (phl_pdo_bind *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_pdo_bind));` |
|  103 | 1832 | `			if( pB == 0 ){` |
|  ! 0 | 1833 | `				return PH7_ContextMemoryError(pCtx);` |
|    - | 1834 | `			}` |
|  103 | 1835 | `			SyZero(pB,sizeof(phl_pdo_bind));` |
|  103 | 1836 | `			pB->pNext = pSt->pColBinds;` |
|  103 | 1837 | `			pSt->pColBinds = pB;` |
|   51 | 1838 | `		}` |
|    - | 1839 | `		/* The entry remembers HOW it was named, replaced entries included: that` |
|    - | 1840 | `		 * is what a later binding by NUMBER matches against (it takes over a` |
|    - | 1841 | `		 * nameless entry and leaves a named one standing). */` |
|  109 | 1842 | `		if( pB->zName ){` |
|    3 | 1843 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pB->zName);` |
|    3 | 1844 | `			pB->zName = 0;` |
|    3 | 1845 | `			pB->nName = 0;` |
|    1 | 1846 | `		}` |
|  109 | 1847 | `		if( zName && nName > 0 ){` |
|   15 | 1848 | `			pB->zName = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nName + 1);` |
|   15 | 1849 | `			if( pB->zName == 0 ){` |
|  ! 0 | 1850 | `				return PH7_ContextMemoryError(pCtx);` |
|    - | 1851 | `			}` |
|   15 | 1852 | `			SyMemcpy(zName,pB->zName,(sxu32)nName);` |
|   15 | 1853 | `			pB->zName[nName] = 0;` |
|   15 | 1854 | `			pB->nName = nName;` |
|    7 | 1855 | `		}` |
|    - | 1856 | `	}` |
|  109 | 1857 | `	pB->iPos = iPos;` |
|  109 | 1858 | `	pB->iType = iType;` |
|  109 | 1859 | `	pB->nSlot = (nArg > 1 && apArg[1]) ? apArg[1]->nIdx : SXU32_HIGH;` |
|  109 | 1860 | `	ph7_result_bool(pCtx,1);` |
|  109 | 1861 | `	return PH7_OK;` |
|   60 | 1862 | `}` |
|    - | 1863 | `/*` |
|    - | 1864 | ` * Write every bound column of the row at the cursor into the variables` |
|    - | 1865 | ` * bindColumn() named.  The value takes the bound TYPE, so an unqualified` |
|    - | 1866 | ` * binding hands back a string where the row itself would have held an int.` |
|    - | 1867 | ` */` |
|   96 | 1868 | `static int PdoWriteBoundColumns(ph7_vm *pVm,phl_pdo_stmt *pSt)` |
|    1 | 1869 | `{` |
|    - | 1870 | `	phl_pdo_bind *pB;` |
|   97 | 1871 | `	int nCol = PH7_PdoSqliteColumnCount(pSt);` |
|  229 | 1872 | `	for( pB = pSt->pColBinds ; pB ; pB = pB->pNext ){` |
|    - | 1873 | `		ph7_value *pSlot;` |
|    - | 1874 | `		ph7_value sVal;` |
|  133 | 1875 | `		if( pB->nSlot == SXU32_HIGH ){` |
|  ! 0 | 1876 | `			continue;` |
|    - | 1877 | `		}` |
|  133 | 1878 | `		pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pB->nSlot);` |
|  133 | 1879 | `		if( pSlot == 0 ){` |
|  ! 0 | 1880 | `			continue;` |
|    - | 1881 | `		}` |
|  133 | 1882 | `		if( pB->iPos < 1 \|\| pB->iPos > nCol ){` |
|    - | 1883 | `			/* A column the statement does not have writes a NULL into its` |
|    - | 1884 | `			 * variable and only THEN refuses (the refusal is the caller's). */` |
|   21 | 1885 | `			PH7_MemObjInit(pVm,&sVal);` |
|   21 | 1886 | `			PH7_MemObjStore(&sVal,pSlot);` |
|   21 | 1887 | `			PH7_MemObjRelease(&sVal);` |
|   21 | 1888 | `			continue;` |
|    - | 1889 | `		}` |
|  113 | 1890 | `		PH7_MemObjInit(pVm,&sVal);` |
|  113 | 1891 | `		PH7_PdoSqliteColumnValue(pSt,pB->iPos - 1,&sVal);` |
|  113 | 1892 | `		PdoApplyValueMods(pSt->pConn,&sVal);` |
|    - | 1893 | `		/* php switches on the type it was GIVEN, flags and all: PARAM_NULL` |
|    - | 1894 | `		 * writes a null whatever the column holds, INT/STR/BOOL convert, and` |
|    - | 1895 | `		 * everything else -- PARAM_LOB, PARAM_STMT, a number no constant names,` |
|    - | 1896 | `		 * or any of these with PARAM_INPUT_OUTPUT ored on -- writes the driver's` |
|    - | 1897 | `		 * own value untouched. A column holding NULL stays null throughout. */` |
|  113 | 1898 | `		if( pB->iType == PDO_PARAM_NULL ){` |
|    5 | 1899 | `			PH7_MemObjRelease(&sVal);` |
|    5 | 1900 | `			PH7_MemObjInit(pVm,&sVal);` |
|  111 | 1901 | `		}else if( (sVal.iFlags & MEMOBJ_NULL) == 0 ){` |
|   95 | 1902 | `			switch( pB->iType ){` |
|    5 | 1903 | `				case PDO_PARAM_INT:  PH7_MemObjToInteger(&sVal); break;` |
|    3 | 1904 | `				case PDO_PARAM_BOOL: PH7_MemObjToBool(&sVal); break;` |
|   79 | 1905 | `				case PDO_PARAM_STR:  PH7_MemObjToString(&sVal); break;` |
|    2 | 1906 | `				case PDO_PARAM_LOB:` |
|    - | 1907 | `					/* php wraps a STRING column in a memory stream here and` |
|    - | 1908 | `					 * leaves every other type as the driver typed it. */` |
|    5 | 1909 | `					if( sVal.iFlags & MEMOBJ_STRING ){` |
|    4 | 1910 | `						io_private *pLobDev = PdoLobStreamNew(pVm,` |
|    2 | 1911 | `							(const char *)SyBlobData(&sVal.sBlob),` |
|    2 | 1912 | `							(int)SyBlobLength(&sVal.sBlob));` |
|    3 | 1913 | `						if( pLobDev ){` |
|    3 | 1914 | `							PH7_MemObjRelease(&sVal);` |
|    3 | 1915 | `							sVal.x.pOther = pLobDev;` |
|    3 | 1916 | `							MemObjSetType(&sVal,MEMOBJ_RES);` |
|    - | 1917 | `							/* Written straight rather than through` |
|    - | 1918 | `							 * ph7_value_resource(), so this value takes the` |
|    - | 1919 | `							 * handle's first count -- see io_private.nValRef. */` |
|    3 | 1920 | `							if( PH7_StreamValueRef(pLobDev) ){` |
|    3 | 1921 | `								sVal.iFlags \|= MEMOBJ_STREAMRES;` |
|    1 | 1922 | `							}` |
|    1 | 1923 | `						}` |
|    1 | 1924 | `					}` |
|    4 | 1925 | `					break;` |
|    6 | 1926 | `				default:             break;   /* the value as the driver typed it */` |
|    - | 1927 | `			}` |
|   47 | 1928 | `		}` |
|  113 | 1929 | `		PH7_MemObjStore(&sVal,pSlot);` |
|  113 | 1930 | `		PH7_MemObjRelease(&sVal);` |
|   57 | 1931 | `	}` |
|   97 | 1932 | `	return 1;` |
|    1 | 1933 | `}` |
|    - | 1934 | `/*` |
|    - | 1935 | ` * Does every binding name a column this statement HAS? The width is the` |
|    - | 1936 | ` * statement's and does not change while it is walked, so this is asked ONCE by` |
|    - | 1937 | ` * the verb rather than per row -- which is also what keeps php's` |
|    - | 1938 | `` * `Invalid column index` out of the middle of fetchAll()'s loop, where a raise`` |
|    - | 1939 | ` * has no frame to leave.` |
|    - | 1940 | ` */` |
|  178 | 1941 | `static int PdoBoundColumnsInRange(phl_pdo_stmt *pSt)` |
|    1 | 1942 | `{` |
|    - | 1943 | `	phl_pdo_bind *pB;` |
|  179 | 1944 | `	int nCol = PH7_PdoSqliteColumnCount(pSt);` |
|  389 | 1945 | `	for( pB = pSt->pColBinds ; pB ; pB = pB->pNext ){` |
|  227 | 1946 | `		if( pB->iPos < 1 \|\| pB->iPos > nCol ){` |
|   17 | 1947 | `			return 0;` |
|    - | 1948 | `		}` |
|  106 | 1949 | `	}` |
|  163 | 1950 | `	return 1;` |
|   90 | 1951 | `}` |
|    - | 1952 | `/*` |
|    - | 1953 | ` * The bound columns of the row a verb is about to hand out. php writes them on` |
|    - | 1954 | ` * EVERY fetch, whatever the mode -- FETCH_BOUND is only the mode that answers` |
|    - | 1955 | `` * `true` INSTEAD of a row, not the one that does the writing -- so this runs`` |
|    - | 1956 | ` * wherever a row is read. A binding OUT of range is not its business: the verb` |
|    - | 1957 | ` * screens for one first and refuses through PdoBoundColumnsRefuse.` |
|    - | 1958 | ` */` |
|  604 | 1959 | `static void PdoBoundColumnsForRow(ph7_vm *pVm,phl_pdo_stmt *pSt)` |
|    2 | 1960 | `{` |
|  606 | 1961 | `	if( pSt->pColBinds == 0 \|\| !pSt->bRowPending \|\| !PdoBoundColumnsInRange(pSt) ){` |
|    - | 1962 | `		/* php looks at the bindings only when there is a ROW to write from, so a` |
|    - | 1963 | `		 * cursor with nothing left answers false rather than refusing; a binding` |
|    - | 1964 | `		 * OUT of range was screened by the verb before the row was read. */` |
|  530 | 1965 | `		return;` |
|    - | 1966 | `	}` |
|   77 | 1967 | `	PdoWriteBoundColumns(pVm,pSt);` |
|  304 | 1968 | `}` |
|    - | 1969 | `/* Has the verb about to read a row a binding it cannot honour? */` |
|  696 | 1970 | `static int PdoBoundColumnsBad(phl_pdo_stmt *pSt)` |
|    2 | 1971 | `{` |
|  698 | 1972 | `	return pSt->pColBinds != 0 && pSt->bRowPending && !PdoBoundColumnsInRange(pSt);` |
|    2 | 1973 | `}` |
|    - | 1974 | `/*` |
|    - | 1975 | ` * php's refusal for a binding naming a column the statement does not have. It` |
|    - | 1976 | ` * reads the row and moves the cursor ON before the refusal surfaces, so three` |
|    - | 1977 | ` * fetches over three rows refuse one by one and the fourth answers false --` |
|    - | 1978 | ` * and fetchAll(), which would have walked the whole set, exhausts it and` |
|    - | 1979 | ` * refuses once.` |
|    - | 1980 | ` */` |
|   16 | 1981 | `static sxi32 PdoBoundColumnsRefuse(ph7_context *pCtx,phl_pdo_stmt *pSt,int bWholeSet)` |
|    1 | 1982 | `{` |
|    - | 1983 | `	/* The bindings it CAN honour are written all the same -- the one it cannot` |
|    - | 1984 | `	 * reach does not stop the rest -- and for the whole-set verb they are` |
|    - | 1985 | `	 * written from EVERY row it walks past, so the caller is left holding the` |
|    - | 1986 | `	 * last row's values exactly as a successful fetchAll() would leave them. */` |
|    8 | 1987 | `	do{` |
|   21 | 1988 | `		PdoWriteBoundColumns(pCtx->pVm,pSt);` |
|   21 | 1989 | `		pSt->bRowPending = 0;` |
|   21 | 1990 | `		PdoStmtStep(pSt);` |
|   21 | 1991 | `	}while( bWholeSet && pSt->bRowPending );` |
|   17 | 1992 | `	PdoStmtOk(pSt);` |
|   17 | 1993 | `	return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    1 | 1994 | `}` |
|    - | 1995 | `/*` |
|    - | 1996 | ` * May this statement be WALKED at all? php's iterator refusals are the verbs'` |
|    - | 1997 | ` * own, and a foreach reaches them through the InternalIterator GUARD -- which` |
|    - | 1998 | ` * is where a call CONTEXT exists, so the throw leaves the loop the way every` |
|    - | 1999 | ` * other one does (raising on the VM from inside the vtable let execution carry` |
|    - | 2000 | ` * on past the loop). Three of them: a binding naming a column the statement` |
|    - | 2001 | ` * does not have -- consuming the row first, exactly as a fetch does -- and the` |
|    - | 2002 | ` * two modes a connection's ATTR_DEFAULT_FETCH_MODE can leave a statement in` |
|    - | 2003 | ` * with nothing beside it to build from.` |
|    - | 2004 | ` */` |
|  416 | 2005 | `static int PdoStmtWalkRefusal(ph7_context *pCtx,phl_pdo_stmt *pSt)` |
|    1 | 2006 | `{` |
|    - | 2007 | `	int iBase;` |
|  417 | 2008 | `	if( pSt == 0 \|\| !pSt->bRowPending ){` |
|  199 | 2009 | `		return 0;   /* nothing to hand out, so nothing to refuse */` |
|    - | 2010 | `	}` |
|  219 | 2011 | `	if( PdoBoundColumnsBad(pSt) ){` |
|    5 | 2012 | `		PdoBoundColumnsRefuse(pCtx,pSt,0);` |
|    5 | 2013 | `		return 1;` |
|    - | 2014 | `	}` |
|  215 | 2015 | `	iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;` |
|  215 | 2016 | `	if( iBase == PDO_FETCH_INTO && pSt->pFetchInto == 0 ){` |
|    3 | 2017 | `		PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 2018 | `			"SQLSTATE[HY000]: General error: No fetch-into object specified.");` |
|    3 | 2019 | `		return 1;` |
|    - | 2020 | `	}` |
|  212 | 2021 | `	if( iBase == PDO_FETCH_CLASS && (pSt->iFetchMode & PDO_FETCH_CLASSTYPE) == 0` |
|   35 | 2022 | `	 && pSt->zFetchClass == 0 ){` |
|    5 | 2023 | `		PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 2024 | `			"SQLSTATE[HY000]: General error: No fetch class specified");` |
|    5 | 2025 | `		return 1;` |
|    - | 2026 | `	}` |
|  209 | 2027 | `	return 0;` |
|  209 | 2028 | `}` |
|  358 | 2029 | `static int PdoStmtIterGuard(ph7_context *pCtx,ph7_class_instance *pIt)` |
|    1 | 2030 | `{` |
|  538 | 2031 | `	return PdoStmtWalkRefusal(pCtx,PdoStmtOfInstance(` |
|  179 | 2032 | `		PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC)));` |
|    1 | 2033 | `}` |
|    - | 2034 | `/* ------------------------------------------------------------------------` |
|    - | 2035 | ` * The blob STREAM: what Pdo\Sqlite::openBlob() answers` |
|    - | 2036 | ` * ------------------------------------------------------------------------ */` |
|    - | 2037 | `/*` |
|    - | 2038 | ` * php answers openBlob() with a php STREAM over one column of one row --` |
|    - | 2039 | `` * `stream_get_meta_data()` names its type `PDOSQLite` -- so the handle behind`` |
|    - | 2040 | ` * it is a device of its own here, the shape vfs_stream.c already carries for` |
|    - | 2041 | ` * php://, data:// and the userland wrappers. It reads and writes through` |
|    - | 2042 | ` * sqlite3_blob_read/_write at an offset it keeps itself, and its LENGTH is` |
|    - | 2043 | ` * fixed: a blob handle addresses the bytes that are already there, so a write` |
|    - | 2044 | `` * past the end is short and `ftruncate()` has nothing to do.`` |
|    - | 2045 | ` */` |
|   16 | 2046 | `static ph7_int64 PdoBlobStream_Read(void *pHandle,void *pBuf,ph7_int64 nDatatoRead)` |
|    1 | 2047 | `{` |
|   17 | 2048 | `	phl_pdo_blob *pBl = (phl_pdo_blob *)pHandle;` |
|   17 | 2049 | `	int nRead = PH7_PdoSqliteBlobIo(pBl,pBuf,(int)nDatatoRead,0);` |
|   17 | 2050 | `	return nRead < 0 ? -1 : (ph7_int64)nRead;` |
|    1 | 2051 | `}` |
|    6 | 2052 | `static ph7_int64 PdoBlobStream_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    1 | 2053 | `{` |
|    7 | 2054 | `	phl_pdo_blob *pBl = (phl_pdo_blob *)pHandle;` |
|    - | 2055 | `	int nDone;` |
|    7 | 2056 | `	if( !pBl->bWrite ){` |
|    - | 2057 | `		/* php's own sentence, from the device rather than from fwrite(): a` |
|    - | 2058 | `		 * handle opened READONLY refuses and the write answers false. */` |
|    3 | 2059 | `		PH7_VmThrowError(pBl->pConn->pVm,0,PH7_CTX_WARNING,` |
|    - | 2060 | `			"fwrite(): Can't write to blob stream: is open as read only");` |
|    3 | 2061 | `		return -1;` |
|    - | 2062 | `	}` |
|    5 | 2063 | `	if( nWrite > 0 && pBl->iOfft + nWrite > pBl->nSize ){` |
|    - | 2064 | `		/* A blob handle addresses the bytes that are already there: php refuses` |
|    - | 2065 | `		 * a write that would need more of them rather than writing what fits. */` |
|    3 | 2066 | `		PH7_VmThrowError(pBl->pConn->pVm,0,PH7_CTX_WARNING,` |
|    - | 2067 | `			"fwrite(): It is not possible to increase the size of a BLOB");` |
|    3 | 2068 | `		return -1;` |
|    - | 2069 | `	}` |
|    3 | 2070 | `	nDone = PH7_PdoSqliteBlobIo(pBl,(void *)pBuf,(int)nWrite,1);` |
|    3 | 2071 | `	return nDone < 0 ? -1 : (ph7_int64)nDone;` |
|    4 | 2072 | `}` |
|   12 | 2073 | `static int PdoBlobStream_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|    1 | 2074 | `{` |
|   13 | 2075 | `	phl_pdo_blob *pBl = (phl_pdo_blob *)pHandle;` |
|   13 | 2076 | `	ph7_int64 iNew = iOfft;` |
|   13 | 2077 | `	if( whence == 1 ){          /* SEEK_CUR */` |
|  ! 0 | 2078 | `		iNew = pBl->iOfft + iOfft;` |
|   13 | 2079 | `	}else if( whence == 2 ){    /* SEEK_END */` |
|    3 | 2080 | `		iNew = pBl->nSize + iOfft;` |
|    1 | 2081 | `	}` |
|   13 | 2082 | `	if( iNew < 0 \|\| iNew > pBl->nSize ){` |
|    - | 2083 | `		/* A blob handle addresses bytes that already exist, so there is nowhere` |
|    - | 2084 | `		 * past the end to seek TO. php's failed seek leaves the position` |
|    - | 2085 | `		 * UNKNOWN -- ftell() answers false and a read answers nothing until a` |
|    - | 2086 | `		 * seek succeeds again -- which is what the flag carries. */` |
|    5 | 2087 | `		pBl->bBadPos = 1;` |
|    5 | 2088 | `		return -1;` |
|    - | 2089 | `	}` |
|    9 | 2090 | `	pBl->bBadPos = 0;` |
|    9 | 2091 | `	pBl->iOfft = iNew;` |
|    9 | 2092 | `	return PH7_OK;` |
|    7 | 2093 | `}` |
|   16 | 2094 | `static ph7_int64 PdoBlobStream_Tell(void *pHandle)` |
|    1 | 2095 | `{` |
|   17 | 2096 | `	phl_pdo_blob *pBl = (phl_pdo_blob *)pHandle;` |
|   17 | 2097 | `	return pBl->bBadPos ? -1 : pBl->iOfft;` |
|    1 | 2098 | `}` |
|    2 | 2099 | `static int PdoBlobStream_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 2100 | `{` |
|    3 | 2101 | `	phl_pdo_blob *pBl = (phl_pdo_blob *)pHandle;` |
|    3 | 2102 | `	ph7_value_int64(pWorker,pBl->nSize);` |
|    3 | 2103 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker);` |
|    3 | 2104 | `	return PH7_OK;` |
|    1 | 2105 | `}` |
|    6 | 2106 | `static void PdoBlobStream_Close(void *pHandle)` |
|    1 | 2107 | `{` |
|    7 | 2108 | `	PH7_PdoSqliteBlobClose((phl_pdo_blob *)pHandle);` |
|    7 | 2109 | `}` |
|    - | 2110 | `static const ph7_io_stream sPdoBlobStream = {` |
|    - | 2111 | `	"PDOSQLite",                /* what stream_get_meta_data() reports */` |
|    - | 2112 | `	PH7_IO_STREAM_VERSION,` |
|    - | 2113 | `	0,                          /* xOpen: openBlob() builds the handle itself */` |
|    - | 2114 | `	0,                          /* xOpenDir */` |
|    - | 2115 | `	PdoBlobStream_Close,` |
|    - | 2116 | `	0,                          /* xCloseDir */` |
|    - | 2117 | `	PdoBlobStream_Read,` |
|    - | 2118 | `	0,                          /* xReadDir */` |
|    - | 2119 | `	PdoBlobStream_Write,` |
|    - | 2120 | `	PdoBlobStream_Seek,` |
|    - | 2121 | `	0,                          /* xLock */` |
|    - | 2122 | `	0,                          /* xRewindDir */` |
|    - | 2123 | `	PdoBlobStream_Tell,` |
|    - | 2124 | `	0,                          /* xTrunc: a blob is the length it was created with */` |
|    - | 2125 | `	0,                          /* xSync */` |
|    - | 2126 | `	PdoBlobStream_Stat` |
|    - | 2127 | `};` |
|    - | 2128 | `/*` |
|    - | 2129 | ` * The other stream this driver hands out: what a PARAM_LOB bound column reads` |
|    - | 2130 | ` * as. php converts a STRING column into a php memory stream there --` |
|    - | 2131 | `` * `stream_get_meta_data()` calls it `MEMORY`, read-only and seekable, with the`` |
|    - | 2132 | ` * value's own length behind it -- so the bytes are copied once when the row is` |
|    - | 2133 | ` * written and the handle owns them. Nothing else is a stream: an int, a float` |
|    - | 2134 | ` * and a null bound as LOB stay what the driver typed them.` |
|    - | 2135 | ` */` |
|    - | 2136 | `typedef struct phl_pdo_lob phl_pdo_lob;` |
|    - | 2137 | `struct phl_pdo_lob {` |
|    - | 2138 | `	ph7_vm *pVm;` |
|    - | 2139 | `	SyBlob sData;` |
|    - | 2140 | `	ph7_int64 iOfft;` |
|    - | 2141 | `};` |
|    6 | 2142 | `static ph7_int64 PdoLobStream_Read(void *pHandle,void *pBuf,ph7_int64 nDatatoRead)` |
|    1 | 2143 | `{` |
|    7 | 2144 | `	phl_pdo_lob *pLob = (phl_pdo_lob *)pHandle;` |
|    7 | 2145 | `	ph7_int64 nLeft = (ph7_int64)SyBlobLength(&pLob->sData) - pLob->iOfft;` |
|    7 | 2146 | `	if( nLeft < 1 \|\| nDatatoRead < 1 ){` |
|    3 | 2147 | `		return 0;` |
|    - | 2148 | `	}` |
|    5 | 2149 | `	if( nDatatoRead > nLeft ){` |
|    3 | 2150 | `		nDatatoRead = nLeft;` |
|    1 | 2151 | `	}` |
|    5 | 2152 | `	SyMemcpy((const char *)SyBlobData(&pLob->sData) + pLob->iOfft,pBuf,(sxu32)nDatatoRead);` |
|    5 | 2153 | `	pLob->iOfft += nDatatoRead;` |
|    5 | 2154 | `	return nDatatoRead;` |
|    4 | 2155 | `}` |
|    2 | 2156 | `static ph7_int64 PdoLobStream_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    1 | 2157 | `{` |
|    1 | 2158 | `	SXUNUSED(pHandle);` |
|    1 | 2159 | `	SXUNUSED(pBuf);` |
|    1 | 2160 | `	SXUNUSED(nWrite);` |
|    3 | 2161 | `	return -1;   /* php's is read-only: fwrite() answers false and says nothing */` |
|    1 | 2162 | `}` |
|    2 | 2163 | `static int PdoLobStream_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|    1 | 2164 | `{` |
|    3 | 2165 | `	phl_pdo_lob *pLob = (phl_pdo_lob *)pHandle;` |
|    3 | 2166 | `	ph7_int64 iNew = iOfft;` |
|    3 | 2167 | `	if( whence == 1 ){` |
|  ! 0 | 2168 | `		iNew = pLob->iOfft + iOfft;` |
|    3 | 2169 | `	}else if( whence == 2 ){` |
|  ! 0 | 2170 | `		iNew = (ph7_int64)SyBlobLength(&pLob->sData) + iOfft;` |
|  ! 0 | 2171 | `	}` |
|    3 | 2172 | `	if( iNew < 0 ){` |
|  ! 0 | 2173 | `		return -1;` |
|    - | 2174 | `	}` |
|    3 | 2175 | `	pLob->iOfft = iNew;` |
|    3 | 2176 | `	return PH7_OK;` |
|    2 | 2177 | `}` |
|    4 | 2178 | `static ph7_int64 PdoLobStream_Tell(void *pHandle)` |
|    1 | 2179 | `{` |
|    5 | 2180 | `	return ((phl_pdo_lob *)pHandle)->iOfft;` |
|    1 | 2181 | `}` |
|    2 | 2182 | `static int PdoLobStream_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 2183 | `{` |
|    3 | 2184 | `	phl_pdo_lob *pLob = (phl_pdo_lob *)pHandle;` |
|    3 | 2185 | `	ph7_value_int64(pWorker,(ph7_int64)SyBlobLength(&pLob->sData));` |
|    3 | 2186 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker);` |
|    3 | 2187 | `	return PH7_OK;` |
|    1 | 2188 | `}` |
|    2 | 2189 | `static void PdoLobStream_Close(void *pHandle)` |
|    1 | 2190 | `{` |
|    3 | 2191 | `	phl_pdo_lob *pLob = (phl_pdo_lob *)pHandle;` |
|    3 | 2192 | `	ph7_vm *pVm = pLob->pVm;` |
|    3 | 2193 | `	SyBlobRelease(&pLob->sData);` |
|    3 | 2194 | `	SyMemBackendFree(&pVm->sAllocator,pLob);` |
|    3 | 2195 | `}` |
|    - | 2196 | `static const ph7_io_stream sPdoLobStream = {` |
|    - | 2197 | `	"MEMORY",                   /* what php's own conversion reports */` |
|    - | 2198 | `	PH7_IO_STREAM_VERSION,` |
|    - | 2199 | `	0, 0,` |
|    - | 2200 | `	PdoLobStream_Close,` |
|    - | 2201 | `	0,` |
|    - | 2202 | `	PdoLobStream_Read,` |
|    - | 2203 | `	0,` |
|    - | 2204 | `	PdoLobStream_Write,` |
|    - | 2205 | `	PdoLobStream_Seek,` |
|    - | 2206 | `	0, 0,` |
|    - | 2207 | `	PdoLobStream_Tell,` |
|    - | 2208 | `	0, 0,` |
|    - | 2209 | `	PdoLobStream_Stat` |
|    - | 2210 | `};` |
|    - | 2211 | `/*` |
|    - | 2212 | ` * Wrap one value's bytes in that stream. Answers 0 when the memory could not` |
|    - | 2213 | ` * be had, which leaves the caller's value as the driver typed it.` |
|    - | 2214 | ` */` |
|    2 | 2215 | `static io_private * PdoLobStreamNew(ph7_vm *pVm,const char *zData,int nData)` |
|    1 | 2216 | `{` |
|    - | 2217 | `	phl_pdo_lob *pLob;` |
|    - | 2218 | `	io_private *pDev;` |
|    3 | 2219 | `	pLob = (phl_pdo_lob *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo_lob));` |
|    3 | 2220 | `	if( pLob == 0 ){` |
|  ! 0 | 2221 | `		return 0;` |
|    - | 2222 | `	}` |
|    3 | 2223 | `	SyZero(pLob,sizeof(phl_pdo_lob));` |
|    3 | 2224 | `	pLob->pVm = pVm;` |
|    3 | 2225 | `	SyBlobInit(&pLob->sData,&pVm->sAllocator);` |
|    3 | 2226 | `	if( nData > 0 ){` |
|    3 | 2227 | `		SyBlobAppend(&pLob->sData,zData,(sxu32)nData);` |
|    1 | 2228 | `	}` |
|    - | 2229 | `	/* The io_private goes back through ph7_context_free_chunk, which is a plain` |
|    - | 2230 | `	 * VM-allocator free for a chunk nothing registered -- so a handle built` |
|    - | 2231 | `	 * where there is no call context (the foreach iterator's row) is released` |
|    - | 2232 | `	 * exactly like one built where there is. */` |
|    3 | 2233 | `	pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    3 | 2234 | `	if( pDev == 0 ){` |
|  ! 0 | 2235 | `		PdoLobStream_Close(pLob);` |
|  ! 0 | 2236 | `		return 0;` |
|    - | 2237 | `	}` |
|    3 | 2238 | `	SyZero(pDev,sizeof(io_private));` |
|    3 | 2239 | `	InitIOPrivate(pVm,&sPdoLobStream,pDev);` |
|    3 | 2240 | `	pDev->pHandle = pLob;` |
|    3 | 2241 | `	SetIOPrivateOpenedAs(pDev,"",0,"rb",2);` |
|    3 | 2242 | `	return pDev;` |
|    2 | 2243 | `}` |
|    - | 2244 | `/*` |
|    - | 2245 | ` * Pdo\Sqlite::openBlob(string $table, string $column, int $rowid,` |
|    - | 2246 | ` *                       ?string $dbname = "main", int $flags = OPEN_READONLY)` |
|    - | 2247 | ` *` |
|    - | 2248 | ` * php reports every failure as a WARNING carrying sqlite's own message and` |
|    - | 2249 | ` * answers false -- the error mode has no say, because this is a stream opener` |
|    - | 2250 | `` * and not a statement. The `$dbname` is passed to sqlite as it stands, which is`` |
|    - | 2251 | `` * why a null one produces `no such table: .b`, and only OPEN_READWRITE among`` |
|    - | 2252 | ` * the flags means anything: sqlite's blob handle is readable either way.` |
|    - | 2253 | ` */` |
|   16 | 2254 | `PH7_PRIVATE int PH7_PdoSqliteOpenBlobMethod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2255 | `{` |
|   17 | 2256 | `	phl_pdo *pConn = PH7_PdoConnOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2257 | `	phl_pdo_blob *pBl;` |
|    - | 2258 | `	io_private *pDev;` |
|   17 | 2259 | `	const char *zTable,*zColumn,*zDb = "main";` |
|   17 | 2260 | `	int nTable = 0,nColumn = 0,nDb = (int)sizeof("main")-1;` |
|    - | 2261 | `	ph7_int64 iRow;` |
|    - | 2262 | `	int iFlags;` |
|   17 | 2263 | `	if( pConn == 0 ){` |
|  ! 0 | 2264 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 2265 | `	}` |
|   17 | 2266 | `	zTable  = nArg > 0 ? ph7_value_to_string(apArg[0],&nTable) : "";` |
|   17 | 2267 | `	zColumn = nArg > 1 ? ph7_value_to_string(apArg[1],&nColumn) : "";` |
|   17 | 2268 | `	iRow    = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|   17 | 2269 | `	if( nArg > 3 && (apArg[3]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    5 | 2270 | `		zDb = ph7_value_to_string(apArg[3],&nDb);` |
|   15 | 2271 | `	}else if( nArg > 3 ){` |
|  ! 0 | 2272 | `		zDb = "";           /* php hands sqlite the empty name a null becomes */` |
|  ! 0 | 2273 | `		nDb = 0;` |
|  ! 0 | 2274 | `	}` |
|   17 | 2275 | `	iFlags = nArg > 4 ? (int)ph7_value_to_int64(apArg[4]) : SQLITE_OPEN_READONLY;` |
|    8 | 2276 | `	SXUNUSED(nDb);` |
|    8 | 2277 | `	SXUNUSED(nTable);` |
|    8 | 2278 | `	SXUNUSED(nColumn);` |
|   25 | 2279 | `	pBl = PH7_PdoSqliteBlobOpen(pConn,zDb,zTable,zColumn,iRow,` |
|   16 | 2280 | `		(iFlags & SQLITE_OPEN_READWRITE) != 0);` |
|   17 | 2281 | `	if( pBl == 0 ){` |
|   11 | 2282 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"Unable to open blob: %s",` |
|   10 | 2283 | `			pConn->zDrvMsg ? pConn->zDrvMsg : "unknown error");` |
|   11 | 2284 | `		ph7_result_bool(pCtx,0);` |
|   11 | 2285 | `		return PH7_OK;` |
|    - | 2286 | `	}` |
|    7 | 2287 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    7 | 2288 | `	if( pDev == 0 ){` |
|  ! 0 | 2289 | `		PH7_PdoSqliteBlobClose(pBl);` |
|  ! 0 | 2290 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2291 | `	}` |
|    7 | 2292 | `	InitIOPrivate(pCtx->pVm,&sPdoBlobStream,pDev);` |
|    7 | 2293 | `	pDev->pHandle = pBl;` |
|    - | 2294 | `	/* php's meta reports no uri for this stream and the mode it opened with. */` |
|    7 | 2295 | `	SetIOPrivateOpenedAs(pDev,"",0,pBl->bWrite ? "r+b" : "rb",pBl->bWrite ? 3 : 2);` |
|    7 | 2296 | `	ph7_result_resource(pCtx,pDev);` |
|    7 | 2297 | `	return PH7_OK;` |
|    9 | 2298 | `}` |
|    - | 2299 | `/* ------------------------------------------------------------------------` |
|    - | 2300 | ` * PDORow: what PDO::FETCH_LAZY answers` |
|    - | 2301 | ` * ------------------------------------------------------------------------ */` |
|    - | 2302 | `/*` |
|    - | 2303 | ` * php's PDORow is a fully VIRTUAL object over a statement's CURRENT row: it` |
|    - | 2304 | `` * declares one property (`queryString`) and holds NONE, every column is read`` |
|    - | 2305 | ` * through its property and dimension handlers, and every write is refused.` |
|    - | 2306 | `` * That split is what makes `get_object_vars()` empty beside a `$row->id` that`` |
|    - | 2307 | `` * works, `var_dump()` show the columns anyway (its get_debug_info handler) and`` |
|    - | 2308 | `` * `(array)`/`var_export()`/`json_encode()` show nothing at all.`` |
|    - | 2309 | ` *` |
|    - | 2310 | ` * One object per statement, handed back by every lazy fetch, so two fetches` |
|    - | 2311 | ` * answer the same object and the FIRST one moves on to the second row. It` |
|    - | 2312 | ` * RETAINS the statement object -- a row outliving the variable that fetched it` |
|    - | 2313 | ` * still reads (and still keeps the database open under it) -- and the statement` |
|    - | 2314 | ` * points back at it without a reference, which the row's own release clears.` |
|    - | 2315 | ` */` |
|    - | 2316 | `/* The row's hidden slots: the statement it reads, and the object that owns it. */` |
|    - | 2317 | `#define PDOROW_RES  "__res"` |
|    - | 2318 | `#define PDOROW_STMT "__stmt"` |
|    - | 2319 |  |
|  274 | 2320 | `static phl_pdo_stmt * PdoRowStmt(ph7_class_instance *pThis)` |
|    1 | 2321 | `{` |
|    - | 2322 | `	SyString sAttr;` |
|    - | 2323 | `	ph7_value *pRes;` |
|  275 | 2324 | `	if( pThis == 0 ){` |
|  ! 0 | 2325 | `		return 0;` |
|    - | 2326 | `	}` |
|  275 | 2327 | `	SyStringInitFromBuf(&sAttr,PDOROW_RES,sizeof(PDOROW_RES)-1);` |
|  275 | 2328 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  275 | 2329 | `	if( pRes == 0 \|\| !ph7_value_is_resource(pRes) ){` |
|  ! 0 | 2330 | `		return 0;` |
|    - | 2331 | `	}` |
|  275 | 2332 | `	return (phl_pdo_stmt *)ph7_value_to_resource(pRes);` |
|  138 | 2333 | `}` |
|    - | 2334 | `/*` |
|    - | 2335 | ` * php reads a property or offset NAME as a column NUMBER when it is an integer` |
|    - | 2336 | `` * string -- `$row->{'0'}` and `$row['1']` are columns, not names -- and as a`` |
|    - | 2337 | ` * column NAME otherwise. The grammar is php's is_numeric_string answering` |
|    - | 2338 | ` * IS_LONG: whitespace, a sign, digits, whitespace, and a value an int64 holds` |
|    - | 2339 | ` * (an overflow reads as a double there, so it falls through to the name lookup` |
|    - | 2340 | ` * and misses).` |
|    - | 2341 | ` */` |
|  172 | 2342 | `static int PdoRowIntName(const char *z,int n,ph7_int64 *pOut)` |
|    1 | 2343 | `{` |
|  173 | 2344 | `	const char *zEnd = z + n;` |
|  173 | 2345 | `	int bNeg = 0, nDigit = 0;` |
|  173 | 2346 | `	ph7_int64 iVal = 0;` |
|  173 | 2347 | `	while( z < zEnd && SyisSpace(z[0]) ){ z++; }` |
|  173 | 2348 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|    3 | 2349 | `		bNeg = (z[0] == '-');` |
|    3 | 2350 | `		z++;` |
|    1 | 2351 | `	}` |
|  207 | 2352 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|   35 | 2353 | `		int iDigit = z[0] - '0';` |
|   35 | 2354 | `		if( iVal > (SXI64_HIGH - iDigit) / 10 ){` |
|  ! 0 | 2355 | `			return 0;` |
|    - | 2356 | `		}` |
|   35 | 2357 | `		iVal = iVal * 10 + iDigit;` |
|   35 | 2358 | `		z++;` |
|   35 | 2359 | `		nDigit++;` |
|    1 | 2360 | `	}` |
|  173 | 2361 | `	while( z < zEnd && SyisSpace(z[0]) ){ z++; }` |
|  173 | 2362 | `	if( nDigit == 0 \|\| z != zEnd ){` |
|  145 | 2363 | `		return 0;` |
|    - | 2364 | `	}` |
|   29 | 2365 | `	*pOut = bNeg ? -iVal : iVal;` |
|   29 | 2366 | `	return 1;` |
|   87 | 2367 | `}` |
|    - | 2368 | ``/* php's read handlers answer `queryString` from the STATEMENT before they look`` |
|    - | 2369 | ` * at any column, so a query selecting a column of that name cannot shadow it --` |
|    - | 2370 | ` * while has_property/has_dimension do not know the name at all. */` |
|  154 | 2371 | `static int PdoRowIsQueryString(const char *zName,int nName)` |
|    1 | 2372 | `{` |
|   87 | 2373 | `	return nName == sizeof("queryString")-1` |
|  154 | 2374 | `		&& SyMemcmp(zName,"queryString",sizeof("queryString")-1) == 0;` |
|    1 | 2375 | `}` |
|   16 | 2376 | `static void PdoRowQueryString(phl_pdo_stmt *pSt,ph7_value *pOut)` |
|    1 | 2377 | `{` |
|   17 | 2378 | `	ph7_value *pQs = pSt->pOwner ? PH7_NativeAttr(pSt->pOwner,"queryString") : 0;` |
|   17 | 2379 | `	if( pQs ){` |
|   17 | 2380 | `		PH7_MemObjStore(pQs,pOut);` |
|    8 | 2381 | `	}` |
|   17 | 2382 | `}` |
|    - | 2383 | `/*` |
|    - | 2384 | ` * How many columns the row HAS, which is the statement's own count and not the` |
|    - | 2385 | ` * capture's: php describes a statement once and answers for those columns for` |
|    - | 2386 | ` * as long as the cursor exists, so a walk that has run out still knows every` |
|    - | 2387 | ` * name and reads each as a null the connection's modifiers may still reshape.` |
|    - | 2388 | ` */` |
|  186 | 2389 | `static int PdoRowColumnCount(phl_pdo_stmt *pSt)` |
|    1 | 2390 | `{` |
|  187 | 2391 | `	return pSt ? PH7_PdoSqliteColumnCount(pSt) : 0;` |
|    1 | 2392 | `}` |
|    - | 2393 | `/*` |
|    - | 2394 | ` * The column a name selects, or -1. php looks a NUMBER up positionally and` |
|    - | 2395 | ` * misses outright when it is out of range -- it never falls back to a name of` |
|    - | 2396 | ` * the same spelling -- and matches a NAME byte for byte, first occurrence` |
|    - | 2397 | ` * winning when a query selects one twice.` |
|    - | 2398 | ` */` |
|  172 | 2399 | `static int PdoRowColumnOf(phl_pdo_stmt *pSt,const char *zName,int nName)` |
|    1 | 2400 | `{` |
|  173 | 2401 | `	int nCol = PdoRowColumnCount(pSt);` |
|    - | 2402 | `	ph7_int64 iPos;` |
|    - | 2403 | `	int iCol;` |
|  173 | 2404 | `	if( nCol < 1 ){` |
|  ! 0 | 2405 | `		return -1;` |
|    - | 2406 | `	}` |
|  173 | 2407 | `	if( PdoRowIntName(zName,nName,&iPos) ){` |
|   29 | 2408 | `		return (iPos >= 0 && iPos < (ph7_int64)nCol) ? (int)iPos : -1;` |
|    - | 2409 | `	}` |
|  353 | 2410 | `	for( iCol = 0 ; iCol < nCol ; ++iCol ){` |
|    - | 2411 | `		SyBlob sHave;` |
|    - | 2412 | `		int nHave, bHit;` |
|  327 | 2413 | `		SyBlobInit(&sHave,&pSt->pConn->pVm->sAllocator);` |
|  327 | 2414 | `		PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sHave);` |
|  327 | 2415 | `		nHave = (int)SyBlobLength(&sHave) - 1;   /* less the terminator */` |
|  444 | 2416 | `		bHit = nHave == nName` |
|  326 | 2417 | `			&& SyMemcmp((const char *)SyBlobData(&sHave),zName,(sxu32)nName) == 0;` |
|  327 | 2418 | `		SyBlobRelease(&sHave);` |
|  327 | 2419 | `		if( bHit ){` |
|  119 | 2420 | `			return iCol;` |
|    - | 2421 | `		}` |
|  105 | 2422 | `	}` |
|   27 | 2423 | `	return -1;` |
|   87 | 2424 | `}` |
|    - | 2425 | `/*` |
|    - | 2426 | ` * One column's value as the SCRIPT sees it: the captured raw value with the` |
|    - | 2427 | ` * connection's presentation modifiers applied now rather than at capture, so a` |
|    - | 2428 | ` * STRINGIFY_FETCHES or ORACLE_NULLS changed between two reads shows in the` |
|    - | 2429 | ` * second -- which is what php's read-through row does.` |
|    - | 2430 | ` */` |
|  168 | 2431 | `static void PdoRowColumnValue(phl_pdo_stmt *pSt,int iCol,ph7_value *pOut)` |
|    1 | 2432 | `{` |
|  253 | 2433 | `	ph7_value *pRaw = pSt->pLazyVals` |
|  157 | 2434 | `		? PdoArrayAtInt(pSt->pConn->pVm,pSt->pLazyVals,(sxi64)iCol) : 0;` |
|  169 | 2435 | `	if( pRaw ){` |
|  147 | 2436 | `		PH7_MemObjStore(pRaw,pOut);` |
|   73 | 2437 | `	}` |
|    - | 2438 | `	/* A column with no captured value is php's null -- and ORACLE_NULLS still` |
|    - | 2439 | `	 * has its say over that null, which is why an exhausted row under` |
|    - | 2440 | `	 * NULL_TO_STRING reads the empty string rather than null. */` |
|  169 | 2441 | `	PdoApplyValueMods(pSt->pConn,pOut);` |
|  169 | 2442 | `}` |
|    - | 2443 | `/*` |
|    - | 2444 | ` * php's read_property / has_property / write_property / unset_property for the` |
|    - | 2445 | ` * row -- and never its "would you take a write" question, which no rail asks of a` |
|    - | 2446 | ` * class that refuses every write outright. A name that is no column at all is` |
|    - | 2447 | ` * NULL to a read and false to an` |
|    - | 2448 | `` * isset(), never a warning -- and `queryString` is answered from the STATEMENT`` |
|    - | 2449 | ` * BEFORE any column is looked at, so a query selecting a column of that name` |
|    - | 2450 | ` * cannot shadow it. The has side does NOT know the name at all, which is why` |
|    - | 2451 | `` * `isset($row->queryString)` is false while reading it works.`` |
|    - | 2452 | ` */` |
|  172 | 2453 | `static void PdoRowProp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|    1 | 2454 | `{` |
|  173 | 2455 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|  173 | 2456 | `	const char *zName = SyStringData(pCtx->pName);` |
|  173 | 2457 | `	int nName = (int)SyStringLength(pCtx->pName);` |
|    - | 2458 | `	int iCol;` |
|   86 | 2459 | `	SXUNUSED(pVm);` |
|  173 | 2460 | `	if( pCtx->iMode == PH7_NATIVE_PROP_WRITE ){` |
|   15 | 2461 | `		pCtx->zThrowClass = "Error";` |
|   15 | 2462 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|    - | 2463 | `			"Cannot write to PDORow property");` |
|   15 | 2464 | `		return;` |
|    - | 2465 | `	}` |
|  159 | 2466 | `	if( pCtx->iMode == PH7_NATIVE_PROP_UNSET ){` |
|    5 | 2467 | `		pCtx->zThrowClass = "Error";` |
|    5 | 2468 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|    - | 2469 | `			"Cannot unset PDORow property");` |
|    5 | 2470 | `		return;` |
|    - | 2471 | `	}` |
|  155 | 2472 | `	if( pCtx->iMode == PH7_NATIVE_PROP_OWNS ){` |
|    - | 2473 | `		/* Never: every write this class sees is refused above, at the member` |
|    - | 2474 | `		 * opcode, so no write rail ever asks whether the handler would take one. */` |
|    5 | 2475 | `		return;` |
|    - | 2476 | `	}` |
|  151 | 2477 | `	pCtx->bAnswered = 1;` |
|  151 | 2478 | `	if( pSt == 0 ){` |
|  ! 0 | 2479 | `		return;   /* the statement is gone: every name reads null */` |
|    - | 2480 | `	}` |
|  151 | 2481 | `	if( (pCtx->iMode == PH7_NATIVE_PROP_READ) && PdoRowIsQueryString(zName,nName) ){` |
|   13 | 2482 | `		PdoRowQueryString(pSt,pCtx->pResult);` |
|   13 | 2483 | `		return;` |
|    - | 2484 | `	}` |
|  139 | 2485 | `	iCol = PdoRowColumnOf(pSt,zName,nName);` |
|  139 | 2486 | `	if( iCol >= 0 ){` |
|  121 | 2487 | `		PdoRowColumnValue(pSt,iCol,pCtx->pResult);` |
|   60 | 2488 | `	}` |
|  139 | 2489 | `	if( pCtx->iMode != PH7_NATIVE_PROP_READ ){` |
|    - | 2490 | `		/* php's has_property fetches the value and judges it -- by NULL-ness for` |
|    - | 2491 | `		 * isset() and by TRUTH for property_exists(), which asks the same handler` |
|    - | 2492 | `		 * with a non-zero check_empty. Either way it does NOT know the name` |
|    - | 2493 | ``		 * `queryString`, which is why reading one works where isset() on it is`` |
|    - | 2494 | `		 * false. */` |
|    - | 2495 | `		int bSet;` |
|   57 | 2496 | `		if( pCtx->iMode == PH7_NATIVE_PROP_ISSET ){` |
|   25 | 2497 | `			bSet = iCol >= 0 && (pCtx->pResult->iFlags & MEMOBJ_NULL) == 0;` |
|   13 | 2498 | `		}else{` |
|    - | 2499 | `			/* php's handler answers the two check_empty questions the same way, so` |
|    - | 2500 | ``			 * `empty($row->c)` and `property_exists($row,'c')` are both the column's`` |
|    - | 2501 | `			 * TRUTH -- a column holding 0 is isset() and is neither of these. */` |
|   33 | 2502 | `			bSet = iCol >= 0 && ph7_value_to_bool(pCtx->pResult);` |
|    - | 2503 | `		}` |
|   57 | 2504 | `		PH7_MemObjRelease(pCtx->pResult);` |
|   57 | 2505 | `		ph7_value_bool(pCtx->pResult,bSet);` |
|   28 | 2506 | `	}` |
|   87 | 2507 | `}` |
|    - | 2508 | `/*` |
|    - | 2509 | ` * php's read_dimension / has_dimension for the row, and the three refusals its` |
|    - | 2510 | ` * write side gives. The offset is the property NAME spelled as a value: an` |
|    - | 2511 | ` * integer is a column number outright, and everything else is converted to a` |
|    - | 2512 | ` * string first -- which is where an object offset raises php's` |
|    - | 2513 | ` * "could not be converted to string" Error and an array warns.` |
|    - | 2514 | ` */` |
|   48 | 2515 | `static void PdoRowDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|    1 | 2516 | `{` |
|   49 | 2517 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|    - | 2518 | `	ph7_value sKey;` |
|    - | 2519 | `	const char *zName;` |
|    - | 2520 | `	int nName, iCol;` |
|   48 | 2521 | `	if( pCtx->iMode == PH7_NATIVE_DIM_WRITE \|\| pCtx->iMode == PH7_NATIVE_DIM_APPEND` |
|   44 | 2522 | `	 \|\| pCtx->iMode == PH7_NATIVE_DIM_UNSET ){` |
|   11 | 2523 | `		pCtx->zThrowClass = "Error";` |
|   16 | 2524 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),"Cannot %s PDORow offset",` |
|   10 | 2525 | `			pCtx->iMode == PH7_NATIVE_DIM_WRITE ? "write to"` |
|    6 | 2526 | `			: (pCtx->iMode == PH7_NATIVE_DIM_APPEND ? "append to" : "unset"));` |
|   13 | 2527 | `		return;` |
|    - | 2528 | `	}` |
|   39 | 2529 | `	if( pCtx->pOffset == 0 \|\| pSt == 0 ){` |
|  ! 0 | 2530 | ``		return;   /* `$row[]` read, or a statement that is gone: null */`` |
|    - | 2531 | `	}` |
|   39 | 2532 | `	if( pCtx->pOffset->iFlags & MEMOBJ_OBJ ){` |
|  ! 0 | 2533 | `		ph7_class_instance *pObj = (ph7_class_instance *)pCtx->pOffset->x.pOther;` |
|  ! 0 | 2534 | `		pCtx->zThrowClass = "Error";` |
|  ! 0 | 2535 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|    - | 2536 | `			"Object of class %.*s could not be converted to string",` |
|  ! 0 | 2537 | `			(int)pObj->pClass->sName.nByte,pObj->pClass->sName.zString);` |
|  ! 0 | 2538 | `		return;` |
|    - | 2539 | `	}` |
|   39 | 2540 | `	PH7_MemObjInit(pVm,&sKey);` |
|   39 | 2541 | `	PH7_MemObjStore(pCtx->pOffset,&sKey);` |
|   39 | 2542 | `	if( sKey.iFlags & MEMOBJ_HASHMAP ){` |
|  ! 0 | 2543 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|  ! 0 | 2544 | `	}` |
|   39 | 2545 | `	PH7_MemObjToString(&sKey);` |
|   39 | 2546 | `	zName = (const char *)SyBlobData(&sKey.sBlob);` |
|   39 | 2547 | `	nName = (int)SyBlobLength(&sKey.sBlob);` |
|   39 | 2548 | `	if( pCtx->iMode == PH7_NATIVE_DIM_READ && zName && PdoRowIsQueryString(zName,nName) ){` |
|    5 | 2549 | `		PdoRowQueryString(pSt,pCtx->pResult);` |
|    5 | 2550 | `		PH7_MemObjRelease(&sKey);` |
|    5 | 2551 | `		return;` |
|    - | 2552 | `	}` |
|   35 | 2553 | `	iCol = PdoRowColumnOf(pSt,zName ? zName : "",zName ? nName : 0);` |
|   35 | 2554 | `	if( iCol >= 0 ){` |
|   21 | 2555 | `		PdoRowColumnValue(pSt,iCol,pCtx->pResult);` |
|   10 | 2556 | `	}` |
|   35 | 2557 | `	if( pCtx->iMode == PH7_NATIVE_DIM_ISSET ){` |
|    7 | 2558 | `		int bSet = iCol >= 0 && (pCtx->pResult->iFlags & MEMOBJ_NULL) == 0;` |
|    7 | 2559 | `		PH7_MemObjRelease(pCtx->pResult);` |
|    7 | 2560 | `		ph7_value_bool(pCtx->pResult,bSet);` |
|    3 | 2561 | `	}` |
|   35 | 2562 | `	PH7_MemObjRelease(&sKey);` |
|   25 | 2563 | `}` |
|    - | 2564 | `/*` |
|    - | 2565 | `` * php's get_debug_info for the row: `queryString` and then every column of the`` |
|    - | 2566 | ` * row it is sitting on, which is why var_dump() shows what get_object_vars()` |
|    - | 2567 | ` * does not. The get_properties half shows nothing at all, so (array), var_export` |
|    - | 2568 | ` * and json_encode answer empty.` |
|    - | 2569 | ` */` |
|   20 | 2570 | `static sxi32 PdoRowPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|    1 | 2571 | `{` |
|   21 | 2572 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|    - | 2573 | `	ph7_value sKey,sVal;` |
|    - | 2574 | `	int nCol,iCol;` |
|   21 | 2575 | `	if( !bDebug \|\| pSt == 0 ){` |
|    7 | 2576 | `		return SXRET_OK;` |
|    - | 2577 | `	}` |
|   15 | 2578 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|   15 | 2579 | `	PH7_MemObjInit(pVm,&sVal);` |
|   15 | 2580 | `	PH7_MemObjStringAppend(&sKey,"queryString",sizeof("queryString")-1);` |
|   15 | 2581 | `	if( pSt->pOwner ){` |
|   15 | 2582 | `		ph7_value *pQs = PH7_NativeAttr(pSt->pOwner,"queryString");` |
|   15 | 2583 | `		if( pQs ){` |
|   15 | 2584 | `			PH7_MemObjStore(pQs,&sVal);` |
|    7 | 2585 | `		}` |
|    7 | 2586 | `	}` |
|   15 | 2587 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    - | 2588 | `	/* The COLUMNS come from the statement rather than from the capture: php` |
|    - | 2589 | `	 * describes them once and shows them for as long as the cursor exists, so a` |
|    - | 2590 | `	 * row whose walk has run out (or whose cursor was closed) still prints every` |
|    - | 2591 | `	 * name, each holding null. */` |
|   15 | 2592 | `	nCol = PdoRowColumnCount(pSt);` |
|   45 | 2593 | `	for( iCol = 0 ; iCol < nCol ; ++iCol ){` |
|    - | 2594 | `		SyBlob sColName;` |
|    - | 2595 | `		int nName;` |
|    - | 2596 | `		const char *zName;` |
|   31 | 2597 | `		SyBlobInit(&sColName,&pVm->sAllocator);` |
|   31 | 2598 | `		PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sColName);` |
|   31 | 2599 | `		zName = (const char *)SyBlobData(&sColName);` |
|   31 | 2600 | `		nName = (int)SyBlobLength(&sColName) - 1;   /* less the terminator */` |
|   31 | 2601 | `		if( zName == 0 \|\| nName < 0 \|\| PdoRowIsQueryString(zName,nName) ){` |
|    - | 2602 | `			/* php builds the columns as a table of their own and merges it` |
|    - | 2603 | `			 * BEHIND the queryString entry, so a column of that name is the one` |
|    - | 2604 | `			 * that loses -- while two columns sharing any other name collapse` |
|    - | 2605 | `			 * to the LAST of them, which the update below does. */` |
|    3 | 2606 | `			SyBlobRelease(&sColName);` |
|    3 | 2607 | `			continue;` |
|    - | 2608 | `		}` |
|   29 | 2609 | `		PH7_MemObjRelease(&sKey);` |
|   29 | 2610 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|   29 | 2611 | `		PH7_MemObjStringAppend(&sKey,zName,(sxu32)nName);` |
|   29 | 2612 | `		PH7_MemObjRelease(&sVal);` |
|   29 | 2613 | `		PH7_MemObjInit(pVm,&sVal);` |
|   29 | 2614 | `		PdoRowColumnValue(pSt,iCol,&sVal);` |
|   29 | 2615 | `		ph7_array_add_elem(pOut,&sKey,&sVal);` |
|   29 | 2616 | `		SyBlobRelease(&sColName);` |
|   15 | 2617 | `	}` |
|   15 | 2618 | `	PH7_MemObjRelease(&sKey);` |
|   15 | 2619 | `	PH7_MemObjRelease(&sVal);` |
|   15 | 2620 | `	return SXRET_OK;` |
|   11 | 2621 | `}` |
|    - | 2622 | `/* The row is going away: the statement must stop pointing at it. */` |
|   34 | 2623 | `static void PdoRowInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    1 | 2624 | `{` |
|   35 | 2625 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|   17 | 2626 | `	SXUNUSED(pVm);` |
|   35 | 2627 | `	if( pSt && pSt->pLazyRow == pThis ){` |
|   35 | 2628 | `		pSt->pLazyRow = 0;` |
|   35 | 2629 | `		PdoStmtLazyClear(pSt);` |
|   17 | 2630 | `	}` |
|   35 | 2631 | `}` |
|    - | 2632 | `/*` |
|    - | 2633 | ` * The statement's row object, built on first use and CAPTURING the row under` |
|    - | 2634 | ` * the cursor, which it also marks as spent. Answers the object with a` |
|    - | 2635 | ` * reference of the caller's own, or 0 when it could not be made. Shared by` |
|    - | 2636 | ` * fetch() and the foreach iterator: php answers both from one lazy row.` |
|    - | 2637 | ` */` |
|   46 | 2638 | `static ph7_class_instance * PdoLazyRowFor(ph7_vm *pVm,phl_pdo_stmt *pSt)` |
|    1 | 2639 | `{` |
|   47 | 2640 | `	ph7_class_instance *pRow = pSt->pLazyRow;` |
|   47 | 2641 | `	PdoBoundColumnsForRow(pVm,pSt);` |
|   47 | 2642 | `	PdoStmtLazyCapture(pSt);` |
|   47 | 2643 | `	if( pRow == 0 ){` |
|   37 | 2644 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,"PDORow",sizeof("PDORow")-1,FALSE,0);` |
|    - | 2645 | `		SyString sAttr;` |
|    - | 2646 | `		ph7_value *pSlot;` |
|   37 | 2647 | `		pRow = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|   37 | 2648 | `		if( pRow == 0 ){` |
|  ! 0 | 2649 | `			return 0;` |
|    - | 2650 | `		}` |
|   37 | 2651 | `		SyStringInitFromBuf(&sAttr,PDOROW_RES,sizeof(PDOROW_RES)-1);` |
|   37 | 2652 | `		pSlot = PH7_ClassInstanceFetchAttr(pRow,&sAttr);` |
|   37 | 2653 | `		if( pSlot == 0 ){` |
|  ! 0 | 2654 | `			PH7_ClassInstanceUnref(pRow);` |
|  ! 0 | 2655 | `			return 0;` |
|    - | 2656 | `		}` |
|   37 | 2657 | `		PH7_MemObjRelease(pSlot);` |
|   37 | 2658 | `		pSlot->x.pOther = pSt;` |
|   37 | 2659 | `		MemObjSetType(pSlot,MEMOBJ_RES);` |
|    - | 2660 | `		/* Retain the STATEMENT object through a slot of the row's own: php's` |
|    - | 2661 | ``		 * row keeps its statement alive, so `unset($stmt)` leaves the row`` |
|    - | 2662 | `		 * reading and the database open. The statement's pointer back here is` |
|    - | 2663 | `		 * deliberately NOT a reference -- that pair would be a cycle no` |
|    - | 2664 | `		 * refcount can break. */` |
|   37 | 2665 | `		SyStringInitFromBuf(&sAttr,PDOROW_STMT,sizeof(PDOROW_STMT)-1);` |
|   37 | 2666 | `		pSlot = PH7_ClassInstanceFetchAttr(pRow,&sAttr);` |
|   37 | 2667 | `		if( pSlot && pSt->pOwner ){` |
|   37 | 2668 | `			PH7_MemObjRelease(pSlot);` |
|   37 | 2669 | `			pSt->pOwner->iRef++;` |
|   37 | 2670 | `			pSlot->x.pOther = pSt->pOwner;` |
|   37 | 2671 | `			MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|   18 | 2672 | `		}` |
|   37 | 2673 | `		pSt->pLazyRow = pRow;` |
|   19 | 2674 | `	}else{` |
|   11 | 2675 | `		pRow->iRef++;   /* the caller's own reference */` |
|    - | 2676 | `	}` |
|   47 | 2677 | `	pSt->bRowPending = 0;` |
|   47 | 2678 | `	return pRow;` |
|   24 | 2679 | `}` |
|    - | 2680 | `/*` |
|    - | 2681 | ` * PDO::FETCH_LAZY: hand the row object back and move the cursor on. The row` |
|    - | 2682 | ` * carries no values of its own -- the capture on the statement is what it` |
|    - | 2683 | ` * reads -- so a second lazy fetch answers the SAME object showing the next` |
|    - | 2684 | ` * row, which is php.` |
|    - | 2685 | ` */` |
|   36 | 2686 | `static int PdoFetchLazyRow(ph7_context *pCtx,phl_pdo_stmt *pSt)` |
|    1 | 2687 | `{` |
|    - | 2688 | `	ph7_class_instance *pRow;` |
|   37 | 2689 | `	if( !PdoStmtHasRow(pSt) ){` |
|    5 | 2690 | `		PdoStmtOk(pSt);` |
|    5 | 2691 | `		ph7_result_bool(pCtx,0);` |
|    5 | 2692 | `		return PH7_OK;` |
|    - | 2693 | `	}` |
|   33 | 2694 | `	pRow = PdoLazyRowFor(pCtx->pVm,pSt);` |
|   33 | 2695 | `	if( pRow == 0 ){` |
|  ! 0 | 2696 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2697 | `	}` |
|   33 | 2698 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2699 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2700 | `		PH7_ClassInstanceUnref(pRow);` |
|  ! 0 | 2701 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2702 | `	}` |
|   33 | 2703 | `	PdoStmtOk(pSt);` |
|   33 | 2704 | `	PH7_NativeResultObject(pCtx,pRow);` |
|   33 | 2705 | `	return PH7_OK;` |
|   19 | 2706 | `}` |
|    - | 2707 | `/*` |
|    - | 2708 | ` * PDOStatement::fetch(int $mode = PDO::FETCH_DEFAULT, ...): mixed` |
|    - | 2709 | ` *` |
|    - | 2710 | ` * FETCH_DEFAULT means the connection's ATTR_DEFAULT_FETCH_MODE, which is` |
|    - | 2711 | ` * FETCH_BOTH unless the script changed it -- so a bare fetch() answers every` |
|    - | 2712 | ` * column twice, once under its name and once under its position.` |
|    - | 2713 | ` */` |
|  346 | 2714 | `static int vm_builtin_PDOStatement_fetch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2715 | `{` |
|  348 | 2716 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2717 | `	ph7_value *pRow;` |
|    - | 2718 | `	int iMode;` |
|    - | 2719 | `	sxi32 rcFlags;` |
|  348 | 2720 | `	if( pSt == 0 ){` |
|    3 | 2721 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2722 | `	}` |
|  346 | 2723 | `	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;` |
|  346 | 2724 | `	rcFlags = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::fetch",1,"mode");` |
|  346 | 2725 | `	if( rcFlags != PH7_OK ){` |
|  ! 0 | 2726 | `		return rcFlags;` |
|    - | 2727 | `	}` |
|  346 | 2728 | `	if( iMode == PDO_FETCH_DEFAULT ){` |
|  176 | 2729 | `		iMode = pSt->iFetchMode;` |
|   87 | 2730 | `	}` |
|  346 | 2731 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_DEFAULT ){` |
|    - | 2732 | `		/* A statement whose own mode is FETCH_DEFAULT -- which only a` |
|    - | 2733 | `		 * connection whose ATTR_DEFAULT_FETCH_MODE is 0 leaves it on -- has no` |
|    - | 2734 | `		 * mode to fall back to, and php says so at the fetch. */` |
|  ! 0 | 2735 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2736 | `			"PDOStatement::fetch(): Argument #1 ($mode) must be a bitmask of "` |
|    - | 2737 | `			"PDO::FETCH_* constants");` |
|    - | 2738 | `	}` |
|  346 | 2739 | `	if( PdoBoundColumnsBad(pSt) ){` |
|   11 | 2740 | `		return PdoBoundColumnsRefuse(pCtx,pSt,0);` |
|    - | 2741 | `	}` |
|  336 | 2742 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_LAZY ){` |
|   37 | 2743 | `		return PdoFetchLazyRow(pCtx,pSt);` |
|    - | 2744 | `	}` |
|  300 | 2745 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_KEY_PAIR ){` |
|    - | 2746 | `		/* php's own fetch() cannot do this mode: it builds a value var_dump` |
|    - | 2747 | `		 * crashes on and json_encode refuses, and one spelling of the same call` |
|    - | 2748 | `		 * aborts the process (the scope policy -- a php defect PHL does not reproduce). The` |
|    - | 2749 | `		 * honest answer is the one the mode NAMES and fetchAll() builds: the` |
|    - | 2750 | `		 * row as a single key => value pair. */` |
|    - | 2751 | `		ph7_value *pPair,*pRowVals,*pKey,*pVal;` |
|   11 | 2752 | `		if( PH7_PdoSqliteColumnCount(pSt) != 2 ){` |
|    3 | 2753 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 2754 | `				"SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires "` |
|    - | 2755 | `				"the result set to contain exactly 2 columns.");` |
|    - | 2756 | `		}` |
|    9 | 2757 | `		if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2758 | `			PdoStmtOk(pSt);` |
|    3 | 2759 | `			ph7_result_bool(pCtx,0);` |
|    3 | 2760 | `			return PH7_OK;` |
|    - | 2761 | `		}` |
|    7 | 2762 | `		pPair    = ph7_context_new_array(pCtx);` |
|    7 | 2763 | `		pRowVals = ph7_context_new_array(pCtx);` |
|    7 | 2764 | `		if( pPair == 0 \|\| pRowVals == 0 ){` |
|  ! 0 | 2765 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 2766 | `		}` |
|    7 | 2767 | `		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pRowVals) ){` |
|  ! 0 | 2768 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 2769 | `			return PH7_OK;` |
|    - | 2770 | `		}` |
|    7 | 2771 | `		pKey = PdoArrayAtInt(pCtx->pVm,pRowVals,0);` |
|    7 | 2772 | `		pVal = PdoArrayAtInt(pCtx->pVm,pRowVals,1);` |
|    7 | 2773 | `		ph7_array_add_elem(pPair,pKey,pVal);` |
|    7 | 2774 | `		ph7_result_value(pCtx,pPair);` |
|    7 | 2775 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2776 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2777 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2778 | `		}` |
|    7 | 2779 | `		PdoStmtOk(pSt);` |
|    7 | 2780 | `		return PH7_OK;` |
|    - | 2781 | `	}` |
|  289 | 2782 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_COLUMN ){` |
|    - | 2783 | `		/* a statement told to fetch one COLUMN answers that column from here` |
|    - | 2784 | `		 * on, whichever verb asks for the row */` |
|    - | 2785 | `		ph7_value *pOneRow,*pOne;` |
|   19 | 2786 | `		if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2787 | `			PdoStmtOk(pSt);` |
|    3 | 2788 | `			ph7_result_bool(pCtx,0);` |
|    3 | 2789 | `			return PH7_OK;` |
|    - | 2790 | `		}` |
|   17 | 2791 | `		if( pSt->iFetchColumn >= PH7_PdoSqliteColumnCount(pSt) ){` |
|    - | 2792 | `			/* php checks the width only once it has a ROW to read it from, so a` |
|    - | 2793 | `			 * cursor with nothing left answers false rather than refusing. */` |
|    5 | 2794 | `			return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    - | 2795 | `		}` |
|   13 | 2796 | `		pOneRow = ph7_context_new_array(pCtx);` |
|   13 | 2797 | `		if( pOneRow == 0 ){` |
|  ! 0 | 2798 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 2799 | `		}` |
|   13 | 2800 | `		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pOneRow) ){` |
|  ! 0 | 2801 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 2802 | `			return PH7_OK;` |
|    - | 2803 | `		}` |
|   13 | 2804 | `		pOne = PdoArrayAtInt(pCtx->pVm,pOneRow,(sxi64)pSt->iFetchColumn);` |
|   13 | 2805 | `		if( pOne ){` |
|   13 | 2806 | `			ph7_result_value(pCtx,pOne);` |
|    7 | 2807 | `		}else{` |
|  ! 0 | 2808 | `			ph7_result_null(pCtx);` |
|    - | 2809 | `		}` |
|   13 | 2810 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2811 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2812 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2813 | `		}` |
|   13 | 2814 | `		PdoStmtOk(pSt);` |
|   13 | 2815 | `		return PH7_OK;` |
|    - | 2816 | `	}` |
|  271 | 2817 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_BOUND ){` |
|   21 | 2818 | `		if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2819 | `			PdoStmtOk(pSt);` |
|    3 | 2820 | `			ph7_result_bool(pCtx,0);` |
|    3 | 2821 | `			return PH7_OK;` |
|    - | 2822 | `		}` |
|   19 | 2823 | `		PdoBoundColumnsForRow(pCtx->pVm,pSt);` |
|   19 | 2824 | `		pSt->bRowPending = 0;` |
|   19 | 2825 | `		ph7_result_bool(pCtx,1);` |
|   19 | 2826 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2827 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2828 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2829 | `		}` |
|   19 | 2830 | `		PdoStmtOk(pSt);` |
|   19 | 2831 | `		return PH7_OK;` |
|    - | 2832 | `	}` |
|  250 | 2833 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_CLASS` |
|  238 | 2834 | `	 \|\| (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_INTO ){` |
|   39 | 2835 | `		return PdoFetchObjectRow(pCtx,pSt,iMode,0,0,"PDOStatement::fetch");` |
|    - | 2836 | `	}` |
|  213 | 2837 | `	if( !PdoStmtHasRow(pSt) ){` |
|   21 | 2838 | `		PdoStmtOk(pSt);` |
|   21 | 2839 | `		ph7_result_bool(pCtx,0);` |
|   21 | 2840 | `		return PH7_OK;` |
|    - | 2841 | `	}` |
|  193 | 2842 | `	pRow = ph7_context_new_array(pCtx);` |
|  193 | 2843 | `	if( pRow == 0 ){` |
|  ! 0 | 2844 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2845 | `	}` |
|  193 | 2846 | `	if( !PdoStmtRow(pCtx->pVm,pSt,iMode & PDO_FETCH_MODE_MASK,pRow) ){` |
|  ! 0 | 2847 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2848 | `		return PH7_OK;` |
|    - | 2849 | `	}` |
|  193 | 2850 | `	ph7_result_value(pCtx,pRow);` |
|    - | 2851 | `	/* step ahead so the next call knows whether a row is waiting without` |
|    - | 2852 | `	 * having to ask twice */` |
|  193 | 2853 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2854 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2855 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2856 | `	}` |
|  193 | 2857 | `	PdoStmtOk(pSt);` |
|  193 | 2858 | `	return PH7_OK;` |
|  175 | 2859 | `}` |
|    - | 2860 | `/*` |
|    - | 2861 | ` * PDOStatement::getColumnMeta(int $column): array\|false` |
|    - | 2862 | ` *` |
|    - | 2863 | ` * php's eight keys. Two of them describe different things and are routinely` |
|    - | 2864 | ``  * confused: `sqlite:decl_type` is what the SCHEMA declares, and `native_type` `` |
|    - | 2865 | ` * is the type of the value in the CURRENT row -- so a TEXT column holding NULL` |
|    - | 2866 | ` * reports "TEXT" and "null" at once, and an exhausted cursor reports "null"` |
|    - | 2867 | ` * for every column.` |
|    - | 2868 | ` *` |
|    - | 2869 | ` * A column that does not exist answers false, and php reports the last STEP's` |
|    - | 2870 | ` * result code as the driver error while doing so: that is why asking for` |
|    - | 2871 | ` * column 99 while a row is up comes back as "100 another row available"` |
|    - | 2872 | ` * instead of anything about the index.` |
|    - | 2873 | ` */` |
|    8 | 2874 | `static int vm_builtin_PDOStatement_getColumnMeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2875 | `{` |
|    9 | 2876 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2877 | `	ph7_value *pMeta,*pCell,*pFlags;` |
|    - | 2878 | `	ph7_int64 iCol;` |
|    - | 2879 | `	const char *zDecl,*zTable,*zName;` |
|    - | 2880 | `	int iType,iPdoType;` |
|    - | 2881 | `	const char *zNative;` |
|    - | 2882 | `	SyBlob sName;` |
|    9 | 2883 | `	if( pSt == 0 ){` |
|  ! 0 | 2884 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2885 | `	}` |
|    9 | 2886 | `	iCol = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    9 | 2887 | `	if( iCol < 0 ){` |
|    3 | 2888 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2889 | `			"PDOStatement::getColumnMeta(): Argument #1 ($column) must be greater than "` |
|    - | 2890 | `			"or equal to 0");` |
|    - | 2891 | `	}` |
|    7 | 2892 | `	if( iCol >= (ph7_int64)PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 2893 | `		int iStep = PH7_PdoSqliteLastStepCode(pSt);` |
|    3 | 2894 | `		ph7_result_bool(pCtx,0);` |
|    - | 2895 | `		/* php reports the last STEP's code as the driver detail here, which is` |
|    - | 2896 | `		 * why an out-of-range index talks about a row being available */` |
|    4 | 2897 | `		PH7_PdoSetError(pSt->pConn,"HY000",iStep,` |
|    1 | 2898 | `			iStep == 100 ? "another row available" : "no more rows available");` |
|    3 | 2899 | `		pSt->pConn->iErrState = PDO_ERR_OK;   /* the CONNECTION did not fail */` |
|    3 | 2900 | `		SyMemcpy("00000",pSt->pConn->zSqlState,sizeof("00000"));` |
|    3 | 2901 | `		PdoStmtFailed(pSt,"HY000");` |
|    3 | 2902 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::getColumnMeta");` |
|    - | 2903 | `	}` |
|    5 | 2904 | `	pMeta = ph7_context_new_array(pCtx);` |
|    5 | 2905 | `	pCell = ph7_context_new_scalar(pCtx);` |
|    5 | 2906 | `	pFlags = ph7_context_new_array(pCtx);` |
|    5 | 2907 | `	if( pMeta == 0 \|\| pCell == 0 \|\| pFlags == 0 ){` |
|  ! 0 | 2908 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2909 | `	}` |
|    5 | 2910 | `	iType = pSt->bRowPending ? PH7_PdoSqliteColumnType(pSt,(int)iCol) : SQLITE_NULL;` |
|    5 | 2911 | `	switch( iType ){` |
|    3 | 2912 | `		case SQLITE_INTEGER: zNative = "integer"; iPdoType = PDO_PARAM_INT; break;` |
|  ! 0 | 2913 | `		case SQLITE_FLOAT:   zNative = "double";  iPdoType = PDO_PARAM_STR; break;` |
|  ! 0 | 2914 | `		case SQLITE_BLOB:    zNative = "blob";    iPdoType = PDO_PARAM_LOB; break;` |
|    3 | 2915 | `		case SQLITE_NULL:    zNative = "null";    iPdoType = PDO_PARAM_NULL; break;` |
|  ! 0 | 2916 | `		default:             zNative = "string";  iPdoType = PDO_PARAM_STR; break;` |
|    - | 2917 | `	}` |
|    5 | 2918 | `	PH7_MemObjRelease(pCell);` |
|    5 | 2919 | `	ph7_value_string(pCell,zNative,(int)SyStrlen(zNative));` |
|    5 | 2920 | `	ph7_array_add_strkey_elem(pMeta,"native_type",pCell);` |
|    5 | 2921 | `	ph7_value_int(pCell,iPdoType);` |
|    5 | 2922 | `	ph7_array_add_strkey_elem(pMeta,"pdo_type",pCell);` |
|    5 | 2923 | `	zDecl = PH7_PdoSqliteColumnDecl(pSt,(int)iCol);` |
|    5 | 2924 | `	if( zDecl ){` |
|    5 | 2925 | `		PH7_MemObjRelease(pCell);` |
|    5 | 2926 | `		ph7_value_string(pCell,zDecl,(int)SyStrlen(zDecl));` |
|    5 | 2927 | `		ph7_array_add_strkey_elem(pMeta,"sqlite:decl_type",pCell);` |
|    2 | 2928 | `	}` |
|    5 | 2929 | `	zTable = PH7_PdoSqliteColumnTable(pSt,(int)iCol);` |
|    5 | 2930 | `	if( zTable ){` |
|    5 | 2931 | `		PH7_MemObjRelease(pCell);` |
|    5 | 2932 | `		ph7_value_string(pCell,zTable,(int)SyStrlen(zTable));` |
|    5 | 2933 | `		ph7_array_add_strkey_elem(pMeta,"table",pCell);` |
|    2 | 2934 | `	}` |
|    5 | 2935 | `	ph7_array_add_strkey_elem(pMeta,"flags",pFlags);` |
|    5 | 2936 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    5 | 2937 | `	zName = PH7_PdoSqliteColumnName(pSt,(int)iCol);` |
|    5 | 2938 | `	PdoColumnName(pSt->pConn,zName,&sName);` |
|    5 | 2939 | `	PH7_MemObjRelease(pCell);` |
|    5 | 2940 | `	ph7_value_string(pCell,(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName) - 1);` |
|    5 | 2941 | `	ph7_array_add_strkey_elem(pMeta,"name",pCell);` |
|    5 | 2942 | `	SyBlobRelease(&sName);` |
|    5 | 2943 | `	ph7_value_int(pCell,-1);` |
|    5 | 2944 | `	ph7_array_add_strkey_elem(pMeta,"len",pCell);` |
|    5 | 2945 | `	ph7_value_int(pCell,0);` |
|    5 | 2946 | `	ph7_array_add_strkey_elem(pMeta,"precision",pCell);` |
|    5 | 2947 | `	ph7_result_value(pCtx,pMeta);` |
|    5 | 2948 | `	return PH7_OK;` |
|    5 | 2949 | `}` |
|    - | 2950 | `/*` |
|    - | 2951 | ` * PDOStatement::nextRowset(): bool` |
|    - | 2952 | ` *` |
|    - | 2953 | ` * sqlite has no second result set to move to, so this is the layer refusal --` |
|    - | 2954 | ` * false, and IM001 with php's own "driver does not support multiple rowsets".` |
|    - | 2955 | ` * It leaves the CURSOR alone: a fetch after it still answers the row that was` |
|    - | 2956 | ` * waiting.` |
|    - | 2957 | ` */` |
|    4 | 2958 | `static int vm_builtin_PDOStatement_nextRowset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2959 | `{` |
|    5 | 2960 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 2961 | `	SXUNUSED(nArg);` |
|    2 | 2962 | `	SXUNUSED(apArg);` |
|    5 | 2963 | `	if( pSt == 0 ){` |
|  ! 0 | 2964 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2965 | `	}` |
|    5 | 2966 | `	ph7_result_bool(pCtx,0);` |
|    5 | 2967 | `	PdoStmtFailed(pSt,"IM001");` |
|    5 | 2968 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::nextRowset","IM001",` |
|    - | 2969 | `		"driver does not support multiple rowsets");` |
|    3 | 2970 | `}` |
|    - | 2971 | `/*` |
|    - | 2972 | ` * PDOStatement::fetchColumn(int $column = 0): mixed` |
|    - | 2973 | ` *` |
|    - | 2974 | ` * One column of the next row, by position. An index outside the RESULT SET is` |
|    - | 2975 | ` * a ValueError rather than a null, and its two refusals are worded unlike` |
|    - | 2976 | ` * fetchAll()'s -- php's own inconsistency, reproduced.` |
|    - | 2977 | ` */` |
|   20 | 2978 | `static int vm_builtin_PDOStatement_fetchColumn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2979 | `{` |
|   21 | 2980 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2981 | `	ph7_int64 iCol;` |
|    - | 2982 | `	ph7_value *pRow,*pCell;` |
|   21 | 2983 | `	if( pSt == 0 ){` |
|  ! 0 | 2984 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2985 | `	}` |
|   21 | 2986 | `	iCol = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|   21 | 2987 | `	if( iCol < 0 ){` |
|    3 | 2988 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2989 | `			"Column index must be greater than or equal to 0");` |
|    - | 2990 | `	}` |
|   19 | 2991 | `	if( iCol >= (ph7_int64)PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 2992 | `		return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    - | 2993 | `	}` |
|   17 | 2994 | `	if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2995 | `		PdoStmtOk(pSt);` |
|    3 | 2996 | `		ph7_result_bool(pCtx,0);` |
|    3 | 2997 | `		return PH7_OK;` |
|    - | 2998 | `	}` |
|   15 | 2999 | `	if( PdoBoundColumnsBad(pSt) ){` |
|  ! 0 | 3000 | `		return PdoBoundColumnsRefuse(pCtx,pSt,0);` |
|    - | 3001 | `	}` |
|   15 | 3002 | `	pRow = ph7_context_new_array(pCtx);` |
|   15 | 3003 | `	if( pRow == 0 ){` |
|  ! 0 | 3004 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3005 | `	}` |
|   15 | 3006 | `	if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pRow) ){` |
|  ! 0 | 3007 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 3008 | `		return PH7_OK;` |
|    - | 3009 | `	}` |
|   15 | 3010 | `	pCell = PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)iCol);` |
|   15 | 3011 | `	if( pCell ){` |
|   15 | 3012 | `		ph7_result_value(pCtx,pCell);` |
|    8 | 3013 | `	}else{` |
|  ! 0 | 3014 | `		ph7_result_null(pCtx);` |
|    - | 3015 | `	}` |
|   15 | 3016 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 3017 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 3018 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchColumn");` |
|    - | 3019 | `	}` |
|   15 | 3020 | `	PdoStmtOk(pSt);` |
|   15 | 3021 | `	return PH7_OK;` |
|   11 | 3022 | `}` |
|    - | 3023 | `/*` |
|    - | 3024 | `` * php's `pdo_stmt_setup_fetch_mode`: the mode a statement will use from here`` |
|    - | 3025 | ` * on, and the whole screen over it. Two verbs give one: setFetchMode()'s first` |
|    - | 3026 | ` * argument and query()'s SECOND, so every diagnostic counts arguments the way` |
|    - | 3027 | `` * the verb that took them does -- `iModeArg` is the mode's own 1-based`` |
|    - | 3028 | ` * position, and the counts php reports are that position plus what the mode` |
|    - | 3029 | ` * needs beside it.` |
|    - | 3030 | ` *` |
|    - | 3031 | ` * The rules are php's, per mode: FETCH_COLUMN wants a column NUMBER and` |
|    - | 3032 | ` * FETCH_INTO an OBJECT, both exactly one; FETCH_CLASS wants a class NAME and` |
|    - | 3033 | ` * accepts constructor arguments behind it -- unless FETCH_CLASSTYPE rides on` |
|    - | 3034 | ` * it, which takes the class from the first column and therefore wants nothing;` |
|    - | 3035 | ` * FETCH_FUNC belongs to fetchAll() alone; and every other mode takes the mode` |
|    - | 3036 | `` * and nothing else. A base outside php's own enum is `must be a bitmask of`` |
|    - | 3037 | `` * PDO::FETCH_* constants`. FETCH_DEFAULT itself names the connection's`` |
|    - | 3038 | ` * ATTR_DEFAULT_FETCH_MODE and leaves the statement on it.` |
|    - | 3039 | ` */` |
|  544 | 3040 | `static sxi32 PdoSetupFetchMode(ph7_context *pCtx,phl_pdo_stmt *pSt,int nArg,ph7_value **apArg,` |
|    - | 3041 | `	int iModeArg,const char *zFn,const char *zModeParam)` |
|    2 | 3042 | `{` |
|  546 | 3043 | `	ph7_value *pMode = nArg >= iModeArg ? apArg[iModeArg-1] : 0;` |
|  546 | 3044 | `	int iMode = pMode ? (int)ph7_value_to_int64(pMode) : PDO_FETCH_DEFAULT;` |
|  546 | 3045 | `	int iBase = iMode & PDO_FETCH_MODE_MASK;` |
|  546 | 3046 | `	int nExtra = nArg - iModeArg;          /* arguments given BEHIND the mode */` |
|    - | 3047 | `	char zBuf[64];` |
|    - | 3048 | `	sxi32 rc;` |
|    - | 3049 | `	/* php clears the statement's mode BEFORE it judges the new one, and clears` |
|    - | 3050 | `	 * it to the CONNECTION's default rather than to what the statement was` |
|    - | 3051 | `	 * carrying -- so a REFUSED setFetchMode() leaves a statement that was` |
|    - | 3052 | `	 * fetching NUM answering whatever ATTR_DEFAULT_FETCH_MODE says. */` |
|  546 | 3053 | `	PdoStmtClearFetchState(pSt);` |
|  546 | 3054 | `	pSt->iFetchMode = pSt->pConn->iDefaultFetch;` |
|  546 | 3055 | `	pSt->iFetchColumn = 0;` |
|  546 | 3056 | `	if( iBase > PDO_FETCH_KEY_PAIR ){` |
|   43 | 3057 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3058 | `			"%s(): Argument #%d ($%s) must be a bitmask of PDO::FETCH_* constants",` |
|   14 | 3059 | `			zFn,iModeArg,zModeParam);` |
|    - | 3060 | `	}` |
|  518 | 3061 | `	rc = PdoCheckFetchFlags(pCtx,iMode,zFn,iModeArg,zModeParam);` |
|  518 | 3062 | `	if( rc != PH7_OK ){` |
|    3 | 3063 | `		return rc;` |
|    - | 3064 | `	}` |
|  516 | 3065 | `	if( iBase == PDO_FETCH_FUNC ){` |
|   43 | 3066 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3067 | `			"%s(): Argument #%d ($%s) PDO::FETCH_FUNC can only be used with "` |
|   14 | 3068 | `			"PDOStatement::fetchAll()",zFn,iModeArg,zModeParam);` |
|    - | 3069 | `	}` |
|  488 | 3070 | `	if( iBase == PDO_FETCH_CLASS && (iMode & PDO_FETCH_CLASSTYPE) == 0 ){` |
|    - | 3071 | `		/* The class NAME, then optional constructor arguments. php checks the` |
|    - | 3072 | `		 * TYPE of what it was handed before it counts, so a wrong second` |
|    - | 3073 | `		 * argument is a TypeError even when a fourth is there too. */` |
|   47 | 3074 | `		if( nExtra < 1 ){` |
|    7 | 3075 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3076 | `				"%s() expects at least %d arguments for the fetch mode provided, %d given",` |
|    2 | 3077 | `				zFn,iModeArg+1,nArg);` |
|    - | 3078 | `		}` |
|   43 | 3079 | `		if( (apArg[iModeArg]->iFlags & MEMOBJ_STRING) == 0 ){` |
|   19 | 3080 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3081 | `				"%s(): Argument #%d must be of type string, %s given",` |
|   12 | 3082 | `				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));` |
|    - | 3083 | `		}` |
|    - | 3084 | `		{` |
|    - | 3085 | `			/* php resolves the name HERE -- before it looks at the constructor` |
|    - | 3086 | `			 * arguments behind it -- so a class that does not exist is refused` |
|    - | 3087 | `			 * where it was named rather than at the first fetch, and one that` |
|    - | 3088 | `			 * merely cannot be instantiated is accepted here and refused there. */` |
|   31 | 3089 | `			int nCls = 0;` |
|   31 | 3090 | `			const char *zCls = ph7_value_to_string(apArg[iModeArg],&nCls);` |
|   30 | 3091 | `			if( zCls == 0 \|\| nCls < 1` |
|   31 | 3092 | `			 \|\| PH7_VmExtractClass(pCtx->pVm,zCls,(sxu32)nCls,FALSE,0) == 0 ){` |
|    4 | 3093 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|    1 | 3094 | `					"%s(): Argument #%d must be a valid class",zFn,iModeArg+1);` |
|    - | 3095 | `			}` |
|    - | 3096 | `		}` |
|   29 | 3097 | `		if( nExtra > 1 && (apArg[iModeArg+1]->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL)) == 0 ){` |
|    7 | 3098 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3099 | `				"%s(): Argument #%d must be of type ?array, %s given",` |
|    4 | 3100 | `				zFn,iModeArg+2,VmValueGivenName(apArg[iModeArg+1],zBuf,sizeof(zBuf)));` |
|    - | 3101 | `		}` |
|   25 | 3102 | `		if( nExtra > 2 ){` |
|  ! 0 | 3103 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3104 | `				"%s() expects at most %d arguments for the fetch mode provided, %d given",` |
|  ! 0 | 3105 | `				zFn,iModeArg+2,nArg);` |
|    1 | 3106 | `		}` |
|  454 | 3107 | `	}else if( iBase == PDO_FETCH_COLUMN \|\| iBase == PDO_FETCH_INTO ){` |
|  105 | 3108 | `		if( nExtra != 1 ){` |
|   67 | 3109 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3110 | `				"%s() expects exactly %d arguments for the fetch mode provided, %d given",` |
|   22 | 3111 | `				zFn,iModeArg+1,nArg);` |
|    - | 3112 | `		}` |
|    - | 3113 | `		/* php's screen is the zval's TYPE: only a real int passes, and a float` |
|    - | 3114 | `		 * whose value happens to be integral does not (the slot may carry the` |
|    - | 3115 | `		 * int flag beside the real one once something has read it as a number,` |
|    - | 3116 | `		 * so the REAL bit is what decides). */` |
|   60 | 3117 | `		if( iBase == PDO_FETCH_COLUMN` |
|   52 | 3118 | `		 && ((apArg[iModeArg]->iFlags & MEMOBJ_INT) == 0` |
|   34 | 3119 | `		  \|\| (apArg[iModeArg]->iFlags & MEMOBJ_REAL) != 0) ){` |
|   28 | 3120 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3121 | `				"%s(): Argument #%d must be of type int, %s given",` |
|   18 | 3122 | `				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));` |
|    - | 3123 | `		}` |
|   43 | 3124 | `		if( iBase == PDO_FETCH_INTO && (apArg[iModeArg]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   13 | 3125 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3126 | `				"%s(): Argument #%d must be of type object, %s given",` |
|    8 | 3127 | `				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));` |
|    - | 3128 | `		}` |
|   35 | 3129 | `		if( iBase == PDO_FETCH_COLUMN && ph7_value_to_int64(apArg[iModeArg]) < 0 ){` |
|    - | 3130 | `			/* A NEGATIVE column is refused where it is given; one merely past` |
|    - | 3131 | `` 			 * the last column is not, and answers php's `Invalid column index` `` |
|    - | 3132 | `			 * at the fetch -- the statement's width is not this screen's` |
|    - | 3133 | `			 * business. */` |
|    4 | 3134 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    1 | 3135 | `				"%s(): Argument #%d must be greater than or equal to 0",zFn,iModeArg+1);` |
|    1 | 3136 | `		}` |
|  354 | 3137 | `	}else if( nExtra > 0 ){` |
|  361 | 3138 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3139 | `			"%s() expects exactly %d arguments for the fetch mode provided, %d given",` |
|  120 | 3140 | `			zFn,iModeArg,nArg);` |
|    - | 3141 | `	}` |
|  166 | 3142 | `	if( iBase == PDO_FETCH_CLASS && (iMode & PDO_FETCH_CLASSTYPE) == 0 ){` |
|   25 | 3143 | `		int nName = 0;` |
|   25 | 3144 | `		const char *zName = ph7_value_to_string(apArg[iModeArg],&nName);` |
|   25 | 3145 | `		if( zName && nName > 0 ){` |
|   49 | 3146 | `			pSt->zFetchClass = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|   24 | 3147 | `				(sxu32)nName + 1);` |
|   25 | 3148 | `			if( pSt->zFetchClass ){` |
|   25 | 3149 | `				SyMemcpy(zName,pSt->zFetchClass,(sxu32)nName);` |
|   25 | 3150 | `				pSt->zFetchClass[nName] = 0;` |
|   25 | 3151 | `				pSt->nFetchClass = nName;` |
|   12 | 3152 | `			}` |
|   12 | 3153 | `		}` |
|   25 | 3154 | `		if( nExtra > 1 && (apArg[iModeArg+1]->iFlags & MEMOBJ_HASHMAP) ){` |
|    9 | 3155 | `			pSt->pFetchArgs = ph7_new_array(pCtx->pVm);` |
|    9 | 3156 | `			if( pSt->pFetchArgs ){` |
|    9 | 3157 | `				PH7_MemObjStore(apArg[iModeArg+1],pSt->pFetchArgs);` |
|    4 | 3158 | `			}` |
|    5 | 3159 | `		}` |
|  142 | 3160 | `	}else if( iBase == PDO_FETCH_INTO ){` |
|   11 | 3161 | `		pSt->pFetchInto = (ph7_class_instance *)apArg[iModeArg]->x.pOther;` |
|   11 | 3162 | `		pSt->pFetchInto->iRef++;   /* the statement writes into it for as long as it lives */` |
|    5 | 3163 | `	}` |
|    - | 3164 | `	/* FETCH_DEFAULT is not a mode to keep: the statement stays on the` |
|    - | 3165 | `	 * connection's default it was just cleared to (php 8.5.11, GH-20214). */` |
|  154 | 3166 | `	if( iBase != PDO_FETCH_DEFAULT ){` |
|  142 | 3167 | `		pSt->iFetchMode = iMode;` |
|   70 | 3168 | `	}` |
|  154 | 3169 | `	pSt->iFetchColumn = iBase == PDO_FETCH_COLUMN` |
|   87 | 3170 | `		? (int)ph7_value_to_int64(apArg[iModeArg]) : 0;` |
|  154 | 3171 | `	return PH7_OK;` |
|  274 | 3172 | `}` |
|    - | 3173 | `/*` |
|    - | 3174 | ` * PDOStatement::setFetchMode(int $mode, mixed ...$args): true` |
|    - | 3175 | ` *` |
|    - | 3176 | ` * The mode a bare fetch()/fetchAll() will use from here on -- one spelling of` |
|    - | 3177 | ` * the screen above, the other being PDO::query()'s second argument.` |
|    - | 3178 | ` */` |
|  296 | 3179 | `static int vm_builtin_PDOStatement_setFetchMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3180 | `{` |
|  298 | 3181 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3182 | `	sxi32 rc;` |
|  298 | 3183 | `	if( pSt == 0 ){` |
|  ! 0 | 3184 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3185 | `	}` |
|  298 | 3186 | `	rc = PdoSetupFetchMode(pCtx,pSt,nArg,apArg,1,"PDOStatement::setFetchMode","mode");` |
|  298 | 3187 | `	if( rc != PH7_OK ){` |
|  199 | 3188 | `		return rc;` |
|    - | 3189 | `	}` |
|  100 | 3190 | `	ph7_result_bool(pCtx,1);` |
|  100 | 3191 | `	return PH7_OK;` |
|  150 | 3192 | `}` |
|    - | 3193 |  |
|    - | 3194 | `/* Add one row under a key, collecting repeats into a list (FETCH_GROUP). */` |
|   18 | 3195 | `static void PdoGroupAppend(ph7_context *pCtx,ph7_value *pOut,ph7_value *pKey,ph7_value *pRow)` |
|    1 | 3196 | `{` |
|   19 | 3197 | `	ph7_value *pList = 0;` |
|   19 | 3198 | `	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){` |
|  ! 0 | 3199 | `		int nKey = 0;` |
|  ! 0 | 3200 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|  ! 0 | 3201 | `		pList = ph7_array_fetch(pOut,zKey,nKey);` |
|   19 | 3202 | `	}else if( pKey ){` |
|    - | 3203 | `		SyBlob sKey;` |
|   19 | 3204 | `		SyBlobInit(&sKey,&pCtx->pVm->sAllocator);` |
|   19 | 3205 | `		SyBlobFormat(&sKey,"%qd",ph7_value_to_int64(pKey));` |
|   19 | 3206 | `		SyBlobAppend(&sKey,"",1);` |
|   28 | 3207 | `		pList = ph7_array_fetch(pOut,(const char *)SyBlobData(&sKey),` |
|   18 | 3208 | `			(int)SyBlobLength(&sKey) - 1);` |
|   19 | 3209 | `		SyBlobRelease(&sKey);` |
|    9 | 3210 | `	}` |
|   19 | 3211 | `	if( pList && (pList->iFlags & MEMOBJ_HASHMAP) ){` |
|    7 | 3212 | `		ph7_array_add_elem(pList,0,pRow);` |
|    7 | 3213 | `		return;` |
|    - | 3214 | `	}` |
|   13 | 3215 | `	pList = ph7_context_new_array(pCtx);` |
|   13 | 3216 | `	if( pList == 0 ){` |
|  ! 0 | 3217 | `		return;` |
|    - | 3218 | `	}` |
|   13 | 3219 | `	ph7_array_add_elem(pList,0,pRow);` |
|   13 | 3220 | `	ph7_array_add_elem(pOut,pKey,pList);` |
|   10 | 3221 | `}` |
|    - | 3222 | `/*` |
|    - | 3223 | ` * PDOStatement::fetchAll(int $mode = PDO::FETCH_DEFAULT, mixed ...$args): array` |
|    - | 3224 | ` *` |
|    - | 3225 | ` * Every remaining row in one array. Four of the modes change the shape of that` |
|    - | 3226 | ` * ARRAY rather than the shape of a row: FETCH_COLUMN reduces each row to one` |
|    - | 3227 | ` * value, FETCH_KEY_PAIR to a key and a value (and refuses a result set that is` |
|    - | 3228 | ` * not exactly two columns wide), FETCH_FUNC replaces it with whatever a` |
|    - | 3229 | ` * callable answers, and GROUP/UNIQUE take the first column as a key -- GROUP` |
|    - | 3230 | ` * collecting every row under it, UNIQUE keeping the last.` |
|    - | 3231 | ` *` |
|    - | 3232 | ` * php counts arguments per mode here too, and its FETCH_FUNC wording is` |
|    - | 3233 | ` * singular ("expects exactly 2 argument"); both are reproduced as they stand.` |
|    - | 3234 | ` */` |
|   94 | 3235 | `static int vm_builtin_PDOStatement_fetchAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3236 | `{` |
|   96 | 3237 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3238 | `	ph7_value *pOut,*pRow;` |
|   96 | 3239 | `	int iMode,iBase,iCol = 0;` |
|    - | 3240 | `	int bGroup,bUnique;` |
|    - | 3241 | `	sxi32 rc;` |
|   96 | 3242 | `	if( pSt == 0 ){` |
|  ! 0 | 3243 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3244 | `	}` |
|   96 | 3245 | `	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;` |
|   96 | 3246 | `	rc = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::fetchAll",1,"mode");` |
|   96 | 3247 | `	if( rc != PH7_OK ){` |
|    3 | 3248 | `		return rc;` |
|    - | 3249 | `	}` |
|   94 | 3250 | `	bGroup = (iMode & PDO_FETCH_GROUP) == PDO_FETCH_GROUP;` |
|   94 | 3251 | `	bUnique = (iMode & PDO_FETCH_UNIQUE) == PDO_FETCH_UNIQUE;` |
|   94 | 3252 | `	iBase = iMode & PDO_FETCH_MODE_MASK;` |
|   94 | 3253 | `	if( iBase == PDO_FETCH_DEFAULT ){` |
|   19 | 3254 | `		iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;` |
|   19 | 3255 | `		bGroup = bGroup \|\| (pSt->iFetchMode & PDO_FETCH_GROUP) == PDO_FETCH_GROUP;` |
|   19 | 3256 | `		bUnique = bUnique \|\| (pSt->iFetchMode & PDO_FETCH_UNIQUE) == PDO_FETCH_UNIQUE;` |
|   19 | 3257 | `		iCol = pSt->iFetchColumn;` |
|    9 | 3258 | `	}` |
|   94 | 3259 | `	if( iBase == PDO_FETCH_DEFAULT ){` |
|  ! 0 | 3260 | `		iBase = pSt->pConn->iDefaultFetch;` |
|  ! 0 | 3261 | `	}` |
|   94 | 3262 | `	if( iBase == PDO_FETCH_COLUMN ){` |
|   17 | 3263 | `		if( nArg > 2 ){` |
|  ! 0 | 3264 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3265 | `				"PDOStatement::fetchAll() expects exactly 2 arguments for the fetch "` |
|  ! 0 | 3266 | `				"mode provided, %d given",nArg);` |
|    - | 3267 | `		}` |
|   17 | 3268 | `		if( nArg > 1 ){` |
|    7 | 3269 | `			ph7_int64 iWant = ph7_value_to_int64(apArg[1]);` |
|    7 | 3270 | `			if( iWant < 0 ){` |
|    3 | 3271 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3272 | `					"PDOStatement::fetchAll(): Argument #2 must be greater than or "` |
|    - | 3273 | `					"equal to 0");` |
|    - | 3274 | `			}` |
|    5 | 3275 | `			iCol = (int)iWant;` |
|    2 | 3276 | `		}` |
|   15 | 3277 | `		if( iCol >= PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 3278 | `			return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    1 | 3279 | `		}` |
|   84 | 3280 | `	}else if( iBase == PDO_FETCH_CLASS ){` |
|   21 | 3281 | `		if( nArg > 3 ){` |
|  ! 0 | 3282 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3283 | `				"PDOStatement::fetchAll() expects at most 3 arguments for the fetch "` |
|  ! 0 | 3284 | `				"mode provided, %d given",nArg);` |
|    1 | 3285 | `		}` |
|   68 | 3286 | `	}else if( iBase == PDO_FETCH_LAZY ){` |
|    5 | 3287 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3288 | `			"PDOStatement::fetchAll(): Argument #1 ($mode) PDO::FETCH_LAZY cannot be "` |
|    - | 3289 | `			"used with PDOStatement::fetchAll()");` |
|   54 | 3290 | `	}else if( iBase == PDO_FETCH_FUNC ){` |
|    7 | 3291 | `		if( nArg != 2 ){` |
|    4 | 3292 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3293 | `				"PDOStatement::fetchAll() expects exactly 2 argument for "` |
|    1 | 3294 | `				"PDO::FETCH_FUNC, %d given",nArg);` |
|    - | 3295 | `		}` |
|    5 | 3296 | `		if( !ph7_value_is_callable(apArg[1]) ){` |
|    - | 3297 | `			/* php checks the callable BEFORE the first row, so an unusable one` |
|    - | 3298 | `			 * is a TypeError from PDO and never the engine's own` |
|    - | 3299 | `			 * "Call to undefined function" from inside the walk */` |
|    3 | 3300 | `			int nName = 0;` |
|    3 | 3301 | `			const char *zName = ph7_value_to_string(apArg[1],&nName);` |
|    4 | 3302 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    1 | 3303 | `				"function \"%.*s\" not found or invalid function name",nName,zName);` |
|    1 | 3304 | `		}` |
|   49 | 3305 | `	}else if( nArg > 1 ){` |
|    4 | 3306 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3307 | `			"PDOStatement::fetchAll() expects exactly 1 argument for the fetch mode "` |
|    1 | 3308 | `			"provided, %d given",nArg);` |
|    - | 3309 | `	}` |
|   80 | 3310 | `	if( iBase == PDO_FETCH_KEY_PAIR && PH7_PdoSqliteColumnCount(pSt) != 2 ){` |
|    5 | 3311 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 3312 | `			"SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires "` |
|    - | 3313 | `			"the result set to contain exactly 2 columns.");` |
|    - | 3314 | `	}` |
|   76 | 3315 | `	pOut = ph7_context_new_array(pCtx);` |
|   76 | 3316 | `	if( pOut == 0 ){` |
|  ! 0 | 3317 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3318 | `	}` |
|   76 | 3319 | `	if( PdoBoundColumnsBad(pSt) ){` |
|    3 | 3320 | `		return PdoBoundColumnsRefuse(pCtx,pSt,1);` |
|    - | 3321 | `	}` |
|  248 | 3322 | `	while( PdoStmtHasRow(pSt) ){` |
|  178 | 3323 | `		int iRowMode = iBase;` |
|  178 | 3324 | `		int iFirst = (bGroup \|\| bUnique) ? 1 : 0;` |
|  176 | 3325 | `		if( iBase == PDO_FETCH_COLUMN \|\| iBase == PDO_FETCH_KEY_PAIR` |
|  137 | 3326 | `		 \|\| iBase == PDO_FETCH_FUNC \|\| iBase == PDO_FETCH_BOUND ){` |
|   64 | 3327 | `			iRowMode = PDO_FETCH_NUM;` |
|   64 | 3328 | `			iFirst = 0;` |
|  146 | 3329 | `		}else if( iBase == PDO_FETCH_CLASS ){` |
|   43 | 3330 | `			iFirst = 0;` |
|   21 | 3331 | `		}` |
|  178 | 3332 | `		pRow = ph7_context_new_array(pCtx);` |
|  178 | 3333 | `		if( pRow == 0 ){` |
|  ! 0 | 3334 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 3335 | `		}` |
|  178 | 3336 | `		if( iFirst ){` |
|    - | 3337 | `			/* the FIRST column is the key and never joins the row */` |
|   31 | 3338 | `			ph7_value *pKeyRow = ph7_context_new_array(pCtx);` |
|    - | 3339 | `			ph7_value *pKey;` |
|   31 | 3340 | `			if( pKeyRow == 0 ){` |
|  ! 0 | 3341 | `				return PH7_ContextMemoryError(pCtx);` |
|    - | 3342 | `			}` |
|   31 | 3343 | `			if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pKeyRow,0) ){` |
|  ! 0 | 3344 | `				break;` |
|    - | 3345 | `			}` |
|   31 | 3346 | `			pKey = PdoArrayAtInt(pCtx->pVm,pKeyRow,0);` |
|    - | 3347 | `			/* the row itself is rebuilt from the SECOND column on; the cursor` |
|    - | 3348 | `			 * has not moved, so this reads the same sqlite row again */` |
|   31 | 3349 | `			pSt->bRowPending = 1;` |
|   31 | 3350 | `			if( !PdoStmtRowFrom(pCtx->pVm,pSt,iRowMode,pRow,1) ){` |
|  ! 0 | 3351 | `				break;` |
|    - | 3352 | `			}` |
|   31 | 3353 | `			if( bUnique ){` |
|   13 | 3354 | `				ph7_array_add_elem(pOut,pKey,pRow);` |
|    7 | 3355 | `			}else{` |
|   19 | 3356 | `				PdoGroupAppend(pCtx,pOut,pKey,pRow);` |
|    1 | 3357 | `			}` |
|  163 | 3358 | `		}else if( iBase == PDO_FETCH_CLASS ){` |
|    - | 3359 | `			/* every row is its own instance; the class and its constructor` |
|    - | 3360 | `			 * arguments are the same for all of them */` |
|    - | 3361 | `			ph7_class *pClass;` |
|    - | 3362 | `			ph7_value sObj;` |
|    - | 3363 | `			sxi32 rcCls;` |
|   43 | 3364 | `			int iFirstCol = 0;` |
|   43 | 3365 | `			if( nArg > 1 ){` |
|   27 | 3366 | `				pClass = PdoResolveFetchClass(pCtx,apArg[1],FALSE,&rcCls);` |
|   30 | 3367 | `			}else if( iMode & PDO_FETCH_CLASSTYPE ){` |
|   13 | 3368 | `				ph7_value *pHead = ph7_context_new_array(pCtx);` |
|   13 | 3369 | `				if( pHead == 0 ){` |
|  ! 0 | 3370 | `					return PH7_ContextMemoryError(pCtx);` |
|    - | 3371 | `				}` |
|   13 | 3372 | `				if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pHead,0) ){` |
|  ! 0 | 3373 | `					break;` |
|    - | 3374 | `				}` |
|   13 | 3375 | `				pSt->bRowPending = 1;` |
|   13 | 3376 | `				pClass = PdoClassTypeClass(pCtx,PdoArrayAtInt(pCtx->pVm,pHead,0));` |
|   13 | 3377 | `				rcCls = PH7_OK;` |
|   13 | 3378 | `				iFirstCol = 1;` |
|   11 | 3379 | `			}else if( pSt->zFetchClass ){` |
|    - | 3380 | `				ph7_value sName;` |
|    - | 3381 | `				SyString sStr;` |
|  ! 0 | 3382 | `				SyStringInitFromBuf(&sStr,pSt->zFetchClass,pSt->nFetchClass);` |
|  ! 0 | 3383 | `				PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|  ! 0 | 3384 | `				pClass = PdoResolveFetchClass(pCtx,&sName,FALSE,&rcCls);` |
|  ! 0 | 3385 | `				PH7_MemObjRelease(&sName);` |
|  ! 0 | 3386 | `			}else{` |
|    5 | 3387 | `				pClass = PH7_VmExtractClass(pCtx->pVm,"stdClass",sizeof("stdClass")-1,` |
|    - | 3388 | `					FALSE,0);` |
|    5 | 3389 | `				rcCls = PH7_OK;` |
|    - | 3390 | `			}` |
|   43 | 3391 | `			if( pClass == 0 ){` |
|    3 | 3392 | `				return rcCls;` |
|    - | 3393 | `			}` |
|   41 | 3394 | `			rcCls = PH7_VmCheckInstantiable(pCtx,pClass);` |
|   41 | 3395 | `			if( rcCls != PH7_OK ){` |
|  ! 0 | 3396 | `				return rcCls;` |
|    - | 3397 | `			}` |
|   41 | 3398 | `			PH7_MemObjInit(pCtx->pVm,&sObj);` |
|   41 | 3399 | `			if( !PdoRowIntoObject(pCtx->pVm,pSt,pClass,nArg > 2 ? apArg[2] : 0,` |
|   40 | 3400 | `				(iMode & PDO_FETCH_PROPS_LATE) != 0,iFirstCol,&sObj) ){` |
|  ! 0 | 3401 | `				PH7_MemObjRelease(&sObj);` |
|  ! 0 | 3402 | `				break;` |
|    - | 3403 | `			}` |
|   41 | 3404 | `			ph7_array_add_elem(pOut,0,&sObj);` |
|   41 | 3405 | `			PH7_MemObjRelease(&sObj);` |
|  126 | 3406 | `		}else if( iBase == PDO_FETCH_BOUND ){` |
|    - | 3407 | `			/* the row goes into the BOUND VARIABLES, not into the result: the` |
|    - | 3408 | `			 * array collects one true per row and the caller reads the last` |
|    - | 3409 | `			 * row's values out of its own variables */` |
|    - | 3410 | `			ph7_value *pTrue;` |
|   11 | 3411 | `			PdoBoundColumnsForRow(pCtx->pVm,pSt);` |
|   11 | 3412 | `			pSt->bRowPending = 0;` |
|   11 | 3413 | `			pTrue = ph7_context_new_scalar(pCtx);` |
|   11 | 3414 | `			if( pTrue ){` |
|   11 | 3415 | `				ph7_value_bool(pTrue,1);` |
|   11 | 3416 | `				ph7_array_add_elem(pOut,0,pTrue);` |
|    6 | 3417 | `			}` |
|  101 | 3418 | `		}else if( !PdoStmtRow(pCtx->pVm,pSt,iRowMode,pRow) ){` |
|  ! 0 | 3419 | `			break;` |
|   96 | 3420 | `		}else if( iBase == PDO_FETCH_COLUMN ){` |
|   37 | 3421 | `			ph7_array_add_elem(pOut,0,PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)iCol));` |
|   78 | 3422 | `		}else if( iBase == PDO_FETCH_KEY_PAIR ){` |
|   17 | 3423 | `			ph7_array_add_elem(pOut,PdoArrayAtInt(pCtx->pVm,pRow,0),` |
|    5 | 3424 | `				PdoArrayAtInt(pCtx->pVm,pRow,1));` |
|   54 | 3425 | `		}else if( iBase == PDO_FETCH_FUNC ){` |
|    - | 3426 | `			ph7_value sRes;` |
|    - | 3427 | `			ph7_value *apCall[32];` |
|    7 | 3428 | `			int n,nCall = PH7_PdoSqliteColumnCount(pSt);` |
|    7 | 3429 | `			if( nCall > (int)SX_ARRAYSIZE(apCall) ){` |
|  ! 0 | 3430 | `				nCall = (int)SX_ARRAYSIZE(apCall);` |
|  ! 0 | 3431 | `			}` |
|   19 | 3432 | `			for( n = 0 ; n < nCall ; ++n ){` |
|   13 | 3433 | `				apCall[n] = PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)n);` |
|    7 | 3434 | `			}` |
|    7 | 3435 | `			PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    7 | 3436 | `			if( PH7_VmCallUserFunction(pCtx->pVm,apArg[1],nCall,apCall,&sRes) != SXRET_OK ){` |
|  ! 0 | 3437 | `				PH7_MemObjRelease(&sRes);` |
|  ! 0 | 3438 | `				return PH7_OK;   /* whatever the callable raised is already in flight */` |
|    - | 3439 | `			}` |
|    7 | 3440 | `			ph7_array_add_elem(pOut,0,&sRes);` |
|    7 | 3441 | `			PH7_MemObjRelease(&sRes);` |
|    4 | 3442 | `		}else{` |
|   43 | 3443 | `			ph7_array_add_elem(pOut,0,pRow);` |
|    - | 3444 | `		}` |
|  176 | 3445 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 3446 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 3447 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchAll");` |
|    - | 3448 | `		}` |
|    2 | 3449 | `	}` |
|   72 | 3450 | `	PdoStmtOk(pSt);` |
|   72 | 3451 | `	ph7_result_value(pCtx,pOut);` |
|   72 | 3452 | `	return PH7_OK;` |
|   49 | 3453 | `}` |
|    - | 3454 | `/*` |
|    - | 3455 | ` * PDOStatement::columnCount(): int` |
|    - | 3456 | ` *` |
|    - | 3457 | ` * 0 for a statement that returns no rows -- and also for one that has been` |
|    - | 3458 | ` * PREPARED but not yet run, even though sqlite already knows the count from` |
|    - | 3459 | ` * the compile. php only publishes it once the statement has executed, so` |
|    - | 3460 | `` * `prepare('SELECT 1')->columnCount()` is 0 and not 1.`` |
|    - | 3461 | ` */` |
|    4 | 3462 | `static int vm_builtin_PDOStatement_columnCount(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3463 | `{` |
|    5 | 3464 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 3465 | `	SXUNUSED(nArg);` |
|    2 | 3466 | `	SXUNUSED(apArg);` |
|    5 | 3467 | `	if( pSt == 0 ){` |
|  ! 0 | 3468 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3469 | `	}` |
|    5 | 3470 | `	ph7_result_int(pCtx,pSt->bExecuted ? PH7_PdoSqliteColumnCount(pSt) : 0);` |
|    5 | 3471 | `	return PH7_OK;` |
|    3 | 3472 | `}` |
|    - | 3473 | `/*` |
|    - | 3474 | ` * PDOStatement::debugDumpParams(): ?bool` |
|    - | 3475 | ` *` |
|    - | 3476 | ` * php's own diagnostic dump, printed rather than returned (it answers null).` |
|    - | 3477 | ` * The bindings appear in the order they were MADE, and the two kinds report` |
|    - | 3478 | ` * differently: a positional one carries its 0-based paramno and an empty name,` |
|    - | 3479 | ` * a named one carries paramno -1 and the name WITH its colon. Both lengths are` |
|    - | 3480 | `` * printed in brackets, php's `[%d]` shape.`` |
|    - | 3481 | ` */` |
|    4 | 3482 | `static int vm_builtin_PDOStatement_debugDumpParams(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3483 | `{` |
|    5 | 3484 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3485 | `	phl_pdo_bind *pB;` |
|    5 | 3486 | `	const char *zSql = "";` |
|    5 | 3487 | `	int nSql = 0,nBind = 0;` |
|    - | 3488 | `	ph7_value *pQuery;` |
|    2 | 3489 | `	SXUNUSED(nArg);` |
|    2 | 3490 | `	SXUNUSED(apArg);` |
|    5 | 3491 | `	if( pSt == 0 ){` |
|  ! 0 | 3492 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3493 | `	}` |
|    5 | 3494 | `	pQuery = pSt->pOwner ? PH7_NativeAttr(pSt->pOwner,"queryString") : 0;` |
|    5 | 3495 | `	if( pQuery ){` |
|    5 | 3496 | `		zSql = ph7_value_to_string(pQuery,&nSql);` |
|    2 | 3497 | `	}` |
|    9 | 3498 | `	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){` |
|    5 | 3499 | `		++nBind;` |
|    3 | 3500 | `	}` |
|    5 | 3501 | `	ph7_context_output_format(pCtx,"SQL: [%d] %.*s\n",nSql,nSql,zSql);` |
|    5 | 3502 | `	ph7_context_output_format(pCtx,"Params:  %d\n",nBind);` |
|    - | 3503 | `	/* the list is built by prepending, so walking it backwards is what puts` |
|    - | 3504 | `	 * the bindings back in the order the script made them */` |
|    - | 3505 | `	{` |
|    - | 3506 | `		phl_pdo_bind *apBind[64];` |
|    5 | 3507 | `		int n = 0,i;` |
|    9 | 3508 | `		for( pB = pSt->pBinds ; pB && n < (int)SX_ARRAYSIZE(apBind) ; pB = pB->pNext ){` |
|    5 | 3509 | `			apBind[n++] = pB;` |
|    3 | 3510 | `		}` |
|    9 | 3511 | `		for( i = n - 1 ; i >= 0 ; --i ){` |
|    5 | 3512 | `			pB = apBind[i];` |
|    5 | 3513 | `			if( pB->zName ){` |
|    4 | 3514 | `				ph7_context_output_format(pCtx,"Key: Name: [%d] %.*s\n",` |
|    1 | 3515 | `					pB->nName,pB->nName,pB->zName);` |
|    3 | 3516 | `				ph7_context_output_format(pCtx,"paramno=-1\n");` |
|    4 | 3517 | `				ph7_context_output_format(pCtx,"name=[%d] \"%.*s\"\n",` |
|    1 | 3518 | `					pB->nName,pB->nName,pB->zName);` |
|    2 | 3519 | `			}else{` |
|    3 | 3520 | `				ph7_context_output_format(pCtx,"Key: Position #%d:\n",pB->iPos - 1);` |
|    3 | 3521 | `				ph7_context_output_format(pCtx,"paramno=%d\n",pB->iPos - 1);` |
|    3 | 3522 | `				ph7_context_output_format(pCtx,"name=[0] \"\"\n");` |
|    - | 3523 | `			}` |
|    5 | 3524 | `			ph7_context_output_format(pCtx,"is_param=1\n");` |
|    5 | 3525 | `			ph7_context_output_format(pCtx,"param_type=%d\n",pB->iType & ~PDO_PARAM_FLAGS);` |
|    3 | 3526 | `		}` |
|    - | 3527 | `	}` |
|    5 | 3528 | `	ph7_result_null(pCtx);` |
|    5 | 3529 | `	return PH7_OK;` |
|    3 | 3530 | `}` |
|    - | 3531 | `/*` |
|    - | 3532 | ` * PDOStatement::getAttribute(int $name): mixed` |
|    - | 3533 | ` *` |
|    - | 3534 | ` * Two of the driver's attributes describe a STATEMENT rather than the` |
|    - | 3535 | ` * connection -- whether it only reads, and whether it is mid-walk -- and both` |
|    - | 3536 | ` * are sqlite's own answers about the compiled statement. Everything else is` |
|    - | 3537 | ` * the same IM001 refusal the connection gives.` |
|    - | 3538 | ` */` |
|    4 | 3539 | `static int vm_builtin_PDOStatement_getAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3540 | `{` |
|    5 | 3541 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3542 | `	ph7_int64 iAttr;` |
|    5 | 3543 | `	if( pSt == 0 ){` |
|  ! 0 | 3544 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3545 | `	}` |
|    5 | 3546 | `	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    5 | 3547 | `	if( iAttr == PDO_SQLITE_ATTR_READONLY_STATEMENT ){` |
|    3 | 3548 | `		ph7_result_bool(pCtx,PH7_PdoSqliteStmtReadonly(pSt));` |
|    3 | 3549 | `		return PH7_OK;` |
|    - | 3550 | `	}` |
|    3 | 3551 | `	if( iAttr == PDO_SQLITE_ATTR_BUSY_STATEMENT ){` |
|    3 | 3552 | `		ph7_result_bool(pCtx,PH7_PdoSqliteStmtBusy(pSt));` |
|    3 | 3553 | `		return PH7_OK;` |
|    - | 3554 | `	}` |
|  ! 0 | 3555 | `	ph7_result_null(pCtx);` |
|  ! 0 | 3556 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::getAttribute","IM001",` |
|    - | 3557 | `		"driver does not support that attribute");` |
|    3 | 3558 | `}` |
|    - | 3559 | `/*` |
|    - | 3560 | ` * PDOStatement::setAttribute(int $attribute, mixed $value): bool` |
|    - | 3561 | ` *` |
|    - | 3562 | ` * This driver carries no SETTABLE statement attribute at all, so every one of` |
|    - | 3563 | ` * them is the same IM001 refusal.` |
|    - | 3564 | ` */` |
|  ! 0 | 3565 | `static int vm_builtin_PDOStatement_setAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 3566 | `{` |
|  ! 0 | 3567 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|  ! 0 | 3568 | `	SXUNUSED(nArg);` |
|  ! 0 | 3569 | `	SXUNUSED(apArg);` |
|  ! 0 | 3570 | `	if( pSt == 0 ){` |
|  ! 0 | 3571 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3572 | `	}` |
|  ! 0 | 3573 | `	ph7_result_bool(pCtx,0);` |
|  ! 0 | 3574 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::setAttribute","IM001",` |
|    - | 3575 | `		"driver does not support that attribute");` |
|  ! 0 | 3576 | `}` |
|    - | 3577 | `/*` |
|    - | 3578 | ` * PDOStatement::rowCount(): int` |
|    - | 3579 | ` *` |
|    - | 3580 | ` * The number of rows a WRITE changed. It is not the size of a result set --` |
|    - | 3581 | ` * sqlite cannot know that without walking it -- so a SELECT answers 0, which` |
|    - | 3582 | ` * is php's answer and the reason its manual warns against this method.` |
|    - | 3583 | ` */` |
|    6 | 3584 | `static int vm_builtin_PDOStatement_rowCount(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3585 | `{` |
|    7 | 3586 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    3 | 3587 | `	SXUNUSED(nArg);` |
|    3 | 3588 | `	SXUNUSED(apArg);` |
|    7 | 3589 | `	if( pSt == 0 ){` |
|  ! 0 | 3590 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3591 | `	}` |
|    7 | 3592 | `	ph7_result_int64(pCtx,pSt->nChanges);` |
|    7 | 3593 | `	return PH7_OK;` |
|    4 | 3594 | `}` |
|    - | 3595 | `/*` |
|    - | 3596 | ` * PDOStatement::closeCursor(): bool` |
|    - | 3597 | ` *` |
|    - | 3598 | ` * Frees the rows a statement is still holding without discarding the statement` |
|    - | 3599 | ` * itself: php answers true and leaves the object reusable, and a fetch after` |
|    - | 3600 | ` * it answers false.` |
|    - | 3601 | ` */` |
|    4 | 3602 | `static int vm_builtin_PDOStatement_closeCursor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3603 | `{` |
|    5 | 3604 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 3605 | `	SXUNUSED(nArg);` |
|    2 | 3606 | `	SXUNUSED(apArg);` |
|    5 | 3607 | `	if( pSt == 0 ){` |
|  ! 0 | 3608 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3609 | `	}` |
|    5 | 3610 | `	if( pSt->pStmt ){` |
|    5 | 3611 | `		sqlite3_reset(pSt->pStmt);` |
|    2 | 3612 | `	}` |
|    5 | 3613 | `	pSt->bRowPending = 0;` |
|    5 | 3614 | `	pSt->bDone = 1;` |
|    - | 3615 | `	/* php frees the row's columns with the cursor, so a lazy object still in a` |
|    - | 3616 | `	 * variable answers null from here on. */` |
|    5 | 3617 | `	PdoStmtLazyClear(pSt);` |
|    5 | 3618 | `	ph7_result_bool(pCtx,1);` |
|    5 | 3619 | `	return PH7_OK;` |
|    3 | 3620 | `}` |
|    - | 3621 | `/* A statement reports its CONNECTION's error state; php keeps one per` |
|    - | 3622 | ` * statement, and every path that sets one sets both. */` |
|    2 | 3623 | `static int vm_builtin_PDOStatement_errorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3624 | `{` |
|    3 | 3625 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    1 | 3626 | `	SXUNUSED(nArg);` |
|    1 | 3627 | `	SXUNUSED(apArg);` |
|    3 | 3628 | `	if( pSt == 0 ){` |
|  ! 0 | 3629 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3630 | `	}` |
|    3 | 3631 | `	if( pSt->iErrState == PDO_ERR_NONE ){` |
|  ! 0 | 3632 | `		ph7_result_null(pCtx);` |
|  ! 0 | 3633 | `	}else{` |
|    3 | 3634 | `		ph7_result_string(pCtx,pSt->zSqlState,(int)SyStrlen(pSt->zSqlState));` |
|    - | 3635 | `	}` |
|    3 | 3636 | `	return PH7_OK;` |
|    2 | 3637 | `}` |
|    2 | 3638 | `static int vm_builtin_PDOStatement_errorInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3639 | `{` |
|    3 | 3640 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    1 | 3641 | `	SXUNUSED(nArg);` |
|    1 | 3642 | `	SXUNUSED(apArg);` |
|    3 | 3643 | `	if( pSt == 0 ){` |
|  ! 0 | 3644 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3645 | `	}` |
|    3 | 3646 | `	return PdoBuildErrorInfo(pCtx,pSt->pConn,pSt->iErrState,pSt->zSqlState);` |
|    2 | 3647 | `}` |
|    - | 3648 | `/*` |
|    - | 3649 | ` * The InternalIterator a foreach over a statement walks.  A statement is a` |
|    - | 3650 | ` * forward cursor, so REWIND does not rewind: it settles on whatever row is` |
|    - | 3651 | ` * pending, which is why a second foreach over the same statement walks nothing` |
|    - | 3652 | ` * at all rather than repeating the set.` |
|    - | 3653 | ` */` |
|  120 | 3654 | `static void PdoStmtIterSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 3655 | `{` |
|  121 | 3656 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|  121 | 3657 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(pSrc);` |
|    - | 3658 | `	ph7_value *pRow;` |
|  121 | 3659 | `	if( pSt == 0 \|\| !PdoStmtHasRow(pSt) ){` |
|   49 | 3660 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|   49 | 3661 | `		return;` |
|    - | 3662 | `	}` |
|    - | 3663 | `	{` |
|    - | 3664 | `		/* A foreach honours the statement's mode, the whole of it: php walks a` |
|    - | 3665 | `		 * LAZY statement with its one row object, a CLASS or INTO one with the` |
|    - | 3666 | `		 * objects those modes build, a COLUMN one with that column's value and` |
|    - | 3667 | ``		 * a BOUND one with `true` per row (the values having gone to the bound`` |
|    - | 3668 | `		 * variables). Only the four row SHAPES are what PdoStmtRow answers. */` |
|   73 | 3669 | `		int iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;` |
|    - | 3670 | `		ph7_value sCur;` |
|   73 | 3671 | `		int bHave = 0;` |
|   73 | 3672 | `		PH7_MemObjInit(pVm,&sCur);` |
|   73 | 3673 | `		if( iBase == PDO_FETCH_LAZY ){` |
|   15 | 3674 | `			ph7_class_instance *pLazy = PdoLazyRowFor(pVm,pSt);` |
|   15 | 3675 | `			if( pLazy ){` |
|   15 | 3676 | `				sCur.x.pOther = pLazy;` |
|   15 | 3677 | `				MemObjSetType(&sCur,MEMOBJ_OBJ);` |
|   15 | 3678 | `				bHave = 1;   /* the reference PdoLazyRowFor took is this value's */` |
|    8 | 3679 | `			}` |
|   66 | 3680 | `		}else if( iBase == PDO_FETCH_BOUND ){` |
|    - | 3681 | `			/* the row IS the bound variables: nothing else reads it, so the` |
|    - | 3682 | `			 * write happens here rather than inside a row build */` |
|    5 | 3683 | `			PdoBoundColumnsForRow(pVm,pSt);` |
|    5 | 3684 | `			pSt->bRowPending = 0;` |
|    5 | 3685 | `			ph7_value_bool(&sCur,1);` |
|    5 | 3686 | `			bHave = 1;` |
|   57 | 3687 | `		}else if( iBase == PDO_FETCH_COLUMN ){` |
|    5 | 3688 | `			ph7_value *pNumRow = ph7_new_array(pVm);` |
|    5 | 3689 | `			if( pNumRow ){` |
|    5 | 3690 | `				if( PdoStmtRowFrom(pVm,pSt,PDO_FETCH_NUM,pNumRow,0) ){` |
|    5 | 3691 | `					ph7_value *pOne = PdoArrayAtInt(pVm,pNumRow,(sxi64)pSt->iFetchColumn);` |
|    5 | 3692 | `					if( pOne ){` |
|    5 | 3693 | `						PH7_MemObjStore(pOne,&sCur);` |
|    2 | 3694 | `					}` |
|    5 | 3695 | `					bHave = 1;` |
|    2 | 3696 | `				}` |
|    5 | 3697 | `				ph7_release_value(pVm,pNumRow);` |
|    3 | 3698 | `			}` |
|   53 | 3699 | `		}else if( iBase == PDO_FETCH_CLASS \|\| iBase == PDO_FETCH_INTO ){` |
|   17 | 3700 | `			ph7_class *pClass = 0;` |
|   17 | 3701 | `			int iFirst = 0;` |
|   17 | 3702 | `			if( iBase == PDO_FETCH_INTO ){` |
|    5 | 3703 | `				if( pSt->pFetchInto ){` |
|    5 | 3704 | `					ph7_value *pRowVals = ph7_new_array(pVm);` |
|    5 | 3705 | `					if( pRowVals && PdoStmtRowFrom(pVm,pSt,PDO_FETCH_ASSOC,pRowVals,0) ){` |
|    5 | 3706 | `						PdoWriteRowProps(pVm,pSt->pFetchInto,pRowVals);` |
|    5 | 3707 | `						pSt->pFetchInto->iRef++;` |
|    5 | 3708 | `						sCur.x.pOther = pSt->pFetchInto;` |
|    5 | 3709 | `						MemObjSetType(&sCur,MEMOBJ_OBJ);` |
|    5 | 3710 | `						bHave = 1;` |
|    2 | 3711 | `					}` |
|    5 | 3712 | `					if( pRowVals ){` |
|    5 | 3713 | `						ph7_release_value(pVm,pRowVals);` |
|    2 | 3714 | `					}` |
|    2 | 3715 | `				}` |
|    3 | 3716 | `			}else{` |
|   13 | 3717 | `				if( pSt->iFetchMode & PDO_FETCH_CLASSTYPE ){` |
|    - | 3718 | `					/* the FIRST column names the class and leaves the row */` |
|    5 | 3719 | `					ph7_value *pHead = ph7_new_array(pVm);` |
|    5 | 3720 | `					if( pHead && PdoStmtRowFrom(pVm,pSt,PDO_FETCH_NUM,pHead,0) ){` |
|    5 | 3721 | `						pSt->bRowPending = 1;   /* the cursor has not moved */` |
|    5 | 3722 | `						pClass = PdoIterClassOf(pVm,PdoArrayAtInt(pVm,pHead,0));` |
|    5 | 3723 | `						iFirst = 1;` |
|    2 | 3724 | `					}` |
|    5 | 3725 | `					if( pHead ){` |
|    5 | 3726 | `						ph7_release_value(pVm,pHead);` |
|    3 | 3727 | `					}` |
|   11 | 3728 | `				}else if( pSt->zFetchClass ){` |
|   13 | 3729 | `					pClass = PH7_VmExtractClass(pVm,pSt->zFetchClass,` |
|    8 | 3730 | `						(sxu32)pSt->nFetchClass,FALSE,0);` |
|    4 | 3731 | `				}` |
|   19 | 3732 | `				if( pClass && PdoRowIntoObject(pVm,pSt,pClass,pSt->pFetchArgs,` |
|   12 | 3733 | `						(pSt->iFetchMode & PDO_FETCH_PROPS_LATE) != 0,iFirst,&sCur) ){` |
|   13 | 3734 | `					bHave = 1;` |
|    6 | 3735 | `				}` |
|    - | 3736 | `			}` |
|    8 | 3737 | `		}` |
|   73 | 3738 | `		if( bHave ){` |
|   58 | 3739 | `			PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,` |
|   19 | 3740 | `				(int)SyStrlen(PH7_NATIVE_IT_CUR),&sCur);` |
|   39 | 3741 | `			PH7_MemObjRelease(&sCur);` |
|   58 | 3742 | `			PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,` |
|   19 | 3743 | `				PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));` |
|   39 | 3744 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|   39 | 3745 | `			PdoStmtStep(pSt);` |
|   39 | 3746 | `			return;` |
|    - | 3747 | `		}` |
|   35 | 3748 | `		PH7_MemObjRelease(&sCur);` |
|   34 | 3749 | `		if( iBase != PDO_FETCH_ASSOC && iBase != PDO_FETCH_NUM && iBase != PDO_FETCH_BOTH` |
|   16 | 3750 | `		 && iBase != PDO_FETCH_OBJ && iBase != PDO_FETCH_NAMED ){` |
|    - | 3751 | `			/* a mode with nothing to hand out ends the walk */` |
|  ! 0 | 3752 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|  ! 0 | 3753 | `			return;` |
|    - | 3754 | `		}` |
|    - | 3755 | `	}` |
|   35 | 3756 | `	pRow = ph7_new_array(pVm);` |
|   35 | 3757 | `	if( pRow == 0 \|\| !PdoStmtRow(pVm,pSt,pSt->iFetchMode,pRow) ){` |
|  ! 0 | 3758 | `		if( pRow ){` |
|  ! 0 | 3759 | `			ph7_release_value(pVm,pRow);` |
|  ! 0 | 3760 | `		}` |
|  ! 0 | 3761 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|  ! 0 | 3762 | `		return;` |
|    - | 3763 | `	}` |
|   35 | 3764 | `	PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,(int)SyStrlen(PH7_NATIVE_IT_CUR),pRow);` |
|   35 | 3765 | `	ph7_release_value(pVm,pRow);` |
|   52 | 3766 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,` |
|   17 | 3767 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));` |
|   35 | 3768 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|   35 | 3769 | `	PdoStmtStep(pSt);` |
|   61 | 3770 | `}` |
|    - | 3771 | `/*` |
|    - | 3772 | ` * A rewind that does NOT rewind, and must not even re-read: the iterator is` |
|    - | 3773 | `` * built already positioned and `foreach` rewinds it again, so a settle here`` |
|    - | 3774 | ` * would swallow the first row. The AUX slot records that the first row has` |
|    - | 3775 | ` * been taken; every later rewind is a no-op, which is also what makes a SECOND` |
|    - | 3776 | ` * foreach over the same statement walk nothing at all -- php's answer, because` |
|    - | 3777 | ` * the cursor is forward-only and has nowhere to go back to.` |
|    - | 3778 | ` */` |
|   94 | 3779 | `static void PdoStmtIterRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 3780 | `{` |
|   95 | 3781 | `	if( PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_AUX) != 0 ){` |
|   47 | 3782 | `		return;` |
|    - | 3783 | `	}` |
|   49 | 3784 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_AUX,1);` |
|   49 | 3785 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|   49 | 3786 | `	PdoStmtIterSettle(&(*pVm),pIt);` |
|   48 | 3787 | `}` |
|   72 | 3788 | `static void PdoStmtIterNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 3789 | `{` |
|  109 | 3790 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|   72 | 3791 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|   73 | 3792 | `	PdoStmtIterSettle(&(*pVm),pIt);` |
|   73 | 3793 | `}` |
|    - | 3794 | `static const PH7_NativeIterVtab sPdoStmtIterVtab = {` |
|    - | 3795 | `	PdoStmtIterRewind, PdoStmtIterNext, 0, PdoStmtIterGuard };` |
|   62 | 3796 | `static int vm_builtin_PDOStatement_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3797 | `{` |
|   63 | 3798 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - | 3799 | `	ph7_class_instance *pIt;` |
|   31 | 3800 | `	SXUNUSED(nArg);` |
|   31 | 3801 | `	SXUNUSED(apArg);` |
|   63 | 3802 | `	if( pThis == 0 ){` |
|  ! 0 | 3803 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement::getIterator() needs a receiver");` |
|    - | 3804 | `	}` |
|   63 | 3805 | `	if( PdoStmtOfInstance(pThis) == 0 ){` |
|    - | 3806 | ``		/* A statement no driver built -- `new PDOStatement()` -- refuses the door`` |
|    - | 3807 | ``		 * the way every other method on one does, and `foreach` is that door: php`` |
|    - | 3808 | `		 * has nothing to iterate and says so instead of walking an empty set. */` |
|    5 | 3809 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3810 | `	}` |
|   59 | 3811 | `	if( PdoStmtWalkRefusal(pCtx,PdoStmtOfInstance(pThis)) ){` |
|    - | 3812 | ``		/* php refuses at the DOOR as well as at every step: `getIterator()` on a`` |
|    - | 3813 | `		 * statement it cannot walk raises there, before an iterator exists. */` |
|   11 | 3814 | `		return PH7_OK;` |
|    - | 3815 | `	}` |
|   49 | 3816 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|   49 | 3817 | `	if( pIt == 0 ){` |
|  ! 0 | 3818 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3819 | `	}` |
|   49 | 3820 | `	PH7_NativeResultObject(pCtx,pIt);` |
|   49 | 3821 | `	return PH7_OK;` |
|   32 | 3822 | `}` |
|    - | 3823 | `/*` |
|    - | 3824 | ` * Record one binding.  A name is kept as the script spelled it -- with or` |
|    - | 3825 | ` * without its colon -- because the resolution happens at execute(), when the` |
|    - | 3826 | ` * statement that knows the names exists.` |
|    - | 3827 | ` */` |
|   32 | 3828 | `static phl_pdo_bind * PdoBindAdd(phl_pdo_stmt *pSt,const char *zName,int nName,int iPos,` |
|    - | 3829 | `	int iType)` |
|    2 | 3830 | `{` |
|   34 | 3831 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
|    - | 3832 | `	phl_pdo_bind *pB;` |
|    - | 3833 | `	/* php REPLACES a binding for the same parameter rather than stacking one */` |
|   48 | 3834 | `	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){` |
|   29 | 3835 | `		if( zName ? (pB->zName && pB->nName == nName` |
|    1 | 3836 | `		             && SyMemcmp(pB->zName,zName,(sxu32)nName) == 0)` |
|   12 | 3837 | `		          : (pB->zName == 0 && pB->iPos == iPos) ){` |
|  ! 0 | 3838 | `			if( pB->pVal ){` |
|  ! 0 | 3839 | `				ph7_release_value(pVm,pB->pVal);` |
|  ! 0 | 3840 | `				pB->pVal = 0;` |
|  ! 0 | 3841 | `			}` |
|  ! 0 | 3842 | `			pB->iType = iType;` |
|  ! 0 | 3843 | `			pB->nSlot = SXU32_HIGH;` |
|  ! 0 | 3844 | `			return pB;` |
|    - | 3845 | `		}` |
|    9 | 3846 | `	}` |
|   34 | 3847 | `	pB = (phl_pdo_bind *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo_bind));` |
|   34 | 3848 | `	if( pB == 0 ){` |
|  ! 0 | 3849 | `		return 0;` |
|    - | 3850 | `	}` |
|   34 | 3851 | `	SyZero(pB,sizeof(phl_pdo_bind));` |
|   34 | 3852 | `	pB->iPos = iPos;` |
|   34 | 3853 | `	pB->iType = iType;` |
|   34 | 3854 | `	pB->nSlot = SXU32_HIGH;` |
|   34 | 3855 | `	if( zName && nName > 0 ){` |
|    6 | 3856 | `		pB->zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nName + 1);` |
|    6 | 3857 | `		if( pB->zName == 0 ){` |
|  ! 0 | 3858 | `			SyMemBackendFree(&pVm->sAllocator,pB);` |
|  ! 0 | 3859 | `			return 0;` |
|    - | 3860 | `		}` |
|    6 | 3861 | `		SyMemcpy(zName,pB->zName,(sxu32)nName);` |
|    6 | 3862 | `		pB->zName[nName] = 0;` |
|    6 | 3863 | `		pB->nName = nName;` |
|    2 | 3864 | `	}` |
|   34 | 3865 | `	pB->pNext = pSt->pBinds;` |
|   34 | 3866 | `	pSt->pBinds = pB;` |
|   34 | 3867 | `	return pB;` |
|   18 | 3868 | `}` |
|    - | 3869 | `/*` |
|    - | 3870 | ` * The shared body of bindValue() and bindParam(): they differ only in WHEN the` |
|    - | 3871 | ` * value is read. Argument #1 is a name or a 1-based position, and php refuses` |
|    - | 3872 | ` * position 0 by ValueError before the statement is consulted at all.` |
|    - | 3873 | ` */` |
|   34 | 3874 | `static int PdoBindArgument(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFn,` |
|    - | 3875 | `	int bByRef)` |
|    2 | 3876 | `{` |
|   36 | 3877 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3878 | `	phl_pdo_bind *pB;` |
|    - | 3879 | `	ph7_value *pKey;` |
|    - | 3880 | `	int iType;` |
|   36 | 3881 | `	if( pSt == 0 ){` |
|  ! 0 | 3882 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3883 | `	}` |
|   36 | 3884 | `	pKey = nArg > 0 ? apArg[0] : 0;` |
|   36 | 3885 | `	iType = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : PDO_PARAM_STR;` |
|   38 | 3886 | `	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){` |
|    6 | 3887 | `		int nName = 0;` |
|    6 | 3888 | `		const char *zName = ph7_value_to_string(pKey,&nName);` |
|    6 | 3889 | `		pB = PdoBindAdd(pSt,zName,nName,0,iType);` |
|    4 | 3890 | `	}else{` |
|   32 | 3891 | `		ph7_int64 iPos = pKey ? ph7_value_to_int64(pKey) : 0;` |
|   32 | 3892 | `		if( iPos < 1 ){` |
|    4 | 3893 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    1 | 3894 | `				"%s(): Argument #1 ($param) must be greater than or equal to 1",zFn);` |
|    - | 3895 | `		}` |
|   30 | 3896 | `		pB = PdoBindAdd(pSt,0,0,(int)iPos,iType);` |
|    - | 3897 | `	}` |
|   34 | 3898 | `	if( pB == 0 ){` |
|  ! 0 | 3899 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3900 | `	}` |
|   34 | 3901 | `	if( bByRef ){` |
|    - | 3902 | `		/* bindParam(): remember the caller's SLOT, so a write to that variable` |
|    - | 3903 | `		 * after this call is the value execute() runs with. The engine hands a` |
|    - | 3904 | `		 * by-reference argument as the caller's own memobj, and its index is` |
|    - | 3905 | `		 * how every other deferred read here finds it again. */` |
|    3 | 3906 | `		pB->nSlot = (nArg > 1 && apArg[1]) ? apArg[1]->nIdx : SXU32_HIGH;` |
|   33 | 3907 | `	}else if( nArg > 1 ){` |
|    - | 3908 | `		/* bindValue(): the statement takes its own copy now */` |
|   32 | 3909 | `		pB->pVal = ph7_new_scalar(pCtx->pVm);` |
|   32 | 3910 | `		if( pB->pVal == 0 ){` |
|  ! 0 | 3911 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 3912 | `		}` |
|   32 | 3913 | `		PH7_MemObjStore(apArg[1],pB->pVal);` |
|   15 | 3914 | `	}` |
|   34 | 3915 | `	ph7_result_bool(pCtx,1);` |
|   34 | 3916 | `	return PH7_OK;` |
|   19 | 3917 | `}` |
|   32 | 3918 | `static int vm_builtin_PDOStatement_bindValue(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3919 | `{` |
|   34 | 3920 | `	return PdoBindArgument(pCtx,nArg,apArg,"PDOStatement::bindValue",FALSE);` |
|    2 | 3921 | `}` |
|    2 | 3922 | `static int vm_builtin_PDOStatement_bindParam(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3923 | `{` |
|    3 | 3924 | `	return PdoBindArgument(pCtx,nArg,apArg,"PDOStatement::bindParam",TRUE);` |
|    1 | 3925 | `}` |
|    - | 3926 | `/* Bind one recorded parameter, resolving a name against the live statement. */` |
|   28 | 3927 | `static int PdoBindApply(ph7_vm *pVm,phl_pdo_stmt *pSt,phl_pdo_bind *pB)` |
|    1 | 3928 | `{` |
|   29 | 3929 | `	ph7_value *pVal = pB->pVal;` |
|   29 | 3930 | `	int iPos = pB->iPos;` |
|   29 | 3931 | `	if( pB->zName ){` |
|    3 | 3932 | `		iPos = PH7_PdoSqliteBindIndexOf(pSt,pB->zName,pB->nName);` |
|    1 | 3933 | `	}` |
|   29 | 3934 | `	if( pB->nSlot != SXU32_HIGH ){` |
|    - | 3935 | `		/* bindParam(): read the caller's variable NOW */` |
|    3 | 3936 | `		pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pB->nSlot);` |
|    1 | 3937 | `	}` |
|   29 | 3938 | `	return PH7_PdoSqliteBindAt(pSt,iPos,pB->iType,pVal);` |
|    1 | 3939 | `}` |
|    - | 3940 | `/*` |
|    - | 3941 | `` * execute()'s `?array $params`: php binds the array INSTEAD of whatever was`` |
|    - | 3942 | ` * recorded, an integer key naming a 1-based position (so element 0 is` |
|    - | 3943 | ` * parameter 1) and a string key naming a placeholder.` |
|    - | 3944 | ` */` |
|   26 | 3945 | `static int PdoBindFromArray(ph7_vm *pVm,phl_pdo_stmt *pSt,ph7_value *pArray)` |
|    1 | 3946 | `{` |
|    - | 3947 | `	ph7_hashmap *pMap;` |
|    - | 3948 | `	ph7_hashmap_node *pEntry;` |
|    - | 3949 | `	sxu32 n,nCount;` |
|   27 | 3950 | `	int rc = 1;` |
|   27 | 3951 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 3952 | `		return 1;` |
|    - | 3953 | `	}` |
|   27 | 3954 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|   27 | 3955 | `	nCount = pMap->nEntry;` |
|   27 | 3956 | `	pEntry = pMap->pFirst;` |
|   53 | 3957 | `	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 3958 | `		ph7_value sKey;` |
|   35 | 3959 | `		ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|    - | 3960 | `		int iPos;` |
|   35 | 3961 | `		PH7_MemObjInit(pVm,&sKey);` |
|   35 | 3962 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   35 | 3963 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   21 | 3964 | `			iPos = (int)sKey.x.iVal + 1;` |
|   11 | 3965 | `		}else{` |
|   15 | 3966 | `			int nName = 0;` |
|   15 | 3967 | `			const char *zName = ph7_value_to_string(&sKey,&nName);` |
|   15 | 3968 | `			iPos = PH7_PdoSqliteBindIndexOf(pSt,zName,nName);` |
|    - | 3969 | `		}` |
|   35 | 3970 | `		PH7_MemObjRelease(&sKey);` |
|    - | 3971 | `		/* php binds every element as a STRING unless the script said otherwise` |
|    - | 3972 | `		 * through bindValue(); a php null still binds as NULL. */` |
|   35 | 3973 | `		if( !PH7_PdoSqliteBindAt(pSt,iPos,PDO_PARAM_STR,pVal) ){` |
|    9 | 3974 | `			rc = 0;` |
|    9 | 3975 | `			break;` |
|    - | 3976 | `		}` |
|   14 | 3977 | `	}` |
|   27 | 3978 | `	return rc;` |
|   14 | 3979 | `}` |
|    - | 3980 | `/*` |
|    - | 3981 | ` * PDOStatement::execute(?array $params = null): bool` |
|    - | 3982 | ` *` |
|    - | 3983 | ` * Runs the statement from the start: the cursor is rewound, the previous run's` |
|    - | 3984 | ` * values are dropped, the parameters are bound and one step is taken -- the` |
|    - | 3985 | ` * same first step query() takes, so columnCount() and the first fetch() behave` |
|    - | 3986 | ` * identically whichever verb produced the statement.` |
|    - | 3987 | ` */` |
|   52 | 3988 | `static int vm_builtin_PDOStatement_execute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3989 | `{` |
|   54 | 3990 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|   54 | 3991 | `	int bOk = 1;` |
|   54 | 3992 | `	if( pSt == 0 ){` |
|  ! 0 | 3993 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3994 | `	}` |
|   54 | 3995 | `	PH7_PdoTouch(pSt->pConn);` |
|   54 | 3996 | `	PH7_PdoSqliteReset(pSt);` |
|   54 | 3997 | `	pSt->bRowPending = 0;` |
|   54 | 3998 | `	pSt->bDone = 0;` |
|   54 | 3999 | `	if( nArg > 0 && apArg[0] && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){` |
|   27 | 4000 | `		bOk = PdoBindFromArray(pCtx->pVm,pSt,apArg[0]);` |
|   14 | 4001 | `	}else{` |
|    - | 4002 | `		phl_pdo_bind *pB;` |
|   56 | 4003 | `		for( pB = pSt->pBinds ; pB && bOk ; pB = pB->pNext ){` |
|   29 | 4004 | `			bOk = PdoBindApply(pCtx->pVm,pSt,pB);` |
|   15 | 4005 | `		}` |
|    - | 4006 | `	}` |
|   54 | 4007 | `	if( !bOk ){` |
|    9 | 4008 | `		ph7_result_bool(pCtx,0);` |
|    9 | 4009 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|    9 | 4010 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::execute");` |
|    - | 4011 | `	}` |
|   46 | 4012 | `	pSt->bExecuted = 1;` |
|   46 | 4013 | `	if( PdoStmtStep(pSt) < 0 ){` |
|    3 | 4014 | `		ph7_result_bool(pCtx,0);` |
|    3 | 4015 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|    3 | 4016 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::execute");` |
|    - | 4017 | `	}` |
|   44 | 4018 | `	PdoStmtOk(pSt);` |
|   44 | 4019 | `	pSt->nChanges = PH7_PdoSqliteColumnCount(pSt) > 0` |
|   23 | 4020 | `		? 0 : PH7_PdoSqliteChanges(pSt->pConn);` |
|   44 | 4021 | `	ph7_result_bool(pCtx,1);` |
|   44 | 4022 | `	return PH7_OK;` |
|   28 | 4023 | `}` |
|    - | 4024 | `/*` |
|    - | 4025 | ` * The class query()/prepare() builds.  ATTR_STATEMENT_CLASS replaces` |
|    - | 4026 | ` * PDOStatement with a subclass of the script's own, and php builds THAT for` |
|    - | 4027 | ` * every statement the connection makes from then on.` |
|    - | 4028 | ` */` |
|    - | 4029 | `/*` |
|    - | 4030 | ` * php builds the statement OBJECT itself and then calls the class's own` |
|    - | 4031 | ` * constructor with the arguments ATTR_STATEMENT_CLASS was given -- and refuses` |
|    - | 4032 | ` * outright when there are arguments and no constructor to take them.` |
|    - | 4033 | ` */` |
|  920 | 4034 | `static sxi32 PdoStatementCtor(ph7_context *pCtx,phl_pdo *pConn,ph7_class *pClass,` |
|    - | 4035 | `	ph7_class_instance *pObj)` |
|    3 | 4036 | `{` |
|  923 | 4037 | `	ph7_class_method *pCons = PH7_ClassExtractMethod(pClass,"__construct",` |
|    - | 4038 | `		sizeof("__construct")-1);` |
|  923 | 4039 | `	if( pCons == 0 ){` |
|  915 | 4040 | `		if( pConn->pStmtArgs ){` |
|    3 | 4041 | `			return PH7_VmThrowException(pCtx,"Error",` |
|    - | 4042 | `				"User-supplied statement does not accept constructor arguments");` |
|    - | 4043 | `		}` |
|  913 | 4044 | `		return PH7_OK;` |
|    - | 4045 | `	}` |
|    9 | 4046 | `	PdoCallCtor(pCtx->pVm,pObj,pCons,pConn->pStmtArgs);` |
|    9 | 4047 | `	return PH7_OK;` |
|  463 | 4048 | `}` |
|  920 | 4049 | `static ph7_class * PdoStatementClass(ph7_context *pCtx,phl_pdo *pConn)` |
|    3 | 4050 | `{` |
|  923 | 4051 | `	if( pConn->zStmtClass ){` |
|   32 | 4052 | `		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,pConn->zStmtClass,` |
|   20 | 4053 | `			(sxu32)pConn->nStmtClass,FALSE,0);` |
|   22 | 4054 | `		if( pClass ){` |
|   22 | 4055 | `			return pClass;` |
|    - | 4056 | `		}` |
|  ! 0 | 4057 | `	}` |
|  903 | 4058 | `	return PH7_VmExtractClass(pCtx->pVm,"PDOStatement",sizeof("PDOStatement")-1,FALSE,0);` |
|  463 | 4059 | `}` |
|    - | 4060 | `/*` |
|    - | 4061 | ` * PDO::prepare(string $query, array $options = []): PDOStatement\|false` |
|    - | 4062 | ` *` |
|    - | 4063 | ` * Compiles without running. The options array is php's per-statement` |
|    - | 4064 | ` * attribute set; the sqlite driver carries none of the ones a script can put` |
|    - | 4065 | ` * there, and php ignores an unusable one rather than refusing the call.` |
|    - | 4066 | ` */` |
|   66 | 4067 | `static int vm_builtin_PDO_prepare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 4068 | `{` |
|   68 | 4069 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 4070 | `	phl_pdo_stmt *pSt;` |
|    - | 4071 | `	ph7_class *pClass;` |
|    - | 4072 | `	ph7_class_instance *pObj;` |
|    - | 4073 | `	const char *zSql;` |
|   68 | 4074 | `	int nSql = 0;` |
|   68 | 4075 | `	if( pConn == 0 ){` |
|  ! 0 | 4076 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4077 | `	}` |
|   68 | 4078 | `	PH7_PdoTouch(pConn);` |
|   68 | 4079 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|   68 | 4080 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 | 4081 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 4082 | `			"PDO::prepare(): Argument #1 ($query) must not be empty");` |
|    - | 4083 | `	}` |
|   66 | 4084 | `	pSt = PH7_PdoNewStmt(pConn);` |
|   66 | 4085 | `	if( pSt == 0 ){` |
|  ! 0 | 4086 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4087 | `	}` |
|   66 | 4088 | `	if( !PH7_PdoSqlitePrepare(pSt,zSql,nSql) ){` |
|    3 | 4089 | `		ph7_result_bool(pCtx,0);` |
|    3 | 4090 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::prepare");` |
|    - | 4091 | `	}` |
|   64 | 4092 | `	pClass = PdoStatementClass(pCtx,pConn);` |
|   64 | 4093 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|   64 | 4094 | `	if( pObj == 0 ){` |
|  ! 0 | 4095 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4096 | `	}` |
|   64 | 4097 | `	if( PdoStmtAttach(pObj,pSt) != 0 ){` |
|  ! 0 | 4098 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4099 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4100 | `	}` |
|   64 | 4101 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,"queryString",zSql,nSql);` |
|    - | 4102 | `	{` |
|   64 | 4103 | `		sxi32 rcCtor = PdoStatementCtor(pCtx,pConn,pClass,pObj);` |
|   64 | 4104 | `		if( rcCtor != PH7_OK ){` |
|  ! 0 | 4105 | `			PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4106 | `			return rcCtor;` |
|    - | 4107 | `		}` |
|    - | 4108 | `	}` |
|   64 | 4109 | `	PH7_NativeResultObject(pCtx,pObj);` |
|   64 | 4110 | `	return PH7_OK;` |
|   35 | 4111 | `}` |
|    - | 4112 | `/*` |
|    - | 4113 | ` * PDO::quote(string $string, int $type = PDO::PARAM_STR): string\|false` |
|    - | 4114 | ` *` |
|    - | 4115 | ` * sqlite's own quoting: single quotes around it, each embedded quote doubled.` |
|    - | 4116 | ` * A NUL byte has no spelling inside a sqlite literal at all, so php refuses` |
|    - | 4117 | ` * one -- with a bare sentence carrying no SQLSTATE, unlike every other` |
|    - | 4118 | ` * PDOException this driver raises.` |
|    - | 4119 | ` */` |
|   10 | 4120 | `static int vm_builtin_PDO_quote(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4121 | `{` |
|   11 | 4122 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 4123 | `	const char *zIn;` |
|   11 | 4124 | `	int nIn = 0,i;` |
|    - | 4125 | `	SyBlob sOut;` |
|   11 | 4126 | `	if( pConn == 0 ){` |
|  ! 0 | 4127 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4128 | `	}` |
|   11 | 4129 | `	PH7_PdoTouch(pConn);` |
|   11 | 4130 | `	zIn = nArg > 0 ? ph7_value_to_string(apArg[0],&nIn) : "";` |
|   31 | 4131 | `	for( i = 0 ; i < nIn ; ++i ){` |
|   23 | 4132 | `		if( zIn[i] == 0 ){` |
|    3 | 4133 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4134 | `				"SQLite PDO::quote does not support null bytes");` |
|    - | 4135 | `		}` |
|   11 | 4136 | `	}` |
|    9 | 4137 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    9 | 4138 | `	SyBlobAppend(&sOut,"'",1);` |
|   27 | 4139 | `	for( i = 0 ; i < nIn ; ++i ){` |
|   19 | 4140 | `		if( zIn[i] == '\'' ){` |
|    3 | 4141 | `			SyBlobAppend(&sOut,"'",1);` |
|    1 | 4142 | `		}` |
|   19 | 4143 | `		SyBlobAppend(&sOut,&zIn[i],1);` |
|   10 | 4144 | `	}` |
|    9 | 4145 | `	SyBlobAppend(&sOut,"'",1);` |
|    9 | 4146 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    9 | 4147 | `	SyBlobRelease(&sOut);` |
|    9 | 4148 | `	return PH7_OK;` |
|    6 | 4149 | `}` |
|    - | 4150 | `/*` |
|    - | 4151 | ` * PDO::query(string $query, ...): PDOStatement\|false` |
|    - | 4152 | ` *` |
|    - | 4153 | `` * Prepares and runs ONE statement -- what follows a `;` is compiled but never`` |
|    - | 4154 | ` * executed, unlike exec(), which runs them all.` |
|    - | 4155 | ` */` |
|  868 | 4156 | `static int vm_builtin_PDO_query(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 4157 | `{` |
|  871 | 4158 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 4159 | `	phl_pdo_stmt *pSt;` |
|    - | 4160 | `	ph7_class *pClass;` |
|    - | 4161 | `	ph7_class_instance *pObj;` |
|    - | 4162 | `	const char *zSql;` |
|  871 | 4163 | `	int nSql = 0;` |
|  871 | 4164 | `	if( pConn == 0 ){` |
|  ! 0 | 4165 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4166 | `	}` |
|  871 | 4167 | `	PH7_PdoTouch(pConn);` |
|  871 | 4168 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|  871 | 4169 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 | 4170 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 4171 | `			"PDO::query(): Argument #1 ($query) must not be empty");` |
|    - | 4172 | `	}` |
|  869 | 4173 | `	pSt = PH7_PdoNewStmt(pConn);` |
|  869 | 4174 | `	if( pSt == 0 ){` |
|  ! 0 | 4175 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4176 | `	}` |
|  869 | 4177 | `	if( !PH7_PdoSqlitePrepare(pSt,zSql,nSql) ){` |
|    7 | 4178 | `		ph7_result_bool(pCtx,0);` |
|    7 | 4179 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::query");` |
|    - | 4180 | `	}` |
|  863 | 4181 | `	pSt->bExecuted = 1;` |
|  863 | 4182 | `	if( PdoStmtStep(pSt) < 0 ){` |
|    3 | 4183 | `		ph7_result_bool(pCtx,0);` |
|    3 | 4184 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::query");` |
|    - | 4185 | `	}` |
|    - | 4186 | `	/* the change count is read once, here: a later statement on the same` |
|    - | 4187 | `	 * connection would otherwise move what this one reports */` |
|  861 | 4188 | `	pSt->nChanges = PH7_PdoSqliteColumnCount(pSt) > 0` |
|  432 | 4189 | `		? 0 : PH7_PdoSqliteChanges(pConn);` |
|  861 | 4190 | `	pClass = PdoStatementClass(pCtx,pConn);` |
|  861 | 4191 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|  861 | 4192 | `	if( pObj == 0 ){` |
|  ! 0 | 4193 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4194 | `	}` |
|  861 | 4195 | `	if( PdoStmtAttach(pObj,pSt) != 0 ){` |
|  ! 0 | 4196 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4197 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4198 | `	}` |
|  861 | 4199 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,"queryString",zSql,nSql);` |
|    - | 4200 | `	{` |
|  861 | 4201 | `		sxi32 rcCtor = PdoStatementCtor(pCtx,pConn,pClass,pObj);` |
|  861 | 4202 | `		if( rcCtor != PH7_OK ){` |
|    3 | 4203 | `			PH7_ClassInstanceUnref(pObj);` |
|    3 | 4204 | `			return rcCtor;` |
|    - | 4205 | `		}` |
|    - | 4206 | `	}` |
|  859 | 4207 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    - | 4208 | `		/* php's second argument IS setFetchMode(), run on the statement this` |
|    - | 4209 | `		 * call just built -- same screen, same per-mode arity, and diagnostics` |
|    - | 4210 | `		 * that count from PDO::query()'s own signature. A refusal leaves the` |
|    - | 4211 | `		 * statement behind (php's does too), so it is raised after the object` |
|    - | 4212 | `		 * exists rather than before the query runs. */` |
|  250 | 4213 | `		sxi32 rcMode = PdoSetupFetchMode(pCtx,pSt,nArg,apArg,2,"PDO::query","fetchMode");` |
|  250 | 4214 | `		if( rcMode != PH7_OK ){` |
|  195 | 4215 | `			PH7_ClassInstanceUnref(pObj);` |
|  195 | 4216 | `			return rcMode;` |
|    - | 4217 | `		}` |
|   27 | 4218 | `	}` |
|  665 | 4219 | `	PdoStmtOk(pSt);   /* it ran, and it ran cleanly */` |
|    - | 4220 | `	/* PH7_NativeResultObject takes the reference this call made: unref'ing` |
|    - | 4221 | `	 * again here frees the object the result slot is still holding. */` |
|  665 | 4222 | `	PH7_NativeResultObject(pCtx,pObj);` |
|  665 | 4223 | `	return PH7_OK;` |
|  437 | 4224 | `}` |
|    - | 4225 |  |
|    - | 4226 | `/* ------------------------------------------------------------------------` |
|    - | 4227 | ` * Transactions` |
|    - | 4228 | ` * ------------------------------------------------------------------------ */` |
|    - | 4229 | `/*` |
|    - | 4230 | ` * PDO::beginTransaction(): bool / commit() / rollBack() / inTransaction()` |
|    - | 4231 | ` *` |
|    - | 4232 | ` * Whether a transaction is open is sqlite's own autocommit flag and not a` |
|    - | 4233 | ` * count this driver keeps, so a BEGIN the script sent through exec() is` |
|    - | 4234 | ` * indistinguishable from beginTransaction() -- inTransaction() answers true` |
|    - | 4235 | ` * for it and a second beginTransaction() refuses.` |
|    - | 4236 | ` *` |
|    - | 4237 | ` * The three refusals are bare sentences with no SQLSTATE in front of them,` |
|    - | 4238 | ` * which is unlike every other PDOException the driver raises; and the four` |
|    - | 4239 | ` * verbs are the ones that do NOT clear the handle's error on entry.` |
|    - | 4240 | ` */` |
|   28 | 4241 | `static int PdoTxRun(ph7_context *pCtx,const char *zSql,const char *zFn)` |
|    1 | 4242 | `{` |
|   29 | 4243 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|   29 | 4244 | `	if( pConn == 0 ){` |
|  ! 0 | 4245 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4246 | `	}` |
|   29 | 4247 | `	if( PH7_PdoSqliteExec(pConn,zSql,(int)SyStrlen(zSql)) < 0 ){` |
|  ! 0 | 4248 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4249 | `		return PH7_PdoRaise(pCtx,pConn,zFn);` |
|    - | 4250 | `	}` |
|   29 | 4251 | `	ph7_result_bool(pCtx,1);` |
|   29 | 4252 | `	return PH7_OK;` |
|   15 | 4253 | `}` |
|   16 | 4254 | `static int vm_builtin_PDO_beginTransaction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4255 | `{` |
|   17 | 4256 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|   17 | 4257 | `	const char *zBegin = "BEGIN";` |
|    8 | 4258 | `	SXUNUSED(nArg);` |
|    8 | 4259 | `	SXUNUSED(apArg);` |
|   17 | 4260 | `	if( pConn == 0 ){` |
|  ! 0 | 4261 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4262 | `	}` |
|   17 | 4263 | `	if( PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 4264 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4265 | `			"There is already an active transaction");` |
|    - | 4266 | `	}` |
|    - | 4267 | `	/* which BEGIN, per Pdo\Sqlite::ATTR_TRANSACTION_MODE */` |
|   15 | 4268 | `	if( pConn->iTxMode == 1 ){` |
|    3 | 4269 | `		zBegin = "BEGIN IMMEDIATE";` |
|   14 | 4270 | `	}else if( pConn->iTxMode == 2 ){` |
|    3 | 4271 | `		zBegin = "BEGIN EXCLUSIVE";` |
|    1 | 4272 | `	}` |
|   15 | 4273 | `	return PdoTxRun(pCtx,zBegin,"PDO::beginTransaction");` |
|    9 | 4274 | `}` |
|   12 | 4275 | `static int vm_builtin_PDO_commit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4276 | `{` |
|   13 | 4277 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    6 | 4278 | `	SXUNUSED(nArg);` |
|    6 | 4279 | `	SXUNUSED(apArg);` |
|   13 | 4280 | `	if( pConn == 0 ){` |
|  ! 0 | 4281 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4282 | `	}` |
|   13 | 4283 | `	if( !PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 4284 | `		return PH7_VmThrowException(pCtx,"PDOException","There is no active transaction");` |
|    - | 4285 | `	}` |
|   11 | 4286 | `	return PdoTxRun(pCtx,"COMMIT","PDO::commit");` |
|    7 | 4287 | `}` |
|    6 | 4288 | `static int vm_builtin_PDO_rollBack(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4289 | `{` |
|    7 | 4290 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    3 | 4291 | `	SXUNUSED(nArg);` |
|    3 | 4292 | `	SXUNUSED(apArg);` |
|    7 | 4293 | `	if( pConn == 0 ){` |
|  ! 0 | 4294 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4295 | `	}` |
|    7 | 4296 | `	if( !PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 4297 | `		return PH7_VmThrowException(pCtx,"PDOException","There is no active transaction");` |
|    - | 4298 | `	}` |
|    5 | 4299 | `	return PdoTxRun(pCtx,"ROLLBACK","PDO::rollBack");` |
|    4 | 4300 | `}` |
|   10 | 4301 | `static int vm_builtin_PDO_inTransaction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4302 | `{` |
|   11 | 4303 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    5 | 4304 | `	SXUNUSED(nArg);` |
|    5 | 4305 | `	SXUNUSED(apArg);` |
|   11 | 4306 | `	if( pConn == 0 ){` |
|  ! 0 | 4307 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4308 | `	}` |
|   11 | 4309 | `	ph7_result_bool(pCtx,PH7_PdoSqliteInTransaction(pConn));` |
|   11 | 4310 | `	return PH7_OK;` |
|    6 | 4311 | `}` |
|    - | 4312 |  |
|    - | 4313 | `/* ------------------------------------------------------------------------` |
|    - | 4314 | ` * Connecting` |
|    - | 4315 | ` * ------------------------------------------------------------------------ */` |
|    - | 4316 | `/*` |
|    - | 4317 | `` * Apply the constructor's `?array $options`.  php walks it before the driver`` |
|    - | 4318 | ` * sees the handle, so an ATTR_ERRMODE in there is already in force when a` |
|    - | 4319 | ` * later failure is routed -- and a key no driver knows is ignored in silence.` |
|    - | 4320 | ` */` |
|   52 | 4321 | `static void PdoApplyOptions(phl_pdo *pConn,ph7_value *pOptions)` |
|    2 | 4322 | `{` |
|    - | 4323 | `	ph7_hashmap *pMap;` |
|    - | 4324 | `	ph7_hashmap_node *pEntry;` |
|    - | 4325 | `	sxu32 n,nCount;` |
|   54 | 4326 | `	if( pOptions == 0 \|\| (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 4327 | `		return;` |
|    - | 4328 | `	}` |
|   54 | 4329 | `	pMap = (ph7_hashmap *)pOptions->x.pOther;` |
|   54 | 4330 | `	nCount = pMap->nEntry;` |
|   54 | 4331 | `	pEntry = pMap->pFirst;` |
|  108 | 4332 | `	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 4333 | `		ph7_value sKey;` |
|    - | 4334 | `		ph7_value *pVal;` |
|    - | 4335 | `		ph7_int64 iKey;` |
|   56 | 4336 | `		if( pEntry->iType != HASHMAP_INT_NODE ){` |
|  ! 0 | 4337 | `			continue;  /* php ignores a string key here */` |
|    - | 4338 | `		}` |
|   56 | 4339 | `		PH7_MemObjInit(pConn->pVm,&sKey);` |
|   56 | 4340 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   56 | 4341 | `		iKey = sKey.x.iVal;` |
|   56 | 4342 | `		PH7_MemObjRelease(&sKey);` |
|   56 | 4343 | `		pVal = (ph7_value *)PH7_MemObjAt(&pConn->pVm->aMemObj,pEntry->nValIdx);` |
|   56 | 4344 | `		if( pVal == 0 ){` |
|  ! 0 | 4345 | `			continue;` |
|    - | 4346 | `		}` |
|   56 | 4347 | `		switch( iKey ){` |
|   24 | 4348 | `			case PDO_ATTR_ERRMODE:            pConn->iErrMode = (int)ph7_value_to_int64(pVal); break;` |
|    9 | 4349 | `			case PDO_ATTR_CASE:               pConn->iCase = (int)ph7_value_to_int64(pVal); break;` |
|    9 | 4350 | `			case PDO_ATTR_ORACLE_NULLS:       pConn->iOracleNulls = (int)ph7_value_to_int64(pVal); break;` |
|    9 | 4351 | `			case PDO_ATTR_DEFAULT_FETCH_MODE: pConn->iDefaultFetch = (int)ph7_value_to_int64(pVal); break;` |
|    3 | 4352 | `			case PDO_ATTR_STRINGIFY_FETCHES:  pConn->bStringify = ph7_value_to_bool(pVal); break;` |
|    - | 4353 | `			/* php remembers this one and reports it back, though a CLI process` |
|    - | 4354 | `			 * has no pool to keep the handle in. */` |
|    3 | 4355 | `			case PDO_ATTR_PERSISTENT:         pConn->bPersistent = ph7_value_to_bool(pVal); break;` |
|  ! 0 | 4356 | `			case PDO_SQLITE_ATTR_TRANSACTION_MODE: pConn->iTxMode = (int)ph7_value_to_int64(pVal); break;` |
|    - | 4357 | `			/* the OPEN flags are read here and used by the open itself, which is` |
|    - | 4358 | `			 * the only moment they mean anything */` |
|    3 | 4359 | `			case PDO_SQLITE_ATTR_OPEN_FLAGS: pConn->iOpenFlags = (int)ph7_value_to_int64(pVal); break;` |
|  ! 0 | 4360 | `			case PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES: pConn->bExtendedCodes = ph7_value_to_bool(pVal); break;` |
|    2 | 4361 | `			default: break;` |
|    - | 4362 | `		}` |
|   29 | 4363 | `	}` |
|   28 | 4364 | `}` |
|    - | 4365 | `/*` |
|    - | 4366 | `` * php's `uri:` DSN: the real DSN is the FIRST LINE of what that URI names, read`` |
|    - | 4367 | ` * through the ordinary stream layer -- so a php:// wrapper or a userland one is` |
|    - | 4368 | ` * a source too. The line is used verbatim, which is why one with leading` |
|    - | 4369 | `` * whitespace, or a second `uri:`, comes back as "could not find driver" rather`` |
|    - | 4370 | ` * than anything more specific: it is simply parsed as a driver name.` |
|    - | 4371 | ` *` |
|    - | 4372 | ` * Returns 0 when the URI could not be read (the caller words the refusal); the` |
|    - | 4373 | ` * open warning underneath it is the stream layer's own, attributed to` |
|    - | 4374 | ` * PDO::__construct the way php attributes it.` |
|    - | 4375 | ` */` |
|    8 | 4376 | `static int PdoResolveUriDsn(ph7_context *pCtx,const char *zUri,int nUri,SyBlob *pOut)` |
|    1 | 4377 | `{` |
|    - | 4378 | `	const ph7_io_stream *pStream;` |
|    - | 4379 | `	const char *zFile,*zWhole;` |
|    - | 4380 | `	void *pHandle;` |
|    - | 4381 | `	SyBlob sRaw;` |
|    - | 4382 | `	const char *zRaw;` |
|    - | 4383 | `	sxu32 n,nRaw;` |
|    9 | 4384 | `	int rc = 0;` |
|    9 | 4385 | `	if( nUri < 1 ){` |
|  ! 0 | 4386 | `		return 0;` |
|    - | 4387 | `	}` |
|    - | 4388 | `	/* the slice is not a C string, and the stream layer wants one */` |
|    9 | 4389 | `	SyBlobInit(&sRaw,&pCtx->pVm->sAllocator);` |
|    9 | 4390 | `	SyBlobAppend(&sRaw,zUri,(sxu32)nUri);` |
|    9 | 4391 | `	SyBlobAppend(&sRaw,"",1);` |
|    9 | 4392 | `	zFile = zWhole = (const char *)SyBlobData(&sRaw);` |
|    9 | 4393 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nUri);` |
|    9 | 4394 | `	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|    4 | 4395 | `		FALSE,0,FALSE,0,"PDO::__construct") : 0;` |
|    9 | 4396 | `	if( pHandle == 0 ){` |
|    - | 4397 | `		/* the device lookup advanced zFile past the wrapper prefix; php's warning` |
|    - | 4398 | ``		 * names the URI the SCRIPT wrote, `file:///nope` and not `/nope` */`` |
|    4 | 4399 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"PDO::__construct(%s): Failed to open stream: %s",` |
|    1 | 4400 | `			zWhole,"No such file or directory");` |
|    3 | 4401 | `		SyBlobRelease(&sRaw);` |
|    3 | 4402 | `		return 0;` |
|    - | 4403 | `	}` |
|    7 | 4404 | `	SyBlobReset(&sRaw);` |
|    7 | 4405 | `	if( PH7_StreamReadWholeFile(pHandle,pStream,&sRaw) == SXRET_OK ){` |
|    7 | 4406 | `		zRaw = (const char *)SyBlobData(&sRaw);` |
|    7 | 4407 | `		nRaw = SyBlobLength(&sRaw);` |
|    - | 4408 | `		/* php reads ONE line and keeps its terminator, so a file written with a` |
|    - | 4409 | `		 * trailing newline yields a DSN that ends in one -- which reaches sqlite` |
|    - | 4410 | ``		 * as part of the PATH. That is why `sqlite::memory:\n` opens a FILE of`` |
|    - | 4411 | `		 * that name rather than a memory database; the newline is not noise the` |
|    - | 4412 | `		 * driver trims, and trimming it here would answer differently. */` |
|  109 | 4413 | `		for( n = 0 ; n < nRaw && zRaw[n] != '\n' ; ++n ){}` |
|    7 | 4414 | `		if( n < nRaw ){` |
|  ! 0 | 4415 | `			++n;  /* the newline belongs to the line */` |
|  ! 0 | 4416 | `		}` |
|    7 | 4417 | `		if( n > 0 ){` |
|    7 | 4418 | `			SyBlobAppend(pOut,zRaw,n);` |
|    7 | 4419 | `			rc = 1;` |
|    3 | 4420 | `		}` |
|    3 | 4421 | `	}` |
|    7 | 4422 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    7 | 4423 | `	SyBlobRelease(&sRaw);` |
|    7 | 4424 | `	return rc;` |
|    5 | 4425 | `}` |
|    - | 4426 | `/*` |
|    - | 4427 | ` * The driver split, over a DSN that is already resolved.  php reads up to the` |
|    - | 4428 | `` * first `:` as the driver name and hands the rest to that driver; the name is`` |
|    - | 4429 | `` * matched case-SENSITIVELY, so `SQLITE:` is "could not find driver" rather`` |
|    - | 4430 | ` * than a connection, and a DSN with no colon at all is refused before any` |
|    - | 4431 | ` * driver is looked for.` |
|    - | 4432 | ` */` |
|  178 | 4433 | `static int PdoOpenParsed(ph7_context *pCtx,phl_pdo *pConn,const char *zDsn,int nDsn)` |
|    4 | 4434 | `{` |
|    - | 4435 | `	int nDriver;` |
| 1262 | 4436 | `	for( nDriver = 0 ; nDriver < nDsn && zDsn[nDriver] != ':' ; ++nDriver ){}` |
|  182 | 4437 | `	if( nDriver >= nDsn ){` |
|    - | 4438 | `		/* no colon: php refuses the ARGUMENT, not the driver */` |
|    5 | 4439 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4440 | `			"PDO::__construct(): Argument #1 ($dsn) must be a valid data source name");` |
|    - | 4441 | `	}` |
|  174 | 4442 | `	if( nDriver != (int)sizeof("sqlite")-1` |
|  173 | 4443 | `	 \|\| SyMemcmp(zDsn,"sqlite",sizeof("sqlite")-1) != 0 ){` |
|    - | 4444 | `		/* the scope policy scopes this build to one driver, so every other name -- and every` |
|    - | 4445 | `		 * other SPELLING of this one -- is what a php without that driver says. */` |
|   13 | 4446 | `		return PH7_VmThrowException(pCtx,"PDOException","could not find driver");` |
|    - | 4447 | `	}` |
|    - | 4448 | `	/* SQLITE_OPEN_URI is passed EXPLICITLY rather than left to the linked` |
|    - | 4449 | `	 * library's compile-time default: a Debian libsqlite3 is built with URI` |
|    - | 4450 | `` 	 * filenames on and a vcpkg one is not, so `sqlite:file::memory:?cache=shared` `` |
|    - | 4451 | `	 * opened a memory database on one platform and created a FILE of that name on` |
|    - | 4452 | `	 * the other. php's own sqlite has them on, so on is the answer everywhere. */` |
|    - | 4453 | `	{` |
|    - | 4454 | `		/* the script's own ATTR_OPEN_FLAGS replace the read-write default */` |
|  247 | 4455 | `		int iFlags = pConn->iOpenFlags` |
|    1 | 4456 | `			? pConn->iOpenFlags` |
|   81 | 4457 | `			: (SQLITE_OPEN_READWRITE\|SQLITE_OPEN_CREATE);` |
|  247 | 4458 | `		return PH7_PdoSqliteOpen(pCtx,pConn,zDsn + nDriver + 1,nDsn - nDriver - 1,` |
|   81 | 4459 | `			iFlags\|SQLITE_OPEN_URI);` |
|    - | 4460 | `	}` |
|   93 | 4461 | `}` |
|    - | 4462 | `/*` |
|    - | 4463 | `` * One DSN, resolved then split: php's `uri:` form is read first, and what it`` |
|    - | 4464 | ` * names replaces the DSN whole.` |
|    - | 4465 | ` */` |
|  180 | 4466 | `static int PdoOpenFromDsn(ph7_context *pCtx,phl_pdo *pConn,ph7_value *pDsn)` |
|    4 | 4467 | `{` |
|    - | 4468 | `	const char *zDsn;` |
|    - | 4469 | `	int nDsn,rc;` |
|    - | 4470 | `	SyBlob sResolved;` |
|  184 | 4471 | `	if( pDsn == 0 ){` |
|  ! 0 | 4472 | `		nDsn = 0;` |
|  ! 0 | 4473 | `		zDsn = "";` |
|  ! 0 | 4474 | `	}else{` |
|  184 | 4475 | `		zDsn = ph7_value_to_string(pDsn,&nDsn);` |
|    - | 4476 | `	}` |
|  184 | 4477 | `	SyBlobInit(&sResolved,&pCtx->pVm->sAllocator);` |
|  184 | 4478 | `	if( nDsn >= (int)sizeof("uri:")-1 && SyMemcmp(zDsn,"uri:",sizeof("uri:")-1) == 0 ){` |
|    9 | 4479 | `		if( !PdoResolveUriDsn(pCtx,zDsn + sizeof("uri:")-1,nDsn - ((int)sizeof("uri:")-1),` |
|    - | 4480 | `			&sResolved) ){` |
|    3 | 4481 | `			SyBlobRelease(&sResolved);` |
|    3 | 4482 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4483 | `				"PDO::__construct(): Argument #1 ($dsn) must be a valid data source URI");` |
|    - | 4484 | `		}` |
|    - | 4485 | `		/* the line replaces the DSN whole, and is NOT resolved again: a nested` |
|    - | 4486 | ``		 * `uri:` is read as a driver name, exactly as php reads it */`` |
|    7 | 4487 | `		zDsn = (const char *)SyBlobData(&sResolved);` |
|    7 | 4488 | `		nDsn = (int)SyBlobLength(&sResolved);` |
|    3 | 4489 | `	}` |
|  182 | 4490 | `	rc = PdoOpenParsed(pCtx,pConn,zDsn,nDsn);` |
|  182 | 4491 | `	SyBlobRelease(&sResolved);` |
|  182 | 4492 | `	return rc;` |
|   94 | 4493 | `}` |
|    - | 4494 | `/*` |
|    - | 4495 | ` * PDO::__construct(string $dsn, ?string $username = null, ?string $password = null,` |
|    - | 4496 | ` *                  ?array $options = null)` |
|    - | 4497 | ` *` |
|    - | 4498 | ` * The two credential arguments are the generic surface: sqlite has no user to` |
|    - | 4499 | ` * be, so php accepts and ignores them rather than refusing a portable call.` |
|    - | 4500 | ` */` |
|  160 | 4501 | `static int vm_builtin_PDO___construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 | 4502 | `{` |
|  164 | 4503 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - | 4504 | `	phl_pdo *pConn;` |
|    - | 4505 | `	int rc;` |
|  164 | 4506 | `	if( pThis == 0 ){` |
|  ! 0 | 4507 | `		return PH7_VmThrowException(pCtx,"Error","PDO::__construct() needs a receiver");` |
|    - | 4508 | `	}` |
|  164 | 4509 | `	pConn = PH7_PdoNewConn(pCtx->pVm);` |
|  164 | 4510 | `	if( pConn == 0 ){` |
|  ! 0 | 4511 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4512 | `	}` |
|  164 | 4513 | `	if( PdoAttach(pThis,pConn) != 0 ){` |
|  ! 0 | 4514 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4515 | `	}` |
|    - | 4516 | `	/* the options are read BEFORE the open, so an ATTR_ERRMODE they carry is` |
|    - | 4517 | `	 * already in force for everything that follows */` |
|  164 | 4518 | `	if( nArg > 3 ){` |
|   52 | 4519 | `		PdoApplyOptions(pConn,apArg[3]);` |
|   25 | 4520 | `	}` |
|  164 | 4521 | `	rc = PdoOpenFromDsn(pCtx,pConn,nArg > 0 ? apArg[0] : 0);` |
|  164 | 4522 | `	return rc;` |
|   84 | 4523 | `}` |
|    - | 4524 | `/*` |
|    - | 4525 | ` * static PDO::connect(string $dsn, ...): static` |
|    - | 4526 | ` *` |
|    - | 4527 | `` * php 8.4's replacement for `new PDO(...)`: same arguments, but the object it`` |
|    - | 4528 | `` * answers is the DRIVER's subclass -- `Pdo\Sqlite` here -- so the`` |
|    - | 4529 | ` * sqlite-specific verbs are callable on it without a cast. Called on a` |
|    - | 4530 | `` * subclass it answers that subclass, which is what `static` means.`` |
|    - | 4531 | ` */` |
|   20 | 4532 | `static int vm_builtin_PDO_connect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 4533 | `{` |
|   23 | 4534 | `	ph7_vm *pVm = pCtx->pVm;` |
|   23 | 4535 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|    - | 4536 | `	ph7_class_instance *pObj;` |
|    - | 4537 | `	phl_pdo *pConn;` |
|    - | 4538 | `	int rc;` |
|   23 | 4539 | `	if( pClass == 0 \|\| SyStrncmp(pClass->sName.zString,"PDO",sizeof("PDO")-1) == 0 ){` |
|    - | 4540 | `		/* PDO::connect() itself answers the driver's class, not PDO */` |
|    6 | 4541 | `		ph7_class *pDrv = PH7_VmExtractClass(&(*pVm),"Pdo\\Sqlite",` |
|    - | 4542 | `			sizeof("Pdo\\Sqlite")-1,FALSE,0);` |
|    6 | 4543 | `		if( pDrv ){` |
|    6 | 4544 | `			pClass = pDrv;` |
|    2 | 4545 | `		}` |
|    2 | 4546 | `	}` |
|   23 | 4547 | `	if( pClass == 0 ){` |
|  ! 0 | 4548 | `		return PH7_VmThrowException(pCtx,"Error","Pdo\\Sqlite is not available");` |
|    - | 4549 | `	}` |
|   23 | 4550 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|   23 | 4551 | `	if( pObj == 0 ){` |
|  ! 0 | 4552 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4553 | `	}` |
|   23 | 4554 | `	pConn = PH7_PdoNewConn(&(*pVm));` |
|   23 | 4555 | `	if( pConn == 0 \|\| PdoAttach(pObj,pConn) != 0 ){` |
|  ! 0 | 4556 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4557 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4558 | `	}` |
|   23 | 4559 | `	if( nArg > 3 ){` |
|    3 | 4560 | `		PdoApplyOptions(pConn,apArg[3]);` |
|    1 | 4561 | `	}` |
|   23 | 4562 | `	rc = PdoOpenFromDsn(pCtx,pConn,nArg > 0 ? apArg[0] : 0);` |
|   23 | 4563 | `	if( rc != PH7_OK ){` |
|    3 | 4564 | `		PH7_ClassInstanceUnref(pObj);` |
|    3 | 4565 | `		return rc;` |
|    - | 4566 | `	}` |
|   21 | 4567 | `	PH7_NativeResultObject(pCtx,pObj);` |
|   21 | 4568 | `	return PH7_OK;` |
|   13 | 4569 | `}` |
|    - | 4570 | `/*` |
|    - | 4571 | ` * The loaded-driver list, the one answer php's two spellings share.  php` |
|    - | 4572 | ` * answers the drivers its ext/pdo actually loaded, which is why an engine` |
|    - | 4573 | ` * with no driver at all answers [] -- here it is always ["sqlite"].` |
|    - | 4574 | ` */` |
|    8 | 4575 | `static int PdoDriverList(ph7_context *pCtx)` |
|    1 | 4576 | `{` |
|    - | 4577 | `	ph7_value *pArray, *pName;` |
|    9 | 4578 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 | 4579 | `	pName  = ph7_context_new_scalar(pCtx);` |
|    9 | 4580 | `	if( pArray == 0 \|\| pName == 0 ){` |
|  ! 0 | 4581 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 | 4582 | `		ph7_result_null(pCtx);` |
|  ! 0 | 4583 | `		return PH7_OK;` |
|    - | 4584 | `	}` |
|    9 | 4585 | `	ph7_value_string(pName,"sqlite",sizeof("sqlite")-1);` |
|    9 | 4586 | `	ph7_array_add_elem(pArray,0,pName);` |
|    9 | 4587 | `	ph7_result_value(pCtx,pArray);` |
|    9 | 4588 | `	return PH7_OK;` |
|    5 | 4589 | `}` |
|    - | 4590 | `/* PDO::getAvailableDrivers(): the static method spelling. */` |
|    4 | 4591 | `static int vm_builtin_PDO_getAvailableDrivers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4592 | `{` |
|    2 | 4593 | `	SXUNUSED(nArg);` |
|    2 | 4594 | `	SXUNUSED(apArg);` |
|    5 | 4595 | `	return PdoDriverList(pCtx);` |
|    1 | 4596 | `}` |
|    - | 4597 | `/*` |
|    - | 4598 | ` * pdo_drivers(): the PROCEDURAL spelling of the same list, and the only` |
|    - | 4599 | ` * FUNCTION ext/pdo declares.  php's two answers are the same array built by` |
|    - | 4600 | `` * the same C routine, so they are `===` to each other.`` |
|    - | 4601 | ` */` |
|    4 | 4602 | `static int vm_builtin_pdo_drivers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4603 | `{` |
|    2 | 4604 | `	SXUNUSED(nArg);` |
|    2 | 4605 | `	SXUNUSED(apArg);` |
|    5 | 4606 | `	return PdoDriverList(pCtx);` |
|    1 | 4607 | `}` |
|    - | 4608 |  |
|    - | 4609 | `/*` |
|    - | 4610 | ` * Install the PDO class library.  Called from PH7_VmInit inside the` |
|    - | 4611 | ` * bCompilingBuiltin window; vm_pdo_sqlite.c's installer runs right after and` |
|    - | 4612 | `` * needs PDO to already be mounted (it is the parent of `Pdo\Sqlite`).`` |
|    - | 4613 | ` */` |
| 7925 | 4614 | `PH7_PRIVATE sxi32 PH7_VmInstallPdo(ph7_vm *pVm)` |
|    5 | 4615 | `{` |
|    - | 4616 | `	/* php's own constant values, in its own declaration order. The seven` |
|    - | 4617 | `	 * deprecated PDO::SQLITE_* rows php still carries are absent by the scope policy; their` |
|    - | 4618 | `	 * successors are declared on Pdo\Sqlite (vm_pdo_sqlite.c). */` |
|    - | 4619 | `#define PDO_INT_CONST(NAME,VALUE) \` |
|    - | 4620 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (ph7_int64)(VALUE), 0, 0.0 }` |
|    - | 4621 | `	static const PH7_NativeConstDef aPdoConst[] = {` |
|    - | 4622 | `		PDO_INT_CONST("PARAM_NULL",             0),` |
|    - | 4623 | `		PDO_INT_CONST("PARAM_BOOL",             5),` |
|    - | 4624 | `		PDO_INT_CONST("PARAM_INT",              1),` |
|    - | 4625 | `		PDO_INT_CONST("PARAM_STR",              2),` |
|    - | 4626 | `		PDO_INT_CONST("PARAM_LOB",              3),` |
|    - | 4627 | `		PDO_INT_CONST("PARAM_STMT",             4),` |
|    - | 4628 | `		PDO_INT_CONST("PARAM_INPUT_OUTPUT",     2147483648LL),` |
|    - | 4629 | `		PDO_INT_CONST("PARAM_STR_NATL",         1073741824LL),` |
|    - | 4630 | `		PDO_INT_CONST("PARAM_STR_CHAR",         536870912LL),` |
|    - | 4631 | `		PDO_INT_CONST("PARAM_EVT_ALLOC",        0),` |
|    - | 4632 | `		PDO_INT_CONST("PARAM_EVT_FREE",         1),` |
|    - | 4633 | `		PDO_INT_CONST("PARAM_EVT_EXEC_PRE",     2),` |
|    - | 4634 | `		PDO_INT_CONST("PARAM_EVT_EXEC_POST",    3),` |
|    - | 4635 | `		PDO_INT_CONST("PARAM_EVT_FETCH_PRE",    4),` |
|    - | 4636 | `		PDO_INT_CONST("PARAM_EVT_FETCH_POST",   5),` |
|    - | 4637 | `		PDO_INT_CONST("PARAM_EVT_NORMALIZE",    6),` |
|    - | 4638 | `		PDO_INT_CONST("FETCH_DEFAULT",          0),` |
|    - | 4639 | `		PDO_INT_CONST("FETCH_LAZY",             1),` |
|    - | 4640 | `		PDO_INT_CONST("FETCH_ASSOC",            2),` |
|    - | 4641 | `		PDO_INT_CONST("FETCH_NUM",              3),` |
|    - | 4642 | `		PDO_INT_CONST("FETCH_BOTH",             4),` |
|    - | 4643 | `		PDO_INT_CONST("FETCH_OBJ",              5),` |
|    - | 4644 | `		PDO_INT_CONST("FETCH_BOUND",            6),` |
|    - | 4645 | `		PDO_INT_CONST("FETCH_COLUMN",           7),` |
|    - | 4646 | `		PDO_INT_CONST("FETCH_CLASS",            8),` |
|    - | 4647 | `		PDO_INT_CONST("FETCH_INTO",             9),` |
|    - | 4648 | `		PDO_INT_CONST("FETCH_FUNC",            10),` |
|    - | 4649 | `		PDO_INT_CONST("FETCH_GROUP",           32),` |
|    - | 4650 | `		PDO_INT_CONST("FETCH_UNIQUE",          64),` |
|    - | 4651 | `		PDO_INT_CONST("FETCH_KEY_PAIR",        12),` |
|    - | 4652 | `		PDO_INT_CONST("FETCH_CLASSTYPE",      128),` |
|    - | 4653 | `		PDO_INT_CONST("FETCH_SERIALIZE",      512),` |
|    - | 4654 | `		PDO_INT_CONST("FETCH_PROPS_LATE",     256),` |
|    - | 4655 | `		PDO_INT_CONST("FETCH_NAMED",           11),` |
|    - | 4656 | `		PDO_INT_CONST("ATTR_AUTOCOMMIT",        0),` |
|    - | 4657 | `		PDO_INT_CONST("ATTR_PREFETCH",          1),` |
|    - | 4658 | `		PDO_INT_CONST("ATTR_TIMEOUT",           2),` |
|    - | 4659 | `		PDO_INT_CONST("ATTR_ERRMODE",           3),` |
|    - | 4660 | `		PDO_INT_CONST("ATTR_SERVER_VERSION",    4),` |
|    - | 4661 | `		PDO_INT_CONST("ATTR_CLIENT_VERSION",    5),` |
|    - | 4662 | `		PDO_INT_CONST("ATTR_SERVER_INFO",       6),` |
|    - | 4663 | `		PDO_INT_CONST("ATTR_CONNECTION_STATUS", 7),` |
|    - | 4664 | `		PDO_INT_CONST("ATTR_CASE",              8),` |
|    - | 4665 | `		PDO_INT_CONST("ATTR_CURSOR_NAME",       9),` |
|    - | 4666 | `		PDO_INT_CONST("ATTR_CURSOR",           10),` |
|    - | 4667 | `		PDO_INT_CONST("ATTR_ORACLE_NULLS",     11),` |
|    - | 4668 | `		PDO_INT_CONST("ATTR_PERSISTENT",       12),` |
|    - | 4669 | `		PDO_INT_CONST("ATTR_STATEMENT_CLASS",  13),` |
|    - | 4670 | `		PDO_INT_CONST("ATTR_FETCH_TABLE_NAMES",14),` |
|    - | 4671 | `		PDO_INT_CONST("ATTR_FETCH_CATALOG_NAMES",15),` |
|    - | 4672 | `		PDO_INT_CONST("ATTR_DRIVER_NAME",      16),` |
|    - | 4673 | `		PDO_INT_CONST("ATTR_STRINGIFY_FETCHES",17),` |
|    - | 4674 | `		PDO_INT_CONST("ATTR_MAX_COLUMN_LEN",   18),` |
|    - | 4675 | `		PDO_INT_CONST("ATTR_EMULATE_PREPARES", 20),` |
|    - | 4676 | `		PDO_INT_CONST("ATTR_DEFAULT_FETCH_MODE",19),` |
|    - | 4677 | `		PDO_INT_CONST("ATTR_DEFAULT_STR_PARAM",21),` |
|    - | 4678 | `		PDO_INT_CONST("ERRMODE_SILENT",         0),` |
|    - | 4679 | `		PDO_INT_CONST("ERRMODE_WARNING",        1),` |
|    - | 4680 | `		PDO_INT_CONST("ERRMODE_EXCEPTION",      2),` |
|    - | 4681 | `		PDO_INT_CONST("CASE_NATURAL",           0),` |
|    - | 4682 | `		PDO_INT_CONST("CASE_LOWER",             2),` |
|    - | 4683 | `		PDO_INT_CONST("CASE_UPPER",             1),` |
|    - | 4684 | `		PDO_INT_CONST("NULL_NATURAL",           0),` |
|    - | 4685 | `		PDO_INT_CONST("NULL_EMPTY_STRING",      1),` |
|    - | 4686 | `		PDO_INT_CONST("NULL_TO_STRING",         2),` |
|    - | 4687 | `		{ "ERR_NONE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, "00000", 0.0 },` |
|    - | 4688 | `		PDO_INT_CONST("FETCH_ORI_NEXT",         0),` |
|    - | 4689 | `		PDO_INT_CONST("FETCH_ORI_PRIOR",        1),` |
|    - | 4690 | `		PDO_INT_CONST("FETCH_ORI_FIRST",        2),` |
|    - | 4691 | `		PDO_INT_CONST("FETCH_ORI_LAST",         3),` |
|    - | 4692 | `		PDO_INT_CONST("FETCH_ORI_ABS",          4),` |
|    - | 4693 | `		PDO_INT_CONST("FETCH_ORI_REL",          5),` |
|    - | 4694 | `		PDO_INT_CONST("CURSOR_FWDONLY",         0),` |
|    - | 4695 | `		PDO_INT_CONST("CURSOR_SCROLL",          1),` |
|    - | 4696 | `	};` |
|    - | 4697 | `	/* php's own signatures and its own stub ORDER: Reflection and` |
|    - | 4698 | `	 * get_class_methods() both answer declaration order, so the two engines` |
|    - | 4699 | `	 * must list one surface. Nearly every return type is TENTATIVE in php's` |
|    - | 4700 | ``	 * stubs (the leading `@`), which is a php-visible difference from a`` |
|    - | 4701 | `	 * declared one -- getReturnType() answers null for a tentative type. */` |
|    - | 4702 | `	static const PH7_NativeMethodDef aPdoMethod[] = {` |
|    - | 4703 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|    - | 4704 | `		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",` |
|    - | 4705 | `		  0, vm_builtin_PDO___construct },` |
|    - | 4706 | `		{ "connect", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|    - | 4707 | `		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",` |
|    - | 4708 | `		  "static", vm_builtin_PDO_connect },` |
|    - | 4709 | `		{ "beginTransaction", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_beginTransaction },` |
|    - | 4710 | `		{ "commit",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_commit },` |
|    - | 4711 | `		{ "errorCode",        PH7_MOD_PUBLIC, "", "@?string", vm_builtin_PDO_errorCode },` |
|    - | 4712 | `		{ "errorInfo",        PH7_MOD_PUBLIC, "", "@array", vm_builtin_PDO_errorInfo },` |
|    - | 4713 | `		{ "exec",             PH7_MOD_PUBLIC, "string $statement", "@int\|false",` |
|    - | 4714 | `		  vm_builtin_PDO_exec },` |
|    - | 4715 | `		{ "getAttribute",     PH7_MOD_PUBLIC, "int $attribute", "@mixed",` |
|    - | 4716 | `		  vm_builtin_PDO_getAttribute },` |
|    - | 4717 | `		{ "getAvailableDrivers", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "@array",` |
|    - | 4718 | `		  vm_builtin_PDO_getAvailableDrivers },` |
|    - | 4719 | `		{ "inTransaction",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_inTransaction },` |
|    - | 4720 | `		{ "lastInsertId",     PH7_MOD_PUBLIC, "?string $name = null", "@string\|false",` |
|    - | 4721 | `		  vm_builtin_PDO_lastInsertId },` |
|    - | 4722 | `		{ "prepare",          PH7_MOD_PUBLIC, "string $query, array $options = []",` |
|    - | 4723 | `		  "@PDOStatement\|false", vm_builtin_PDO_prepare },` |
|    - | 4724 | `		{ "query",            PH7_MOD_PUBLIC,` |
|    - | 4725 | `		  "string $query, ?int $fetchMode = null, mixed ...$fetchModeArgs",` |
|    - | 4726 | `		  "@PDOStatement\|false", vm_builtin_PDO_query },` |
|    - | 4727 | `		{ "quote",            PH7_MOD_PUBLIC, "string $string, int $type = PDO::PARAM_STR",` |
|    - | 4728 | `		  "@string\|false", vm_builtin_PDO_quote },` |
|    - | 4729 | `		{ "rollBack",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_rollBack },` |
|    - | 4730 | `		{ "setAttribute",     PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",` |
|    - | 4731 | `		  vm_builtin_PDO_setAttribute },` |
|    - | 4732 | `	};` |
|    - | 4733 | `	static const PH7_NativeMethodDef aStmtMethod[] = {` |
|    - | 4734 | `		{ "bindColumn",   PH7_MOD_PUBLIC,` |
|    - | 4735 | `		  "string\|int $column, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "` |
|    - | 4736 | `		  "mixed $driverOptions = null", "@bool", vm_builtin_PDOStatement_bindColumn },` |
|    - | 4737 | `		{ "bindParam",    PH7_MOD_PUBLIC,` |
|    - | 4738 | `		  "string\|int $param, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "` |
|    - | 4739 | `		  "mixed $driverOptions = null", "@bool", vm_builtin_PDOStatement_bindParam },` |
|    - | 4740 | `		{ "bindValue",    PH7_MOD_PUBLIC,` |
|    - | 4741 | `		  "string\|int $param, mixed $value, int $type = PDO::PARAM_STR", "@bool",` |
|    - | 4742 | `		  vm_builtin_PDOStatement_bindValue },` |
|    - | 4743 | `		{ "closeCursor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDOStatement_closeCursor },` |
|    - | 4744 | `		{ "columnCount",  PH7_MOD_PUBLIC, "", "@int", vm_builtin_PDOStatement_columnCount },` |
|    - | 4745 | `		{ "debugDumpParams", PH7_MOD_PUBLIC, "", "@?bool",` |
|    - | 4746 | `		  vm_builtin_PDOStatement_debugDumpParams },` |
|    - | 4747 | `		{ "errorCode",    PH7_MOD_PUBLIC, "", "@?string", vm_builtin_PDOStatement_errorCode },` |
|    - | 4748 | `		{ "errorInfo",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_PDOStatement_errorInfo },` |
|    - | 4749 | `		{ "execute",      PH7_MOD_PUBLIC, "?array $params = null", "@bool",` |
|    - | 4750 | `		  vm_builtin_PDOStatement_execute },` |
|    - | 4751 | `		{ "fetch",        PH7_MOD_PUBLIC,` |
|    - | 4752 | `		  "int $mode = PDO::FETCH_DEFAULT, int $cursorOrientation = PDO::FETCH_ORI_NEXT, "` |
|    - | 4753 | `		  "int $cursorOffset = 0", "@mixed", vm_builtin_PDOStatement_fetch },` |
|    - | 4754 | `		{ "fetchAll",     PH7_MOD_PUBLIC, "int $mode = PDO::FETCH_DEFAULT, mixed ...$args",` |
|    - | 4755 | `		  "@array", vm_builtin_PDOStatement_fetchAll },` |
|    - | 4756 | `		{ "fetchColumn",  PH7_MOD_PUBLIC, "int $column = 0", "@mixed",` |
|    - | 4757 | `		  vm_builtin_PDOStatement_fetchColumn },` |
|    - | 4758 | `		{ "fetchObject",  PH7_MOD_PUBLIC,` |
|    - | 4759 | `		  "?string $class = 'stdClass', array $constructorArgs = []", "@object\|false",` |
|    - | 4760 | `		  vm_builtin_PDOStatement_fetchObject },` |
|    - | 4761 | `		{ "getAttribute", PH7_MOD_PUBLIC, "int $name", "@mixed",` |
|    - | 4762 | `		  vm_builtin_PDOStatement_getAttribute },` |
|    - | 4763 | `		{ "getColumnMeta",PH7_MOD_PUBLIC, "int $column", "@array\|false",` |
|    - | 4764 | `		  vm_builtin_PDOStatement_getColumnMeta },` |
|    - | 4765 | `		{ "nextRowset",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDOStatement_nextRowset },` |
|    - | 4766 | `		{ "rowCount",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_PDOStatement_rowCount },` |
|    - | 4767 | `		{ "setAttribute", PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",` |
|    - | 4768 | `		  vm_builtin_PDOStatement_setAttribute },` |
|    - | 4769 | `		{ "setFetchMode", PH7_MOD_PUBLIC, "int $mode, mixed ...$args", "@true",` |
|    - | 4770 | `		  vm_builtin_PDOStatement_setFetchMode },` |
|    - | 4771 | `		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_PDOStatement_getIterator },` |
|    - | 4772 | `	};` |
|    - | 4773 | `	/* The one property php PRESENTS on a statement: var_dump of a PDOStatement` |
|    - | 4774 | ``	 * shows `queryString` and nothing else. It is typed and has no default --`` |
|    - | 4775 | ``	 * `new PDOStatement()` (which php allows) leaves it uninitialized. */`` |
|    - | 4776 | `	static const PH7_NativePropDef aStmtProp[] = {` |
|    - | 4777 | `		{ "queryString", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|    - | 4778 | `		/* the cursor, hidden the way the connection's handle is */` |
|    - | 4779 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 4780 | `	};` |
|    - | 4781 | `	/*` |
|    - | 4782 | ``	 * PDORow declares `public string $queryString;` and holds NO property at`` |
|    - | 4783 | `	 * all: the object's whole surface is its handlers (PdoRowProp/PdoRowDim),` |
|    - | 4784 | `	 * so the declaration is marked LAZY below and nothing ever materializes it.` |
|    - | 4785 | `	 * The two engine slots beside it are hidden the way every other handle is.` |
|    - | 4786 | `	 */` |
|    - | 4787 | `	static const PH7_NativePropDef aRowProp[] = {` |
|    - | 4788 | `		{ "queryString", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|    - | 4789 | `		{ PDOROW_RES,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 4790 | `		{ PDOROW_STMT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|    - | 4791 | `	};` |
|    - | 4792 | `	/* php redeclares Exception::$code UNTYPED here so a SQLSTATE -- a string` |
|    - | 4793 | `	 * like 'HY000' -- can live in it, and adds the driver's raw error triple. */` |
|    - | 4794 | `	static const PH7_NativePropDef aExcProp[] = {` |
|    - | 4795 | `		{ "code",      PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|    - | 4796 | `		{ "errorInfo", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?array" },` |
|    - | 4797 | `	};` |
|    - | 4798 | `	/* The connection handle: storage the class owns and NEVER presents -- php` |
|    - | 4799 | `	 * shows no property at all on a PDO, so the slot is hidden (which is what` |
|    - | 4800 | `	 * keeps it out of var_dump, (array), get_object_vars and Reflection). */` |
|    - | 4801 | `	static const PH7_NativePropDef aPdoProp[] = {` |
|    - | 4802 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 4803 | `	};` |
|    - | 4804 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|    - | 4805 | ``		/* Both handles refuse `clone` and `serialize`: php declares neither a`` |
|    - | 4806 | `		 * clone handler nor a serializer for them, so the copy would carry the` |
|    - | 4807 | `		 * same sqlite3 pointer in its hidden slot. */` |
|    - | 4808 | `		{ "PDO", 0, 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 4809 | `		  aPdoMethod, SX_ARRAYSIZE(aPdoMethod),` |
|    - | 4810 | `		  aPdoConst, SX_ARRAYSIZE(aPdoConst),` |
|    - | 4811 | `		  aPdoProp, SX_ARRAYSIZE(aPdoProp),` |
|    - | 4812 | `		  PdoInstanceRelease, 0, 0 },` |
|    - | 4813 | `		{ "PDOStatement", 0, "IteratorAggregate", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 4814 | `		  aStmtMethod, SX_ARRAYSIZE(aStmtMethod),` |
|    - | 4815 | `		  0, 0,` |
|    - | 4816 | `		  aStmtProp, SX_ARRAYSIZE(aStmtProp),` |
|    - | 4817 | `		  PdoStmtInstanceRelease, &sPdoStmtIterVtab, 0 },` |
|    - | 4818 | `		{ "PDOException", "RuntimeException", 0, 0,` |
|    - | 4819 | `		  0, 0, 0, 0,` |
|    - | 4820 | `		  aExcProp, SX_ARRAYSIZE(aExcProp),` |
|    - | 4821 | `		  0, 0, 0 },` |
|    - | 4822 | ``		/* FINAL, uncloneable, unserializable, and refusing `new` with php's own`` |
|    - | 4823 | `		 * sentence -- which is a PDOException here and an Error everywhere else,` |
|    - | 4824 | `		 * so the class carries the exception name beside the text. */` |
|    - | 4825 | `		{ "PDORow", 0, 0,` |
|    - | 4826 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 4827 | `		  0, 0, 0, 0,` |
|    - | 4828 | `		  aRowProp, SX_ARRAYSIZE(aRowProp),` |
|    - | 4829 | `		  PdoRowInstanceRelease, 0, PdoRowPresent }` |
|    - | 4830 | `	};` |
|    - | 4831 | `#undef PDO_INT_CONST` |
|    - | 4832 | `	sxi32 rc;` |
| 7930 | 4833 | `	pVm->pPdoConns = 0;` |
|    - | 4834 | `	/* ext/pdo declares exactly one function beside its classes. */` |
| 7930 | 4835 | `	ph7_create_function(&(*pVm),"pdo_drivers",vm_builtin_pdo_drivers,0);` |
| 7930 | 4836 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
| 7930 | 4837 | `	if( rc == SXRET_OK ){` |
|    - | 4838 | `		ph7_class *pRow;` |
|    - | 4839 | `		/* php's compare handler for an opaque handle, on all three of ext/pdo's.` |
|    - | 4840 | ``		 * php gives each of them `zend_objects_not_comparable`: no two connections,`` |
|    - | 4841 | ``		 * statements or rows are ever equal, `<=>` answers the uncomparable 1 from`` |
|    - | 4842 | `` 		 * either side, and every relational spelling is false -- `$row == $row` `` |
|    - | 4843 | `		 * alone is true, and that is the engine's identity shortcut answering` |
|    - | 4844 | ``		 * before any handler. Inherited, so `class MyPdo extends PDO` and`` |
|    - | 4845 | ``		 * `Pdo\Sqlite` get it the way php's handler table does. */`` |
| 7930 | 4846 | `		PH7_NativeClassInstallCmpHook(&(*pVm),"PDO",PH7_NativeCmpOpaqueHandle);` |
| 7930 | 4847 | `		PH7_NativeClassInstallCmpHook(&(*pVm),"PDOStatement",PH7_NativeCmpOpaqueHandle);` |
| 7930 | 4848 | `		pRow = PH7_VmExtractClass(&(*pVm),"PDORow",sizeof("PDORow")-1,FALSE,0);` |
| 7930 | 4849 | `		if( pRow ){` |
| 7930 | 4850 | `			pRow->zNewRefusal = "You may not create a PDORow manually";` |
| 7930 | 4851 | `			pRow->zNewRefusalClass = "PDOException";` |
| 7930 | 4852 | `			pRow->xDim = PdoRowDim;` |
| 7930 | 4853 | `			pRow->xCmp = PH7_NativeCmpOpaqueHandle;` |
| 3957 | 4854 | `		}` |
|    - | 4855 | `		/* The declaration php makes and the object never holds: marked LAZY, and` |
|    - | 4856 | `		 * nothing materializes it -- every write to this class is refused. */` |
| 7930 | 4857 | `		PH7_NativeClassMarkLazyProps(&(*pVm),"PDORow",0);` |
| 7930 | 4858 | `		PH7_NativeClassInstallPropHook(&(*pVm),"PDORow",PdoRowProp);` |
| 3957 | 4859 | `	}` |
| 7930 | 4860 | `	return rc;` |
|    5 | 4861 | `}` |
|    - | 4862 |  |
|    - | 4863 | `#else` |
|    - | 4864 | `/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */` |
|    - | 4865 | `typedef int vm_pdo_unused;` |
|    - | 4866 | `#endif /* PH7_ENABLE_SQLITE */` |
|    - | 4867 |  |
