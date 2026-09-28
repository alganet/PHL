/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h> /* touch() resolves php's "now" default here, not in the driver */

#include <sys/types.h>
#include <sys/stat.h>  /* the shared fstat() shape of a stream's stat answer */
#ifdef __UNIXES__
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#endif
/*
 * This file implement a virtual file systems (VFS) for the PH7 engine.
 */
/*
 * Given a string containing the path of a file or directory, this function
 * return the parent directory's path.
 */
PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)
{
	/* php_dirname: strip any trailing separators, cut at the last remaining one,
	 * then strip trailing separators off the parent too. The previous version
	 * scanned back to the first separator and stopped, so it never coped with a
	 * trailing separator or a run of them: dirname("/a/") answered "/a" instead
	 * of "/", dirname("a//b") answered "a/", and dirname("///") answered "//".
	 * It also answered "." for the empty string, where php answers "". */
	int c,d,iEnd,i;
#ifdef __WINNT__
	const char *zRoot = "\\";
#else
	const char *zRoot = "/";
#endif
	c = d = '/';
#ifdef __WINNT__
	d = '\\';
#endif
#define DIR_IS_SEP(x) ( (int)(x) == c || (int)(x) == d )
	if( nByte < 1 ){
		/* php returns the empty string for the empty path */
		*pLen = 0;
		return "";
	}
	iEnd = nByte;
	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){
		iEnd--;
	}
	if( iEnd == 0 ){
		/* The path is nothing but separators: the root is its own parent */
		*pLen = (int)sizeof(char);
		return zRoot;
	}
	/* Walk back to the separator that ends the parent directory */
	i = iEnd;
	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){
		i--;
	}
	if( i == 0 ){
		/* No separator at all,return "." as the current directory */
		*pLen = (int)sizeof(char);
		return ".";
	}
	/* Drop the separator, plus any that repeat before it */
	while( i > 1 && DIR_IS_SEP(zPath[i - 1]) ){
		i--;
	}
	if( i == 1 && DIR_IS_SEP(zPath[0]) ){
		*pLen = (int)sizeof(char);
		return zRoot;
	}
	*pLen = i;
	return zPath;
#undef DIR_IS_SEP
}
/*
 * php_basename: drop any trailing separators, then answer what follows the last
 * remaining one. Shared by basename() and pathinfo() — they used to hand-roll the
 * same walk separately, and pathinfo()'s copy kept the trailing separator run
 * (`pathinfo("/var/www/")` answered basename "" where php answers "www").
 */
PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen)
{
	int c,d,iEnd,i;
	c = d = '/';
#ifdef __WINNT__
	d = '\\';
#endif
#define DIR_IS_SEP(x) ( (int)(x) == c || (int)(x) == d )
	iEnd = nByte;
	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){
		iEnd--;
	}
	if( iEnd < 1 ){
		/* Empty, or nothing but separators: php answers the empty string */
		*pLen = 0;
		return "";
	}
	i = iEnd;
	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){
		i--;
	}
	*pLen = iEnd - i;
	return &zPath[i];
#undef DIR_IS_SEP
}
/*
 * php's `ValueError: Path must not be empty`, raised by its STREAM LAYER before
 * anything is looked up -- so every door that opens one gets it, unqualified by
 * a function name, and a program that hands `''` to file_get_contents() catches
 * an exception rather than reading a warning and a false.
 *
 * Three doors say it in their own words instead, naming the argument
 * (parse_ini_file, scandir, and the DirectoryIterator/DOM constructors that
 * already did), and the STAT family says nothing at all: an empty path there is
 * a silent false, where a missing one is php's `stat failed for` warning.
 *
 * Answers 1 once the exception is raised, which is the caller's cue to return.
 */
PH7_PRIVATE int PH7_VfsEmptyPathRefused(ph7_context *pCtx,int nPath)
{
	if( nPath > 0 ){
		return 0;
	}
	PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");
	return 1;
}
/*
 * Compile the VFS implementations when builtins are enabled OR when disk I/O
 * is explicitly enabled (i.e. PH7_DISABLE_DISK_IO is NOT defined).
 */
#ifndef PH7_DISABLE_DISK_IO
/*
 * strerror() trips MSVC's C4996 "may be unsafe" deprecation under /WX. It is a
 * standard C function we use deliberately to mirror php's IO error text; wrap it
 * once with the deprecation suppressed. The pragma is _MSC_VER-guarded so the
 * GCC/-Werror Linux build never sees an unknown-pragma warning.
 */
PH7_PRIVATE const char * VfsStrerror(int iErr)
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996)
#endif
	return strerror(iErr);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
}
/*
 * php's non-open IO failures: "unlink(/nope): No such file or directory".
 * PH7 returned FALSE in SILENCE for unlink/rmdir/mkdir/rename/chdir/opendir/scandir and
 * filesize, so a script could not tell a failed operation from a successful one without
 * checking the return value it never got told to check.
 */
static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)
{
	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",
		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));
}
/*
 * php's "fopen(data://x): Failed to open stream: rfc2397: no comma in URL".
 *
 * Two things in that sentence were this engine's own. The NAME was whatever
 * PH7_VmGetStreamDevice() left after the scheme, so a wrapper's failure blamed
 * a path the script never wrote (`fopen(x)`, `fopen(nosuchthing)`); it is the
 * whole URI when the caller is reporting the open that lookup resolved, which
 * is what the pointer it handed back identifies. And the REASON was errno --
 * which only the plain-file wrapper sets, so every other one reported whatever
 * errno happened to be lying around, including `Success` for a failed open.
 * php's reason is the wrapper's: its own sentence when it logged one, and a
 * flat "operation failed" when it did not.
 */
PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zName = zFile ? zFile : "";
	int nName = -1;
	if( zFile != 0 && zFile == pVm->zOpenUriTail && pVm->zOpenUri != 0 ){
		zName = pVm->zOpenUri;
		nName = pVm->nOpenUri;
	}
	PH7_VmThrowWarningFmt(pVm,"%s(%.*s): Failed to open stream: %s",
		ph7_function_name(pCtx),nName < 0 ? (int)SyStrlen(zName) : nName,zName,
		pVm->zOpenErr ? pVm->zOpenErr : VfsStrerror(errno));
}
/*
 * php's answer when NO wrapper will take a name is a reason of its own, raised
 * before the operation's own failure and naming the scheme the script wrote:
 *
 *   file_get_contents(): Unable to find the wrapper "zzz" - did you forget to
 *   enable it when you configured PHP?
 *
 * PHL raised one PH7-specific sentence -- "No such stream device,PH7 is
 * returning FALSE" -- which names neither the function's argument nor what was
 * wrong with it, and two more call sites had a third wording of their own.
 */
PH7_PRIVATE void VfsThrowNoDeviceWarning(ph7_context *pCtx,const char *zUri,int bDir)
{
	const char *zFunc = ph7_function_name(pCtx);
	const char *zWhat = bDir ? "directory" : "stream";
	int nScheme = 0;
	if( zUri == 0 ){
		zUri = "";
	}
	if( PH7_VmStreamDeviceIsRemoteHost(zUri,-1,&nScheme) ){
		/* A wrapper WAS found for the scheme and refused the name, which php
		 * words differently from a scheme nothing is registered under. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Remote host file access not supported, %s",
			zFunc,zUri);
		PH7_VmThrowWarningFmt(pCtx->pVm,
			"%s(%s): Failed to open %s: no suitable wrapper could be found",zFunc,zUri,zWhat);
		return;
	}
	if( nScheme > 0 ){
		PH7_VmThrowWarningFmt(pCtx->pVm,
			"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it when you configured PHP?",
			zFunc,nScheme,zUri);
	}
	/* A name with no scheme is the plain-files wrapper's, and that is the ONE
	 * php reports as switched off rather than missing: its fallback branch runs
	 * after the hash lookup, so an explicit `file://` gets both sentences and a
	 * bare path only the second. Every other unregistered wrapper is
	 * indistinguishable from one that never existed, and php words it that way. */
	if( (nScheme == 0
	  || (nScheme == (int)sizeof("file")-1 && SyStrnicmp(zUri,"file",sizeof("file")-1) == 0))
	 && PH7_VmStreamSchemeDisabled(pCtx->pVm,"file",(int)sizeof("file")-1) ){
		PH7_VmThrowWarningFmt(pCtx->pVm,
			"%s(): file:// wrapper is disabled in the server configuration",zFunc);
		PH7_VmThrowWarningFmt(pCtx->pVm,
			"%s(%s): Failed to open %s: no suitable wrapper could be found",zFunc,zUri,zWhat);
		return;
	}
	PH7_VmThrowWarningFmt(pCtx->pVm,
		"%s(%s): Failed to open %s: No such file or directory",zFunc,zUri,zWhat);
}
/*
 * The FIRST half of the sentence above, on its own: php's
 * `Unable to find the wrapper "zzz" - did you forget to enable it when you
 * configured PHP?`. A caller that resolves a wrapper WITHOUT opening anything
 * (get_headers()) reports only this one -- the failed-open line under it comes
 * from the open, and there is none.
 */
PH7_PRIVATE void VfsThrowUnknownWrapperWarning(ph7_context *pCtx,const char *zUri)
{
	int nScheme = 0;
	if( zUri == 0 ){
		zUri = "";
	}
	PH7_VmStreamDeviceIsRemoteHost(zUri,-1,&nScheme);
	if( nScheme > 0 ){
		PH7_VmThrowWarningFmt(pCtx->pVm,
			"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it when you configured PHP?",
			ph7_function_name(pCtx),nScheme,zUri);
	}
}
/*
 * php's stat-failure warning: `filemtime(): stat failed for /nope`, and
 * `filetype(): Lstat failed for /nope` for the two members that LSTAT. php raises
 * it from php_stat() for the whole family and answers FALSE; PHL answered the
 * VFS's raw -1 (or the string "unknown") for most of them, in silence -- and -1 is
 * TRUTHY, so `if (filemtime($f))` took the found branch for a file that is not
 * there and `filemtime($a) > filemtime($b)` compared a real time against it.
 */
static void VfsThrowStatWarning(ph7_context *pCtx,const char *zPath,int bLstat)
{
	if( zPath == 0 || zPath[0] == 0 ){
		/* php's stat family says NOTHING about an empty path -- it answers the
		 * same FALSE a missing one gets and skips the warning, which is the one
		 * place in the family where the empty path is not the opener's
		 * ValueError but a silence of its own. */
		return;
	}
	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s failed for %s",
		ph7_function_name(pCtx),bLstat ? "Lstat" : "stat",zPath);
}
/*
 * php's stat() answer is TWENTY-SIX entries, not thirteen: the same thirteen
 * fields once at numeric indices 0..12 and once under their names, in this
 * order. The numeric half is what php's own documentation indexes by ($s[7] is
 * the size) and it is what a `list()`/destructuring reader takes, so a script
 * written against php read `Undefined array key 7` here and answered NULL.
 *
 * The VFS fills the NAMED half (both the unix and Windows implementations use
 * exactly these keys), so the doubling is done once, here, rather than in every
 * xStat: pOut gets the numeric run first and then the names, which is php's own
 * insertion order — visible through foreach, print_r, var_dump and json_encode.
 */
PH7_PRIVATE int PH7_VfsStatDoubleUp(ph7_value *pIn,ph7_value *pOut)
{
	static const char * const azField[] = {
		"dev","ino","mode","nlink","uid","gid","rdev","size",
		"atime","mtime","ctime","blksize","blocks"
	};
	sxu32 i;
	if( pIn == 0 || pOut == 0 ){
		return -1;
	}
	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){
		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);
		if( pField == 0 ){
			/* A VFS that does not report this field: php always has all thirteen,
			 * so the doubling would silently shift every later index. Hand the
			 * caller the named-only array it already had instead. */
			return -1;
		}
		ph7_array_add_elem(pOut,0,pField);
	}
	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){
		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);
		ph7_array_add_strkey_elem(pOut,azField[i],pField);
	}
	return PH7_OK;
}
/*
 * The thirteen NAMED fields of a stat answer, filled from thirteen values in
 * php's own order. Two devices that never had a stat need one: php's memory
 * streams (php://memory, php://temp, data://) answer a SYNTHETIC record that
 * describes no file at all, and a pipe/socket/standard descriptor answers the
 * real fstat() of its handle. Both go through here so the field list -- and
 * therefore what PH7_VfsStatDoubleUp() can double -- exists in one place.
 */
PH7_PRIVATE int PH7_VfsStatFill(ph7_value *pArray,ph7_value *pWorker,const ph7_int64 *aVal)
{
	static const char * const azField[] = {
		"dev","ino","mode","nlink","uid","gid","rdev","size",
		"atime","mtime","ctime","blksize","blocks"
	};
	sxu32 i;
	if( pArray == 0 || pWorker == 0 || aVal == 0 ){
		return -1;
	}
	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){
		ph7_value_int64(pWorker,aVal[i]);
		ph7_array_add_strkey_elem(pArray,azField[i],pWorker); /* Takes its own copy */
	}
	return PH7_OK;
}
/*
 * The real fstat() of an open descriptor, in the shape above. php's pipe,
 * socket and php://stdin|stdout|stderr streams all answer exactly this -- the
 * same call, so the same platform answers, including the FAILURE that makes
 * fstat() report false.
 */
PH7_PRIVATE int PH7_VfsStatFromFd(int iFd,ph7_value *pArray,ph7_value *pWorker)
{
#ifdef __WINNT__
	struct _stat64 st;
	if( iFd < 0 || _fstat64(iFd,&st) != 0 ){
		return -1;
	}
#else
	struct stat st;
	if( iFd < 0 || fstat(iFd,&st) != 0 ){
		return -1;
	}
#endif
	{
		ph7_int64 aVal[13];
		aVal[0]  = (ph7_int64)st.st_dev;
		aVal[1]  = (ph7_int64)st.st_ino;
		aVal[2]  = (ph7_int64)st.st_mode;
		aVal[3]  = (ph7_int64)st.st_nlink;
		aVal[4]  = (ph7_int64)st.st_uid;
		aVal[5]  = (ph7_int64)st.st_gid;
		aVal[6]  = (ph7_int64)st.st_rdev;
		aVal[7]  = (ph7_int64)st.st_size;
		aVal[8]  = (ph7_int64)st.st_atime;
		aVal[9]  = (ph7_int64)st.st_mtime;
		aVal[10] = (ph7_int64)st.st_ctime;
#ifdef __WINNT__
		/* Windows has neither field, and php reports -1 for both there -- on
		 * EVERY stream, a plain file included (read back from php 8.5.8 on the
		 * gate guest). */
		aVal[11] = -1;
		aVal[12] = -1;
#else
		aVal[11] = (ph7_int64)st.st_blksize;
		aVal[12] = (ph7_int64)st.st_blocks;
#endif
		return PH7_VfsStatFill(pArray,pWorker,aVal);
	}
}
/*
 * Can this path be stat'ed at all? The three TIME readers report a failure as -1,
 * which is also a legitimate timestamp (a file stamped in the last second before
 * the epoch), so the failure verdict is asked of the VFS separately rather than
 * read off the value -- one extra call, and only on the negative branch.
 */
