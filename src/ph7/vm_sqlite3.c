/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_SQLITE
#include "ph7int.h"
#include <sqlite3.h>

/*
 * Section:
 *    ext/sqlite3 -- php's OTHER sqlite surface, the one beside the PDO driver:
 *    `SQLite3`, `SQLite3Stmt`, `SQLite3Result` and `SQLite3Exception`.
 * Status:
 *    Complete: all 24 methods php declares on SQLite3, all 13 on SQLite3Stmt,
 *    all 8 on SQLite3Result, and the twelve global constants.
 *
 * Nothing here goes through ext/pdo. The two extensions share a LIBRARY and
 * not a model, and every difference between them is deliberate on php's side:
 * pdo reports SQLSTATE strings, sqlite3 reports sqlite's own integer codes;
 * pdo's error mode is an attribute with three settings, sqlite3's is one
 * boolean; pdo opens with `SQLITE_OPEN_URI` and sqlite3 does not, so
 * `new SQLite3('file:/tmp/x')` is a failed open where the same string behind
 * `sqlite:` is a URI. Sharing a connection record between them would mean
 * reconciling those, which php never does.
 *
 * php's object carries TWO pieces of state a script can distinguish, and the
 * whole "has this been closed" surface follows from the pair:
 *   - `initialised`, raised by a successful open and never lowered;
 *   - the `sqlite3 *` itself, which close() drops.
 * A never-opened object fails both, so every verb on it is the Error below. A
 * CLOSED one still passes the first, which is why lastErrorCode(), lastErrorMsg()
 * and lastExtendedErrorCode() answer 0 / "" / 0 there while every other verb is
 * the same Error -- and why open() may be called on it again.
 */

/*
 * One connection. Lives on the per-VM chain (pVm->pSq3Conns) and is reached
 * from its SQLite3 object through the hidden `__res` slot -- the model ext/pdo
 * and XMLWriter use, safe here for the same reason: `clone` is refused, so no
 * second object can ever hold the same handle.
 */
typedef struct phl_sq3 phl_sq3;
typedef struct phl_sq3_stmt phl_sq3_stmt;
typedef struct phl_sq3_res phl_sq3_res;
struct phl_sq3 {
	sqlite3 *pDb;                 /* 0 before the first open and after close() */
	int bInitialised;             /* an open has SUCCEEDED on this object */
	int bExceptions;              /* enableExceptions(): what routes a failure */
	ph7_vm *pVm;
	ph7_class_instance *pOwner;   /* the object whose slot holds it */
	phl_sq3_stmt *pStmts;         /* statements prepared on it: sqlite will not close a
	                               * database while one of them is alive */
	phl_sq3_res *pResults;        /* the result objects walking those statements */
	struct phl_sq3_blob *pBlobs;  /* openBlob() handles a script has not closed. sqlite
	                               * REFUSES to close a database while one is open, which is
	                               * a failure php reports rather than forces past -- only
	                               * the teardown paths close them behind a script's back */
	struct phl_sq3_udf *pUdfs;    /* createFunction/createAggregate/createCollation and the
	                               * authorizer: kept alive for as long as sqlite may call
	                               * them, which is until the connection closes */
	ph7_context *pVerbCtx;        /* the call a diagnostic raised from INSIDE sqlite belongs
	                               * to. php prints the method that was running -- the
	                               * `SQLite3Result::fetchAll(): ` in front of a collation's
	                               * complaint -- and only the verb knows which one it is */
	const char *zVerbFn;
	sxi32 iCallbackExc;           /* the status a callback threw with, PARKED: sqlite has to
	                               * finish unwinding before the engine may raise it, and the
	                               * verb that started the step answers exactly this */
	phl_sq3 *pNext;
};
/*
 * One prepared statement, REFERENCE-COUNTED -- which is the shape php's own
 * object graph has and the reason two verbs that look alike behave differently.
 * `query()` builds a statement nothing but its result holds, so finalizing that
 * result destroys it; `execute()` hands out a result over a statement the script
 * ALSO holds, so finalizing that result leaves the statement runnable. The count
 * is what tells the two apart at the moment of the finalize.
 *
 * `close()` on the statement is not a release: it finalizes NOW, whatever
 * results are still pointing at it, and every one of them starts answering the
 * Error instead.
 */
struct phl_sq3_stmt {
	sqlite3_stmt *pStmt;          /* 0 for a statement of NOTHING (see below), and once closed */
	phl_sq3 *pConn;
	ph7_class_instance *pOwner;   /* the SQLite3Stmt object, when a script holds one */
	ph7_class_instance *pConnObj; /* the SQLite3 object, RETAINED: a result outlives the
	                               * variable its connection was in, and php keeps the
	                               * database open through exactly this reference */
	int bInitialised;             /* php's per-STATEMENT flag: raised by prepare, lowered by
	                               * close() and by the connection's own close. It is what
	                               * tells a closed statement (the Error naming SQLite3) from
	                               * a statement of NOTHING (the one naming SQLite3Stmt) */
	int nRef;                     /* holders: the statement object, and each live result */
	struct phl_sq3_bind *pBinds;  /* what bindValue()/bindParam() recorded, applied at
	                               * every execute -- php's bindings, not sqlite's */
	phl_sq3_stmt *pNext;
};
/*
 * One result -- php's cursor over a statement, and nothing more. It owns no row
 * of its own: `numColumns()` reads the statement's column count, `columnType()`
 * reads the row sqlite has UP, and `fetchArray()` steps.
 */
struct phl_sq3_res {
	phl_sq3_stmt *pSt;            /* 0 once finalize() has run on THIS result */
	phl_sq3 *pConn;               /* kept beside pSt: the record has to find its chain
	                               * again after finalize() has let the statement go */
	ph7_class_instance *pOwner;
	ph7_class_instance *pStmtObj; /* the SQLite3Stmt it came from, RETAINED, or 0 for the
	                               * anonymous statement query() builds */
	phl_sq3_res *pNext;
};

/* The hidden slot every one of these classes reaches its record through. */
#define SQ3_RES "__res"

/* ------------------------------------------------------------------------
 * Lifetime
 * ------------------------------------------------------------------------ */
/*
 * Blank one object's hidden slot: the record behind it is going away and the
 * object may well outlive it.
 */
static void Sq3BlankSlot(ph7_class_instance *pOwner);
/* Drop what bindValue()/bindParam() recorded (defined with the statement). */
static void Sq3BindsClear(phl_sq3_stmt *pSt);
/* Let go of every callback a script registered (defined with them, below). */
static void Sq3UdfSweep(phl_sq3 *pConn);
/* Close every blob handle a script left open (defined with them, below). */
static void Sq3BlobSweep(phl_sq3 *pConn);
/*
 * Close the database, and every statement standing on it with it.
 *
 * The statements go FIRST and they are not merely released: php keeps a list of
 * everything a connection handed out and cleans it here, so `close()` finalizes
 * a statement a script is still holding and every result walking one starts
 * answering the Error. The records themselves stay -- the objects that reach
 * them are still alive and have to find an emptied handle rather than freed
 * memory.
 *
 * That is also why dropping the last reference to a SQLite3 is NOT the same
 * thing: a live statement retains the object, so there is no last reference to
 * drop while one exists, and a result outlives the variable its connection was
 * in.
 */
static void Sq3FinalizeStmts(phl_sq3 *pConn)
{
	phl_sq3_stmt *pSt;
	for( pSt = pConn->pStmts ; pSt ; pSt = pSt->pNext ){
		if( pSt->pStmt ){
			sqlite3_finalize(pSt->pStmt);
			pSt->pStmt = 0;
		}
		pSt->bInitialised = 0;
	}
}
/* What is left to let go of once the handle itself is gone. */
static void Sq3CloseDone(phl_sq3 *pConn)
{
	pConn->pDb = 0;
	/* the callbacks go last: nothing can reach them once the database that
	 * would have called them is closed */
	Sq3UdfSweep(pConn);
	pConn->iCallbackExc = 0;
}
/*
 * The close a script cannot refuse: every blob handle goes too and the handle
 * is dropped with _v2, so the connection really is gone. This is the TEARDOWN
 * path -- the object dying, or the VM being reset -- where nothing is left to
 * report a failure to.
 */
static void Sq3Close(phl_sq3 *pConn)
{
	Sq3FinalizeStmts(pConn);
	Sq3BlobSweep(pConn);
	if( pConn->pDb ){
		sqlite3_close_v2(pConn->pDb);
	}
	Sq3CloseDone(pConn);
}
static phl_sq3 * Sq3NewConn(ph7_vm *pVm)
{
	phl_sq3 *pConn = (phl_sq3 *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_sq3));
	if( pConn == 0 ){
		return 0;
	}
	SyZero(pConn,sizeof(phl_sq3));
	pConn->pVm = pVm;
	pConn->pNext = (phl_sq3 *)pVm->pSq3Conns;
	pVm->pSq3Conns = pConn;
	return pConn;
}
/*
 * Let go of one hold on a statement. The LAST holder finalizes it -- which is
 * how a result over an anonymous statement destroys it while a result over a
 * script's own statement does not.
 */
static void Sq3StmtUnref(phl_sq3_stmt *pSt)
{
	phl_sq3 *pConn;
	phl_sq3_stmt *pCur,*pPrev = 0;
	if( pSt == 0 || --pSt->nRef > 0 ){
		return;
	}
	pConn = pSt->pConn;
	Sq3BindsClear(pSt);
	if( pSt->pStmt ){
		sqlite3_finalize(pSt->pStmt);
		pSt->pStmt = 0;
	}
	for( pCur = pConn->pStmts ; pCur ; pPrev = pCur, pCur = pCur->pNext ){
		if( pCur == pSt ){
			if( pPrev ){
				pPrev->pNext = pCur->pNext;
			}else{
				pConn->pStmts = pCur->pNext;
			}
			break;
		}
	}
	if( pSt->pConnObj ){
		/* drop the reference taken at creation; the connection may go now */
		ph7_class_instance *pObj = pSt->pConnObj;
		pSt->pConnObj = 0;
		PH7_ClassInstanceUnref(pObj);
	}
	SyMemBackendFree(&pConn->pVm->sAllocator,pSt);
}
/*
 * Let go of what a result is HOLDING without freeing the record: `finalize()`
 * does exactly this and leaves the object standing, answering the Error.
 */
static void Sq3ResDetach(phl_sq3_res *pRes)
{
	if( pRes->pSt ){
		Sq3StmtUnref(pRes->pSt);
		pRes->pSt = 0;
	}
	if( pRes->pStmtObj ){
		ph7_class_instance *pObj = pRes->pStmtObj;
		pRes->pStmtObj = 0;
		PH7_ClassInstanceUnref(pObj);
	}
}
/* Free one result record: what it holds, then its place on the chain. */
static void Sq3FreeRes(phl_sq3_res *pRes)
{
	phl_sq3 *pConn = pRes->pConn;
	phl_sq3_res *pCur,*pPrev = 0;
	Sq3ResDetach(pRes);
	for( pCur = pConn->pResults ; pCur ; pPrev = pCur, pCur = pCur->pNext ){
		if( pCur == pRes ){
			if( pPrev ){
				pPrev->pNext = pCur->pNext;
			}else{
				pConn->pResults = pCur->pNext;
			}
			break;
		}
	}
	SyMemBackendFree(&pConn->pVm->sAllocator,pRes);
}
static void Sq3ResSweep(phl_sq3 *pConn)
{
	while( pConn->pResults ){
		phl_sq3_res *pRes = pConn->pResults;
		Sq3BlankSlot(pRes->pOwner);
		Sq3FreeRes(pRes);
	}
}
static void Sq3StmtSweep(phl_sq3 *pConn)
{
	while( pConn->pStmts ){
		phl_sq3_stmt *pSt = pConn->pStmts;
		Sq3BlankSlot(pSt->pOwner);
		pConn->pStmts = pSt->pNext;
		Sq3BindsClear(pSt);
		if( pSt->pStmt ){
			sqlite3_finalize(pSt->pStmt);
			pSt->pStmt = 0;
		}
		if( pSt->pConnObj ){
			ph7_class_instance *pObj = pSt->pConnObj;
			pSt->pConnObj = 0;
			PH7_ClassInstanceUnref(pObj);
		}
		SyMemBackendFree(&pConn->pVm->sAllocator,pSt);
	}
}
static void Sq3FreeConn(phl_sq3 *pConn)
{
	ph7_vm *pVm = pConn->pVm;
	phl_sq3 *pCur,*pPrev = 0;
	/* Results first, then statements: a result names a statement, and sqlite
	 * refuses to close a database while a statement of its own is alive. */
	Sq3ResSweep(pConn);
	Sq3StmtSweep(pConn);
	Sq3Close(pConn);
	for( pCur = (phl_sq3 *)pVm->pSq3Conns ; pCur ; pPrev = pCur, pCur = pCur->pNext ){
		if( pCur == pConn ){
			if( pPrev ){
				pPrev->pNext = pCur->pNext;
			}else{
				pVm->pSq3Conns = pCur->pNext;
			}
			break;
		}
	}
	SyMemBackendFree(&pVm->sAllocator,pConn);
}
static void Sq3VmSweep(ph7_vm *pVm)
{
	while( pVm->pSq3Conns ){
		phl_sq3 *pConn = (phl_sq3 *)pVm->pSq3Conns;
		Sq3BlankSlot(pConn->pOwner);
		Sq3FreeConn(pConn);
	}
}
/*
 * A reused VM (the -S server's) must not answer the next request through a
 * handle this one opened, and a sqlite3 handle lives outside SyMemBackend, so
 * the wholesale release at the end would leak both it and the file lock.
 */
PH7_PRIVATE void PH7_Sqlite3VmReset(ph7_vm *pVm)
{
	Sq3VmSweep(&(*pVm));
}
PH7_PRIVATE void PH7_Sqlite3VmRelease(ph7_vm *pVm)
{
	Sq3VmSweep(&(*pVm));
}

/* ------------------------------------------------------------------------
 * The object and its hidden slot
 * ------------------------------------------------------------------------ */
/* The record behind any of the three classes' hidden slots. */
static void * Sq3ResourceOf(ph7_class_instance *pThis)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr,SQ3_RES,sizeof(SQ3_RES)-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 || !ph7_value_is_resource(pRes) ){
		return 0;
	}
	return ph7_value_to_resource(pRes);
}
static void Sq3BlankSlot(ph7_class_instance *pOwner)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pOwner == 0 ){
		return;
	}
	SyStringInitFromBuf(&sAttr,SQ3_RES,sizeof(SQ3_RES)-1);
	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);
	if( pRes ){
		PH7_MemObjRelease(pRes);
		MemObjSetType(pRes,MEMOBJ_NULL);
	}
}
static int Sq3AttachRes(ph7_class_instance *pThis,void *pRecord)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return -1;
	}
	SyStringInitFromBuf(&sAttr,SQ3_RES,sizeof(SQ3_RES)-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 ){
		return -1;
	}
	PH7_MemObjRelease(pRes);
	pRes->x.pOther = pRecord;
	MemObjSetType(pRes,MEMOBJ_RES);
	return 0;
}
static phl_sq3 * Sq3OfInstance(ph7_class_instance *pThis)
{
	return (phl_sq3 *)Sq3ResourceOf(pThis);
}
static int Sq3Attach(ph7_class_instance *pThis,phl_sq3 *pConn)
{
	if( Sq3AttachRes(pThis,pConn) != 0 ){
		return -1;
	}
	pConn->pOwner = pThis;
	return 0;
}
/*
 * The object is going away: close its database HERE rather than at VM reset,
 * so a script that drops its last reference releases the file lock there --
 * which is what php does, and what a test that unlinks the file afterwards
 * needs. The record itself stays on the registry for the sweep to free.
 */
static void Sq3InstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_sq3 *pConn = Sq3OfInstance(pThis);
	SXUNUSED(pVm);
	if( pConn == 0 || pConn->pOwner != pThis ){
		return;
	}
	Sq3Close(pConn);
	pConn->pOwner = 0;
}
/*
 * The record behind `$this`, made on demand: php's object exists before any
 * open (`newInstanceWithoutConstructor()` builds one, and open() may be called
 * on it later), so the slot is filled at the first verb that needs it rather
 * than at `new`.
 */
static phl_sq3 * Sq3Bind(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_sq3 *pConn;
	if( pThis == 0 ){
		return 0;
	}
	pConn = Sq3OfInstance(pThis);
	if( pConn ){
		return pConn;
	}
	pConn = Sq3NewConn(pCtx->pVm);
	if( pConn == 0 ){
		return 0;
	}
	if( Sq3Attach(pThis,pConn) != 0 ){
		Sq3FreeConn(pConn);
		return 0;
	}
	return pConn;
}
/*
 * php's `SQLITE3_CHECK_INITIALIZED`: one sentence for both halves of the
 * state, which is why a never-opened object and a closed one are told the same
 * thing.
 */
static sxi32 Sq3Uninitialised(ph7_context *pCtx,const char *zClass)
{
	return PH7_VmThrowException(pCtx,"Error",
		"The %s object has not been correctly initialised or is already closed",zClass);
}

