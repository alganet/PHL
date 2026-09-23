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
PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)
{
	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open stream: %s",
		ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));
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
	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s failed for %s",
		ph7_function_name(pCtx),bLstat ? "Lstat" : "stat",zPath ? zPath : "");
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
 */
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
	rc = pVfs->xChroot(zPath);
	/* IO return value */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
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
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
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
static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iRecursive = 0;
	const char *zPath;
	ph7_vfs *pVfs;
	int iMode,rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
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
	errno = 0;
	rc = pVfs->xMkdir(zPath,iMode,iRecursive);
	if( rc != PH7_OK ){
		/* php does NOT name the path for mkdir: "mkdir(): File exists" */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",
			ph7_function_name(pCtx),VfsStrerror(errno));
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
	int rc;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
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
	/* Perform the requested operation */
	zPath = ph7_value_to_string(apArg[0],0);
	rc = pVfs->xRealpath(zPath,pCtx);
	if( rc != PH7_OK ){
	 ph7_result_bool(pCtx,0);
	}
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
	int rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
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
	rc = pVfs->xChmod(zPath,iMode);
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
		 * truthy and compares equal to nothing a caller would test for. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): stat failed for %s",
			ph7_function_name(pCtx),zPath);
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
 * string getenv(string $varname)
 *  Gets the value of an environment variable.
 * Parameters
 *  $varname
 *   The variable name.
 * Return
 *  Returns the value of the environment variable varname, or FALSE if the environment
 * variable varname does not exist.
 */
static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zEnv;
	ph7_vfs *pVfs;
	int iLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
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
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the setting variable */
	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);
	if( iLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Parse the setting */
	zEnd = &zSettings[iLen];
	zValue = 0;
	zName = zSettings;
	while( zSettings < zEnd ){
		if( zSettings[0] == '=' ){
			/* Null terminate the name */
			zSettings[0] = 0;
			zValue = &zSettings[1];
			break;
		}
		zSettings++;
	}
	/* Install the environment variable in the $_Env array */
	if( zValue == 0 || zName[0] == 0 || zValue >= zEnd || zName >= zValue ){
		/* Invalid settings,retun FALSE */
		ph7_result_bool(pCtx,0);
		if( zSettings  < zEnd ){
			zSettings[0] = '=';
		}
		return PH7_OK;
	}
	ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_ENV_ATTR,zName,zValue,(int)(zEnd-zValue));
	/* Point to the underlying vfs */
	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);
	if( pVfs == 0 || pVfs->xSetenv == 0 ){
		/* IO routine not implemented,return NULL */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",
			ph7_function_name(pCtx)
			);
		ph7_result_bool(pCtx,0);
		zSettings[0] = '=';
		return PH7_OK;
	}
	/* Perform the requested operation */
	rc = pVfs->xSetenv(zName,zValue);
	ph7_result_bool(pCtx,rc == PH7_OK );
	zSettings[0] = '=';
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
	rc = pVfs->xTouch(zFile,nTime,nAccess);
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
static int patternCompare(
  const u8 *zPattern,              /* The glob pattern */
  const u8 *zString,               /* The string to compare against the glob */
  const int esc,                    /* The escape character */
  int noCase
){
  int c, c2;
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
	  if( (esc==0) || (matchSet<0x80) ) return 0;
	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){
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
		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;
      }
      return 0;
    }else if( !prevEscape && c==matchOne ){
      if( PH7_Utf8Read(zString, 0, &zString)==0 ){
        return 0;
      }
    }else if( c==matchSet ){
      int prior_c = 0;
      if( esc == 0 ) return 0;
      seen = 0;
      invert = 0;
      c = PH7_Utf8Read(zString, 0, &zString);
      if( c==0 ) return 0;
      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);
      if( c2=='^' ){
        invert = 1;
        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);
      }
      if( c2==']' ){
        if( c==']' ) seen = 1;
        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);
      }
      while( c2 && c2!=']' ){
        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){
          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);
          if( c>=prior_c && c<=c2 ) seen = 1;
          prior_c = 0;
        }else{
          if( c==c2 ){
            seen = 1;
          }
          prior_c = c2;
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
static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)
{
	int rc;
	if( iEsc < 0 ){
		iEsc = '\\';
	}
	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);
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
	/* Go globbing */
	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);
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
	/* Go globbing */
	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);
	/* Globbing result */
	ph7_result_bool(pCtx,rc);
	return PH7_OK;
}
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
	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);
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
	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);
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
 * string php_uname([ string $mode = "a" ])
 *  Returns information about the host operating system.
 * Parameters
 *  $mode
 *   mode is a single character that defines what information is returned:
 *    'a': This is the default. Contains all modes in the sequence "s n r v m".
 *    's': Operating system name. eg. FreeBSD.
 *    'n': Host name. eg. localhost.example.com.
 *    'r': Release name. eg. 5.1.2-RELEASE.
 *    'v': Version information. Varies a lot between operating systems.
 *    'm': Machine type. eg. i386.
 * Return
 *  OS description as a string.
 */