static int VfsPathStatable(ph7_vfs *pVfs,const char *zPath)
{
	if( pVfs == 0 || pVfs->xFileExists == 0 ){
		return 0;
	}
	return pVfs->xFileExists(zPath) == PH7_OK;
}
/*
 * bool chdir(string $directory)
 *  Change the current directory.
 * Parameters
 *  $directory
 *   The new current directory
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	/* Only the ARITY is checked here: php coerces a scalar $directory to string,
	 * so chdir(123) attempts "123" and warns that it does not exist. Requiring a
	 * string outright made that call return FALSE silently, with no diagnostic. */
	if( nArg < 1 ){
		/* Missing argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xChdir == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	errno = 0;
	rc = pVfs->xChdir(zPath);
	if( rc != PH7_OK ){
		/* chdir has its own php shape: no path, and the errno spelled out. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",
			ph7_function_name(pCtx),VfsStrerror(errno),errno);
	}
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool chroot(string $directory)
 *  Change the root directory.
 * Parameters
 *  $directory
 *   The path to change the root directory to
 * Return
 *  TRUE on success or FALSE on failure.
 *
 * POSIX only, like php's: the registration below is guarded the same way, and an
 * unreferenced static is an error under the Windows build's /W4 /WX.
 */
#ifndef __WINNT__
static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xChroot == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	errno = 0;
	rc = pVfs->xChroot(zPath);
	if( rc != PH7_OK ){
		/* php's own wording, and the failure a script actually meets: chroot(2)
		 * needs privilege, so an ordinary process gets EPERM. PHL answered the
		 * bare false in SILENCE — a refused chroot() and a chroot() that did
		 * nothing looked the same to the caller. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",
			ph7_function_name(pCtx),VfsStrerror(errno),errno);
	}
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
#endif /* __WINNT__ */
/*
 * string getcwd(void)
 *  Gets the current working directory.
 * Parameters
 *  None
 * Return
 *  Returns the current working directory on success, or FALSE on failure.
 */
static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vfs *pVfs;
	int rc;
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xGetcwd == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,"",0);
	/* Perform the requested operation */
	rc = pVfs->xGetcwd(pCtx);
	if( rc != PH7_OK ){
		/* Error,return FALSE */
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * bool rmdir(string $directory)
 *  Removes directory.
 * Parameters
 *  $directory
 *   The path to the directory
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc,bThrew = 0;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php's `?resource $context`, refused when it is a resource of another kind.
	 * Nothing here CONSUMES it — this operation never opens a stream, and the
	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched
	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it
	 * was accepted in silence. */
	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xRmdir == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	errno = 0;
	rc = pVfs->xRmdir(zPath);
	if( rc != PH7_OK ){
		VfsThrowSysWarning(pCtx,zPath);
	}
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool is_dir(string $filename)
 *  Tells whether the given filename is a directory.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xIsdir == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	rc = pVfs->xIsdir(zPath);
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])
 *  Make a directory.
 * Parameters
 *  $pathname
 *   The directory path.
 * $mode
 *  The mode is 0777 by default, which means the widest possible access.
 *  Note:
 *   mode is ignored on Windows.
 *   Note that you probably want to specify the mode as an octal number, which means
 *   it should have a leading zero. The mode is also modified by the current umask
 *   which you can change using umask().
 * $recursive
 *  Allows the creation of nested directories specified in the pathname.
 *  Defaults to FALSE. (Not used)
 * Return
 *  TRUE on success or FALSE on failure.
 */
/*
 * A prefix the recursive mkdir must not try to CREATE: it names a volume rather
 * than a directory. POSIX has none of these (the leading "/" is never a prefix
 * here, since the walk starts one byte in).
 */
static int VfsMkdirVolumePrefix(const char *z,int n)
{
#ifdef __WINNT__
	int i,nSep = 0;
	if( n < 1 ){
		return 1;
	}
	if( n == 2 && z[1] == ':' ){
		return 1; /* a bare drive, "C:" */
	}
	if( n > 1 && (z[0] == '/' || z[0] == '\\') && (z[1] == '/' || z[1] == '\\') ){
		for( i = 2 ; i < n ; i++ ){
			if( z[i] == '/' || z[i] == '\\' ){
				nSep++;
			}
		}
		return nSep < 2; /* still inside \\server\share */
	}
	return 0;
#else
	SXUNUSED(z);
	return n < 1;
#endif
}
#ifdef __WINNT__
#define VFS_MKDIR_SLASH(c) ((c) == '/' || (c) == '\\')
#else
#define VFS_MKDIR_SLASH(c) ((c) == '/')
#endif
/*
 * php's $recursive: create every missing ancestor, then the directory itself.
 * The flag reached the VFS and both back ends dropped it (`SXUNUSED(recursive)`),
 * so `mkdir("$d/a/b", 0777, true)` -- the everyday way a script prepares an
 * output tree -- warned "No such file or directory" and answered false whenever
 * more than one level was missing.
 *
 * php does the walk in the WRAPPER too, not in the syscall, and the rules the
 * oracle shows are: the mode is applied to every level it creates; an ancestor
 * that already exists is skipped in silence; the LEAF is always attempted, so an
 * existing one is "File exists" exactly as without the flag; a trailing
 * separator names the same directory; and the empty path is refused up front
 * with a message of its own.
 */
static int VfsMkdirRecursive(ph7_context *pCtx,ph7_vfs *pVfs,const char *zPath,int iMode)
{
	SyBlob sWorker;
	const char *zLocal;
	int i,nPath,nScheme,rc = PH7_OK;
	/* The strip first: the component walk must not cut a "file://" scheme up. */
	zLocal = PH7_VmFileUrlLocalPath(zPath);
	nScheme = PH7_VmUrlSchemeLen(zPath,-1);
	if( zLocal == zPath && nScheme == (int)sizeof("file")-1
	 && SyStrnicmp(zPath,"file",sizeof("file")-1) == 0 ){
		/* A file:// AUTHORITY this build will not reach: the strip handed the
		 * URL straight back. php answers false and creates NOTHING, where the
		 * walk below would cut the URL into components and make a directory
		 * literally called "file:". (A name under any OTHER scheme really does
		 * become a directory of that name on php too -- measured: `zzz://a/b`
		 * leaves a `zzz:` behind there as well -- so only this one is refused.) */
		return -1;
	}
	zPath = zLocal;
	nPath = (int)SyStrlen(zPath);
	/* A trailing separator names the same directory; php's own expand_filepath
	 * drops it before it starts. */
	while( nPath > 1 && VFS_MKDIR_SLASH(zPath[nPath-1]) ){
		nPath--;
	}
	if( nPath < 1 ){
		/* php: expand_filepath() refuses it, with this wording and no path. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Invalid path",ph7_function_name(pCtx));
		return -1;
	}
	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);
	for( i = 1 ; i <= nPath ; i++ ){
		int bLeaf = (i == nPath);
		if( !bLeaf ){
			/* Only at a separator that ENDS a component: a run of them names
			 * the same ancestor once. */
			if( !VFS_MKDIR_SLASH(zPath[i]) || VFS_MKDIR_SLASH(zPath[i-1]) ){
				continue;
			}
			if( VfsMkdirVolumePrefix(zPath,i) ){
				continue;
			}
		}
		SyBlobReset(&sWorker);
		if( SyBlobAppend(&sWorker,zPath,(sxu32)i) != SXRET_OK
		 || SyBlobNullAppend(&sWorker) != SXRET_OK ){
			rc = -1;
			break;
		}
		if( !bLeaf && VfsPathStatable(pVfs,(const char *)SyBlobData(&sWorker)) ){
			continue; /* an ancestor that is already there */
		}
		errno = 0;
		rc = pVfs->xMkdir((const char *)SyBlobData(&sWorker),iMode,0);
		if( rc != PH7_OK ){
			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",
				ph7_function_name(pCtx),VfsStrerror(errno));
			break;
		}
	}
	SyBlobRelease(&sWorker);
	return rc;
}
static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iRecursive = 0;
	const char *zPath;
	ph7_vfs *pVfs;
	int iMode,rc,bThrew = 0;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php's `?resource $context`, refused when it is a resource of another kind.
	 * Nothing here CONSUMES it — this operation never opens a stream, and the
	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched
	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it
	 * was accepted in silence. */
	PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xMkdir == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
#ifdef __WINNT__
	iMode = 0;
#else
	/* Assume UNIX */
	iMode = 0777;
#endif
	if( nArg > 1 ){
		iMode = ph7_value_to_int(apArg[1]);
		if( nArg > 2 ){
			iRecursive = ph7_value_to_bool(apArg[2]);
		}
	}
	/* Perform the requested operation */
	if( iRecursive ){
		rc = VfsMkdirRecursive(pCtx,pVfs,zPath,iMode);
	}else{
		errno = 0;
		rc = pVfs->xMkdir(zPath,iMode,0);
		if( rc != PH7_OK ){
			/* php does NOT name the path for mkdir: "mkdir(): File exists" */
			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",
				ph7_function_name(pCtx),VfsStrerror(errno));
		}
	}
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool rename(string $oldname,string $newname)
 *  Attempts to rename oldname to newname.
 * Parameters
 *  $oldname
 *   Old name.
 *  $newname
 *   New name.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zOld,*zNew;
	ph7_vfs *pVfs;
	int rc,bThrew = 0;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php's `?resource $context`, refused when it is a resource of another kind.
	 * Nothing here CONSUMES it — this operation never opens a stream, and the
	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched
	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it
	 * was accepted in silence. */
	PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xRename == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zOld = ph7_value_to_string(apArg[0],0);
	zNew = ph7_value_to_string(apArg[1],0);
	errno = 0;
	rc = pVfs->xRename(zOld,zNew);
	if( rc != PH7_OK ){
		/* php names BOTH paths here */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",
			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));
	}
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK );
	return PH7_OK;
}
/*
 * string realpath(string $path)
 *  Returns canonicalized absolute pathname.
 * Parameters
 *  $path
 *   Target path.
 * Return
 *  Canonicalized absolute pathname on success. or FALSE on failure.
 */
static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
        int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xRealpath == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Set an empty string untnil the underlying OS interface change that */
	ph7_result_string(pCtx,"",0);
	/* Perform the requested operation. php resolves an EMPTY path as `.` and so
	 * answers the working directory, where this answered false. */
	zPath = ph7_value_to_string(apArg[0],0);
	if( zPath == 0 || zPath[0] == 0 ){
		zPath = ".";
	}
	rc = pVfs->xRealpath(zPath,pCtx);
	if( rc != PH7_OK ){
	 ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * Does this candidate name something, and if so what is its canonical path?
 * The existence question is asked separately because xRealpath() writes STRAIGHT
 * into the call's result, so it may only be run on the winner.
 */
static int VfsResolveTry(ph7_vfs *pVfs,ph7_context *pCtx,const char *zCand)
{
	if( pVfs->xFileExists(zCand) != PH7_OK ){
		return 0;
	}
	/* The VFS APPENDS into the call's result, so make it an empty string first
	 * -- the realpath() builtin beside this one seeds the same way. */
	ph7_result_string(pCtx,"",0);
	if( pVfs->xRealpath(zCand,pCtx) == PH7_OK ){
		return 1;
	}
	ph7_result_bool(pCtx,0);
	return 0;
}
/*
 * Is this an ABSOLUTE path, by php's rule for this platform? A lone leading
 * slash is NOT absolute on Windows -- php walks the include_path for it.
 */
static int VfsPathIsAbsolute(const char *z,int n)
{
#ifdef __WINNT__
	if( n >= 2 && ((z[0] >= 'A' && z[0] <= 'Z') || (z[0] >= 'a' && z[0] <= 'z')) && z[1] == ':' ){
		return 1;
	}
	return n >= 2 && (z[0] == '/' || z[0] == '\\') && (z[1] == '/' || z[1] == '\\');
#else
	return n >= 1 && z[0] == '/';
#endif
}
/* "./x" and "../x": php reads these against the CWD and never walks the path. */
static int VfsPathIsDotRelative(const char *z,int n)
{
#ifdef __WINNT__
#define VFS_RESOLVE_SLASH(c) ((c) == '/' || (c) == '\\')
#else
#define VFS_RESOLVE_SLASH(c) ((c) == '/')
#endif
	if( n < 2 || z[0] != '.' ){
		return 0;
	}
	if( VFS_RESOLVE_SLASH(z[1]) ){
		return 1;
	}
	return n > 2 && z[1] == '.' && VFS_RESOLVE_SLASH(z[2]);
}
/*
 * string|false stream_resolve_include_path(string $filename)
 *  Where would include/require find this name? php's own php_resolve_path,
 *  which is the ONLY way a script can ask that question without opening
 *  anything -- and the way an autoloader decides whether a class file exists
 *  before requiring it.
 * Return
 *  The canonical path on success, FALSE when nothing answers.
 */
static int PH7_vfs_stream_resolve_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_vfs *pVfs;
	const char *zPath;
	SyString *aEntry;
	SyString sDir;
	SyBlob sWorker;
	int nPath = 0, nScheme, c;
	sxu32 n;
	/* FALSE until something resolves: xRealpath() overwrites it on the winner. */
	ph7_result_bool(pCtx,0);
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( nArg < 1 || pVfs == 0 || pVfs->xRealpath == 0 || pVfs->xFileExists == 0 ){
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	if( nPath < 0 ){
		nPath = 0;
	}
	nScheme = PH7_VmUrlSchemeLen(zPath,nPath);
	if( nScheme > 0 ){
		/* A name that carries a scheme is never walked. php resolves exactly one
		 * of them -- file://, which it realpaths where it stands -- and answers
		 * false for every other wrapper. An unreachable authority (and any other
		 * scheme) comes back unchanged from the strip, and is false. */
		const char *zLocal = PH7_VmFileUrlLocalPath(zPath);
		if( zLocal != zPath ){
			VfsResolveTry(pVfs,pCtx,zLocal);
		}
		return PH7_OK;
	}
	if( VfsPathIsDotRelative(zPath,nPath) || VfsPathIsAbsolute(zPath,nPath)
	 || SySetUsed(&pVm->aPaths) < 1 ){
		VfsResolveTry(pVfs,pCtx,zPath);
		return PH7_OK;
	}
	c = '/';
#ifdef __WINNT__
	c = '\\';
#endif
	SyBlobInit(&sWorker,&pVm->sAllocator);
	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);
	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){
		SyString sFile;
		SyStringInitFromBuf(&sFile,zPath,(sxu32)nPath);
		SyBlobReset(&sWorker);
		SyBlobFormat(&sWorker,"%z%c%z",&aEntry[n],c,&sFile);
		if( SXRET_OK != SyBlobNullAppend(&sWorker) ){
			continue;
		}
		if( VfsResolveTry(pVfs,pCtx,(const char *)SyBlobData(&sWorker)) ){
			SyBlobRelease(&sWorker);
			return PH7_OK;
		}
	}
	/* The same last resort the opener uses: the executing file's directory. */
	if( PH7_VmExecutingDir(pVm,&sDir) ){
		SyString sFile;
		SyStringInitFromBuf(&sFile,zPath,(sxu32)nPath);
		SyBlobReset(&sWorker);
		SyBlobFormat(&sWorker,"%z%c%z",&sDir,c,&sFile);
		if( SXRET_OK == SyBlobNullAppend(&sWorker) ){
			VfsResolveTry(pVfs,pCtx,(const char *)SyBlobData(&sWorker));
		}
	}
	SyBlobRelease(&sWorker);
	return PH7_OK;
}
/*
 * int sleep(int $seconds)
 *  Delays the program execution for the given number of seconds.
 * Parameters
 *  $seconds
 *   Halt time in seconds.
 * Return
 *  Zero on success or FALSE on failure.
 */
static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vfs *pVfs;
	int rc,nSleep;
	if( nArg < 1 || !ph7_value_is_int(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xSleep == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Amount to sleep */
	nSleep = ph7_value_to_int(apArg[0]);
	if( nSleep < 0 ){
		/* php raises rather than sleeping for an unsigned-wrapped eternity */
		return PH7_VmThrowException(pCtx,"ValueError",
			"sleep(): Argument #1 ($seconds) must be greater than or equal to 0");
	}
	/* Perform the requested operation (Microseconds) */
	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));
	if( rc != PH7_OK ){
		/* Return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Return zero */
		ph7_result_int(pCtx,0);
	}
	return PH7_OK;
}
/*
 * void usleep(int $micro_seconds)
 *  Delays program execution for the given number of micro seconds.
 * Parameters
 *  $micro_seconds
 *   Halt time in micro seconds. A micro second is one millionth of a second.
 * Return
 *  None.
 */
