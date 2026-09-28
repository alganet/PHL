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
 *    Growing by slice. This unit owns the connection.
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
	int nRef;                     /* holders: the statement object, and each live result */
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
static void Sq3Close(phl_sq3 *pConn)
{
	phl_sq3_stmt *pSt;
	for( pSt = pConn->pStmts ; pSt ; pSt = pSt->pNext ){
		if( pSt->pStmt ){
			sqlite3_finalize(pSt->pStmt);
			pSt->pStmt = 0;
		}
	}
	if( pConn->pDb ){
		/* _v2 anyway, so a handle something else still owns defers the close
		 * rather than leaking. */
		sqlite3_close_v2(pConn->pDb);
		pConn->pDb = 0;
	}
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
 * Blank one object's hidden slot: the record behind it is going away and the
 * object may well outlive it.
 */
static void Sq3BlankSlot(ph7_class_instance *pOwner);
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
	if( pConn && pConn->bExceptions ){
		return PH7_VmThrowExceptionCode(pCtx,"SQLite3Exception",(sxi32)iCode,"%s",zMsg);
	}
	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",zFn,zMsg);
	return PH7_OK;
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
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pConn ){
		Sq3Close(pConn);
	}
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
	rcSql = sqlite3_exec(pConn->pDb,(const char *)SyBlobData(&sSql),0,0,0);
	SyBlobRelease(&sSql);
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
	SXUNUSED(nArg);
	if( pConn == 0 ){
		return rc;
	}
	zSql = ph7_value_to_string(apArg[0],&nSql);
	pSt = Sq3Prepare(pCtx,pConn,zSql,nSql,"SQLite3::query",&rc);
	if( pSt == 0 ){
		ph7_result_bool(pCtx,0);
		return rc;
	}
	if( !Sq3StepOnce(pCtx,pSt,"SQLite3::query",&rc) ){
		Sq3StmtUnref(pSt);
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
	int iStep;
	if( pConn == 0 ){
		return rc;
	}
	zSql = ph7_value_to_string(apArg[0],&nSql);
	pSt = Sq3Prepare(pCtx,pConn,zSql,nSql,"SQLite3::querySingle",&rc);
	if( pSt == 0 ){
		ph7_result_bool(pCtx,0);
		return rc;
	}
	iStep = pSt->pStmt ? sqlite3_step(pSt->pStmt) : SQLITE_MISUSE;
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
	int iStep;
	if( pRes == 0 ){
		return rc;
	}
	iStep = Sq3StepFetch(pCtx,pRes,"SQLite3Result::fetchArray",&rc);
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
	int iStep;
	if( pRes == 0 ){
		return rc;
	}
	pAll = ph7_context_new_array(pCtx);
	pCell = ph7_context_new_scalar(pCtx);
	if( pAll == 0 || pCell == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	for(;;){
		iStep = Sq3StepFetch(pCtx,pRes,"SQLite3Result::fetchAll",&rc);
		if( iStep != 1 ){
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
		{ "escapeString", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "string $string", "@string",
		  vm_builtin_SQLite3_escapeString },
		{ "exec", PH7_MOD_PUBLIC, "string $query", "@bool", vm_builtin_SQLite3_exec },
		{ "query", PH7_MOD_PUBLIC, "string $query", "@SQLite3Result|false",
		  vm_builtin_SQLite3_query },
		{ "querySingle", PH7_MOD_PUBLIC, "string $query, bool $entireRow = false", "@mixed",
		  vm_builtin_SQLite3_querySingle },
		{ "enableExceptions", PH7_MOD_PUBLIC, "bool $enable = false", "@bool",
		  vm_builtin_SQLite3_enableExceptions },
		{ "enableExtendedResultCodes", PH7_MOD_PUBLIC, "bool $enable = true", "@bool",
		  vm_builtin_SQLite3_enableExtendedResultCodes },
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