/* ------------------------------------------------------------------------
 * Reporting a failure
 * ------------------------------------------------------------------------ */
/*
 * php's `php_sqlite3_error()`: one routine, two destinations. With exceptions
 * enabled the text becomes a SQLite3Exception carrying sqlite's own code;
 * without them it is an E_WARNING that names the METHOD, which is the
 * docref prefix php puts on every diagnostic raised from inside a call.
 *
 * The exception's message has no prefix at all -- the same text, told twice in
 * two shapes.
 */
static sxi32 Sq3Error(ph7_context *pCtx,phl_sq3 *pConn,const char *zFn,int iCode,
	const char *zMsg)
{
	if( pConn && pConn->iCallbackExc != 0 ){
		/* A callback has already thrown and the library is only reporting that
		 * it was told to stop. php raises the script's own exception and says
		 * nothing about the statement sqlite abandoned. */
		return PH7_OK;
	}
	if( pConn && pConn->bExceptions ){
		return PH7_VmThrowExceptionCode(pCtx,"SQLite3Exception",(sxi32)iCode,"%s",zMsg);
	}
	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,zMsg);
	return PH7_OK;
}
/*
 * A verb that can re-enter PHP -- anything that steps the library, since a
 * callback may fire from inside it -- announces itself on the connection for
 * the length of the call. Two things read that: a diagnostic raised from inside
 * a callback, which php prints under the METHOD's name, and the parked status a
 * callback threw with, which this verb is the one to raise.
 *
 * The frame is saved and restored rather than assigned, because a callback may
 * perfectly well run a query of its own on the same connection.
 */
typedef struct Sq3Verb Sq3Verb;
struct Sq3Verb {
	ph7_context *pCtx;
	const char *zFn;
};
static void Sq3VerbEnter(phl_sq3 *pConn,ph7_context *pCtx,const char *zFn,Sq3Verb *pSave)
{
	pSave->pCtx = pConn->pVerbCtx;
	pSave->zFn = pConn->zVerbFn;
	pConn->pVerbCtx = pCtx;
	pConn->zVerbFn = zFn;
}
/*
 * Leave the frame and answer the status a callback parked, if any -- taking it
 * OFF the connection as it goes, so the next verb starts clean whatever this
 * one's caller does with it.
 */
static sxi32 Sq3VerbLeave(phl_sq3 *pConn,Sq3Verb *pSave)
{
	sxi32 rc = pConn->iCallbackExc;
	pConn->iCallbackExc = 0;
	pConn->pVerbCtx = pSave->pCtx;
	pConn->zVerbFn = pSave->zFn;
	return rc;
}
/* The same, worded from the library's own view of the last failure. */
static sxi32 Sq3ErrorFromDb(ph7_context *pCtx,phl_sq3 *pConn,const char *zFn)
{
	int iCode = pConn->pDb ? sqlite3_errcode(pConn->pDb) : SQLITE_ERROR;
	const char *zMsg = pConn->pDb ? sqlite3_errmsg(pConn->pDb) : sqlite3_errstr(SQLITE_ERROR);
	return Sq3Error(pCtx,pConn,zFn,iCode,zMsg);
}

/* ------------------------------------------------------------------------
 * Opening and closing
 * ------------------------------------------------------------------------ */
/*
 * Is this an absolute path already? The Windows spelling has three shapes a
 * POSIX one does not -- a drive letter, a leading backslash and a UNC share.
 */
static int Sq3PathIsAbsolute(const char *zPath,int nPath)
{
	if( nPath < 1 ){
		return 0;
	}
	if( zPath[0] == '/' ){
		return 1;
	}
#ifdef __WINNT__
	if( zPath[0] == '\\' ){
		return 1;
	}
	if( nPath > 2 && zPath[1] == ':' && (zPath[2] == '/' || zPath[2] == '\\') ){
		return 1;
	}
#endif
	return 0;
}
/*
 * php's `expand_filepath()`, which every ext/sqlite3 open runs the filename
 * through unless it is exactly `:memory:` or the empty string. It is what makes
 * a relative name resolve against the working directory -- and, less obviously,
 * what keeps `file:...` from ever reaching sqlite as a URI, because by then the
 * name starts with the directory instead. The engine cannot leave that to the
 * library: a Debian libsqlite3 is compiled with URI filenames ON and a vcpkg one
 * is not, so an unexpanded `file:x` would open two different things on this
 * engine's two platforms while php opens neither.
 *
 * The `.` and `..` segments are collapsed HERE rather than by the filesystem,
 * which is php's own behaviour: `sub/../db` opens `db` even when no `sub`
 * directory exists, where handing the OS the uncollapsed path is ENOENT.
 */
static void Sq3ExpandPath(ph7_context *pCtx,const char *zPath,int nPath,SyBlob *pOut)
{
	SyBlob sRaw;
	const char *z;
	sxu32 n,nRoot,nRaw;
	SyBlobInit(pOut,&pCtx->pVm->sAllocator);
	SyBlobInit(&sRaw,&pCtx->pVm->sAllocator);
	if( !Sq3PathIsAbsolute(zPath,nPath) ){
		/* the VFS answers through the context's RESULT slot, which the caller
		 * overwrites with its own return value afterwards */
		const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
		PH7_MemObjRelease(pCtx->pRet);
		if( pVfs && pVfs->xGetcwd && pVfs->xGetcwd(pCtx) == PH7_OK
		 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0 ){
			SyBlobAppend(&sRaw,SyBlobData(&pCtx->pRet->sBlob),
				SyBlobLength(&pCtx->pRet->sBlob));
		}
		PH7_MemObjRelease(pCtx->pRet);
		SyBlobAppend(&sRaw,"/",sizeof(char));
	}
	if( nPath > 0 ){
		SyBlobAppend(&sRaw,zPath,(sxu32)nPath);
	}
	/* Keep the root -- a leading slash, or a drive prefix -- and rebuild the
	 * rest segment by segment. */
	z = (const char *)SyBlobData(&sRaw);
	nRaw = SyBlobLength(&sRaw);
	nRoot = 0;
#ifdef __WINNT__
	if( nRaw > 1 && z[1] == ':' ){
		nRoot = 2;
	}
#endif
	if( nRoot < nRaw && (z[nRoot] == '/' || z[nRoot] == '\\') ){
		++nRoot;
	}
	SyBlobAppend(pOut,z,nRoot);
	for( n = nRoot ; n < nRaw ; ){
		sxu32 nStart = n;
		sxu32 nSeg;
		while( n < nRaw && z[n] != '/' && z[n] != '\\' ){
			++n;
		}
		nSeg = n - nStart;
		if( n < nRaw ){
			++n;   /* step past the separator */
		}
		if( nSeg == 0 || (nSeg == 1 && z[nStart] == '.') ){
			continue;   /* `//` and `.` name the directory they stand in */
		}
		if( nSeg == 2 && z[nStart] == '.' && z[nStart+1] == '.' ){
			/* pop the previous segment; `..` above the root is the root */
			sxu32 nHave = SyBlobLength(pOut);
			while( nHave > nRoot && ((char *)SyBlobData(pOut))[nHave-1] != '/' ){
				--nHave;
			}
			if( nHave > nRoot ){
				--nHave;   /* and the separator that held it */
			}
			pOut->nByte = nHave;   /* the blob has no truncate of its own */
			continue;
		}
		if( SyBlobLength(pOut) > nRoot ){
			SyBlobAppend(pOut,"/",sizeof(char));
		}
		SyBlobAppend(pOut,&z[nStart],nSeg);
	}
	SyBlobRelease(&sRaw);
	SyBlobNullAppend(pOut);
}
/*
 * The open both `__construct` and `open` are. php refuses a SECOND open on a
 * live handle before it looks at the arguments' meaning, throws a plain
 * Exception (not SQLite3Exception -- the object cannot have been told to use
 * them yet) for a failure, and hands sqlite the EXPANDED filename.
 *
 * Two names are handed over untouched instead, and they are the two that name
 * no file: `:memory:` exactly, and the empty string (a private temporary
 * database sqlite deletes with the connection). Everything else is a path,
 * `:memory:extra` included.
 *
 * `SQLITE_OPEN_URI` is deliberately NOT passed -- php's ext/sqlite3 does not,
 * and the expansion above makes the question moot anyway. ext/pdo DOES pass it,
 * which is why the same string behind a `sqlite:` DSN opens a URI there; the
 * divergence is php's own, between its two extensions.
 */
static sxi32 Sq3OpenImpl(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_sq3 *pConn = Sq3Bind(pCtx);
	const char *zFile;
	int nFile = 0;
	int iFlags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;
	SyBlob sPath;
	int rc;
	if( pConn == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pConn->pDb ){
		return PH7_VmThrowException(pCtx,"Exception","Already initialised DB Object");
	}
	zFile = ph7_value_to_string(apArg[0],&nFile);
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int64(apArg[1]);
	}
	/* $encryptionKey is read and dropped: php's own build has no codec either,
	 * so the argument reaches nothing there. */
	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);
	if( nFile == (int)sizeof(":memory:")-1
	 && SyMemcmp(zFile,":memory:",sizeof(":memory:")-1) == 0 ){
		SyBlobAppend(&sPath,":memory:",sizeof(":memory:")-1);
		SyBlobNullAppend(&sPath);
	}else if( nFile < 1 ){
		SyBlobNullAppend(&sPath);
	}else{
		SyBlobRelease(&sPath);
		Sq3ExpandPath(pCtx,zFile,nFile,&sPath);
	}
	rc = sqlite3_open_v2((const char *)SyBlobData(&sPath),&pConn->pDb,iFlags,0);
	SyBlobRelease(&sPath);
	if( rc != SQLITE_OK ){
		/* sqlite hands a handle back even on failure so the message can be read
		 * off it; take the text first, then drop the handle. */
		const char *zMsg = pConn->pDb ? sqlite3_errmsg(pConn->pDb) : sqlite3_errstr(rc);
		SyBlob sMsg;
		sxi32 rcThrow;
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		SyBlobAppend(&sMsg,zMsg,SyStrlen(zMsg));
		SyBlobAppend(&sMsg,"",1);
		rcThrow = PH7_VmThrowException(pCtx,"Exception","Unable to open database: %s",
			(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
		Sq3Close(pConn);
		return rcThrow;
	}
	/* php's `sqlite3.defensive`, applied to every connection it opens: with it
	 * on, `PRAGMA writable_schema` still succeeds and the UPDATE behind it does
	 * not ("table sqlite_master may not be modified"). */
	if( PH7_VmIniGetBool(pCtx->pVm,"sqlite3.defensive",1) ){
		sqlite3_db_config(pConn->pDb,SQLITE_DBCONFIG_DEFENSIVE,1,(int *)0);
	}
	pConn->bInitialised = 1;
	return PH7_OK;
}
/*
 * SQLite3::__construct(string $filename, int $flags = SQLITE3_OPEN_READWRITE |
 *   SQLITE3_OPEN_CREATE, string $encryptionKey = '')
 */
static int vm_builtin_SQLite3_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return Sq3OpenImpl(pCtx,nArg,apArg);
}
/* SQLite3::open(...): void -- the constructor's body, callable again after close() */
static int vm_builtin_SQLite3_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return Sq3OpenImpl(pCtx,nArg,apArg);
}
/*
 * SQLite3::close(): bool
 *
 * TRUE for every call, including one on an object that was never opened: php
 * asks nothing here, it just drops whatever handle is there. What close does
 * NOT do is lower `initialised`, which is what keeps lastErrorMsg() answering
 * afterwards.
 */