static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vfs *pVfs;
	int nSleep;
	if( nArg < 1 || !ph7_value_is_int(apArg[0]) ){
		/* Missing/Invalid argument,return immediately */
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xSleep == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS",
			ph7_function_name(pCtx)
			);
		return PH7_OK;
	}
	/* Amount to sleep */
	nSleep = ph7_value_to_int(apArg[0]);
	if( nSleep < 0 ){
		/* php raises rather than sleeping for an unsigned-wrapped eternity */
		return PH7_VmThrowException(pCtx,"ValueError",
			"usleep(): Argument #1 ($microseconds) must be greater than or equal to 0");
	}
	/* Perform the requested operation (Microseconds) */
	pVfs->xSleep((unsigned int)nSleep);
	return PH7_OK;
}
/*
 * bool unlink (string $filename)
 *  Delete a file.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc,bThrew = 0;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php's `?resource $context`, refused when it is a resource of another kind.
	 * Nothing here CONSUMES it — this operation never opens a stream, and the
	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched
	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it
	 * was accepted in silence. */
	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xUnlink == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	errno = 0;
	rc = pVfs->xUnlink(zPath);
	if( rc != PH7_OK ){
		VfsThrowSysWarning(pCtx,zPath);
	}
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool chmod(string $filename,int $mode)
 *  Attempts to change the mode of the specified file to that given in mode.
 * Parameters
 *  $filename
 *   Path to the file.
 * $mode
 *   Mode (Must be an integer)
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int iMode;
	int rc;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xChmod == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Extract the mode */
	iMode = ph7_value_to_int(apArg[1]);
	/* Perform the requested operation */
	errno = 0;
	rc = pVfs->xChmod(zPath,iMode);
	if( rc != PH7_OK ){
		/* php warns with the C library's own reason and names NO path -- one of
		 * the family's two shapes, the one mkdir() and the link pair share.
		 * PHL answered FALSE in silence. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",
			ph7_function_name(pCtx),VfsStrerror(errno));
	}
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool chown(string $filename,string $user)
 *  Attempts to change the owner of the file filename to user user.
 * Parameters
 *  $filename
 *   Path to the file.
 * $user
 *   Username.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath,*zUser;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xChown == 0 ){
		/* No ownership to change: Windows has neither a uid nor anything to look
		 * one up in, and php's own build there keeps the function and answers a
		 * SILENT false for it -- asked of php 8.5.8 on the gate guest. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Extract the user */
	zUser = ph7_value_to_string(apArg[1],0);
	/* Perform the requested operation */
	errno = 0;
	rc = pVfs->xChown(zPath,zUser);
	if( rc != PH7_OK ){
		/* php words a failed NAME lookup differently from a failed syscall, and names no
		 * path in either: "chown(): Unable to find uid for bogus" vs
		 * "chown(): Operation not permitted". */
		if( rc == -2 ){
			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",
				ph7_function_name(pCtx),zUser);
		}else{
			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",
				ph7_function_name(pCtx),VfsStrerror(errno));
		}
	}
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool chgrp(string $filename,string $group)
 *  Attempts to change the group of the file filename to group.
 * Parameters
 *  $filename
 *   Path to the file.
 * $group
 *   groupname.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath,*zGroup;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xChgrp == 0 ){
		/* No ownership to change: Windows has neither a gid nor anything to look
		 * one up in, and php's own build there keeps the function and answers a
		 * SILENT false for it -- asked of php 8.5.8 on the gate guest. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Extract the user */
	zGroup = ph7_value_to_string(apArg[1],0);
	/* Perform the requested operation */
	errno = 0;
	rc = pVfs->xChgrp(zPath,zGroup);
	if( rc != PH7_OK ){
		/* php words a failed NAME lookup differently from a failed syscall, and names no
		 * path in either: "chown(): Unable to find uid for bogus" vs
		 * "chown(): Operation not permitted". */
		if( rc == -2 ){
			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",
				ph7_function_name(pCtx),zGroup);
		}else{
			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",
				ph7_function_name(pCtx),VfsStrerror(errno));
		}
	}
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * int64 disk_free_space(string $directory)
 *  Returns available space on filesystem or disk partition.
 * Parameters
 *  $directory
 *   A directory of the filesystem or disk partition.
 * Return
 *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.
 */
static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_int64 iSize;
	ph7_vfs *pVfs;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xFreeSpace == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	if( zPath == 0 || zPath[0] == 0 ){
		/* php answers FALSE for an empty one and says nothing, as the stat
		 * family does. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	errno = 0;
	iSize = pVfs->xFreeSpace(zPath);
	if( iSize < 0 ){
		/* php answers FALSE and warns with the C library's own reason
		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --
		 * truthy, and a plausible-looking byte count for a caller that only tests
		 * `if ($free)` or compares it against a threshold. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",
			ph7_function_name(pCtx),VfsStrerror(errno));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php's return type is FLOAT, not int: the value can exceed what an int holds
	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the
	 * difference on every volume. */
	ph7_result_double(pCtx,(double)iSize);
	return PH7_OK;
}
/*
 * int64 disk_total_space(string $directory)
 *  Returns the total size of a filesystem or disk partition.
 * Parameters
 *  $directory
 *   A directory of the filesystem or disk partition.
 * Return
 *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.
 */
static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_int64 iSize;
	ph7_vfs *pVfs;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xTotalSpace == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	errno = 0;
	iSize = pVfs->xTotalSpace(zPath);
	if( iSize < 0 ){
		/* php answers FALSE and warns with the C library's own reason
		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --
		 * truthy, and a plausible-looking byte count for a caller that only tests
		 * `if ($free)` or compares it against a threshold. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",
			ph7_function_name(pCtx),VfsStrerror(errno));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php's return type is FLOAT, not int: the value can exceed what an int holds
	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the
	 * difference on every volume. */
	ph7_result_double(pCtx,(double)iSize);
	return PH7_OK;
}
/*
 * bool file_exists(string $filename)
 *  Checks whether a file or directory exists.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xFileExists == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	rc = pVfs->xFileExists(zPath);
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * int64 file_size(string $filename)
 *  Gets the size for the given file.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  File size on success or FALSE on failure.
 */
static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_int64 iSize;
	ph7_vfs *pVfs;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xFileSize == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	iSize = pVfs->xFileSize(zPath);
	if( iSize < 0 ){
		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is
		 * truthy and compares equal to nothing a caller would test for. An EMPTY
		 * path is the family's silence (VfsThrowStatWarning). */
		VfsThrowStatWarning(pCtx,zPath,0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* IO return value */
	ph7_result_int64(pCtx,iSize);
	return PH7_OK;
}
/*
 * int64 fileatime(string $filename)
 *  Gets the last access time of the given file.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  File atime on success or FALSE on failure.
 */
static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_int64 iTime;
	ph7_vfs *pVfs;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xFileAtime == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	iTime = pVfs->xFileAtime(zPath);
	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){
		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --
		 * truthy, and one second before the epoch is also a real timestamp, which
		 * is why the verdict comes from the VFS rather than from the value. */
		VfsThrowStatWarning(pCtx,zPath,0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* IO return value */
	ph7_result_int64(pCtx,iTime);
	return PH7_OK;
}
/*
 * int64 filemtime(string $filename)
 *  Gets file modification time.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  File mtime on success or FALSE on failure.
 */
static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_int64 iTime;
	ph7_vfs *pVfs;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xFileMtime == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	iTime = pVfs->xFileMtime(zPath);
	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){
		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --
		 * truthy, and one second before the epoch is also a real timestamp, which
		 * is why the verdict comes from the VFS rather than from the value. */
		VfsThrowStatWarning(pCtx,zPath,0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* IO return value */
	ph7_result_int64(pCtx,iTime);
	return PH7_OK;
}
/*
 * int64 filectime(string $filename)
 *  Gets inode change time of file.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  File ctime on success or FALSE on failure.
 */
static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_int64 iTime;
	ph7_vfs *pVfs;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xFileCtime == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	iTime = pVfs->xFileCtime(zPath);
	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){
		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --
		 * truthy, and one second before the epoch is also a real timestamp, which
		 * is why the verdict comes from the VFS rather than from the value. */
		VfsThrowStatWarning(pCtx,zPath,0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* IO return value */
	ph7_result_int64(pCtx,iTime);
	return PH7_OK;
}
/*
 * bool is_file(string $filename)
 *  Tells whether the filename is a regular file.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xIsfile == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	rc = pVfs->xIsfile(zPath);
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool is_link(string $filename)
 *  Tells whether the filename is a symbolic link.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xIslink == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	rc = pVfs->xIslink(zPath);
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool is_readable(string $filename)
 *  Tells whether a file exists and is readable.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xReadable == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	rc = pVfs->xReadable(zPath);
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool is_writable(string $filename)
 *  Tells whether the filename is writable.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xWritable == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	rc = pVfs->xWritable(zPath);
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool is_executable(string $filename)
 *  Tells whether the filename is executable.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xExecutable == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	rc = pVfs->xExecutable(zPath);
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * string filetype(string $filename)
 *  Gets file type.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  The type of the file. Possible values are fifo, char, dir, block, link
 *  file, socket and unknown.
 */
static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return 'unknown' */
		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xFiletype == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the desired directory */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Set the empty string as the default return value */
	ph7_result_string(pCtx,"",0);
	/* Perform the requested operation */
	if( pVfs->xFiletype(zPath,pCtx) != PH7_OK ){
		/* php LSTATs here (which is why a symlink answers "link") and a failure is
		 * the `Lstat failed for` warning plus FALSE. PHL answered the string
		 * "unknown" -- a real return value of this function, so a caller could not
		 * tell a missing path from a socket or a fifo. */
		VfsThrowStatWarning(pCtx,zPath,1);
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * array stat(string $filename)
 *  Gives information about a file.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  An associative array on success holding the following entries on success
 *  0   dev     device number
 * 1    ino     inode number (zero on windows)
 * 2    mode    inode protection mode
 * 3    nlink   number of links
 * 4    uid     userid of owner (zero on windows)
 * 5    gid     groupid of owner (zero on windows)
 * 6    rdev    device type, if inode device
 * 7    size    size in bytes
 * 8    atime   time of last access (Unix timestamp)
 * 9    mtime   time of last modification (Unix timestamp)
 * 10   ctime   time of last inode change (Unix timestamp)
 * 11   blksize blocksize of filesystem IO (zero on windows)
 * 12   blocks  number of 512-byte blocks allocated.
 * Note:
 *  FALSE is returned on failure.
 */
static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pValue;
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xStat == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Create the array and the working value */
	pArray = ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	rc = pVfs->xStat(zPath,pArray,pValue);
	if( rc != PH7_OK ){
		/* php warns before answering FALSE -- the same `stat failed for` /
		 * `Lstat failed for` line the rest of the family raises. PHL returned the
		 * FALSE in silence, so a missing path and an empty result looked alike. */
		VfsThrowStatWarning(pCtx,zPath,0);
		ph7_result_bool(pCtx,0);
	}else{
		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */
		ph7_value *pFull = ph7_context_new_array(pCtx);
		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){
			ph7_result_value(pCtx,pFull);
		}else{
			ph7_result_value(pCtx,pArray);
		}
	}
	/* Don't worry about freeing memory here,everything will be released
	 * automatically as soon we return from this function. */
	return PH7_OK;
}
/*
 * array lstat(string $filename)
 *  Gives information about a file or symbolic link.
 * Parameters
 *  $filename
 *   Path to the file.
 * Return
 *  An associative array on success holding the following entries on success
 *  0   dev     device number
 * 1    ino     inode number (zero on windows)
 * 2    mode    inode protection mode
 * 3    nlink   number of links
 * 4    uid     userid of owner (zero on windows)
 * 5    gid     groupid of owner (zero on windows)
 * 6    rdev    device type, if inode device
 * 7    size    size in bytes
 * 8    atime   time of last access (Unix timestamp)
 * 9    mtime   time of last modification (Unix timestamp)
 * 10   ctime   time of last inode change (Unix timestamp)
 * 11   blksize blocksize of filesystem IO (zero on windows)
 * 12   blocks  number of 512-byte blocks allocated.
 * Note:
 *  FALSE is returned on failure.
 */
static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pValue;
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xlStat == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Create the array and the working value */
	pArray = ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zPath = ph7_value_to_string(apArg[0],0);
	/* Perform the requested operation */
	rc = pVfs->xlStat(zPath,pArray,pValue);
	if( rc != PH7_OK ){
		/* php warns before answering FALSE -- the same `stat failed for` /
		 * `Lstat failed for` line the rest of the family raises. PHL returned the
		 * FALSE in silence, so a missing path and an empty result looked alike. */
		VfsThrowStatWarning(pCtx,zPath,1);
		ph7_result_bool(pCtx,0);
	}else{
		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */
		ph7_value *pFull = ph7_context_new_array(pCtx);
		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){
			ph7_result_value(pCtx,pFull);
		}else{
			ph7_result_value(pCtx,pArray);
		}
	}
	/* Don't worry about freeing memory here,everything will be released
	 * automatically as soon we return from this function. */
	return PH7_OK;
}
/*
 * int|false fileowner / filegroup / fileinode / fileperms (string $filename)
 *  One stat() with one of its fields taken out of it, which is exactly how php
 *  implements them (php_stat's FS_OWNER / FS_GROUP / FS_INODE / FS_PERMS arms).
 *
 * They were prelude PHP wrapping stat(), which cost them php's diagnostic twice
 * over: three of the four said NOTHING on a failed stat (the fourth raised its own
 * `trigger_error`, so its errno was E_USER_WARNING's 512 rather than E_WARNING's 2
 * and its line was the prelude's, not the caller's). In C the family shares one
 * warning site with the rest of stat(), and the four get real signature rows.
 */
