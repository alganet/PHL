# src/ph7/vm_pdo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2609/2887 lines (90.37%)

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
|    - |   21 | ` * from the manual, and §10's non-deprecated rule removes part of it: php 8.5` |
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
|  176 |   49 | `PH7_PRIVATE phl_pdo * PH7_PdoNewConn(ph7_vm *pVm)` |
|    4 |   50 | `{` |
|  180 |   51 | `	phl_pdo *pConn = (phl_pdo *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo));` |
|  180 |   52 | `	if( pConn == 0 ){` |
|  ! 0 |   53 | `		return 0;` |
|    - |   54 | `	}` |
|  180 |   55 | `	SyZero(pConn,sizeof(phl_pdo));` |
|  180 |   56 | `	pConn->pVm = pVm;` |
|    - |   57 | `	/* php's defaults for a fresh sqlite handle: exceptions on, both column` |
|    - |   58 | `	 * shapes, no case folding, no null rewriting, native column types. */` |
|  180 |   59 | `	pConn->iErrMode = PDO_ERRMODE_EXCEPTION;` |
|  180 |   60 | `	pConn->iCase = PDO_CASE_NATURAL;` |
|  180 |   61 | `	pConn->iOracleNulls = PDO_NULL_NATURAL;` |
|  180 |   62 | `	pConn->iDefaultFetch = PDO_FETCH_BOTH;` |
|  180 |   63 | `	pConn->iErrState = PDO_ERR_NONE;` |
|  180 |   64 | `	pConn->pNext = (phl_pdo *)pVm->pPdoConns;` |
|  180 |   65 | `	pVm->pPdoConns = pConn;` |
|  180 |   66 | `	return pConn;` |
|   92 |   67 | `}` |
|  176 |   68 | `PH7_PRIVATE void PH7_PdoFreeConn(phl_pdo *pConn)` |
|    4 |   69 | `{` |
|    - |   70 | `	/* sqlite refuses to close a database that still has a live statement or an` |
|    - |   71 | `	 * open blob handle, so the cursors go first and the blobs beside them. */` |
|  180 |   72 | `	PdoStmtSweep(pConn);` |
|  180 |   73 | `	PH7_PdoSqliteBlobSweep(pConn);` |
|    - |   74 | `	{` |
|    - |   75 | `		/* the callbacks sqlite still points at; the close is what makes them` |
|    - |   76 | `		 * unreachable, so they are released after it below */` |
|  180 |   77 | `		phl_pdo_udf *pUdf = pConn->pUdfs;` |
|  206 |   78 | `		while( pUdf ){` |
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
|  180 |   92 | `		pConn->pUdfs = 0;` |
|    - |   93 | `	}` |
|  180 |   94 | `	PH7_PdoSqliteClose(pConn);` |
|  180 |   95 | `	if( pConn->zDrvMsg ){` |
|   15 |   96 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);` |
|   15 |   97 | `		pConn->zDrvMsg = 0;` |
|    7 |   98 | `	}` |
|  180 |   99 | `	if( pConn->pStmtArgs ){` |
|    3 |  100 | `		ph7_release_value(pConn->pVm,pConn->pStmtArgs);` |
|    3 |  101 | `		pConn->pStmtArgs = 0;` |
|    1 |  102 | `	}` |
|  180 |  103 | `	if( pConn->zStmtClass ){` |
|    8 |  104 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zStmtClass);` |
|    8 |  105 | `		pConn->zStmtClass = 0;` |
|    3 |  106 | `	}` |
|  180 |  107 | `}` |
|    - |  108 | `/*` |
|    - |  109 | ` * Free every registered connection.  Called from PH7_PdoVmReset (a reused VM --` |
|    - |  110 | ` * the -S server's -- must not answer the next request through the previous` |
|    - |  111 | ` * one's database) and from PH7_PdoVmRelease before the allocator that holds the` |
|    - |  112 | ` * shells is torn down.` |
|    - |  113 | ` */` |
| 4974 |  114 | `static void PdoVmSweep(ph7_vm *pVm)` |
|    5 |  115 | `{` |
| 4979 |  116 | `	phl_pdo *pConn = (phl_pdo *)pVm->pPdoConns;` |
| 5155 |  117 | `	while( pConn ){` |
|  180 |  118 | `		phl_pdo *pNext = pConn->pNext;` |
|  180 |  119 | `		PdoBlankSlot(pConn->pOwner);` |
|  180 |  120 | `		PH7_PdoFreeConn(pConn);` |
|  180 |  121 | `		SyMemBackendFree(&pVm->sAllocator,pConn);` |
|  180 |  122 | `		pConn = pNext;` |
|    4 |  123 | `	}` |
| 4979 |  124 | `	pVm->pPdoConns = 0;` |
| 4979 |  125 | `}` |
|   16 |  126 | `PH7_PRIVATE void PH7_PdoVmReset(ph7_vm *pVm)` |
|  ! 0 |  127 | `{` |
|   16 |  128 | `	PdoVmSweep(&(*pVm));` |
|   16 |  129 | `}` |
| 4958 |  130 | `PH7_PRIVATE void PH7_PdoVmRelease(ph7_vm *pVm)` |
|    5 |  131 | `{` |
| 4963 |  132 | `	PdoVmSweep(&(*pVm));` |
| 4963 |  133 | `}` |
|    - |  134 | `/*` |
|    - |  135 | ` * Blank the hidden slot of the object that holds a record we are about to` |
|    - |  136 | ` * free.  Without this the object outlives its record -- a PDOStatement whose` |
|    - |  137 | ` * connection was released first, or any handle alive at VM teardown -- and its` |
|    - |  138 | ` * own release reads freed memory to ask whether it still owns one. (ASan found` |
|    - |  139 | ` * exactly that; nothing in the ordinary build noticed.)` |
|    - |  140 | ` */` |
| 1130 |  141 | `static void PdoBlankSlot(ph7_class_instance *pOwner)` |
|    4 |  142 | `{` |
|    - |  143 | `	SyString sAttr;` |
|    - |  144 | `	ph7_value *pRes;` |
| 1134 |  145 | `	if( pOwner == 0 ){` |
|  944 |  146 | `		return;` |
|    - |  147 | `	}` |
|  193 |  148 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  193 |  149 | `	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);` |
|  193 |  150 | `	if( pRes ){` |
|  193 |  151 | `		PH7_MemObjRelease(pRes);` |
|  193 |  152 | `		MemObjSetType(pRes,MEMOBJ_NULL);` |
|   95 |  153 | `	}` |
|  569 |  154 | `}` |
|    - |  155 | ``/* The connection behind a `__res` slot value. */`` |
| 1660 |  156 | `static phl_pdo * PdoOfValue(ph7_value *pVal)` |
|    4 |  157 | `{` |
| 1664 |  158 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   37 |  159 | `		return 0;` |
|    - |  160 | `	}` |
| 1628 |  161 | `	return (phl_pdo *)ph7_value_to_resource(pVal);` |
|  834 |  162 | `}` |
|   50 |  163 | `PH7_PRIVATE phl_pdo * PH7_PdoConnOfInstance(ph7_class_instance *pThis)` |
|    1 |  164 | `{` |
|   51 |  165 | `	return PdoOfInstance(pThis);` |
|    1 |  166 | `}` |
| 1660 |  167 | `static phl_pdo * PdoOfInstance(ph7_class_instance *pThis)` |
|    4 |  168 | `{` |
|    - |  169 | `	SyString sAttr;` |
| 1664 |  170 | `	if( pThis == 0 ){` |
|  ! 0 |  171 | `		return 0;` |
|    - |  172 | `	}` |
| 1664 |  173 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
| 1664 |  174 | `	return PdoOfValue(PH7_ClassInstanceFetchAttr(pThis,&sAttr));` |
|  834 |  175 | `}` |
|    - |  176 | `/* Store one connection in the receiver's hidden slot. */` |
|  176 |  177 | `static int PdoAttach(ph7_class_instance *pThis,phl_pdo *pConn)` |
|    4 |  178 | `{` |
|    - |  179 | `	SyString sAttr;` |
|    - |  180 | `	ph7_value *pRes;` |
|  180 |  181 | `	if( pThis == 0 ){` |
|  ! 0 |  182 | `		return -1;` |
|    - |  183 | `	}` |
|  180 |  184 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  180 |  185 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  180 |  186 | `	if( pRes == 0 ){` |
|  ! 0 |  187 | `		return -1;` |
|    - |  188 | `	}` |
|  180 |  189 | `	PH7_MemObjRelease(pRes);` |
|  180 |  190 | `	pRes->x.pOther = pConn;` |
|  180 |  191 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|  180 |  192 | `	pConn->pOwner = pThis;` |
|  180 |  193 | `	return 0;` |
|   92 |  194 | `}` |
|    - |  195 | `/*` |
|    - |  196 | ` * The object is going away: close its database now rather than at VM reset, so` |
|    - |  197 | ` * a script that unsets its last reference releases the file lock there -- which` |
|    - |  198 | ` * is what php does, and what a test that unlinks the file afterwards needs.` |
|    - |  199 | ` * The shell stays on the registry (the sweep frees it) because the slot is` |
|    - |  200 | ` * still reachable while the instance is being torn down.` |
|    - |  201 | ` */` |
|  128 |  202 | `static void PdoInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    3 |  203 | `{` |
|  131 |  204 | `	phl_pdo *pConn = PdoOfInstance(pThis);` |
|   64 |  205 | `	SXUNUSED(pVm);` |
|  131 |  206 | `	if( pConn == 0 \|\| pConn->pOwner != pThis ){` |
|   37 |  207 | `		return;` |
|    - |  208 | `	}` |
|   95 |  209 | `	PdoStmtSweep(pConn);` |
|   95 |  210 | `	PH7_PdoSqliteBlobSweep(pConn);` |
|   95 |  211 | `	PH7_PdoSqliteClose(pConn);` |
|   95 |  212 | `	pConn->pOwner = 0;` |
|   67 |  213 | `}` |
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
|  928 |  224 | `PH7_PRIVATE phl_pdo_stmt * PH7_PdoNewStmt(phl_pdo *pConn)` |
|    3 |  225 | `{` |
|  931 |  226 | `	phl_pdo_stmt *pSt = (phl_pdo_stmt *)SyMemBackendAlloc(&pConn->pVm->sAllocator,` |
|    - |  227 | `		sizeof(phl_pdo_stmt));` |
|  931 |  228 | `	if( pSt == 0 ){` |
|  ! 0 |  229 | `		return 0;` |
|    - |  230 | `	}` |
|  931 |  231 | `	SyZero(pSt,sizeof(phl_pdo_stmt));` |
|  931 |  232 | `	pSt->pConn = pConn;` |
|  931 |  233 | `	pSt->iFetchMode = pConn->iDefaultFetch;` |
|  931 |  234 | `	pSt->iErrState = PDO_ERR_NONE;` |
|    - |  235 | ``	/* Retain the PDO object. `$db->query(...)` on a temporary connection hands`` |
|    - |  236 | `	 * back a statement that outlives it, and php keeps the connection alive` |
|    - |  237 | `	 * through exactly this reference -- without it the database closes while` |
|    - |  238 | `	 * the statement is still being read. */` |
|  931 |  239 | `	pSt->pConnObj = pConn->pOwner;` |
|  931 |  240 | `	if( pSt->pConnObj ){` |
|  931 |  241 | `		pSt->pConnObj->iRef++;` |
|  464 |  242 | `	}` |
|  931 |  243 | `	pSt->pNext = pConn->pStmts;` |
|  931 |  244 | `	pConn->pStmts = pSt;` |
|  931 |  245 | `	return pSt;` |
|  467 |  246 | `}` |
|    - |  247 | `/* Drop what bindValue()/bindParam() recorded. */` |
| 1856 |  248 | `static void PdoBindListFree(ph7_vm *pVm,phl_pdo_bind *pB)` |
|    3 |  249 | `{` |
| 1993 |  250 | `	while( pB ){` |
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
| 1859 |  261 | `}` |
|  928 |  262 | `static void PdoBindsClear(phl_pdo_stmt *pSt)` |
|    3 |  263 | `{` |
|  931 |  264 | `	PdoBindListFree(pSt->pConn->pVm,pSt->pBinds);` |
|  931 |  265 | `	PdoBindListFree(pSt->pConn->pVm,pSt->pColBinds);` |
|  931 |  266 | `	pSt->pBinds = 0;` |
|  931 |  267 | `	pSt->pColBinds = 0;` |
|  931 |  268 | `}` |
|  928 |  269 | `PH7_PRIVATE void PH7_PdoFreeStmt(phl_pdo_stmt *pSt)` |
|    3 |  270 | `{` |
|  931 |  271 | `	PdoBindsClear(pSt);` |
|  931 |  272 | `	PdoStmtLazyClear(pSt);` |
|  931 |  273 | `	if( pSt->pLazyRow ){` |
|    - |  274 | `		/* A lazy row RETAINS its statement object, so this cannot run while one` |
|    - |  275 | `		 * is alive -- except at VM teardown, which releases in no order. Cut the` |
|    - |  276 | `		 * link from both ends rather than leave the row reading freed memory. */` |
|   27 |  277 | `		PdoBlankSlot(pSt->pLazyRow);` |
|   27 |  278 | `		pSt->pLazyRow = 0;` |
|   13 |  279 | `	}` |
|  931 |  280 | `	PdoStmtClearFetchState(pSt);` |
|  931 |  281 | `	PH7_PdoSqliteFinalize(pSt);` |
|  931 |  282 | `	if( pSt->pConnObj ){` |
|    - |  283 | `		/* drop the reference taken at creation; the connection may go now */` |
|  931 |  284 | `		ph7_class_instance *pObj = pSt->pConnObj;` |
|  931 |  285 | `		pSt->pConnObj = 0;` |
|  931 |  286 | `		PH7_ClassInstanceUnref(pObj);` |
|  464 |  287 | `	}` |
|  931 |  288 | `}` |
|    - |  289 | `/* Finalize and free every statement of one connection. */` |
|  268 |  290 | `static void PdoStmtSweep(phl_pdo *pConn)` |
|    4 |  291 | `{` |
|  272 |  292 | `	phl_pdo_stmt *pSt = pConn->pStmts;` |
| 1200 |  293 | `	while( pSt ){` |
|  931 |  294 | `		phl_pdo_stmt *pNext = pSt->pNext;` |
|  931 |  295 | `		PdoBlankSlot(pSt->pOwner);` |
|  931 |  296 | `		PH7_PdoFreeStmt(pSt);` |
|  931 |  297 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pSt);` |
|  931 |  298 | `		pSt = pNext;` |
|    3 |  299 | `	}` |
|  272 |  300 | `	pConn->pStmts = 0;` |
|  272 |  301 | `}` |
| 2382 |  302 | `static phl_pdo_stmt * PdoStmtOfInstance(ph7_class_instance *pThis)` |
|    3 |  303 | `{` |
|    - |  304 | `	SyString sAttr;` |
|    - |  305 | `	ph7_value *pRes;` |
| 2385 |  306 | `	if( pThis == 0 ){` |
|  ! 0 |  307 | `		return 0;` |
|    - |  308 | `	}` |
| 2385 |  309 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
| 2385 |  310 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
| 2385 |  311 | `	if( pRes == 0 \|\| !ph7_value_is_resource(pRes) ){` |
|  ! 0 |  312 | `		return 0;` |
|    - |  313 | `	}` |
| 2385 |  314 | `	return (phl_pdo_stmt *)ph7_value_to_resource(pRes);` |
| 1194 |  315 | `}` |
|  918 |  316 | `static int PdoStmtAttach(ph7_class_instance *pThis,phl_pdo_stmt *pSt)` |
|    3 |  317 | `{` |
|    - |  318 | `	SyString sAttr;` |
|    - |  319 | `	ph7_value *pRes;` |
|  921 |  320 | `	if( pThis == 0 ){` |
|  ! 0 |  321 | `		return -1;` |
|    - |  322 | `	}` |
|  921 |  323 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  921 |  324 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  921 |  325 | `	if( pRes == 0 ){` |
|  ! 0 |  326 | `		return -1;` |
|    - |  327 | `	}` |
|  921 |  328 | `	PH7_MemObjRelease(pRes);` |
|  921 |  329 | `	pRes->x.pOther = pSt;` |
|  921 |  330 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|  921 |  331 | `	pSt->pOwner = pThis;` |
|    - |  332 | ``	/* php's write_property refuses a store to `queryString` on a statement a`` |
|    - |  333 | ``	 * driver built -- and takes one on a `new PDOStatement()`, which has no`` |
|    - |  334 | `	 * cursor for it to describe. */` |
|  921 |  335 | `	PH7_NativeMarkAttrReadOnly(pThis,"queryString");` |
|  921 |  336 | `	return 0;` |
|  462 |  337 | `}` |
|    - |  338 | `/* The statement object is going away: release its cursor now, as php does. */` |
|  838 |  339 | `static void PdoStmtInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    3 |  340 | `{` |
|  841 |  341 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(pThis);` |
|  419 |  342 | `	SXUNUSED(pVm);` |
|  841 |  343 | `	if( pSt == 0 \|\| pSt->pOwner != pThis ){` |
|  ! 0 |  344 | `		return;` |
|    - |  345 | `	}` |
|  841 |  346 | `	PH7_PdoSqliteFinalize(pSt);` |
|  841 |  347 | `	pSt->pOwner = 0;` |
|  422 |  348 | `}` |
|    - |  349 |  |
|    - |  350 | `/* ------------------------------------------------------------------------` |
|    - |  351 | ` * The error surface` |
|    - |  352 | ` * ------------------------------------------------------------------------ */` |
| 1426 |  353 | `PH7_PRIVATE void PH7_PdoClearError(phl_pdo *pConn)` |
|    4 |  354 | `{` |
| 1430 |  355 | `	pConn->iErrState = PDO_ERR_OK;` |
| 1430 |  356 | `	pConn->iDrvCode = 0;` |
| 1430 |  357 | `	pConn->bNoDrvDetail = 0;` |
| 1430 |  358 | `	SyMemcpy("00000",pConn->zSqlState,sizeof("00000"));` |
| 1430 |  359 | `	if( pConn->zDrvMsg ){` |
|   27 |  360 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);` |
|   27 |  361 | `		pConn->zDrvMsg = 0;` |
|   13 |  362 | `	}` |
| 1430 |  363 | `}` |
|    - |  364 | `/*` |
|    - |  365 | ` * php clears the handle's error at the ENTRY of most verbs -- exec, query,` |
|    - |  366 | ` * prepare, quote, lastInsertId and both attribute accessors -- so a failure is` |
|    - |  367 | ` * invisible to errorCode() as soon as any of them is called, even on a handle` |
|    - |  368 | ` * that has never run anything (NULL becomes "00000"). The verbs that do NOT` |
|    - |  369 | ` * clear are the two reporters themselves and the transaction quartet.` |
|    - |  370 | ` */` |
| 1426 |  371 | `PH7_PRIVATE void PH7_PdoTouch(phl_pdo *pConn)` |
|    4 |  372 | `{` |
| 1430 |  373 | `	PH7_PdoClearError(pConn);` |
| 1430 |  374 | `}` |
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
|  349 |  684 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
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
|  238 |  963 | `static int vm_builtin_PDO_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  964 | `{` |
|  242 |  965 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - |  966 | `	const char *zSql;` |
|  242 |  967 | `	int nSql = 0;   /* the length is only written when the argument IS read */` |
|    - |  968 | `	ph7_int64 nChange;` |
|  242 |  969 | `	if( pConn == 0 ){` |
|  ! 0 |  970 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  971 | `	}` |
|  242 |  972 | `	PH7_PdoTouch(pConn);` |
|  242 |  973 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|  242 |  974 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 |  975 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  976 | `			"PDO::exec(): Argument #1 ($statement) must not be empty");` |
|    - |  977 | `	}` |
|  240 |  978 | `	nChange = PH7_PdoSqliteExec(pConn,zSql,nSql);` |
|  240 |  979 | `	if( nChange < 0 ){` |
|   17 |  980 | `		ph7_result_bool(pCtx,0);` |
|   17 |  981 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::exec");` |
|    - |  982 | `	}` |
|  224 |  983 | `	ph7_result_int64(pCtx,nChange);` |
|  224 |  984 | `	return PH7_OK;` |
|  123 |  985 | `}` |
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
| 1130 | 1050 | `static void PdoStmtOk(phl_pdo_stmt *pSt)` |
|    3 | 1051 | `{` |
| 1133 | 1052 | `	pSt->iErrState = PDO_ERR_OK;` |
| 1133 | 1053 | `	SyMemcpy("00000",pSt->zSqlState,sizeof("00000"));` |
| 1133 | 1054 | `}` |
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
| 1472 | 1149 | `static void PdoStmtClearFetchState(phl_pdo_stmt *pSt)` |
|    3 | 1150 | `{` |
| 1475 | 1151 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
| 1475 | 1152 | `	if( pSt->zFetchClass ){` |
|   25 | 1153 | `		SyMemBackendFree(&pVm->sAllocator,pSt->zFetchClass);` |
|   25 | 1154 | `		pSt->zFetchClass = 0;` |
|   25 | 1155 | `		pSt->nFetchClass = 0;` |
|   12 | 1156 | `	}` |
| 1475 | 1157 | `	if( pSt->pFetchArgs ){` |
|    9 | 1158 | `		ph7_release_value(pVm,pSt->pFetchArgs);` |
|    9 | 1159 | `		pSt->pFetchArgs = 0;` |
|    4 | 1160 | `	}` |
| 1475 | 1161 | `	if( pSt->pFetchInto ){` |
|   11 | 1162 | `		ph7_class_instance *pObj = pSt->pFetchInto;` |
|   11 | 1163 | `		pSt->pFetchInto = 0;` |
|   11 | 1164 | `		PH7_ClassInstanceUnref(pObj);` |
|    5 | 1165 | `	}` |
| 1475 | 1166 | `}` |
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
|   21 | 1180 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
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
| 1012 | 1254 | `static void PdoStmtLazyClear(phl_pdo_stmt *pSt)` |
|    3 | 1255 | `{` |
| 1015 | 1256 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
| 1015 | 1257 | `	if( pSt->pLazyVals ){` |
|   61 | 1258 | `		ph7_release_value(pVm,pSt->pLazyVals);` |
|   61 | 1259 | `		pSt->pLazyVals = 0;` |
|   30 | 1260 | `	}` |
| 1015 | 1261 | `}` |
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
| 1476 | 1326 | `static int PdoStmtStep(phl_pdo_stmt *pSt)` |
|    3 | 1327 | `{` |
| 1479 | 1328 | `	int rc = PH7_PdoSqliteStep(pSt);` |
| 1479 | 1329 | `	if( rc < 0 ){` |
|    5 | 1330 | `		pSt->bDone = 1;` |
|    5 | 1331 | `		pSt->bRowPending = 0;` |
|    5 | 1332 | `		return -1;` |
|    - | 1333 | `	}` |
| 1475 | 1334 | `	pSt->bRowPending = (rc == 1);` |
| 1475 | 1335 | `	pSt->bDone = (rc == 0);` |
| 1475 | 1336 | `	return rc;` |
|  741 | 1337 | `}` |
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
|   29 | 1469 | `		ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
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
|  135 | 1523 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
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
|  133 | 1878 | `		pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pB->nSlot);` |
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
|    1 | 1917 | `						}` |
|    1 | 1918 | `					}` |
|    4 | 1919 | `					break;` |
|    6 | 1920 | `				default:             break;   /* the value as the driver typed it */` |
|    - | 1921 | `			}` |
|   47 | 1922 | `		}` |
|  113 | 1923 | `		PH7_MemObjStore(&sVal,pSlot);` |
|  113 | 1924 | `		PH7_MemObjRelease(&sVal);` |
|   57 | 1925 | `	}` |
|   97 | 1926 | `	return 1;` |
|    1 | 1927 | `}` |
|    - | 1928 | `/*` |
|    - | 1929 | ` * Does every binding name a column this statement HAS? The width is the` |
|    - | 1930 | ` * statement's and does not change while it is walked, so this is asked ONCE by` |
|    - | 1931 | ` * the verb rather than per row -- which is also what keeps php's` |
|    - | 1932 | `` * `Invalid column index` out of the middle of fetchAll()'s loop, where a raise`` |
|    - | 1933 | ` * has no frame to leave.` |
|    - | 1934 | ` */` |
|  178 | 1935 | `static int PdoBoundColumnsInRange(phl_pdo_stmt *pSt)` |
|    1 | 1936 | `{` |
|    - | 1937 | `	phl_pdo_bind *pB;` |
|  179 | 1938 | `	int nCol = PH7_PdoSqliteColumnCount(pSt);` |
|  389 | 1939 | `	for( pB = pSt->pColBinds ; pB ; pB = pB->pNext ){` |
|  227 | 1940 | `		if( pB->iPos < 1 \|\| pB->iPos > nCol ){` |
|   17 | 1941 | `			return 0;` |
|    - | 1942 | `		}` |
|  106 | 1943 | `	}` |
|  163 | 1944 | `	return 1;` |
|   90 | 1945 | `}` |
|    - | 1946 | `/*` |
|    - | 1947 | ` * The bound columns of the row a verb is about to hand out. php writes them on` |
|    - | 1948 | ` * EVERY fetch, whatever the mode -- FETCH_BOUND is only the mode that answers` |
|    - | 1949 | `` * `true` INSTEAD of a row, not the one that does the writing -- so this runs`` |
|    - | 1950 | ` * wherever a row is read. A binding OUT of range is not its business: the verb` |
|    - | 1951 | ` * screens for one first and refuses through PdoBoundColumnsRefuse.` |
|    - | 1952 | ` */` |
|  604 | 1953 | `static void PdoBoundColumnsForRow(ph7_vm *pVm,phl_pdo_stmt *pSt)` |
|    2 | 1954 | `{` |
|  606 | 1955 | `	if( pSt->pColBinds == 0 \|\| !pSt->bRowPending \|\| !PdoBoundColumnsInRange(pSt) ){` |
|    - | 1956 | `		/* php looks at the bindings only when there is a ROW to write from, so a` |
|    - | 1957 | `		 * cursor with nothing left answers false rather than refusing; a binding` |
|    - | 1958 | `		 * OUT of range was screened by the verb before the row was read. */` |
|  530 | 1959 | `		return;` |
|    - | 1960 | `	}` |
|   77 | 1961 | `	PdoWriteBoundColumns(pVm,pSt);` |
|  304 | 1962 | `}` |
|    - | 1963 | `/* Has the verb about to read a row a binding it cannot honour? */` |
|  696 | 1964 | `static int PdoBoundColumnsBad(phl_pdo_stmt *pSt)` |
|    2 | 1965 | `{` |
|  698 | 1966 | `	return pSt->pColBinds != 0 && pSt->bRowPending && !PdoBoundColumnsInRange(pSt);` |
|    2 | 1967 | `}` |
|    - | 1968 | `/*` |
|    - | 1969 | ` * php's refusal for a binding naming a column the statement does not have. It` |
|    - | 1970 | ` * reads the row and moves the cursor ON before the refusal surfaces, so three` |
|    - | 1971 | ` * fetches over three rows refuse one by one and the fourth answers false --` |
|    - | 1972 | ` * and fetchAll(), which would have walked the whole set, exhausts it and` |
|    - | 1973 | ` * refuses once.` |
|    - | 1974 | ` */` |
|   16 | 1975 | `static sxi32 PdoBoundColumnsRefuse(ph7_context *pCtx,phl_pdo_stmt *pSt,int bWholeSet)` |
|    1 | 1976 | `{` |
|    - | 1977 | `	/* The bindings it CAN honour are written all the same -- the one it cannot` |
|    - | 1978 | `	 * reach does not stop the rest -- and for the whole-set verb they are` |
|    - | 1979 | `	 * written from EVERY row it walks past, so the caller is left holding the` |
|    - | 1980 | `	 * last row's values exactly as a successful fetchAll() would leave them. */` |
|    8 | 1981 | `	do{` |
|   21 | 1982 | `		PdoWriteBoundColumns(pCtx->pVm,pSt);` |
|   21 | 1983 | `		pSt->bRowPending = 0;` |
|   21 | 1984 | `		PdoStmtStep(pSt);` |
|   21 | 1985 | `	}while( bWholeSet && pSt->bRowPending );` |
|   17 | 1986 | `	PdoStmtOk(pSt);` |
|   17 | 1987 | `	return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    1 | 1988 | `}` |
|    - | 1989 | `/*` |
|    - | 1990 | ` * May this statement be WALKED at all? php's iterator refusals are the verbs'` |
|    - | 1991 | ` * own, and a foreach reaches them through the InternalIterator GUARD -- which` |
|    - | 1992 | ` * is where a call CONTEXT exists, so the throw leaves the loop the way every` |
|    - | 1993 | ` * other one does (raising on the VM from inside the vtable let execution carry` |
|    - | 1994 | ` * on past the loop). Three of them: a binding naming a column the statement` |
|    - | 1995 | ` * does not have -- consuming the row first, exactly as a fetch does -- and the` |
|    - | 1996 | ` * two modes a connection's ATTR_DEFAULT_FETCH_MODE can leave a statement in` |
|    - | 1997 | ` * with nothing beside it to build from.` |
|    - | 1998 | ` */` |
|  416 | 1999 | `static int PdoStmtWalkRefusal(ph7_context *pCtx,phl_pdo_stmt *pSt)` |
|    1 | 2000 | `{` |
|    - | 2001 | `	int iBase;` |
|  417 | 2002 | `	if( pSt == 0 \|\| !pSt->bRowPending ){` |
|  199 | 2003 | `		return 0;   /* nothing to hand out, so nothing to refuse */` |
|    - | 2004 | `	}` |
|  219 | 2005 | `	if( PdoBoundColumnsBad(pSt) ){` |
|    5 | 2006 | `		PdoBoundColumnsRefuse(pCtx,pSt,0);` |
|    5 | 2007 | `		return 1;` |
|    - | 2008 | `	}` |
|  215 | 2009 | `	iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;` |
|  215 | 2010 | `	if( iBase == PDO_FETCH_INTO && pSt->pFetchInto == 0 ){` |
|    3 | 2011 | `		PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 2012 | `			"SQLSTATE[HY000]: General error: No fetch-into object specified.");` |
|    3 | 2013 | `		return 1;` |
|    - | 2014 | `	}` |
|  212 | 2015 | `	if( iBase == PDO_FETCH_CLASS && (pSt->iFetchMode & PDO_FETCH_CLASSTYPE) == 0` |
|   35 | 2016 | `	 && pSt->zFetchClass == 0 ){` |
|    5 | 2017 | `		PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 2018 | `			"SQLSTATE[HY000]: General error: No fetch class specified");` |
|    5 | 2019 | `		return 1;` |
|    - | 2020 | `	}` |
|  209 | 2021 | `	return 0;` |
|  209 | 2022 | `}` |
|  358 | 2023 | `static int PdoStmtIterGuard(ph7_context *pCtx,ph7_class_instance *pIt)` |
|    1 | 2024 | `{` |
|  538 | 2025 | `	return PdoStmtWalkRefusal(pCtx,PdoStmtOfInstance(` |
|  179 | 2026 | `		PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC)));` |
|    1 | 2027 | `}` |
|    - | 2028 | `/* ------------------------------------------------------------------------` |
|    - | 2029 | ` * The blob STREAM: what Pdo\Sqlite::openBlob() answers` |
|    - | 2030 | ` * ------------------------------------------------------------------------ */` |
|    - | 2031 | `/*` |
|    - | 2032 | ` * php answers openBlob() with a php STREAM over one column of one row --` |
|    - | 2033 | `` * `stream_get_meta_data()` names its type `PDOSQLite` -- so the handle behind`` |
|    - | 2034 | ` * it is a device of its own here, the shape vfs_stream.c already carries for` |
|    - | 2035 | ` * php://, data:// and the userland wrappers. It reads and writes through` |
|    - | 2036 | ` * sqlite3_blob_read/_write at an offset it keeps itself, and its LENGTH is` |
|    - | 2037 | ` * fixed: a blob handle addresses the bytes that are already there, so a write` |
|    - | 2038 | `` * past the end is short and `ftruncate()` has nothing to do.`` |
|    - | 2039 | ` */` |
|   16 | 2040 | `static ph7_int64 PdoBlobStream_Read(void *pHandle,void *pBuf,ph7_int64 nDatatoRead)` |
|    1 | 2041 | `{` |
|   17 | 2042 | `	phl_pdo_blob *pBl = (phl_pdo_blob *)pHandle;` |
|   17 | 2043 | `	int nRead = PH7_PdoSqliteBlobIo(pBl,pBuf,(int)nDatatoRead,0);` |
|   17 | 2044 | `	return nRead < 0 ? -1 : (ph7_int64)nRead;` |
|    1 | 2045 | `}` |
|    6 | 2046 | `static ph7_int64 PdoBlobStream_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    1 | 2047 | `{` |
|    7 | 2048 | `	phl_pdo_blob *pBl = (phl_pdo_blob *)pHandle;` |
|    - | 2049 | `	int nDone;` |
|    7 | 2050 | `	if( !pBl->bWrite ){` |
|    - | 2051 | `		/* php's own sentence, from the device rather than from fwrite(): a` |
|    - | 2052 | `		 * handle opened READONLY refuses and the write answers false. */` |
|    3 | 2053 | `		PH7_VmThrowError(pBl->pConn->pVm,0,PH7_CTX_WARNING,` |
|    - | 2054 | `			"fwrite(): Can't write to blob stream: is open as read only");` |
|    3 | 2055 | `		return -1;` |
|    - | 2056 | `	}` |
|    5 | 2057 | `	if( nWrite > 0 && pBl->iOfft + nWrite > pBl->nSize ){` |
|    - | 2058 | `		/* A blob handle addresses the bytes that are already there: php refuses` |
|    - | 2059 | `		 * a write that would need more of them rather than writing what fits. */` |
|    3 | 2060 | `		PH7_VmThrowError(pBl->pConn->pVm,0,PH7_CTX_WARNING,` |
|    - | 2061 | `			"fwrite(): It is not possible to increase the size of a BLOB");` |
|    3 | 2062 | `		return -1;` |
|    - | 2063 | `	}` |
|    3 | 2064 | `	nDone = PH7_PdoSqliteBlobIo(pBl,(void *)pBuf,(int)nWrite,1);` |
|    3 | 2065 | `	return nDone < 0 ? -1 : (ph7_int64)nDone;` |
|    4 | 2066 | `}` |
|   12 | 2067 | `static int PdoBlobStream_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|    1 | 2068 | `{` |
|   13 | 2069 | `	phl_pdo_blob *pBl = (phl_pdo_blob *)pHandle;` |
|   13 | 2070 | `	ph7_int64 iNew = iOfft;` |
|   13 | 2071 | `	if( whence == 1 ){          /* SEEK_CUR */` |
|  ! 0 | 2072 | `		iNew = pBl->iOfft + iOfft;` |
|   13 | 2073 | `	}else if( whence == 2 ){    /* SEEK_END */` |
|    3 | 2074 | `		iNew = pBl->nSize + iOfft;` |
|    1 | 2075 | `	}` |
|   13 | 2076 | `	if( iNew < 0 \|\| iNew > pBl->nSize ){` |
|    - | 2077 | `		/* A blob handle addresses bytes that already exist, so there is nowhere` |
|    - | 2078 | `		 * past the end to seek TO. php's failed seek leaves the position` |
|    - | 2079 | `		 * UNKNOWN -- ftell() answers false and a read answers nothing until a` |
|    - | 2080 | `		 * seek succeeds again -- which is what the flag carries. */` |
|    5 | 2081 | `		pBl->bBadPos = 1;` |
|    5 | 2082 | `		return -1;` |
|    - | 2083 | `	}` |
|    9 | 2084 | `	pBl->bBadPos = 0;` |
|    9 | 2085 | `	pBl->iOfft = iNew;` |
|    9 | 2086 | `	return PH7_OK;` |
|    7 | 2087 | `}` |
|   20 | 2088 | `static ph7_int64 PdoBlobStream_Tell(void *pHandle)` |
|    1 | 2089 | `{` |
|   21 | 2090 | `	phl_pdo_blob *pBl = (phl_pdo_blob *)pHandle;` |
|   21 | 2091 | `	return pBl->bBadPos ? -1 : pBl->iOfft;` |
|    1 | 2092 | `}` |
|    2 | 2093 | `static int PdoBlobStream_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 2094 | `{` |
|    3 | 2095 | `	phl_pdo_blob *pBl = (phl_pdo_blob *)pHandle;` |
|    3 | 2096 | `	ph7_value_int64(pWorker,pBl->nSize);` |
|    3 | 2097 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker);` |
|    3 | 2098 | `	return PH7_OK;` |
|    1 | 2099 | `}` |
|    4 | 2100 | `static void PdoBlobStream_Close(void *pHandle)` |
|    1 | 2101 | `{` |
|    5 | 2102 | `	PH7_PdoSqliteBlobClose((phl_pdo_blob *)pHandle);` |
|    5 | 2103 | `}` |
|    - | 2104 | `static const ph7_io_stream sPdoBlobStream = {` |
|    - | 2105 | `	"PDOSQLite",                /* what stream_get_meta_data() reports */` |
|    - | 2106 | `	PH7_IO_STREAM_VERSION,` |
|    - | 2107 | `	0,                          /* xOpen: openBlob() builds the handle itself */` |
|    - | 2108 | `	0,                          /* xOpenDir */` |
|    - | 2109 | `	PdoBlobStream_Close,` |
|    - | 2110 | `	0,                          /* xCloseDir */` |
|    - | 2111 | `	PdoBlobStream_Read,` |
|    - | 2112 | `	0,                          /* xReadDir */` |
|    - | 2113 | `	PdoBlobStream_Write,` |
|    - | 2114 | `	PdoBlobStream_Seek,` |
|    - | 2115 | `	0,                          /* xLock */` |
|    - | 2116 | `	0,                          /* xRewindDir */` |
|    - | 2117 | `	PdoBlobStream_Tell,` |
|    - | 2118 | `	0,                          /* xTrunc: a blob is the length it was created with */` |
|    - | 2119 | `	0,                          /* xSync */` |
|    - | 2120 | `	PdoBlobStream_Stat` |
|    - | 2121 | `};` |
|    - | 2122 | `/*` |
|    - | 2123 | ` * The other stream this driver hands out: what a PARAM_LOB bound column reads` |
|    - | 2124 | ` * as. php converts a STRING column into a php memory stream there --` |
|    - | 2125 | `` * `stream_get_meta_data()` calls it `MEMORY`, read-only and seekable, with the`` |
|    - | 2126 | ` * value's own length behind it -- so the bytes are copied once when the row is` |
|    - | 2127 | ` * written and the handle owns them. Nothing else is a stream: an int, a float` |
|    - | 2128 | ` * and a null bound as LOB stay what the driver typed them.` |
|    - | 2129 | ` */` |
|    - | 2130 | `typedef struct phl_pdo_lob phl_pdo_lob;` |
|    - | 2131 | `struct phl_pdo_lob {` |
|    - | 2132 | `	ph7_vm *pVm;` |
|    - | 2133 | `	SyBlob sData;` |
|    - | 2134 | `	ph7_int64 iOfft;` |
|    - | 2135 | `};` |
|    6 | 2136 | `static ph7_int64 PdoLobStream_Read(void *pHandle,void *pBuf,ph7_int64 nDatatoRead)` |
|    1 | 2137 | `{` |
|    7 | 2138 | `	phl_pdo_lob *pLob = (phl_pdo_lob *)pHandle;` |
|    7 | 2139 | `	ph7_int64 nLeft = (ph7_int64)SyBlobLength(&pLob->sData) - pLob->iOfft;` |
|    7 | 2140 | `	if( nLeft < 1 \|\| nDatatoRead < 1 ){` |
|    3 | 2141 | `		return 0;` |
|    - | 2142 | `	}` |
|    5 | 2143 | `	if( nDatatoRead > nLeft ){` |
|    3 | 2144 | `		nDatatoRead = nLeft;` |
|    1 | 2145 | `	}` |
|    5 | 2146 | `	SyMemcpy((const char *)SyBlobData(&pLob->sData) + pLob->iOfft,pBuf,(sxu32)nDatatoRead);` |
|    5 | 2147 | `	pLob->iOfft += nDatatoRead;` |
|    5 | 2148 | `	return nDatatoRead;` |
|    4 | 2149 | `}` |
|    2 | 2150 | `static ph7_int64 PdoLobStream_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    1 | 2151 | `{` |
|    1 | 2152 | `	SXUNUSED(pHandle);` |
|    1 | 2153 | `	SXUNUSED(pBuf);` |
|    1 | 2154 | `	SXUNUSED(nWrite);` |
|    3 | 2155 | `	return -1;   /* php's is read-only: fwrite() answers false and says nothing */` |
|    1 | 2156 | `}` |
|    2 | 2157 | `static int PdoLobStream_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|    1 | 2158 | `{` |
|    3 | 2159 | `	phl_pdo_lob *pLob = (phl_pdo_lob *)pHandle;` |
|    3 | 2160 | `	ph7_int64 iNew = iOfft;` |
|    3 | 2161 | `	if( whence == 1 ){` |
|  ! 0 | 2162 | `		iNew = pLob->iOfft + iOfft;` |
|    3 | 2163 | `	}else if( whence == 2 ){` |
|  ! 0 | 2164 | `		iNew = (ph7_int64)SyBlobLength(&pLob->sData) + iOfft;` |
|  ! 0 | 2165 | `	}` |
|    3 | 2166 | `	if( iNew < 0 ){` |
|  ! 0 | 2167 | `		return -1;` |
|    - | 2168 | `	}` |
|    3 | 2169 | `	pLob->iOfft = iNew;` |
|    3 | 2170 | `	return PH7_OK;` |
|    2 | 2171 | `}` |
|    4 | 2172 | `static ph7_int64 PdoLobStream_Tell(void *pHandle)` |
|    1 | 2173 | `{` |
|    5 | 2174 | `	return ((phl_pdo_lob *)pHandle)->iOfft;` |
|    1 | 2175 | `}` |
|    2 | 2176 | `static int PdoLobStream_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 2177 | `{` |
|    3 | 2178 | `	phl_pdo_lob *pLob = (phl_pdo_lob *)pHandle;` |
|    3 | 2179 | `	ph7_value_int64(pWorker,(ph7_int64)SyBlobLength(&pLob->sData));` |
|    3 | 2180 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker);` |
|    3 | 2181 | `	return PH7_OK;` |
|    1 | 2182 | `}` |
|    2 | 2183 | `static void PdoLobStream_Close(void *pHandle)` |
|    1 | 2184 | `{` |
|    3 | 2185 | `	phl_pdo_lob *pLob = (phl_pdo_lob *)pHandle;` |
|    3 | 2186 | `	ph7_vm *pVm = pLob->pVm;` |
|    3 | 2187 | `	SyBlobRelease(&pLob->sData);` |
|    3 | 2188 | `	SyMemBackendFree(&pVm->sAllocator,pLob);` |
|    3 | 2189 | `}` |
|    - | 2190 | `static const ph7_io_stream sPdoLobStream = {` |
|    - | 2191 | `	"MEMORY",                   /* what php's own conversion reports */` |
|    - | 2192 | `	PH7_IO_STREAM_VERSION,` |
|    - | 2193 | `	0, 0,` |
|    - | 2194 | `	PdoLobStream_Close,` |
|    - | 2195 | `	0,` |
|    - | 2196 | `	PdoLobStream_Read,` |
|    - | 2197 | `	0,` |
|    - | 2198 | `	PdoLobStream_Write,` |
|    - | 2199 | `	PdoLobStream_Seek,` |
|    - | 2200 | `	0, 0,` |
|    - | 2201 | `	PdoLobStream_Tell,` |
|    - | 2202 | `	0, 0,` |
|    - | 2203 | `	PdoLobStream_Stat` |
|    - | 2204 | `};` |
|    - | 2205 | `/*` |
|    - | 2206 | ` * Wrap one value's bytes in that stream. Answers 0 when the memory could not` |
|    - | 2207 | ` * be had, which leaves the caller's value as the driver typed it.` |
|    - | 2208 | ` */` |
|    2 | 2209 | `static io_private * PdoLobStreamNew(ph7_vm *pVm,const char *zData,int nData)` |
|    1 | 2210 | `{` |
|    - | 2211 | `	phl_pdo_lob *pLob;` |
|    - | 2212 | `	io_private *pDev;` |
|    3 | 2213 | `	pLob = (phl_pdo_lob *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo_lob));` |
|    3 | 2214 | `	if( pLob == 0 ){` |
|  ! 0 | 2215 | `		return 0;` |
|    - | 2216 | `	}` |
|    3 | 2217 | `	SyZero(pLob,sizeof(phl_pdo_lob));` |
|    3 | 2218 | `	pLob->pVm = pVm;` |
|    3 | 2219 | `	SyBlobInit(&pLob->sData,&pVm->sAllocator);` |
|    3 | 2220 | `	if( nData > 0 ){` |
|    3 | 2221 | `		SyBlobAppend(&pLob->sData,zData,(sxu32)nData);` |
|    1 | 2222 | `	}` |
|    - | 2223 | `	/* The io_private goes back through ph7_context_free_chunk, which is a plain` |
|    - | 2224 | `	 * VM-allocator free for a chunk nothing registered -- so a handle built` |
|    - | 2225 | `	 * where there is no call context (the foreach iterator's row) is released` |
|    - | 2226 | `	 * exactly like one built where there is. */` |
|    3 | 2227 | `	pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    3 | 2228 | `	if( pDev == 0 ){` |
|  ! 0 | 2229 | `		PdoLobStream_Close(pLob);` |
|  ! 0 | 2230 | `		return 0;` |
|    - | 2231 | `	}` |
|    3 | 2232 | `	SyZero(pDev,sizeof(io_private));` |
|    3 | 2233 | `	InitIOPrivate(pVm,&sPdoLobStream,pDev);` |
|    3 | 2234 | `	pDev->pHandle = pLob;` |
|    3 | 2235 | `	SetIOPrivateOpenedAs(pDev,"",0,"rb",2);` |
|    3 | 2236 | `	return pDev;` |
|    2 | 2237 | `}` |
|    - | 2238 | `/*` |
|    - | 2239 | ` * Pdo\Sqlite::openBlob(string $table, string $column, int $rowid,` |
|    - | 2240 | ` *                       ?string $dbname = "main", int $flags = OPEN_READONLY)` |
|    - | 2241 | ` *` |
|    - | 2242 | ` * php reports every failure as a WARNING carrying sqlite's own message and` |
|    - | 2243 | ` * answers false -- the error mode has no say, because this is a stream opener` |
|    - | 2244 | `` * and not a statement. The `$dbname` is passed to sqlite as it stands, which is`` |
|    - | 2245 | `` * why a null one produces `no such table: .b`, and only OPEN_READWRITE among`` |
|    - | 2246 | ` * the flags means anything: sqlite's blob handle is readable either way.` |
|    - | 2247 | ` */` |
|   16 | 2248 | `PH7_PRIVATE int PH7_PdoSqliteOpenBlobMethod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2249 | `{` |
|   17 | 2250 | `	phl_pdo *pConn = PH7_PdoConnOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2251 | `	phl_pdo_blob *pBl;` |
|    - | 2252 | `	io_private *pDev;` |
|   17 | 2253 | `	const char *zTable,*zColumn,*zDb = "main";` |
|   17 | 2254 | `	int nTable = 0,nColumn = 0,nDb = (int)sizeof("main")-1;` |
|    - | 2255 | `	ph7_int64 iRow;` |
|    - | 2256 | `	int iFlags;` |
|   17 | 2257 | `	if( pConn == 0 ){` |
|  ! 0 | 2258 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 2259 | `	}` |
|   17 | 2260 | `	zTable  = nArg > 0 ? ph7_value_to_string(apArg[0],&nTable) : "";` |
|   17 | 2261 | `	zColumn = nArg > 1 ? ph7_value_to_string(apArg[1],&nColumn) : "";` |
|   17 | 2262 | `	iRow    = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|   17 | 2263 | `	if( nArg > 3 && (apArg[3]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    5 | 2264 | `		zDb = ph7_value_to_string(apArg[3],&nDb);` |
|   15 | 2265 | `	}else if( nArg > 3 ){` |
|  ! 0 | 2266 | `		zDb = "";           /* php hands sqlite the empty name a null becomes */` |
|  ! 0 | 2267 | `		nDb = 0;` |
|  ! 0 | 2268 | `	}` |
|   17 | 2269 | `	iFlags = nArg > 4 ? (int)ph7_value_to_int64(apArg[4]) : SQLITE_OPEN_READONLY;` |
|    8 | 2270 | `	SXUNUSED(nDb);` |
|    8 | 2271 | `	SXUNUSED(nTable);` |
|    8 | 2272 | `	SXUNUSED(nColumn);` |
|   25 | 2273 | `	pBl = PH7_PdoSqliteBlobOpen(pConn,zDb,zTable,zColumn,iRow,` |
|   16 | 2274 | `		(iFlags & SQLITE_OPEN_READWRITE) != 0);` |
|   17 | 2275 | `	if( pBl == 0 ){` |
|   11 | 2276 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"Unable to open blob: %s",` |
|   10 | 2277 | `			pConn->zDrvMsg ? pConn->zDrvMsg : "unknown error");` |
|   11 | 2278 | `		ph7_result_bool(pCtx,0);` |
|   11 | 2279 | `		return PH7_OK;` |
|    - | 2280 | `	}` |
|    7 | 2281 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    7 | 2282 | `	if( pDev == 0 ){` |
|  ! 0 | 2283 | `		PH7_PdoSqliteBlobClose(pBl);` |
|  ! 0 | 2284 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2285 | `	}` |
|    7 | 2286 | `	InitIOPrivate(pCtx->pVm,&sPdoBlobStream,pDev);` |
|    7 | 2287 | `	pDev->pHandle = pBl;` |
|    - | 2288 | `	/* php's meta reports no uri for this stream and the mode it opened with. */` |
|    7 | 2289 | `	SetIOPrivateOpenedAs(pDev,"",0,pBl->bWrite ? "r+b" : "rb",pBl->bWrite ? 3 : 2);` |
|    7 | 2290 | `	ph7_result_resource(pCtx,pDev);` |
|    7 | 2291 | `	return PH7_OK;` |
|    9 | 2292 | `}` |
|    - | 2293 | `/* ------------------------------------------------------------------------` |
|    - | 2294 | ` * PDORow: what PDO::FETCH_LAZY answers` |
|    - | 2295 | ` * ------------------------------------------------------------------------ */` |
|    - | 2296 | `/*` |
|    - | 2297 | ` * php's PDORow is a fully VIRTUAL object over a statement's CURRENT row: it` |
|    - | 2298 | `` * declares one property (`queryString`) and holds NONE, every column is read`` |
|    - | 2299 | ` * through its property and dimension handlers, and every write is refused.` |
|    - | 2300 | `` * That split is what makes `get_object_vars()` empty beside a `$row->id` that`` |
|    - | 2301 | `` * works, `var_dump()` show the columns anyway (its get_debug_info handler) and`` |
|    - | 2302 | `` * `(array)`/`var_export()`/`json_encode()` show nothing at all.`` |
|    - | 2303 | ` *` |
|    - | 2304 | ` * One object per statement, handed back by every lazy fetch, so two fetches` |
|    - | 2305 | ` * answer the same object and the FIRST one moves on to the second row. It` |
|    - | 2306 | ` * RETAINS the statement object -- a row outliving the variable that fetched it` |
|    - | 2307 | ` * still reads (and still keeps the database open under it) -- and the statement` |
|    - | 2308 | ` * points back at it without a reference, which the row's own release clears.` |
|    - | 2309 | ` */` |
|    - | 2310 | `/* The row's hidden slots: the statement it reads, and the object that owns it. */` |
|    - | 2311 | `#define PDOROW_RES  "__res"` |
|    - | 2312 | `#define PDOROW_STMT "__stmt"` |
|    - | 2313 |  |
|  244 | 2314 | `static phl_pdo_stmt * PdoRowStmt(ph7_class_instance *pThis)` |
|    1 | 2315 | `{` |
|    - | 2316 | `	SyString sAttr;` |
|    - | 2317 | `	ph7_value *pRes;` |
|  245 | 2318 | `	if( pThis == 0 ){` |
|  ! 0 | 2319 | `		return 0;` |
|    - | 2320 | `	}` |
|  245 | 2321 | `	SyStringInitFromBuf(&sAttr,PDOROW_RES,sizeof(PDOROW_RES)-1);` |
|  245 | 2322 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  245 | 2323 | `	if( pRes == 0 \|\| !ph7_value_is_resource(pRes) ){` |
|  ! 0 | 2324 | `		return 0;` |
|    - | 2325 | `	}` |
|  245 | 2326 | `	return (phl_pdo_stmt *)ph7_value_to_resource(pRes);` |
|  123 | 2327 | `}` |
|    - | 2328 | `/*` |
|    - | 2329 | ` * php reads a property or offset NAME as a column NUMBER when it is an integer` |
|    - | 2330 | `` * string -- `$row->{'0'}` and `$row['1']` are columns, not names -- and as a`` |
|    - | 2331 | ` * column NAME otherwise. The grammar is php's is_numeric_string answering` |
|    - | 2332 | ` * IS_LONG: whitespace, a sign, digits, whitespace, and a value an int64 holds` |
|    - | 2333 | ` * (an overflow reads as a double there, so it falls through to the name lookup` |
|    - | 2334 | ` * and misses).` |
|    - | 2335 | ` */` |
|  172 | 2336 | `static int PdoRowIntName(const char *z,int n,ph7_int64 *pOut)` |
|    1 | 2337 | `{` |
|  173 | 2338 | `	const char *zEnd = z + n;` |
|  173 | 2339 | `	int bNeg = 0, nDigit = 0;` |
|  173 | 2340 | `	ph7_int64 iVal = 0;` |
|  173 | 2341 | `	while( z < zEnd && SyisSpace(z[0]) ){ z++; }` |
|  173 | 2342 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|    3 | 2343 | `		bNeg = (z[0] == '-');` |
|    3 | 2344 | `		z++;` |
|    1 | 2345 | `	}` |
|  207 | 2346 | `	while( z < zEnd && SyisDigit(z[0]) ){` |
|   35 | 2347 | `		int iDigit = z[0] - '0';` |
|   35 | 2348 | `		if( iVal > (SXI64_HIGH - iDigit) / 10 ){` |
|  ! 0 | 2349 | `			return 0;` |
|    - | 2350 | `		}` |
|   35 | 2351 | `		iVal = iVal * 10 + iDigit;` |
|   35 | 2352 | `		z++;` |
|   35 | 2353 | `		nDigit++;` |
|    1 | 2354 | `	}` |
|  173 | 2355 | `	while( z < zEnd && SyisSpace(z[0]) ){ z++; }` |
|  173 | 2356 | `	if( nDigit == 0 \|\| z != zEnd ){` |
|  145 | 2357 | `		return 0;` |
|    - | 2358 | `	}` |
|   29 | 2359 | `	*pOut = bNeg ? -iVal : iVal;` |
|   29 | 2360 | `	return 1;` |
|   87 | 2361 | `}` |
|    - | 2362 | ``/* php's read handlers answer `queryString` from the STATEMENT before they look`` |
|    - | 2363 | ` * at any column, so a query selecting a column of that name cannot shadow it --` |
|    - | 2364 | ` * while has_property/has_dimension do not know the name at all. */` |
|  154 | 2365 | `static int PdoRowIsQueryString(const char *zName,int nName)` |
|    1 | 2366 | `{` |
|   87 | 2367 | `	return nName == sizeof("queryString")-1` |
|  154 | 2368 | `		&& SyMemcmp(zName,"queryString",sizeof("queryString")-1) == 0;` |
|    1 | 2369 | `}` |
|   16 | 2370 | `static void PdoRowQueryString(phl_pdo_stmt *pSt,ph7_value *pOut)` |
|    1 | 2371 | `{` |
|   17 | 2372 | `	ph7_value *pQs = pSt->pOwner ? PH7_NativeAttr(pSt->pOwner,"queryString") : 0;` |
|   17 | 2373 | `	if( pQs ){` |
|   17 | 2374 | `		PH7_MemObjStore(pQs,pOut);` |
|    8 | 2375 | `	}` |
|   17 | 2376 | `}` |
|    - | 2377 | `/*` |
|    - | 2378 | ` * How many columns the row HAS, which is the statement's own count and not the` |
|    - | 2379 | ` * capture's: php describes a statement once and answers for those columns for` |
|    - | 2380 | ` * as long as the cursor exists, so a walk that has run out still knows every` |
|    - | 2381 | ` * name and reads each as a null the connection's modifiers may still reshape.` |
|    - | 2382 | ` */` |
|  186 | 2383 | `static int PdoRowColumnCount(phl_pdo_stmt *pSt)` |
|    1 | 2384 | `{` |
|  187 | 2385 | `	return pSt ? PH7_PdoSqliteColumnCount(pSt) : 0;` |
|    1 | 2386 | `}` |
|    - | 2387 | `/*` |
|    - | 2388 | ` * The column a name selects, or -1. php looks a NUMBER up positionally and` |
|    - | 2389 | ` * misses outright when it is out of range -- it never falls back to a name of` |
|    - | 2390 | ` * the same spelling -- and matches a NAME byte for byte, first occurrence` |
|    - | 2391 | ` * winning when a query selects one twice.` |
|    - | 2392 | ` */` |
|  172 | 2393 | `static int PdoRowColumnOf(phl_pdo_stmt *pSt,const char *zName,int nName)` |
|    1 | 2394 | `{` |
|  173 | 2395 | `	int nCol = PdoRowColumnCount(pSt);` |
|    - | 2396 | `	ph7_int64 iPos;` |
|    - | 2397 | `	int iCol;` |
|  173 | 2398 | `	if( nCol < 1 ){` |
|  ! 0 | 2399 | `		return -1;` |
|    - | 2400 | `	}` |
|  173 | 2401 | `	if( PdoRowIntName(zName,nName,&iPos) ){` |
|   29 | 2402 | `		return (iPos >= 0 && iPos < (ph7_int64)nCol) ? (int)iPos : -1;` |
|    - | 2403 | `	}` |
|  353 | 2404 | `	for( iCol = 0 ; iCol < nCol ; ++iCol ){` |
|    - | 2405 | `		SyBlob sHave;` |
|    - | 2406 | `		int nHave, bHit;` |
|  327 | 2407 | `		SyBlobInit(&sHave,&pSt->pConn->pVm->sAllocator);` |
|  327 | 2408 | `		PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sHave);` |
|  327 | 2409 | `		nHave = (int)SyBlobLength(&sHave) - 1;   /* less the terminator */` |
|  444 | 2410 | `		bHit = nHave == nName` |
|  326 | 2411 | `			&& SyMemcmp((const char *)SyBlobData(&sHave),zName,(sxu32)nName) == 0;` |
|  327 | 2412 | `		SyBlobRelease(&sHave);` |
|  327 | 2413 | `		if( bHit ){` |
|  119 | 2414 | `			return iCol;` |
|    - | 2415 | `		}` |
|  105 | 2416 | `	}` |
|   27 | 2417 | `	return -1;` |
|   87 | 2418 | `}` |
|    - | 2419 | `/*` |
|    - | 2420 | ` * One column's value as the SCRIPT sees it: the captured raw value with the` |
|    - | 2421 | ` * connection's presentation modifiers applied now rather than at capture, so a` |
|    - | 2422 | ` * STRINGIFY_FETCHES or ORACLE_NULLS changed between two reads shows in the` |
|    - | 2423 | ` * second -- which is what php's read-through row does.` |
|    - | 2424 | ` */` |
|  168 | 2425 | `static void PdoRowColumnValue(phl_pdo_stmt *pSt,int iCol,ph7_value *pOut)` |
|    1 | 2426 | `{` |
|  253 | 2427 | `	ph7_value *pRaw = pSt->pLazyVals` |
|  157 | 2428 | `		? PdoArrayAtInt(pSt->pConn->pVm,pSt->pLazyVals,(sxi64)iCol) : 0;` |
|  169 | 2429 | `	if( pRaw ){` |
|  147 | 2430 | `		PH7_MemObjStore(pRaw,pOut);` |
|   73 | 2431 | `	}` |
|    - | 2432 | `	/* A column with no captured value is php's null -- and ORACLE_NULLS still` |
|    - | 2433 | `	 * has its say over that null, which is why an exhausted row under` |
|    - | 2434 | `	 * NULL_TO_STRING reads the empty string rather than null. */` |
|  169 | 2435 | `	PdoApplyValueMods(pSt->pConn,pOut);` |
|  169 | 2436 | `}` |
|    - | 2437 | `/*` |
|    - | 2438 | ` * php's read_property / has_property / write_property / unset_property for the` |
|    - | 2439 | ` * row. A name that is no column at all is NULL to a read and false to an` |
|    - | 2440 | `` * isset(), never a warning -- and `queryString` is answered from the STATEMENT`` |
|    - | 2441 | ` * BEFORE any column is looked at, so a query selecting a column of that name` |
|    - | 2442 | ` * cannot shadow it. The has side does NOT know the name at all, which is why` |
|    - | 2443 | `` * `isset($row->queryString)` is false while reading it works.`` |
|    - | 2444 | ` */` |
|  168 | 2445 | `static void PdoRowProp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|    1 | 2446 | `{` |
|  169 | 2447 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|  169 | 2448 | `	const char *zName = SyStringData(pCtx->pName);` |
|  169 | 2449 | `	int nName = (int)SyStringLength(pCtx->pName);` |
|    - | 2450 | `	int iCol;` |
|   84 | 2451 | `	SXUNUSED(pVm);` |
|  169 | 2452 | `	if( pCtx->iMode == PH7_NATIVE_PROP_WRITE ){` |
|   15 | 2453 | `		pCtx->zThrowClass = "Error";` |
|   15 | 2454 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|    - | 2455 | `			"Cannot write to PDORow property");` |
|   15 | 2456 | `		return;` |
|    - | 2457 | `	}` |
|  155 | 2458 | `	if( pCtx->iMode == PH7_NATIVE_PROP_UNSET ){` |
|    5 | 2459 | `		pCtx->zThrowClass = "Error";` |
|    5 | 2460 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|    - | 2461 | `			"Cannot unset PDORow property");` |
|    5 | 2462 | `		return;` |
|    - | 2463 | `	}` |
|  151 | 2464 | `	pCtx->bAnswered = 1;` |
|  151 | 2465 | `	if( pSt == 0 ){` |
|  ! 0 | 2466 | `		return;   /* the statement is gone: every name reads null */` |
|    - | 2467 | `	}` |
|  151 | 2468 | `	if( (pCtx->iMode == PH7_NATIVE_PROP_READ) && PdoRowIsQueryString(zName,nName) ){` |
|   13 | 2469 | `		PdoRowQueryString(pSt,pCtx->pResult);` |
|   13 | 2470 | `		return;` |
|    - | 2471 | `	}` |
|  139 | 2472 | `	iCol = PdoRowColumnOf(pSt,zName,nName);` |
|  139 | 2473 | `	if( iCol >= 0 ){` |
|  121 | 2474 | `		PdoRowColumnValue(pSt,iCol,pCtx->pResult);` |
|   60 | 2475 | `	}` |
|  139 | 2476 | `	if( pCtx->iMode != PH7_NATIVE_PROP_READ ){` |
|    - | 2477 | `		/* php's has_property fetches the value and judges it -- by NULL-ness for` |
|    - | 2478 | `		 * isset() and by TRUTH for property_exists(), which asks the same handler` |
|    - | 2479 | `		 * with a non-zero check_empty. Either way it does NOT know the name` |
|    - | 2480 | ``		 * `queryString`, which is why reading one works where isset() on it is`` |
|    - | 2481 | `		 * false. */` |
|    - | 2482 | `		int bSet;` |
|   57 | 2483 | `		if( pCtx->iMode == PH7_NATIVE_PROP_EXISTS ){` |
|   33 | 2484 | `			bSet = iCol >= 0 && ph7_value_to_bool(pCtx->pResult);` |
|   17 | 2485 | `		}else{` |
|   25 | 2486 | `			bSet = iCol >= 0 && (pCtx->pResult->iFlags & MEMOBJ_NULL) == 0;` |
|    - | 2487 | `		}` |
|   57 | 2488 | `		PH7_MemObjRelease(pCtx->pResult);` |
|   57 | 2489 | `		ph7_value_bool(pCtx->pResult,bSet);` |
|   28 | 2490 | `	}` |
|   85 | 2491 | `}` |
|    - | 2492 | `/*` |
|    - | 2493 | ` * php's read_dimension / has_dimension for the row, and the three refusals its` |
|    - | 2494 | ` * write side gives. The offset is the property NAME spelled as a value: an` |
|    - | 2495 | ` * integer is a column number outright, and everything else is converted to a` |
|    - | 2496 | ` * string first -- which is where an object offset raises php's` |
|    - | 2497 | ` * "could not be converted to string" Error and an array warns.` |
|    - | 2498 | ` */` |
|   46 | 2499 | `static void PdoRowDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|    1 | 2500 | `{` |
|   47 | 2501 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|    - | 2502 | `	ph7_value sKey;` |
|    - | 2503 | `	const char *zName;` |
|    - | 2504 | `	int nName, iCol;` |
|   46 | 2505 | `	if( pCtx->iMode == PH7_NATIVE_DIM_WRITE \|\| pCtx->iMode == PH7_NATIVE_DIM_APPEND` |
|   42 | 2506 | `	 \|\| pCtx->iMode == PH7_NATIVE_DIM_UNSET ){` |
|    9 | 2507 | `		pCtx->zThrowClass = "Error";` |
|   13 | 2508 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),"Cannot %s PDORow offset",` |
|    8 | 2509 | `			pCtx->iMode == PH7_NATIVE_DIM_WRITE ? "write to"` |
|    4 | 2510 | `			: (pCtx->iMode == PH7_NATIVE_DIM_APPEND ? "append to" : "unset"));` |
|   11 | 2511 | `		return;` |
|    - | 2512 | `	}` |
|   39 | 2513 | `	if( pCtx->pOffset == 0 \|\| pSt == 0 ){` |
|  ! 0 | 2514 | ``		return;   /* `$row[]` read, or a statement that is gone: null */`` |
|    - | 2515 | `	}` |
|   39 | 2516 | `	if( pCtx->pOffset->iFlags & MEMOBJ_OBJ ){` |
|  ! 0 | 2517 | `		ph7_class_instance *pObj = (ph7_class_instance *)pCtx->pOffset->x.pOther;` |
|  ! 0 | 2518 | `		pCtx->zThrowClass = "Error";` |
|  ! 0 | 2519 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|    - | 2520 | `			"Object of class %.*s could not be converted to string",` |
|  ! 0 | 2521 | `			(int)pObj->pClass->sName.nByte,pObj->pClass->sName.zString);` |
|  ! 0 | 2522 | `		return;` |
|    - | 2523 | `	}` |
|   39 | 2524 | `	PH7_MemObjInit(pVm,&sKey);` |
|   39 | 2525 | `	PH7_MemObjStore(pCtx->pOffset,&sKey);` |
|   39 | 2526 | `	if( sKey.iFlags & MEMOBJ_HASHMAP ){` |
|  ! 0 | 2527 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|  ! 0 | 2528 | `	}` |
|   39 | 2529 | `	PH7_MemObjToString(&sKey);` |
|   39 | 2530 | `	zName = (const char *)SyBlobData(&sKey.sBlob);` |
|   39 | 2531 | `	nName = (int)SyBlobLength(&sKey.sBlob);` |
|   39 | 2532 | `	if( pCtx->iMode == PH7_NATIVE_DIM_READ && zName && PdoRowIsQueryString(zName,nName) ){` |
|    5 | 2533 | `		PdoRowQueryString(pSt,pCtx->pResult);` |
|    5 | 2534 | `		PH7_MemObjRelease(&sKey);` |
|    5 | 2535 | `		return;` |
|    - | 2536 | `	}` |
|   35 | 2537 | `	iCol = PdoRowColumnOf(pSt,zName ? zName : "",zName ? nName : 0);` |
|   35 | 2538 | `	if( iCol >= 0 ){` |
|   21 | 2539 | `		PdoRowColumnValue(pSt,iCol,pCtx->pResult);` |
|   10 | 2540 | `	}` |
|   35 | 2541 | `	if( pCtx->iMode == PH7_NATIVE_DIM_ISSET ){` |
|    7 | 2542 | `		int bSet = iCol >= 0 && (pCtx->pResult->iFlags & MEMOBJ_NULL) == 0;` |
|    7 | 2543 | `		PH7_MemObjRelease(pCtx->pResult);` |
|    7 | 2544 | `		ph7_value_bool(pCtx->pResult,bSet);` |
|    3 | 2545 | `	}` |
|   35 | 2546 | `	PH7_MemObjRelease(&sKey);` |
|   24 | 2547 | `}` |
|    - | 2548 | `/*` |
|    - | 2549 | `` * php's get_debug_info for the row: `queryString` and then every column of the`` |
|    - | 2550 | ` * row it is sitting on, which is why var_dump() shows what get_object_vars()` |
|    - | 2551 | ` * does not. The get_properties half shows nothing at all, so (array), var_export` |
|    - | 2552 | ` * and json_encode answer empty.` |
|    - | 2553 | ` */` |
|   20 | 2554 | `static sxi32 PdoRowPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|    1 | 2555 | `{` |
|   21 | 2556 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|    - | 2557 | `	ph7_value sKey,sVal;` |
|    - | 2558 | `	int nCol,iCol;` |
|   21 | 2559 | `	if( !bDebug \|\| pSt == 0 ){` |
|    7 | 2560 | `		return SXRET_OK;` |
|    - | 2561 | `	}` |
|   15 | 2562 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|   15 | 2563 | `	PH7_MemObjInit(pVm,&sVal);` |
|   15 | 2564 | `	PH7_MemObjStringAppend(&sKey,"queryString",sizeof("queryString")-1);` |
|   15 | 2565 | `	if( pSt->pOwner ){` |
|   15 | 2566 | `		ph7_value *pQs = PH7_NativeAttr(pSt->pOwner,"queryString");` |
|   15 | 2567 | `		if( pQs ){` |
|   15 | 2568 | `			PH7_MemObjStore(pQs,&sVal);` |
|    7 | 2569 | `		}` |
|    7 | 2570 | `	}` |
|   15 | 2571 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    - | 2572 | `	/* The COLUMNS come from the statement rather than from the capture: php` |
|    - | 2573 | `	 * describes them once and shows them for as long as the cursor exists, so a` |
|    - | 2574 | `	 * row whose walk has run out (or whose cursor was closed) still prints every` |
|    - | 2575 | `	 * name, each holding null. */` |
|   15 | 2576 | `	nCol = PdoRowColumnCount(pSt);` |
|   45 | 2577 | `	for( iCol = 0 ; iCol < nCol ; ++iCol ){` |
|    - | 2578 | `		SyBlob sColName;` |
|    - | 2579 | `		int nName;` |
|    - | 2580 | `		const char *zName;` |
|   31 | 2581 | `		SyBlobInit(&sColName,&pVm->sAllocator);` |
|   31 | 2582 | `		PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sColName);` |
|   31 | 2583 | `		zName = (const char *)SyBlobData(&sColName);` |
|   31 | 2584 | `		nName = (int)SyBlobLength(&sColName) - 1;   /* less the terminator */` |
|   31 | 2585 | `		if( zName == 0 \|\| nName < 0 \|\| PdoRowIsQueryString(zName,nName) ){` |
|    - | 2586 | `			/* php builds the columns as a table of their own and merges it` |
|    - | 2587 | `			 * BEHIND the queryString entry, so a column of that name is the one` |
|    - | 2588 | `			 * that loses -- while two columns sharing any other name collapse` |
|    - | 2589 | `			 * to the LAST of them, which the update below does. */` |
|    3 | 2590 | `			SyBlobRelease(&sColName);` |
|    3 | 2591 | `			continue;` |
|    - | 2592 | `		}` |
|   29 | 2593 | `		PH7_MemObjRelease(&sKey);` |
|   29 | 2594 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|   29 | 2595 | `		PH7_MemObjStringAppend(&sKey,zName,(sxu32)nName);` |
|   29 | 2596 | `		PH7_MemObjRelease(&sVal);` |
|   29 | 2597 | `		PH7_MemObjInit(pVm,&sVal);` |
|   29 | 2598 | `		PdoRowColumnValue(pSt,iCol,&sVal);` |
|   29 | 2599 | `		ph7_array_add_elem(pOut,&sKey,&sVal);` |
|   29 | 2600 | `		SyBlobRelease(&sColName);` |
|   15 | 2601 | `	}` |
|   15 | 2602 | `	PH7_MemObjRelease(&sKey);` |
|   15 | 2603 | `	PH7_MemObjRelease(&sVal);` |
|   15 | 2604 | `	return SXRET_OK;` |
|   11 | 2605 | `}` |
|    - | 2606 | `/*` |
|    - | 2607 | `` * php gives the row `zend_objects_not_comparable`: no two PDORows are ever`` |
|    - | 2608 | `` * equal, `<=>` answers the uncomparable 1 from either side, and every`` |
|    - | 2609 | `` * relational spelling is false -- `$row == $row` alone is true, and that is the`` |
|    - | 2610 | ` * engine's identity shortcut answering before any handler. A BOOL partner is` |
|    - | 2611 | ` * not this handler's business in php either: that comparison converts both` |
|    - | 2612 | `` * sides, which is why `$row == true` is true.`` |
|    - | 2613 | ` */` |
|   10 | 2614 | `static void PdoRowCmp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)` |
|    1 | 2615 | `{` |
|    5 | 2616 | `	SXUNUSED(pVm);` |
|    5 | 2617 | `	SXUNUSED(pThis);` |
|   11 | 2618 | `	if( pCtx->pOtherValue && (pCtx->pOtherValue->iFlags & MEMOBJ_BOOL) ){` |
|    3 | 2619 | `		return;   /* declined: php's cast rule decides an object against a bool */` |
|    - | 2620 | `	}` |
|    9 | 2621 | `	pCtx->bAnswered = 1;` |
|    9 | 2622 | `	pCtx->iResult = 1;   /* php's ZEND_UNCOMPARABLE, the same from both directions */` |
|    6 | 2623 | `}` |
|    - | 2624 | `/* The row is going away: the statement must stop pointing at it. */` |
|   10 | 2625 | `static void PdoRowInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    1 | 2626 | `{` |
|   11 | 2627 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|    5 | 2628 | `	SXUNUSED(pVm);` |
|   11 | 2629 | `	if( pSt && pSt->pLazyRow == pThis ){` |
|   11 | 2630 | `		pSt->pLazyRow = 0;` |
|   11 | 2631 | `		PdoStmtLazyClear(pSt);` |
|    5 | 2632 | `	}` |
|   11 | 2633 | `}` |
|    - | 2634 | `/*` |
|    - | 2635 | ` * The statement's row object, built on first use and CAPTURING the row under` |
|    - | 2636 | ` * the cursor, which it also marks as spent. Answers the object with a` |
|    - | 2637 | ` * reference of the caller's own, or 0 when it could not be made. Shared by` |
|    - | 2638 | ` * fetch() and the foreach iterator: php answers both from one lazy row.` |
|    - | 2639 | ` */` |
|   46 | 2640 | `static ph7_class_instance * PdoLazyRowFor(ph7_vm *pVm,phl_pdo_stmt *pSt)` |
|    1 | 2641 | `{` |
|   47 | 2642 | `	ph7_class_instance *pRow = pSt->pLazyRow;` |
|   47 | 2643 | `	PdoBoundColumnsForRow(pVm,pSt);` |
|   47 | 2644 | `	PdoStmtLazyCapture(pSt);` |
|   47 | 2645 | `	if( pRow == 0 ){` |
|   37 | 2646 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,"PDORow",sizeof("PDORow")-1,FALSE,0);` |
|    - | 2647 | `		SyString sAttr;` |
|    - | 2648 | `		ph7_value *pSlot;` |
|   37 | 2649 | `		pRow = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|   37 | 2650 | `		if( pRow == 0 ){` |
|  ! 0 | 2651 | `			return 0;` |
|    - | 2652 | `		}` |
|   37 | 2653 | `		SyStringInitFromBuf(&sAttr,PDOROW_RES,sizeof(PDOROW_RES)-1);` |
|   37 | 2654 | `		pSlot = PH7_ClassInstanceFetchAttr(pRow,&sAttr);` |
|   37 | 2655 | `		if( pSlot == 0 ){` |
|  ! 0 | 2656 | `			PH7_ClassInstanceUnref(pRow);` |
|  ! 0 | 2657 | `			return 0;` |
|    - | 2658 | `		}` |
|   37 | 2659 | `		PH7_MemObjRelease(pSlot);` |
|   37 | 2660 | `		pSlot->x.pOther = pSt;` |
|   37 | 2661 | `		MemObjSetType(pSlot,MEMOBJ_RES);` |
|    - | 2662 | `		/* Retain the STATEMENT object through a slot of the row's own: php's` |
|    - | 2663 | ``		 * row keeps its statement alive, so `unset($stmt)` leaves the row`` |
|    - | 2664 | `		 * reading and the database open. The statement's pointer back here is` |
|    - | 2665 | `		 * deliberately NOT a reference -- that pair would be a cycle no` |
|    - | 2666 | `		 * refcount can break. */` |
|   37 | 2667 | `		SyStringInitFromBuf(&sAttr,PDOROW_STMT,sizeof(PDOROW_STMT)-1);` |
|   37 | 2668 | `		pSlot = PH7_ClassInstanceFetchAttr(pRow,&sAttr);` |
|   37 | 2669 | `		if( pSlot && pSt->pOwner ){` |
|   37 | 2670 | `			PH7_MemObjRelease(pSlot);` |
|   37 | 2671 | `			pSt->pOwner->iRef++;` |
|   37 | 2672 | `			pSlot->x.pOther = pSt->pOwner;` |
|   37 | 2673 | `			MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|   18 | 2674 | `		}` |
|   37 | 2675 | `		pSt->pLazyRow = pRow;` |
|   19 | 2676 | `	}else{` |
|   11 | 2677 | `		pRow->iRef++;   /* the caller's own reference */` |
|    - | 2678 | `	}` |
|   47 | 2679 | `	pSt->bRowPending = 0;` |
|   47 | 2680 | `	return pRow;` |
|   24 | 2681 | `}` |
|    - | 2682 | `/*` |
|    - | 2683 | ` * PDO::FETCH_LAZY: hand the row object back and move the cursor on. The row` |
|    - | 2684 | ` * carries no values of its own -- the capture on the statement is what it` |
|    - | 2685 | ` * reads -- so a second lazy fetch answers the SAME object showing the next` |
|    - | 2686 | ` * row, which is php.` |
|    - | 2687 | ` */` |
|   36 | 2688 | `static int PdoFetchLazyRow(ph7_context *pCtx,phl_pdo_stmt *pSt)` |
|    1 | 2689 | `{` |
|    - | 2690 | `	ph7_class_instance *pRow;` |
|   37 | 2691 | `	if( !PdoStmtHasRow(pSt) ){` |
|    5 | 2692 | `		PdoStmtOk(pSt);` |
|    5 | 2693 | `		ph7_result_bool(pCtx,0);` |
|    5 | 2694 | `		return PH7_OK;` |
|    - | 2695 | `	}` |
|   33 | 2696 | `	pRow = PdoLazyRowFor(pCtx->pVm,pSt);` |
|   33 | 2697 | `	if( pRow == 0 ){` |
|  ! 0 | 2698 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2699 | `	}` |
|   33 | 2700 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2701 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2702 | `		PH7_ClassInstanceUnref(pRow);` |
|  ! 0 | 2703 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2704 | `	}` |
|   33 | 2705 | `	PdoStmtOk(pSt);` |
|   33 | 2706 | `	PH7_NativeResultObject(pCtx,pRow);` |
|   33 | 2707 | `	return PH7_OK;` |
|   19 | 2708 | `}` |
|    - | 2709 | `/*` |
|    - | 2710 | ` * PDOStatement::fetch(int $mode = PDO::FETCH_DEFAULT, ...): mixed` |
|    - | 2711 | ` *` |
|    - | 2712 | ` * FETCH_DEFAULT means the connection's ATTR_DEFAULT_FETCH_MODE, which is` |
|    - | 2713 | ` * FETCH_BOTH unless the script changed it -- so a bare fetch() answers every` |
|    - | 2714 | ` * column twice, once under its name and once under its position.` |
|    - | 2715 | ` */` |
|  344 | 2716 | `static int vm_builtin_PDOStatement_fetch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2717 | `{` |
|  346 | 2718 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2719 | `	ph7_value *pRow;` |
|    - | 2720 | `	int iMode;` |
|    - | 2721 | `	sxi32 rcFlags;` |
|  346 | 2722 | `	if( pSt == 0 ){` |
|  ! 0 | 2723 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2724 | `	}` |
|  346 | 2725 | `	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;` |
|  346 | 2726 | `	rcFlags = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::fetch",1,"mode");` |
|  346 | 2727 | `	if( rcFlags != PH7_OK ){` |
|  ! 0 | 2728 | `		return rcFlags;` |
|    - | 2729 | `	}` |
|  346 | 2730 | `	if( iMode == PDO_FETCH_DEFAULT ){` |
|  176 | 2731 | `		iMode = pSt->iFetchMode;` |
|   87 | 2732 | `	}` |
|  346 | 2733 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_DEFAULT ){` |
|    - | 2734 | `		/* A statement whose own mode is FETCH_DEFAULT -- which only a` |
|    - | 2735 | `		 * connection whose ATTR_DEFAULT_FETCH_MODE is 0 leaves it on -- has no` |
|    - | 2736 | `		 * mode to fall back to, and php says so at the fetch. */` |
|  ! 0 | 2737 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2738 | `			"PDOStatement::fetch(): Argument #1 ($mode) must be a bitmask of "` |
|    - | 2739 | `			"PDO::FETCH_* constants");` |
|    - | 2740 | `	}` |
|  346 | 2741 | `	if( PdoBoundColumnsBad(pSt) ){` |
|   11 | 2742 | `		return PdoBoundColumnsRefuse(pCtx,pSt,0);` |
|    - | 2743 | `	}` |
|  336 | 2744 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_LAZY ){` |
|   37 | 2745 | `		return PdoFetchLazyRow(pCtx,pSt);` |
|    - | 2746 | `	}` |
|  300 | 2747 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_KEY_PAIR ){` |
|    - | 2748 | `		/* php's own fetch() cannot do this mode: it builds a value var_dump` |
|    - | 2749 | `		 * crashes on and json_encode refuses, and one spelling of the same call` |
|    - | 2750 | `		 * aborts the process (§10 -- a php defect PHL does not reproduce). The` |
|    - | 2751 | `		 * honest answer is the one the mode NAMES and fetchAll() builds: the` |
|    - | 2752 | `		 * row as a single key => value pair. */` |
|    - | 2753 | `		ph7_value *pPair,*pRowVals,*pKey,*pVal;` |
|   11 | 2754 | `		if( PH7_PdoSqliteColumnCount(pSt) != 2 ){` |
|    3 | 2755 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 2756 | `				"SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires "` |
|    - | 2757 | `				"the result set to contain exactly 2 columns.");` |
|    - | 2758 | `		}` |
|    9 | 2759 | `		if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2760 | `			PdoStmtOk(pSt);` |
|    3 | 2761 | `			ph7_result_bool(pCtx,0);` |
|    3 | 2762 | `			return PH7_OK;` |
|    - | 2763 | `		}` |
|    7 | 2764 | `		pPair    = ph7_context_new_array(pCtx);` |
|    7 | 2765 | `		pRowVals = ph7_context_new_array(pCtx);` |
|    7 | 2766 | `		if( pPair == 0 \|\| pRowVals == 0 ){` |
|  ! 0 | 2767 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 2768 | `		}` |
|    7 | 2769 | `		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pRowVals) ){` |
|  ! 0 | 2770 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 2771 | `			return PH7_OK;` |
|    - | 2772 | `		}` |
|    7 | 2773 | `		pKey = PdoArrayAtInt(pCtx->pVm,pRowVals,0);` |
|    7 | 2774 | `		pVal = PdoArrayAtInt(pCtx->pVm,pRowVals,1);` |
|    7 | 2775 | `		ph7_array_add_elem(pPair,pKey,pVal);` |
|    7 | 2776 | `		ph7_result_value(pCtx,pPair);` |
|    7 | 2777 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2778 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2779 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2780 | `		}` |
|    7 | 2781 | `		PdoStmtOk(pSt);` |
|    7 | 2782 | `		return PH7_OK;` |
|    - | 2783 | `	}` |
|  289 | 2784 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_COLUMN ){` |
|    - | 2785 | `		/* a statement told to fetch one COLUMN answers that column from here` |
|    - | 2786 | `		 * on, whichever verb asks for the row */` |
|    - | 2787 | `		ph7_value *pOneRow,*pOne;` |
|   19 | 2788 | `		if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2789 | `			PdoStmtOk(pSt);` |
|    3 | 2790 | `			ph7_result_bool(pCtx,0);` |
|    3 | 2791 | `			return PH7_OK;` |
|    - | 2792 | `		}` |
|   17 | 2793 | `		if( pSt->iFetchColumn >= PH7_PdoSqliteColumnCount(pSt) ){` |
|    - | 2794 | `			/* php checks the width only once it has a ROW to read it from, so a` |
|    - | 2795 | `			 * cursor with nothing left answers false rather than refusing. */` |
|    5 | 2796 | `			return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    - | 2797 | `		}` |
|   13 | 2798 | `		pOneRow = ph7_context_new_array(pCtx);` |
|   13 | 2799 | `		if( pOneRow == 0 ){` |
|  ! 0 | 2800 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 2801 | `		}` |
|   13 | 2802 | `		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pOneRow) ){` |
|  ! 0 | 2803 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 2804 | `			return PH7_OK;` |
|    - | 2805 | `		}` |
|   13 | 2806 | `		pOne = PdoArrayAtInt(pCtx->pVm,pOneRow,(sxi64)pSt->iFetchColumn);` |
|   13 | 2807 | `		if( pOne ){` |
|   13 | 2808 | `			ph7_result_value(pCtx,pOne);` |
|    7 | 2809 | `		}else{` |
|  ! 0 | 2810 | `			ph7_result_null(pCtx);` |
|    - | 2811 | `		}` |
|   13 | 2812 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2813 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2814 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2815 | `		}` |
|   13 | 2816 | `		PdoStmtOk(pSt);` |
|   13 | 2817 | `		return PH7_OK;` |
|    - | 2818 | `	}` |
|  271 | 2819 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_BOUND ){` |
|   21 | 2820 | `		if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2821 | `			PdoStmtOk(pSt);` |
|    3 | 2822 | `			ph7_result_bool(pCtx,0);` |
|    3 | 2823 | `			return PH7_OK;` |
|    - | 2824 | `		}` |
|   19 | 2825 | `		PdoBoundColumnsForRow(pCtx->pVm,pSt);` |
|   19 | 2826 | `		pSt->bRowPending = 0;` |
|   19 | 2827 | `		ph7_result_bool(pCtx,1);` |
|   19 | 2828 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2829 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2830 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2831 | `		}` |
|   19 | 2832 | `		PdoStmtOk(pSt);` |
|   19 | 2833 | `		return PH7_OK;` |
|    - | 2834 | `	}` |
|  250 | 2835 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_CLASS` |
|  238 | 2836 | `	 \|\| (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_INTO ){` |
|   39 | 2837 | `		return PdoFetchObjectRow(pCtx,pSt,iMode,0,0,"PDOStatement::fetch");` |
|    - | 2838 | `	}` |
|  213 | 2839 | `	if( !PdoStmtHasRow(pSt) ){` |
|   21 | 2840 | `		PdoStmtOk(pSt);` |
|   21 | 2841 | `		ph7_result_bool(pCtx,0);` |
|   21 | 2842 | `		return PH7_OK;` |
|    - | 2843 | `	}` |
|  193 | 2844 | `	pRow = ph7_context_new_array(pCtx);` |
|  193 | 2845 | `	if( pRow == 0 ){` |
|  ! 0 | 2846 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2847 | `	}` |
|  193 | 2848 | `	if( !PdoStmtRow(pCtx->pVm,pSt,iMode & PDO_FETCH_MODE_MASK,pRow) ){` |
|  ! 0 | 2849 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2850 | `		return PH7_OK;` |
|    - | 2851 | `	}` |
|  193 | 2852 | `	ph7_result_value(pCtx,pRow);` |
|    - | 2853 | `	/* step ahead so the next call knows whether a row is waiting without` |
|    - | 2854 | `	 * having to ask twice */` |
|  193 | 2855 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2856 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2857 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2858 | `	}` |
|  193 | 2859 | `	PdoStmtOk(pSt);` |
|  193 | 2860 | `	return PH7_OK;` |
|  174 | 2861 | `}` |
|    - | 2862 | `/*` |
|    - | 2863 | ` * PDOStatement::getColumnMeta(int $column): array\|false` |
|    - | 2864 | ` *` |
|    - | 2865 | ` * php's eight keys. Two of them describe different things and are routinely` |
|    - | 2866 | ``  * confused: `sqlite:decl_type` is what the SCHEMA declares, and `native_type` `` |
|    - | 2867 | ` * is the type of the value in the CURRENT row -- so a TEXT column holding NULL` |
|    - | 2868 | ` * reports "TEXT" and "null" at once, and an exhausted cursor reports "null"` |
|    - | 2869 | ` * for every column.` |
|    - | 2870 | ` *` |
|    - | 2871 | ` * A column that does not exist answers false, and php reports the last STEP's` |
|    - | 2872 | ` * result code as the driver error while doing so: that is why asking for` |
|    - | 2873 | ` * column 99 while a row is up comes back as "100 another row available"` |
|    - | 2874 | ` * instead of anything about the index.` |
|    - | 2875 | ` */` |
|    8 | 2876 | `static int vm_builtin_PDOStatement_getColumnMeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2877 | `{` |
|    9 | 2878 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2879 | `	ph7_value *pMeta,*pCell,*pFlags;` |
|    - | 2880 | `	ph7_int64 iCol;` |
|    - | 2881 | `	const char *zDecl,*zTable,*zName;` |
|    - | 2882 | `	int iType,iPdoType;` |
|    - | 2883 | `	const char *zNative;` |
|    - | 2884 | `	SyBlob sName;` |
|    9 | 2885 | `	if( pSt == 0 ){` |
|  ! 0 | 2886 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2887 | `	}` |
|    9 | 2888 | `	iCol = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    9 | 2889 | `	if( iCol < 0 ){` |
|    3 | 2890 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2891 | `			"PDOStatement::getColumnMeta(): Argument #1 ($column) must be greater than "` |
|    - | 2892 | `			"or equal to 0");` |
|    - | 2893 | `	}` |
|    7 | 2894 | `	if( iCol >= (ph7_int64)PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 2895 | `		int iStep = PH7_PdoSqliteLastStepCode(pSt);` |
|    3 | 2896 | `		ph7_result_bool(pCtx,0);` |
|    - | 2897 | `		/* php reports the last STEP's code as the driver detail here, which is` |
|    - | 2898 | `		 * why an out-of-range index talks about a row being available */` |
|    4 | 2899 | `		PH7_PdoSetError(pSt->pConn,"HY000",iStep,` |
|    1 | 2900 | `			iStep == 100 ? "another row available" : "no more rows available");` |
|    3 | 2901 | `		pSt->pConn->iErrState = PDO_ERR_OK;   /* the CONNECTION did not fail */` |
|    3 | 2902 | `		SyMemcpy("00000",pSt->pConn->zSqlState,sizeof("00000"));` |
|    3 | 2903 | `		PdoStmtFailed(pSt,"HY000");` |
|    3 | 2904 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::getColumnMeta");` |
|    - | 2905 | `	}` |
|    5 | 2906 | `	pMeta = ph7_context_new_array(pCtx);` |
|    5 | 2907 | `	pCell = ph7_context_new_scalar(pCtx);` |
|    5 | 2908 | `	pFlags = ph7_context_new_array(pCtx);` |
|    5 | 2909 | `	if( pMeta == 0 \|\| pCell == 0 \|\| pFlags == 0 ){` |
|  ! 0 | 2910 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2911 | `	}` |
|    5 | 2912 | `	iType = pSt->bRowPending ? PH7_PdoSqliteColumnType(pSt,(int)iCol) : SQLITE_NULL;` |
|    5 | 2913 | `	switch( iType ){` |
|    3 | 2914 | `		case SQLITE_INTEGER: zNative = "integer"; iPdoType = PDO_PARAM_INT; break;` |
|  ! 0 | 2915 | `		case SQLITE_FLOAT:   zNative = "double";  iPdoType = PDO_PARAM_STR; break;` |
|  ! 0 | 2916 | `		case SQLITE_BLOB:    zNative = "blob";    iPdoType = PDO_PARAM_LOB; break;` |
|    3 | 2917 | `		case SQLITE_NULL:    zNative = "null";    iPdoType = PDO_PARAM_NULL; break;` |
|  ! 0 | 2918 | `		default:             zNative = "string";  iPdoType = PDO_PARAM_STR; break;` |
|    - | 2919 | `	}` |
|    5 | 2920 | `	PH7_MemObjRelease(pCell);` |
|    5 | 2921 | `	ph7_value_string(pCell,zNative,(int)SyStrlen(zNative));` |
|    5 | 2922 | `	ph7_array_add_strkey_elem(pMeta,"native_type",pCell);` |
|    5 | 2923 | `	ph7_value_int(pCell,iPdoType);` |
|    5 | 2924 | `	ph7_array_add_strkey_elem(pMeta,"pdo_type",pCell);` |
|    5 | 2925 | `	zDecl = PH7_PdoSqliteColumnDecl(pSt,(int)iCol);` |
|    5 | 2926 | `	if( zDecl ){` |
|    5 | 2927 | `		PH7_MemObjRelease(pCell);` |
|    5 | 2928 | `		ph7_value_string(pCell,zDecl,(int)SyStrlen(zDecl));` |
|    5 | 2929 | `		ph7_array_add_strkey_elem(pMeta,"sqlite:decl_type",pCell);` |
|    2 | 2930 | `	}` |
|    5 | 2931 | `	zTable = PH7_PdoSqliteColumnTable(pSt,(int)iCol);` |
|    5 | 2932 | `	if( zTable ){` |
|    5 | 2933 | `		PH7_MemObjRelease(pCell);` |
|    5 | 2934 | `		ph7_value_string(pCell,zTable,(int)SyStrlen(zTable));` |
|    5 | 2935 | `		ph7_array_add_strkey_elem(pMeta,"table",pCell);` |
|    2 | 2936 | `	}` |
|    5 | 2937 | `	ph7_array_add_strkey_elem(pMeta,"flags",pFlags);` |
|    5 | 2938 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    5 | 2939 | `	zName = PH7_PdoSqliteColumnName(pSt,(int)iCol);` |
|    5 | 2940 | `	PdoColumnName(pSt->pConn,zName,&sName);` |
|    5 | 2941 | `	PH7_MemObjRelease(pCell);` |
|    5 | 2942 | `	ph7_value_string(pCell,(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName) - 1);` |
|    5 | 2943 | `	ph7_array_add_strkey_elem(pMeta,"name",pCell);` |
|    5 | 2944 | `	SyBlobRelease(&sName);` |
|    5 | 2945 | `	ph7_value_int(pCell,-1);` |
|    5 | 2946 | `	ph7_array_add_strkey_elem(pMeta,"len",pCell);` |
|    5 | 2947 | `	ph7_value_int(pCell,0);` |
|    5 | 2948 | `	ph7_array_add_strkey_elem(pMeta,"precision",pCell);` |
|    5 | 2949 | `	ph7_result_value(pCtx,pMeta);` |
|    5 | 2950 | `	return PH7_OK;` |
|    5 | 2951 | `}` |
|    - | 2952 | `/*` |
|    - | 2953 | ` * PDOStatement::nextRowset(): bool` |
|    - | 2954 | ` *` |
|    - | 2955 | ` * sqlite has no second result set to move to, so this is the layer refusal --` |
|    - | 2956 | ` * false, and IM001 with php's own "driver does not support multiple rowsets".` |
|    - | 2957 | ` * It leaves the CURSOR alone: a fetch after it still answers the row that was` |
|    - | 2958 | ` * waiting.` |
|    - | 2959 | ` */` |
|    4 | 2960 | `static int vm_builtin_PDOStatement_nextRowset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2961 | `{` |
|    5 | 2962 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 2963 | `	SXUNUSED(nArg);` |
|    2 | 2964 | `	SXUNUSED(apArg);` |
|    5 | 2965 | `	if( pSt == 0 ){` |
|  ! 0 | 2966 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2967 | `	}` |
|    5 | 2968 | `	ph7_result_bool(pCtx,0);` |
|    5 | 2969 | `	PdoStmtFailed(pSt,"IM001");` |
|    5 | 2970 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::nextRowset","IM001",` |
|    - | 2971 | `		"driver does not support multiple rowsets");` |
|    3 | 2972 | `}` |
|    - | 2973 | `/*` |
|    - | 2974 | ` * PDOStatement::fetchColumn(int $column = 0): mixed` |
|    - | 2975 | ` *` |
|    - | 2976 | ` * One column of the next row, by position. An index outside the RESULT SET is` |
|    - | 2977 | ` * a ValueError rather than a null, and its two refusals are worded unlike` |
|    - | 2978 | ` * fetchAll()'s -- php's own inconsistency, reproduced.` |
|    - | 2979 | ` */` |
|   20 | 2980 | `static int vm_builtin_PDOStatement_fetchColumn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2981 | `{` |
|   21 | 2982 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2983 | `	ph7_int64 iCol;` |
|    - | 2984 | `	ph7_value *pRow,*pCell;` |
|   21 | 2985 | `	if( pSt == 0 ){` |
|  ! 0 | 2986 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2987 | `	}` |
|   21 | 2988 | `	iCol = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|   21 | 2989 | `	if( iCol < 0 ){` |
|    3 | 2990 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2991 | `			"Column index must be greater than or equal to 0");` |
|    - | 2992 | `	}` |
|   19 | 2993 | `	if( iCol >= (ph7_int64)PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 2994 | `		return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    - | 2995 | `	}` |
|   17 | 2996 | `	if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2997 | `		PdoStmtOk(pSt);` |
|    3 | 2998 | `		ph7_result_bool(pCtx,0);` |
|    3 | 2999 | `		return PH7_OK;` |
|    - | 3000 | `	}` |
|   15 | 3001 | `	if( PdoBoundColumnsBad(pSt) ){` |
|  ! 0 | 3002 | `		return PdoBoundColumnsRefuse(pCtx,pSt,0);` |
|    - | 3003 | `	}` |
|   15 | 3004 | `	pRow = ph7_context_new_array(pCtx);` |
|   15 | 3005 | `	if( pRow == 0 ){` |
|  ! 0 | 3006 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3007 | `	}` |
|   15 | 3008 | `	if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pRow) ){` |
|  ! 0 | 3009 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 3010 | `		return PH7_OK;` |
|    - | 3011 | `	}` |
|   15 | 3012 | `	pCell = PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)iCol);` |
|   15 | 3013 | `	if( pCell ){` |
|   15 | 3014 | `		ph7_result_value(pCtx,pCell);` |
|    8 | 3015 | `	}else{` |
|  ! 0 | 3016 | `		ph7_result_null(pCtx);` |
|    - | 3017 | `	}` |
|   15 | 3018 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 3019 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 3020 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchColumn");` |
|    - | 3021 | `	}` |
|   15 | 3022 | `	PdoStmtOk(pSt);` |
|   15 | 3023 | `	return PH7_OK;` |
|   11 | 3024 | `}` |
|    - | 3025 | `/*` |
|    - | 3026 | `` * php's `pdo_stmt_setup_fetch_mode`: the mode a statement will use from here`` |
|    - | 3027 | ` * on, and the whole screen over it. Two verbs give one: setFetchMode()'s first` |
|    - | 3028 | ` * argument and query()'s SECOND, so every diagnostic counts arguments the way` |
|    - | 3029 | `` * the verb that took them does -- `iModeArg` is the mode's own 1-based`` |
|    - | 3030 | ` * position, and the counts php reports are that position plus what the mode` |
|    - | 3031 | ` * needs beside it.` |
|    - | 3032 | ` *` |
|    - | 3033 | ` * The rules are php's, per mode: FETCH_COLUMN wants a column NUMBER and` |
|    - | 3034 | ` * FETCH_INTO an OBJECT, both exactly one; FETCH_CLASS wants a class NAME and` |
|    - | 3035 | ` * accepts constructor arguments behind it -- unless FETCH_CLASSTYPE rides on` |
|    - | 3036 | ` * it, which takes the class from the first column and therefore wants nothing;` |
|    - | 3037 | ` * FETCH_FUNC belongs to fetchAll() alone; and every other mode takes the mode` |
|    - | 3038 | `` * and nothing else. A base outside php's own enum is `must be a bitmask of`` |
|    - | 3039 | `` * PDO::FETCH_* constants`. FETCH_DEFAULT itself names the connection's`` |
|    - | 3040 | ` * ATTR_DEFAULT_FETCH_MODE and leaves the statement on it.` |
|    - | 3041 | ` */` |
|  544 | 3042 | `static sxi32 PdoSetupFetchMode(ph7_context *pCtx,phl_pdo_stmt *pSt,int nArg,ph7_value **apArg,` |
|    - | 3043 | `	int iModeArg,const char *zFn,const char *zModeParam)` |
|    2 | 3044 | `{` |
|  546 | 3045 | `	ph7_value *pMode = nArg >= iModeArg ? apArg[iModeArg-1] : 0;` |
|  546 | 3046 | `	int iMode = pMode ? (int)ph7_value_to_int64(pMode) : PDO_FETCH_DEFAULT;` |
|  546 | 3047 | `	int iBase = iMode & PDO_FETCH_MODE_MASK;` |
|  546 | 3048 | `	int nExtra = nArg - iModeArg;          /* arguments given BEHIND the mode */` |
|    - | 3049 | `	char zBuf[64];` |
|    - | 3050 | `	sxi32 rc;` |
|    - | 3051 | `	/* php clears the statement's mode BEFORE it judges the new one, and clears` |
|    - | 3052 | `	 * it to the CONNECTION's default rather than to what the statement was` |
|    - | 3053 | `	 * carrying -- so a REFUSED setFetchMode() leaves a statement that was` |
|    - | 3054 | `	 * fetching NUM answering whatever ATTR_DEFAULT_FETCH_MODE says. */` |
|  546 | 3055 | `	PdoStmtClearFetchState(pSt);` |
|  546 | 3056 | `	pSt->iFetchMode = pSt->pConn->iDefaultFetch;` |
|  546 | 3057 | `	pSt->iFetchColumn = 0;` |
|  546 | 3058 | `	if( iBase > PDO_FETCH_KEY_PAIR ){` |
|   43 | 3059 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3060 | `			"%s(): Argument #%d ($%s) must be a bitmask of PDO::FETCH_* constants",` |
|   14 | 3061 | `			zFn,iModeArg,zModeParam);` |
|    - | 3062 | `	}` |
|  518 | 3063 | `	rc = PdoCheckFetchFlags(pCtx,iMode,zFn,iModeArg,zModeParam);` |
|  518 | 3064 | `	if( rc != PH7_OK ){` |
|    3 | 3065 | `		return rc;` |
|    - | 3066 | `	}` |
|  516 | 3067 | `	if( iBase == PDO_FETCH_FUNC ){` |
|   43 | 3068 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3069 | `			"%s(): Argument #%d ($%s) PDO::FETCH_FUNC can only be used with "` |
|   14 | 3070 | `			"PDOStatement::fetchAll()",zFn,iModeArg,zModeParam);` |
|    - | 3071 | `	}` |
|  488 | 3072 | `	if( iBase == PDO_FETCH_CLASS && (iMode & PDO_FETCH_CLASSTYPE) == 0 ){` |
|    - | 3073 | `		/* The class NAME, then optional constructor arguments. php checks the` |
|    - | 3074 | `		 * TYPE of what it was handed before it counts, so a wrong second` |
|    - | 3075 | `		 * argument is a TypeError even when a fourth is there too. */` |
|   47 | 3076 | `		if( nExtra < 1 ){` |
|    7 | 3077 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3078 | `				"%s() expects at least %d arguments for the fetch mode provided, %d given",` |
|    2 | 3079 | `				zFn,iModeArg+1,nArg);` |
|    - | 3080 | `		}` |
|   43 | 3081 | `		if( (apArg[iModeArg]->iFlags & MEMOBJ_STRING) == 0 ){` |
|   19 | 3082 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3083 | `				"%s(): Argument #%d must be of type string, %s given",` |
|   12 | 3084 | `				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));` |
|    - | 3085 | `		}` |
|    - | 3086 | `		{` |
|    - | 3087 | `			/* php resolves the name HERE -- before it looks at the constructor` |
|    - | 3088 | `			 * arguments behind it -- so a class that does not exist is refused` |
|    - | 3089 | `			 * where it was named rather than at the first fetch, and one that` |
|    - | 3090 | `			 * merely cannot be instantiated is accepted here and refused there. */` |
|   31 | 3091 | `			int nCls = 0;` |
|   31 | 3092 | `			const char *zCls = ph7_value_to_string(apArg[iModeArg],&nCls);` |
|   30 | 3093 | `			if( zCls == 0 \|\| nCls < 1` |
|   31 | 3094 | `			 \|\| PH7_VmExtractClass(pCtx->pVm,zCls,(sxu32)nCls,FALSE,0) == 0 ){` |
|    4 | 3095 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|    1 | 3096 | `					"%s(): Argument #%d must be a valid class",zFn,iModeArg+1);` |
|    - | 3097 | `			}` |
|    - | 3098 | `		}` |
|   29 | 3099 | `		if( nExtra > 1 && (apArg[iModeArg+1]->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL)) == 0 ){` |
|    7 | 3100 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3101 | `				"%s(): Argument #%d must be of type ?array, %s given",` |
|    4 | 3102 | `				zFn,iModeArg+2,VmValueGivenName(apArg[iModeArg+1],zBuf,sizeof(zBuf)));` |
|    - | 3103 | `		}` |
|   25 | 3104 | `		if( nExtra > 2 ){` |
|  ! 0 | 3105 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3106 | `				"%s() expects at most %d arguments for the fetch mode provided, %d given",` |
|  ! 0 | 3107 | `				zFn,iModeArg+2,nArg);` |
|    1 | 3108 | `		}` |
|  454 | 3109 | `	}else if( iBase == PDO_FETCH_COLUMN \|\| iBase == PDO_FETCH_INTO ){` |
|  105 | 3110 | `		if( nExtra != 1 ){` |
|   67 | 3111 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3112 | `				"%s() expects exactly %d arguments for the fetch mode provided, %d given",` |
|   22 | 3113 | `				zFn,iModeArg+1,nArg);` |
|    - | 3114 | `		}` |
|    - | 3115 | `		/* php's screen is the zval's TYPE: only a real int passes, and a float` |
|    - | 3116 | `		 * whose value happens to be integral does not (the slot may carry the` |
|    - | 3117 | `		 * int flag beside the real one once something has read it as a number,` |
|    - | 3118 | `		 * so the REAL bit is what decides). */` |
|   60 | 3119 | `		if( iBase == PDO_FETCH_COLUMN` |
|   52 | 3120 | `		 && ((apArg[iModeArg]->iFlags & MEMOBJ_INT) == 0` |
|   34 | 3121 | `		  \|\| (apArg[iModeArg]->iFlags & MEMOBJ_REAL) != 0) ){` |
|   28 | 3122 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3123 | `				"%s(): Argument #%d must be of type int, %s given",` |
|   18 | 3124 | `				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));` |
|    - | 3125 | `		}` |
|   43 | 3126 | `		if( iBase == PDO_FETCH_INTO && (apArg[iModeArg]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   13 | 3127 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3128 | `				"%s(): Argument #%d must be of type object, %s given",` |
|    8 | 3129 | `				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));` |
|    - | 3130 | `		}` |
|   35 | 3131 | `		if( iBase == PDO_FETCH_COLUMN && ph7_value_to_int64(apArg[iModeArg]) < 0 ){` |
|    - | 3132 | `			/* A NEGATIVE column is refused where it is given; one merely past` |
|    - | 3133 | `` 			 * the last column is not, and answers php's `Invalid column index` `` |
|    - | 3134 | `			 * at the fetch -- the statement's width is not this screen's` |
|    - | 3135 | `			 * business. */` |
|    4 | 3136 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    1 | 3137 | `				"%s(): Argument #%d must be greater than or equal to 0",zFn,iModeArg+1);` |
|    1 | 3138 | `		}` |
|  354 | 3139 | `	}else if( nExtra > 0 ){` |
|  361 | 3140 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3141 | `			"%s() expects exactly %d arguments for the fetch mode provided, %d given",` |
|  120 | 3142 | `			zFn,iModeArg,nArg);` |
|    - | 3143 | `	}` |
|  166 | 3144 | `	if( iBase == PDO_FETCH_CLASS && (iMode & PDO_FETCH_CLASSTYPE) == 0 ){` |
|   25 | 3145 | `		int nName = 0;` |
|   25 | 3146 | `		const char *zName = ph7_value_to_string(apArg[iModeArg],&nName);` |
|   25 | 3147 | `		if( zName && nName > 0 ){` |
|   49 | 3148 | `			pSt->zFetchClass = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|   24 | 3149 | `				(sxu32)nName + 1);` |
|   25 | 3150 | `			if( pSt->zFetchClass ){` |
|   25 | 3151 | `				SyMemcpy(zName,pSt->zFetchClass,(sxu32)nName);` |
|   25 | 3152 | `				pSt->zFetchClass[nName] = 0;` |
|   25 | 3153 | `				pSt->nFetchClass = nName;` |
|   12 | 3154 | `			}` |
|   12 | 3155 | `		}` |
|   25 | 3156 | `		if( nExtra > 1 && (apArg[iModeArg+1]->iFlags & MEMOBJ_HASHMAP) ){` |
|    9 | 3157 | `			pSt->pFetchArgs = ph7_new_array(pCtx->pVm);` |
|    9 | 3158 | `			if( pSt->pFetchArgs ){` |
|    9 | 3159 | `				PH7_MemObjStore(apArg[iModeArg+1],pSt->pFetchArgs);` |
|    4 | 3160 | `			}` |
|    5 | 3161 | `		}` |
|  142 | 3162 | `	}else if( iBase == PDO_FETCH_INTO ){` |
|   11 | 3163 | `		pSt->pFetchInto = (ph7_class_instance *)apArg[iModeArg]->x.pOther;` |
|   11 | 3164 | `		pSt->pFetchInto->iRef++;   /* the statement writes into it for as long as it lives */` |
|    5 | 3165 | `	}` |
|    - | 3166 | `	/* FETCH_DEFAULT is not a mode to keep: the statement stays on the` |
|    - | 3167 | `	 * connection's default it was just cleared to (php 8.5.11, GH-20214). */` |
|  154 | 3168 | `	if( iBase != PDO_FETCH_DEFAULT ){` |
|  142 | 3169 | `		pSt->iFetchMode = iMode;` |
|   70 | 3170 | `	}` |
|  154 | 3171 | `	pSt->iFetchColumn = iBase == PDO_FETCH_COLUMN` |
|   87 | 3172 | `		? (int)ph7_value_to_int64(apArg[iModeArg]) : 0;` |
|  154 | 3173 | `	return PH7_OK;` |
|  274 | 3174 | `}` |
|    - | 3175 | `/*` |
|    - | 3176 | ` * PDOStatement::setFetchMode(int $mode, mixed ...$args): true` |
|    - | 3177 | ` *` |
|    - | 3178 | ` * The mode a bare fetch()/fetchAll() will use from here on -- one spelling of` |
|    - | 3179 | ` * the screen above, the other being PDO::query()'s second argument.` |
|    - | 3180 | ` */` |
|  296 | 3181 | `static int vm_builtin_PDOStatement_setFetchMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3182 | `{` |
|  298 | 3183 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3184 | `	sxi32 rc;` |
|  298 | 3185 | `	if( pSt == 0 ){` |
|  ! 0 | 3186 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3187 | `	}` |
|  298 | 3188 | `	rc = PdoSetupFetchMode(pCtx,pSt,nArg,apArg,1,"PDOStatement::setFetchMode","mode");` |
|  298 | 3189 | `	if( rc != PH7_OK ){` |
|  199 | 3190 | `		return rc;` |
|    - | 3191 | `	}` |
|  100 | 3192 | `	ph7_result_bool(pCtx,1);` |
|  100 | 3193 | `	return PH7_OK;` |
|  150 | 3194 | `}` |
|    - | 3195 |  |
|    - | 3196 | `/* Add one row under a key, collecting repeats into a list (FETCH_GROUP). */` |
|   18 | 3197 | `static void PdoGroupAppend(ph7_context *pCtx,ph7_value *pOut,ph7_value *pKey,ph7_value *pRow)` |
|    1 | 3198 | `{` |
|   19 | 3199 | `	ph7_value *pList = 0;` |
|   19 | 3200 | `	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){` |
|  ! 0 | 3201 | `		int nKey = 0;` |
|  ! 0 | 3202 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|  ! 0 | 3203 | `		pList = ph7_array_fetch(pOut,zKey,nKey);` |
|   19 | 3204 | `	}else if( pKey ){` |
|    - | 3205 | `		SyBlob sKey;` |
|   19 | 3206 | `		SyBlobInit(&sKey,&pCtx->pVm->sAllocator);` |
|   19 | 3207 | `		SyBlobFormat(&sKey,"%qd",ph7_value_to_int64(pKey));` |
|   19 | 3208 | `		SyBlobAppend(&sKey,"",1);` |
|   28 | 3209 | `		pList = ph7_array_fetch(pOut,(const char *)SyBlobData(&sKey),` |
|   18 | 3210 | `			(int)SyBlobLength(&sKey) - 1);` |
|   19 | 3211 | `		SyBlobRelease(&sKey);` |
|    9 | 3212 | `	}` |
|   19 | 3213 | `	if( pList && (pList->iFlags & MEMOBJ_HASHMAP) ){` |
|    7 | 3214 | `		ph7_array_add_elem(pList,0,pRow);` |
|    7 | 3215 | `		return;` |
|    - | 3216 | `	}` |
|   13 | 3217 | `	pList = ph7_context_new_array(pCtx);` |
|   13 | 3218 | `	if( pList == 0 ){` |
|  ! 0 | 3219 | `		return;` |
|    - | 3220 | `	}` |
|   13 | 3221 | `	ph7_array_add_elem(pList,0,pRow);` |
|   13 | 3222 | `	ph7_array_add_elem(pOut,pKey,pList);` |
|   10 | 3223 | `}` |
|    - | 3224 | `/*` |
|    - | 3225 | ` * PDOStatement::fetchAll(int $mode = PDO::FETCH_DEFAULT, mixed ...$args): array` |
|    - | 3226 | ` *` |
|    - | 3227 | ` * Every remaining row in one array. Four of the modes change the shape of that` |
|    - | 3228 | ` * ARRAY rather than the shape of a row: FETCH_COLUMN reduces each row to one` |
|    - | 3229 | ` * value, FETCH_KEY_PAIR to a key and a value (and refuses a result set that is` |
|    - | 3230 | ` * not exactly two columns wide), FETCH_FUNC replaces it with whatever a` |
|    - | 3231 | ` * callable answers, and GROUP/UNIQUE take the first column as a key -- GROUP` |
|    - | 3232 | ` * collecting every row under it, UNIQUE keeping the last.` |
|    - | 3233 | ` *` |
|    - | 3234 | ` * php counts arguments per mode here too, and its FETCH_FUNC wording is` |
|    - | 3235 | ` * singular ("expects exactly 2 argument"); both are reproduced as they stand.` |
|    - | 3236 | ` */` |
|   94 | 3237 | `static int vm_builtin_PDOStatement_fetchAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3238 | `{` |
|   96 | 3239 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3240 | `	ph7_value *pOut,*pRow;` |
|   96 | 3241 | `	int iMode,iBase,iCol = 0;` |
|    - | 3242 | `	int bGroup,bUnique;` |
|    - | 3243 | `	sxi32 rc;` |
|   96 | 3244 | `	if( pSt == 0 ){` |
|  ! 0 | 3245 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3246 | `	}` |
|   96 | 3247 | `	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;` |
|   96 | 3248 | `	rc = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::fetchAll",1,"mode");` |
|   96 | 3249 | `	if( rc != PH7_OK ){` |
|    3 | 3250 | `		return rc;` |
|    - | 3251 | `	}` |
|   94 | 3252 | `	bGroup = (iMode & PDO_FETCH_GROUP) == PDO_FETCH_GROUP;` |
|   94 | 3253 | `	bUnique = (iMode & PDO_FETCH_UNIQUE) == PDO_FETCH_UNIQUE;` |
|   94 | 3254 | `	iBase = iMode & PDO_FETCH_MODE_MASK;` |
|   94 | 3255 | `	if( iBase == PDO_FETCH_DEFAULT ){` |
|   19 | 3256 | `		iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;` |
|   19 | 3257 | `		bGroup = bGroup \|\| (pSt->iFetchMode & PDO_FETCH_GROUP) == PDO_FETCH_GROUP;` |
|   19 | 3258 | `		bUnique = bUnique \|\| (pSt->iFetchMode & PDO_FETCH_UNIQUE) == PDO_FETCH_UNIQUE;` |
|   19 | 3259 | `		iCol = pSt->iFetchColumn;` |
|    9 | 3260 | `	}` |
|   94 | 3261 | `	if( iBase == PDO_FETCH_DEFAULT ){` |
|  ! 0 | 3262 | `		iBase = pSt->pConn->iDefaultFetch;` |
|  ! 0 | 3263 | `	}` |
|   94 | 3264 | `	if( iBase == PDO_FETCH_COLUMN ){` |
|   17 | 3265 | `		if( nArg > 2 ){` |
|  ! 0 | 3266 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3267 | `				"PDOStatement::fetchAll() expects exactly 2 arguments for the fetch "` |
|  ! 0 | 3268 | `				"mode provided, %d given",nArg);` |
|    - | 3269 | `		}` |
|   17 | 3270 | `		if( nArg > 1 ){` |
|    7 | 3271 | `			ph7_int64 iWant = ph7_value_to_int64(apArg[1]);` |
|    7 | 3272 | `			if( iWant < 0 ){` |
|    3 | 3273 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3274 | `					"PDOStatement::fetchAll(): Argument #2 must be greater than or "` |
|    - | 3275 | `					"equal to 0");` |
|    - | 3276 | `			}` |
|    5 | 3277 | `			iCol = (int)iWant;` |
|    2 | 3278 | `		}` |
|   15 | 3279 | `		if( iCol >= PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 3280 | `			return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    1 | 3281 | `		}` |
|   84 | 3282 | `	}else if( iBase == PDO_FETCH_CLASS ){` |
|   21 | 3283 | `		if( nArg > 3 ){` |
|  ! 0 | 3284 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3285 | `				"PDOStatement::fetchAll() expects at most 3 arguments for the fetch "` |
|  ! 0 | 3286 | `				"mode provided, %d given",nArg);` |
|    1 | 3287 | `		}` |
|   68 | 3288 | `	}else if( iBase == PDO_FETCH_LAZY ){` |
|    5 | 3289 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3290 | `			"PDOStatement::fetchAll(): Argument #1 ($mode) PDO::FETCH_LAZY cannot be "` |
|    - | 3291 | `			"used with PDOStatement::fetchAll()");` |
|   54 | 3292 | `	}else if( iBase == PDO_FETCH_FUNC ){` |
|    7 | 3293 | `		if( nArg != 2 ){` |
|    4 | 3294 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3295 | `				"PDOStatement::fetchAll() expects exactly 2 argument for "` |
|    1 | 3296 | `				"PDO::FETCH_FUNC, %d given",nArg);` |
|    - | 3297 | `		}` |
|    5 | 3298 | `		if( !ph7_value_is_callable(apArg[1]) ){` |
|    - | 3299 | `			/* php checks the callable BEFORE the first row, so an unusable one` |
|    - | 3300 | `			 * is a TypeError from PDO and never the engine's own` |
|    - | 3301 | `			 * "Call to undefined function" from inside the walk */` |
|    3 | 3302 | `			int nName = 0;` |
|    3 | 3303 | `			const char *zName = ph7_value_to_string(apArg[1],&nName);` |
|    4 | 3304 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    1 | 3305 | `				"function \"%.*s\" not found or invalid function name",nName,zName);` |
|    1 | 3306 | `		}` |
|   49 | 3307 | `	}else if( nArg > 1 ){` |
|    4 | 3308 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3309 | `			"PDOStatement::fetchAll() expects exactly 1 argument for the fetch mode "` |
|    1 | 3310 | `			"provided, %d given",nArg);` |
|    - | 3311 | `	}` |
|   80 | 3312 | `	if( iBase == PDO_FETCH_KEY_PAIR && PH7_PdoSqliteColumnCount(pSt) != 2 ){` |
|    5 | 3313 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 3314 | `			"SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires "` |
|    - | 3315 | `			"the result set to contain exactly 2 columns.");` |
|    - | 3316 | `	}` |
|   76 | 3317 | `	pOut = ph7_context_new_array(pCtx);` |
|   76 | 3318 | `	if( pOut == 0 ){` |
|  ! 0 | 3319 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3320 | `	}` |
|   76 | 3321 | `	if( PdoBoundColumnsBad(pSt) ){` |
|    3 | 3322 | `		return PdoBoundColumnsRefuse(pCtx,pSt,1);` |
|    - | 3323 | `	}` |
|  248 | 3324 | `	while( PdoStmtHasRow(pSt) ){` |
|  178 | 3325 | `		int iRowMode = iBase;` |
|  178 | 3326 | `		int iFirst = (bGroup \|\| bUnique) ? 1 : 0;` |
|  176 | 3327 | `		if( iBase == PDO_FETCH_COLUMN \|\| iBase == PDO_FETCH_KEY_PAIR` |
|  137 | 3328 | `		 \|\| iBase == PDO_FETCH_FUNC \|\| iBase == PDO_FETCH_BOUND ){` |
|   64 | 3329 | `			iRowMode = PDO_FETCH_NUM;` |
|   64 | 3330 | `			iFirst = 0;` |
|  146 | 3331 | `		}else if( iBase == PDO_FETCH_CLASS ){` |
|   43 | 3332 | `			iFirst = 0;` |
|   21 | 3333 | `		}` |
|  178 | 3334 | `		pRow = ph7_context_new_array(pCtx);` |
|  178 | 3335 | `		if( pRow == 0 ){` |
|  ! 0 | 3336 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 3337 | `		}` |
|  178 | 3338 | `		if( iFirst ){` |
|    - | 3339 | `			/* the FIRST column is the key and never joins the row */` |
|   31 | 3340 | `			ph7_value *pKeyRow = ph7_context_new_array(pCtx);` |
|    - | 3341 | `			ph7_value *pKey;` |
|   31 | 3342 | `			if( pKeyRow == 0 ){` |
|  ! 0 | 3343 | `				return PH7_ContextMemoryError(pCtx);` |
|    - | 3344 | `			}` |
|   31 | 3345 | `			if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pKeyRow,0) ){` |
|  ! 0 | 3346 | `				break;` |
|    - | 3347 | `			}` |
|   31 | 3348 | `			pKey = PdoArrayAtInt(pCtx->pVm,pKeyRow,0);` |
|    - | 3349 | `			/* the row itself is rebuilt from the SECOND column on; the cursor` |
|    - | 3350 | `			 * has not moved, so this reads the same sqlite row again */` |
|   31 | 3351 | `			pSt->bRowPending = 1;` |
|   31 | 3352 | `			if( !PdoStmtRowFrom(pCtx->pVm,pSt,iRowMode,pRow,1) ){` |
|  ! 0 | 3353 | `				break;` |
|    - | 3354 | `			}` |
|   31 | 3355 | `			if( bUnique ){` |
|   13 | 3356 | `				ph7_array_add_elem(pOut,pKey,pRow);` |
|    7 | 3357 | `			}else{` |
|   19 | 3358 | `				PdoGroupAppend(pCtx,pOut,pKey,pRow);` |
|    1 | 3359 | `			}` |
|  163 | 3360 | `		}else if( iBase == PDO_FETCH_CLASS ){` |
|    - | 3361 | `			/* every row is its own instance; the class and its constructor` |
|    - | 3362 | `			 * arguments are the same for all of them */` |
|    - | 3363 | `			ph7_class *pClass;` |
|    - | 3364 | `			ph7_value sObj;` |
|    - | 3365 | `			sxi32 rcCls;` |
|   43 | 3366 | `			int iFirstCol = 0;` |
|   43 | 3367 | `			if( nArg > 1 ){` |
|   27 | 3368 | `				pClass = PdoResolveFetchClass(pCtx,apArg[1],FALSE,&rcCls);` |
|   30 | 3369 | `			}else if( iMode & PDO_FETCH_CLASSTYPE ){` |
|   13 | 3370 | `				ph7_value *pHead = ph7_context_new_array(pCtx);` |
|   13 | 3371 | `				if( pHead == 0 ){` |
|  ! 0 | 3372 | `					return PH7_ContextMemoryError(pCtx);` |
|    - | 3373 | `				}` |
|   13 | 3374 | `				if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pHead,0) ){` |
|  ! 0 | 3375 | `					break;` |
|    - | 3376 | `				}` |
|   13 | 3377 | `				pSt->bRowPending = 1;` |
|   13 | 3378 | `				pClass = PdoClassTypeClass(pCtx,PdoArrayAtInt(pCtx->pVm,pHead,0));` |
|   13 | 3379 | `				rcCls = PH7_OK;` |
|   13 | 3380 | `				iFirstCol = 1;` |
|   11 | 3381 | `			}else if( pSt->zFetchClass ){` |
|    - | 3382 | `				ph7_value sName;` |
|    - | 3383 | `				SyString sStr;` |
|  ! 0 | 3384 | `				SyStringInitFromBuf(&sStr,pSt->zFetchClass,pSt->nFetchClass);` |
|  ! 0 | 3385 | `				PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|  ! 0 | 3386 | `				pClass = PdoResolveFetchClass(pCtx,&sName,FALSE,&rcCls);` |
|  ! 0 | 3387 | `				PH7_MemObjRelease(&sName);` |
|  ! 0 | 3388 | `			}else{` |
|    5 | 3389 | `				pClass = PH7_VmExtractClass(pCtx->pVm,"stdClass",sizeof("stdClass")-1,` |
|    - | 3390 | `					FALSE,0);` |
|    5 | 3391 | `				rcCls = PH7_OK;` |
|    - | 3392 | `			}` |
|   43 | 3393 | `			if( pClass == 0 ){` |
|    3 | 3394 | `				return rcCls;` |
|    - | 3395 | `			}` |
|   41 | 3396 | `			rcCls = PH7_VmCheckInstantiable(pCtx,pClass);` |
|   41 | 3397 | `			if( rcCls != PH7_OK ){` |
|  ! 0 | 3398 | `				return rcCls;` |
|    - | 3399 | `			}` |
|   41 | 3400 | `			PH7_MemObjInit(pCtx->pVm,&sObj);` |
|   41 | 3401 | `			if( !PdoRowIntoObject(pCtx->pVm,pSt,pClass,nArg > 2 ? apArg[2] : 0,` |
|   40 | 3402 | `				(iMode & PDO_FETCH_PROPS_LATE) != 0,iFirstCol,&sObj) ){` |
|  ! 0 | 3403 | `				PH7_MemObjRelease(&sObj);` |
|  ! 0 | 3404 | `				break;` |
|    - | 3405 | `			}` |
|   41 | 3406 | `			ph7_array_add_elem(pOut,0,&sObj);` |
|   41 | 3407 | `			PH7_MemObjRelease(&sObj);` |
|  126 | 3408 | `		}else if( iBase == PDO_FETCH_BOUND ){` |
|    - | 3409 | `			/* the row goes into the BOUND VARIABLES, not into the result: the` |
|    - | 3410 | `			 * array collects one true per row and the caller reads the last` |
|    - | 3411 | `			 * row's values out of its own variables */` |
|    - | 3412 | `			ph7_value *pTrue;` |
|   11 | 3413 | `			PdoBoundColumnsForRow(pCtx->pVm,pSt);` |
|   11 | 3414 | `			pSt->bRowPending = 0;` |
|   11 | 3415 | `			pTrue = ph7_context_new_scalar(pCtx);` |
|   11 | 3416 | `			if( pTrue ){` |
|   11 | 3417 | `				ph7_value_bool(pTrue,1);` |
|   11 | 3418 | `				ph7_array_add_elem(pOut,0,pTrue);` |
|    6 | 3419 | `			}` |
|  101 | 3420 | `		}else if( !PdoStmtRow(pCtx->pVm,pSt,iRowMode,pRow) ){` |
|  ! 0 | 3421 | `			break;` |
|   96 | 3422 | `		}else if( iBase == PDO_FETCH_COLUMN ){` |
|   37 | 3423 | `			ph7_array_add_elem(pOut,0,PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)iCol));` |
|   78 | 3424 | `		}else if( iBase == PDO_FETCH_KEY_PAIR ){` |
|   17 | 3425 | `			ph7_array_add_elem(pOut,PdoArrayAtInt(pCtx->pVm,pRow,0),` |
|    5 | 3426 | `				PdoArrayAtInt(pCtx->pVm,pRow,1));` |
|   54 | 3427 | `		}else if( iBase == PDO_FETCH_FUNC ){` |
|    - | 3428 | `			ph7_value sRes;` |
|    - | 3429 | `			ph7_value *apCall[32];` |
|    7 | 3430 | `			int n,nCall = PH7_PdoSqliteColumnCount(pSt);` |
|    7 | 3431 | `			if( nCall > (int)SX_ARRAYSIZE(apCall) ){` |
|  ! 0 | 3432 | `				nCall = (int)SX_ARRAYSIZE(apCall);` |
|  ! 0 | 3433 | `			}` |
|   19 | 3434 | `			for( n = 0 ; n < nCall ; ++n ){` |
|   13 | 3435 | `				apCall[n] = PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)n);` |
|    7 | 3436 | `			}` |
|    7 | 3437 | `			PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    7 | 3438 | `			if( PH7_VmCallUserFunction(pCtx->pVm,apArg[1],nCall,apCall,&sRes) != SXRET_OK ){` |
|  ! 0 | 3439 | `				PH7_MemObjRelease(&sRes);` |
|  ! 0 | 3440 | `				return PH7_OK;   /* whatever the callable raised is already in flight */` |
|    - | 3441 | `			}` |
|    7 | 3442 | `			ph7_array_add_elem(pOut,0,&sRes);` |
|    7 | 3443 | `			PH7_MemObjRelease(&sRes);` |
|    4 | 3444 | `		}else{` |
|   43 | 3445 | `			ph7_array_add_elem(pOut,0,pRow);` |
|    - | 3446 | `		}` |
|  176 | 3447 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 3448 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 3449 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchAll");` |
|    - | 3450 | `		}` |
|    2 | 3451 | `	}` |
|   72 | 3452 | `	PdoStmtOk(pSt);` |
|   72 | 3453 | `	ph7_result_value(pCtx,pOut);` |
|   72 | 3454 | `	return PH7_OK;` |
|   49 | 3455 | `}` |
|    - | 3456 | `/*` |
|    - | 3457 | ` * PDOStatement::columnCount(): int` |
|    - | 3458 | ` *` |
|    - | 3459 | ` * 0 for a statement that returns no rows -- and also for one that has been` |
|    - | 3460 | ` * PREPARED but not yet run, even though sqlite already knows the count from` |
|    - | 3461 | ` * the compile. php only publishes it once the statement has executed, so` |
|    - | 3462 | `` * `prepare('SELECT 1')->columnCount()` is 0 and not 1.`` |
|    - | 3463 | ` */` |
|    4 | 3464 | `static int vm_builtin_PDOStatement_columnCount(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3465 | `{` |
|    5 | 3466 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 3467 | `	SXUNUSED(nArg);` |
|    2 | 3468 | `	SXUNUSED(apArg);` |
|    5 | 3469 | `	if( pSt == 0 ){` |
|  ! 0 | 3470 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3471 | `	}` |
|    5 | 3472 | `	ph7_result_int(pCtx,pSt->bExecuted ? PH7_PdoSqliteColumnCount(pSt) : 0);` |
|    5 | 3473 | `	return PH7_OK;` |
|    3 | 3474 | `}` |
|    - | 3475 | `/*` |
|    - | 3476 | ` * PDOStatement::debugDumpParams(): ?bool` |
|    - | 3477 | ` *` |
|    - | 3478 | ` * php's own diagnostic dump, printed rather than returned (it answers null).` |
|    - | 3479 | ` * The bindings appear in the order they were MADE, and the two kinds report` |
|    - | 3480 | ` * differently: a positional one carries its 0-based paramno and an empty name,` |
|    - | 3481 | ` * a named one carries paramno -1 and the name WITH its colon. Both lengths are` |
|    - | 3482 | `` * printed in brackets, php's `[%d]` shape.`` |
|    - | 3483 | ` */` |
|    4 | 3484 | `static int vm_builtin_PDOStatement_debugDumpParams(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3485 | `{` |
|    5 | 3486 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3487 | `	phl_pdo_bind *pB;` |
|    5 | 3488 | `	const char *zSql = "";` |
|    5 | 3489 | `	int nSql = 0,nBind = 0;` |
|    - | 3490 | `	ph7_value *pQuery;` |
|    2 | 3491 | `	SXUNUSED(nArg);` |
|    2 | 3492 | `	SXUNUSED(apArg);` |
|    5 | 3493 | `	if( pSt == 0 ){` |
|  ! 0 | 3494 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3495 | `	}` |
|    5 | 3496 | `	pQuery = pSt->pOwner ? PH7_NativeAttr(pSt->pOwner,"queryString") : 0;` |
|    5 | 3497 | `	if( pQuery ){` |
|    5 | 3498 | `		zSql = ph7_value_to_string(pQuery,&nSql);` |
|    2 | 3499 | `	}` |
|    9 | 3500 | `	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){` |
|    5 | 3501 | `		++nBind;` |
|    3 | 3502 | `	}` |
|    5 | 3503 | `	ph7_context_output_format(pCtx,"SQL: [%d] %.*s\n",nSql,nSql,zSql);` |
|    5 | 3504 | `	ph7_context_output_format(pCtx,"Params:  %d\n",nBind);` |
|    - | 3505 | `	/* the list is built by prepending, so walking it backwards is what puts` |
|    - | 3506 | `	 * the bindings back in the order the script made them */` |
|    - | 3507 | `	{` |
|    - | 3508 | `		phl_pdo_bind *apBind[64];` |
|    5 | 3509 | `		int n = 0,i;` |
|    9 | 3510 | `		for( pB = pSt->pBinds ; pB && n < (int)SX_ARRAYSIZE(apBind) ; pB = pB->pNext ){` |
|    5 | 3511 | `			apBind[n++] = pB;` |
|    3 | 3512 | `		}` |
|    9 | 3513 | `		for( i = n - 1 ; i >= 0 ; --i ){` |
|    5 | 3514 | `			pB = apBind[i];` |
|    5 | 3515 | `			if( pB->zName ){` |
|    4 | 3516 | `				ph7_context_output_format(pCtx,"Key: Name: [%d] %.*s\n",` |
|    1 | 3517 | `					pB->nName,pB->nName,pB->zName);` |
|    3 | 3518 | `				ph7_context_output_format(pCtx,"paramno=-1\n");` |
|    4 | 3519 | `				ph7_context_output_format(pCtx,"name=[%d] \"%.*s\"\n",` |
|    1 | 3520 | `					pB->nName,pB->nName,pB->zName);` |
|    2 | 3521 | `			}else{` |
|    3 | 3522 | `				ph7_context_output_format(pCtx,"Key: Position #%d:\n",pB->iPos - 1);` |
|    3 | 3523 | `				ph7_context_output_format(pCtx,"paramno=%d\n",pB->iPos - 1);` |
|    3 | 3524 | `				ph7_context_output_format(pCtx,"name=[0] \"\"\n");` |
|    - | 3525 | `			}` |
|    5 | 3526 | `			ph7_context_output_format(pCtx,"is_param=1\n");` |
|    5 | 3527 | `			ph7_context_output_format(pCtx,"param_type=%d\n",pB->iType & ~PDO_PARAM_FLAGS);` |
|    3 | 3528 | `		}` |
|    - | 3529 | `	}` |
|    5 | 3530 | `	ph7_result_null(pCtx);` |
|    5 | 3531 | `	return PH7_OK;` |
|    3 | 3532 | `}` |
|    - | 3533 | `/*` |
|    - | 3534 | ` * PDOStatement::getAttribute(int $name): mixed` |
|    - | 3535 | ` *` |
|    - | 3536 | ` * Two of the driver's attributes describe a STATEMENT rather than the` |
|    - | 3537 | ` * connection -- whether it only reads, and whether it is mid-walk -- and both` |
|    - | 3538 | ` * are sqlite's own answers about the compiled statement. Everything else is` |
|    - | 3539 | ` * the same IM001 refusal the connection gives.` |
|    - | 3540 | ` */` |
|    4 | 3541 | `static int vm_builtin_PDOStatement_getAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3542 | `{` |
|    5 | 3543 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3544 | `	ph7_int64 iAttr;` |
|    5 | 3545 | `	if( pSt == 0 ){` |
|  ! 0 | 3546 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3547 | `	}` |
|    5 | 3548 | `	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    5 | 3549 | `	if( iAttr == PDO_SQLITE_ATTR_READONLY_STATEMENT ){` |
|    3 | 3550 | `		ph7_result_bool(pCtx,PH7_PdoSqliteStmtReadonly(pSt));` |
|    3 | 3551 | `		return PH7_OK;` |
|    - | 3552 | `	}` |
|    3 | 3553 | `	if( iAttr == PDO_SQLITE_ATTR_BUSY_STATEMENT ){` |
|    3 | 3554 | `		ph7_result_bool(pCtx,PH7_PdoSqliteStmtBusy(pSt));` |
|    3 | 3555 | `		return PH7_OK;` |
|    - | 3556 | `	}` |
|  ! 0 | 3557 | `	ph7_result_null(pCtx);` |
|  ! 0 | 3558 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::getAttribute","IM001",` |
|    - | 3559 | `		"driver does not support that attribute");` |
|    3 | 3560 | `}` |
|    - | 3561 | `/*` |
|    - | 3562 | ` * PDOStatement::setAttribute(int $attribute, mixed $value): bool` |
|    - | 3563 | ` *` |
|    - | 3564 | ` * This driver carries no SETTABLE statement attribute at all, so every one of` |
|    - | 3565 | ` * them is the same IM001 refusal.` |
|    - | 3566 | ` */` |
|  ! 0 | 3567 | `static int vm_builtin_PDOStatement_setAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 3568 | `{` |
|  ! 0 | 3569 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|  ! 0 | 3570 | `	SXUNUSED(nArg);` |
|  ! 0 | 3571 | `	SXUNUSED(apArg);` |
|  ! 0 | 3572 | `	if( pSt == 0 ){` |
|  ! 0 | 3573 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3574 | `	}` |
|  ! 0 | 3575 | `	ph7_result_bool(pCtx,0);` |
|  ! 0 | 3576 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::setAttribute","IM001",` |
|    - | 3577 | `		"driver does not support that attribute");` |
|  ! 0 | 3578 | `}` |
|    - | 3579 | `/*` |
|    - | 3580 | ` * PDOStatement::rowCount(): int` |
|    - | 3581 | ` *` |
|    - | 3582 | ` * The number of rows a WRITE changed. It is not the size of a result set --` |
|    - | 3583 | ` * sqlite cannot know that without walking it -- so a SELECT answers 0, which` |
|    - | 3584 | ` * is php's answer and the reason its manual warns against this method.` |
|    - | 3585 | ` */` |
|    6 | 3586 | `static int vm_builtin_PDOStatement_rowCount(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3587 | `{` |
|    7 | 3588 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    3 | 3589 | `	SXUNUSED(nArg);` |
|    3 | 3590 | `	SXUNUSED(apArg);` |
|    7 | 3591 | `	if( pSt == 0 ){` |
|  ! 0 | 3592 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3593 | `	}` |
|    7 | 3594 | `	ph7_result_int64(pCtx,pSt->nChanges);` |
|    7 | 3595 | `	return PH7_OK;` |
|    4 | 3596 | `}` |
|    - | 3597 | `/*` |
|    - | 3598 | ` * PDOStatement::closeCursor(): bool` |
|    - | 3599 | ` *` |
|    - | 3600 | ` * Frees the rows a statement is still holding without discarding the statement` |
|    - | 3601 | ` * itself: php answers true and leaves the object reusable, and a fetch after` |
|    - | 3602 | ` * it answers false.` |
|    - | 3603 | ` */` |
|    4 | 3604 | `static int vm_builtin_PDOStatement_closeCursor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3605 | `{` |
|    5 | 3606 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 3607 | `	SXUNUSED(nArg);` |
|    2 | 3608 | `	SXUNUSED(apArg);` |
|    5 | 3609 | `	if( pSt == 0 ){` |
|  ! 0 | 3610 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3611 | `	}` |
|    5 | 3612 | `	if( pSt->pStmt ){` |
|    5 | 3613 | `		sqlite3_reset(pSt->pStmt);` |
|    2 | 3614 | `	}` |
|    5 | 3615 | `	pSt->bRowPending = 0;` |
|    5 | 3616 | `	pSt->bDone = 1;` |
|    - | 3617 | `	/* php frees the row's columns with the cursor, so a lazy object still in a` |
|    - | 3618 | `	 * variable answers null from here on. */` |
|    5 | 3619 | `	PdoStmtLazyClear(pSt);` |
|    5 | 3620 | `	ph7_result_bool(pCtx,1);` |
|    5 | 3621 | `	return PH7_OK;` |
|    3 | 3622 | `}` |
|    - | 3623 | `/* A statement reports its CONNECTION's error state; php keeps one per` |
|    - | 3624 | ` * statement, and every path that sets one sets both. */` |
|    2 | 3625 | `static int vm_builtin_PDOStatement_errorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3626 | `{` |
|    3 | 3627 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    1 | 3628 | `	SXUNUSED(nArg);` |
|    1 | 3629 | `	SXUNUSED(apArg);` |
|    3 | 3630 | `	if( pSt == 0 ){` |
|  ! 0 | 3631 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3632 | `	}` |
|    3 | 3633 | `	if( pSt->iErrState == PDO_ERR_NONE ){` |
|  ! 0 | 3634 | `		ph7_result_null(pCtx);` |
|  ! 0 | 3635 | `	}else{` |
|    3 | 3636 | `		ph7_result_string(pCtx,pSt->zSqlState,(int)SyStrlen(pSt->zSqlState));` |
|    - | 3637 | `	}` |
|    3 | 3638 | `	return PH7_OK;` |
|    2 | 3639 | `}` |
|    2 | 3640 | `static int vm_builtin_PDOStatement_errorInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3641 | `{` |
|    3 | 3642 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    1 | 3643 | `	SXUNUSED(nArg);` |
|    1 | 3644 | `	SXUNUSED(apArg);` |
|    3 | 3645 | `	if( pSt == 0 ){` |
|  ! 0 | 3646 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3647 | `	}` |
|    3 | 3648 | `	return PdoBuildErrorInfo(pCtx,pSt->pConn,pSt->iErrState,pSt->zSqlState);` |
|    2 | 3649 | `}` |
|    - | 3650 | `/*` |
|    - | 3651 | ` * The InternalIterator a foreach over a statement walks.  A statement is a` |
|    - | 3652 | ` * forward cursor, so REWIND does not rewind: it settles on whatever row is` |
|    - | 3653 | ` * pending, which is why a second foreach over the same statement walks nothing` |
|    - | 3654 | ` * at all rather than repeating the set.` |
|    - | 3655 | ` */` |
|  120 | 3656 | `static void PdoStmtIterSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 3657 | `{` |
|  121 | 3658 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|  121 | 3659 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(pSrc);` |
|    - | 3660 | `	ph7_value *pRow;` |
|  121 | 3661 | `	if( pSt == 0 \|\| !PdoStmtHasRow(pSt) ){` |
|   49 | 3662 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|   49 | 3663 | `		return;` |
|    - | 3664 | `	}` |
|    - | 3665 | `	{` |
|    - | 3666 | `		/* A foreach honours the statement's mode, the whole of it: php walks a` |
|    - | 3667 | `		 * LAZY statement with its one row object, a CLASS or INTO one with the` |
|    - | 3668 | `		 * objects those modes build, a COLUMN one with that column's value and` |
|    - | 3669 | ``		 * a BOUND one with `true` per row (the values having gone to the bound`` |
|    - | 3670 | `		 * variables). Only the four row SHAPES are what PdoStmtRow answers. */` |
|   73 | 3671 | `		int iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;` |
|    - | 3672 | `		ph7_value sCur;` |
|   73 | 3673 | `		int bHave = 0;` |
|   73 | 3674 | `		PH7_MemObjInit(pVm,&sCur);` |
|   73 | 3675 | `		if( iBase == PDO_FETCH_LAZY ){` |
|   15 | 3676 | `			ph7_class_instance *pLazy = PdoLazyRowFor(pVm,pSt);` |
|   15 | 3677 | `			if( pLazy ){` |
|   15 | 3678 | `				sCur.x.pOther = pLazy;` |
|   15 | 3679 | `				MemObjSetType(&sCur,MEMOBJ_OBJ);` |
|   15 | 3680 | `				bHave = 1;   /* the reference PdoLazyRowFor took is this value's */` |
|    8 | 3681 | `			}` |
|   66 | 3682 | `		}else if( iBase == PDO_FETCH_BOUND ){` |
|    - | 3683 | `			/* the row IS the bound variables: nothing else reads it, so the` |
|    - | 3684 | `			 * write happens here rather than inside a row build */` |
|    5 | 3685 | `			PdoBoundColumnsForRow(pVm,pSt);` |
|    5 | 3686 | `			pSt->bRowPending = 0;` |
|    5 | 3687 | `			ph7_value_bool(&sCur,1);` |
|    5 | 3688 | `			bHave = 1;` |
|   57 | 3689 | `		}else if( iBase == PDO_FETCH_COLUMN ){` |
|    5 | 3690 | `			ph7_value *pNumRow = ph7_new_array(pVm);` |
|    5 | 3691 | `			if( pNumRow ){` |
|    5 | 3692 | `				if( PdoStmtRowFrom(pVm,pSt,PDO_FETCH_NUM,pNumRow,0) ){` |
|    5 | 3693 | `					ph7_value *pOne = PdoArrayAtInt(pVm,pNumRow,(sxi64)pSt->iFetchColumn);` |
|    5 | 3694 | `					if( pOne ){` |
|    5 | 3695 | `						PH7_MemObjStore(pOne,&sCur);` |
|    2 | 3696 | `					}` |
|    5 | 3697 | `					bHave = 1;` |
|    2 | 3698 | `				}` |
|    5 | 3699 | `				ph7_release_value(pVm,pNumRow);` |
|    3 | 3700 | `			}` |
|   53 | 3701 | `		}else if( iBase == PDO_FETCH_CLASS \|\| iBase == PDO_FETCH_INTO ){` |
|   17 | 3702 | `			ph7_class *pClass = 0;` |
|   17 | 3703 | `			int iFirst = 0;` |
|   17 | 3704 | `			if( iBase == PDO_FETCH_INTO ){` |
|    5 | 3705 | `				if( pSt->pFetchInto ){` |
|    5 | 3706 | `					ph7_value *pRowVals = ph7_new_array(pVm);` |
|    5 | 3707 | `					if( pRowVals && PdoStmtRowFrom(pVm,pSt,PDO_FETCH_ASSOC,pRowVals,0) ){` |
|    5 | 3708 | `						PdoWriteRowProps(pVm,pSt->pFetchInto,pRowVals);` |
|    5 | 3709 | `						pSt->pFetchInto->iRef++;` |
|    5 | 3710 | `						sCur.x.pOther = pSt->pFetchInto;` |
|    5 | 3711 | `						MemObjSetType(&sCur,MEMOBJ_OBJ);` |
|    5 | 3712 | `						bHave = 1;` |
|    2 | 3713 | `					}` |
|    5 | 3714 | `					if( pRowVals ){` |
|    5 | 3715 | `						ph7_release_value(pVm,pRowVals);` |
|    2 | 3716 | `					}` |
|    2 | 3717 | `				}` |
|    3 | 3718 | `			}else{` |
|   13 | 3719 | `				if( pSt->iFetchMode & PDO_FETCH_CLASSTYPE ){` |
|    - | 3720 | `					/* the FIRST column names the class and leaves the row */` |
|    5 | 3721 | `					ph7_value *pHead = ph7_new_array(pVm);` |
|    5 | 3722 | `					if( pHead && PdoStmtRowFrom(pVm,pSt,PDO_FETCH_NUM,pHead,0) ){` |
|    5 | 3723 | `						pSt->bRowPending = 1;   /* the cursor has not moved */` |
|    5 | 3724 | `						pClass = PdoIterClassOf(pVm,PdoArrayAtInt(pVm,pHead,0));` |
|    5 | 3725 | `						iFirst = 1;` |
|    2 | 3726 | `					}` |
|    5 | 3727 | `					if( pHead ){` |
|    5 | 3728 | `						ph7_release_value(pVm,pHead);` |
|    3 | 3729 | `					}` |
|   11 | 3730 | `				}else if( pSt->zFetchClass ){` |
|   13 | 3731 | `					pClass = PH7_VmExtractClass(pVm,pSt->zFetchClass,` |
|    8 | 3732 | `						(sxu32)pSt->nFetchClass,FALSE,0);` |
|    4 | 3733 | `				}` |
|   19 | 3734 | `				if( pClass && PdoRowIntoObject(pVm,pSt,pClass,pSt->pFetchArgs,` |
|   12 | 3735 | `						(pSt->iFetchMode & PDO_FETCH_PROPS_LATE) != 0,iFirst,&sCur) ){` |
|   13 | 3736 | `					bHave = 1;` |
|    6 | 3737 | `				}` |
|    - | 3738 | `			}` |
|    8 | 3739 | `		}` |
|   73 | 3740 | `		if( bHave ){` |
|   58 | 3741 | `			PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,` |
|   19 | 3742 | `				(int)SyStrlen(PH7_NATIVE_IT_CUR),&sCur);` |
|   39 | 3743 | `			PH7_MemObjRelease(&sCur);` |
|   58 | 3744 | `			PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,` |
|   19 | 3745 | `				PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));` |
|   39 | 3746 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|   39 | 3747 | `			PdoStmtStep(pSt);` |
|   39 | 3748 | `			return;` |
|    - | 3749 | `		}` |
|   35 | 3750 | `		PH7_MemObjRelease(&sCur);` |
|   34 | 3751 | `		if( iBase != PDO_FETCH_ASSOC && iBase != PDO_FETCH_NUM && iBase != PDO_FETCH_BOTH` |
|   16 | 3752 | `		 && iBase != PDO_FETCH_OBJ && iBase != PDO_FETCH_NAMED ){` |
|    - | 3753 | `			/* a mode with nothing to hand out ends the walk */` |
|  ! 0 | 3754 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|  ! 0 | 3755 | `			return;` |
|    - | 3756 | `		}` |
|    - | 3757 | `	}` |
|   35 | 3758 | `	pRow = ph7_new_array(pVm);` |
|   35 | 3759 | `	if( pRow == 0 \|\| !PdoStmtRow(pVm,pSt,pSt->iFetchMode,pRow) ){` |
|  ! 0 | 3760 | `		if( pRow ){` |
|  ! 0 | 3761 | `			ph7_release_value(pVm,pRow);` |
|  ! 0 | 3762 | `		}` |
|  ! 0 | 3763 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|  ! 0 | 3764 | `		return;` |
|    - | 3765 | `	}` |
|   35 | 3766 | `	PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,(int)SyStrlen(PH7_NATIVE_IT_CUR),pRow);` |
|   35 | 3767 | `	ph7_release_value(pVm,pRow);` |
|   52 | 3768 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,` |
|   17 | 3769 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));` |
|   35 | 3770 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|   35 | 3771 | `	PdoStmtStep(pSt);` |
|   61 | 3772 | `}` |
|    - | 3773 | `/*` |
|    - | 3774 | ` * A rewind that does NOT rewind, and must not even re-read: the iterator is` |
|    - | 3775 | `` * built already positioned and `foreach` rewinds it again, so a settle here`` |
|    - | 3776 | ` * would swallow the first row. The AUX slot records that the first row has` |
|    - | 3777 | ` * been taken; every later rewind is a no-op, which is also what makes a SECOND` |
|    - | 3778 | ` * foreach over the same statement walk nothing at all -- php's answer, because` |
|    - | 3779 | ` * the cursor is forward-only and has nowhere to go back to.` |
|    - | 3780 | ` */` |
|   94 | 3781 | `static void PdoStmtIterRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 3782 | `{` |
|   95 | 3783 | `	if( PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_AUX) != 0 ){` |
|   47 | 3784 | `		return;` |
|    - | 3785 | `	}` |
|   49 | 3786 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_AUX,1);` |
|   49 | 3787 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|   49 | 3788 | `	PdoStmtIterSettle(&(*pVm),pIt);` |
|   48 | 3789 | `}` |
|   72 | 3790 | `static void PdoStmtIterNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 3791 | `{` |
|  109 | 3792 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|   72 | 3793 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|   73 | 3794 | `	PdoStmtIterSettle(&(*pVm),pIt);` |
|   73 | 3795 | `}` |
|    - | 3796 | `static const PH7_NativeIterVtab sPdoStmtIterVtab = {` |
|    - | 3797 | `	PdoStmtIterRewind, PdoStmtIterNext, 0, PdoStmtIterGuard };` |
|   58 | 3798 | `static int vm_builtin_PDOStatement_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3799 | `{` |
|   59 | 3800 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - | 3801 | `	ph7_class_instance *pIt;` |
|   29 | 3802 | `	SXUNUSED(nArg);` |
|   29 | 3803 | `	SXUNUSED(apArg);` |
|   59 | 3804 | `	if( pThis == 0 ){` |
|  ! 0 | 3805 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement::getIterator() needs a receiver");` |
|    - | 3806 | `	}` |
|   59 | 3807 | `	if( PdoStmtWalkRefusal(pCtx,PdoStmtOfInstance(pThis)) ){` |
|    - | 3808 | ``		/* php refuses at the DOOR as well as at every step: `getIterator()` on a`` |
|    - | 3809 | `		 * statement it cannot walk raises there, before an iterator exists. */` |
|   11 | 3810 | `		return PH7_OK;` |
|    - | 3811 | `	}` |
|   49 | 3812 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|   49 | 3813 | `	if( pIt == 0 ){` |
|  ! 0 | 3814 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3815 | `	}` |
|   49 | 3816 | `	PH7_NativeResultObject(pCtx,pIt);` |
|   49 | 3817 | `	return PH7_OK;` |
|   30 | 3818 | `}` |
|    - | 3819 | `/*` |
|    - | 3820 | ` * Record one binding.  A name is kept as the script spelled it -- with or` |
|    - | 3821 | ` * without its colon -- because the resolution happens at execute(), when the` |
|    - | 3822 | ` * statement that knows the names exists.` |
|    - | 3823 | ` */` |
|   32 | 3824 | `static phl_pdo_bind * PdoBindAdd(phl_pdo_stmt *pSt,const char *zName,int nName,int iPos,` |
|    - | 3825 | `	int iType)` |
|    2 | 3826 | `{` |
|   34 | 3827 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
|    - | 3828 | `	phl_pdo_bind *pB;` |
|    - | 3829 | `	/* php REPLACES a binding for the same parameter rather than stacking one */` |
|   48 | 3830 | `	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){` |
|   29 | 3831 | `		if( zName ? (pB->zName && pB->nName == nName` |
|    1 | 3832 | `		             && SyMemcmp(pB->zName,zName,(sxu32)nName) == 0)` |
|   12 | 3833 | `		          : (pB->zName == 0 && pB->iPos == iPos) ){` |
|  ! 0 | 3834 | `			if( pB->pVal ){` |
|  ! 0 | 3835 | `				ph7_release_value(pVm,pB->pVal);` |
|  ! 0 | 3836 | `				pB->pVal = 0;` |
|  ! 0 | 3837 | `			}` |
|  ! 0 | 3838 | `			pB->iType = iType;` |
|  ! 0 | 3839 | `			pB->nSlot = SXU32_HIGH;` |
|  ! 0 | 3840 | `			return pB;` |
|    - | 3841 | `		}` |
|    9 | 3842 | `	}` |
|   34 | 3843 | `	pB = (phl_pdo_bind *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo_bind));` |
|   34 | 3844 | `	if( pB == 0 ){` |
|  ! 0 | 3845 | `		return 0;` |
|    - | 3846 | `	}` |
|   34 | 3847 | `	SyZero(pB,sizeof(phl_pdo_bind));` |
|   34 | 3848 | `	pB->iPos = iPos;` |
|   34 | 3849 | `	pB->iType = iType;` |
|   34 | 3850 | `	pB->nSlot = SXU32_HIGH;` |
|   34 | 3851 | `	if( zName && nName > 0 ){` |
|    6 | 3852 | `		pB->zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nName + 1);` |
|    6 | 3853 | `		if( pB->zName == 0 ){` |
|  ! 0 | 3854 | `			SyMemBackendFree(&pVm->sAllocator,pB);` |
|  ! 0 | 3855 | `			return 0;` |
|    - | 3856 | `		}` |
|    6 | 3857 | `		SyMemcpy(zName,pB->zName,(sxu32)nName);` |
|    6 | 3858 | `		pB->zName[nName] = 0;` |
|    6 | 3859 | `		pB->nName = nName;` |
|    2 | 3860 | `	}` |
|   34 | 3861 | `	pB->pNext = pSt->pBinds;` |
|   34 | 3862 | `	pSt->pBinds = pB;` |
|   34 | 3863 | `	return pB;` |
|   18 | 3864 | `}` |
|    - | 3865 | `/*` |
|    - | 3866 | ` * The shared body of bindValue() and bindParam(): they differ only in WHEN the` |
|    - | 3867 | ` * value is read. Argument #1 is a name or a 1-based position, and php refuses` |
|    - | 3868 | ` * position 0 by ValueError before the statement is consulted at all.` |
|    - | 3869 | ` */` |
|   34 | 3870 | `static int PdoBindArgument(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFn,` |
|    - | 3871 | `	int bByRef)` |
|    2 | 3872 | `{` |
|   36 | 3873 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3874 | `	phl_pdo_bind *pB;` |
|    - | 3875 | `	ph7_value *pKey;` |
|    - | 3876 | `	int iType;` |
|   36 | 3877 | `	if( pSt == 0 ){` |
|  ! 0 | 3878 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3879 | `	}` |
|   36 | 3880 | `	pKey = nArg > 0 ? apArg[0] : 0;` |
|   36 | 3881 | `	iType = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : PDO_PARAM_STR;` |
|   38 | 3882 | `	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){` |
|    6 | 3883 | `		int nName = 0;` |
|    6 | 3884 | `		const char *zName = ph7_value_to_string(pKey,&nName);` |
|    6 | 3885 | `		pB = PdoBindAdd(pSt,zName,nName,0,iType);` |
|    4 | 3886 | `	}else{` |
|   32 | 3887 | `		ph7_int64 iPos = pKey ? ph7_value_to_int64(pKey) : 0;` |
|   32 | 3888 | `		if( iPos < 1 ){` |
|    4 | 3889 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    1 | 3890 | `				"%s(): Argument #1 ($param) must be greater than or equal to 1",zFn);` |
|    - | 3891 | `		}` |
|   30 | 3892 | `		pB = PdoBindAdd(pSt,0,0,(int)iPos,iType);` |
|    - | 3893 | `	}` |
|   34 | 3894 | `	if( pB == 0 ){` |
|  ! 0 | 3895 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3896 | `	}` |
|   34 | 3897 | `	if( bByRef ){` |
|    - | 3898 | `		/* bindParam(): remember the caller's SLOT, so a write to that variable` |
|    - | 3899 | `		 * after this call is the value execute() runs with. The engine hands a` |
|    - | 3900 | `		 * by-reference argument as the caller's own memobj, and its index is` |
|    - | 3901 | `		 * how every other deferred read here finds it again. */` |
|    3 | 3902 | `		pB->nSlot = (nArg > 1 && apArg[1]) ? apArg[1]->nIdx : SXU32_HIGH;` |
|   33 | 3903 | `	}else if( nArg > 1 ){` |
|    - | 3904 | `		/* bindValue(): the statement takes its own copy now */` |
|   32 | 3905 | `		pB->pVal = ph7_new_scalar(pCtx->pVm);` |
|   32 | 3906 | `		if( pB->pVal == 0 ){` |
|  ! 0 | 3907 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 3908 | `		}` |
|   32 | 3909 | `		PH7_MemObjStore(apArg[1],pB->pVal);` |
|   15 | 3910 | `	}` |
|   34 | 3911 | `	ph7_result_bool(pCtx,1);` |
|   34 | 3912 | `	return PH7_OK;` |
|   19 | 3913 | `}` |
|   32 | 3914 | `static int vm_builtin_PDOStatement_bindValue(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3915 | `{` |
|   34 | 3916 | `	return PdoBindArgument(pCtx,nArg,apArg,"PDOStatement::bindValue",FALSE);` |
|    2 | 3917 | `}` |
|    2 | 3918 | `static int vm_builtin_PDOStatement_bindParam(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3919 | `{` |
|    3 | 3920 | `	return PdoBindArgument(pCtx,nArg,apArg,"PDOStatement::bindParam",TRUE);` |
|    1 | 3921 | `}` |
|    - | 3922 | `/* Bind one recorded parameter, resolving a name against the live statement. */` |
|   28 | 3923 | `static int PdoBindApply(ph7_vm *pVm,phl_pdo_stmt *pSt,phl_pdo_bind *pB)` |
|    1 | 3924 | `{` |
|   29 | 3925 | `	ph7_value *pVal = pB->pVal;` |
|   29 | 3926 | `	int iPos = pB->iPos;` |
|   29 | 3927 | `	if( pB->zName ){` |
|    3 | 3928 | `		iPos = PH7_PdoSqliteBindIndexOf(pSt,pB->zName,pB->nName);` |
|    1 | 3929 | `	}` |
|   29 | 3930 | `	if( pB->nSlot != SXU32_HIGH ){` |
|    - | 3931 | `		/* bindParam(): read the caller's variable NOW */` |
|    3 | 3932 | `		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pB->nSlot);` |
|    1 | 3933 | `	}` |
|   29 | 3934 | `	return PH7_PdoSqliteBindAt(pSt,iPos,pB->iType,pVal);` |
|    1 | 3935 | `}` |
|    - | 3936 | `/*` |
|    - | 3937 | `` * execute()'s `?array $params`: php binds the array INSTEAD of whatever was`` |
|    - | 3938 | ` * recorded, an integer key naming a 1-based position (so element 0 is` |
|    - | 3939 | ` * parameter 1) and a string key naming a placeholder.` |
|    - | 3940 | ` */` |
|   26 | 3941 | `static int PdoBindFromArray(ph7_vm *pVm,phl_pdo_stmt *pSt,ph7_value *pArray)` |
|    1 | 3942 | `{` |
|    - | 3943 | `	ph7_hashmap *pMap;` |
|    - | 3944 | `	ph7_hashmap_node *pEntry;` |
|    - | 3945 | `	sxu32 n,nCount;` |
|   27 | 3946 | `	int rc = 1;` |
|   27 | 3947 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 3948 | `		return 1;` |
|    - | 3949 | `	}` |
|   27 | 3950 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|   27 | 3951 | `	nCount = pMap->nEntry;` |
|   27 | 3952 | `	pEntry = pMap->pFirst;` |
|   53 | 3953 | `	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 3954 | `		ph7_value sKey;` |
|   35 | 3955 | `		ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|    - | 3956 | `		int iPos;` |
|   35 | 3957 | `		PH7_MemObjInit(pVm,&sKey);` |
|   35 | 3958 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   35 | 3959 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   21 | 3960 | `			iPos = (int)sKey.x.iVal + 1;` |
|   11 | 3961 | `		}else{` |
|   15 | 3962 | `			int nName = 0;` |
|   15 | 3963 | `			const char *zName = ph7_value_to_string(&sKey,&nName);` |
|   15 | 3964 | `			iPos = PH7_PdoSqliteBindIndexOf(pSt,zName,nName);` |
|    - | 3965 | `		}` |
|   35 | 3966 | `		PH7_MemObjRelease(&sKey);` |
|    - | 3967 | `		/* php binds every element as a STRING unless the script said otherwise` |
|    - | 3968 | `		 * through bindValue(); a php null still binds as NULL. */` |
|   35 | 3969 | `		if( !PH7_PdoSqliteBindAt(pSt,iPos,PDO_PARAM_STR,pVal) ){` |
|    9 | 3970 | `			rc = 0;` |
|    9 | 3971 | `			break;` |
|    - | 3972 | `		}` |
|   14 | 3973 | `	}` |
|   27 | 3974 | `	return rc;` |
|   14 | 3975 | `}` |
|    - | 3976 | `/*` |
|    - | 3977 | ` * PDOStatement::execute(?array $params = null): bool` |
|    - | 3978 | ` *` |
|    - | 3979 | ` * Runs the statement from the start: the cursor is rewound, the previous run's` |
|    - | 3980 | ` * values are dropped, the parameters are bound and one step is taken -- the` |
|    - | 3981 | ` * same first step query() takes, so columnCount() and the first fetch() behave` |
|    - | 3982 | ` * identically whichever verb produced the statement.` |
|    - | 3983 | ` */` |
|   52 | 3984 | `static int vm_builtin_PDOStatement_execute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3985 | `{` |
|   54 | 3986 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|   54 | 3987 | `	int bOk = 1;` |
|   54 | 3988 | `	if( pSt == 0 ){` |
|  ! 0 | 3989 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3990 | `	}` |
|   54 | 3991 | `	PH7_PdoTouch(pSt->pConn);` |
|   54 | 3992 | `	PH7_PdoSqliteReset(pSt);` |
|   54 | 3993 | `	pSt->bRowPending = 0;` |
|   54 | 3994 | `	pSt->bDone = 0;` |
|   54 | 3995 | `	if( nArg > 0 && apArg[0] && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){` |
|   27 | 3996 | `		bOk = PdoBindFromArray(pCtx->pVm,pSt,apArg[0]);` |
|   14 | 3997 | `	}else{` |
|    - | 3998 | `		phl_pdo_bind *pB;` |
|   56 | 3999 | `		for( pB = pSt->pBinds ; pB && bOk ; pB = pB->pNext ){` |
|   29 | 4000 | `			bOk = PdoBindApply(pCtx->pVm,pSt,pB);` |
|   15 | 4001 | `		}` |
|    - | 4002 | `	}` |
|   54 | 4003 | `	if( !bOk ){` |
|    9 | 4004 | `		ph7_result_bool(pCtx,0);` |
|    9 | 4005 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|    9 | 4006 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::execute");` |
|    - | 4007 | `	}` |
|   46 | 4008 | `	pSt->bExecuted = 1;` |
|   46 | 4009 | `	if( PdoStmtStep(pSt) < 0 ){` |
|    3 | 4010 | `		ph7_result_bool(pCtx,0);` |
|    3 | 4011 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|    3 | 4012 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::execute");` |
|    - | 4013 | `	}` |
|   44 | 4014 | `	PdoStmtOk(pSt);` |
|   44 | 4015 | `	pSt->nChanges = PH7_PdoSqliteColumnCount(pSt) > 0` |
|   23 | 4016 | `		? 0 : PH7_PdoSqliteChanges(pSt->pConn);` |
|   44 | 4017 | `	ph7_result_bool(pCtx,1);` |
|   44 | 4018 | `	return PH7_OK;` |
|   28 | 4019 | `}` |
|    - | 4020 | `/*` |
|    - | 4021 | ` * The class query()/prepare() builds.  ATTR_STATEMENT_CLASS replaces` |
|    - | 4022 | ` * PDOStatement with a subclass of the script's own, and php builds THAT for` |
|    - | 4023 | ` * every statement the connection makes from then on.` |
|    - | 4024 | ` */` |
|    - | 4025 | `/*` |
|    - | 4026 | ` * php builds the statement OBJECT itself and then calls the class's own` |
|    - | 4027 | ` * constructor with the arguments ATTR_STATEMENT_CLASS was given -- and refuses` |
|    - | 4028 | ` * outright when there are arguments and no constructor to take them.` |
|    - | 4029 | ` */` |
|  918 | 4030 | `static sxi32 PdoStatementCtor(ph7_context *pCtx,phl_pdo *pConn,ph7_class *pClass,` |
|    - | 4031 | `	ph7_class_instance *pObj)` |
|    3 | 4032 | `{` |
|  921 | 4033 | `	ph7_class_method *pCons = PH7_ClassExtractMethod(pClass,"__construct",` |
|    - | 4034 | `		sizeof("__construct")-1);` |
|  921 | 4035 | `	if( pCons == 0 ){` |
|  913 | 4036 | `		if( pConn->pStmtArgs ){` |
|    3 | 4037 | `			return PH7_VmThrowException(pCtx,"Error",` |
|    - | 4038 | `				"User-supplied statement does not accept constructor arguments");` |
|    - | 4039 | `		}` |
|  911 | 4040 | `		return PH7_OK;` |
|    - | 4041 | `	}` |
|    9 | 4042 | `	PdoCallCtor(pCtx->pVm,pObj,pCons,pConn->pStmtArgs);` |
|    9 | 4043 | `	return PH7_OK;` |
|  462 | 4044 | `}` |
|  918 | 4045 | `static ph7_class * PdoStatementClass(ph7_context *pCtx,phl_pdo *pConn)` |
|    3 | 4046 | `{` |
|  921 | 4047 | `	if( pConn->zStmtClass ){` |
|   32 | 4048 | `		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,pConn->zStmtClass,` |
|   20 | 4049 | `			(sxu32)pConn->nStmtClass,FALSE,0);` |
|   22 | 4050 | `		if( pClass ){` |
|   22 | 4051 | `			return pClass;` |
|    - | 4052 | `		}` |
|  ! 0 | 4053 | `	}` |
|  901 | 4054 | `	return PH7_VmExtractClass(pCtx->pVm,"PDOStatement",sizeof("PDOStatement")-1,FALSE,0);` |
|  462 | 4055 | `}` |
|    - | 4056 | `/*` |
|    - | 4057 | ` * PDO::prepare(string $query, array $options = []): PDOStatement\|false` |
|    - | 4058 | ` *` |
|    - | 4059 | ` * Compiles without running. The options array is php's per-statement` |
|    - | 4060 | ` * attribute set; the sqlite driver carries none of the ones a script can put` |
|    - | 4061 | ` * there, and php ignores an unusable one rather than refusing the call.` |
|    - | 4062 | ` */` |
|   66 | 4063 | `static int vm_builtin_PDO_prepare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 4064 | `{` |
|   68 | 4065 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 4066 | `	phl_pdo_stmt *pSt;` |
|    - | 4067 | `	ph7_class *pClass;` |
|    - | 4068 | `	ph7_class_instance *pObj;` |
|    - | 4069 | `	const char *zSql;` |
|   68 | 4070 | `	int nSql = 0;` |
|   68 | 4071 | `	if( pConn == 0 ){` |
|  ! 0 | 4072 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4073 | `	}` |
|   68 | 4074 | `	PH7_PdoTouch(pConn);` |
|   68 | 4075 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|   68 | 4076 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 | 4077 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 4078 | `			"PDO::prepare(): Argument #1 ($query) must not be empty");` |
|    - | 4079 | `	}` |
|   66 | 4080 | `	pSt = PH7_PdoNewStmt(pConn);` |
|   66 | 4081 | `	if( pSt == 0 ){` |
|  ! 0 | 4082 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4083 | `	}` |
|   66 | 4084 | `	if( !PH7_PdoSqlitePrepare(pSt,zSql,nSql) ){` |
|    3 | 4085 | `		ph7_result_bool(pCtx,0);` |
|    3 | 4086 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::prepare");` |
|    - | 4087 | `	}` |
|   64 | 4088 | `	pClass = PdoStatementClass(pCtx,pConn);` |
|   64 | 4089 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|   64 | 4090 | `	if( pObj == 0 ){` |
|  ! 0 | 4091 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4092 | `	}` |
|   64 | 4093 | `	if( PdoStmtAttach(pObj,pSt) != 0 ){` |
|  ! 0 | 4094 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4095 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4096 | `	}` |
|   64 | 4097 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,"queryString",zSql,nSql);` |
|    - | 4098 | `	{` |
|   64 | 4099 | `		sxi32 rcCtor = PdoStatementCtor(pCtx,pConn,pClass,pObj);` |
|   64 | 4100 | `		if( rcCtor != PH7_OK ){` |
|  ! 0 | 4101 | `			PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4102 | `			return rcCtor;` |
|    - | 4103 | `		}` |
|    - | 4104 | `	}` |
|   64 | 4105 | `	PH7_NativeResultObject(pCtx,pObj);` |
|   64 | 4106 | `	return PH7_OK;` |
|   35 | 4107 | `}` |
|    - | 4108 | `/*` |
|    - | 4109 | ` * PDO::quote(string $string, int $type = PDO::PARAM_STR): string\|false` |
|    - | 4110 | ` *` |
|    - | 4111 | ` * sqlite's own quoting: single quotes around it, each embedded quote doubled.` |
|    - | 4112 | ` * A NUL byte has no spelling inside a sqlite literal at all, so php refuses` |
|    - | 4113 | ` * one -- with a bare sentence carrying no SQLSTATE, unlike every other` |
|    - | 4114 | ` * PDOException this driver raises.` |
|    - | 4115 | ` */` |
|   10 | 4116 | `static int vm_builtin_PDO_quote(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4117 | `{` |
|   11 | 4118 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 4119 | `	const char *zIn;` |
|   11 | 4120 | `	int nIn = 0,i;` |
|    - | 4121 | `	SyBlob sOut;` |
|   11 | 4122 | `	if( pConn == 0 ){` |
|  ! 0 | 4123 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4124 | `	}` |
|   11 | 4125 | `	PH7_PdoTouch(pConn);` |
|   11 | 4126 | `	zIn = nArg > 0 ? ph7_value_to_string(apArg[0],&nIn) : "";` |
|   31 | 4127 | `	for( i = 0 ; i < nIn ; ++i ){` |
|   23 | 4128 | `		if( zIn[i] == 0 ){` |
|    3 | 4129 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4130 | `				"SQLite PDO::quote does not support null bytes");` |
|    - | 4131 | `		}` |
|   11 | 4132 | `	}` |
|    9 | 4133 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    9 | 4134 | `	SyBlobAppend(&sOut,"'",1);` |
|   27 | 4135 | `	for( i = 0 ; i < nIn ; ++i ){` |
|   19 | 4136 | `		if( zIn[i] == '\'' ){` |
|    3 | 4137 | `			SyBlobAppend(&sOut,"'",1);` |
|    1 | 4138 | `		}` |
|   19 | 4139 | `		SyBlobAppend(&sOut,&zIn[i],1);` |
|   10 | 4140 | `	}` |
|    9 | 4141 | `	SyBlobAppend(&sOut,"'",1);` |
|    9 | 4142 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    9 | 4143 | `	SyBlobRelease(&sOut);` |
|    9 | 4144 | `	return PH7_OK;` |
|    6 | 4145 | `}` |
|    - | 4146 | `/*` |
|    - | 4147 | ` * PDO::query(string $query, ...): PDOStatement\|false` |
|    - | 4148 | ` *` |
|    - | 4149 | `` * Prepares and runs ONE statement -- what follows a `;` is compiled but never`` |
|    - | 4150 | ` * executed, unlike exec(), which runs them all.` |
|    - | 4151 | ` */` |
|  866 | 4152 | `static int vm_builtin_PDO_query(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 4153 | `{` |
|  869 | 4154 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 4155 | `	phl_pdo_stmt *pSt;` |
|    - | 4156 | `	ph7_class *pClass;` |
|    - | 4157 | `	ph7_class_instance *pObj;` |
|    - | 4158 | `	const char *zSql;` |
|  869 | 4159 | `	int nSql = 0;` |
|  869 | 4160 | `	if( pConn == 0 ){` |
|  ! 0 | 4161 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4162 | `	}` |
|  869 | 4163 | `	PH7_PdoTouch(pConn);` |
|  869 | 4164 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|  869 | 4165 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 | 4166 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 4167 | `			"PDO::query(): Argument #1 ($query) must not be empty");` |
|    - | 4168 | `	}` |
|  867 | 4169 | `	pSt = PH7_PdoNewStmt(pConn);` |
|  867 | 4170 | `	if( pSt == 0 ){` |
|  ! 0 | 4171 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4172 | `	}` |
|  867 | 4173 | `	if( !PH7_PdoSqlitePrepare(pSt,zSql,nSql) ){` |
|    7 | 4174 | `		ph7_result_bool(pCtx,0);` |
|    7 | 4175 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::query");` |
|    - | 4176 | `	}` |
|  861 | 4177 | `	pSt->bExecuted = 1;` |
|  861 | 4178 | `	if( PdoStmtStep(pSt) < 0 ){` |
|    3 | 4179 | `		ph7_result_bool(pCtx,0);` |
|    3 | 4180 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::query");` |
|    - | 4181 | `	}` |
|    - | 4182 | `	/* the change count is read once, here: a later statement on the same` |
|    - | 4183 | `	 * connection would otherwise move what this one reports */` |
|  859 | 4184 | `	pSt->nChanges = PH7_PdoSqliteColumnCount(pSt) > 0` |
|  431 | 4185 | `		? 0 : PH7_PdoSqliteChanges(pConn);` |
|  859 | 4186 | `	pClass = PdoStatementClass(pCtx,pConn);` |
|  859 | 4187 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|  859 | 4188 | `	if( pObj == 0 ){` |
|  ! 0 | 4189 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4190 | `	}` |
|  859 | 4191 | `	if( PdoStmtAttach(pObj,pSt) != 0 ){` |
|  ! 0 | 4192 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4193 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4194 | `	}` |
|  859 | 4195 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,"queryString",zSql,nSql);` |
|    - | 4196 | `	{` |
|  859 | 4197 | `		sxi32 rcCtor = PdoStatementCtor(pCtx,pConn,pClass,pObj);` |
|  859 | 4198 | `		if( rcCtor != PH7_OK ){` |
|    3 | 4199 | `			PH7_ClassInstanceUnref(pObj);` |
|    3 | 4200 | `			return rcCtor;` |
|    - | 4201 | `		}` |
|    - | 4202 | `	}` |
|  857 | 4203 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    - | 4204 | `		/* php's second argument IS setFetchMode(), run on the statement this` |
|    - | 4205 | `		 * call just built -- same screen, same per-mode arity, and diagnostics` |
|    - | 4206 | `		 * that count from PDO::query()'s own signature. A refusal leaves the` |
|    - | 4207 | `		 * statement behind (php's does too), so it is raised after the object` |
|    - | 4208 | `		 * exists rather than before the query runs. */` |
|  250 | 4209 | `		sxi32 rcMode = PdoSetupFetchMode(pCtx,pSt,nArg,apArg,2,"PDO::query","fetchMode");` |
|  250 | 4210 | `		if( rcMode != PH7_OK ){` |
|  195 | 4211 | `			PH7_ClassInstanceUnref(pObj);` |
|  195 | 4212 | `			return rcMode;` |
|    - | 4213 | `		}` |
|   27 | 4214 | `	}` |
|  663 | 4215 | `	PdoStmtOk(pSt);   /* it ran, and it ran cleanly */` |
|    - | 4216 | `	/* PH7_NativeResultObject takes the reference this call made: unref'ing` |
|    - | 4217 | `	 * again here frees the object the result slot is still holding. */` |
|  663 | 4218 | `	PH7_NativeResultObject(pCtx,pObj);` |
|  663 | 4219 | `	return PH7_OK;` |
|  436 | 4220 | `}` |
|    - | 4221 |  |
|    - | 4222 | `/* ------------------------------------------------------------------------` |
|    - | 4223 | ` * Transactions` |
|    - | 4224 | ` * ------------------------------------------------------------------------ */` |
|    - | 4225 | `/*` |
|    - | 4226 | ` * PDO::beginTransaction(): bool / commit() / rollBack() / inTransaction()` |
|    - | 4227 | ` *` |
|    - | 4228 | ` * Whether a transaction is open is sqlite's own autocommit flag and not a` |
|    - | 4229 | ` * count this driver keeps, so a BEGIN the script sent through exec() is` |
|    - | 4230 | ` * indistinguishable from beginTransaction() -- inTransaction() answers true` |
|    - | 4231 | ` * for it and a second beginTransaction() refuses.` |
|    - | 4232 | ` *` |
|    - | 4233 | ` * The three refusals are bare sentences with no SQLSTATE in front of them,` |
|    - | 4234 | ` * which is unlike every other PDOException the driver raises; and the four` |
|    - | 4235 | ` * verbs are the ones that do NOT clear the handle's error on entry.` |
|    - | 4236 | ` */` |
|   28 | 4237 | `static int PdoTxRun(ph7_context *pCtx,const char *zSql,const char *zFn)` |
|    1 | 4238 | `{` |
|   29 | 4239 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|   29 | 4240 | `	if( pConn == 0 ){` |
|  ! 0 | 4241 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4242 | `	}` |
|   29 | 4243 | `	if( PH7_PdoSqliteExec(pConn,zSql,(int)SyStrlen(zSql)) < 0 ){` |
|  ! 0 | 4244 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4245 | `		return PH7_PdoRaise(pCtx,pConn,zFn);` |
|    - | 4246 | `	}` |
|   29 | 4247 | `	ph7_result_bool(pCtx,1);` |
|   29 | 4248 | `	return PH7_OK;` |
|   15 | 4249 | `}` |
|   16 | 4250 | `static int vm_builtin_PDO_beginTransaction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4251 | `{` |
|   17 | 4252 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|   17 | 4253 | `	const char *zBegin = "BEGIN";` |
|    8 | 4254 | `	SXUNUSED(nArg);` |
|    8 | 4255 | `	SXUNUSED(apArg);` |
|   17 | 4256 | `	if( pConn == 0 ){` |
|  ! 0 | 4257 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4258 | `	}` |
|   17 | 4259 | `	if( PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 4260 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4261 | `			"There is already an active transaction");` |
|    - | 4262 | `	}` |
|    - | 4263 | `	/* which BEGIN, per Pdo\Sqlite::ATTR_TRANSACTION_MODE */` |
|   15 | 4264 | `	if( pConn->iTxMode == 1 ){` |
|    3 | 4265 | `		zBegin = "BEGIN IMMEDIATE";` |
|   14 | 4266 | `	}else if( pConn->iTxMode == 2 ){` |
|    3 | 4267 | `		zBegin = "BEGIN EXCLUSIVE";` |
|    1 | 4268 | `	}` |
|   15 | 4269 | `	return PdoTxRun(pCtx,zBegin,"PDO::beginTransaction");` |
|    9 | 4270 | `}` |
|   12 | 4271 | `static int vm_builtin_PDO_commit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4272 | `{` |
|   13 | 4273 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    6 | 4274 | `	SXUNUSED(nArg);` |
|    6 | 4275 | `	SXUNUSED(apArg);` |
|   13 | 4276 | `	if( pConn == 0 ){` |
|  ! 0 | 4277 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4278 | `	}` |
|   13 | 4279 | `	if( !PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 4280 | `		return PH7_VmThrowException(pCtx,"PDOException","There is no active transaction");` |
|    - | 4281 | `	}` |
|   11 | 4282 | `	return PdoTxRun(pCtx,"COMMIT","PDO::commit");` |
|    7 | 4283 | `}` |
|    6 | 4284 | `static int vm_builtin_PDO_rollBack(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4285 | `{` |
|    7 | 4286 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    3 | 4287 | `	SXUNUSED(nArg);` |
|    3 | 4288 | `	SXUNUSED(apArg);` |
|    7 | 4289 | `	if( pConn == 0 ){` |
|  ! 0 | 4290 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4291 | `	}` |
|    7 | 4292 | `	if( !PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 4293 | `		return PH7_VmThrowException(pCtx,"PDOException","There is no active transaction");` |
|    - | 4294 | `	}` |
|    5 | 4295 | `	return PdoTxRun(pCtx,"ROLLBACK","PDO::rollBack");` |
|    4 | 4296 | `}` |
|   10 | 4297 | `static int vm_builtin_PDO_inTransaction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4298 | `{` |
|   11 | 4299 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    5 | 4300 | `	SXUNUSED(nArg);` |
|    5 | 4301 | `	SXUNUSED(apArg);` |
|   11 | 4302 | `	if( pConn == 0 ){` |
|  ! 0 | 4303 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4304 | `	}` |
|   11 | 4305 | `	ph7_result_bool(pCtx,PH7_PdoSqliteInTransaction(pConn));` |
|   11 | 4306 | `	return PH7_OK;` |
|    6 | 4307 | `}` |
|    - | 4308 |  |
|    - | 4309 | `/* ------------------------------------------------------------------------` |
|    - | 4310 | ` * Connecting` |
|    - | 4311 | ` * ------------------------------------------------------------------------ */` |
|    - | 4312 | `/*` |
|    - | 4313 | `` * Apply the constructor's `?array $options`.  php walks it before the driver`` |
|    - | 4314 | ` * sees the handle, so an ATTR_ERRMODE in there is already in force when a` |
|    - | 4315 | ` * later failure is routed -- and a key no driver knows is ignored in silence.` |
|    - | 4316 | ` */` |
|   52 | 4317 | `static void PdoApplyOptions(phl_pdo *pConn,ph7_value *pOptions)` |
|    2 | 4318 | `{` |
|    - | 4319 | `	ph7_hashmap *pMap;` |
|    - | 4320 | `	ph7_hashmap_node *pEntry;` |
|    - | 4321 | `	sxu32 n,nCount;` |
|   54 | 4322 | `	if( pOptions == 0 \|\| (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 4323 | `		return;` |
|    - | 4324 | `	}` |
|   54 | 4325 | `	pMap = (ph7_hashmap *)pOptions->x.pOther;` |
|   54 | 4326 | `	nCount = pMap->nEntry;` |
|   54 | 4327 | `	pEntry = pMap->pFirst;` |
|  108 | 4328 | `	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 4329 | `		ph7_value sKey;` |
|    - | 4330 | `		ph7_value *pVal;` |
|    - | 4331 | `		ph7_int64 iKey;` |
|   56 | 4332 | `		if( pEntry->iType != HASHMAP_INT_NODE ){` |
|  ! 0 | 4333 | `			continue;  /* php ignores a string key here */` |
|    - | 4334 | `		}` |
|   56 | 4335 | `		PH7_MemObjInit(pConn->pVm,&sKey);` |
|   56 | 4336 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   56 | 4337 | `		iKey = sKey.x.iVal;` |
|   56 | 4338 | `		PH7_MemObjRelease(&sKey);` |
|   56 | 4339 | `		pVal = (ph7_value *)SySetAt(&pConn->pVm->aMemObj,pEntry->nValIdx);` |
|   56 | 4340 | `		if( pVal == 0 ){` |
|  ! 0 | 4341 | `			continue;` |
|    - | 4342 | `		}` |
|   56 | 4343 | `		switch( iKey ){` |
|   24 | 4344 | `			case PDO_ATTR_ERRMODE:            pConn->iErrMode = (int)ph7_value_to_int64(pVal); break;` |
|    9 | 4345 | `			case PDO_ATTR_CASE:               pConn->iCase = (int)ph7_value_to_int64(pVal); break;` |
|    9 | 4346 | `			case PDO_ATTR_ORACLE_NULLS:       pConn->iOracleNulls = (int)ph7_value_to_int64(pVal); break;` |
|    9 | 4347 | `			case PDO_ATTR_DEFAULT_FETCH_MODE: pConn->iDefaultFetch = (int)ph7_value_to_int64(pVal); break;` |
|    3 | 4348 | `			case PDO_ATTR_STRINGIFY_FETCHES:  pConn->bStringify = ph7_value_to_bool(pVal); break;` |
|    - | 4349 | `			/* php remembers this one and reports it back, though a CLI process` |
|    - | 4350 | `			 * has no pool to keep the handle in. */` |
|    3 | 4351 | `			case PDO_ATTR_PERSISTENT:         pConn->bPersistent = ph7_value_to_bool(pVal); break;` |
|  ! 0 | 4352 | `			case PDO_SQLITE_ATTR_TRANSACTION_MODE: pConn->iTxMode = (int)ph7_value_to_int64(pVal); break;` |
|    - | 4353 | `			/* the OPEN flags are read here and used by the open itself, which is` |
|    - | 4354 | `			 * the only moment they mean anything */` |
|    3 | 4355 | `			case PDO_SQLITE_ATTR_OPEN_FLAGS: pConn->iOpenFlags = (int)ph7_value_to_int64(pVal); break;` |
|  ! 0 | 4356 | `			case PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES: pConn->bExtendedCodes = ph7_value_to_bool(pVal); break;` |
|    2 | 4357 | `			default: break;` |
|    - | 4358 | `		}` |
|   29 | 4359 | `	}` |
|   28 | 4360 | `}` |
|    - | 4361 | `/*` |
|    - | 4362 | `` * php's `uri:` DSN: the real DSN is the FIRST LINE of what that URI names, read`` |
|    - | 4363 | ` * through the ordinary stream layer -- so a php:// wrapper or a userland one is` |
|    - | 4364 | ` * a source too. The line is used verbatim, which is why one with leading` |
|    - | 4365 | `` * whitespace, or a second `uri:`, comes back as "could not find driver" rather`` |
|    - | 4366 | ` * than anything more specific: it is simply parsed as a driver name.` |
|    - | 4367 | ` *` |
|    - | 4368 | ` * Returns 0 when the URI could not be read (the caller words the refusal); the` |
|    - | 4369 | ` * open warning underneath it is the stream layer's own, attributed to` |
|    - | 4370 | ` * PDO::__construct the way php attributes it.` |
|    - | 4371 | ` */` |
|    8 | 4372 | `static int PdoResolveUriDsn(ph7_context *pCtx,const char *zUri,int nUri,SyBlob *pOut)` |
|    1 | 4373 | `{` |
|    - | 4374 | `	const ph7_io_stream *pStream;` |
|    - | 4375 | `	const char *zFile,*zWhole;` |
|    - | 4376 | `	void *pHandle;` |
|    - | 4377 | `	SyBlob sRaw;` |
|    - | 4378 | `	const char *zRaw;` |
|    - | 4379 | `	sxu32 n,nRaw;` |
|    9 | 4380 | `	int rc = 0;` |
|    9 | 4381 | `	if( nUri < 1 ){` |
|  ! 0 | 4382 | `		return 0;` |
|    - | 4383 | `	}` |
|    - | 4384 | `	/* the slice is not a C string, and the stream layer wants one */` |
|    9 | 4385 | `	SyBlobInit(&sRaw,&pCtx->pVm->sAllocator);` |
|    9 | 4386 | `	SyBlobAppend(&sRaw,zUri,(sxu32)nUri);` |
|    9 | 4387 | `	SyBlobAppend(&sRaw,"",1);` |
|    9 | 4388 | `	zFile = zWhole = (const char *)SyBlobData(&sRaw);` |
|    9 | 4389 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nUri);` |
|    9 | 4390 | `	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|    4 | 4391 | `		FALSE,0,FALSE,0,"PDO::__construct") : 0;` |
|    9 | 4392 | `	if( pHandle == 0 ){` |
|    - | 4393 | `		/* the device lookup advanced zFile past the wrapper prefix; php's warning` |
|    - | 4394 | ``		 * names the URI the SCRIPT wrote, `file:///nope` and not `/nope` */`` |
|    4 | 4395 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"PDO::__construct(%s): Failed to open stream: %s",` |
|    1 | 4396 | `			zWhole,"No such file or directory");` |
|    3 | 4397 | `		SyBlobRelease(&sRaw);` |
|    3 | 4398 | `		return 0;` |
|    - | 4399 | `	}` |
|    7 | 4400 | `	SyBlobReset(&sRaw);` |
|    7 | 4401 | `	if( PH7_StreamReadWholeFile(pHandle,pStream,&sRaw) == SXRET_OK ){` |
|    7 | 4402 | `		zRaw = (const char *)SyBlobData(&sRaw);` |
|    7 | 4403 | `		nRaw = SyBlobLength(&sRaw);` |
|    - | 4404 | `		/* php reads ONE line and keeps its terminator, so a file written with a` |
|    - | 4405 | `		 * trailing newline yields a DSN that ends in one -- which reaches sqlite` |
|    - | 4406 | ``		 * as part of the PATH. That is why `sqlite::memory:\n` opens a FILE of`` |
|    - | 4407 | `		 * that name rather than a memory database; the newline is not noise the` |
|    - | 4408 | `		 * driver trims, and trimming it here would answer differently. */` |
|  109 | 4409 | `		for( n = 0 ; n < nRaw && zRaw[n] != '\n' ; ++n ){}` |
|    7 | 4410 | `		if( n < nRaw ){` |
|  ! 0 | 4411 | `			++n;  /* the newline belongs to the line */` |
|  ! 0 | 4412 | `		}` |
|    7 | 4413 | `		if( n > 0 ){` |
|    7 | 4414 | `			SyBlobAppend(pOut,zRaw,n);` |
|    7 | 4415 | `			rc = 1;` |
|    3 | 4416 | `		}` |
|    3 | 4417 | `	}` |
|    7 | 4418 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    7 | 4419 | `	SyBlobRelease(&sRaw);` |
|    7 | 4420 | `	return rc;` |
|    5 | 4421 | `}` |
|    - | 4422 | `/*` |
|    - | 4423 | ` * The driver split, over a DSN that is already resolved.  php reads up to the` |
|    - | 4424 | `` * first `:` as the driver name and hands the rest to that driver; the name is`` |
|    - | 4425 | `` * matched case-SENSITIVELY, so `SQLITE:` is "could not find driver" rather`` |
|    - | 4426 | ` * than a connection, and a DSN with no colon at all is refused before any` |
|    - | 4427 | ` * driver is looked for.` |
|    - | 4428 | ` */` |
|  174 | 4429 | `static int PdoOpenParsed(ph7_context *pCtx,phl_pdo *pConn,const char *zDsn,int nDsn)` |
|    4 | 4430 | `{` |
|    - | 4431 | `	int nDriver;` |
| 1234 | 4432 | `	for( nDriver = 0 ; nDriver < nDsn && zDsn[nDriver] != ':' ; ++nDriver ){}` |
|  178 | 4433 | `	if( nDriver >= nDsn ){` |
|    - | 4434 | `		/* no colon: php refuses the ARGUMENT, not the driver */` |
|    5 | 4435 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4436 | `			"PDO::__construct(): Argument #1 ($dsn) must be a valid data source name");` |
|    - | 4437 | `	}` |
|  170 | 4438 | `	if( nDriver != (int)sizeof("sqlite")-1` |
|  169 | 4439 | `	 \|\| SyMemcmp(zDsn,"sqlite",sizeof("sqlite")-1) != 0 ){` |
|    - | 4440 | `		/* §10 scopes this build to one driver, so every other name -- and every` |
|    - | 4441 | `		 * other SPELLING of this one -- is what a php without that driver says. */` |
|   13 | 4442 | `		return PH7_VmThrowException(pCtx,"PDOException","could not find driver");` |
|    - | 4443 | `	}` |
|    - | 4444 | `	/* SQLITE_OPEN_URI is passed EXPLICITLY rather than left to the linked` |
|    - | 4445 | `	 * library's compile-time default: a Debian libsqlite3 is built with URI` |
|    - | 4446 | `` 	 * filenames on and a vcpkg one is not, so `sqlite:file::memory:?cache=shared` `` |
|    - | 4447 | `	 * opened a memory database on one platform and created a FILE of that name on` |
|    - | 4448 | `	 * the other. php's own sqlite has them on, so on is the answer everywhere. */` |
|    - | 4449 | `	{` |
|    - | 4450 | `		/* the script's own ATTR_OPEN_FLAGS replace the read-write default */` |
|  241 | 4451 | `		int iFlags = pConn->iOpenFlags` |
|    1 | 4452 | `			? pConn->iOpenFlags` |
|   79 | 4453 | `			: (SQLITE_OPEN_READWRITE\|SQLITE_OPEN_CREATE);` |
|  241 | 4454 | `		return PH7_PdoSqliteOpen(pCtx,pConn,zDsn + nDriver + 1,nDsn - nDriver - 1,` |
|   79 | 4455 | `			iFlags\|SQLITE_OPEN_URI);` |
|    - | 4456 | `	}` |
|   91 | 4457 | `}` |
|    - | 4458 | `/*` |
|    - | 4459 | `` * One DSN, resolved then split: php's `uri:` form is read first, and what it`` |
|    - | 4460 | ` * names replaces the DSN whole.` |
|    - | 4461 | ` */` |
|  176 | 4462 | `static int PdoOpenFromDsn(ph7_context *pCtx,phl_pdo *pConn,ph7_value *pDsn)` |
|    4 | 4463 | `{` |
|    - | 4464 | `	const char *zDsn;` |
|    - | 4465 | `	int nDsn,rc;` |
|    - | 4466 | `	SyBlob sResolved;` |
|  180 | 4467 | `	if( pDsn == 0 ){` |
|  ! 0 | 4468 | `		nDsn = 0;` |
|  ! 0 | 4469 | `		zDsn = "";` |
|  ! 0 | 4470 | `	}else{` |
|  180 | 4471 | `		zDsn = ph7_value_to_string(pDsn,&nDsn);` |
|    - | 4472 | `	}` |
|  180 | 4473 | `	SyBlobInit(&sResolved,&pCtx->pVm->sAllocator);` |
|  180 | 4474 | `	if( nDsn >= (int)sizeof("uri:")-1 && SyMemcmp(zDsn,"uri:",sizeof("uri:")-1) == 0 ){` |
|    9 | 4475 | `		if( !PdoResolveUriDsn(pCtx,zDsn + sizeof("uri:")-1,nDsn - ((int)sizeof("uri:")-1),` |
|    - | 4476 | `			&sResolved) ){` |
|    3 | 4477 | `			SyBlobRelease(&sResolved);` |
|    3 | 4478 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4479 | `				"PDO::__construct(): Argument #1 ($dsn) must be a valid data source URI");` |
|    - | 4480 | `		}` |
|    - | 4481 | `		/* the line replaces the DSN whole, and is NOT resolved again: a nested` |
|    - | 4482 | ``		 * `uri:` is read as a driver name, exactly as php reads it */`` |
|    7 | 4483 | `		zDsn = (const char *)SyBlobData(&sResolved);` |
|    7 | 4484 | `		nDsn = (int)SyBlobLength(&sResolved);` |
|    3 | 4485 | `	}` |
|  178 | 4486 | `	rc = PdoOpenParsed(pCtx,pConn,zDsn,nDsn);` |
|  178 | 4487 | `	SyBlobRelease(&sResolved);` |
|  178 | 4488 | `	return rc;` |
|   92 | 4489 | `}` |
|    - | 4490 | `/*` |
|    - | 4491 | ` * PDO::__construct(string $dsn, ?string $username = null, ?string $password = null,` |
|    - | 4492 | ` *                  ?array $options = null)` |
|    - | 4493 | ` *` |
|    - | 4494 | ` * The two credential arguments are the generic surface: sqlite has no user to` |
|    - | 4495 | ` * be, so php accepts and ignores them rather than refusing a portable call.` |
|    - | 4496 | ` */` |
|  156 | 4497 | `static int vm_builtin_PDO___construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 | 4498 | `{` |
|  160 | 4499 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - | 4500 | `	phl_pdo *pConn;` |
|    - | 4501 | `	int rc;` |
|  160 | 4502 | `	if( pThis == 0 ){` |
|  ! 0 | 4503 | `		return PH7_VmThrowException(pCtx,"Error","PDO::__construct() needs a receiver");` |
|    - | 4504 | `	}` |
|  160 | 4505 | `	pConn = PH7_PdoNewConn(pCtx->pVm);` |
|  160 | 4506 | `	if( pConn == 0 ){` |
|  ! 0 | 4507 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4508 | `	}` |
|  160 | 4509 | `	if( PdoAttach(pThis,pConn) != 0 ){` |
|  ! 0 | 4510 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4511 | `	}` |
|    - | 4512 | `	/* the options are read BEFORE the open, so an ATTR_ERRMODE they carry is` |
|    - | 4513 | `	 * already in force for everything that follows */` |
|  160 | 4514 | `	if( nArg > 3 ){` |
|   52 | 4515 | `		PdoApplyOptions(pConn,apArg[3]);` |
|   25 | 4516 | `	}` |
|  160 | 4517 | `	rc = PdoOpenFromDsn(pCtx,pConn,nArg > 0 ? apArg[0] : 0);` |
|  160 | 4518 | `	return rc;` |
|   82 | 4519 | `}` |
|    - | 4520 | `/*` |
|    - | 4521 | ` * static PDO::connect(string $dsn, ...): static` |
|    - | 4522 | ` *` |
|    - | 4523 | `` * php 8.4's replacement for `new PDO(...)`: same arguments, but the object it`` |
|    - | 4524 | `` * answers is the DRIVER's subclass -- `Pdo\Sqlite` here -- so the`` |
|    - | 4525 | ` * sqlite-specific verbs are callable on it without a cast. Called on a` |
|    - | 4526 | `` * subclass it answers that subclass, which is what `static` means.`` |
|    - | 4527 | ` */` |
|   20 | 4528 | `static int vm_builtin_PDO_connect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 4529 | `{` |
|   23 | 4530 | `	ph7_vm *pVm = pCtx->pVm;` |
|   23 | 4531 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|    - | 4532 | `	ph7_class_instance *pObj;` |
|    - | 4533 | `	phl_pdo *pConn;` |
|    - | 4534 | `	int rc;` |
|   23 | 4535 | `	if( pClass == 0 \|\| SyStrncmp(pClass->sName.zString,"PDO",sizeof("PDO")-1) == 0 ){` |
|    - | 4536 | `		/* PDO::connect() itself answers the driver's class, not PDO */` |
|    6 | 4537 | `		ph7_class *pDrv = PH7_VmExtractClass(&(*pVm),"Pdo\\Sqlite",` |
|    - | 4538 | `			sizeof("Pdo\\Sqlite")-1,FALSE,0);` |
|    6 | 4539 | `		if( pDrv ){` |
|    6 | 4540 | `			pClass = pDrv;` |
|    2 | 4541 | `		}` |
|    2 | 4542 | `	}` |
|   23 | 4543 | `	if( pClass == 0 ){` |
|  ! 0 | 4544 | `		return PH7_VmThrowException(pCtx,"Error","Pdo\\Sqlite is not available");` |
|    - | 4545 | `	}` |
|   23 | 4546 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|   23 | 4547 | `	if( pObj == 0 ){` |
|  ! 0 | 4548 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4549 | `	}` |
|   23 | 4550 | `	pConn = PH7_PdoNewConn(&(*pVm));` |
|   23 | 4551 | `	if( pConn == 0 \|\| PdoAttach(pObj,pConn) != 0 ){` |
|  ! 0 | 4552 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4553 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4554 | `	}` |
|   23 | 4555 | `	if( nArg > 3 ){` |
|    3 | 4556 | `		PdoApplyOptions(pConn,apArg[3]);` |
|    1 | 4557 | `	}` |
|   23 | 4558 | `	rc = PdoOpenFromDsn(pCtx,pConn,nArg > 0 ? apArg[0] : 0);` |
|   23 | 4559 | `	if( rc != PH7_OK ){` |
|    3 | 4560 | `		PH7_ClassInstanceUnref(pObj);` |
|    3 | 4561 | `		return rc;` |
|    - | 4562 | `	}` |
|   21 | 4563 | `	PH7_NativeResultObject(pCtx,pObj);` |
|   21 | 4564 | `	return PH7_OK;` |
|   13 | 4565 | `}` |
|    - | 4566 | `/*` |
|    - | 4567 | ` * The loaded-driver list, the one answer php's two spellings share.  php` |
|    - | 4568 | ` * answers the drivers its ext/pdo actually loaded, which is why an engine` |
|    - | 4569 | ` * with no driver at all answers [] -- here it is always ["sqlite"].` |
|    - | 4570 | ` */` |
|    8 | 4571 | `static int PdoDriverList(ph7_context *pCtx)` |
|    1 | 4572 | `{` |
|    - | 4573 | `	ph7_value *pArray, *pName;` |
|    9 | 4574 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 | 4575 | `	pName  = ph7_context_new_scalar(pCtx);` |
|    9 | 4576 | `	if( pArray == 0 \|\| pName == 0 ){` |
|  ! 0 | 4577 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 | 4578 | `		ph7_result_null(pCtx);` |
|  ! 0 | 4579 | `		return PH7_OK;` |
|    - | 4580 | `	}` |
|    9 | 4581 | `	ph7_value_string(pName,"sqlite",sizeof("sqlite")-1);` |
|    9 | 4582 | `	ph7_array_add_elem(pArray,0,pName);` |
|    9 | 4583 | `	ph7_result_value(pCtx,pArray);` |
|    9 | 4584 | `	return PH7_OK;` |
|    5 | 4585 | `}` |
|    - | 4586 | `/* PDO::getAvailableDrivers(): the static method spelling. */` |
|    4 | 4587 | `static int vm_builtin_PDO_getAvailableDrivers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4588 | `{` |
|    2 | 4589 | `	SXUNUSED(nArg);` |
|    2 | 4590 | `	SXUNUSED(apArg);` |
|    5 | 4591 | `	return PdoDriverList(pCtx);` |
|    1 | 4592 | `}` |
|    - | 4593 | `/*` |
|    - | 4594 | ` * pdo_drivers(): the PROCEDURAL spelling of the same list, and the only` |
|    - | 4595 | ` * FUNCTION ext/pdo declares.  php's two answers are the same array built by` |
|    - | 4596 | `` * the same C routine, so they are `===` to each other.`` |
|    - | 4597 | ` */` |
|    4 | 4598 | `static int vm_builtin_pdo_drivers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4599 | `{` |
|    2 | 4600 | `	SXUNUSED(nArg);` |
|    2 | 4601 | `	SXUNUSED(apArg);` |
|    5 | 4602 | `	return PdoDriverList(pCtx);` |
|    1 | 4603 | `}` |
|    - | 4604 |  |
|    - | 4605 | `/*` |
|    - | 4606 | ` * Install the PDO class library.  Called from PH7_VmInit inside the` |
|    - | 4607 | ` * bCompilingBuiltin window; vm_pdo_sqlite.c's installer runs right after and` |
|    - | 4608 | `` * needs PDO to already be mounted (it is the parent of `Pdo\Sqlite`).`` |
|    - | 4609 | ` */` |
| 5740 | 4610 | `PH7_PRIVATE sxi32 PH7_VmInstallPdo(ph7_vm *pVm)` |
|    5 | 4611 | `{` |
|    - | 4612 | `	/* php's own constant values, in its own declaration order. The seven` |
|    - | 4613 | `	 * deprecated PDO::SQLITE_* rows php still carries are absent by §10; their` |
|    - | 4614 | `	 * successors are declared on Pdo\Sqlite (vm_pdo_sqlite.c). */` |
|    - | 4615 | `#define PDO_INT_CONST(NAME,VALUE) \` |
|    - | 4616 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (ph7_int64)(VALUE), 0, 0.0 }` |
|    - | 4617 | `	static const PH7_NativeConstDef aPdoConst[] = {` |
|    - | 4618 | `		PDO_INT_CONST("PARAM_NULL",             0),` |
|    - | 4619 | `		PDO_INT_CONST("PARAM_BOOL",             5),` |
|    - | 4620 | `		PDO_INT_CONST("PARAM_INT",              1),` |
|    - | 4621 | `		PDO_INT_CONST("PARAM_STR",              2),` |
|    - | 4622 | `		PDO_INT_CONST("PARAM_LOB",              3),` |
|    - | 4623 | `		PDO_INT_CONST("PARAM_STMT",             4),` |
|    - | 4624 | `		PDO_INT_CONST("PARAM_INPUT_OUTPUT",     2147483648LL),` |
|    - | 4625 | `		PDO_INT_CONST("PARAM_STR_NATL",         1073741824LL),` |
|    - | 4626 | `		PDO_INT_CONST("PARAM_STR_CHAR",         536870912LL),` |
|    - | 4627 | `		PDO_INT_CONST("PARAM_EVT_ALLOC",        0),` |
|    - | 4628 | `		PDO_INT_CONST("PARAM_EVT_FREE",         1),` |
|    - | 4629 | `		PDO_INT_CONST("PARAM_EVT_EXEC_PRE",     2),` |
|    - | 4630 | `		PDO_INT_CONST("PARAM_EVT_EXEC_POST",    3),` |
|    - | 4631 | `		PDO_INT_CONST("PARAM_EVT_FETCH_PRE",    4),` |
|    - | 4632 | `		PDO_INT_CONST("PARAM_EVT_FETCH_POST",   5),` |
|    - | 4633 | `		PDO_INT_CONST("PARAM_EVT_NORMALIZE",    6),` |
|    - | 4634 | `		PDO_INT_CONST("FETCH_DEFAULT",          0),` |
|    - | 4635 | `		PDO_INT_CONST("FETCH_LAZY",             1),` |
|    - | 4636 | `		PDO_INT_CONST("FETCH_ASSOC",            2),` |
|    - | 4637 | `		PDO_INT_CONST("FETCH_NUM",              3),` |
|    - | 4638 | `		PDO_INT_CONST("FETCH_BOTH",             4),` |
|    - | 4639 | `		PDO_INT_CONST("FETCH_OBJ",              5),` |
|    - | 4640 | `		PDO_INT_CONST("FETCH_BOUND",            6),` |
|    - | 4641 | `		PDO_INT_CONST("FETCH_COLUMN",           7),` |
|    - | 4642 | `		PDO_INT_CONST("FETCH_CLASS",            8),` |
|    - | 4643 | `		PDO_INT_CONST("FETCH_INTO",             9),` |
|    - | 4644 | `		PDO_INT_CONST("FETCH_FUNC",            10),` |
|    - | 4645 | `		PDO_INT_CONST("FETCH_GROUP",           32),` |
|    - | 4646 | `		PDO_INT_CONST("FETCH_UNIQUE",          64),` |
|    - | 4647 | `		PDO_INT_CONST("FETCH_KEY_PAIR",        12),` |
|    - | 4648 | `		PDO_INT_CONST("FETCH_CLASSTYPE",      128),` |
|    - | 4649 | `		PDO_INT_CONST("FETCH_SERIALIZE",      512),` |
|    - | 4650 | `		PDO_INT_CONST("FETCH_PROPS_LATE",     256),` |
|    - | 4651 | `		PDO_INT_CONST("FETCH_NAMED",           11),` |
|    - | 4652 | `		PDO_INT_CONST("ATTR_AUTOCOMMIT",        0),` |
|    - | 4653 | `		PDO_INT_CONST("ATTR_PREFETCH",          1),` |
|    - | 4654 | `		PDO_INT_CONST("ATTR_TIMEOUT",           2),` |
|    - | 4655 | `		PDO_INT_CONST("ATTR_ERRMODE",           3),` |
|    - | 4656 | `		PDO_INT_CONST("ATTR_SERVER_VERSION",    4),` |
|    - | 4657 | `		PDO_INT_CONST("ATTR_CLIENT_VERSION",    5),` |
|    - | 4658 | `		PDO_INT_CONST("ATTR_SERVER_INFO",       6),` |
|    - | 4659 | `		PDO_INT_CONST("ATTR_CONNECTION_STATUS", 7),` |
|    - | 4660 | `		PDO_INT_CONST("ATTR_CASE",              8),` |
|    - | 4661 | `		PDO_INT_CONST("ATTR_CURSOR_NAME",       9),` |
|    - | 4662 | `		PDO_INT_CONST("ATTR_CURSOR",           10),` |
|    - | 4663 | `		PDO_INT_CONST("ATTR_ORACLE_NULLS",     11),` |
|    - | 4664 | `		PDO_INT_CONST("ATTR_PERSISTENT",       12),` |
|    - | 4665 | `		PDO_INT_CONST("ATTR_STATEMENT_CLASS",  13),` |
|    - | 4666 | `		PDO_INT_CONST("ATTR_FETCH_TABLE_NAMES",14),` |
|    - | 4667 | `		PDO_INT_CONST("ATTR_FETCH_CATALOG_NAMES",15),` |
|    - | 4668 | `		PDO_INT_CONST("ATTR_DRIVER_NAME",      16),` |
|    - | 4669 | `		PDO_INT_CONST("ATTR_STRINGIFY_FETCHES",17),` |
|    - | 4670 | `		PDO_INT_CONST("ATTR_MAX_COLUMN_LEN",   18),` |
|    - | 4671 | `		PDO_INT_CONST("ATTR_EMULATE_PREPARES", 20),` |
|    - | 4672 | `		PDO_INT_CONST("ATTR_DEFAULT_FETCH_MODE",19),` |
|    - | 4673 | `		PDO_INT_CONST("ATTR_DEFAULT_STR_PARAM",21),` |
|    - | 4674 | `		PDO_INT_CONST("ERRMODE_SILENT",         0),` |
|    - | 4675 | `		PDO_INT_CONST("ERRMODE_WARNING",        1),` |
|    - | 4676 | `		PDO_INT_CONST("ERRMODE_EXCEPTION",      2),` |
|    - | 4677 | `		PDO_INT_CONST("CASE_NATURAL",           0),` |
|    - | 4678 | `		PDO_INT_CONST("CASE_LOWER",             2),` |
|    - | 4679 | `		PDO_INT_CONST("CASE_UPPER",             1),` |
|    - | 4680 | `		PDO_INT_CONST("NULL_NATURAL",           0),` |
|    - | 4681 | `		PDO_INT_CONST("NULL_EMPTY_STRING",      1),` |
|    - | 4682 | `		PDO_INT_CONST("NULL_TO_STRING",         2),` |
|    - | 4683 | `		{ "ERR_NONE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, "00000", 0.0 },` |
|    - | 4684 | `		PDO_INT_CONST("FETCH_ORI_NEXT",         0),` |
|    - | 4685 | `		PDO_INT_CONST("FETCH_ORI_PRIOR",        1),` |
|    - | 4686 | `		PDO_INT_CONST("FETCH_ORI_FIRST",        2),` |
|    - | 4687 | `		PDO_INT_CONST("FETCH_ORI_LAST",         3),` |
|    - | 4688 | `		PDO_INT_CONST("FETCH_ORI_ABS",          4),` |
|    - | 4689 | `		PDO_INT_CONST("FETCH_ORI_REL",          5),` |
|    - | 4690 | `		PDO_INT_CONST("CURSOR_FWDONLY",         0),` |
|    - | 4691 | `		PDO_INT_CONST("CURSOR_SCROLL",          1),` |
|    - | 4692 | `	};` |
|    - | 4693 | `	/* php's own signatures and its own stub ORDER: Reflection and` |
|    - | 4694 | `	 * get_class_methods() both answer declaration order, so the two engines` |
|    - | 4695 | `	 * must list one surface. Nearly every return type is TENTATIVE in php's` |
|    - | 4696 | ``	 * stubs (the leading `@`), which is a php-visible difference from a`` |
|    - | 4697 | `	 * declared one -- getReturnType() answers null for a tentative type. */` |
|    - | 4698 | `	static const PH7_NativeMethodDef aPdoMethod[] = {` |
|    - | 4699 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|    - | 4700 | `		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",` |
|    - | 4701 | `		  0, vm_builtin_PDO___construct },` |
|    - | 4702 | `		{ "connect", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|    - | 4703 | `		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",` |
|    - | 4704 | `		  "static", vm_builtin_PDO_connect },` |
|    - | 4705 | `		{ "beginTransaction", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_beginTransaction },` |
|    - | 4706 | `		{ "commit",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_commit },` |
|    - | 4707 | `		{ "errorCode",        PH7_MOD_PUBLIC, "", "@?string", vm_builtin_PDO_errorCode },` |
|    - | 4708 | `		{ "errorInfo",        PH7_MOD_PUBLIC, "", "@array", vm_builtin_PDO_errorInfo },` |
|    - | 4709 | `		{ "exec",             PH7_MOD_PUBLIC, "string $statement", "@int\|false",` |
|    - | 4710 | `		  vm_builtin_PDO_exec },` |
|    - | 4711 | `		{ "getAttribute",     PH7_MOD_PUBLIC, "int $attribute", "@mixed",` |
|    - | 4712 | `		  vm_builtin_PDO_getAttribute },` |
|    - | 4713 | `		{ "getAvailableDrivers", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "@array",` |
|    - | 4714 | `		  vm_builtin_PDO_getAvailableDrivers },` |
|    - | 4715 | `		{ "inTransaction",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_inTransaction },` |
|    - | 4716 | `		{ "lastInsertId",     PH7_MOD_PUBLIC, "?string $name = null", "@string\|false",` |
|    - | 4717 | `		  vm_builtin_PDO_lastInsertId },` |
|    - | 4718 | `		{ "prepare",          PH7_MOD_PUBLIC, "string $query, array $options = []",` |
|    - | 4719 | `		  "@PDOStatement\|false", vm_builtin_PDO_prepare },` |
|    - | 4720 | `		{ "query",            PH7_MOD_PUBLIC,` |
|    - | 4721 | `		  "string $query, ?int $fetchMode = null, mixed ...$fetchModeArgs",` |
|    - | 4722 | `		  "@PDOStatement\|false", vm_builtin_PDO_query },` |
|    - | 4723 | `		{ "quote",            PH7_MOD_PUBLIC, "string $string, int $type = PDO::PARAM_STR",` |
|    - | 4724 | `		  "@string\|false", vm_builtin_PDO_quote },` |
|    - | 4725 | `		{ "rollBack",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_rollBack },` |
|    - | 4726 | `		{ "setAttribute",     PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",` |
|    - | 4727 | `		  vm_builtin_PDO_setAttribute },` |
|    - | 4728 | `	};` |
|    - | 4729 | `	static const PH7_NativeMethodDef aStmtMethod[] = {` |
|    - | 4730 | `		{ "bindColumn",   PH7_MOD_PUBLIC,` |
|    - | 4731 | `		  "string\|int $column, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "` |
|    - | 4732 | `		  "mixed $driverOptions = null", "@bool", vm_builtin_PDOStatement_bindColumn },` |
|    - | 4733 | `		{ "bindParam",    PH7_MOD_PUBLIC,` |
|    - | 4734 | `		  "string\|int $param, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "` |
|    - | 4735 | `		  "mixed $driverOptions = null", "@bool", vm_builtin_PDOStatement_bindParam },` |
|    - | 4736 | `		{ "bindValue",    PH7_MOD_PUBLIC,` |
|    - | 4737 | `		  "string\|int $param, mixed $value, int $type = PDO::PARAM_STR", "@bool",` |
|    - | 4738 | `		  vm_builtin_PDOStatement_bindValue },` |
|    - | 4739 | `		{ "closeCursor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDOStatement_closeCursor },` |
|    - | 4740 | `		{ "columnCount",  PH7_MOD_PUBLIC, "", "@int", vm_builtin_PDOStatement_columnCount },` |
|    - | 4741 | `		{ "debugDumpParams", PH7_MOD_PUBLIC, "", "@?bool",` |
|    - | 4742 | `		  vm_builtin_PDOStatement_debugDumpParams },` |
|    - | 4743 | `		{ "errorCode",    PH7_MOD_PUBLIC, "", "@?string", vm_builtin_PDOStatement_errorCode },` |
|    - | 4744 | `		{ "errorInfo",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_PDOStatement_errorInfo },` |
|    - | 4745 | `		{ "execute",      PH7_MOD_PUBLIC, "?array $params = null", "@bool",` |
|    - | 4746 | `		  vm_builtin_PDOStatement_execute },` |
|    - | 4747 | `		{ "fetch",        PH7_MOD_PUBLIC,` |
|    - | 4748 | `		  "int $mode = PDO::FETCH_DEFAULT, int $cursorOrientation = PDO::FETCH_ORI_NEXT, "` |
|    - | 4749 | `		  "int $cursorOffset = 0", "@mixed", vm_builtin_PDOStatement_fetch },` |
|    - | 4750 | `		{ "fetchAll",     PH7_MOD_PUBLIC, "int $mode = PDO::FETCH_DEFAULT, mixed ...$args",` |
|    - | 4751 | `		  "@array", vm_builtin_PDOStatement_fetchAll },` |
|    - | 4752 | `		{ "fetchColumn",  PH7_MOD_PUBLIC, "int $column = 0", "@mixed",` |
|    - | 4753 | `		  vm_builtin_PDOStatement_fetchColumn },` |
|    - | 4754 | `		{ "fetchObject",  PH7_MOD_PUBLIC,` |
|    - | 4755 | `		  "?string $class = 'stdClass', array $constructorArgs = []", "@object\|false",` |
|    - | 4756 | `		  vm_builtin_PDOStatement_fetchObject },` |
|    - | 4757 | `		{ "getAttribute", PH7_MOD_PUBLIC, "int $name", "@mixed",` |
|    - | 4758 | `		  vm_builtin_PDOStatement_getAttribute },` |
|    - | 4759 | `		{ "getColumnMeta",PH7_MOD_PUBLIC, "int $column", "@array\|false",` |
|    - | 4760 | `		  vm_builtin_PDOStatement_getColumnMeta },` |
|    - | 4761 | `		{ "nextRowset",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDOStatement_nextRowset },` |
|    - | 4762 | `		{ "rowCount",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_PDOStatement_rowCount },` |
|    - | 4763 | `		{ "setAttribute", PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",` |
|    - | 4764 | `		  vm_builtin_PDOStatement_setAttribute },` |
|    - | 4765 | `		{ "setFetchMode", PH7_MOD_PUBLIC, "int $mode, mixed ...$args", "@true",` |
|    - | 4766 | `		  vm_builtin_PDOStatement_setFetchMode },` |
|    - | 4767 | `		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_PDOStatement_getIterator },` |
|    - | 4768 | `	};` |
|    - | 4769 | `	/* The one property php PRESENTS on a statement: var_dump of a PDOStatement` |
|    - | 4770 | ``	 * shows `queryString` and nothing else. It is typed and has no default --`` |
|    - | 4771 | ``	 * `new PDOStatement()` (which php allows) leaves it uninitialized. */`` |
|    - | 4772 | `	static const PH7_NativePropDef aStmtProp[] = {` |
|    - | 4773 | `		{ "queryString", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|    - | 4774 | `		/* the cursor, hidden the way the connection's handle is */` |
|    - | 4775 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 4776 | `	};` |
|    - | 4777 | `	/*` |
|    - | 4778 | ``	 * PDORow declares `public string $queryString;` and holds NO property at`` |
|    - | 4779 | `	 * all: the object's whole surface is its handlers (PdoRowProp/PdoRowDim),` |
|    - | 4780 | `	 * so the declaration is marked LAZY below and nothing ever materializes it.` |
|    - | 4781 | `	 * The two engine slots beside it are hidden the way every other handle is.` |
|    - | 4782 | `	 */` |
|    - | 4783 | `	static const PH7_NativePropDef aRowProp[] = {` |
|    - | 4784 | `		{ "queryString", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|    - | 4785 | `		{ PDOROW_RES,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 4786 | `		{ PDOROW_STMT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|    - | 4787 | `	};` |
|    - | 4788 | `	/* php redeclares Exception::$code UNTYPED here so a SQLSTATE -- a string` |
|    - | 4789 | `	 * like 'HY000' -- can live in it, and adds the driver's raw error triple. */` |
|    - | 4790 | `	static const PH7_NativePropDef aExcProp[] = {` |
|    - | 4791 | `		{ "code",      PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|    - | 4792 | `		{ "errorInfo", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?array" },` |
|    - | 4793 | `	};` |
|    - | 4794 | `	/* The connection handle: storage the class owns and NEVER presents -- php` |
|    - | 4795 | `	 * shows no property at all on a PDO, so the slot is hidden (which is what` |
|    - | 4796 | `	 * keeps it out of var_dump, (array), get_object_vars and Reflection). */` |
|    - | 4797 | `	static const PH7_NativePropDef aPdoProp[] = {` |
|    - | 4798 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 4799 | `	};` |
|    - | 4800 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|    - | 4801 | ``		/* Both handles refuse `clone` and `serialize`: php declares neither a`` |
|    - | 4802 | `		 * clone handler nor a serializer for them, so the copy would carry the` |
|    - | 4803 | `		 * same sqlite3 pointer in its hidden slot. */` |
|    - | 4804 | `		{ "PDO", 0, 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 4805 | `		  aPdoMethod, SX_ARRAYSIZE(aPdoMethod),` |
|    - | 4806 | `		  aPdoConst, SX_ARRAYSIZE(aPdoConst),` |
|    - | 4807 | `		  aPdoProp, SX_ARRAYSIZE(aPdoProp),` |
|    - | 4808 | `		  PdoInstanceRelease, 0, 0 },` |
|    - | 4809 | `		{ "PDOStatement", 0, "IteratorAggregate", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 4810 | `		  aStmtMethod, SX_ARRAYSIZE(aStmtMethod),` |
|    - | 4811 | `		  0, 0,` |
|    - | 4812 | `		  aStmtProp, SX_ARRAYSIZE(aStmtProp),` |
|    - | 4813 | `		  PdoStmtInstanceRelease, &sPdoStmtIterVtab, 0 },` |
|    - | 4814 | `		{ "PDOException", "RuntimeException", 0, 0,` |
|    - | 4815 | `		  0, 0, 0, 0,` |
|    - | 4816 | `		  aExcProp, SX_ARRAYSIZE(aExcProp),` |
|    - | 4817 | `		  0, 0, 0 },` |
|    - | 4818 | ``		/* FINAL, uncloneable, unserializable, and refusing `new` with php's own`` |
|    - | 4819 | `		 * sentence -- which is a PDOException here and an Error everywhere else,` |
|    - | 4820 | `		 * so the class carries the exception name beside the text. */` |
|    - | 4821 | `		{ "PDORow", 0, 0,` |
|    - | 4822 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 4823 | `		  0, 0, 0, 0,` |
|    - | 4824 | `		  aRowProp, SX_ARRAYSIZE(aRowProp),` |
|    - | 4825 | `		  PdoRowInstanceRelease, 0, PdoRowPresent }` |
|    - | 4826 | `	};` |
|    - | 4827 | `#undef PDO_INT_CONST` |
|    - | 4828 | `	sxi32 rc;` |
| 5745 | 4829 | `	pVm->pPdoConns = 0;` |
|    - | 4830 | `	/* ext/pdo declares exactly one function beside its classes. */` |
| 5745 | 4831 | `	ph7_create_function(&(*pVm),"pdo_drivers",vm_builtin_pdo_drivers,0);` |
| 5745 | 4832 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
| 5745 | 4833 | `	if( rc == SXRET_OK ){` |
| 5745 | 4834 | `		ph7_class *pRow = PH7_VmExtractClass(&(*pVm),"PDORow",sizeof("PDORow")-1,FALSE,0);` |
| 5745 | 4835 | `		if( pRow ){` |
| 5745 | 4836 | `			pRow->zNewRefusal = "You may not create a PDORow manually";` |
| 5745 | 4837 | `			pRow->zNewRefusalClass = "PDOException";` |
| 5745 | 4838 | `			pRow->xDim = PdoRowDim;` |
| 5745 | 4839 | `			pRow->xCmp = PdoRowCmp;` |
| 2870 | 4840 | `		}` |
|    - | 4841 | `		/* The declaration php makes and the object never holds: marked LAZY, and` |
|    - | 4842 | `		 * nothing materializes it -- every write to this class is refused. */` |
| 5745 | 4843 | `		PH7_NativeClassMarkLazyProps(&(*pVm),"PDORow",0);` |
| 5745 | 4844 | `		PH7_NativeClassInstallPropHook(&(*pVm),"PDORow",PdoRowProp);` |
| 2870 | 4845 | `	}` |
| 5745 | 4846 | `	return rc;` |
|    5 | 4847 | `}` |
|    - | 4848 |  |
|    - | 4849 | `#else` |
|    - | 4850 | `/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */` |
|    - | 4851 | `typedef int vm_pdo_unused;` |
|    - | 4852 | `#endif /* PH7_ENABLE_SQLITE */` |
|    - | 4853 |  |
