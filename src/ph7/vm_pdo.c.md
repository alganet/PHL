# src/ph7/vm_pdo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1812/2064 lines (87.79%)

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
|    - |   31 |  |
|    - |   32 | `/* ------------------------------------------------------------------------` |
|    - |   33 | ` * Connection lifetime` |
|    - |   34 | ` * ------------------------------------------------------------------------ */` |
|    - |   35 | `/*` |
|    - |   36 | `` * A connection is reached from its PDO object through the hidden `__res` slot`` |
|    - |   37 | ` * and is ALSO chained on the per-VM registry, because PH7 resources have no` |
|    - |   38 | ` * destructor hook: the sweep at VM reset/release is what closes a database a` |
|    - |   39 | `` * script left open.  `clone` is refused on both classes, so no second object`` |
|    - |   40 | ` * can ever reach one record.` |
|    - |   41 | ` */` |
|  146 |   42 | `PH7_PRIVATE phl_pdo * PH7_PdoNewConn(ph7_vm *pVm)` |
|    3 |   43 | `{` |
|  149 |   44 | `	phl_pdo *pConn = (phl_pdo *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo));` |
|  149 |   45 | `	if( pConn == 0 ){` |
|  ! 0 |   46 | `		return 0;` |
|    - |   47 | `	}` |
|  149 |   48 | `	SyZero(pConn,sizeof(phl_pdo));` |
|  149 |   49 | `	pConn->pVm = pVm;` |
|    - |   50 | `	/* php's defaults for a fresh sqlite handle: exceptions on, both column` |
|    - |   51 | `	 * shapes, no case folding, no null rewriting, native column types. */` |
|  149 |   52 | `	pConn->iErrMode = PDO_ERRMODE_EXCEPTION;` |
|  149 |   53 | `	pConn->iCase = PDO_CASE_NATURAL;` |
|  149 |   54 | `	pConn->iOracleNulls = PDO_NULL_NATURAL;` |
|  149 |   55 | `	pConn->iDefaultFetch = PDO_FETCH_BOTH;` |
|  149 |   56 | `	pConn->iErrState = PDO_ERR_NONE;` |
|  149 |   57 | `	pConn->pNext = (phl_pdo *)pVm->pPdoConns;` |
|  149 |   58 | `	pVm->pPdoConns = pConn;` |
|  149 |   59 | `	return pConn;` |
|   76 |   60 | `}` |
|  146 |   61 | `PH7_PRIVATE void PH7_PdoFreeConn(phl_pdo *pConn)` |
|    3 |   62 | `{` |
|    - |   63 | `	/* sqlite refuses to close a database that still has a live statement, so` |
|    - |   64 | `	 * the cursors go first. */` |
|  149 |   65 | `	PdoStmtSweep(pConn);` |
|    - |   66 | `	{` |
|    - |   67 | `		/* the callbacks sqlite still points at; the close is what makes them` |
|    - |   68 | `		 * unreachable, so they are released after it below */` |
|  149 |   69 | `		phl_pdo_udf *pUdf = pConn->pUdfs;` |
|  175 |   70 | `		while( pUdf ){` |
|   27 |   71 | `			phl_pdo_udf *pNext = pUdf->pNext;` |
|   27 |   72 | `			if( pUdf->pCallback ){` |
|   27 |   73 | `				ph7_release_value(pConn->pVm,pUdf->pCallback);` |
|   13 |   74 | `			}` |
|   27 |   75 | `			if( pUdf->pFinalize ){` |
|    3 |   76 | `				ph7_release_value(pConn->pVm,pUdf->pFinalize);` |
|    1 |   77 | `			}` |
|   27 |   78 | `			if( pUdf->zName ){` |
|   27 |   79 | `				SyMemBackendFree(&pConn->pVm->sAllocator,pUdf->zName);` |
|   13 |   80 | `			}` |
|   27 |   81 | `			SyMemBackendFree(&pConn->pVm->sAllocator,pUdf);` |
|   27 |   82 | `			pUdf = pNext;` |
|    1 |   83 | `		}` |
|  149 |   84 | `		pConn->pUdfs = 0;` |
|    - |   85 | `	}` |
|  149 |   86 | `	PH7_PdoSqliteClose(pConn);` |
|  149 |   87 | `	if( pConn->zDrvMsg ){` |
|   15 |   88 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);` |
|   15 |   89 | `		pConn->zDrvMsg = 0;` |
|    7 |   90 | `	}` |
|  149 |   91 | `	if( pConn->zStmtClass ){` |
|    6 |   92 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zStmtClass);` |
|    6 |   93 | `		pConn->zStmtClass = 0;` |
|    2 |   94 | `	}` |
|  149 |   95 | `}` |
|    - |   96 | `/*` |
|    - |   97 | ` * Free every registered connection.  Called from PH7_PdoVmReset (a reused VM --` |
|    - |   98 | ` * the -S server's -- must not answer the next request through the previous` |
|    - |   99 | ` * one's database) and from PH7_PdoVmRelease before the allocator that holds the` |
|    - |  100 | ` * shells is torn down.` |
|    - |  101 | ` */` |
| 4672 |  102 | `static void PdoVmSweep(ph7_vm *pVm)` |
|    5 |  103 | `{` |
| 4677 |  104 | `	phl_pdo *pConn = (phl_pdo *)pVm->pPdoConns;` |
| 4823 |  105 | `	while( pConn ){` |
|  149 |  106 | `		phl_pdo *pNext = pConn->pNext;` |
|  149 |  107 | `		PdoBlankSlot(pConn->pOwner);` |
|  149 |  108 | `		PH7_PdoFreeConn(pConn);` |
|  149 |  109 | `		SyMemBackendFree(&pVm->sAllocator,pConn);` |
|  149 |  110 | `		pConn = pNext;` |
|    3 |  111 | `	}` |
| 4677 |  112 | `	pVm->pPdoConns = 0;` |
| 4677 |  113 | `}` |
|   16 |  114 | `PH7_PRIVATE void PH7_PdoVmReset(ph7_vm *pVm)` |
|  ! 0 |  115 | `{` |
|   16 |  116 | `	PdoVmSweep(&(*pVm));` |
|   16 |  117 | `}` |
| 4656 |  118 | `PH7_PRIVATE void PH7_PdoVmRelease(ph7_vm *pVm)` |
|    5 |  119 | `{` |
| 4661 |  120 | `	PdoVmSweep(&(*pVm));` |
| 4661 |  121 | `}` |
|    - |  122 | `/*` |
|    - |  123 | ` * Blank the hidden slot of the object that holds a record we are about to` |
|    - |  124 | ` * free.  Without this the object outlives its record -- a PDOStatement whose` |
|    - |  125 | ` * connection was released first, or any handle alive at VM teardown -- and its` |
|    - |  126 | ` * own release reads freed memory to ask whether it still owns one. (ASan found` |
|    - |  127 | ` * exactly that; nothing in the ordinary build noticed.)` |
|    - |  128 | ` */` |
|  428 |  129 | `static void PdoBlankSlot(ph7_class_instance *pOwner)` |
|    3 |  130 | `{` |
|    - |  131 | `	SyString sAttr;` |
|    - |  132 | `	ph7_value *pRes;` |
|  431 |  133 | `	if( pOwner == 0 ){` |
|  349 |  134 | `		return;` |
|    - |  135 | `	}` |
|   85 |  136 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|   85 |  137 | `	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);` |
|   85 |  138 | `	if( pRes ){` |
|   85 |  139 | `		PH7_MemObjRelease(pRes);` |
|   85 |  140 | `		MemObjSetType(pRes,MEMOBJ_NULL);` |
|   41 |  141 | `	}` |
|  217 |  142 | `}` |
|    - |  143 | ``/* The connection behind a `__res` slot value. */`` |
|  872 |  144 | `static phl_pdo * PdoOfValue(ph7_value *pVal)` |
|    3 |  145 | `{` |
|  875 |  146 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   37 |  147 | `		return 0;` |
|    - |  148 | `	}` |
|  839 |  149 | `	return (phl_pdo *)ph7_value_to_resource(pVal);` |
|  439 |  150 | `}` |
|   34 |  151 | `PH7_PRIVATE phl_pdo * PH7_PdoConnOfInstance(ph7_class_instance *pThis)` |
|    1 |  152 | `{` |
|   35 |  153 | `	return PdoOfInstance(pThis);` |
|    1 |  154 | `}` |
|  872 |  155 | `static phl_pdo * PdoOfInstance(ph7_class_instance *pThis)` |
|    3 |  156 | `{` |
|    - |  157 | `	SyString sAttr;` |
|  875 |  158 | `	if( pThis == 0 ){` |
|  ! 0 |  159 | `		return 0;` |
|    - |  160 | `	}` |
|  875 |  161 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  875 |  162 | `	return PdoOfValue(PH7_ClassInstanceFetchAttr(pThis,&sAttr));` |
|  439 |  163 | `}` |
|    - |  164 | `/* Store one connection in the receiver's hidden slot. */` |
|  146 |  165 | `static int PdoAttach(ph7_class_instance *pThis,phl_pdo *pConn)` |
|    3 |  166 | `{` |
|    - |  167 | `	SyString sAttr;` |
|    - |  168 | `	ph7_value *pRes;` |
|  149 |  169 | `	if( pThis == 0 ){` |
|  ! 0 |  170 | `		return -1;` |
|    - |  171 | `	}` |
|  149 |  172 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  149 |  173 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  149 |  174 | `	if( pRes == 0 ){` |
|  ! 0 |  175 | `		return -1;` |
|    - |  176 | `	}` |
|  149 |  177 | `	PH7_MemObjRelease(pRes);` |
|  149 |  178 | `	pRes->x.pOther = pConn;` |
|  149 |  179 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|  149 |  180 | `	pConn->pOwner = pThis;` |
|  149 |  181 | `	return 0;` |
|   76 |  182 | `}` |
|    - |  183 | `/*` |
|    - |  184 | ` * The object is going away: close its database now rather than at VM reset, so` |
|    - |  185 | ` * a script that unsets its last reference releases the file lock there -- which` |
|    - |  186 | ` * is what php does, and what a test that unlinks the file afterwards needs.` |
|    - |  187 | ` * The shell stays on the registry (the sweep frees it) because the slot is` |
|    - |  188 | ` * still reachable while the instance is being torn down.` |
|    - |  189 | ` */` |
|  128 |  190 | `static void PdoInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    2 |  191 | `{` |
|  130 |  192 | `	phl_pdo *pConn = PdoOfInstance(pThis);` |
|   64 |  193 | `	SXUNUSED(pVm);` |
|  130 |  194 | `	if( pConn == 0 \|\| pConn->pOwner != pThis ){` |
|   37 |  195 | `		return;` |
|    - |  196 | `	}` |
|   94 |  197 | `	PdoStmtSweep(pConn);` |
|   94 |  198 | `	PH7_PdoSqliteClose(pConn);` |
|   94 |  199 | `	pConn->pOwner = 0;` |
|   66 |  200 | `}` |
|    - |  201 |  |
|    - |  202 | `/* ------------------------------------------------------------------------` |
|    - |  203 | ` * Statement lifetime` |
|    - |  204 | ` * ------------------------------------------------------------------------ */` |
|    - |  205 | `/*` |
|    - |  206 | ` * A statement is chained on its CONNECTION rather than on the VM: sqlite will` |
|    - |  207 | ` * not close a database with a live statement on it, so the connection's own` |
|    - |  208 | ` * close has to finalize them first. The object reaches it through the same` |
|    - |  209 | ` * hidden slot a connection uses.` |
|    - |  210 | ` */` |
|  282 |  211 | `PH7_PRIVATE phl_pdo_stmt * PH7_PdoNewStmt(phl_pdo *pConn)` |
|    2 |  212 | `{` |
|  284 |  213 | `	phl_pdo_stmt *pSt = (phl_pdo_stmt *)SyMemBackendAlloc(&pConn->pVm->sAllocator,` |
|    - |  214 | `		sizeof(phl_pdo_stmt));` |
|  284 |  215 | `	if( pSt == 0 ){` |
|  ! 0 |  216 | `		return 0;` |
|    - |  217 | `	}` |
|  284 |  218 | `	SyZero(pSt,sizeof(phl_pdo_stmt));` |
|  284 |  219 | `	pSt->pConn = pConn;` |
|  284 |  220 | `	pSt->iFetchMode = pConn->iDefaultFetch;` |
|  284 |  221 | `	pSt->iErrState = PDO_ERR_NONE;` |
|    - |  222 | ``	/* Retain the PDO object. `$db->query(...)` on a temporary connection hands`` |
|    - |  223 | `	 * back a statement that outlives it, and php keeps the connection alive` |
|    - |  224 | `	 * through exactly this reference -- without it the database closes while` |
|    - |  225 | `	 * the statement is still being read. */` |
|  284 |  226 | `	pSt->pConnObj = pConn->pOwner;` |
|  284 |  227 | `	if( pSt->pConnObj ){` |
|  284 |  228 | `		pSt->pConnObj->iRef++;` |
|  141 |  229 | `	}` |
|  284 |  230 | `	pSt->pNext = pConn->pStmts;` |
|  284 |  231 | `	pConn->pStmts = pSt;` |
|  284 |  232 | `	return pSt;` |
|  143 |  233 | `}` |
|    - |  234 | `/* Drop what bindValue()/bindParam() recorded. */` |
|  564 |  235 | `static void PdoBindListFree(ph7_vm *pVm,phl_pdo_bind *pB)` |
|    2 |  236 | `{` |
|  612 |  237 | `	while( pB ){` |
|   48 |  238 | `		phl_pdo_bind *pNext = pB->pNext;` |
|   48 |  239 | `		if( pB->zName ){` |
|    6 |  240 | `			SyMemBackendFree(&pVm->sAllocator,pB->zName);` |
|    2 |  241 | `		}` |
|   48 |  242 | `		if( pB->pVal ){` |
|   32 |  243 | `			ph7_release_value(pVm,pB->pVal);` |
|   15 |  244 | `		}` |
|   48 |  245 | `		SyMemBackendFree(&pVm->sAllocator,pB);` |
|   48 |  246 | `		pB = pNext;` |
|    2 |  247 | `	}` |
|  566 |  248 | `}` |
|  282 |  249 | `static void PdoBindsClear(phl_pdo_stmt *pSt)` |
|    2 |  250 | `{` |
|  284 |  251 | `	PdoBindListFree(pSt->pConn->pVm,pSt->pBinds);` |
|  284 |  252 | `	PdoBindListFree(pSt->pConn->pVm,pSt->pColBinds);` |
|  284 |  253 | `	pSt->pBinds = 0;` |
|  284 |  254 | `	pSt->pColBinds = 0;` |
|  284 |  255 | `}` |
|  282 |  256 | `PH7_PRIVATE void PH7_PdoFreeStmt(phl_pdo_stmt *pSt)` |
|    2 |  257 | `{` |
|  284 |  258 | `	PdoBindsClear(pSt);` |
|  284 |  259 | `	PdoStmtClearFetchState(pSt);` |
|  284 |  260 | `	PH7_PdoSqliteFinalize(pSt);` |
|  284 |  261 | `	if( pSt->pConnObj ){` |
|    - |  262 | `		/* drop the reference taken at creation; the connection may go now */` |
|  284 |  263 | `		ph7_class_instance *pObj = pSt->pConnObj;` |
|  284 |  264 | `		pSt->pConnObj = 0;` |
|  284 |  265 | `		PH7_ClassInstanceUnref(pObj);` |
|  141 |  266 | `	}` |
|  284 |  267 | `}` |
|    - |  268 | `/* Finalize and free every statement of one connection. */` |
|  238 |  269 | `static void PdoStmtSweep(phl_pdo *pConn)` |
|    3 |  270 | `{` |
|  241 |  271 | `	phl_pdo_stmt *pSt = pConn->pStmts;` |
|  523 |  272 | `	while( pSt ){` |
|  284 |  273 | `		phl_pdo_stmt *pNext = pSt->pNext;` |
|  284 |  274 | `		PdoBlankSlot(pSt->pOwner);` |
|  284 |  275 | `		PH7_PdoFreeStmt(pSt);` |
|  284 |  276 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pSt);` |
|  284 |  277 | `		pSt = pNext;` |
|    2 |  278 | `	}` |
|  241 |  279 | `	pConn->pStmts = 0;` |
|  241 |  280 | `}` |
|  662 |  281 | `static phl_pdo_stmt * PdoStmtOfInstance(ph7_class_instance *pThis)` |
|    2 |  282 | `{` |
|    - |  283 | `	SyString sAttr;` |
|    - |  284 | `	ph7_value *pRes;` |
|  664 |  285 | `	if( pThis == 0 ){` |
|  ! 0 |  286 | `		return 0;` |
|    - |  287 | `	}` |
|  664 |  288 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  664 |  289 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  664 |  290 | `	if( pRes == 0 \|\| !ph7_value_is_resource(pRes) ){` |
|  ! 0 |  291 | `		return 0;` |
|    - |  292 | `	}` |
|  664 |  293 | `	return (phl_pdo_stmt *)ph7_value_to_resource(pRes);` |
|  333 |  294 | `}` |
|  272 |  295 | `static int PdoStmtAttach(ph7_class_instance *pThis,phl_pdo_stmt *pSt)` |
|    2 |  296 | `{` |
|    - |  297 | `	SyString sAttr;` |
|    - |  298 | `	ph7_value *pRes;` |
|  274 |  299 | `	if( pThis == 0 ){` |
|  ! 0 |  300 | `		return -1;` |
|    - |  301 | `	}` |
|  274 |  302 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|  274 |  303 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  274 |  304 | `	if( pRes == 0 ){` |
|  ! 0 |  305 | `		return -1;` |
|    - |  306 | `	}` |
|  274 |  307 | `	PH7_MemObjRelease(pRes);` |
|  274 |  308 | `	pRes->x.pOther = pSt;` |
|  274 |  309 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|  274 |  310 | `	pSt->pOwner = pThis;` |
|  274 |  311 | `	return 0;` |
|  138 |  312 | `}` |
|    - |  313 | `/* The statement object is going away: release its cursor now, as php does. */` |
|  244 |  314 | `static void PdoStmtInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    2 |  315 | `{` |
|  246 |  316 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(pThis);` |
|  122 |  317 | `	SXUNUSED(pVm);` |
|  246 |  318 | `	if( pSt == 0 \|\| pSt->pOwner != pThis ){` |
|  ! 0 |  319 | `		return;` |
|    - |  320 | `	}` |
|  246 |  321 | `	PH7_PdoSqliteFinalize(pSt);` |
|  246 |  322 | `	pSt->pOwner = 0;` |
|  124 |  323 | `}` |
|    - |  324 |  |
|    - |  325 | `/* ------------------------------------------------------------------------` |
|    - |  326 | ` * The error surface` |
|    - |  327 | ` * ------------------------------------------------------------------------ */` |
|  652 |  328 | `PH7_PRIVATE void PH7_PdoClearError(phl_pdo *pConn)` |
|    3 |  329 | `{` |
|  655 |  330 | `	pConn->iErrState = PDO_ERR_OK;` |
|  655 |  331 | `	pConn->iDrvCode = 0;` |
|  655 |  332 | `	pConn->bNoDrvDetail = 0;` |
|  655 |  333 | `	SyMemcpy("00000",pConn->zSqlState,sizeof("00000"));` |
|  655 |  334 | `	if( pConn->zDrvMsg ){` |
|   25 |  335 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);` |
|   25 |  336 | `		pConn->zDrvMsg = 0;` |
|   12 |  337 | `	}` |
|  655 |  338 | `}` |
|    - |  339 | `/*` |
|    - |  340 | ` * php clears the handle's error at the ENTRY of most verbs -- exec, query,` |
|    - |  341 | ` * prepare, quote, lastInsertId and both attribute accessors -- so a failure is` |
|    - |  342 | ` * invisible to errorCode() as soon as any of them is called, even on a handle` |
|    - |  343 | ` * that has never run anything (NULL becomes "00000"). The verbs that do NOT` |
|    - |  344 | ` * clear are the two reporters themselves and the transaction quartet.` |
|    - |  345 | ` */` |
|  652 |  346 | `PH7_PRIVATE void PH7_PdoTouch(phl_pdo *pConn)` |
|    3 |  347 | `{` |
|  655 |  348 | `	PH7_PdoClearError(pConn);` |
|  655 |  349 | `}` |
|   66 |  350 | `PH7_PRIVATE void PH7_PdoSetError(phl_pdo *pConn,const char *zSqlState,int iCode,const char *zMsg)` |
|    1 |  351 | `{` |
|    - |  352 | `	sxu32 n;` |
|   67 |  353 | `	pConn->iErrState = PDO_ERR_FAILED;` |
|   67 |  354 | `	pConn->bNoDrvDetail = 0;` |
|   67 |  355 | `	pConn->iDrvCode = iCode;` |
|  397 |  356 | `	for( n = 0 ; n < 5 && zSqlState[n] ; ++n ){` |
|  331 |  357 | `		pConn->zSqlState[n] = zSqlState[n];` |
|  166 |  358 | `	}` |
|   67 |  359 | `	pConn->zSqlState[n] = 0;` |
|   67 |  360 | `	if( pConn->zDrvMsg ){` |
|  ! 0 |  361 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pConn->zDrvMsg);` |
|  ! 0 |  362 | `		pConn->zDrvMsg = 0;` |
|  ! 0 |  363 | `	}` |
|   67 |  364 | `	if( zMsg ){` |
|   39 |  365 | `		n = SyStrlen(zMsg);` |
|   39 |  366 | `		pConn->zDrvMsg = (char *)SyMemBackendAlloc(&pConn->pVm->sAllocator,n + 1);` |
|   39 |  367 | `		if( pConn->zDrvMsg ){` |
|   39 |  368 | `			SyMemcpy(zMsg,pConn->zDrvMsg,n);` |
|   39 |  369 | `			pConn->zDrvMsg[n] = 0;` |
|   19 |  370 | `		}` |
|   19 |  371 | `	}` |
|   67 |  372 | `}` |
|    - |  373 | `/*` |
|    - |  374 | ` * Build a PDOException the way php does: the message php prints, the SQLSTATE` |
|    - |  375 | ` * or driver code in $code, and the raw triple in $errorInfo. The generic` |
|    - |  376 | ` * exception path cannot do this -- it takes an int code and knows no extra` |
|    - |  377 | ` * property -- so the object is constructed here and thrown raw.` |
|    - |  378 | ` */` |
|   66 |  379 | `static sxi32 PdoThrowException(ph7_context *pCtx,const char *zMsg,const char *zSqlState,` |
|    - |  380 | `	int iCode,int bIntCode,const char *zDrvMsg,int nInfo)` |
|    2 |  381 | `{` |
|   68 |  382 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  383 | `	ph7_class *pClass;` |
|    - |  384 | `	ph7_class_instance *pThis;` |
|    - |  385 | `	ph7_class_method *pCons;` |
|    - |  386 | `	ph7_value sArg;` |
|    - |  387 | `	ph7_value *apArg[1];` |
|    - |  388 | `	SyString sMsgStr;` |
|    - |  389 | `	sxi32 rc;` |
|    - |  390 |  |
|   68 |  391 | `	pClass = PH7_VmExtractClass(&(*pVm),"PDOException",sizeof("PDOException")-1,TRUE,0);` |
|   68 |  392 | `	if( pClass == 0 ){` |
|  ! 0 |  393 | `		return PH7_VmThrowException(pCtx,"Error","PDOException is not available");` |
|    - |  394 | `	}` |
|   68 |  395 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|   68 |  396 | `	if( pThis == 0 ){` |
|  ! 0 |  397 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  398 | `	}` |
|   68 |  399 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   68 |  400 | `	if( pCons ){` |
|   68 |  401 | `		SyStringInitFromBuf(&sMsgStr,zMsg,SyStrlen(zMsg));` |
|   68 |  402 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|   68 |  403 | `		apArg[0] = &sArg;` |
|   68 |  404 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|   68 |  405 | `		PH7_MemObjRelease(&sArg);` |
|   33 |  406 | `	}` |
|    - |  407 | `	/* php's $code here is the SQLSTATE STRING for a statement failure and the` |
|    - |  408 | `	 * driver's own INT for a connect failure -- which is why Exception::$code` |
|    - |  409 | `	 * is redeclared untyped on this class. */` |
|   68 |  410 | `	if( bIntCode ){` |
|    9 |  411 | `		PH7_NativeSetAttrInt(&(*pVm),pThis,"code",(sxi64)iCode);` |
|    5 |  412 | `	}else{` |
|   59 |  413 | `		PH7_NativeSetAttrStr(&(*pVm),pThis,"code",zSqlState,SyStrlen(zSqlState));` |
|    - |  414 | `	}` |
|    - |  415 | `	/* A database failure reports all three cells; a layer refusal reports two,` |
|    - |  416 | `	 * because there is no driver message under it. */` |
|   68 |  417 | `	if( nInfo > 0 ){` |
|   68 |  418 | `		ph7_value *pInfo = ph7_context_new_array(pCtx);` |
|   68 |  419 | `		ph7_value *pCell = ph7_context_new_scalar(pCtx);` |
|   68 |  420 | `		if( pInfo && pCell ){` |
|   68 |  421 | `			ph7_value_string(pCell,zSqlState,(int)SyStrlen(zSqlState));` |
|   68 |  422 | `			ph7_array_add_elem(pInfo,0,pCell);` |
|   68 |  423 | `			ph7_value_int64(pCell,(ph7_int64)iCode);` |
|   68 |  424 | `			ph7_array_add_elem(pInfo,0,pCell);` |
|   68 |  425 | `			if( nInfo > 2 ){` |
|    - |  426 | `				/* the cell is an int here, so the string write resets it */` |
|   36 |  427 | `				if( zDrvMsg ){` |
|   36 |  428 | `					ph7_value_string(pCell,zDrvMsg,(int)SyStrlen(zDrvMsg));` |
|   19 |  429 | `				}else{` |
|  ! 0 |  430 | `					ph7_value_null(pCell);` |
|    - |  431 | `				}` |
|   36 |  432 | `				ph7_array_add_elem(pInfo,0,pCell);` |
|   17 |  433 | `			}` |
|   68 |  434 | `			PH7_NativeSetProp(&(*pVm),pThis,"errorInfo",sizeof("errorInfo")-1,pInfo);` |
|   33 |  435 | `		}` |
|   33 |  436 | `	}` |
|   68 |  437 | `	rc = VmThrowException(&(*pVm),pThis);` |
|   68 |  438 | `	PH7_ClassInstanceUnref(pThis);` |
|   68 |  439 | `	if( rc == SXERR_ABORT ){` |
|  ! 0 |  440 | `		pCtx->nThrowRc = PH7_ABORT;` |
|  ! 0 |  441 | `		return PH7_ABORT;` |
|    - |  442 | `	}` |
|   68 |  443 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|   68 |  444 | `	return PH7_EXCEPTION;` |
|   35 |  445 | `}` |
|    - |  446 | `/*` |
|    - |  447 | ` * A failed connect.  php never routes this one through the error mode: the` |
|    - |  448 | ` * constructor throws whatever ATTR_ERRMODE the options carried, and the` |
|    - |  449 | `` * message is `SQLSTATE[HY000] [14] unable to open database file` -- a shape no`` |
|    - |  450 | ` * other failure uses.` |
|    - |  451 | ` */` |
|    8 |  452 | `PH7_PRIVATE sxi32 PH7_PdoThrowConstruct(ph7_context *pCtx,const char *zSqlState,int iCode,` |
|    - |  453 | `	const char *zMsg)` |
|    1 |  454 | `{` |
|    - |  455 | `	SyBlob sMsg;` |
|    - |  456 | `	sxi32 rc;` |
|    9 |  457 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    9 |  458 | `	SyBlobFormat(&sMsg,"SQLSTATE[%s] [%d] %s",zSqlState,iCode,zMsg ? zMsg : "");` |
|    9 |  459 | `	SyBlobAppend(&sMsg,"",1);` |
|    9 |  460 | `	rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),zSqlState,iCode,TRUE,zMsg,3);` |
|    9 |  461 | `	SyBlobRelease(&sMsg);` |
|    9 |  462 | `	return rc;` |
|    1 |  463 | `}` |
|    - |  464 | `/*` |
|    - |  465 | ` * php's SQLSTATE-to-description table, which is what a failure message says` |
|    - |  466 | ` * BEFORE the driver's own code and text: a constraint violation reads` |
|    - |  467 | `` * `SQLSTATE[23000]: Integrity constraint violation: 19 UNIQUE constraint`` |
|    - |  468 | `` * failed: u.v`, not "General error". Only the rows this driver can actually`` |
|    - |  469 | ` * reach are here; anything else falls back to HY000's sentence, which is what` |
|    - |  470 | ` * php answers for an unlisted state too.` |
|    - |  471 | ` *` |
|    - |  472 | ` * HY000, 23000 and IM001 are probe-verified against the oracle. The remaining` |
|    - |  473 | ` * three are php's own wording for states the sqlite driver maps but cannot` |
|    - |  474 | ` * reach in practice -- SQLITE_TOOBIG needs a value past the 1 GB limit,` |
|    - |  475 | ` * SQLITE_INTERRUPT an interrupt this engine never issues -- so no probe can` |
|    - |  476 | ` * confirm them and none can contradict them either.` |
|    - |  477 | ` */` |
|   64 |  478 | `static const char * PdoStateDescription(const char *zState)` |
|    1 |  479 | `{` |
|    - |  480 | `	static const struct { const char *zState; const char *zText; } aState[] = {` |
|    - |  481 | `		{ "HY000", "General error" },` |
|    - |  482 | `		{ "23000", "Integrity constraint violation" },` |
|    - |  483 | `		{ "IM001", "Driver does not support this function" },` |
|    - |  484 | `		{ "42S02", "Base table or view not found" },` |
|    - |  485 | `		{ "22001", "String data, right truncated" },` |
|    - |  486 | `		{ "HYC00", "Optional feature not implemented" },` |
|    - |  487 | `		{ "57014", "Statement canceled" },` |
|    - |  488 | `	};` |
|    - |  489 | `	sxu32 n;` |
|  133 |  490 | `	for( n = 0 ; n < SX_ARRAYSIZE(aState) ; ++n ){` |
|  133 |  491 | `		if( SyStrncmp(zState,aState[n].zState,6) == 0 ){` |
|   65 |  492 | `			return aState[n].zText;` |
|    - |  493 | `		}` |
|   35 |  494 | `	}` |
|  ! 0 |  495 | `	return "General error";` |
|   33 |  496 | `}` |
|    - |  497 | `/*` |
|    - |  498 | ` * A failed OPERATION, routed through ATTR_ERRMODE: silent leaves the answer to` |
|    - |  499 | ` * errorCode()/errorInfo(), warning adds php's E_WARNING naming the method, and` |
|    - |  500 | ` * exception throws. The wording is one sentence in all three:` |
|    - |  501 | `` * `SQLSTATE[HY000]: General error: 1 no such column: bogus`.`` |
|    - |  502 | ` */` |
|   26 |  503 | `PH7_PRIVATE sxi32 PH7_PdoRaise(ph7_context *pCtx,phl_pdo *pConn,const char *zFn)` |
|    1 |  504 | `{` |
|    - |  505 | `	SyBlob sMsg;` |
|   27 |  506 | `	sxi32 rc = PH7_OK;` |
|   27 |  507 | `	if( pConn->iCallbackExc != 0 ){` |
|    3 |  508 | `		sxi32 rcExc = pConn->iCallbackExc;` |
|    3 |  509 | `		pConn->iCallbackExc = 0;` |
|    3 |  510 | `		return rcExc;` |
|    - |  511 | `	}` |
|   25 |  512 | `	if( pConn->iErrMode == PDO_ERRMODE_SILENT ){` |
|    9 |  513 | `		return PH7_OK;` |
|    - |  514 | `	}` |
|   17 |  515 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|   17 |  516 | `	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %d %s",pConn->zSqlState,` |
|   16 |  517 | `		PdoStateDescription(pConn->zSqlState),` |
|   16 |  518 | `		pConn->iDrvCode,pConn->zDrvMsg ? pConn->zDrvMsg : "");` |
|   17 |  519 | `	SyBlobAppend(&sMsg,"",1);` |
|   17 |  520 | `	if( pConn->iErrMode == PDO_ERRMODE_WARNING ){` |
|    3 |  521 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));` |
|    2 |  522 | `	}else{` |
|   22 |  523 | `		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),pConn->zSqlState,` |
|   14 |  524 | `			pConn->iDrvCode,FALSE,pConn->zDrvMsg,3);` |
|    - |  525 | `	}` |
|   17 |  526 | `	SyBlobRelease(&sMsg);` |
|   17 |  527 | `	return rc;` |
|   14 |  528 | `}` |
|    - |  529 | `/*` |
|    - |  530 | ` * The same two raisers, for a failure that belongs to a STATEMENT. They differ` |
|    - |  531 | ` * from the connection's only in which object's SQLSTATE the message carries --` |
|    - |  532 | ` * the driver detail under it is shared -- and in leaving the connection's own` |
|    - |  533 | `` * state alone, which is what lets `$db->errorCode()` read "00000" while`` |
|    - |  534 | `` * `$stmt->errorCode()` reports the failure.`` |
|    - |  535 | ` */` |
|   12 |  536 | `PH7_PRIVATE sxi32 PH7_PdoRaiseStmt(ph7_context *pCtx,phl_pdo_stmt *pSt,const char *zFn)` |
|    1 |  537 | `{` |
|    - |  538 | `	SyBlob sMsg;` |
|   13 |  539 | `	sxi32 rc = PH7_OK;` |
|   13 |  540 | `	phl_pdo *pConn = pSt->pConn;` |
|   13 |  541 | `	if( pConn->iCallbackExc != 0 ){` |
|    - |  542 | `		/* the step did not fail: a userland callback THREW inside it, and what` |
|    - |  543 | `		 * the script must see is that exception rather than a PDOException` |
|    - |  544 | `		 * about the statement sqlite stopped */` |
|  ! 0 |  545 | `		sxi32 rcExc = pConn->iCallbackExc;` |
|  ! 0 |  546 | `		pConn->iCallbackExc = 0;` |
|  ! 0 |  547 | `		return rcExc;` |
|    - |  548 | `	}` |
|   13 |  549 | `	if( pConn->iErrMode == PDO_ERRMODE_SILENT ){` |
|  ! 0 |  550 | `		return PH7_OK;` |
|    - |  551 | `	}` |
|   13 |  552 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|   13 |  553 | `	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %d %s",pSt->zSqlState,` |
|   12 |  554 | `		PdoStateDescription(pSt->zSqlState),pConn->iDrvCode,` |
|   12 |  555 | `		pConn->zDrvMsg ? pConn->zDrvMsg : "");` |
|   13 |  556 | `	SyBlobAppend(&sMsg,"",1);` |
|   13 |  557 | `	if( pConn->iErrMode == PDO_ERRMODE_WARNING ){` |
|  ! 0 |  558 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));` |
|  ! 0 |  559 | `	}else{` |
|   19 |  560 | `		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),pSt->zSqlState,` |
|   12 |  561 | `			pConn->iDrvCode,FALSE,pConn->zDrvMsg,3);` |
|    - |  562 | `	}` |
|   13 |  563 | `	SyBlobRelease(&sMsg);` |
|   13 |  564 | `	return rc;` |
|    7 |  565 | `}` |
|    8 |  566 | `PH7_PRIVATE sxi32 PH7_PdoRaiseImplStmt(ph7_context *pCtx,phl_pdo_stmt *pSt,const char *zFn,` |
|    - |  567 | `	const char *zSqlState,const char *zMsg)` |
|    1 |  568 | `{` |
|    - |  569 | `	SyBlob sMsg;` |
|    9 |  570 | `	sxi32 rc = PH7_OK;` |
|    9 |  571 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|   13 |  572 | `	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %s",zSqlState,` |
|    4 |  573 | `		PdoStateDescription(zSqlState),zMsg);` |
|    9 |  574 | `	SyBlobAppend(&sMsg,"",1);` |
|    9 |  575 | `	if( pSt->pConn->iErrMode == PDO_ERRMODE_EXCEPTION ){` |
|    5 |  576 | `		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),zSqlState,0,FALSE,0,2);` |
|    3 |  577 | `	}else{` |
|    5 |  578 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));` |
|    - |  579 | `	}` |
|    9 |  580 | `	SyBlobRelease(&sMsg);` |
|    9 |  581 | `	return rc;` |
|    1 |  582 | `}` |
|    - |  583 | `/*` |
|    - |  584 | ` * php's pdo_raise_impl_error: a refusal by the LAYER rather than the database` |
|    - |  585 | ` * -- asking a driver for an attribute it does not carry is the whole of it` |
|    - |  586 | ` * here. It differs from the failure above twice over: the warning fires even` |
|    - |  587 | ` * in SILENT mode, and the triple's third cell is absent, so errorInfo() is` |
|    - |  588 | ` * two cells long.` |
|    - |  589 | ` */` |
|   28 |  590 | `PH7_PRIVATE sxi32 PH7_PdoRaiseImpl(ph7_context *pCtx,phl_pdo *pConn,const char *zFn,` |
|    - |  591 | `	const char *zSqlState,const char *zMsg)` |
|    1 |  592 | `{` |
|    - |  593 | `	SyBlob sMsg;` |
|   29 |  594 | `	sxi32 rc = PH7_OK;` |
|   29 |  595 | `	if( pConn ){` |
|   29 |  596 | `		PH7_PdoSetError(pConn,zSqlState,0,0);` |
|   29 |  597 | `		pConn->bNoDrvDetail = 1;` |
|   14 |  598 | `	}` |
|   29 |  599 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|   43 |  600 | `	SyBlobFormat(&sMsg,"SQLSTATE[%s]: %s: %s",zSqlState,` |
|   14 |  601 | `		PdoStateDescription(zSqlState),zMsg);` |
|   29 |  602 | `	SyBlobAppend(&sMsg,"",1);` |
|   29 |  603 | `	if( pConn && pConn->iErrMode == PDO_ERRMODE_EXCEPTION ){` |
|   29 |  604 | `		rc = PdoThrowException(pCtx,(const char *)SyBlobData(&sMsg),zSqlState,0,FALSE,0,2);` |
|   15 |  605 | `	}else{` |
|  ! 0 |  606 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,(const char *)SyBlobData(&sMsg));` |
|    - |  607 | `	}` |
|   29 |  608 | `	SyBlobRelease(&sMsg);` |
|   29 |  609 | `	return rc;` |
|    1 |  610 | `}` |
|    - |  611 |  |
|    - |  612 | `/* ------------------------------------------------------------------------` |
|    - |  613 | ` * Attributes` |
|    - |  614 | ` * ------------------------------------------------------------------------ */` |
|    - |  615 | `/* php's PDO::ATTR_* numbering, and the two driver attributes this slice reads. */` |
|    - |  616 | `#define PDO_ATTR_AUTOCOMMIT           0` |
|    - |  617 | `#define PDO_ATTR_PREFETCH             1` |
|    - |  618 | `#define PDO_ATTR_TIMEOUT              2` |
|    - |  619 | `#define PDO_ATTR_ERRMODE              3` |
|    - |  620 | `#define PDO_ATTR_SERVER_VERSION       4` |
|    - |  621 | `#define PDO_ATTR_CLIENT_VERSION       5` |
|    - |  622 | `#define PDO_ATTR_SERVER_INFO          6` |
|    - |  623 | `#define PDO_ATTR_CONNECTION_STATUS    7` |
|    - |  624 | `#define PDO_ATTR_CASE                 8` |
|    - |  625 | `#define PDO_ATTR_CURSOR_NAME          9` |
|    - |  626 | `#define PDO_ATTR_CURSOR              10` |
|    - |  627 | `#define PDO_ATTR_ORACLE_NULLS        11` |
|    - |  628 | `#define PDO_ATTR_PERSISTENT          12` |
|    - |  629 | `#define PDO_ATTR_STATEMENT_CLASS     13` |
|    - |  630 | `#define PDO_ATTR_FETCH_TABLE_NAMES   14` |
|    - |  631 | `#define PDO_ATTR_FETCH_CATALOG_NAMES 15` |
|    - |  632 | `#define PDO_ATTR_DRIVER_NAME         16` |
|    - |  633 | `#define PDO_ATTR_STRINGIFY_FETCHES   17` |
|    - |  634 | `#define PDO_ATTR_MAX_COLUMN_LEN      18` |
|    - |  635 | `#define PDO_ATTR_DEFAULT_FETCH_MODE  19` |
|    - |  636 | `#define PDO_ATTR_EMULATE_PREPARES    20` |
|    - |  637 | `#define PDO_ATTR_DEFAULT_STR_PARAM   21` |
|    - |  638 | `#define PDO_SQLITE_ATTR_OPEN_FLAGS            1000` |
|    - |  639 | `#define PDO_SQLITE_ATTR_READONLY_STATEMENT    1001` |
|    - |  640 | `#define PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES 1002` |
|    - |  641 | `#define PDO_SQLITE_ATTR_BUSY_STATEMENT        1003` |
|    - |  642 | `#define PDO_SQLITE_ATTR_TRANSACTION_MODE 1005` |
|    - |  643 |  |
|    - |  644 | `/* One int-keyed element of an array value, or 0 when the key is absent. */` |
|  118 |  645 | `static ph7_value * PdoArrayAtInt(ph7_vm *pVm,ph7_value *pArray,sxi64 iKey)` |
|    2 |  646 | `{` |
|  120 |  647 | `	ph7_hashmap_node *pNode = 0;` |
|    - |  648 | `	ph7_value sKey;` |
|    - |  649 | `	sxi32 rc;` |
|  120 |  650 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 |  651 | `		return 0;` |
|    - |  652 | `	}` |
|  120 |  653 | `	PH7_MemObjInitFromInt(pVm,&sKey,iKey);` |
|  120 |  654 | `	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&sKey,&pNode);` |
|  120 |  655 | `	PH7_MemObjRelease(&sKey);` |
|  120 |  656 | `	if( rc != SXRET_OK \|\| pNode == 0 ){` |
|    6 |  657 | `		return 0;` |
|    - |  658 | `	}` |
|  116 |  659 | `	return (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|   61 |  660 | `}` |
|    - |  661 | `/*` |
|    - |  662 | ` * ATTR_STATEMENT_CLASS's own validation, which is four refusals deep and in` |
|    - |  663 | ` * php's own order: the value must be an array, it must carry a class name at` |
|    - |  664 | ` * index 0, that name must BE a class and must derive from PDOStatement, and` |
|    - |  665 | ` * anything at index 1 must be an array of constructor arguments. Keys past 1` |
|    - |  666 | `` * are ignored, and index 1 refuses even a null -- despite the `?array` the`` |
|    - |  667 | ` * message spells, which is php's wording rather than its test. That message` |
|    - |  668 | ` * also names the type of the WHOLE value rather than the element's, so a` |
|    - |  669 | ` * string at index 1 reports "array given"; both are reproduced.` |
|    - |  670 | ` */` |
|   10 |  671 | `static sxi32 PdoSetStatementClass(ph7_context *pCtx,phl_pdo *pConn,ph7_value *pVal)` |
|    2 |  672 | `{` |
|    - |  673 | `	ph7_value *pName,*pArgs;` |
|    - |  674 | `	ph7_class *pClass,*pBase;` |
|    - |  675 | `	const char *zName;` |
|    - |  676 | `	int nName;` |
|    - |  677 | `	char zBuf[64];` |
|   12 |  678 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    4 |  679 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  680 | `			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS value "` |
|    - |  681 | `			"must be of type array, %s given",` |
|    1 |  682 | `			VmValueGivenName(pVal,zBuf,sizeof(zBuf)));` |
|    - |  683 | `	}` |
|   10 |  684 | `	pName = PdoArrayAtInt(pCtx->pVm,pVal,0);` |
|   10 |  685 | `	if( pName == 0 ){` |
|    3 |  686 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  687 | `			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS value "` |
|    - |  688 | `			"must be an array with the format array(classname, constructor_args)");` |
|    - |  689 | `	}` |
|    8 |  690 | `	zName = 0;` |
|    8 |  691 | `	nName = 0;` |
|    8 |  692 | `	if( pName->iFlags & MEMOBJ_STRING ){` |
|    8 |  693 | `		zName = ph7_value_to_string(pName,&nName);` |
|    3 |  694 | `	}` |
|    8 |  695 | `	pClass = (zName && nName > 0)` |
|    9 |  696 | `		? PH7_VmExtractClass(pCtx->pVm,zName,(sxu32)nName,FALSE,0) : 0;` |
|    8 |  697 | `	if( pClass == 0 ){` |
|  ! 0 |  698 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  699 | `			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS class "` |
|    - |  700 | `			"must be a valid class");` |
|    - |  701 | `	}` |
|    8 |  702 | `	pBase = PH7_VmExtractClass(pCtx->pVm,"PDOStatement",sizeof("PDOStatement")-1,FALSE,0);` |
|    8 |  703 | `	if( pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|    3 |  704 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  705 | `			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS class "` |
|    - |  706 | `			"must be derived from PDOStatement");` |
|    - |  707 | `	}` |
|    6 |  708 | `	pArgs = PdoArrayAtInt(pCtx->pVm,pVal,1);` |
|    6 |  709 | `	if( pArgs != 0 && (pArgs->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 |  710 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  711 | `			"PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS "` |
|    - |  712 | `			"constructor_args must be of type ?array, array given");` |
|    - |  713 | `	}` |
|    - |  714 | `	/* remember it: every later query()/prepare() builds THIS class */` |
|    6 |  715 | `	if( pConn->zStmtClass ){` |
|  ! 0 |  716 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pConn->zStmtClass);` |
|  ! 0 |  717 | `		pConn->zStmtClass = 0;` |
|  ! 0 |  718 | `		pConn->nStmtClass = 0;` |
|  ! 0 |  719 | `	}` |
|    6 |  720 | `	pConn->zStmtClass = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nName + 1);` |
|    6 |  721 | `	if( pConn->zStmtClass ){` |
|    6 |  722 | `		SyMemcpy(zName,pConn->zStmtClass,(sxu32)nName);` |
|    6 |  723 | `		pConn->zStmtClass[nName] = 0;` |
|    6 |  724 | `		pConn->nStmtClass = nName;` |
|    2 |  725 | `	}` |
|    6 |  726 | `	return PH7_OK;` |
|    7 |  727 | `}` |
|    - |  728 | `/* php's "the driver has no such attribute" refusal, worded once. */` |
|   28 |  729 | `static sxi32 PdoNoSuchAttr(ph7_context *pCtx,phl_pdo *pConn,const char *zFn)` |
|    1 |  730 | `{` |
|   29 |  731 | `	return PH7_PdoRaiseImpl(pCtx,pConn,zFn,"IM001",` |
|    - |  732 | `		"driver does not support that attribute");` |
|    1 |  733 | `}` |
|    - |  734 | `/*` |
|    - |  735 | ` * PDO::getAttribute(int $attribute): mixed` |
|    - |  736 | ` *` |
|    - |  737 | ` * Only the attributes the sqlite driver actually carries answer; every other` |
|    - |  738 | ` * one -- including the generic PDO::ATTR_* names other drivers implement -- is` |
|    - |  739 | ` * php's IM001. The two version attributes answer the LINKED library's version,` |
|    - |  740 | ` * which is why no test may pin them.` |
|    - |  741 | ` */` |
|   78 |  742 | `static int vm_builtin_PDO_getAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  743 | `{` |
|   81 |  744 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - |  745 | `	ph7_int64 iAttr;` |
|   81 |  746 | `	if( pConn == 0 ){` |
|  ! 0 |  747 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  748 | `	}` |
|   81 |  749 | `	PH7_PdoTouch(pConn);` |
|   81 |  750 | `	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|   81 |  751 | `	switch( iAttr ){` |
|    8 |  752 | `		case PDO_ATTR_ERRMODE:            ph7_result_int(pCtx,pConn->iErrMode); break;` |
|    5 |  753 | `		case PDO_ATTR_CASE:               ph7_result_int(pCtx,pConn->iCase); break;` |
|    5 |  754 | `		case PDO_ATTR_ORACLE_NULLS:       ph7_result_int(pCtx,pConn->iOracleNulls); break;` |
|    5 |  755 | `		case PDO_ATTR_DEFAULT_FETCH_MODE: ph7_result_int(pCtx,pConn->iDefaultFetch); break;` |
|    5 |  756 | `		case PDO_ATTR_STRINGIFY_FETCHES:  ph7_result_bool(pCtx,pConn->bStringify); break;` |
|    6 |  757 | `		case PDO_ATTR_PERSISTENT:         ph7_result_bool(pCtx,pConn->bPersistent); break;` |
|   13 |  758 | `		case PDO_SQLITE_ATTR_TRANSACTION_MODE: ph7_result_int(pCtx,pConn->iTxMode); break;` |
|    - |  759 | `		/* ATTR_EXTENDED_RESULT_CODES is write-ONLY: php refuses to read it back` |
|    - |  760 | `		 * like any attribute the driver does not carry. */` |
|    1 |  761 | `		case PDO_ATTR_DRIVER_NAME:` |
|    3 |  762 | `			ph7_result_string(pCtx,"sqlite",sizeof("sqlite")-1);` |
|    3 |  763 | `			break;` |
|    3 |  764 | `		case PDO_ATTR_SERVER_VERSION:` |
|    - |  765 | `		case PDO_ATTR_CLIENT_VERSION: {` |
|    7 |  766 | `			const char *zVer = PH7_PdoSqliteLibVersion();` |
|    7 |  767 | `			ph7_result_string(pCtx,zVer,(int)SyStrlen(zVer));` |
|    7 |  768 | `			break;` |
|    - |  769 | `		}` |
|    2 |  770 | `		case PDO_ATTR_STATEMENT_CLASS: {` |
|    - |  771 | `			/* php answers the class name alone until a constructor-argument` |
|    - |  772 | `			 * array is set beside it. */` |
|    6 |  773 | `			ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    6 |  774 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    6 |  775 | `			if( pArray == 0 \|\| pName == 0 ){` |
|  ! 0 |  776 | `				return PH7_ContextMemoryError(pCtx);` |
|    - |  777 | `			}` |
|    6 |  778 | `			if( pConn->zStmtClass ){` |
|    3 |  779 | `				ph7_value_string(pName,pConn->zStmtClass,pConn->nStmtClass);` |
|    2 |  780 | `			}else{` |
|    3 |  781 | `				ph7_value_string(pName,"PDOStatement",sizeof("PDOStatement")-1);` |
|    - |  782 | `			}` |
|    6 |  783 | `			ph7_array_add_elem(pArray,0,pName);` |
|    6 |  784 | `			ph7_result_value(pCtx,pArray);` |
|    6 |  785 | `			break;` |
|    - |  786 | `		}` |
|   14 |  787 | `		default:` |
|   29 |  788 | `			ph7_result_null(pCtx);` |
|   29 |  789 | `			return PdoNoSuchAttr(pCtx,pConn,"PDO::getAttribute");` |
|    - |  790 | `	}` |
|   53 |  791 | `	return PH7_OK;` |
|   42 |  792 | `}` |
|    - |  793 | `/*` |
|    - |  794 | ` * PDO::setAttribute(int $attribute, mixed $value): bool` |
|    - |  795 | ` *` |
|    - |  796 | ` * Three outcomes, and which one an attribute takes is php's own table: the` |
|    - |  797 | ` * five the driver carries return true, the four that describe the CONNECTION` |
|    - |  798 | ` * rather than configure it (the versions, the driver name, persistence) answer` |
|    - |  799 | ` * false without a diagnostic -- as does an attribute no driver defines -- and` |
|    - |  800 | ` * the rest are the same IM001 refusal getAttribute raises.` |
|    - |  801 | ` */` |
|   48 |  802 | `static int vm_builtin_PDO_setAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  803 | `{` |
|   50 |  804 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - |  805 | `	ph7_int64 iAttr;` |
|    - |  806 | `	ph7_value *pVal;` |
|   50 |  807 | `	if( pConn == 0 ){` |
|  ! 0 |  808 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  809 | `	}` |
|   50 |  810 | `	PH7_PdoTouch(pConn);` |
|   50 |  811 | `	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|   50 |  812 | `	pVal = nArg > 1 ? apArg[1] : 0;` |
|   50 |  813 | `	switch( iAttr ){` |
|    4 |  814 | `		case PDO_ATTR_ERRMODE: {` |
|    9 |  815 | `			ph7_int64 iMode = pVal ? ph7_value_to_int64(pVal) : 0;` |
|    8 |  816 | `			if( iMode != PDO_ERRMODE_SILENT && iMode != PDO_ERRMODE_WARNING` |
|    6 |  817 | `			 && iMode != PDO_ERRMODE_EXCEPTION ){` |
|    3 |  818 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  819 | `					"PDO::setAttribute(): Argument #2 ($value) Error mode must be one of "` |
|    - |  820 | `					"the PDO::ERRMODE_* constants");` |
|    - |  821 | `			}` |
|    7 |  822 | `			pConn->iErrMode = (int)iMode;` |
|    7 |  823 | `			break;` |
|    - |  824 | `		}` |
|    2 |  825 | `		case PDO_ATTR_CASE: {` |
|    5 |  826 | `			ph7_int64 iCase = pVal ? ph7_value_to_int64(pVal) : 0;` |
|    5 |  827 | `			if( iCase != PDO_CASE_NATURAL && iCase != PDO_CASE_UPPER && iCase != PDO_CASE_LOWER ){` |
|    3 |  828 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  829 | `					"PDO::setAttribute(): Argument #2 ($value) Case folding mode must be "` |
|    - |  830 | `					"one of the PDO::CASE_* constants");` |
|    - |  831 | `			}` |
|    3 |  832 | `			pConn->iCase = (int)iCase;` |
|    3 |  833 | `			break;` |
|    - |  834 | `		}` |
|    1 |  835 | `		case PDO_ATTR_ORACLE_NULLS:` |
|    3 |  836 | `			pConn->iOracleNulls = (int)(pVal ? ph7_value_to_int64(pVal) : 0);` |
|    3 |  837 | `			break;` |
|    1 |  838 | `		case PDO_ATTR_DEFAULT_FETCH_MODE:` |
|    3 |  839 | `			pConn->iDefaultFetch = (int)(pVal ? ph7_value_to_int64(pVal) : 0);` |
|    3 |  840 | `			break;` |
|    1 |  841 | `		case PDO_ATTR_STRINGIFY_FETCHES:` |
|    3 |  842 | `			pConn->bStringify = pVal ? ph7_value_to_bool(pVal) : 0;` |
|    3 |  843 | `			break;` |
|    1 |  844 | `		case PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES:` |
|    3 |  845 | `			pConn->bExtendedCodes = pVal ? ph7_value_to_bool(pVal) : 0;` |
|    3 |  846 | `			PH7_PdoSqliteExtendedCodes(pConn,pConn->bExtendedCodes);` |
|    3 |  847 | `			break;` |
|    4 |  848 | `		case PDO_SQLITE_ATTR_TRANSACTION_MODE: {` |
|    - |  849 | `			/* only php's three modes; anything else answers false in silence */` |
|    9 |  850 | `			ph7_int64 iTx = pVal ? ph7_value_to_int64(pVal) : 0;` |
|    9 |  851 | `			if( iTx < 0 \|\| iTx > 2 ){` |
|    3 |  852 | `				ph7_result_bool(pCtx,0);` |
|    3 |  853 | `				return PH7_OK;` |
|    - |  854 | `			}` |
|    7 |  855 | `			pConn->iTxMode = (int)iTx;` |
|    7 |  856 | `			break;` |
|    - |  857 | `		}` |
|    5 |  858 | `		case PDO_ATTR_STATEMENT_CLASS: {` |
|    - |  859 | `			/* Validated now; the class is USED when a statement is built. */` |
|   12 |  860 | `			sxi32 rcSet = PdoSetStatementClass(pCtx,pConn,pVal);` |
|   12 |  861 | `			if( rcSet != PH7_OK ){` |
|    7 |  862 | `				return rcSet;` |
|    - |  863 | `			}` |
|    6 |  864 | `			break;` |
|    - |  865 | `		}` |
|    - |  866 | `		/* Read-only descriptions of the connection: php answers false and says` |
|    - |  867 | `		 * nothing at all. An attribute no driver knows lands here too. */` |
|    4 |  868 | `		case PDO_ATTR_SERVER_VERSION:` |
|    - |  869 | `		case PDO_ATTR_CLIENT_VERSION:` |
|    - |  870 | `		case PDO_ATTR_DRIVER_NAME:` |
|    - |  871 | `		case PDO_ATTR_PERSISTENT:` |
|    9 |  872 | `			ph7_result_bool(pCtx,0);` |
|    9 |  873 | `			return PH7_OK;` |
|    - |  874 | `		/* The generic attributes other drivers carry and this one does not. */` |
|  ! 0 |  875 | `		case PDO_ATTR_AUTOCOMMIT:` |
|    - |  876 | `		case PDO_ATTR_PREFETCH:` |
|    - |  877 | `		case PDO_ATTR_TIMEOUT:` |
|    - |  878 | `		case PDO_ATTR_SERVER_INFO:` |
|    - |  879 | `		case PDO_ATTR_CONNECTION_STATUS:` |
|    - |  880 | `		case PDO_ATTR_CURSOR_NAME:` |
|    - |  881 | `		case PDO_ATTR_CURSOR:` |
|    - |  882 | `		case PDO_ATTR_FETCH_TABLE_NAMES:` |
|    - |  883 | `		case PDO_ATTR_FETCH_CATALOG_NAMES:` |
|    - |  884 | `		case PDO_ATTR_MAX_COLUMN_LEN:` |
|    - |  885 | `		case PDO_ATTR_EMULATE_PREPARES:` |
|    - |  886 | `		case PDO_ATTR_DEFAULT_STR_PARAM:` |
|  ! 0 |  887 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  888 | `			return PdoNoSuchAttr(pCtx,pConn,"PDO::setAttribute");` |
|    1 |  889 | `		default:` |
|    3 |  890 | `			ph7_result_bool(pCtx,0);` |
|    3 |  891 | `			return PH7_OK;` |
|    - |  892 | `	}` |
|   28 |  893 | `	ph7_result_bool(pCtx,1);` |
|   28 |  894 | `	return PH7_OK;` |
|   26 |  895 | `}` |
|    - |  896 |  |
|    - |  897 | `/* ------------------------------------------------------------------------` |
|    - |  898 | ` * Running statements, and reporting what happened` |
|    - |  899 | ` * ------------------------------------------------------------------------ */` |
|    - |  900 | `/*` |
|    - |  901 | ` * PDO::exec(string $statement): int\|false` |
|    - |  902 | ` *` |
|    - |  903 | ` * Runs every statement the string holds and answers the number of rows the` |
|    - |  904 | ` * last one CHANGED. A statement that changes nothing -- a SELECT, a CREATE,` |
|    - |  905 | ` * whitespace, a comment -- leaves sqlite's counter alone, so exec() answers` |
|    - |  906 | ` * whatever the previous write did rather than 0; that is php's answer too,` |
|    - |  907 | ` * because php reads the same counter.` |
|    - |  908 | ` */` |
|  170 |  909 | `static int vm_builtin_PDO_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  910 | `{` |
|  173 |  911 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - |  912 | `	const char *zSql;` |
|  173 |  913 | `	int nSql = 0;   /* the length is only written when the argument IS read */` |
|    - |  914 | `	ph7_int64 nChange;` |
|  173 |  915 | `	if( pConn == 0 ){` |
|  ! 0 |  916 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  917 | `	}` |
|  173 |  918 | `	PH7_PdoTouch(pConn);` |
|  173 |  919 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|  173 |  920 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 |  921 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  922 | `			"PDO::exec(): Argument #1 ($statement) must not be empty");` |
|    - |  923 | `	}` |
|  171 |  924 | `	nChange = PH7_PdoSqliteExec(pConn,zSql,nSql);` |
|  171 |  925 | `	if( nChange < 0 ){` |
|   17 |  926 | `		ph7_result_bool(pCtx,0);` |
|   17 |  927 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::exec");` |
|    - |  928 | `	}` |
|  155 |  929 | `	ph7_result_int64(pCtx,nChange);` |
|  155 |  930 | `	return PH7_OK;` |
|   88 |  931 | `}` |
|    - |  932 | `/*` |
|    - |  933 | ` * PDO::errorCode(): ?string` |
|    - |  934 | ` *` |
|    - |  935 | ` * Three answers, not two: a handle nothing has run on yet answers NULL, one` |
|    - |  936 | ` * whose last operation succeeded answers "00000", and a failed one answers the` |
|    - |  937 | ` * SQLSTATE. errorInfo() splits the same three ways, and its FIRST cell is the` |
|    - |  938 | ` * empty string -- not null -- in the never-used case.` |
|    - |  939 | ` */` |
|   20 |  940 | `static int vm_builtin_PDO_errorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  941 | `{` |
|   21 |  942 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|   10 |  943 | `	SXUNUSED(nArg);` |
|   10 |  944 | `	SXUNUSED(apArg);` |
|   21 |  945 | `	if( pConn == 0 ){` |
|  ! 0 |  946 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - |  947 | `	}` |
|   21 |  948 | `	if( pConn->iErrState == PDO_ERR_NONE ){` |
|    3 |  949 | `		ph7_result_null(pCtx);` |
|    2 |  950 | `	}else{` |
|   19 |  951 | `		ph7_result_string(pCtx,pConn->zSqlState,(int)SyStrlen(pConn->zSqlState));` |
|    - |  952 | `	}` |
|   21 |  953 | `	return PH7_OK;` |
|   11 |  954 | `}` |
|    - |  955 | `/*` |
|    - |  956 | ` * The three cells errorInfo() answers, for a connection or a statement alike.` |
|    - |  957 | ` * The first is the OBJECT's own SQLSTATE; the other two are the DRIVER's last` |
|    - |  958 | ` * code and message, which are shared and are reported whenever the object's` |
|    - |  959 | ` * own state is not a success -- so a statement that has never run shows the` |
|    - |  960 | ` * previous statement's driver detail beside an empty state, exactly as php` |
|    - |  961 | ` * does.` |
|    - |  962 | ` */` |
|   18 |  963 | `static int PdoBuildErrorInfo(ph7_context *pCtx,phl_pdo *pConn,int iErrState,` |
|    - |  964 | `	const char *zSqlState)` |
|    1 |  965 | `{` |
|   19 |  966 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|   19 |  967 | `	ph7_value *pCell = ph7_context_new_scalar(pCtx);` |
|   19 |  968 | `	if( pArray == 0 \|\| pCell == 0 ){` |
|  ! 0 |  969 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  970 | `	}` |
|   19 |  971 | `	if( iErrState == PDO_ERR_NONE ){` |
|    3 |  972 | `		ph7_value_string(pCell,"",0);` |
|    2 |  973 | `	}else{` |
|   17 |  974 | `		ph7_value_string(pCell,zSqlState,(int)SyStrlen(zSqlState));` |
|    - |  975 | `	}` |
|   19 |  976 | `	ph7_array_add_elem(pArray,0,pCell);` |
|   19 |  977 | `	if( iErrState != PDO_ERR_OK && pConn->iDrvCode != 0 && !pConn->bNoDrvDetail ){` |
|   11 |  978 | `		ph7_value_int64(pCell,(ph7_int64)pConn->iDrvCode);` |
|   11 |  979 | `		ph7_array_add_elem(pArray,0,pCell);` |
|   11 |  980 | `		PH7_MemObjRelease(pCell);` |
|   11 |  981 | `		if( pConn->zDrvMsg ){` |
|   11 |  982 | `			ph7_value_string(pCell,pConn->zDrvMsg,(int)SyStrlen(pConn->zDrvMsg));` |
|    6 |  983 | `		}else{` |
|  ! 0 |  984 | `			ph7_value_null(pCell);` |
|    - |  985 | `		}` |
|   11 |  986 | `		ph7_array_add_elem(pArray,0,pCell);` |
|    6 |  987 | `	}else{` |
|    9 |  988 | `		ph7_value_null(pCell);` |
|    9 |  989 | `		ph7_array_add_elem(pArray,0,pCell);` |
|    9 |  990 | `		ph7_array_add_elem(pArray,0,pCell);` |
|    - |  991 | `	}` |
|   19 |  992 | `	ph7_result_value(pCtx,pArray);` |
|   19 |  993 | `	return PH7_OK;` |
|   10 |  994 | `}` |
|    - |  995 | `/* A statement's own state, moved to "the last thing succeeded". */` |
|  478 |  996 | `static void PdoStmtOk(phl_pdo_stmt *pSt)` |
|    2 |  997 | `{` |
|  480 |  998 | `	pSt->iErrState = PDO_ERR_OK;` |
|  480 |  999 | `	SyMemcpy("00000",pSt->zSqlState,sizeof("00000"));` |
|  480 | 1000 | `}` |
|    - | 1001 | `/* A statement's own state, moved to a failure. The driver detail (if any) is` |
|    - | 1002 | ` * already on the connection, where both objects read it from. */` |
|   16 | 1003 | `static void PdoStmtFailed(phl_pdo_stmt *pSt,const char *zSqlState)` |
|    1 | 1004 | `{` |
|    - | 1005 | `	sxu32 n;` |
|   17 | 1006 | `	pSt->iErrState = PDO_ERR_FAILED;` |
|   97 | 1007 | `	for( n = 0 ; n < 5 && zSqlState[n] ; ++n ){` |
|   81 | 1008 | `		pSt->zSqlState[n] = zSqlState[n];` |
|   41 | 1009 | `	}` |
|   17 | 1010 | `	pSt->zSqlState[n] = 0;` |
|   17 | 1011 | `}` |
|   16 | 1012 | `static int vm_builtin_PDO_errorInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1013 | `{` |
|   17 | 1014 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    8 | 1015 | `	SXUNUSED(nArg);` |
|    8 | 1016 | `	SXUNUSED(apArg);` |
|   17 | 1017 | `	if( pConn == 0 ){` |
|  ! 0 | 1018 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 1019 | `	}` |
|   17 | 1020 | `	return PdoBuildErrorInfo(pCtx,pConn,pConn->iErrState,pConn->zSqlState);` |
|    9 | 1021 | `}` |
|    - | 1022 | `/*` |
|    - | 1023 | ` * PDO::lastInsertId(?string $name = null): string\|false` |
|    - | 1024 | ` *` |
|    - | 1025 | ` * sqlite's rowid of the last insert, as a STRING -- php's portable answer,` |
|    - | 1026 | ` * since another driver's sequence may not fit an int. A handle that has` |
|    - | 1027 | ` * inserted nothing answers "0" rather than false, and the $name a sequence` |
|    - | 1028 | ` * driver would use is accepted and ignored here, as php accepts it.` |
|    - | 1029 | ` */` |
|   10 | 1030 | `static int vm_builtin_PDO_lastInsertId(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1031 | `{` |
|   11 | 1032 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 1033 | `	char zBuf[32];` |
|    - | 1034 | `	int nBuf;` |
|    5 | 1035 | `	SXUNUSED(nArg);` |
|    5 | 1036 | `	SXUNUSED(apArg);` |
|   11 | 1037 | `	if( pConn == 0 ){` |
|  ! 0 | 1038 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 1039 | `	}` |
|   11 | 1040 | `	PH7_PdoTouch(pConn);` |
|   11 | 1041 | `	nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%qd",PH7_PdoSqliteLastInsertId(pConn));` |
|   11 | 1042 | `	ph7_result_string(pCtx,zBuf,nBuf);` |
|   11 | 1043 | `	return PH7_OK;` |
|    6 | 1044 | `}` |
|    - | 1045 |  |
|    - | 1046 | `/* ------------------------------------------------------------------------` |
|    - | 1047 | ` * Statements: running one, and reading its rows` |
|    - | 1048 | ` * ------------------------------------------------------------------------ */` |
|    - | 1049 | `/* php's PDO::FETCH_* values. */` |
|    - | 1050 | `#define PDO_FETCH_DEFAULT 0` |
|    - | 1051 | `#define PDO_FETCH_ASSOC   2` |
|    - | 1052 | `#define PDO_FETCH_NUM     3` |
|    - | 1053 | `#define PDO_FETCH_OBJ     5` |
|    - | 1054 | `#define PDO_FETCH_NAMED  11` |
|    - | 1055 |  |
|    - | 1056 | `/*` |
|    - | 1057 | ` * php's fetch FLAGS, which ride on top of a mode. These are the values the` |
|    - | 1058 | ` * SCRIPT sees (PDO::FETCH_GROUP is 32), not the shifted ones php uses inside` |
|    - | 1059 | ` * its own C -- the modes themselves occupy the low four bits.` |
|    - | 1060 | ` */` |
|    - | 1061 | `#define PDO_FETCH_MODE_MASK   0x0F` |
|    - | 1062 | `#define PDO_FETCH_GROUP       0x20` |
|    - | 1063 | `#define PDO_FETCH_UNIQUE      0x40` |
|    - | 1064 | `#define PDO_FETCH_CLASSTYPE   0x80` |
|    - | 1065 | `#define PDO_FETCH_PROPS_LATE  0x100` |
|    - | 1066 | `#define PDO_FETCH_SERIALIZE   0x200` |
|    - | 1067 | `#define PDO_FETCH_FLAGS       (~PDO_FETCH_MODE_MASK)` |
|    - | 1068 | `#define PDO_FETCH_BOUND        6` |
|    - | 1069 | `#define PDO_FETCH_COLUMN       7` |
|    - | 1070 | `#define PDO_FETCH_CLASS        8` |
|    - | 1071 | `#define PDO_FETCH_FUNC        10` |
|    - | 1072 | `#define PDO_FETCH_KEY_PAIR    12` |
|    - | 1073 |  |
|    - | 1074 | `/*` |
|    - | 1075 | ` * php's three CLASS-only flags refuse to ride on any other mode, and the` |
|    - | 1076 | ` * refusal names all three whichever one was set. Every entry point that takes` |
|    - | 1077 | ` * a mode checks this before it counts arguments.` |
|    - | 1078 | ` */` |
|  242 | 1079 | `static sxi32 PdoCheckFetchFlags(ph7_context *pCtx,int iMode,const char *zFn)` |
|    1 | 1080 | `{` |
|  243 | 1081 | `	int iFlags = iMode & (PDO_FETCH_CLASSTYPE\|PDO_FETCH_SERIALIZE\|PDO_FETCH_PROPS_LATE);` |
|  243 | 1082 | `	if( iFlags != 0 && (iMode & PDO_FETCH_MODE_MASK) != PDO_FETCH_CLASS ){` |
|    7 | 1083 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1084 | `			"%s(): Argument #1 ($mode) cannot use PDO::FETCH_CLASSTYPE, "` |
|    - | 1085 | `			"PDO::FETCH_PROPS_LATE, or PDO::FETCH_SERIALIZE fetch flags with a fetch "` |
|    2 | 1086 | `			"mode other than PDO::FETCH_CLASS",zFn);` |
|    - | 1087 | `	}` |
|  239 | 1088 | `	return PH7_OK;` |
|  122 | 1089 | `}` |
|    - | 1090 | `#define PDO_FETCH_LAZY         1` |
|    - | 1091 | `#define PDO_FETCH_INTO         9` |
|    - | 1092 |  |
|    - | 1093 | `/* Drop whatever a previous setFetchMode() attached to the statement. */` |
|  294 | 1094 | `static void PdoStmtClearFetchState(phl_pdo_stmt *pSt)` |
|    2 | 1095 | `{` |
|  296 | 1096 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
|  296 | 1097 | `	if( pSt->zFetchClass ){` |
|    3 | 1098 | `		SyMemBackendFree(&pVm->sAllocator,pSt->zFetchClass);` |
|    3 | 1099 | `		pSt->zFetchClass = 0;` |
|    3 | 1100 | `		pSt->nFetchClass = 0;` |
|    1 | 1101 | `	}` |
|  296 | 1102 | `	if( pSt->pFetchArgs ){` |
|  ! 0 | 1103 | `		ph7_release_value(pVm,pSt->pFetchArgs);` |
|  ! 0 | 1104 | `		pSt->pFetchArgs = 0;` |
|  ! 0 | 1105 | `	}` |
|  296 | 1106 | `	if( pSt->pFetchInto ){` |
|    5 | 1107 | `		ph7_class_instance *pObj = pSt->pFetchInto;` |
|    5 | 1108 | `		pSt->pFetchInto = 0;` |
|    5 | 1109 | `		PH7_ClassInstanceUnref(pObj);` |
|    2 | 1110 | `	}` |
|  296 | 1111 | `}` |
|    - | 1112 |  |
|    - | 1113 | `/* Call a constructor with the arguments FETCH_CLASS was given, if any. */` |
|   22 | 1114 | `static void PdoCallCtor(ph7_vm *pVm,ph7_class_instance *pObj,ph7_class_method *pCons,` |
|    - | 1115 | `	ph7_value *pArgs)` |
|    1 | 1116 | `{` |
|    - | 1117 | `	ph7_value *apArg[16];` |
|   23 | 1118 | `	int nArg = 0;` |
|   23 | 1119 | `	if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) ){` |
|   11 | 1120 | `		ph7_hashmap *pMap = (ph7_hashmap *)pArgs->x.pOther;` |
|   11 | 1121 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|   11 | 1122 | `		sxu32 n,nCount = pMap->nEntry;` |
|   21 | 1123 | `		for( n = 0 ; n < nCount && pEntry && nArg < (int)SX_ARRAYSIZE(apArg) ;` |
|   11 | 1124 | `		     ++n, pEntry = pEntry->pPrev ){` |
|   11 | 1125 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|   11 | 1126 | `			if( pVal ){` |
|   11 | 1127 | `				apArg[nArg++] = pVal;` |
|    5 | 1128 | `			}` |
|    6 | 1129 | `		}` |
|    5 | 1130 | `	}` |
|   23 | 1131 | `	PH7_VmCallClassMethod(pVm,pObj,pCons,0,nArg,nArg ? apArg : 0);` |
|   23 | 1132 | `}` |
|    - | 1133 | `/*` |
|    - | 1134 | ` * A column NAME as the connection presents it: ATTR_CASE folds it, and php` |
|    - | 1135 | ` * folds the name only -- never a value, and never a positional key.` |
|    - | 1136 | ` */` |
|  596 | 1137 | `static void PdoColumnName(phl_pdo *pConn,const char *zName,SyBlob *pOut)` |
|    1 | 1138 | `{` |
|    - | 1139 | `	sxu32 n;` |
|  597 | 1140 | `	sxu32 nName = SyStrlen(zName);` |
|    - | 1141 | `	char *zBuf;` |
|  597 | 1142 | `	SyBlobReset(pOut);` |
|  597 | 1143 | `	SyBlobAppend(pOut,zName,nName);` |
|    - | 1144 | `	/* the array setter takes a C STRING and no length, so the terminator is` |
|    - | 1145 | `	 * part of the buffer and never part of the name */` |
|  597 | 1146 | `	SyBlobAppend(pOut,"",1);` |
|  597 | 1147 | `	if( pConn->iCase == PDO_CASE_NATURAL \|\| nName < 1 ){` |
|  549 | 1148 | `		return;` |
|    - | 1149 | `	}` |
|   49 | 1150 | `	zBuf = (char *)SyBlobData(pOut);` |
|  229 | 1151 | `	for( n = 0 ; n < nName ; ++n ){` |
|  361 | 1152 | `		zBuf[n] = (char)(pConn->iCase == PDO_CASE_UPPER` |
|  180 | 1153 | `			? SyToUpper(zBuf[n]) : SyToLower(zBuf[n]));` |
|   91 | 1154 | `	}` |
|  299 | 1155 | `}` |
|    - | 1156 | `/*` |
|    - | 1157 | ` * The two rewrites php applies to a fetched VALUE, in php's own order.` |
|    - | 1158 | ` *` |
|    - | 1159 | ` * ATTR_STRINGIFY_FETCHES turns everything the driver typed into a string --` |
|    - | 1160 | ` * everything except a null, which stays null. ATTR_ORACLE_NULLS is the empty` |
|    - | 1161 | ` * string and null trading places: NULL_EMPTY_STRING makes an empty string` |
|    - | 1162 | ` * null, NULL_TO_STRING makes a null the empty string. Neither touches a string` |
|    - | 1163 | ` * that merely LOOKS empty, so a single space survives both.` |
|    - | 1164 | ` */` |
|  598 | 1165 | `static void PdoApplyValueMods(phl_pdo *pConn,ph7_value *pVal)` |
|    1 | 1166 | `{` |
|  599 | 1167 | `	if( pConn->bStringify && (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|    7 | 1168 | `		int nByte = 0;` |
|    7 | 1169 | `		const char *zStr = ph7_value_to_string(pVal,&nByte);` |
|    - | 1170 | `		SyBlob sTmp;` |
|    7 | 1171 | `		SyBlobInit(&sTmp,&pConn->pVm->sAllocator);` |
|    7 | 1172 | `		SyBlobAppend(&sTmp,zStr,(sxu32)nByte);` |
|    7 | 1173 | `		PH7_MemObjRelease(pVal);` |
|    7 | 1174 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sTmp),(int)SyBlobLength(&sTmp));` |
|    7 | 1175 | `		SyBlobRelease(&sTmp);` |
|    3 | 1176 | `	}` |
|  599 | 1177 | `	if( pConn->iOracleNulls == PDO_NULL_EMPTY_STRING ){` |
|    9 | 1178 | `		if( (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) == 0 ){` |
|    3 | 1179 | `			PH7_MemObjRelease(pVal);` |
|    3 | 1180 | `			ph7_value_null(pVal);` |
|    2 | 1181 | `		}` |
|  595 | 1182 | `	}else if( pConn->iOracleNulls == PDO_NULL_TO_STRING ){` |
|    9 | 1183 | `		if( pVal->iFlags & MEMOBJ_NULL ){` |
|    3 | 1184 | `			PH7_MemObjRelease(pVal);` |
|    3 | 1185 | `			ph7_value_string(pVal,"",0);` |
|    1 | 1186 | `		}` |
|    4 | 1187 | `	}` |
|  599 | 1188 | `}` |
|    - | 1189 |  |
|    - | 1190 | `/*` |
|    - | 1191 | ` * Step the cursor once and remember what happened. php's driver does this at` |
|    - | 1192 | ` * execute() so columnCount() has an answer before anything is fetched, and the` |
|    - | 1193 | ` * row it lands on is the one the FIRST fetch() hands back.` |
|    - | 1194 | ` *` |
|    - | 1195 | ` * No diagnostic is raised here: the iterator walks through this too, and its` |
|    - | 1196 | ` * vtable is handed a VM with no call context to raise INTO. The failure is` |
|    - | 1197 | ` * recorded on the connection either way, and the callers that DO have a` |
|    - | 1198 | ` * context route it.` |
|    - | 1199 | ` */` |
|  564 | 1200 | `static int PdoStmtStep(phl_pdo_stmt *pSt)` |
|    2 | 1201 | `{` |
|  566 | 1202 | `	int rc = PH7_PdoSqliteStep(pSt);` |
|  566 | 1203 | `	if( rc < 0 ){` |
|    5 | 1204 | `		pSt->bDone = 1;` |
|    5 | 1205 | `		pSt->bRowPending = 0;` |
|    5 | 1206 | `		return -1;` |
|    - | 1207 | `	}` |
|  562 | 1208 | `	pSt->bRowPending = (rc == 1);` |
|  562 | 1209 | `	pSt->bDone = (rc == 0);` |
|  562 | 1210 | `	return rc;` |
|  284 | 1211 | `}` |
|    - | 1212 | `/*` |
|    - | 1213 | ` * Build one row in the requested shape.  Answers 0 when the cursor has nothing` |
|    - | 1214 | ` * to hand back, which is what makes fetch() answer false at the end.` |
|    - | 1215 | ` */` |
|  316 | 1216 | `static int PdoStmtRowFrom(ph7_vm *pVm,phl_pdo_stmt *pSt,int iMode,ph7_value *pOut,` |
|    - | 1217 | `	int iFirstCol)` |
|    1 | 1218 | `{` |
|    - | 1219 | `	int nCol,iCol;` |
|    - | 1220 | `	ph7_value *pCell;` |
|    - | 1221 | `	SyBlob sName;` |
|  317 | 1222 | `	if( !pSt->bRowPending ){` |
|  ! 0 | 1223 | `		return 0;` |
|    - | 1224 | `	}` |
|  317 | 1225 | `	nCol = PH7_PdoSqliteColumnCount(pSt);` |
|  317 | 1226 | `	pCell = ph7_new_scalar(pVm);` |
|  317 | 1227 | `	if( pCell == 0 ){` |
|  ! 0 | 1228 | `		return 0;` |
|    - | 1229 | `	}` |
|  317 | 1230 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|  899 | 1231 | `	for( iCol = iFirstCol ; iCol < nCol ; ++iCol ){` |
|  583 | 1232 | `		PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sName);` |
|  583 | 1233 | `		PH7_PdoSqliteColumnValue(pSt,iCol,pCell);` |
|  583 | 1234 | `		PdoApplyValueMods(pSt->pConn,pCell);` |
|    - | 1235 | `		/* FETCH_BOTH is not a third shape: it is both of the other two, so` |
|    - | 1236 | `		 * every column lands twice -- named, then positional. FETCH_OBJ builds` |
|    - | 1237 | `		 * the named shape and is converted below. */` |
|  583 | 1238 | `		if( iMode != PDO_FETCH_NUM ){` |
|  313 | 1239 | `			const char *zKey = (const char *)SyBlobData(&sName);` |
|  313 | 1240 | `			int nKey = (int)SyBlobLength(&sName) - 1;   /* less the terminator */` |
|  313 | 1241 | `			if( iMode == PDO_FETCH_NAMED ){` |
|    - | 1242 | `				/* php's answer to two columns of one name: the first stays a` |
|    - | 1243 | `				 * scalar, and a second occurrence turns the entry into a LIST` |
|    - | 1244 | `				 * of every value under that name. Every other named mode keeps` |
|    - | 1245 | `				 * the last one and drops the rest. */` |
|    7 | 1246 | `				ph7_value *pPrev = ph7_array_fetch(pOut,zKey,nKey);` |
|    7 | 1247 | `				if( pPrev == 0 ){` |
|    3 | 1248 | `					ph7_array_add_strkey_elem(pOut,zKey,pCell);` |
|    6 | 1249 | `				}else if( pPrev->iFlags & MEMOBJ_HASHMAP ){` |
|    3 | 1250 | `					ph7_array_add_elem(pPrev,0,pCell);` |
|    2 | 1251 | `				}else{` |
|    3 | 1252 | `					ph7_value *pList = ph7_new_array(pVm);` |
|    3 | 1253 | `					if( pList ){` |
|    3 | 1254 | `						ph7_array_add_elem(pList,0,pPrev);` |
|    3 | 1255 | `						ph7_array_add_elem(pList,0,pCell);` |
|    3 | 1256 | `						ph7_array_add_strkey_elem(pOut,zKey,pList);` |
|    3 | 1257 | `						ph7_release_value(pVm,pList);` |
|    1 | 1258 | `					}` |
|    - | 1259 | `				}` |
|    4 | 1260 | `			}else{` |
|  307 | 1261 | `				ph7_array_add_strkey_elem(pOut,zKey,pCell);` |
|    - | 1262 | `			}` |
|  156 | 1263 | `		}` |
|  582 | 1264 | `		if( iMode != PDO_FETCH_ASSOC && iMode != PDO_FETCH_OBJ` |
|  363 | 1265 | `		 && iMode != PDO_FETCH_NAMED ){` |
|  337 | 1266 | `			if( iMode == PDO_FETCH_NUM ){` |
|    - | 1267 | `				/* a NUM row is renumbered from 0 when a leading column was` |
|    - | 1268 | `				 * dropped; a BOTH row keeps the column's original position,` |
|    - | 1269 | `				 * which is php's own asymmetry under FETCH_GROUP */` |
|  271 | 1270 | `				ph7_array_add_elem(pOut,0,pCell);` |
|  136 | 1271 | `			}else{` |
|    - | 1272 | `				ph7_value sIdx;` |
|   67 | 1273 | `				PH7_MemObjInitFromInt(pVm,&sIdx,(sxi64)iCol);` |
|   67 | 1274 | `				ph7_array_add_elem(pOut,&sIdx,pCell);` |
|   67 | 1275 | `				PH7_MemObjRelease(&sIdx);` |
|    - | 1276 | `			}` |
|  168 | 1277 | `		}` |
|  292 | 1278 | `	}` |
|  317 | 1279 | `	SyBlobRelease(&sName);` |
|  317 | 1280 | `	ph7_release_value(pVm,pCell);` |
|  317 | 1281 | `	if( iMode == PDO_FETCH_OBJ ){` |
|    - | 1282 | `		/* php's stdClass row is the associative one cast to an object -- one` |
|    - | 1283 | ``		 * DYNAMIC property per column, which is what `(object)` builds and what`` |
|    - | 1284 | `		 * json_decode() answers for the same reason. */` |
|   15 | 1285 | `		PH7_MemObjToObject(pOut);` |
|    7 | 1286 | `	}` |
|    - | 1287 | `	/* the row is spent: the next step looks for another */` |
|  317 | 1288 | `	pSt->bRowPending = 0;` |
|  317 | 1289 | `	return 1;` |
|  159 | 1290 | `}` |
|  212 | 1291 | `static int PdoStmtRow(ph7_vm *pVm,phl_pdo_stmt *pSt,int iMode,ph7_value *pOut)` |
|    1 | 1292 | `{` |
|  213 | 1293 | `	return PdoStmtRowFrom(pVm,pSt,iMode,pOut,0);` |
|    1 | 1294 | `}` |
|    - | 1295 | `/*` |
|    - | 1296 | ` * Write one column onto an object.  A DECLARED property takes the native` |
|    - | 1297 | ` * setter, whatever its visibility -- php fills a private or protected one` |
|    - | 1298 | ` * named like a column just the same. A class that declares nothing (stdClass,` |
|    - | 1299 | ` * which is what FETCH_CLASS falls back to) gets a dynamic property instead,` |
|    - | 1300 | `` * the same one an `(object)` cast would create.`` |
|    - | 1301 | ` */` |
|   92 | 1302 | `static void PdoWriteOneProp(ph7_vm *pVm,ph7_class_instance *pObj,const char *zKey,` |
|    - | 1303 | `	int nKey,ph7_value *pVal)` |
|    1 | 1304 | `{` |
|   93 | 1305 | `	if( PH7_NativeAttr(pObj,zKey) != 0 ){` |
|   73 | 1306 | `		PH7_NativeSetProp(pVm,pObj,zKey,(sxu32)nKey,pVal);` |
|   73 | 1307 | `		return;` |
|    - | 1308 | `	}` |
|    - | 1309 | `	{` |
|    - | 1310 | `		SyString sName;` |
|    - | 1311 | `		ph7_value *pSlot;` |
|   21 | 1312 | `		SyStringInitFromBuf(&sName,zKey,nKey);` |
|   21 | 1313 | `		pSlot = PH7_ClassInstanceFetchAttr(pObj,&sName);` |
|   21 | 1314 | `		if( pSlot ){` |
|  ! 0 | 1315 | `			PH7_MemObjStore(pVal,pSlot);` |
|  ! 0 | 1316 | `			return;` |
|    - | 1317 | `		}` |
|   21 | 1318 | `		pSlot = PH7_VmCreateDynamicAttr(pVm,pObj,zKey,(sxu32)nKey,0);` |
|   21 | 1319 | `		if( pSlot ){` |
|   21 | 1320 | `			PH7_MemObjStore(pVal,pSlot);` |
|   10 | 1321 | `		}` |
|    - | 1322 | `	}` |
|   47 | 1323 | `}` |
|    - | 1324 | `/* Write every column of a row onto an object, visibility ignored -- php fills` |
|    - | 1325 | ` * a private or protected property named like a column just the same. */` |
|    6 | 1326 | `static void PdoWriteRowProps(ph7_vm *pVm,ph7_class_instance *pObj,ph7_value *pRow)` |
|    1 | 1327 | `{` |
|    - | 1328 | `	ph7_hashmap *pMap;` |
|    - | 1329 | `	ph7_hashmap_node *pEntry;` |
|    - | 1330 | `	sxu32 n,nCount;` |
|    7 | 1331 | `	if( pRow == 0 \|\| (pRow->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 1332 | `		return;` |
|    - | 1333 | `	}` |
|    7 | 1334 | `	pMap = (ph7_hashmap *)pRow->x.pOther;` |
|    7 | 1335 | `	pEntry = pMap->pFirst;` |
|    7 | 1336 | `	nCount = pMap->nEntry;` |
|   19 | 1337 | `	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 1338 | `		ph7_value sKey;` |
|   13 | 1339 | `		ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|   13 | 1340 | `		int nKey = 0;` |
|    - | 1341 | `		const char *zKey;` |
|   13 | 1342 | `		PH7_MemObjInit(pVm,&sKey);` |
|   13 | 1343 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   13 | 1344 | `		zKey = ph7_value_to_string(&sKey,&nKey);` |
|   13 | 1345 | `		if( pVal && zKey && nKey > 0 ){` |
|   13 | 1346 | `			PdoWriteOneProp(pVm,pObj,zKey,nKey,pVal);` |
|    6 | 1347 | `		}` |
|   13 | 1348 | `		PH7_MemObjRelease(&sKey);` |
|    7 | 1349 | `	}` |
|    4 | 1350 | `}` |
|    - | 1351 | `/*` |
|    - | 1352 | ` * Build one object for FETCH_CLASS / fetchObject().  php writes the columns as` |
|    - | 1353 | ` * properties and runs the constructor AFTER them, so a constructor that` |
|    - | 1354 | ` * assigns a property wins over the column of the same name -- unless` |
|    - | 1355 | ` * FETCH_PROPS_LATE reverses the order, which is the whole point of that flag.` |
|    - | 1356 | ` * The write ignores visibility: a private or protected property named like a` |
|    - | 1357 | ` * column is filled just the same, which is why this cannot go through the` |
|    - | 1358 | ` * ordinary property-store path.` |
|    - | 1359 | ` */` |
|   40 | 1360 | `static int PdoRowIntoObject(ph7_context *pCtx,phl_pdo_stmt *pSt,ph7_class *pClass,` |
|    - | 1361 | `	ph7_value *pArgs,int bPropsLate,int iFirstCol,ph7_value *pResult)` |
|    1 | 1362 | `{` |
|   41 | 1363 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1364 | `	ph7_class_instance *pObj;` |
|    - | 1365 | `	ph7_class_method *pCons;` |
|    - | 1366 | `	ph7_value *pRow;` |
|   41 | 1367 | `	int rc = 0;` |
|   41 | 1368 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|   41 | 1369 | `	if( pObj == 0 ){` |
|  ! 0 | 1370 | `		return 0;` |
|    - | 1371 | `	}` |
|   41 | 1372 | `	pRow = ph7_context_new_array(pCtx);` |
|   41 | 1373 | `	if( pRow == 0 ){` |
|  ! 0 | 1374 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 1375 | `		return 0;` |
|    - | 1376 | `	}` |
|   41 | 1377 | `	if( !PdoStmtRowFrom(pVm,pSt,PDO_FETCH_ASSOC,pRow,iFirstCol) ){` |
|  ! 0 | 1378 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 1379 | `		return 0;` |
|    - | 1380 | `	}` |
|   41 | 1381 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|   41 | 1382 | `	if( bPropsLate && pCons ){` |
|    5 | 1383 | `		PdoCallCtor(pVm,pObj,pCons,pArgs);` |
|    2 | 1384 | `	}` |
|    - | 1385 | `	{` |
|   41 | 1386 | `		ph7_hashmap *pMap = (ph7_hashmap *)pRow->x.pOther;` |
|   41 | 1387 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|   41 | 1388 | `		sxu32 n,nCount = pMap->nEntry;` |
|  121 | 1389 | `		for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 1390 | `			ph7_value sKey;` |
|   81 | 1391 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|   81 | 1392 | `			int nKey = 0;` |
|    - | 1393 | `			const char *zKey;` |
|   81 | 1394 | `			PH7_MemObjInit(pVm,&sKey);` |
|   81 | 1395 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   81 | 1396 | `			zKey = ph7_value_to_string(&sKey,&nKey);` |
|   81 | 1397 | `			if( pVal && zKey && nKey > 0 ){` |
|   81 | 1398 | `				PdoWriteOneProp(pVm,pObj,zKey,nKey,pVal);` |
|   40 | 1399 | `			}` |
|   81 | 1400 | `			PH7_MemObjRelease(&sKey);` |
|   41 | 1401 | `		}` |
|    - | 1402 | `	}` |
|   41 | 1403 | `	if( !bPropsLate && pCons ){` |
|   19 | 1404 | `		PdoCallCtor(pVm,pObj,pCons,pArgs);` |
|    9 | 1405 | `	}` |
|   41 | 1406 | `	PH7_MemObjRelease(pResult);` |
|   41 | 1407 | `	pResult->x.pOther = pObj;` |
|   41 | 1408 | `	pResult->iFlags = MEMOBJ_OBJ;` |
|   41 | 1409 | `	rc = 1;` |
|   41 | 1410 | `	return rc;` |
|   21 | 1411 | `}` |
|    - | 1412 | `/*` |
|    - | 1413 | ` * The class a FETCH_CLASS or fetchObject() names.  The two verbs word the same` |
|    - | 1414 | ` * refusal differently -- fetchAll() names the ARGUMENT POSITION, fetchObject()` |
|    - | 1415 | ` * names the class it was given -- so the caller supplies the sentence.` |
|    - | 1416 | ` */` |
|   42 | 1417 | `static ph7_class * PdoResolveFetchClass(ph7_context *pCtx,ph7_value *pName,int bObjectVerb,` |
|    - | 1418 | `	sxi32 *pRc)` |
|    1 | 1419 | `{` |
|    - | 1420 | `	ph7_class *pClass;` |
|    - | 1421 | `	const char *zName;` |
|   43 | 1422 | `	int nName = 0;` |
|   43 | 1423 | `	*pRc = PH7_OK;` |
|   43 | 1424 | `	if( pName == 0 \|\| (pName->iFlags & MEMOBJ_NULL) ){` |
|    5 | 1425 | `		return PH7_VmExtractClass(pCtx->pVm,"stdClass",sizeof("stdClass")-1,FALSE,0);` |
|    - | 1426 | `	}` |
|   39 | 1427 | `	zName = ph7_value_to_string(pName,&nName);` |
|   39 | 1428 | `	pClass = (zName && nName > 0)` |
|   57 | 1429 | `		? PH7_VmExtractClass(pCtx->pVm,zName,(sxu32)nName,TRUE,0) : 0;` |
|   39 | 1430 | `	if( pClass == 0 ){` |
|    5 | 1431 | `		if( bObjectVerb ){` |
|    4 | 1432 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1433 | `				"PDOStatement::fetchObject(): Argument #1 ($class) must be a valid "` |
|    1 | 1434 | `				"class name, %.*s given",nName,zName ? zName : "");` |
|    2 | 1435 | `		}else{` |
|    3 | 1436 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1437 | `				"PDOStatement::fetchAll(): Argument #2 must be a valid class");` |
|    - | 1438 | `		}` |
|    2 | 1439 | `	}` |
|   39 | 1440 | `	return pClass;` |
|   22 | 1441 | `}` |
|    - | 1442 | `/*` |
|    - | 1443 | ` * PDOStatement::fetchObject(?string $class = "stdClass", array $ctorArgs = []): object\|false` |
|    - | 1444 | ` *` |
|    - | 1445 | ` * FETCH_CLASS for exactly one row, with its own refusal wording.` |
|    - | 1446 | ` */` |
|   10 | 1447 | `static int vm_builtin_PDOStatement_fetchObject(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1448 | `{` |
|   11 | 1449 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 1450 | `	ph7_class *pClass;` |
|    - | 1451 | `	ph7_value sRes;` |
|    - | 1452 | `	sxi32 rc;` |
|   11 | 1453 | `	if( pSt == 0 ){` |
|  ! 0 | 1454 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 1455 | `	}` |
|   11 | 1456 | `	pClass = PdoResolveFetchClass(pCtx,nArg > 0 ? apArg[0] : 0,TRUE,&rc);` |
|   11 | 1457 | `	if( pClass == 0 ){` |
|    3 | 1458 | `		return rc;` |
|    - | 1459 | `	}` |
|    9 | 1460 | `	if( !pSt->bRowPending ){` |
|    3 | 1461 | `		PdoStmtOk(pSt);` |
|    3 | 1462 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1463 | `		return PH7_OK;` |
|    - | 1464 | `	}` |
|    7 | 1465 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    7 | 1466 | `	if( !PdoRowIntoObject(pCtx,pSt,pClass,nArg > 1 ? apArg[1] : 0,FALSE,0,&sRes) ){` |
|  ! 0 | 1467 | `		PH7_MemObjRelease(&sRes);` |
|  ! 0 | 1468 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1469 | `		return PH7_OK;` |
|    - | 1470 | `	}` |
|    7 | 1471 | `	ph7_result_value(pCtx,&sRes);` |
|    7 | 1472 | `	PH7_MemObjRelease(&sRes);` |
|    7 | 1473 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 1474 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 1475 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchObject");` |
|    - | 1476 | `	}` |
|    7 | 1477 | `	PdoStmtOk(pSt);` |
|    7 | 1478 | `	return PH7_OK;` |
|    6 | 1479 | `}` |
|    - | 1480 | `/*` |
|    - | 1481 | ` * One row as an OBJECT: FETCH_CLASS builds a new instance, FETCH_INTO fills` |
|    - | 1482 | ` * the one the script handed setFetchMode(). A fetch(FETCH_INTO) with no such` |
|    - | 1483 | ` * object is php's own "No fetch-into object specified." -- a PDOException with` |
|    - | 1484 | ` * no driver behind it.` |
|    - | 1485 | ` */` |
|   10 | 1486 | `static int PdoFetchObjectRow(ph7_context *pCtx,phl_pdo_stmt *pSt,int iMode,` |
|    - | 1487 | `	ph7_value *pClassName,ph7_value *pArgs,const char *zFn)` |
|    1 | 1488 | `{` |
|   11 | 1489 | `	int iBase = iMode & PDO_FETCH_MODE_MASK;` |
|   11 | 1490 | `	int bLate = (iMode & PDO_FETCH_PROPS_LATE) != 0;` |
|   11 | 1491 | `	int iFirst = 0;` |
|   11 | 1492 | `	ph7_class *pClass = 0;` |
|    - | 1493 | `	ph7_value sRes;` |
|    - | 1494 | `	sxi32 rc;` |
|    5 | 1495 | `	SXUNUSED(zFn);` |
|   11 | 1496 | `	if( !pSt->bRowPending ){` |
|  ! 0 | 1497 | `		PdoStmtOk(pSt);` |
|  ! 0 | 1498 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1499 | `		return PH7_OK;` |
|    - | 1500 | `	}` |
|   11 | 1501 | `	if( iBase == PDO_FETCH_INTO ){` |
|    - | 1502 | `		ph7_value *pRow;` |
|    9 | 1503 | `		if( pSt->pFetchInto == 0 ){` |
|    3 | 1504 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 1505 | `				"SQLSTATE[HY000]: General error: No fetch-into object specified.");` |
|    - | 1506 | `		}` |
|    7 | 1507 | `		pRow = ph7_context_new_array(pCtx);` |
|    7 | 1508 | `		if( pRow == 0 ){` |
|  ! 0 | 1509 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 1510 | `		}` |
|    7 | 1511 | `		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_ASSOC,pRow) ){` |
|  ! 0 | 1512 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 1513 | `			return PH7_OK;` |
|    - | 1514 | `		}` |
|    7 | 1515 | `		PdoWriteRowProps(pCtx->pVm,pSt->pFetchInto,pRow);` |
|    7 | 1516 | `		PH7_NativeResultObject(pCtx,pSt->pFetchInto);` |
|    7 | 1517 | `		pSt->pFetchInto->iRef++;   /* the result took one; the statement keeps its own */` |
|    7 | 1518 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 1519 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 1520 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 1521 | `		}` |
|    7 | 1522 | `		PdoStmtOk(pSt);` |
|    7 | 1523 | `		return PH7_OK;` |
|    - | 1524 | `	}` |
|    - | 1525 | `	/* FETCH_CLASS: the class comes from this call or from setFetchMode() */` |
|    3 | 1526 | `	if( pClassName ){` |
|  ! 0 | 1527 | `		pClass = PdoResolveFetchClass(pCtx,pClassName,FALSE,&rc);` |
|  ! 0 | 1528 | `		if( pClass == 0 ){` |
|  ! 0 | 1529 | `			return rc;` |
|  ! 0 | 1530 | `		}` |
|    3 | 1531 | `	}else if( iMode & PDO_FETCH_CLASSTYPE ){` |
|    - | 1532 | `		/* the FIRST column names the class, and leaves the row */` |
|  ! 0 | 1533 | `		ph7_value *pHead = ph7_context_new_array(pCtx);` |
|    - | 1534 | `		ph7_value *pName;` |
|  ! 0 | 1535 | `		if( pHead == 0 ){` |
|  ! 0 | 1536 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 1537 | `		}` |
|  ! 0 | 1538 | `		if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pHead,0) ){` |
|  ! 0 | 1539 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 1540 | `			return PH7_OK;` |
|    - | 1541 | `		}` |
|  ! 0 | 1542 | `		pSt->bRowPending = 1;   /* the cursor has not moved */` |
|  ! 0 | 1543 | `		pName = PdoArrayAtInt(pCtx->pVm,pHead,0);` |
|  ! 0 | 1544 | `		pClass = PdoResolveFetchClass(pCtx,pName,FALSE,&rc);` |
|  ! 0 | 1545 | `		if( pClass == 0 ){` |
|  ! 0 | 1546 | `			return rc;` |
|    - | 1547 | `		}` |
|  ! 0 | 1548 | `		iFirst = 1;` |
|    3 | 1549 | `	}else if( pSt->zFetchClass ){` |
|    - | 1550 | `		ph7_value sName;` |
|    - | 1551 | `		SyString sStr;` |
|    3 | 1552 | `		SyStringInitFromBuf(&sStr,pSt->zFetchClass,pSt->nFetchClass);` |
|    3 | 1553 | `		PH7_MemObjInitFromString(pCtx->pVm,&sName,&sStr);` |
|    3 | 1554 | `		pClass = PdoResolveFetchClass(pCtx,&sName,FALSE,&rc);` |
|    3 | 1555 | `		PH7_MemObjRelease(&sName);` |
|    3 | 1556 | `		if( pClass == 0 ){` |
|  ! 0 | 1557 | `			return rc;` |
|    - | 1558 | `		}` |
|    3 | 1559 | `		if( pArgs == 0 ){` |
|    3 | 1560 | `			pArgs = pSt->pFetchArgs;` |
|    1 | 1561 | `		}` |
|    2 | 1562 | `	}else{` |
|  ! 0 | 1563 | `		pClass = PH7_VmExtractClass(pCtx->pVm,"stdClass",sizeof("stdClass")-1,FALSE,0);` |
|    - | 1564 | `	}` |
|    3 | 1565 | `	if( pClass == 0 ){` |
|  ! 0 | 1566 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1567 | `	}` |
|    3 | 1568 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    3 | 1569 | `	if( !PdoRowIntoObject(pCtx,pSt,pClass,pArgs,bLate,iFirst,&sRes) ){` |
|  ! 0 | 1570 | `		PH7_MemObjRelease(&sRes);` |
|  ! 0 | 1571 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1572 | `		return PH7_OK;` |
|    - | 1573 | `	}` |
|    3 | 1574 | `	ph7_result_value(pCtx,&sRes);` |
|    3 | 1575 | `	PH7_MemObjRelease(&sRes);` |
|    3 | 1576 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 1577 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 1578 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 1579 | `	}` |
|    3 | 1580 | `	PdoStmtOk(pSt);` |
|    3 | 1581 | `	return PH7_OK;` |
|    6 | 1582 | `}` |
|    - | 1583 | `/*` |
|    - | 1584 | ` * PDOStatement::bindColumn(string\|int $column, mixed &$var, int $type = PDO::PARAM_STR, ...): bool` |
|    - | 1585 | ` *` |
|    - | 1586 | ` * Attach a variable to a column, to be written on every FETCH_BOUND fetch.` |
|    - | 1587 | ` * The column is 1-based like a parameter, and a NAME is resolved NOW against` |
|    - | 1588 | ` * the statement's columns -- a name that is not there warns immediately (in` |
|    - | 1589 | ` * every error mode, the way a layer refusal does) and is simply not bound,` |
|    - | 1590 | ` * while an out-of-range INDEX is accepted here and refused by the fetch.` |
|    - | 1591 | ` */` |
|   20 | 1592 | `static int vm_builtin_PDOStatement_bindColumn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1593 | `{` |
|   21 | 1594 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 1595 | `	phl_pdo_bind *pB;` |
|    - | 1596 | `	ph7_value *pKey;` |
|   21 | 1597 | `	int iPos = 0,iType;` |
|   21 | 1598 | `	if( pSt == 0 ){` |
|  ! 0 | 1599 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 1600 | `	}` |
|   21 | 1601 | `	pKey = nArg > 0 ? apArg[0] : 0;` |
|   21 | 1602 | `	iType = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : PDO_PARAM_STR;` |
|   22 | 1603 | `	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){` |
|    7 | 1604 | `		int nName = 0,iCol,nCol;` |
|    7 | 1605 | `		const char *zName = ph7_value_to_string(pKey,&nName);` |
|    - | 1606 | `		SyBlob sName;` |
|    7 | 1607 | `		iPos = 0;` |
|    7 | 1608 | `		nCol = PH7_PdoSqliteColumnCount(pSt);` |
|    7 | 1609 | `		SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|   15 | 1610 | `		for( iCol = 0 ; iCol < nCol ; ++iCol ){` |
|   11 | 1611 | `			PdoColumnName(pSt->pConn,PH7_PdoSqliteColumnName(pSt,iCol),&sName);` |
|   10 | 1612 | `			if( (int)SyBlobLength(&sName) - 1 == nName` |
|    8 | 1613 | `			 && SyMemcmp(SyBlobData(&sName),zName,(sxu32)nName) == 0 ){` |
|    3 | 1614 | `				iPos = iCol + 1;` |
|    3 | 1615 | `				break;` |
|    - | 1616 | `			}` |
|    5 | 1617 | `		}` |
|    7 | 1618 | `		SyBlobRelease(&sName);` |
|    7 | 1619 | `		if( iPos == 0 ){` |
|    - | 1620 | `			/* php routes this one through the error mode like any layer refusal:` |
|    - | 1621 | `			 * a warning in silent and warning modes, a throw in exception mode --` |
|    - | 1622 | `			 * and the column is simply not bound either way. */` |
|    - | 1623 | `			SyBlob sMsg;` |
|    - | 1624 | `			sxi32 rcWarn;` |
|    5 | 1625 | `			ph7_result_bool(pCtx,1);` |
|    5 | 1626 | `			SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    5 | 1627 | `			SyBlobFormat(&sMsg,"Did not find column name '%.*s' in the defined "` |
|    2 | 1628 | `				"columns; it will not be bound",nName,zName ? zName : "");` |
|    5 | 1629 | `			SyBlobAppend(&sMsg,"",1);` |
|    7 | 1630 | `			rcWarn = PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::bindColumn","HY000",` |
|    4 | 1631 | `				(const char *)SyBlobData(&sMsg));` |
|    5 | 1632 | `			SyBlobRelease(&sMsg);` |
|    5 | 1633 | `			return rcWarn;` |
|    - | 1634 | `		}` |
|    2 | 1635 | `	}else{` |
|   15 | 1636 | `		ph7_int64 iWant = pKey ? ph7_value_to_int64(pKey) : 0;` |
|   15 | 1637 | `		if( iWant < 1 ){` |
|    3 | 1638 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1639 | `				"PDOStatement::bindColumn(): Argument #1 ($column) must be greater "` |
|    - | 1640 | `				"than or equal to 1");` |
|    - | 1641 | `		}` |
|   13 | 1642 | `		iPos = (int)iWant;` |
|    - | 1643 | `	}` |
|   15 | 1644 | `	pB = (phl_pdo_bind *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_pdo_bind));` |
|   15 | 1645 | `	if( pB == 0 ){` |
|  ! 0 | 1646 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1647 | `	}` |
|   15 | 1648 | `	SyZero(pB,sizeof(phl_pdo_bind));` |
|   15 | 1649 | `	pB->iPos = iPos;` |
|   15 | 1650 | `	pB->iType = iType;` |
|   15 | 1651 | `	pB->nSlot = (nArg > 1 && apArg[1]) ? apArg[1]->nIdx : SXU32_HIGH;` |
|   15 | 1652 | `	pB->pNext = pSt->pColBinds;` |
|   15 | 1653 | `	pSt->pColBinds = pB;` |
|   15 | 1654 | `	ph7_result_bool(pCtx,1);` |
|   15 | 1655 | `	return PH7_OK;` |
|   11 | 1656 | `}` |
|    - | 1657 | `/*` |
|    - | 1658 | ` * Write every bound column of the row at the cursor into the variables` |
|    - | 1659 | ` * bindColumn() named.  The value takes the bound TYPE, so an unqualified` |
|    - | 1660 | ` * binding hands back a string where the row itself would have held an int.` |
|    - | 1661 | ` */` |
|   24 | 1662 | `static sxi32 PdoWriteBoundColumns(ph7_context *pCtx,phl_pdo_stmt *pSt)` |
|    1 | 1663 | `{` |
|    - | 1664 | `	phl_pdo_bind *pB;` |
|   25 | 1665 | `	int nCol = PH7_PdoSqliteColumnCount(pSt);` |
|   41 | 1666 | `	for( pB = pSt->pColBinds ; pB ; pB = pB->pNext ){` |
|    - | 1667 | `		ph7_value *pSlot;` |
|    - | 1668 | `		ph7_value sVal;` |
|   19 | 1669 | `		if( pB->iPos < 1 \|\| pB->iPos > nCol ){` |
|    3 | 1670 | `			return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    - | 1671 | `		}` |
|   17 | 1672 | `		if( pB->nSlot == SXU32_HIGH ){` |
|  ! 0 | 1673 | `			continue;` |
|    - | 1674 | `		}` |
|   17 | 1675 | `		pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pB->nSlot);` |
|   17 | 1676 | `		if( pSlot == 0 ){` |
|  ! 0 | 1677 | `			continue;` |
|    - | 1678 | `		}` |
|   17 | 1679 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|   17 | 1680 | `		PH7_PdoSqliteColumnValue(pSt,pB->iPos - 1,&sVal);` |
|   17 | 1681 | `		PdoApplyValueMods(pSt->pConn,&sVal);` |
|   17 | 1682 | `		if( (sVal.iFlags & MEMOBJ_NULL) == 0 ){` |
|   17 | 1683 | `			switch( pB->iType & ~PDO_PARAM_FLAGS ){` |
|    3 | 1684 | `				case PDO_PARAM_INT:  PH7_MemObjToInteger(&sVal); break;` |
|  ! 0 | 1685 | `				case PDO_PARAM_BOOL: PH7_MemObjToBool(&sVal); break;` |
|  ! 0 | 1686 | `				case PDO_PARAM_LOB:  break;` |
|   15 | 1687 | `				default:             PH7_MemObjToString(&sVal); break;` |
|    - | 1688 | `			}` |
|    8 | 1689 | `		}` |
|   17 | 1690 | `		PH7_MemObjStore(&sVal,pSlot);` |
|   17 | 1691 | `		PH7_MemObjRelease(&sVal);` |
|    9 | 1692 | `	}` |
|   23 | 1693 | `	return PH7_OK;` |
|   13 | 1694 | `}` |
|    - | 1695 | `/*` |
|    - | 1696 | ` * PDOStatement::fetch(int $mode = PDO::FETCH_DEFAULT, ...): mixed` |
|    - | 1697 | ` *` |
|    - | 1698 | ` * FETCH_DEFAULT means the connection's ATTR_DEFAULT_FETCH_MODE, which is` |
|    - | 1699 | ` * FETCH_BOTH unless the script changed it -- so a bare fetch() answers every` |
|    - | 1700 | ` * column twice, once under its name and once under its position.` |
|    - | 1701 | ` */` |
|  142 | 1702 | `static int vm_builtin_PDOStatement_fetch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1703 | `{` |
|  143 | 1704 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 1705 | `	ph7_value *pRow;` |
|    - | 1706 | `	int iMode;` |
|    - | 1707 | `	sxi32 rcFlags;` |
|  143 | 1708 | `	if( pSt == 0 ){` |
|  ! 0 | 1709 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 1710 | `	}` |
|  143 | 1711 | `	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;` |
|  143 | 1712 | `	rcFlags = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::fetch");` |
|  143 | 1713 | `	if( rcFlags != PH7_OK ){` |
|  ! 0 | 1714 | `		return rcFlags;` |
|    - | 1715 | `	}` |
|  143 | 1716 | `	if( iMode == PDO_FETCH_DEFAULT ){` |
|   25 | 1717 | `		iMode = pSt->iFetchMode;` |
|   12 | 1718 | `	}` |
|  143 | 1719 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_COLUMN ){` |
|    - | 1720 | `		/* a statement told to fetch one COLUMN answers that column from here` |
|    - | 1721 | `		 * on, whichever verb asks for the row */` |
|    - | 1722 | `		ph7_value *pOneRow,*pOne;` |
|    3 | 1723 | `		if( !pSt->bRowPending ){` |
|  ! 0 | 1724 | `			PdoStmtOk(pSt);` |
|  ! 0 | 1725 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 1726 | `			return PH7_OK;` |
|    - | 1727 | `		}` |
|    3 | 1728 | `		pOneRow = ph7_context_new_array(pCtx);` |
|    3 | 1729 | `		if( pOneRow == 0 ){` |
|  ! 0 | 1730 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 1731 | `		}` |
|    3 | 1732 | `		if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pOneRow) ){` |
|  ! 0 | 1733 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 1734 | `			return PH7_OK;` |
|    - | 1735 | `		}` |
|    3 | 1736 | `		pOne = PdoArrayAtInt(pCtx->pVm,pOneRow,(sxi64)pSt->iFetchColumn);` |
|    3 | 1737 | `		if( pOne ){` |
|    3 | 1738 | `			ph7_result_value(pCtx,pOne);` |
|    2 | 1739 | `		}else{` |
|  ! 0 | 1740 | `			ph7_result_null(pCtx);` |
|    - | 1741 | `		}` |
|    3 | 1742 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 1743 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 1744 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 1745 | `		}` |
|    3 | 1746 | `		PdoStmtOk(pSt);` |
|    3 | 1747 | `		return PH7_OK;` |
|    - | 1748 | `	}` |
|  141 | 1749 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_BOUND ){` |
|    - | 1750 | `		sxi32 rcBound;` |
|   17 | 1751 | `		if( !pSt->bRowPending ){` |
|    3 | 1752 | `			PdoStmtOk(pSt);` |
|    3 | 1753 | `			ph7_result_bool(pCtx,0);` |
|    3 | 1754 | `			return PH7_OK;` |
|    - | 1755 | `		}` |
|   15 | 1756 | `		rcBound = PdoWriteBoundColumns(pCtx,pSt);` |
|   15 | 1757 | `		if( rcBound != PH7_OK ){` |
|    3 | 1758 | `			return rcBound;` |
|    - | 1759 | `		}` |
|   13 | 1760 | `		pSt->bRowPending = 0;` |
|   13 | 1761 | `		ph7_result_bool(pCtx,1);` |
|   13 | 1762 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 1763 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 1764 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 1765 | `		}` |
|   13 | 1766 | `		PdoStmtOk(pSt);` |
|   13 | 1767 | `		return PH7_OK;` |
|    - | 1768 | `	}` |
|  124 | 1769 | `	if( (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_CLASS` |
|  124 | 1770 | `	 \|\| (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_INTO ){` |
|   11 | 1771 | `		return PdoFetchObjectRow(pCtx,pSt,iMode,0,0,"PDOStatement::fetch");` |
|    - | 1772 | `	}` |
|  115 | 1773 | `	if( !pSt->bRowPending ){` |
|    9 | 1774 | `		PdoStmtOk(pSt);` |
|    9 | 1775 | `		ph7_result_bool(pCtx,0);` |
|    9 | 1776 | `		return PH7_OK;` |
|    - | 1777 | `	}` |
|  107 | 1778 | `	pRow = ph7_context_new_array(pCtx);` |
|  107 | 1779 | `	if( pRow == 0 ){` |
|  ! 0 | 1780 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1781 | `	}` |
|  107 | 1782 | `	if( !PdoStmtRow(pCtx->pVm,pSt,iMode & PDO_FETCH_MODE_MASK,pRow) ){` |
|  ! 0 | 1783 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1784 | `		return PH7_OK;` |
|    - | 1785 | `	}` |
|  107 | 1786 | `	ph7_result_value(pCtx,pRow);` |
|    - | 1787 | `	/* step ahead so the next call knows whether a row is waiting without` |
|    - | 1788 | `	 * having to ask twice */` |
|  107 | 1789 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 1790 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 1791 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetch");` |
|    - | 1792 | `	}` |
|  107 | 1793 | `	PdoStmtOk(pSt);` |
|  107 | 1794 | `	return PH7_OK;` |
|   72 | 1795 | `}` |
|    - | 1796 | `/*` |
|    - | 1797 | ` * PDOStatement::getColumnMeta(int $column): array\|false` |
|    - | 1798 | ` *` |
|    - | 1799 | ` * php's eight keys. Two of them describe different things and are routinely` |
|    - | 1800 | ``  * confused: `sqlite:decl_type` is what the SCHEMA declares, and `native_type` `` |
|    - | 1801 | ` * is the type of the value in the CURRENT row -- so a TEXT column holding NULL` |
|    - | 1802 | ` * reports "TEXT" and "null" at once, and an exhausted cursor reports "null"` |
|    - | 1803 | ` * for every column.` |
|    - | 1804 | ` *` |
|    - | 1805 | ` * A column that does not exist answers false, and php reports the last STEP's` |
|    - | 1806 | ` * result code as the driver error while doing so: that is why asking for` |
|    - | 1807 | ` * column 99 while a row is up comes back as "100 another row available"` |
|    - | 1808 | ` * instead of anything about the index.` |
|    - | 1809 | ` */` |
|    8 | 1810 | `static int vm_builtin_PDOStatement_getColumnMeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1811 | `{` |
|    9 | 1812 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 1813 | `	ph7_value *pMeta,*pCell,*pFlags;` |
|    - | 1814 | `	ph7_int64 iCol;` |
|    - | 1815 | `	const char *zDecl,*zTable,*zName;` |
|    - | 1816 | `	int iType,iPdoType;` |
|    - | 1817 | `	const char *zNative;` |
|    - | 1818 | `	SyBlob sName;` |
|    9 | 1819 | `	if( pSt == 0 ){` |
|  ! 0 | 1820 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 1821 | `	}` |
|    9 | 1822 | `	iCol = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    9 | 1823 | `	if( iCol < 0 ){` |
|    3 | 1824 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1825 | `			"PDOStatement::getColumnMeta(): Argument #1 ($column) must be greater than "` |
|    - | 1826 | `			"or equal to 0");` |
|    - | 1827 | `	}` |
|    7 | 1828 | `	if( iCol >= (ph7_int64)PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 1829 | `		int iStep = PH7_PdoSqliteLastStepCode(pSt);` |
|    3 | 1830 | `		ph7_result_bool(pCtx,0);` |
|    - | 1831 | `		/* php reports the last STEP's code as the driver detail here, which is` |
|    - | 1832 | `		 * why an out-of-range index talks about a row being available */` |
|    4 | 1833 | `		PH7_PdoSetError(pSt->pConn,"HY000",iStep,` |
|    1 | 1834 | `			iStep == 100 ? "another row available" : "no more rows available");` |
|    3 | 1835 | `		pSt->pConn->iErrState = PDO_ERR_OK;   /* the CONNECTION did not fail */` |
|    3 | 1836 | `		SyMemcpy("00000",pSt->pConn->zSqlState,sizeof("00000"));` |
|    3 | 1837 | `		PdoStmtFailed(pSt,"HY000");` |
|    3 | 1838 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::getColumnMeta");` |
|    - | 1839 | `	}` |
|    5 | 1840 | `	pMeta = ph7_context_new_array(pCtx);` |
|    5 | 1841 | `	pCell = ph7_context_new_scalar(pCtx);` |
|    5 | 1842 | `	pFlags = ph7_context_new_array(pCtx);` |
|    5 | 1843 | `	if( pMeta == 0 \|\| pCell == 0 \|\| pFlags == 0 ){` |
|  ! 0 | 1844 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1845 | `	}` |
|    5 | 1846 | `	iType = pSt->bRowPending ? PH7_PdoSqliteColumnType(pSt,(int)iCol) : SQLITE_NULL;` |
|    5 | 1847 | `	switch( iType ){` |
|    3 | 1848 | `		case SQLITE_INTEGER: zNative = "integer"; iPdoType = PDO_PARAM_INT; break;` |
|  ! 0 | 1849 | `		case SQLITE_FLOAT:   zNative = "double";  iPdoType = PDO_PARAM_STR; break;` |
|  ! 0 | 1850 | `		case SQLITE_BLOB:    zNative = "blob";    iPdoType = PDO_PARAM_LOB; break;` |
|    3 | 1851 | `		case SQLITE_NULL:    zNative = "null";    iPdoType = PDO_PARAM_NULL; break;` |
|  ! 0 | 1852 | `		default:             zNative = "string";  iPdoType = PDO_PARAM_STR; break;` |
|    - | 1853 | `	}` |
|    5 | 1854 | `	PH7_MemObjRelease(pCell);` |
|    5 | 1855 | `	ph7_value_string(pCell,zNative,(int)SyStrlen(zNative));` |
|    5 | 1856 | `	ph7_array_add_strkey_elem(pMeta,"native_type",pCell);` |
|    5 | 1857 | `	ph7_value_int(pCell,iPdoType);` |
|    5 | 1858 | `	ph7_array_add_strkey_elem(pMeta,"pdo_type",pCell);` |
|    5 | 1859 | `	zDecl = PH7_PdoSqliteColumnDecl(pSt,(int)iCol);` |
|    5 | 1860 | `	if( zDecl ){` |
|    5 | 1861 | `		PH7_MemObjRelease(pCell);` |
|    5 | 1862 | `		ph7_value_string(pCell,zDecl,(int)SyStrlen(zDecl));` |
|    5 | 1863 | `		ph7_array_add_strkey_elem(pMeta,"sqlite:decl_type",pCell);` |
|    2 | 1864 | `	}` |
|    5 | 1865 | `	zTable = PH7_PdoSqliteColumnTable(pSt,(int)iCol);` |
|    5 | 1866 | `	if( zTable ){` |
|    5 | 1867 | `		PH7_MemObjRelease(pCell);` |
|    5 | 1868 | `		ph7_value_string(pCell,zTable,(int)SyStrlen(zTable));` |
|    5 | 1869 | `		ph7_array_add_strkey_elem(pMeta,"table",pCell);` |
|    2 | 1870 | `	}` |
|    5 | 1871 | `	ph7_array_add_strkey_elem(pMeta,"flags",pFlags);` |
|    5 | 1872 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    5 | 1873 | `	zName = PH7_PdoSqliteColumnName(pSt,(int)iCol);` |
|    5 | 1874 | `	PdoColumnName(pSt->pConn,zName,&sName);` |
|    5 | 1875 | `	PH7_MemObjRelease(pCell);` |
|    5 | 1876 | `	ph7_value_string(pCell,(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName) - 1);` |
|    5 | 1877 | `	ph7_array_add_strkey_elem(pMeta,"name",pCell);` |
|    5 | 1878 | `	SyBlobRelease(&sName);` |
|    5 | 1879 | `	ph7_value_int(pCell,-1);` |
|    5 | 1880 | `	ph7_array_add_strkey_elem(pMeta,"len",pCell);` |
|    5 | 1881 | `	ph7_value_int(pCell,0);` |
|    5 | 1882 | `	ph7_array_add_strkey_elem(pMeta,"precision",pCell);` |
|    5 | 1883 | `	ph7_result_value(pCtx,pMeta);` |
|    5 | 1884 | `	return PH7_OK;` |
|    5 | 1885 | `}` |
|    - | 1886 | `/*` |
|    - | 1887 | ` * PDOStatement::nextRowset(): bool` |
|    - | 1888 | ` *` |
|    - | 1889 | ` * sqlite has no second result set to move to, so this is the layer refusal --` |
|    - | 1890 | ` * false, and IM001 with php's own "driver does not support multiple rowsets".` |
|    - | 1891 | ` * It leaves the CURSOR alone: a fetch after it still answers the row that was` |
|    - | 1892 | ` * waiting.` |
|    - | 1893 | ` */` |
|    4 | 1894 | `static int vm_builtin_PDOStatement_nextRowset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1895 | `{` |
|    5 | 1896 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 1897 | `	SXUNUSED(nArg);` |
|    2 | 1898 | `	SXUNUSED(apArg);` |
|    5 | 1899 | `	if( pSt == 0 ){` |
|  ! 0 | 1900 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 1901 | `	}` |
|    5 | 1902 | `	ph7_result_bool(pCtx,0);` |
|    5 | 1903 | `	PdoStmtFailed(pSt,"IM001");` |
|    5 | 1904 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::nextRowset","IM001",` |
|    - | 1905 | `		"driver does not support multiple rowsets");` |
|    3 | 1906 | `}` |
|    - | 1907 | `/*` |
|    - | 1908 | ` * PDOStatement::fetchColumn(int $column = 0): mixed` |
|    - | 1909 | ` *` |
|    - | 1910 | ` * One column of the next row, by position. An index outside the RESULT SET is` |
|    - | 1911 | ` * a ValueError rather than a null, and its two refusals are worded unlike` |
|    - | 1912 | ` * fetchAll()'s -- php's own inconsistency, reproduced.` |
|    - | 1913 | ` */` |
|   16 | 1914 | `static int vm_builtin_PDOStatement_fetchColumn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1915 | `{` |
|   17 | 1916 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 1917 | `	ph7_int64 iCol;` |
|    - | 1918 | `	ph7_value *pRow,*pCell;` |
|   17 | 1919 | `	if( pSt == 0 ){` |
|  ! 0 | 1920 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 1921 | `	}` |
|   17 | 1922 | `	iCol = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|   17 | 1923 | `	if( iCol < 0 ){` |
|    3 | 1924 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1925 | `			"Column index must be greater than or equal to 0");` |
|    - | 1926 | `	}` |
|   15 | 1927 | `	if( iCol >= (ph7_int64)PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 1928 | `		return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    - | 1929 | `	}` |
|   13 | 1930 | `	if( !pSt->bRowPending ){` |
|    3 | 1931 | `		PdoStmtOk(pSt);` |
|    3 | 1932 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1933 | `		return PH7_OK;` |
|    - | 1934 | `	}` |
|   11 | 1935 | `	pRow = ph7_context_new_array(pCtx);` |
|   11 | 1936 | `	if( pRow == 0 ){` |
|  ! 0 | 1937 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1938 | `	}` |
|   11 | 1939 | `	if( !PdoStmtRow(pCtx->pVm,pSt,PDO_FETCH_NUM,pRow) ){` |
|  ! 0 | 1940 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1941 | `		return PH7_OK;` |
|    - | 1942 | `	}` |
|   11 | 1943 | `	pCell = PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)iCol);` |
|   11 | 1944 | `	if( pCell ){` |
|   11 | 1945 | `		ph7_result_value(pCtx,pCell);` |
|    6 | 1946 | `	}else{` |
|  ! 0 | 1947 | `		ph7_result_null(pCtx);` |
|    - | 1948 | `	}` |
|   11 | 1949 | `	if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 1950 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 1951 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchColumn");` |
|    - | 1952 | `	}` |
|   11 | 1953 | `	PdoStmtOk(pSt);` |
|   11 | 1954 | `	return PH7_OK;` |
|    9 | 1955 | `}` |
|    - | 1956 | `/*` |
|    - | 1957 | ` * PDOStatement::setFetchMode(int $mode, mixed ...$args): true` |
|    - | 1958 | ` *` |
|    - | 1959 | ` * The mode a bare fetch()/fetchAll() will use from here on. FETCH_COLUMN needs` |
|    - | 1960 | ` * its column beside it, and php counts arguments PER MODE -- so the arity` |
|    - | 1961 | ` * refusal names the fetch mode rather than the method's own signature.` |
|    - | 1962 | ` */` |
|   18 | 1963 | `static int vm_builtin_PDOStatement_setFetchMode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1964 | `{` |
|   19 | 1965 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 1966 | `	int iMode;` |
|    - | 1967 | `	sxi32 rc;` |
|   19 | 1968 | `	if( pSt == 0 ){` |
|  ! 0 | 1969 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 1970 | `	}` |
|   19 | 1971 | `	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;` |
|   19 | 1972 | `	rc = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::setFetchMode");` |
|   19 | 1973 | `	if( rc != PH7_OK ){` |
|    3 | 1974 | `		return rc;` |
|    - | 1975 | `	}` |
|    - | 1976 | `	{` |
|   17 | 1977 | `		int iBase = iMode & PDO_FETCH_MODE_MASK;` |
|   16 | 1978 | `		if( (iBase == PDO_FETCH_COLUMN \|\| iBase == PDO_FETCH_INTO` |
|   17 | 1979 | `		  \|\| (iBase == PDO_FETCH_CLASS && (iMode & PDO_FETCH_CLASSTYPE) == 0)) && nArg < 2 ){` |
|    7 | 1980 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 1981 | `				"PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch "` |
|    2 | 1982 | `				"mode provided, %d given",nArg);` |
|    - | 1983 | `		}` |
|   13 | 1984 | `		PdoStmtClearFetchState(pSt);` |
|   14 | 1985 | `		if( iBase == PDO_FETCH_CLASS && nArg > 1 ){` |
|    3 | 1986 | `			int nName = 0;` |
|    3 | 1987 | `			const char *zName = ph7_value_to_string(apArg[1],&nName);` |
|    3 | 1988 | `			if( zName && nName > 0 ){` |
|    5 | 1989 | `				pSt->zFetchClass = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|    2 | 1990 | `					(sxu32)nName + 1);` |
|    3 | 1991 | `				if( pSt->zFetchClass ){` |
|    3 | 1992 | `					SyMemcpy(zName,pSt->zFetchClass,(sxu32)nName);` |
|    3 | 1993 | `					pSt->zFetchClass[nName] = 0;` |
|    3 | 1994 | `					pSt->nFetchClass = nName;` |
|    1 | 1995 | `				}` |
|    1 | 1996 | `			}` |
|    3 | 1997 | `			if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_HASHMAP) ){` |
|  ! 0 | 1998 | `				pSt->pFetchArgs = ph7_new_array(pCtx->pVm);` |
|  ! 0 | 1999 | `				if( pSt->pFetchArgs ){` |
|  ! 0 | 2000 | `					PH7_MemObjStore(apArg[2],pSt->pFetchArgs);` |
|  ! 0 | 2001 | `				}` |
|  ! 0 | 2002 | `			}` |
|   12 | 2003 | `		}else if( iBase == PDO_FETCH_INTO && nArg > 1` |
|    5 | 2004 | `		       && (apArg[1]->iFlags & MEMOBJ_OBJ) ){` |
|    5 | 2005 | `			pSt->pFetchInto = (ph7_class_instance *)apArg[1]->x.pOther;` |
|    5 | 2006 | `			pSt->pFetchInto->iRef++;   /* the statement writes into it for as long as it lives */` |
|    2 | 2007 | `		}` |
|    - | 2008 | `	}` |
|   13 | 2009 | `	pSt->iFetchMode = iMode;` |
|   11 | 2010 | `	pSt->iFetchColumn = (nArg > 1 && (iMode & PDO_FETCH_MODE_MASK) == PDO_FETCH_COLUMN)` |
|   11 | 2011 | `		? (int)ph7_value_to_int64(apArg[1]) : 0;` |
|   13 | 2012 | `	ph7_result_bool(pCtx,1);` |
|   13 | 2013 | `	return PH7_OK;` |
|   10 | 2014 | `}` |
|    - | 2015 | `/* Add one row under a key, collecting repeats into a list (FETCH_GROUP). */` |
|   18 | 2016 | `static void PdoGroupAppend(ph7_context *pCtx,ph7_value *pOut,ph7_value *pKey,ph7_value *pRow)` |
|    1 | 2017 | `{` |
|   19 | 2018 | `	ph7_value *pList = 0;` |
|   19 | 2019 | `	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){` |
|  ! 0 | 2020 | `		int nKey = 0;` |
|  ! 0 | 2021 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|  ! 0 | 2022 | `		pList = ph7_array_fetch(pOut,zKey,nKey);` |
|   19 | 2023 | `	}else if( pKey ){` |
|    - | 2024 | `		SyBlob sKey;` |
|   19 | 2025 | `		SyBlobInit(&sKey,&pCtx->pVm->sAllocator);` |
|   19 | 2026 | `		SyBlobFormat(&sKey,"%qd",ph7_value_to_int64(pKey));` |
|   19 | 2027 | `		SyBlobAppend(&sKey,"",1);` |
|   28 | 2028 | `		pList = ph7_array_fetch(pOut,(const char *)SyBlobData(&sKey),` |
|   18 | 2029 | `			(int)SyBlobLength(&sKey) - 1);` |
|   19 | 2030 | `		SyBlobRelease(&sKey);` |
|    9 | 2031 | `	}` |
|   19 | 2032 | `	if( pList && (pList->iFlags & MEMOBJ_HASHMAP) ){` |
|    7 | 2033 | `		ph7_array_add_elem(pList,0,pRow);` |
|    7 | 2034 | `		return;` |
|    - | 2035 | `	}` |
|   13 | 2036 | `	pList = ph7_context_new_array(pCtx);` |
|   13 | 2037 | `	if( pList == 0 ){` |
|  ! 0 | 2038 | `		return;` |
|    - | 2039 | `	}` |
|   13 | 2040 | `	ph7_array_add_elem(pList,0,pRow);` |
|   13 | 2041 | `	ph7_array_add_elem(pOut,pKey,pList);` |
|   10 | 2042 | `}` |
|    - | 2043 | `/*` |
|    - | 2044 | ` * PDOStatement::fetchAll(int $mode = PDO::FETCH_DEFAULT, mixed ...$args): array` |
|    - | 2045 | ` *` |
|    - | 2046 | ` * Every remaining row in one array. Four of the modes change the shape of that` |
|    - | 2047 | ` * ARRAY rather than the shape of a row: FETCH_COLUMN reduces each row to one` |
|    - | 2048 | ` * value, FETCH_KEY_PAIR to a key and a value (and refuses a result set that is` |
|    - | 2049 | ` * not exactly two columns wide), FETCH_FUNC replaces it with whatever a` |
|    - | 2050 | ` * callable answers, and GROUP/UNIQUE take the first column as a key -- GROUP` |
|    - | 2051 | ` * collecting every row under it, UNIQUE keeping the last.` |
|    - | 2052 | ` *` |
|    - | 2053 | ` * php counts arguments per mode here too, and its FETCH_FUNC wording is` |
|    - | 2054 | ` * singular ("expects exactly 2 argument"); both are reproduced as they stand.` |
|    - | 2055 | ` */` |
|   82 | 2056 | `static int vm_builtin_PDOStatement_fetchAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2057 | `{` |
|   83 | 2058 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2059 | `	ph7_value *pOut,*pRow;` |
|   83 | 2060 | `	int iMode,iBase,iCol = 0;` |
|    - | 2061 | `	int bGroup,bUnique;` |
|    - | 2062 | `	sxi32 rc;` |
|   83 | 2063 | `	if( pSt == 0 ){` |
|  ! 0 | 2064 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2065 | `	}` |
|   83 | 2066 | `	iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : PDO_FETCH_DEFAULT;` |
|   83 | 2067 | `	rc = PdoCheckFetchFlags(pCtx,iMode,"PDOStatement::fetchAll");` |
|   83 | 2068 | `	if( rc != PH7_OK ){` |
|    3 | 2069 | `		return rc;` |
|    - | 2070 | `	}` |
|   81 | 2071 | `	bGroup = (iMode & PDO_FETCH_GROUP) == PDO_FETCH_GROUP;` |
|   81 | 2072 | `	bUnique = (iMode & PDO_FETCH_UNIQUE) == PDO_FETCH_UNIQUE;` |
|   81 | 2073 | `	iBase = iMode & PDO_FETCH_MODE_MASK;` |
|   81 | 2074 | `	if( iBase == PDO_FETCH_DEFAULT ){` |
|   13 | 2075 | `		iBase = pSt->iFetchMode & PDO_FETCH_MODE_MASK;` |
|   13 | 2076 | `		bGroup = bGroup \|\| (pSt->iFetchMode & PDO_FETCH_GROUP) == PDO_FETCH_GROUP;` |
|   13 | 2077 | `		bUnique = bUnique \|\| (pSt->iFetchMode & PDO_FETCH_UNIQUE) == PDO_FETCH_UNIQUE;` |
|   13 | 2078 | `		iCol = pSt->iFetchColumn;` |
|    6 | 2079 | `	}` |
|   81 | 2080 | `	if( iBase == PDO_FETCH_DEFAULT ){` |
|  ! 0 | 2081 | `		iBase = pSt->pConn->iDefaultFetch;` |
|  ! 0 | 2082 | `	}` |
|   81 | 2083 | `	if( iBase == PDO_FETCH_COLUMN ){` |
|   17 | 2084 | `		if( nArg > 2 ){` |
|  ! 0 | 2085 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 2086 | `				"PDOStatement::fetchAll() expects exactly 2 arguments for the fetch "` |
|  ! 0 | 2087 | `				"mode provided, %d given",nArg);` |
|    - | 2088 | `		}` |
|   17 | 2089 | `		if( nArg > 1 ){` |
|    7 | 2090 | `			ph7_int64 iWant = ph7_value_to_int64(apArg[1]);` |
|    7 | 2091 | `			if( iWant < 0 ){` |
|    3 | 2092 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2093 | `					"PDOStatement::fetchAll(): Argument #2 must be greater than or "` |
|    - | 2094 | `					"equal to 0");` |
|    - | 2095 | `			}` |
|    5 | 2096 | `			iCol = (int)iWant;` |
|    2 | 2097 | `		}` |
|   15 | 2098 | `		if( iCol >= PH7_PdoSqliteColumnCount(pSt) ){` |
|    3 | 2099 | `			return PH7_VmThrowException(pCtx,"ValueError","Invalid column index");` |
|    1 | 2100 | `		}` |
|   71 | 2101 | `	}else if( iBase == PDO_FETCH_CLASS ){` |
|   19 | 2102 | `		if( nArg > 3 ){` |
|  ! 0 | 2103 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 2104 | `				"PDOStatement::fetchAll() expects at most 3 arguments for the fetch "` |
|  ! 0 | 2105 | `				"mode provided, %d given",nArg);` |
|    1 | 2106 | `		}` |
|   56 | 2107 | `	}else if( iBase == PDO_FETCH_LAZY ){` |
|  ! 0 | 2108 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2109 | `			"PDOStatement::fetchAll(): Argument #1 ($mode) PDO::FETCH_LAZY cannot be "` |
|    - | 2110 | `			"used with PDOStatement::fetchAll()");` |
|   47 | 2111 | `	}else if( iBase == PDO_FETCH_FUNC ){` |
|    7 | 2112 | `		if( nArg != 2 ){` |
|    4 | 2113 | `			return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 2114 | `				"PDOStatement::fetchAll() expects exactly 2 argument for "` |
|    1 | 2115 | `				"PDO::FETCH_FUNC, %d given",nArg);` |
|    - | 2116 | `		}` |
|    5 | 2117 | `		if( !ph7_value_is_callable(apArg[1]) ){` |
|    - | 2118 | `			/* php checks the callable BEFORE the first row, so an unusable one` |
|    - | 2119 | `			 * is a TypeError from PDO and never the engine's own` |
|    - | 2120 | `			 * "Call to undefined function" from inside the walk */` |
|    3 | 2121 | `			int nName = 0;` |
|    3 | 2122 | `			const char *zName = ph7_value_to_string(apArg[1],&nName);` |
|    4 | 2123 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    1 | 2124 | `				"function \"%.*s\" not found or invalid function name",nName,zName);` |
|    1 | 2125 | `		}` |
|   42 | 2126 | `	}else if( nArg > 1 ){` |
|    4 | 2127 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    - | 2128 | `			"PDOStatement::fetchAll() expects exactly 1 argument for the fetch mode "` |
|    1 | 2129 | `			"provided, %d given",nArg);` |
|    - | 2130 | `	}` |
|   71 | 2131 | `	if( iBase == PDO_FETCH_KEY_PAIR && PH7_PdoSqliteColumnCount(pSt) != 2 ){` |
|    5 | 2132 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 2133 | `			"SQLSTATE[HY000]: General error: PDO::FETCH_KEY_PAIR fetch mode requires "` |
|    - | 2134 | `			"the result set to contain exactly 2 columns.");` |
|    - | 2135 | `	}` |
|   67 | 2136 | `	pOut = ph7_context_new_array(pCtx);` |
|   67 | 2137 | `	if( pOut == 0 ){` |
|  ! 0 | 2138 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2139 | `	}` |
|  223 | 2140 | `	while( pSt->bRowPending ){` |
|  159 | 2141 | `		int iRowMode = iBase;` |
|  159 | 2142 | `		int iFirst = (bGroup \|\| bUnique) ? 1 : 0;` |
|  158 | 2143 | `		if( iBase == PDO_FETCH_COLUMN \|\| iBase == PDO_FETCH_KEY_PAIR` |
|  120 | 2144 | `		 \|\| iBase == PDO_FETCH_FUNC \|\| iBase == PDO_FETCH_BOUND ){` |
|   59 | 2145 | `			iRowMode = PDO_FETCH_NUM;` |
|   59 | 2146 | `			iFirst = 0;` |
|  130 | 2147 | `		}else if( iBase == PDO_FETCH_CLASS ){` |
|   35 | 2148 | `			iFirst = 0;` |
|   17 | 2149 | `		}` |
|  159 | 2150 | `		pRow = ph7_context_new_array(pCtx);` |
|  159 | 2151 | `		if( pRow == 0 ){` |
|  ! 0 | 2152 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 2153 | `		}` |
|  159 | 2154 | `		if( iFirst ){` |
|    - | 2155 | `			/* the FIRST column is the key and never joins the row */` |
|   31 | 2156 | `			ph7_value *pKeyRow = ph7_context_new_array(pCtx);` |
|    - | 2157 | `			ph7_value *pKey;` |
|   31 | 2158 | `			if( pKeyRow == 0 ){` |
|  ! 0 | 2159 | `				return PH7_ContextMemoryError(pCtx);` |
|    - | 2160 | `			}` |
|   31 | 2161 | `			if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pKeyRow,0) ){` |
|  ! 0 | 2162 | `				break;` |
|    - | 2163 | `			}` |
|   31 | 2164 | `			pKey = PdoArrayAtInt(pCtx->pVm,pKeyRow,0);` |
|    - | 2165 | `			/* the row itself is rebuilt from the SECOND column on; the cursor` |
|    - | 2166 | `			 * has not moved, so this reads the same sqlite row again */` |
|   31 | 2167 | `			pSt->bRowPending = 1;` |
|   31 | 2168 | `			if( !PdoStmtRowFrom(pCtx->pVm,pSt,iRowMode,pRow,1) ){` |
|  ! 0 | 2169 | `				break;` |
|    - | 2170 | `			}` |
|   31 | 2171 | `			if( bUnique ){` |
|   13 | 2172 | `				ph7_array_add_elem(pOut,pKey,pRow);` |
|    7 | 2173 | `			}else{` |
|   19 | 2174 | `				PdoGroupAppend(pCtx,pOut,pKey,pRow);` |
|    1 | 2175 | `			}` |
|  144 | 2176 | `		}else if( iBase == PDO_FETCH_CLASS ){` |
|    - | 2177 | `			/* every row is its own instance; the class and its constructor` |
|    - | 2178 | `			 * arguments are the same for all of them */` |
|    - | 2179 | `			ph7_class *pClass;` |
|    - | 2180 | `			ph7_value sObj;` |
|    - | 2181 | `			sxi32 rcCls;` |
|   35 | 2182 | `			int iFirstCol = 0;` |
|   35 | 2183 | `			if( nArg > 1 ){` |
|   27 | 2184 | `				pClass = PdoResolveFetchClass(pCtx,apArg[1],FALSE,&rcCls);` |
|   22 | 2185 | `			}else if( iMode & PDO_FETCH_CLASSTYPE ){` |
|    5 | 2186 | `				ph7_value *pHead = ph7_context_new_array(pCtx);` |
|    5 | 2187 | `				if( pHead == 0 ){` |
|  ! 0 | 2188 | `					return PH7_ContextMemoryError(pCtx);` |
|    - | 2189 | `				}` |
|    5 | 2190 | `				if( !PdoStmtRowFrom(pCtx->pVm,pSt,PDO_FETCH_NUM,pHead,0) ){` |
|  ! 0 | 2191 | `					break;` |
|    - | 2192 | `				}` |
|    5 | 2193 | `				pSt->bRowPending = 1;` |
|    5 | 2194 | `				pClass = PdoResolveFetchClass(pCtx,PdoArrayAtInt(pCtx->pVm,pHead,0),` |
|    - | 2195 | `					FALSE,&rcCls);` |
|    5 | 2196 | `				iFirstCol = 1;` |
|    3 | 2197 | `			}else{` |
|    5 | 2198 | `				pClass = PH7_VmExtractClass(pCtx->pVm,"stdClass",sizeof("stdClass")-1,` |
|    - | 2199 | `					FALSE,0);` |
|    5 | 2200 | `				rcCls = PH7_OK;` |
|    - | 2201 | `			}` |
|   35 | 2202 | `			if( pClass == 0 ){` |
|    3 | 2203 | `				return rcCls;` |
|    - | 2204 | `			}` |
|   33 | 2205 | `			PH7_MemObjInit(pCtx->pVm,&sObj);` |
|   33 | 2206 | `			if( !PdoRowIntoObject(pCtx,pSt,pClass,nArg > 2 ? apArg[2] : 0,` |
|   32 | 2207 | `				(iMode & PDO_FETCH_PROPS_LATE) != 0,iFirstCol,&sObj) ){` |
|  ! 0 | 2208 | `				PH7_MemObjRelease(&sObj);` |
|  ! 0 | 2209 | `				break;` |
|    - | 2210 | `			}` |
|   33 | 2211 | `			ph7_array_add_elem(pOut,0,&sObj);` |
|   33 | 2212 | `			PH7_MemObjRelease(&sObj);` |
|  111 | 2213 | `		}else if( iBase == PDO_FETCH_BOUND ){` |
|    - | 2214 | `			/* the row goes into the BOUND VARIABLES, not into the result: the` |
|    - | 2215 | `			 * array collects one true per row and the caller reads the last` |
|    - | 2216 | `			 * row's values out of its own variables */` |
|    - | 2217 | `			ph7_value *pTrue;` |
|   11 | 2218 | `			sxi32 rcBound = PdoWriteBoundColumns(pCtx,pSt);` |
|   11 | 2219 | `			if( rcBound != PH7_OK ){` |
|  ! 0 | 2220 | `				return rcBound;` |
|    - | 2221 | `			}` |
|   11 | 2222 | `			pSt->bRowPending = 0;` |
|   11 | 2223 | `			pTrue = ph7_context_new_scalar(pCtx);` |
|   11 | 2224 | `			if( pTrue ){` |
|   11 | 2225 | `				ph7_value_bool(pTrue,1);` |
|   11 | 2226 | `				ph7_array_add_elem(pOut,0,pTrue);` |
|    6 | 2227 | `			}` |
|   90 | 2228 | `		}else if( !PdoStmtRow(pCtx->pVm,pSt,iRowMode,pRow) ){` |
|  ! 0 | 2229 | `			break;` |
|   85 | 2230 | `		}else if( iBase == PDO_FETCH_COLUMN ){` |
|   37 | 2231 | `			ph7_array_add_elem(pOut,0,PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)iCol));` |
|   67 | 2232 | `		}else if( iBase == PDO_FETCH_KEY_PAIR ){` |
|   10 | 2233 | `			ph7_array_add_elem(pOut,PdoArrayAtInt(pCtx->pVm,pRow,0),` |
|    3 | 2234 | `				PdoArrayAtInt(pCtx->pVm,pRow,1));` |
|   46 | 2235 | `		}else if( iBase == PDO_FETCH_FUNC ){` |
|    - | 2236 | `			ph7_value sRes;` |
|    - | 2237 | `			ph7_value *apCall[32];` |
|    7 | 2238 | `			int n,nCall = PH7_PdoSqliteColumnCount(pSt);` |
|    7 | 2239 | `			if( nCall > (int)SX_ARRAYSIZE(apCall) ){` |
|  ! 0 | 2240 | `				nCall = (int)SX_ARRAYSIZE(apCall);` |
|  ! 0 | 2241 | `			}` |
|   19 | 2242 | `			for( n = 0 ; n < nCall ; ++n ){` |
|   13 | 2243 | `				apCall[n] = PdoArrayAtInt(pCtx->pVm,pRow,(sxi64)n);` |
|    7 | 2244 | `			}` |
|    7 | 2245 | `			PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    7 | 2246 | `			if( PH7_VmCallUserFunction(pCtx->pVm,apArg[1],nCall,apCall,&sRes) != SXRET_OK ){` |
|  ! 0 | 2247 | `				PH7_MemObjRelease(&sRes);` |
|  ! 0 | 2248 | `				return PH7_OK;   /* whatever the callable raised is already in flight */` |
|    - | 2249 | `			}` |
|    7 | 2250 | `			ph7_array_add_elem(pOut,0,&sRes);` |
|    7 | 2251 | `			PH7_MemObjRelease(&sRes);` |
|    4 | 2252 | `		}else{` |
|   37 | 2253 | `			ph7_array_add_elem(pOut,0,pRow);` |
|    - | 2254 | `		}` |
|  157 | 2255 | `		if( PdoStmtStep(pSt) < 0 ){` |
|  ! 0 | 2256 | `			PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|  ! 0 | 2257 | `			return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::fetchAll");` |
|    - | 2258 | `		}` |
|    1 | 2259 | `	}` |
|   65 | 2260 | `	PdoStmtOk(pSt);` |
|   65 | 2261 | `	ph7_result_value(pCtx,pOut);` |
|   65 | 2262 | `	return PH7_OK;` |
|   42 | 2263 | `}` |
|    - | 2264 | `/*` |
|    - | 2265 | ` * PDOStatement::columnCount(): int` |
|    - | 2266 | ` *` |
|    - | 2267 | ` * 0 for a statement that returns no rows -- and also for one that has been` |
|    - | 2268 | ` * PREPARED but not yet run, even though sqlite already knows the count from` |
|    - | 2269 | ` * the compile. php only publishes it once the statement has executed, so` |
|    - | 2270 | `` * `prepare('SELECT 1')->columnCount()` is 0 and not 1.`` |
|    - | 2271 | ` */` |
|    4 | 2272 | `static int vm_builtin_PDOStatement_columnCount(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2273 | `{` |
|    5 | 2274 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    2 | 2275 | `	SXUNUSED(nArg);` |
|    2 | 2276 | `	SXUNUSED(apArg);` |
|    5 | 2277 | `	if( pSt == 0 ){` |
|  ! 0 | 2278 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2279 | `	}` |
|    5 | 2280 | `	ph7_result_int(pCtx,pSt->bExecuted ? PH7_PdoSqliteColumnCount(pSt) : 0);` |
|    5 | 2281 | `	return PH7_OK;` |
|    3 | 2282 | `}` |
|    - | 2283 | `/*` |
|    - | 2284 | ` * PDOStatement::debugDumpParams(): ?bool` |
|    - | 2285 | ` *` |
|    - | 2286 | ` * php's own diagnostic dump, printed rather than returned (it answers null).` |
|    - | 2287 | ` * The bindings appear in the order they were MADE, and the two kinds report` |
|    - | 2288 | ` * differently: a positional one carries its 0-based paramno and an empty name,` |
|    - | 2289 | ` * a named one carries paramno -1 and the name WITH its colon. Both lengths are` |
|    - | 2290 | `` * printed in brackets, php's `[%d]` shape.`` |
|    - | 2291 | ` */` |
|    4 | 2292 | `static int vm_builtin_PDOStatement_debugDumpParams(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2293 | `{` |
|    5 | 2294 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2295 | `	phl_pdo_bind *pB;` |
|    5 | 2296 | `	const char *zSql = "";` |
|    5 | 2297 | `	int nSql = 0,nBind = 0;` |
|    - | 2298 | `	ph7_value *pQuery;` |
|    2 | 2299 | `	SXUNUSED(nArg);` |
|    2 | 2300 | `	SXUNUSED(apArg);` |
|    5 | 2301 | `	if( pSt == 0 ){` |
|  ! 0 | 2302 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2303 | `	}` |
|    5 | 2304 | `	pQuery = pSt->pOwner ? PH7_NativeAttr(pSt->pOwner,"queryString") : 0;` |
|    5 | 2305 | `	if( pQuery ){` |
|    5 | 2306 | `		zSql = ph7_value_to_string(pQuery,&nSql);` |
|    2 | 2307 | `	}` |
|    9 | 2308 | `	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){` |
|    5 | 2309 | `		++nBind;` |
|    3 | 2310 | `	}` |
|    5 | 2311 | `	ph7_context_output_format(pCtx,"SQL: [%d] %.*s\n",nSql,nSql,zSql);` |
|    5 | 2312 | `	ph7_context_output_format(pCtx,"Params:  %d\n",nBind);` |
|    - | 2313 | `	/* the list is built by prepending, so walking it backwards is what puts` |
|    - | 2314 | `	 * the bindings back in the order the script made them */` |
|    - | 2315 | `	{` |
|    - | 2316 | `		phl_pdo_bind *apBind[64];` |
|    5 | 2317 | `		int n = 0,i;` |
|    9 | 2318 | `		for( pB = pSt->pBinds ; pB && n < (int)SX_ARRAYSIZE(apBind) ; pB = pB->pNext ){` |
|    5 | 2319 | `			apBind[n++] = pB;` |
|    3 | 2320 | `		}` |
|    9 | 2321 | `		for( i = n - 1 ; i >= 0 ; --i ){` |
|    5 | 2322 | `			pB = apBind[i];` |
|    5 | 2323 | `			if( pB->zName ){` |
|    4 | 2324 | `				ph7_context_output_format(pCtx,"Key: Name: [%d] %.*s\n",` |
|    1 | 2325 | `					pB->nName,pB->nName,pB->zName);` |
|    3 | 2326 | `				ph7_context_output_format(pCtx,"paramno=-1\n");` |
|    4 | 2327 | `				ph7_context_output_format(pCtx,"name=[%d] \"%.*s\"\n",` |
|    1 | 2328 | `					pB->nName,pB->nName,pB->zName);` |
|    2 | 2329 | `			}else{` |
|    3 | 2330 | `				ph7_context_output_format(pCtx,"Key: Position #%d:\n",pB->iPos - 1);` |
|    3 | 2331 | `				ph7_context_output_format(pCtx,"paramno=%d\n",pB->iPos - 1);` |
|    3 | 2332 | `				ph7_context_output_format(pCtx,"name=[0] \"\"\n");` |
|    - | 2333 | `			}` |
|    5 | 2334 | `			ph7_context_output_format(pCtx,"is_param=1\n");` |
|    5 | 2335 | `			ph7_context_output_format(pCtx,"param_type=%d\n",pB->iType & ~PDO_PARAM_FLAGS);` |
|    3 | 2336 | `		}` |
|    - | 2337 | `	}` |
|    5 | 2338 | `	ph7_result_null(pCtx);` |
|    5 | 2339 | `	return PH7_OK;` |
|    3 | 2340 | `}` |
|    - | 2341 | `/*` |
|    - | 2342 | ` * PDOStatement::getAttribute(int $name): mixed` |
|    - | 2343 | ` *` |
|    - | 2344 | ` * Two of the driver's attributes describe a STATEMENT rather than the` |
|    - | 2345 | ` * connection -- whether it only reads, and whether it is mid-walk -- and both` |
|    - | 2346 | ` * are sqlite's own answers about the compiled statement. Everything else is` |
|    - | 2347 | ` * the same IM001 refusal the connection gives.` |
|    - | 2348 | ` */` |
|    4 | 2349 | `static int vm_builtin_PDOStatement_getAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2350 | `{` |
|    5 | 2351 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2352 | `	ph7_int64 iAttr;` |
|    5 | 2353 | `	if( pSt == 0 ){` |
|  ! 0 | 2354 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2355 | `	}` |
|    5 | 2356 | `	iAttr = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    5 | 2357 | `	if( iAttr == PDO_SQLITE_ATTR_READONLY_STATEMENT ){` |
|    3 | 2358 | `		ph7_result_bool(pCtx,PH7_PdoSqliteStmtReadonly(pSt));` |
|    3 | 2359 | `		return PH7_OK;` |
|    - | 2360 | `	}` |
|    3 | 2361 | `	if( iAttr == PDO_SQLITE_ATTR_BUSY_STATEMENT ){` |
|    3 | 2362 | `		ph7_result_bool(pCtx,PH7_PdoSqliteStmtBusy(pSt));` |
|    3 | 2363 | `		return PH7_OK;` |
|    - | 2364 | `	}` |
|  ! 0 | 2365 | `	ph7_result_null(pCtx);` |
|  ! 0 | 2366 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::getAttribute","IM001",` |
|    - | 2367 | `		"driver does not support that attribute");` |
|    3 | 2368 | `}` |
|    - | 2369 | `/*` |
|    - | 2370 | ` * PDOStatement::setAttribute(int $attribute, mixed $value): bool` |
|    - | 2371 | ` *` |
|    - | 2372 | ` * This driver carries no SETTABLE statement attribute at all, so every one of` |
|    - | 2373 | ` * them is the same IM001 refusal.` |
|    - | 2374 | ` */` |
|  ! 0 | 2375 | `static int vm_builtin_PDOStatement_setAttribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 2376 | `{` |
|  ! 0 | 2377 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|  ! 0 | 2378 | `	SXUNUSED(nArg);` |
|  ! 0 | 2379 | `	SXUNUSED(apArg);` |
|  ! 0 | 2380 | `	if( pSt == 0 ){` |
|  ! 0 | 2381 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2382 | `	}` |
|  ! 0 | 2383 | `	ph7_result_bool(pCtx,0);` |
|  ! 0 | 2384 | `	return PH7_PdoRaiseImplStmt(pCtx,pSt,"PDOStatement::setAttribute","IM001",` |
|    - | 2385 | `		"driver does not support that attribute");` |
|  ! 0 | 2386 | `}` |
|    - | 2387 | `/*` |
|    - | 2388 | ` * PDOStatement::rowCount(): int` |
|    - | 2389 | ` *` |
|    - | 2390 | ` * The number of rows a WRITE changed. It is not the size of a result set --` |
|    - | 2391 | ` * sqlite cannot know that without walking it -- so a SELECT answers 0, which` |
|    - | 2392 | ` * is php's answer and the reason its manual warns against this method.` |
|    - | 2393 | ` */` |
|    6 | 2394 | `static int vm_builtin_PDOStatement_rowCount(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2395 | `{` |
|    7 | 2396 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    3 | 2397 | `	SXUNUSED(nArg);` |
|    3 | 2398 | `	SXUNUSED(apArg);` |
|    7 | 2399 | `	if( pSt == 0 ){` |
|  ! 0 | 2400 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2401 | `	}` |
|    7 | 2402 | `	ph7_result_int64(pCtx,pSt->nChanges);` |
|    7 | 2403 | `	return PH7_OK;` |
|    4 | 2404 | `}` |
|    - | 2405 | `/*` |
|    - | 2406 | ` * PDOStatement::closeCursor(): bool` |
|    - | 2407 | ` *` |
|    - | 2408 | ` * Frees the rows a statement is still holding without discarding the statement` |
|    - | 2409 | ` * itself: php answers true and leaves the object reusable, and a fetch after` |
|    - | 2410 | ` * it answers false.` |
|    - | 2411 | ` */` |
|    2 | 2412 | `static int vm_builtin_PDOStatement_closeCursor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2413 | `{` |
|    3 | 2414 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    1 | 2415 | `	SXUNUSED(nArg);` |
|    1 | 2416 | `	SXUNUSED(apArg);` |
|    3 | 2417 | `	if( pSt == 0 ){` |
|  ! 0 | 2418 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2419 | `	}` |
|    3 | 2420 | `	if( pSt->pStmt ){` |
|    3 | 2421 | `		sqlite3_reset(pSt->pStmt);` |
|    1 | 2422 | `	}` |
|    3 | 2423 | `	pSt->bRowPending = 0;` |
|    3 | 2424 | `	pSt->bDone = 1;` |
|    3 | 2425 | `	ph7_result_bool(pCtx,1);` |
|    3 | 2426 | `	return PH7_OK;` |
|    2 | 2427 | `}` |
|    - | 2428 | `/* A statement reports its CONNECTION's error state; php keeps one per` |
|    - | 2429 | ` * statement, and every path that sets one sets both. */` |
|    2 | 2430 | `static int vm_builtin_PDOStatement_errorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2431 | `{` |
|    3 | 2432 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    1 | 2433 | `	SXUNUSED(nArg);` |
|    1 | 2434 | `	SXUNUSED(apArg);` |
|    3 | 2435 | `	if( pSt == 0 ){` |
|  ! 0 | 2436 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2437 | `	}` |
|    3 | 2438 | `	if( pSt->iErrState == PDO_ERR_NONE ){` |
|  ! 0 | 2439 | `		ph7_result_null(pCtx);` |
|  ! 0 | 2440 | `	}else{` |
|    3 | 2441 | `		ph7_result_string(pCtx,pSt->zSqlState,(int)SyStrlen(pSt->zSqlState));` |
|    - | 2442 | `	}` |
|    3 | 2443 | `	return PH7_OK;` |
|    2 | 2444 | `}` |
|    2 | 2445 | `static int vm_builtin_PDOStatement_errorInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2446 | `{` |
|    3 | 2447 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    1 | 2448 | `	SXUNUSED(nArg);` |
|    1 | 2449 | `	SXUNUSED(apArg);` |
|    3 | 2450 | `	if( pSt == 0 ){` |
|  ! 0 | 2451 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2452 | `	}` |
|    3 | 2453 | `	return PdoBuildErrorInfo(pCtx,pSt->pConn,pSt->iErrState,pSt->zSqlState);` |
|    2 | 2454 | `}` |
|    - | 2455 | `/*` |
|    - | 2456 | ` * The InternalIterator a foreach over a statement walks.  A statement is a` |
|    - | 2457 | ` * forward cursor, so REWIND does not rewind: it settles on whatever row is` |
|    - | 2458 | ` * pending, which is why a second foreach over the same statement walks nothing` |
|    - | 2459 | ` * at all rather than repeating the set.` |
|    - | 2460 | ` */` |
|   10 | 2461 | `static void PdoStmtIterSettle(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 2462 | `{` |
|   11 | 2463 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|   11 | 2464 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(pSrc);` |
|    - | 2465 | `	ph7_value *pRow;` |
|   11 | 2466 | `	if( pSt == 0 \|\| !pSt->bRowPending ){` |
|    7 | 2467 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    7 | 2468 | `		return;` |
|    - | 2469 | `	}` |
|    5 | 2470 | `	pRow = ph7_new_array(pVm);` |
|    5 | 2471 | `	if( pRow == 0 \|\| !PdoStmtRow(pVm,pSt,pSt->iFetchMode,pRow) ){` |
|  ! 0 | 2472 | `		if( pRow ){` |
|  ! 0 | 2473 | `			ph7_release_value(pVm,pRow);` |
|  ! 0 | 2474 | `		}` |
|  ! 0 | 2475 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|  ! 0 | 2476 | `		return;` |
|    - | 2477 | `	}` |
|    5 | 2478 | `	PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,(int)SyStrlen(PH7_NATIVE_IT_CUR),pRow);` |
|    5 | 2479 | `	ph7_release_value(pVm,pRow);` |
|    7 | 2480 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,` |
|    2 | 2481 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));` |
|    5 | 2482 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    5 | 2483 | `	PdoStmtStep(pSt);` |
|    6 | 2484 | `}` |
|    - | 2485 | `/*` |
|    - | 2486 | ` * A rewind that does NOT rewind, and must not even re-read: the iterator is` |
|    - | 2487 | `` * built already positioned and `foreach` rewinds it again, so a settle here`` |
|    - | 2488 | ` * would swallow the first row. The AUX slot records that the first row has` |
|    - | 2489 | ` * been taken; every later rewind is a no-op, which is also what makes a SECOND` |
|    - | 2490 | ` * foreach over the same statement walk nothing at all -- php's answer, because` |
|    - | 2491 | ` * the cursor is forward-only and has nowhere to go back to.` |
|    - | 2492 | ` */` |
|   10 | 2493 | `static void PdoStmtIterRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 2494 | `{` |
|   11 | 2495 | `	if( PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_AUX) != 0 ){` |
|    5 | 2496 | `		return;` |
|    - | 2497 | `	}` |
|    7 | 2498 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_AUX,1);` |
|    7 | 2499 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|    7 | 2500 | `	PdoStmtIterSettle(&(*pVm),pIt);` |
|    6 | 2501 | `}` |
|    4 | 2502 | `static void PdoStmtIterNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 2503 | `{` |
|    7 | 2504 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|    4 | 2505 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|    5 | 2506 | `	PdoStmtIterSettle(&(*pVm),pIt);` |
|    5 | 2507 | `}` |
|    - | 2508 | `static const PH7_NativeIterVtab sPdoStmtIterVtab = { PdoStmtIterRewind, PdoStmtIterNext };` |
|    6 | 2509 | `static int vm_builtin_PDOStatement_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2510 | `{` |
|    7 | 2511 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - | 2512 | `	ph7_class_instance *pIt;` |
|    3 | 2513 | `	SXUNUSED(nArg);` |
|    3 | 2514 | `	SXUNUSED(apArg);` |
|    7 | 2515 | `	if( pThis == 0 ){` |
|  ! 0 | 2516 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement::getIterator() needs a receiver");` |
|    - | 2517 | `	}` |
|    7 | 2518 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    7 | 2519 | `	if( pIt == 0 ){` |
|  ! 0 | 2520 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2521 | `	}` |
|    7 | 2522 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    7 | 2523 | `	return PH7_OK;` |
|    4 | 2524 | `}` |
|    - | 2525 | `/*` |
|    - | 2526 | ` * Record one binding.  A name is kept as the script spelled it -- with or` |
|    - | 2527 | ` * without its colon -- because the resolution happens at execute(), when the` |
|    - | 2528 | ` * statement that knows the names exists.` |
|    - | 2529 | ` */` |
|   32 | 2530 | `static phl_pdo_bind * PdoBindAdd(phl_pdo_stmt *pSt,const char *zName,int nName,int iPos,` |
|    - | 2531 | `	int iType)` |
|    2 | 2532 | `{` |
|   34 | 2533 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
|    - | 2534 | `	phl_pdo_bind *pB;` |
|    - | 2535 | `	/* php REPLACES a binding for the same parameter rather than stacking one */` |
|   48 | 2536 | `	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){` |
|   29 | 2537 | `		if( zName ? (pB->zName && pB->nName == nName` |
|    1 | 2538 | `		             && SyMemcmp(pB->zName,zName,(sxu32)nName) == 0)` |
|   12 | 2539 | `		          : (pB->zName == 0 && pB->iPos == iPos) ){` |
|  ! 0 | 2540 | `			if( pB->pVal ){` |
|  ! 0 | 2541 | `				ph7_release_value(pVm,pB->pVal);` |
|  ! 0 | 2542 | `				pB->pVal = 0;` |
|  ! 0 | 2543 | `			}` |
|  ! 0 | 2544 | `			pB->iType = iType;` |
|  ! 0 | 2545 | `			pB->nSlot = SXU32_HIGH;` |
|  ! 0 | 2546 | `			return pB;` |
|    - | 2547 | `		}` |
|    9 | 2548 | `	}` |
|   34 | 2549 | `	pB = (phl_pdo_bind *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_pdo_bind));` |
|   34 | 2550 | `	if( pB == 0 ){` |
|  ! 0 | 2551 | `		return 0;` |
|    - | 2552 | `	}` |
|   34 | 2553 | `	SyZero(pB,sizeof(phl_pdo_bind));` |
|   34 | 2554 | `	pB->iPos = iPos;` |
|   34 | 2555 | `	pB->iType = iType;` |
|   34 | 2556 | `	pB->nSlot = SXU32_HIGH;` |
|   34 | 2557 | `	if( zName && nName > 0 ){` |
|    6 | 2558 | `		pB->zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)nName + 1);` |
|    6 | 2559 | `		if( pB->zName == 0 ){` |
|  ! 0 | 2560 | `			SyMemBackendFree(&pVm->sAllocator,pB);` |
|  ! 0 | 2561 | `			return 0;` |
|    - | 2562 | `		}` |
|    6 | 2563 | `		SyMemcpy(zName,pB->zName,(sxu32)nName);` |
|    6 | 2564 | `		pB->zName[nName] = 0;` |
|    6 | 2565 | `		pB->nName = nName;` |
|    2 | 2566 | `	}` |
|   34 | 2567 | `	pB->pNext = pSt->pBinds;` |
|   34 | 2568 | `	pSt->pBinds = pB;` |
|   34 | 2569 | `	return pB;` |
|   18 | 2570 | `}` |
|    - | 2571 | `/*` |
|    - | 2572 | ` * The shared body of bindValue() and bindParam(): they differ only in WHEN the` |
|    - | 2573 | ` * value is read. Argument #1 is a name or a 1-based position, and php refuses` |
|    - | 2574 | ` * position 0 by ValueError before the statement is consulted at all.` |
|    - | 2575 | ` */` |
|   34 | 2576 | `static int PdoBindArgument(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zFn,` |
|    - | 2577 | `	int bByRef)` |
|    2 | 2578 | `{` |
|   36 | 2579 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2580 | `	phl_pdo_bind *pB;` |
|    - | 2581 | `	ph7_value *pKey;` |
|    - | 2582 | `	int iType;` |
|   36 | 2583 | `	if( pSt == 0 ){` |
|  ! 0 | 2584 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2585 | `	}` |
|   36 | 2586 | `	pKey = nArg > 0 ? apArg[0] : 0;` |
|   36 | 2587 | `	iType = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : PDO_PARAM_STR;` |
|   38 | 2588 | `	if( pKey && (pKey->iFlags & MEMOBJ_STRING) ){` |
|    6 | 2589 | `		int nName = 0;` |
|    6 | 2590 | `		const char *zName = ph7_value_to_string(pKey,&nName);` |
|    6 | 2591 | `		pB = PdoBindAdd(pSt,zName,nName,0,iType);` |
|    4 | 2592 | `	}else{` |
|   32 | 2593 | `		ph7_int64 iPos = pKey ? ph7_value_to_int64(pKey) : 0;` |
|   32 | 2594 | `		if( iPos < 1 ){` |
|    4 | 2595 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    1 | 2596 | `				"%s(): Argument #1 ($param) must be greater than or equal to 1",zFn);` |
|    - | 2597 | `		}` |
|   30 | 2598 | `		pB = PdoBindAdd(pSt,0,0,(int)iPos,iType);` |
|    - | 2599 | `	}` |
|   34 | 2600 | `	if( pB == 0 ){` |
|  ! 0 | 2601 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2602 | `	}` |
|   34 | 2603 | `	if( bByRef ){` |
|    - | 2604 | `		/* bindParam(): remember the caller's SLOT, so a write to that variable` |
|    - | 2605 | `		 * after this call is the value execute() runs with. The engine hands a` |
|    - | 2606 | `		 * by-reference argument as the caller's own memobj, and its index is` |
|    - | 2607 | `		 * how every other deferred read here finds it again. */` |
|    3 | 2608 | `		pB->nSlot = (nArg > 1 && apArg[1]) ? apArg[1]->nIdx : SXU32_HIGH;` |
|   33 | 2609 | `	}else if( nArg > 1 ){` |
|    - | 2610 | `		/* bindValue(): the statement takes its own copy now */` |
|   32 | 2611 | `		pB->pVal = ph7_new_scalar(pCtx->pVm);` |
|   32 | 2612 | `		if( pB->pVal == 0 ){` |
|  ! 0 | 2613 | `			return PH7_ContextMemoryError(pCtx);` |
|    - | 2614 | `		}` |
|   32 | 2615 | `		PH7_MemObjStore(apArg[1],pB->pVal);` |
|   15 | 2616 | `	}` |
|   34 | 2617 | `	ph7_result_bool(pCtx,1);` |
|   34 | 2618 | `	return PH7_OK;` |
|   19 | 2619 | `}` |
|   32 | 2620 | `static int vm_builtin_PDOStatement_bindValue(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2621 | `{` |
|   34 | 2622 | `	return PdoBindArgument(pCtx,nArg,apArg,"PDOStatement::bindValue",FALSE);` |
|    2 | 2623 | `}` |
|    2 | 2624 | `static int vm_builtin_PDOStatement_bindParam(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2625 | `{` |
|    3 | 2626 | `	return PdoBindArgument(pCtx,nArg,apArg,"PDOStatement::bindParam",TRUE);` |
|    1 | 2627 | `}` |
|    - | 2628 | `/* Bind one recorded parameter, resolving a name against the live statement. */` |
|   28 | 2629 | `static int PdoBindApply(ph7_vm *pVm,phl_pdo_stmt *pSt,phl_pdo_bind *pB)` |
|    1 | 2630 | `{` |
|   29 | 2631 | `	ph7_value *pVal = pB->pVal;` |
|   29 | 2632 | `	int iPos = pB->iPos;` |
|   29 | 2633 | `	if( pB->zName ){` |
|    3 | 2634 | `		iPos = PH7_PdoSqliteBindIndexOf(pSt,pB->zName,pB->nName);` |
|    1 | 2635 | `	}` |
|   29 | 2636 | `	if( pB->nSlot != SXU32_HIGH ){` |
|    - | 2637 | `		/* bindParam(): read the caller's variable NOW */` |
|    3 | 2638 | `		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pB->nSlot);` |
|    1 | 2639 | `	}` |
|   29 | 2640 | `	return PH7_PdoSqliteBindAt(pSt,iPos,pB->iType,pVal);` |
|    1 | 2641 | `}` |
|    - | 2642 | `/*` |
|    - | 2643 | `` * execute()'s `?array $params`: php binds the array INSTEAD of whatever was`` |
|    - | 2644 | ` * recorded, an integer key naming a 1-based position (so element 0 is` |
|    - | 2645 | ` * parameter 1) and a string key naming a placeholder.` |
|    - | 2646 | ` */` |
|   26 | 2647 | `static int PdoBindFromArray(ph7_vm *pVm,phl_pdo_stmt *pSt,ph7_value *pArray)` |
|    1 | 2648 | `{` |
|    - | 2649 | `	ph7_hashmap *pMap;` |
|    - | 2650 | `	ph7_hashmap_node *pEntry;` |
|    - | 2651 | `	sxu32 n,nCount;` |
|   27 | 2652 | `	int rc = 1;` |
|   27 | 2653 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 2654 | `		return 1;` |
|    - | 2655 | `	}` |
|   27 | 2656 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|   27 | 2657 | `	nCount = pMap->nEntry;` |
|   27 | 2658 | `	pEntry = pMap->pFirst;` |
|   53 | 2659 | `	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 2660 | `		ph7_value sKey;` |
|   35 | 2661 | `		ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|    - | 2662 | `		int iPos;` |
|   35 | 2663 | `		PH7_MemObjInit(pVm,&sKey);` |
|   35 | 2664 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   35 | 2665 | `		if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   21 | 2666 | `			iPos = (int)sKey.x.iVal + 1;` |
|   11 | 2667 | `		}else{` |
|   15 | 2668 | `			int nName = 0;` |
|   15 | 2669 | `			const char *zName = ph7_value_to_string(&sKey,&nName);` |
|   15 | 2670 | `			iPos = PH7_PdoSqliteBindIndexOf(pSt,zName,nName);` |
|    - | 2671 | `		}` |
|   35 | 2672 | `		PH7_MemObjRelease(&sKey);` |
|    - | 2673 | `		/* php binds every element as a STRING unless the script said otherwise` |
|    - | 2674 | `		 * through bindValue(); a php null still binds as NULL. */` |
|   35 | 2675 | `		if( !PH7_PdoSqliteBindAt(pSt,iPos,PDO_PARAM_STR,pVal) ){` |
|    9 | 2676 | `			rc = 0;` |
|    9 | 2677 | `			break;` |
|    - | 2678 | `		}` |
|   14 | 2679 | `	}` |
|   27 | 2680 | `	return rc;` |
|   14 | 2681 | `}` |
|    - | 2682 | `/*` |
|    - | 2683 | ` * PDOStatement::execute(?array $params = null): bool` |
|    - | 2684 | ` *` |
|    - | 2685 | ` * Runs the statement from the start: the cursor is rewound, the previous run's` |
|    - | 2686 | ` * values are dropped, the parameters are bound and one step is taken -- the` |
|    - | 2687 | ` * same first step query() takes, so columnCount() and the first fetch() behave` |
|    - | 2688 | ` * identically whichever verb produced the statement.` |
|    - | 2689 | ` */` |
|   50 | 2690 | `static int vm_builtin_PDOStatement_execute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2691 | `{` |
|   52 | 2692 | `	phl_pdo_stmt *pSt = PdoStmtOfInstance(PH7_ContextThis(pCtx));` |
|   52 | 2693 | `	int bOk = 1;` |
|   52 | 2694 | `	if( pSt == 0 ){` |
|  ! 0 | 2695 | `		return PH7_VmThrowException(pCtx,"Error","PDOStatement object is uninitialized");` |
|    - | 2696 | `	}` |
|   52 | 2697 | `	PH7_PdoTouch(pSt->pConn);` |
|   52 | 2698 | `	PH7_PdoSqliteReset(pSt);` |
|   52 | 2699 | `	pSt->bRowPending = 0;` |
|   52 | 2700 | `	pSt->bDone = 0;` |
|   52 | 2701 | `	if( nArg > 0 && apArg[0] && (apArg[0]->iFlags & MEMOBJ_HASHMAP) ){` |
|   27 | 2702 | `		bOk = PdoBindFromArray(pCtx->pVm,pSt,apArg[0]);` |
|   14 | 2703 | `	}else{` |
|    - | 2704 | `		phl_pdo_bind *pB;` |
|   54 | 2705 | `		for( pB = pSt->pBinds ; pB && bOk ; pB = pB->pNext ){` |
|   29 | 2706 | `			bOk = PdoBindApply(pCtx->pVm,pSt,pB);` |
|   15 | 2707 | `		}` |
|    - | 2708 | `	}` |
|   52 | 2709 | `	if( !bOk ){` |
|    9 | 2710 | `		ph7_result_bool(pCtx,0);` |
|    9 | 2711 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|    9 | 2712 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::execute");` |
|    - | 2713 | `	}` |
|   44 | 2714 | `	pSt->bExecuted = 1;` |
|   44 | 2715 | `	if( PdoStmtStep(pSt) < 0 ){` |
|    3 | 2716 | `		ph7_result_bool(pCtx,0);` |
|    3 | 2717 | `		PdoStmtFailed(pSt,pSt->pConn->zSqlState);` |
|    3 | 2718 | `		return PH7_PdoRaiseStmt(pCtx,pSt,"PDOStatement::execute");` |
|    - | 2719 | `	}` |
|   42 | 2720 | `	PdoStmtOk(pSt);` |
|   42 | 2721 | `	pSt->nChanges = PH7_PdoSqliteColumnCount(pSt) > 0` |
|   21 | 2722 | `		? 0 : PH7_PdoSqliteChanges(pSt->pConn);` |
|   42 | 2723 | `	ph7_result_bool(pCtx,1);` |
|   42 | 2724 | `	return PH7_OK;` |
|   27 | 2725 | `}` |
|    - | 2726 | `/*` |
|    - | 2727 | ` * The class query()/prepare() builds.  ATTR_STATEMENT_CLASS replaces` |
|    - | 2728 | ` * PDOStatement with a subclass of the script's own, and php builds THAT for` |
|    - | 2729 | ` * every statement the connection makes from then on.` |
|    - | 2730 | ` */` |
|  272 | 2731 | `static ph7_class * PdoStatementClass(ph7_context *pCtx,phl_pdo *pConn)` |
|    2 | 2732 | `{` |
|  274 | 2733 | `	if( pConn->zStmtClass ){` |
|    7 | 2734 | `		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,pConn->zStmtClass,` |
|    4 | 2735 | `			(sxu32)pConn->nStmtClass,FALSE,0);` |
|    5 | 2736 | `		if( pClass ){` |
|    5 | 2737 | `			return pClass;` |
|    - | 2738 | `		}` |
|  ! 0 | 2739 | `	}` |
|  270 | 2740 | `	return PH7_VmExtractClass(pCtx->pVm,"PDOStatement",sizeof("PDOStatement")-1,FALSE,0);` |
|  138 | 2741 | `}` |
|    - | 2742 | `/*` |
|    - | 2743 | ` * PDO::prepare(string $query, array $options = []): PDOStatement\|false` |
|    - | 2744 | ` *` |
|    - | 2745 | ` * Compiles without running. The options array is php's per-statement` |
|    - | 2746 | ` * attribute set; the sqlite driver carries none of the ones a script can put` |
|    - | 2747 | ` * there, and php ignores an unusable one rather than refusing the call.` |
|    - | 2748 | ` */` |
|   60 | 2749 | `static int vm_builtin_PDO_prepare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2750 | `{` |
|   62 | 2751 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2752 | `	phl_pdo_stmt *pSt;` |
|    - | 2753 | `	ph7_class *pClass;` |
|    - | 2754 | `	ph7_class_instance *pObj;` |
|    - | 2755 | `	const char *zSql;` |
|   62 | 2756 | `	int nSql = 0;` |
|   62 | 2757 | `	if( pConn == 0 ){` |
|  ! 0 | 2758 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 2759 | `	}` |
|   62 | 2760 | `	PH7_PdoTouch(pConn);` |
|   62 | 2761 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|   62 | 2762 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 | 2763 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2764 | `			"PDO::prepare(): Argument #1 ($query) must not be empty");` |
|    - | 2765 | `	}` |
|   60 | 2766 | `	pSt = PH7_PdoNewStmt(pConn);` |
|   60 | 2767 | `	if( pSt == 0 ){` |
|  ! 0 | 2768 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2769 | `	}` |
|   60 | 2770 | `	if( !PH7_PdoSqlitePrepare(pSt,zSql,nSql) ){` |
|    3 | 2771 | `		ph7_result_bool(pCtx,0);` |
|    3 | 2772 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::prepare");` |
|    - | 2773 | `	}` |
|   58 | 2774 | `	pClass = PdoStatementClass(pCtx,pConn);` |
|   58 | 2775 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|   58 | 2776 | `	if( pObj == 0 ){` |
|  ! 0 | 2777 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2778 | `	}` |
|   58 | 2779 | `	if( PdoStmtAttach(pObj,pSt) != 0 ){` |
|  ! 0 | 2780 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 2781 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2782 | `	}` |
|   58 | 2783 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,"queryString",zSql,nSql);` |
|   58 | 2784 | `	PH7_NativeResultObject(pCtx,pObj);` |
|   58 | 2785 | `	return PH7_OK;` |
|   32 | 2786 | `}` |
|    - | 2787 | `/*` |
|    - | 2788 | ` * PDO::quote(string $string, int $type = PDO::PARAM_STR): string\|false` |
|    - | 2789 | ` *` |
|    - | 2790 | ` * sqlite's own quoting: single quotes around it, each embedded quote doubled.` |
|    - | 2791 | ` * A NUL byte has no spelling inside a sqlite literal at all, so php refuses` |
|    - | 2792 | ` * one -- with a bare sentence carrying no SQLSTATE, unlike every other` |
|    - | 2793 | ` * PDOException this driver raises.` |
|    - | 2794 | ` */` |
|   10 | 2795 | `static int vm_builtin_PDO_quote(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2796 | `{` |
|   11 | 2797 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2798 | `	const char *zIn;` |
|   11 | 2799 | `	int nIn = 0,i;` |
|    - | 2800 | `	SyBlob sOut;` |
|   11 | 2801 | `	if( pConn == 0 ){` |
|  ! 0 | 2802 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 2803 | `	}` |
|   11 | 2804 | `	PH7_PdoTouch(pConn);` |
|   11 | 2805 | `	zIn = nArg > 0 ? ph7_value_to_string(apArg[0],&nIn) : "";` |
|   31 | 2806 | `	for( i = 0 ; i < nIn ; ++i ){` |
|   23 | 2807 | `		if( zIn[i] == 0 ){` |
|    3 | 2808 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 2809 | `				"SQLite PDO::quote does not support null bytes");` |
|    - | 2810 | `		}` |
|   11 | 2811 | `	}` |
|    9 | 2812 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    9 | 2813 | `	SyBlobAppend(&sOut,"'",1);` |
|   27 | 2814 | `	for( i = 0 ; i < nIn ; ++i ){` |
|   19 | 2815 | `		if( zIn[i] == '\'' ){` |
|    3 | 2816 | `			SyBlobAppend(&sOut,"'",1);` |
|    1 | 2817 | `		}` |
|   19 | 2818 | `		SyBlobAppend(&sOut,&zIn[i],1);` |
|   10 | 2819 | `	}` |
|    9 | 2820 | `	SyBlobAppend(&sOut,"'",1);` |
|    9 | 2821 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    9 | 2822 | `	SyBlobRelease(&sOut);` |
|    9 | 2823 | `	return PH7_OK;` |
|    6 | 2824 | `}` |
|    - | 2825 | `/*` |
|    - | 2826 | ` * PDO::query(string $query, ...): PDOStatement\|false` |
|    - | 2827 | ` *` |
|    - | 2828 | `` * Prepares and runs ONE statement -- what follows a `;` is compiled but never`` |
|    - | 2829 | ` * executed, unlike exec(), which runs them all.` |
|    - | 2830 | ` */` |
|  226 | 2831 | `static int vm_builtin_PDO_query(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2832 | `{` |
|  228 | 2833 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    - | 2834 | `	phl_pdo_stmt *pSt;` |
|    - | 2835 | `	ph7_class *pClass;` |
|    - | 2836 | `	ph7_class_instance *pObj;` |
|    - | 2837 | `	const char *zSql;` |
|  228 | 2838 | `	int nSql = 0;` |
|  228 | 2839 | `	if( pConn == 0 ){` |
|  ! 0 | 2840 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 2841 | `	}` |
|  228 | 2842 | `	PH7_PdoTouch(pConn);` |
|  228 | 2843 | `	zSql = nArg > 0 ? ph7_value_to_string(apArg[0],&nSql) : 0;` |
|  228 | 2844 | `	if( zSql == 0 \|\| nSql < 1 ){` |
|    3 | 2845 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2846 | `			"PDO::query(): Argument #1 ($query) must not be empty");` |
|    - | 2847 | `	}` |
|  226 | 2848 | `	pSt = PH7_PdoNewStmt(pConn);` |
|  226 | 2849 | `	if( pSt == 0 ){` |
|  ! 0 | 2850 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2851 | `	}` |
|  226 | 2852 | `	if( !PH7_PdoSqlitePrepare(pSt,zSql,nSql) ){` |
|    7 | 2853 | `		ph7_result_bool(pCtx,0);` |
|    7 | 2854 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::query");` |
|    - | 2855 | `	}` |
|  220 | 2856 | `	pSt->bExecuted = 1;` |
|  220 | 2857 | `	if( PdoStmtStep(pSt) < 0 ){` |
|    3 | 2858 | `		ph7_result_bool(pCtx,0);` |
|    3 | 2859 | `		return PH7_PdoRaise(pCtx,pConn,"PDO::query");` |
|    - | 2860 | `	}` |
|    - | 2861 | `	/* the change count is read once, here: a later statement on the same` |
|    - | 2862 | `	 * connection would otherwise move what this one reports */` |
|  218 | 2863 | `	pSt->nChanges = PH7_PdoSqliteColumnCount(pSt) > 0` |
|  111 | 2864 | `		? 0 : PH7_PdoSqliteChanges(pConn);` |
|  218 | 2865 | `	pClass = PdoStatementClass(pCtx,pConn);` |
|  218 | 2866 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|  218 | 2867 | `	if( pObj == 0 ){` |
|  ! 0 | 2868 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2869 | `	}` |
|  218 | 2870 | `	if( PdoStmtAttach(pObj,pSt) != 0 ){` |
|  ! 0 | 2871 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 2872 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2873 | `	}` |
|  218 | 2874 | `	PH7_NativeSetAttrStr(pCtx->pVm,pObj,"queryString",zSql,nSql);` |
|  218 | 2875 | `	PdoStmtOk(pSt);   /* it ran, and it ran cleanly */` |
|    - | 2876 | `	/* PH7_NativeResultObject takes the reference this call made: unref'ing` |
|    - | 2877 | `	 * again here frees the object the result slot is still holding. */` |
|  218 | 2878 | `	PH7_NativeResultObject(pCtx,pObj);` |
|  218 | 2879 | `	return PH7_OK;` |
|  115 | 2880 | `}` |
|    - | 2881 |  |
|    - | 2882 | `/* ------------------------------------------------------------------------` |
|    - | 2883 | ` * Transactions` |
|    - | 2884 | ` * ------------------------------------------------------------------------ */` |
|    - | 2885 | `/*` |
|    - | 2886 | ` * PDO::beginTransaction(): bool / commit() / rollBack() / inTransaction()` |
|    - | 2887 | ` *` |
|    - | 2888 | ` * Whether a transaction is open is sqlite's own autocommit flag and not a` |
|    - | 2889 | ` * count this driver keeps, so a BEGIN the script sent through exec() is` |
|    - | 2890 | ` * indistinguishable from beginTransaction() -- inTransaction() answers true` |
|    - | 2891 | ` * for it and a second beginTransaction() refuses.` |
|    - | 2892 | ` *` |
|    - | 2893 | ` * The three refusals are bare sentences with no SQLSTATE in front of them,` |
|    - | 2894 | ` * which is unlike every other PDOException the driver raises; and the four` |
|    - | 2895 | ` * verbs are the ones that do NOT clear the handle's error on entry.` |
|    - | 2896 | ` */` |
|   28 | 2897 | `static int PdoTxRun(ph7_context *pCtx,const char *zSql,const char *zFn)` |
|    1 | 2898 | `{` |
|   29 | 2899 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|   29 | 2900 | `	if( pConn == 0 ){` |
|  ! 0 | 2901 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 2902 | `	}` |
|   29 | 2903 | `	if( PH7_PdoSqliteExec(pConn,zSql,(int)SyStrlen(zSql)) < 0 ){` |
|  ! 0 | 2904 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2905 | `		return PH7_PdoRaise(pCtx,pConn,zFn);` |
|    - | 2906 | `	}` |
|   29 | 2907 | `	ph7_result_bool(pCtx,1);` |
|   29 | 2908 | `	return PH7_OK;` |
|   15 | 2909 | `}` |
|   16 | 2910 | `static int vm_builtin_PDO_beginTransaction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2911 | `{` |
|   17 | 2912 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|   17 | 2913 | `	const char *zBegin = "BEGIN";` |
|    8 | 2914 | `	SXUNUSED(nArg);` |
|    8 | 2915 | `	SXUNUSED(apArg);` |
|   17 | 2916 | `	if( pConn == 0 ){` |
|  ! 0 | 2917 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 2918 | `	}` |
|   17 | 2919 | `	if( PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 2920 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 2921 | `			"There is already an active transaction");` |
|    - | 2922 | `	}` |
|    - | 2923 | `	/* which BEGIN, per Pdo\Sqlite::ATTR_TRANSACTION_MODE */` |
|   15 | 2924 | `	if( pConn->iTxMode == 1 ){` |
|    3 | 2925 | `		zBegin = "BEGIN IMMEDIATE";` |
|   14 | 2926 | `	}else if( pConn->iTxMode == 2 ){` |
|    3 | 2927 | `		zBegin = "BEGIN EXCLUSIVE";` |
|    1 | 2928 | `	}` |
|   15 | 2929 | `	return PdoTxRun(pCtx,zBegin,"PDO::beginTransaction");` |
|    9 | 2930 | `}` |
|   12 | 2931 | `static int vm_builtin_PDO_commit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2932 | `{` |
|   13 | 2933 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    6 | 2934 | `	SXUNUSED(nArg);` |
|    6 | 2935 | `	SXUNUSED(apArg);` |
|   13 | 2936 | `	if( pConn == 0 ){` |
|  ! 0 | 2937 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 2938 | `	}` |
|   13 | 2939 | `	if( !PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 2940 | `		return PH7_VmThrowException(pCtx,"PDOException","There is no active transaction");` |
|    - | 2941 | `	}` |
|   11 | 2942 | `	return PdoTxRun(pCtx,"COMMIT","PDO::commit");` |
|    7 | 2943 | `}` |
|    6 | 2944 | `static int vm_builtin_PDO_rollBack(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2945 | `{` |
|    7 | 2946 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    3 | 2947 | `	SXUNUSED(nArg);` |
|    3 | 2948 | `	SXUNUSED(apArg);` |
|    7 | 2949 | `	if( pConn == 0 ){` |
|  ! 0 | 2950 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 2951 | `	}` |
|    7 | 2952 | `	if( !PH7_PdoSqliteInTransaction(pConn) ){` |
|    3 | 2953 | `		return PH7_VmThrowException(pCtx,"PDOException","There is no active transaction");` |
|    - | 2954 | `	}` |
|    5 | 2955 | `	return PdoTxRun(pCtx,"ROLLBACK","PDO::rollBack");` |
|    4 | 2956 | `}` |
|   10 | 2957 | `static int vm_builtin_PDO_inTransaction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2958 | `{` |
|   11 | 2959 | `	phl_pdo *pConn = PdoOfInstance(PH7_ContextThis(pCtx));` |
|    5 | 2960 | `	SXUNUSED(nArg);` |
|    5 | 2961 | `	SXUNUSED(apArg);` |
|   11 | 2962 | `	if( pConn == 0 ){` |
|  ! 0 | 2963 | `		return PH7_VmThrowException(pCtx,"Error","PDO object is uninitialized");` |
|    - | 2964 | `	}` |
|   11 | 2965 | `	ph7_result_bool(pCtx,PH7_PdoSqliteInTransaction(pConn));` |
|   11 | 2966 | `	return PH7_OK;` |
|    6 | 2967 | `}` |
|    - | 2968 |  |
|    - | 2969 | `/* ------------------------------------------------------------------------` |
|    - | 2970 | ` * Connecting` |
|    - | 2971 | ` * ------------------------------------------------------------------------ */` |
|    - | 2972 | `/*` |
|    - | 2973 | `` * Apply the constructor's `?array $options`.  php walks it before the driver`` |
|    - | 2974 | ` * sees the handle, so an ATTR_ERRMODE in there is already in force when a` |
|    - | 2975 | ` * later failure is routed -- and a key no driver knows is ignored in silence.` |
|    - | 2976 | ` */` |
|   42 | 2977 | `static void PdoApplyOptions(phl_pdo *pConn,ph7_value *pOptions)` |
|    2 | 2978 | `{` |
|    - | 2979 | `	ph7_hashmap *pMap;` |
|    - | 2980 | `	ph7_hashmap_node *pEntry;` |
|    - | 2981 | `	sxu32 n,nCount;` |
|   44 | 2982 | `	if( pOptions == 0 \|\| (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 2983 | `		return;` |
|    - | 2984 | `	}` |
|   44 | 2985 | `	pMap = (ph7_hashmap *)pOptions->x.pOther;` |
|   44 | 2986 | `	nCount = pMap->nEntry;` |
|   44 | 2987 | `	pEntry = pMap->pFirst;` |
|   88 | 2988 | `	for( n = 0 ; n < nCount && pEntry ; ++n, pEntry = pEntry->pPrev ){` |
|    - | 2989 | `		ph7_value sKey;` |
|    - | 2990 | `		ph7_value *pVal;` |
|    - | 2991 | `		ph7_int64 iKey;` |
|   46 | 2992 | `		if( pEntry->iType != HASHMAP_INT_NODE ){` |
|  ! 0 | 2993 | `			continue;  /* php ignores a string key here */` |
|    - | 2994 | `		}` |
|   46 | 2995 | `		PH7_MemObjInit(pConn->pVm,&sKey);` |
|   46 | 2996 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   46 | 2997 | `		iKey = sKey.x.iVal;` |
|   46 | 2998 | `		PH7_MemObjRelease(&sKey);` |
|   46 | 2999 | `		pVal = (ph7_value *)SySetAt(&pConn->pVm->aMemObj,pEntry->nValIdx);` |
|   46 | 3000 | `		if( pVal == 0 ){` |
|  ! 0 | 3001 | `			continue;` |
|    - | 3002 | `		}` |
|   46 | 3003 | `		switch( iKey ){` |
|   24 | 3004 | `			case PDO_ATTR_ERRMODE:            pConn->iErrMode = (int)ph7_value_to_int64(pVal); break;` |
|    7 | 3005 | `			case PDO_ATTR_CASE:               pConn->iCase = (int)ph7_value_to_int64(pVal); break;` |
|    7 | 3006 | `			case PDO_ATTR_ORACLE_NULLS:       pConn->iOracleNulls = (int)ph7_value_to_int64(pVal); break;` |
|    3 | 3007 | `			case PDO_ATTR_DEFAULT_FETCH_MODE: pConn->iDefaultFetch = (int)ph7_value_to_int64(pVal); break;` |
|    3 | 3008 | `			case PDO_ATTR_STRINGIFY_FETCHES:  pConn->bStringify = ph7_value_to_bool(pVal); break;` |
|    - | 3009 | `			/* php remembers this one and reports it back, though a CLI process` |
|    - | 3010 | `			 * has no pool to keep the handle in. */` |
|    3 | 3011 | `			case PDO_ATTR_PERSISTENT:         pConn->bPersistent = ph7_value_to_bool(pVal); break;` |
|  ! 0 | 3012 | `			case PDO_SQLITE_ATTR_TRANSACTION_MODE: pConn->iTxMode = (int)ph7_value_to_int64(pVal); break;` |
|    - | 3013 | `			/* the OPEN flags are read here and used by the open itself, which is` |
|    - | 3014 | `			 * the only moment they mean anything */` |
|    3 | 3015 | `			case PDO_SQLITE_ATTR_OPEN_FLAGS: pConn->iOpenFlags = (int)ph7_value_to_int64(pVal); break;` |
|  ! 0 | 3016 | `			case PDO_SQLITE_ATTR_EXTENDED_RESULT_CODES: pConn->bExtendedCodes = ph7_value_to_bool(pVal); break;` |
|    2 | 3017 | `			default: break;` |
|    - | 3018 | `		}` |
|   24 | 3019 | `	}` |
|   23 | 3020 | `}` |
|    - | 3021 | `/*` |
|    - | 3022 | `` * php's `uri:` DSN: the real DSN is the FIRST LINE of what that URI names, read`` |
|    - | 3023 | ` * through the ordinary stream layer -- so a php:// wrapper or a userland one is` |
|    - | 3024 | ` * a source too. The line is used verbatim, which is why one with leading` |
|    - | 3025 | `` * whitespace, or a second `uri:`, comes back as "could not find driver" rather`` |
|    - | 3026 | ` * than anything more specific: it is simply parsed as a driver name.` |
|    - | 3027 | ` *` |
|    - | 3028 | ` * Returns 0 when the URI could not be read (the caller words the refusal); the` |
|    - | 3029 | ` * open warning underneath it is the stream layer's own, attributed to` |
|    - | 3030 | ` * PDO::__construct the way php attributes it.` |
|    - | 3031 | ` */` |
|    8 | 3032 | `static int PdoResolveUriDsn(ph7_context *pCtx,const char *zUri,int nUri,SyBlob *pOut)` |
|    1 | 3033 | `{` |
|    - | 3034 | `	const ph7_io_stream *pStream;` |
|    - | 3035 | `	const char *zFile,*zWhole;` |
|    - | 3036 | `	void *pHandle;` |
|    - | 3037 | `	SyBlob sRaw;` |
|    - | 3038 | `	const char *zRaw;` |
|    - | 3039 | `	sxu32 n,nRaw;` |
|    9 | 3040 | `	int rc = 0;` |
|    9 | 3041 | `	if( nUri < 1 ){` |
|  ! 0 | 3042 | `		return 0;` |
|    - | 3043 | `	}` |
|    - | 3044 | `	/* the slice is not a C string, and the stream layer wants one */` |
|    9 | 3045 | `	SyBlobInit(&sRaw,&pCtx->pVm->sAllocator);` |
|    9 | 3046 | `	SyBlobAppend(&sRaw,zUri,(sxu32)nUri);` |
|    9 | 3047 | `	SyBlobAppend(&sRaw,"",1);` |
|    9 | 3048 | `	zFile = zWhole = (const char *)SyBlobData(&sRaw);` |
|    9 | 3049 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nUri);` |
|    9 | 3050 | `	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|    4 | 3051 | `		FALSE,0,FALSE,0,"PDO::__construct") : 0;` |
|    9 | 3052 | `	if( pHandle == 0 ){` |
|    - | 3053 | `		/* the device lookup advanced zFile past the wrapper prefix; php's warning` |
|    - | 3054 | ``		 * names the URI the SCRIPT wrote, `file:///nope` and not `/nope` */`` |
|    4 | 3055 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"PDO::__construct(%s): Failed to open stream: %s",` |
|    1 | 3056 | `			zWhole,"No such file or directory");` |
|    3 | 3057 | `		SyBlobRelease(&sRaw);` |
|    3 | 3058 | `		return 0;` |
|    - | 3059 | `	}` |
|    7 | 3060 | `	SyBlobReset(&sRaw);` |
|    7 | 3061 | `	if( PH7_StreamReadWholeFile(pHandle,pStream,&sRaw) == SXRET_OK ){` |
|    7 | 3062 | `		zRaw = (const char *)SyBlobData(&sRaw);` |
|    7 | 3063 | `		nRaw = SyBlobLength(&sRaw);` |
|    - | 3064 | `		/* php reads ONE line and keeps its terminator, so a file written with a` |
|    - | 3065 | `		 * trailing newline yields a DSN that ends in one -- which reaches sqlite` |
|    - | 3066 | ``		 * as part of the PATH. That is why `sqlite::memory:\n` opens a FILE of`` |
|    - | 3067 | `		 * that name rather than a memory database; the newline is not noise the` |
|    - | 3068 | `		 * driver trims, and trimming it here would answer differently. */` |
|  109 | 3069 | `		for( n = 0 ; n < nRaw && zRaw[n] != '\n' ; ++n ){}` |
|    7 | 3070 | `		if( n < nRaw ){` |
|  ! 0 | 3071 | `			++n;  /* the newline belongs to the line */` |
|  ! 0 | 3072 | `		}` |
|    7 | 3073 | `		if( n > 0 ){` |
|    7 | 3074 | `			SyBlobAppend(pOut,zRaw,n);` |
|    7 | 3075 | `			rc = 1;` |
|    3 | 3076 | `		}` |
|    3 | 3077 | `	}` |
|    7 | 3078 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    7 | 3079 | `	SyBlobRelease(&sRaw);` |
|    7 | 3080 | `	return rc;` |
|    5 | 3081 | `}` |
|    - | 3082 | `/*` |
|    - | 3083 | ` * The driver split, over a DSN that is already resolved.  php reads up to the` |
|    - | 3084 | `` * first `:` as the driver name and hands the rest to that driver; the name is`` |
|    - | 3085 | `` * matched case-SENSITIVELY, so `SQLITE:` is "could not find driver" rather`` |
|    - | 3086 | ` * than a connection, and a DSN with no colon at all is refused before any` |
|    - | 3087 | ` * driver is looked for.` |
|    - | 3088 | ` */` |
|  144 | 3089 | `static int PdoOpenParsed(ph7_context *pCtx,phl_pdo *pConn,const char *zDsn,int nDsn)` |
|    3 | 3090 | `{` |
|    - | 3091 | `	int nDriver;` |
| 1023 | 3092 | `	for( nDriver = 0 ; nDriver < nDsn && zDsn[nDriver] != ':' ; ++nDriver ){}` |
|  147 | 3093 | `	if( nDriver >= nDsn ){` |
|    - | 3094 | `		/* no colon: php refuses the ARGUMENT, not the driver */` |
|    5 | 3095 | `		return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 3096 | `			"PDO::__construct(): Argument #1 ($dsn) must be a valid data source name");` |
|    - | 3097 | `	}` |
|  140 | 3098 | `	if( nDriver != (int)sizeof("sqlite")-1` |
|  138 | 3099 | `	 \|\| SyMemcmp(zDsn,"sqlite",sizeof("sqlite")-1) != 0 ){` |
|    - | 3100 | `		/* §10 scopes this build to one driver, so every other name -- and every` |
|    - | 3101 | `		 * other SPELLING of this one -- is what a php without that driver says. */` |
|   13 | 3102 | `		return PH7_VmThrowException(pCtx,"PDOException","could not find driver");` |
|    - | 3103 | `	}` |
|    - | 3104 | `	/* SQLITE_OPEN_URI is passed EXPLICITLY rather than left to the linked` |
|    - | 3105 | `	 * library's compile-time default: a Debian libsqlite3 is built with URI` |
|    - | 3106 | `` 	 * filenames on and a vcpkg one is not, so `sqlite:file::memory:?cache=shared` `` |
|    - | 3107 | `	 * opened a memory database on one platform and created a FILE of that name on` |
|    - | 3108 | `	 * the other. php's own sqlite has them on, so on is the answer everywhere. */` |
|    - | 3109 | `	{` |
|    - | 3110 | `		/* the script's own ATTR_OPEN_FLAGS replace the read-write default */` |
|  195 | 3111 | `		int iFlags = pConn->iOpenFlags` |
|    1 | 3112 | `			? pConn->iOpenFlags` |
|   64 | 3113 | `			: (SQLITE_OPEN_READWRITE\|SQLITE_OPEN_CREATE);` |
|  195 | 3114 | `		return PH7_PdoSqliteOpen(pCtx,pConn,zDsn + nDriver + 1,nDsn - nDriver - 1,` |
|   64 | 3115 | `			iFlags\|SQLITE_OPEN_URI);` |
|    - | 3116 | `	}` |
|   75 | 3117 | `}` |
|    - | 3118 | `/*` |
|    - | 3119 | `` * One DSN, resolved then split: php's `uri:` form is read first, and what it`` |
|    - | 3120 | ` * names replaces the DSN whole.` |
|    - | 3121 | ` */` |
|  146 | 3122 | `static int PdoOpenFromDsn(ph7_context *pCtx,phl_pdo *pConn,ph7_value *pDsn)` |
|    3 | 3123 | `{` |
|    - | 3124 | `	const char *zDsn;` |
|    - | 3125 | `	int nDsn,rc;` |
|    - | 3126 | `	SyBlob sResolved;` |
|  149 | 3127 | `	if( pDsn == 0 ){` |
|  ! 0 | 3128 | `		nDsn = 0;` |
|  ! 0 | 3129 | `		zDsn = "";` |
|  ! 0 | 3130 | `	}else{` |
|  149 | 3131 | `		zDsn = ph7_value_to_string(pDsn,&nDsn);` |
|    - | 3132 | `	}` |
|  149 | 3133 | `	SyBlobInit(&sResolved,&pCtx->pVm->sAllocator);` |
|  149 | 3134 | `	if( nDsn >= (int)sizeof("uri:")-1 && SyMemcmp(zDsn,"uri:",sizeof("uri:")-1) == 0 ){` |
|    9 | 3135 | `		if( !PdoResolveUriDsn(pCtx,zDsn + sizeof("uri:")-1,nDsn - ((int)sizeof("uri:")-1),` |
|    - | 3136 | `			&sResolved) ){` |
|    3 | 3137 | `			SyBlobRelease(&sResolved);` |
|    3 | 3138 | `			return PH7_VmThrowException(pCtx,"PDOException",` |
|    - | 3139 | `				"PDO::__construct(): Argument #1 ($dsn) must be a valid data source URI");` |
|    - | 3140 | `		}` |
|    - | 3141 | `		/* the line replaces the DSN whole, and is NOT resolved again: a nested` |
|    - | 3142 | ``		 * `uri:` is read as a driver name, exactly as php reads it */`` |
|    7 | 3143 | `		zDsn = (const char *)SyBlobData(&sResolved);` |
|    7 | 3144 | `		nDsn = (int)SyBlobLength(&sResolved);` |
|    3 | 3145 | `	}` |
|  147 | 3146 | `	rc = PdoOpenParsed(pCtx,pConn,zDsn,nDsn);` |
|  147 | 3147 | `	SyBlobRelease(&sResolved);` |
|  147 | 3148 | `	return rc;` |
|   76 | 3149 | `}` |
|    - | 3150 | `/*` |
|    - | 3151 | ` * PDO::__construct(string $dsn, ?string $username = null, ?string $password = null,` |
|    - | 3152 | ` *                  ?array $options = null)` |
|    - | 3153 | ` *` |
|    - | 3154 | ` * The two credential arguments are the generic surface: sqlite has no user to` |
|    - | 3155 | ` * be, so php accepts and ignores them rather than refusing a portable call.` |
|    - | 3156 | ` */` |
|  126 | 3157 | `static int vm_builtin_PDO___construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 3158 | `{` |
|  129 | 3159 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - | 3160 | `	phl_pdo *pConn;` |
|    - | 3161 | `	int rc;` |
|  129 | 3162 | `	if( pThis == 0 ){` |
|  ! 0 | 3163 | `		return PH7_VmThrowException(pCtx,"Error","PDO::__construct() needs a receiver");` |
|    - | 3164 | `	}` |
|  129 | 3165 | `	pConn = PH7_PdoNewConn(pCtx->pVm);` |
|  129 | 3166 | `	if( pConn == 0 ){` |
|  ! 0 | 3167 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3168 | `	}` |
|  129 | 3169 | `	if( PdoAttach(pThis,pConn) != 0 ){` |
|  ! 0 | 3170 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3171 | `	}` |
|    - | 3172 | `	/* the options are read BEFORE the open, so an ATTR_ERRMODE they carry is` |
|    - | 3173 | `	 * already in force for everything that follows */` |
|  129 | 3174 | `	if( nArg > 3 ){` |
|   42 | 3175 | `		PdoApplyOptions(pConn,apArg[3]);` |
|   20 | 3176 | `	}` |
|  129 | 3177 | `	rc = PdoOpenFromDsn(pCtx,pConn,nArg > 0 ? apArg[0] : 0);` |
|  129 | 3178 | `	return rc;` |
|   66 | 3179 | `}` |
|    - | 3180 | `/*` |
|    - | 3181 | ` * static PDO::connect(string $dsn, ...): static` |
|    - | 3182 | ` *` |
|    - | 3183 | `` * php 8.4's replacement for `new PDO(...)`: same arguments, but the object it`` |
|    - | 3184 | `` * answers is the DRIVER's subclass -- `Pdo\Sqlite` here -- so the`` |
|    - | 3185 | ` * sqlite-specific verbs are callable on it without a cast. Called on a` |
|    - | 3186 | `` * subclass it answers that subclass, which is what `static` means.`` |
|    - | 3187 | ` */` |
|   20 | 3188 | `static int vm_builtin_PDO_connect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3189 | `{` |
|   22 | 3190 | `	ph7_vm *pVm = pCtx->pVm;` |
|   22 | 3191 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|    - | 3192 | `	ph7_class_instance *pObj;` |
|    - | 3193 | `	phl_pdo *pConn;` |
|    - | 3194 | `	int rc;` |
|   22 | 3195 | `	if( pClass == 0 \|\| SyStrncmp(pClass->sName.zString,"PDO",sizeof("PDO")-1) == 0 ){` |
|    - | 3196 | `		/* PDO::connect() itself answers the driver's class, not PDO */` |
|    5 | 3197 | `		ph7_class *pDrv = PH7_VmExtractClass(&(*pVm),"Pdo\\Sqlite",` |
|    - | 3198 | `			sizeof("Pdo\\Sqlite")-1,FALSE,0);` |
|    5 | 3199 | `		if( pDrv ){` |
|    5 | 3200 | `			pClass = pDrv;` |
|    2 | 3201 | `		}` |
|    2 | 3202 | `	}` |
|   22 | 3203 | `	if( pClass == 0 ){` |
|  ! 0 | 3204 | `		return PH7_VmThrowException(pCtx,"Error","Pdo\\Sqlite is not available");` |
|    - | 3205 | `	}` |
|   22 | 3206 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|   22 | 3207 | `	if( pObj == 0 ){` |
|  ! 0 | 3208 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3209 | `	}` |
|   22 | 3210 | `	pConn = PH7_PdoNewConn(&(*pVm));` |
|   22 | 3211 | `	if( pConn == 0 \|\| PdoAttach(pObj,pConn) != 0 ){` |
|  ! 0 | 3212 | `		PH7_ClassInstanceUnref(pObj);` |
|  ! 0 | 3213 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3214 | `	}` |
|   22 | 3215 | `	if( nArg > 3 ){` |
|    3 | 3216 | `		PdoApplyOptions(pConn,apArg[3]);` |
|    1 | 3217 | `	}` |
|   22 | 3218 | `	rc = PdoOpenFromDsn(pCtx,pConn,nArg > 0 ? apArg[0] : 0);` |
|   22 | 3219 | `	if( rc != PH7_OK ){` |
|    3 | 3220 | `		PH7_ClassInstanceUnref(pObj);` |
|    3 | 3221 | `		return rc;` |
|    - | 3222 | `	}` |
|   20 | 3223 | `	PH7_NativeResultObject(pCtx,pObj);` |
|   20 | 3224 | `	return PH7_OK;` |
|   12 | 3225 | `}` |
|    - | 3226 | `/*` |
|    - | 3227 | ` * PDO::getAvailableDrivers(): the one name this build carries.  php answers` |
|    - | 3228 | ` * the list of drivers its ext/pdo actually loaded, which is why an engine` |
|    - | 3229 | ` * with no driver at all answers [] -- here it is always ["sqlite"].` |
|    - | 3230 | ` */` |
|    2 | 3231 | `static int vm_builtin_PDO_getAvailableDrivers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3232 | `{` |
|    - | 3233 | `	ph7_value *pArray, *pName;` |
|    1 | 3234 | `	SXUNUSED(nArg);` |
|    1 | 3235 | `	SXUNUSED(apArg);` |
|    3 | 3236 | `	pArray = ph7_context_new_array(pCtx);` |
|    3 | 3237 | `	pName  = ph7_context_new_scalar(pCtx);` |
|    3 | 3238 | `	if( pArray == 0 \|\| pName == 0 ){` |
|  ! 0 | 3239 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 | 3240 | `		ph7_result_null(pCtx);` |
|  ! 0 | 3241 | `		return PH7_OK;` |
|    - | 3242 | `	}` |
|    3 | 3243 | `	ph7_value_string(pName,"sqlite",sizeof("sqlite")-1);` |
|    3 | 3244 | `	ph7_array_add_elem(pArray,0,pName);` |
|    3 | 3245 | `	ph7_result_value(pCtx,pArray);` |
|    3 | 3246 | `	return PH7_OK;` |
|    2 | 3247 | `}` |
|    - | 3248 |  |
|    - | 3249 | `/*` |
|    - | 3250 | ` * Install the PDO class library.  Called from PH7_VmInit inside the` |
|    - | 3251 | ` * bCompilingBuiltin window; vm_pdo_sqlite.c's installer runs right after and` |
|    - | 3252 | `` * needs PDO to already be mounted (it is the parent of `Pdo\Sqlite`).`` |
|    - | 3253 | ` */` |
| 5254 | 3254 | `PH7_PRIVATE sxi32 PH7_VmInstallPdo(ph7_vm *pVm)` |
|    5 | 3255 | `{` |
|    - | 3256 | `	/* php's own constant values, in its own declaration order. The seven` |
|    - | 3257 | `	 * deprecated PDO::SQLITE_* rows php still carries are absent by §10; their` |
|    - | 3258 | `	 * successors are declared on Pdo\Sqlite (vm_pdo_sqlite.c). */` |
|    - | 3259 | `#define PDO_INT_CONST(NAME,VALUE) \` |
|    - | 3260 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (ph7_int64)(VALUE), 0, 0.0 }` |
|    - | 3261 | `	static const PH7_NativeConstDef aPdoConst[] = {` |
|    - | 3262 | `		PDO_INT_CONST("PARAM_NULL",             0),` |
|    - | 3263 | `		PDO_INT_CONST("PARAM_BOOL",             5),` |
|    - | 3264 | `		PDO_INT_CONST("PARAM_INT",              1),` |
|    - | 3265 | `		PDO_INT_CONST("PARAM_STR",              2),` |
|    - | 3266 | `		PDO_INT_CONST("PARAM_LOB",              3),` |
|    - | 3267 | `		PDO_INT_CONST("PARAM_STMT",             4),` |
|    - | 3268 | `		PDO_INT_CONST("PARAM_INPUT_OUTPUT",     2147483648LL),` |
|    - | 3269 | `		PDO_INT_CONST("PARAM_STR_NATL",         1073741824LL),` |
|    - | 3270 | `		PDO_INT_CONST("PARAM_STR_CHAR",         536870912LL),` |
|    - | 3271 | `		PDO_INT_CONST("PARAM_EVT_ALLOC",        0),` |
|    - | 3272 | `		PDO_INT_CONST("PARAM_EVT_FREE",         1),` |
|    - | 3273 | `		PDO_INT_CONST("PARAM_EVT_EXEC_PRE",     2),` |
|    - | 3274 | `		PDO_INT_CONST("PARAM_EVT_EXEC_POST",    3),` |
|    - | 3275 | `		PDO_INT_CONST("PARAM_EVT_FETCH_PRE",    4),` |
|    - | 3276 | `		PDO_INT_CONST("PARAM_EVT_FETCH_POST",   5),` |
|    - | 3277 | `		PDO_INT_CONST("PARAM_EVT_NORMALIZE",    6),` |
|    - | 3278 | `		PDO_INT_CONST("FETCH_DEFAULT",          0),` |
|    - | 3279 | `		PDO_INT_CONST("FETCH_LAZY",             1),` |
|    - | 3280 | `		PDO_INT_CONST("FETCH_ASSOC",            2),` |
|    - | 3281 | `		PDO_INT_CONST("FETCH_NUM",              3),` |
|    - | 3282 | `		PDO_INT_CONST("FETCH_BOTH",             4),` |
|    - | 3283 | `		PDO_INT_CONST("FETCH_OBJ",              5),` |
|    - | 3284 | `		PDO_INT_CONST("FETCH_BOUND",            6),` |
|    - | 3285 | `		PDO_INT_CONST("FETCH_COLUMN",           7),` |
|    - | 3286 | `		PDO_INT_CONST("FETCH_CLASS",            8),` |
|    - | 3287 | `		PDO_INT_CONST("FETCH_INTO",             9),` |
|    - | 3288 | `		PDO_INT_CONST("FETCH_FUNC",            10),` |
|    - | 3289 | `		PDO_INT_CONST("FETCH_GROUP",           32),` |
|    - | 3290 | `		PDO_INT_CONST("FETCH_UNIQUE",          64),` |
|    - | 3291 | `		PDO_INT_CONST("FETCH_KEY_PAIR",        12),` |
|    - | 3292 | `		PDO_INT_CONST("FETCH_CLASSTYPE",      128),` |
|    - | 3293 | `		PDO_INT_CONST("FETCH_SERIALIZE",      512),` |
|    - | 3294 | `		PDO_INT_CONST("FETCH_PROPS_LATE",     256),` |
|    - | 3295 | `		PDO_INT_CONST("FETCH_NAMED",           11),` |
|    - | 3296 | `		PDO_INT_CONST("ATTR_AUTOCOMMIT",        0),` |
|    - | 3297 | `		PDO_INT_CONST("ATTR_PREFETCH",          1),` |
|    - | 3298 | `		PDO_INT_CONST("ATTR_TIMEOUT",           2),` |
|    - | 3299 | `		PDO_INT_CONST("ATTR_ERRMODE",           3),` |
|    - | 3300 | `		PDO_INT_CONST("ATTR_SERVER_VERSION",    4),` |
|    - | 3301 | `		PDO_INT_CONST("ATTR_CLIENT_VERSION",    5),` |
|    - | 3302 | `		PDO_INT_CONST("ATTR_SERVER_INFO",       6),` |
|    - | 3303 | `		PDO_INT_CONST("ATTR_CONNECTION_STATUS", 7),` |
|    - | 3304 | `		PDO_INT_CONST("ATTR_CASE",              8),` |
|    - | 3305 | `		PDO_INT_CONST("ATTR_CURSOR_NAME",       9),` |
|    - | 3306 | `		PDO_INT_CONST("ATTR_CURSOR",           10),` |
|    - | 3307 | `		PDO_INT_CONST("ATTR_ORACLE_NULLS",     11),` |
|    - | 3308 | `		PDO_INT_CONST("ATTR_PERSISTENT",       12),` |
|    - | 3309 | `		PDO_INT_CONST("ATTR_STATEMENT_CLASS",  13),` |
|    - | 3310 | `		PDO_INT_CONST("ATTR_FETCH_TABLE_NAMES",14),` |
|    - | 3311 | `		PDO_INT_CONST("ATTR_FETCH_CATALOG_NAMES",15),` |
|    - | 3312 | `		PDO_INT_CONST("ATTR_DRIVER_NAME",      16),` |
|    - | 3313 | `		PDO_INT_CONST("ATTR_STRINGIFY_FETCHES",17),` |
|    - | 3314 | `		PDO_INT_CONST("ATTR_MAX_COLUMN_LEN",   18),` |
|    - | 3315 | `		PDO_INT_CONST("ATTR_EMULATE_PREPARES", 20),` |
|    - | 3316 | `		PDO_INT_CONST("ATTR_DEFAULT_FETCH_MODE",19),` |
|    - | 3317 | `		PDO_INT_CONST("ATTR_DEFAULT_STR_PARAM",21),` |
|    - | 3318 | `		PDO_INT_CONST("ERRMODE_SILENT",         0),` |
|    - | 3319 | `		PDO_INT_CONST("ERRMODE_WARNING",        1),` |
|    - | 3320 | `		PDO_INT_CONST("ERRMODE_EXCEPTION",      2),` |
|    - | 3321 | `		PDO_INT_CONST("CASE_NATURAL",           0),` |
|    - | 3322 | `		PDO_INT_CONST("CASE_LOWER",             2),` |
|    - | 3323 | `		PDO_INT_CONST("CASE_UPPER",             1),` |
|    - | 3324 | `		PDO_INT_CONST("NULL_NATURAL",           0),` |
|    - | 3325 | `		PDO_INT_CONST("NULL_EMPTY_STRING",      1),` |
|    - | 3326 | `		PDO_INT_CONST("NULL_TO_STRING",         2),` |
|    - | 3327 | `		{ "ERR_NONE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, "00000", 0.0 },` |
|    - | 3328 | `		PDO_INT_CONST("FETCH_ORI_NEXT",         0),` |
|    - | 3329 | `		PDO_INT_CONST("FETCH_ORI_PRIOR",        1),` |
|    - | 3330 | `		PDO_INT_CONST("FETCH_ORI_FIRST",        2),` |
|    - | 3331 | `		PDO_INT_CONST("FETCH_ORI_LAST",         3),` |
|    - | 3332 | `		PDO_INT_CONST("FETCH_ORI_ABS",          4),` |
|    - | 3333 | `		PDO_INT_CONST("FETCH_ORI_REL",          5),` |
|    - | 3334 | `		PDO_INT_CONST("CURSOR_FWDONLY",         0),` |
|    - | 3335 | `		PDO_INT_CONST("CURSOR_SCROLL",          1),` |
|    - | 3336 | `	};` |
|    - | 3337 | `	/* php's own signatures and its own stub ORDER: Reflection and` |
|    - | 3338 | `	 * get_class_methods() both answer declaration order, so the two engines` |
|    - | 3339 | `	 * must list one surface. Nearly every return type is TENTATIVE in php's` |
|    - | 3340 | ``	 * stubs (the leading `@`), which is a php-visible difference from a`` |
|    - | 3341 | `	 * declared one -- getReturnType() answers null for a tentative type. */` |
|    - | 3342 | `	static const PH7_NativeMethodDef aPdoMethod[] = {` |
|    - | 3343 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|    - | 3344 | `		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",` |
|    - | 3345 | `		  0, vm_builtin_PDO___construct },` |
|    - | 3346 | `		{ "connect", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|    - | 3347 | `		  "string $dsn, ?string $username = null, ?string $password = null, ?array $options = null",` |
|    - | 3348 | `		  "static", vm_builtin_PDO_connect },` |
|    - | 3349 | `		{ "beginTransaction", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_beginTransaction },` |
|    - | 3350 | `		{ "commit",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_commit },` |
|    - | 3351 | `		{ "errorCode",        PH7_MOD_PUBLIC, "", "@?string", vm_builtin_PDO_errorCode },` |
|    - | 3352 | `		{ "errorInfo",        PH7_MOD_PUBLIC, "", "@array", vm_builtin_PDO_errorInfo },` |
|    - | 3353 | `		{ "exec",             PH7_MOD_PUBLIC, "string $statement", "@int\|false",` |
|    - | 3354 | `		  vm_builtin_PDO_exec },` |
|    - | 3355 | `		{ "getAttribute",     PH7_MOD_PUBLIC, "int $attribute", "@mixed",` |
|    - | 3356 | `		  vm_builtin_PDO_getAttribute },` |
|    - | 3357 | `		{ "getAvailableDrivers", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "@array",` |
|    - | 3358 | `		  vm_builtin_PDO_getAvailableDrivers },` |
|    - | 3359 | `		{ "inTransaction",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_inTransaction },` |
|    - | 3360 | `		{ "lastInsertId",     PH7_MOD_PUBLIC, "?string $name = null", "@string\|false",` |
|    - | 3361 | `		  vm_builtin_PDO_lastInsertId },` |
|    - | 3362 | `		{ "prepare",          PH7_MOD_PUBLIC, "string $query, array $options = []",` |
|    - | 3363 | `		  "@PDOStatement\|false", vm_builtin_PDO_prepare },` |
|    - | 3364 | `		{ "query",            PH7_MOD_PUBLIC,` |
|    - | 3365 | `		  "string $query, ?int $fetchMode = null, mixed ...$fetchModeArgs",` |
|    - | 3366 | `		  "@PDOStatement\|false", vm_builtin_PDO_query },` |
|    - | 3367 | `		{ "quote",            PH7_MOD_PUBLIC, "string $string, int $type = PDO::PARAM_STR",` |
|    - | 3368 | `		  "@string\|false", vm_builtin_PDO_quote },` |
|    - | 3369 | `		{ "rollBack",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDO_rollBack },` |
|    - | 3370 | `		{ "setAttribute",     PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",` |
|    - | 3371 | `		  vm_builtin_PDO_setAttribute },` |
|    - | 3372 | `	};` |
|    - | 3373 | `	static const PH7_NativeMethodDef aStmtMethod[] = {` |
|    - | 3374 | `		{ "bindColumn",   PH7_MOD_PUBLIC,` |
|    - | 3375 | `		  "string\|int $column, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "` |
|    - | 3376 | `		  "mixed $driverOptions = null", "@bool", vm_builtin_PDOStatement_bindColumn },` |
|    - | 3377 | `		{ "bindParam",    PH7_MOD_PUBLIC,` |
|    - | 3378 | `		  "string\|int $param, mixed &$var, int $type = PDO::PARAM_STR, int $maxLength = 0, "` |
|    - | 3379 | `		  "mixed $driverOptions = null", "@bool", vm_builtin_PDOStatement_bindParam },` |
|    - | 3380 | `		{ "bindValue",    PH7_MOD_PUBLIC,` |
|    - | 3381 | `		  "string\|int $param, mixed $value, int $type = PDO::PARAM_STR", "@bool",` |
|    - | 3382 | `		  vm_builtin_PDOStatement_bindValue },` |
|    - | 3383 | `		{ "closeCursor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDOStatement_closeCursor },` |
|    - | 3384 | `		{ "columnCount",  PH7_MOD_PUBLIC, "", "@int", vm_builtin_PDOStatement_columnCount },` |
|    - | 3385 | `		{ "debugDumpParams", PH7_MOD_PUBLIC, "", "@?bool",` |
|    - | 3386 | `		  vm_builtin_PDOStatement_debugDumpParams },` |
|    - | 3387 | `		{ "errorCode",    PH7_MOD_PUBLIC, "", "@?string", vm_builtin_PDOStatement_errorCode },` |
|    - | 3388 | `		{ "errorInfo",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_PDOStatement_errorInfo },` |
|    - | 3389 | `		{ "execute",      PH7_MOD_PUBLIC, "?array $params = null", "@bool",` |
|    - | 3390 | `		  vm_builtin_PDOStatement_execute },` |
|    - | 3391 | `		{ "fetch",        PH7_MOD_PUBLIC,` |
|    - | 3392 | `		  "int $mode = PDO::FETCH_DEFAULT, int $cursorOrientation = PDO::FETCH_ORI_NEXT, "` |
|    - | 3393 | `		  "int $cursorOffset = 0", "@mixed", vm_builtin_PDOStatement_fetch },` |
|    - | 3394 | `		{ "fetchAll",     PH7_MOD_PUBLIC, "int $mode = PDO::FETCH_DEFAULT, mixed ...$args",` |
|    - | 3395 | `		  "@array", vm_builtin_PDOStatement_fetchAll },` |
|    - | 3396 | `		{ "fetchColumn",  PH7_MOD_PUBLIC, "int $column = 0", "@mixed",` |
|    - | 3397 | `		  vm_builtin_PDOStatement_fetchColumn },` |
|    - | 3398 | `		{ "fetchObject",  PH7_MOD_PUBLIC,` |
|    - | 3399 | `		  "?string $class = 'stdClass', array $constructorArgs = []", "@object\|false",` |
|    - | 3400 | `		  vm_builtin_PDOStatement_fetchObject },` |
|    - | 3401 | `		{ "getAttribute", PH7_MOD_PUBLIC, "int $name", "@mixed",` |
|    - | 3402 | `		  vm_builtin_PDOStatement_getAttribute },` |
|    - | 3403 | `		{ "getColumnMeta",PH7_MOD_PUBLIC, "int $column", "@array\|false",` |
|    - | 3404 | `		  vm_builtin_PDOStatement_getColumnMeta },` |
|    - | 3405 | `		{ "nextRowset",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PDOStatement_nextRowset },` |
|    - | 3406 | `		{ "rowCount",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_PDOStatement_rowCount },` |
|    - | 3407 | `		{ "setAttribute", PH7_MOD_PUBLIC, "int $attribute, mixed $value", "@bool",` |
|    - | 3408 | `		  vm_builtin_PDOStatement_setAttribute },` |
|    - | 3409 | `		{ "setFetchMode", PH7_MOD_PUBLIC, "int $mode, mixed ...$args", "@true",` |
|    - | 3410 | `		  vm_builtin_PDOStatement_setFetchMode },` |
|    - | 3411 | `		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_PDOStatement_getIterator },` |
|    - | 3412 | `	};` |
|    - | 3413 | `	/* The one property php PRESENTS on a statement: var_dump of a PDOStatement` |
|    - | 3414 | ``	 * shows `queryString` and nothing else. It is typed and has no default --`` |
|    - | 3415 | ``	 * `new PDOStatement()` (which php allows) leaves it uninitialized. */`` |
|    - | 3416 | `	static const PH7_NativePropDef aStmtProp[] = {` |
|    - | 3417 | `		{ "queryString", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|    - | 3418 | `		/* the cursor, hidden the way the connection's handle is */` |
|    - | 3419 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 3420 | `	};` |
|    - | 3421 | `	/* php redeclares Exception::$code UNTYPED here so a SQLSTATE -- a string` |
|    - | 3422 | `	 * like 'HY000' -- can live in it, and adds the driver's raw error triple. */` |
|    - | 3423 | `	static const PH7_NativePropDef aExcProp[] = {` |
|    - | 3424 | `		{ "code",      PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|    - | 3425 | `		{ "errorInfo", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?array" },` |
|    - | 3426 | `	};` |
|    - | 3427 | `	/* The connection handle: storage the class owns and NEVER presents -- php` |
|    - | 3428 | `	 * shows no property at all on a PDO, so the slot is hidden (which is what` |
|    - | 3429 | `	 * keeps it out of var_dump, (array), get_object_vars and Reflection). */` |
|    - | 3430 | `	static const PH7_NativePropDef aPdoProp[] = {` |
|    - | 3431 | `		{ "__res", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 3432 | `	};` |
|    - | 3433 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|    - | 3434 | ``		/* Both handles refuse `clone` and `serialize`: php declares neither a`` |
|    - | 3435 | `		 * clone handler nor a serializer for them, so the copy would carry the` |
|    - | 3436 | `		 * same sqlite3 pointer in its hidden slot. */` |
|    - | 3437 | `		{ "PDO", 0, 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 3438 | `		  aPdoMethod, SX_ARRAYSIZE(aPdoMethod),` |
|    - | 3439 | `		  aPdoConst, SX_ARRAYSIZE(aPdoConst),` |
|    - | 3440 | `		  aPdoProp, SX_ARRAYSIZE(aPdoProp),` |
|    - | 3441 | `		  PdoInstanceRelease, 0, 0 },` |
|    - | 3442 | `		{ "PDOStatement", 0, "IteratorAggregate", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|    - | 3443 | `		  aStmtMethod, SX_ARRAYSIZE(aStmtMethod),` |
|    - | 3444 | `		  0, 0,` |
|    - | 3445 | `		  aStmtProp, SX_ARRAYSIZE(aStmtProp),` |
|    - | 3446 | `		  PdoStmtInstanceRelease, &sPdoStmtIterVtab, 0 },` |
|    - | 3447 | `		{ "PDOException", "RuntimeException", 0, 0,` |
|    - | 3448 | `		  0, 0, 0, 0,` |
|    - | 3449 | `		  aExcProp, SX_ARRAYSIZE(aExcProp),` |
|    - | 3450 | `		  0, 0, 0 },` |
|    - | 3451 | `	};` |
|    - | 3452 | `#undef PDO_INT_CONST` |
| 5259 | 3453 | `	pVm->pPdoConns = 0;` |
| 5259 | 3454 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    5 | 3455 | `}` |
|    - | 3456 |  |
|    - | 3457 | `#else` |
|    - | 3458 | `/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */` |
|    - | 3459 | `typedef int vm_pdo_unused;` |
|    - | 3460 | `#endif /* PH7_ENABLE_SQLITE */` |
|    - | 3461 |  |