static int VfsStatField(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zField)
{
	ph7_value *pArray,*pValue,*pField;
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xStat == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],0);
	rc = pVfs->xStat(zPath,pArray,pValue);
	if( rc != PH7_OK ){
		VfsThrowStatWarning(pCtx,zPath,0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pField = ph7_array_fetch(pArray,zField,-1);
	if( pField == 0 ){
		/* The VFS answered a stat array without this field: nothing to report but
		 * the failure itself, which is what php answers when its own stat has no
		 * such member either. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,ph7_value_to_int64(pField));
	return PH7_OK;
}
static int PH7_vfs_file_owner(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return VfsStatField(pCtx,nArg,apArg,"uid");
}
static int PH7_vfs_file_group(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return VfsStatField(pCtx,nArg,apArg,"gid");
}
static int PH7_vfs_file_inode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return VfsStatField(pCtx,nArg,apArg,"ino");
}
static int PH7_vfs_file_perms(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return VfsStatField(pCtx,nArg,apArg,"mode");
}
/*
 * array|string|false getenv(?string $name = null, bool $local_only = false)
 *  Gets the value of an environment variable.
 * Parameters
 *  $name
 *   The variable name -- or NOTHING, which is the documented way to ask for the
 *   WHOLE environment as a name => value array. That form answered FALSE here,
 *   so `foreach (getenv() as $k => $v)` iterated over a bool.
 *  $local_only
 *   Ask only the process's own environment rather than the SAPI's. On the CLI
 *   they are the same environment, so the argument selects the same answer --
 *   but it must still be ACCEPTED, and asking for the whole map with it set
 *   answered false too.
 * Return
 *  The value of the environment variable, or FALSE when it does not exist, or
 *  the whole environment when no name is given.
 */
static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zEnv;
	ph7_vfs *pVfs;
	int iLen;
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( nArg < 1 || ph7_value_is_null(apArg[0]) ){
		/* The whole environment. xEnviron was APPENDED to ph7_vfs, so an
		 * embedder VFS built against version 2 does not have the field at all --
		 * reading it would run off the end of their struct. */
		if( pVfs == 0 || pVfs->iVersion < 3 || pVfs->xEnviron == 0
		 || pVfs->xEnviron(pCtx) != PH7_OK ){
			ph7_result_bool(pCtx,0);
		}
		return PH7_OK;
	}
	if( pVfs == 0 || pVfs->xGetenv == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the environment variable */
	zEnv = ph7_value_to_string(apArg[0],&iLen);
	/* Set a boolean FALSE as the default return value */
	ph7_result_bool(pCtx,0);
	if( iLen < 1 ){
		/* Empty string */
		return PH7_OK;
	}
	/* Perform the requested operation */
	pVfs->xGetenv(zEnv,pCtx);
	return PH7_OK;
}
/*
 * bool putenv(string $settings)
 *  Set the value of an environment variable.
 * Parameters
 *  $setting
 *   The setting, like "FOO=BAR"
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName,*zValue;
	char *zSettings,*zEnd;
	ph7_vfs *pVfs;
	int iLen,rc;
	if( nArg < 1 ){
		/* Missing argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the setting variable. It is NOT required to already BE a string:
	 * the declared parameter is `string $assignment`, so php coerces an int or a
	 * __toString() object first, where PH7 answered false and did nothing. */
	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);
	if( iLen < 1 || zSettings[0] == '=' ){
		/* php's whole validity rule: an empty assignment, or one with no name in
		 * front of the '='. Everything else is accepted. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"putenv(): Argument #1 ($assignment) must have a valid syntax");
	}
	/* Parse the setting. php looks for the '=' with strchr(), so an embedded NUL
	 * ENDS the search: putenv("FO\0O=BAR") finds no '=' at all and removes the
	 * variable named "FO" instead of setting one. */
	zEnd = &zSettings[iLen];
	zValue = 0;
	zName = zSettings;
	while( zSettings < zEnd && zSettings[0] != 0 ){
		if( zSettings[0] == '=' ){
			/* Null terminate the name */
			zSettings[0] = 0;
			zValue = &zSettings[1];
			break;
		}
		zSettings++;
	}
	/* A missing '=' is not invalid syntax: `putenv("NAME")` REMOVES the variable,
	 * which is the documented way to unset one, and PH7 read it as a failure and
	 * left the old value in place. An empty VALUE is a value too
	 * (`putenv("NAME=")`), which the old `zValue >= zEnd` test rejected.
	 * php does NOT touch $_ENV here: that array is the SAPI's startup snapshot,
	 * and a putenv() after it changes the process environment alone. PH7 wrote
	 * the pair into $_ENV as well, so a script could read back through $_ENV a
	 * variable php only exposes through getenv(). */
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xSetenv == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		if( zValue ){
			zSettings[0] = '=';
		}
		return PH7_OK;
	}
	/* Perform the requested operation. A NULL value means REMOVE, and php reports
	 * TRUE for that whether or not the variable was there (or nameable) at all --
	 * only a failed SET is false. */
	rc = pVfs->xSetenv(zName,zValue);
	ph7_result_bool(pCtx,zValue == 0 || rc == PH7_OK );
	if( zValue ){
		/* Put back the '=' the name was terminated on. Without one, zSettings
		 * stopped on the terminator or on an embedded NUL, neither of which this
		 * routine wrote. */
		zSettings[0] = '=';
	}
	return PH7_OK;
}
/*
 * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])
 *  Sets access and modification time of file.
 * Note: On windows
 *   If the file does not exists,it will not be created.
 * Parameters
 *  $filename
 *   The name of the file being touched.
 *  $time
 *   The touch time. If time is not supplied, the current system time is used.
 * $atime
 *   If present, the access time of the given filename is set to the value of atime.
 *   Otherwise, it is set to the value passed to the time parameter. If neither are
 *   present, the current system time is used.
 * Return
 *  TRUE on success or FALSE on failure.
*/
static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 nTime,nAccess;
	const char *zFile;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xTouch == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Resolve php's defaults HERE, so the driver only ever sees real timestamps: a
	 * NEGATIVE stamp is perfectly legal to php (`touch($f, -100)` is 1969), so it
	 * cannot double as the "not given" sentinel the drivers used to read it as. An
	 * omitted/null $mtime is NOW; an omitted/null $atime follows $mtime. $atime also
	 * used to be read from apArg[1] — the mtime — so touch($f, $m, $a) silently
	 * stamped the modification time onto both. */
	zFile = ph7_value_to_string(apArg[0],0);
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) && nArg > 1 && ph7_value_is_null(apArg[1]) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"touch(): Argument #2 ($mtime) cannot be null when argument #3 ($atime) "
			"is an integer");
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		nTime = ph7_value_to_int64(apArg[1]);
	}else{
		time_t tNow;
		time(&tNow);
		nTime = (ph7_int64)tNow;
	}
	nAccess = nTime;
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		nAccess = ph7_value_to_int64(apArg[2]);
	}
	errno = 0;
	rc = pVfs->xTouch(zFile,nTime,nAccess);
	if( rc != PH7_OK ){
		/* php's own sentence for this one, which names both the path and the
		 * reason and reads like neither of the family's other two. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to create file %s because %s",
			ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));
	}
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * Path processing functions that do not need access to the VFS layer
 * Status:
 *    Stable.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * string dirname(string $path)

 *  Returns parent directory's path.
 * Parameters
 * $path
 *  Target path.
 *  On Windows, both slash (/) and backslash (\) are used as directory separator character.
 *  In other environments, it is the forward slash (/).
 * Return
 *  The path of the parent directory. If there are no slashes in path, a dot ('.')
 *  is returned, indicating the current directory.
 */
static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath,*zDir;
	int iLen,iDirlen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Point to the target path */
	zPath = ph7_value_to_string(apArg[0],&iLen);
	if( iLen < 1 ){
		/* php answers "" for the empty path, not "." */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* $levels (php 7.0) was ACCEPTED AND IGNORED, so dirname($p, 3) silently answered
	 * the one-level parent — the caller's own answer, one or more levels too deep. Each
	 * level re-runs php_dirname on the previous result and stops as soon as the answer
	 * stops moving (the filesystem root, or "." for a relative path), which is what php
	 * does; php also rejects a level below 1 outright. */
	zDir = zPath;
	iDirlen = iLen;
	if( nArg > 1 ){
		ph7_int64 nLevels = ph7_value_to_int64(apArg[1]);
		ph7_int64 i;
		if( nLevels < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"dirname(): Argument #2 ($levels) must be greater than or equal to 1");
		}
		for( i = 0 ; i < nLevels ; ++i ){
			int iPrevLen = iDirlen;
			const char *zPrev = zDir;
			zDir = PH7_ExtractDirName(zPrev,iPrevLen,&iDirlen);
			if( iDirlen == iPrevLen && SyMemcmp(zDir,zPrev,(sxu32)iDirlen) == 0 ){
				break; /* fixed point: "/" and "." are their own parents */
			}
		}
	}else{
		zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);
	}
	/* Return directory name */
	ph7_result_string(pCtx,zDir,iDirlen);
	return PH7_OK;
}
/*
 * string basename(string $path[, string $suffix ])
 *  Returns trailing name component of path.
 * Parameters
 * $path
 *  Target path.
 *  On Windows, both slash (/) and backslash (\) are used as directory separator character.
 *  In other environments, it is the forward slash (/).
 * $suffix
 *  If the name component ends in suffix this will also be cut off.
 * Return
 *  The base name of the given path.
 */
static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath,*zBase;
	int iLen,nBase;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Point to the target path */
	zPath = ph7_value_to_string(apArg[0],&iLen);
	/* php_basename, shared with pathinfo(): the hand-rolled walk this used to carry
	 * kept a leading separator on a single-component path (basename("/a") answered
	 * "/a", basename("/.") answered "/.") because it stopped one byte short. */
	zBase = PH7_ExtractBaseName(zPath,iLen,&nBase);
	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
		const char *zSuffix;
		int nSuffix;
		/* Strip suffix — php leaves the basename alone when it IS the suffix */
		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);
		if( nSuffix > 0 && nSuffix < nBase
		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,nSuffix) == 0 ){
			nBase -= nSuffix;
		}
	}
	/* Store the basename */
	ph7_result_string(pCtx,zBase,nBase);
	return PH7_OK;
}
/*
 * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME | PATHINFO_BASENAME | PATHINFO_EXTENSION | PATHINFO_FILENAME ])
 *  Returns information about a file path.
 * Parameter
 *  $path
 *   The path to be parsed.
 *  $options
 *    If present, specifies a specific element to be returned; one of
 *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.
 * Return
 *  If the options parameter is not passed, an associative array containing the following
 *  elements is returned: dirname, basename, extension (if any), and filename.
 *  If options is present, returns a string containing the requested element.
 */
typedef struct path_info path_info;
struct path_info
{
	SyString sDir; /* Directory [i.e: /var/www] */
	SyString sBasename; /* Basename [i.e httpd.conf] */
	SyString sExtension; /* File extension [i.e xml,pdf..] */
	SyString sFilename;  /* Filename */
	int iPresent;        /* Which components php would EMIT (PH7_PATHINFO_* bits) */
};
/*
 * Extract path fields exactly as php's pathinfo() assembles them.
 *
 * Two things this has to get right beyond the values themselves:
 *
 *  - php looks for the LAST dot ANYWHERE in the basename, a leading one included,
 *    so `.bashrc` has extension "bashrc" and filename "" (PH7 stopped the scan
 *    before the first byte, so it reported no extension and filename ".bashrc").
 *  - EMPTY is not the same as ABSENT. php always emits basename and filename when
 *    they are asked for, emits extension whenever a dot exists (even for `x.`,
 *    whose extension is ""), and emits dirname only when it is non-empty. The
 *    scalar form answers with the first EMITTED component, so conflating the two
 *    makes `pathinfo("x.", PATHINFO_EXTENSION|PATHINFO_FILENAME)` fall through to
 *    the filename ("x") where php answers "" — iPresent keeps them apart.
 *
 * dirname and basename come from the shared php_dirname/php_basename helpers
 * rather than a third hand-rolled walk, so the trailing-separator and
 * relative-path rules ("file.txt" -> ".", "/var/www/" -> "/var" + "www") cannot
 * drift between the two builtins and this one.
 */
static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)
{
	const char *zBase,*zDir,*zDot;
	int nBase,nDir,i;
	/* Zero the structure */
	SyZero(pOut,sizeof(path_info));
	zDir = PH7_ExtractDirName(zPath,nByte,&nDir);
	if( nDir > 0 ){
		SyStringInitFromBuf(&pOut->sDir,zDir,nDir);
		pOut->iPresent |= PH7_PATHINFO_DIRNAME;
	}
	zBase = PH7_ExtractBaseName(zPath,nByte,&nBase);
	SyStringInitFromBuf(&pOut->sBasename,zBase,nBase);
	pOut->iPresent |= PH7_PATHINFO_BASENAME|PH7_PATHINFO_FILENAME;
	/* Last dot anywhere in the basename splits filename from extension */
	zDot = 0;
	for( i = nBase ; i > 0 ; --i ){
		if( zBase[i - 1] == '.' ){
			zDot = &zBase[i - 1];
			break;
		}
	}
	if( zDot ){
		SyStringInitFromBuf(&pOut->sExtension,zDot + 1,(int)(&zBase[nBase] - (zDot + 1)));
		pOut->iPresent |= PH7_PATHINFO_EXTENSION;
		SyStringInitFromBuf(&pOut->sFilename,zBase,(int)(zDot - zBase));
	}else{
		SyStringInitFromBuf(&pOut->sFilename,zBase,nBase);
	}
	return SXRET_OK;
}
/*
 * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME | PATHINFO_BASENAME | PATHINFO_EXTENSION | PATHINFO_FILENAME ])
 *  See block comment above.
 */
static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	path_info sInfo;
	int iLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Point to the target path. The EMPTY path is not a special case: php still
	 * answers with the array `["basename" => "", "filename" => ""]` (and "" for a
	 * scalar request), where PH7 short-circuited to "" and returned the wrong TYPE. */
	zPath = ph7_value_to_string(apArg[0],&iLen);
	/* Extract path info */
	ExtractPathInfo(zPath,iLen,&sInfo);
	/* Read the mask at 64-bit width: ph7_value_to_int() truncates to `int`, so a
	 * flags value congruent to PATHINFO_ALL mod 2^32 (4294967311, -4294967281 …)
	 * would take the ARRAY branch and answer with the wrong TYPE. */
	if( nArg > 1 && ph7_value_is_int(apArg[1])
	 && ph7_value_to_int64(apArg[1]) != (ph7_int64)PH7_PATHINFO_ALL ){
		/* $flags is a BITMASK, not an enum: php assembles the requested components in
		 * the fixed order below and, for anything other than PATHINFO_ALL, hands back
		 * the FIRST one it EMITTED (zend_hash_get_current_data on the fresh array).
		 * So `PATHINFO_DIRNAME|PATHINFO_BASENAME` answers the dirname, and an unknown
		 * bit that happens to carry a known one along (99 = 1|2|32|64) answers as if
		 * only the known ones were passed. PH7 numbered the components 1/2/3/4 and
		 * switched on the whole value, so it read a two-flag mask as a different single
		 * component and answered "" for everything else. Emission is iPresent, NOT
		 * "non-empty": an emitted-but-empty component ends the search with "". */
		ph7_int64 nComp = ph7_value_to_int64(apArg[1]);
		static const int aBit[4] = {
			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,
			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME
		};
		SyString *apComp[4];
		int i;
		apComp[0] = &sInfo.sDir;
		apComp[1] = &sInfo.sBasename;
		apComp[2] = &sInfo.sExtension;
		apComp[3] = &sInfo.sFilename;
		/* Expand the empty string unless a requested component is emitted */
		ph7_result_string(pCtx,"",0);
		for( i = 0 ; i < 4 ; ++i ){
			if( (nComp & aBit[i]) == aBit[i] && (sInfo.iPresent & aBit[i]) ){
				ph7_result_string(pCtx,apComp[i]->zString,(int)apComp[i]->nByte);
				break;
			}
		}
	}else{
		/* Return an associative array */
		ph7_value *pArray,*pValue;
		pArray = ph7_context_new_array(pCtx);
		pValue = ph7_context_new_scalar(pCtx);
		if( pArray == 0 || pValue == 0 ){
			/* Out of mem,return NULL */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Emitted-but-EMPTY components are in the array too (php keys `basename` and
		 * `filename` for "/" with ""), so this walks iPresent, not the lengths. */
		{
		static const char *azKey[4] = {"dirname","basename","extension","filename"};
		static const int aBit[4] = {
			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,
			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME
		};
		SyString *apComp[4];
		int i;
		apComp[0] = &sInfo.sDir;
		apComp[1] = &sInfo.sBasename;
		apComp[2] = &sInfo.sExtension;
		apComp[3] = &sInfo.sFilename;
		for( i = 0 ; i < 4 ; ++i ){
			if( (sInfo.iPresent & aBit[i]) == 0 ){
				continue;
			}
			ph7_value_reset_string_cursor(pValue);
			ph7_value_string(pValue,apComp[i]->zString,(int)apComp[i]->nByte);
			ph7_array_add_strkey_elem(pArray,azKey[i],pValue); /* Will make it's own copy */
		}
		}
		/* Return the created array */
		ph7_result_value(pCtx,pArray);
		/* Don't worry about freeing memory, everything will be released
		 * automatically as soon we return from this foreign function.
		 */
	}
	return PH7_OK;
}
/* SPDX-SnippetBegin */
/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */
/* SPDX-License-Identifier: blessing */
/*
 * Globbing implementation extracted from the sqlite3 source tree.

 * Original author: D. Richard Hipp (http://www.sqlite.org)
 * Status: Public Domain
 */
