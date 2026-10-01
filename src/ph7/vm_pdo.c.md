# src/ph7/vm_pdo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2609/2884 lines (90.46%)

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
|  180 |   49 | `PH7_PRIVATE phl_pdo * PH7_PdoNewConn(ph7_vm *pVm)` |
|    5 |   50 | `{` |
|  185 |   51 | `	phl_pdo *pConn = (phl_pdo *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo));` |
|  185 |   52 | `	if( pConn == 0 ){` |
|  ! 0 |   53 | `		return 0;` |
|    - |   54 | `	}` |
|  185 |   55 | `	SyZero(pConn,sizeof(phl_pdo));` |
|  185 |   56 | `	pConn->pVm = pVm;` |
|    - |   57 | `	/* php's defaults for a fresh sqlite handle: exceptions on, both column` |
|    - |   58 | `	 * shapes, no case folding, no null rewriting, native column types. */` |
|  185 |   59 | `	pConn->iErrMode = PDO_ERRMODE_EXCEPTION;` |
|  185 |   60 | `	pConn->iCase = PDO_CASE_NATURAL;` |
|  185 |   61 | `	pConn->iOracleNulls = PDO_NULL_NATURAL;` |
|  185 |   62 | `	pConn->iDefaultFetch = PDO_FETCH_BOTH;` |
|  185 |   63 | `	pConn->iErrState = PDO_ERR_NONE;` |
|  185 |   64 | `	pConn->pNext = (phl_pdo *)pVm->pPdoConns;` |
|  185 |   65 | `	pVm->pPdoConns = pConn;` |
|  185 |   66 | `	return pConn;` |
|   95 |   67 | `}` |
|  180 |   68 | `PH7_PRIVATE void PH7_PdoFreeConn(phl_pdo *pConn)` |
|    5 |   69 | `{` |
|    - |   70 | `	/* sqlite refuses to close a database that still has a live statement or an` |
|    - |   71 | `	 * open blob handle, so the cursors go first and the blobs beside them. */` |
|  185 |   72 | `	PdoStmtSweep(pConn);` |
|  185 |   73 | `	PH7_PdoSqliteBlobSweep(pConn);` |
|    - |   74 | `	{` |
|    - |   75 | `		/* the callbacks sqlite still points at; the close is what makes them` |
|    - |   76 | `		 * unreachable, so they are released after it below */` |
|  185 |   77 | `		phl_pdo_udf *pUdf = pConn->pUdfs;` |
|  211 |   78 | `		while( pUdf ){` |
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
|  185 |   92 | `		pConn->pUdfs = 0;` |
|    - |   93 | `	}` |
|  185 |   94 | `	PH7_PdoSqliteClose(pConn);` |
|  185 |   95 | `	if( pConn->zDrvMsg ){` |
|   15 |   96 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);` |
|   15 |   97 | `		pConn->zDrvMsg = 0;` |
|    7 |   98 | `	}` |
|  185 |   99 | `	if( pConn->pStmtArgs ){` |
|    3 |  100 | `		ph7_release_value(pConn->pVm,pConn->pStmtArgs);` |
|    3 |  101 | `		pConn->pStmtArgs = 0;` |
|    1 |  102 | `	}` |
|  185 |  103 | `	if( pConn->zStmtClass ){` |
|    8 |  104 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zStmtClass);` |
|    8 |  105 | `		pConn->zStmtClass = 0;` |
|    3 |  106 | `	}` |
|  185 |  107 | `}` |
|    - |  108 | `/*` |
|    - |  109 | ` * Free every registered connection.  Called from PH7_PdoVmReset (a reused VM --` |
|    - |  110 | ` * the -S server's -- must not answer the next request through the previous` |
|    - |  111 | ` * one's database) and from PH7_PdoVmRelease before the allocator that holds the` |
|    - |  112 | ` * shells is torn down.` |
|    - |  113 | ` */` |
| 5645 |  114 | `static void PdoVmSweep(ph7_vm *pVm)` |
|    5 |  115 | `{` |
| 5650 |  116 | `	phl_pdo *pConn = (phl_pdo *)pVm->pPdoConns;` |
| 5830 |  117 | `	while( pConn ){` |
|  185 |  118 | `		phl_pdo *pNext = pConn->pNext;` |
|  185 |  119 | `		PdoBlankSlot(pConn->pOwner);` |
|  185 |  120 | `		PH7_PdoFreeConn(pConn);` |
|  185 |  121 | `		SyMemBackendFree(&pVm->sAllocator,pConn);` |
|  185 |  122 | `		pConn = pNext;` |
|    5 |  123 | `	}` |
| 5650 |  124 | `	pVm->pPdoConns = 0;` |
| 5650 |  125 | `}` |
|   16 |  126 | `PH7_PRIVATE void PH7_PdoVmReset(ph7_vm *pVm)` |
|  ! 0 |  127 | `{` |
|   16 |  128 | `	PdoVmSweep(&(*pVm));` |
|   16 |  129 | `}` |
| 5629 |  130 | `PH7_PRIVATE void PH7_PdoVmRelease(ph7_vm *pVm)` |
|    5 |  131 | `{` |
| 5634 |  132 | `	PdoVmSweep(&(*pVm));` |
| 5634 |  133 | `}` |
|    - |  134 | `/*` |
|    - |  135 | ` * Blank the hidden slot of the object that holds a record we are about to` |
|    - |  136 | ` * free.  Without this the object outlives its record -- a PDOStatement whose` |
|    - |  137 | ` * connection was released first, or any handle alive at VM teardown -- and its` |
|    - |  138 | ` * own release reads freed memory to ask whether it still owns one. (ASan found` |
|    - |  139 | ` * exactly that; nothing in the ordinary build noticed.)` |
|    - |  140 | ` */` |
| 1112 |  141 | `static void PdoBlankSlot(ph7_class_instance *pOwner)` |
|    5 |  142 | `{` |
|    - |  143 | `	SyString sAttr;` |
|    - |  144 | `	ph7_value *pRes;` |
| 1117 |  145 | `	if( pOwner == 0 ){` |
| 1028 |  146 | `		return;` |
|    - |  147 | `	}` |
|   92 |  148 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|   92 |  149 | `	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);` |
|   92 |  150 | `	if( pRes ){` |
|   92 |  151 | `		PH7_MemObjRelease(pRes);` |
|   92 |  152 | `		MemObjSetType(pRes,MEMOBJ_NULL);` |
|   44 |  153 | `	}` |
|  561 |  154 | `}` |
|    - |  155 | ``/* The connection behind a `__res` slot value. */`` |
| 1678 |  156 | `static phl_pdo * PdoOfValue(ph7_value *pVal)` |
|    5 |  157 | `{` |
| 1683 |  158 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   45 |  159 | `		return 0;` |
|    - |  160 | `	}` |
| 1639 |  161 | `	return (phl_pdo *)ph7_value_to_resource(pVal);` |
|  844 |  162 | `}` |
|   50 |  163 | `PH7_PRIVATE phl_pdo * PH7_PdoConnOfInstance(ph7_class_instance *pThis)` |
|    1 |  164 | `{` |
|   51 |  165 | `	return PdoOfInstance(pThis);` |
|    1 |  166 | `}` |
| 1678 |  167 | `static phl_pdo * PdoOfInstance(ph7_class_instance *pThis)` |
|    5 |  168 | `{` |
|    - |  169 | `	SyString sAttr;` |
| 1683 |  170 | `	if( pThis == 0 ){` |
|  ! 0 |  171 | `		return 0;` |
|    - |  172 | `	}` |
| 1683 |  173 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
| 1683 |  174 | `	return PdoOfValue(PH7_ClassInstanceFetchAttr(pThis,&sAttr));` |
|  844 |  175 | `}` |
|    - |  176 | `/* Store one connection in the receiver's hidden slot. */` |
|  180 |  177 | `static int PdoAttach(ph7_class_instance *pThis,phl_pdo *pConn)` |
|    5 |  178 | `{` |
|    - |  179 | `	SyString sAttr;` |
|    - |  180 | `	ph7_value *pRes;` |
|  185 |  181 | `	if( pThis == 0 ){` |
|  ! 0 |  182 | `		return -1;` |
|    - |  183 | `	}` |
|  185 |  184 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  185 |  185 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  185 |  186 | `	if( pRes == 0 ){` |
|  ! 0 |  187 | `		return -1;` |
|    - |  188 | `	}` |
|  185 |  189 | `	PH7_MemObjRelease(pRes);` |
|  185 |  190 | `	pRes->x.pOther = pConn;` |
|  185 |  191 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|  185 |  192 | `	pConn->pOwner = pThis;` |
|  185 |  193 | `	return 0;` |
|   95 |  194 | `}` |
|    - |  195 | `/*` |
|    - |  196 | ` * The object is going away: close its database now rather than at VM reset, so` |
|    - |  197 | ` * a script that unsets its last reference releases the file lock there -- which` |
|    - |  198 | ` * is what php does, and what a test that unlinks the file afterwards needs.` |
|    - |  199 | ` * The shell stays on the registry (the sweep frees it) because the slot is` |
|    - |  200 | ` * still reachable while the instance is being torn down.` |
|    - |  201 | ` */` |
|  142 |  202 | `static void PdoInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    3 |  203 | `{` |
|  145 |  204 | `	phl_pdo *pConn = PdoOfInstance(pThis);` |
|   71 |  205 | `	SXUNUSED(pVm);` |
|  145 |  206 | `	if( pConn == 0 \|\| pConn->pOwner != pThis ){` |
|   45 |  207 | `		return;` |
|    - |  208 | `	}` |
|  101 |  209 | `	PdoStmtSweep(pConn);` |
|  101 |  210 | `	PH7_PdoSqliteBlobSweep(pConn);` |
|  101 |  211 | `	PH7_PdoSqliteClose(pConn);` |
|  101 |  212 | `	pConn->pOwner = 0;` |
|   74 |  213 | `}` |
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
|    4 |  225 | `{` |
|  934 |  226 | `	phl_pdo_stmt *pSt = (phl_pdo_stmt *)SyMemBackendAlloc(&pConn->pVm->sAllocator,` |
|    - |  227 | `		sizeof(phl_pdo_stmt));` |
|  934 |  228 | `	if( pSt == 0 ){` |
|  ! 0 |  229 | `		return 0;` |
|    - |  230 | `	}` |
|  934 |  231 | `	SyZero(pSt,sizeof(phl_pdo_stmt));` |
|  934 |  232 | `	pSt->pConn = pConn;` |
|  934 |  233 | `	pSt->iFetchMode = pConn->iDefaultFetch;` |
|  934 |  234 | `	pSt->iErrState = PDO_ERR_NONE;` |
|    - |  235 | ``	/* Retain the PDO object. `$db->query(...)` on a temporary connection hands`` |
|    - |  236 | `	 * back a statement that outlives it, and php keeps the connection alive` |
|    - |  237 | `	 * through exactly this reference -- without it the database closes while` |
|    - |  238 | `	 * the statement is still being read. */` |
|  934 |  239 | `	pSt->pConnObj = pConn->pOwner;` |
|  934 |  240 | `	if( pSt->pConnObj ){` |
|  934 |  241 | `		pSt->pConnObj->iRef++;` |
|  465 |  242 | `	}` |
|  934 |  243 | `	pSt->pNext = pConn->pStmts;` |
|  934 |  244 | `	pConn->pStmts = pSt;` |
|  934 |  245 | `	return pSt;` |
|  469 |  246 | `}` |
|    - |  247 | `/* Drop what bindValue()/bindParam() recorded. */` |
| 1860 |  248 | `static void PdoBindListFree(ph7_vm *pVm,phl_pdo_bind *pB)` |
|    4 |  249 | `{` |
| 1998 |  250 | `	while( pB ){` |
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
| 1864 |  261 | `}` |
|  930 |  262 | `static void PdoBindsClear(phl_pdo_stmt *pSt)` |
|    4 |  263 | `{` |
|  934 |  264 | `	PdoBindListFree(pSt->pConn->pVm,pSt->pBinds);` |
|  934 |  265 | `	PdoBindListFree(pSt->pConn->pVm,pSt->pColBinds);` |
|  934 |  266 | `	pSt->pBinds = 0;` |
|  934 |  267 | `	pSt->pColBinds = 0;` |
|  934 |  268 | `}` |
|  930 |  269 | `PH7_PRIVATE void PH7_PdoFreeStmt(phl_pdo_stmt *pSt)` |
|    4 |  270 | `{` |
|  934 |  271 | `	PdoBindsClear(pSt);` |
|  934 |  272 | `	PdoStmtLazyClear(pSt);` |
|  934 |  273 | `	if( pSt->pLazyRow ){` |
|    - |  274 | `		/* A lazy row RETAINS its statement object, so this cannot run while one` |
|    - |  275 | `		 * is alive -- except at VM teardown, which releases in no order. Cut the` |
|    - |  276 | `		 * link from both ends rather than leave the row reading freed memory. */` |
|    3 |  277 | `		PdoBlankSlot(pSt->pLazyRow);` |
|    3 |  278 | `		pSt->pLazyRow = 0;` |
|    1 |  279 | `	}` |
|  934 |  280 | `	PdoStmtClearFetchState(pSt);` |
|  934 |  281 | `	PH7_PdoSqliteFinalize(pSt);` |
|  934 |  282 | `	if( pSt->pConnObj ){` |
|    - |  283 | `		/* drop the reference taken at creation; the connection may go now */` |
|  934 |  284 | `		ph7_class_instance *pObj = pSt->pConnObj;` |
|  934 |  285 | `		pSt->pConnObj = 0;` |
|  934 |  286 | `		PH7_ClassInstanceUnref(pObj);` |
|  465 |  287 | `	}` |
|  934 |  288 | `}` |
|    - |  289 | `/* Finalize and free every statement of one connection. */` |
|  278 |  290 | `static void PdoStmtSweep(phl_pdo *pConn)` |
|    5 |  291 | `{` |
|  283 |  292 | `	phl_pdo_stmt *pSt = pConn->pStmts;` |
| 1213 |  293 | `	while( pSt ){` |
|  934 |  294 | `		phl_pdo_stmt *pNext = pSt->pNext;` |
|  934 |  295 | `		PdoBlankSlot(pSt->pOwner);` |
|  934 |  296 | `		PH7_PdoFreeStmt(pSt);` |
|  934 |  297 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pSt);` |
|  934 |  298 | `		pSt = pNext;` |
|    4 |  299 | `	}` |
|  283 |  300 | `	pConn->pStmts = 0;` |
|  283 |  301 | `}` |
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
|    4 |  317 | `{` |
|    - |  318 | `	SyString sAttr;` |
|    - |  319 | `	ph7_value *pRes;` |
|  924 |  320 | `	if( pThis == 0 ){` |
|  ! 0 |  321 | `		return -1;` |
|    - |  322 | `	}` |
|  924 |  323 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  924 |  324 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  924 |  325 | `	if( pRes == 0 ){` |
|  ! 0 |  326 | `		return -1;` |
|    - |  327 | `	}` |
|  924 |  328 | `	PH7_MemObjRelease(pRes);` |
|  924 |  329 | `	pRes->x.pOther = pSt;` |
|  924 |  330 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|  924 |  331 | `	pSt->pOwner = pThis;` |
|    - |  332 | ``	/* php's write_property refuses a store to `queryString` on a statement a`` |
|    - |  333 | ``	 * driver built -- and takes one on a `new PDOStatement()`, which has no`` |
|    - |  334 | `	 * cursor for it to describe. */` |
|  924 |  335 | `	PH7_NativeMarkAttrReadOnly(pThis,"queryString");` |
|  924 |  336 | `	return 0;` |
|  464 |  337 | `}` |
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
|    5 |  354 | `{` |
| 1435 |  355 | `	pConn->iErrState = PDO_ERR_OK;` |
| 1435 |  356 | `	pConn->iDrvCode = 0;` |
| 1435 |  357 | `	pConn->bNoDrvDetail = 0;` |
| 1435 |  358 | `	SyMemcpy("00000",pConn->zSqlState,sizeof("00000"));` |
| 1435 |  359 | `	if( pConn->zDrvMsg ){` |
|   27 |  360 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);` |
|   27 |  361 | `		pConn->zDrvMsg = 0;` |
|   13 |  362 | `	}` |
| 1435 |  363 | `}` |
|    - |  364 | `/*` |
|    - |  365 | ` * php clears the handle's error at the ENTRY of most verbs -- exec, query,` |
|    - |  366 | ` * prepare, quote, lastInsertId and both attribute accessors -- so a failure is` |
|    - |  367 | ` * invisible to errorCode() as soon as any of them is called, even on a handle` |
|    - |  368 | ` * that has never run anything (NULL becomes "00000"). The verbs that do NOT` |
|    - |  369 | ` * clear are the two reporters themselves and the transaction quartet.` |
|    - |  370 | ` */` |
| 1430 |  371 | `PH7_PRIVATE void PH7_PdoTouch(phl_pdo *pConn)` |
|    5 |  372 | `{` |
| 1435 |  373 | `	PH7_PdoClearError(pConn);` |
| 1435 |  374 | `}` |
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
|    5 |  964 | `{` |
|  245 |  965 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - |  966 | `	const char *zSql;` |
|  245 |  967 | `	int nSql = 0;   /* the length is only written when the argument IS read */` |
|    - |  968 | `	ph7_int64 nChange;` |
|  245 |  969 | `	if( pConn == 0 ){` |
|  ! 0 |  970 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  971 | `	}` |
|  245 |  972 | `	PH7_PdoTouch(pConn);` |
|  245 |  973 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|  245 |  974 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 |  975 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  976 | `			"PDO::exec(): Argument #1 ($statement) must not be empty");` |
|    - |  977 | `	}` |
|  243 |  978 | `	nChange = PH7_PdoSqliteExec(pConn,zSql,nSql);` |
|  243 |  979 | `	if( nChange < 0 ){` |
|   17 |  980 | `		ph7_result_bool(pCtx,0);` |
|   17 |  981 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::exec");` |
|    - |  982 | `	}` |
|  227 |  983 | `	ph7_result_int64(pCtx,nChange);` |
|  227 |  984 | `	return PH7_OK;` |
|  125 |  985 | `}` |
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
|    4 | 1051 | `{` |
| 1136 | 1052 | `	pSt->iErrState = PDO_ERR_OK;` |
| 1136 | 1053 | `	SyMemcpy("00000",pSt->zSqlState,sizeof("00000"));` |
| 1136 | 1054 | `}` |
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
|    4 | 1150 | `{` |
| 1478 | 1151 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
| 1478 | 1152 | `	if( pSt->zFetchClass ){` |
|   25 | 1153 | `		SyMemBackendFree(&pVm->sAllocator,pSt->zFetchClass);` |
|   25 | 1154 | `		pSt->zFetchClass = 0;` |
|   25 | 1155 | `		pSt->nFetchClass = 0;` |
|   12 | 1156 | `	}` |
| 1478 | 1157 | `	if( pSt->pFetchArgs ){` |
|    9 | 1158 | `		ph7_release_value(pVm,pSt->pFetchArgs);` |
|    9 | 1159 | `		pSt->pFetchArgs = 0;` |
|    4 | 1160 | `	}` |
| 1478 | 1161 | `	if( pSt->pFetchInto ){` |
|   11 | 1162 | `		ph7_class_instance *pObj = pSt->pFetchInto;` |
|   11 | 1163 | `		pSt->pFetchInto = 0;` |
|   11 | 1164 | `		PH7_ClassInstanceUnref(pObj);` |
|    5 | 1165 | `	}` |
| 1478 | 1166 | `}` |
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
|    4 | 1255 | `{` |
| 1042 | 1256 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
| 1042 | 1257 | `	if( pSt->pLazyVals ){` |
|   61 | 1258 | `		ph7_release_value(pVm,pSt->pLazyVals);` |
|   61 | 1259 | `		pSt->pLazyVals = 0;` |
|   30 | 1260 | `	}` |
| 1042 | 1261 | `}` |
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
|    4 | 1327 | `{` |
| 1482 | 1328 | `	int rc = PH7_PdoSqliteStep(pSt);` |
| 1482 | 1329 | `	if( rc < 0 ){` |
|    5 | 1330 | `		pSt->bDone = 1;` |
|    5 | 1331 | `		pSt->bRowPending = 0;` |
|    5 | 1332 | `		return -1;` |
|    - | 1333 | `	}` |
| 1478 | 1334 | `	pSt->bRowPending = (rc == 1);` |
| 1478 | 1335 | `	pSt->bDone = (rc == 0);` |
| 1478 | 1336 | `	return rc;` |
|  743 | 1337 | `}` |
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
|  274 | 2314 | `static phl_pdo_stmt * PdoRowStmt(ph7_class_instance *pThis)` |
|    1 | 2315 | `{` |
|    - | 2316 | `	SyString sAttr;` |
|    - | 2317 | `	ph7_value *pRes;` |
|  275 | 2318 | `	if( pThis == 0 ){` |
|  ! 0 | 2319 | `		return 0;` |
|    - | 2320 | `	}` |
|  275 | 2321 | `	SyStringInitFromBuf(&sAttr,PDOROW_RES,sizeof(PDOROW_RES)-1);` |
|  275 | 2322 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  275 | 2323 | `	if( pRes == 0 \|\| !ph7_value_is_resource(pRes) ){` |
|  ! 0 | 2324 | `		return 0;` |
|    - | 2325 | `	}` |
|  275 | 2326 | `	return (phl_pdo_stmt *)ph7_value_to_resource(pRes);` |
|  138 | 2327 | `}` |
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
|    - | 2439 | ` * row -- and never its "would you take a write" question, which no rail asks of a` |
|    - | 2440 | ` * class that refuses every write outright. A name that is no column at all is` |
|    - | 2441 | ` * NULL to a read and false to an` |
|    - | 2442 | `` * isset(), never a warning -- and `queryString` is answered from the STATEMENT`` |
|    - | 2443 | ` * BEFORE any column is looked at, so a query selecting a column of that name` |
|    - | 2444 | ` * cannot shadow it. The has side does NOT know the name at all, which is why` |
|    - | 2445 | `` * `isset($row->queryString)` is false while reading it works.`` |
|    - | 2446 | ` */` |
|  172 | 2447 | `static void PdoRowProp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|    1 | 2448 | `{` |
|  173 | 2449 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|  173 | 2450 | `	const char *zName = SyStringData(pCtx->pName);` |
|  173 | 2451 | `	int nName = (int)SyStringLength(pCtx->pName);` |
|    - | 2452 | `	int iCol;` |
|   86 | 2453 | `	SXUNUSED(pVm);` |
|  173 | 2454 | `	if( pCtx->iMode == PH7_NATIVE_PROP_WRITE ){` |
|   15 | 2455 | `		pCtx->zThrowClass = "Error";` |
|   15 | 2456 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|    - | 2457 | `			"Cannot write to PDORow property");` |
|   15 | 2458 | `		return;` |
|    - | 2459 | `	}` |
|  159 | 2460 | `	if( pCtx->iMode == PH7_NATIVE_PROP_UNSET ){` |
|    5 | 2461 | `		pCtx->zThrowClass = "Error";` |
|    5 | 2462 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|    - | 2463 | `			"Cannot unset PDORow property");` |
|    5 | 2464 | `		return;` |
|    - | 2465 | `	}` |
|  155 | 2466 | `	if( pCtx->iMode == PH7_NATIVE_PROP_OWNS ){` |
|    - | 2467 | `		/* Never: every write this class sees is refused above, at the member` |
|    - | 2468 | `		 * opcode, so no write rail ever asks whether the handler would take one. */` |
|    5 | 2469 | `		return;` |
|    - | 2470 | `	}` |
|  151 | 2471 | `	pCtx->bAnswered = 1;` |
|  151 | 2472 | `	if( pSt == 0 ){` |
|  ! 0 | 2473 | `		return;   /* the statement is gone: every name reads null */` |
|    - | 2474 | `	}` |
|  151 | 2475 | `	if( (pCtx->iMode == PH7_NATIVE_PROP_READ) && PdoRowIsQueryString(zName,nName) ){` |
|   13 | 2476 | `		PdoRowQueryString(pSt,pCtx->pResult);` |
|   13 | 2477 | `		return;` |
|    - | 2478 | `	}` |
|  139 | 2479 | `	iCol = PdoRowColumnOf(pSt,zName,nName);` |
|  139 | 2480 | `	if( iCol >= 0 ){` |
|  121 | 2481 | `		PdoRowColumnValue(pSt,iCol,pCtx->pResult);` |
|   60 | 2482 | `	}` |
|  139 | 2483 | `	if( pCtx->iMode != PH7_NATIVE_PROP_READ ){` |
|    - | 2484 | `		/* php's has_property fetches the value and judges it -- by NULL-ness for` |
|    - | 2485 | `		 * isset() and by TRUTH for property_exists(), which asks the same handler` |
|    - | 2486 | `		 * with a non-zero check_empty. Either way it does NOT know the name` |
|    - | 2487 | ``		 * `queryString`, which is why reading one works where isset() on it is`` |
|    - | 2488 | `		 * false. */` |
|    - | 2489 | `		int bSet;` |
|   57 | 2490 | `		if( pCtx->iMode == PH7_NATIVE_PROP_ISSET ){` |
|   25 | 2491 | `			bSet = iCol >= 0 && (pCtx->pResult->iFlags & MEMOBJ_NULL) == 0;` |
|   13 | 2492 | `		}else{` |
|    - | 2493 | `			/* php's handler answers the two check_empty questions the same way, so` |
|    - | 2494 | ``			 * `empty($row->c)` and `property_exists($row,'c')` are both the column's`` |
|    - | 2495 | `			 * TRUTH -- a column holding 0 is isset() and is neither of these. */` |
|   33 | 2496 | `			bSet = iCol >= 0 && ph7_value_to_bool(pCtx->pResult);` |
|    - | 2497 | `		}` |
|   57 | 2498 | `		PH7_MemObjRelease(pCtx->pResult);` |
|   57 | 2499 | `		ph7_value_bool(pCtx->pResult,bSet);` |
|   28 | 2500 | `	}` |
|   87 | 2501 | `}` |
|    - | 2502 | `/*` |
|    - | 2503 | ` * php's read_dimension / has_dimension for the row, and the three refusals its` |
|    - | 2504 | ` * write side gives. The offset is the property NAME spelled as a value: an` |
|    - | 2505 | ` * integer is a column number outright, and everything else is converted to a` |
|    - | 2506 | ` * string first -- which is where an object offset raises php's` |
|    - | 2507 | ` * "could not be converted to string" Error and an array warns.` |
|    - | 2508 | ` */` |
|   48 | 2509 | `static void PdoRowDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|    1 | 2510 | `{` |
|   49 | 2511 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|    - | 2512 | `	ph7_value sKey;` |
|    - | 2513 | `	const char *zName;` |
|    - | 2514 | `	int nName, iCol;` |
|   48 | 2515 | `	if( pCtx->iMode == PH7_NATIVE_DIM_WRITE \|\| pCtx->iMode == PH7_NATIVE_DIM_APPEND` |
|   44 | 2516 | `	 \|\| pCtx->iMode == PH7_NATIVE_DIM_UNSET ){` |
|   11 | 2517 | `		pCtx->zThrowClass = "Error";` |
|   16 | 2518 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),"Cannot %s PDORow offset",` |
|   10 | 2519 | `			pCtx->iMode == PH7_NATIVE_DIM_WRITE ? "write to"` |
|    6 | 2520 | `			: (pCtx->iMode == PH7_NATIVE_DIM_APPEND ? "append to" : "unset"));` |
|   13 | 2521 | `		return;` |
|    - | 2522 | `	}` |
|   39 | 2523 | `	if( pCtx->pOffset == 0 \|\| pSt == 0 ){` |
|  ! 0 | 2524 | ``		return;   /* `$row[]` read, or a statement that is gone: null */`` |
|    - | 2525 | `	}` |
|   39 | 2526 | `	if( pCtx->pOffset->iFlags & MEMOBJ_OBJ ){` |
|  ! 0 | 2527 | `		ph7_class_instance *pObj = (ph7_class_instance *)pCtx->pOffset->x.pOther;` |
|  ! 0 | 2528 | `		pCtx->zThrowClass = "Error";` |
|  ! 0 | 2529 | `		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|    - | 2530 | `			"Object of class %.*s could not be converted to string",` |
|  ! 0 | 2531 | `			(int)pObj->pClass->sName.nByte,pObj->pClass->sName.zString);` |
|  ! 0 | 2532 | `		return;` |
|    - | 2533 | `	}` |
|   39 | 2534 | `	PH7_MemObjInit(pVm,&sKey);` |
|   39 | 2535 | `	PH7_MemObjStore(pCtx->pOffset,&sKey);` |
|   39 | 2536 | `	if( sKey.iFlags & MEMOBJ_HASHMAP ){` |
|  ! 0 | 2537 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|  ! 0 | 2538 | `	}` |
|   39 | 2539 | `	PH7_MemObjToString(&sKey);` |
|   39 | 2540 | `	zName = (const char *)SyBlobData(&sKey.sBlob);` |
|   39 | 2541 | `	nName = (int)SyBlobLength(&sKey.sBlob);` |
|   39 | 2542 | `	if( pCtx->iMode == PH7_NATIVE_DIM_READ && zName && PdoRowIsQueryString(zName,nName) ){` |
|    5 | 2543 | `		PdoRowQueryString(pSt,pCtx->pResult);` |
|    5 | 2544 | `		PH7_MemObjRelease(&sKey);` |
|    5 | 2545 | `		return;` |
|    - | 2546 | `	}` |
|   35 | 2547 | `	iCol = PdoRowColumnOf(pSt,zName ? zName : "",zName ? nName : 0);` |
|   35 | 2548 | `	if( iCol >= 0 ){` |
|   21 | 2549 | `		PdoRowColumnValue(pSt,iCol,pCtx->pResult);` |
|   10 | 2550 | `	}` |
|   35 | 2551 | `	if( pCtx->iMode == PH7_NATIVE_DIM_ISSET ){` |
|    7 | 2552 | `		int bSet = iCol >= 0 && (pCtx->pResult->iFlags & MEMOBJ_NULL) == 0;` |
|    7 | 2553 | `		PH7_MemObjRelease(pCtx->pResult);` |
|    7 | 2554 | `		ph7_value_bool(pCtx->pResult,bSet);` |
|    3 | 2555 | `	}` |
|   35 | 2556 | `	PH7_MemObjRelease(&sKey);` |
|   25 | 2557 | `}` |
|    - | 2558 | `/*` |
|    - | 2559 | `` * php's get_debug_info for the row: `queryString` and then every column of the`` |
|    - | 2560 | ` * row it is sitting on, which is why var_dump() shows what get_object_vars()` |
|    - | 2561 | ` * does not. The get_properties half shows nothing at all, so (array), var_export` |
|    - | 2562 | ` * and json_encode answer empty.` |
|    - | 2563 | ` */` |
|   20 | 2564 | `static sxi32 PdoRowPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|    1 | 2565 | `{` |
|   21 | 2566 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|    - | 2567 | `	ph7_value sKey,sVal;` |
|    - | 2568 | `	int nCol,iCol;` |
|   21 | 2569 | `	if( !bDebug \|\| pSt == 0 ){` |
|    7 | 2570 | `		return SXRET_OK;` |
|    - | 2571 | `	}` |
|   15 | 2572 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|   15 | 2573 | `	PH7_MemObjInit(pVm,&sVal);` |
|   15 | 2574 | `	PH7_MemObjStringAppend(&sKey,"queryString",sizeof("queryString")-1);` |
|   15 | 2575 | `	if( pSt->pOwner ){` |
|   15 | 2576 | `		ph7_value *pQs = PH7_NativeAttr(pSt->pOwner,"queryString");` |
|   15 | 2577 | `		if( pQs ){` |
|   15 | 2578 | `			PH7_MemObjStore(pQs,&sVal);` |
|    7 | 2579 | `		}` |
|    7 | 2580 | `	}` |
|   15 | 2581 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    - | 2582 | `	/* The COLUMNS come from the statement rather than from the capture: php` |
|    - | 2583 | `	 * describes them once and shows them for as long as the cursor exists, so a` |
|    - | 2584 | `	 * row whose walk has run out (or whose cursor was closed) still prints every` |
|    - | 2585 | `	 * name, each holding null. */` |
|   15 | 2586 | `	nCol = PdoRowColumnCount(pSt);` |
|   45 | 2587 | `	for( iCol = 0 ; iCol < nCol ; ++iCol ){` |
|    - | 2588 | `		SyBlob sColName;` |
|    - | 2589 | `		int nName;` |
|    - | 2590 | `		const char *zName;` |
|   31 | 2591 | `		SyBlobInit(&sColName,&pVm->sAllocator);` |
|   31 | 2592 | `		PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sColName);` |
|   31 | 2593 | `		zName = (const char *)SyBlobData(&sColName);` |
|   31 | 2594 | `		nName = (int)SyBlobLength(&sColName) - 1;   /* less the terminator */` |
|   31 | 2595 | `		if( zName == 0 \|\| nName < 0 \|\| PdoRowIsQueryString(zName,nName) ){` |
|    - | 2596 | `			/* php builds the columns as a table of their own and merges it` |
|    - | 2597 | `			 * BEHIND the queryString entry, so a column of that name is the one` |
|    - | 2598 | `			 * that loses -- while two columns sharing any other name collapse` |
|    - | 2599 | `			 * to the LAST of them, which the update below does. */` |
|    3 | 2600 | `			SyBlobRelease(&sColName);` |
|    3 | 2601 | `			continue;` |
|    - | 2602 | `		}` |
|   29 | 2603 | `		PH7_MemObjRelease(&sKey);` |
|   29 | 2604 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|   29 | 2605 | `		PH7_MemObjStringAppend(&sKey,zName,(sxu32)nName);` |
|   29 | 2606 | `		PH7_MemObjRelease(&sVal);` |
|   29 | 2607 | `		PH7_MemObjInit(pVm,&sVal);` |
|   29 | 2608 | `		PdoRowColumnValue(pSt,iCol,&sVal);` |
|   29 | 2609 | `		ph7_array_add_elem(pOut,&sKey,&sVal);` |
|   29 | 2610 | `		SyBlobRelease(&sColName);` |
|   15 | 2611 | `	}` |
|   15 | 2612 | `	PH7_MemObjRelease(&sKey);` |
|   15 | 2613 | `	PH7_MemObjRelease(&sVal);` |
|   15 | 2614 | `	return SXRET_OK;` |
|   11 | 2615 | `}` |
|    - | 2616 | `/* The row is going away: the statement must stop pointing at it. */` |
|   34 | 2617 | `static void PdoRowInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    1 | 2618 | `{` |
|   35 | 2619 | `	phl_pdo_stmt *pSt = PdoRowStmt(pThis);` |
|   17 | 2620 | `	SXUNUSED(pVm);` |
|   35 | 2621 | `	if( pSt && pSt->pLazyRow == pThis ){` |
|   35 | 2622 | `		pSt->pLazyRow = 0;` |
|   35 | 2623 | `		PdoStmtLazyClear(pSt);` |
|   17 | 2624 | `	}` |
|   35 | 2625 | `}` |
|    - | 2626 | `/*` |
|    - | 2627 | ` * The statement's row object, built on first use and CAPTURING the row under` |
|    - | 2628 | ` * the cursor, which it also marks as spent. Answers the object with a` |
|    - | 2629 | ` * reference of the caller's own, or 0 when it could not be made. Shared by` |
|    - | 2630 | ` * fetch() and the foreach iterator: php answers both from one lazy row.` |
|    - | 2631 | ` */` |
|   46 | 2632 | `static ph7_class_instance * PdoLazyRowFor(ph7_vm *pVm,phl_pdo_stmt *pSt)` |
|    1 | 2633 | `{` |
|   47 | 2634 | `	ph7_class_instance *pRow = pSt->pLazyRow;` |
|   47 | 2635 | `	PdoBoundColumnsForRow(pVm,pSt);` |
|   47 | 2636 | `	PdoStmtLazyCapture(pSt);` |
|   47 | 2637 | `	if( pRow == 0 ){` |
|   37 | 2638 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,"PDORow",sizeof("PDORow")-1,FALSE,0);` |
|    - | 2639 | `		SyString sAttr;` |
|    - | 2640 | `		ph7_value *pSlot;` |
|   37 | 2641 | `		pRow = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|   37 | 2642 | `		if( pRow == 0 ){` |
|  ! 0 | 2643 | `			return 0;` |
|    - | 2644 | `		}` |
|   37 | 2645 | `		SyStringInitFromBuf(&sAttr,PDOROW_RES,sizeof(PDOROW_RES)-1);` |
|   37 | 2646 | `		pSlot = PH7_ClassInstanceFetchAttr(pRow,&sAttr);` |
|   37 | 2647 | `		if( pSlot == 0 ){` |
|  ! 0 | 2648 | `			PH7_ClassInstanceUnref(pRow);` |
|  ! 0 | 2649 | `			return 0;` |
|    - | 2650 | `		}` |
|   37 | 2651 | `		PH7_MemObjRelease(pSlot);` |
|   37 | 2652 | `		pSlot->x.pOther = pSt;` |
|   37 | 2653 | `		MemObjSetType(pSlot,MEMOBJ_RES);` |
|    - | 2654 | `		/* Retain the STATEMENT object through a slot of the row's own: php's` |
|    - | 2655 | ``		 * row keeps its statement alive, so `unset($stmt)` leaves the row`` |
|    - | 2656 | `		 * reading and the database open. The statement's pointer back here is` |
|    - | 2657 | `		 * deliberately NOT a reference -- that pair would be a cycle no` |
|    - | 2658 | `		 * refcount can break. */` |
|   37 | 2659 | `		SyStringInitFromBuf(&sAttr,PDOROW_STMT,sizeof(PDOROW_STMT)-1);` |
|   37 | 2660 | `		pSlot = PH7_ClassInstanceFetchAttr(pRow,&sAttr);` |
|   37 | 2661 | `		if( pSlot && pSt->pOwner ){` |
|   37 | 2662 | `			PH7_MemObjRelease(pSlot);` |
|   37 | 2663 | `			pSt->pOwner->iRef++;` |
|   37 | 2664 | `			pSlot->x.pOther = pSt->pOwner;` |
|   37 | 2665 | `			MemObjSetType(pSlot,MEMOBJ_OBJ);` |
|   18 | 2666 | `		}` |
|   37 | 2667 | `		pSt->pLazyRow = pRow;` |
|   19 | 2668 | `	}else{` |
|   11 | 2669 | `		pRow->iRef++;   /* the caller's own reference */` |
|    - | 2670 | `	}` |
|   47 | 2671 | `	pSt->bRowPending = 0;` |
|   47 | 2672 | `	return pRow;` |
|   24 | 2673 | `}` |
|    - | 2674 | `/*` |
|    - | 2675 | ` * PDO::FETCH_LAZY: hand the row object back and move the cursor on. The row` |
|    - | 2676 | ` * carries no values of its own -- the capture on the statement is what it` |
|    - | 2677 | ` * reads -- so a second lazy fetch answers the SAME object showing the next` |
|    - | 2678 | ` * row, which is php.` |
|    - | 2679 | ` */` |
|   36 | 2680 | `static int PdoFetchLazyRow(ph7_context *pCtx,phl_pdo_stmt *pSt)` |
|    1 | 2681 | `{` |
|    - | 2682 | `	ph7_class_instance *pRow;` |
|   37 | 2683 | `	if( !PdoStmtHasRow(pSt) ){` |
|    5 | 2684 | `		PdoStmtOk(pSt);` |
|    5 | 2685 | `		ph7_result_bool(pCtx,0);` |
|    5 | 2686 | `		return PH7_OK;` |
|    - | 2687 | `	}` |
|   33 | 2688 | `	pRow = PdoLazyRowFor(pCtx->pVm,pSt);` |
|   33 | 2689 | `	if( pRow == 0 ){` |
|  ! 0 | 2690 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2691 | `	}` |
|   33 | 2692 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2693 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2694 | `		PH7_ClassInstanceUnref(pRow);` |
|  ! 0 | 2695 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2696 | `	}` |
|   33 | 2697 | `	PdoStmtOk(pSt);` |
|   33 | 2698 | `	PH7_NativeResultObject(pCtx,pRow);` |
|   33 | 2699 | `	return PH7_OK;` |
|   19 | 2700 | `}` |
|    - | 2701 | `/*` |
|    - | 2702 | ` * PDOStatement::fetch(int $mode = PDO::FETCH_DEFAULT, ...): mixed` |
|    - | 2703 | ` *` |
|    - | 2704 | ` * FETCH_DEFAULT means the connection's ATTR_DEFAULT_FETCH_MODE, which is` |
|    - | 2705 | ` * FETCH_BOTH unless the script changed it -- so a bare fetch() answers every` |
|    - | 2706 | ` * column twice, once under its name and once under its position.` |
|    - | 2707 | ` */` |
|  346 | 2708 | `static int vm_builtin_PDOStatement_fetch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2709 | `{` |
|  348 | 2710 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2711 | `	ph7_value *pRow;` |
|    - | 2712 | `	int iMode;` |
|    - | 2713 | `	sxi32 rcFlags;` |
|  348 | 2714 | `	if( pSt == 0 ){` |
|    3 | 2715 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2716 | `	}` |
|  346 | 2717 | `	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;` |
|  346 | 2718 | `	rcFlags = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::fetch",1,"mode");` |
|  346 | 2719 | `	if( rcFlags != PH7_OK ){` |
|  ! 0 | 2720 | `		return rcFlags;` |
|    - | 2721 | `	}` |
|  346 | 2722 | `	if( iMode == PDO_FETCH_DEFAULT ){` |
|  176 | 2723 | `		iMode = pSt->iFetchMode;` |
|   87 | 2724 | `	}` |
|  346 | 2725 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_DEFAULT ){` |
|    - | 2726 | `		/* A statement whose own mode is FETCH_DEFAULT -- which only a` |
|    - | 2727 | `		 * connection whose ATTR_DEFAULT_FETCH_MODE is 0 leaves it on -- has no` |
|    - | 2728 | `		 * mode to fall back to, and php says so at the fetch. */` |
|  ! 0 | 2729 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2730 | `			"PDOStatement::fetch(): Argument #1 ($mode) must be a bitmask of "` |
|    - | 2731 | `			"PDO::FETCH_* constants");` |
|    - | 2732 | `	}` |
|  346 | 2733 | `	if( PdoBoundColumnsBad(pSt) ){` |
|   11 | 2734 | `		return PdoBoundColumnsRefuse(pCtx,pSt,0);` |
|    - | 2735 | `	}` |
|  336 | 2736 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_LAZY ){` |
|   37 | 2737 | `		return PdoFetchLazyRow(pCtx,pSt);` |
|    - | 2738 | `	}` |
|  300 | 2739 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_KEY_PAIR ){` |
|    - | 2740 | `		/* php's own fetch() cannot do this mode: it builds a value var_dump` |
|    - | 2741 | `		 * crashes on and json_encode refuses, and one spelling of the same call` |
|    - | 2742 | `		 * aborts the process (§10 -- a php defect PHL does not reproduce). The` |
|    - | 2743 | `		 * honest answer is the one the mode NAMES and fetchAll() builds: the` |
|    - | 2744 | `		 * row as a single key => value pair. */` |
|    - | 2745 | `		ph7_value *pPair,*pRowVals,*pKey,*pVal;` |
|   11 | 2746 | `		if( PH7_PdoSqliteColumnCount(pSt) != 2 ){` |
|    3 | 2747 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 2748 | `				"SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires "` |
|    - | 2749 | `				"the result set to contain exactly 2 columns.");` |
|    - | 2750 | `		}` |
|    9 | 2751 | `		if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2752 | `			PdoStmtOk(pSt);` |
|    3 | 2753 | `			ph7_result_bool(pCtx,0);` |
|    3 | 2754 | `			return PH7_OK;` |
|    - | 2755 | `		}` |
|    7 | 2756 | `		pPair    = ph7_context_new_array(pCtx);` |
|    7 | 2757 | `		pRowVals = ph7_context_new_array(pCtx);` |
|    7 | 2758 | `		if( pPair == 0 \|\| pRowVals == 0 ){` |
|  ! 0 | 2759 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 2760 | `		}` |
|    7 | 2761 | `		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pRowVals) ){` |
|  ! 0 | 2762 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 2763 | `			return PH7_OK;` |
|    - | 2764 | `		}` |
|    7 | 2765 | `		pKey = PdoArrayAtInt(pCtx->pVm,pRowVals,0);` |
|    7 | 2766 | `		pVal = PdoArrayAtInt(pCtx->pVm,pRowVals,1);` |
|    7 | 2767 | `		ph7_array_add_elem(pPair,pKey,pVal);` |
|    7 | 2768 | `		ph7_result_value(pCtx,pPair);` |
|    7 | 2769 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2770 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2771 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2772 | `		}` |
|    7 | 2773 | `		PdoStmtOk(pSt);` |
|    7 | 2774 | `		return PH7_OK;` |
|    - | 2775 | `	}` |
|  289 | 2776 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_COLUMN ){` |
|    - | 2777 | `		/* a statement told to fetch one COLUMN answers that column from here` |
|    - | 2778 | `		 * on, whichever verb asks for the row */` |
|    - | 2779 | `		ph7_value *pOneRow,*pOne;` |
|   19 | 2780 | `		if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2781 | `			PdoStmtOk(pSt);` |
|    3 | 2782 | `			ph7_result_bool(pCtx,0);` |
|    3 | 2783 | `			return PH7_OK;` |
|    - | 2784 | `		}` |
|   17 | 2785 | `		if( pSt->iFetchColumn >= PH7_PdoSqliteColumnCount(pSt) ){` |
|    - | 2786 | `			/* php checks the width only once it has a ROW to read it from, so a` |
|    - | 2787 | `			 * cursor with nothing left answers false rather than refusing. */` |
|    5 | 2788 | `			return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    - | 2789 | `		}` |
|   13 | 2790 | `		pOneRow = ph7_context_new_array(pCtx);` |
|   13 | 2791 | `		if( pOneRow == 0 ){` |
|  ! 0 | 2792 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 2793 | `		}` |
|   13 | 2794 | `		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pOneRow) ){` |
|  ! 0 | 2795 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 2796 | `			return PH7_OK;` |
|    - | 2797 | `		}` |
|   13 | 2798 | `		pOne = PdoArrayAtInt(pCtx->pVm,pOneRow,(sxi64)pSt->iFetchColumn);` |
|   13 | 2799 | `		if( pOne ){` |
|   13 | 2800 | `			ph7_result_value(pCtx,pOne);` |
|    7 | 2801 | `		}else{` |
|  ! 0 | 2802 | `			ph7_result_null(pCtx);` |
|    - | 2803 | `		}` |
|   13 | 2804 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2805 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2806 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2807 | `		}` |
|   13 | 2808 | `		PdoStmtOk(pSt);` |
|   13 | 2809 | `		return PH7_OK;` |
|    - | 2810 | `	}` |
|  271 | 2811 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_BOUND ){` |
|   21 | 2812 | `		if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2813 | `			PdoStmtOk(pSt);` |
|    3 | 2814 | `			ph7_result_bool(pCtx,0);` |
|    3 | 2815 | `			return PH7_OK;` |
|    - | 2816 | `		}` |
|   19 | 2817 | `		PdoBoundColumnsForRow(pCtx->pVm,pSt);` |
|   19 | 2818 | `		pSt->bRowPending = 0;` |
|   19 | 2819 | `		ph7_result_bool(pCtx,1);` |
|   19 | 2820 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2821 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2822 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2823 | `		}` |
|   19 | 2824 | `		PdoStmtOk(pSt);` |
|   19 | 2825 | `		return PH7_OK;` |
|    - | 2826 | `	}` |
|  250 | 2827 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_CLASS` |
|  238 | 2828 | `	 \|\| (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_INTO ){` |
|   39 | 2829 | `		return PdoFetchObjectRow(pCtx,pSt,iMode,0,0,"PDOStatement::fetch");` |
|    - | 2830 | `	}` |
|  213 | 2831 | `	if( !PdoStmtHasRow(pSt) ){` |
|   21 | 2832 | `		PdoStmtOk(pSt);` |
|   21 | 2833 | `		ph7_result_bool(pCtx,0);` |
|   21 | 2834 | `		return PH7_OK;` |
|    - | 2835 | `	}` |
|  193 | 2836 | `	pRow = ph7_context_new_array(pCtx);` |
|  193 | 2837 | `	if( pRow == 0 ){` |
|  ! 0 | 2838 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2839 | `	}` |
|  193 | 2840 | `	if( !PdoStmtRow(pCtx->pVm,pSt,iMode & PDO_FETCH_MODE_MASK,pRow) ){` |
|  ! 0 | 2841 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2842 | `		return PH7_OK;` |
|    - | 2843 | `	}` |
|  193 | 2844 | `	ph7_result_value(pCtx,pRow);` |
|    - | 2845 | `	/* step ahead so the next call knows whether a row is waiting without` |
|    - | 2846 | `	 * having to ask twice */` |
|  193 | 2847 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2848 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2849 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 2850 | `	}` |
|  193 | 2851 | `	PdoStmtOk(pSt);` |
|  193 | 2852 | `	return PH7_OK;` |
|  175 | 2853 | `}` |
|    - | 2854 | `/*` |
|    - | 2855 | ` * PDOStatement::getColumnMeta(int $column): array\|false` |
|    - | 2856 | ` *` |
|    - | 2857 | ` * php's eight keys. Two of them describe different things and are routinely` |
|    - | 2858 | ``  * confused: `sqlite:decl_type` is what the SCHEMA declares, and `native_type` `` |
|    - | 2859 | ` * is the type of the value in the CURRENT row -- so a TEXT column holding NULL` |
|    - | 2860 | ` * reports "TEXT" and "null" at once, and an exhausted cursor reports "null"` |
|    - | 2861 | ` * for every column.` |
|    - | 2862 | ` *` |
|    - | 2863 | ` * A column that does not exist answers false, and php reports the last STEP's` |
|    - | 2864 | ` * result code as the driver error while doing so: that is why asking for` |
|    - | 2865 | ` * column 99 while a row is up comes back as "100 another row available"` |
|    - | 2866 | ` * instead of anything about the index.` |
|    - | 2867 | ` */` |
|    8 | 2868 | `static int vm_builtin_PDOStatement_getColumnMeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2869 | `{` |
|    9 | 2870 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2871 | `	ph7_value *pMeta,*pCell,*pFlags;` |
|    - | 2872 | `	ph7_int64 iCol;` |
|    - | 2873 | `	const char *zDecl,*zTable,*zName;` |
|    - | 2874 | `	int iType,iPdoType;` |
|    - | 2875 | `	const char *zNative;` |
|    - | 2876 | `	SyBlob sName;` |
|    9 | 2877 | `	if( pSt == 0 ){` |
|  ! 0 | 2878 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2879 | `	}` |
|    9 | 2880 | `	iCol = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    9 | 2881 | `	if( iCol < 0 ){` |
|    3 | 2882 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2883 | `			"PDOStatement::getColumnMeta(): Argument #1 ($column) must be greater than "` |
|    - | 2884 | `			"or equal to 0");` |
|    - | 2885 | `	}` |
|    7 | 2886 | `	if( iCol >= (ph7_int64)PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 2887 | `		int iStep = PH7_PdoSqliteLastStepCode(pSt);` |
|    3 | 2888 | `		ph7_result_bool(pCtx,0);` |
|    - | 2889 | `		/* php reports the last STEP's code as the driver detail here, which is` |
|    - | 2890 | `		 * why an out-of-range index talks about a row being available */` |
|    4 | 2891 | `		PH7_PdoSetError(pSt->pConn,"HY000",iStep,` |
|    1 | 2892 | `			iStep == 100 ? "another row available" : "no more rows available");` |
|    3 | 2893 | `		pSt->pConn->iErrState = PDO_ERR_OK;   /* the CONNECTION did not fail */` |
|    3 | 2894 | `		SyMemcpy("00000",pSt->pConn->zSqlState,sizeof("00000"));` |
|    3 | 2895 | `		PdoStmtFailed(pSt,"HY000");` |
|    3 | 2896 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::getColumnMeta");` |
|    - | 2897 | `	}` |
|    5 | 2898 | `	pMeta = ph7_context_new_array(pCtx);` |
|    5 | 2899 | `	pCell = ph7_context_new_scalar(pCtx);` |
|    5 | 2900 | `	pFlags = ph7_context_new_array(pCtx);` |
|    5 | 2901 | `	if( pMeta == 0 \|\| pCell == 0 \|\| pFlags == 0 ){` |
|  ! 0 | 2902 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2903 | `	}` |
|    5 | 2904 | `	iType = pSt->bRowPending ? PH7_PdoSqliteColumnType(pSt,(int)iCol) : SQLITE_NULL;` |
|    5 | 2905 | `	switch( iType ){` |
|    3 | 2906 | `		case SQLITE_INTEGER: zNative = "integer"; iPdoType = PDO_PARAM_INT; break;` |
|  ! 0 | 2907 | `		case SQLITE_FLOAT:   zNative = "double";  iPdoType = PDO_PARAM_STR; break;` |
|  ! 0 | 2908 | `		case SQLITE_BLOB:    zNative = "blob";    iPdoType = PDO_PARAM_LOB; break;` |
|    3 | 2909 | `		case SQLITE_NULL:    zNative = "null";    iPdoType = PDO_PARAM_NULL; break;` |
|  ! 0 | 2910 | `		default:             zNative = "string";  iPdoType = PDO_PARAM_STR; break;` |
|    - | 2911 | `	}` |
|    5 | 2912 | `	PH7_MemObjRelease(pCell);` |
|    5 | 2913 | `	ph7_value_string(pCell,zNative,(int)SyStrlen(zNative));` |
|    5 | 2914 | `	ph7_array_add_strkey_elem(pMeta,"native_type",pCell);` |
|    5 | 2915 | `	ph7_value_int(pCell,iPdoType);` |
|    5 | 2916 | `	ph7_array_add_strkey_elem(pMeta,"pdo_type",pCell);` |
|    5 | 2917 | `	zDecl = PH7_PdoSqliteColumnDecl(pSt,(int)iCol);` |
|    5 | 2918 | `	if( zDecl ){` |
|    5 | 2919 | `		PH7_MemObjRelease(pCell);` |
|    5 | 2920 | `		ph7_value_string(pCell,zDecl,(int)SyStrlen(zDecl));` |
|    5 | 2921 | `		ph7_array_add_strkey_elem(pMeta,"sqlite:decl_type",pCell);` |
|    2 | 2922 | `	}` |
|    5 | 2923 | `	zTable = PH7_PdoSqliteColumnTable(pSt,(int)iCol);` |
|    5 | 2924 | `	if( zTable ){` |
|    5 | 2925 | `		PH7_MemObjRelease(pCell);` |
|    5 | 2926 | `		ph7_value_string(pCell,zTable,(int)SyStrlen(zTable));` |
|    5 | 2927 | `		ph7_array_add_strkey_elem(pMeta,"table",pCell);` |
|    2 | 2928 | `	}` |
|    5 | 2929 | `	ph7_array_add_strkey_elem(pMeta,"flags",pFlags);` |
|    5 | 2930 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    5 | 2931 | `	zName = PH7_PdoSqliteColumnName(pSt,(int)iCol);` |
|    5 | 2932 | `	PdoColumnName(pSt->pConn,zName,&sName);` |
|    5 | 2933 | `	PH7_MemObjRelease(pCell);` |
|    5 | 2934 | `	ph7_value_string(pCell,(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName) - 1);` |
|    5 | 2935 | `	ph7_array_add_strkey_elem(pMeta,"name",pCell);` |
|    5 | 2936 | `	SyBlobRelease(&sName);` |
|    5 | 2937 | `	ph7_value_int(pCell,-1);` |
|    5 | 2938 | `	ph7_array_add_strkey_elem(pMeta,"len",pCell);` |
|    5 | 2939 | `	ph7_value_int(pCell,0);` |
|    5 | 2940 | `	ph7_array_add_strkey_elem(pMeta,"precision",pCell);` |
|    5 | 2941 | `	ph7_result_value(pCtx,pMeta);` |
|    5 | 2942 | `	return PH7_OK;` |
|    5 | 2943 | `}` |
|    - | 2944 | `/*` |
|    - | 2945 | ` * PDOStatement::nextRowset(): bool` |
|    - | 2946 | ` *` |
|    - | 2947 | ` * sqlite has no second result set to move to, so this is the layer refusal --` |
|    - | 2948 | ` * false, and IM001 with php's own "driver does not support multiple rowsets".` |
|    - | 2949 | ` * It leaves the CURSOR alone: a fetch after it still answers the row that was` |
|    - | 2950 | ` * waiting.` |
|    - | 2951 | ` */` |
|    4 | 2952 | `static int vm_builtin_PDOStatement_nextRowset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2953 | `{` |
|    5 | 2954 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 2955 | `	SXUNUSED(nArg);` |
|    2 | 2956 | `	SXUNUSED(apArg);` |
|    5 | 2957 | `	if( pSt == 0 ){` |
|  ! 0 | 2958 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2959 | `	}` |
|    5 | 2960 | `	ph7_result_bool(pCtx,0);` |
|    5 | 2961 | `	PdoStmtFailed(pSt,"IM001");` |
|    5 | 2962 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::nextRowset","IM001",` |
|    - | 2963 | `		"driver does not support multiple rowsets");` |
|    3 | 2964 | `}` |
|    - | 2965 | `/*` |
|    - | 2966 | ` * PDOStatement::fetchColumn(int $column = 0): mixed` |
|    - | 2967 | ` *` |
|    - | 2968 | ` * One column of the next row, by position. An index outside the RESULT SET is` |
|    - | 2969 | ` * a ValueError rather than a null, and its two refusals are worded unlike` |
|    - | 2970 | ` * fetchAll()'s -- php's own inconsistency, reproduced.` |
|    - | 2971 | ` */` |
|   20 | 2972 | `static int vm_builtin_PDOStatement_fetchColumn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2973 | `{` |
|   21 | 2974 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2975 | `	ph7_int64 iCol;` |
|    - | 2976 | `	ph7_value *pRow,*pCell;` |
|   21 | 2977 | `	if( pSt == 0 ){` |
|  ! 0 | 2978 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2979 | `	}` |
|   21 | 2980 | `	iCol = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|   21 | 2981 | `	if( iCol < 0 ){` |
|    3 | 2982 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2983 | `			"Column index must be greater than or equal to 0");` |
|    - | 2984 | `	}` |
|   19 | 2985 | `	if( iCol >= (ph7_int64)PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 2986 | `		return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    - | 2987 | `	}` |
|   17 | 2988 | `	if( !PdoStmtHasRow(pSt) ){` |
|    3 | 2989 | `		PdoStmtOk(pSt);` |
|    3 | 2990 | `		ph7_result_bool(pCtx,0);` |
|    3 | 2991 | `		return PH7_OK;` |
|    - | 2992 | `	}` |
|   15 | 2993 | `	if( PdoBoundColumnsBad(pSt) ){` |
|  ! 0 | 2994 | `		return PdoBoundColumnsRefuse(pCtx,pSt,0);` |
|    - | 2995 | `	}` |
|   15 | 2996 | `	pRow = ph7_context_new_array(pCtx);` |
|   15 | 2997 | `	if( pRow == 0 ){` |
|  ! 0 | 2998 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2999 | `	}` |
|   15 | 3000 | `	if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pRow) ){` |
|  ! 0 | 3001 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 3002 | `		return PH7_OK;` |
|    - | 3003 | `	}` |
|   15 | 3004 | `	pCell = PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)iCol);` |
|   15 | 3005 | `	if( pCell ){` |
|   15 | 3006 | `		ph7_result_value(pCtx,pCell);` |
|    8 | 3007 | `	}else{` |
|  ! 0 | 3008 | `		ph7_result_null(pCtx);` |
|    - | 3009 | `	}` |
|   15 | 3010 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 3011 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 3012 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchColumn");` |
|    - | 3013 | `	}` |
|   15 | 3014 | `	PdoStmtOk(pSt);` |
|   15 | 3015 | `	return PH7_OK;` |
|   11 | 3016 | `}` |
|    - | 3017 | `/*` |
|    - | 3018 | `` * php's `pdo_stmt_setup_fetch_mode`: the mode a statement will use from here`` |
|    - | 3019 | ` * on, and the whole screen over it. Two verbs give one: setFetchMode()'s first` |
|    - | 3020 | ` * argument and query()'s SECOND, so every diagnostic counts arguments the way` |
|    - | 3021 | `` * the verb that took them does -- `iModeArg` is the mode's own 1-based`` |
|    - | 3022 | ` * position, and the counts php reports are that position plus what the mode` |
|    - | 3023 | ` * needs beside it.` |
|    - | 3024 | ` *` |
|    - | 3025 | ` * The rules are php's, per mode: FETCH_COLUMN wants a column NUMBER and` |
|    - | 3026 | ` * FETCH_INTO an OBJECT, both exactly one; FETCH_CLASS wants a class NAME and` |
|    - | 3027 | ` * accepts constructor arguments behind it -- unless FETCH_CLASSTYPE rides on` |
|    - | 3028 | ` * it, which takes the class from the first column and therefore wants nothing;` |
|    - | 3029 | ` * FETCH_FUNC belongs to fetchAll() alone; and every other mode takes the mode` |
|    - | 3030 | `` * and nothing else. A base outside php's own enum is `must be a bitmask of`` |
|    - | 3031 | `` * PDO::FETCH_* constants`. FETCH_DEFAULT itself names the connection's`` |
|    - | 3032 | ` * ATTR_DEFAULT_FETCH_MODE and leaves the statement on it.` |
|    - | 3033 | ` */` |
|  544 | 3034 | `static sxi32 PdoSetupFetchMode(ph7_context *pCtx,phl_pdo_stmt *pSt,int nArg,ph7_value **apArg,` |
|    - | 3035 | `	int iModeArg,const char *zFn,const char *zModeParam)` |
|    2 | 3036 | `{` |
|  546 | 3037 | `	ph7_value *pMode = nArg >= iModeArg ? apArg[iModeArg-1] : 0;` |
|  546 | 3038 | `	int iMode = pMode ? (int)ph7_value_to_int64(pMode) : PDO_FETCH_DEFAULT;` |
|  546 | 3039 | `	int iBase = iMode & PDO_FETCH_MODE_MASK;` |
|  546 | 3040 | `	int nExtra = nArg - iModeArg;          /* arguments given BEHIND the mode */` |
|    - | 3041 | `	char zBuf[64];` |
|    - | 3042 | `	sxi32 rc;` |
|    - | 3043 | `	/* php clears the statement's mode BEFORE it judges the new one, and clears` |
|    - | 3044 | `	 * it to the CONNECTION's default rather than to what the statement was` |
|    - | 3045 | `	 * carrying -- so a REFUSED setFetchMode() leaves a statement that was` |
|    - | 3046 | `	 * fetching NUM answering whatever ATTR_DEFAULT_FETCH_MODE says. */` |
|  546 | 3047 | `	PdoStmtClearFetchState(pSt);` |
|  546 | 3048 | `	pSt->iFetchMode = pSt->pConn->iDefaultFetch;` |
|  546 | 3049 | `	pSt->iFetchColumn = 0;` |
|  546 | 3050 | `	if( iBase > PDO_FETCH_KEY_PAIR ){` |
|   43 | 3051 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3052 | `			"%s(): Argument #%d ($%s) must be a bitmask of PDO::FETCH_* constants",` |
|   14 | 3053 | `			zFn,iModeArg,zModeParam);` |
|    - | 3054 | `	}` |
|  518 | 3055 | `	rc = PdoCheckFetchFlags(pCtx,iMode,zFn,iModeArg,zModeParam);` |
|  518 | 3056 | `	if( rc != PH7_OK ){` |
|    3 | 3057 | `		return rc;` |
|    - | 3058 | `	}` |
|  516 | 3059 | `	if( iBase == PDO_FETCH_FUNC ){` |
|   43 | 3060 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3061 | `			"%s(): Argument #%d ($%s) PDO::FETCH_FUNC can only be used with "` |
|   14 | 3062 | `			"PDOStatement::fetchAll()",zFn,iModeArg,zModeParam);` |
|    - | 3063 | `	}` |
|  488 | 3064 | `	if( iBase == PDO_FETCH_CLASS && (iMode & PDO_FETCH_CLASSTYPE) == 0 ){` |
|    - | 3065 | `		/* The class NAME, then optional constructor arguments. php checks the` |
|    - | 3066 | `		 * TYPE of what it was handed before it counts, so a wrong second` |
|    - | 3067 | `		 * argument is a TypeError even when a fourth is there too. */` |
|   47 | 3068 | `		if( nExtra < 1 ){` |
|    7 | 3069 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3070 | `				"%s() expects at least %d arguments for the fetch mode provided, %d given",` |
|    2 | 3071 | `				zFn,iModeArg+1,nArg);` |
|    - | 3072 | `		}` |
|   43 | 3073 | `		if( (apArg[iModeArg]->iFlags & MEMOBJ_STRING) == 0 ){` |
|   19 | 3074 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3075 | `				"%s(): Argument #%d must be of type string, %s given",` |
|   12 | 3076 | `				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));` |
|    - | 3077 | `		}` |
|    - | 3078 | `		{` |
|    - | 3079 | `			/* php resolves the name HERE -- before it looks at the constructor` |
|    - | 3080 | `			 * arguments behind it -- so a class that does not exist is refused` |
|    - | 3081 | `			 * where it was named rather than at the first fetch, and one that` |
|    - | 3082 | `			 * merely cannot be instantiated is accepted here and refused there. */` |
|   31 | 3083 | `			int nCls = 0;` |
|   31 | 3084 | `			const char *zCls = ph7_value_to_string(apArg[iModeArg],&nCls);` |
|   30 | 3085 | `			if( zCls == 0 \|\| nCls < 1` |
|   31 | 3086 | `			 \|\| PH7_VmExtractClass(pCtx->pVm,zCls,(sxu32)nCls,FALSE,0) == 0 ){` |
|    4 | 3087 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|    1 | 3088 | `					"%s(): Argument #%d must be a valid class",zFn,iModeArg+1);` |
|    - | 3089 | `			}` |
|    - | 3090 | `		}` |
|   29 | 3091 | `		if( nExtra > 1 && (apArg[iModeArg+1]->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL)) == 0 ){` |
|    7 | 3092 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3093 | `				"%s(): Argument #%d must be of type ?array, %s given",` |
|    4 | 3094 | `				zFn,iModeArg+2,VmValueGivenName(apArg[iModeArg+1],zBuf,sizeof(zBuf)));` |
|    - | 3095 | `		}` |
|   25 | 3096 | `		if( nExtra > 2 ){` |
|  ! 0 | 3097 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3098 | `				"%s() expects at most %d arguments for the fetch mode provided, %d given",` |
|  ! 0 | 3099 | `				zFn,iModeArg+2,nArg);` |
|    1 | 3100 | `		}` |
|  454 | 3101 | `	}else if( iBase == PDO_FETCH_COLUMN \|\| iBase == PDO_FETCH_INTO ){` |
|  105 | 3102 | `		if( nExtra != 1 ){` |
|   67 | 3103 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3104 | `				"%s() expects exactly %d arguments for the fetch mode provided, %d given",` |
|   22 | 3105 | `				zFn,iModeArg+1,nArg);` |
|    - | 3106 | `		}` |
|    - | 3107 | `		/* php's screen is the zval's TYPE: only a real int passes, and a float` |
|    - | 3108 | `		 * whose value happens to be integral does not (the slot may carry the` |
|    - | 3109 | `		 * int flag beside the real one once something has read it as a number,` |
|    - | 3110 | `		 * so the REAL bit is what decides). */` |
|   60 | 3111 | `		if( iBase == PDO_FETCH_COLUMN` |
|   52 | 3112 | `		 && ((apArg[iModeArg]->iFlags & MEMOBJ_INT) == 0` |
|   34 | 3113 | `		  \|\| (apArg[iModeArg]->iFlags & MEMOBJ_REAL) != 0) ){` |
|   28 | 3114 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3115 | `				"%s(): Argument #%d must be of type int, %s given",` |
|   18 | 3116 | `				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));` |
|    - | 3117 | `		}` |
|   43 | 3118 | `		if( iBase == PDO_FETCH_INTO && (apArg[iModeArg]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   13 | 3119 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 3120 | `				"%s(): Argument #%d must be of type object, %s given",` |
|    8 | 3121 | `				zFn,iModeArg+1,VmValueGivenName(apArg[iModeArg],zBuf,sizeof(zBuf)));` |
|    - | 3122 | `		}` |
|   35 | 3123 | `		if( iBase == PDO_FETCH_COLUMN && ph7_value_to_int64(apArg[iModeArg]) < 0 ){` |
|    - | 3124 | `			/* A NEGATIVE column is refused where it is given; one merely past` |
|    - | 3125 | `` 			 * the last column is not, and answers php's `Invalid column index` `` |
|    - | 3126 | `			 * at the fetch -- the statement's width is not this screen's` |
|    - | 3127 | `			 * business. */` |
|    4 | 3128 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    1 | 3129 | `				"%s(): Argument #%d must be greater than or equal to 0",zFn,iModeArg+1);` |
|    1 | 3130 | `		}` |
|  354 | 3131 | `	}else if( nExtra > 0 ){` |
|  361 | 3132 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3133 | `			"%s() expects exactly %d arguments for the fetch mode provided, %d given",` |
|  120 | 3134 | `			zFn,iModeArg,nArg);` |
|    - | 3135 | `	}` |
|  166 | 3136 | `	if( iBase == PDO_FETCH_CLASS && (iMode & PDO_FETCH_CLASSTYPE) == 0 ){` |
|   25 | 3137 | `		int nName = 0;` |
|   25 | 3138 | `		const char *zName = ph7_value_to_string(apArg[iModeArg],&nName);` |
|   25 | 3139 | `		if( zName && nName > 0 ){` |
|   49 | 3140 | `			pSt->zFetchClass = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|   24 | 3141 | `				(sxu32)nName + 1);` |
|   25 | 3142 | `			if( pSt->zFetchClass ){` |
|   25 | 3143 | `				SyMemcpy(zName,pSt->zFetchClass,(sxu32)nName);` |
|   25 | 3144 | `				pSt->zFetchClass[nName] = 0;` |
|   25 | 3145 | `				pSt->nFetchClass = nName;` |
|   12 | 3146 | `			}` |
|   12 | 3147 | `		}` |
|   25 | 3148 | `		if( nExtra > 1 && (apArg[iModeArg+1]->iFlags & MEMOBJ_HASHMAP) ){` |
|    9 | 3149 | `			pSt->pFetchArgs = ph7_new_array(pCtx->pVm);` |
|    9 | 3150 | `			if( pSt->pFetchArgs ){` |
|    9 | 3151 | `				PH7_MemObjStore(apArg[iModeArg+1],pSt->pFetchArgs);` |
|    4 | 3152 | `			}` |
|    5 | 3153 | `		}` |
|  142 | 3154 | `	}else if( iBase == PDO_FETCH_INTO ){` |
|   11 | 3155 | `		pSt->pFetchInto = (ph7_class_instance *)apArg[iModeArg]->x.pOther;` |
|   11 | 3156 | `		pSt->pFetchInto->iRef++;   /* the statement writes into it for as long as it lives */` |
|    5 | 3157 | `	}` |
|    - | 3158 | `	/* FETCH_DEFAULT is not a mode to keep: the statement stays on the` |
|    - | 3159 | `	 * connection's default it was just cleared to (php 8.5.11, GH-20214). */` |
|  154 | 3160 | `	if( iBase != PDO_FETCH_DEFAULT ){` |
|  142 | 3161 | `		pSt->iFetchMode = iMode;` |
|   70 | 3162 | `	}` |
|  154 | 3163 | `	pSt->iFetchColumn = iBase == PDO_FETCH_COLUMN` |
|   87 | 3164 | `		? (int)ph7_value_to_int64(apArg[iModeArg]) : 0;` |
|  154 | 3165 | `	return PH7_OK;` |
|  274 | 3166 | `}` |
|    - | 3167 | `/*` |
|    - | 3168 | ` * PDOStatement::setFetchMode(int $mode, mixed ...$args): true` |
|    - | 3169 | ` *` |
|    - | 3170 | ` * The mode a bare fetch()/fetchAll() will use from here on -- one spelling of` |
|    - | 3171 | ` * the screen above, the other being PDO::query()'s second argument.` |
|    - | 3172 | ` */` |
|  296 | 3173 | `static int vm_builtin_PDOStatement_setFetchMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3174 | `{` |
|  298 | 3175 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3176 | `	sxi32 rc;` |
|  298 | 3177 | `	if( pSt == 0 ){` |
|  ! 0 | 3178 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3179 | `	}` |
|  298 | 3180 | `	rc = PdoSetupFetchMode(pCtx,pSt,nArg,apArg,1,"PDOStatement::setFetchMode","mode");` |
|  298 | 3181 | `	if( rc != PH7_OK ){` |
|  199 | 3182 | `		return rc;` |
|    - | 3183 | `	}` |
|  100 | 3184 | `	ph7_result_bool(pCtx,1);` |
|  100 | 3185 | `	return PH7_OK;` |
|  150 | 3186 | `}` |
|    - | 3187 |  |
|    - | 3188 | `/* Add one row under a key, collecting repeats into a list (FETCH_GROUP). */` |
|   18 | 3189 | `static void PdoGroupAppend(ph7_context *pCtx,ph7_value *pOut,ph7_value *pKey,ph7_value *pRow)` |
|    1 | 3190 | `{` |
|   19 | 3191 | `	ph7_value *pList = 0;` |
|   19 | 3192 | `	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){` |
|  ! 0 | 3193 | `		int nKey = 0;` |
|  ! 0 | 3194 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|  ! 0 | 3195 | `		pList = ph7_array_fetch(pOut,zKey,nKey);` |
|   19 | 3196 | `	}else if( pKey ){` |
|    - | 3197 | `		SyBlob sKey;` |
|   19 | 3198 | `		SyBlobInit(&sKey,&pCtx->pVm->sAllocator);` |
|   19 | 3199 | `		SyBlobFormat(&sKey,"%qd",ph7_value_to_int64(pKey));` |
|   19 | 3200 | `		SyBlobAppend(&sKey,"",1);` |
|   28 | 3201 | `		pList = ph7_array_fetch(pOut,(const char *)SyBlobData(&sKey),` |
|   18 | 3202 | `			(int)SyBlobLength(&sKey) - 1);` |
|   19 | 3203 | `		SyBlobRelease(&sKey);` |
|    9 | 3204 | `	}` |
|   19 | 3205 | `	if( pList && (pList->iFlags & MEMOBJ_HASHMAP) ){` |
|    7 | 3206 | `		ph7_array_add_elem(pList,0,pRow);` |
|    7 | 3207 | `		return;` |
|    - | 3208 | `	}` |
|   13 | 3209 | `	pList = ph7_context_new_array(pCtx);` |
|   13 | 3210 | `	if( pList == 0 ){` |
|  ! 0 | 3211 | `		return;` |
|    - | 3212 | `	}` |
|   13 | 3213 | `	ph7_array_add_elem(pList,0,pRow);` |
|   13 | 3214 | `	ph7_array_add_elem(pOut,pKey,pList);` |
|   10 | 3215 | `}` |
|    - | 3216 | `/*` |
|    - | 3217 | ` * PDOStatement::fetchAll(int $mode = PDO::FETCH_DEFAULT, mixed ...$args): array` |
|    - | 3218 | ` *` |
|    - | 3219 | ` * Every remaining row in one array. Four of the modes change the shape of that` |
|    - | 3220 | ` * ARRAY rather than the shape of a row: FETCH_COLUMN reduces each row to one` |
|    - | 3221 | ` * value, FETCH_KEY_PAIR to a key and a value (and refuses a result set that is` |
|    - | 3222 | ` * not exactly two columns wide), FETCH_FUNC replaces it with whatever a` |
|    - | 3223 | ` * callable answers, and GROUP/UNIQUE take the first column as a key -- GROUP` |
|    - | 3224 | ` * collecting every row under it, UNIQUE keeping the last.` |
|    - | 3225 | ` *` |
|    - | 3226 | ` * php counts arguments per mode here too, and its FETCH_FUNC wording is` |
|    - | 3227 | ` * singular ("expects exactly 2 argument"); both are reproduced as they stand.` |
|    - | 3228 | ` */` |
|   94 | 3229 | `static int vm_builtin_PDOStatement_fetchAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3230 | `{` |
|   96 | 3231 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3232 | `	ph7_value *pOut,*pRow;` |
|   96 | 3233 | `	int iMode,iBase,iCol = 0;` |
|    - | 3234 | `	int bGroup,bUnique;` |
|    - | 3235 | `	sxi32 rc;` |
|   96 | 3236 | `	if( pSt == 0 ){` |
|  ! 0 | 3237 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3238 | `	}` |
|   96 | 3239 | `	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;` |
|   96 | 3240 | `	rc = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::fetchAll",1,"mode");` |
|   96 | 3241 | `	if( rc != PH7_OK ){` |
|    3 | 3242 | `		return rc;` |
|    - | 3243 | `	}` |
|   94 | 3244 | `	bGroup = (iMode & PDO_FETCH_GROUP) == PDO_FETCH_GROUP;` |
|   94 | 3245 | `	bUnique = (iMode & PDO_FETCH_UNIQUE) == PDO_FETCH_UNIQUE;` |
|   94 | 3246 | `	iBase = iMode & PDO_FETCH_MODE_MASK;` |
|   94 | 3247 | `	if( iBase == PDO_FETCH_DEFAULT ){` |
|   19 | 3248 | `		iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;` |
|   19 | 3249 | `		bGroup = bGroup \|\| (pSt->iFetchMode & PDO_FETCH_GROUP) == PDO_FETCH_GROUP;` |
|   19 | 3250 | `		bUnique = bUnique \|\| (pSt->iFetchMode & PDO_FETCH_UNIQUE) == PDO_FETCH_UNIQUE;` |
|   19 | 3251 | `		iCol = pSt->iFetchColumn;` |
|    9 | 3252 | `	}` |
|   94 | 3253 | `	if( iBase == PDO_FETCH_DEFAULT ){` |
|  ! 0 | 3254 | `		iBase = pSt->pConn->iDefaultFetch;` |
|  ! 0 | 3255 | `	}` |
|   94 | 3256 | `	if( iBase == PDO_FETCH_COLUMN ){` |
|   17 | 3257 | `		if( nArg > 2 ){` |
|  ! 0 | 3258 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3259 | `				"PDOStatement::fetchAll() expects exactly 2 arguments for the fetch "` |
|  ! 0 | 3260 | `				"mode provided, %d given",nArg);` |
|    - | 3261 | `		}` |
|   17 | 3262 | `		if( nArg > 1 ){` |
|    7 | 3263 | `			ph7_int64 iWant = ph7_value_to_int64(apArg[1]);` |
|    7 | 3264 | `			if( iWant < 0 ){` |
|    3 | 3265 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3266 | `					"PDOStatement::fetchAll(): Argument #2 must be greater than or "` |
|    - | 3267 | `					"equal to 0");` |
|    - | 3268 | `			}` |
|    5 | 3269 | `			iCol = (int)iWant;` |
|    2 | 3270 | `		}` |
|   15 | 3271 | `		if( iCol >= PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 3272 | `			return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    1 | 3273 | `		}` |
|   84 | 3274 | `	}else if( iBase == PDO_FETCH_CLASS ){` |
|   21 | 3275 | `		if( nArg > 3 ){` |
|  ! 0 | 3276 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3277 | `				"PDOStatement::fetchAll() expects at most 3 arguments for the fetch "` |
|  ! 0 | 3278 | `				"mode provided, %d given",nArg);` |
|    1 | 3279 | `		}` |
|   68 | 3280 | `	}else if( iBase == PDO_FETCH_LAZY ){` |
|    5 | 3281 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 3282 | `			"PDOStatement::fetchAll(): Argument #1 ($mode) PDO::FETCH_LAZY cannot be "` |
|    - | 3283 | `			"used with PDOStatement::fetchAll()");` |
|   54 | 3284 | `	}else if( iBase == PDO_FETCH_FUNC ){` |
|    7 | 3285 | `		if( nArg != 2 ){` |
|    4 | 3286 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3287 | `				"PDOStatement::fetchAll() expects exactly 2 argument for "` |
|    1 | 3288 | `				"PDO::FETCH_FUNC, %d given",nArg);` |
|    - | 3289 | `		}` |
|    5 | 3290 | `		if( !ph7_value_is_callable(apArg[1]) ){` |
|    - | 3291 | `			/* php checks the callable BEFORE the first row, so an unusable one` |
|    - | 3292 | `			 * is a TypeError from PDO and never the engine's own` |
|    - | 3293 | `			 * "Call to undefined function" from inside the walk */` |
|    3 | 3294 | `			int nName = 0;` |
|    3 | 3295 | `			const char *zName = ph7_value_to_string(apArg[1],&nName);` |
|    4 | 3296 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    1 | 3297 | `				"function \"%.*s\" not found or invalid function name",nName,zName);` |
|    1 | 3298 | `		}` |
|   49 | 3299 | `	}else if( nArg > 1 ){` |
|    4 | 3300 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 3301 | `			"PDOStatement::fetchAll() expects exactly 1 argument for the fetch mode "` |
|    1 | 3302 | `			"provided, %d given",nArg);` |
|    - | 3303 | `	}` |
|   80 | 3304 | `	if( iBase == PDO_FETCH_KEY_PAIR && PH7_PdoSqliteColumnCount(pSt) != 2 ){` |
|    5 | 3305 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 3306 | `			"SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires "` |
|    - | 3307 | `			"the result set to contain exactly 2 columns.");` |
|    - | 3308 | `	}` |
|   76 | 3309 | `	pOut = ph7_context_new_array(pCtx);` |
|   76 | 3310 | `	if( pOut == 0 ){` |
|  ! 0 | 3311 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3312 | `	}` |
|   76 | 3313 | `	if( PdoBoundColumnsBad(pSt) ){` |
|    3 | 3314 | `		return PdoBoundColumnsRefuse(pCtx,pSt,1);` |
|    - | 3315 | `	}` |
|  248 | 3316 | `	while( PdoStmtHasRow(pSt) ){` |
|  178 | 3317 | `		int iRowMode = iBase;` |
|  178 | 3318 | `		int iFirst = (bGroup \|\| bUnique) ? 1 : 0;` |
|  176 | 3319 | `		if( iBase == PDO_FETCH_COLUMN \|\| iBase == PDO_FETCH_KEY_PAIR` |
|  137 | 3320 | `		 \|\| iBase == PDO_FETCH_FUNC \|\| iBase == PDO_FETCH_BOUND ){` |
|   64 | 3321 | `			iRowMode = PDO_FETCH_NUM;` |
|   64 | 3322 | `			iFirst = 0;` |
|  146 | 3323 | `		}else if( iBase == PDO_FETCH_CLASS ){` |
|   43 | 3324 | `			iFirst = 0;` |
|   21 | 3325 | `		}` |
|  178 | 3326 | `		pRow = ph7_context_new_array(pCtx);` |
|  178 | 3327 | `		if( pRow == 0 ){` |
|  ! 0 | 3328 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 3329 | `		}` |
|  178 | 3330 | `		if( iFirst ){` |
|    - | 3331 | `			/* the FIRST column is the key and never joins the row */` |
|   31 | 3332 | `			ph7_value *pKeyRow = ph7_context_new_array(pCtx);` |
|    - | 3333 | `			ph7_value *pKey;` |
|   31 | 3334 | `			if( pKeyRow == 0 ){` |
|  ! 0 | 3335 | `				return PH7_ContextMemoryError(pCtx);` |
|    - | 3336 | `			}` |
|   31 | 3337 | `			if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pKeyRow,0) ){` |
|  ! 0 | 3338 | `				break;` |
|    - | 3339 | `			}` |
|   31 | 3340 | `			pKey = PdoArrayAtInt(pCtx->pVm,pKeyRow,0);` |
|    - | 3341 | `			/* the row itself is rebuilt from the SECOND column on; the cursor` |
|    - | 3342 | `			 * has not moved, so this reads the same sqlite row again */` |
|   31 | 3343 | `			pSt->bRowPending = 1;` |
|   31 | 3344 | `			if( !PdoStmtRowFrom(pCtx->pVm,pSt,iRowMode,pRow,1) ){` |
|  ! 0 | 3345 | `				break;` |
|    - | 3346 | `			}` |
|   31 | 3347 | `			if( bUnique ){` |
|   13 | 3348 | `				ph7_array_add_elem(pOut,pKey,pRow);` |
|    7 | 3349 | `			}else{` |
|   19 | 3350 | `				PdoGroupAppend(pCtx,pOut,pKey,pRow);` |
|    1 | 3351 | `			}` |
|  163 | 3352 | `		}else if( iBase == PDO_FETCH_CLASS ){` |
|    - | 3353 | `			/* every row is its own instance; the class and its constructor` |
|    - | 3354 | `			 * arguments are the same for all of them */` |
|    - | 3355 | `			ph7_class *pClass;` |
|    - | 3356 | `			ph7_value sObj;` |
|    - | 3357 | `			sxi32 rcCls;` |
|   43 | 3358 | `			int iFirstCol = 0;` |
|   43 | 3359 | `			if( nArg > 1 ){` |
|   27 | 3360 | `				pClass = PdoResolveFetchClass(pCtx,apArg[1],FALSE,&rcCls);` |
|   30 | 3361 | `			}else if( iMode & PDO_FETCH_CLASSTYPE ){` |
|   13 | 3362 | `				ph7_value *pHead = ph7_context_new_array(pCtx);` |
|   13 | 3363 | `				if( pHead == 0 ){` |
|  ! 0 | 3364 | `					return PH7_ContextMemoryError(pCtx);` |
|    - | 3365 | `				}` |
|   13 | 3366 | `				if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pHead,0) ){` |
|  ! 0 | 3367 | `					break;` |
|    - | 3368 | `				}` |
|   13 | 3369 | `				pSt->bRowPending = 1;` |
|   13 | 3370 | `				pClass = PdoClassTypeClass(pCtx,PdoArrayAtInt(pCtx->pVm,pHead,0));` |
|   13 | 3371 | `				rcCls = PH7_OK;` |
|   13 | 3372 | `				iFirstCol = 1;` |
|   11 | 3373 | `			}else if( pSt->zFetchClass ){` |
|    - | 3374 | `				ph7_value sName;` |
|    - | 3375 | `				SyString sStr;` |
|  ! 0 | 3376 | `				SyStringInitFromBuf(&sStr,pSt->zFetchClass,pSt->nFetchClass);` |
|  ! 0 | 3377 | `				PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|  ! 0 | 3378 | `				pClass = PdoResolveFetchClass(pCtx,&sName,FALSE,&rcCls);` |
|  ! 0 | 3379 | `				PH7_MemObjRelease(&sName);` |
|  ! 0 | 3380 | `			}else{` |
|    5 | 3381 | `				pClass = PH7_VmExtractClass(pCtx->pVm,"stdClass",sizeof("stdClass")-1,` |
|    - | 3382 | `					FALSE,0);` |
|    5 | 3383 | `				rcCls = PH7_OK;` |
|    - | 3384 | `			}` |
|   43 | 3385 | `			if( pClass == 0 ){` |
|    3 | 3386 | `				return rcCls;` |
|    - | 3387 | `			}` |
|   41 | 3388 | `			rcCls = PH7_VmCheckInstantiable(pCtx,pClass);` |
|   41 | 3389 | `			if( rcCls != PH7_OK ){` |
|  ! 0 | 3390 | `				return rcCls;` |
|    - | 3391 | `			}` |
|   41 | 3392 | `			PH7_MemObjInit(pCtx->pVm,&sObj);` |
|   41 | 3393 | `			if( !PdoRowIntoObject(pCtx->pVm,pSt,pClass,nArg > 2 ? apArg[2] : 0,` |
|   40 | 3394 | `				(iMode & PDO_FETCH_PROPS_LATE) != 0,iFirstCol,&sObj) ){` |
|  ! 0 | 3395 | `				PH7_MemObjRelease(&sObj);` |
|  ! 0 | 3396 | `				break;` |
|    - | 3397 | `			}` |
|   41 | 3398 | `			ph7_array_add_elem(pOut,0,&sObj);` |
|   41 | 3399 | `			PH7_MemObjRelease(&sObj);` |
|  126 | 3400 | `		}else if( iBase == PDO_FETCH_BOUND ){` |
|    - | 3401 | `			/* the row goes into the BOUND VARIABLES, not into the result: the` |
|    - | 3402 | `			 * array collects one true per row and the caller reads the last` |
|    - | 3403 | `			 * row's values out of its own variables */` |
|    - | 3404 | `			ph7_value *pTrue;` |
|   11 | 3405 | `			PdoBoundColumnsForRow(pCtx->pVm,pSt);` |
|   11 | 3406 | `			pSt->bRowPending = 0;` |
|   11 | 3407 | `			pTrue = ph7_context_new_scalar(pCtx);` |
|   11 | 3408 | `			if( pTrue ){` |
|   11 | 3409 | `				ph7_value_bool(pTrue,1);` |
|   11 | 3410 | `				ph7_array_add_elem(pOut,0,pTrue);` |
|    6 | 3411 | `			}` |
|  101 | 3412 | `		}else if( !PdoStmtRow(pCtx->pVm,pSt,iRowMode,pRow) ){` |
|  ! 0 | 3413 | `			break;` |
|   96 | 3414 | `		}else if( iBase == PDO_FETCH_COLUMN ){` |
|   37 | 3415 | `			ph7_array_add_elem(pOut,0,PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)iCol));` |
|   78 | 3416 | `		}else if( iBase == PDO_FETCH_KEY_PAIR ){` |
|   17 | 3417 | `			ph7_array_add_elem(pOut,PdoArrayAtInt(pCtx->pVm,pRow,0),` |
|    5 | 3418 | `				PdoArrayAtInt(pCtx->pVm,pRow,1));` |
|   54 | 3419 | `		}else if( iBase == PDO_FETCH_FUNC ){` |
|    - | 3420 | `			ph7_value sRes;` |
|    - | 3421 | `			ph7_value *apCall[32];` |
|    7 | 3422 | `			int n,nCall = PH7_PdoSqliteColumnCount(pSt);` |
|    7 | 3423 | `			if( nCall > (int)SX_ARRAYSIZE(apCall) ){` |
|  ! 0 | 3424 | `				nCall = (int)SX_ARRAYSIZE(apCall);` |
|  ! 0 | 3425 | `			}` |
|   19 | 3426 | `			for( n = 0 ; n < nCall ; ++n ){` |
|   13 | 3427 | `				apCall[n] = PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)n);` |
|    7 | 3428 | `			}` |
|    7 | 3429 | `			PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    7 | 3430 | `			if( PH7_VmCallUserFunction(pCtx->pVm,apArg[1],nCall,apCall,&sRes) != SXRET_OK ){` |
|  ! 0 | 3431 | `				PH7_MemObjRelease(&sRes);` |
|  ! 0 | 3432 | `				return PH7_OK;   /* whatever the callable raised is already in flight */` |
|    - | 3433 | `			}` |
|    7 | 3434 | `			ph7_array_add_elem(pOut,0,&sRes);` |
|    7 | 3435 | `			PH7_MemObjRelease(&sRes);` |
|    4 | 3436 | `		}else{` |
|   43 | 3437 | `			ph7_array_add_elem(pOut,0,pRow);` |
|    - | 3438 | `		}` |
|  176 | 3439 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 3440 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 3441 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchAll");` |
|    - | 3442 | `		}` |
|    2 | 3443 | `	}` |
|   72 | 3444 | `	PdoStmtOk(pSt);` |
|   72 | 3445 | `	ph7_result_value(pCtx,pOut);` |
|   72 | 3446 | `	return PH7_OK;` |
|   49 | 3447 | `}` |
|    - | 3448 | `/*` |
|    - | 3449 | ` * PDOStatement::columnCount(): int` |
|    - | 3450 | ` *` |
|    - | 3451 | ` * 0 for a statement that returns no rows -- and also for one that has been` |
|    - | 3452 | ` * PREPARED but not yet run, even though sqlite already knows the count from` |
|    - | 3453 | ` * the compile. php only publishes it once the statement has executed, so` |
|    - | 3454 | `` * `prepare('SELECT 1')->columnCount()` is 0 and not 1.`` |
|    - | 3455 | ` */` |
|    4 | 3456 | `static int vm_builtin_PDOStatement_columnCount(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3457 | `{` |
|    5 | 3458 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 3459 | `	SXUNUSED(nArg);` |
|    2 | 3460 | `	SXUNUSED(apArg);` |
|    5 | 3461 | `	if( pSt == 0 ){` |
|  ! 0 | 3462 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3463 | `	}` |
|    5 | 3464 | `	ph7_result_int(pCtx,pSt->bExecuted ? PH7_PdoSqliteColumnCount(pSt) : 0);` |
|    5 | 3465 | `	return PH7_OK;` |
|    3 | 3466 | `}` |
|    - | 3467 | `/*` |
|    - | 3468 | ` * PDOStatement::debugDumpParams(): ?bool` |
|    - | 3469 | ` *` |
|    - | 3470 | ` * php's own diagnostic dump, printed rather than returned (it answers null).` |
|    - | 3471 | ` * The bindings appear in the order they were MADE, and the two kinds report` |
|    - | 3472 | ` * differently: a positional one carries its 0-based paramno and an empty name,` |
|    - | 3473 | ` * a named one carries paramno -1 and the name WITH its colon. Both lengths are` |
|    - | 3474 | `` * printed in brackets, php's `[%d]` shape.`` |
|    - | 3475 | ` */` |
|    4 | 3476 | `static int vm_builtin_PDOStatement_debugDumpParams(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3477 | `{` |
|    5 | 3478 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3479 | `	phl_pdo_bind *pB;` |
|    5 | 3480 | `	const char *zSql = "";` |
|    5 | 3481 | `	int nSql = 0,nBind = 0;` |
|    - | 3482 | `	ph7_value *pQuery;` |
|    2 | 3483 | `	SXUNUSED(nArg);` |
|    2 | 3484 | `	SXUNUSED(apArg);` |
|    5 | 3485 | `	if( pSt == 0 ){` |
|  ! 0 | 3486 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3487 | `	}` |
|    5 | 3488 | `	pQuery = pSt->pOwner ? PH7_NativeAttr(pSt->pOwner,"queryString") : 0;` |
|    5 | 3489 | `	if( pQuery ){` |
|    5 | 3490 | `		zSql = ph7_value_to_string(pQuery,&nSql);` |
|    2 | 3491 | `	}` |
|    9 | 3492 | `	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){` |
|    5 | 3493 | `		++nBind;` |
|    3 | 3494 | `	}` |
|    5 | 3495 | `	ph7_context_output_format(pCtx,"SQL: [%d] %.*s\n",nSql,nSql,zSql);` |
|    5 | 3496 | `	ph7_context_output_format(pCtx,"Params:  %d\n",nBind);` |
|    - | 3497 | `	/* the list is built by prepending, so walking it backwards is what puts` |
|    - | 3498 | `	 * the bindings back in the order the script made them */` |
|    - | 3499 | `	{` |
|    - | 3500 | `		phl_pdo_bind *apBind[64];` |
|    5 | 3501 | `		int n = 0,i;` |
|    9 | 3502 | `		for( pB = pSt->pBinds ; pB && n < (int)SX_ARRAYSIZE(apBind) ; pB = pB->pNext ){` |
|    5 | 3503 | `			apBind[n++] = pB;` |
|    3 | 3504 | `		}` |
|    9 | 3505 | `		for( i = n - 1 ; i >= 0 ; --i ){` |
|    5 | 3506 | `			pB = apBind[i];` |
|    5 | 3507 | `			if( pB->zName ){` |
|    4 | 3508 | `				ph7_context_output_format(pCtx,"Key: Name: [%d] %.*s\n",` |
|    1 | 3509 | `					pB->nName,pB->nName,pB->zName);` |
|    3 | 3510 | `				ph7_context_output_format(pCtx,"paramno=-1\n");` |
|    4 | 3511 | `				ph7_context_output_format(pCtx,"name=[%d] \"%.*s\"\n",` |
|    1 | 3512 | `					pB->nName,pB->nName,pB->zName);` |
|    2 | 3513 | `			}else{` |
|    3 | 3514 | `				ph7_context_output_format(pCtx,"Key: Position #%d:\n",pB->iPos - 1);` |
|    3 | 3515 | `				ph7_context_output_format(pCtx,"paramno=%d\n",pB->iPos - 1);` |
|    3 | 3516 | `				ph7_context_output_format(pCtx,"name=[0] \"\"\n");` |
|    - | 3517 | `			}` |
|    5 | 3518 | `			ph7_context_output_format(pCtx,"is_param=1\n");` |
|    5 | 3519 | `			ph7_context_output_format(pCtx,"param_type=%d\n",pB->iType & ~PDO_PARAM_FLAGS);` |
|    3 | 3520 | `		}` |
|    - | 3521 | `	}` |
|    5 | 3522 | `	ph7_result_null(pCtx);` |
|    5 | 3523 | `	return PH7_OK;` |
|    3 | 3524 | `}` |
|    - | 3525 | `/*` |
|    - | 3526 | ` * PDOStatement::getAttribute(int $name): mixed` |
|    - | 3527 | ` *` |
|    - | 3528 | ` * Two of the driver's attributes describe a STATEMENT rather than the` |
|    - | 3529 | ` * connection -- whether it only reads, and whether it is mid-walk -- and both` |
|    - | 3530 | ` * are sqlite's own answers about the compiled statement. Everything else is` |
|    - | 3531 | ` * the same IM001 refusal the connection gives.` |
|    - | 3532 | ` */` |
|    4 | 3533 | `static int vm_builtin_PDOStatement_getAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3534 | `{` |
|    5 | 3535 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3536 | `	ph7_int64 iAttr;` |
|    5 | 3537 | `	if( pSt == 0 ){` |
|  ! 0 | 3538 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3539 | `	}` |
|    5 | 3540 | `	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    5 | 3541 | `	if( iAttr == PDO_SQLITE_ATTR_READONLY_STATEMENT ){` |
|    3 | 3542 | `		ph7_result_bool(pCtx,PH7_PdoSqliteStmtReadonly(pSt));` |
|    3 | 3543 | `		return PH7_OK;` |
|    - | 3544 | `	}` |
|    3 | 3545 | `	if( iAttr == PDO_SQLITE_ATTR_BUSY_STATEMENT ){` |
|    3 | 3546 | `		ph7_result_bool(pCtx,PH7_PdoSqliteStmtBusy(pSt));` |
|    3 | 3547 | `		return PH7_OK;` |
|    - | 3548 | `	}` |
|  ! 0 | 3549 | `	ph7_result_null(pCtx);` |
|  ! 0 | 3550 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::getAttribute","IM001",` |
|    - | 3551 | `		"driver does not support that attribute");` |
|    3 | 3552 | `}` |
|    - | 3553 | `/*` |
|    - | 3554 | ` * PDOStatement::setAttribute(int $attribute, mixed $value): bool` |
|    - | 3555 | ` *` |
|    - | 3556 | ` * This driver carries no SETTABLE statement attribute at all, so every one of` |
|    - | 3557 | ` * them is the same IM001 refusal.` |
|    - | 3558 | ` */` |
|  ! 0 | 3559 | `static int vm_builtin_PDOStatement_setAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 3560 | `{` |
|  ! 0 | 3561 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|  ! 0 | 3562 | `	SXUNUSED(nArg);` |
|  ! 0 | 3563 | `	SXUNUSED(apArg);` |
|  ! 0 | 3564 | `	if( pSt == 0 ){` |
|  ! 0 | 3565 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3566 | `	}` |
|  ! 0 | 3567 | `	ph7_result_bool(pCtx,0);` |
|  ! 0 | 3568 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::setAttribute","IM001",` |
|    - | 3569 | `		"driver does not support that attribute");` |
|  ! 0 | 3570 | `}` |
|    - | 3571 | `/*` |
|    - | 3572 | ` * PDOStatement::rowCount(): int` |
|    - | 3573 | ` *` |
|    - | 3574 | ` * The number of rows a WRITE changed. It is not the size of a result set --` |
|    - | 3575 | ` * sqlite cannot know that without walking it -- so a SELECT answers 0, which` |
|    - | 3576 | ` * is php's answer and the reason its manual warns against this method.` |
|    - | 3577 | ` */` |
|    6 | 3578 | `static int vm_builtin_PDOStatement_rowCount(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3579 | `{` |
|    7 | 3580 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    3 | 3581 | `	SXUNUSED(nArg);` |
|    3 | 3582 | `	SXUNUSED(apArg);` |
|    7 | 3583 | `	if( pSt == 0 ){` |
|  ! 0 | 3584 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3585 | `	}` |
|    7 | 3586 | `	ph7_result_int64(pCtx,pSt->nChanges);` |
|    7 | 3587 | `	return PH7_OK;` |
|    4 | 3588 | `}` |
|    - | 3589 | `/*` |
|    - | 3590 | ` * PDOStatement::closeCursor(): bool` |
|    - | 3591 | ` *` |
|    - | 3592 | ` * Frees the rows a statement is still holding without discarding the statement` |
|    - | 3593 | ` * itself: php answers true and leaves the object reusable, and a fetch after` |
|    - | 3594 | ` * it answers false.` |
|    - | 3595 | ` */` |
|    4 | 3596 | `static int vm_builtin_PDOStatement_closeCursor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3597 | `{` |
|    5 | 3598 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 3599 | `	SXUNUSED(nArg);` |
|    2 | 3600 | `	SXUNUSED(apArg);` |
|    5 | 3601 | `	if( pSt == 0 ){` |
|  ! 0 | 3602 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3603 | `	}` |
|    5 | 3604 | `	if( pSt->pStmt ){` |
|    5 | 3605 | `		sqlite3_reset(pSt->pStmt);` |
|    2 | 3606 | `	}` |
|    5 | 3607 | `	pSt->bRowPending = 0;` |
|    5 | 3608 | `	pSt->bDone = 1;` |
|    - | 3609 | `	/* php frees the row's columns with the cursor, so a lazy object still in a` |
|    - | 3610 | `	 * variable answers null from here on. */` |
|    5 | 3611 | `	PdoStmtLazyClear(pSt);` |
|    5 | 3612 | `	ph7_result_bool(pCtx,1);` |
|    5 | 3613 | `	return PH7_OK;` |
|    3 | 3614 | `}` |
|    - | 3615 | `/* A statement reports its CONNECTION's error state; php keeps one per` |
|    - | 3616 | ` * statement, and every path that sets one sets both. */` |
|    2 | 3617 | `static int vm_builtin_PDOStatement_errorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3618 | `{` |
|    3 | 3619 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    1 | 3620 | `	SXUNUSED(nArg);` |
|    1 | 3621 | `	SXUNUSED(apArg);` |
|    3 | 3622 | `	if( pSt == 0 ){` |
|  ! 0 | 3623 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3624 | `	}` |
|    3 | 3625 | `	if( pSt->iErrState == PDO_ERR_NONE ){` |
|  ! 0 | 3626 | `		ph7_result_null(pCtx);` |
|  ! 0 | 3627 | `	}else{` |
|    3 | 3628 | `		ph7_result_string(pCtx,pSt->zSqlState,(int)SyStrlen(pSt->zSqlState));` |
|    - | 3629 | `	}` |
|    3 | 3630 | `	return PH7_OK;` |
|    2 | 3631 | `}` |
|    2 | 3632 | `static int vm_builtin_PDOStatement_errorInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3633 | `{` |
|    3 | 3634 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    1 | 3635 | `	SXUNUSED(nArg);` |
|    1 | 3636 | `	SXUNUSED(apArg);` |
|    3 | 3637 | `	if( pSt == 0 ){` |
|  ! 0 | 3638 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3639 | `	}` |
|    3 | 3640 | `	return PdoBuildErrorInfo(pCtx,pSt->pConn,pSt->iErrState,pSt->zSqlState);` |
|    2 | 3641 | `}` |
|    - | 3642 | `/*` |
|    - | 3643 | ` * The InternalIterator a foreach over a statement walks.  A statement is a` |
|    - | 3644 | ` * forward cursor, so REWIND does not rewind: it settles on whatever row is` |
|    - | 3645 | ` * pending, which is why a second foreach over the same statement walks nothing` |
|    - | 3646 | ` * at all rather than repeating the set.` |
|    - | 3647 | ` */` |
|  120 | 3648 | `static void PdoStmtIterSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 3649 | `{` |
|  121 | 3650 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|  121 | 3651 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(pSrc);` |
|    - | 3652 | `	ph7_value *pRow;` |
|  121 | 3653 | `	if( pSt == 0 \|\| !PdoStmtHasRow(pSt) ){` |
|   49 | 3654 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|   49 | 3655 | `		return;` |
|    - | 3656 | `	}` |
|    - | 3657 | `	{` |
|    - | 3658 | `		/* A foreach honours the statement's mode, the whole of it: php walks a` |
|    - | 3659 | `		 * LAZY statement with its one row object, a CLASS or INTO one with the` |
|    - | 3660 | `		 * objects those modes build, a COLUMN one with that column's value and` |
|    - | 3661 | ``		 * a BOUND one with `true` per row (the values having gone to the bound`` |
|    - | 3662 | `		 * variables). Only the four row SHAPES are what PdoStmtRow answers. */` |
|   73 | 3663 | `		int iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;` |
|    - | 3664 | `		ph7_value sCur;` |
|   73 | 3665 | `		int bHave = 0;` |
|   73 | 3666 | `		PH7_MemObjInit(pVm,&sCur);` |
|   73 | 3667 | `		if( iBase == PDO_FETCH_LAZY ){` |
|   15 | 3668 | `			ph7_class_instance *pLazy = PdoLazyRowFor(pVm,pSt);` |
|   15 | 3669 | `			if( pLazy ){` |
|   15 | 3670 | `				sCur.x.pOther = pLazy;` |
|   15 | 3671 | `				MemObjSetType(&sCur,MEMOBJ_OBJ);` |
|   15 | 3672 | `				bHave = 1;   /* the reference PdoLazyRowFor took is this value's */` |
|    8 | 3673 | `			}` |
|   66 | 3674 | `		}else if( iBase == PDO_FETCH_BOUND ){` |
|    - | 3675 | `			/* the row IS the bound variables: nothing else reads it, so the` |
|    - | 3676 | `			 * write happens here rather than inside a row build */` |
|    5 | 3677 | `			PdoBoundColumnsForRow(pVm,pSt);` |
|    5 | 3678 | `			pSt->bRowPending = 0;` |
|    5 | 3679 | `			ph7_value_bool(&sCur,1);` |
|    5 | 3680 | `			bHave = 1;` |
|   57 | 3681 | `		}else if( iBase == PDO_FETCH_COLUMN ){` |
|    5 | 3682 | `			ph7_value *pNumRow = ph7_new_array(pVm);` |
|    5 | 3683 | `			if( pNumRow ){` |
|    5 | 3684 | `				if( PdoStmtRowFrom(pVm,pSt,PDO_FETCH_NUM,pNumRow,0) ){` |
|    5 | 3685 | `					ph7_value *pOne = PdoArrayAtInt(pVm,pNumRow,(sxi64)pSt->iFetchColumn);` |
|    5 | 3686 | `					if( pOne ){` |
|    5 | 3687 | `						PH7_MemObjStore(pOne,&sCur);` |
|    2 | 3688 | `					}` |
|    5 | 3689 | `					bHave = 1;` |
|    2 | 3690 | `				}` |
|    5 | 3691 | `				ph7_release_value(pVm,pNumRow);` |
|    3 | 3692 | `			}` |
|   53 | 3693 | `		}else if( iBase == PDO_FETCH_CLASS \|\| iBase == PDO_FETCH_INTO ){` |
|   17 | 3694 | `			ph7_class *pClass = 0;` |
|   17 | 3695 | `			int iFirst = 0;` |
|   17 | 3696 | `			if( iBase == PDO_FETCH_INTO ){` |
|    5 | 3697 | `				if( pSt->pFetchInto ){` |
|    5 | 3698 | `					ph7_value *pRowVals = ph7_new_array(pVm);` |
|    5 | 3699 | `					if( pRowVals && PdoStmtRowFrom(pVm,pSt,PDO_FETCH_ASSOC,pRowVals,0) ){` |
|    5 | 3700 | `						PdoWriteRowProps(pVm,pSt->pFetchInto,pRowVals);` |
|    5 | 3701 | `						pSt->pFetchInto->iRef++;` |
|    5 | 3702 | `						sCur.x.pOther = pSt->pFetchInto;` |
|    5 | 3703 | `						MemObjSetType(&sCur,MEMOBJ_OBJ);` |
|    5 | 3704 | `						bHave = 1;` |
|    2 | 3705 | `					}` |
|    5 | 3706 | `					if( pRowVals ){` |
|    5 | 3707 | `						ph7_release_value(pVm,pRowVals);` |
|    2 | 3708 | `					}` |
|    2 | 3709 | `				}` |
|    3 | 3710 | `			}else{` |
|   13 | 3711 | `				if( pSt->iFetchMode & PDO_FETCH_CLASSTYPE ){` |
|    - | 3712 | `					/* the FIRST column names the class and leaves the row */` |
|    5 | 3713 | `					ph7_value *pHead = ph7_new_array(pVm);` |
|    5 | 3714 | `					if( pHead && PdoStmtRowFrom(pVm,pSt,PDO_FETCH_NUM,pHead,0) ){` |
|    5 | 3715 | `						pSt->bRowPending = 1;   /* the cursor has not moved */` |
|    5 | 3716 | `						pClass = PdoIterClassOf(pVm,PdoArrayAtInt(pVm,pHead,0));` |
|    5 | 3717 | `						iFirst = 1;` |
|    2 | 3718 | `					}` |
|    5 | 3719 | `					if( pHead ){` |
|    5 | 3720 | `						ph7_release_value(pVm,pHead);` |
|    3 | 3721 | `					}` |
|   11 | 3722 | `				}else if( pSt->zFetchClass ){` |
|   13 | 3723 | `					pClass = PH7_VmExtractClass(pVm,pSt->zFetchClass,` |
|    8 | 3724 | `						(sxu32)pSt->nFetchClass,FALSE,0);` |
|    4 | 3725 | `				}` |
|   19 | 3726 | `				if( pClass && PdoRowIntoObject(pVm,pSt,pClass,pSt->pFetchArgs,` |
|   12 | 3727 | `						(pSt->iFetchMode & PDO_FETCH_PROPS_LATE) != 0,iFirst,&sCur) ){` |
|   13 | 3728 | `					bHave = 1;` |
|    6 | 3729 | `				}` |
|    - | 3730 | `			}` |
|    8 | 3731 | `		}` |
|   73 | 3732 | `		if( bHave ){` |
|   58 | 3733 | `			PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,` |
|   19 | 3734 | `				(int)SyStrlen(PH7_NATIVE_IT_CUR),&sCur);` |
|   39 | 3735 | `			PH7_MemObjRelease(&sCur);` |
|   58 | 3736 | `			PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,` |
|   19 | 3737 | `				PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));` |
|   39 | 3738 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|   39 | 3739 | `			PdoStmtStep(pSt);` |
|   39 | 3740 | `			return;` |
|    - | 3741 | `		}` |
|   35 | 3742 | `		PH7_MemObjRelease(&sCur);` |
|   34 | 3743 | `		if( iBase != PDO_FETCH_ASSOC && iBase != PDO_FETCH_NUM && iBase != PDO_FETCH_BOTH` |
|   16 | 3744 | `		 && iBase != PDO_FETCH_OBJ && iBase != PDO_FETCH_NAMED ){` |
|    - | 3745 | `			/* a mode with nothing to hand out ends the walk */` |
|  ! 0 | 3746 | `			PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|  ! 0 | 3747 | `			return;` |
|    - | 3748 | `		}` |
|    - | 3749 | `	}` |
|   35 | 3750 | `	pRow = ph7_new_array(pVm);` |
|   35 | 3751 | `	if( pRow == 0 \|\| !PdoStmtRow(pVm,pSt,pSt->iFetchMode,pRow) ){` |
|  ! 0 | 3752 | `		if( pRow ){` |
|  ! 0 | 3753 | `			ph7_release_value(pVm,pRow);` |
|  ! 0 | 3754 | `		}` |
|  ! 0 | 3755 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|  ! 0 | 3756 | `		return;` |
|    - | 3757 | `	}` |
|   35 | 3758 | `	PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,(int)SyStrlen(PH7_NATIVE_IT_CUR),pRow);` |
|   35 | 3759 | `	ph7_release_value(pVm,pRow);` |
|   52 | 3760 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,` |
|   17 | 3761 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));` |
|   35 | 3762 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|   35 | 3763 | `	PdoStmtStep(pSt);` |
|   61 | 3764 | `}` |
|    - | 3765 | `/*` |
|    - | 3766 | ` * A rewind that does NOT rewind, and must not even re-read: the iterator is` |
|    - | 3767 | `` * built already positioned and `foreach` rewinds it again, so a settle here`` |
|    - | 3768 | ` * would swallow the first row. The AUX slot records that the first row has` |
|    - | 3769 | ` * been taken; every later rewind is a no-op, which is also what makes a SECOND` |
|    - | 3770 | ` * foreach over the same statement walk nothing at all -- php's answer, because` |
|    - | 3771 | ` * the cursor is forward-only and has nowhere to go back to.` |
|    - | 3772 | ` */` |
|   94 | 3773 | `static void PdoStmtIterRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 3774 | `{` |
|   95 | 3775 | `	if( PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_AUX) != 0 ){` |
|   47 | 3776 | `		return;` |
|    - | 3777 | `	}` |
|   49 | 3778 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_AUX,1);` |
|   49 | 3779 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|   49 | 3780 | `	PdoStmtIterSettle(&(*pVm),pIt);` |
|   48 | 3781 | `}` |
|   72 | 3782 | `static void PdoStmtIterNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 3783 | `{` |
|  109 | 3784 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|   72 | 3785 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|   73 | 3786 | `	PdoStmtIterSettle(&(*pVm),pIt);` |
|   73 | 3787 | `}` |
|    - | 3788 | `static const PH7_NativeIterVtab sPdoStmtIterVtab = {` |
|    - | 3789 | `	PdoStmtIterRewind, PdoStmtIterNext, 0, PdoStmtIterGuard };` |
|   62 | 3790 | `static int vm_builtin_PDOStatement_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3791 | `{` |
|   63 | 3792 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - | 3793 | `	ph7_class_instance *pIt;` |
|   31 | 3794 | `	SXUNUSED(nArg);` |
|   31 | 3795 | `	SXUNUSED(apArg);` |
|   63 | 3796 | `	if( pThis == 0 ){` |
|  ! 0 | 3797 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement::getIterator() needs a receiver");` |
|    - | 3798 | `	}` |
|   63 | 3799 | `	if( PdoStmtOfInstance(pThis) == 0 ){` |
|    - | 3800 | ``		/* A statement no driver built -- `new PDOStatement()` -- refuses the door`` |
|    - | 3801 | ``		 * the way every other method on one does, and `foreach` is that door: php`` |
|    - | 3802 | `		 * has nothing to iterate and says so instead of walking an empty set. */` |
|    5 | 3803 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3804 | `	}` |
|   59 | 3805 | `	if( PdoStmtWalkRefusal(pCtx,PdoStmtOfInstance(pThis)) ){` |
|    - | 3806 | ``		/* php refuses at the DOOR as well as at every step: `getIterator()` on a`` |
|    - | 3807 | `		 * statement it cannot walk raises there, before an iterator exists. */` |
|   11 | 3808 | `		return PH7_OK;` |
|    - | 3809 | `	}` |
|   49 | 3810 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|   49 | 3811 | `	if( pIt == 0 ){` |
|  ! 0 | 3812 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3813 | `	}` |
|   49 | 3814 | `	PH7_NativeResultObject(pCtx,pIt);` |
|   49 | 3815 | `	return PH7_OK;` |
|   32 | 3816 | `}` |
|    - | 3817 | `/*` |
|    - | 3818 | ` * Record one binding.  A name is kept as the script spelled it -- with or` |
|    - | 3819 | ` * without its colon -- because the resolution happens at execute(), when the` |
|    - | 3820 | ` * statement that knows the names exists.` |
|    - | 3821 | ` */` |
|   32 | 3822 | `static phl_pdo_bind * PdoBindAdd(phl_pdo_stmt *pSt,const char *zName,int nName,int iPos,` |
|    - | 3823 | `	int iType)` |
|    2 | 3824 | `{` |
|   34 | 3825 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
|    - | 3826 | `	phl_pdo_bind *pB;` |
|    - | 3827 | `	/* php REPLACES a binding for the same parameter rather than stacking one */` |
|   48 | 3828 | `	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){` |
|   29 | 3829 | `		if( zName ? (pB->zName && pB->nName == nName` |
|    1 | 3830 | `		             && SyMemcmp(pB->zName,zName,(sxu32)nName) == 0)` |
|   12 | 3831 | `		          : (pB->zName == 0 && pB->iPos == iPos) ){` |
|  ! 0 | 3832 | `			if( pB->pVal ){` |
|  ! 0 | 3833 | `				ph7_release_value(pVm,pB->pVal);` |
|  ! 0 | 3834 | `				pB->pVal = 0;` |
|  ! 0 | 3835 | `			}` |
|  ! 0 | 3836 | `			pB->iType = iType;` |
|  ! 0 | 3837 | `			pB->nSlot = SXU32_HIGH;` |
|  ! 0 | 3838 | `			return pB;` |
|    - | 3839 | `		}` |
|    9 | 3840 | `	}` |
|   34 | 3841 | `	pB = (phl_pdo_bind *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo_bind));` |
|   34 | 3842 | `	if( pB == 0 ){` |
|  ! 0 | 3843 | `		return 0;` |
|    - | 3844 | `	}` |
|   34 | 3845 | `	SyZero(pB,sizeof(phl_pdo_bind));` |
|   34 | 3846 | `	pB->iPos = iPos;` |
|   34 | 3847 | `	pB->iType = iType;` |
|   34 | 3848 | `	pB->nSlot = SXU32_HIGH;` |
|   34 | 3849 | `	if( zName && nName > 0 ){` |
|    6 | 3850 | `		pB->zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nName + 1);` |
|    6 | 3851 | `		if( pB->zName == 0 ){` |
|  ! 0 | 3852 | `			SyMemBackendFree(&pVm->sAllocator,pB);` |
|  ! 0 | 3853 | `			return 0;` |
|    - | 3854 | `		}` |
|    6 | 3855 | `		SyMemcpy(zName,pB->zName,(sxu32)nName);` |
|    6 | 3856 | `		pB->zName[nName] = 0;` |
|    6 | 3857 | `		pB->nName = nName;` |
|    2 | 3858 | `	}` |
|   34 | 3859 | `	pB->pNext = pSt->pBinds;` |
|   34 | 3860 | `	pSt->pBinds = pB;` |
|   34 | 3861 | `	return pB;` |
|   18 | 3862 | `}` |
|    - | 3863 | `/*` |
|    - | 3864 | ` * The shared body of bindValue() and bindParam(): they differ only in WHEN the` |
|    - | 3865 | ` * value is read. Argument #1 is a name or a 1-based position, and php refuses` |
|    - | 3866 | ` * position 0 by ValueError before the statement is consulted at all.` |
|    - | 3867 | ` */` |
|   34 | 3868 | `static int PdoBindArgument(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFn,` |
|    - | 3869 | `	int bByRef)` |
|    2 | 3870 | `{` |
|   36 | 3871 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 3872 | `	phl_pdo_bind *pB;` |
|    - | 3873 | `	ph7_value *pKey;` |
|    - | 3874 | `	int iType;` |
|   36 | 3875 | `	if( pSt == 0 ){` |
|  ! 0 | 3876 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3877 | `	}` |
|   36 | 3878 | `	pKey = nArg > 0 ? apArg[0] : 0;` |
|   36 | 3879 | `	iType = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : PDO_PARAM_STR;` |
|   38 | 3880 | `	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){` |
|    6 | 3881 | `		int nName = 0;` |
|    6 | 3882 | `		const char *zName = ph7_value_to_string(pKey,&nName);` |
|    6 | 3883 | `		pB = PdoBindAdd(pSt,zName,nName,0,iType);` |
|    4 | 3884 | `	}else{` |
|   32 | 3885 | `		ph7_int64 iPos = pKey ? ph7_value_to_int64(pKey) : 0;` |
|   32 | 3886 | `		if( iPos < 1 ){` |
|    4 | 3887 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    1 | 3888 | `				"%s(): Argument #1 ($param) must be greater than or equal to 1",zFn);` |
|    - | 3889 | `		}` |
|   30 | 3890 | `		pB = PdoBindAdd(pSt,0,0,(int)iPos,iType);` |
|    - | 3891 | `	}` |
|   34 | 3892 | `	if( pB == 0 ){` |
|  ! 0 | 3893 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3894 | `	}` |
|   34 | 3895 | `	if( bByRef ){` |
|    - | 3896 | `		/* bindParam(): remember the caller's SLOT, so a write to that variable` |
|    - | 3897 | `		 * after this call is the value execute() runs with. The engine hands a` |
|    - | 3898 | `		 * by-reference argument as the caller's own memobj, and its index is` |
|    - | 3899 | `		 * how every other deferred read here finds it again. */` |
|    3 | 3900 | `		pB->nSlot = (nArg > 1 && apArg[1]) ? apArg[1]->nIdx : SXU32_HIGH;` |
|   33 | 3901 | `	}else if( nArg > 1 ){` |
|    - | 3902 | `		/* bindValue(): the statement takes its own copy now */` |
|   32 | 3903 | `		pB->pVal = ph7_new_scalar(pCtx->pVm);` |
|   32 | 3904 | `		if( pB->pVal == 0 ){` |
|  ! 0 | 3905 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 3906 | `		}` |
|   32 | 3907 | `		PH7_MemObjStore(apArg[1],pB->pVal);` |
|   15 | 3908 | `	}` |
|   34 | 3909 | `	ph7_result_bool(pCtx,1);` |
|   34 | 3910 | `	return PH7_OK;` |
|   19 | 3911 | `}` |
|   32 | 3912 | `static int vm_builtin_PDOStatement_bindValue(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3913 | `{` |
|   34 | 3914 | `	return PdoBindArgument(pCtx,nArg,apArg,"PDOStatement::bindValue",FALSE);` |
|    2 | 3915 | `}` |
|    2 | 3916 | `static int vm_builtin_PDOStatement_bindParam(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3917 | `{` |
|    3 | 3918 | `	return PdoBindArgument(pCtx,nArg,apArg,"PDOStatement::bindParam",TRUE);` |
|    1 | 3919 | `}` |
|    - | 3920 | `/* Bind one recorded parameter, resolving a name against the live statement. */` |
|   28 | 3921 | `static int PdoBindApply(ph7_vm *pVm,phl_pdo_stmt *pSt,phl_pdo_bind *pB)` |
|    1 | 3922 | `{` |
|   29 | 3923 | `	ph7_value *pVal = pB->pVal;` |
|   29 | 3924 | `	int iPos = pB->iPos;` |
|   29 | 3925 | `	if( pB->zName ){` |
|    3 | 3926 | `		iPos = PH7_PdoSqliteBindIndexOf(pSt,pB->zName,pB->nName);` |
|    1 | 3927 | `	}` |
|   29 | 3928 | `	if( pB->nSlot != SXU32_HIGH ){` |
|    - | 3929 | `		/* bindParam(): read the caller's variable NOW */` |
|    3 | 3930 | `		pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pB->nSlot);` |
|    1 | 3931 | `	}` |
|   29 | 3932 | `	return PH7_PdoSqliteBindAt(pSt,iPos,pB->iType,pVal);` |
|    1 | 3933 | `}` |
|    - | 3934 | `/*` |
|    - | 3935 | `` * execute()'s `?array $params`: php binds the array INSTEAD of whatever was`` |
|    - | 3936 | ` * recorded, an integer key naming a 1-based position (so element 0 is` |
|    - | 3937 | ` * parameter 1) and a string key naming a placeholder.` |
|    - | 3938 | ` */` |
|   26 | 3939 | `static int PdoBindFromArray(ph7_vm *pVm,phl_pdo_stmt *pSt,ph7_value *pArray)` |
|    1 | 3940 | `{` |
|    - | 3941 | `	ph7_hashmap *pMap;` |
|    - | 3942 | `	ph7_hashmap_node *pEntry;` |
|    - | 3943 | `	sxu32 n,nCount;` |
|   27 | 3944 | `	int rc = 1;` |
|   27 | 3945 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 3946 | `		return 1;` |
|    - | 3947 | `	}` |
|   27 | 3948 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|   27 | 3949 | `	nCount = pMap->nEntry;` |
|   27 | 3950 | `	pEntry = pMap->pFirst;` |
|   53 | 3951 | `	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 3952 | `		ph7_value sKey;` |
|   35 | 3953 | `		ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nValIdx);` |
|    - | 3954 | `		int iPos;` |
|   35 | 3955 | `		PH7_MemObjInit(pVm,&sKey);` |
|   35 | 3956 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   35 | 3957 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   21 | 3958 | `			iPos = (int)sKey.x.iVal + 1;` |
|   11 | 3959 | `		}else{` |
|   15 | 3960 | `			int nName = 0;` |
|   15 | 3961 | `			const char *zName = ph7_value_to_string(&sKey,&nName);` |
|   15 | 3962 | `			iPos = PH7_PdoSqliteBindIndexOf(pSt,zName,nName);` |
|    - | 3963 | `		}` |
|   35 | 3964 | `		PH7_MemObjRelease(&sKey);` |
|    - | 3965 | `		/* php binds every element as a STRING unless the script said otherwise` |
|    - | 3966 | `		 * through bindValue(); a php null still binds as NULL. */` |
|   35 | 3967 | `		if( !PH7_PdoSqliteBindAt(pSt,iPos,PDO_PARAM_STR,pVal) ){` |
|    9 | 3968 | `			rc = 0;` |
|    9 | 3969 | `			break;` |
|    - | 3970 | `		}` |
|   14 | 3971 | `	}` |
|   27 | 3972 | `	return rc;` |
|   14 | 3973 | `}` |
|    - | 3974 | `/*` |
|    - | 3975 | ` * PDOStatement::execute(?array $params = null): bool` |
|    - | 3976 | ` *` |
|    - | 3977 | ` * Runs the statement from the start: the cursor is rewound, the previous run's` |
|    - | 3978 | ` * values are dropped, the parameters are bound and one step is taken -- the` |
|    - | 3979 | ` * same first step query() takes, so columnCount() and the first fetch() behave` |
|    - | 3980 | ` * identically whichever verb produced the statement.` |
|    - | 3981 | ` */` |
|   52 | 3982 | `static int vm_builtin_PDOStatement_execute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3983 | `{` |
|   54 | 3984 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|   54 | 3985 | `	int bOk = 1;` |
|   54 | 3986 | `	if( pSt == 0 ){` |
|  ! 0 | 3987 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 3988 | `	}` |
|   54 | 3989 | `	PH7_PdoTouch(pSt->pConn);` |
|   54 | 3990 | `	PH7_PdoSqliteReset(pSt);` |
|   54 | 3991 | `	pSt->bRowPending = 0;` |
|   54 | 3992 | `	pSt->bDone = 0;` |
|   54 | 3993 | `	if( nArg > 0 && apArg[0] && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){` |
|   27 | 3994 | `		bOk = PdoBindFromArray(pCtx->pVm,pSt,apArg[0]);` |
|   14 | 3995 | `	}else{` |
|    - | 3996 | `		phl_pdo_bind *pB;` |
|   56 | 3997 | `		for( pB = pSt->pBinds ; pB && bOk ; pB = pB->pNext ){` |
|   29 | 3998 | `			bOk = PdoBindApply(pCtx->pVm,pSt,pB);` |
|   15 | 3999 | `		}` |
|    - | 4000 | `	}` |
|   54 | 4001 | `	if( !bOk ){` |
|    9 | 4002 | `		ph7_result_bool(pCtx,0);` |
|    9 | 4003 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|    9 | 4004 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::execute");` |
|    - | 4005 | `	}` |
|   46 | 4006 | `	pSt->bExecuted = 1;` |
|   46 | 4007 | `	if( PdoStmtStep(pSt) < 0 ){` |
|    3 | 4008 | `		ph7_result_bool(pCtx,0);` |
|    3 | 4009 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|    3 | 4010 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::execute");` |
|    - | 4011 | `	}` |
|   44 | 4012 | `	PdoStmtOk(pSt);` |
|   44 | 4013 | `	pSt->nChanges = PH7_PdoSqliteColumnCount(pSt) > 0` |
|   23 | 4014 | `		? 0 : PH7_PdoSqliteChanges(pSt->pConn);` |
|   44 | 4015 | `	ph7_result_bool(pCtx,1);` |
|   44 | 4016 | `	return PH7_OK;` |
|   28 | 4017 | `}` |
|    - | 4018 | `/*` |
|    - | 4019 | ` * The class query()/prepare() builds.  ATTR_STATEMENT_CLASS replaces` |
|    - | 4020 | ` * PDOStatement with a subclass of the script's own, and php builds THAT for` |
|    - | 4021 | ` * every statement the connection makes from then on.` |
|    - | 4022 | ` */` |
|    - | 4023 | `/*` |
|    - | 4024 | ` * php builds the statement OBJECT itself and then calls the class's own` |
|    - | 4025 | ` * constructor with the arguments ATTR_STATEMENT_CLASS was given -- and refuses` |
|    - | 4026 | ` * outright when there are arguments and no constructor to take them.` |
|    - | 4027 | ` */` |
|  920 | 4028 | `static sxi32 PdoStatementCtor(ph7_context *pCtx,phl_pdo *pConn,ph7_class *pClass,` |
|    - | 4029 | `	ph7_class_instance *pObj)` |
|    4 | 4030 | `{` |
|  924 | 4031 | `	ph7_class_method *pCons = PH7_ClassExtractMethod(pClass,"__construct",` |
|    - | 4032 | `		sizeof("__construct")-1);` |
|  924 | 4033 | `	if( pCons == 0 ){` |
|  916 | 4034 | `		if( pConn->pStmtArgs ){` |
|    3 | 4035 | `			return PH7_VmThrowException(pCtx,"Error",` |
|    - | 4036 | `				"User-supplied statement does not accept constructor arguments");` |
|    - | 4037 | `		}` |
|  914 | 4038 | `		return PH7_OK;` |
|    - | 4039 | `	}` |
|    9 | 4040 | `	PdoCallCtor(pCtx->pVm,pObj,pCons,pConn->pStmtArgs);` |
|    9 | 4041 | `	return PH7_OK;` |
|  464 | 4042 | `}` |
|  920 | 4043 | `static ph7_class * PdoStatementClass(ph7_context *pCtx,phl_pdo *pConn)` |
|    4 | 4044 | `{` |
|  924 | 4045 | `	if( pConn->zStmtClass ){` |
|   32 | 4046 | `		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,pConn->zStmtClass,` |
|   20 | 4047 | `			(sxu32)pConn->nStmtClass,FALSE,0);` |
|   22 | 4048 | `		if( pClass ){` |
|   22 | 4049 | `			return pClass;` |
|    - | 4050 | `		}` |
|  ! 0 | 4051 | `	}` |
|  904 | 4052 | `	return PH7_VmExtractClass(pCtx->pVm,"PDOStatement",sizeof("PDOStatement")-1,FALSE,0);` |
|  464 | 4053 | `}` |
|    - | 4054 | `/*` |
|    - | 4055 | ` * PDO::prepare(string $query, array $options = []): PDOStatement\|false` |
|    - | 4056 | ` *` |
|    - | 4057 | ` * Compiles without running. The options array is php's per-statement` |
|    - | 4058 | ` * attribute set; the sqlite driver carries none of the ones a script can put` |
|    - | 4059 | ` * there, and php ignores an unusable one rather than refusing the call.` |
|    - | 4060 | ` */` |
|   66 | 4061 | `static int vm_builtin_PDO_prepare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 4062 | `{` |
|   68 | 4063 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 4064 | `	phl_pdo_stmt *pSt;` |
|    - | 4065 | `	ph7_class *pClass;` |
|    - | 4066 | `	ph7_class_instance *pObj;` |
|    - | 4067 | `	const char *zSql;` |
|   68 | 4068 | `	int nSql = 0;` |
|   68 | 4069 | `	if( pConn == 0 ){` |
|  ! 0 | 4070 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4071 | `	}` |
|   68 | 4072 | `	PH7_PdoTouch(pConn);` |
|   68 | 4073 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|   68 | 4074 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 | 4075 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 4076 | `			"PDO::prepare(): Argument #1 ($query) must not be empty");` |
|    - | 4077 | `	}` |
|   66 | 4078 | `	pSt = PH7_PdoNewStmt(pConn);` |
|   66 | 4079 | `	if( pSt == 0 ){` |
|  ! 0 | 4080 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4081 | `	}` |
|   66 | 4082 | `	if( !PH7_PdoSqlitePrepare(pSt,zSql,nSql) ){` |
|    3 | 4083 | `		ph7_result_bool(pCtx,0);` |
|    3 | 4084 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::prepare");` |
|    - | 4085 | `	}` |
|   64 | 4086 | `	pClass = PdoStatementClass(pCtx,pConn);` |
|   64 | 4087 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|   64 | 4088 | `	if( pObj == 0 ){` |
|  ! 0 | 4089 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4090 | `	}` |
|   64 | 4091 | `	if( PdoStmtAttach(pObj,pSt) != 0 ){` |
|  ! 0 | 4092 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4093 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4094 | `	}` |
|   64 | 4095 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,"queryString",zSql,nSql);` |
|    - | 4096 | `	{` |
|   64 | 4097 | `		sxi32 rcCtor = PdoStatementCtor(pCtx,pConn,pClass,pObj);` |
|   64 | 4098 | `		if( rcCtor != PH7_OK ){` |
|  ! 0 | 4099 | `			PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4100 | `			return rcCtor;` |
|    - | 4101 | `		}` |
|    - | 4102 | `	}` |
|   64 | 4103 | `	PH7_NativeResultObject(pCtx,pObj);` |
|   64 | 4104 | `	return PH7_OK;` |
|   35 | 4105 | `}` |
|    - | 4106 | `/*` |
|    - | 4107 | ` * PDO::quote(string $string, int $type = PDO::PARAM_STR): string\|false` |
|    - | 4108 | ` *` |
|    - | 4109 | ` * sqlite's own quoting: single quotes around it, each embedded quote doubled.` |
|    - | 4110 | ` * A NUL byte has no spelling inside a sqlite literal at all, so php refuses` |
|    - | 4111 | ` * one -- with a bare sentence carrying no SQLSTATE, unlike every other` |
|    - | 4112 | ` * PDOException this driver raises.` |
|    - | 4113 | ` */` |
|   10 | 4114 | `static int vm_builtin_PDO_quote(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4115 | `{` |
|   11 | 4116 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 4117 | `	const char *zIn;` |
|   11 | 4118 | `	int nIn = 0,i;` |
|    - | 4119 | `	SyBlob sOut;` |
|   11 | 4120 | `	if( pConn == 0 ){` |
|  ! 0 | 4121 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4122 | `	}` |
|   11 | 4123 | `	PH7_PdoTouch(pConn);` |
|   11 | 4124 | `	zIn = nArg > 0 ? ph7_value_to_string(apArg[0],&nIn) : "";` |
|   31 | 4125 | `	for( i = 0 ; i < nIn ; ++i ){` |
|   23 | 4126 | `		if( zIn[i] == 0 ){` |
|    3 | 4127 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4128 | `				"SQLite PDO::quote does not support null bytes");` |
|    - | 4129 | `		}` |
|   11 | 4130 | `	}` |
|    9 | 4131 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    9 | 4132 | `	SyBlobAppend(&sOut,"'",1);` |
|   27 | 4133 | `	for( i = 0 ; i < nIn ; ++i ){` |
|   19 | 4134 | `		if( zIn[i] == '\'' ){` |
|    3 | 4135 | `			SyBlobAppend(&sOut,"'",1);` |
|    1 | 4136 | `		}` |
|   19 | 4137 | `		SyBlobAppend(&sOut,&zIn[i],1);` |
|   10 | 4138 | `	}` |
|    9 | 4139 | `	SyBlobAppend(&sOut,"'",1);` |
|    9 | 4140 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    9 | 4141 | `	SyBlobRelease(&sOut);` |
|    9 | 4142 | `	return PH7_OK;` |
|    6 | 4143 | `}` |
|    - | 4144 | `/*` |
|    - | 4145 | ` * PDO::query(string $query, ...): PDOStatement\|false` |
|    - | 4146 | ` *` |
|    - | 4147 | `` * Prepares and runs ONE statement -- what follows a `;` is compiled but never`` |
|    - | 4148 | ` * executed, unlike exec(), which runs them all.` |
|    - | 4149 | ` */` |
|  868 | 4150 | `static int vm_builtin_PDO_query(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 | 4151 | `{` |
|  872 | 4152 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 4153 | `	phl_pdo_stmt *pSt;` |
|    - | 4154 | `	ph7_class *pClass;` |
|    - | 4155 | `	ph7_class_instance *pObj;` |
|    - | 4156 | `	const char *zSql;` |
|  872 | 4157 | `	int nSql = 0;` |
|  872 | 4158 | `	if( pConn == 0 ){` |
|  ! 0 | 4159 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4160 | `	}` |
|  872 | 4161 | `	PH7_PdoTouch(pConn);` |
|  872 | 4162 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|  872 | 4163 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 | 4164 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 4165 | `			"PDO::query(): Argument #1 ($query) must not be empty");` |
|    - | 4166 | `	}` |
|  870 | 4167 | `	pSt = PH7_PdoNewStmt(pConn);` |
|  870 | 4168 | `	if( pSt == 0 ){` |
|  ! 0 | 4169 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4170 | `	}` |
|  870 | 4171 | `	if( !PH7_PdoSqlitePrepare(pSt,zSql,nSql) ){` |
|    7 | 4172 | `		ph7_result_bool(pCtx,0);` |
|    7 | 4173 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::query");` |
|    - | 4174 | `	}` |
|  864 | 4175 | `	pSt->bExecuted = 1;` |
|  864 | 4176 | `	if( PdoStmtStep(pSt) < 0 ){` |
|    3 | 4177 | `		ph7_result_bool(pCtx,0);` |
|    3 | 4178 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::query");` |
|    - | 4179 | `	}` |
|    - | 4180 | `	/* the change count is read once, here: a later statement on the same` |
|    - | 4181 | `	 * connection would otherwise move what this one reports */` |
|  862 | 4182 | `	pSt->nChanges = PH7_PdoSqliteColumnCount(pSt) > 0` |
|  432 | 4183 | `		? 0 : PH7_PdoSqliteChanges(pConn);` |
|  862 | 4184 | `	pClass = PdoStatementClass(pCtx,pConn);` |
|  862 | 4185 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|  862 | 4186 | `	if( pObj == 0 ){` |
|  ! 0 | 4187 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4188 | `	}` |
|  862 | 4189 | `	if( PdoStmtAttach(pObj,pSt) != 0 ){` |
|  ! 0 | 4190 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4191 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4192 | `	}` |
|  862 | 4193 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,"queryString",zSql,nSql);` |
|    - | 4194 | `	{` |
|  862 | 4195 | `		sxi32 rcCtor = PdoStatementCtor(pCtx,pConn,pClass,pObj);` |
|  862 | 4196 | `		if( rcCtor != PH7_OK ){` |
|    3 | 4197 | `			PH7_ClassInstanceUnref(pObj);` |
|    3 | 4198 | `			return rcCtor;` |
|    - | 4199 | `		}` |
|    - | 4200 | `	}` |
|  860 | 4201 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    - | 4202 | `		/* php's second argument IS setFetchMode(), run on the statement this` |
|    - | 4203 | `		 * call just built -- same screen, same per-mode arity, and diagnostics` |
|    - | 4204 | `		 * that count from PDO::query()'s own signature. A refusal leaves the` |
|    - | 4205 | `		 * statement behind (php's does too), so it is raised after the object` |
|    - | 4206 | `		 * exists rather than before the query runs. */` |
|  250 | 4207 | `		sxi32 rcMode = PdoSetupFetchMode(pCtx,pSt,nArg,apArg,2,"PDO::query","fetchMode");` |
|  250 | 4208 | `		if( rcMode != PH7_OK ){` |
|  195 | 4209 | `			PH7_ClassInstanceUnref(pObj);` |
|  195 | 4210 | `			return rcMode;` |
|    - | 4211 | `		}` |
|   27 | 4212 | `	}` |
|  666 | 4213 | `	PdoStmtOk(pSt);   /* it ran, and it ran cleanly */` |
|    - | 4214 | `	/* PH7_NativeResultObject takes the reference this call made: unref'ing` |
|    - | 4215 | `	 * again here frees the object the result slot is still holding. */` |
|  666 | 4216 | `	PH7_NativeResultObject(pCtx,pObj);` |
|  666 | 4217 | `	return PH7_OK;` |
|  438 | 4218 | `}` |
|    - | 4219 |  |
|    - | 4220 | `/* ------------------------------------------------------------------------` |
|    - | 4221 | ` * Transactions` |
|    - | 4222 | ` * ------------------------------------------------------------------------ */` |
|    - | 4223 | `/*` |
|    - | 4224 | ` * PDO::beginTransaction(): bool / commit() / rollBack() / inTransaction()` |
|    - | 4225 | ` *` |
|    - | 4226 | ` * Whether a transaction is open is sqlite's own autocommit flag and not a` |
|    - | 4227 | ` * count this driver keeps, so a BEGIN the script sent through exec() is` |
|    - | 4228 | ` * indistinguishable from beginTransaction() -- inTransaction() answers true` |
|    - | 4229 | ` * for it and a second beginTransaction() refuses.` |
|    - | 4230 | ` *` |
|    - | 4231 | ` * The three refusals are bare sentences with no SQLSTATE in front of them,` |
|    - | 4232 | ` * which is unlike every other PDOException the driver raises; and the four` |
|    - | 4233 | ` * verbs are the ones that do NOT clear the handle's error on entry.` |
|    - | 4234 | ` */` |
|   28 | 4235 | `static int PdoTxRun(ph7_context *pCtx,const char *zSql,const char *zFn)` |
|    1 | 4236 | `{` |
|   29 | 4237 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|   29 | 4238 | `	if( pConn == 0 ){` |
|  ! 0 | 4239 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4240 | `	}` |
|   29 | 4241 | `	if( PH7_PdoSqliteExec(pConn,zSql,(int)SyStrlen(zSql)) < 0 ){` |
|  ! 0 | 4242 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4243 | `		return PH7_PdoRaise(pCtx,pConn,zFn);` |
|    - | 4244 | `	}` |
|   29 | 4245 | `	ph7_result_bool(pCtx,1);` |
|   29 | 4246 | `	return PH7_OK;` |
|   15 | 4247 | `}` |
|   16 | 4248 | `static int vm_builtin_PDO_beginTransaction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4249 | `{` |
|   17 | 4250 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|   17 | 4251 | `	const char *zBegin = "BEGIN";` |
|    8 | 4252 | `	SXUNUSED(nArg);` |
|    8 | 4253 | `	SXUNUSED(apArg);` |
|   17 | 4254 | `	if( pConn == 0 ){` |
|  ! 0 | 4255 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4256 | `	}` |
|   17 | 4257 | `	if( PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 4258 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4259 | `			"There is already an active transaction");` |
|    - | 4260 | `	}` |
|    - | 4261 | `	/* which BEGIN, per Pdo\Sqlite::ATTR_TRANSACTION_MODE */` |
|   15 | 4262 | `	if( pConn->iTxMode == 1 ){` |
|    3 | 4263 | `		zBegin = "BEGIN IMMEDIATE";` |
|   14 | 4264 | `	}else if( pConn->iTxMode == 2 ){` |
|    3 | 4265 | `		zBegin = "BEGIN EXCLUSIVE";` |
|    1 | 4266 | `	}` |
|   15 | 4267 | `	return PdoTxRun(pCtx,zBegin,"PDO::beginTransaction");` |
|    9 | 4268 | `}` |
|   12 | 4269 | `static int vm_builtin_PDO_commit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4270 | `{` |
|   13 | 4271 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    6 | 4272 | `	SXUNUSED(nArg);` |
|    6 | 4273 | `	SXUNUSED(apArg);` |
|   13 | 4274 | `	if( pConn == 0 ){` |
|  ! 0 | 4275 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4276 | `	}` |
|   13 | 4277 | `	if( !PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 4278 | `		return PH7_VmThrowException(pCtx,"PDOException","There is no active transaction");` |
|    - | 4279 | `	}` |
|   11 | 4280 | `	return PdoTxRun(pCtx,"COMMIT","PDO::commit");` |
|    7 | 4281 | `}` |
|    6 | 4282 | `static int vm_builtin_PDO_rollBack(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4283 | `{` |
|    7 | 4284 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    3 | 4285 | `	SXUNUSED(nArg);` |
|    3 | 4286 | `	SXUNUSED(apArg);` |
|    7 | 4287 | `	if( pConn == 0 ){` |
|  ! 0 | 4288 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4289 | `	}` |
|    7 | 4290 | `	if( !PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 4291 | `		return PH7_VmThrowException(pCtx,"PDOException","There is no active transaction");` |
|    - | 4292 | `	}` |
|    5 | 4293 | `	return PdoTxRun(pCtx,"ROLLBACK","PDO::rollBack");` |
|    4 | 4294 | `}` |
|   10 | 4295 | `static int vm_builtin_PDO_inTransaction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4296 | `{` |
|   11 | 4297 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    5 | 4298 | `	SXUNUSED(nArg);` |
|    5 | 4299 | `	SXUNUSED(apArg);` |
|   11 | 4300 | `	if( pConn == 0 ){` |
|  ! 0 | 4301 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 4302 | `	}` |
|   11 | 4303 | `	ph7_result_bool(pCtx,PH7_PdoSqliteInTransaction(pConn));` |
|   11 | 4304 | `	return PH7_OK;` |
|    6 | 4305 | `}` |
|    - | 4306 |  |
|    - | 4307 | `/* ------------------------------------------------------------------------` |
|    - | 4308 | ` * Connecting` |
|    - | 4309 | ` * ------------------------------------------------------------------------ */` |
|    - | 4310 | `/*` |
|    - | 4311 | `` * Apply the constructor's `?array $options`.  php walks it before the driver`` |
|    - | 4312 | ` * sees the handle, so an ATTR_ERRMODE in there is already in force when a` |
|    - | 4313 | ` * later failure is routed -- and a key no driver knows is ignored in silence.` |
|    - | 4314 | ` */` |
|   52 | 4315 | `static void PdoApplyOptions(phl_pdo *pConn,ph7_value *pOptions)` |
|    2 | 4316 | `{` |
|    - | 4317 | `	ph7_hashmap *pMap;` |
|    - | 4318 | `	ph7_hashmap_node *pEntry;` |
|    - | 4319 | `	sxu32 n,nCount;` |
|   54 | 4320 | `	if( pOptions == 0 \|\| (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 4321 | `		return;` |
|    - | 4322 | `	}` |
|   54 | 4323 | `	pMap = (ph7_hashmap *)pOptions->x.pOther;` |
|   54 | 4324 | `	nCount = pMap->nEntry;` |
|   54 | 4325 | `	pEntry = pMap->pFirst;` |
|  108 | 4326 | `	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 4327 | `		ph7_value sKey;` |
|    - | 4328 | `		ph7_value *pVal;` |
|    - | 4329 | `		ph7_int64 iKey;` |
|   56 | 4330 | `		if( pEntry->iType != HASHMAP_INT_NODE ){` |
|  ! 0 | 4331 | `			continue;  /* php ignores a string key here */` |
|    - | 4332 | `		}` |
|   56 | 4333 | `		PH7_MemObjInit(pConn->pVm,&sKey);` |
|   56 | 4334 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   56 | 4335 | `		iKey = sKey.x.iVal;` |
|   56 | 4336 | `		PH7_MemObjRelease(&sKey);` |
|   56 | 4337 | `		pVal = (ph7_value *)PH7_MemObjAt(&pConn->pVm->aMemObj,pEntry->nValIdx);` |
|   56 | 4338 | `		if( pVal == 0 ){` |
|  ! 0 | 4339 | `			continue;` |
|    - | 4340 | `		}` |
|   56 | 4341 | `		switch( iKey ){` |
|   24 | 4342 | `			case PDO_ATTR_ERRMODE:            pConn->iErrMode = (int)ph7_value_to_int64(pVal); break;` |
|    9 | 4343 | `			case PDO_ATTR_CASE:               pConn->iCase = (int)ph7_value_to_int64(pVal); break;` |
|    9 | 4344 | `			case PDO_ATTR_ORACLE_NULLS:       pConn->iOracleNulls = (int)ph7_value_to_int64(pVal); break;` |
|    9 | 4345 | `			case PDO_ATTR_DEFAULT_FETCH_MODE: pConn->iDefaultFetch = (int)ph7_value_to_int64(pVal); break;` |
|    3 | 4346 | `			case PDO_ATTR_STRINGIFY_FETCHES:  pConn->bStringify = ph7_value_to_bool(pVal); break;` |
|    - | 4347 | `			/* php remembers this one and reports it back, though a CLI process` |
|    - | 4348 | `			 * has no pool to keep the handle in. */` |
|    3 | 4349 | `			case PDO_ATTR_PERSISTENT:         pConn->bPersistent = ph7_value_to_bool(pVal); break;` |
|  ! 0 | 4350 | `			case PDO_SQLITE_ATTR_TRANSACTION_MODE: pConn->iTxMode = (int)ph7_value_to_int64(pVal); break;` |
|    - | 4351 | `			/* the OPEN flags are read here and used by the open itself, which is` |
|    - | 4352 | `			 * the only moment they mean anything */` |
|    3 | 4353 | `			case PDO_SQLITE_ATTR_OPEN_FLAGS: pConn->iOpenFlags = (int)ph7_value_to_int64(pVal); break;` |
|  ! 0 | 4354 | `			case PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES: pConn->bExtendedCodes = ph7_value_to_bool(pVal); break;` |
|    2 | 4355 | `			default: break;` |
|    - | 4356 | `		}` |
|   29 | 4357 | `	}` |
|   28 | 4358 | `}` |
|    - | 4359 | `/*` |
|    - | 4360 | `` * php's `uri:` DSN: the real DSN is the FIRST LINE of what that URI names, read`` |
|    - | 4361 | ` * through the ordinary stream layer -- so a php:// wrapper or a userland one is` |
|    - | 4362 | ` * a source too. The line is used verbatim, which is why one with leading` |
|    - | 4363 | `` * whitespace, or a second `uri:`, comes back as "could not find driver" rather`` |
|    - | 4364 | ` * than anything more specific: it is simply parsed as a driver name.` |
|    - | 4365 | ` *` |
|    - | 4366 | ` * Returns 0 when the URI could not be read (the caller words the refusal); the` |
|    - | 4367 | ` * open warning underneath it is the stream layer's own, attributed to` |
|    - | 4368 | ` * PDO::__construct the way php attributes it.` |
|    - | 4369 | ` */` |
|    8 | 4370 | `static int PdoResolveUriDsn(ph7_context *pCtx,const char *zUri,int nUri,SyBlob *pOut)` |
|    1 | 4371 | `{` |
|    - | 4372 | `	const ph7_io_stream *pStream;` |
|    - | 4373 | `	const char *zFile,*zWhole;` |
|    - | 4374 | `	void *pHandle;` |
|    - | 4375 | `	SyBlob sRaw;` |
|    - | 4376 | `	const char *zRaw;` |
|    - | 4377 | `	sxu32 n,nRaw;` |
|    9 | 4378 | `	int rc = 0;` |
|    9 | 4379 | `	if( nUri < 1 ){` |
|  ! 0 | 4380 | `		return 0;` |
|    - | 4381 | `	}` |
|    - | 4382 | `	/* the slice is not a C string, and the stream layer wants one */` |
|    9 | 4383 | `	SyBlobInit(&sRaw,&pCtx->pVm->sAllocator);` |
|    9 | 4384 | `	SyBlobAppend(&sRaw,zUri,(sxu32)nUri);` |
|    9 | 4385 | `	SyBlobAppend(&sRaw,"",1);` |
|    9 | 4386 | `	zFile = zWhole = (const char *)SyBlobData(&sRaw);` |
|    9 | 4387 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nUri);` |
|    9 | 4388 | `	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|    4 | 4389 | `		FALSE,0,FALSE,0,"PDO::__construct") : 0;` |
|    9 | 4390 | `	if( pHandle == 0 ){` |
|    - | 4391 | `		/* the device lookup advanced zFile past the wrapper prefix; php's warning` |
|    - | 4392 | ``		 * names the URI the SCRIPT wrote, `file:///nope` and not `/nope` */`` |
|    4 | 4393 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"PDO::__construct(%s): Failed to open stream: %s",` |
|    1 | 4394 | `			zWhole,"No such file or directory");` |
|    3 | 4395 | `		SyBlobRelease(&sRaw);` |
|    3 | 4396 | `		return 0;` |
|    - | 4397 | `	}` |
|    7 | 4398 | `	SyBlobReset(&sRaw);` |
|    7 | 4399 | `	if( PH7_StreamReadWholeFile(pHandle,pStream,&sRaw) == SXRET_OK ){` |
|    7 | 4400 | `		zRaw = (const char *)SyBlobData(&sRaw);` |
|    7 | 4401 | `		nRaw = SyBlobLength(&sRaw);` |
|    - | 4402 | `		/* php reads ONE line and keeps its terminator, so a file written with a` |
|    - | 4403 | `		 * trailing newline yields a DSN that ends in one -- which reaches sqlite` |
|    - | 4404 | ``		 * as part of the PATH. That is why `sqlite::memory:\n` opens a FILE of`` |
|    - | 4405 | `		 * that name rather than a memory database; the newline is not noise the` |
|    - | 4406 | `		 * driver trims, and trimming it here would answer differently. */` |
|  109 | 4407 | `		for( n = 0 ; n < nRaw && zRaw[n] != '\n' ; ++n ){}` |
|    7 | 4408 | `		if( n < nRaw ){` |
|  ! 0 | 4409 | `			++n;  /* the newline belongs to the line */` |
|  ! 0 | 4410 | `		}` |
|    7 | 4411 | `		if( n > 0 ){` |
|    7 | 4412 | `			SyBlobAppend(pOut,zRaw,n);` |
|    7 | 4413 | `			rc = 1;` |
|    3 | 4414 | `		}` |
|    3 | 4415 | `	}` |
|    7 | 4416 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    7 | 4417 | `	SyBlobRelease(&sRaw);` |
|    7 | 4418 | `	return rc;` |
|    5 | 4419 | `}` |
|    - | 4420 | `/*` |
|    - | 4421 | ` * The driver split, over a DSN that is already resolved.  php reads up to the` |
|    - | 4422 | `` * first `:` as the driver name and hands the rest to that driver; the name is`` |
|    - | 4423 | `` * matched case-SENSITIVELY, so `SQLITE:` is "could not find driver" rather`` |
|    - | 4424 | ` * than a connection, and a DSN with no colon at all is refused before any` |
|    - | 4425 | ` * driver is looked for.` |
|    - | 4426 | ` */` |
|  178 | 4427 | `static int PdoOpenParsed(ph7_context *pCtx,phl_pdo *pConn,const char *zDsn,int nDsn)` |
|    5 | 4428 | `{` |
|    - | 4429 | `	int nDriver;` |
| 1263 | 4430 | `	for( nDriver = 0 ; nDriver < nDsn && zDsn[nDriver] != ':' ; ++nDriver ){}` |
|  183 | 4431 | `	if( nDriver >= nDsn ){` |
|    - | 4432 | `		/* no colon: php refuses the ARGUMENT, not the driver */` |
|    5 | 4433 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4434 | `			"PDO::__construct(): Argument #1 ($dsn) must be a valid data source name");` |
|    - | 4435 | `	}` |
|  174 | 4436 | `	if( nDriver != (int)sizeof("sqlite")-1` |
|  174 | 4437 | `	 \|\| SyMemcmp(zDsn,"sqlite",sizeof("sqlite")-1) != 0 ){` |
|    - | 4438 | `		/* §10 scopes this build to one driver, so every other name -- and every` |
|    - | 4439 | `		 * other SPELLING of this one -- is what a php without that driver says. */` |
|   13 | 4440 | `		return PH7_VmThrowException(pCtx,"PDOException","could not find driver");` |
|    - | 4441 | `	}` |
|    - | 4442 | `	/* SQLITE_OPEN_URI is passed EXPLICITLY rather than left to the linked` |
|    - | 4443 | `	 * library's compile-time default: a Debian libsqlite3 is built with URI` |
|    - | 4444 | `` 	 * filenames on and a vcpkg one is not, so `sqlite:file::memory:?cache=shared` `` |
|    - | 4445 | `	 * opened a memory database on one platform and created a FILE of that name on` |
|    - | 4446 | `	 * the other. php's own sqlite has them on, so on is the answer everywhere. */` |
|    - | 4447 | `	{` |
|    - | 4448 | `		/* the script's own ATTR_OPEN_FLAGS replace the read-write default */` |
|  248 | 4449 | `		int iFlags = pConn->iOpenFlags` |
|    1 | 4450 | `			? pConn->iOpenFlags` |
|   81 | 4451 | `			: (SQLITE_OPEN_READWRITE\|SQLITE_OPEN_CREATE);` |
|  248 | 4452 | `		return PH7_PdoSqliteOpen(pCtx,pConn,zDsn + nDriver + 1,nDsn - nDriver - 1,` |
|   81 | 4453 | `			iFlags\|SQLITE_OPEN_URI);` |
|    - | 4454 | `	}` |
|   94 | 4455 | `}` |
|    - | 4456 | `/*` |
|    - | 4457 | `` * One DSN, resolved then split: php's `uri:` form is read first, and what it`` |
|    - | 4458 | ` * names replaces the DSN whole.` |
|    - | 4459 | ` */` |
|  180 | 4460 | `static int PdoOpenFromDsn(ph7_context *pCtx,phl_pdo *pConn,ph7_value *pDsn)` |
|    5 | 4461 | `{` |
|    - | 4462 | `	const char *zDsn;` |
|    - | 4463 | `	int nDsn,rc;` |
|    - | 4464 | `	SyBlob sResolved;` |
|  185 | 4465 | `	if( pDsn == 0 ){` |
|  ! 0 | 4466 | `		nDsn = 0;` |
|  ! 0 | 4467 | `		zDsn = "";` |
|  ! 0 | 4468 | `	}else{` |
|  185 | 4469 | `		zDsn = ph7_value_to_string(pDsn,&nDsn);` |
|    - | 4470 | `	}` |
|  185 | 4471 | `	SyBlobInit(&sResolved,&pCtx->pVm->sAllocator);` |
|  185 | 4472 | `	if( nDsn >= (int)sizeof("uri:")-1 && SyMemcmp(zDsn,"uri:",sizeof("uri:")-1) == 0 ){` |
|    9 | 4473 | `		if( !PdoResolveUriDsn(pCtx,zDsn + sizeof("uri:")-1,nDsn - ((int)sizeof("uri:")-1),` |
|    - | 4474 | `			&sResolved) ){` |
|    3 | 4475 | `			SyBlobRelease(&sResolved);` |
|    3 | 4476 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 4477 | `				"PDO::__construct(): Argument #1 ($dsn) must be a valid data source URI");` |
|    - | 4478 | `		}` |
|    - | 4479 | `		/* the line replaces the DSN whole, and is NOT resolved again: a nested` |
|    - | 4480 | ``		 * `uri:` is read as a driver name, exactly as php reads it */`` |
|    7 | 4481 | `		zDsn = (const char *)SyBlobData(&sResolved);` |
|    7 | 4482 | `		nDsn = (int)SyBlobLength(&sResolved);` |
|    3 | 4483 | `	}` |
|  183 | 4484 | `	rc = PdoOpenParsed(pCtx,pConn,zDsn,nDsn);` |
|  183 | 4485 | `	SyBlobRelease(&sResolved);` |
|  183 | 4486 | `	return rc;` |
|   95 | 4487 | `}` |
|    - | 4488 | `/*` |
|    - | 4489 | ` * PDO::__construct(string $dsn, ?string $username = null, ?string $password = null,` |
|    - | 4490 | ` *                  ?array $options = null)` |
|    - | 4491 | ` *` |
|    - | 4492 | ` * The two credential arguments are the generic surface: sqlite has no user to` |
|    - | 4493 | ` * be, so php accepts and ignores them rather than refusing a portable call.` |
|    - | 4494 | ` */` |
|  160 | 4495 | `static int vm_builtin_PDO___construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 | 4496 | `{` |
|  165 | 4497 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - | 4498 | `	phl_pdo *pConn;` |
|    - | 4499 | `	int rc;` |
|  165 | 4500 | `	if( pThis == 0 ){` |
|  ! 0 | 4501 | `		return PH7_VmThrowException(pCtx,"Error","PDO::__construct() needs a receiver");` |
|    - | 4502 | `	}` |
|  165 | 4503 | `	pConn = PH7_PdoNewConn(pCtx->pVm);` |
|  165 | 4504 | `	if( pConn == 0 ){` |
|  ! 0 | 4505 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4506 | `	}` |
|  165 | 4507 | `	if( PdoAttach(pThis,pConn) != 0 ){` |
|  ! 0 | 4508 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4509 | `	}` |
|    - | 4510 | `	/* the options are read BEFORE the open, so an ATTR_ERRMODE they carry is` |
|    - | 4511 | `	 * already in force for everything that follows */` |
|  165 | 4512 | `	if( nArg > 3 ){` |
|   52 | 4513 | `		PdoApplyOptions(pConn,apArg[3]);` |
|   25 | 4514 | `	}` |
|  165 | 4515 | `	rc = PdoOpenFromDsn(pCtx,pConn,nArg > 0 ? apArg[0] : 0);` |
|  165 | 4516 | `	return rc;` |
|   85 | 4517 | `}` |
|    - | 4518 | `/*` |
|    - | 4519 | ` * static PDO::connect(string $dsn, ...): static` |
|    - | 4520 | ` *` |
|    - | 4521 | `` * php 8.4's replacement for `new PDO(...)`: same arguments, but the object it`` |
|    - | 4522 | `` * answers is the DRIVER's subclass -- `Pdo\Sqlite` here -- so the`` |
|    - | 4523 | ` * sqlite-specific verbs are callable on it without a cast. Called on a` |
|    - | 4524 | `` * subclass it answers that subclass, which is what `static` means.`` |
|    - | 4525 | ` */` |
|   20 | 4526 | `static int vm_builtin_PDO_connect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 4527 | `{` |
|   23 | 4528 | `	ph7_vm *pVm = pCtx->pVm;` |
|   23 | 4529 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|    - | 4530 | `	ph7_class_instance *pObj;` |
|    - | 4531 | `	phl_pdo *pConn;` |
|    - | 4532 | `	int rc;` |
|   23 | 4533 | `	if( pClass == 0 \|\| SyStrncmp(pClass->sName.zString,"PDO",sizeof("PDO")-1) == 0 ){` |
|    - | 4534 | `		/* PDO::connect() itself answers the driver's class, not PDO */` |
|    6 | 4535 | `		ph7_class *pDrv = PH7_VmExtractClass(&(*pVm),"Pdo\\Sqlite",` |
|    - | 4536 | `			sizeof("Pdo\\Sqlite")-1,FALSE,0);` |
|    6 | 4537 | `		if( pDrv ){` |
|    6 | 4538 | `			pClass = pDrv;` |
|    2 | 4539 | `		}` |
|    2 | 4540 | `	}` |
|   23 | 4541 | `	if( pClass == 0 ){` |
|  ! 0 | 4542 | `		return PH7_VmThrowException(pCtx,"Error","Pdo\\Sqlite is not available");` |
|    - | 4543 | `	}` |
|   23 | 4544 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|   23 | 4545 | `	if( pObj == 0 ){` |
|  ! 0 | 4546 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4547 | `	}` |
|   23 | 4548 | `	pConn = PH7_PdoNewConn(&(*pVm));` |
|   23 | 4549 | `	if( pConn == 0 \|\| PdoAttach(pObj,pConn) != 0 ){` |
|  ! 0 | 4550 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 4551 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4552 | `	}` |
|   23 | 4553 | `	if( nArg > 3 ){` |
|    3 | 4554 | `		PdoApplyOptions(pConn,apArg[3]);` |
|    1 | 4555 | `	}` |
|   23 | 4556 | `	rc = PdoOpenFromDsn(pCtx,pConn,nArg > 0 ? apArg[0] : 0);` |
|   23 | 4557 | `	if( rc != PH7_OK ){` |
|    3 | 4558 | `		PH7_ClassInstanceUnref(pObj);` |
|    3 | 4559 | `		return rc;` |
|    - | 4560 | `	}` |
|   21 | 4561 | `	PH7_NativeResultObject(pCtx,pObj);` |
|   21 | 4562 | `	return PH7_OK;` |
|   13 | 4563 | `}` |
|    - | 4564 | `/*` |
|    - | 4565 | ` * The loaded-driver list, the one answer php's two spellings share.  php` |
|    - | 4566 | ` * answers the drivers its ext/pdo actually loaded, which is why an engine` |
|    - | 4567 | ` * with no driver at all answers [] -- here it is always ["sqlite"].` |
|    - | 4568 | ` */` |
|    8 | 4569 | `static int PdoDriverList(ph7_context *pCtx)` |
|    1 | 4570 | `{` |
|    - | 4571 | `	ph7_value *pArray, *pName;` |
|    9 | 4572 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 | 4573 | `	pName  = ph7_context_new_scalar(pCtx);` |
|    9 | 4574 | `	if( pArray == 0 \|\| pName == 0 ){` |
|  ! 0 | 4575 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 | 4576 | `		ph7_result_null(pCtx);` |
|  ! 0 | 4577 | `		return PH7_OK;` |
|    - | 4578 | `	}` |
|    9 | 4579 | `	ph7_value_string(pName,"sqlite",sizeof("sqlite")-1);` |
|    9 | 4580 | `	ph7_array_add_elem(pArray,0,pName);` |
|    9 | 4581 | `	ph7_result_value(pCtx,pArray);` |
|    9 | 4582 | `	return PH7_OK;` |
|    5 | 4583 | `}` |
|    - | 4584 | `/* PDO::getAvailableDrivers(): the static method spelling. */` |
|    4 | 4585 | `static int vm_builtin_PDO_getAvailableDrivers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4586 | `{` |
|    2 | 4587 | `	SXUNUSED(nArg);` |
|    2 | 4588 | `	SXUNUSED(apArg);` |
|    5 | 4589 | `	return PdoDriverList(pCtx);` |
|    1 | 4590 | `}` |
|    - | 4591 | `/*` |
|    - | 4592 | ` * pdo_drivers(): the PROCEDURAL spelling of the same list, and the only` |
|    - | 4593 | ` * FUNCTION ext/pdo declares.  php's two answers are the same array built by` |
|    - | 4594 | `` * the same C routine, so they are `===` to each other.`` |
|    - | 4595 | ` */` |
|    4 | 4596 | `static int vm_builtin_pdo_drivers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4597 | `{` |
|    2 | 4598 | `	SXUNUSED(nArg);` |
|    2 | 4599 | `	SXUNUSED(apArg);` |
|    5 | 4600 | `	return PdoDriverList(pCtx);` |
|    1 | 4601 | `}` |
|    - | 4602 |  |
|    - | 4603 | `/*` |
|    - | 4604 | ` * Install the PDO class library.  Called from PH7_VmInit inside the` |
|    - | 4605 | ` * bCompilingBuiltin window; vm_pdo_sqlite.c's installer runs right after and` |
|    - | 4606 | `` * needs PDO to already be mounted (it is the parent of `Pdo\Sqlite`).`` |
|    - | 4607 | ` */` |
| 6721 | 4608 | `PH7_PRIVATE sxi32 PH7_VmInstallPdo(ph7_vm *pVm)` |
|    5 | 4609 | `{` |
|    - | 4610 | `	/* php's own constant values, in its own declaration order. The seven` |
|    - | 4611 | `	 * deprecated PDO::SQLITE_* rows php still carries are absent by §10; their` |
|    - | 4612 | `	 * successors are declared on Pdo\Sqlite (vm_pdo_sqlite.c). */` |
|    - | 4613 | `#define PDO_INT_CONST(NAME,VALUE) \` |
|    - | 4614 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (ph7_int64)(VALUE), 0, 0.0 }` |
|    - | 4615 | `	static const PH7_NativeConstDef aPdoConst[] = {` |
|    - | 4616 | `		PDO_INT_CONST("PARAM_NULL",             0),` |
|    - | 4617 | `		PDO_INT_CONST("PARAM_BOOL",             5),` |
|    - | 4618 | `		PDO_INT_CONST("PARAM_INT",              1),` |
|    - | 4619 | `		PDO_INT_CONST("PARAM_STR",              2),` |
|    - | 4620 | `		PDO_INT_CONST("PARAM_LOB",              3),` |
|    - | 4621 | `		PDO_INT_CONST("PARAM_STMT",             4),` |
|    - | 4622 | `		PDO_INT_CONST("PARAM_INPUT_OUTPUT",     2147483648LL),` |
|    - | 4623 | `		PDO_INT_CONST("PARAM_STR_NATL",         1073741824LL),` |
|    - | 4624 | `		PDO_INT_CONST("PARAM_STR_CHAR",         536870912LL),` |
|    - | 4625 | `		PDO_INT_CONST("PARAM_EVT_ALLOC",        0),` |
|    - | 4626 | `		PDO_INT_CONST("PARAM_EVT_FREE",         1),` |
|    - | 4627 | `		PDO_INT_CONST("PARAM_EVT_EXEC_PRE",     2),` |
|    - | 4628 | `		PDO_INT_CONST("PARAM_EVT_EXEC_POST",    3),` |
|    - | 4629 | `		PDO_INT_CONST("PARAM_EVT_FETCH_PRE",    4),` |
|    - | 4630 | `		PDO_INT_CONST("PARAM_EVT_FETCH_POST",   5),` |
|    - | 4631 | `		PDO_INT_CONST("PARAM_EVT_NORMALIZE",    6),` |
|    - | 4632 | `		PDO_INT_CONST("FETCH_DEFAULT",          0),` |
|    - | 4633 | `		PDO_INT_CONST("FETCH_LAZY",             1),` |
|    - | 4634 | `		PDO_INT_CONST("FETCH_ASSOC",            2),` |
|    - | 4635 | `		PDO_INT_CONST("FETCH_NUM",              3),` |
|    - | 4636 | `		PDO_INT_CONST("FETCH_BOTH",             4),` |
|    - | 4637 | `		PDO_INT_CONST("FETCH_OBJ",              5),` |
|    - | 4638 | `		PDO_INT_CONST("FETCH_BOUND",            6),` |
|    - | 4639 | `		PDO_INT_CONST("FETCH_COLUMN",           7),` |
|    - | 4640 | `		PDO_INT_CONST("FETCH_CLASS",            8),` |
|    - | 4641 | `		PDO_INT_CONST("FETCH_INTO",             9),` |
|    - | 4642 | `		PDO_INT_CONST("FETCH_FUNC",            10),` |
|    - | 4643 | `		PDO_INT_CONST("FETCH_GROUP",           32),` |
|    - | 4644 | `		PDO_INT_CONST("FETCH_UNIQUE",          64),` |
|    - | 4645 | `		PDO_INT_CONST("FETCH_KEY_PAIR",        12),` |
|    - | 4646 | `		PDO_INT_CONST("FETCH_CLASSTYPE",      128),` |
|    - | 4647 | `		PDO_INT_CONST("FETCH_SERIALIZE",      512),` |
|    - | 4648 | `		PDO_INT_CONST("FETCH_PROPS_LATE",     256),` |
|    - | 4649 | `		PDO_INT_CONST("FETCH_NAMED",           11),` |
|    - | 4650 | `		PDO_INT_CONST("ATTR_AUTOCOMMIT",        0),` |
|    - | 4651 | `		PDO_INT_CONST("ATTR_PREFETCH",          1),` |
|    - | 4652 | `		PDO_INT_CONST("ATTR_TIMEOUT",           2),` |
|    - | 4653 | `		PDO_INT_CONST("ATTR_ERRMODE",           3),` |
|    - | 4654 | `		PDO_INT_CONST("ATTR_SERVER_VERSION",    4),` |
|    - | 4655 | `		PDO_INT_CONST("ATTR_CLIENT_VERSION",    5),` |
|    - | 4656 | `		PDO_INT_CONST("ATTR_SERVER_INFO",       6),` |
|    - | 4657 | `		PDO_INT_CONST("ATTR_CONNECTION_STATUS", 7),` |
|    - | 4658 | `		PDO_INT_CONST("ATTR_CASE",              8),` |
|    - | 4659 | `		PDO_INT_CONST("ATTR_CURSOR_NAME",       9),` |
|    - | 4660 | `		PDO_INT_CONST("ATTR_CURSOR",           10),` |
|    - | 4661 | `		PDO_INT_CONST("ATTR_ORACLE_NULLS",     11),` |
|    - | 4662 | `		PDO_INT_CONST("ATTR_PERSISTENT",       12),` |
|    - | 4663 | `		PDO_INT_CONST("ATTR_STATEMENT_CLASS",  13),` |
|    - | 4664 | `		PDO_INT_CONST("ATTR_FETCH_TABLE_NAMES",14),` |
|    - | 4665 | `		PDO_INT_CONST("ATTR_FETCH_CATALOG_NAMES",15),` |
|    - | 4666 | `		PDO_INT_CONST("ATTR_DRIVER_NAME",      16),` |
|    - | 4667 | `		PDO_INT_CONST("ATTR_STRINGIFY_FETCHES",17),` |
|    - | 4668 | `		PDO_INT_CONST("ATTR_MAX_COLUMN_LEN",   18),` |
|    - | 4669 | `		PDO_INT_CONST("ATTR_EMULATE_PREPARES", 20),` |
|    - | 4670 | `		PDO_INT_CONST("ATTR_DEFAULT_FETCH_MODE",19),` |
|    - | 4671 | `		PDO_INT_CONST("ATTR_DEFAULT_STR_PARAM",21),` |
|    - | 4672 | `		PDO_INT_CONST("ERRMODE_SILENT",         0),` |
|    - | 4673 | `		PDO_INT_CONST("ERRMODE_WARNING",        1),` |
|    - | 4674 | `		PDO_INT_CONST("ERRMODE_EXCEPTION",      2),` |
|    - | 4675 | `		PDO_INT_CONST("CASE_NATURAL",           0),` |
|    - | 4676 | `		PDO_INT_CONST("CASE_LOWER",             2),` |
|    - | 4677 | `		PDO_INT_CONST("CASE_UPPER",             1),` |
|    - | 4678 | `		PDO_INT_CONST("NULL_NATURAL",           0),` |
|    - | 4679 | `		PDO_INT_CONST("NULL_EMPTY_STRING",      1),` |
|    - | 4680 | `		PDO_INT_CONST("NULL_TO_STRING",         2),` |
|    - | 4681 | `		{ "ERR_NONE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, "00000", 0.0 },` |
|    - | 4682 | `		PDO_INT_CONST("FETCH_ORI_NEXT",         0),` |
|    - | 4683 | `		PDO_INT_CONST("FETCH_ORI_PRIOR",        1),` |
|    - | 4684 | `		PDO_INT_CONST("FETCH_ORI_FIRST",        2),` |
|    - | 4685 | `		PDO_INT_CONST("FETCH_ORI_LAST",         3),` |
|    - | 4686 | `		PDO_INT_CONST("FETCH_ORI_ABS",          4),` |
|    - | 4687 | `		PDO_INT_CONST("FETCH_ORI_REL",          5),` |
|    - | 4688 | `		PDO_INT_CONST("CURSOR_FWDONLY",         0),` |
|    - | 4689 | `		PDO_INT_CONST("CURSOR_SCROLL",          1),` |
|    - | 4690 | `	};` |
|    - | 4691 | `	/* php's own signatures and its own stub ORDER: Reflection and` |
|    - | 4692 | `	 * get_class_methods() both answer declaration order, so the two engines` |
|    - | 4693 | `	 * must list one surface. Nearly every return type is TENTATIVE in php's` |
|    - | 4694 | ``	 * stubs (the leading `@`), which is a php-visible difference from a`` |
|    - | 4695 | `	 * declared one -- getReturnType() answers null for a tentative type. */` |
|    - | 4696 | `	static const PH7_NativeMethodDef aPdoMethod[] = {` |
|    - | 4697 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|    - | 4698 | `		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",` |
|    - | 4699 | `		  0, vm_builtin_PDO___construct },` |
|    - | 4700 | `		{ "connect", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|    - | 4701 | `		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",` |
|    - | 4702 | `		  "static", vm_builtin_PDO_connect },` |
|    - | 4703 | `		{ "beginTransaction", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_beginTransaction },` |
|    - | 4704 | `		{ "commit",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_commit },` |
|    - | 4705 | `		{ "errorCode",        PH7_MOD_PUBLIC, "", "@?string", vm_builtin_PDO_errorCode },` |
|    - | 4706 | `		{ "errorInfo",        PH7_MOD_PUBLIC, "", "@array", vm_builtin_PDO_errorInfo },` |
|    - | 4707 | `		{ "exec",             PH7_MOD_PUBLIC, "string $statement", "@int\|false",` |
|    - | 4708 | `		  vm_builtin_PDO_exec },` |
|    - | 4709 | `		{ "getAttribute",     PH7_MOD_PUBLIC, "int $attribute", "@mixed",` |
|    - | 4710 | `		  vm_builtin_PDO_getAttribute },` |
|    - | 4711 | `		{ "getAvailableDrivers", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "@array",` |
|    - | 4712 | `		  vm_builtin_PDO_getAvailableDrivers },` |
|    - | 4713 | `		{ "inTransaction",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_inTransaction },` |
|    - | 4714 | `		{ "lastInsertId",     PH7_MOD_PUBLIC, "?string $name = null", "@string\|false",` |
|    - | 4715 | `		  vm_builtin_PDO_lastInsertId },` |
|    - | 4716 | `		{ "prepare",          PH7_MOD_PUBLIC, "string $query, array $options = []",` |
|    - | 4717 | `		  "@PDOStatement\|false", vm_builtin_PDO_prepare },` |
|    - | 4718 | `		{ "query",            PH7_MOD_PUBLIC,` |
|    - | 4719 | `		  "string $query, ?int $fetchMode = null, mixed ...$fetchModeArgs",` |
|    - | 4720 | `		  "@PDOStatement\|false", vm_builtin_PDO_query },` |
|    - | 4721 | `		{ "quote",            PH7_MOD_PUBLIC, "string $string, int $type = PDO::PARAM_STR",` |
|    - | 4722 | `		  "@string\|false", vm_builtin_PDO_quote },` |
|    - | 4723 | `		{ "rollBack",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_rollBack },` |
|    - | 4724 | `		{ "setAttribute",     PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",` |
|    - | 4725 | `		  vm_builtin_PDO_setAttribute },` |
|    - | 4726 | `	};` |
|    - | 4727 | `	static const PH7_NativeMethodDef aStmtMethod[] = {` |
|    - | 4728 | `		{ "bindColumn",   PH7_MOD_PUBLIC,` |
|    - | 4729 | `		  "string\|int $column, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "` |
|    - | 4730 | `		  "mixed $driverOptions = null", "@bool", vm_builtin_PDOStatement_bindColumn },` |
|    - | 4731 | `		{ "bindParam",    PH7_MOD_PUBLIC,` |
|    - | 4732 | `		  "string\|int $param, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "` |
|    - | 4733 | `		  "mixed $driverOptions = null", "@bool", vm_builtin_PDOStatement_bindParam },` |
|    - | 4734 | `		{ "bindValue",    PH7_MOD_PUBLIC,` |
|    - | 4735 | `		  "string\|int $param, mixed $value, int $type = PDO::PARAM_STR", "@bool",` |
|    - | 4736 | `		  vm_builtin_PDOStatement_bindValue },` |
|    - | 4737 | `		{ "closeCursor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDOStatement_closeCursor },` |
|    - | 4738 | `		{ "columnCount",  PH7_MOD_PUBLIC, "", "@int", vm_builtin_PDOStatement_columnCount },` |
|    - | 4739 | `		{ "debugDumpParams", PH7_MOD_PUBLIC, "", "@?bool",` |
|    - | 4740 | `		  vm_builtin_PDOStatement_debugDumpParams },` |
|    - | 4741 | `		{ "errorCode",    PH7_MOD_PUBLIC, "", "@?string", vm_builtin_PDOStatement_errorCode },` |
|    - | 4742 | `		{ "errorInfo",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_PDOStatement_errorInfo },` |
|    - | 4743 | `		{ "execute",      PH7_MOD_PUBLIC, "?array $params = null", "@bool",` |
|    - | 4744 | `		  vm_builtin_PDOStatement_execute },` |
|    - | 4745 | `		{ "fetch",        PH7_MOD_PUBLIC,` |
|    - | 4746 | `		  "int $mode = PDO::FETCH_DEFAULT, int $cursorOrientation = PDO::FETCH_ORI_NEXT, "` |
|    - | 4747 | `		  "int $cursorOffset = 0", "@mixed", vm_builtin_PDOStatement_fetch },` |
|    - | 4748 | `		{ "fetchAll",     PH7_MOD_PUBLIC, "int $mode = PDO::FETCH_DEFAULT, mixed ...$args",` |
|    - | 4749 | `		  "@array", vm_builtin_PDOStatement_fetchAll },` |
|    - | 4750 | `		{ "fetchColumn",  PH7_MOD_PUBLIC, "int $column = 0", "@mixed",` |
|    - | 4751 | `		  vm_builtin_PDOStatement_fetchColumn },` |
|    - | 4752 | `		{ "fetchObject",  PH7_MOD_PUBLIC,` |
|    - | 4753 | `		  "?string $class = 'stdClass', array $constructorArgs = []", "@object\|false",` |
|    - | 4754 | `		  vm_builtin_PDOStatement_fetchObject },` |
|    - | 4755 | `		{ "getAttribute", PH7_MOD_PUBLIC, "int $name", "@mixed",` |
|    - | 4756 | `		  vm_builtin_PDOStatement_getAttribute },` |
|    - | 4757 | `		{ "getColumnMeta",PH7_MOD_PUBLIC, "int $column", "@array\|false",` |
|    - | 4758 | `		  vm_builtin_PDOStatement_getColumnMeta },` |
|    - | 4759 | `		{ "nextRowset",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDOStatement_nextRowset },` |
|    - | 4760 | `		{ "rowCount",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_PDOStatement_rowCount },` |
|    - | 4761 | `		{ "setAttribute", PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",` |
|    - | 4762 | `		  vm_builtin_PDOStatement_setAttribute },` |
|    - | 4763 | `		{ "setFetchMode", PH7_MOD_PUBLIC, "int $mode, mixed ...$args", "@true",` |
|    - | 4764 | `		  vm_builtin_PDOStatement_setFetchMode },` |
|    - | 4765 | `		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_PDOStatement_getIterator },` |
|    - | 4766 | `	};` |
|    - | 4767 | `	/* The one property php PRESENTS on a statement: var_dump of a PDOStatement` |
|    - | 4768 | ``	 * shows `queryString` and nothing else. It is typed and has no default --`` |
|    - | 4769 | ``	 * `new PDOStatement()` (which php allows) leaves it uninitialized. */`` |
|    - | 4770 | `	static const PH7_NativePropDef aStmtProp[] = {` |
|    - | 4771 | `		{ "queryString", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|    - | 4772 | `		/* the cursor, hidden the way the connection's handle is */` |
|    - | 4773 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 4774 | `	};` |
|    - | 4775 | `	/*` |
|    - | 4776 | ``	 * PDORow declares `public string $queryString;` and holds NO property at`` |
|    - | 4777 | `	 * all: the object's whole surface is its handlers (PdoRowProp/PdoRowDim),` |
|    - | 4778 | `	 * so the declaration is marked LAZY below and nothing ever materializes it.` |
|    - | 4779 | `	 * The two engine slots beside it are hidden the way every other handle is.` |
|    - | 4780 | `	 */` |
|    - | 4781 | `	static const PH7_NativePropDef aRowProp[] = {` |
|    - | 4782 | `		{ "queryString", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|    - | 4783 | `		{ PDOROW_RES,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 4784 | `		{ PDOROW_STMT, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|    - | 4785 | `	};` |
|    - | 4786 | `	/* php redeclares Exception::$code UNTYPED here so a SQLSTATE -- a string` |
|    - | 4787 | `	 * like 'HY000' -- can live in it, and adds the driver's raw error triple. */` |
|    - | 4788 | `	static const PH7_NativePropDef aExcProp[] = {` |
|    - | 4789 | `		{ "code",      PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|    - | 4790 | `		{ "errorInfo", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?array" },` |
|    - | 4791 | `	};` |
|    - | 4792 | `	/* The connection handle: storage the class owns and NEVER presents -- php` |
|    - | 4793 | `	 * shows no property at all on a PDO, so the slot is hidden (which is what` |
|    - | 4794 | `	 * keeps it out of var_dump, (array), get_object_vars and Reflection). */` |
|    - | 4795 | `	static const PH7_NativePropDef aPdoProp[] = {` |
|    - | 4796 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 4797 | `	};` |
|    - | 4798 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|    - | 4799 | ``		/* Both handles refuse `clone` and `serialize`: php declares neither a`` |
|    - | 4800 | `		 * clone handler nor a serializer for them, so the copy would carry the` |
|    - | 4801 | `		 * same sqlite3 pointer in its hidden slot. */` |
|    - | 4802 | `		{ "PDO", 0, 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 4803 | `		  aPdoMethod, SX_ARRAYSIZE(aPdoMethod),` |
|    - | 4804 | `		  aPdoConst, SX_ARRAYSIZE(aPdoConst),` |
|    - | 4805 | `		  aPdoProp, SX_ARRAYSIZE(aPdoProp),` |
|    - | 4806 | `		  PdoInstanceRelease, 0, 0 },` |
|    - | 4807 | `		{ "PDOStatement", 0, "IteratorAggregate", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 4808 | `		  aStmtMethod, SX_ARRAYSIZE(aStmtMethod),` |
|    - | 4809 | `		  0, 0,` |
|    - | 4810 | `		  aStmtProp, SX_ARRAYSIZE(aStmtProp),` |
|    - | 4811 | `		  PdoStmtInstanceRelease, &sPdoStmtIterVtab, 0 },` |
|    - | 4812 | `		{ "PDOException", "RuntimeException", 0, 0,` |
|    - | 4813 | `		  0, 0, 0, 0,` |
|    - | 4814 | `		  aExcProp, SX_ARRAYSIZE(aExcProp),` |
|    - | 4815 | `		  0, 0, 0 },` |
|    - | 4816 | ``		/* FINAL, uncloneable, unserializable, and refusing `new` with php's own`` |
|    - | 4817 | `		 * sentence -- which is a PDOException here and an Error everywhere else,` |
|    - | 4818 | `		 * so the class carries the exception name beside the text. */` |
|    - | 4819 | `		{ "PDORow", 0, 0,` |
|    - | 4820 | `		  PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 4821 | `		  0, 0, 0, 0,` |
|    - | 4822 | `		  aRowProp, SX_ARRAYSIZE(aRowProp),` |
|    - | 4823 | `		  PdoRowInstanceRelease, 0, PdoRowPresent }` |
|    - | 4824 | `	};` |
|    - | 4825 | `#undef PDO_INT_CONST` |
|    - | 4826 | `	sxi32 rc;` |
| 6726 | 4827 | `	pVm->pPdoConns = 0;` |
|    - | 4828 | `	/* ext/pdo declares exactly one function beside its classes. */` |
| 6726 | 4829 | `	ph7_create_function(&(*pVm),"pdo_drivers",vm_builtin_pdo_drivers,0);` |
| 6726 | 4830 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
| 6726 | 4831 | `	if( rc == SXRET_OK ){` |
|    - | 4832 | `		ph7_class *pRow;` |
|    - | 4833 | `		/* php's compare handler for an opaque handle, on all three of ext/pdo's.` |
|    - | 4834 | ``		 * php gives each of them `zend_objects_not_comparable`: no two connections,`` |
|    - | 4835 | ``		 * statements or rows are ever equal, `<=>` answers the uncomparable 1 from`` |
|    - | 4836 | `` 		 * either side, and every relational spelling is false -- `$row == $row` `` |
|    - | 4837 | `		 * alone is true, and that is the engine's identity shortcut answering` |
|    - | 4838 | ``		 * before any handler. Inherited, so `class MyPdo extends PDO` and`` |
|    - | 4839 | ``		 * `Pdo\Sqlite` get it the way php's handler table does. */`` |
| 6726 | 4840 | `		PH7_NativeClassInstallCmpHook(&(*pVm),"PDO",PH7_NativeCmpOpaqueHandle);` |
| 6726 | 4841 | `		PH7_NativeClassInstallCmpHook(&(*pVm),"PDOStatement",PH7_NativeCmpOpaqueHandle);` |
| 6726 | 4842 | `		pRow = PH7_VmExtractClass(&(*pVm),"PDORow",sizeof("PDORow")-1,FALSE,0);` |
| 6726 | 4843 | `		if( pRow ){` |
| 6726 | 4844 | `			pRow->zNewRefusal = "You may not create a PDORow manually";` |
| 6726 | 4845 | `			pRow->zNewRefusalClass = "PDOException";` |
| 6726 | 4846 | `			pRow->xDim = PdoRowDim;` |
| 6726 | 4847 | `			pRow->xCmp = PH7_NativeCmpOpaqueHandle;` |
| 3356 | 4848 | `		}` |
|    - | 4849 | `		/* The declaration php makes and the object never holds: marked LAZY, and` |
|    - | 4850 | `		 * nothing materializes it -- every write to this class is refused. */` |
| 6726 | 4851 | `		PH7_NativeClassMarkLazyProps(&(*pVm),"PDORow",0);` |
| 6726 | 4852 | `		PH7_NativeClassInstallPropHook(&(*pVm),"PDORow",PdoRowProp);` |
| 3356 | 4853 | `	}` |
| 6726 | 4854 | `	return rc;` |
|    5 | 4855 | `}` |
|    - | 4856 |  |
|    - | 4857 | `#else` |
|    - | 4858 | `/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */` |
|    - | 4859 | `typedef int vm_pdo_unused;` |
|    - | 4860 | `#endif /* PH7_ENABLE_SQLITE */` |
|    - | 4861 |  |
