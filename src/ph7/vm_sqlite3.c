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
struct phl_sq3 {
	sqlite3 *pDb;                 /* 0 before the first open and after close() */
	int bInitialised;             /* an open has SUCCEEDED on this object */
	int bExceptions;              /* enableExceptions(): what routes a failure */
	ph7_vm *pVm;
	ph7_class_instance *pOwner;   /* the object whose slot holds it */
	phl_sq3 *pNext;
};

/* The hidden slot every one of these classes reaches its record through. */
#define SQ3_RES "__res"

/* ------------------------------------------------------------------------
 * Lifetime
 * ------------------------------------------------------------------------ */
static void Sq3Close(phl_sq3 *pConn)
{
	if( pConn->pDb ){
		/* _v2, so a statement this slice does not yet know about defers the
		 * close rather than leaking the handle. */
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
static void Sq3FreeConn(phl_sq3 *pConn)
{
	ph7_vm *pVm = pConn->pVm;
	phl_sq3 *pCur,*pPrev = 0;
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
/*
 * Blank the hidden slot of the object that holds a record about to be freed.
 * Without it the object outlives its record -- any handle still alive at VM
 * teardown -- and its own release reads freed memory to ask whether it still
 * owns one. ext/pdo learned this from ASan; nothing in an ordinary build
 * notices.
 */
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
static phl_sq3 * Sq3OfInstance(ph7_class_instance *pThis)
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
	return (phl_sq3 *)ph7_value_to_resource(pRes);
}
static int Sq3Attach(ph7_class_instance *pThis,phl_sq3 *pConn)
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
	pRes->x.pOther = pConn;
	MemObjSetType(pRes,MEMOBJ_RES);
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