typedef unsigned char u8;
/* An array to map all upper-case characters into their corresponding
** lower-case character.
**
** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not
** handle case conversions for the UTF character set since the tables
** involved are nearly as big or bigger than SQLite itself.
*/
static const unsigned char sqlite3UpperToLower[] = {
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,
     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,
    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,
    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,
    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,
    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,
    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,
    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,
    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,
    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,
    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,
    252,253,254,255
};
#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }
/*
** Assuming zIn points to the first byte of a UTF-8 character,
** advance zIn to point to the first byte of the next UTF-8 character.
*/
#define SQLITE_SKIP_UTF8(zIn) {                        \
  if( (*(zIn++))>=0xc0 ){                              \
    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \
  }                                                    \
}
/*
** Compare two UTF-8 strings for equality where the first string can
** potentially be a "glob" expression.  Return true (1) if they
** are the same and false (0) if they are different.
**
** Globbing rules:
**
**      '*'       Matches any sequence of zero or more characters.
**
**      '?'       Matches exactly one character.
**
**     [...]      Matches one character from the enclosed list of
**                characters.
**
**     [^...]     Matches one character not in the enclosed list.
**
** With the [...] and [^...] matching, a ']' character can be included
** in the list by making it the first character after '[' or '^'.  A
** range of characters can be specified using '-'.  Example:
** "[a-z]" matches any single lower-case letter.  To match a '-', make
** it the last character in the list.
**
** This routine is usually quick, but can be N**2 in the worst case.
**
** Hints: to match '*' or '?', put them in "[]".  Like this:
**
**         abc[*]xyz        Matches "abc*xyz" only
*/
/*
 * One POSIX character class of a `[...]` set, as glibc's matcher answers it.
 * The classes are ASCII-only in the C locale php runs its fnmatch()/glob() in,
 * so a code point past 127 belongs to none of them.
 */
static int PatternPosixClass(const unsigned char *zName,int nName,int c)
{
	static const struct { const char *zName; int nName; } aClass[] = {
		{ "alnum", 5 }, { "alpha", 5 }, { "blank", 5 }, { "cntrl", 5 },
		{ "digit", 5 }, { "graph", 5 }, { "lower", 5 }, { "print", 5 },
		{ "punct", 5 }, { "space", 5 }, { "upper", 5 }, { "xdigit", 6 },
	};
	int i,iWhich = -1;
	for( i = 0 ; i < (int)(sizeof(aClass)/sizeof(aClass[0])) ; ++i ){
		if( aClass[i].nName == nName
		 && SyMemcmp(aClass[i].zName,(const char *)zName,(sxu32)nName) == 0 ){
			iWhich = i;
			break;
		}
	}
	if( iWhich < 0 || c < 0 || c > 127 ){
		/* An unknown class name matches nothing, which is what a matcher that
		 * cannot name the set can honestly say. */
		return 0;
	}
	switch( iWhich ){
		case 0: return SyisAlphaNum(c);
		case 1: return SyisAlpha(c);
		case 2: return c == ' ' || c == '\t';
		case 3: return c < 0x20 || c == 0x7F;
		case 4: return SyisDigit(c);
		case 5: return c > 0x20 && c < 0x7F;
		case 6: return SyisLower(c);
		case 7: return c >= 0x20 && c < 0x7F;
		case 8: return c > 0x20 && c < 0x7F && !SyisAlphaNum(c);
		case 9: return SyisSpace(c);
		case 10: return SyisUpper(c);
		default: return SyisHex(c);
	}
}
static int patternCompare(
  const u8 *zPattern,              /* The glob pattern */
  const u8 *zString,               /* The string to compare against the glob */
  const int esc,                    /* The escape character */
  int noCase,
  int bCaret                        /* `[^...]` inverts (fnmatch) or is a literal `^` (glob) */
){
  int c, c2, cLow;
  int invert;
  int seen;
  u8 matchOne = '?';
  u8 matchAll = '*';
  u8 matchSet = '[';
  int prevEscape = 0;     /* True if the previous character was 'escape' */

  if( !zPattern || !zString ) return 0;
  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){
    if( !prevEscape && c==matchAll ){
      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll
               || c == matchOne ){
        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){
          return 0;
        }
      }
      if( c==0 ){
        return 1;
      }else if( c==esc ){
        c = PH7_Utf8Read(zPattern, 0, &zPattern);
        if( c==0 ){
          return 0;
        }
      }else if( c==matchSet ){
        /* A `[...]` set right after a `*`: try it at every remaining position.
         * The two asserts SQLite has here became guards, and one of them --
         * "'[' is a single-byte character" -- is ALWAYS true, so this branch
         * returned 0 for every pattern of the shape `*[...]`. `*[ab]`,
         * `a*[0-9]` and `*[[:digit:]]` matched NOTHING, in fnmatch(), in
         * glob() and in strglob() alike. */
        while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase,bCaret)==0 ){
          SQLITE_SKIP_UTF8(zString);
        }
        return *zString!=0;
      }
      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){
        if( noCase ){
          GlogUpperToLower(c2);
          GlogUpperToLower(c);
          while( c2 != 0 && c2 != c ){
            c2 = PH7_Utf8Read(zString, 0, &zString);
            GlogUpperToLower(c2);
          }
        }else{
          while( c2 != 0 && c2 != c ){
            c2 = PH7_Utf8Read(zString, 0, &zString);
          }
        }
        if( c2==0 ) return 0;
		if( patternCompare(zPattern,zString,esc,noCase,bCaret) ) return 1;
      }
      return 0;
    }else if( !prevEscape && c==matchOne ){
      if( PH7_Utf8Read(zString, 0, &zString)==0 ){
        return 0;
      }
    }else if( c==matchSet ){
      int prior_c = 0;
      /* SQLite asserts here that its GLOB has no escape character; the guard
       * that replaced the assert reads the condition BACKWARDS, so a set
       * matched nothing whenever escaping was turned off -- every `[...]` in
       * an `fnmatch($p,$s,FNM_NOESCAPE)` call answered false. */
      seen = 0;
      invert = 0;
      c = PH7_Utf8Read(zString, 0, &zString);
      if( c==0 ) return 0;
      /* A case-INSENSITIVE match folds inside the set too: this branch ignored
       * noCase entirely, so `fnmatch('[a-c]','B',FNM_CASEFOLD)` was false and
       * its negation `[!a-c]` was true -- both the opposite of php's. The
       * folded subject is what MEMBERS and RANGES are compared against; a
       * character CLASS is not folded at all (glibc tests `[[:upper:]]`
       * against the character as written, FNM_CASEFOLD or not). */
      cLow = c;
      if( noCase ){
        GlogUpperToLower(cLow);
      }
      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);
      /* POSIX spells the negation `!` and glibc accepts `^` as well; php's
       * fnmatch()/glob() are glibc's, so BOTH invert. Only `^` did here, which
       * made `[!a]` a set holding `!` and `a` -- the exact INVERSE answer for
       * the spelling a shell uses. */
      if( c2=='!' || (bCaret && c2=='^') ){
        invert = 1;
        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);
      }
      if( c2==']' ){
        if( c==']' ) seen = 1;
        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);
      }
      while( c2 && c2!=']' ){
        int cFold = c2;
        if( noCase ){
          GlogUpperToLower(cFold);
        }
        if( c2=='[' && zPattern[0]==':' ){
          /* A POSIX character CLASS, `[:alpha:]`, which glibc's matcher knows
           * and this one did not -- the whole `[[:digit:]]` bracket read as the
           * literal set `[:digt` and matched the wrong characters in silence. */
          const unsigned char *zName = &zPattern[1];
          const unsigned char *zEnd = zName;
          while( zEnd[0] != 0 && !(zEnd[0]==':' && zEnd[1]==']') ){
            zEnd++;
          }
          if( zEnd[0] != 0 ){
            if( PatternPosixClass(zName,(int)(zEnd - zName),c) ){
              seen = 1;
            }
            zPattern = zEnd + 2;
            prior_c = 0;
            c2 = PH7_Utf8Read(zPattern, 0, &zPattern);
            continue;
          }
        }
        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){
          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);
          cFold = c2;
          if( noCase ){
            GlogUpperToLower(cFold);
          }
          if( cLow>=prior_c && cLow<=cFold ) seen = 1;
          prior_c = 0;
        }else{
          if( cLow==cFold ){
            seen = 1;
          }
          prior_c = cFold;
        }
        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);
      }
      if( c2==0 || (seen ^ invert)==0 ){
        return 0;
      }
    }else if( esc==c && !prevEscape ){
      prevEscape = 1;
    }else{
      c2 = PH7_Utf8Read(zString, 0, &zString);
      if( noCase ){
        GlogUpperToLower(c);
        GlogUpperToLower(c2);
      }
      if( c!=c2 ){
        return 0;
      }
      prevEscape = 0;
    }
  }
  return *zString==0;
}
/* SPDX-SnippetEnd */
/*
 * Wrapper around patternCompare() defined above.
 * See block comment above for more information.
 */
static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,
	int CaseCompare,int bCaret)
{
	int rc;
	if( iEsc < 0 ){
		iEsc = '\\';
	}
	rc = patternCompare(zPattern,zString,iEsc,CaseCompare,bCaret);
	return rc;
}
/*
 * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])
 *  Match filename against a pattern.
 * Parameters
 *  $pattern
 *   The shell wildcard pattern.
 * $string
 *  The tested string.
 * $flags
 *   A list of possible flags:
 *    FNM_NOESCAPE 	Disable backslash escaping.
 *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.
 *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.
 *    FNM_CASEFOLD 	Caseless match.
 * Return
 *  TRUE if there is a match, FALSE otherwise.
 */
static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zPattern;
	int iEsc = '\\';
	int noCase = 0;
	int rc;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the pattern and the string */
	zPattern  = ph7_value_to_string(apArg[0],0);
	zString = ph7_value_to_string(apArg[1],0);
	/* Extract the flags if avaialble */
	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){
		rc = ph7_value_to_int(apArg[2]);
		if( rc & 2 /*FNM_NOESCAPE (php value)*/){
			iEsc = 0;
		}
		if( rc & 16 /*FNM_CASEFOLD (php value)*/){
			noCase = 1;
		}
	}
	/* Go globbing. fnmatch() is glibc's, whose matcher takes `^` as a second
	 * spelling of the negation -- glob(3)'s does NOT, and strglob() below
	 * carries glob()'s rule because that is what the prelude glob() drives. */
	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase,TRUE);
	/* Globbing result */
	ph7_result_bool(pCtx,rc);
	return PH7_OK;
}
/*
 * bool strglob(string $pattern,string $string)
 *  Match string against a pattern.
 * Parameters
 *  $pattern
 *   The shell wildcard pattern.
 * $string
 *  The tested string.
 * Return
 *  TRUE if there is a match, FALSE otherwise.
 * Note that this a symisc eXtension.
 */
static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zPattern;
	int iEsc = '\\';
	int rc;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the pattern and the string */
	zPattern  = ph7_value_to_string(apArg[0],0);
	zString = ph7_value_to_string(apArg[1],0);
	/* Go globbing, with glob(3)'s set rules: only `!` inverts, and a `^` right
	 * after the `[` is an ordinary member of the set. */
	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0,FALSE);
	/* Globbing result */
	ph7_result_bool(pCtx,rc);
	return PH7_OK;
}
#ifndef PH7_DISABLE_DISK_IO
/* Every buffer below is one path, and php's own limit for one is PATH_MAX; the
 * SPL directory opener already refuses a longer one, and a pattern past it can
 * name nothing that exists. It also bounds the recursion: each level of the
 * walk consumes at least one slash of the pattern. */
#define PH7_GLOB_PATH_MAX 4096
/*
 * ---------------------------------------------------------------------------
 * The glob:// stream device.
 *
 * php's glob wrapper is a DIRECTORY whose entries are a pattern's matches:
 * `opendir('glob://src/' . '*.php')` hands out one BASENAME per match, and
 * GlobIterator is that stream behind the whole DirectoryIterator machinery. It
 * is a dir_opener and NOTHING else -- php gives it no stream opener (so
 * `fopen('glob://…')` is "wrapper does not support stream open") and no
 * url_stat (so `file_exists()` and `is_dir()` answer false for one).
 *
 * The expansion is glob(3) with NO flags, which is what php's opener asks for,
 * so it has to agree name for name AND order for order with the prelude
 * glob(). Two rules are the whole of it -- a pattern is matched one SEGMENT at
 * a time, and the answer is sorted by BYTES rather than by value -- and both
 * are spelled here the way that function spells them, because the two
 * implementations must not drift: 001-smoke/glob_stream_device.phpt walks a
 * table of patterns through both and compares, which is what pins them.
 *
 * php's `pglob->path` is the directory of the match a read just handed OUT,
 * never the pattern's: `glob://a/` + `*` + `/` + `*.txt` reports `a/sub1`, then
 * `a/sub2`; a match with no slash in it reports the EMPTY string; and running
 * out clears it, which is why GlobIterator::getPathname() answers "" past the
 * end.
 * ---------------------------------------------------------------------------
 */
/* One matched path. The blob it points into grows as the walk does, so an
 * OFFSET is what may be kept -- a pointer would not survive the next append. */
typedef struct glob_hit glob_hit;
struct glob_hit
{
	sxu32 nOfs;   /* where this path starts in glob_stream.sHit */
	sxu32 nLen;
};
typedef struct glob_stream glob_stream;
struct glob_stream
{
	ph7_vm *pVm;
	SyBlob sHit;   /* every matched path, back to back */
	SySet aHit;    /* one glob_hit per match, in php's order */
	sxu32 nCur;    /* php's pglob->index -- it counts PAST the end too */
	SyBlob sDir;   /* php's pglob->path: the directory of the CURRENT match */
};
/* php's glob_pattern_p: is there anything here for glob(3) to expand? */
static int GlobHasMeta(const char *zPat,int nPat)
{
	int i;
	for( i = 0 ; i < nPat ; ++i ){
		if( zPat[i] == '*' || zPat[i] == '?' || zPat[i] == '[' ){
			return 1;
		}
	}
	return 0;
}
/* strcoll() in the C locale php runs in: unsigned bytes, then the shorter one
 * first. The same comparison the prelude glob() gets out of SORT_STRING. */
static int GlobCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)
{
	sxu32 nMin = nA < nB ? nA : nB;
	sxu32 i;
	for( i = 0 ; i < nMin ; ++i ){
		int ca = (unsigned char)zA[i];
		int cb = (unsigned char)zB[i];
		if( ca != cb ){
			return ca < cb ? -1 : 1;
		}
	}
	if( nA == nB ){
		return 0;
	}
	return nA < nB ? -1 : 1;
}
/*
 * Order one call's own answers. glob(3) sorts the whole list it is about to
 * return rather than each directory it walked, and so does the prelude glob(),
 * so every branch below sorts the range IT produced.
 *
 * A shell sort: filenames within one answer are unique, so nothing here needs
 * to be stable, and a directory of ten thousand entries must not cost the
 * hundred million comparisons an insertion sort would.
 */
static void GlobSort(SyBlob *pHit,SySet *pSet,sxu32 nStart)
{
	glob_hit *aHit = (glob_hit *)SySetBasePtr(pSet);
	const char *zBase = (const char *)SyBlobData(pHit);
	sxu32 nEnd = SySetUsed(pSet);
	sxu32 nSpan,nGap;
	if( nEnd - nStart < 2 ){
		return;
	}
	nSpan = nEnd - nStart;
	for( nGap = nSpan / 2 ; nGap > 0 ; nGap /= 2 ){
		sxu32 i;
		for( i = nStart + nGap ; i < nEnd ; ++i ){
			glob_hit sTmp = aHit[i];
			sxu32 j = i;
			while( j >= nStart + nGap
			 && GlobCmp(&zBase[aHit[j-nGap].nOfs],aHit[j-nGap].nLen,
			            &zBase[sTmp.nOfs],sTmp.nLen) > 0 ){
				aHit[j] = aHit[j-nGap];
				j -= nGap;
			}
			aHit[j] = sTmp;
		}
	}
}
/* Record one match, spelled as a head and a tail so that the trailing-slash
 * branch can put its slash back without a second buffer. */