static int vm_builtin_SQLite3_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_sq3 *pConn = Sq3OfInstance(PH7_ContextThis(pCtx));
	int rcClose;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 || pConn->pDb == 0 ){
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	/* the statements go first and unconditionally: a script's own statement is
	 * finalized even when the close that follows FAILS */
	Sq3FinalizeStmts(pConn);
	/* and the close itself is the one that can fail -- not _v2, so an open blob
	 * handle is a refusal a script is told about rather than something closed
	 * behind its back */
	rcClose = sqlite3_close(pConn->pDb);
	if( rcClose != SQLITE_OK ){
		SyBlob sMsg;
		sxi32 rc;
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		SyBlobFormat(&sMsg,"Unable to close database: %s",sqlite3_errmsg(pConn->pDb));
		SyBlobNullAppend(&sMsg);
		ph7_result_bool(pCtx,0);
		rc = Sq3Error(pCtx,pConn,"SQLite3::close",rcClose,(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
		return rc;   /* the connection is still open, and still usable */
	}
	Sq3CloseDone(pConn);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * What the library says about itself
 * ------------------------------------------------------------------------ */
/*
 * SQLite3::version(): array
 *
 * The LINKED library's version, so it differs between this engine's platforms
 * (3.45 on a Debian host, whatever vcpkg last shipped on Windows) -- which is
 * why no test may pin the numbers.
 */
static int vm_builtin_SQLite3_version(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pVal;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_value_string(pVal,sqlite3_libversion(),-1);
	ph7_array_add_strkey_elem(pArray,"versionString",pVal);
	ph7_value_int64(pVal,(ph7_int64)sqlite3_libversion_number());
	ph7_array_add_strkey_elem(pArray,"versionNumber",pVal);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * SQLite3::escapeString(string $string): string
 *
 * sqlite's own `%q`, which doubles every single quote and NOTHING else -- so
 * the answer is what goes INSIDE a pair of quotes, unlike PDO::quote(), which
 * supplies them. It is a C string to the library, so a NUL byte ends the
 * answer there rather than being escaped; php has the same cut.
 */
static int vm_builtin_SQLite3_escapeString(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nIn = 0;
	char *zOut;
	SyBlob sTerm;
	SXUNUSED(nArg);
	zIn = ph7_value_to_string(apArg[0],&nIn);
	if( nIn < 1 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	SyBlobInit(&sTerm,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sTerm,zIn,(sxu32)nIn);
	SyBlobAppend(&sTerm,"",1);
	zOut = sqlite3_mprintf("%q",(const char *)SyBlobData(&sTerm));
	SyBlobRelease(&sTerm);
	if( zOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_result_string(pCtx,zOut,-1);
	sqlite3_free(zOut);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * The last failure, as the connection reports it
 * ------------------------------------------------------------------------ */
/*
 * The three error readers share php's softer guard: they need an object that
 * was opened ONCE, not one that is open NOW. A closed connection has no
 * library state left to ask, so each answers its type's zero.
 */
static phl_sq3 * Sq3ErrReader(ph7_context *pCtx,sxi32 *pRc)
{
	phl_sq3 *pConn = Sq3OfInstance(PH7_ContextThis(pCtx));
	*pRc = PH7_OK;
	if( pConn == 0 || !pConn->bInitialised ){
		*pRc = Sq3Uninitialised(pCtx,"SQLite3");
		return 0;
	}
	return pConn;
}
static int vm_builtin_SQLite3_lastErrorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3ErrReader(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return rc;
	}
	/* sqlite3_errcode answers the EXTENDED code once extended result codes are
	 * on, so this reader and lastExtendedErrorCode() agree from then on -- which
	 * is php's answer too, since php reads the same two functions. */
	ph7_result_int64(pCtx,pConn->pDb ? (ph7_int64)sqlite3_errcode(pConn->pDb) : 0);
	return PH7_OK;
}
static int vm_builtin_SQLite3_lastExtendedErrorCode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3ErrReader(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return rc;
	}
	ph7_result_int64(pCtx,pConn->pDb ? (ph7_int64)sqlite3_extended_errcode(pConn->pDb) : 0);
	return PH7_OK;
}
static int vm_builtin_SQLite3_lastErrorMsg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3ErrReader(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return rc;
	}
	/* An open handle that has met no failure says "not an error"; a closed one
	 * says the empty string, because there is nothing left to ask. */
	if( pConn->pDb ){
		ph7_result_string(pCtx,sqlite3_errmsg(pConn->pDb),-1);
	}else{
		ph7_result_string(pCtx,"",0);
	}
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Running one string of SQL
 * ------------------------------------------------------------------------ */
/* The guard every verb that TOUCHES the database shares. */
static phl_sq3 * Sq3LiveDb(ph7_context *pCtx,sxi32 *pRc)
{
	phl_sq3 *pConn = Sq3OfInstance(PH7_ContextThis(pCtx));
	*pRc = PH7_OK;
	if( pConn == 0 || pConn->pDb == 0 ){
		*pRc = Sq3Uninitialised(pCtx,"SQLite3");
		return 0;
	}
	return pConn;
}
/*
 * SQLite3::exec(string $query): bool
 *
 * sqlite3_exec over the whole string: every statement in it runs, and the
 * answer is only whether they all did. The empty string is a successful run of
 * nothing, which is TRUE -- unlike PDO::exec(), whose own ValueError refuses
 * it.
 */
static int vm_builtin_SQLite3_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);
	const char *zSql;
	int nSql = 0;
	SyBlob sSql;
	Sq3Verb sVerb;
	int rcSql;
	SXUNUSED(nArg);
	if( pConn == 0 ){
		return rc;
	}
	zSql = ph7_value_to_string(apArg[0],&nSql);
	SyBlobInit(&sSql,&pCtx->pVm->sAllocator);
	if( nSql > 0 ){
		SyBlobAppend(&sSql,zSql,(sxu32)nSql);
	}
	SyBlobAppend(&sSql,"",1);
	Sq3VerbEnter(pConn,pCtx,"SQLite3::exec",&sVerb);
	rcSql = sqlite3_exec(pConn->pDb,(const char *)SyBlobData(&sSql),0,0,0);
	SyBlobRelease(&sSql);
	rc = Sq3VerbLeave(pConn,&sVerb);
	if( rc != PH7_OK ){
		ph7_result_bool(pCtx,0);
		return rc;   /* a callback threw: that is the answer, not sqlite's */
	}
	if( rcSql != SQLITE_OK ){
		ph7_result_bool(pCtx,0);
		return Sq3ErrorFromDb(pCtx,pConn,"SQLite3::exec");
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Preparing, stepping, and the cursor a script walks
 * ------------------------------------------------------------------------ */
/* A fresh statement record, chained on its connection and retaining the object
 * the connection lives in. */
static phl_sq3_stmt * Sq3NewStmt(phl_sq3 *pConn)
{
	phl_sq3_stmt *pSt = (phl_sq3_stmt *)SyMemBackendAlloc(&pConn->pVm->sAllocator,
		sizeof(phl_sq3_stmt));
	if( pSt == 0 ){
		return 0;
	}
	SyZero(pSt,sizeof(phl_sq3_stmt));
	pSt->pConn = pConn;
	pSt->nRef = 1;
	pSt->pConnObj = pConn->pOwner;
	if( pSt->pConnObj ){
		pSt->pConnObj->iRef++;
	}
	pSt->pNext = pConn->pStmts;
	pConn->pStmts = pSt;
	return pSt;
}
/*
 * php's prepare, shared by prepare(), query() and querySingle().
 *
 * Two answers are not failures and are not the same either. An EMPTY query is
 * refused before the library is asked -- php checks the length itself, so there
 * is no diagnostic to report. A query that holds no STATEMENT (whitespace, a
 * comment) prepares perfectly well and leaves sqlite's out-parameter NULL; php
 * keeps that as a statement of nothing, and everything a script then does with
 * it answers off the missing handle rather than off an error.
 *
 * Answers 0 when the caller should hand back false -- which both of those are,
 * told apart only by whether anything was reported on the way.
 */
static phl_sq3_stmt * Sq3Prepare(ph7_context *pCtx,phl_sq3 *pConn,const char *zSql,int nSql,
	const char *zFn,sxi32 *pRc)
{
	phl_sq3_stmt *pSt;
	sqlite3_stmt *pRaw = 0;
	int rc;
	*pRc = PH7_OK;
	if( nSql < 1 ){
		return 0;
	}
	rc = sqlite3_prepare_v2(pConn->pDb,zSql,nSql,&pRaw,0);
	if( rc != SQLITE_OK ){
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		SyBlobFormat(&sMsg,"Unable to prepare statement: %s",sqlite3_errmsg(pConn->pDb));
		SyBlobNullAppend(&sMsg);
		*pRc = Sq3Error(pCtx,pConn,zFn,sqlite3_errcode(pConn->pDb),
			(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
		return 0;
	}
	pSt = Sq3NewStmt(pConn);
	if( pSt == 0 ){
		if( pRaw ){
			sqlite3_finalize(pRaw);
		}
		*pRc = PH7_ContextMemoryError(pCtx);
		return 0;
	}
	pSt->pStmt = pRaw;
	pSt->bInitialised = 1;
	return pSt;
}
/*
 * Run a statement once and rewind it, which is what BOTH `query()` and
 * `execute()` do before they hand a cursor back. It is not a lookahead: the
 * step really runs, so `query("INSERT ...")` inserts, and the reset that
 * follows is what makes the result's first fetch start at the first row --
 * and what makes a fetch on that INSERT's result insert a SECOND time.
 *
 * A statement of NOTHING has no handle to step and is a failure here, reported
 * off the CONNECTION -- which has met no error, so what a comment-only query
 * says is `Unable to execute statement: not an error`.
 */
static int Sq3StepOnce(ph7_context *pCtx,phl_sq3_stmt *pSt,const char *zFn,sxi32 *pRc)
{
	int rc;
	*pRc = PH7_OK;
	if( pSt->pStmt ){
		rc = sqlite3_step(pSt->pStmt);
		if( rc == SQLITE_ROW || rc == SQLITE_DONE ){
			sqlite3_reset(pSt->pStmt);
			return 1;
		}
	}
	{
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		SyBlobFormat(&sMsg,"Unable to execute statement: %s",
			sqlite3_errmsg(pSt->pConn->pDb));
		SyBlobNullAppend(&sMsg);
		*pRc = Sq3Error(pCtx,pSt->pConn,zFn,sqlite3_errcode(pSt->pConn->pDb),
			(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
	}
	return 0;
}
/*
 * One column of the row sqlite has up, in the type sqlite says it is.
 *
 * The cursor is reset first because callers reuse ONE scalar down a row: a
 * string write appends, so without this the second text column of a row comes
 * back carrying the first one's bytes in front of it.
 */
static void Sq3ColumnValue(sqlite3_stmt *pStmt,int iCol,ph7_value *pOut)
{
	ph7_value_reset_string_cursor(pOut);
	switch( sqlite3_column_type(pStmt,iCol) ){
		case SQLITE_INTEGER:
			ph7_value_int64(pOut,(ph7_int64)sqlite3_column_int64(pStmt,iCol));
			break;
#ifndef PH7_OMIT_FLOATING_POINT
		case SQLITE_FLOAT:
			ph7_value_double(pOut,sqlite3_column_double(pStmt,iCol));
			break;
#endif
		case SQLITE_NULL:
			ph7_value_null(pOut);
			break;
		case SQLITE_BLOB: {
			const void *pBlob = sqlite3_column_blob(pStmt,iCol);
			int nByte = sqlite3_column_bytes(pStmt,iCol);
			ph7_value_string(pOut,pBlob ? (const char *)pBlob : "",nByte);
			break;
		}
		default: {
			const unsigned char *zText = sqlite3_column_text(pStmt,iCol);
			int nByte = sqlite3_column_bytes(pStmt,iCol);
			ph7_value_string(pOut,zText ? (const char *)zText : "",nByte);
			break;
		}
	}
}
/* Build the SQLite3Result object over one statement. */
static ph7_class_instance * Sq3NewResultObject(ph7_context *pCtx,phl_sq3_stmt *pSt,
	ph7_class_instance *pStmtObj)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"SQLite3Result",
		sizeof("SQLite3Result")-1,FALSE,0);
	ph7_class_instance *pObj;
	phl_sq3_res *pRes;
	if( pClass == 0 ){
		return 0;
	}
	pObj = PH7_NewClassInstance(&(*pVm),pClass);
	if( pObj == 0 ){
		return 0;
	}
	pRes = (phl_sq3_res *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_sq3_res));
	if( pRes == 0 ){
		PH7_ClassInstanceUnref(pObj);
		return 0;
	}
	SyZero(pRes,sizeof(phl_sq3_res));
	pRes->pSt = pSt;
	pRes->pConn = pSt->pConn;
	pRes->pOwner = pObj;
	pRes->pStmtObj = pStmtObj;
	if( pStmtObj ){
		pStmtObj->iRef++;
	}
	pRes->pNext = pSt->pConn->pResults;
	pSt->pConn->pResults = pRes;
	if( Sq3AttachRes(pObj,pRes) != 0 ){
		pRes->pSt = 0;   /* the caller still owns the statement hold */
		Sq3FreeRes(pRes);
		PH7_ClassInstanceUnref(pObj);
		return 0;
	}
	return pObj;
}
static phl_sq3_res * Sq3ResOfInstance(ph7_class_instance *pThis)
{
	return (phl_sq3_res *)Sq3ResourceOf(pThis);
}
/*
 * The guard every SQLite3Result verb shares. Two different things reach it:
 * a result whose own finalize() has run, and one whose STATEMENT was closed
 * from the other side -- php tells them apart nowhere, and neither does this.
 */
static phl_sq3_res * Sq3LiveRes(ph7_context *pCtx,sxi32 *pRc)
{
	phl_sq3_res *pRes = Sq3ResOfInstance(PH7_ContextThis(pCtx));
	*pRc = PH7_OK;
	if( pRes == 0 || pRes->pSt == 0 || pRes->pSt->pStmt == 0 ){
		*pRc = Sq3Uninitialised(pCtx,"SQLite3Result");
		return 0;
	}
	return pRes;
}
/* The result object is going away: let go of the statement now. */
static void Sq3ResInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_sq3_res *pRes = Sq3ResOfInstance(pThis);
	SXUNUSED(pVm);
	if( pRes == 0 || pRes->pOwner != pThis ){
		return;
	}
	Sq3FreeRes(pRes);
}

/*
 * SQLite3::query(string $query): SQLite3Result|false
 *
 * Prepare, run once, rewind, and hand back a cursor. Only the FIRST statement
 * of the string is ever prepared, so `query('SELECT 1; garbage')` answers a
 * row and never sees the garbage -- unlike exec(), which runs the whole string
 * through sqlite3_exec and refuses on the tail.
 */
static int vm_builtin_SQLite3_query(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);
	const char *zSql;
	int nSql = 0;
	phl_sq3_stmt *pSt;
	ph7_class_instance *pObj;
	Sq3Verb sVerb;
	SXUNUSED(nArg);
	if( pConn == 0 ){
		return rc;
	}
	zSql = ph7_value_to_string(apArg[0],&nSql);
	Sq3VerbEnter(pConn,pCtx,"SQLite3::query",&sVerb);
	pSt = Sq3Prepare(pCtx,pConn,zSql,nSql,"SQLite3::query",&rc);
	if( pSt != 0 && !Sq3StepOnce(pCtx,pSt,"SQLite3::query",&rc) ){
		Sq3StmtUnref(pSt);
		pSt = 0;
	}
	{
		sxi32 rcCb = Sq3VerbLeave(pConn,&sVerb);
		if( rcCb != PH7_OK ){
			if( pSt ){
				Sq3StmtUnref(pSt);
			}
			ph7_result_bool(pCtx,0);
			return rcCb;
		}
	}
	if( pSt == 0 ){
		ph7_result_bool(pCtx,0);
		return rc;
	}
	pObj = Sq3NewResultObject(pCtx,pSt,0);
	if( pObj == 0 ){
		Sq3StmtUnref(pSt);
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/*
 * SQLite3::querySingle(string $query, bool $entireRow = false): mixed
 *
 * One statement, one step, and the statement is gone again -- there is no
 * cursor to hand back. What "no row" means depends on the shape the caller
 * asked for: a scalar query answers NULL and a whole-row query answers the
 * EMPTY ARRAY, which is why a write (which produces no row at all) answers
 * null rather than false.
 */
static int vm_builtin_SQLite3_querySingle(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);
	const char *zSql;
	int nSql = 0;
	int bWhole = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0;
	phl_sq3_stmt *pSt;
	Sq3Verb sVerb;
	int iStep;
	if( pConn == 0 ){
		return rc;
	}
	zSql = ph7_value_to_string(apArg[0],&nSql);
	Sq3VerbEnter(pConn,pCtx,"SQLite3::querySingle",&sVerb);
	pSt = Sq3Prepare(pCtx,pConn,zSql,nSql,"SQLite3::querySingle",&rc);
	iStep = (pSt && pSt->pStmt) ? sqlite3_step(pSt->pStmt) : SQLITE_MISUSE;
	{
		sxi32 rcCb = Sq3VerbLeave(pConn,&sVerb);
		if( rcCb != PH7_OK ){
			if( pSt ){
				Sq3StmtUnref(pSt);
			}
			ph7_result_bool(pCtx,0);
			return rcCb;
		}
	}
	if( pSt == 0 ){
		ph7_result_bool(pCtx,0);
		return rc;
	}
	if( iStep != SQLITE_ROW && iStep != SQLITE_DONE ){
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		SyBlobFormat(&sMsg,"Unable to execute statement: %s",sqlite3_errmsg(pConn->pDb));
		SyBlobNullAppend(&sMsg);
		ph7_result_bool(pCtx,0);
		rc = Sq3Error(pCtx,pConn,"SQLite3::querySingle",sqlite3_errcode(pConn->pDb),
			(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
		Sq3StmtUnref(pSt);
		return rc;
	}
	if( bWhole ){
		ph7_value *pRow = ph7_context_new_array(pCtx);
		ph7_value *pCell = ph7_context_new_scalar(pCtx);
		int i,nCol = iStep == SQLITE_ROW ? sqlite3_data_count(pSt->pStmt) : 0;
		if( pRow == 0 || pCell == 0 ){
			Sq3StmtUnref(pSt);
			return PH7_ContextMemoryError(pCtx);
		}
		for( i = 0 ; i < nCol ; ++i ){
			Sq3ColumnValue(pSt->pStmt,i,pCell);
			ph7_array_add_strkey_elem(pRow,sqlite3_column_name(pSt->pStmt,i),pCell);
		}
		ph7_result_value(pCtx,pRow);
	}else if( iStep == SQLITE_ROW && sqlite3_data_count(pSt->pStmt) > 0 ){
		ph7_value *pCell = ph7_context_new_scalar(pCtx);
		if( pCell == 0 ){
			Sq3StmtUnref(pSt);
			return PH7_ContextMemoryError(pCtx);
		}
		Sq3ColumnValue(pSt->pStmt,0,pCell);
		ph7_result_value(pCtx,pCell);
	}else{
		ph7_result_null(pCtx);
	}
	Sq3StmtUnref(pSt);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Copying a database, and reading one value as a stream
 * ------------------------------------------------------------------------ */
/*
 * SQLite3::backup(SQLite3 $destination, string $sourceDatabase = 'main',
 *   string $destinationDatabase = 'main'): bool
 *
 * sqlite's own online backup, run to completion in one call. What php does NOT
 * do is check the two names: `sqlite3_backup_init` answers nothing for a
 * database neither connection carries, and php reads that as having no work to
 * do rather than as a failure -- so a misspelt source is a quiet true and an
 * empty destination. The one refusal it words itself is the pair being the SAME
 * connection, which sqlite would deadlock on.
 */
static int vm_builtin_SQLite3_backup(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);
	phl_sq3 *pDest;
	const char *zSrc = "main",*zDest = "main";
	int nSrc = (int)sizeof("main")-1,nDest = (int)sizeof("main")-1;
	SyBlob sSrc,sDest;
	sqlite3_backup *pBackup;
	if( pConn == 0 ){
		return rc;
	}
	pDest = (apArg[0]->iFlags & MEMOBJ_OBJ) != 0
		? Sq3OfInstance((ph7_class_instance *)apArg[0]->x.pOther) : 0;
	if( pDest == 0 || pDest->pDb == 0 ){
		/* the destination is asked the same question the receiver was */
		return Sq3Uninitialised(pCtx,"SQLite3");
	}
	if( pDest == pConn ){
		ph7_result_bool(pCtx,0);
		return Sq3Error(pCtx,pConn,"SQLite3::backup",0,
			"Backup failed: source and destination must be distinct");
	}
	if( nArg > 1 ){
		zSrc = ph7_value_to_string(apArg[1],&nSrc);
	}
	if( nArg > 2 ){
		zDest = ph7_value_to_string(apArg[2],&nDest);
	}
	SyBlobInit(&sSrc,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sSrc,zSrc,(sxu32)nSrc);
	SyBlobNullAppend(&sSrc);
	SyBlobInit(&sDest,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sDest,zDest,(sxu32)nDest);
	SyBlobNullAppend(&sDest);
	pBackup = sqlite3_backup_init(pDest->pDb,(const char *)SyBlobData(&sDest),
		pConn->pDb,(const char *)SyBlobData(&sSrc));
	SyBlobRelease(&sSrc);
	SyBlobRelease(&sDest);
	if( pBackup == 0 ){
		/* nothing to copy, and php calls that done */
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	/* -1 pages is "all of it": one step, no progress callback to report to */
	sqlite3_backup_step(pBackup,-1);
	ph7_result_bool(pCtx,sqlite3_backup_finish(pBackup) == SQLITE_OK);
	return PH7_OK;
}
/*
 * One open blob: a HANDLE on the bytes of one value, which php hands back as a
 * stream. It addresses bytes that already exist -- there is no growing one and
 * nowhere past the end to seek to -- and sqlite refuses to close a database
 * while one is open, which is the one thing that makes SQLite3::close() fail.
 */
typedef struct phl_sq3_blob phl_sq3_blob;
struct phl_sq3_blob {
	sqlite3_blob *pBlob;          /* 0 once closed */
	ph7_vm *pVm;                  /* the allocator: a handle ORPHANED by its connection
	                               * still has to give its own memory back */
	phl_sq3 *pConn;               /* 0 once the connection closed underneath it */
	ph7_int64 iOfft;              /* the stream cursor: sqlite reads at an offset */
	ph7_int64 nSize;              /* the blob's length, fixed for its lifetime */
	int bWrite;                   /* the HANDLE sqlite opened is writable: flags & READWRITE */
	int iFlags;                   /* and the flags themselves, which the stream reads
	                               * separately -- php refuses a write for the READONLY bit
	                               * being SET rather than for READWRITE being absent, so the
	                               * two are different questions and 0 answers neither */
	int bBadPos;                  /* a seek OUT of the blob left the position unknown */
	io_private *pDev;             /* the stream the script holds: the read below marks its
	                               * end-of-file itself, since reaching the last byte is
	                               * what php calls eof here and a SEEK to the end is not */
	phl_sq3_blob *pNext;
};
static void Sq3BlobClose(phl_sq3_blob *pBl)
{
	phl_sq3 *pConn = pBl->pConn;
	if( pBl->pBlob ){
		sqlite3_blob_close(pBl->pBlob);
		pBl->pBlob = 0;
	}
	if( pConn ){
		phl_sq3_blob *pCur,*pPrev = 0;
		for( pCur = pConn->pBlobs ; pCur ; pPrev = pCur, pCur = pCur->pNext ){
			if( pCur == pBl ){
				if( pPrev ){
					pPrev->pNext = pCur->pNext;
				}else{
					pConn->pBlobs = pCur->pNext;
				}
				break;
			}
		}
	}
	SyMemBackendFree(&pBl->pVm->sAllocator,pBl);
}
/*
 * Close every handle a script left open and CUT them loose. Only the teardown
 * paths do this: a script's own close() is refused while one is open rather
 * than taking it away.
 */
static void Sq3BlobSweep(phl_sq3 *pConn)
{
	while( pConn->pBlobs ){
		phl_sq3_blob *pBl = pConn->pBlobs;
		pConn->pBlobs = pBl->pNext;
		pBl->pNext = 0;
		pBl->pConn = 0;
		if( pBl->pBlob ){
			sqlite3_blob_close(pBl->pBlob);
			pBl->pBlob = 0;
		}
	}
}
static ph7_int64 Sq3BlobStream_Read(void *pHandle,void *pBuf,ph7_int64 nDatatoRead)
{
	phl_sq3_blob *pBl = (phl_sq3_blob *)pHandle;
	ph7_int64 nLeft;
	if( pBl->pBlob == 0 || pBl->bBadPos ){
		return -1;
	}
	nLeft = pBl->nSize - pBl->iOfft;
	if( pBl->pDev && pBl->iOfft + nDatatoRead >= pBl->nSize ){
		/* php marks the end when a read REACHES the last byte, not when one
		 * comes back empty -- so reading a blob whole leaves feof() true, while
		 * seeking to the end leaves it false. */
		pBl->pDev->bEof = 1;
	}
	if( nLeft < 1 || nDatatoRead < 1 ){
		return 0;
	}
	if( nDatatoRead > nLeft ){
		nDatatoRead = nLeft;
	}
	if( sqlite3_blob_read(pBl->pBlob,pBuf,(int)nDatatoRead,(int)pBl->iOfft) != SQLITE_OK ){
		return -1;
	}
	pBl->iOfft += nDatatoRead;
	return nDatatoRead;
}
static ph7_int64 Sq3BlobStream_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)
{
	phl_sq3_blob *pBl = (phl_sq3_blob *)pHandle;
	if( pBl->pBlob == 0 || pBl->bBadPos ){
		return -1;
	}
	if( nWrite < 1 ){
		return 0;   /* php asks nothing at all of a write of nothing */
	}
	if( pBl->iFlags & SQLITE_OPEN_READONLY ){
		/* php's own sentence, from the device rather than from fwrite(), and it
		 * is asked FIRST. Note what it tests: the READONLY bit being SET, not
		 * READWRITE being absent -- so a handle opened with neither (0, or
		 * CREATE, which means nothing here) reaches the checks below and then
		 * fails SILENTLY in sqlite, which php reports as a plain false. */
		PH7_VmThrowError(pBl->pVm,0,PH7_CTX_WARNING,
			"fwrite(): Can't write to blob stream: is open as read only");
		return -1;
	}
	if( pBl->iOfft + nWrite > pBl->nSize ){
		/* the handle addresses the bytes that are already there: php refuses a
		 * write needing more of them rather than writing what fits */
		PH7_VmThrowError(pBl->pVm,0,PH7_CTX_WARNING,
			"fwrite(): It is not possible to increase the size of a BLOB");
		return -1;
	}
	if( sqlite3_blob_write(pBl->pBlob,pBuf,(int)nWrite,(int)pBl->iOfft) != SQLITE_OK ){
		return -1;
	}
	pBl->iOfft += nWrite;
	return nWrite;
}
static int Sq3BlobStream_Seek(void *pHandle,ph7_int64 iOfft,int whence)
{
	phl_sq3_blob *pBl = (phl_sq3_blob *)pHandle;
	ph7_int64 iNew = iOfft;
	if( whence == 1 ){          /* SEEK_CUR */
		iNew = pBl->iOfft + iOfft;
	}else if( whence == 2 ){    /* SEEK_END */
		iNew = pBl->nSize + iOfft;
	}
	if( iNew < 0 || iNew > pBl->nSize ){
		/* nowhere past the end to seek TO, and a failed seek leaves the position
		 * UNKNOWN -- ftell() answers false until a seek succeeds again */
		pBl->bBadPos = 1;
		return -1;
	}
	pBl->bBadPos = 0;
	pBl->iOfft = iNew;
	return PH7_OK;
}
static ph7_int64 Sq3BlobStream_Tell(void *pHandle)
{
	phl_sq3_blob *pBl = (phl_sq3_blob *)pHandle;
	return pBl->bBadPos ? -1 : pBl->iOfft;
}
/*
 * php's stat for a blob handle: the whole thirteen-field shape with a SIZE and
 * nothing else -- every other field is a zero rather than absent, which is what
 * makes fstat() answer the 26 entries (numeric and named) it answers for a
 * file.
 */
static int Sq3BlobStream_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)
{
	static const char *const azField[] = {
		"dev","ino","mode","nlink","uid","gid","rdev","size",
		"atime","mtime","ctime","blksize","blocks"
	};
	phl_sq3_blob *pBl = (phl_sq3_blob *)pHandle;
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(azField) ; ++n ){
		ph7_value_int64(pWorker,n == 7 /* size */ ? pBl->nSize : 0);
		ph7_array_add_strkey_elem(pArray,azField[n],pWorker);
	}
	return PH7_OK;
}
static void Sq3BlobStream_Close(void *pHandle)
{
	Sq3BlobClose((phl_sq3_blob *)pHandle);
}
static const ph7_io_stream sSq3BlobStream = {
	"SQLite3",                  /* what stream_get_meta_data() reports */
	PH7_IO_STREAM_VERSION,
	0,                          /* xOpen: openBlob() builds the handle itself */
	0,                          /* xOpenDir */
	Sq3BlobStream_Close,
	0,                          /* xCloseDir */
	Sq3BlobStream_Read,
	0,                          /* xReadDir */
	Sq3BlobStream_Write,
	Sq3BlobStream_Seek,
	0,                          /* xLock */
	0,                          /* xRewindDir */
	Sq3BlobStream_Tell,
	0,                          /* xTrunc: a blob is the length it was created with */
	0,                          /* xSync */
	Sq3BlobStream_Stat
};
/*
 * SQLite3::openBlob(string $table, string $column, int $rowid,
 *   string $database = 'main', int $flags = SQLITE3_OPEN_READONLY)
 *
 * The only flag read is READWRITE; everything else opens a read-only handle,
 * so SQLITE3_OPEN_CREATE is accepted and means nothing here. Every failure is
 * sqlite's own sentence behind php's `Unable to open blob: `, which is how a
 * missing row, a missing column, a missing table and a NULL value are told
 * apart.
 */
static int vm_builtin_SQLite3_openBlob(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);
	const char *zTable,*zColumn,*zDb = "main";
	int nTable = 0,nColumn = 0,nDb = (int)sizeof("main")-1;
	ph7_int64 iRow;
	int iFlags,bWrite;
	SyBlob sTable,sColumn,sDb;
	sqlite3_blob *pRaw = 0;
	phl_sq3_blob *pBl;
	io_private *pDev;
	if( pConn == 0 ){
		return rc;
	}
	zTable = ph7_value_to_string(apArg[0],&nTable);
	zColumn = ph7_value_to_string(apArg[1],&nColumn);
	iRow = ph7_value_to_int64(apArg[2]);
	if( nArg > 3 ){
		zDb = ph7_value_to_string(apArg[3],&nDb);
	}
	iFlags = nArg > 4 ? (int)ph7_value_to_int64(apArg[4]) : SQLITE_OPEN_READONLY;
	bWrite = (iFlags & SQLITE_OPEN_READWRITE) != 0;
	SyBlobInit(&sTable,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sTable,zTable,(sxu32)nTable);
	SyBlobNullAppend(&sTable);
	SyBlobInit(&sColumn,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sColumn,zColumn,(sxu32)nColumn);
	SyBlobNullAppend(&sColumn);
	SyBlobInit(&sDb,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sDb,zDb,(sxu32)nDb);
	SyBlobNullAppend(&sDb);
	rc = sqlite3_blob_open(pConn->pDb,(const char *)SyBlobData(&sDb),
		(const char *)SyBlobData(&sTable),(const char *)SyBlobData(&sColumn),
		(sqlite3_int64)iRow,bWrite,&pRaw);
	SyBlobRelease(&sTable);
	SyBlobRelease(&sColumn);
	SyBlobRelease(&sDb);
	if( rc != SQLITE_OK || pRaw == 0 ){
		SyBlob sMsg;
		sxi32 rcErr;
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		SyBlobFormat(&sMsg,"Unable to open blob: %s",sqlite3_errmsg(pConn->pDb));
		SyBlobNullAppend(&sMsg);
		ph7_result_bool(pCtx,0);
		rcErr = Sq3Error(pCtx,pConn,"SQLite3::openBlob",sqlite3_errcode(pConn->pDb),
			(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
		return rcErr;
	}
	pBl = (phl_sq3_blob *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_sq3_blob));
	if( pBl == 0 ){
		sqlite3_blob_close(pRaw);
		return PH7_ContextMemoryError(pCtx);
	}
	SyZero(pBl,sizeof(phl_sq3_blob));
	pBl->pBlob = pRaw;
	pBl->pVm = pCtx->pVm;
	pBl->pConn = pConn;
	pBl->nSize = (ph7_int64)sqlite3_blob_bytes(pRaw);
	pBl->bWrite = bWrite;
	pBl->iFlags = iFlags;
	pBl->pNext = pConn->pBlobs;
	pConn->pBlobs = pBl;
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	if( pDev == 0 ){
		Sq3BlobClose(pBl);
		return PH7_ContextMemoryError(pCtx);
	}
	InitIOPrivate(pCtx->pVm,&sSq3BlobStream,pDev);
	pDev->pHandle = pBl;
	pBl->pDev = pDev;
	/* php's meta reports no uri for this stream and the mode it opened with. */
	SetIOPrivateOpenedAs(pDev,"",0,bWrite ? "r+b" : "rb",bWrite ? 3 : 2);
	ph7_result_resource(pCtx,pDev);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Userland callbacks, called from inside sqlite's own loop
 * ------------------------------------------------------------------------ */
/*
 * One callback a script registered: a scalar function, an aggregate's step and
 * finalize pair, a collation, or the authorizer. The connection owns the
 * callable, because sqlite will call it long after the registering verb has
 * returned, and lets go of all of them only when it closes.
 */
typedef struct phl_sq3_udf phl_sq3_udf;
struct phl_sq3_udf {
	phl_sq3 *pConn;
	ph7_value *pCallback;
	ph7_value *pFinalize;         /* an aggregate's second half, or 0 */
	phl_sq3_udf *pNext;
};
/*
 * One aggregate in progress. sqlite hands the same scratch buffer to every step
 * of one group and then to the finalizer, which is where the running context
 * and the row count live -- both visible to the callbacks as their first two
 * arguments.
 */
typedef struct phl_sq3_agg phl_sq3_agg;
struct phl_sq3_agg {
	ph7_value *pCtx;              /* whatever the last step returned */
	int nRow;                     /* steps taken so far */
};
/* Register one callable on the connection, kept for the connection's lifetime. */
static phl_sq3_udf * Sq3NewUdf(phl_sq3 *pConn,ph7_value *pCallback,ph7_value *pFinalize)
{
	phl_sq3_udf *pUdf = (phl_sq3_udf *)SyMemBackendAlloc(&pConn->pVm->sAllocator,
		sizeof(phl_sq3_udf));
	if( pUdf == 0 ){
		return 0;
	}
	SyZero(pUdf,sizeof(phl_sq3_udf));
	pUdf->pConn = pConn;
	pUdf->pCallback = ph7_new_scalar(pConn->pVm);
	if( pUdf->pCallback == 0 ){
		SyMemBackendFree(&pConn->pVm->sAllocator,pUdf);
		return 0;
	}
	PH7_MemObjStore(pCallback,pUdf->pCallback);
	if( pFinalize ){
		pUdf->pFinalize = ph7_new_scalar(pConn->pVm);
		if( pUdf->pFinalize ){
			PH7_MemObjStore(pFinalize,pUdf->pFinalize);
		}
	}
	pUdf->pNext = pConn->pUdfs;
	pConn->pUdfs = pUdf;
	return pUdf;
}
static void Sq3UdfSweep(phl_sq3 *pConn)
{
	phl_sq3_udf *pUdf = pConn->pUdfs;
	while( pUdf ){
		phl_sq3_udf *pNext = pUdf->pNext;
		if( pUdf->pCallback ){
			ph7_release_value(pConn->pVm,pUdf->pCallback);
		}
		if( pUdf->pFinalize ){
			ph7_release_value(pConn->pVm,pUdf->pFinalize);
		}
		SyMemBackendFree(&pConn->pVm->sAllocator,pUdf);
		pUdf = pNext;
	}
	pConn->pUdfs = 0;
}
/*
 * The rule every trampoline here shares: sqlite is in the middle of a step when
 * the engine re-enters PHP, and a throw out of that PHP cannot travel back
 * through sqlite's C frames. So the status is PARKED on the connection, sqlite
 * is told to stop, and the verb that started the step raises exactly the parked
 * status once the library has unwound. A second callback while one is parked
 * does not re-enter PHP at all.
 */
static int Sq3UdfParked(phl_sq3_udf *pUdf)
{
	return pUdf->pConn->iCallbackExc != 0;
}
static void Sq3UdfPark(phl_sq3_udf *pUdf,sxi32 rc,sqlite3_context *pCtx)
{
	pUdf->pConn->iCallbackExc = PH7_CALLBACK_UNWOUND(rc) ? rc : PH7_EXCEPTION;
	if( pCtx ){
		sqlite3_result_error(pCtx,"PHL: callback raised",-1);
	}
}
/*
 * A diagnostic raised from INSIDE the library, in the name of the verb that is
 * running -- which is the only thing that knows it. php prints the method, so a
 * collation's complaint is `SQLite3::query(): ...` under one caller and
 * `SQLite3Result::fetchAll(): ...` under another.
 */
static void Sq3UdfWarn(phl_sq3 *pConn,const char *zMsg)
{
	if( pConn->pVerbCtx == 0 || pConn->zVerbFn == 0 ){
		return;
	}
	Sq3Error(pConn->pVerbCtx,pConn,pConn->zVerbFn,0,zMsg);
}
/* One sqlite value as a php value, in sqlite's own types. */
static void Sq3UdfArgValue(ph7_vm *pVm,sqlite3_value *pIn,ph7_value *pOut)
{
	PH7_MemObjInit(pVm,pOut);
	switch( sqlite3_value_type(pIn) ){
		case SQLITE_INTEGER:
			ph7_value_int64(pOut,(ph7_int64)sqlite3_value_int64(pIn));
			break;
#ifndef PH7_OMIT_FLOATING_POINT
		case SQLITE_FLOAT:
			ph7_value_double(pOut,(ph7_real)sqlite3_value_double(pIn));
			break;
#endif
		case SQLITE_NULL:
			ph7_value_null(pOut);
			break;
		case SQLITE_BLOB:
			ph7_value_string(pOut,(const char *)sqlite3_value_blob(pIn),
				sqlite3_value_bytes(pIn));
			break;
		default:
			ph7_value_string(pOut,(const char *)sqlite3_value_text(pIn),
				sqlite3_value_bytes(pIn));
			break;
	}
}
/*
 * What a callback RETURNED, as a sqlite result. null, int and float pass
 * straight through and everything else takes a string CAST -- so a bool comes
 * back as "1" and an array as "Array", with php's own conversion warning behind
 * it.
 *
 * The string is handed to sqlite as a C STRING, so it stops at the first NUL:
 * a function returning "a\0b" produces a one-byte value and one returning
 * "\0x" produces an empty one. php has the same cut.
 */
static void Sq3UdfResult(sqlite3_context *pCtx,ph7_value *pVal)
{
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) ){
		sqlite3_result_null(pCtx);
#ifndef PH7_OMIT_FLOATING_POINT
	}else if( pVal->iFlags & MEMOBJ_REAL ){
		sqlite3_result_double(pCtx,(double)ph7_value_to_double(pVal));
#endif
	}else if( pVal->iFlags & MEMOBJ_INT ){
		sqlite3_result_int64(pCtx,(sqlite3_int64)ph7_value_to_int64(pVal));
	}else{
		int nByte = 0,n;
		const char *zStr;
		PH7_MemObjToStringUV(pVal);
		zStr = ph7_value_to_string(pVal,&nByte);
		for( n = 0 ; n < nByte && zStr[n] ; ++n ){}
		sqlite3_result_text(pCtx,zStr ? zStr : "",n,SQLITE_TRANSIENT);
	}
}
/* A scalar function's body: build the arguments, call, convert the answer. */
static void Sq3UdfScalar(sqlite3_context *pCtx,int nArg,sqlite3_value **apVal)
{
	phl_sq3_udf *pUdf = (phl_sq3_udf *)sqlite3_user_data(pCtx);
	ph7_vm *pVm = pUdf->pConn->pVm;
	ph7_value *apArg[16];
	ph7_value sArgs[16];
	ph7_value sRes;
	int n,nCall = nArg;
	sxi32 rc;
	if( Sq3UdfParked(pUdf) ){
		sqlite3_result_error(pCtx,"PHL: callback raised",-1);
		return;
	}
	if( nCall > (int)SX_ARRAYSIZE(sArgs) ){
		nCall = (int)SX_ARRAYSIZE(sArgs);
	}
	for( n = 0 ; n < nCall ; ++n ){
		Sq3UdfArgValue(pVm,apVal[n],&sArgs[n]);
		apArg[n] = &sArgs[n];
	}
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,nCall,nCall ? apArg : 0,&sRes);
	if( rc != SXRET_OK ){
		Sq3UdfPark(pUdf,rc,pCtx);
	}else{
		Sq3UdfResult(pCtx,&sRes);
	}
	PH7_MemObjRelease(&sRes);
	for( n = 0 ; n < nCall ; ++n ){
		PH7_MemObjRelease(&sArgs[n]);
	}
}
/*
 * A collation: two strings in, an ordering out. Only an INT is an ordering --
 * a float, a numeric string, a bool, null and an array are each the same
 * complaint and a verdict of EQUAL, which leaves sqlite sorting by nothing.
 */
static int Sq3UdfCollate(void *pUser,int nLeft,const void *pLeft,int nRight,const void *pRight)
{
	phl_sq3_udf *pUdf = (phl_sq3_udf *)pUser;
	ph7_vm *pVm;
	ph7_value sL,sR,sRes;
	ph7_value *apArg[2];
	sxi32 rc;
	int iCmp = 0;
	if( pUdf == 0 || Sq3UdfParked(pUdf) ){
		return 0;
	}
	pVm = pUdf->pConn->pVm;
	PH7_MemObjInit(pVm,&sL);
	PH7_MemObjInit(pVm,&sR);
	ph7_value_string(&sL,(const char *)pLeft,nLeft);
	ph7_value_string(&sR,(const char *)pRight,nRight);
	apArg[0] = &sL;
	apArg[1] = &sR;
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,2,apArg,&sRes);
	if( rc != SXRET_OK ){
		/* no sqlite3_context here to fail through: park it and order the pair
		 * as equal, which leaves the walk to end on the parked status */
		Sq3UdfPark(pUdf,rc,0);
	}else if( (sRes.iFlags & MEMOBJ_INT) == 0 || (sRes.iFlags & MEMOBJ_REAL) != 0 ){
		Sq3UdfWarn(pUdf->pConn,
			"An error occurred while invoking the compare callback (invalid return type)."
			"  Collation behaviour is undefined.");
	}else{
		ph7_int64 iVal = ph7_value_to_int64(&sRes);
		iCmp = iVal < 0 ? -1 : (iVal > 0 ? 1 : 0);
	}
	PH7_MemObjRelease(&sRes);
	PH7_MemObjRelease(&sL);
	PH7_MemObjRelease(&sR);
	return iCmp;
}
/*
 * An aggregate's two halves. Both are given the running CONTEXT and a ROW
 * COUNT as their first two arguments, and whatever step returns becomes the
 * context of the next one. The count the STEP sees is 1-based; the one the
 * finalizer sees is always 0, which is php's own answer and not a count of
 * anything.
 */
static void Sq3UdfStep(sqlite3_context *pCtx,int nArg,sqlite3_value **apVal)
{
	phl_sq3_udf *pUdf = (phl_sq3_udf *)sqlite3_user_data(pCtx);
	ph7_vm *pVm = pUdf->pConn->pVm;
	phl_sq3_agg *pAgg;
	ph7_value sArgs[16];
	ph7_value *apArg[18];
	ph7_value sCount,sRes;
	int n,nCall = nArg;
	sxi32 rc;
	if( Sq3UdfParked(pUdf) ){
		sqlite3_result_error(pCtx,"PHL: callback raised",-1);
		return;
	}
	pAgg = (phl_sq3_agg *)sqlite3_aggregate_context(pCtx,(int)sizeof(phl_sq3_agg));
	if( pAgg == 0 ){
		return;
	}
	if( pAgg->pCtx == 0 ){
		pAgg->pCtx = ph7_new_scalar(pVm);
		pAgg->nRow = 0;
	}
	pAgg->nRow++;
	if( nCall > (int)SX_ARRAYSIZE(sArgs) ){
		nCall = (int)SX_ARRAYSIZE(sArgs);
	}
	PH7_MemObjInit(pVm,&sCount);
	ph7_value_int(&sCount,pAgg->nRow);
	apArg[0] = pAgg->pCtx;
	apArg[1] = &sCount;
	for( n = 0 ; n < nCall ; ++n ){
		Sq3UdfArgValue(pVm,apVal[n],&sArgs[n]);
		apArg[n + 2] = &sArgs[n];
	}
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,nCall + 2,apArg,&sRes);
	if( rc != SXRET_OK ){
		Sq3UdfPark(pUdf,rc,pCtx);
	}else if( pAgg->pCtx ){
		PH7_MemObjStore(&sRes,pAgg->pCtx);
	}
	PH7_MemObjRelease(&sRes);
	PH7_MemObjRelease(&sCount);
	for( n = 0 ; n < nCall ; ++n ){
		PH7_MemObjRelease(&sArgs[n]);
	}
}
static void Sq3UdfFinal(sqlite3_context *pCtx)
{
	phl_sq3_udf *pUdf = (phl_sq3_udf *)sqlite3_user_data(pCtx);
	ph7_vm *pVm = pUdf->pConn->pVm;
	phl_sq3_agg *pAgg = (phl_sq3_agg *)sqlite3_aggregate_context(pCtx,0);
	ph7_value sCtx,sCount,sRes;
	ph7_value *apArg[2];
	sxi32 rc;
	if( Sq3UdfParked(pUdf) ){
		sqlite3_result_error(pCtx,"PHL: callback raised",-1);
		return;
	}
	PH7_MemObjInit(pVm,&sCtx);
	PH7_MemObjInit(pVm,&sCount);
	if( pAgg && pAgg->pCtx ){
		PH7_MemObjStore(pAgg->pCtx,&sCtx);
	}
	ph7_value_int(&sCount,0);
	apArg[0] = &sCtx;
	apArg[1] = &sCount;
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pUdf->pFinalize ? pUdf->pFinalize : pUdf->pCallback,
		2,apArg,&sRes);
	if( rc != SXRET_OK ){
		Sq3UdfPark(pUdf,rc,pCtx);
	}else{
		Sq3UdfResult(pCtx,&sRes);
	}
	if( pAgg && pAgg->pCtx ){
		ph7_release_value(pVm,pAgg->pCtx);
		pAgg->pCtx = 0;
	}
	PH7_MemObjRelease(&sRes);
	PH7_MemObjRelease(&sCtx);
	PH7_MemObjRelease(&sCount);
}
/*
 * The authorizer: sqlite asks before it COMPILES each action, so a refusal here
 * stops a PREPARE rather than a step -- which is why a denied SELECT fails at
 * query() and never reaches a fetch. Its verdict is one of three ints and
 * nothing else will do: any other type is the complaint below and a denial,
 * as is a callback that threw.
 */