static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
#if defined(__WINNT__)
	const char *zName = "Microsoft Windows";
	OSVERSIONINFOW sVer;
#elif defined(__UNIXES__)
	struct utsname sName;
#endif
	const char *zMode = "a";
	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){
		/* Extract the desired mode */
		zMode = ph7_value_to_string(apArg[0],0);
	}
#if defined(__WINNT__)
	sVer.dwOSVersionInfoSize = sizeof(sVer);
	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996)
#endif
	if( TRUE != GetVersionExW(&sVer)){
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
		ph7_result_string(pCtx,zName,-1);
		return PH7_OK;
	}
	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){
		if( sVer.dwMajorVersion <= 4 ){
			zName = "Microsoft Windows NT";
		}else if( sVer.dwMajorVersion == 5 ){
			switch(sVer.dwMinorVersion){
				case 0:	zName = "Microsoft Windows 2000"; break;
				case 1: zName = "Microsoft Windows XP";   break;
				case 2: zName = "Microsoft Windows Server 2003"; break;
			}
		}else if( sVer.dwMajorVersion == 6){
				switch(sVer.dwMinorVersion){
					case 0: zName = "Microsoft Windows Vista"; break;
					case 1: zName = "Microsoft Windows 7"; break;
					case 2: zName = "Microsoft Windows Server 2008"; break;
					case 3: zName = "Microsoft Windows 8"; break;
					default: break;
				}
		}
	}
	switch(zMode[0]){
	case 's':
		/* Operating system name */
		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);
		break;
	case 'n':
		/* Host name */
		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);
		break;
	case 'r':
	case 'v':
		/* Version information. */
		ph7_result_string_format(pCtx,"%u.%u build %u",
			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber
			);
		break;
	case 'm':
		/* Machine name */
		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);
		break;
	default:
		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",
			zName,
			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber
			);
		break;
	}
#elif defined(__UNIXES__)
	if( uname(&sName) != 0 ){
		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);
		return PH7_OK;
	}
	switch(zMode[0]){
	case 's':
		/* Operating system name */
		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);
		break;
	case 'n':
		/* Host name */
		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);
		break;
	case 'r':
		/* Release information */
		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);
		break;
	case 'v':
		/* Version information. */
		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);
		break;
	case 'm':
		/* Machine name */
		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);
		break;
	default:
		ph7_result_string_format(pCtx,
			"%s %s %s %s %s",
			sName.sysname,
			sName.release,
			sName.version,
			sName.nodename,
			sName.machine
			);
		break;
	}
#else
	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);
#endif
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
	0  /* int (*xReadlink)(const char *,ph7_context *) */
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
		{"chroot",  PH7_vfs_chroot  },
		{"getcwd",  PH7_vfs_getcwd  },
		{"rmdir",   PH7_vfs_rmdir   },
		{"is_dir",  PH7_vfs_is_dir  },
		{"mkdir",   PH7_vfs_mkdir   },
		{"rename",  PH7_vfs_rename  },
		{"realpath",PH7_vfs_realpath},
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
		{"stream_context_create",PH7_builtin_stream_context_create },
		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },
		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },
		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },
#ifdef PH7_ENABLE_NET
		{"fsockopen",  PH7_builtin_fsockopen },
		{"pfsockopen", PH7_builtin_fsockopen },
		{"stream_socket_client", PH7_builtin_fsockopen },
#endif
		{"popen",     PH7_builtin_popen  },
		{"proc_open",      PH7_builtin_proc_open      },
		{"proc_close",     PH7_builtin_proc_close     },
		{"proc_terminate", PH7_builtin_proc_terminate },
		{"proc_get_status",PH7_builtin_proc_get_status},
		{"shell_exec", PH7_builtin_shell_exec },
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
#ifdef PH7_ENABLE_NET
	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);
#endif
	if( pFileStream ){
		/* Install the file:// stream */
		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);
	}
#endif /* PH7_DISABLE_DISK_IO */

	return SXRET_OK;
}