static sxi32 GlobAdd(SyBlob *pHit,SySet *pSet,const char *zHead,sxu32 nHead,
	const char *zTail,sxu32 nTail)
{
	glob_hit sHit;
	sHit.nOfs = SyBlobLength(pHit);
	sHit.nLen = nHead + nTail;
	if( nHead > 0 && SyBlobAppend(pHit,zHead,nHead) != SXRET_OK ){
		return SXERR_MEM;
	}
	if( nTail > 0 && SyBlobAppend(pHit,zTail,nTail) != SXRET_OK ){
		return SXERR_MEM;
	}
	return SySetPut(pSet,(const void *)&sHit);
}
/* Forward: the two halves of the walk call each other. */
static sxi32 GlobExpand(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,
	SyBlob *pHit,SySet *pSet);
/*
 * php's leaf: read the directory the pattern's last slash names and keep every
 * entry the segment after it matches, with that literal prefix back in front.
 * A directory that cannot be opened is zero matches in SILENCE, which is what
 * glob(3) answers for a path that is not there.
 */
static sxi32 GlobLeaf(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,
	SyBlob *pHit,SySet *pSet)
{
	const ph7_vfs *pVfs = pVm->pEngine->pVfs;
	const ph7_io_stream *pStream;
	const char *zDev,*zSeg;
	void *pHandle = 0;
	ph7_context sCtx;
	ph7_value sEntry;
	char zDir[PH7_GLOB_PATH_MAX],zSegBuf[PH7_GLOB_PATH_MAX],zEnt[PH7_GLOB_PATH_MAX];
	int nDir,nPrefix,nSeg,i,iSlash = -1;
	sxi32 rc = SXRET_OK;
	for( i = nPat - 1 ; i >= 0 ; --i ){
		if( zPat[i] == '/' ){
			iSlash = i;
			break;
		}
	}
	if( iSlash < 0 ){
		/* no directory part at all: php's own `.` */
		zDir[0] = '.';
		nDir = 1;
		nPrefix = 0;
	}else if( iSlash == 0 ){
		/* the pattern is rooted: the directory is `/` itself */
		zDir[0] = '/';
		nDir = 1;
		nPrefix = 1;
	}else{
		nDir = iSlash;
		SyMemcpy(zPat,zDir,(sxu32)nDir);
		nPrefix = iSlash + 1;
	}
	zDir[nDir] = 0;
	zSeg = &zPat[iSlash + 1];
	nSeg = nPat - (iSlash + 1);
	if( nSeg >= (int)sizeof(zSegBuf) ){
		return SXRET_OK;
	}
	SyMemcpy(zSeg,zSegBuf,(sxu32)nSeg);
	zSegBuf[nSeg] = 0;
	/* The directory is opened through the SAME lookup opendir() uses, so the
	 * two implementations see one filesystem: the prelude glob() reaches it by
	 * calling opendir() itself. */
	zDev = zDir;
	pStream = PH7_VmGetStreamDevice(pVm,&zDev,nDir);
	if( pStream == 0 || pStream->xOpenDir == 0 || pStream->xReadDir == 0 ){
		return SXRET_OK;
	}
	/* The VFS reports a name by writing a RESULT, so the read needs a call
	 * context of its own, and the cursor is reset between entries because a
	 * result APPENDS (rule 54). The same value carries the VM into the open:
	 * a device reaches it through that argument and nothing else. */
	PH7_MemObjInit(pVm,&sEntry);
	if( pStream->xOpenDir(zDev,&sEntry,&pHandle) != PH7_OK ){
		PH7_MemObjRelease(&sEntry);
		return SXRET_OK;
	}
	VmInitCallContext(&sCtx,pVm,0,&sEntry,0);
	for(;;){
		const char *zName;
		int nName = 0;
		ph7_value_reset_string_cursor(&sEntry);
		if( pStream->xReadDir(pHandle,&sCtx) != PH7_OK ){
			break;
		}
		zName = ph7_value_to_string(&sEntry,&nName);
		if( nName < 1 || nName >= (int)sizeof(zEnt)
		 || nPrefix + nName >= (int)sizeof(zEnt) ){
			continue;
		}
		SyMemcpy(zName,zEnt,(sxu32)nName);
		zEnt[nName] = 0;
		/* php's FNM_PERIOD: a leading dot is matched only by a pattern that
		 * spells one, which is what keeps `.`, `..` and every hidden name out
		 * of an ordinary `*`. */
		if( zEnt[0] == '.' && (nSeg < 1 || zSegBuf[0] != '.') ){
			continue;
		}
		if( !Glob((const unsigned char *)zSegBuf,(const unsigned char *)zEnt,'\\',0,FALSE) ){
			continue;
		}
		if( bOnlyDir ){
			/* GLOB_ONLYDIR, which only the trailing-slash branch below asks
			 * for -- the device itself always globs with no flags at all. */
			char zProbe[PH7_GLOB_PATH_MAX * 2];
			SyMemcpy(zDir,zProbe,(sxu32)nDir);
			zProbe[nDir] = '/';
			SyMemcpy(zEnt,&zProbe[nDir+1],(sxu32)nName);
			zProbe[nDir + 1 + nName] = 0;
			if( pVfs == 0 || pVfs->xIsdir == 0 || pVfs->xIsdir(zProbe) != PH7_OK ){
				continue;
			}
		}
		rc = GlobAdd(pHit,pSet,zPat,(sxu32)nPrefix,zEnt,(sxu32)nName);
		if( rc != SXRET_OK ){
			break;
		}
	}
	VmReleaseCallContext(&sCtx);
	PH7_MemObjRelease(&sEntry);
	if( pStream->xCloseDir ){
		pStream->xCloseDir(pHandle);
	}
	return rc;
}
/*
 * One pattern, every segment of it. php's three branches, in php's order: a
 * pattern that ENDS in a slash names directories and KEEPS the slash; a
 * wildcard in the directory part is walked level by level; anything else is
 * one directory read. Each branch sorts the range it produced.
 */
static sxi32 GlobExpand(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,
	SyBlob *pHit,SySet *pSet)
{
	sxu32 nStart = SySetUsed(pSet);
	SyBlob sSub;
	SySet aSub;
	glob_hit *aRec;
	sxu32 n,nRec;
	int i,iSlash = -1;
	sxi32 rc;
	if( nPat < 1 || nPat >= PH7_GLOB_PATH_MAX ){
		return SXRET_OK;
	}
	if( zPat[nPat-1] == '/' ){
		/* `d/` is ['d/'] and `d/` + `*` + `/` is ['d/a/','d/b/']: answer the base as
		 * DIRECTORIES and put ONE slash back, so a pattern ending in two keeps
		 * both. php sorts the names it ANSWERS, slash included. */
		const ph7_vfs *pVfs = pVm->pEngine->pVfs;
		if( nPat == 1 ){
			if( pVfs && pVfs->xIsdir && pVfs->xIsdir("/") == PH7_OK ){
				return GlobAdd(pHit,pSet,"/",1,0,0);
			}
			return SXRET_OK;
		}
		SyBlobInit(&sSub,&pVm->sAllocator);
		SySetInit(&aSub,&pVm->sAllocator,sizeof(glob_hit));
		rc = GlobExpand(pVm,zPat,nPat-1,TRUE,&sSub,&aSub);
		if( rc == SXRET_OK ){
			aRec = (glob_hit *)SySetBasePtr(&aSub);
			nRec = SySetUsed(&aSub);
			for( n = 0 ; n < nRec ; ++n ){
				rc = GlobAdd(pHit,pSet,
					&((const char *)SyBlobData(&sSub))[aRec[n].nOfs],aRec[n].nLen,"/",1);
				if( rc != SXRET_OK ){
					break;
				}
			}
		}
		SyBlobRelease(&sSub);
		SySetRelease(&aSub);
		if( rc == SXRET_OK ){
			GlobSort(pHit,pSet,nStart);
		}
		return rc;
	}
	for( i = nPat - 1 ; i >= 0 ; --i ){
		if( zPat[i] == '/' ){
			iSlash = i;
			break;
		}
	}
	if( iSlash > 0 && GlobHasMeta(zPat,iSlash) ){
		/* A wildcard in the DIRECTORY part is matched level by level, which is
		 * what glob(3) does: list the directories that part names, then glob
		 * the last component inside each. Reading only the last component
		 * answers [] for `src/` + `*` + `/` + `*.php`, the everyday two-level
		 * spelling, and for every deeper one. */
		SyBlobInit(&sSub,&pVm->sAllocator);
		SySetInit(&aSub,&pVm->sAllocator,sizeof(glob_hit));
		rc = GlobExpand(pVm,zPat,iSlash+1,FALSE,&sSub,&aSub);
		if( rc == SXRET_OK ){
			SyBlob sJoin;
			SyBlobInit(&sJoin,&pVm->sAllocator);
			aRec = (glob_hit *)SySetBasePtr(&aSub);
			nRec = SySetUsed(&aSub);
			for( n = 0 ; n < nRec ; ++n ){
				SyBlobReset(&sJoin);
				if( SyBlobAppend(&sJoin,
					&((const char *)SyBlobData(&sSub))[aRec[n].nOfs],aRec[n].nLen) != SXRET_OK
				 || SyBlobAppend(&sJoin,&zPat[iSlash+1],(sxu32)(nPat - iSlash - 1)) != SXRET_OK ){
					rc = SXERR_MEM;
					break;
				}
				rc = GlobExpand(pVm,(const char *)SyBlobData(&sJoin),
					(int)SyBlobLength(&sJoin),bOnlyDir,pHit,pSet);
				if( rc != SXRET_OK ){
					break;
				}
			}
			SyBlobRelease(&sJoin);
		}
		SyBlobRelease(&sSub);
		SySetRelease(&aSub);
		if( rc == SXRET_OK ){
			GlobSort(pHit,pSet,nStart);
		}
		return rc;
	}
	rc = GlobLeaf(pVm,zPat,nPat,bOnlyDir,pHit,pSet);
	if( rc == SXRET_OK ){
		GlobSort(pHit,pSet,nStart);
	}
	return rc;
}
/* void (*xCloseDir)(void *) */
static void GlobStream_CloseDir(void *pHandle)
{
	glob_stream *pGlob = (glob_stream *)pHandle;
	ph7_vm *pVm;
	if( pGlob == 0 ){
		return;
	}
	pVm = pGlob->pVm;
	SyBlobRelease(&pGlob->sHit);
	SyBlobRelease(&pGlob->sDir);
	SySetRelease(&pGlob->aHit);
	SyMemBackendFree(&pVm->sAllocator,pGlob);
}
/*
 * int (*xOpenDir)(const char *,ph7_value *,void **)
 *
 * php's opener fails only on a glob(3) ERROR: no matches at all is an OPEN
 * stream with nothing in it, which is why `new GlobIterator('nope/' . '*')` is a
 * working object whose count() is 0 rather than a constructor that throws.
 *
 * The VM comes in through the context argument, the way data:// takes it: the
 * walk allocates, and reads a directory through a call context of its own.
 */
static int GlobStream_OpenDir(const char *zPattern,ph7_value *pResource,void **ppHandle)
{
	ph7_vm *pVm = pResource ? pResource->pVm : 0;
	glob_stream *pGlob;
	if( pVm == 0 ){
		return -1;
	}
	pGlob = (glob_stream *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(glob_stream));
	if( pGlob == 0 ){
		return -1;
	}
	pGlob->pVm = pVm;
	pGlob->nCur = 0;
	SyBlobInit(&pGlob->sHit,&pVm->sAllocator);
	SyBlobInit(&pGlob->sDir,&pVm->sAllocator);
	SySetInit(&pGlob->aHit,&pVm->sAllocator,sizeof(glob_hit));
	/* A pattern longer than one path is zero matches rather than a refusal (it
	 * can name nothing that exists), which is glob(3)'s GLOB_NOMATCH and an
	 * open stream either way. */
	if( GlobExpand(pVm,zPattern,(int)SyStrlen(zPattern),FALSE,
		&pGlob->sHit,&pGlob->aHit) != SXRET_OK ){
		GlobStream_CloseDir(pGlob);
		return -1;
	}
	*ppHandle = (void *)pGlob;
	return PH7_OK;
}
/*
 * int (*xReadDir)(void *,ph7_context *)
 *
 * php's php_glob_stream_path_split, which runs on every read: the directory is
 * everything before the LAST slash and the ENTRY is what follows it. So a
 * match with no slash in it reports an empty directory and itself, `a/`
 * reports `a` and an EMPTY entry -- and an empty entry is what the SPL walk
 * reads as the end, which is why GlobIterator over a trailing-slash pattern
 * counts its matches and yields none of them.
 */
static int GlobStream_ReadDir(void *pHandle,ph7_context *pCtx)
{
	glob_stream *pGlob = (glob_stream *)pHandle;
	glob_hit *aHit;
	const char *zPath;
	sxu32 nPath;
	int i,iSlash = -1;
	if( pGlob == 0 ){
		return -1;
	}
	if( pGlob->nCur >= SySetUsed(&pGlob->aHit) ){
		/* php drops the path when the walk runs out, and counts on past it. */
		pGlob->nCur++;
		SyBlobReset(&pGlob->sDir);
		return -1;
	}
	aHit = (glob_hit *)SySetBasePtr(&pGlob->aHit);
	zPath = &((const char *)SyBlobData(&pGlob->sHit))[aHit[pGlob->nCur].nOfs];
	nPath = aHit[pGlob->nCur].nLen;
	pGlob->nCur++;
	for( i = (int)nPath - 1 ; i >= 0 ; --i ){
		if( zPath[i] == '/' ){
			iSlash = i;
			break;
		}
	}
	SyBlobReset(&pGlob->sDir);
	if( iSlash >= 0 ){
		if( iSlash > 0 && SyBlobAppend(&pGlob->sDir,zPath,(sxu32)iSlash) != SXRET_OK ){
			return -1;
		}
		ph7_result_string(pCtx,&zPath[iSlash+1],(int)nPath - iSlash - 1);
	}else{
		ph7_result_string(pCtx,zPath,(int)nPath);
	}
	return PH7_OK;
}
/* void (*xRewindDir)(void *): php's rewind moves the INDEX and leaves the path
 * where the last read put it -- every caller reads straight afterwards. */
static void GlobStream_RewindDir(void *pHandle)
{
	glob_stream *pGlob = (glob_stream *)pHandle;
	if( pGlob ){
		pGlob->nCur = 0;
	}
}
PH7_PRIVATE const ph7_io_stream sGLOB_Stream = {
	"glob",
	PH7_IO_STREAM_VERSION,
	0,                    /* xOpen: php's wrapper has no stream opener at all */
	GlobStream_OpenDir,   /* xOpenDir */
	0,                    /* xClose */
	GlobStream_CloseDir,  /* xCloseDir */
	0,                    /* xRead */
	GlobStream_ReadDir,   /* xReadDir */
	0,                    /* xWrite */
	0,                    /* xSeek */
	0,                    /* xLock */
	GlobStream_RewindDir, /* xRewindDir */
	0,                    /* xTell */
	0,                    /* xTrunc */
	0,                    /* xSync */
	0                     /* xStat */
};
/* Is this the glob device? php's php_stream_is(), which is how SPL tells a
 * GlobIterator's directory handle from an ordinary one. */
PH7_PRIVATE int PH7_GlobStreamIs(const ph7_io_stream *pStream)
{
	return pStream == &sGLOB_Stream;
}
/* php's php_glob_stream_get_path: the directory of the CURRENT match, empty
 * both before the first read and after the last. */