static int Sq3UdfAuthorize(void *pUser,int iAction,const char *z1,const char *z2,
	const char *z3,const char *z4)
{
	phl_sq3_udf *pUdf = (phl_sq3_udf *)pUser;
	ph7_vm *pVm;
	ph7_value sArgs[5],sRes;
	ph7_value *apArg[5];
	const char *azIn[4];
	int n,iVerdict = SQLITE_OK;
	sxi32 rc;
	if( pUdf == 0 || Sq3UdfParked(pUdf) ){
		return SQLITE_DENY;
	}
	pVm = pUdf->pConn->pVm;
	azIn[0] = z1; azIn[1] = z2; azIn[2] = z3; azIn[3] = z4;
	PH7_MemObjInit(pVm,&sArgs[0]);
	ph7_value_int(&sArgs[0],iAction);
	apArg[0] = &sArgs[0];
	for( n = 0 ; n < 4 ; ++n ){
		PH7_MemObjInit(pVm,&sArgs[n + 1]);
		if( azIn[n] ){
			ph7_value_string(&sArgs[n + 1],azIn[n],(int)SyStrlen(azIn[n]));
		}else{
			ph7_value_null(&sArgs[n + 1]);
		}
		apArg[n + 1] = &sArgs[n + 1];
	}
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pUdf->pCallback,5,apArg,&sRes);
	if( rc != SXRET_OK ){
		Sq3UdfPark(pUdf,rc,0);
		iVerdict = SQLITE_DENY;
	}else if( (sRes.iFlags & MEMOBJ_INT) == 0 || (sRes.iFlags & MEMOBJ_REAL) != 0 ){
		Sq3UdfWarn(pUdf->pConn,
			"The authorizer callback returned an invalid type: expected int");
		iVerdict = SQLITE_DENY;
	}else{
		ph7_int64 iVal = ph7_value_to_int64(&sRes);
		iVerdict = (iVal == SQLITE_IGNORE) ? SQLITE_IGNORE
			: ((iVal == SQLITE_OK) ? SQLITE_OK : SQLITE_DENY);
	}
	PH7_MemObjRelease(&sRes);
	for( n = 0 ; n < 5 ; ++n ){
		PH7_MemObjRelease(&sArgs[n]);
	}
	return iVerdict;
}
/*
 * The name every registering verb screens the same way: php refuses an EMPTY
 * name outright, and takes it as a C string, so one carrying a NUL registers
 * the part in front of it. The ARITY is not screened: php hands it straight to
 * sqlite, whose ceiling is the linked library's own (127 for years, higher in
 * newer builds), so the answer is the library's.
 */
