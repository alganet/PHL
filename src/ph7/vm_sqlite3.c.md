# src/ph7/vm_sqlite3.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1660/1898 lines (87.46%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_SQLITE` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <sqlite3.h>` |
|      - |    8 |  |
|      - |    9 | `/*` |
|      - |   10 | ` * Section:` |
|      - |   11 | ` *    ext/sqlite3 -- php's OTHER sqlite surface, the one beside the PDO driver:` |
|      - |   12 | `` *    `SQLite3`, `SQLite3Stmt`, `SQLite3Result` and `SQLite3Exception`.`` |
|      - |   13 | ` * Status:` |
|      - |   14 | ` *    Complete: all 24 methods php declares on SQLite3, all 13 on SQLite3Stmt,` |
|      - |   15 | ` *    all 8 on SQLite3Result, and the twelve global constants.` |
|      - |   16 | ` *` |
|      - |   17 | ` * Nothing here goes through ext/pdo. The two extensions share a LIBRARY and` |
|      - |   18 | ` * not a model, and every difference between them is deliberate on php's side:` |
|      - |   19 | ` * pdo reports SQLSTATE strings, sqlite3 reports sqlite's own integer codes;` |
|      - |   20 | ` * pdo's error mode is an attribute with three settings, sqlite3's is one` |
|      - |   21 | `` * boolean; pdo opens with `SQLITE_OPEN_URI` and sqlite3 does not, so`` |
|      - |   22 | `` * `new SQLite3('file:/tmp/x')` is a failed open where the same string behind`` |
|      - |   23 | `` * `sqlite:` is a URI. Sharing a connection record between them would mean`` |
|      - |   24 | ` * reconciling those, which php never does.` |
|      - |   25 | ` *` |
|      - |   26 | ` * php's object carries TWO pieces of state a script can distinguish, and the` |
|      - |   27 | ` * whole "has this been closed" surface follows from the pair:` |
|      - |   28 | `` *   - `initialised`, raised by a successful open and never lowered;`` |
|      - |   29 | `` *   - the `sqlite3 *` itself, which close() drops.`` |
|      - |   30 | ` * A never-opened object fails both, so every verb on it is the Error below. A` |
|      - |   31 | ` * CLOSED one still passes the first, which is why lastErrorCode(), lastErrorMsg()` |
|      - |   32 | ` * and lastExtendedErrorCode() answer 0 / "" / 0 there while every other verb is` |
|      - |   33 | ` * the same Error -- and why open() may be called on it again.` |
|      - |   34 | ` */` |
|      - |   35 |  |
|      - |   36 | `/*` |
|      - |   37 | ` * One connection. Lives on the per-VM chain (pVm->pSq3Conns) and is reached` |
|      - |   38 | `` * from its SQLite3 object through the hidden `__res` slot -- the model ext/pdo`` |
|      - |   39 | `` * and XMLWriter use, safe here for the same reason: `clone` is refused, so no`` |
|      - |   40 | ` * second object can ever hold the same handle.` |
|      - |   41 | ` */` |
|      - |   42 | `typedef struct phl_sq3 phl_sq3;` |
|      - |   43 | `typedef struct phl_sq3_stmt phl_sq3_stmt;` |
|      - |   44 | `typedef struct phl_sq3_res phl_sq3_res;` |
|      - |   45 | `struct phl_sq3 {` |
|      - |   46 | `	sqlite3 *pDb;                 /* 0 before the first open and after close() */` |
|      - |   47 | `	int bInitialised;             /* an open has SUCCEEDED on this object */` |
|      - |   48 | `	int bExceptions;              /* enableExceptions(): what routes a failure */` |
|      - |   49 | `	ph7_vm *pVm;` |
|      - |   50 | `	ph7_class_instance *pOwner;   /* the object whose slot holds it */` |
|      - |   51 | `	phl_sq3_stmt *pStmts;         /* statements prepared on it: sqlite will not close a` |
|      - |   52 | `	                               * database while one of them is alive */` |
|      - |   53 | `	phl_sq3_res *pResults;        /* the result objects walking those statements */` |
|      - |   54 | `	struct phl_sq3_blob *pBlobs;  /* openBlob() handles a script has not closed. sqlite` |
|      - |   55 | `	                               * REFUSES to close a database while one is open, which is` |
|      - |   56 | `	                               * a failure php reports rather than forces past -- only` |
|      - |   57 | `	                               * the teardown paths close them behind a script's back */` |
|      - |   58 | `	struct phl_sq3_udf *pUdfs;    /* createFunction/createAggregate/createCollation and the` |
|      - |   59 | `	                               * authorizer: kept alive for as long as sqlite may call` |
|      - |   60 | `	                               * them, which is until the connection closes */` |
|      - |   61 | `	ph7_context *pVerbCtx;        /* the call a diagnostic raised from INSIDE sqlite belongs` |
|      - |   62 | `	                               * to. php prints the method that was running -- the` |
|      - |   63 | ``	                               * `SQLite3Result::fetchAll(): ` in front of a collation's`` |
|      - |   64 | `	                               * complaint -- and only the verb knows which one it is */` |
|      - |   65 | `	const char *zVerbFn;` |
|      - |   66 | `	sxi32 iCallbackExc;           /* the status a callback threw with, PARKED: sqlite has to` |
|      - |   67 | `	                               * finish unwinding before the engine may raise it, and the` |
|      - |   68 | `	                               * verb that started the step answers exactly this */` |
|      - |   69 | `	phl_sq3 *pNext;` |
|      - |   70 | `};` |
|      - |   71 | `/*` |
|      - |   72 | ` * One prepared statement, REFERENCE-COUNTED -- which is the shape php's own` |
|      - |   73 | ` * object graph has and the reason two verbs that look alike behave differently.` |
|      - |   74 | `` * `query()` builds a statement nothing but its result holds, so finalizing that`` |
|      - |   75 | `` * result destroys it; `execute()` hands out a result over a statement the script`` |
|      - |   76 | ` * ALSO holds, so finalizing that result leaves the statement runnable. The count` |
|      - |   77 | ` * is what tells the two apart at the moment of the finalize.` |
|      - |   78 | ` *` |
|      - |   79 | `` * `close()` on the statement is not a release: it finalizes NOW, whatever`` |
|      - |   80 | ` * results are still pointing at it, and every one of them starts answering the` |
|      - |   81 | ` * Error instead.` |
|      - |   82 | ` */` |
|      - |   83 | `struct phl_sq3_stmt {` |
|      - |   84 | `	sqlite3_stmt *pStmt;          /* 0 for a statement of NOTHING (see below), and once closed */` |
|      - |   85 | `	phl_sq3 *pConn;` |
|      - |   86 | `	ph7_class_instance *pOwner;   /* the SQLite3Stmt object, when a script holds one */` |
|      - |   87 | `	ph7_class_instance *pConnObj; /* the SQLite3 object, RETAINED: a result outlives the` |
|      - |   88 | `	                               * variable its connection was in, and php keeps the` |
|      - |   89 | `	                               * database open through exactly this reference */` |
|      - |   90 | `	int bInitialised;             /* php's per-STATEMENT flag: raised by prepare, lowered by` |
|      - |   91 | `	                               * close() and by the connection's own close. It is what` |
|      - |   92 | `	                               * tells a closed statement (the Error naming SQLite3) from` |
|      - |   93 | `	                               * a statement of NOTHING (the one naming SQLite3Stmt) */` |
|      - |   94 | `	int nRef;                     /* holders: the statement object, and each live result */` |
|      - |   95 | `	struct phl_sq3_bind *pBinds;  /* what bindValue()/bindParam() recorded, applied at` |
|      - |   96 | `	                               * every execute -- php's bindings, not sqlite's */` |
|      - |   97 | `	phl_sq3_stmt *pNext;` |
|      - |   98 | `};` |
|      - |   99 | `/*` |
|      - |  100 | ` * One result -- php's cursor over a statement, and nothing more. It owns no row` |
|      - |  101 | ``  * of its own: `numColumns()` reads the statement's column count, `columnType()` `` |
|      - |  102 | `` * reads the row sqlite has UP, and `fetchArray()` steps.`` |
|      - |  103 | ` */` |
|      - |  104 | `struct phl_sq3_res {` |
|      - |  105 | `	phl_sq3_stmt *pSt;            /* 0 once finalize() has run on THIS result */` |
|      - |  106 | `	phl_sq3 *pConn;               /* kept beside pSt: the record has to find its chain` |
|      - |  107 | `	                               * again after finalize() has let the statement go */` |
|      - |  108 | `	ph7_class_instance *pOwner;` |
|      - |  109 | `	ph7_class_instance *pStmtObj; /* the SQLite3Stmt it came from, RETAINED, or 0 for the` |
|      - |  110 | `	                               * anonymous statement query() builds */` |
|      - |  111 | `	phl_sq3_res *pNext;` |
|      - |  112 | `};` |
|      - |  113 |  |
|      - |  114 | `/* The hidden slot every one of these classes reaches its record through. */` |
|      - |  115 | `#define SQ3_RES "__res"` |
|      - |  116 |  |
|      - |  117 | `/* ------------------------------------------------------------------------` |
|      - |  118 | ` * Lifetime` |
|      - |  119 | ` * ------------------------------------------------------------------------ */` |
|      - |  120 | `/*` |
|      - |  121 | ` * Blank one object's hidden slot: the record behind it is going away and the` |
|      - |  122 | ` * object may well outlive it.` |
|      - |  123 | ` */` |
|      - |  124 | `static void Sq3BlankSlot(ph7_class_instance *pOwner);` |
|      - |  125 | `/* Drop what bindValue()/bindParam() recorded (defined with the statement). */` |
|      - |  126 | `static void Sq3BindsClear(phl_sq3_stmt *pSt);` |
|      - |  127 | `/* Let go of every callback a script registered (defined with them, below). */` |
|      - |  128 | `static void Sq3UdfSweep(phl_sq3 *pConn);` |
|      - |  129 | `/* Close every blob handle a script left open (defined with them, below). */` |
|      - |  130 | `static void Sq3BlobSweep(phl_sq3 *pConn);` |
|      - |  131 | `/*` |
|      - |  132 | ` * Close the database, and every statement standing on it with it.` |
|      - |  133 | ` *` |
|      - |  134 | ` * The statements go FIRST and they are not merely released: php keeps a list of` |
|      - |  135 | `` * everything a connection handed out and cleans it here, so `close()` finalizes`` |
|      - |  136 | ` * a statement a script is still holding and every result walking one starts` |
|      - |  137 | ` * answering the Error. The records themselves stay -- the objects that reach` |
|      - |  138 | ` * them are still alive and have to find an emptied handle rather than freed` |
|      - |  139 | ` * memory.` |
|      - |  140 | ` *` |
|      - |  141 | ` * That is also why dropping the last reference to a SQLite3 is NOT the same` |
|      - |  142 | ` * thing: a live statement retains the object, so there is no last reference to` |
|      - |  143 | ` * drop while one exists, and a result outlives the variable its connection was` |
|      - |  144 | ` * in.` |
|      - |  145 | ` */` |
|    414 |  146 | `static void Sq3FinalizeStmts(phl_sq3 *pConn)` |
|      4 |  147 | `{` |
|      - |  148 | `	phl_sq3_stmt *pSt;` |
|    426 |  149 | `	for( pSt = pConn->pStmts ; pSt ; pSt = pSt->pNext ){` |
|     10 |  150 | `		if( pSt->pStmt ){` |
|     10 |  151 | `			sqlite3_finalize(pSt->pStmt);` |
|     10 |  152 | `			pSt->pStmt = 0;` |
|      4 |  153 | `		}` |
|     10 |  154 | `		pSt->bInitialised = 0;` |
|      6 |  155 | `	}` |
|    418 |  156 | `}` |
|      - |  157 | `/* What is left to let go of once the handle itself is gone. */` |
|    410 |  158 | `static void Sq3CloseDone(phl_sq3 *pConn)` |
|      4 |  159 | `{` |
|    414 |  160 | `	pConn->pDb = 0;` |
|      - |  161 | `	/* the callbacks go last: nothing can reach them once the database that` |
|      - |  162 | `	 * would have called them is closed */` |
|    414 |  163 | `	Sq3UdfSweep(pConn);` |
|    414 |  164 | `	pConn->iCallbackExc = 0;` |
|    414 |  165 | `}` |
|      - |  166 | `/*` |
|      - |  167 | ` * The close a script cannot refuse: every blob handle goes too and the handle` |
|      - |  168 | ` * is dropped with _v2, so the connection really is gone. This is the TEARDOWN` |
|      - |  169 | ` * path -- the object dying, or the VM being reset -- where nothing is left to` |
|      - |  170 | ` * report a failure to.` |
|      - |  171 | ` */` |
|    350 |  172 | `static void Sq3Close(phl_sq3 *pConn)` |
|      4 |  173 | `{` |
|    354 |  174 | `	Sq3FinalizeStmts(pConn);` |
|    354 |  175 | `	Sq3BlobSweep(pConn);` |
|    354 |  176 | `	if( pConn->pDb ){` |
|    118 |  177 | `		sqlite3_close_v2(pConn->pDb);` |
|     57 |  178 | `	}` |
|    354 |  179 | `	Sq3CloseDone(pConn);` |
|    354 |  180 | `}` |
|    172 |  181 | `static phl_sq3 * Sq3NewConn(ph7_vm *pVm)` |
|      4 |  182 | `{` |
|    176 |  183 | `	phl_sq3 *pConn = (phl_sq3 *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_sq3));` |
|    176 |  184 | `	if( pConn == 0 ){` |
|    ! 0 |  185 | `		return 0;` |
|      - |  186 | `	}` |
|    176 |  187 | `	SyZero(pConn,sizeof(phl_sq3));` |
|    176 |  188 | `	pConn->pVm = pVm;` |
|    176 |  189 | `	pConn->pNext = (phl_sq3 *)pVm->pSq3Conns;` |
|    176 |  190 | `	pVm->pSq3Conns = pConn;` |
|    176 |  191 | `	return pConn;` |
|     90 |  192 | `}` |
|      - |  193 | `/*` |
|      - |  194 | ` * Let go of one hold on a statement. The LAST holder finalizes it -- which is` |
|      - |  195 | ` * how a result over an anonymous statement destroys it while a result over a` |
|      - |  196 | ` * script's own statement does not.` |
|      - |  197 | ` */` |
|    308 |  198 | `static void Sq3StmtUnref(phl_sq3_stmt *pSt)` |
|      3 |  199 | `{` |
|      - |  200 | `	phl_sq3 *pConn;` |
|    311 |  201 | `	phl_sq3_stmt *pCur,*pPrev = 0;` |
|    311 |  202 | `	if( pSt == 0 \|\| --pSt->nRef > 0 ){` |
|     71 |  203 | `		return;` |
|      - |  204 | `	}` |
|    241 |  205 | `	pConn = pSt->pConn;` |
|    241 |  206 | `	Sq3BindsClear(pSt);` |
|    241 |  207 | `	if( pSt->pStmt ){` |
|    215 |  208 | `		sqlite3_finalize(pSt->pStmt);` |
|    215 |  209 | `		pSt->pStmt = 0;` |
|    106 |  210 | `	}` |
|    263 |  211 | `	for( pCur = pConn->pStmts ; pCur ; pPrev = pCur, pCur = pCur->pNext ){` |
|    263 |  212 | `		if( pCur == pSt ){` |
|    241 |  213 | `			if( pPrev ){` |
|     23 |  214 | `				pPrev->pNext = pCur->pNext;` |
|     12 |  215 | `			}else{` |
|    219 |  216 | `				pConn->pStmts = pCur->pNext;` |
|      - |  217 | `			}` |
|    241 |  218 | `			break;` |
|      - |  219 | `		}` |
|     12 |  220 | `	}` |
|    241 |  221 | `	if( pSt->pConnObj ){` |
|      - |  222 | `		/* drop the reference taken at creation; the connection may go now */` |
|    241 |  223 | `		ph7_class_instance *pObj = pSt->pConnObj;` |
|    241 |  224 | `		pSt->pConnObj = 0;` |
|    241 |  225 | `		PH7_ClassInstanceUnref(pObj);` |
|    119 |  226 | `	}` |
|    241 |  227 | `	SyMemBackendFree(&pConn->pVm->sAllocator,pSt);` |
|    157 |  228 | `}` |
|      - |  229 | `/*` |
|      - |  230 | ``  * Let go of what a result is HOLDING without freeing the record: `finalize()` `` |
|      - |  231 | ` * does exactly this and leaves the object standing, answering the Error.` |
|      - |  232 | ` */` |
|    120 |  233 | `static void Sq3ResDetach(phl_sq3_res *pRes)` |
|      1 |  234 | `{` |
|    121 |  235 | `	if( pRes->pSt ){` |
|    117 |  236 | `		Sq3StmtUnref(pRes->pSt);` |
|    117 |  237 | `		pRes->pSt = 0;` |
|     58 |  238 | `	}` |
|    121 |  239 | `	if( pRes->pStmtObj ){` |
|     71 |  240 | `		ph7_class_instance *pObj = pRes->pStmtObj;` |
|     71 |  241 | `		pRes->pStmtObj = 0;` |
|     71 |  242 | `		PH7_ClassInstanceUnref(pObj);` |
|     35 |  243 | `	}` |
|    121 |  244 | `}` |
|      - |  245 | `/* Free one result record: what it holds, then its place on the chain. */` |
|    116 |  246 | `static void Sq3FreeRes(phl_sq3_res *pRes)` |
|      1 |  247 | `{` |
|    117 |  248 | `	phl_sq3 *pConn = pRes->pConn;` |
|    117 |  249 | `	phl_sq3_res *pCur,*pPrev = 0;` |
|    117 |  250 | `	Sq3ResDetach(pRes);` |
|    119 |  251 | `	for( pCur = pConn->pResults ; pCur ; pPrev = pCur, pCur = pCur->pNext ){` |
|    119 |  252 | `		if( pCur == pRes ){` |
|    117 |  253 | `			if( pPrev ){` |
|      3 |  254 | `				pPrev->pNext = pCur->pNext;` |
|      2 |  255 | `			}else{` |
|    115 |  256 | `				pConn->pResults = pCur->pNext;` |
|      - |  257 | `			}` |
|    117 |  258 | `			break;` |
|      - |  259 | `		}` |
|      2 |  260 | `	}` |
|    117 |  261 | `	SyMemBackendFree(&pConn->pVm->sAllocator,pRes);` |
|    117 |  262 | `}` |
|    172 |  263 | `static void Sq3ResSweep(phl_sq3 *pConn)` |
|      4 |  264 | `{` |
|    176 |  265 | `	while( pConn->pResults ){` |
|    ! 0 |  266 | `		phl_sq3_res *pRes = pConn->pResults;` |
|    ! 0 |  267 | `		Sq3BlankSlot(pRes->pOwner);` |
|    ! 0 |  268 | `		Sq3FreeRes(pRes);` |
|    ! 0 |  269 | `	}` |
|    176 |  270 | `}` |
|    172 |  271 | `static void Sq3StmtSweep(phl_sq3 *pConn)` |
|      4 |  272 | `{` |
|    176 |  273 | `	while( pConn->pStmts ){` |
|    ! 0 |  274 | `		phl_sq3_stmt *pSt = pConn->pStmts;` |
|    ! 0 |  275 | `		Sq3BlankSlot(pSt->pOwner);` |
|    ! 0 |  276 | `		pConn->pStmts = pSt->pNext;` |
|    ! 0 |  277 | `		Sq3BindsClear(pSt);` |
|    ! 0 |  278 | `		if( pSt->pStmt ){` |
|    ! 0 |  279 | `			sqlite3_finalize(pSt->pStmt);` |
|    ! 0 |  280 | `			pSt->pStmt = 0;` |
|    ! 0 |  281 | `		}` |
|    ! 0 |  282 | `		if( pSt->pConnObj ){` |
|    ! 0 |  283 | `			ph7_class_instance *pObj = pSt->pConnObj;` |
|    ! 0 |  284 | `			pSt->pConnObj = 0;` |
|    ! 0 |  285 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  286 | `		}` |
|    ! 0 |  287 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pSt);` |
|    ! 0 |  288 | `	}` |
|    176 |  289 | `}` |
|    172 |  290 | `static void Sq3FreeConn(phl_sq3 *pConn)` |
|      4 |  291 | `{` |
|    176 |  292 | `	ph7_vm *pVm = pConn->pVm;` |
|    176 |  293 | `	phl_sq3 *pCur,*pPrev = 0;` |
|      - |  294 | `	/* Results first, then statements: a result names a statement, and sqlite` |
|      - |  295 | `	 * refuses to close a database while a statement of its own is alive. */` |
|    176 |  296 | `	Sq3ResSweep(pConn);` |
|    176 |  297 | `	Sq3StmtSweep(pConn);` |
|    176 |  298 | `	Sq3Close(pConn);` |
|    176 |  299 | `	for( pCur = (phl_sq3 *)pVm->pSq3Conns ; pCur ; pPrev = pCur, pCur = pCur->pNext ){` |
|    176 |  300 | `		if( pCur == pConn ){` |
|    176 |  301 | `			if( pPrev ){` |
|    ! 0 |  302 | `				pPrev->pNext = pCur->pNext;` |
|    ! 0 |  303 | `			}else{` |
|    176 |  304 | `				pVm->pSq3Conns = pCur->pNext;` |
|      - |  305 | `			}` |
|    176 |  306 | `			break;` |
|      - |  307 | `		}` |
|    ! 0 |  308 | `	}` |
|    176 |  309 | `	SyMemBackendFree(&pVm->sAllocator,pConn);` |
|    176 |  310 | `}` |
|   5645 |  311 | `static void Sq3VmSweep(ph7_vm *pVm)` |
|      5 |  312 | `{` |
|   5822 |  313 | `	while( pVm->pSq3Conns ){` |
|    176 |  314 | `		phl_sq3 *pConn = (phl_sq3 *)pVm->pSq3Conns;` |
|    176 |  315 | `		Sq3BlankSlot(pConn->pOwner);` |
|    176 |  316 | `		Sq3FreeConn(pConn);` |
|      4 |  317 | `	}` |
|   5650 |  318 | `}` |
|      - |  319 | `/*` |
|      - |  320 | ` * A reused VM (the -S server's) must not answer the next request through a` |
|      - |  321 | ` * handle this one opened, and a sqlite3 handle lives outside SyMemBackend, so` |
|      - |  322 | ` * the wholesale release at the end would leak both it and the file lock.` |
|      - |  323 | ` */` |
|     16 |  324 | `PH7_PRIVATE void PH7_Sqlite3VmReset(ph7_vm *pVm)` |
|    ! 0 |  325 | `{` |
|     16 |  326 | `	Sq3VmSweep(&(*pVm));` |
|     16 |  327 | `}` |
|   5629 |  328 | `PH7_PRIVATE void PH7_Sqlite3VmRelease(ph7_vm *pVm)` |
|      5 |  329 | `{` |
|   5634 |  330 | `	Sq3VmSweep(&(*pVm));` |
|   5634 |  331 | `}` |
|      - |  332 |  |
|      - |  333 | `/* ------------------------------------------------------------------------` |
|      - |  334 | ` * The object and its hidden slot` |
|      - |  335 | ` * ------------------------------------------------------------------------ */` |
|      - |  336 | `/* The record behind any of the three classes' hidden slots. */` |
|   1704 |  337 | `static void * Sq3ResourceOf(ph7_class_instance *pThis)` |
|      4 |  338 | `{` |
|      - |  339 | `	SyString sAttr;` |
|      - |  340 | `	ph7_value *pRes;` |
|   1708 |  341 | `	if( pThis == 0 ){` |
|    ! 0 |  342 | `		return 0;` |
|      - |  343 | `	}` |
|   1708 |  344 | `	SyStringInitFromBuf(&sAttr,SQ3_RES,sizeof(SQ3_RES)-1);` |
|   1708 |  345 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   1708 |  346 | `	if( pRes == 0 \|\| !ph7_value_is_resource(pRes) ){` |
|    202 |  347 | `		return 0;` |
|      - |  348 | `	}` |
|   1510 |  349 | `	return ph7_value_to_resource(pRes);` |
|    856 |  350 | `}` |
|    172 |  351 | `static void Sq3BlankSlot(ph7_class_instance *pOwner)` |
|      4 |  352 | `{` |
|      - |  353 | `	SyString sAttr;` |
|      - |  354 | `	ph7_value *pRes;` |
|    176 |  355 | `	if( pOwner == 0 ){` |
|    176 |  356 | `		return;` |
|      - |  357 | `	}` |
|    ! 0 |  358 | `	SyStringInitFromBuf(&sAttr,SQ3_RES,sizeof(SQ3_RES)-1);` |
|    ! 0 |  359 | `	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);` |
|    ! 0 |  360 | `	if( pRes ){` |
|    ! 0 |  361 | `		PH7_MemObjRelease(pRes);` |
|    ! 0 |  362 | `		MemObjSetType(pRes,MEMOBJ_NULL);` |
|    ! 0 |  363 | `	}` |
|     90 |  364 | `}` |
|    372 |  365 | `static int Sq3AttachRes(ph7_class_instance *pThis,void *pRecord)` |
|      4 |  366 | `{` |
|      - |  367 | `	SyString sAttr;` |
|      - |  368 | `	ph7_value *pRes;` |
|    376 |  369 | `	if( pThis == 0 ){` |
|    ! 0 |  370 | `		return -1;` |
|      - |  371 | `	}` |
|    376 |  372 | `	SyStringInitFromBuf(&sAttr,SQ3_RES,sizeof(SQ3_RES)-1);` |
|    376 |  373 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    376 |  374 | `	if( pRes == 0 ){` |
|    ! 0 |  375 | `		return -1;` |
|      - |  376 | `	}` |
|    376 |  377 | `	PH7_MemObjRelease(pRes);` |
|    376 |  378 | `	pRes->x.pOther = pRecord;` |
|    376 |  379 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|    376 |  380 | `	return 0;` |
|    190 |  381 | `}` |
|   1130 |  382 | `static phl_sq3 * Sq3OfInstance(ph7_class_instance *pThis)` |
|      4 |  383 | `{` |
|   1134 |  384 | `	return (phl_sq3 *)Sq3ResourceOf(pThis);` |
|      4 |  385 | `}` |
|    172 |  386 | `static int Sq3Attach(ph7_class_instance *pThis,phl_sq3 *pConn)` |
|      4 |  387 | `{` |
|    176 |  388 | `	if( Sq3AttachRes(pThis,pConn) != 0 ){` |
|    ! 0 |  389 | `		return -1;` |
|      - |  390 | `	}` |
|    176 |  391 | `	pConn->pOwner = pThis;` |
|    176 |  392 | `	return 0;` |
|     90 |  393 | `}` |
|      - |  394 | `/*` |
|      - |  395 | ` * The object is going away: close its database HERE rather than at VM reset,` |
|      - |  396 | ` * so a script that drops its last reference releases the file lock there --` |
|      - |  397 | ` * which is what php does, and what a test that unlinks the file afterwards` |
|      - |  398 | ` * needs. The record itself stays on the registry for the sweep to free.` |
|      - |  399 | ` */` |
|    182 |  400 | `static void Sq3InstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      4 |  401 | `{` |
|    186 |  402 | `	phl_sq3 *pConn = Sq3OfInstance(pThis);` |
|     91 |  403 | `	SXUNUSED(pVm);` |
|    186 |  404 | `	if( pConn == 0 \|\| pConn->pOwner != pThis ){` |
|     11 |  405 | `		return;` |
|      - |  406 | `	}` |
|    176 |  407 | `	Sq3Close(pConn);` |
|    176 |  408 | `	pConn->pOwner = 0;` |
|     95 |  409 | `}` |
|      - |  410 | `/*` |
|      - |  411 | `` * The record behind `$this`, made on demand: php's object exists before any`` |
|      - |  412 | `` * open (`newInstanceWithoutConstructor()` builds one, and open() may be called`` |
|      - |  413 | ` * on it later), so the slot is filled at the first verb that needs it rather` |
|      - |  414 | `` * than at `new`.`` |
|      - |  415 | ` */` |
|    188 |  416 | `static phl_sq3 * Sq3Bind(ph7_context *pCtx)` |
|      4 |  417 | `{` |
|    192 |  418 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  419 | `	phl_sq3 *pConn;` |
|    192 |  420 | `	if( pThis == 0 ){` |
|    ! 0 |  421 | `		return 0;` |
|      - |  422 | `	}` |
|    192 |  423 | `	pConn = Sq3OfInstance(pThis);` |
|    192 |  424 | `	if( pConn ){` |
|     17 |  425 | `		return pConn;` |
|      - |  426 | `	}` |
|    176 |  427 | `	pConn = Sq3NewConn(pCtx->pVm);` |
|    176 |  428 | `	if( pConn == 0 ){` |
|    ! 0 |  429 | `		return 0;` |
|      - |  430 | `	}` |
|    176 |  431 | `	if( Sq3Attach(pThis,pConn) != 0 ){` |
|    ! 0 |  432 | `		Sq3FreeConn(pConn);` |
|    ! 0 |  433 | `		return 0;` |
|      - |  434 | `	}` |
|    176 |  435 | `	return pConn;` |
|     98 |  436 | `}` |
|      - |  437 | `/*` |
|      - |  438 | `` * php's `SQLITE3_CHECK_INITIALIZED`: one sentence for both halves of the`` |
|      - |  439 | ` * state, which is why a never-opened object and a closed one are told the same` |
|      - |  440 | ` * thing.` |
|      - |  441 | ` */` |
|     46 |  442 | `static sxi32 Sq3Uninitialised(ph7_context *pCtx,const char *zClass)` |
|      2 |  443 | `{` |
|     71 |  444 | `	return PH7_VmThrowException(pCtx,"Error",` |
|     23 |  445 | `		"The %s object has not been correctly initialised or is already closed",zClass);` |
|      2 |  446 | `}` |
|      - |  447 |  |
|      - |  448 | `/* ------------------------------------------------------------------------` |
|      - |  449 | ` * Reporting a failure` |
|      - |  450 | ` * ------------------------------------------------------------------------ */` |
|      - |  451 | `/*` |
|      - |  452 | `` * php's `php_sqlite3_error()`: one routine, two destinations. With exceptions`` |
|      - |  453 | ` * enabled the text becomes a SQLite3Exception carrying sqlite's own code;` |
|      - |  454 | ` * without them it is an E_WARNING that names the METHOD, which is the` |
|      - |  455 | ` * docref prefix php puts on every diagnostic raised from inside a call.` |
|      - |  456 | ` *` |
|      - |  457 | ` * The exception's message has no prefix at all -- the same text, told twice in` |
|      - |  458 | ` * two shapes.` |
|      - |  459 | ` */` |
|     72 |  460 | `static sxi32 Sq3Error(ph7_context *pCtx,phl_sq3 *pConn,const char *zFn,int iCode,` |
|      - |  461 | `	const char *zMsg)` |
|      2 |  462 | `{` |
|     74 |  463 | `	if( pConn && pConn->iCallbackExc != 0 ){` |
|      - |  464 | `		/* A callback has already thrown and the library is only reporting that` |
|      - |  465 | `		 * it was told to stop. php raises the script's own exception and says` |
|      - |  466 | `		 * nothing about the statement sqlite abandoned. */` |
|      5 |  467 | `		return PH7_OK;` |
|      - |  468 | `	}` |
|     70 |  469 | `	if( pConn && pConn->bExceptions ){` |
|      5 |  470 | `		return PH7_VmThrowExceptionCode(pCtx,"SQLite3Exception",(sxi32)iCode,"%s",zMsg);` |
|      - |  471 | `	}` |
|     66 |  472 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,zMsg);` |
|     66 |  473 | `	return PH7_OK;` |
|     38 |  474 | `}` |
|      - |  475 | `/*` |
|      - |  476 | ` * A verb that can re-enter PHP -- anything that steps the library, since a` |
|      - |  477 | ` * callback may fire from inside it -- announces itself on the connection for` |
|      - |  478 | ` * the length of the call. Two things read that: a diagnostic raised from inside` |
|      - |  479 | ` * a callback, which php prints under the METHOD's name, and the parked status a` |
|      - |  480 | ` * callback threw with, which this verb is the one to raise.` |
|      - |  481 | ` *` |
|      - |  482 | ` * The frame is saved and restored rather than assigned, because a callback may` |
|      - |  483 | ` * perfectly well run a query of its own on the same connection.` |
|      - |  484 | ` */` |
|      - |  485 | `typedef struct Sq3Verb Sq3Verb;` |
|      - |  486 | `struct Sq3Verb {` |
|      - |  487 | `	ph7_context *pCtx;` |
|      - |  488 | `	const char *zFn;` |
|      - |  489 | `};` |
|    666 |  490 | `static void Sq3VerbEnter(phl_sq3 *pConn,ph7_context *pCtx,const char *zFn,Sq3Verb *pSave)` |
|      4 |  491 | `{` |
|    670 |  492 | `	pSave->pCtx = pConn->pVerbCtx;` |
|    670 |  493 | `	pSave->zFn = pConn->zVerbFn;` |
|    670 |  494 | `	pConn->pVerbCtx = pCtx;` |
|    670 |  495 | `	pConn->zVerbFn = zFn;` |
|    670 |  496 | `}` |
|      - |  497 | `/*` |
|      - |  498 | ` * Leave the frame and answer the status a callback parked, if any -- taking it` |
|      - |  499 | ` * OFF the connection as it goes, so the next verb starts clean whatever this` |
|      - |  500 | ` * one's caller does with it.` |
|      - |  501 | ` */` |
|    666 |  502 | `static sxi32 Sq3VerbLeave(phl_sq3 *pConn,Sq3Verb *pSave)` |
|      4 |  503 | `{` |
|    670 |  504 | `	sxi32 rc = pConn->iCallbackExc;` |
|    670 |  505 | `	pConn->iCallbackExc = 0;` |
|    670 |  506 | `	pConn->pVerbCtx = pSave->pCtx;` |
|    670 |  507 | `	pConn->zVerbFn = pSave->zFn;` |
|    670 |  508 | `	return rc;` |
|      4 |  509 | `}` |
|      - |  510 | `/* The same, worded from the library's own view of the last failure. */` |
|     12 |  511 | `static sxi32 Sq3ErrorFromDb(ph7_context *pCtx,phl_sq3 *pConn,const char *zFn)` |
|      1 |  512 | `{` |
|     13 |  513 | `	int iCode = pConn->pDb ? sqlite3_errcode(pConn->pDb) : SQLITE_ERROR;` |
|     13 |  514 | `	const char *zMsg = pConn->pDb ? sqlite3_errmsg(pConn->pDb) : sqlite3_errstr(SQLITE_ERROR);` |
|     13 |  515 | `	return Sq3Error(pCtx,pConn,zFn,iCode,zMsg);` |
|      1 |  516 | `}` |
|      - |  517 |  |
|      - |  518 | `/* ------------------------------------------------------------------------` |
|      - |  519 | ` * Opening and closing` |
|      - |  520 | ` * ------------------------------------------------------------------------ */` |
|      - |  521 | `/*` |
|      - |  522 | `` * php's `expand_filepath()`, which every ext/sqlite3 open runs the filename`` |
|      - |  523 | `` * through unless it is exactly `:memory:` or the empty string. It is what makes`` |
|      - |  524 | ` * a relative name resolve against the working directory -- and, less obviously,` |
|      - |  525 | `` * what keeps `file:...` from ever reaching sqlite as a URI, because by then the`` |
|      - |  526 | ` * name starts with the directory instead. The engine cannot leave that to the` |
|      - |  527 | ` * library: a Debian libsqlite3 is compiled with URI filenames ON and a vcpkg one` |
|      - |  528 | `` * is not, so an unexpanded `file:x` would open two different things on this`` |
|      - |  529 | ` * engine's two platforms while php opens neither.` |
|      - |  530 | ` *` |
|      - |  531 | ` * The walk itself is PH7_VfsExpandPath(), which ext/zip runs its own filenames` |
|      - |  532 | ` * through for the same reason.` |
|      - |  533 | ` *` |
|      - |  534 | `` * The open both `__construct` and `open` are. php refuses a SECOND open on a`` |
|      - |  535 | ` * live handle before it looks at the arguments' meaning, throws a plain` |
|      - |  536 | ` * Exception (not SQLite3Exception -- the object cannot have been told to use` |
|      - |  537 | ` * them yet) for a failure, and hands sqlite the EXPANDED filename.` |
|      - |  538 | ` *` |
|      - |  539 | ` * Two names are handed over untouched instead, and they are the two that name` |
|      - |  540 | `` * no file: `:memory:` exactly, and the empty string (a private temporary`` |
|      - |  541 | ` * database sqlite deletes with the connection). Everything else is a path,` |
|      - |  542 | `` * `:memory:extra` included.`` |
|      - |  543 | ` *` |
|      - |  544 | `` * `SQLITE_OPEN_URI` is deliberately NOT passed -- php's ext/sqlite3 does not,`` |
|      - |  545 | ` * and the expansion above makes the question moot anyway. ext/pdo DOES pass it,` |
|      - |  546 | `` * which is why the same string behind a `sqlite:` DSN opens a URI there; the`` |
|      - |  547 | ` * divergence is php's own, between its two extensions.` |
|      - |  548 | ` */` |
|    176 |  549 | `static sxi32 Sq3OpenImpl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  550 | `{` |
|    180 |  551 | `	phl_sq3 *pConn = Sq3Bind(pCtx);` |
|      - |  552 | `	const char *zFile;` |
|    180 |  553 | `	int nFile = 0;` |
|    180 |  554 | `	int iFlags = SQLITE_OPEN_READWRITE\|SQLITE_OPEN_CREATE;` |
|      - |  555 | `	SyBlob sPath;` |
|      - |  556 | `	int rc;` |
|    180 |  557 | `	if( pConn == 0 ){` |
|    ! 0 |  558 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  559 | `	}` |
|    180 |  560 | `	if( pConn->pDb ){` |
|      3 |  561 | `		return PH7_VmThrowException(pCtx,"Exception","Already initialised DB Object");` |
|      - |  562 | `	}` |
|    178 |  563 | `	zFile = ph7_value_to_string(apArg[0],&nFile);` |
|    178 |  564 | `	if( nArg > 1 ){` |
|      3 |  565 | `		iFlags = (int)ph7_value_to_int64(apArg[1]);` |
|      1 |  566 | `	}` |
|      - |  567 | `	/* $encryptionKey is read and dropped: php's own build has no codec either,` |
|      - |  568 | `	 * so the argument reaches nothing there. */` |
|    178 |  569 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|    174 |  570 | `	if( nFile == (int)sizeof(":memory:")-1` |
|    168 |  571 | `	 && SyMemcmp(zFile,":memory:",sizeof(":memory:")-1) == 0 ){` |
|    154 |  572 | `		SyBlobAppend(&sPath,":memory:",sizeof(":memory:")-1);` |
|    154 |  573 | `		SyBlobNullAppend(&sPath);` |
|    101 |  574 | `	}else if( nFile < 1 ){` |
|      3 |  575 | `		SyBlobNullAppend(&sPath);` |
|      2 |  576 | `	}else{` |
|     24 |  577 | `		SyBlobRelease(&sPath);` |
|     24 |  578 | `		PH7_VfsExpandPath(pCtx,zFile,nFile,&sPath);` |
|      - |  579 | `	}` |
|    178 |  580 | `	rc = sqlite3_open_v2((const char *)SyBlobData(&sPath),&pConn->pDb,iFlags,0);` |
|    178 |  581 | `	SyBlobRelease(&sPath);` |
|    178 |  582 | `	if( rc != SQLITE_OK ){` |
|      - |  583 | `		/* sqlite hands a handle back even on failure so the message can be read` |
|      - |  584 | `		 * off it; take the text first, then drop the handle. */` |
|      8 |  585 | `		const char *zMsg = pConn->pDb ? sqlite3_errmsg(pConn->pDb) : sqlite3_errstr(rc);` |
|      - |  586 | `		SyBlob sMsg;` |
|      - |  587 | `		sxi32 rcThrow;` |
|      8 |  588 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|      8 |  589 | `		SyBlobAppend(&sMsg,zMsg,SyStrlen(zMsg));` |
|      8 |  590 | `		SyBlobAppend(&sMsg,"",1);` |
|     11 |  591 | `		rcThrow = PH7_VmThrowException(pCtx,"Exception","Unable to open database: %s",` |
|      6 |  592 | `			(const char *)SyBlobData(&sMsg));` |
|      8 |  593 | `		SyBlobRelease(&sMsg);` |
|      8 |  594 | `		Sq3Close(pConn);` |
|      8 |  595 | `		return rcThrow;` |
|      - |  596 | `	}` |
|      - |  597 | ``	/* php's `sqlite3.defensive`, applied to every connection it opens: with it`` |
|      - |  598 | ``	 * on, `PRAGMA writable_schema` still succeeds and the UPDATE behind it does`` |
|      - |  599 | `	 * not ("table sqlite_master may not be modified"). */` |
|    172 |  600 | `	if( PH7_VmIniGetBool(pCtx->pVm,"sqlite3.defensive",1) ){` |
|    172 |  601 | `		sqlite3_db_config(pConn->pDb,SQLITE_DBCONFIG_DEFENSIVE,1,(int *)0);` |
|     84 |  602 | `	}` |
|    172 |  603 | `	pConn->bInitialised = 1;` |
|    172 |  604 | `	return PH7_OK;` |
|     92 |  605 | `}` |
|      - |  606 | `/*` |
|      - |  607 | ` * SQLite3::__construct(string $filename, int $flags = SQLITE3_OPEN_READWRITE \|` |
|      - |  608 | ` *   SQLITE3_OPEN_CREATE, string $encryptionKey = '')` |
|      - |  609 | ` */` |
|    168 |  610 | `static int vm_builtin_SQLite3_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  611 | `{` |
|    172 |  612 | `	return Sq3OpenImpl(pCtx,nArg,apArg);` |
|      4 |  613 | `}` |
|      - |  614 | `/* SQLite3::open(...): void -- the constructor's body, callable again after close() */` |
|      8 |  615 | `static int vm_builtin_SQLite3_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  616 | `{` |
|      9 |  617 | `	return Sq3OpenImpl(pCtx,nArg,apArg);` |
|      1 |  618 | `}` |
|      - |  619 | `/*` |
|      - |  620 | ` * SQLite3::close(): bool` |
|      - |  621 | ` *` |
|      - |  622 | ` * TRUE for every call, including one on an object that was never opened: php` |
|      - |  623 | ` * asks nothing here, it just drops whatever handle is there. What close does` |
|      - |  624 | `` * NOT do is lower `initialised`, which is what keeps lastErrorMsg() answering`` |
|      - |  625 | ` * afterwards.` |
|      - |  626 | ` */` |
|     68 |  627 | `static int vm_builtin_SQLite3_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  628 | `{` |
|     71 |  629 | `	phl_sq3 *pConn = Sq3OfInstance(PH7_ContextThis(pCtx));` |
|      - |  630 | `	int rcClose;` |
|     34 |  631 | `	SXUNUSED(nArg);` |
|     34 |  632 | `	SXUNUSED(apArg);` |
|     71 |  633 | `	if( pConn == 0 \|\| pConn->pDb == 0 ){` |
|      5 |  634 | `		ph7_result_bool(pCtx,1);` |
|      5 |  635 | `		return PH7_OK;` |
|      - |  636 | `	}` |
|      - |  637 | `	/* the statements go first and unconditionally: a script's own statement is` |
|      - |  638 | `	 * finalized even when the close that follows FAILS */` |
|     67 |  639 | `	Sq3FinalizeStmts(pConn);` |
|      - |  640 | `	/* and the close itself is the one that can fail -- not _v2, so an open blob` |
|      - |  641 | `	 * handle is a refusal a script is told about rather than something closed` |
|      - |  642 | `	 * behind its back */` |
|     67 |  643 | `	rcClose = sqlite3_close(pConn->pDb);` |
|     67 |  644 | `	if( rcClose != SQLITE_OK ){` |
|      - |  645 | `		SyBlob sMsg;` |
|      - |  646 | `		sxi32 rc;` |
|      5 |  647 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|      5 |  648 | `		SyBlobFormat(&sMsg,"Unable to close database: %s",sqlite3_errmsg(pConn->pDb));` |
|      5 |  649 | `		SyBlobNullAppend(&sMsg);` |
|      5 |  650 | `		ph7_result_bool(pCtx,0);` |
|      5 |  651 | `		rc = Sq3Error(pCtx,pConn,"SQLite3::close",rcClose,(const char *)SyBlobData(&sMsg));` |
|      5 |  652 | `		SyBlobRelease(&sMsg);` |
|      5 |  653 | `		return rc;   /* the connection is still open, and still usable */` |
|      - |  654 | `	}` |
|     63 |  655 | `	Sq3CloseDone(pConn);` |
|     63 |  656 | `	ph7_result_bool(pCtx,1);` |
|     63 |  657 | `	return PH7_OK;` |
|     37 |  658 | `}` |
|      - |  659 |  |
|      - |  660 | `/* ------------------------------------------------------------------------` |
|      - |  661 | ` * What the library says about itself` |
|      - |  662 | ` * ------------------------------------------------------------------------ */` |
|      - |  663 | `/*` |
|      - |  664 | ` * SQLite3::version(): array` |
|      - |  665 | ` *` |
|      - |  666 | ` * The LINKED library's version, so it differs between this engine's platforms` |
|      - |  667 | ` * (3.45 on a Debian host, whatever vcpkg last shipped on Windows) -- which is` |
|      - |  668 | ` * why no test may pin the numbers.` |
|      - |  669 | ` */` |
|      2 |  670 | `static int vm_builtin_SQLite3_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  671 | `{` |
|      - |  672 | `	ph7_value *pArray,*pVal;` |
|      1 |  673 | `	SXUNUSED(nArg);` |
|      1 |  674 | `	SXUNUSED(apArg);` |
|      3 |  675 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 |  676 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      3 |  677 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  678 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  679 | `	}` |
|      3 |  680 | `	ph7_value_string(pVal,sqlite3_libversion(),-1);` |
|      3 |  681 | `	ph7_array_add_strkey_elem(pArray,"versionString",pVal);` |
|      3 |  682 | `	ph7_value_int64(pVal,(ph7_int64)sqlite3_libversion_number());` |
|      3 |  683 | `	ph7_array_add_strkey_elem(pArray,"versionNumber",pVal);` |
|      3 |  684 | `	ph7_result_value(pCtx,pArray);` |
|      3 |  685 | `	return PH7_OK;` |
|      2 |  686 | `}` |
|      - |  687 | `/*` |
|      - |  688 | ` * SQLite3::escapeString(string $string): string` |
|      - |  689 | ` *` |
|      - |  690 | `` * sqlite's own `%q`, which doubles every single quote and NOTHING else -- so`` |
|      - |  691 | ` * the answer is what goes INSIDE a pair of quotes, unlike PDO::quote(), which` |
|      - |  692 | ` * supplies them. It is a C string to the library, so a NUL byte ends the` |
|      - |  693 | ` * answer there rather than being escaped; php has the same cut.` |
|      - |  694 | ` */` |
|      8 |  695 | `static int vm_builtin_SQLite3_escapeString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  696 | `{` |
|      - |  697 | `	const char *zIn;` |
|      9 |  698 | `	int nIn = 0;` |
|      - |  699 | `	char *zOut;` |
|      - |  700 | `	SyBlob sTerm;` |
|      4 |  701 | `	SXUNUSED(nArg);` |
|      9 |  702 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|      9 |  703 | `	if( nIn < 1 ){` |
|      3 |  704 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  705 | `		return PH7_OK;` |
|      - |  706 | `	}` |
|      7 |  707 | `	SyBlobInit(&sTerm,&pCtx->pVm->sAllocator);` |
|      7 |  708 | `	SyBlobAppend(&sTerm,zIn,(sxu32)nIn);` |
|      7 |  709 | `	SyBlobAppend(&sTerm,"",1);` |
|      7 |  710 | `	zOut = sqlite3_mprintf("%q",(const char *)SyBlobData(&sTerm));` |
|      7 |  711 | `	SyBlobRelease(&sTerm);` |
|      7 |  712 | `	if( zOut == 0 ){` |
|    ! 0 |  713 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  714 | `	}` |
|      7 |  715 | `	ph7_result_string(pCtx,zOut,-1);` |
|      7 |  716 | `	sqlite3_free(zOut);` |
|      7 |  717 | `	return PH7_OK;` |
|      5 |  718 | `}` |
|      - |  719 |  |
|      - |  720 | `/* ------------------------------------------------------------------------` |
|      - |  721 | ` * The last failure, as the connection reports it` |
|      - |  722 | ` * ------------------------------------------------------------------------ */` |
|      - |  723 | `/*` |
|      - |  724 | ` * The three error readers share php's softer guard: they need an object that` |
|      - |  725 | ` * was opened ONCE, not one that is open NOW. A closed connection has no` |
|      - |  726 | ` * library state left to ask, so each answers its type's zero.` |
|      - |  727 | ` */` |
|     44 |  728 | `static phl_sq3 * Sq3ErrReader(ph7_context *pCtx,sxi32 *pRc)` |
|      3 |  729 | `{` |
|     47 |  730 | `	phl_sq3 *pConn = Sq3OfInstance(PH7_ContextThis(pCtx));` |
|     47 |  731 | `	*pRc = PH7_OK;` |
|     47 |  732 | `	if( pConn == 0 \|\| !pConn->bInitialised ){` |
|      5 |  733 | `		*pRc = Sq3Uninitialised(pCtx,"SQLite3");` |
|      5 |  734 | `		return 0;` |
|      - |  735 | `	}` |
|     43 |  736 | `	return pConn;` |
|     25 |  737 | `}` |
|     20 |  738 | `static int vm_builtin_SQLite3_lastErrorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  739 | `{` |
|      - |  740 | `	sxi32 rc;` |
|     23 |  741 | `	phl_sq3 *pConn = Sq3ErrReader(pCtx,&rc);` |
|     10 |  742 | `	SXUNUSED(nArg);` |
|     10 |  743 | `	SXUNUSED(apArg);` |
|     23 |  744 | `	if( pConn == 0 ){` |
|      3 |  745 | `		return rc;` |
|      - |  746 | `	}` |
|      - |  747 | `	/* sqlite3_errcode answers the EXTENDED code once extended result codes are` |
|      - |  748 | `	 * on, so this reader and lastExtendedErrorCode() agree from then on -- which` |
|      - |  749 | `	 * is php's answer too, since php reads the same two functions. */` |
|     21 |  750 | `	ph7_result_int64(pCtx,pConn->pDb ? (ph7_int64)sqlite3_errcode(pConn->pDb) : 0);` |
|     21 |  751 | `	return PH7_OK;` |
|     13 |  752 | `}` |
|      8 |  753 | `static int vm_builtin_SQLite3_lastExtendedErrorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  754 | `{` |
|      - |  755 | `	sxi32 rc;` |
|      9 |  756 | `	phl_sq3 *pConn = Sq3ErrReader(pCtx,&rc);` |
|      4 |  757 | `	SXUNUSED(nArg);` |
|      4 |  758 | `	SXUNUSED(apArg);` |
|      9 |  759 | `	if( pConn == 0 ){` |
|    ! 0 |  760 | `		return rc;` |
|      - |  761 | `	}` |
|      9 |  762 | `	ph7_result_int64(pCtx,pConn->pDb ? (ph7_int64)sqlite3_extended_errcode(pConn->pDb) : 0);` |
|      9 |  763 | `	return PH7_OK;` |
|      5 |  764 | `}` |
|      8 |  765 | `static int vm_builtin_SQLite3_lastErrorMsg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  766 | `{` |
|      - |  767 | `	sxi32 rc;` |
|      9 |  768 | `	phl_sq3 *pConn = Sq3ErrReader(pCtx,&rc);` |
|      4 |  769 | `	SXUNUSED(nArg);` |
|      4 |  770 | `	SXUNUSED(apArg);` |
|      9 |  771 | `	if( pConn == 0 ){` |
|    ! 0 |  772 | `		return rc;` |
|      - |  773 | `	}` |
|      - |  774 | `	/* An open handle that has met no failure says "not an error"; a closed one` |
|      - |  775 | `	 * says the empty string, because there is nothing left to ask. */` |
|      9 |  776 | `	if( pConn->pDb ){` |
|      7 |  777 | `		ph7_result_string(pCtx,sqlite3_errmsg(pConn->pDb),-1);` |
|      4 |  778 | `	}else{` |
|      3 |  779 | `		ph7_result_string(pCtx,"",0);` |
|      - |  780 | `	}` |
|      9 |  781 | `	return PH7_OK;` |
|      5 |  782 | `}` |
|      - |  783 |  |
|      - |  784 | `/* ------------------------------------------------------------------------` |
|      - |  785 | ` * Running one string of SQL` |
|      - |  786 | ` * ------------------------------------------------------------------------ */` |
|      - |  787 | `/* The guard every verb that TOUCHES the database shares. */` |
|    640 |  788 | `static phl_sq3 * Sq3LiveDb(ph7_context *pCtx,sxi32 *pRc)` |
|      4 |  789 | `{` |
|    644 |  790 | `	phl_sq3 *pConn = Sq3OfInstance(PH7_ContextThis(pCtx));` |
|    644 |  791 | `	*pRc = PH7_OK;` |
|    644 |  792 | `	if( pConn == 0 \|\| pConn->pDb == 0 ){` |
|     16 |  793 | `		*pRc = Sq3Uninitialised(pCtx,"SQLite3");` |
|     16 |  794 | `		return 0;` |
|      - |  795 | `	}` |
|    630 |  796 | `	return pConn;` |
|    324 |  797 | `}` |
|      - |  798 | `/*` |
|      - |  799 | ` * SQLite3::exec(string $query): bool` |
|      - |  800 | ` *` |
|      - |  801 | ` * sqlite3_exec over the whole string: every statement in it runs, and the` |
|      - |  802 | ` * answer is only whether they all did. The empty string is a successful run of` |
|      - |  803 | ` * nothing, which is TRUE -- unlike PDO::exec(), whose own ValueError refuses` |
|      - |  804 | ` * it.` |
|      - |  805 | ` */` |
|    240 |  806 | `static int vm_builtin_SQLite3_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  807 | `{` |
|      - |  808 | `	sxi32 rc;` |
|    243 |  809 | `	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);` |
|      - |  810 | `	const char *zSql;` |
|    243 |  811 | `	int nSql = 0;` |
|      - |  812 | `	SyBlob sSql;` |
|      - |  813 | `	Sq3Verb sVerb;` |
|      - |  814 | `	int rcSql;` |
|    120 |  815 | `	SXUNUSED(nArg);` |
|    243 |  816 | `	if( pConn == 0 ){` |
|      5 |  817 | `		return rc;` |
|      - |  818 | `	}` |
|    239 |  819 | `	zSql = ph7_value_to_string(apArg[0],&nSql);` |
|    239 |  820 | `	SyBlobInit(&sSql,&pCtx->pVm->sAllocator);` |
|    239 |  821 | `	if( nSql > 0 ){` |
|    237 |  822 | `		SyBlobAppend(&sSql,zSql,(sxu32)nSql);` |
|    117 |  823 | `	}` |
|    239 |  824 | `	SyBlobAppend(&sSql,"",1);` |
|    239 |  825 | `	Sq3VerbEnter(pConn,pCtx,"SQLite3::exec",&sVerb);` |
|    239 |  826 | `	rcSql = sqlite3_exec(pConn->pDb,(const char *)SyBlobData(&sSql),0,0,0);` |
|    239 |  827 | `	SyBlobRelease(&sSql);` |
|    239 |  828 | `	rc = Sq3VerbLeave(pConn,&sVerb);` |
|    239 |  829 | `	if( rc != PH7_OK ){` |
|    ! 0 |  830 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  831 | `		return rc;   /* a callback threw: that is the answer, not sqlite's */` |
|      - |  832 | `	}` |
|    239 |  833 | `	if( rcSql != SQLITE_OK ){` |
|     13 |  834 | `		ph7_result_bool(pCtx,0);` |
|     13 |  835 | `		return Sq3ErrorFromDb(pCtx,pConn,"SQLite3::exec");` |
|      - |  836 | `	}` |
|    227 |  837 | `	ph7_result_bool(pCtx,1);` |
|    227 |  838 | `	return PH7_OK;` |
|    123 |  839 | `}` |
|      - |  840 |  |
|      - |  841 | `/* ------------------------------------------------------------------------` |
|      - |  842 | ` * Preparing, stepping, and the cursor a script walks` |
|      - |  843 | ` * ------------------------------------------------------------------------ */` |
|      - |  844 | `/* A fresh statement record, chained on its connection and retaining the object` |
|      - |  845 | ` * the connection lives in. */` |
|    238 |  846 | `static phl_sq3_stmt * Sq3NewStmt(phl_sq3 *pConn)` |
|      3 |  847 | `{` |
|    241 |  848 | `	phl_sq3_stmt *pSt = (phl_sq3_stmt *)SyMemBackendAlloc(&pConn->pVm->sAllocator,` |
|      - |  849 | `		sizeof(phl_sq3_stmt));` |
|    241 |  850 | `	if( pSt == 0 ){` |
|    ! 0 |  851 | `		return 0;` |
|      - |  852 | `	}` |
|    241 |  853 | `	SyZero(pSt,sizeof(phl_sq3_stmt));` |
|    241 |  854 | `	pSt->pConn = pConn;` |
|    241 |  855 | `	pSt->nRef = 1;` |
|    241 |  856 | `	pSt->pConnObj = pConn->pOwner;` |
|    241 |  857 | `	if( pSt->pConnObj ){` |
|    241 |  858 | `		pSt->pConnObj->iRef++;` |
|    119 |  859 | `	}` |
|    241 |  860 | `	pSt->pNext = pConn->pStmts;` |
|    241 |  861 | `	pConn->pStmts = pSt;` |
|    241 |  862 | `	return pSt;` |
|    122 |  863 | `}` |
|      - |  864 | `/*` |
|      - |  865 | ` * php's prepare, shared by prepare(), query() and querySingle().` |
|      - |  866 | ` *` |
|      - |  867 | ` * Two answers are not failures and are not the same either. An EMPTY query is` |
|      - |  868 | ` * refused before the library is asked -- php checks the length itself, so there` |
|      - |  869 | ` * is no diagnostic to report. A query that holds no STATEMENT (whitespace, a` |
|      - |  870 | ` * comment) prepares perfectly well and leaves sqlite's out-parameter NULL; php` |
|      - |  871 | ` * keeps that as a statement of nothing, and everything a script then does with` |
|      - |  872 | ` * it answers off the missing handle rather than off an error.` |
|      - |  873 | ` *` |
|      - |  874 | ` * Answers 0 when the caller should hand back false -- which both of those are,` |
|      - |  875 | ` * told apart only by whether anything was reported on the way.` |
|      - |  876 | ` */` |
|    258 |  877 | `static phl_sq3_stmt * Sq3Prepare(ph7_context *pCtx,phl_sq3 *pConn,const char *zSql,int nSql,` |
|      - |  878 | `	const char *zFn,sxi32 *pRc)` |
|      3 |  879 | `{` |
|      - |  880 | `	phl_sq3_stmt *pSt;` |
|    261 |  881 | `	sqlite3_stmt *pRaw = 0;` |
|      - |  882 | `	int rc;` |
|    261 |  883 | `	*pRc = PH7_OK;` |
|    261 |  884 | `	if( nSql < 1 ){` |
|      7 |  885 | `		return 0;` |
|      - |  886 | `	}` |
|    255 |  887 | `	rc = sqlite3_prepare_v2(pConn->pDb,zSql,nSql,&pRaw,0);` |
|    255 |  888 | `	if( rc != SQLITE_OK ){` |
|      - |  889 | `		SyBlob sMsg;` |
|     15 |  890 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|     15 |  891 | `		SyBlobFormat(&sMsg,"Unable to prepare statement: %s",sqlite3_errmsg(pConn->pDb));` |
|     15 |  892 | `		SyBlobNullAppend(&sMsg);` |
|     22 |  893 | `		*pRc = Sq3Error(pCtx,pConn,zFn,sqlite3_errcode(pConn->pDb),` |
|     14 |  894 | `			(const char *)SyBlobData(&sMsg));` |
|     15 |  895 | `		SyBlobRelease(&sMsg);` |
|     15 |  896 | `		return 0;` |
|      - |  897 | `	}` |
|    241 |  898 | `	pSt = Sq3NewStmt(pConn);` |
|    241 |  899 | `	if( pSt == 0 ){` |
|    ! 0 |  900 | `		if( pRaw ){` |
|    ! 0 |  901 | `			sqlite3_finalize(pRaw);` |
|    ! 0 |  902 | `		}` |
|    ! 0 |  903 | `		*pRc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  904 | `		return 0;` |
|      - |  905 | `	}` |
|    241 |  906 | `	pSt->pStmt = pRaw;` |
|    241 |  907 | `	pSt->bInitialised = 1;` |
|    241 |  908 | `	return pSt;` |
|    132 |  909 | `}` |
|      - |  910 | `/*` |
|      - |  911 | `` * Run a statement once and rewind it, which is what BOTH `query()` and`` |
|      - |  912 | `` * `execute()` do before they hand a cursor back. It is not a lookahead: the`` |
|      - |  913 | `` * step really runs, so `query("INSERT ...")` inserts, and the reset that`` |
|      - |  914 | ` * follows is what makes the result's first fetch start at the first row --` |
|      - |  915 | ` * and what makes a fetch on that INSERT's result insert a SECOND time.` |
|      - |  916 | ` *` |
|      - |  917 | ` * A statement of NOTHING has no handle to step and is a failure here, reported` |
|      - |  918 | ` * off the CONNECTION -- which has met no error, so what a comment-only query` |
|      - |  919 | `` * says is `Unable to execute statement: not an error`.`` |
|      - |  920 | ` */` |
|     54 |  921 | `static int Sq3StepOnce(ph7_context *pCtx,phl_sq3_stmt *pSt,const char *zFn,sxi32 *pRc)` |
|      1 |  922 | `{` |
|      - |  923 | `	int rc;` |
|     55 |  924 | `	*pRc = PH7_OK;` |
|     55 |  925 | `	if( pSt->pStmt ){` |
|     53 |  926 | `		rc = sqlite3_step(pSt->pStmt);` |
|     53 |  927 | `		if( rc == SQLITE_ROW \|\| rc == SQLITE_DONE ){` |
|     49 |  928 | `			sqlite3_reset(pSt->pStmt);` |
|     49 |  929 | `			return 1;` |
|      - |  930 | `		}` |
|      2 |  931 | `	}` |
|      - |  932 | `	{` |
|      - |  933 | `		SyBlob sMsg;` |
|      7 |  934 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|      7 |  935 | `		SyBlobFormat(&sMsg,"Unable to execute statement: %s",` |
|      6 |  936 | `			sqlite3_errmsg(pSt->pConn->pDb));` |
|      7 |  937 | `		SyBlobNullAppend(&sMsg);` |
|     10 |  938 | `		*pRc = Sq3Error(pCtx,pSt->pConn,zFn,sqlite3_errcode(pSt->pConn->pDb),` |
|      6 |  939 | `			(const char *)SyBlobData(&sMsg));` |
|      7 |  940 | `		SyBlobRelease(&sMsg);` |
|      - |  941 | `	}` |
|      7 |  942 | `	return 0;` |
|     28 |  943 | `}` |
|      - |  944 | `/*` |
|      - |  945 | ` * One column of the row sqlite has up, in the type sqlite says it is.` |
|      - |  946 | ` *` |
|      - |  947 | ` * The cursor is reset first because callers reuse ONE scalar down a row: a` |
|      - |  948 | ` * string write appends, so without this the second text column of a row comes` |
|      - |  949 | ` * back carrying the first one's bytes in front of it.` |
|      - |  950 | ` */` |
|    274 |  951 | `static void Sq3ColumnValue(sqlite3_stmt *pStmt,int iCol,ph7_value *pOut)` |
|      2 |  952 | `{` |
|    276 |  953 | `	ph7_value_reset_string_cursor(pOut);` |
|    276 |  954 | `	switch( sqlite3_column_type(pStmt,iCol) ){` |
|     43 |  955 | `		case SQLITE_INTEGER:` |
|     88 |  956 | `			ph7_value_int64(pOut,(ph7_int64)sqlite3_column_int64(pStmt,iCol));` |
|     88 |  957 | `			break;` |
|      - |  958 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      6 |  959 | `		case SQLITE_FLOAT:` |
|     13 |  960 | `			ph7_value_double(pOut,sqlite3_column_double(pStmt,iCol));` |
|     13 |  961 | `			break;` |
|      - |  962 | `#endif` |
|      9 |  963 | `		case SQLITE_NULL:` |
|     19 |  964 | `			ph7_value_null(pOut);` |
|     19 |  965 | `			break;` |
|     12 |  966 | `		case SQLITE_BLOB: {` |
|     26 |  967 | `			const void *pBlob = sqlite3_column_blob(pStmt,iCol);` |
|     26 |  968 | `			int nByte = sqlite3_column_bytes(pStmt,iCol);` |
|     26 |  969 | `			ph7_value_string(pOut,pBlob ? (const char *)pBlob : "",nByte);` |
|     26 |  970 | `			break;` |
|      - |  971 | `		}` |
|     67 |  972 | `		default: {` |
|    136 |  973 | `			const unsigned char *zText = sqlite3_column_text(pStmt,iCol);` |
|    136 |  974 | `			int nByte = sqlite3_column_bytes(pStmt,iCol);` |
|    136 |  975 | `			ph7_value_string(pOut,zText ? (const char *)zText : "",nByte);` |
|    134 |  976 | `			break;` |
|      - |  977 | `		}` |
|      - |  978 | `	}` |
|    276 |  979 | `}` |
|      - |  980 | `/* Build the SQLite3Result object over one statement. */` |
|    116 |  981 | `static ph7_class_instance * Sq3NewResultObject(ph7_context *pCtx,phl_sq3_stmt *pSt,` |
|      - |  982 | `	ph7_class_instance *pStmtObj)` |
|      1 |  983 | `{` |
|    117 |  984 | `	ph7_vm *pVm = pCtx->pVm;` |
|    117 |  985 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"SQLite3Result",` |
|      - |  986 | `		sizeof("SQLite3Result")-1,FALSE,0);` |
|      - |  987 | `	ph7_class_instance *pObj;` |
|      - |  988 | `	phl_sq3_res *pRes;` |
|    117 |  989 | `	if( pClass == 0 ){` |
|    ! 0 |  990 | `		return 0;` |
|      - |  991 | `	}` |
|    117 |  992 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|    117 |  993 | `	if( pObj == 0 ){` |
|    ! 0 |  994 | `		return 0;` |
|      - |  995 | `	}` |
|    117 |  996 | `	pRes = (phl_sq3_res *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_sq3_res));` |
|    117 |  997 | `	if( pRes == 0 ){` |
|    ! 0 |  998 | `		PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  999 | `		return 0;` |
|      - | 1000 | `	}` |
|    117 | 1001 | `	SyZero(pRes,sizeof(phl_sq3_res));` |
|    117 | 1002 | `	pRes->pSt = pSt;` |
|    117 | 1003 | `	pRes->pConn = pSt->pConn;` |
|    117 | 1004 | `	pRes->pOwner = pObj;` |
|    117 | 1005 | `	pRes->pStmtObj = pStmtObj;` |
|    117 | 1006 | `	if( pStmtObj ){` |
|     71 | 1007 | `		pStmtObj->iRef++;` |
|     35 | 1008 | `	}` |
|    117 | 1009 | `	pRes->pNext = pSt->pConn->pResults;` |
|    117 | 1010 | `	pSt->pConn->pResults = pRes;` |
|    117 | 1011 | `	if( Sq3AttachRes(pObj,pRes) != 0 ){` |
|    ! 0 | 1012 | `		pRes->pSt = 0;   /* the caller still owns the statement hold */` |
|    ! 0 | 1013 | `		Sq3FreeRes(pRes);` |
|    ! 0 | 1014 | `		PH7_ClassInstanceUnref(pObj);` |
|    ! 0 | 1015 | `		return 0;` |
|      - | 1016 | `	}` |
|    117 | 1017 | `	return pObj;` |
|     59 | 1018 | `}` |
|    274 | 1019 | `static phl_sq3_res * Sq3ResOfInstance(ph7_class_instance *pThis)` |
|      1 | 1020 | `{` |
|    275 | 1021 | `	return (phl_sq3_res *)Sq3ResourceOf(pThis);` |
|      1 | 1022 | `}` |
|      - | 1023 | `/*` |
|      - | 1024 | ` * The guard every SQLite3Result verb shares. Two different things reach it:` |
|      - | 1025 | ` * a result whose own finalize() has run, and one whose STATEMENT was closed` |
|      - | 1026 | ` * from the other side -- php tells them apart nowhere, and neither does this.` |
|      - | 1027 | ` */` |
|    156 | 1028 | `static phl_sq3_res * Sq3LiveRes(ph7_context *pCtx,sxi32 *pRc)` |
|      1 | 1029 | `{` |
|    157 | 1030 | `	phl_sq3_res *pRes = Sq3ResOfInstance(PH7_ContextThis(pCtx));` |
|    157 | 1031 | `	*pRc = PH7_OK;` |
|    157 | 1032 | `	if( pRes == 0 \|\| pRes->pSt == 0 \|\| pRes->pSt->pStmt == 0 ){` |
|     17 | 1033 | `		*pRc = Sq3Uninitialised(pCtx,"SQLite3Result");` |
|     17 | 1034 | `		return 0;` |
|      - | 1035 | `	}` |
|    141 | 1036 | `	return pRes;` |
|     79 | 1037 | `}` |
|      - | 1038 | `/* The result object is going away: let go of the statement now. */` |
|    118 | 1039 | `static void Sq3ResInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 | 1040 | `{` |
|    119 | 1041 | `	phl_sq3_res *pRes = Sq3ResOfInstance(pThis);` |
|     59 | 1042 | `	SXUNUSED(pVm);` |
|    119 | 1043 | `	if( pRes == 0 \|\| pRes->pOwner != pThis ){` |
|      3 | 1044 | `		return;` |
|      - | 1045 | `	}` |
|    117 | 1046 | `	Sq3FreeRes(pRes);` |
|     60 | 1047 | `}` |
|      - | 1048 |  |
|      - | 1049 | `/*` |
|      - | 1050 | ` * SQLite3::query(string $query): SQLite3Result\|false` |
|      - | 1051 | ` *` |
|      - | 1052 | ` * Prepare, run once, rewind, and hand back a cursor. Only the FIRST statement` |
|      - | 1053 | `` * of the string is ever prepared, so `query('SELECT 1; garbage')` answers a`` |
|      - | 1054 | ` * row and never sees the garbage -- unlike exec(), which runs the whole string` |
|      - | 1055 | ` * through sqlite3_exec and refuses on the tail.` |
|      - | 1056 | ` */` |
|     66 | 1057 | `static int vm_builtin_SQLite3_query(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1058 | `{` |
|      - | 1059 | `	sxi32 rc;` |
|     67 | 1060 | `	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);` |
|      - | 1061 | `	const char *zSql;` |
|     67 | 1062 | `	int nSql = 0;` |
|      - | 1063 | `	phl_sq3_stmt *pSt;` |
|      - | 1064 | `	ph7_class_instance *pObj;` |
|      - | 1065 | `	Sq3Verb sVerb;` |
|     33 | 1066 | `	SXUNUSED(nArg);` |
|     67 | 1067 | `	if( pConn == 0 ){` |
|      3 | 1068 | `		return rc;` |
|      - | 1069 | `	}` |
|     65 | 1070 | `	zSql = ph7_value_to_string(apArg[0],&nSql);` |
|     65 | 1071 | `	Sq3VerbEnter(pConn,pCtx,"SQLite3::query",&sVerb);` |
|     65 | 1072 | `	pSt = Sq3Prepare(pCtx,pConn,zSql,nSql,"SQLite3::query",&rc);` |
|     65 | 1073 | `	if( pSt != 0 && !Sq3StepOnce(pCtx,pSt,"SQLite3::query",&rc) ){` |
|      7 | 1074 | `		Sq3StmtUnref(pSt);` |
|      7 | 1075 | `		pSt = 0;` |
|      3 | 1076 | `	}` |
|      - | 1077 | `	{` |
|     65 | 1078 | `		sxi32 rcCb = Sq3VerbLeave(pConn,&sVerb);` |
|     65 | 1079 | `		if( rcCb != PH7_OK ){` |
|      7 | 1080 | `			if( pSt ){` |
|      3 | 1081 | `				Sq3StmtUnref(pSt);` |
|      1 | 1082 | `			}` |
|      7 | 1083 | `			ph7_result_bool(pCtx,0);` |
|      7 | 1084 | `			return rcCb;` |
|      - | 1085 | `		}` |
|      - | 1086 | `	}` |
|     59 | 1087 | `	if( pSt == 0 ){` |
|     13 | 1088 | `		ph7_result_bool(pCtx,0);` |
|     13 | 1089 | `		return rc;` |
|      - | 1090 | `	}` |
|     47 | 1091 | `	pObj = Sq3NewResultObject(pCtx,pSt,0);` |
|     47 | 1092 | `	if( pObj == 0 ){` |
|    ! 0 | 1093 | `		Sq3StmtUnref(pSt);` |
|    ! 0 | 1094 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1095 | `	}` |
|     47 | 1096 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     47 | 1097 | `	return PH7_OK;` |
|     34 | 1098 | `}` |
|      - | 1099 | `/*` |
|      - | 1100 | ` * SQLite3::querySingle(string $query, bool $entireRow = false): mixed` |
|      - | 1101 | ` *` |
|      - | 1102 | ` * One statement, one step, and the statement is gone again -- there is no` |
|      - | 1103 | ` * cursor to hand back. What "no row" means depends on the shape the caller` |
|      - | 1104 | ` * asked for: a scalar query answers NULL and a whole-row query answers the` |
|      - | 1105 | ` * EMPTY ARRAY, which is why a write (which produces no row at all) answers` |
|      - | 1106 | ` * null rather than false.` |
|      - | 1107 | ` */` |
|    106 | 1108 | `static int vm_builtin_SQLite3_querySingle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1109 | `{` |
|      - | 1110 | `	sxi32 rc;` |
|    108 | 1111 | `	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);` |
|      - | 1112 | `	const char *zSql;` |
|    108 | 1113 | `	int nSql = 0;` |
|    108 | 1114 | `	int bWhole = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0;` |
|      - | 1115 | `	phl_sq3_stmt *pSt;` |
|      - | 1116 | `	Sq3Verb sVerb;` |
|      - | 1117 | `	int iStep;` |
|    108 | 1118 | `	if( pConn == 0 ){` |
|    ! 0 | 1119 | `		return rc;` |
|      - | 1120 | `	}` |
|    108 | 1121 | `	zSql = ph7_value_to_string(apArg[0],&nSql);` |
|    108 | 1122 | `	Sq3VerbEnter(pConn,pCtx,"SQLite3::querySingle",&sVerb);` |
|    108 | 1123 | `	pSt = Sq3Prepare(pCtx,pConn,zSql,nSql,"SQLite3::querySingle",&rc);` |
|    108 | 1124 | `	iStep = (pSt && pSt->pStmt) ? sqlite3_step(pSt->pStmt) : SQLITE_MISUSE;` |
|      - | 1125 | `	{` |
|    108 | 1126 | `		sxi32 rcCb = Sq3VerbLeave(pConn,&sVerb);` |
|    108 | 1127 | `		if( rcCb != PH7_OK ){` |
|      5 | 1128 | `			if( pSt ){` |
|      5 | 1129 | `				Sq3StmtUnref(pSt);` |
|      2 | 1130 | `			}` |
|      5 | 1131 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1132 | `			return rcCb;` |
|      - | 1133 | `		}` |
|      - | 1134 | `	}` |
|    104 | 1135 | `	if( pSt == 0 ){` |
|      7 | 1136 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1137 | `		return rc;` |
|      - | 1138 | `	}` |
|     98 | 1139 | `	if( iStep != SQLITE_ROW && iStep != SQLITE_DONE ){` |
|      - | 1140 | `		SyBlob sMsg;` |
|      3 | 1141 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|      3 | 1142 | `		SyBlobFormat(&sMsg,"Unable to execute statement: %s",sqlite3_errmsg(pConn->pDb));` |
|      3 | 1143 | `		SyBlobNullAppend(&sMsg);` |
|      3 | 1144 | `		ph7_result_bool(pCtx,0);` |
|      4 | 1145 | `		rc = Sq3Error(pCtx,pConn,"SQLite3::querySingle",sqlite3_errcode(pConn->pDb),` |
|      2 | 1146 | `			(const char *)SyBlobData(&sMsg));` |
|      3 | 1147 | `		SyBlobRelease(&sMsg);` |
|      3 | 1148 | `		Sq3StmtUnref(pSt);` |
|      3 | 1149 | `		return rc;` |
|      - | 1150 | `	}` |
|     96 | 1151 | `	if( bWhole ){` |
|      5 | 1152 | `		ph7_value *pRow = ph7_context_new_array(pCtx);` |
|      5 | 1153 | `		ph7_value *pCell = ph7_context_new_scalar(pCtx);` |
|      5 | 1154 | `		int i,nCol = iStep == SQLITE_ROW ? sqlite3_data_count(pSt->pStmt) : 0;` |
|      5 | 1155 | `		if( pRow == 0 \|\| pCell == 0 ){` |
|    ! 0 | 1156 | `			Sq3StmtUnref(pSt);` |
|    ! 0 | 1157 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1158 | `		}` |
|      9 | 1159 | `		for( i = 0 ; i < nCol ; ++i ){` |
|      5 | 1160 | `			Sq3ColumnValue(pSt->pStmt,i,pCell);` |
|      5 | 1161 | `			ph7_array_add_strkey_elem(pRow,sqlite3_column_name(pSt->pStmt,i),pCell);` |
|      3 | 1162 | `		}` |
|      5 | 1163 | `		ph7_result_value(pCtx,pRow);` |
|    137 | 1164 | `	}else if( iStep == SQLITE_ROW && sqlite3_data_count(pSt->pStmt) > 0 ){` |
|     88 | 1165 | `		ph7_value *pCell = ph7_context_new_scalar(pCtx);` |
|     88 | 1166 | `		if( pCell == 0 ){` |
|    ! 0 | 1167 | `			Sq3StmtUnref(pSt);` |
|    ! 0 | 1168 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1169 | `		}` |
|     88 | 1170 | `		Sq3ColumnValue(pSt->pStmt,0,pCell);` |
|     88 | 1171 | `		ph7_result_value(pCtx,pCell);` |
|     45 | 1172 | `	}else{` |
|      5 | 1173 | `		ph7_result_null(pCtx);` |
|      - | 1174 | `	}` |
|     96 | 1175 | `	Sq3StmtUnref(pSt);` |
|     96 | 1176 | `	return PH7_OK;` |
|     55 | 1177 | `}` |
|      - | 1178 |  |
|      - | 1179 | `/* ------------------------------------------------------------------------` |
|      - | 1180 | ` * Copying a database, and reading one value as a stream` |
|      - | 1181 | ` * ------------------------------------------------------------------------ */` |
|      - | 1182 | `/*` |
|      - | 1183 | ` * SQLite3::backup(SQLite3 $destination, string $sourceDatabase = 'main',` |
|      - | 1184 | ` *   string $destinationDatabase = 'main'): bool` |
|      - | 1185 | ` *` |
|      - | 1186 | ` * sqlite's own online backup, run to completion in one call. What php does NOT` |
|      - | 1187 | `` * do is check the two names: `sqlite3_backup_init` answers nothing for a`` |
|      - | 1188 | ` * database neither connection carries, and php reads that as having no work to` |
|      - | 1189 | ` * do rather than as a failure -- so a misspelt source is a quiet true and an` |
|      - | 1190 | ` * empty destination. The one refusal it words itself is the pair being the SAME` |
|      - | 1191 | ` * connection, which sqlite would deadlock on.` |
|      - | 1192 | ` */` |
|     10 | 1193 | `static int vm_builtin_SQLite3_backup(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1194 | `{` |
|      - | 1195 | `	sxi32 rc;` |
|     11 | 1196 | `	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);` |
|      - | 1197 | `	phl_sq3 *pDest;` |
|     11 | 1198 | `	const char *zSrc = "main",*zDest = "main";` |
|     11 | 1199 | `	int nSrc = (int)sizeof("main")-1,nDest = (int)sizeof("main")-1;` |
|      - | 1200 | `	SyBlob sSrc,sDest;` |
|      - | 1201 | `	sqlite3_backup *pBackup;` |
|     11 | 1202 | `	if( pConn == 0 ){` |
|      3 | 1203 | `		return rc;` |
|      - | 1204 | `	}` |
|     13 | 1205 | `	pDest = (apArg[0]->iFlags & MEMOBJ_OBJ) != 0` |
|      8 | 1206 | `		? Sq3OfInstance((ph7_class_instance *)apArg[0]->x.pOther) : 0;` |
|      9 | 1207 | `	if( pDest == 0 \|\| pDest->pDb == 0 ){` |
|      - | 1208 | `		/* the destination is asked the same question the receiver was */` |
|      3 | 1209 | `		return Sq3Uninitialised(pCtx,"SQLite3");` |
|      - | 1210 | `	}` |
|      7 | 1211 | `	if( pDest == pConn ){` |
|      3 | 1212 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1213 | `		return Sq3Error(pCtx,pConn,"SQLite3::backup",0,` |
|      - | 1214 | `			"Backup failed: source and destination must be distinct");` |
|      - | 1215 | `	}` |
|      5 | 1216 | `	if( nArg > 1 ){` |
|      3 | 1217 | `		zSrc = ph7_value_to_string(apArg[1],&nSrc);` |
|      1 | 1218 | `	}` |
|      5 | 1219 | `	if( nArg > 2 ){` |
|    ! 0 | 1220 | `		zDest = ph7_value_to_string(apArg[2],&nDest);` |
|    ! 0 | 1221 | `	}` |
|      5 | 1222 | `	SyBlobInit(&sSrc,&pCtx->pVm->sAllocator);` |
|      5 | 1223 | `	SyBlobAppend(&sSrc,zSrc,(sxu32)nSrc);` |
|      5 | 1224 | `	SyBlobNullAppend(&sSrc);` |
|      5 | 1225 | `	SyBlobInit(&sDest,&pCtx->pVm->sAllocator);` |
|      5 | 1226 | `	SyBlobAppend(&sDest,zDest,(sxu32)nDest);` |
|      5 | 1227 | `	SyBlobNullAppend(&sDest);` |
|      7 | 1228 | `	pBackup = sqlite3_backup_init(pDest->pDb,(const char *)SyBlobData(&sDest),` |
|      4 | 1229 | `		pConn->pDb,(const char *)SyBlobData(&sSrc));` |
|      5 | 1230 | `	SyBlobRelease(&sSrc);` |
|      5 | 1231 | `	SyBlobRelease(&sDest);` |
|      5 | 1232 | `	if( pBackup == 0 ){` |
|      - | 1233 | `		/* nothing to copy, and php calls that done */` |
|      3 | 1234 | `		ph7_result_bool(pCtx,1);` |
|      3 | 1235 | `		return PH7_OK;` |
|      - | 1236 | `	}` |
|      - | 1237 | `	/* -1 pages is "all of it": one step, no progress callback to report to */` |
|      3 | 1238 | `	sqlite3_backup_step(pBackup,-1);` |
|      3 | 1239 | `	ph7_result_bool(pCtx,sqlite3_backup_finish(pBackup) == SQLITE_OK);` |
|      3 | 1240 | `	return PH7_OK;` |
|      6 | 1241 | `}` |
|      - | 1242 | `/*` |
|      - | 1243 | ` * One open blob: a HANDLE on the bytes of one value, which php hands back as a` |
|      - | 1244 | ` * stream. It addresses bytes that already exist -- there is no growing one and` |
|      - | 1245 | ` * nowhere past the end to seek to -- and sqlite refuses to close a database` |
|      - | 1246 | ` * while one is open, which is the one thing that makes SQLite3::close() fail.` |
|      - | 1247 | ` */` |
|      - | 1248 | `typedef struct phl_sq3_blob phl_sq3_blob;` |
|      - | 1249 | `struct phl_sq3_blob {` |
|      - | 1250 | `	sqlite3_blob *pBlob;          /* 0 once closed */` |
|      - | 1251 | `	ph7_vm *pVm;                  /* the allocator: a handle ORPHANED by its connection` |
|      - | 1252 | `	                               * still has to give its own memory back */` |
|      - | 1253 | `	phl_sq3 *pConn;               /* 0 once the connection closed underneath it */` |
|      - | 1254 | `	ph7_int64 iOfft;              /* the stream cursor: sqlite reads at an offset */` |
|      - | 1255 | `	ph7_int64 nSize;              /* the blob's length, fixed for its lifetime */` |
|      - | 1256 | `	int bWrite;                   /* the HANDLE sqlite opened is writable: flags & READWRITE */` |
|      - | 1257 | `	int iFlags;                   /* and the flags themselves, which the stream reads` |
|      - | 1258 | `	                               * separately -- php refuses a write for the READONLY bit` |
|      - | 1259 | `	                               * being SET rather than for READWRITE being absent, so the` |
|      - | 1260 | `	                               * two are different questions and 0 answers neither */` |
|      - | 1261 | `	int bBadPos;                  /* a seek OUT of the blob left the position unknown */` |
|      - | 1262 | `	io_private *pDev;             /* the stream the script holds: the read below marks its` |
|      - | 1263 | `	                               * end-of-file itself, since reaching the last byte is` |
|      - | 1264 | `	                               * what php calls eof here and a SEEK to the end is not */` |
|      - | 1265 | `	phl_sq3_blob *pNext;` |
|      - | 1266 | `};` |
|      4 | 1267 | `static void Sq3BlobClose(phl_sq3_blob *pBl)` |
|      1 | 1268 | `{` |
|      5 | 1269 | `	phl_sq3 *pConn = pBl->pConn;` |
|      5 | 1270 | `	if( pBl->pBlob ){` |
|      5 | 1271 | `		sqlite3_blob_close(pBl->pBlob);` |
|      5 | 1272 | `		pBl->pBlob = 0;` |
|      2 | 1273 | `	}` |
|      5 | 1274 | `	if( pConn ){` |
|      5 | 1275 | `		phl_sq3_blob *pCur,*pPrev = 0;` |
|      5 | 1276 | `		for( pCur = pConn->pBlobs ; pCur ; pPrev = pCur, pCur = pCur->pNext ){` |
|      5 | 1277 | `			if( pCur == pBl ){` |
|      5 | 1278 | `				if( pPrev ){` |
|    ! 0 | 1279 | `					pPrev->pNext = pCur->pNext;` |
|    ! 0 | 1280 | `				}else{` |
|      5 | 1281 | `					pConn->pBlobs = pCur->pNext;` |
|      - | 1282 | `				}` |
|      5 | 1283 | `				break;` |
|      - | 1284 | `			}` |
|    ! 0 | 1285 | `		}` |
|      2 | 1286 | `	}` |
|      5 | 1287 | `	SyMemBackendFree(&pBl->pVm->sAllocator,pBl);` |
|      5 | 1288 | `}` |
|      - | 1289 | `/*` |
|      - | 1290 | ` * Close every handle a script left open and CUT them loose. Only the teardown` |
|      - | 1291 | ` * paths do this: a script's own close() is refused while one is open rather` |
|      - | 1292 | ` * than taking it away.` |
|      - | 1293 | ` */` |
|    350 | 1294 | `static void Sq3BlobSweep(phl_sq3 *pConn)` |
|      4 | 1295 | `{` |
|    565 | 1296 | `	while( pConn->pBlobs ){` |
|     37 | 1297 | `		phl_sq3_blob *pBl = pConn->pBlobs;` |
|     37 | 1298 | `		pConn->pBlobs = pBl->pNext;` |
|     37 | 1299 | `		pBl->pNext = 0;` |
|     37 | 1300 | `		pBl->pConn = 0;` |
|     37 | 1301 | `		if( pBl->pBlob ){` |
|     37 | 1302 | `			sqlite3_blob_close(pBl->pBlob);` |
|     37 | 1303 | `			pBl->pBlob = 0;` |
|     18 | 1304 | `		}` |
|      1 | 1305 | `	}` |
|    354 | 1306 | `}` |
|     20 | 1307 | `static ph7_int64 Sq3BlobStream_Read(void *pHandle,void *pBuf,ph7_int64 nDatatoRead)` |
|      1 | 1308 | `{` |
|     21 | 1309 | `	phl_sq3_blob *pBl = (phl_sq3_blob *)pHandle;` |
|      - | 1310 | `	ph7_int64 nLeft;` |
|     21 | 1311 | `	if( pBl->pBlob == 0 \|\| pBl->bBadPos ){` |
|    ! 0 | 1312 | `		return -1;` |
|      - | 1313 | `	}` |
|     21 | 1314 | `	nLeft = pBl->nSize - pBl->iOfft;` |
|     21 | 1315 | `	if( pBl->pDev && pBl->iOfft + nDatatoRead >= pBl->nSize ){` |
|      - | 1316 | `		/* php marks the end when a read REACHES the last byte, not when one` |
|      - | 1317 | `		 * comes back empty -- so reading a blob whole leaves feof() true, while` |
|      - | 1318 | `		 * seeking to the end leaves it false. */` |
|     17 | 1319 | `		pBl->pDev->bEof = 1;` |
|      8 | 1320 | `	}` |
|     21 | 1321 | `	if( nLeft < 1 \|\| nDatatoRead < 1 ){` |
|      7 | 1322 | `		return 0;` |
|      - | 1323 | `	}` |
|     15 | 1324 | `	if( nDatatoRead > nLeft ){` |
|      9 | 1325 | `		nDatatoRead = nLeft;` |
|      4 | 1326 | `	}` |
|     15 | 1327 | `	if( sqlite3_blob_read(pBl->pBlob,pBuf,(int)nDatatoRead,(int)pBl->iOfft) != SQLITE_OK ){` |
|    ! 0 | 1328 | `		return -1;` |
|      - | 1329 | `	}` |
|     15 | 1330 | `	pBl->iOfft += nDatatoRead;` |
|     15 | 1331 | `	return nDatatoRead;` |
|     11 | 1332 | `}` |
|     18 | 1333 | `static ph7_int64 Sq3BlobStream_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 1334 | `{` |
|     19 | 1335 | `	phl_sq3_blob *pBl = (phl_sq3_blob *)pHandle;` |
|     19 | 1336 | `	if( pBl->pBlob == 0 \|\| pBl->bBadPos ){` |
|    ! 0 | 1337 | `		return -1;` |
|      - | 1338 | `	}` |
|     19 | 1339 | `	if( nWrite < 1 ){` |
|    ! 0 | 1340 | `		return 0;   /* php asks nothing at all of a write of nothing */` |
|      - | 1341 | `	}` |
|     19 | 1342 | `	if( pBl->iFlags & SQLITE_OPEN_READONLY ){` |
|      - | 1343 | `		/* php's own sentence, from the device rather than from fwrite(), and it` |
|      - | 1344 | `		 * is asked FIRST. Note what it tests: the READONLY bit being SET, not` |
|      - | 1345 | `		 * READWRITE being absent -- so a handle opened with neither (0, or` |
|      - | 1346 | `		 * CREATE, which means nothing here) reaches the checks below and then` |
|      - | 1347 | `		 * fails SILENTLY in sqlite, which php reports as a plain false. */` |
|      9 | 1348 | `		PH7_VmThrowError(pBl->pVm,0,PH7_CTX_WARNING,` |
|      - | 1349 | `			"fwrite(): Can't write to blob stream: is open as read only");` |
|      9 | 1350 | `		return -1;` |
|      - | 1351 | `	}` |
|     11 | 1352 | `	if( pBl->iOfft + nWrite > pBl->nSize ){` |
|      - | 1353 | `		/* the handle addresses the bytes that are already there: php refuses a` |
|      - | 1354 | `		 * write needing more of them rather than writing what fits */` |
|      3 | 1355 | `		PH7_VmThrowError(pBl->pVm,0,PH7_CTX_WARNING,` |
|      - | 1356 | `			"fwrite(): It is not possible to increase the size of a BLOB");` |
|      3 | 1357 | `		return -1;` |
|      - | 1358 | `	}` |
|      9 | 1359 | `	if( sqlite3_blob_write(pBl->pBlob,pBuf,(int)nWrite,(int)pBl->iOfft) != SQLITE_OK ){` |
|      5 | 1360 | `		return -1;` |
|      - | 1361 | `	}` |
|      5 | 1362 | `	pBl->iOfft += nWrite;` |
|      5 | 1363 | `	return nWrite;` |
|     10 | 1364 | `}` |
|     16 | 1365 | `static int Sq3BlobStream_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 1366 | `{` |
|     17 | 1367 | `	phl_sq3_blob *pBl = (phl_sq3_blob *)pHandle;` |
|     17 | 1368 | `	ph7_int64 iNew = iOfft;` |
|     17 | 1369 | `	if( whence == 1 ){          /* SEEK_CUR */` |
|    ! 0 | 1370 | `		iNew = pBl->iOfft + iOfft;` |
|     17 | 1371 | `	}else if( whence == 2 ){    /* SEEK_END */` |
|      5 | 1372 | `		iNew = pBl->nSize + iOfft;` |
|      2 | 1373 | `	}` |
|     17 | 1374 | `	if( iNew < 0 \|\| iNew > pBl->nSize ){` |
|      - | 1375 | `		/* nowhere past the end to seek TO, and a failed seek leaves the position` |
|      - | 1376 | `		 * UNKNOWN -- ftell() answers false until a seek succeeds again */` |
|      3 | 1377 | `		pBl->bBadPos = 1;` |
|      3 | 1378 | `		return -1;` |
|      - | 1379 | `	}` |
|     15 | 1380 | `	pBl->bBadPos = 0;` |
|     15 | 1381 | `	pBl->iOfft = iNew;` |
|     15 | 1382 | `	return PH7_OK;` |
|      9 | 1383 | `}` |
|     18 | 1384 | `static ph7_int64 Sq3BlobStream_Tell(void *pHandle)` |
|      1 | 1385 | `{` |
|     19 | 1386 | `	phl_sq3_blob *pBl = (phl_sq3_blob *)pHandle;` |
|     19 | 1387 | `	return pBl->bBadPos ? -1 : pBl->iOfft;` |
|      1 | 1388 | `}` |
|      - | 1389 | `/*` |
|      - | 1390 | ` * php's stat for a blob handle: the whole thirteen-field shape with a SIZE and` |
|      - | 1391 | ` * nothing else -- every other field is a zero rather than absent, which is what` |
|      - | 1392 | ` * makes fstat() answer the 26 entries (numeric and named) it answers for a` |
|      - | 1393 | ` * file.` |
|      - | 1394 | ` */` |
|      4 | 1395 | `static int Sq3BlobStream_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|      1 | 1396 | `{` |
|      - | 1397 | `	static const char *const azField[] = {` |
|      - | 1398 | `		"dev","ino","mode","nlink","uid","gid","rdev","size",` |
|      - | 1399 | `		"atime","mtime","ctime","blksize","blocks"` |
|      - | 1400 | `	};` |
|      5 | 1401 | `	phl_sq3_blob *pBl = (phl_sq3_blob *)pHandle;` |
|      - | 1402 | `	sxu32 n;` |
|     57 | 1403 | `	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){` |
|     53 | 1404 | `		ph7_value_int64(pWorker,n == 7 /* size */ ? pBl->nSize : 0);` |
|     53 | 1405 | `		ph7_array_add_strkey_elem(pArray,azField[n],pWorker);` |
|     27 | 1406 | `	}` |
|      5 | 1407 | `	return PH7_OK;` |
|      1 | 1408 | `}` |
|      4 | 1409 | `static void Sq3BlobStream_Close(void *pHandle)` |
|      1 | 1410 | `{` |
|      5 | 1411 | `	Sq3BlobClose((phl_sq3_blob *)pHandle);` |
|      5 | 1412 | `}` |
|      - | 1413 | `static const ph7_io_stream sSq3BlobStream = {` |
|      - | 1414 | `	"SQLite3",                  /* what stream_get_meta_data() reports */` |
|      - | 1415 | `	PH7_IO_STREAM_VERSION,` |
|      - | 1416 | `	0,                          /* xOpen: openBlob() builds the handle itself */` |
|      - | 1417 | `	0,                          /* xOpenDir */` |
|      - | 1418 | `	Sq3BlobStream_Close,` |
|      - | 1419 | `	0,                          /* xCloseDir */` |
|      - | 1420 | `	Sq3BlobStream_Read,` |
|      - | 1421 | `	0,                          /* xReadDir */` |
|      - | 1422 | `	Sq3BlobStream_Write,` |
|      - | 1423 | `	Sq3BlobStream_Seek,` |
|      - | 1424 | `	0,                          /* xLock */` |
|      - | 1425 | `	0,                          /* xRewindDir */` |
|      - | 1426 | `	Sq3BlobStream_Tell,` |
|      - | 1427 | `	0,                          /* xTrunc: a blob is the length it was created with */` |
|      - | 1428 | `	0,                          /* xSync */` |
|      - | 1429 | `	Sq3BlobStream_Stat` |
|      - | 1430 | `};` |
|      - | 1431 | `/*` |
|      - | 1432 | ` * SQLite3::openBlob(string $table, string $column, int $rowid,` |
|      - | 1433 | ` *   string $database = 'main', int $flags = SQLITE3_OPEN_READONLY)` |
|      - | 1434 | ` *` |
|      - | 1435 | ` * The only flag read is READWRITE; everything else opens a read-only handle,` |
|      - | 1436 | ` * so SQLITE3_OPEN_CREATE is accepted and means nothing here. Every failure is` |
|      - | 1437 | `` * sqlite's own sentence behind php's `Unable to open blob: `, which is how a`` |
|      - | 1438 | ` * missing row, a missing column, a missing table and a NULL value are told` |
|      - | 1439 | ` * apart.` |
|      - | 1440 | ` */` |
|     50 | 1441 | `static int vm_builtin_SQLite3_openBlob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1442 | `{` |
|      - | 1443 | `	sxi32 rc;` |
|     51 | 1444 | `	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);` |
|     51 | 1445 | `	const char *zTable,*zColumn,*zDb = "main";` |
|     51 | 1446 | `	int nTable = 0,nColumn = 0,nDb = (int)sizeof("main")-1;` |
|      - | 1447 | `	ph7_int64 iRow;` |
|      - | 1448 | `	int iFlags,bWrite;` |
|      - | 1449 | `	SyBlob sTable,sColumn,sDb;` |
|     51 | 1450 | `	sqlite3_blob *pRaw = 0;` |
|      - | 1451 | `	phl_sq3_blob *pBl;` |
|      - | 1452 | `	io_private *pDev;` |
|     51 | 1453 | `	if( pConn == 0 ){` |
|    ! 0 | 1454 | `		return rc;` |
|      - | 1455 | `	}` |
|     51 | 1456 | `	zTable = ph7_value_to_string(apArg[0],&nTable);` |
|     51 | 1457 | `	zColumn = ph7_value_to_string(apArg[1],&nColumn);` |
|     51 | 1458 | `	iRow = ph7_value_to_int64(apArg[2]);` |
|     51 | 1459 | `	if( nArg > 3 ){` |
|     25 | 1460 | `		zDb = ph7_value_to_string(apArg[3],&nDb);` |
|     12 | 1461 | `	}` |
|     51 | 1462 | `	iFlags = nArg > 4 ? (int)ph7_value_to_int64(apArg[4]) : SQLITE_OPEN_READONLY;` |
|     51 | 1463 | `	bWrite = (iFlags & SQLITE_OPEN_READWRITE) != 0;` |
|     51 | 1464 | `	SyBlobInit(&sTable,&pCtx->pVm->sAllocator);` |
|     51 | 1465 | `	SyBlobAppend(&sTable,zTable,(sxu32)nTable);` |
|     51 | 1466 | `	SyBlobNullAppend(&sTable);` |
|     51 | 1467 | `	SyBlobInit(&sColumn,&pCtx->pVm->sAllocator);` |
|     51 | 1468 | `	SyBlobAppend(&sColumn,zColumn,(sxu32)nColumn);` |
|     51 | 1469 | `	SyBlobNullAppend(&sColumn);` |
|     51 | 1470 | `	SyBlobInit(&sDb,&pCtx->pVm->sAllocator);` |
|     51 | 1471 | `	SyBlobAppend(&sDb,zDb,(sxu32)nDb);` |
|     51 | 1472 | `	SyBlobNullAppend(&sDb);` |
|    101 | 1473 | `	rc = sqlite3_blob_open(pConn->pDb,(const char *)SyBlobData(&sDb),` |
|     50 | 1474 | `		(const char *)SyBlobData(&sTable),(const char *)SyBlobData(&sColumn),` |
|     25 | 1475 | `		(sqlite3_int64)iRow,bWrite,&pRaw);` |
|     51 | 1476 | `	SyBlobRelease(&sTable);` |
|     51 | 1477 | `	SyBlobRelease(&sColumn);` |
|     51 | 1478 | `	SyBlobRelease(&sDb);` |
|     51 | 1479 | `	if( rc != SQLITE_OK \|\| pRaw == 0 ){` |
|      - | 1480 | `		SyBlob sMsg;` |
|      - | 1481 | `		sxi32 rcErr;` |
|     11 | 1482 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|     11 | 1483 | `		SyBlobFormat(&sMsg,"Unable to open blob: %s",sqlite3_errmsg(pConn->pDb));` |
|     11 | 1484 | `		SyBlobNullAppend(&sMsg);` |
|     11 | 1485 | `		ph7_result_bool(pCtx,0);` |
|     16 | 1486 | `		rcErr = Sq3Error(pCtx,pConn,"SQLite3::openBlob",sqlite3_errcode(pConn->pDb),` |
|     10 | 1487 | `			(const char *)SyBlobData(&sMsg));` |
|     11 | 1488 | `		SyBlobRelease(&sMsg);` |
|     11 | 1489 | `		return rcErr;` |
|      - | 1490 | `	}` |
|     41 | 1491 | `	pBl = (phl_sq3_blob *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_sq3_blob));` |
|     41 | 1492 | `	if( pBl == 0 ){` |
|    ! 0 | 1493 | `		sqlite3_blob_close(pRaw);` |
|    ! 0 | 1494 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1495 | `	}` |
|     41 | 1496 | `	SyZero(pBl,sizeof(phl_sq3_blob));` |
|     41 | 1497 | `	pBl->pBlob = pRaw;` |
|     41 | 1498 | `	pBl->pVm = pCtx->pVm;` |
|     41 | 1499 | `	pBl->pConn = pConn;` |
|     41 | 1500 | `	pBl->nSize = (ph7_int64)sqlite3_blob_bytes(pRaw);` |
|     41 | 1501 | `	pBl->bWrite = bWrite;` |
|     41 | 1502 | `	pBl->iFlags = iFlags;` |
|     41 | 1503 | `	pBl->pNext = pConn->pBlobs;` |
|     41 | 1504 | `	pConn->pBlobs = pBl;` |
|     41 | 1505 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     41 | 1506 | `	if( pDev == 0 ){` |
|    ! 0 | 1507 | `		Sq3BlobClose(pBl);` |
|    ! 0 | 1508 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1509 | `	}` |
|     41 | 1510 | `	InitIOPrivate(pCtx->pVm,&sSq3BlobStream,pDev);` |
|     41 | 1511 | `	pDev->pHandle = pBl;` |
|     41 | 1512 | `	pBl->pDev = pDev;` |
|      - | 1513 | `	/* php's meta reports no uri for this stream and the mode it opened with. */` |
|     41 | 1514 | `	SetIOPrivateOpenedAs(pDev,"",0,bWrite ? "r+b" : "rb",bWrite ? 3 : 2);` |
|     41 | 1515 | `	ph7_result_resource(pCtx,pDev);` |
|     41 | 1516 | `	return PH7_OK;` |
|     26 | 1517 | `}` |
|      - | 1518 |  |
|      - | 1519 | `/* ------------------------------------------------------------------------` |
|      - | 1520 | ` * Userland callbacks, called from inside sqlite's own loop` |
|      - | 1521 | ` * ------------------------------------------------------------------------ */` |
|      - | 1522 | `/*` |
|      - | 1523 | ` * One callback a script registered: a scalar function, an aggregate's step and` |
|      - | 1524 | ` * finalize pair, a collation, or the authorizer. The connection owns the` |
|      - | 1525 | ` * callable, because sqlite will call it long after the registering verb has` |
|      - | 1526 | ` * returned, and lets go of all of them only when it closes.` |
|      - | 1527 | ` */` |
|      - | 1528 | `typedef struct phl_sq3_udf phl_sq3_udf;` |
|      - | 1529 | `struct phl_sq3_udf {` |
|      - | 1530 | `	phl_sq3 *pConn;` |
|      - | 1531 | `	ph7_value *pCallback;` |
|      - | 1532 | `	ph7_value *pFinalize;         /* an aggregate's second half, or 0 */` |
|      - | 1533 | `	phl_sq3_udf *pNext;` |
|      - | 1534 | `};` |
|      - | 1535 | `/*` |
|      - | 1536 | ` * One aggregate in progress. sqlite hands the same scratch buffer to every step` |
|      - | 1537 | ` * of one group and then to the finalizer, which is where the running context` |
|      - | 1538 | ` * and the row count live -- both visible to the callbacks as their first two` |
|      - | 1539 | ` * arguments.` |
|      - | 1540 | ` */` |
|      - | 1541 | `typedef struct phl_sq3_agg phl_sq3_agg;` |
|      - | 1542 | `struct phl_sq3_agg {` |
|      - | 1543 | `	ph7_value *pCtx;              /* whatever the last step returned */` |
|      - | 1544 | `	int nRow;                     /* steps taken so far */` |
|      - | 1545 | `};` |
|      - | 1546 | `/* Register one callable on the connection, kept for the connection's lifetime. */` |
|     56 | 1547 | `static phl_sq3_udf * Sq3NewUdf(phl_sq3 *pConn,ph7_value *pCallback,ph7_value *pFinalize)` |
|      1 | 1548 | `{` |
|     57 | 1549 | `	phl_sq3_udf *pUdf = (phl_sq3_udf *)SyMemBackendAlloc(&pConn->pVm->sAllocator,` |
|      - | 1550 | `		sizeof(phl_sq3_udf));` |
|     57 | 1551 | `	if( pUdf == 0 ){` |
|    ! 0 | 1552 | `		return 0;` |
|      - | 1553 | `	}` |
|     57 | 1554 | `	SyZero(pUdf,sizeof(phl_sq3_udf));` |
|     57 | 1555 | `	pUdf->pConn = pConn;` |
|     57 | 1556 | `	pUdf->pCallback = ph7_new_scalar(pConn->pVm);` |
|     57 | 1557 | `	if( pUdf->pCallback == 0 ){` |
|    ! 0 | 1558 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pUdf);` |
|    ! 0 | 1559 | `		return 0;` |
|      - | 1560 | `	}` |
|     57 | 1561 | `	PH7_MemObjStore(pCallback,pUdf->pCallback);` |
|     57 | 1562 | `	if( pFinalize ){` |
|      7 | 1563 | `		pUdf->pFinalize = ph7_new_scalar(pConn->pVm);` |
|      7 | 1564 | `		if( pUdf->pFinalize ){` |
|      7 | 1565 | `			PH7_MemObjStore(pFinalize,pUdf->pFinalize);` |
|      3 | 1566 | `		}` |
|      3 | 1567 | `	}` |
|     57 | 1568 | `	pUdf->pNext = pConn->pUdfs;` |
|     57 | 1569 | `	pConn->pUdfs = pUdf;` |
|     57 | 1570 | `	return pUdf;` |
|     29 | 1571 | `}` |
|    410 | 1572 | `static void Sq3UdfSweep(phl_sq3 *pConn)` |
|      4 | 1573 | `{` |
|    414 | 1574 | `	phl_sq3_udf *pUdf = pConn->pUdfs;` |
|    470 | 1575 | `	while( pUdf ){` |
|     57 | 1576 | `		phl_sq3_udf *pNext = pUdf->pNext;` |
|     57 | 1577 | `		if( pUdf->pCallback ){` |
|     57 | 1578 | `			ph7_release_value(pConn->pVm,pUdf->pCallback);` |
|     28 | 1579 | `		}` |
|     57 | 1580 | `		if( pUdf->pFinalize ){` |
|      7 | 1581 | `			ph7_release_value(pConn->pVm,pUdf->pFinalize);` |
|      3 | 1582 | `		}` |
|     57 | 1583 | `		SyMemBackendFree(&pConn->pVm->sAllocator,pUdf);` |
|     57 | 1584 | `		pUdf = pNext;` |
|      1 | 1585 | `	}` |
|    414 | 1586 | `	pConn->pUdfs = 0;` |
|    414 | 1587 | `}` |
|      - | 1588 | `/*` |
|      - | 1589 | ` * The rule every trampoline here shares: sqlite is in the middle of a step when` |
|      - | 1590 | ` * the engine re-enters PHP, and a throw out of that PHP cannot travel back` |
|      - | 1591 | ` * through sqlite's C frames. So the status is PARKED on the connection, sqlite` |
|      - | 1592 | ` * is told to stop, and the verb that started the step raises exactly the parked` |
|      - | 1593 | ` * status once the library has unwound. A second callback while one is parked` |
|      - | 1594 | ` * does not re-enter PHP at all.` |
|      - | 1595 | ` */` |
|    124 | 1596 | `static int Sq3UdfParked(phl_sq3_udf *pUdf)` |
|      1 | 1597 | `{` |
|    125 | 1598 | `	return pUdf->pConn->iCallbackExc != 0;` |
|      1 | 1599 | `}` |
|     10 | 1600 | `static void Sq3UdfPark(phl_sq3_udf *pUdf,sxi32 rc,sqlite3_context *pCtx)` |
|      1 | 1601 | `{` |
|     11 | 1602 | `	pUdf->pConn->iCallbackExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;` |
|     11 | 1603 | `	if( pCtx ){` |
|      7 | 1604 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|      3 | 1605 | `	}` |
|     11 | 1606 | `}` |
|      - | 1607 | `/*` |
|      - | 1608 | ` * A diagnostic raised from INSIDE the library, in the name of the verb that is` |
|      - | 1609 | ` * running -- which is the only thing that knows it. php prints the method, so a` |
|      - | 1610 | `` * collation's complaint is `SQLite3::query(): ...` under one caller and`` |
|      - | 1611 | `` * `SQLite3Result::fetchAll(): ...` under another.`` |
|      - | 1612 | ` */` |
|     10 | 1613 | `static void Sq3UdfWarn(phl_sq3 *pConn,const char *zMsg)` |
|      1 | 1614 | `{` |
|     11 | 1615 | `	if( pConn->pVerbCtx == 0 \|\| pConn->zVerbFn == 0 ){` |
|    ! 0 | 1616 | `		return;` |
|      - | 1617 | `	}` |
|     11 | 1618 | `	Sq3Error(pConn->pVerbCtx,pConn,pConn->zVerbFn,0,zMsg);` |
|      6 | 1619 | `}` |
|      - | 1620 | `/* One sqlite value as a php value, in sqlite's own types. */` |
|     48 | 1621 | `static void Sq3UdfArgValue(ph7_vm *pVm,sqlite3_value *pIn,ph7_value *pOut)` |
|      1 | 1622 | `{` |
|     49 | 1623 | `	PH7_MemObjInit(pVm,pOut);` |
|     49 | 1624 | `	switch( sqlite3_value_type(pIn) ){` |
|     20 | 1625 | `		case SQLITE_INTEGER:` |
|     41 | 1626 | `			ph7_value_int64(pOut,(ph7_int64)sqlite3_value_int64(pIn));` |
|     41 | 1627 | `			break;` |
|      - | 1628 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      1 | 1629 | `		case SQLITE_FLOAT:` |
|      3 | 1630 | `			ph7_value_double(pOut,(ph7_real)sqlite3_value_double(pIn));` |
|      3 | 1631 | `			break;` |
|      - | 1632 | `#endif` |
|      1 | 1633 | `		case SQLITE_NULL:` |
|      3 | 1634 | `			ph7_value_null(pOut);` |
|      3 | 1635 | `			break;` |
|      1 | 1636 | `		case SQLITE_BLOB:` |
|      4 | 1637 | `			ph7_value_string(pOut,(const char *)sqlite3_value_blob(pIn),` |
|      1 | 1638 | `				sqlite3_value_bytes(pIn));` |
|      3 | 1639 | `			break;` |
|      1 | 1640 | `		default:` |
|      4 | 1641 | `			ph7_value_string(pOut,(const char *)sqlite3_value_text(pIn),` |
|      1 | 1642 | `				sqlite3_value_bytes(pIn));` |
|      2 | 1643 | `			break;` |
|      - | 1644 | `	}` |
|     49 | 1645 | `}` |
|      - | 1646 | `/*` |
|      - | 1647 | ` * What a callback RETURNED, as a sqlite result. null, int and float pass` |
|      - | 1648 | ` * straight through and everything else takes a string CAST -- so a bool comes` |
|      - | 1649 | ` * back as "1" and an array as "Array", with php's own conversion warning behind` |
|      - | 1650 | ` * it.` |
|      - | 1651 | ` *` |
|      - | 1652 | ` * The string is handed to sqlite as a C STRING, so it stops at the first NUL:` |
|      - | 1653 | ` * a function returning "a\0b" produces a one-byte value and one returning` |
|      - | 1654 | ` * "\0x" produces an empty one. php has the same cut.` |
|      - | 1655 | ` */` |
|     64 | 1656 | `static void Sq3UdfResult(sqlite3_context *pCtx,ph7_value *pVal)` |
|      1 | 1657 | `{` |
|     65 | 1658 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      5 | 1659 | `		sqlite3_result_null(pCtx);` |
|      - | 1660 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     63 | 1661 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|      5 | 1662 | `		sqlite3_result_double(pCtx,(double)ph7_value_to_double(pVal));` |
|      - | 1663 | `#endif` |
|     59 | 1664 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|     23 | 1665 | `		sqlite3_result_int64(pCtx,(sqlite3_int64)ph7_value_to_int64(pVal));` |
|     12 | 1666 | `	}else{` |
|     35 | 1667 | `		int nByte = 0,n;` |
|      - | 1668 | `		const char *zStr;` |
|     35 | 1669 | `		PH7_MemObjToStringUV(pVal);` |
|     35 | 1670 | `		zStr = ph7_value_to_string(pVal,&nByte);` |
|    233 | 1671 | `		for( n = 0 ; n < nByte && zStr[n] ; ++n ){}` |
|     35 | 1672 | `		sqlite3_result_text(pCtx,zStr ? zStr : "",n,SQLITE_TRANSIENT);` |
|      - | 1673 | `	}` |
|     65 | 1674 | `}` |
|      - | 1675 | `/* A scalar function's body: build the arguments, call, convert the answer. */` |
|     62 | 1676 | `static void Sq3UdfScalar(sqlite3_context *pCtx,int nArg,sqlite3_value **apVal)` |
|      1 | 1677 | `{` |
|     63 | 1678 | `	phl_sq3_udf *pUdf = (phl_sq3_udf *)sqlite3_user_data(pCtx);` |
|     63 | 1679 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|      - | 1680 | `	ph7_value *apArg[16];` |
|      - | 1681 | `	ph7_value sArgs[16];` |
|      - | 1682 | `	ph7_value sRes;` |
|     63 | 1683 | `	int n,nCall = nArg;` |
|      - | 1684 | `	sxi32 rc;` |
|     63 | 1685 | `	if( Sq3UdfParked(pUdf) ){` |
|    ! 0 | 1686 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|    ! 0 | 1687 | `		return;` |
|      - | 1688 | `	}` |
|     63 | 1689 | `	if( nCall > (int)SX_ARRAYSIZE(sArgs) ){` |
|    ! 0 | 1690 | `		nCall = (int)SX_ARRAYSIZE(sArgs);` |
|    ! 0 | 1691 | `	}` |
|     97 | 1692 | `	for( n = 0 ; n < nCall ; ++n ){` |
|     35 | 1693 | `		Sq3UdfArgValue(pVm,apVal[n],&sArgs[n]);` |
|     35 | 1694 | `		apArg[n] = &sArgs[n];` |
|     18 | 1695 | `	}` |
|     63 | 1696 | `	PH7_MemObjInit(pVm,&sRes);` |
|     63 | 1697 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,nCall,nCall ? apArg : 0,&sRes);` |
|     63 | 1698 | `	if( rc != SXRET_OK ){` |
|      5 | 1699 | `		Sq3UdfPark(pUdf,rc,pCtx);` |
|      3 | 1700 | `	}else{` |
|     59 | 1701 | `		Sq3UdfResult(pCtx,&sRes);` |
|      - | 1702 | `	}` |
|     63 | 1703 | `	PH7_MemObjRelease(&sRes);` |
|     97 | 1704 | `	for( n = 0 ; n < nCall ; ++n ){` |
|     35 | 1705 | `		PH7_MemObjRelease(&sArgs[n]);` |
|     18 | 1706 | `	}` |
|     32 | 1707 | `}` |
|      - | 1708 | `/*` |
|      - | 1709 | ` * A collation: two strings in, an ordering out. Only an INT is an ordering --` |
|      - | 1710 | ` * a float, a numeric string, a bool, null and an array are each the same` |
|      - | 1711 | ` * complaint and a verdict of EQUAL, which leaves sqlite sorting by nothing.` |
|      - | 1712 | ` */` |
|     24 | 1713 | `static int Sq3UdfCollate(void *pUser,int nLeft,const void *pLeft,int nRight,const void *pRight)` |
|      1 | 1714 | `{` |
|     25 | 1715 | `	phl_sq3_udf *pUdf = (phl_sq3_udf *)pUser;` |
|      - | 1716 | `	ph7_vm *pVm;` |
|      - | 1717 | `	ph7_value sL,sR,sRes;` |
|      - | 1718 | `	ph7_value *apArg[2];` |
|      - | 1719 | `	sxi32 rc;` |
|     25 | 1720 | `	int iCmp = 0;` |
|     25 | 1721 | `	if( pUdf == 0 \|\| Sq3UdfParked(pUdf) ){` |
|      3 | 1722 | `		return 0;` |
|      - | 1723 | `	}` |
|     23 | 1724 | `	pVm = pUdf->pConn->pVm;` |
|     23 | 1725 | `	PH7_MemObjInit(pVm,&sL);` |
|     23 | 1726 | `	PH7_MemObjInit(pVm,&sR);` |
|     23 | 1727 | `	ph7_value_string(&sL,(const char *)pLeft,nLeft);` |
|     23 | 1728 | `	ph7_value_string(&sR,(const char *)pRight,nRight);` |
|     23 | 1729 | `	apArg[0] = &sL;` |
|     23 | 1730 | `	apArg[1] = &sR;` |
|     23 | 1731 | `	PH7_MemObjInit(pVm,&sRes);` |
|     23 | 1732 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,2,apArg,&sRes);` |
|     23 | 1733 | `	if( rc != SXRET_OK ){` |
|      - | 1734 | `		/* no sqlite3_context here to fail through: park it and order the pair` |
|      - | 1735 | `		 * as equal, which leaves the walk to end on the parked status */` |
|      3 | 1736 | `		Sq3UdfPark(pUdf,rc,0);` |
|     22 | 1737 | `	}else if( (sRes.iFlags & MEMOBJ_INT) == 0 \|\| (sRes.iFlags & MEMOBJ_REAL) != 0 ){` |
|      9 | 1738 | `		Sq3UdfWarn(pUdf->pConn,` |
|      - | 1739 | `			"An error occurred while invoking the compare callback (invalid return type)."` |
|      - | 1740 | `			"  Collation behaviour is undefined.");` |
|      5 | 1741 | `	}else{` |
|     13 | 1742 | `		ph7_int64 iVal = ph7_value_to_int64(&sRes);` |
|     13 | 1743 | `		iCmp = iVal < 0 ? -1 : (iVal > 0 ? 1 : 0);` |
|      - | 1744 | `	}` |
|     23 | 1745 | `	PH7_MemObjRelease(&sRes);` |
|     23 | 1746 | `	PH7_MemObjRelease(&sL);` |
|     23 | 1747 | `	PH7_MemObjRelease(&sR);` |
|     23 | 1748 | `	return iCmp;` |
|     13 | 1749 | `}` |
|      - | 1750 | `/*` |
|      - | 1751 | ` * An aggregate's two halves. Both are given the running CONTEXT and a ROW` |
|      - | 1752 | ` * COUNT as their first two arguments, and whatever step returns becomes the` |
|      - | 1753 | ` * context of the next one. The count the STEP sees is 1-based; the one the` |
|      - | 1754 | ` * finalizer sees is always 0, which is php's own answer and not a count of` |
|      - | 1755 | ` * anything.` |
|      - | 1756 | ` */` |
|     14 | 1757 | `static void Sq3UdfStep(sqlite3_context *pCtx,int nArg,sqlite3_value **apVal)` |
|      1 | 1758 | `{` |
|     15 | 1759 | `	phl_sq3_udf *pUdf = (phl_sq3_udf *)sqlite3_user_data(pCtx);` |
|     15 | 1760 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|      - | 1761 | `	phl_sq3_agg *pAgg;` |
|      - | 1762 | `	ph7_value sArgs[16];` |
|      - | 1763 | `	ph7_value *apArg[18];` |
|      - | 1764 | `	ph7_value sCount,sRes;` |
|     15 | 1765 | `	int n,nCall = nArg;` |
|      - | 1766 | `	sxi32 rc;` |
|     15 | 1767 | `	if( Sq3UdfParked(pUdf) ){` |
|    ! 0 | 1768 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|    ! 0 | 1769 | `		return;` |
|      - | 1770 | `	}` |
|     15 | 1771 | `	pAgg = (phl_sq3_agg *)sqlite3_aggregate_context(pCtx,(int)sizeof(phl_sq3_agg));` |
|     15 | 1772 | `	if( pAgg == 0 ){` |
|    ! 0 | 1773 | `		return;` |
|      - | 1774 | `	}` |
|     15 | 1775 | `	if( pAgg->pCtx == 0 ){` |
|      7 | 1776 | `		pAgg->pCtx = ph7_new_scalar(pVm);` |
|      7 | 1777 | `		pAgg->nRow = 0;` |
|      3 | 1778 | `	}` |
|     15 | 1779 | `	pAgg->nRow++;` |
|     15 | 1780 | `	if( nCall > (int)SX_ARRAYSIZE(sArgs) ){` |
|    ! 0 | 1781 | `		nCall = (int)SX_ARRAYSIZE(sArgs);` |
|    ! 0 | 1782 | `	}` |
|     15 | 1783 | `	PH7_MemObjInit(pVm,&sCount);` |
|     15 | 1784 | `	ph7_value_int(&sCount,pAgg->nRow);` |
|     15 | 1785 | `	apArg[0] = pAgg->pCtx;` |
|     15 | 1786 | `	apArg[1] = &sCount;` |
|     29 | 1787 | `	for( n = 0 ; n < nCall ; ++n ){` |
|     15 | 1788 | `		Sq3UdfArgValue(pVm,apVal[n],&sArgs[n]);` |
|     15 | 1789 | `		apArg[n + 2] = &sArgs[n];` |
|      8 | 1790 | `	}` |
|     15 | 1791 | `	PH7_MemObjInit(pVm,&sRes);` |
|     15 | 1792 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,nCall + 2,apArg,&sRes);` |
|     15 | 1793 | `	if( rc != SXRET_OK ){` |
|      3 | 1794 | `		Sq3UdfPark(pUdf,rc,pCtx);` |
|     14 | 1795 | `	}else if( pAgg->pCtx ){` |
|     13 | 1796 | `		PH7_MemObjStore(&sRes,pAgg->pCtx);` |
|      6 | 1797 | `	}` |
|     15 | 1798 | `	PH7_MemObjRelease(&sRes);` |
|     15 | 1799 | `	PH7_MemObjRelease(&sCount);` |
|     29 | 1800 | `	for( n = 0 ; n < nCall ; ++n ){` |
|     15 | 1801 | `		PH7_MemObjRelease(&sArgs[n]);` |
|      8 | 1802 | `	}` |
|      8 | 1803 | `}` |
|      8 | 1804 | `static void Sq3UdfFinal(sqlite3_context *pCtx)` |
|      1 | 1805 | `{` |
|      9 | 1806 | `	phl_sq3_udf *pUdf = (phl_sq3_udf *)sqlite3_user_data(pCtx);` |
|      9 | 1807 | `	ph7_vm *pVm = pUdf->pConn->pVm;` |
|      9 | 1808 | `	phl_sq3_agg *pAgg = (phl_sq3_agg *)sqlite3_aggregate_context(pCtx,0);` |
|      - | 1809 | `	ph7_value sCtx,sCount,sRes;` |
|      - | 1810 | `	ph7_value *apArg[2];` |
|      - | 1811 | `	sxi32 rc;` |
|      9 | 1812 | `	if( Sq3UdfParked(pUdf) ){` |
|      3 | 1813 | `		sqlite3_result_error(pCtx,"PHL: callback raised",-1);` |
|      3 | 1814 | `		return;` |
|      - | 1815 | `	}` |
|      7 | 1816 | `	PH7_MemObjInit(pVm,&sCtx);` |
|      7 | 1817 | `	PH7_MemObjInit(pVm,&sCount);` |
|      7 | 1818 | `	if( pAgg && pAgg->pCtx ){` |
|      5 | 1819 | `		PH7_MemObjStore(pAgg->pCtx,&sCtx);` |
|      2 | 1820 | `	}` |
|      7 | 1821 | `	ph7_value_int(&sCount,0);` |
|      7 | 1822 | `	apArg[0] = &sCtx;` |
|      7 | 1823 | `	apArg[1] = &sCount;` |
|      7 | 1824 | `	PH7_MemObjInit(pVm,&sRes);` |
|      7 | 1825 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pFinalize ? pUdf->pFinalize : pUdf->pCallback,` |
|      3 | 1826 | `		2,apArg,&sRes);` |
|      7 | 1827 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1828 | `		Sq3UdfPark(pUdf,rc,pCtx);` |
|    ! 0 | 1829 | `	}else{` |
|      7 | 1830 | `		Sq3UdfResult(pCtx,&sRes);` |
|      - | 1831 | `	}` |
|      7 | 1832 | `	if( pAgg && pAgg->pCtx ){` |
|      5 | 1833 | `		ph7_release_value(pVm,pAgg->pCtx);` |
|      5 | 1834 | `		pAgg->pCtx = 0;` |
|      2 | 1835 | `	}` |
|      7 | 1836 | `	PH7_MemObjRelease(&sRes);` |
|      7 | 1837 | `	PH7_MemObjRelease(&sCtx);` |
|      7 | 1838 | `	PH7_MemObjRelease(&sCount);` |
|      5 | 1839 | `}` |
|      - | 1840 | `/*` |
|      - | 1841 | ` * The authorizer: sqlite asks before it COMPILES each action, so a refusal here` |
|      - | 1842 | ` * stops a PREPARE rather than a step -- which is why a denied SELECT fails at` |
|      - | 1843 | ` * query() and never reaches a fetch. Its verdict is one of three ints and` |
|      - | 1844 | ` * nothing else will do: any other type is the complaint below and a denial,` |
|      - | 1845 | ` * as is a callback that threw.` |
|      - | 1846 | ` */` |
|     16 | 1847 | `static int Sq3UdfAuthorize(void *pUser,int iAction,const char *z1,const char *z2,` |
|      - | 1848 | `	const char *z3,const char *z4)` |
|      1 | 1849 | `{` |
|     17 | 1850 | `	phl_sq3_udf *pUdf = (phl_sq3_udf *)pUser;` |
|      - | 1851 | `	ph7_vm *pVm;` |
|      - | 1852 | `	ph7_value sArgs[5],sRes;` |
|      - | 1853 | `	ph7_value *apArg[5];` |
|      - | 1854 | `	const char *azIn[4];` |
|     17 | 1855 | `	int n,iVerdict = SQLITE_OK;` |
|      - | 1856 | `	sxi32 rc;` |
|     17 | 1857 | `	if( pUdf == 0 \|\| Sq3UdfParked(pUdf) ){` |
|    ! 0 | 1858 | `		return SQLITE_DENY;` |
|      - | 1859 | `	}` |
|     17 | 1860 | `	pVm = pUdf->pConn->pVm;` |
|     17 | 1861 | `	azIn[0] = z1; azIn[1] = z2; azIn[2] = z3; azIn[3] = z4;` |
|     17 | 1862 | `	PH7_MemObjInit(pVm,&sArgs[0]);` |
|     17 | 1863 | `	ph7_value_int(&sArgs[0],iAction);` |
|     17 | 1864 | `	apArg[0] = &sArgs[0];` |
|     81 | 1865 | `	for( n = 0 ; n < 4 ; ++n ){` |
|     65 | 1866 | `		PH7_MemObjInit(pVm,&sArgs[n + 1]);` |
|     65 | 1867 | `		if( azIn[n] ){` |
|     19 | 1868 | `			ph7_value_string(&sArgs[n + 1],azIn[n],(int)SyStrlen(azIn[n]));` |
|     10 | 1869 | `		}else{` |
|     47 | 1870 | `			ph7_value_null(&sArgs[n + 1]);` |
|      - | 1871 | `		}` |
|     65 | 1872 | `		apArg[n + 1] = &sArgs[n + 1];` |
|     33 | 1873 | `	}` |
|     17 | 1874 | `	PH7_MemObjInit(pVm,&sRes);` |
|     17 | 1875 | `	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,5,apArg,&sRes);` |
|     17 | 1876 | `	if( rc != SXRET_OK ){` |
|      3 | 1877 | `		Sq3UdfPark(pUdf,rc,0);` |
|      3 | 1878 | `		iVerdict = SQLITE_DENY;` |
|     16 | 1879 | `	}else if( (sRes.iFlags & MEMOBJ_INT) == 0 \|\| (sRes.iFlags & MEMOBJ_REAL) != 0 ){` |
|      3 | 1880 | `		Sq3UdfWarn(pUdf->pConn,` |
|      - | 1881 | `			"The authorizer callback returned an invalid type: expected int");` |
|      3 | 1882 | `		iVerdict = SQLITE_DENY;` |
|      2 | 1883 | `	}else{` |
|     13 | 1884 | `		ph7_int64 iVal = ph7_value_to_int64(&sRes);` |
|     13 | 1885 | `		iVerdict = (iVal == SQLITE_IGNORE) ? SQLITE_IGNORE` |
|     10 | 1886 | `			: ((iVal == SQLITE_OK) ? SQLITE_OK : SQLITE_DENY);` |
|      - | 1887 | `	}` |
|     17 | 1888 | `	PH7_MemObjRelease(&sRes);` |
|     97 | 1889 | `	for( n = 0 ; n < 5 ; ++n ){` |
|     81 | 1890 | `		PH7_MemObjRelease(&sArgs[n]);` |
|     41 | 1891 | `	}` |
|     17 | 1892 | `	return iVerdict;` |
|      9 | 1893 | `}` |
|      - | 1894 | `/*` |
|      - | 1895 | ` * The name every registering verb screens the same way: php refuses an EMPTY` |
|      - | 1896 | ` * name outright, and takes it as a C string, so one carrying a NUL registers` |
|      - | 1897 | ` * the part in front of it. The ARITY is not screened: php hands it straight to` |
|      - | 1898 | ` * sqlite, whose ceiling is the linked library's own (127 for years, higher in` |
|      - | 1899 | ` * newer builds), so the answer is the library's.` |
|      - | 1900 | ` */` |
|     48 | 1901 | `static int Sq3UdfNameOk(ph7_context *pCtx,ph7_value *pName,SyBlob *pOut)` |
|      1 | 1902 | `{` |
|      - | 1903 | `	const char *zName;` |
|     49 | 1904 | `	int nName = 0;` |
|     49 | 1905 | `	zName = ph7_value_to_string(pName,&nName);` |
|     49 | 1906 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|     49 | 1907 | `	if( nName < 1 ){` |
|      3 | 1908 | `		return 0;` |
|      - | 1909 | `	}` |
|     47 | 1910 | `	SyBlobAppend(pOut,zName,(sxu32)nName);` |
|     47 | 1911 | `	SyBlobNullAppend(pOut);` |
|     47 | 1912 | `	return 1;` |
|     25 | 1913 | `}` |
|      - | 1914 | `/*` |
|      - | 1915 | ` * SQLite3::createFunction(string $name, callable $callback, int $argCount = -1,` |
|      - | 1916 | ` *   int $flags = 0): bool` |
|      - | 1917 | ` */` |
|     42 | 1918 | `static int vm_builtin_SQLite3_createFunction(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1919 | `{` |
|      - | 1920 | `	sxi32 rc;` |
|      - | 1921 | `	phl_sq3 *pConn;` |
|      - | 1922 | `	/* php screens the callable at ZPP time, so it is refused ahead of every` |
|      - | 1923 | `	 * question about the connection -- a closed one included. */` |
|      - | 1924 | `	{` |
|     43 | 1925 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|     43 | 1926 | `		if( rcCb != PH7_OK ){` |
|      5 | 1927 | `			return rcCb;` |
|      - | 1928 | `		}` |
|      - | 1929 | `	}` |
|     39 | 1930 | `	pConn = Sq3LiveDb(pCtx,&rc);` |
|     39 | 1931 | `	int nFuncArg = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : -1;` |
|     39 | 1932 | `	int iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3]) : 0;` |
|      - | 1933 | `	phl_sq3_udf *pUdf;` |
|      - | 1934 | `	SyBlob sName;` |
|     39 | 1935 | `	if( pConn == 0 ){` |
|      3 | 1936 | `		return rc;` |
|      - | 1937 | `	}` |
|     37 | 1938 | `	if( !Sq3UdfNameOk(pCtx,apArg[0],&sName) ){` |
|      3 | 1939 | `		SyBlobRelease(&sName);` |
|      3 | 1940 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1941 | `		return PH7_OK;` |
|      - | 1942 | `	}` |
|     35 | 1943 | `	pUdf = Sq3NewUdf(pConn,apArg[1],0);` |
|     35 | 1944 | `	if( pUdf == 0 ){` |
|    ! 0 | 1945 | `		SyBlobRelease(&sName);` |
|    ! 0 | 1946 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1947 | `	}` |
|     52 | 1948 | `	ph7_result_bool(pCtx,` |
|     51 | 1949 | `		sqlite3_create_function_v2(pConn->pDb,(const char *)SyBlobData(&sName),nFuncArg,` |
|     34 | 1950 | `			SQLITE_UTF8 \| (iFlags & SQLITE_DETERMINISTIC),pUdf,` |
|     17 | 1951 | `			Sq3UdfScalar,0,0,0) == SQLITE_OK);` |
|     35 | 1952 | `	SyBlobRelease(&sName);` |
|     35 | 1953 | `	return PH7_OK;` |
|     22 | 1954 | `}` |
|      - | 1955 | `/*` |
|      - | 1956 | ` * SQLite3::createAggregate(string $name, callable $stepCallback,` |
|      - | 1957 | ` *   callable $finalCallback, int $argCount = -1): bool` |
|      - | 1958 | ` */` |
|      6 | 1959 | `static int vm_builtin_SQLite3_createAggregate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1960 | `{` |
|      - | 1961 | `	sxi32 rc;` |
|      - | 1962 | `	phl_sq3 *pConn;` |
|      - | 1963 | `	/* php screens the callable at ZPP time, so it is refused ahead of every` |
|      - | 1964 | `	 * question about the connection -- a closed one included. */` |
|      - | 1965 | `	{` |
|      7 | 1966 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"stepCallback",0);` |
|      7 | 1967 | `		if( rcCb != PH7_OK ){` |
|    ! 0 | 1968 | `			return rcCb;` |
|      - | 1969 | `		}` |
|      - | 1970 | `	}` |
|      - | 1971 | `	{` |
|      7 | 1972 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[2],3,"finalCallback",0);` |
|      7 | 1973 | `		if( rcCb != PH7_OK ){` |
|    ! 0 | 1974 | `			return rcCb;` |
|      - | 1975 | `		}` |
|      - | 1976 | `	}` |
|      7 | 1977 | `	pConn = Sq3LiveDb(pCtx,&rc);` |
|      7 | 1978 | `	int nFuncArg = nArg > 3 ? (int)ph7_value_to_int64(apArg[3]) : -1;` |
|      - | 1979 | `	phl_sq3_udf *pUdf;` |
|      - | 1980 | `	SyBlob sName;` |
|      7 | 1981 | `	if( pConn == 0 ){` |
|    ! 0 | 1982 | `		return rc;` |
|      - | 1983 | `	}` |
|      7 | 1984 | `	if( !Sq3UdfNameOk(pCtx,apArg[0],&sName) ){` |
|    ! 0 | 1985 | `		SyBlobRelease(&sName);` |
|    ! 0 | 1986 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1987 | `		return PH7_OK;` |
|      - | 1988 | `	}` |
|      7 | 1989 | `	pUdf = Sq3NewUdf(pConn,apArg[1],apArg[2]);` |
|      7 | 1990 | `	if( pUdf == 0 ){` |
|    ! 0 | 1991 | `		SyBlobRelease(&sName);` |
|    ! 0 | 1992 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1993 | `	}` |
|     10 | 1994 | `	ph7_result_bool(pCtx,` |
|      9 | 1995 | `		sqlite3_create_function_v2(pConn->pDb,(const char *)SyBlobData(&sName),nFuncArg,` |
|      6 | 1996 | `			SQLITE_UTF8,pUdf,0,Sq3UdfStep,Sq3UdfFinal,0) == SQLITE_OK);` |
|      7 | 1997 | `	SyBlobRelease(&sName);` |
|      7 | 1998 | `	return PH7_OK;` |
|      4 | 1999 | `}` |
|      - | 2000 | `/* SQLite3::createCollation(string $name, callable $callback): bool */` |
|      6 | 2001 | `static int vm_builtin_SQLite3_createCollation(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2002 | `{` |
|      - | 2003 | `	sxi32 rc;` |
|      - | 2004 | `	phl_sq3 *pConn;` |
|      - | 2005 | `	/* php screens the callable at ZPP time, so it is refused ahead of every` |
|      - | 2006 | `	 * question about the connection -- a closed one included. */` |
|      - | 2007 | `	{` |
|      7 | 2008 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|      7 | 2009 | `		if( rcCb != PH7_OK ){` |
|    ! 0 | 2010 | `			return rcCb;` |
|      - | 2011 | `		}` |
|      - | 2012 | `	}` |
|      7 | 2013 | `	pConn = Sq3LiveDb(pCtx,&rc);` |
|      - | 2014 | `	phl_sq3_udf *pUdf;` |
|      - | 2015 | `	SyBlob sName;` |
|      3 | 2016 | `	SXUNUSED(nArg);` |
|      7 | 2017 | `	if( pConn == 0 ){` |
|    ! 0 | 2018 | `		return rc;` |
|      - | 2019 | `	}` |
|      7 | 2020 | `	if( !Sq3UdfNameOk(pCtx,apArg[0],&sName) ){` |
|    ! 0 | 2021 | `		SyBlobRelease(&sName);` |
|    ! 0 | 2022 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2023 | `		return PH7_OK;` |
|      - | 2024 | `	}` |
|      7 | 2025 | `	pUdf = Sq3NewUdf(pConn,apArg[1],0);` |
|      7 | 2026 | `	if( pUdf == 0 ){` |
|    ! 0 | 2027 | `		SyBlobRelease(&sName);` |
|    ! 0 | 2028 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2029 | `	}` |
|     10 | 2030 | `	ph7_result_bool(pCtx,` |
|      9 | 2031 | `		sqlite3_create_collation_v2(pConn->pDb,(const char *)SyBlobData(&sName),SQLITE_UTF8,` |
|      6 | 2032 | `			pUdf,Sq3UdfCollate,0) == SQLITE_OK);` |
|      7 | 2033 | `	SyBlobRelease(&sName);` |
|      7 | 2034 | `	return PH7_OK;` |
|      4 | 2035 | `}` |
|      - | 2036 | `/*` |
|      - | 2037 | ` * SQLite3::setAuthorizer(?callable $callback): bool` |
|      - | 2038 | ` *` |
|      - | 2039 | ` * One at a time: a second call replaces the first, and NULL takes it off.` |
|      - | 2040 | ` */` |
|     16 | 2041 | `static int vm_builtin_SQLite3_setAuthorizer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2042 | `{` |
|      - | 2043 | `	sxi32 rc;` |
|      - | 2044 | `	phl_sq3 *pConn;` |
|      - | 2045 | `	phl_sq3_udf *pUdf;` |
|      8 | 2046 | `	SXUNUSED(nArg);` |
|     17 | 2047 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|     13 | 2048 | `		rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);` |
|     13 | 2049 | `		if( rc != PH7_OK ){` |
|      3 | 2050 | `			return rc;` |
|      - | 2051 | `		}` |
|      5 | 2052 | `	}` |
|     15 | 2053 | `	pConn = Sq3LiveDb(pCtx,&rc);` |
|     15 | 2054 | `	if( pConn == 0 ){` |
|    ! 0 | 2055 | `		return rc;` |
|      - | 2056 | `	}` |
|     15 | 2057 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      5 | 2058 | `		sqlite3_set_authorizer(pConn->pDb,0,0);` |
|      5 | 2059 | `		ph7_result_bool(pCtx,1);` |
|      5 | 2060 | `		return PH7_OK;` |
|      - | 2061 | `	}` |
|     11 | 2062 | `	pUdf = Sq3NewUdf(pConn,apArg[0],0);` |
|     11 | 2063 | `	if( pUdf == 0 ){` |
|    ! 0 | 2064 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2065 | `	}` |
|     16 | 2066 | `	ph7_result_bool(pCtx,` |
|     10 | 2067 | `		sqlite3_set_authorizer(pConn->pDb,Sq3UdfAuthorize,pUdf) == SQLITE_OK);` |
|     11 | 2068 | `	return PH7_OK;` |
|      9 | 2069 | `}` |
|      - | 2070 |  |
|      - | 2071 | `/* ------------------------------------------------------------------------` |
|      - | 2072 | ` * SQLite3Stmt -- a statement a script holds itself` |
|      - | 2073 | ` * ------------------------------------------------------------------------ */` |
|      - | 2074 | `/*` |
|      - | 2075 | ` * One parameter a script bound before execute().` |
|      - | 2076 | ` *` |
|      - | 2077 | ` * The bindings are php's, not sqlite's: they are recorded here and applied at` |
|      - | 2078 | ` * every execute, which is what makes bindParam() read its variable LATE -- the` |
|      - | 2079 | ` * value the statement runs with is whatever the variable holds when execute()` |
|      - | 2080 | ` * is called, not when the binding was made.` |
|      - | 2081 | ` */` |
|      - | 2082 | `typedef struct phl_sq3_bind phl_sq3_bind;` |
|      - | 2083 | `struct phl_sq3_bind {` |
|      - | 2084 | `	int iPos;                     /* 1-based; a NAME is resolved at bind time */` |
|      - | 2085 | `	char *zName;                  /* the name the script spelled, or 0 for a positional bind:` |
|      - | 2086 | `	                               * php keys its table by NAME for one and by POSITION for` |
|      - | 2087 | ``	                               * the other, so `:a` and `1` naming the same parameter are`` |
|      - | 2088 | `	                               * two bindings that are both applied */` |
|      - | 2089 | `	int nName;` |
|      - | 2090 | `	int iType;                    /* SQLITE3_* -- the declared one, or the value's own */` |
|      - | 2091 | `	sxu32 nSlot;                  /* bindParam: the caller's memobj index (SXU32_HIGH = none) */` |
|      - | 2092 | `	ph7_value *pVal;              /* bindValue: this statement's own copy */` |
|      - | 2093 | `	phl_sq3_bind *pNext;` |
|      - | 2094 | `};` |
|      - | 2095 | `/* Drop every binding of one statement. */` |
|    246 | 2096 | `static void Sq3BindsClear(phl_sq3_stmt *pSt)` |
|      3 | 2097 | `{` |
|    249 | 2098 | `	phl_sq3_bind *pB = pSt->pBinds;` |
|    325 | 2099 | `	while( pB ){` |
|     78 | 2100 | `		phl_sq3_bind *pNext = pB->pNext;` |
|     78 | 2101 | `		if( pB->pVal ){` |
|     72 | 2102 | `			ph7_release_value(pSt->pConn->pVm,pB->pVal);` |
|     35 | 2103 | `		}` |
|     78 | 2104 | `		if( pB->zName ){` |
|      5 | 2105 | `			SyMemBackendFree(&pSt->pConn->pVm->sAllocator,pB->zName);` |
|      2 | 2106 | `		}` |
|     78 | 2107 | `		SyMemBackendFree(&pSt->pConn->pVm->sAllocator,pB);` |
|     78 | 2108 | `		pB = pNext;` |
|      2 | 2109 | `	}` |
|    249 | 2110 | `	pSt->pBinds = 0;` |
|    249 | 2111 | `}` |
|      - | 2112 | `/*` |
|      - | 2113 | ` * Record one binding, replacing whatever the same KEY already held -- and the` |
|      - | 2114 | ` * key is the name for a named bind and the position for a positional one, so` |
|      - | 2115 | `` * re-binding `:a` overwrites the earlier `:a` while `bindValue(1,...)` beside it`` |
|      - | 2116 | ` * is a second entry that also runs.` |
|      - | 2117 | ` *` |
|      - | 2118 | ` * New entries go on the END: php's table keeps insertion order and applies them` |
|      - | 2119 | ` * in it, which is the order any diagnostics come out in.` |
|      - | 2120 | ` */` |
|     78 | 2121 | `static phl_sq3_bind * Sq3BindSlot(phl_sq3_stmt *pSt,int iPos,const char *zName,int nName)` |
|      2 | 2122 | `{` |
|     80 | 2123 | `	ph7_vm *pVm = pSt->pConn->pVm;` |
|     80 | 2124 | `	phl_sq3_bind *pB,*pPrev = 0,*pTail = 0;` |
|    110 | 2125 | `	for( pB = pSt->pBinds ; pB ; pPrev = pB, pB = pB->pNext ){` |
|     49 | 2126 | `		int bSame = zName` |
|    ! 0 | 2127 | `			? (pB->zName != 0 && pB->nName == nName` |
|    ! 0 | 2128 | `			   && SyMemcmp(pB->zName,zName,(sxu32)nName) == 0)` |
|     32 | 2129 | `			: (pB->zName == 0 && pB->iPos == iPos);` |
|     33 | 2130 | `		if( bSame ){` |
|      - | 2131 | `			/* php replaces a binding by REMOVING the old entry and adding the` |
|      - | 2132 | `			 * new one, so re-binding a key moves it to the end of the run --` |
|      - | 2133 | `			 * which is the order any failures are reported in. */` |
|      3 | 2134 | `			if( pB->pVal ){` |
|      3 | 2135 | `				ph7_release_value(pVm,pB->pVal);` |
|      3 | 2136 | `				pB->pVal = 0;` |
|      1 | 2137 | `			}` |
|      3 | 2138 | `			if( pPrev ){` |
|    ! 0 | 2139 | `				pPrev->pNext = pB->pNext;` |
|    ! 0 | 2140 | `			}else{` |
|      3 | 2141 | `				pSt->pBinds = pB->pNext;` |
|      - | 2142 | `			}` |
|      3 | 2143 | `			pB->pNext = 0;` |
|      3 | 2144 | `			pB->iPos = iPos;` |
|      3 | 2145 | `			pB->nSlot = SXU32_HIGH;` |
|      3 | 2146 | `			break;` |
|      - | 2147 | `		}` |
|     16 | 2148 | `	}` |
|     80 | 2149 | `	if( pB == 0 ){` |
|     78 | 2150 | `		pB = (phl_sq3_bind *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_sq3_bind));` |
|     78 | 2151 | `		if( pB == 0 ){` |
|    ! 0 | 2152 | `			return 0;` |
|      - | 2153 | `		}` |
|     78 | 2154 | `		SyZero(pB,sizeof(phl_sq3_bind));` |
|     78 | 2155 | `		pB->iPos = iPos;` |
|     78 | 2156 | `		pB->nSlot = SXU32_HIGH;` |
|     78 | 2157 | `		if( zName && nName > 0 ){` |
|      5 | 2158 | `			pB->zName = (char *)SyMemBackendDup(&pVm->sAllocator,zName,(sxu32)nName);` |
|      5 | 2159 | `			if( pB->zName == 0 ){` |
|    ! 0 | 2160 | `				SyMemBackendFree(&pVm->sAllocator,pB);` |
|    ! 0 | 2161 | `				return 0;` |
|      - | 2162 | `			}` |
|      5 | 2163 | `			pB->nName = nName;` |
|      2 | 2164 | `		}` |
|     38 | 2165 | `	}` |
|     80 | 2166 | `	for( pTail = pSt->pBinds ; pTail && pTail->pNext ; pTail = pTail->pNext ){}` |
|     80 | 2167 | `	if( pTail ){` |
|     33 | 2168 | `		pTail->pNext = pB;` |
|     17 | 2169 | `	}else{` |
|     48 | 2170 | `		pSt->pBinds = pB;` |
|      - | 2171 | `	}` |
|     80 | 2172 | `	return pB;` |
|     41 | 2173 | `}` |
|      - | 2174 | `/*` |
|      - | 2175 | `` * Which position a `string\|int $param` names.`` |
|      - | 2176 | ` *` |
|      - | 2177 | ` * An INT is the position itself, and 0 is the one php refuses outright -- every` |
|      - | 2178 | ` * other number is accepted here and only fails when execute() tries to bind it.` |
|      - | 2179 | ` * A NAME is looked up in the statement, first as the script spelled it and then` |
|      - | 2180 | ``  * with a `:` in front, so `:a` and `a` both find the same parameter while `@a` `` |
|      - | 2181 | ` * finds nothing. Answers 0 for a name the statement does not carry.` |
|      - | 2182 | ` */` |
|     84 | 2183 | `static int Sq3BindPosition(phl_sq3_stmt *pSt,ph7_value *pParam)` |
|      2 | 2184 | `{` |
|      - | 2185 | `	const char *zName;` |
|     86 | 2186 | `	int nName = 0;` |
|      - | 2187 | `	SyBlob sName;` |
|      - | 2188 | `	int iPos;` |
|     86 | 2189 | `	if( (pParam->iFlags & MEMOBJ_STRING) == 0 ){` |
|     78 | 2190 | `		return (int)ph7_value_to_int64(pParam);` |
|      - | 2191 | `	}` |
|      9 | 2192 | `	zName = ph7_value_to_string(pParam,&nName);` |
|      9 | 2193 | `	SyBlobInit(&sName,&pSt->pConn->pVm->sAllocator);` |
|      9 | 2194 | `	SyBlobAppend(&sName,zName,(sxu32)nName);` |
|      9 | 2195 | `	SyBlobNullAppend(&sName);` |
|      9 | 2196 | `	iPos = sqlite3_bind_parameter_index(pSt->pStmt,(const char *)SyBlobData(&sName));` |
|      9 | 2197 | `	if( iPos == 0 ){` |
|      7 | 2198 | `		SyBlobReset(&sName);` |
|      7 | 2199 | `		SyBlobAppend(&sName,":",sizeof(char));` |
|      7 | 2200 | `		SyBlobAppend(&sName,zName,(sxu32)nName);` |
|      7 | 2201 | `		SyBlobNullAppend(&sName);` |
|      7 | 2202 | `		iPos = sqlite3_bind_parameter_index(pSt->pStmt,(const char *)SyBlobData(&sName));` |
|      3 | 2203 | `	}` |
|      9 | 2204 | `	SyBlobRelease(&sName);` |
|      9 | 2205 | `	return iPos;` |
|     44 | 2206 | `}` |
|      - | 2207 | `/*` |
|      - | 2208 | ` * php's type for a value nobody declared one for: the zval's own kind, with` |
|      - | 2209 | ` * bool counting as an integer and everything that is not a number or null` |
|      - | 2210 | ` * counting as text.` |
|      - | 2211 | ` */` |
|     48 | 2212 | `static int Sq3TypeOfValue(ph7_value *pVal)` |
|      1 | 2213 | `{` |
|     49 | 2214 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) != 0 ){` |
|      5 | 2215 | `		return SQLITE_NULL;` |
|      - | 2216 | `	}` |
|      - | 2217 | `	/* REAL is asked first: a PHL float carries the integer flag beside it, so` |
|      - | 2218 | `	 * the other order makes 0.0 an integer. */` |
|     45 | 2219 | `	if( (pVal->iFlags & MEMOBJ_REAL) != 0 ){` |
|      9 | 2220 | `		return SQLITE_FLOAT;` |
|      - | 2221 | `	}` |
|     37 | 2222 | `	if( (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) != 0 ){` |
|     25 | 2223 | `		return SQLITE_INTEGER;` |
|      - | 2224 | `	}` |
|     13 | 2225 | `	return SQLITE3_TEXT;` |
|     25 | 2226 | `}` |
|      - | 2227 | `/*` |
|      - | 2228 | ` * Hand one recorded binding to sqlite, converting the value the way the` |
|      - | 2229 | ` * declared type asks for. A NULL value is bound as NULL whatever the type says` |
|      - | 2230 | ` * -- php asks that question first -- and every other type is php's own cast,` |
|      - | 2231 | `` * so `bindValue(1,"12abc",SQLITE3_INTEGER)` binds 12 and a bool bound as text`` |
|      - | 2232 | ` * binds "1" or "".` |
|      - | 2233 | ` */` |
|     84 | 2234 | `static int Sq3BindApply(ph7_context *pCtx,phl_sq3_stmt *pSt,phl_sq3_bind *pB,` |
|      - | 2235 | `	ph7_value *pVal)` |
|      2 | 2236 | `{` |
|     86 | 2237 | `	sqlite3_stmt *pStmt = pSt->pStmt;` |
|     86 | 2238 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) != 0 \|\| pB->iType == SQLITE_NULL ){` |
|     11 | 2239 | `		return sqlite3_bind_null(pStmt,pB->iPos);` |
|      - | 2240 | `	}` |
|     76 | 2241 | `	switch( pB->iType ){` |
|     16 | 2242 | `		case SQLITE_INTEGER:` |
|     49 | 2243 | `			return sqlite3_bind_int64(pStmt,pB->iPos,` |
|     32 | 2244 | `				(sqlite3_int64)ph7_value_to_int64(pVal));` |
|      - | 2245 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      6 | 2246 | `		case SQLITE_FLOAT:` |
|     13 | 2247 | `			return sqlite3_bind_double(pStmt,pB->iPos,ph7_value_to_double(pVal));` |
|      - | 2248 | `#endif` |
|     13 | 2249 | `		case SQLITE_BLOB:` |
|      - | 2250 | `		case SQLITE3_TEXT: {` |
|      - | 2251 | `			/* php's own conversion, with its own diagnostics -- an array is the` |
|      - | 2252 | ``			 * `Array to string conversion` warning and the string "Array", an`` |
|      - | 2253 | `			 * object with no __toString the catchable Error. It runs on a COPY:` |
|      - | 2254 | `			 * bindParam names the caller's variable and php never rewrites it. */` |
|      - | 2255 | `			ph7_value sTmp;` |
|     27 | 2256 | `			const char *zVal = 0;` |
|     27 | 2257 | `			int nVal = 0, rcBind;` |
|     27 | 2258 | `			PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     27 | 2259 | `			PH7_MemObjStore(pVal,&sTmp);` |
|     27 | 2260 | `			if( PH7_ValueToStringUV(pCtx,&sTmp,&zVal,&nVal) != SXRET_OK ){` |
|      3 | 2261 | `				PH7_MemObjRelease(&sTmp);` |
|      3 | 2262 | `				return SQLITE_OK;   /* the throw is already parked on the context */` |
|      - | 2263 | `			}` |
|     25 | 2264 | `			if( pB->iType == SQLITE_BLOB ){` |
|      5 | 2265 | `				rcBind = sqlite3_bind_blob(pStmt,pB->iPos,zVal ? zVal : "",nVal,` |
|      - | 2266 | `					SQLITE_TRANSIENT);` |
|      3 | 2267 | `			}else{` |
|     21 | 2268 | `				rcBind = sqlite3_bind_text(pStmt,pB->iPos,zVal ? zVal : "",nVal,` |
|      - | 2269 | `					SQLITE_TRANSIENT);` |
|      - | 2270 | `			}` |
|     25 | 2271 | `			PH7_MemObjRelease(&sTmp);` |
|     25 | 2272 | `			return rcBind;` |
|      - | 2273 | `		}` |
|      2 | 2274 | `		default:` |
|      4 | 2275 | `			break;` |
|      - | 2276 | `	}` |
|      5 | 2277 | `	return -1;   /* a type php has no case for; the caller reports it */` |
|     44 | 2278 | `}` |
|      - | 2279 | `/*` |
|      - | 2280 | ` * Apply every binding, in the order they were MADE. A failure is reported and` |
|      - | 2281 | ` * the run goes ON -- php reports the bind and steps the statement anyway, so a` |
|      - | 2282 | ` * parameter that could not be bound is simply the NULL it already was.` |
|      - | 2283 | ` */` |
|     82 | 2284 | `static sxi32 Sq3BindsApply(ph7_context *pCtx,phl_sq3_stmt *pSt,const char *zFn)` |
|      2 | 2285 | `{` |
|      - | 2286 | `	phl_sq3_bind *pB;` |
|     84 | 2287 | `	sxi32 rc = PH7_OK;` |
|    162 | 2288 | `	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){` |
|     86 | 2289 | `		ph7_value *pVal = pB->pVal;` |
|      - | 2290 | `		int rcBind;` |
|     86 | 2291 | `		if( pB->nSlot != SXU32_HIGH ){` |
|      - | 2292 | `			/* bindParam: the caller's variable, read HERE rather than at bind */` |
|      7 | 2293 | `			pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pB->nSlot);` |
|      3 | 2294 | `		}` |
|     86 | 2295 | `		rcBind = Sq3BindApply(pCtx,pSt,pB,pVal);` |
|     86 | 2296 | `		if( pCtx->nThrowRc != PH7_OK ){` |
|      3 | 2297 | `			return pCtx->nThrowRc;   /* the conversion threw; php propagates it too */` |
|      - | 2298 | `		}` |
|     84 | 2299 | `		if( rcBind < 0 ){` |
|      - | 2300 | `			/* A type php has no case for. It is a programming error rather than` |
|      - | 2301 | `			 * a database one, so it stops the run instead of being reported and` |
|      - | 2302 | `			 * carried past -- and it is the ONE answer here that cannot be` |
|      - | 2303 | `			 * checked against php, whose own sentence for it carries a printf` |
|      - | 2304 | `			 * modifier its engine no longer supports: naming an unknown type` |
|      - | 2305 | `			 * ENDS the request there rather than printing anything. The text is` |
|      - | 2306 | `			 * php's with the two numbers filled in. */` |
|      7 | 2307 | `			return PH7_VmThrowException(pCtx,"Error",` |
|      2 | 2308 | `				"Unknown parameter type: %d for parameter %d",pB->iType,pB->iPos);` |
|      - | 2309 | `		}` |
|     79 | 2310 | `		if( rcBind != SQLITE_OK ){` |
|      - | 2311 | `			SyBlob sMsg;` |
|      7 | 2312 | `			SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|      7 | 2313 | `			SyBlobFormat(&sMsg,"Unable to bind parameter number %d",pB->iPos);` |
|      7 | 2314 | `			SyBlobNullAppend(&sMsg);` |
|      7 | 2315 | `			rc = Sq3Error(pCtx,pSt->pConn,zFn,rcBind,(const char *)SyBlobData(&sMsg));` |
|      7 | 2316 | `			SyBlobRelease(&sMsg);` |
|      7 | 2317 | `			if( rc != PH7_OK ){` |
|    ! 0 | 2318 | `				return rc;` |
|      - | 2319 | `			}` |
|      3 | 2320 | `		}` |
|     40 | 2321 | `	}` |
|     77 | 2322 | `	return rc;` |
|     43 | 2323 | `}` |
|    300 | 2324 | `static phl_sq3_stmt * Sq3StmtOfInstance(ph7_class_instance *pThis)` |
|      3 | 2325 | `{` |
|    303 | 2326 | `	return (phl_sq3_stmt *)Sq3ResourceOf(pThis);` |
|      3 | 2327 | `}` |
|      - | 2328 | `/*` |
|      - | 2329 | ` * php asks TWO questions about a statement and words them with two different` |
|      - | 2330 | ` * class names, which is visible to a script.` |
|      - | 2331 | ` *` |
|      - | 2332 | ` * The FIRST is whether the statement is attached to a live connection at all --` |
|      - | 2333 | ` * php's macro is given the DATABASE object there, so a never-prepared object` |
|      - | 2334 | `` * and a closed statement are both `The SQLite3 object ...`. execute() and`` |
|      - | 2335 | ` * close() ask only this one, which is why close() succeeds on a statement of` |
|      - | 2336 | ` * NOTHING and a second close() is the Error.` |
|      - | 2337 | ` */` |
|    214 | 2338 | `static phl_sq3_stmt * Sq3LiveStmtNull(ph7_context *pCtx,sxi32 *pRc)` |
|      3 | 2339 | `{` |
|    217 | 2340 | `	phl_sq3_stmt *pSt = Sq3StmtOfInstance(PH7_ContextThis(pCtx));` |
|    217 | 2341 | `	*pRc = PH7_OK;` |
|    217 | 2342 | `	if( pSt == 0 \|\| !pSt->bInitialised ){` |
|      8 | 2343 | `		*pRc = Sq3Uninitialised(pCtx,"SQLite3");` |
|      8 | 2344 | `		return 0;` |
|      - | 2345 | `	}` |
|    210 | 2346 | `	return pSt;` |
|    110 | 2347 | `}` |
|      - | 2348 | `/*` |
|      - | 2349 | ` * The SECOND is whether there is a handle to work on, and it names the` |
|      - | 2350 | ` * STATEMENT class -- so a comment-only prepare, which php keeps as an object` |
|      - | 2351 | `` * with no handle, answers `The SQLite3Stmt object ...` to every accessor.`` |
|      - | 2352 | ` */` |
|    128 | 2353 | `static phl_sq3_stmt * Sq3LiveStmt(ph7_context *pCtx,sxi32 *pRc)` |
|      2 | 2354 | `{` |
|    130 | 2355 | `	phl_sq3_stmt *pSt = Sq3LiveStmtNull(pCtx,pRc);` |
|    130 | 2356 | `	if( pSt == 0 ){` |
|      3 | 2357 | `		return 0;` |
|      - | 2358 | `	}` |
|    128 | 2359 | `	if( pSt->pStmt == 0 ){` |
|      5 | 2360 | `		*pRc = Sq3Uninitialised(pCtx,"SQLite3Stmt");` |
|      5 | 2361 | `		return 0;` |
|      - | 2362 | `	}` |
|    124 | 2363 | `	return pSt;` |
|     66 | 2364 | `}` |
|      - | 2365 | `/* The statement object is going away: let go of its hold on the statement. */` |
|     86 | 2366 | `static void Sq3StmtInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      3 | 2367 | `{` |
|     89 | 2368 | `	phl_sq3_stmt *pSt = Sq3StmtOfInstance(pThis);` |
|     43 | 2369 | `	SXUNUSED(pVm);` |
|     89 | 2370 | `	if( pSt == 0 \|\| pSt->pOwner != pThis ){` |
|      3 | 2371 | `		return;` |
|      - | 2372 | `	}` |
|     87 | 2373 | `	pSt->pOwner = 0;` |
|     87 | 2374 | `	Sq3StmtUnref(pSt);` |
|     46 | 2375 | `}` |
|      - | 2376 | `/*` |
|      - | 2377 | ` * SQLite3Stmt::__construct(SQLite3 $sqlite3, string $query) -- private, and it` |
|      - | 2378 | ` * never runs: prepare() builds every statement itself.` |
|      - | 2379 | ` */` |
|    ! 0 | 2380 | `static int vm_builtin_SQLite3Stmt_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2381 | `{` |
|    ! 0 | 2382 | `	SXUNUSED(nArg);` |
|    ! 0 | 2383 | `	SXUNUSED(apArg);` |
|    ! 0 | 2384 | `	ph7_result_null(pCtx);` |
|    ! 0 | 2385 | `	return PH7_OK;` |
|    ! 0 | 2386 | `}` |
|      - | 2387 | `/*` |
|      - | 2388 | ` * SQLite3::prepare(string $query): SQLite3Stmt\|false` |
|      - | 2389 | ` *` |
|      - | 2390 | ` * Compiles without running. A query that holds no statement still answers an` |
|      - | 2391 | ` * OBJECT -- one whose handle is NULL, so every accessor on it is the Error and` |
|      - | 2392 | ` * only close() answers -- while the EMPTY query is the silent false.` |
|      - | 2393 | ` */` |
|     88 | 2394 | `static int vm_builtin_SQLite3_prepare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2395 | `{` |
|      - | 2396 | `	sxi32 rc;` |
|     91 | 2397 | `	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);` |
|      - | 2398 | `	const char *zSql;` |
|     91 | 2399 | `	int nSql = 0;` |
|      - | 2400 | `	phl_sq3_stmt *pSt;` |
|      - | 2401 | `	ph7_class *pClass;` |
|      - | 2402 | `	ph7_class_instance *pObj;` |
|      - | 2403 | `	Sq3Verb sVerb;` |
|     44 | 2404 | `	SXUNUSED(nArg);` |
|     91 | 2405 | `	if( pConn == 0 ){` |
|    ! 0 | 2406 | `		return rc;` |
|      - | 2407 | `	}` |
|     91 | 2408 | `	zSql = ph7_value_to_string(apArg[0],&nSql);` |
|     91 | 2409 | `	Sq3VerbEnter(pConn,pCtx,"SQLite3::prepare",&sVerb);` |
|     91 | 2410 | `	pSt = Sq3Prepare(pCtx,pConn,zSql,nSql,"SQLite3::prepare",&rc);` |
|      - | 2411 | `	{` |
|      - | 2412 | `		/* the AUTHORIZER runs while sqlite compiles, so even a prepare can be` |
|      - | 2413 | `		 * the call a callback threw out of */` |
|     91 | 2414 | `		sxi32 rcCb = Sq3VerbLeave(pConn,&sVerb);` |
|     91 | 2415 | `		if( rcCb != PH7_OK ){` |
|    ! 0 | 2416 | `			if( pSt ){` |
|    ! 0 | 2417 | `				Sq3StmtUnref(pSt);` |
|    ! 0 | 2418 | `			}` |
|    ! 0 | 2419 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2420 | `			return rcCb;` |
|      - | 2421 | `		}` |
|      - | 2422 | `	}` |
|     91 | 2423 | `	if( pSt == 0 ){` |
|      5 | 2424 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2425 | `		return rc;` |
|      - | 2426 | `	}` |
|     87 | 2427 | `	pClass = PH7_VmExtractClass(pCtx->pVm,"SQLite3Stmt",sizeof("SQLite3Stmt")-1,FALSE,0);` |
|     87 | 2428 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|     87 | 2429 | `	if( pObj == 0 \|\| Sq3AttachRes(pObj,pSt) != 0 ){` |
|    ! 0 | 2430 | `		if( pObj ){` |
|    ! 0 | 2431 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 | 2432 | `		}` |
|    ! 0 | 2433 | `		Sq3StmtUnref(pSt);` |
|    ! 0 | 2434 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2435 | `	}` |
|     87 | 2436 | `	pSt->pOwner = pObj;` |
|     87 | 2437 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     87 | 2438 | `	return PH7_OK;` |
|     47 | 2439 | `}` |
|      - | 2440 | `/*` |
|      - | 2441 | ` * SQLite3Stmt::execute(): SQLite3Result\|false` |
|      - | 2442 | ` *` |
|      - | 2443 | ` * The bindings are applied, the statement is run once and rewound, and the` |
|      - | 2444 | ` * result walks the SAME statement -- so two results handed out by one` |
|      - | 2445 | ` * statement are two views of one cursor, and executing again rewinds both.` |
|      - | 2446 | ` *` |
|      - | 2447 | ` * A statement of NOTHING has no handle to step, and php asks the NULL one for` |
|      - | 2448 | ` * its database rather than the connection it came from: what sqlite says about` |
|      - | 2449 | `` * no connection at all is `out of memory`, and that is the message.`` |
|      - | 2450 | ` */` |
|     80 | 2451 | `static int vm_builtin_SQLite3Stmt_execute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2452 | `{` |
|      - | 2453 | `	sxi32 rc;` |
|     83 | 2454 | `	phl_sq3_stmt *pSt = Sq3LiveStmtNull(pCtx,&rc);` |
|      - | 2455 | `	ph7_class_instance *pObj;` |
|      - | 2456 | `	Sq3Verb sVerb;` |
|      - | 2457 | `	int iStep;` |
|     40 | 2458 | `	SXUNUSED(nArg);` |
|     40 | 2459 | `	SXUNUSED(apArg);` |
|     83 | 2460 | `	if( pSt == 0 ){` |
|      6 | 2461 | `		return rc;` |
|      - | 2462 | `	}` |
|     78 | 2463 | `	if( pSt->pStmt == 0 ){` |
|      3 | 2464 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2465 | `		return Sq3Error(pCtx,pSt->pConn,"SQLite3Stmt::execute",0,` |
|      - | 2466 | `			"Unable to execute statement: out of memory");` |
|      - | 2467 | `	}` |
|     76 | 2468 | `	sqlite3_reset(pSt->pStmt);` |
|     76 | 2469 | `	Sq3VerbEnter(pSt->pConn,pCtx,"SQLite3Stmt::execute",&sVerb);` |
|     76 | 2470 | `	rc = Sq3BindsApply(pCtx,pSt,"SQLite3Stmt::execute");` |
|     76 | 2471 | `	iStep = rc == PH7_OK ? sqlite3_step(pSt->pStmt) : SQLITE_OK;` |
|      - | 2472 | `	{` |
|     76 | 2473 | `		sxi32 rcCb = Sq3VerbLeave(pSt->pConn,&sVerb);` |
|     76 | 2474 | `		if( rcCb != PH7_OK ){` |
|    ! 0 | 2475 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2476 | `			return rcCb;` |
|      - | 2477 | `		}` |
|      - | 2478 | `	}` |
|     76 | 2479 | `	if( rc != PH7_OK ){` |
|      6 | 2480 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2481 | `		return rc;` |
|      - | 2482 | `	}` |
|     71 | 2483 | `	if( iStep != SQLITE_ROW && iStep != SQLITE_DONE ){` |
|      - | 2484 | `		SyBlob sMsg;` |
|    ! 0 | 2485 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    ! 0 | 2486 | `		SyBlobFormat(&sMsg,"Unable to execute statement: %s",sqlite3_errmsg(pSt->pConn->pDb));` |
|    ! 0 | 2487 | `		SyBlobNullAppend(&sMsg);` |
|    ! 0 | 2488 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2489 | `		rc = Sq3Error(pCtx,pSt->pConn,"SQLite3Stmt::execute",` |
|    ! 0 | 2490 | `			sqlite3_errcode(pSt->pConn->pDb),(const char *)SyBlobData(&sMsg));` |
|    ! 0 | 2491 | `		SyBlobRelease(&sMsg);` |
|    ! 0 | 2492 | `		return rc;` |
|      - | 2493 | `	}` |
|     71 | 2494 | `	sqlite3_reset(pSt->pStmt);` |
|     71 | 2495 | `	pSt->nRef++;   /* the result is a SECOND holder: the object keeps its own */` |
|     71 | 2496 | `	pObj = Sq3NewResultObject(pCtx,pSt,pSt->pOwner);` |
|     71 | 2497 | `	if( pObj == 0 ){` |
|    ! 0 | 2498 | `		Sq3StmtUnref(pSt);` |
|    ! 0 | 2499 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2500 | `	}` |
|     71 | 2501 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     71 | 2502 | `	return PH7_OK;` |
|     43 | 2503 | `}` |
|      - | 2504 | `/* SQLite3Stmt::bindValue(string\|int $param, mixed $value, int $type = SQLITE3_TEXT): bool` |
|      - | 2505 | ` * SQLite3Stmt::bindParam(string\|int $param, mixed &$var, int $type = SQLITE3_TEXT): bool` |
|      - | 2506 | ` *` |
|      - | 2507 | ` * The two differ in WHEN the value is read and in what an omitted $type means.` |
|      - | 2508 | ` * bindValue copies the value now and, with no type given, takes the type from` |
|      - | 2509 | ` * that value; bindParam records the caller's SLOT and reads it at execute --` |
|      - | 2510 | ` * where there is no value yet to take a type from, so the declared default` |
|      - | 2511 | ` * stands and an unqualified bindParam() binds TEXT even for an integer.` |
|      - | 2512 | ` */` |
|     86 | 2513 | `static int Sq3BindOne(ph7_context *pCtx,int nArg,ph7_value **apArg,int bByRef)` |
|      2 | 2514 | `{` |
|      - | 2515 | `	sxi32 rc;` |
|     88 | 2516 | `	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);` |
|      - | 2517 | `	phl_sq3_bind *pB;` |
|     88 | 2518 | `	const char *zName = 0;` |
|     88 | 2519 | `	int nName = 0, iPos;` |
|     88 | 2520 | `	if( pSt == 0 ){` |
|      3 | 2521 | `		return rc;` |
|      - | 2522 | `	}` |
|     86 | 2523 | `	iPos = Sq3BindPosition(pSt,apArg[0]);` |
|     86 | 2524 | `	if( iPos < 1 ){` |
|      7 | 2525 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2526 | `		return PH7_OK;` |
|      - | 2527 | `	}` |
|     80 | 2528 | `	if( (apArg[0]->iFlags & MEMOBJ_STRING) != 0 ){` |
|      5 | 2529 | `		zName = ph7_value_to_string(apArg[0],&nName);` |
|      2 | 2530 | `	}` |
|     80 | 2531 | `	pB = Sq3BindSlot(pSt,iPos,zName,nName);` |
|     80 | 2532 | `	if( pB == 0 ){` |
|    ! 0 | 2533 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2534 | `	}` |
|     80 | 2535 | `	if( nArg > 2 ){` |
|     26 | 2536 | `		pB->iType = (int)ph7_value_to_int64(apArg[2]);` |
|     14 | 2537 | `	}else{` |
|     55 | 2538 | `		pB->iType = bByRef ? SQLITE3_TEXT : Sq3TypeOfValue(apArg[1]);` |
|      - | 2539 | `	}` |
|     80 | 2540 | `	if( bByRef ){` |
|      7 | 2541 | `		pB->nSlot = apArg[1]->nIdx;` |
|      4 | 2542 | `	}else{` |
|      - | 2543 | `		/* the copy has to OUTLIVE this call, so it is the VM's rather than the` |
|      - | 2544 | `		 * context's -- a context value dies with the call that made it */` |
|     74 | 2545 | `		pB->pVal = ph7_new_scalar(pCtx->pVm);` |
|     74 | 2546 | `		if( pB->pVal == 0 ){` |
|    ! 0 | 2547 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2548 | `		}` |
|     74 | 2549 | `		PH7_MemObjStore(apArg[1],pB->pVal);` |
|      - | 2550 | `	}` |
|     80 | 2551 | `	ph7_result_bool(pCtx,1);` |
|     80 | 2552 | `	return PH7_OK;` |
|     45 | 2553 | `}` |
|     80 | 2554 | `static int vm_builtin_SQLite3Stmt_bindValue(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2555 | `{` |
|     82 | 2556 | `	return Sq3BindOne(pCtx,nArg,apArg,0);` |
|      2 | 2557 | `}` |
|      6 | 2558 | `static int vm_builtin_SQLite3Stmt_bindParam(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2559 | `{` |
|      7 | 2560 | `	return Sq3BindOne(pCtx,nArg,apArg,1);` |
|      1 | 2561 | `}` |
|      - | 2562 | `/* SQLite3Stmt::clear(): bool -- drop the bindings, php's and sqlite's both. */` |
|      2 | 2563 | `static int vm_builtin_SQLite3Stmt_clear(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2564 | `{` |
|      - | 2565 | `	sxi32 rc;` |
|      3 | 2566 | `	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);` |
|      1 | 2567 | `	SXUNUSED(nArg);` |
|      1 | 2568 | `	SXUNUSED(apArg);` |
|      3 | 2569 | `	if( pSt == 0 ){` |
|    ! 0 | 2570 | `		return rc;` |
|      - | 2571 | `	}` |
|      3 | 2572 | `	Sq3BindsClear(pSt);` |
|      3 | 2573 | `	ph7_result_bool(pCtx,sqlite3_clear_bindings(pSt->pStmt) == SQLITE_OK);` |
|      3 | 2574 | `	return PH7_OK;` |
|      2 | 2575 | `}` |
|      - | 2576 | `/*` |
|      - | 2577 | ` * SQLite3Stmt::close(): true` |
|      - | 2578 | ` *` |
|      - | 2579 | ` * Finalizes NOW, whatever results are still walking it -- they start answering` |
|      - | 2580 | ` * the Error. A second close() is the Error too, since there is no handle left` |
|      - | 2581 | ` * to close.` |
|      - | 2582 | ` */` |
|      6 | 2583 | `static int vm_builtin_SQLite3Stmt_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2584 | `{` |
|      - | 2585 | `	sxi32 rc;` |
|      7 | 2586 | `	phl_sq3_stmt *pSt = Sq3LiveStmtNull(pCtx,&rc);` |
|      3 | 2587 | `	SXUNUSED(nArg);` |
|      3 | 2588 | `	SXUNUSED(apArg);` |
|      7 | 2589 | `	if( pSt == 0 ){` |
|    ! 0 | 2590 | `		return rc;` |
|      - | 2591 | `	}` |
|      7 | 2592 | `	if( pSt->pStmt ){` |
|      5 | 2593 | `		sqlite3_finalize(pSt->pStmt);` |
|      5 | 2594 | `		pSt->pStmt = 0;` |
|      2 | 2595 | `	}` |
|      7 | 2596 | `	pSt->bInitialised = 0;` |
|      7 | 2597 | `	Sq3BindsClear(pSt);` |
|      7 | 2598 | `	ph7_result_bool(pCtx,1);` |
|      7 | 2599 | `	return PH7_OK;` |
|      4 | 2600 | `}` |
|      - | 2601 | `/* SQLite3Stmt::reset(): bool -- rewind, keeping the bindings. */` |
|    ! 0 | 2602 | `static int vm_builtin_SQLite3Stmt_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2603 | `{` |
|      - | 2604 | `	sxi32 rc;` |
|    ! 0 | 2605 | `	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);` |
|    ! 0 | 2606 | `	SXUNUSED(nArg);` |
|    ! 0 | 2607 | `	SXUNUSED(apArg);` |
|    ! 0 | 2608 | `	if( pSt == 0 ){` |
|    ! 0 | 2609 | `		return rc;` |
|      - | 2610 | `	}` |
|    ! 0 | 2611 | `	ph7_result_bool(pCtx,sqlite3_reset(pSt->pStmt) == SQLITE_OK);` |
|    ! 0 | 2612 | `	return PH7_OK;` |
|    ! 0 | 2613 | `}` |
|      - | 2614 | `/* SQLite3Stmt::paramCount(): int */` |
|      8 | 2615 | `static int vm_builtin_SQLite3Stmt_paramCount(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2616 | `{` |
|      - | 2617 | `	sxi32 rc;` |
|     10 | 2618 | `	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);` |
|      4 | 2619 | `	SXUNUSED(nArg);` |
|      4 | 2620 | `	SXUNUSED(apArg);` |
|     10 | 2621 | `	if( pSt == 0 ){` |
|      5 | 2622 | `		return rc;` |
|      - | 2623 | `	}` |
|      6 | 2624 | `	ph7_result_int64(pCtx,(ph7_int64)sqlite3_bind_parameter_count(pSt->pStmt));` |
|      6 | 2625 | `	return PH7_OK;` |
|      6 | 2626 | `}` |
|      - | 2627 | `/* SQLite3Stmt::readOnly(): bool -- whether running it can change the database. */` |
|      4 | 2628 | `static int vm_builtin_SQLite3Stmt_readOnly(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2629 | `{` |
|      - | 2630 | `	sxi32 rc;` |
|      5 | 2631 | `	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);` |
|      2 | 2632 | `	SXUNUSED(nArg);` |
|      2 | 2633 | `	SXUNUSED(apArg);` |
|      5 | 2634 | `	if( pSt == 0 ){` |
|    ! 0 | 2635 | `		return rc;` |
|      - | 2636 | `	}` |
|      5 | 2637 | `	ph7_result_bool(pCtx,sqlite3_stmt_readonly(pSt->pStmt) != 0);` |
|      5 | 2638 | `	return PH7_OK;` |
|      3 | 2639 | `}` |
|      - | 2640 | `/* SQLite3Stmt::busy(): bool -- whether a walk is under way on it. */` |
|      4 | 2641 | `static int vm_builtin_SQLite3Stmt_busy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2642 | `{` |
|      - | 2643 | `	sxi32 rc;` |
|      5 | 2644 | `	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);` |
|      2 | 2645 | `	SXUNUSED(nArg);` |
|      2 | 2646 | `	SXUNUSED(apArg);` |
|      5 | 2647 | `	if( pSt == 0 ){` |
|    ! 0 | 2648 | `		return rc;` |
|      - | 2649 | `	}` |
|      5 | 2650 | `	ph7_result_bool(pCtx,sqlite3_stmt_busy(pSt->pStmt) != 0);` |
|      5 | 2651 | `	return PH7_OK;` |
|      3 | 2652 | `}` |
|      - | 2653 | `/*` |
|      - | 2654 | ` * SQLite3Stmt::getSQL(bool $expand = false): string\|false` |
|      - | 2655 | ` *` |
|      - | 2656 | ` * The text as it was PREPARED, or -- expanded -- the same text with every` |
|      - | 2657 | ` * bound parameter written into it, which is sqlite's own rendering and not a` |
|      - | 2658 | ` * substitution php performs.` |
|      - | 2659 | ` */` |
|      8 | 2660 | `static int vm_builtin_SQLite3Stmt_getSQL(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2661 | `{` |
|      - | 2662 | `	sxi32 rc;` |
|     10 | 2663 | `	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);` |
|     10 | 2664 | `	int bExpand = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|      - | 2665 | `	const char *zSql;` |
|     10 | 2666 | `	if( pSt == 0 ){` |
|    ! 0 | 2667 | `		return rc;` |
|      - | 2668 | `	}` |
|      - | 2669 | `	/* php binds FIRST and asks about $expand afterwards -- the same helper` |
|      - | 2670 | `	 * execute() uses -- so the rendering shows what the statement would run` |
|      - | 2671 | `	 * with, including the value a bindParam()ed variable holds right now, and` |
|      - | 2672 | `	 * even the UNEXPANDED spelling reports a binding that could not be applied.` |
|      - | 2673 | `	 * A failed bind is reported and the answer is produced without it. */` |
|     10 | 2674 | `	rc = Sq3BindsApply(pCtx,pSt,"SQLite3Stmt::getSQL");` |
|     10 | 2675 | `	if( rc != PH7_OK ){` |
|      3 | 2676 | `		return rc;` |
|      - | 2677 | `	}` |
|      7 | 2678 | `	if( bExpand ){` |
|      5 | 2679 | `		char *zExp = sqlite3_expanded_sql(pSt->pStmt);` |
|      5 | 2680 | `		if( zExp == 0 ){` |
|    ! 0 | 2681 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2682 | `			return PH7_OK;` |
|      - | 2683 | `		}` |
|      5 | 2684 | `		ph7_result_string(pCtx,zExp,-1);` |
|      5 | 2685 | `		sqlite3_free(zExp);` |
|      5 | 2686 | `		return PH7_OK;` |
|      - | 2687 | `	}` |
|      3 | 2688 | `	zSql = sqlite3_sql(pSt->pStmt);` |
|      3 | 2689 | `	if( zSql == 0 ){` |
|    ! 0 | 2690 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2691 | `		return PH7_OK;` |
|      - | 2692 | `	}` |
|      3 | 2693 | `	ph7_result_string(pCtx,zSql,-1);` |
|      3 | 2694 | `	return PH7_OK;` |
|      6 | 2695 | `}` |
|      - | 2696 | `/* SQLite3Stmt::explain(): int -- which of the three plans the statement runs. */` |
|      8 | 2697 | `static int vm_builtin_SQLite3Stmt_explain(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2698 | `{` |
|      - | 2699 | `	sxi32 rc;` |
|      9 | 2700 | `	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);` |
|      4 | 2701 | `	SXUNUSED(nArg);` |
|      4 | 2702 | `	SXUNUSED(apArg);` |
|      9 | 2703 | `	if( pSt == 0 ){` |
|    ! 0 | 2704 | `		return rc;` |
|      - | 2705 | `	}` |
|      9 | 2706 | `	ph7_result_int64(pCtx,(ph7_int64)sqlite3_stmt_isexplain(pSt->pStmt));` |
|      9 | 2707 | `	return PH7_OK;` |
|      5 | 2708 | `}` |
|      - | 2709 | `/*` |
|      - | 2710 | ` * SQLite3Stmt::setExplain(int $mode): bool` |
|      - | 2711 | ` *` |
|      - | 2712 | ` * Re-aims the SAME statement at its own query plan: mode 1 makes it answer the` |
|      - | 2713 | ` * eight columns of EXPLAIN and mode 2 the four of EXPLAIN QUERY PLAN, and mode` |
|      - | 2714 | ` * 0 puts it back. Anything else is a ValueError naming the constants.` |
|      - | 2715 | ` */` |
|      8 | 2716 | `static int vm_builtin_SQLite3Stmt_setExplain(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2717 | `{` |
|      - | 2718 | `	sxi32 rc;` |
|      9 | 2719 | `	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);` |
|      - | 2720 | `	ph7_int64 iMode;` |
|      4 | 2721 | `	SXUNUSED(nArg);` |
|      9 | 2722 | `	if( pSt == 0 ){` |
|    ! 0 | 2723 | `		return rc;` |
|      - | 2724 | `	}` |
|      9 | 2725 | `	iMode = ph7_value_to_int64(apArg[0]);` |
|      9 | 2726 | `	if( iMode < 0 \|\| iMode > 2 ){` |
|      3 | 2727 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2728 | `			"SQLite3Stmt::setExplain(): Argument #1 ($mode) must be one of the "` |
|      - | 2729 | `			"SQLite3Stmt::EXPLAIN_MODE_* constants");` |
|      - | 2730 | `	}` |
|      7 | 2731 | `	ph7_result_bool(pCtx,sqlite3_stmt_explain(pSt->pStmt,(int)iMode) == SQLITE_OK);` |
|      7 | 2732 | `	return PH7_OK;` |
|      5 | 2733 | `}` |
|      - | 2734 |  |
|      - | 2735 | `/* ------------------------------------------------------------------------` |
|      - | 2736 | ` * SQLite3Result -- the cursor` |
|      - | 2737 | ` * ------------------------------------------------------------------------ */` |
|      - | 2738 | `/*` |
|      - | 2739 | ` * SQLite3Result::__construct() -- private, and it never runs: the extension` |
|      - | 2740 | ` * builds every result itself. The body stands only so the declaration php makes` |
|      - | 2741 | ` * exists to be reflected.` |
|      - | 2742 | ` */` |
|    ! 0 | 2743 | `static int vm_builtin_SQLite3Result_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2744 | `{` |
|    ! 0 | 2745 | `	SXUNUSED(nArg);` |
|    ! 0 | 2746 | `	SXUNUSED(apArg);` |
|    ! 0 | 2747 | `	ph7_result_null(pCtx);` |
|    ! 0 | 2748 | `	return PH7_OK;` |
|    ! 0 | 2749 | `}` |
|      - | 2750 | `/* SQLite3Result::numColumns(): int -- what the STATEMENT declares, so a write` |
|      - | 2751 | ` * answers 0 and a SELECT answers its width before any row has been read. */` |
|     16 | 2752 | `static int vm_builtin_SQLite3Result_numColumns(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2753 | `{` |
|      - | 2754 | `	sxi32 rc;` |
|     17 | 2755 | `	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);` |
|      8 | 2756 | `	SXUNUSED(nArg);` |
|      8 | 2757 | `	SXUNUSED(apArg);` |
|     17 | 2758 | `	if( pRes == 0 ){` |
|      7 | 2759 | `		return rc;` |
|      - | 2760 | `	}` |
|     11 | 2761 | `	ph7_result_int64(pCtx,(ph7_int64)sqlite3_column_count(pRes->pSt->pStmt));` |
|     11 | 2762 | `	return PH7_OK;` |
|      9 | 2763 | `}` |
|      - | 2764 | `/* SQLite3Result::columnName(int $column): string\|false -- also from the` |
|      - | 2765 | ` * statement, so it answers with no row up. */` |
|      8 | 2766 | `static int vm_builtin_SQLite3Result_columnName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2767 | `{` |
|      - | 2768 | `	sxi32 rc;` |
|      9 | 2769 | `	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);` |
|      - | 2770 | `	int iCol;` |
|      - | 2771 | `	const char *zName;` |
|      4 | 2772 | `	SXUNUSED(nArg);` |
|      9 | 2773 | `	if( pRes == 0 ){` |
|    ! 0 | 2774 | `		return rc;` |
|      - | 2775 | `	}` |
|      9 | 2776 | `	iCol = (int)ph7_value_to_int64(apArg[0]);` |
|      - | 2777 | `	/* sqlite answers NULL for a column that is not there, and false is what php` |
|      - | 2778 | `	 * makes of that -- no range check of php's own. */` |
|      9 | 2779 | `	zName = sqlite3_column_name(pRes->pSt->pStmt,iCol);` |
|      9 | 2780 | `	if( zName == 0 ){` |
|      5 | 2781 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2782 | `		return PH7_OK;` |
|      - | 2783 | `	}` |
|      5 | 2784 | `	ph7_result_string(pCtx,zName,-1);` |
|      5 | 2785 | `	return PH7_OK;` |
|      5 | 2786 | `}` |
|      - | 2787 | `/*` |
|      - | 2788 | ` * SQLite3Result::columnType(int $column): int\|false` |
|      - | 2789 | ` *` |
|      - | 2790 | ` * A type belongs to a VALUE, not to a column, so this one answers only while a` |
|      - | 2791 | ` * row is UP: sqlite's data_count is the width of the row it is holding, and it` |
|      - | 2792 | ` * is 0 before the first fetch and 0 again once the walk has run out. The width` |
|      - | 2793 | ` * columnName() answers from does not move that way.` |
|      - | 2794 | ` *` |
|      - | 2795 | ` * There is no range check beyond that gate. A column past the row's width --` |
|      - | 2796 | ` * and a NEGATIVE one -- is SQLITE3_NULL rather than false, and asking leaves` |
|      - | 2797 | `` * `column index out of range` on the CONNECTION, which the same question to`` |
|      - | 2798 | ` * columnName() never does.` |
|      - | 2799 | ` */` |
|     18 | 2800 | `static int vm_builtin_SQLite3Result_columnType(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2801 | `{` |
|      - | 2802 | `	sxi32 rc;` |
|     19 | 2803 | `	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);` |
|      9 | 2804 | `	SXUNUSED(nArg);` |
|     19 | 2805 | `	if( pRes == 0 ){` |
|    ! 0 | 2806 | `		return rc;` |
|      - | 2807 | `	}` |
|     19 | 2808 | `	if( sqlite3_data_count(pRes->pSt->pStmt) < 1 ){` |
|      5 | 2809 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2810 | `		return PH7_OK;` |
|      - | 2811 | `	}` |
|     22 | 2812 | `	ph7_result_int64(pCtx,` |
|     14 | 2813 | `		(ph7_int64)sqlite3_column_type(pRes->pSt->pStmt,(int)ph7_value_to_int64(apArg[0])));` |
|     15 | 2814 | `	return PH7_OK;` |
|     10 | 2815 | `}` |
|      - | 2816 | `/*` |
|      - | 2817 | ` * One row into pOut, in the shape $mode asks for. The two bits are read` |
|      - | 2818 | ` * independently and both may be set, so a BOTH row carries each column twice --` |
|      - | 2819 | ` * position first, name second, column by column.` |
|      - | 2820 | ` */` |
|    112 | 2821 | `static void Sq3RowInto(sqlite3_stmt *pStmt,int iMode,ph7_value *pOut,ph7_value *pCell)` |
|      1 | 2822 | `{` |
|    113 | 2823 | `	int i,nCol = sqlite3_data_count(pStmt);` |
|    297 | 2824 | `	for( i = 0 ; i < nCol ; ++i ){` |
|    185 | 2825 | `		Sq3ColumnValue(pStmt,i,pCell);` |
|    185 | 2826 | `		if( iMode & 2 /* SQLITE3_NUM */ ){` |
|    165 | 2827 | `			ph7_array_add_intkey_elem(pOut,i,pCell);` |
|     82 | 2828 | `		}` |
|    185 | 2829 | `		if( iMode & 1 /* SQLITE3_ASSOC */ ){` |
|     53 | 2830 | `			ph7_array_add_strkey_elem(pOut,sqlite3_column_name(pStmt,i),pCell);` |
|     26 | 2831 | `		}` |
|     93 | 2832 | `	}` |
|    113 | 2833 | `}` |
|      - | 2834 | `/*` |
|      - | 2835 | ` * Step once for a script. Nothing latches a finished flag and nothing resets:` |
|      - | 2836 | ` * a statement prepared with prepare_v2 rewinds ITSELF when it is stepped after` |
|      - | 2837 | ` * SQLITE_DONE, which is why a walk that has run out starts over on the next` |
|      - | 2838 | ` * call and why fetchAll() may be asked twice.` |
|      - | 2839 | ` *` |
|      - | 2840 | ` * Leaving the reset out is visible from the other side of the connection: the` |
|      - | 2841 | ` * end of a walk is SQLITE_DONE and stays on the handle, so lastErrorCode()` |
|      - | 2842 | `` * reads 101 and lastErrorMsg() `no more rows available` after the fetch that`` |
|      - | 2843 | ` * ran out. An explicit reset would clear both back to "not an error".` |
|      - | 2844 | ` *` |
|      - | 2845 | ` * Answers 1 for a row, 0 for the end, -1 for a failure already reported.` |
|      - | 2846 | ` */` |
|    138 | 2847 | `static int Sq3StepFetch(ph7_context *pCtx,phl_sq3_res *pRes,const char *zFn,sxi32 *pRc)` |
|      1 | 2848 | `{` |
|    139 | 2849 | `	int rc = sqlite3_step(pRes->pSt->pStmt);` |
|    139 | 2850 | `	*pRc = PH7_OK;` |
|    139 | 2851 | `	if( rc == SQLITE_ROW ){` |
|    113 | 2852 | `		return 1;` |
|      - | 2853 | `	}` |
|     27 | 2854 | `	if( rc == SQLITE_DONE ){` |
|     27 | 2855 | `		return 0;` |
|      - | 2856 | `	}` |
|      - | 2857 | `	{` |
|      - | 2858 | `		SyBlob sMsg;` |
|    ! 0 | 2859 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    ! 0 | 2860 | `		SyBlobFormat(&sMsg,"Unable to execute statement: %s",` |
|    ! 0 | 2861 | `			sqlite3_errmsg(pRes->pSt->pConn->pDb));` |
|    ! 0 | 2862 | `		SyBlobNullAppend(&sMsg);` |
|    ! 0 | 2863 | `		*pRc = Sq3Error(pCtx,pRes->pSt->pConn,zFn,sqlite3_errcode(pRes->pSt->pConn->pDb),` |
|    ! 0 | 2864 | `			(const char *)SyBlobData(&sMsg));` |
|    ! 0 | 2865 | `		SyBlobRelease(&sMsg);` |
|      - | 2866 | `	}` |
|    ! 0 | 2867 | `	return -1;` |
|     70 | 2868 | `}` |
|      - | 2869 | `/* SQLite3Result::fetchArray(int $mode = SQLITE3_BOTH): array\|false */` |
|     88 | 2870 | `static int vm_builtin_SQLite3Result_fetchArray(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2871 | `{` |
|      - | 2872 | `	sxi32 rc;` |
|     89 | 2873 | `	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);` |
|     89 | 2874 | `	int iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : 3;` |
|      - | 2875 | `	ph7_value *pRow,*pCell;` |
|      - | 2876 | `	Sq3Verb sVerb;` |
|      - | 2877 | `	int iStep;` |
|     89 | 2878 | `	if( pRes == 0 ){` |
|      9 | 2879 | `		return rc;` |
|      - | 2880 | `	}` |
|     81 | 2881 | `	Sq3VerbEnter(pRes->pSt->pConn,pCtx,"SQLite3Result::fetchArray",&sVerb);` |
|     81 | 2882 | `	iStep = Sq3StepFetch(pCtx,pRes,"SQLite3Result::fetchArray",&rc);` |
|      - | 2883 | `	{` |
|     81 | 2884 | `		sxi32 rcCb = Sq3VerbLeave(pRes->pSt->pConn,&sVerb);` |
|     81 | 2885 | `		if( rcCb != PH7_OK ){` |
|    ! 0 | 2886 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2887 | `			return rcCb;` |
|      - | 2888 | `		}` |
|      - | 2889 | `	}` |
|     81 | 2890 | `	if( iStep != 1 ){` |
|      9 | 2891 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2892 | `		return rc;` |
|      - | 2893 | `	}` |
|     73 | 2894 | `	pRow = ph7_context_new_array(pCtx);` |
|     73 | 2895 | `	pCell = ph7_context_new_scalar(pCtx);` |
|     73 | 2896 | `	if( pRow == 0 \|\| pCell == 0 ){` |
|    ! 0 | 2897 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2898 | `	}` |
|     73 | 2899 | `	Sq3RowInto(pRes->pSt->pStmt,iMode,pRow,pCell);` |
|     73 | 2900 | `	ph7_result_value(pCtx,pRow);` |
|     73 | 2901 | `	return PH7_OK;` |
|     45 | 2902 | `}` |
|      - | 2903 | `/*` |
|      - | 2904 | ` * SQLite3Result::fetchAll(int $mode = SQLITE3_BOTH): array\|false` |
|      - | 2905 | ` *` |
|      - | 2906 | ` * The rows still AHEAD of the cursor, not all the rows: a walk already` |
|      - | 2907 | ` * underway hands back what is left of it. The end rewinds the statement the` |
|      - | 2908 | ` * way a single fetch does, so a second fetchAll() answers the whole set again.` |
|      - | 2909 | ` */` |
|     18 | 2910 | `static int vm_builtin_SQLite3Result_fetchAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2911 | `{` |
|      - | 2912 | `	sxi32 rc;` |
|     19 | 2913 | `	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);` |
|     19 | 2914 | `	int iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : 3;` |
|      - | 2915 | `	ph7_value *pAll,*pRow,*pCell;` |
|      - | 2916 | `	Sq3Verb sVerb;` |
|      - | 2917 | `	int iStep;` |
|     19 | 2918 | `	if( pRes == 0 ){` |
|    ! 0 | 2919 | `		return rc;` |
|      - | 2920 | `	}` |
|     19 | 2921 | `	pAll = ph7_context_new_array(pCtx);` |
|     19 | 2922 | `	pCell = ph7_context_new_scalar(pCtx);` |
|     19 | 2923 | `	if( pAll == 0 \|\| pCell == 0 ){` |
|    ! 0 | 2924 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2925 | `	}` |
|     19 | 2926 | `	Sq3VerbEnter(pRes->pSt->pConn,pCtx,"SQLite3Result::fetchAll",&sVerb);` |
|     29 | 2927 | `	for(;;){` |
|     59 | 2928 | `		iStep = Sq3StepFetch(pCtx,pRes,"SQLite3Result::fetchAll",&rc);` |
|     59 | 2929 | `		if( iStep != 1 \|\| pRes->pSt->pConn->iCallbackExc != 0 ){` |
|     10 | 2930 | `			break;` |
|      - | 2931 | `		}` |
|     41 | 2932 | `		pRow = ph7_context_new_array(pCtx);` |
|     41 | 2933 | `		if( pRow == 0 ){` |
|    ! 0 | 2934 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2935 | `		}` |
|     41 | 2936 | `		Sq3RowInto(pRes->pSt->pStmt,iMode,pRow,pCell);` |
|     41 | 2937 | `		ph7_array_add_elem(pAll,0,pRow);` |
|     41 | 2938 | `		ph7_context_release_value(pCtx,pRow);` |
|      1 | 2939 | `	}` |
|      - | 2940 | `	{` |
|     19 | 2941 | `		sxi32 rcCb = Sq3VerbLeave(pRes->pSt->pConn,&sVerb);` |
|     19 | 2942 | `		if( rcCb != PH7_OK ){` |
|    ! 0 | 2943 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2944 | `			return rcCb;` |
|      - | 2945 | `		}` |
|      - | 2946 | `	}` |
|     19 | 2947 | `	if( iStep < 0 ){` |
|    ! 0 | 2948 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2949 | `		return rc;` |
|      - | 2950 | `	}` |
|     19 | 2951 | `	ph7_result_value(pCtx,pAll);` |
|     19 | 2952 | `	return PH7_OK;` |
|     10 | 2953 | `}` |
|      - | 2954 | `/* SQLite3Result::reset(): bool -- rewind, so the next fetch is the first row. */` |
|      2 | 2955 | `static int vm_builtin_SQLite3Result_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2956 | `{` |
|      - | 2957 | `	sxi32 rc;` |
|      3 | 2958 | `	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);` |
|      1 | 2959 | `	SXUNUSED(nArg);` |
|      1 | 2960 | `	SXUNUSED(apArg);` |
|      3 | 2961 | `	if( pRes == 0 ){` |
|    ! 0 | 2962 | `		return rc;` |
|      - | 2963 | `	}` |
|      3 | 2964 | `	ph7_result_bool(pCtx,sqlite3_reset(pRes->pSt->pStmt) == SQLITE_OK);` |
|      3 | 2965 | `	return PH7_OK;` |
|      2 | 2966 | `}` |
|      - | 2967 | `/*` |
|      - | 2968 | ` * SQLite3Result::finalize(): true` |
|      - | 2969 | ` *` |
|      - | 2970 | ` * Not a close of the statement -- a release of THIS result's hold on it. The` |
|      - | 2971 | ` * statement query() built has no other holder, so it is finalized here; one a` |
|      - | 2972 | ` * script prepared itself is still runnable afterwards. Either way the result` |
|      - | 2973 | ` * itself is spent, and a second finalize() is the Error rather than a second` |
|      - | 2974 | ` * true.` |
|      - | 2975 | ` */` |
|      6 | 2976 | `static int vm_builtin_SQLite3Result_finalize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2977 | `{` |
|      - | 2978 | `	sxi32 rc;` |
|      7 | 2979 | `	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);` |
|      3 | 2980 | `	SXUNUSED(nArg);` |
|      3 | 2981 | `	SXUNUSED(apArg);` |
|      7 | 2982 | `	if( pRes == 0 ){` |
|      3 | 2983 | `		return rc;` |
|      - | 2984 | `	}` |
|      5 | 2985 | `	Sq3ResDetach(pRes);` |
|      5 | 2986 | `	ph7_result_bool(pCtx,1);` |
|      5 | 2987 | `	return PH7_OK;` |
|      4 | 2988 | `}` |
|      - | 2989 |  |
|      - | 2990 | `/* ------------------------------------------------------------------------` |
|      - | 2991 | ` * Small readers and switches` |
|      - | 2992 | ` * ------------------------------------------------------------------------ */` |
|      - | 2993 | `/* SQLite3::lastInsertRowID(): int */` |
|      2 | 2994 | `static int vm_builtin_SQLite3_lastInsertRowID(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2995 | `{` |
|      - | 2996 | `	sxi32 rc;` |
|      3 | 2997 | `	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);` |
|      1 | 2998 | `	SXUNUSED(nArg);` |
|      1 | 2999 | `	SXUNUSED(apArg);` |
|      3 | 3000 | `	if( pConn == 0 ){` |
|    ! 0 | 3001 | `		return rc;` |
|      - | 3002 | `	}` |
|      3 | 3003 | `	ph7_result_int64(pCtx,(ph7_int64)sqlite3_last_insert_rowid(pConn->pDb));` |
|      3 | 3004 | `	return PH7_OK;` |
|      2 | 3005 | `}` |
|      - | 3006 | `/*` |
|      - | 3007 | ` * SQLite3::changes(): int` |
|      - | 3008 | ` *` |
|      - | 3009 | ` * sqlite's own counter, which only a write moves: a SELECT, a CREATE or a` |
|      - | 3010 | ` * statement that matched nothing leaves the PREVIOUS write's count standing.` |
|      - | 3011 | ` */` |
|      6 | 3012 | `static int vm_builtin_SQLite3_changes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3013 | `{` |
|      - | 3014 | `	sxi32 rc;` |
|      7 | 3015 | `	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);` |
|      3 | 3016 | `	SXUNUSED(nArg);` |
|      3 | 3017 | `	SXUNUSED(apArg);` |
|      7 | 3018 | `	if( pConn == 0 ){` |
|      3 | 3019 | `		return rc;` |
|      - | 3020 | `	}` |
|      5 | 3021 | `	ph7_result_int64(pCtx,(ph7_int64)sqlite3_changes(pConn->pDb));` |
|      5 | 3022 | `	return PH7_OK;` |
|      4 | 3023 | `}` |
|      - | 3024 | `/*` |
|      - | 3025 | ` * SQLite3::busyTimeout(int $milliseconds): bool` |
|      - | 3026 | ` *` |
|      - | 3027 | `` * The argument reaches sqlite as a C `int`, so a value past that width is`` |
|      - | 3028 | ` * truncated on the way -- php's own narrowing, not a check.` |
|      - | 3029 | ` */` |
|      4 | 3030 | `static int vm_builtin_SQLite3_busyTimeout(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3031 | `{` |
|      - | 3032 | `	sxi32 rc;` |
|      5 | 3033 | `	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);` |
|      2 | 3034 | `	SXUNUSED(nArg);` |
|      5 | 3035 | `	if( pConn == 0 ){` |
|      3 | 3036 | `		return rc;` |
|      - | 3037 | `	}` |
|      4 | 3038 | `	ph7_result_bool(pCtx,` |
|      2 | 3039 | `		sqlite3_busy_timeout(pConn->pDb,(int)ph7_value_to_int64(apArg[0])) == SQLITE_OK);` |
|      3 | 3040 | `	return PH7_OK;` |
|      3 | 3041 | `}` |
|      - | 3042 | `/*` |
|      - | 3043 | ` * SQLite3::enableExceptions(bool $enable = false): bool` |
|      - | 3044 | ` *` |
|      - | 3045 | ` * Answers the PREVIOUS setting, and asks nothing about the connection's state` |
|      - | 3046 | ` * -- it is settable on an object that was never opened and on a closed one.` |
|      - | 3047 | ` * php 8.3 deprecated the warning mode itself, so asking for it says so, every` |
|      - | 3048 | ` * time and whatever the setting already was.` |
|      - | 3049 | ` */` |
|     12 | 3050 | `static int vm_builtin_SQLite3_enableExceptions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3051 | `{` |
|     13 | 3052 | `	phl_sq3 *pConn = Sq3Bind(pCtx);` |
|     13 | 3053 | `	int bEnable = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|     13 | 3054 | `	if( pConn == 0 ){` |
|    ! 0 | 3055 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3056 | `	}` |
|     13 | 3057 | `	if( !bEnable ){` |
|      5 | 3058 | `		VmErrorFormat(pCtx->pVm,8192 /* E_DEPRECATED */,` |
|      - | 3059 | `			"SQLite3::enableExceptions(): Use of warnings for SQLite3 is deprecated");` |
|      2 | 3060 | `	}` |
|     13 | 3061 | `	ph7_result_bool(pCtx,pConn->bExceptions);` |
|     13 | 3062 | `	pConn->bExceptions = bEnable;` |
|     13 | 3063 | `	return PH7_OK;` |
|      7 | 3064 | `}` |
|      - | 3065 | `/*` |
|      - | 3066 | ` * SQLite3::enableExtendedResultCodes(bool $enable = true): bool` |
|      - | 3067 | ` *` |
|      - | 3068 | ` * Answers whether the LIBRARY took the switch -- so it splits the two states` |
|      - | 3069 | ` * the way the error readers do rather than the way enableExceptions() does: a` |
|      - | 3070 | ` * never-opened object is the Error, and a CLOSED one is a plain false, because` |
|      - | 3071 | ` * there is no handle left to tell.` |
|      - | 3072 | ` */` |
|      8 | 3073 | `static int vm_builtin_SQLite3_enableExtendedResultCodes(ph7_context *pCtx,int nArg,` |
|      - | 3074 | `	ph7_value **apArg)` |
|      1 | 3075 | `{` |
|      - | 3076 | `	sxi32 rc;` |
|      9 | 3077 | `	phl_sq3 *pConn = Sq3ErrReader(pCtx,&rc);` |
|      9 | 3078 | `	int bEnable = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 1;` |
|      9 | 3079 | `	if( pConn == 0 ){` |
|      3 | 3080 | `		return rc;` |
|      - | 3081 | `	}` |
|      7 | 3082 | `	if( pConn->pDb == 0 ){` |
|      3 | 3083 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3084 | `		return PH7_OK;` |
|      - | 3085 | `	}` |
|      7 | 3086 | `	ph7_result_bool(pCtx,` |
|      4 | 3087 | `		sqlite3_extended_result_codes(pConn->pDb,bEnable) == SQLITE_OK);` |
|      5 | 3088 | `	return PH7_OK;` |
|      5 | 3089 | `}` |
|      - | 3090 | `/*` |
|      - | 3091 | ` * SQLite3::loadExtension(string $name): bool` |
|      - | 3092 | ` *` |
|      - | 3093 | ` * Three refusals in php's order, and the DIRECTORY decides the first: an empty` |
|      - | 3094 | `` * `sqlite3.extension_dir` means the door is shut and nothing else is asked, so`` |
|      - | 3095 | ` * the name is not even looked at. With a directory set, the name is joined to` |
|      - | 3096 | ` * it -- always with a separator and never as an absolute path of its own, so a` |
|      - | 3097 | `` * name starting with `/` lands under the directory twice over -- and the empty`` |
|      - | 3098 | ` * name is the ValueError that only a set directory can reach.` |
|      - | 3099 | ` */` |
|      4 | 3100 | `static int vm_builtin_SQLite3_loadExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3101 | `{` |
|      - | 3102 | `	sxi32 rc;` |
|      5 | 3103 | `	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);` |
|      - | 3104 | `	const char *zName;` |
|      5 | 3105 | `	int nName = 0;` |
|      - | 3106 | `	SyBlob sDir,sPath;` |
|      5 | 3107 | `	char *zErr = 0;` |
|      - | 3108 | `	int rcLoad;` |
|      2 | 3109 | `	SXUNUSED(nArg);` |
|      5 | 3110 | `	if( pConn == 0 ){` |
|    ! 0 | 3111 | `		return rc;` |
|      - | 3112 | `	}` |
|      5 | 3113 | `	SyBlobInit(&sDir,&pCtx->pVm->sAllocator);` |
|      5 | 3114 | `	PH7_VmIniGetStr(pCtx->pVm,"sqlite3.extension_dir",&sDir);` |
|      5 | 3115 | `	if( SyBlobLength(&sDir) < 1 ){` |
|      5 | 3116 | `		SyBlobRelease(&sDir);` |
|      5 | 3117 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3118 | `		return Sq3Error(pCtx,pConn,"SQLite3::loadExtension",0,` |
|      - | 3119 | `			"SQLite Extensions are disabled");` |
|      - | 3120 | `	}` |
|    ! 0 | 3121 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    ! 0 | 3122 | `	if( nName < 1 ){` |
|    ! 0 | 3123 | `		SyBlobRelease(&sDir);` |
|    ! 0 | 3124 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3125 | `			"SQLite3::loadExtension(): Argument #1 ($name) must not be empty");` |
|      - | 3126 | `	}` |
|    ! 0 | 3127 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|    ! 0 | 3128 | `	SyBlobAppend(&sPath,SyBlobData(&sDir),SyBlobLength(&sDir));` |
|    ! 0 | 3129 | `	SyBlobAppend(&sPath,"/",1);` |
|    ! 0 | 3130 | `	SyBlobAppend(&sPath,zName,(sxu32)nName);` |
|    ! 0 | 3131 | `	SyBlobAppend(&sPath,"",1);` |
|    ! 0 | 3132 | `	SyBlobRelease(&sDir);` |
|    ! 0 | 3133 | `	sqlite3_enable_load_extension(pConn->pDb,1);` |
|    ! 0 | 3134 | `	rcLoad = sqlite3_load_extension(pConn->pDb,(const char *)SyBlobData(&sPath),0,&zErr);` |
|    ! 0 | 3135 | `	sqlite3_enable_load_extension(pConn->pDb,0);` |
|    ! 0 | 3136 | `	if( zErr ){` |
|    ! 0 | 3137 | `		sqlite3_free(zErr);` |
|    ! 0 | 3138 | `	}` |
|    ! 0 | 3139 | `	if( rcLoad != SQLITE_OK ){` |
|      - | 3140 | `		SyBlob sMsg;` |
|      - | 3141 | `		sxi32 rcErr;` |
|    ! 0 | 3142 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|      - | 3143 | `		/* php reports the path it BUILT, not the name the script wrote, and the` |
|      - | 3144 | `		 * path is a C string by then -- so a name carrying a NUL is reported cut` |
|      - | 3145 | `		 * at it. */` |
|    ! 0 | 3146 | `		SyBlobFormat(&sMsg,"Unable to load extension at '%s'",` |
|    ! 0 | 3147 | `			(const char *)SyBlobData(&sPath));` |
|    ! 0 | 3148 | `		SyBlobAppend(&sMsg,"",1);` |
|    ! 0 | 3149 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3150 | `		rcErr = Sq3Error(pCtx,pConn,"SQLite3::loadExtension",0,` |
|    ! 0 | 3151 | `			(const char *)SyBlobData(&sMsg));` |
|    ! 0 | 3152 | `		SyBlobRelease(&sMsg);` |
|    ! 0 | 3153 | `		SyBlobRelease(&sPath);` |
|    ! 0 | 3154 | `		return rcErr;` |
|      - | 3155 | `	}` |
|    ! 0 | 3156 | `	SyBlobRelease(&sPath);` |
|    ! 0 | 3157 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 3158 | `	return PH7_OK;` |
|      3 | 3159 | `}` |
|      - | 3160 |  |
|      - | 3161 | `/* ------------------------------------------------------------------------` |
|      - | 3162 | ` * Installation` |
|      - | 3163 | ` * ------------------------------------------------------------------------ */` |
|      - | 3164 | `/* The twelve constants ext/sqlite3 declares globally. The type and open codes` |
|      - | 3165 | ` * are sqlite's own macros -- they belong to the library, so they are read from` |
|      - | 3166 | ` * its header rather than copied; ASSOC/NUM/BOTH are php's own numbering for a` |
|      - | 3167 | ` * fetch mode the library knows nothing about. */` |
|      - | 3168 | `static const struct Sq3Constant {` |
|      - | 3169 | `	const char *zName;` |
|      - | 3170 | `	sxi64 iValue;` |
|      - | 3171 | `} aSq3Const[] = {` |
|      - | 3172 | `	{ "SQLITE3_ASSOC",          1 },` |
|      - | 3173 | `	{ "SQLITE3_NUM",            2 },` |
|      - | 3174 | `	{ "SQLITE3_BOTH",           3 },` |
|      - | 3175 | `	{ "SQLITE3_INTEGER",        SQLITE_INTEGER },` |
|      - | 3176 | `	{ "SQLITE3_FLOAT",          SQLITE_FLOAT },` |
|      - | 3177 | `	{ "SQLITE3_TEXT",           SQLITE3_TEXT },` |
|      - | 3178 | `	{ "SQLITE3_BLOB",           SQLITE_BLOB },` |
|      - | 3179 | `	{ "SQLITE3_NULL",           SQLITE_NULL },` |
|      - | 3180 | `	{ "SQLITE3_OPEN_READONLY",  SQLITE_OPEN_READONLY },` |
|      - | 3181 | `	{ "SQLITE3_OPEN_READWRITE", SQLITE_OPEN_READWRITE },` |
|      - | 3182 | `	{ "SQLITE3_OPEN_CREATE",    SQLITE_OPEN_CREATE },` |
|      - | 3183 | `	{ "SQLITE3_DETERMINISTIC",  SQLITE_DETERMINISTIC },` |
|      - | 3184 | `};` |
|    930 | 3185 | `static void Sq3ConstExpand(ph7_value *pVal,void *pUserData)` |
|      4 | 3186 | `{` |
|    934 | 3187 | `	ph7_value_int64(pVal,((const struct Sq3Constant *)pUserData)->iValue);` |
|    934 | 3188 | `}` |
|   6721 | 3189 | `PH7_PRIVATE void PH7_RegisterSqlite3Constants(ph7_vm *pVm)` |
|      5 | 3190 | `{` |
|      - | 3191 | `	sxu32 n;` |
|  87378 | 3192 | `	for( n = 0 ; n < SX_ARRAYSIZE(aSq3Const) ; ++n ){` |
| 120929 | 3193 | `		ph7_create_constant(&(*pVm),aSq3Const[n].zName,Sq3ConstExpand,` |
|  80652 | 3194 | `			(void *)&aSq3Const[n]);` |
|  40277 | 3195 | `	}` |
|   6726 | 3196 | `}` |
|   6721 | 3197 | `PH7_PRIVATE sxi32 PH7_VmInstallSqlite3(ph7_vm *pVm)` |
|      5 | 3198 | `{` |
|      - | 3199 | `#define SQ3_INT_CONST(NAME,VALUE) \` |
|      - | 3200 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (ph7_int64)(VALUE), 0, 0.0 }` |
|      - | 3201 | `	/*` |
|      - | 3202 | `	 * The authorizer's vocabulary: three verdicts and the thirty-three actions` |
|      - | 3203 | `	 * a callback is told about, in php's own declaration order -- which is not` |
|      - | 3204 | ``	 * numeric order, since `COPY` (0, and retired by sqlite long ago) sits after`` |
|      - | 3205 | ``	 * `SAVEPOINT` and `RECURSIVE` closes the list. Every value is the library's`` |
|      - | 3206 | `	 * macro. The callback that reads them arrives with setAuthorizer().` |
|      - | 3207 | `	 */` |
|      - | 3208 | `	static const PH7_NativeConstDef aSq3ClassConst[] = {` |
|      - | 3209 | `		SQ3_INT_CONST("OK",                  SQLITE_OK),` |
|      - | 3210 | `		SQ3_INT_CONST("DENY",                SQLITE_DENY),` |
|      - | 3211 | `		SQ3_INT_CONST("IGNORE",              SQLITE_IGNORE),` |
|      - | 3212 | `		SQ3_INT_CONST("CREATE_INDEX",        SQLITE_CREATE_INDEX),` |
|      - | 3213 | `		SQ3_INT_CONST("CREATE_TABLE",        SQLITE_CREATE_TABLE),` |
|      - | 3214 | `		SQ3_INT_CONST("CREATE_TEMP_INDEX",   SQLITE_CREATE_TEMP_INDEX),` |
|      - | 3215 | `		SQ3_INT_CONST("CREATE_TEMP_TABLE",   SQLITE_CREATE_TEMP_TABLE),` |
|      - | 3216 | `		SQ3_INT_CONST("CREATE_TEMP_TRIGGER", SQLITE_CREATE_TEMP_TRIGGER),` |
|      - | 3217 | `		SQ3_INT_CONST("CREATE_TEMP_VIEW",    SQLITE_CREATE_TEMP_VIEW),` |
|      - | 3218 | `		SQ3_INT_CONST("CREATE_TRIGGER",      SQLITE_CREATE_TRIGGER),` |
|      - | 3219 | `		SQ3_INT_CONST("CREATE_VIEW",         SQLITE_CREATE_VIEW),` |
|      - | 3220 | `		SQ3_INT_CONST("DELETE",              SQLITE_DELETE),` |
|      - | 3221 | `		SQ3_INT_CONST("DROP_INDEX",          SQLITE_DROP_INDEX),` |
|      - | 3222 | `		SQ3_INT_CONST("DROP_TABLE",          SQLITE_DROP_TABLE),` |
|      - | 3223 | `		SQ3_INT_CONST("DROP_TEMP_INDEX",     SQLITE_DROP_TEMP_INDEX),` |
|      - | 3224 | `		SQ3_INT_CONST("DROP_TEMP_TABLE",     SQLITE_DROP_TEMP_TABLE),` |
|      - | 3225 | `		SQ3_INT_CONST("DROP_TEMP_TRIGGER",   SQLITE_DROP_TEMP_TRIGGER),` |
|      - | 3226 | `		SQ3_INT_CONST("DROP_TEMP_VIEW",      SQLITE_DROP_TEMP_VIEW),` |
|      - | 3227 | `		SQ3_INT_CONST("DROP_TRIGGER",        SQLITE_DROP_TRIGGER),` |
|      - | 3228 | `		SQ3_INT_CONST("DROP_VIEW",           SQLITE_DROP_VIEW),` |
|      - | 3229 | `		SQ3_INT_CONST("INSERT",              SQLITE_INSERT),` |
|      - | 3230 | `		SQ3_INT_CONST("PRAGMA",              SQLITE_PRAGMA),` |
|      - | 3231 | `		SQ3_INT_CONST("READ",                SQLITE_READ),` |
|      - | 3232 | `		SQ3_INT_CONST("SELECT",              SQLITE_SELECT),` |
|      - | 3233 | `		SQ3_INT_CONST("TRANSACTION",         SQLITE_TRANSACTION),` |
|      - | 3234 | `		SQ3_INT_CONST("UPDATE",              SQLITE_UPDATE),` |
|      - | 3235 | `		SQ3_INT_CONST("ATTACH",              SQLITE_ATTACH),` |
|      - | 3236 | `		SQ3_INT_CONST("DETACH",              SQLITE_DETACH),` |
|      - | 3237 | `		SQ3_INT_CONST("ALTER_TABLE",         SQLITE_ALTER_TABLE),` |
|      - | 3238 | `		SQ3_INT_CONST("REINDEX",             SQLITE_REINDEX),` |
|      - | 3239 | `		SQ3_INT_CONST("ANALYZE",             SQLITE_ANALYZE),` |
|      - | 3240 | `		SQ3_INT_CONST("CREATE_VTABLE",       SQLITE_CREATE_VTABLE),` |
|      - | 3241 | `		SQ3_INT_CONST("DROP_VTABLE",         SQLITE_DROP_VTABLE),` |
|      - | 3242 | `		SQ3_INT_CONST("FUNCTION",            SQLITE_FUNCTION),` |
|      - | 3243 | `		SQ3_INT_CONST("SAVEPOINT",           SQLITE_SAVEPOINT),` |
|      - | 3244 | `		SQ3_INT_CONST("COPY",                SQLITE_COPY),` |
|      - | 3245 | `		SQ3_INT_CONST("RECURSIVE",           SQLITE_RECURSIVE),` |
|      - | 3246 | `	};` |
|      - | 3247 | `	/*` |
|      - | 3248 | `	 * php's declaration ORDER, which get_class_methods() and Reflection both` |
|      - | 3249 | `	 * answer in. The two static verbs sit among the instance ones rather than` |
|      - | 3250 | `	 * being grouped, and the slices still to come keep their places.` |
|      - | 3251 | `	 */` |
|      - | 3252 | `	static const PH7_NativeMethodDef aSq3Method[] = {` |
|      - | 3253 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|      - | 3254 | `		  "string $filename, int $flags = SQLITE3_OPEN_READWRITE \| SQLITE3_OPEN_CREATE, "` |
|      - | 3255 | `		  "string $encryptionKey = ''", 0, vm_builtin_SQLite3_construct },` |
|      - | 3256 | `		{ "open", PH7_MOD_PUBLIC,` |
|      - | 3257 | `		  "string $filename, int $flags = SQLITE3_OPEN_READWRITE \| SQLITE3_OPEN_CREATE, "` |
|      - | 3258 | `		  "string $encryptionKey = ''", "@void", vm_builtin_SQLite3_open },` |
|      - | 3259 | `		{ "close", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SQLite3_close },` |
|      - | 3260 | `		{ "version", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "@array",` |
|      - | 3261 | `		  vm_builtin_SQLite3_version },` |
|      - | 3262 | `		{ "lastInsertRowID", PH7_MOD_PUBLIC, "", "@int",` |
|      - | 3263 | `		  vm_builtin_SQLite3_lastInsertRowID },` |
|      - | 3264 | `		{ "lastErrorCode", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SQLite3_lastErrorCode },` |
|      - | 3265 | `		{ "lastExtendedErrorCode", PH7_MOD_PUBLIC, "", "@int",` |
|      - | 3266 | `		  vm_builtin_SQLite3_lastExtendedErrorCode },` |
|      - | 3267 | `		{ "lastErrorMsg", PH7_MOD_PUBLIC, "", "@string", vm_builtin_SQLite3_lastErrorMsg },` |
|      - | 3268 | `		{ "changes", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SQLite3_changes },` |
|      - | 3269 | `		{ "busyTimeout", PH7_MOD_PUBLIC, "int $milliseconds", "@bool",` |
|      - | 3270 | `		  vm_builtin_SQLite3_busyTimeout },` |
|      - | 3271 | `		{ "loadExtension", PH7_MOD_PUBLIC, "string $name", "@bool",` |
|      - | 3272 | `		  vm_builtin_SQLite3_loadExtension },` |
|      - | 3273 | `		{ "backup", PH7_MOD_PUBLIC,` |
|      - | 3274 | `		  "SQLite3 $destination, string $sourceDatabase = 'main', "` |
|      - | 3275 | `		  "string $destinationDatabase = 'main'", "@bool", vm_builtin_SQLite3_backup },` |
|      - | 3276 | `		{ "escapeString", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $string", "@string",` |
|      - | 3277 | `		  vm_builtin_SQLite3_escapeString },` |
|      - | 3278 | `		{ "prepare", PH7_MOD_PUBLIC, "string $query", "@SQLite3Stmt\|false",` |
|      - | 3279 | `		  vm_builtin_SQLite3_prepare },` |
|      - | 3280 | `		{ "exec", PH7_MOD_PUBLIC, "string $query", "@bool", vm_builtin_SQLite3_exec },` |
|      - | 3281 | `		{ "query", PH7_MOD_PUBLIC, "string $query", "@SQLite3Result\|false",` |
|      - | 3282 | `		  vm_builtin_SQLite3_query },` |
|      - | 3283 | `		{ "querySingle", PH7_MOD_PUBLIC, "string $query, bool $entireRow = false", "@mixed",` |
|      - | 3284 | `		  vm_builtin_SQLite3_querySingle },` |
|      - | 3285 | `		{ "createFunction", PH7_MOD_PUBLIC,` |
|      - | 3286 | `		  "string $name, callable $callback, int $argCount = -1, int $flags = 0", "@bool",` |
|      - | 3287 | `		  vm_builtin_SQLite3_createFunction },` |
|      - | 3288 | `		{ "createAggregate", PH7_MOD_PUBLIC,` |
|      - | 3289 | `		  "string $name, callable $stepCallback, callable $finalCallback, "` |
|      - | 3290 | `		  "int $argCount = -1", "@bool", vm_builtin_SQLite3_createAggregate },` |
|      - | 3291 | `		{ "createCollation", PH7_MOD_PUBLIC, "string $name, callable $callback", "@bool",` |
|      - | 3292 | `		  vm_builtin_SQLite3_createCollation },` |
|      - | 3293 | `		{ "openBlob", PH7_MOD_PUBLIC,` |
|      - | 3294 | `		  "string $table, string $column, int $rowid, string $database = 'main', "` |
|      - | 3295 | `		  "int $flags = SQLITE3_OPEN_READONLY", 0, vm_builtin_SQLite3_openBlob },` |
|      - | 3296 | `		{ "enableExceptions", PH7_MOD_PUBLIC, "bool $enable = false", "@bool",` |
|      - | 3297 | `		  vm_builtin_SQLite3_enableExceptions },` |
|      - | 3298 | `		{ "enableExtendedResultCodes", PH7_MOD_PUBLIC, "bool $enable = true", "@bool",` |
|      - | 3299 | `		  vm_builtin_SQLite3_enableExtendedResultCodes },` |
|      - | 3300 | `		{ "setAuthorizer", PH7_MOD_PUBLIC, "?callable $callback", "@bool",` |
|      - | 3301 | `		  vm_builtin_SQLite3_setAuthorizer },` |
|      - | 3302 | `	};` |
|      - | 3303 | `	/* The handle: storage the class owns and never presents -- php shows no` |
|      - | 3304 | `	 * property at all on a SQLite3, so the slot is hidden. */` |
|      - | 3305 | `	static const PH7_NativePropDef aSq3Prop[] = {` |
|      - | 3306 | `		{ SQ3_RES, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|      - | 3307 | `		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 3308 | `	};` |
|      - | 3309 | `	/*` |
|      - | 3310 | `	 * php's cursor. Its constructor is PRIVATE and takes nothing: only the` |
|      - | 3311 | `	 * extension ever builds one, and a script that tries is refused by the` |
|      - | 3312 | `	 * engine's own visibility rule rather than by a body.` |
|      - | 3313 | `	 */` |
|      - | 3314 | `	static const PH7_NativeMethodDef aSq3ResMethod[] = {` |
|      - | 3315 | `		{ "__construct", PH7_MOD_PRIVATE, "", 0, vm_builtin_SQLite3Result_construct },` |
|      - | 3316 | `		{ "numColumns", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SQLite3Result_numColumns },` |
|      - | 3317 | `		{ "columnName", PH7_MOD_PUBLIC, "int $column", "@string\|false",` |
|      - | 3318 | `		  vm_builtin_SQLite3Result_columnName },` |
|      - | 3319 | `		{ "columnType", PH7_MOD_PUBLIC, "int $column", "@int\|false",` |
|      - | 3320 | `		  vm_builtin_SQLite3Result_columnType },` |
|      - | 3321 | `		{ "fetchArray", PH7_MOD_PUBLIC, "int $mode = SQLITE3_BOTH", "@array\|false",` |
|      - | 3322 | `		  vm_builtin_SQLite3Result_fetchArray },` |
|      - | 3323 | `		{ "fetchAll", PH7_MOD_PUBLIC, "int $mode = SQLITE3_BOTH", "array\|false",` |
|      - | 3324 | `		  vm_builtin_SQLite3Result_fetchAll },` |
|      - | 3325 | `		{ "reset", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SQLite3Result_reset },` |
|      - | 3326 | `		{ "finalize", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SQLite3Result_finalize },` |
|      - | 3327 | `	};` |
|      - | 3328 | `	static const PH7_NativePropDef aSq3ResProp[] = {` |
|      - | 3329 | `		{ SQ3_RES, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|      - | 3330 | `		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 3331 | `	};` |
|      - | 3332 | `	/*` |
|      - | 3333 | `	 * php's own prepared statement. Its constructor is PRIVATE and takes the` |
|      - | 3334 | `	 * connection and the query, which nothing may call: prepare() builds every` |
|      - | 3335 | `	 * one. The three EXPLAIN_MODE_* constants are php 8.4's, over sqlite's` |
|      - | 3336 | `	 * sqlite3_stmt_explain().` |
|      - | 3337 | `	 */` |
|      - | 3338 | `	static const PH7_NativeConstDef aSq3StmtConst[] = {` |
|      - | 3339 | `		{ "EXPLAIN_MODE_PREPARED",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },` |
|      - | 3340 | `		{ "EXPLAIN_MODE_EXPLAIN",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|      - | 3341 | `		{ "EXPLAIN_MODE_EXPLAIN_QUERY_PLAN", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|      - | 3342 | `	};` |
|      - | 3343 | `	static const PH7_NativeMethodDef aSq3StmtMethod[] = {` |
|      - | 3344 | `		{ "__construct", PH7_MOD_PRIVATE, "SQLite3 $sqlite3, string $query", 0,` |
|      - | 3345 | `		  vm_builtin_SQLite3Stmt_construct },` |
|      - | 3346 | `		{ "bindParam", PH7_MOD_PUBLIC,` |
|      - | 3347 | `		  "string\|int $param, mixed &$var, int $type = SQLITE3_TEXT", "@bool",` |
|      - | 3348 | `		  vm_builtin_SQLite3Stmt_bindParam },` |
|      - | 3349 | `		{ "bindValue", PH7_MOD_PUBLIC,` |
|      - | 3350 | `		  "string\|int $param, mixed $value, int $type = SQLITE3_TEXT", "@bool",` |
|      - | 3351 | `		  vm_builtin_SQLite3Stmt_bindValue },` |
|      - | 3352 | `		{ "clear", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SQLite3Stmt_clear },` |
|      - | 3353 | `		{ "close", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SQLite3Stmt_close },` |
|      - | 3354 | `		{ "execute", PH7_MOD_PUBLIC, "", "@SQLite3Result\|false",` |
|      - | 3355 | `		  vm_builtin_SQLite3Stmt_execute },` |
|      - | 3356 | `		{ "getSQL", PH7_MOD_PUBLIC, "bool $expand = false", "@string\|false",` |
|      - | 3357 | `		  vm_builtin_SQLite3Stmt_getSQL },` |
|      - | 3358 | `		{ "paramCount", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SQLite3Stmt_paramCount },` |
|      - | 3359 | `		{ "readOnly", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SQLite3Stmt_readOnly },` |
|      - | 3360 | `		{ "reset", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SQLite3Stmt_reset },` |
|      - | 3361 | `		/* the three php declares WITHOUT the tentative marker */` |
|      - | 3362 | `		{ "busy", PH7_MOD_PUBLIC, "", "bool", vm_builtin_SQLite3Stmt_busy },` |
|      - | 3363 | `		{ "explain", PH7_MOD_PUBLIC, "", "int", vm_builtin_SQLite3Stmt_explain },` |
|      - | 3364 | `		{ "setExplain", PH7_MOD_PUBLIC, "int $mode", "bool",` |
|      - | 3365 | `		  vm_builtin_SQLite3Stmt_setExplain },` |
|      - | 3366 | `	};` |
|      - | 3367 | `	static const PH7_NativePropDef aSq3StmtProp[] = {` |
|      - | 3368 | `		{ SQ3_RES, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|      - | 3369 | `		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 3370 | `	};` |
|      - | 3371 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 3372 | `		/* php registers the exception FIRST, and ReflectionExtension answers the` |
|      - | 3373 | `		 * class list in that order. */` |
|      - | 3374 | `		{ "SQLite3Exception", "Exception", 0, 0,` |
|      - | 3375 | `		  0, 0, 0, 0, 0, 0,` |
|      - | 3376 | `		  0, 0, 0 },` |
|      - | 3377 | ``		/* `clone` and `serialize` are refused: php declares neither handler, so a`` |
|      - | 3378 | `		 * copy would carry the same sqlite3 pointer in its hidden slot. */` |
|      - | 3379 | `		{ "SQLite3", 0, 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 3380 | `		  aSq3Method, SX_ARRAYSIZE(aSq3Method),` |
|      - | 3381 | `		  aSq3ClassConst, SX_ARRAYSIZE(aSq3ClassConst),` |
|      - | 3382 | `		  aSq3Prop, SX_ARRAYSIZE(aSq3Prop),` |
|      - | 3383 | `		  Sq3InstanceRelease, 0, 0 },` |
|      - | 3384 | `		{ "SQLite3Stmt", 0, 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 3385 | `		  aSq3StmtMethod, SX_ARRAYSIZE(aSq3StmtMethod),` |
|      - | 3386 | `		  aSq3StmtConst, SX_ARRAYSIZE(aSq3StmtConst),` |
|      - | 3387 | `		  aSq3StmtProp, SX_ARRAYSIZE(aSq3StmtProp),` |
|      - | 3388 | `		  Sq3StmtInstanceRelease, 0, 0 },` |
|      - | 3389 | `		{ "SQLite3Result", 0, 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 3390 | `		  aSq3ResMethod, SX_ARRAYSIZE(aSq3ResMethod),` |
|      - | 3391 | `		  0, 0,` |
|      - | 3392 | `		  aSq3ResProp, SX_ARRAYSIZE(aSq3ResProp),` |
|      - | 3393 | `		  Sq3ResInstanceRelease, 0, 0 },` |
|      - | 3394 | `	};` |
|      - | 3395 | `#undef SQ3_INT_CONST` |
|   6726 | 3396 | `	pVm->pSq3Conns = 0;` |
|   6726 | 3397 | `	PH7_RegisterSqlite3Constants(&(*pVm));` |
|   6726 | 3398 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|      5 | 3399 | `}` |
|      - | 3400 |  |
|      - | 3401 | `#else` |
|      - | 3402 | `/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */` |
|      - | 3403 | `typedef int vm_sqlite3_unused;` |
|      - | 3404 | `#endif /* PH7_ENABLE_SQLITE */` |
|      - | 3405 |  |