PH7_PRIVATE const char * PH7_GlobStreamPath(void *pHandle,int *pnLen)
{
	glob_stream *pGlob = (glob_stream *)pHandle;
	if( pGlob == 0 ){
		*pnLen = 0;
		return "";
	}
	*pnLen = (int)SyBlobLength(&pGlob->sDir);
	return *pnLen > 0 ? (const char *)SyBlobData(&pGlob->sDir) : "";
}
/* php's php_glob_stream_get_count: what GlobIterator::count() answers, and it
 * does not move with the walk. */
PH7_PRIVATE sxi64 PH7_GlobStreamCount(void *pHandle)
{
	glob_stream *pGlob = (glob_stream *)pHandle;
	return pGlob ? (sxi64)SySetUsed(&pGlob->aHit) : 0;
}
#endif /* PH7_DISABLE_DISK_IO */
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * bool link(string $target,string $link)

 *  Create a hard link.
 * Parameters
 *  $target
 *   Target of the link.
 *  $link
 *   The link name.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zTarget,*zLink;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xLink == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the given arguments */
	zTarget  = ph7_value_to_string(apArg[0],0);
	zLink = ph7_value_to_string(apArg[1],0);
	/* Perform the requested operation */
	errno = 0;
	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);
	if( rc != PH7_OK ){
		/* php's no-path shape again, the same chmod() takes. PHL answered FALSE
		 * in silence, so a failed link was indistinguishable from a made one
		 * without testing the return value. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",
			ph7_function_name(pCtx),VfsStrerror(errno));
	}
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK );
	return PH7_OK;
}
/*
 * string|false readlink(string $path)
 *  Returns the target of a symbolic link.
 * Parameters
 *  $path
 *   The symbolic link path.
 * Return
 *  The contents of the link, or FALSE (with a warning) when $path is not a link
 *  or cannot be read -- php's own answer, error text included.
 */
static int PH7_vfs_readlink(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xReadlink == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],0);
	rc = pVfs->xReadlink(zPath,pCtx);
	if( rc != PH7_OK ){
		/* php's wording is the errno text alone -- the engine prefixes the
		 * function name already. */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"%s",VfsStrerror(errno));
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * bool symlink(string $target,string $link)
 *  Creates a symbolic link.
 * Parameters
 *  $target
 *   Target of the link.
 *  $link
 *   The link name.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zTarget,*zLink;
	ph7_vfs *pVfs;
	int rc;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xLink == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the given arguments */
	zTarget  = ph7_value_to_string(apArg[0],0);
	zLink = ph7_value_to_string(apArg[1],0);
	/* Perform the requested operation */
	errno = 0;
	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);
	if( rc != PH7_OK ){
		/* php's no-path shape again, the same chmod() takes. PHL answered FALSE
		 * in silence, so a failed link was indistinguishable from a made one
		 * without testing the return value. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",
			ph7_function_name(pCtx),VfsStrerror(errno));
	}
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK );
	return PH7_OK;
}
/*
 * int umask([ int $mask ])
 *  Changes the current umask.
 * Parameters
 *  $mask
 *   The new umask.
 * Return
 *  umask() without arguments simply returns the current umask.
 *  Otherwise the old umask is returned.
 */
static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iOld,iNew;
	ph7_vfs *pVfs;
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xUmask == 0 ){
		/* IO routine not implemented,return -1 */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS",
			ph7_function_name(pCtx)
			);
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	iNew = 0;
	if( nArg > 0 ){
		iNew = ph7_value_to_int(apArg[0]);
	}
	/* Perform the requested operation */
	iOld = pVfs->xUmask(iNew);
	/* Old mask */
	ph7_result_int(pCtx,iOld);
	return PH7_OK;
}
/*
 * string sys_get_temp_dir()
 *  Returns directory path used for temporary files.
 * Parameters
 *  None
 * Return
 *  Returns the path of the temporary directory.
 */
static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vfs *pVfs;
	/* Set the empty string as the default return value */
	ph7_result_string(pCtx,"",0);
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xTempDir == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* IO routine not implemented,return "" */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS",
			ph7_function_name(pCtx)
			);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pVfs->xTempDir(pCtx);
	return PH7_OK;
}
/*
 * string get_current_user()
 *  Returns the name of the current working user.
 * Parameters
 *  None
 * Return
 *  Returns the name of the current working user.
 */
static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vfs *pVfs;
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xUsername == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* IO routine not implemented */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS",
			ph7_function_name(pCtx)
			);
		/* Set a dummy username */
		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pVfs->xUsername(pCtx);
	return PH7_OK;
}
/*
 * int64 getmypid()
 *  Gets process ID.
 * Parameters
 *  None
 * Return
 *  Returns the process ID.
 */
static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 nProcessId;
	ph7_vfs *pVfs;
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xProcessId == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* IO routine not implemented,return -1 */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS",
			ph7_function_name(pCtx)
			);
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Perform the requested operation */
	nProcessId = (ph7_int64)pVfs->xProcessId();
	/* Set the result */
	ph7_result_int64(pCtx,nProcessId);
	return PH7_OK;
}
/*
 * int getmyuid()
 *  Get user ID.
 * Parameters
 *  None
 * Return
 *  Returns the user ID.
 */
static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vfs *pVfs;
	int nUid;
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xUid == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* IO routine not implemented,return -1 */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS",
			ph7_function_name(pCtx)
			);
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Perform the requested operation */
	nUid = pVfs->xUid();
	/* Set the result */
	ph7_result_int(pCtx,nUid);
	return PH7_OK;
}
/*
 * int getmygid()
 *  Get group ID.
 * Parameters
 *  None
 * Return
 *  Returns the group ID.
 */
static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vfs *pVfs;
	int nGid;
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xGid == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* IO routine not implemented,return -1 */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS",
			ph7_function_name(pCtx)
			);
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Perform the requested operation */
	nGid = pVfs->xGid();
	/* Set the result */
	ph7_result_int(pCtx,nGid);
	return PH7_OK;
}
#ifdef __WINNT__
#include <Windows.h>
#elif defined(__UNIXES__)
#include <sys/utsname.h>
#endif
/*
 * php's five uname FIELDS, filled once per call. php answers one of them for a
 * single-letter mode and all five, space-separated, for `a` -- and the ORDER of
 * that composite is `s n r v m`, the OS, the HOST, the release, the version and
 * the machine. This engine used to answer `s r v n m`, so the host name stood
 * in the version's place in every string a program logged.
 */
typedef struct vfs_uname_fields vfs_uname_fields;
struct vfs_uname_fields {
	const char *zSys;     /* 's' */
	const char *zNode;    /* 'n' */
	const char *zRel;     /* 'r' */
	const char *zVer;     /* 'v' */
	const char *zMachine; /* 'm' */
};
#if defined(__WINNT__)
/*
 * The product NAME php prints inside the version field. php reads the real
 * version through ntdll's RtlGetVersion (GetVersionEx() lies to any binary
 * without a compatibility manifest -- it answers 6.2 on Windows 10 and 11) and
 * names it from a table of its own. The rows below are the versions this port
 * targets and the only ones an oracle can be asked about; anything older
 * answers the bare "Windows" rather than a name nobody can verify.
 */
static const char * VfsWinProductName(unsigned long nMajor,unsigned long nMinor,
	unsigned long nBuild,int bWorkstation)
{
	if( nMajor == 10 && nMinor == 0 ){
		if( bWorkstation ){
			return nBuild >= 22000 ? "Windows 11" : "Windows 10";
		}
		if( nBuild >= 26100 ){
			return "Windows Server 2025";
		}
		if( nBuild >= 20348 ){
			return "Windows Server 2022";
		}
		if( nBuild >= 17763 ){
			return "Windows Server 2019";
		}
		return "Windows Server 2016";
	}
	return "Windows";
}
/*
 * The true OS version. RtlGetVersion is the only call that answers it for an
 * unmanifested binary, and it lives in ntdll rather than in an import library.
 */
static void VfsWinVersion(unsigned long *pnMajor,unsigned long *pnMinor,
	unsigned long *pnBuild,int *pbWorkstation)
{
	/* RTL_OSVERSIONINFOEXW's documented layout, declared here rather than
	 * taken from a header: the name only appears in some SDK versions, and
	 * nothing else in this file needs ntdll. */
	typedef struct vfs_rtl_osversion {
		ULONG dwOSVersionInfoSize;
		ULONG dwMajorVersion;
		ULONG dwMinorVersion;
		ULONG dwBuildNumber;
		ULONG dwPlatformId;
		WCHAR szCSDVersion[128];
		USHORT wServicePackMajor;
		USHORT wServicePackMinor;
		USHORT wSuiteMask;
		UCHAR wProductType;
		UCHAR wReserved;
	} vfs_rtl_osversion;
	typedef LONG (WINAPI *rtl_get_version)(vfs_rtl_osversion *);
	vfs_rtl_osversion sInfo;
	rtl_get_version xGet;
	HMODULE hNtdll;
	*pnMajor = 0;
	*pnMinor = 0;
	*pnBuild = 0;
	*pbWorkstation = 1;
	SyZero(&sInfo,(sxu32)sizeof(sInfo));
	sInfo.dwOSVersionInfoSize = (ULONG)sizeof(sInfo);
	hNtdll = GetModuleHandleA("ntdll.dll");
	if( hNtdll == 0 ){
		return;
	}
	xGet = (rtl_get_version)GetProcAddress(hNtdll,"RtlGetVersion");
	if( xGet == 0 || xGet(&sInfo) != 0 ){
		return;
	}
	*pnMajor = (unsigned long)sInfo.dwMajorVersion;
	*pnMinor = (unsigned long)sInfo.dwMinorVersion;
	*pnBuild = (unsigned long)sInfo.dwBuildNumber;
	/* VER_NT_WORKSTATION is 1; spelled out so the struct above needs no
	 * header of its own. */
	*pbWorkstation = (sInfo.wProductType == 1);
}
/* php's machine field: the NATIVE architecture, named the way php names it. */
static const char * VfsWinMachine(char *zBuf,int nBuf)
{
	SYSTEM_INFO sInfo;
	SyZero(&sInfo,(sxu32)sizeof(sInfo));
	GetNativeSystemInfo(&sInfo);
	switch( sInfo.wProcessorArchitecture ){
		case PROCESSOR_ARCHITECTURE_AMD64: return "AMD64";
		case PROCESSOR_ARCHITECTURE_ARM:   return "ARM";
#ifdef PROCESSOR_ARCHITECTURE_ARM64
		case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";
#endif
		case PROCESSOR_ARCHITECTURE_IA64:  return "IA64";
		case PROCESSOR_ARCHITECTURE_INTEL:
			SyBufferFormat(zBuf,(sxu32)nBuf,"i%u",(unsigned int)sInfo.dwProcessorType);
			return zBuf;
		default: break;
	}
	return "Unknown";
}
#endif /* __WINNT__ */
/*
 * string php_uname([ string $mode = "a" ])
 *  Returns information about the host operating system.
 * Parameters
 *  $mode
 *   ONE character out of `a m n r s v`; php refuses every other spelling with
 *   a ValueError, and refuses a longer or empty string with a different one.
 *    'a': the default -- all five fields in the sequence "s n r v m".
 *    's': operating system name.
 *    'n': host name.
 *    'r': release name.
 *    'v': version information.
 *    'm': machine type.
 * Return
 *  The requested field, or all five.
 */
static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	vfs_uname_fields sF;
	const char *zMode;
	int nMode = 1,c;
#if defined(__WINNT__)
	char zHost[256],zRel[32],zVer[128],zMach[32];
	unsigned long nMajor,nMinor,nBuild;
	int bWorkstation;
#elif defined(__UNIXES__)
	struct utsname sName;
#endif
	zMode = "a";
	if( nArg > 0 ){
		zMode = ph7_value_to_string(apArg[0],&nMode);
		if( nMode != 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"php_uname(): Argument #1 ($mode) must be a single character");
		}
	}
	c = zMode[0];
	if( c != 'a' && c != 'm' && c != 'n' && c != 'r' && c != 's' && c != 'v' ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"php_uname(): Argument #1 ($mode) must be one of \"a\", \"m\", \"n\", \"r\", \"s\", or \"v\"");
	}
#if defined(__WINNT__)
	VfsWinVersion(&nMajor,&nMinor,&nBuild,&bWorkstation);
	zHost[0] = 0;
	{
		DWORD nName = (DWORD)sizeof(zHost);
		if( !GetComputerNameA(zHost,&nName) ){
			zHost[0] = 0;
		}
	}
	SyBufferFormat(zRel,(sxu32)sizeof(zRel),"%u.%u",
		(unsigned int)nMajor,(unsigned int)nMinor);
	SyBufferFormat(zVer,(sxu32)sizeof(zVer),"build %u (%s)",
		(unsigned int)nBuild,VfsWinProductName(nMajor,nMinor,nBuild,bWorkstation));
	/* php's own answer on Windows is the KERNEL's name and never the product's,
	 * which is what makes `php_uname('s')` the same three words on every
	 * Windows there is. */
	sF.zSys = "Windows NT";
	sF.zNode = zHost;
	sF.zRel = zRel;
	sF.zVer = zVer;
	sF.zMachine = VfsWinMachine(zMach,(int)sizeof(zMach));
#elif defined(__UNIXES__)
	if( uname(&sName) != 0 ){
		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);
		return PH7_OK;
	}
	sF.zSys = sName.sysname;
	sF.zNode = sName.nodename;
	sF.zRel = sName.release;
	sF.zVer = sName.version;
	sF.zMachine = sName.machine;
#else
	sF.zSys = "Unknown";
	sF.zNode = "";
	sF.zRel = "";
	sF.zVer = "";
	sF.zMachine = "";
#endif
	switch( c ){
		case 's': ph7_result_string(pCtx,sF.zSys,-1); break;
		case 'n': ph7_result_string(pCtx,sF.zNode,-1); break;
		case 'r': ph7_result_string(pCtx,sF.zRel,-1); break;
		case 'v': ph7_result_string(pCtx,sF.zVer,-1); break;
		case 'm': ph7_result_string(pCtx,sF.zMachine,-1); break;
		default:
			ph7_result_string_format(pCtx,"%s %s %s %s %s",
				sF.zSys,sF.zNode,sF.zRel,sF.zVer,sF.zMachine);
			break;
	}
	return PH7_OK;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC || PH7_DISABLE_DISK_IO */