static int Sq3UdfNameOk(ph7_context *pCtx,ph7_value *pName,SyBlob *pOut)
{
	const char *zName;
	int nName = 0;
	zName = ph7_value_to_string(pName,&nName);
	SyBlobInit(pOut,&pCtx->pVm->sAllocator);
	if( nName < 1 ){
		return 0;
	}
	SyBlobAppend(pOut,zName,(sxu32)nName);
	SyBlobNullAppend(pOut);
	return 1;
}
/*
 * SQLite3::createFunction(string $name, callable $callback, int $argCount = -1,
 *   int $flags = 0): bool
 */
static int vm_builtin_SQLite3_createFunction(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn;
	/* php screens the callable at ZPP time, so it is refused ahead of every
	 * question about the connection -- a closed one included. */
	{
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	pConn = Sq3LiveDb(pCtx,&rc);
	int nFuncArg = nArg > 2 ? (int)ph7_value_to_int64(apArg[2]) : -1;
	int iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3]) : 0;
	phl_sq3_udf *pUdf;
	SyBlob sName;
	if( pConn == 0 ){
		return rc;
	}
	if( !Sq3UdfNameOk(pCtx,apArg[0],&sName) ){
		SyBlobRelease(&sName);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pUdf = Sq3NewUdf(pConn,apArg[1],0);
	if( pUdf == 0 ){
		SyBlobRelease(&sName);
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_result_bool(pCtx,
		sqlite3_create_function_v2(pConn->pDb,(const char *)SyBlobData(&sName),nFuncArg,
			SQLITE_UTF8 | (iFlags & SQLITE_DETERMINISTIC),pUdf,
			Sq3UdfScalar,0,0,0) == SQLITE_OK);
	SyBlobRelease(&sName);
	return PH7_OK;
}
/*
 * SQLite3::createAggregate(string $name, callable $stepCallback,
 *   callable $finalCallback, int $argCount = -1): bool
 */
static int vm_builtin_SQLite3_createAggregate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn;
	/* php screens the callable at ZPP time, so it is refused ahead of every
	 * question about the connection -- a closed one included. */
	{
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"stepCallback",0);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	{
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[2],3,"finalCallback",0);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	pConn = Sq3LiveDb(pCtx,&rc);
	int nFuncArg = nArg > 3 ? (int)ph7_value_to_int64(apArg[3]) : -1;
	phl_sq3_udf *pUdf;
	SyBlob sName;
	if( pConn == 0 ){
		return rc;
	}
	if( !Sq3UdfNameOk(pCtx,apArg[0],&sName) ){
		SyBlobRelease(&sName);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pUdf = Sq3NewUdf(pConn,apArg[1],apArg[2]);
	if( pUdf == 0 ){
		SyBlobRelease(&sName);
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_result_bool(pCtx,
		sqlite3_create_function_v2(pConn->pDb,(const char *)SyBlobData(&sName),nFuncArg,
			SQLITE_UTF8,pUdf,0,Sq3UdfStep,Sq3UdfFinal,0) == SQLITE_OK);
	SyBlobRelease(&sName);
	return PH7_OK;
}
/* SQLite3::createCollation(string $name, callable $callback): bool */
static int vm_builtin_SQLite3_createCollation(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn;
	/* php screens the callable at ZPP time, so it is refused ahead of every
	 * question about the connection -- a closed one included. */
	{
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	pConn = Sq3LiveDb(pCtx,&rc);
	phl_sq3_udf *pUdf;
	SyBlob sName;
	SXUNUSED(nArg);
	if( pConn == 0 ){
		return rc;
	}
	if( !Sq3UdfNameOk(pCtx,apArg[0],&sName) ){
		SyBlobRelease(&sName);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pUdf = Sq3NewUdf(pConn,apArg[1],0);
	if( pUdf == 0 ){
		SyBlobRelease(&sName);
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_result_bool(pCtx,
		sqlite3_create_collation_v2(pConn->pDb,(const char *)SyBlobData(&sName),SQLITE_UTF8,
			pUdf,Sq3UdfCollate,0) == SQLITE_OK);
	SyBlobRelease(&sName);
	return PH7_OK;
}
/*
 * SQLite3::setAuthorizer(?callable $callback): bool
 *
 * One at a time: a second call replaces the first, and NULL takes it off.
 */
static int vm_builtin_SQLite3_setAuthorizer(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn;
	phl_sq3_udf *pUdf;
	SXUNUSED(nArg);
	if( !ph7_value_is_null(apArg[0]) ){
		rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",1);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	pConn = Sq3LiveDb(pCtx,&rc);
	if( pConn == 0 ){
		return rc;
	}
	if( ph7_value_is_null(apArg[0]) ){
		sqlite3_set_authorizer(pConn->pDb,0,0);
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	pUdf = Sq3NewUdf(pConn,apArg[0],0);
	if( pUdf == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_result_bool(pCtx,
		sqlite3_set_authorizer(pConn->pDb,Sq3UdfAuthorize,pUdf) == SQLITE_OK);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * SQLite3Stmt -- a statement a script holds itself
 * ------------------------------------------------------------------------ */
/*
 * One parameter a script bound before execute().
 *
 * The bindings are php's, not sqlite's: they are recorded here and applied at
 * every execute, which is what makes bindParam() read its variable LATE -- the
 * value the statement runs with is whatever the variable holds when execute()
 * is called, not when the binding was made.
 */
typedef struct phl_sq3_bind phl_sq3_bind;
struct phl_sq3_bind {
	int iPos;                     /* 1-based; a NAME is resolved at bind time */
	char *zName;                  /* the name the script spelled, or 0 for a positional bind:
	                               * php keys its table by NAME for one and by POSITION for
	                               * the other, so `:a` and `1` naming the same parameter are
	                               * two bindings that are both applied */
	int nName;
	int iType;                    /* SQLITE3_* -- the declared one, or the value's own */
	sxu32 nSlot;                  /* bindParam: the caller's memobj index (SXU32_HIGH = none) */
	ph7_value *pVal;              /* bindValue: this statement's own copy */
	phl_sq3_bind *pNext;
};
/* Drop every binding of one statement. */
static void Sq3BindsClear(phl_sq3_stmt *pSt)
{
	phl_sq3_bind *pB = pSt->pBinds;
	while( pB ){
		phl_sq3_bind *pNext = pB->pNext;
		if( pB->pVal ){
			ph7_release_value(pSt->pConn->pVm,pB->pVal);
		}
		if( pB->zName ){
			SyMemBackendFree(&pSt->pConn->pVm->sAllocator,pB->zName);
		}
		SyMemBackendFree(&pSt->pConn->pVm->sAllocator,pB);
		pB = pNext;
	}
	pSt->pBinds = 0;
}
/*
 * Record one binding, replacing whatever the same KEY already held -- and the
 * key is the name for a named bind and the position for a positional one, so
 * re-binding `:a` overwrites the earlier `:a` while `bindValue(1,...)` beside it
 * is a second entry that also runs.
 *
 * New entries go on the END: php's table keeps insertion order and applies them
 * in it, which is the order any diagnostics come out in.
 */
static phl_sq3_bind * Sq3BindSlot(phl_sq3_stmt *pSt,int iPos,const char *zName,int nName)
{
	ph7_vm *pVm = pSt->pConn->pVm;
	phl_sq3_bind *pB,*pPrev = 0,*pTail = 0;
	for( pB = pSt->pBinds ; pB ; pPrev = pB, pB = pB->pNext ){
		int bSame = zName
			? (pB->zName != 0 && pB->nName == nName
			   && SyMemcmp(pB->zName,zName,(sxu32)nName) == 0)
			: (pB->zName == 0 && pB->iPos == iPos);
		if( bSame ){
			/* php replaces a binding by REMOVING the old entry and adding the
			 * new one, so re-binding a key moves it to the end of the run --
			 * which is the order any failures are reported in. */
			if( pB->pVal ){
				ph7_release_value(pVm,pB->pVal);
				pB->pVal = 0;
			}
			if( pPrev ){
				pPrev->pNext = pB->pNext;
			}else{
				pSt->pBinds = pB->pNext;
			}
			pB->pNext = 0;
			pB->iPos = iPos;
			pB->nSlot = SXU32_HIGH;
			break;
		}
	}
	if( pB == 0 ){
		pB = (phl_sq3_bind *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_sq3_bind));
		if( pB == 0 ){
			return 0;
		}
		SyZero(pB,sizeof(phl_sq3_bind));
		pB->iPos = iPos;
		pB->nSlot = SXU32_HIGH;
		if( zName && nName > 0 ){
			pB->zName = (char *)SyMemBackendDup(&pVm->sAllocator,zName,(sxu32)nName);
			if( pB->zName == 0 ){
				SyMemBackendFree(&pVm->sAllocator,pB);
				return 0;
			}
			pB->nName = nName;
		}
	}
	for( pTail = pSt->pBinds ; pTail && pTail->pNext ; pTail = pTail->pNext ){}
	if( pTail ){
		pTail->pNext = pB;
	}else{
		pSt->pBinds = pB;
	}
	return pB;
}
/*
 * Which position a `string|int $param` names.
 *
 * An INT is the position itself, and 0 is the one php refuses outright -- every
 * other number is accepted here and only fails when execute() tries to bind it.
 * A NAME is looked up in the statement, first as the script spelled it and then
 * with a `:` in front, so `:a` and `a` both find the same parameter while `@a`
 * finds nothing. Answers 0 for a name the statement does not carry.
 */
static int Sq3BindPosition(phl_sq3_stmt *pSt,ph7_value *pParam)
{
	const char *zName;
	int nName = 0;
	SyBlob sName;
	int iPos;
	if( (pParam->iFlags & MEMOBJ_STRING) == 0 ){
		return (int)ph7_value_to_int64(pParam);
	}
	zName = ph7_value_to_string(pParam,&nName);
	SyBlobInit(&sName,&pSt->pConn->pVm->sAllocator);
	SyBlobAppend(&sName,zName,(sxu32)nName);
	SyBlobNullAppend(&sName);
	iPos = sqlite3_bind_parameter_index(pSt->pStmt,(const char *)SyBlobData(&sName));
	if( iPos == 0 ){
		SyBlobReset(&sName);
		SyBlobAppend(&sName,":",sizeof(char));
		SyBlobAppend(&sName,zName,(sxu32)nName);
		SyBlobNullAppend(&sName);
		iPos = sqlite3_bind_parameter_index(pSt->pStmt,(const char *)SyBlobData(&sName));
	}
	SyBlobRelease(&sName);
	return iPos;
}
/*
 * php's type for a value nobody declared one for: the zval's own kind, with
 * bool counting as an integer and everything that is not a number or null
 * counting as text.
 */
static int Sq3TypeOfValue(ph7_value *pVal)
{
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) != 0 ){
		return SQLITE_NULL;
	}
	/* REAL is asked first: a PHL float carries the integer flag beside it, so
	 * the other order makes 0.0 an integer. */
	if( (pVal->iFlags & MEMOBJ_REAL) != 0 ){
		return SQLITE_FLOAT;
	}
	if( (pVal->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL)) != 0 ){
		return SQLITE_INTEGER;
	}
	return SQLITE3_TEXT;
}
/*
 * Hand one recorded binding to sqlite, converting the value the way the
 * declared type asks for. A NULL value is bound as NULL whatever the type says
 * -- php asks that question first -- and every other type is php's own cast,
 * so `bindValue(1,"12abc",SQLITE3_INTEGER)` binds 12 and a bool bound as text
 * binds "1" or "".
 */
static int Sq3BindApply(ph7_context *pCtx,phl_sq3_stmt *pSt,phl_sq3_bind *pB,
	ph7_value *pVal)
{
	sqlite3_stmt *pStmt = pSt->pStmt;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) != 0 || pB->iType == SQLITE_NULL ){
		return sqlite3_bind_null(pStmt,pB->iPos);
	}
	switch( pB->iType ){
		case SQLITE_INTEGER:
			return sqlite3_bind_int64(pStmt,pB->iPos,
				(sqlite3_int64)ph7_value_to_int64(pVal));
#ifndef PH7_OMIT_FLOATING_POINT
		case SQLITE_FLOAT:
			return sqlite3_bind_double(pStmt,pB->iPos,ph7_value_to_double(pVal));
#endif
		case SQLITE_BLOB:
		case SQLITE3_TEXT: {
			/* php's own conversion, with its own diagnostics -- an array is the
			 * `Array to string conversion` warning and the string "Array", an
			 * object with no __toString the catchable Error. It runs on a COPY:
			 * bindParam names the caller's variable and php never rewrites it. */
			ph7_value sTmp;
			const char *zVal = 0;
			int nVal = 0, rcBind;
			PH7_MemObjInit(pCtx->pVm,&sTmp);
			PH7_MemObjStore(pVal,&sTmp);
			if( PH7_ValueToStringUV(pCtx,&sTmp,&zVal,&nVal) != SXRET_OK ){
				PH7_MemObjRelease(&sTmp);
				return SQLITE_OK;   /* the throw is already parked on the context */
			}
			if( pB->iType == SQLITE_BLOB ){
				rcBind = sqlite3_bind_blob(pStmt,pB->iPos,zVal ? zVal : "",nVal,
					SQLITE_TRANSIENT);
			}else{
				rcBind = sqlite3_bind_text(pStmt,pB->iPos,zVal ? zVal : "",nVal,
					SQLITE_TRANSIENT);
			}
			PH7_MemObjRelease(&sTmp);
			return rcBind;
		}
		default:
			break;
	}
	return -1;   /* a type php has no case for; the caller reports it */
}
/*
 * Apply every binding, in the order they were MADE. A failure is reported and
 * the run goes ON -- php reports the bind and steps the statement anyway, so a
 * parameter that could not be bound is simply the NULL it already was.
 */
static sxi32 Sq3BindsApply(ph7_context *pCtx,phl_sq3_stmt *pSt,const char *zFn)
{
	phl_sq3_bind *pB;
	sxi32 rc = PH7_OK;
	for( pB = pSt->pBinds ; pB ; pB = pB->pNext ){
		ph7_value *pVal = pB->pVal;
		int rcBind;
		if( pB->nSlot != SXU32_HIGH ){
			/* bindParam: the caller's variable, read HERE rather than at bind */
			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pB->nSlot);
		}
		rcBind = Sq3BindApply(pCtx,pSt,pB,pVal);
		if( pCtx->nThrowRc != PH7_OK ){
			return pCtx->nThrowRc;   /* the conversion threw; php propagates it too */
		}
		if( rcBind < 0 ){
			/* A type php has no case for. It is a programming error rather than
			 * a database one, so it stops the run instead of being reported and
			 * carried past -- and it is the ONE answer here that cannot be
			 * checked against php, whose own sentence for it carries a printf
			 * modifier its engine no longer supports: naming an unknown type
			 * ENDS the request there rather than printing anything. The text is
			 * php's with the two numbers filled in. */
			return PH7_VmThrowException(pCtx,"Error",
				"Unknown parameter type: %d for parameter %d",pB->iType,pB->iPos);
		}
		if( rcBind != SQLITE_OK ){
			SyBlob sMsg;
			SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
			SyBlobFormat(&sMsg,"Unable to bind parameter number %d",pB->iPos);
			SyBlobNullAppend(&sMsg);
			rc = Sq3Error(pCtx,pSt->pConn,zFn,rcBind,(const char *)SyBlobData(&sMsg));
			SyBlobRelease(&sMsg);
			if( rc != PH7_OK ){
				return rc;
			}
		}
	}
	return rc;
}
static phl_sq3_stmt * Sq3StmtOfInstance(ph7_class_instance *pThis)
{
	return (phl_sq3_stmt *)Sq3ResourceOf(pThis);
}
/*
 * php asks TWO questions about a statement and words them with two different
 * class names, which is visible to a script.
 *
 * The FIRST is whether the statement is attached to a live connection at all --
 * php's macro is given the DATABASE object there, so a never-prepared object
 * and a closed statement are both `The SQLite3 object ...`. execute() and
 * close() ask only this one, which is why close() succeeds on a statement of
 * NOTHING and a second close() is the Error.
 */
static phl_sq3_stmt * Sq3LiveStmtNull(ph7_context *pCtx,sxi32 *pRc)
{
	phl_sq3_stmt *pSt = Sq3StmtOfInstance(PH7_ContextThis(pCtx));
	*pRc = PH7_OK;
	if( pSt == 0 || !pSt->bInitialised ){
		*pRc = Sq3Uninitialised(pCtx,"SQLite3");
		return 0;
	}
	return pSt;
}
/*
 * The SECOND is whether there is a handle to work on, and it names the
 * STATEMENT class -- so a comment-only prepare, which php keeps as an object
 * with no handle, answers `The SQLite3Stmt object ...` to every accessor.
 */
static phl_sq3_stmt * Sq3LiveStmt(ph7_context *pCtx,sxi32 *pRc)
{
	phl_sq3_stmt *pSt = Sq3LiveStmtNull(pCtx,pRc);
	if( pSt == 0 ){
		return 0;
	}
	if( pSt->pStmt == 0 ){
		*pRc = Sq3Uninitialised(pCtx,"SQLite3Stmt");
		return 0;
	}
	return pSt;
}
/* The statement object is going away: let go of its hold on the statement. */
static void Sq3StmtInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_sq3_stmt *pSt = Sq3StmtOfInstance(pThis);
	SXUNUSED(pVm);
	if( pSt == 0 || pSt->pOwner != pThis ){
		return;
	}
	pSt->pOwner = 0;
	Sq3StmtUnref(pSt);
}
/*
 * SQLite3Stmt::__construct(SQLite3 $sqlite3, string $query) -- private, and it
 * never runs: prepare() builds every statement itself.
 */
static int vm_builtin_SQLite3Stmt_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * SQLite3::prepare(string $query): SQLite3Stmt|false
 *
 * Compiles without running. A query that holds no statement still answers an
 * OBJECT -- one whose handle is NULL, so every accessor on it is the Error and
 * only close() answers -- while the EMPTY query is the silent false.
 */
static int vm_builtin_SQLite3_prepare(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);
	const char *zSql;
	int nSql = 0;
	phl_sq3_stmt *pSt;
	ph7_class *pClass;
	ph7_class_instance *pObj;
	Sq3Verb sVerb;
	SXUNUSED(nArg);
	if( pConn == 0 ){
		return rc;
	}
	zSql = ph7_value_to_string(apArg[0],&nSql);
	Sq3VerbEnter(pConn,pCtx,"SQLite3::prepare",&sVerb);
	pSt = Sq3Prepare(pCtx,pConn,zSql,nSql,"SQLite3::prepare",&rc);
	{
		/* the AUTHORIZER runs while sqlite compiles, so even a prepare can be
		 * the call a callback threw out of */
		sxi32 rcCb = Sq3VerbLeave(pConn,&sVerb);
		if( rcCb != PH7_OK ){
			if( pSt ){
				Sq3StmtUnref(pSt);
			}
			ph7_result_bool(pCtx,0);
			return rcCb;
		}
	}
	if( pSt == 0 ){
		ph7_result_bool(pCtx,0);
		return rc;
	}
	pClass = PH7_VmExtractClass(pCtx->pVm,"SQLite3Stmt",sizeof("SQLite3Stmt")-1,FALSE,0);
	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
	if( pObj == 0 || Sq3AttachRes(pObj,pSt) != 0 ){
		if( pObj ){
			PH7_ClassInstanceUnref(pObj);
		}
		Sq3StmtUnref(pSt);
		return PH7_ContextMemoryError(pCtx);
	}
	pSt->pOwner = pObj;
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/*
 * SQLite3Stmt::execute(): SQLite3Result|false
 *
 * The bindings are applied, the statement is run once and rewound, and the
 * result walks the SAME statement -- so two results handed out by one
 * statement are two views of one cursor, and executing again rewinds both.
 *
 * A statement of NOTHING has no handle to step, and php asks the NULL one for
 * its database rather than the connection it came from: what sqlite says about
 * no connection at all is `out of memory`, and that is the message.
 */
static int vm_builtin_SQLite3Stmt_execute(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmtNull(pCtx,&rc);
	ph7_class_instance *pObj;
	Sq3Verb sVerb;
	int iStep;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return rc;
	}
	if( pSt->pStmt == 0 ){
		ph7_result_bool(pCtx,0);
		return Sq3Error(pCtx,pSt->pConn,"SQLite3Stmt::execute",0,
			"Unable to execute statement: out of memory");
	}
	sqlite3_reset(pSt->pStmt);
	Sq3VerbEnter(pSt->pConn,pCtx,"SQLite3Stmt::execute",&sVerb);
	rc = Sq3BindsApply(pCtx,pSt,"SQLite3Stmt::execute");
	iStep = rc == PH7_OK ? sqlite3_step(pSt->pStmt) : SQLITE_OK;
	{
		sxi32 rcCb = Sq3VerbLeave(pSt->pConn,&sVerb);
		if( rcCb != PH7_OK ){
			ph7_result_bool(pCtx,0);
			return rcCb;
		}
	}
	if( rc != PH7_OK ){
		ph7_result_bool(pCtx,0);
		return rc;
	}
	if( iStep != SQLITE_ROW && iStep != SQLITE_DONE ){
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		SyBlobFormat(&sMsg,"Unable to execute statement: %s",sqlite3_errmsg(pSt->pConn->pDb));
		SyBlobNullAppend(&sMsg);
		ph7_result_bool(pCtx,0);
		rc = Sq3Error(pCtx,pSt->pConn,"SQLite3Stmt::execute",
			sqlite3_errcode(pSt->pConn->pDb),(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
		return rc;
	}
	sqlite3_reset(pSt->pStmt);
	pSt->nRef++;   /* the result is a SECOND holder: the object keeps its own */
	pObj = Sq3NewResultObject(pCtx,pSt,pSt->pOwner);
	if( pObj == 0 ){
		Sq3StmtUnref(pSt);
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/* SQLite3Stmt::bindValue(string|int $param, mixed $value, int $type = SQLITE3_TEXT): bool
 * SQLite3Stmt::bindParam(string|int $param, mixed &$var, int $type = SQLITE3_TEXT): bool
 *
 * The two differ in WHEN the value is read and in what an omitted $type means.
 * bindValue copies the value now and, with no type given, takes the type from
 * that value; bindParam records the caller's SLOT and reads it at execute --
 * where there is no value yet to take a type from, so the declared default
 * stands and an unqualified bindParam() binds TEXT even for an integer.
 */
static int Sq3BindOne(ph7_context *pCtx,int nArg,ph7_value **apArg,int bByRef)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);
	phl_sq3_bind *pB;
	const char *zName = 0;
	int nName = 0, iPos;
	if( pSt == 0 ){
		return rc;
	}
	iPos = Sq3BindPosition(pSt,apArg[0]);
	if( iPos < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( (apArg[0]->iFlags & MEMOBJ_STRING) != 0 ){
		zName = ph7_value_to_string(apArg[0],&nName);
	}
	pB = Sq3BindSlot(pSt,iPos,zName,nName);
	if( pB == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( nArg > 2 ){
		pB->iType = (int)ph7_value_to_int64(apArg[2]);
	}else{
		pB->iType = bByRef ? SQLITE3_TEXT : Sq3TypeOfValue(apArg[1]);
	}
	if( bByRef ){
		pB->nSlot = apArg[1]->nIdx;
	}else{
		/* the copy has to OUTLIVE this call, so it is the VM's rather than the
		 * context's -- a context value dies with the call that made it */
		pB->pVal = ph7_new_scalar(pCtx->pVm);
		if( pB->pVal == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		PH7_MemObjStore(apArg[1],pB->pVal);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_SQLite3Stmt_bindValue(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return Sq3BindOne(pCtx,nArg,apArg,0);
}
static int vm_builtin_SQLite3Stmt_bindParam(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return Sq3BindOne(pCtx,nArg,apArg,1);
}
/* SQLite3Stmt::clear(): bool -- drop the bindings, php's and sqlite's both. */
static int vm_builtin_SQLite3Stmt_clear(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return rc;
	}
	Sq3BindsClear(pSt);
	ph7_result_bool(pCtx,sqlite3_clear_bindings(pSt->pStmt) == SQLITE_OK);
	return PH7_OK;
}
/*
 * SQLite3Stmt::close(): true
 *
 * Finalizes NOW, whatever results are still walking it -- they start answering
 * the Error. A second close() is the Error too, since there is no handle left
 * to close.
 */
static int vm_builtin_SQLite3Stmt_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmtNull(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return rc;
	}
	if( pSt->pStmt ){
		sqlite3_finalize(pSt->pStmt);
		pSt->pStmt = 0;
	}
	pSt->bInitialised = 0;
	Sq3BindsClear(pSt);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* SQLite3Stmt::reset(): bool -- rewind, keeping the bindings. */
static int vm_builtin_SQLite3Stmt_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return rc;
	}
	ph7_result_bool(pCtx,sqlite3_reset(pSt->pStmt) == SQLITE_OK);
	return PH7_OK;
}
/* SQLite3Stmt::paramCount(): int */
static int vm_builtin_SQLite3Stmt_paramCount(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return rc;
	}
	ph7_result_int64(pCtx,(ph7_int64)sqlite3_bind_parameter_count(pSt->pStmt));
	return PH7_OK;
}
/* SQLite3Stmt::readOnly(): bool -- whether running it can change the database. */
static int vm_builtin_SQLite3Stmt_readOnly(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return rc;
	}
	ph7_result_bool(pCtx,sqlite3_stmt_readonly(pSt->pStmt) != 0);
	return PH7_OK;
}
/* SQLite3Stmt::busy(): bool -- whether a walk is under way on it. */
static int vm_builtin_SQLite3Stmt_busy(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return rc;
	}
	ph7_result_bool(pCtx,sqlite3_stmt_busy(pSt->pStmt) != 0);
	return PH7_OK;
}
/*
 * SQLite3Stmt::getSQL(bool $expand = false): string|false
 *
 * The text as it was PREPARED, or -- expanded -- the same text with every
 * bound parameter written into it, which is sqlite's own rendering and not a
 * substitution php performs.
 */
static int vm_builtin_SQLite3Stmt_getSQL(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);
	int bExpand = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;
	const char *zSql;
	if( pSt == 0 ){
		return rc;
	}
	/* php binds FIRST and asks about $expand afterwards -- the same helper
	 * execute() uses -- so the rendering shows what the statement would run
	 * with, including the value a bindParam()ed variable holds right now, and
	 * even the UNEXPANDED spelling reports a binding that could not be applied.
	 * A failed bind is reported and the answer is produced without it. */
	rc = Sq3BindsApply(pCtx,pSt,"SQLite3Stmt::getSQL");
	if( rc != PH7_OK ){
		return rc;
	}
	if( bExpand ){
		char *zExp = sqlite3_expanded_sql(pSt->pStmt);
		if( zExp == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_string(pCtx,zExp,-1);
		sqlite3_free(zExp);
		return PH7_OK;
	}
	zSql = sqlite3_sql(pSt->pStmt);
	if( zSql == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,zSql,-1);
	return PH7_OK;
}
/* SQLite3Stmt::explain(): int -- which of the three plans the statement runs. */
static int vm_builtin_SQLite3Stmt_explain(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSt == 0 ){
		return rc;
	}
	ph7_result_int64(pCtx,(ph7_int64)sqlite3_stmt_isexplain(pSt->pStmt));
	return PH7_OK;
}
/*
 * SQLite3Stmt::setExplain(int $mode): bool
 *
 * Re-aims the SAME statement at its own query plan: mode 1 makes it answer the
 * eight columns of EXPLAIN and mode 2 the four of EXPLAIN QUERY PLAN, and mode
 * 0 puts it back. Anything else is a ValueError naming the constants.
 */