/* NULL VFS [i.e: a no-op VFS]*/
#if defined(_MSC_VER)
static const ph7_vfs null_vfs = {
#else
static const ph7_vfs null_vfs __attribute__((unused)) = {
#endif
	"null_vfs",
	PH7_VFS_VERSION,
	0, /* int (*xChdir)(const char *) */
	0, /* int (*xChroot)(const char *); */
	0, /* int (*xGetcwd)(ph7_context *) */
	0, /* int (*xMkdir)(const char *,int,int) */
	0, /* int (*xRmdir)(const char *) */
	0, /* int (*xIsdir)(const char *) */
	0, /* int (*xRename)(const char *,const char *) */
	0, /*int (*xRealpath)(const char *,ph7_context *)*/
	0, /* int (*xSleep)(unsigned int) */
	0, /* int (*xUnlink)(const char *) */
	0, /* int (*xFileExists)(const char *) */
	0, /*int (*xChmod)(const char *,int)*/
	0, /*int (*xChown)(const char *,const char *)*/
	0, /*int (*xChgrp)(const char *,const char *)*/
	0, /* ph7_int64 (*xFreeSpace)(const char *) */
	0, /* ph7_int64 (*xTotalSpace)(const char *) */
	0, /* ph7_int64 (*xFileSize)(const char *) */
	0, /* ph7_int64 (*xFileAtime)(const char *) */
	0, /* ph7_int64 (*xFileMtime)(const char *) */
	0, /* ph7_int64 (*xFileCtime)(const char *) */
	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */
	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */
	0, /* int (*xIsfile)(const char *) */
	0, /* int (*xIslink)(const char *) */
	0, /* int (*xReadable)(const char *) */
	0, /* int (*xWritable)(const char *) */
	0, /* int (*xExecutable)(const char *) */
	0, /* int (*xFiletype)(const char *,ph7_context *) */
	0, /* int (*xGetenv)(const char *,ph7_context *) */
	0, /* int (*xSetenv)(const char *,const char *) */
	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */
	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */
	0, /* void (*xUnmap)(void *,ph7_int64);  */
	0, /* int (*xLink)(const char *,const char *,int) */
	0, /* int (*xUmask)(int) */
	0, /* void (*xTempDir)(ph7_context *) */
	0, /* unsigned int (*xProcessId)(void) */
	0, /* int (*xUid)(void) */
	0, /* int (*xGid)(void) */
	0, /* void (*xUsername)(ph7_context *) */
	0, /* int (*xExec)(const char *,ph7_context *) */
	0, /* int (*xReadlink)(const char *,ph7_context *) */
	0  /* int (*xEnviron)(ph7_context *) */
};
/* Windows VFS implementation moved to vfs_win.c */
/* Unix VFS implementation moved to vfs_unix.c */
/*
 * Export the builtin vfs.
 * Return a pointer to the builtin vfs if available.
 * Otherwise return the null_vfs [i.e: a no-op vfs] instead.
 * Note:
 *  The built-in vfs is always available for Windows/UNIX systems.
 * Note:
 *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC
 *  directives defined then this function return the null_vfs instead.
 */
PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)
{
#if !defined(PH7_DISABLE_BUILTIN_FUNC) || !defined(PH7_DISABLE_DISK_IO)
#ifdef PH7_DISABLE_DISK_IO
	return &null_vfs;
#else
#ifdef __WINNT__
	return &sWinVfs;
#elif defined(__UNIXES__)
	return &sUnixVfs;
#else
	return &null_vfs;
#endif /* __WINNT__/__UNIXES__ */
#endif /*PH7_DISABLE_DISK_IO*/
#else
	return &null_vfs;
#endif /* PH7_DISABLE_BUILTIN_FUNC || PH7_DISABLE_DISK_IO */
}
/*
 * Export the IO routines defined above and the built-in IO streams
 * [i.e: file://,php://].
 * Note:
 *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive
 *  defined then this function is a no-op.
 */
PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)
{
	/*
	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.
	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.
	 */
#ifndef PH7_DISABLE_DISK_IO
	/* VFS: disk I/O related functions */
	static const ph7_builtin_func aVfsDiskFunc[] = {
		{"chdir",   PH7_vfs_chdir   },
#ifndef __WINNT__
		/* php declares chroot() on POSIX only — there is no such call on Windows,
		 * so `function_exists('chroot')` is FALSE there and the name is free for a
		 * script to define. PHL used to declare it on both and answer a
		 * "not implemented in the underlying VFS" warning + false on Windows,
		 * which is a different thing from php's undefined function. (chown/chgrp/
		 * link/symlink/readlink stay: php declares all five on Windows.) */
		{"chroot",  PH7_vfs_chroot  },
#endif
		{"getcwd",  PH7_vfs_getcwd  },
		{"rmdir",   PH7_vfs_rmdir   },
		{"is_dir",  PH7_vfs_is_dir  },
		{"mkdir",   PH7_vfs_mkdir   },
		{"rename",  PH7_vfs_rename  },
		{"realpath",PH7_vfs_realpath},
		/* php's own resolver, and the question include/require answer silently:
		 * it walks the same include_path in the same order, so it belongs beside
		 * realpath() rather than with the stream builtins. */
		{"stream_resolve_include_path",PH7_vfs_stream_resolve_include_path},
		{"sleep",   PH7_vfs_sleep   },
		{"usleep",  PH7_vfs_usleep  },
		{"unlink",  PH7_vfs_unlink  },
		{"delete",  PH7_vfs_unlink  },
		{"chmod",   PH7_vfs_chmod   },
		{"chown",   PH7_vfs_chown   },
		{"chgrp",   PH7_vfs_chgrp   },
		{"disk_free_space",PH7_vfs_disk_free_space  },
		{"diskfreespace",  PH7_vfs_disk_free_space  },
		{"disk_total_space",PH7_vfs_disk_total_space},
		{"file_exists", PH7_vfs_file_exists },
		{"filesize",    PH7_vfs_file_size   },
		{"fileatime",   PH7_vfs_file_atime  },
		{"filemtime",   PH7_vfs_file_mtime  },
		{"filectime",   PH7_vfs_file_ctime  },
		{"is_file",     PH7_vfs_is_file  },
		{"is_link",     PH7_vfs_is_link  },
		{"is_readable", PH7_vfs_is_readable   },
		{"is_writable", PH7_vfs_is_writable   },
		/* php's own alias, spelled the other way; the diagnostics every
		 * builtin raises name the INVOKED name, so the one routine serves
		 * both. */
		{"is_writeable",PH7_vfs_is_writable   },
		{"is_executable",PH7_vfs_is_executable},
		{"filetype",    PH7_vfs_filetype },
		{"stat",        PH7_vfs_stat     },
		{"lstat",       PH7_vfs_lstat    },
		{"fileowner",   PH7_vfs_file_owner},
		{"filegroup",   PH7_vfs_file_group},
		{"fileinode",   PH7_vfs_file_inode},
		{"fileperms",   PH7_vfs_file_perms},
		{"getenv",      PH7_vfs_getenv   },
		{"setenv",      PH7_vfs_putenv   },
		{"putenv",      PH7_vfs_putenv   },
		{"touch",       PH7_vfs_touch    },
		{"link",        PH7_vfs_link     },
		{"symlink",     PH7_vfs_symlink  },
		{"readlink",    PH7_vfs_readlink },
		{"umask",       PH7_vfs_umask    },
		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },
		{"get_current_user", PH7_vfs_get_current_user },
		{"getmypid",    PH7_vfs_getmypid },
		{"getpid",      PH7_vfs_getmypid },
		{"getmyuid",    PH7_vfs_getmyuid },
		{"getuid",      PH7_vfs_getmyuid },
		{"getmygid",    PH7_vfs_getmygid },
		{"getgid",      PH7_vfs_getmygid },
		{"ph7_uname",   PH7_vfs_ph7_uname},
		{"php_uname",   PH7_vfs_ph7_uname}
	};
	/* IO stream / file operation functions (disk-related)
	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.
	 */
	static const ph7_builtin_func aIOFunc[] = {
		{"ftruncate", PH7_builtin_ftruncate },
		{"fseek",     PH7_builtin_fseek  },
		{"ftell",     PH7_builtin_ftell  },
		{"rewind",    PH7_builtin_rewind },
		{"fflush",    PH7_builtin_fflush },
		{"feof",      PH7_builtin_feof   },
		{"fgetc",     PH7_builtin_fgetc  },
		{"fgets",     PH7_builtin_fgets  },
		{"fscanf",    PH7_builtin_fscanf },
		{"stream_get_line", PH7_builtin_stream_get_line },
		{"fread",     PH7_builtin_fread  },
		{"fgetcsv",   PH7_builtin_fgetcsv},
		{"fgetss",    PH7_builtin_fgetss },
		{"readdir",   PH7_builtin_readdir},
		{"rewinddir", PH7_builtin_rewinddir },
		{"closedir",  PH7_builtin_closedir},
		{"opendir",   PH7_builtin_opendir },
		/* php's dir() lives with opendir(), which is what it calls and what its
		 * failure warning is worded by. Registering it here also means the TINY
		 * build drops BOTH: the prelude copy was defined there and fataled on
		 * "Call to undefined function opendir()" the moment it was called. */
		{"dir",       PH7_builtin_dir },
		{"readfile",  PH7_builtin_readfile},
		{"file_get_contents", PH7_builtin_file_get_contents},
		{"file_put_contents", PH7_builtin_file_put_contents},
		{"file",      PH7_builtin_file   },
		{"copy",      PH7_builtin_copy   },
		{"fstat",     PH7_builtin_fstat  },
		{"stream_isatty", PH7_builtin_stream_isatty },
		{"fwrite",    PH7_builtin_fwrite },
		{"fputs",     PH7_builtin_fwrite },
		{"flock",     PH7_builtin_flock  },
		{"fclose",    PH7_builtin_fclose },
		{"fopen",     PH7_builtin_fopen  },
		{"stream_get_contents",  PH7_builtin_stream_get_contents },
		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },
		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },
		/* php's own alias, kept from the days sockets had a separate API. */
		{"socket_get_status",    PH7_builtin_stream_get_meta_data },
		{"stream_set_blocking",  PH7_builtin_stream_set_blocking },
		{"socket_set_blocking",  PH7_builtin_stream_set_blocking },
		{"stream_set_timeout",   PH7_builtin_stream_set_timeout },
		{"stream_set_chunk_size",PH7_builtin_stream_set_chunk_size },
		{"stream_set_read_buffer",  PH7_builtin_stream_set_read_buffer },
		{"stream_set_write_buffer", PH7_builtin_stream_set_write_buffer },
		{"set_file_buffer",         PH7_builtin_stream_set_write_buffer },
		{"stream_supports_lock", PH7_builtin_stream_supports_lock },
		{"stream_is_local",      PH7_builtin_stream_is_local },
		{"stream_copy_to_stream",PH7_builtin_stream_copy_to_stream },
		{"stream_get_transports",PH7_builtin_stream_get_transports },
		/* Not under PH7_ENABLE_NET: a script selects over FILES and pipes in a
		 * build with no networking at all. */
		{"stream_select",        PH7_builtin_stream_select },
		{"stream_context_create",PH7_builtin_stream_context_create },
		{"stream_context_get_options",PH7_builtin_stream_context_get_options },
		{"stream_context_set_option", PH7_builtin_stream_context_set_option },
		{"stream_context_set_options",PH7_builtin_stream_context_set_options },
		{"stream_context_get_params", PH7_builtin_stream_context_get_params },
		{"stream_context_set_params", PH7_builtin_stream_context_set_params },
		{"stream_context_get_default",PH7_builtin_stream_context_get_default },
		{"stream_context_set_default",PH7_builtin_stream_context_set_default },
		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },
		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },
		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },
		{"stream_wrapper_restore",    PH7_builtin_stream_wrapper_restore },
		{"stream_filter_append",  PH7_builtin_stream_filter_append },
		{"stream_filter_prepend", PH7_builtin_stream_filter_prepend },
		{"stream_filter_remove",  PH7_builtin_stream_filter_remove },
		{"stream_get_filters",    PH7_builtin_stream_get_filters },
		{"stream_filter_register",PH7_builtin_stream_filter_register },
		{"stream_bucket_make_writeable", PH7_builtin_stream_bucket_make_writeable },
		{"stream_bucket_append",  PH7_builtin_stream_bucket_append },
		{"stream_bucket_prepend", PH7_builtin_stream_bucket_prepend },
		{"stream_bucket_new",     PH7_builtin_stream_bucket_new },
#ifdef PH7_ENABLE_NET
		{"fsockopen",  PH7_builtin_fsockopen },
		{"pfsockopen", PH7_builtin_fsockopen },
		{"stream_socket_client", PH7_builtin_fsockopen },
		{"stream_socket_server", PH7_builtin_stream_socket_server },
		{"stream_socket_accept", PH7_builtin_stream_socket_accept },
		{"stream_socket_get_name", PH7_builtin_stream_socket_get_name },
		{"stream_socket_pair",   PH7_builtin_stream_socket_pair },
		{"stream_socket_shutdown", PH7_builtin_stream_socket_shutdown },
		{"stream_socket_recvfrom", PH7_builtin_stream_socket_recvfrom },
		{"stream_socket_sendto",   PH7_builtin_stream_socket_sendto },
		/* The address converters and the host name: php's ext/standard
		 * network trio, which sits with the socket family here because the
		 * last of the three is an OS call the others share a build flag with. */
		{"inet_pton",  PH7_builtin_inet_pton },
		{"inet_ntop",  PH7_builtin_inet_ntop },
		{"gethostname",PH7_builtin_gethostname },
#endif
		{"popen",     PH7_builtin_popen  },
		{"proc_open",      PH7_builtin_proc_open      },
		{"proc_close",     PH7_builtin_proc_close     },
		{"proc_terminate", PH7_builtin_proc_terminate },
		{"proc_get_status",PH7_builtin_proc_get_status},
		{"proc_nice",      PH7_builtin_proc_nice      },
		{"shell_exec", PH7_builtin_shell_exec },
		{"exec",       PH7_builtin_exec     },
		{"system",     PH7_builtin_system   },
		{"passthru",   PH7_builtin_passthru },
		/* The shell-escaping pair lives with the command runners it exists to
		 * feed: a build without process execution has nothing to escape for. */
		{"escapeshellarg", PH7_builtin_escapeshellarg },
		{"escapeshellcmd", PH7_builtin_escapeshellcmd },
		{"pclose",    PH7_builtin_pclose },
		{"fpassthru", PH7_builtin_fpassthru },
		{"fputcsv",   PH7_builtin_fputcsv },
		{"fprintf",   PH7_builtin_fprintf },
#if !defined(PH7_DISABLE_HASH_FUNC)
		{"md5_file",  PH7_builtin_md5_file},
		{"sha1_file", PH7_builtin_sha1_file},
		/* The hash extension's file readers live with the disk table for the
		 * same reason md5_file does: without disk IO there is nothing to read. */
		{"hash_file",          PH7_builtin_hash_file },
		{"hash_hmac_file",     PH7_builtin_hash_hmac_file },
		{"hash_update_file",   PH7_builtin_hash_update_file },
		{"hash_update_stream", PH7_builtin_hash_update_stream },
#endif /* PH7_DISABLE_HASH_FUNC */
		{"parse_ini_file", PH7_builtin_parse_ini_file},
		{"vfprintf",  PH7_builtin_vfprintf}
	};
	const ph7_io_stream *pFileStream = 0;
	sxu32 n = 0;
	/* Register disk-related functions */
	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){
		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){
		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);
	}
#else
	SXUNUSED(pVm);
#endif /* PH7_DISABLE_DISK_IO */

	/*
	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC
	 * is not set (preserve previous behavior for those helpers).
	 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
	static const ph7_builtin_func aVfsHelperFunc[] = {
		/* Path processing */
		{"dirname",     PH7_builtin_dirname  },
		{"basename",    PH7_builtin_basename },
		{"pathinfo",    PH7_builtin_pathinfo },
		{"strglob",     PH7_builtin_strglob  },
		{"fnmatch",     PH7_builtin_fnmatch  }
	};
	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){
		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);
	}
	/* The three names a script asks an http:// exchange about. */
	PH7_HttpInstallFuncs(&(*pVm));
#endif /* PH7_DISABLE_BUILTIN_FUNC */

	/* Install streams if disk I/O is enabled */
#ifndef PH7_DISABLE_DISK_IO
#ifdef __WINNT__
	pFileStream = &sWinFileStream;
#elif defined(__UNIXES__)
	pFileStream = &sUnixFileStream;
#endif
	/* Install the php:// stream */
	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);
	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);
#ifndef PH7_DISABLE_BUILTIN_FUNC
	/* glob:// lives beside the pattern matcher it drives, so it is only in the
	 * build when that is. */
	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sGLOB_Stream);
#endif
#ifdef PH7_ENABLE_NET
	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);
	/* php's one built-in protocol wrapper. It speaks over the same sockets
	 * tcp:// hands out, so it is in the build exactly when they are. */
	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sHTTP_Stream);
#endif
	if( pFileStream ){
		/* Install the file:// stream */
		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);
	}
#endif /* PH7_DISABLE_DISK_IO */

	return SXRET_OK;
}