static int vm_builtin_SQLite3Stmt_setExplain(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_stmt *pSt = Sq3LiveStmt(pCtx,&rc);
	ph7_int64 iMode;
	SXUNUSED(nArg);
	if( pSt == 0 ){
		return rc;
	}
	iMode = ph7_value_to_int64(apArg[0]);
	if( iMode < 0 || iMode > 2 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"SQLite3Stmt::setExplain(): Argument #1 ($mode) must be one of the "
			"SQLite3Stmt::EXPLAIN_MODE_* constants");
	}
	ph7_result_bool(pCtx,sqlite3_stmt_explain(pSt->pStmt,(int)iMode) == SQLITE_OK);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * SQLite3Result -- the cursor
 * ------------------------------------------------------------------------ */
/*
 * SQLite3Result::__construct() -- private, and it never runs: the extension
 * builds every result itself. The body stands only so the declaration php makes
 * exists to be reflected.
 */
static int vm_builtin_SQLite3Result_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/* SQLite3Result::numColumns(): int -- what the STATEMENT declares, so a write
 * answers 0 and a SELECT answers its width before any row has been read. */
static int vm_builtin_SQLite3Result_numColumns(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pRes == 0 ){
		return rc;
	}
	ph7_result_int64(pCtx,(ph7_int64)sqlite3_column_count(pRes->pSt->pStmt));
	return PH7_OK;
}
/* SQLite3Result::columnName(int $column): string|false -- also from the
 * statement, so it answers with no row up. */
static int vm_builtin_SQLite3Result_columnName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);
	int iCol;
	const char *zName;
	SXUNUSED(nArg);
	if( pRes == 0 ){
		return rc;
	}
	iCol = (int)ph7_value_to_int64(apArg[0]);
	/* sqlite answers NULL for a column that is not there, and false is what php
	 * makes of that -- no range check of php's own. */
	zName = sqlite3_column_name(pRes->pSt->pStmt,iCol);
	if( zName == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,zName,-1);
	return PH7_OK;
}
/*
 * SQLite3Result::columnType(int $column): int|false
 *
 * A type belongs to a VALUE, not to a column, so this one answers only while a
 * row is UP: sqlite's data_count is the width of the row it is holding, and it
 * is 0 before the first fetch and 0 again once the walk has run out. The width
 * columnName() answers from does not move that way.
 *
 * There is no range check beyond that gate. A column past the row's width --
 * and a NEGATIVE one -- is SQLITE3_NULL rather than false, and asking leaves
 * `column index out of range` on the CONNECTION, which the same question to
 * columnName() never does.
 */
static int vm_builtin_SQLite3Result_columnType(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);
	SXUNUSED(nArg);
	if( pRes == 0 ){
		return rc;
	}
	if( sqlite3_data_count(pRes->pSt->pStmt) < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,
		(ph7_int64)sqlite3_column_type(pRes->pSt->pStmt,(int)ph7_value_to_int64(apArg[0])));
	return PH7_OK;
}
/*
 * One row into pOut, in the shape $mode asks for. The two bits are read
 * independently and both may be set, so a BOTH row carries each column twice --
 * position first, name second, column by column.
 */
static void Sq3RowInto(sqlite3_stmt *pStmt,int iMode,ph7_value *pOut,ph7_value *pCell)
{
	int i,nCol = sqlite3_data_count(pStmt);
	for( i = 0 ; i < nCol ; ++i ){
		Sq3ColumnValue(pStmt,i,pCell);
		if( iMode & 2 /* SQLITE3_NUM */ ){
			ph7_array_add_intkey_elem(pOut,i,pCell);
		}
		if( iMode & 1 /* SQLITE3_ASSOC */ ){
			ph7_array_add_strkey_elem(pOut,sqlite3_column_name(pStmt,i),pCell);
		}
	}
}
/*
 * Step once for a script. Nothing latches a finished flag and nothing resets:
 * a statement prepared with prepare_v2 rewinds ITSELF when it is stepped after
 * SQLITE_DONE, which is why a walk that has run out starts over on the next
 * call and why fetchAll() may be asked twice.
 *
 * Leaving the reset out is visible from the other side of the connection: the
 * end of a walk is SQLITE_DONE and stays on the handle, so lastErrorCode()
 * reads 101 and lastErrorMsg() `no more rows available` after the fetch that
 * ran out. An explicit reset would clear both back to "not an error".
 *
 * Answers 1 for a row, 0 for the end, -1 for a failure already reported.
 */
static int Sq3StepFetch(ph7_context *pCtx,phl_sq3_res *pRes,const char *zFn,sxi32 *pRc)
{
	int rc = sqlite3_step(pRes->pSt->pStmt);
	*pRc = PH7_OK;
	if( rc == SQLITE_ROW ){
		return 1;
	}
	if( rc == SQLITE_DONE ){
		return 0;
	}
	{
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		SyBlobFormat(&sMsg,"Unable to execute statement: %s",
			sqlite3_errmsg(pRes->pSt->pConn->pDb));
		SyBlobNullAppend(&sMsg);
		*pRc = Sq3Error(pCtx,pRes->pSt->pConn,zFn,sqlite3_errcode(pRes->pSt->pConn->pDb),
			(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
	}
	return -1;
}
/* SQLite3Result::fetchArray(int $mode = SQLITE3_BOTH): array|false */
static int vm_builtin_SQLite3Result_fetchArray(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);
	int iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : 3;
	ph7_value *pRow,*pCell;
	Sq3Verb sVerb;
	int iStep;
	if( pRes == 0 ){
		return rc;
	}
	Sq3VerbEnter(pRes->pSt->pConn,pCtx,"SQLite3Result::fetchArray",&sVerb);
	iStep = Sq3StepFetch(pCtx,pRes,"SQLite3Result::fetchArray",&rc);
	{
		sxi32 rcCb = Sq3VerbLeave(pRes->pSt->pConn,&sVerb);
		if( rcCb != PH7_OK ){
			ph7_result_bool(pCtx,0);
			return rcCb;
		}
	}
	if( iStep != 1 ){
		ph7_result_bool(pCtx,0);
		return rc;
	}
	pRow = ph7_context_new_array(pCtx);
	pCell = ph7_context_new_scalar(pCtx);
	if( pRow == 0 || pCell == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	Sq3RowInto(pRes->pSt->pStmt,iMode,pRow,pCell);
	ph7_result_value(pCtx,pRow);
	return PH7_OK;
}
/*
 * SQLite3Result::fetchAll(int $mode = SQLITE3_BOTH): array|false
 *
 * The rows still AHEAD of the cursor, not all the rows: a walk already
 * underway hands back what is left of it. The end rewinds the statement the
 * way a single fetch does, so a second fetchAll() answers the whole set again.
 */
static int vm_builtin_SQLite3Result_fetchAll(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);
	int iMode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : 3;
	ph7_value *pAll,*pRow,*pCell;
	Sq3Verb sVerb;
	int iStep;
	if( pRes == 0 ){
		return rc;
	}
	pAll = ph7_context_new_array(pCtx);
	pCell = ph7_context_new_scalar(pCtx);
	if( pAll == 0 || pCell == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	Sq3VerbEnter(pRes->pSt->pConn,pCtx,"SQLite3Result::fetchAll",&sVerb);
	for(;;){
		iStep = Sq3StepFetch(pCtx,pRes,"SQLite3Result::fetchAll",&rc);
		if( iStep != 1 || pRes->pSt->pConn->iCallbackExc != 0 ){
			break;
		}
		pRow = ph7_context_new_array(pCtx);
		if( pRow == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		Sq3RowInto(pRes->pSt->pStmt,iMode,pRow,pCell);
		ph7_array_add_elem(pAll,0,pRow);
		ph7_context_release_value(pCtx,pRow);
	}
	{
		sxi32 rcCb = Sq3VerbLeave(pRes->pSt->pConn,&sVerb);
		if( rcCb != PH7_OK ){
			ph7_result_bool(pCtx,0);
			return rcCb;
		}
	}
	if( iStep < 0 ){
		ph7_result_bool(pCtx,0);
		return rc;
	}
	ph7_result_value(pCtx,pAll);
	return PH7_OK;
}
/* SQLite3Result::reset(): bool -- rewind, so the next fetch is the first row. */
static int vm_builtin_SQLite3Result_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pRes == 0 ){
		return rc;
	}
	ph7_result_bool(pCtx,sqlite3_reset(pRes->pSt->pStmt) == SQLITE_OK);
	return PH7_OK;
}
/*
 * SQLite3Result::finalize(): true
 *
 * Not a close of the statement -- a release of THIS result's hold on it. The
 * statement query() built has no other holder, so it is finalized here; one a
 * script prepared itself is still runnable afterwards. Either way the result
 * itself is spent, and a second finalize() is the Error rather than a second
 * true.
 */
static int vm_builtin_SQLite3Result_finalize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3_res *pRes = Sq3LiveRes(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pRes == 0 ){
		return rc;
	}
	Sq3ResDetach(pRes);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Small readers and switches
 * ------------------------------------------------------------------------ */
/* SQLite3::lastInsertRowID(): int */
static int vm_builtin_SQLite3_lastInsertRowID(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return rc;
	}
	ph7_result_int64(pCtx,(ph7_int64)sqlite3_last_insert_rowid(pConn->pDb));
	return PH7_OK;
}
/*
 * SQLite3::changes(): int
 *
 * sqlite's own counter, which only a write moves: a SELECT, a CREATE or a
 * statement that matched nothing leaves the PREVIOUS write's count standing.
 */
static int vm_builtin_SQLite3_changes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn == 0 ){
		return rc;
	}
	ph7_result_int64(pCtx,(ph7_int64)sqlite3_changes(pConn->pDb));
	return PH7_OK;
}
/*
 * SQLite3::busyTimeout(int $milliseconds): bool
 *
 * The argument reaches sqlite as a C `int`, so a value past that width is
 * truncated on the way -- php's own narrowing, not a check.
 */
static int vm_builtin_SQLite3_busyTimeout(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);
	SXUNUSED(nArg);
	if( pConn == 0 ){
		return rc;
	}
	ph7_result_bool(pCtx,
		sqlite3_busy_timeout(pConn->pDb,(int)ph7_value_to_int64(apArg[0])) == SQLITE_OK);
	return PH7_OK;
}
/*
 * SQLite3::enableExceptions(bool $enable = false): bool
 *
 * Answers the PREVIOUS setting, and asks nothing about the connection's state
 * -- it is settable on an object that was never opened and on a closed one.
 * php 8.3 deprecated the warning mode itself, so asking for it says so, every
 * time and whatever the setting already was.
 */
static int vm_builtin_SQLite3_enableExceptions(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_sq3 *pConn = Sq3Bind(pCtx);
	int bEnable = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;
	if( pConn == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !bEnable ){
		VmErrorFormat(pCtx->pVm,8192 /* E_DEPRECATED */,
			"SQLite3::enableExceptions(): Use of warnings for SQLite3 is deprecated");
	}
	ph7_result_bool(pCtx,pConn->bExceptions);
	pConn->bExceptions = bEnable;
	return PH7_OK;
}
/*
 * SQLite3::enableExtendedResultCodes(bool $enable = true): bool
 *
 * Answers whether the LIBRARY took the switch -- so it splits the two states
 * the way the error readers do rather than the way enableExceptions() does: a
 * never-opened object is the Error, and a CLOSED one is a plain false, because
 * there is no handle left to tell.
 */
static int vm_builtin_SQLite3_enableExtendedResultCodes(ph7_context *pCtx,int nArg,
	ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3ErrReader(pCtx,&rc);
	int bEnable = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 1;
	if( pConn == 0 ){
		return rc;
	}
	if( pConn->pDb == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,
		sqlite3_extended_result_codes(pConn->pDb,bEnable) == SQLITE_OK);
	return PH7_OK;
}
/*
 * SQLite3::loadExtension(string $name): bool
 *
 * Three refusals in php's order, and the DIRECTORY decides the first: an empty
 * `sqlite3.extension_dir` means the door is shut and nothing else is asked, so
 * the name is not even looked at. With a directory set, the name is joined to
 * it -- always with a separator and never as an absolute path of its own, so a
 * name starting with `/` lands under the directory twice over -- and the empty
 * name is the ValueError that only a set directory can reach.
 */
static int vm_builtin_SQLite3_loadExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	phl_sq3 *pConn = Sq3LiveDb(pCtx,&rc);
	const char *zName;
	int nName = 0;
	SyBlob sDir,sPath;
	char *zErr = 0;
	int rcLoad;
	SXUNUSED(nArg);
	if( pConn == 0 ){
		return rc;
	}
	SyBlobInit(&sDir,&pCtx->pVm->sAllocator);
	PH7_VmIniGetStr(pCtx->pVm,"sqlite3.extension_dir",&sDir);
	if( SyBlobLength(&sDir) < 1 ){
		SyBlobRelease(&sDir);
		ph7_result_bool(pCtx,0);
		return Sq3Error(pCtx,pConn,"SQLite3::loadExtension",0,
			"SQLite Extensions are disabled");
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	if( nName < 1 ){
		SyBlobRelease(&sDir);
		return PH7_VmThrowException(pCtx,"ValueError",
			"SQLite3::loadExtension(): Argument #1 ($name) must not be empty");
	}
	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sPath,SyBlobData(&sDir),SyBlobLength(&sDir));
	SyBlobAppend(&sPath,"/",1);
	SyBlobAppend(&sPath,zName,(sxu32)nName);
	SyBlobAppend(&sPath,"",1);
	SyBlobRelease(&sDir);
	sqlite3_enable_load_extension(pConn->pDb,1);
	rcLoad = sqlite3_load_extension(pConn->pDb,(const char *)SyBlobData(&sPath),0,&zErr);
	sqlite3_enable_load_extension(pConn->pDb,0);
	if( zErr ){
		sqlite3_free(zErr);
	}
	if( rcLoad != SQLITE_OK ){
		SyBlob sMsg;
		sxi32 rcErr;
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		/* php reports the path it BUILT, not the name the script wrote, and the
		 * path is a C string by then -- so a name carrying a NUL is reported cut
		 * at it. */
		SyBlobFormat(&sMsg,"Unable to load extension at '%s'",
			(const char *)SyBlobData(&sPath));
		SyBlobAppend(&sMsg,"",1);
		ph7_result_bool(pCtx,0);
		rcErr = Sq3Error(pCtx,pConn,"SQLite3::loadExtension",0,
			(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
		SyBlobRelease(&sPath);
		return rcErr;
	}
	SyBlobRelease(&sPath);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Installation
 * ------------------------------------------------------------------------ */
/* The twelve constants ext/sqlite3 declares globally. The type and open codes
 * are sqlite's own macros -- they belong to the library, so they are read from
 * its header rather than copied; ASSOC/NUM/BOTH are php's own numbering for a
 * fetch mode the library knows nothing about. */
static const struct Sq3Constant {
	const char *zName;
	sxi64 iValue;
} aSq3Const[] = {
	{ "SQLITE3_ASSOC",          1 },
	{ "SQLITE3_NUM",            2 },
	{ "SQLITE3_BOTH",           3 },
	{ "SQLITE3_INTEGER",        SQLITE_INTEGER },
	{ "SQLITE3_FLOAT",          SQLITE_FLOAT },
	{ "SQLITE3_TEXT",           SQLITE3_TEXT },
	{ "SQLITE3_BLOB",           SQLITE_BLOB },
	{ "SQLITE3_NULL",           SQLITE_NULL },
	{ "SQLITE3_OPEN_READONLY",  SQLITE_OPEN_READONLY },
	{ "SQLITE3_OPEN_READWRITE", SQLITE_OPEN_READWRITE },
	{ "SQLITE3_OPEN_CREATE",    SQLITE_OPEN_CREATE },
	{ "SQLITE3_DETERMINISTIC",  SQLITE_DETERMINISTIC },
};
static void Sq3ConstExpand(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,((const struct Sq3Constant *)pUserData)->iValue);
}
PH7_PRIVATE void PH7_RegisterSqlite3Constants(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aSq3Const) ; ++n ){
		ph7_create_constant(&(*pVm),aSq3Const[n].zName,Sq3ConstExpand,
			(void *)&aSq3Const[n]);
	}
}
PH7_PRIVATE sxi32 PH7_VmInstallSqlite3(ph7_vm *pVm)
{
#define SQ3_INT_CONST(NAME,VALUE) \
	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (ph7_int64)(VALUE), 0, 0.0 }
	/*
	 * The authorizer's vocabulary: three verdicts and the thirty-three actions
	 * a callback is told about, in php's own declaration order -- which is not
	 * numeric order, since `COPY` (0, and retired by sqlite long ago) sits after
	 * `SAVEPOINT` and `RECURSIVE` closes the list. Every value is the library's
	 * macro. The callback that reads them arrives with setAuthorizer().
	 */
	static const PH7_NativeConstDef aSq3ClassConst[] = {
		SQ3_INT_CONST("OK",                  SQLITE_OK),
		SQ3_INT_CONST("DENY",                SQLITE_DENY),
		SQ3_INT_CONST("IGNORE",              SQLITE_IGNORE),
		SQ3_INT_CONST("CREATE_INDEX",        SQLITE_CREATE_INDEX),
		SQ3_INT_CONST("CREATE_TABLE",        SQLITE_CREATE_TABLE),
		SQ3_INT_CONST("CREATE_TEMP_INDEX",   SQLITE_CREATE_TEMP_INDEX),
		SQ3_INT_CONST("CREATE_TEMP_TABLE",   SQLITE_CREATE_TEMP_TABLE),
		SQ3_INT_CONST("CREATE_TEMP_TRIGGER", SQLITE_CREATE_TEMP_TRIGGER),
		SQ3_INT_CONST("CREATE_TEMP_VIEW",    SQLITE_CREATE_TEMP_VIEW),
		SQ3_INT_CONST("CREATE_TRIGGER",      SQLITE_CREATE_TRIGGER),
		SQ3_INT_CONST("CREATE_VIEW",         SQLITE_CREATE_VIEW),
		SQ3_INT_CONST("DELETE",              SQLITE_DELETE),
		SQ3_INT_CONST("DROP_INDEX",          SQLITE_DROP_INDEX),
		SQ3_INT_CONST("DROP_TABLE",          SQLITE_DROP_TABLE),
		SQ3_INT_CONST("DROP_TEMP_INDEX",     SQLITE_DROP_TEMP_INDEX),
		SQ3_INT_CONST("DROP_TEMP_TABLE",     SQLITE_DROP_TEMP_TABLE),
		SQ3_INT_CONST("DROP_TEMP_TRIGGER",   SQLITE_DROP_TEMP_TRIGGER),
		SQ3_INT_CONST("DROP_TEMP_VIEW",      SQLITE_DROP_TEMP_VIEW),
		SQ3_INT_CONST("DROP_TRIGGER",        SQLITE_DROP_TRIGGER),
		SQ3_INT_CONST("DROP_VIEW",           SQLITE_DROP_VIEW),
		SQ3_INT_CONST("INSERT",              SQLITE_INSERT),
		SQ3_INT_CONST("PRAGMA",              SQLITE_PRAGMA),
		SQ3_INT_CONST("READ",                SQLITE_READ),
		SQ3_INT_CONST("SELECT",              SQLITE_SELECT),
		SQ3_INT_CONST("TRANSACTION",         SQLITE_TRANSACTION),
		SQ3_INT_CONST("UPDATE",              SQLITE_UPDATE),
		SQ3_INT_CONST("ATTACH",              SQLITE_ATTACH),
		SQ3_INT_CONST("DETACH",              SQLITE_DETACH),
		SQ3_INT_CONST("ALTER_TABLE",         SQLITE_ALTER_TABLE),
		SQ3_INT_CONST("REINDEX",             SQLITE_REINDEX),
		SQ3_INT_CONST("ANALYZE",             SQLITE_ANALYZE),
		SQ3_INT_CONST("CREATE_VTABLE",       SQLITE_CREATE_VTABLE),
		SQ3_INT_CONST("DROP_VTABLE",         SQLITE_DROP_VTABLE),
		SQ3_INT_CONST("FUNCTION",            SQLITE_FUNCTION),
		SQ3_INT_CONST("SAVEPOINT",           SQLITE_SAVEPOINT),
		SQ3_INT_CONST("COPY",                SQLITE_COPY),
		SQ3_INT_CONST("RECURSIVE",           SQLITE_RECURSIVE),
	};
	/*
	 * php's declaration ORDER, which get_class_methods() and Reflection both
	 * answer in. The two static verbs sit among the instance ones rather than
	 * being grouped, and the slices still to come keep their places.
	 */
	static const PH7_NativeMethodDef aSq3Method[] = {
		{ "__construct", PH7_MOD_PUBLIC,
		  "string $filename, int $flags = SQLITE3_OPEN_READWRITE | SQLITE3_OPEN_CREATE, "
		  "string $encryptionKey = ''", 0, vm_builtin_SQLite3_construct },
		{ "open", PH7_MOD_PUBLIC,
		  "string $filename, int $flags = SQLITE3_OPEN_READWRITE | SQLITE3_OPEN_CREATE, "
		  "string $encryptionKey = ''", "@void", vm_builtin_SQLite3_open },
		{ "close", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SQLite3_close },
		{ "version", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "", "@array",
		  vm_builtin_SQLite3_version },
		{ "lastInsertRowID", PH7_MOD_PUBLIC, "", "@int",
		  vm_builtin_SQLite3_lastInsertRowID },
		{ "lastErrorCode", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SQLite3_lastErrorCode },
		{ "lastExtendedErrorCode", PH7_MOD_PUBLIC, "", "@int",
		  vm_builtin_SQLite3_lastExtendedErrorCode },
		{ "lastErrorMsg", PH7_MOD_PUBLIC, "", "@string", vm_builtin_SQLite3_lastErrorMsg },
		{ "changes", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SQLite3_changes },
		{ "busyTimeout", PH7_MOD_PUBLIC, "int $milliseconds", "@bool",
		  vm_builtin_SQLite3_busyTimeout },
		{ "loadExtension", PH7_MOD_PUBLIC, "string $name", "@bool",
		  vm_builtin_SQLite3_loadExtension },
		{ "backup", PH7_MOD_PUBLIC,
		  "SQLite3 $destination, string $sourceDatabase = 'main', "
		  "string $destinationDatabase = 'main'", "@bool", vm_builtin_SQLite3_backup },
		{ "escapeString", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "string $string", "@string",
		  vm_builtin_SQLite3_escapeString },
		{ "prepare", PH7_MOD_PUBLIC, "string $query", "@SQLite3Stmt|false",
		  vm_builtin_SQLite3_prepare },
		{ "exec", PH7_MOD_PUBLIC, "string $query", "@bool", vm_builtin_SQLite3_exec },
		{ "query", PH7_MOD_PUBLIC, "string $query", "@SQLite3Result|false",
		  vm_builtin_SQLite3_query },
		{ "querySingle", PH7_MOD_PUBLIC, "string $query, bool $entireRow = false", "@mixed",
		  vm_builtin_SQLite3_querySingle },
		{ "createFunction", PH7_MOD_PUBLIC,
		  "string $name, callable $callback, int $argCount = -1, int $flags = 0", "@bool",
		  vm_builtin_SQLite3_createFunction },
		{ "createAggregate", PH7_MOD_PUBLIC,
		  "string $name, callable $stepCallback, callable $finalCallback, "
		  "int $argCount = -1", "@bool", vm_builtin_SQLite3_createAggregate },
		{ "createCollation", PH7_MOD_PUBLIC, "string $name, callable $callback", "@bool",
		  vm_builtin_SQLite3_createCollation },
		{ "openBlob", PH7_MOD_PUBLIC,
		  "string $table, string $column, int $rowid, string $database = 'main', "
		  "int $flags = SQLITE3_OPEN_READONLY", 0, vm_builtin_SQLite3_openBlob },
		{ "enableExceptions", PH7_MOD_PUBLIC, "bool $enable = false", "@bool",
		  vm_builtin_SQLite3_enableExceptions },
		{ "enableExtendedResultCodes", PH7_MOD_PUBLIC, "bool $enable = true", "@bool",
		  vm_builtin_SQLite3_enableExtendedResultCodes },
		{ "setAuthorizer", PH7_MOD_PUBLIC, "?callable $callback", "@bool",
		  vm_builtin_SQLite3_setAuthorizer },
	};
	/* The handle: storage the class owns and never presents -- php shows no
	 * property at all on a SQLite3, so the slot is hidden. */
	static const PH7_NativePropDef aSq3Prop[] = {
		{ SQ3_RES, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	/*
	 * php's cursor. Its constructor is PRIVATE and takes nothing: only the
	 * extension ever builds one, and a script that tries is refused by the
	 * engine's own visibility rule rather than by a body.
	 */
	static const PH7_NativeMethodDef aSq3ResMethod[] = {
		{ "__construct", PH7_MOD_PRIVATE, "", 0, vm_builtin_SQLite3Result_construct },
		{ "numColumns", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SQLite3Result_numColumns },
		{ "columnName", PH7_MOD_PUBLIC, "int $column", "@string|false",
		  vm_builtin_SQLite3Result_columnName },
		{ "columnType", PH7_MOD_PUBLIC, "int $column", "@int|false",
		  vm_builtin_SQLite3Result_columnType },
		{ "fetchArray", PH7_MOD_PUBLIC, "int $mode = SQLITE3_BOTH", "@array|false",
		  vm_builtin_SQLite3Result_fetchArray },
		{ "fetchAll", PH7_MOD_PUBLIC, "int $mode = SQLITE3_BOTH", "array|false",
		  vm_builtin_SQLite3Result_fetchAll },
		{ "reset", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SQLite3Result_reset },
		{ "finalize", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SQLite3Result_finalize },
	};
	static const PH7_NativePropDef aSq3ResProp[] = {
		{ SQ3_RES, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	/*
	 * php's own prepared statement. Its constructor is PRIVATE and takes the
	 * connection and the query, which nothing may call: prepare() builds every
	 * one. The three EXPLAIN_MODE_* constants are php 8.4's, over sqlite's
	 * sqlite3_stmt_explain().
	 */
	static const PH7_NativeConstDef aSq3StmtConst[] = {
		{ "EXPLAIN_MODE_PREPARED",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },
		{ "EXPLAIN_MODE_EXPLAIN",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },
		{ "EXPLAIN_MODE_EXPLAIN_QUERY_PLAN", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },
	};
	static const PH7_NativeMethodDef aSq3StmtMethod[] = {
		{ "__construct", PH7_MOD_PRIVATE, "SQLite3 $sqlite3, string $query", 0,
		  vm_builtin_SQLite3Stmt_construct },
		{ "bindParam", PH7_MOD_PUBLIC,
		  "string|int $param, mixed &$var, int $type = SQLITE3_TEXT", "@bool",
		  vm_builtin_SQLite3Stmt_bindParam },
		{ "bindValue", PH7_MOD_PUBLIC,
		  "string|int $param, mixed $value, int $type = SQLITE3_TEXT", "@bool",
		  vm_builtin_SQLite3Stmt_bindValue },
		{ "clear", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SQLite3Stmt_clear },
		{ "close", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SQLite3Stmt_close },
		{ "execute", PH7_MOD_PUBLIC, "", "@SQLite3Result|false",
		  vm_builtin_SQLite3Stmt_execute },
		{ "getSQL", PH7_MOD_PUBLIC, "bool $expand = false", "@string|false",
		  vm_builtin_SQLite3Stmt_getSQL },
		{ "paramCount", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SQLite3Stmt_paramCount },
		{ "readOnly", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SQLite3Stmt_readOnly },
		{ "reset", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SQLite3Stmt_reset },
		/* the three php declares WITHOUT the tentative marker */
		{ "busy", PH7_MOD_PUBLIC, "", "bool", vm_builtin_SQLite3Stmt_busy },
		{ "explain", PH7_MOD_PUBLIC, "", "int", vm_builtin_SQLite3Stmt_explain },
		{ "setExplain", PH7_MOD_PUBLIC, "int $mode", "bool",
		  vm_builtin_SQLite3Stmt_setExplain },
	};
	static const PH7_NativePropDef aSq3StmtProp[] = {
		{ SQ3_RES, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		/* php registers the exception FIRST, and ReflectionExtension answers the
		 * class list in that order. */
		{ "SQLite3Exception", "Exception", 0, 0,
		  0, 0, 0, 0, 0, 0,
		  0, 0, 0 },
		/* `clone` and `serialize` are refused: php declares neither handler, so a
		 * copy would carry the same sqlite3 pointer in its hidden slot. */
		{ "SQLite3", 0, 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aSq3Method, SX_ARRAYSIZE(aSq3Method),
		  aSq3ClassConst, SX_ARRAYSIZE(aSq3ClassConst),
		  aSq3Prop, SX_ARRAYSIZE(aSq3Prop),
		  Sq3InstanceRelease, 0, 0 },
		{ "SQLite3Stmt", 0, 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aSq3StmtMethod, SX_ARRAYSIZE(aSq3StmtMethod),
		  aSq3StmtConst, SX_ARRAYSIZE(aSq3StmtConst),
		  aSq3StmtProp, SX_ARRAYSIZE(aSq3StmtProp),
		  Sq3StmtInstanceRelease, 0, 0 },
		{ "SQLite3Result", 0, 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aSq3ResMethod, SX_ARRAYSIZE(aSq3ResMethod),
		  0, 0,
		  aSq3ResProp, SX_ARRAYSIZE(aSq3ResProp),
		  Sq3ResInstanceRelease, 0, 0 },
	};
#undef SQ3_INT_CONST
	pVm->pSq3Conns = 0;
	PH7_RegisterSqlite3Constants(&(*pVm));
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}

#else
/* Ensure non-empty translation unit when sqlite is disabled (MSVC C4206) */
typedef int vm_sqlite3_unused;
#endif /* PH7_ENABLE_SQLITE */
