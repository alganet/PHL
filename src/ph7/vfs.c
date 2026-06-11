/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdio.h>
#ifdef __UNIXES__
#include <unistd.h>
#include <sys/wait.h>
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
	const char *zEnd = &zPath[nByte - 1];
	int c,d;
	c = d = '/';
#ifdef __WINNT__
	d = '\\';
#endif
	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){
		zEnd--;
	}
	*pLen = (int)(zEnd-zPath);
#ifdef __WINNT__
	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){
		/* Normalize path on windows */
		return "\\";
	}
#endif
	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){
		/* No separator,return "." as the current directory */
		*pLen = sizeof(char);
		return ".";
	}
	if( (*pLen) == 0 ){
		*pLen = sizeof(char);
#ifdef __WINNT__
		return "\\";
#else
		return "/";
#endif
	}
	return zPath;
}
/*
 * Compile the VFS implementations when builtins are enabled OR when disk I/O
 * is explicitly enabled (i.e. PH7_DISABLE_DISK_IO is NOT defined).
 */
#ifndef PH7_DISABLE_DISK_IO
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
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
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
	rc = pVfs->xChdir(zPath);
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
	rc = pVfs->xRmdir(zPath);
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
	rc = pVfs->xMkdir(zPath,iMode,iRecursive);
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
	rc = pVfs->xRename(zOld,zNew);
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
		/* Invalid value,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
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
		/* Invalid value,return immediately */
		return PH7_OK;
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
	rc = pVfs->xUnlink(zPath);
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
	rc = pVfs->xChown(zPath,zUser);
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
	rc = pVfs->xChgrp(zPath,zGroup);
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
	iSize = pVfs->xFreeSpace(zPath);
	/* IO return value */
	ph7_result_int64(pCtx,iSize);
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
	iSize = pVfs->xTotalSpace(zPath);
	/* IO return value */
	ph7_result_int64(pCtx,iSize);
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
	pVfs->xFiletype(zPath,pCtx);
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
		/* IO error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Return the associative array */
		ph7_result_value(pCtx,pArray);
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
		/* IO error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Return the associative array */
		ph7_result_value(pCtx,pArray);
	}
	/* Don't worry about freeing memory here,everything will be released
	 * automatically as soon we return from this function. */
	return PH7_OK;
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
	/* Perform the requested operation */
	nTime = nAccess = -1;
	zFile = ph7_value_to_string(apArg[0],0);
	if( nArg > 1 ){
		nTime = ph7_value_to_int64(apArg[1]);
		if( nArg > 2 ){
			nAccess = ph7_value_to_int64(apArg[1]);
		}else{
			nAccess = nTime;
		}
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
		/* Reuturn "." */
		ph7_result_string(pCtx,".",sizeof(char));
		return PH7_OK;
	}
	/* Perform the requested operation */
	zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);
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
	const char *zPath,*zBase,*zEnd;
	int c,d,iLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	c = d = '/';
#ifdef __WINNT__
	d = '\\';
#endif
	/* Point to the target path */
	zPath = ph7_value_to_string(apArg[0],&iLen);
	if( iLen < 1 ){
		/* Empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zPath[iLen - 1];
	/* Ignore trailing '/' */
	while( zEnd > zPath && ( (int)zEnd[0] == c || (int)zEnd[0] == d ) ){
		zEnd--;
	}
	iLen = (int)(&zEnd[1]-zPath);
	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){
		zEnd--;
	}
	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;
	zEnd = &zPath[iLen];
	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
		const char *zSuffix;
		int nSuffix;
		/* Strip suffix */
		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);
		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){
			zEnd -= nSuffix;
		}
	}
	/* Store the basename */
	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));
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
};
/*
 * Extract path fields.
 */
static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)
{
	const char *zPtr,*zEnd = &zPath[nByte - 1];
	SyString *pCur;
	int c,d;
	c = d = '/';
#ifdef __WINNT__
	d = '\\';
#endif
	/* Zero the structure */
	SyZero(pOut,sizeof(path_info));
	/* Handle special case */
	if( nByte == sizeof(char) && ( (int)zPath[0] == c || (int)zPath[0] == d ) ){
#ifdef __WINNT__
		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));
#else
		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));
#endif
		return SXRET_OK;
	}
	/* Extract the basename */
	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){
		zEnd--;
	}
	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;
	zEnd = &zPath[nByte];
	/* dirname */
	pCur = &pOut->sDir;
	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);
	if( pCur->nByte > 1 ){
		SyStringTrimTrailingChar(pCur,'/');
#ifdef __WINNT__
		SyStringTrimTrailingChar(pCur,'\\');
#endif
	}else if( (int)zPath[0] == c || (int)zPath[0] == d ){
#ifdef __WINNT__
		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));
#else
		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));
#endif
	}
	/* basename/filename */
	pCur = &pOut->sBasename;
	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);
	SyStringTrimLeadingChar(pCur,'/');
#ifdef __WINNT__
	SyStringTrimLeadingChar(pCur,'\\');
#endif
	SyStringDupPtr(&pOut->sFilename,pCur);
	if( pCur->nByte > 0 ){
		/* extension */
		zEnd--;
		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){
			zEnd--;
		}
		if( zEnd > pCur->zString ){
			zEnd++; /* Jump leading dot */
			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);
			/* Fix filename */
			pCur = &pOut->sFilename;
			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){
				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);
			}
		}
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
	SyString *pComp;
	int iLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Point to the target path */
	zPath = ph7_value_to_string(apArg[0],&iLen);
	if( iLen < 1 ){
		/* Empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract path info */
	ExtractPathInfo(zPath,iLen,&sInfo);
	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){
		/* Return path component */
		int nComp = ph7_value_to_int(apArg[1]);
		switch(nComp){
		case 1: /* PATHINFO_DIRNAME */
			pComp = &sInfo.sDir;
			if( pComp->nByte > 0 ){
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}else{
				/* Expand the empty string */
				ph7_result_string(pCtx,"",0);
			}
			break;
		case 2: /*PATHINFO_BASENAME*/
			pComp = &sInfo.sBasename;
			if( pComp->nByte > 0 ){
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}else{
				/* Expand the empty string */
				ph7_result_string(pCtx,"",0);
			}
			break;
		case 3: /*PATHINFO_EXTENSION*/
			pComp = &sInfo.sExtension;
			if( pComp->nByte > 0 ){
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}else{
				/* Expand the empty string */
				ph7_result_string(pCtx,"",0);
			}
			break;
		case 4: /*PATHINFO_FILENAME*/
			pComp = &sInfo.sFilename;
			if( pComp->nByte > 0 ){
				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);
			}else{
				/* Expand the empty string */
				ph7_result_string(pCtx,"",0);
			}
			break;
		default:
			/* Expand the empty string */
			ph7_result_string(pCtx,"",0);
			break;
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
		/* dirname */
		pComp = &sInfo.sDir;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			/* Perform the insertion */
			ph7_array_add_strkey_elem(pArray,"dirname",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		/* basername */
		pComp = &sInfo.sBasename;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			/* Perform the insertion */
			ph7_array_add_strkey_elem(pArray,"basename",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		/* extension */
		pComp = &sInfo.sExtension;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			/* Perform the insertion */
			ph7_array_add_strkey_elem(pArray,"extension",pValue); /* Will make it's own copy */
		}
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		/* filename */
		pComp = &sInfo.sFilename;
		if( pComp->nByte > 0 ){
			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);
			/* Perform the insertion */
			ph7_array_add_strkey_elem(pArray,"filename",pValue); /* Will make it's own copy */
		}
		/* Return the created array */
		ph7_result_value(pCtx,pArray);
		/* Don't worry about freeing memory, everything will be released
		 * automatically as soon we return from this foreign function.
		 */
	}
	return PH7_OK;
}
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
		if( rc & 0x01 /*FNM_NOESCAPE*/){
			iEsc = 0;
		}
		if( rc & 0x08 /*FNM_CASEFOLD*/){
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
/*
 * Section:
 *    IO stream implementation.
 * Status:
 *    Stable.
 */
typedef struct io_private io_private;
struct io_private
{
	const ph7_io_stream *pStream; /* Underlying IO device */
	void *pHandle; /* IO handle */
	/* Unbuffered IO */
	SyBlob sBuffer; /* Working buffer */
	sxu32 nOfft;    /* Current read offset */
	sxu32 iMagic;   /* Sanity check to avoid misuse */
};
#define IO_PRIVATE_MAGIC 0xFEAC14
/* Make sure we are dealing with a valid io_private instance */
#define IO_PRIVATE_INVALID(IO) ( IO == 0 || IO->iMagic != IO_PRIVATE_MAGIC )
/* Forward declaration */
static void ResetIOPrivate(io_private *pDev);
/*
 * bool ftruncate(resource $handle,int64 $size)
 *  Truncates a file to a given length.
 * Parameters
 *  $handle
 *   The file pointer.
 *   Note:
 *    The handle must be open for writing.
 * $size
 *   The size to truncate to.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int rc;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xTrunc == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));
	if( rc == PH7_OK ){
		/* Discard buffered data */
		ResetIOPrivate(pDev);
	}
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])
 *  Seeks on a file pointer.
 * Parameters
 *  $handle
 *   A file system pointer resource that is typically created using fopen().
 * $offset
 *   The offset.
 *   To move to a position before the end-of-file, you need to pass a negative
 *   value in offset and set whence to SEEK_END.
 *   whence
 *   whence values are:
 *    SEEK_SET - Set position equal to offset bytes.
 *    SEEK_CUR - Set position to current location plus offset.
 *    SEEK_END - Set position to end-of-file plus offset.
 * Return
 *  0 on success,-1 on failure
 */
static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_int64 iOfft;
	int whence;
	int rc;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xSeek == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Extract the offset */
	iOfft = ph7_value_to_int64(apArg[1]);
	whence = 0;/* SEEK_SET */
	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){
		whence = ph7_value_to_int(apArg[2]);
	}
	/* Perform the requested operation */
	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);
	if( rc == PH7_OK ){
		/* Ignore buffered data */
		ResetIOPrivate(pDev);
	}
	/* IO result */
	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);
	return PH7_OK;
}
/*
 * int64 ftell(resource $handle)
 *  Returns the current position of the file read/write pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  Returns the position of the file pointer referenced by handle
 *  as an integer; i.e., its offset into the file stream.
 *  FALSE is returned on failure.
 */
static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_int64 iOfft;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xTell == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	iOfft = pStream->xTell(pDev->pHandle);
	/* IO result */
	ph7_result_int64(pCtx,iOfft);
	return PH7_OK;
}
/*
 * bool rewind(resource $handle)
 *  Rewind the position of a file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xSeek == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);
	if( rc == PH7_OK ){
		/* Ignore buffered data */
		ResetIOPrivate(pDev);
	}
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool fflush(resource $handle)
 *  Flushes the output to a file.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0 || pStream->xSync == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	rc = pStream->xSync(pDev->pHandle);
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool feof(resource $handle)
 *  Tests for end-of-file on a file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  Returns TRUE if the file pointer is at EOF.FALSE otherwise
 */
static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	rc = SXERR_EOF;
	/* Perform the requested operation */
	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){
		/* Data is available */
		rc = PH7_OK;
	}else{
		char zBuf[4096];
		ph7_int64 n;
		/* Perform a buffered read */
		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));
		if( n > 0 ){
			/* Copy buffered data */
			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);
			rc = PH7_OK;
		}
	}
	/* EOF or not */
	ph7_result_bool(pCtx,rc == SXERR_EOF);
	return PH7_OK;
}
/*
 * Read n bytes from the underlying IO stream device.
 * Return total numbers of bytes readen on success. A number < 1 on failure
 * [i.e: IO error ] or EOF.
 */
static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)
{
	const ph7_io_stream *pStream = pDev->pStream;
	char *zBuf = (char *)pBuf;
	ph7_int64 n,nRead;
	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;
	if( n > 0 ){
		if( n > nLen ){
			n = nLen;
		}
		/* Copy the buffered data */
		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);
		/* Update the read offset */
		pDev->nOfft += (sxu32)n;
		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){
			/* Reset the working buffer so that we avoid excessive memory allocation */
			SyBlobReset(&pDev->sBuffer);
			pDev->nOfft = 0;
		}
		nLen -= n;
		if( nLen < 1 ){
			/* All done */
			return n;
		}
		/* Advance the cursor */
		zBuf += n;
	}
	/* Read without buffering */
	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);
	if( nRead > 0 ){
		n += nRead;
	}else if( n < 1 ){
		/* EOF or IO error */
		return nRead;
	}
	return n;
}
/*
 * Extract a single line from the buffered input.
 */
static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)
{
	const char *zIn,*zEnd,*zPtr;
	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);
	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];
	zPtr = zIn;
	while( zIn < zEnd ){
		if( zIn[0] == '\n' ){
			/* Line found */
			zIn++; /* Include the line ending as requested by the PHP specification */
			*pLen = (ph7_int64)(zIn-zPtr);
			*pzLine = zPtr;
			return SXRET_OK;
		}
		zIn++;
	}
	/* No line were found */
	return SXERR_NOTFOUND;
}
/*
 * Read a single line from the underlying IO stream device.
 */
static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)
{
	const ph7_io_stream *pStream = pDev->pStream;
	char zBuf[8192];
	ph7_int64 n;
	sxi32 rc;
	n = 0;
	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){
		/* Reset the working buffer so that we avoid excessive memory allocation */
		SyBlobReset(&pDev->sBuffer);
		pDev->nOfft = 0;
	}
	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){
		/* Check if there is a line */
		rc = GetLine(pDev,&n,pzData);
		if( rc == SXRET_OK ){
			/* Got line,update the cursor  */
			pDev->nOfft += (sxu32)n;
			return n;
		}
	}
	/* Perform the read operation until a new line is extracted or length
	 * limit is reached.
	 */
	for(;;){
		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error */
			break;
		}
		/* Append the data just read */
		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);
		/* Try to extract a line */
		rc = GetLine(pDev,&n,pzData);
		if( rc == SXRET_OK ){
			/* Got one,return immediately */
			pDev->nOfft += (sxu32)n;
			return n;
		}
		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){
			/* Read limit reached,return the available data */
			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);
			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;
			/* Reset the working buffer */
			SyBlobReset(&pDev->sBuffer);
			pDev->nOfft = 0;
			return n;
		}
	}
	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){
		/* Read limit reached,return the available data */
		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);
		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;
		/* Reset the working buffer */
		SyBlobReset(&pDev->sBuffer);
		pDev->nOfft = 0;
	}
	return n;
}
/*
 * Open an IO stream handle.
 * Notes on stream:
 * According to the PHP reference manual.
 * In its simplest definition, a stream is a resource object which exhibits streamable behavior.
 * That is, it can be read from or written to in a linear fashion, and may be able to fseek()
 * to an arbitrary locations within the stream.
 * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.
 * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file
 * on a remote server.
 * A stream is referenced as: scheme://target
 *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...
 *   If no wrapper is specified, the function default is used (typically file://).
 *   target - Depends on the wrapper used. For filesystem related streams this is typically a path
 *  and filename of the desired file. For network related streams this is typically a hostname, often
 *  with a path appended.
 *
 * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.
 * Please refer to the official documentation for a full discussion.
 * This function return a handle on success. Otherwise null.
 */
PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,
	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)
{
	void *pHandle = 0; /* cc warning */
	SyString sFile;
	int rc;
	if( pStream == 0 ){
		/* No such stream device */
		return 0;
	}
	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));
	if( use_include ){
		if(	sFile.zString[0] == '/' ||
#ifdef __WINNT__
			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' || sFile.zString[2] == '/') ) ||
#endif
			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') ||
			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){
				/*  Open the file directly */
				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);
		}else{
			SyString *pPath;
			SyBlob sWorker;
#ifdef __WINNT__
			static const int c = '\\';
#else
			static const int c = '/';
#endif
			/* Init the path builder working buffer */
			SyBlobInit(&sWorker,&pVm->sAllocator);
			/* Build a path from the set of include path */
			SySetResetCursor(&pVm->aPaths);
			rc = SXERR_IO;
			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){
				/* Build full path */
				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);
				/* Append null terminator */
				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){
					continue;
				}
				/* Try to open the file */
				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);
				if( rc == PH7_OK ){
					if( bPushInclude ){
						/* Mark as included */
						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);
					}
					break;
				}
				/* Reset the working buffer */
				SyBlobReset(&sWorker);
				/* Check the next path */
			}
			SyBlobRelease(&sWorker);
		}
		if( rc == PH7_OK ){
			if( bPushInclude ){
				/* Mark as included */
				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);
			}
		}
	}else{
		/* Open the URI direcly */
		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);
	}
	if( rc != PH7_OK ){
		/* IO error */
		return 0;
	}
	/* Return the file handle */
	return pHandle;
}
/*
 * Read the whole contents of an open IO stream handle [i.e local file/URL..]
 * Store the read data in the given BLOB (last argument).
 * The read operation is stopped when he hit the EOF or an IO error occurs.
 */
PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)
{
	ph7_int64 nRead;
	char zBuf[8192]; /* 8K */
	int rc;
	/* Perform the requested operation */
	for(;;){
		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));
		if( nRead < 1 ){
			/* EOF or IO error */
			break;
		}
		/* Append contents */
		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);
		if( rc != SXRET_OK ){
			break;
		}
	}
	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;
}
/*
 * Close an open IO stream handle [i.e local file/URI..].
 */
PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)
{
	if( pStream->xClose ){
		pStream->xClose(pHandle);
	}
}
/*
 * string fgetc(resource $handle)
 *  Gets a character from the given file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  Returns a string containing a single character read from the file
 *  pointed to by handle. Returns FALSE on EOF.
 * WARNING
 *  This operation is extremely slow.Avoid using it.
 */
static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int c,n;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));
	/* IO result */
	if( n < 1 ){
		/* EOF or error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Return the string holding the character */
		ph7_result_string(pCtx,(const char *)&c,sizeof(char));
	}
	return PH7_OK;
}
/*
 * string fgets(resource $handle[,int64 $length ])
 *  Gets line from file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * $length
 *  Reading ends when length - 1 bytes have been read, on a newline
 *  (which is included in the return value), or on EOF (whichever comes first).
 *  If no length is specified, it will keep reading from the stream until it reaches
 *  the end of the line.
 * Return
 *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.
 *  If there is no more data to read in the file pointer, then FALSE is returned.
 *  If an error occurs, FALSE is returned.
 */
static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zLine;
	io_private *pDev;
	ph7_int64 n,nLen;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nLen = -1;
	if( nArg > 1 ){
		/* Maximum data to read */
		nLen = ph7_value_to_int64(apArg[1]);
	}
	/* Perform the requested operation */
	n = StreamReadLine(pDev,&zLine,nLen);
	if( n < 1 ){
		/* EOF or IO error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Return the freshly extracted line */
		ph7_result_string(pCtx,zLine,(int)n);
	}
	return PH7_OK;
}
/*
 * string fread(resource $handle,int64 $length)
 *  Binary-safe file read.
 * Parameters
 *  $handle
 *   The file pointer.
 * $length
 *  Up to length number of bytes read.
 * Return
 *  The data readen on success or FALSE on failure.
 */
static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_int64 nRead;
	void *pBuf;
	int nLen;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
        nLen = 4096;
	if( nArg > 1 ){
 	  nLen = ph7_value_to_int(apArg[1]);
	  if( nLen < 1 ){
		/* Invalid length,set a default length */
		nLen = 4096;
	  }
        }
	/* Allocate enough buffer */
	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);
	if( pBuf == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);
	if( nRead < 1 ){
		/* Nothing read,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Make a copy of the data just read */
		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);
	}
	/* Release the buffer */
	ph7_context_free_chunk(pCtx,pBuf);
	return PH7_OK;
}
/*
 * array fgetcsv(resource $handle [, int $length = 0
 *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])
 * Gets line from file pointer and parse for CSV fields.
 * Parameters
 * $handle
 *   The file pointer.
 * $length
 *  Reading ends when length - 1 bytes have been read, on a newline
 *  (which is included in the return value), or on EOF (whichever comes first).
 *  If no length is specified, it will keep reading from the stream until it reaches
 *  the end of the line.
 * $delimiter
 *   Set the field delimiter (one character only).
 * $enclosure
 *   Set the field enclosure character (one character only).
 * $escape
 *   Set the escape character (one character only). Defaults as a backslash (\)
 * Return
 *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.
 *  If there is no more data to read in the file pointer, then FALSE is returned.
 *  If an error occurs, FALSE is returned.
 */
static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zLine;
	io_private *pDev;
	ph7_int64 n,nLen;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nLen = -1;
	if( nArg > 1 ){
		/* Maximum data to read */
		nLen = ph7_value_to_int64(apArg[1]);
	}
	/* Perform the requested operation */
	n = StreamReadLine(pDev,&zLine,nLen);
	if( n < 1 ){
		/* EOF or IO error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		ph7_value *pArray;
		int delim  = ',';   /* Delimiter */
		int encl   = '"' ;  /* Enclosure */
		int escape = '\\';  /* Escape character */
		if( nArg > 2 ){
			const char *zPtr;
			int i;
			if( ph7_value_is_string(apArg[2]) ){
				/* Extract the delimiter */
				zPtr = ph7_value_to_string(apArg[2],&i);
				if( i > 0 ){
					delim = zPtr[0];
				}
			}
			if( nArg > 3 ){
				if( ph7_value_is_string(apArg[3]) ){
					/* Extract the enclosure */
					zPtr = ph7_value_to_string(apArg[3],&i);
					if( i > 0 ){
						encl = zPtr[0];
					}
				}
				if( nArg > 4 ){
					if( ph7_value_is_string(apArg[4]) ){
						/* Extract the escape character */
						zPtr = ph7_value_to_string(apArg[4],&i);
						if( i > 0 ){
							escape = zPtr[0];
						}
					}
				}
			}
		}
		/* Create our array */
		pArray = ph7_context_new_array(pCtx);
		if( pArray == 0 ){
			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		/* Parse the raw input */
		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);
		/* Return the freshly created array  */
		ph7_result_value(pCtx,pArray);
	}
	return PH7_OK;
}
/*
 * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])
 *  Gets line from file pointer and strip HTML tags.
 * Parameters
 * $handle
 *   The file pointer.
 * $length
 *  Reading ends when length - 1 bytes have been read, on a newline
 *  (which is included in the return value), or on EOF (whichever comes first).
 *  If no length is specified, it will keep reading from the stream until it reaches
 *  the end of the line.
 * $allowable_tags
 *  You can use the optional second parameter to specify tags which should not be stripped.
 * Return
 *  Returns a string of up to length - 1 bytes read from the file pointed to by
 *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.
 */
static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zLine;
	io_private *pDev;
	ph7_int64 n,nLen;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nLen = -1;
	if( nArg > 1 ){
		/* Maximum data to read */
		nLen = ph7_value_to_int64(apArg[1]);
	}
	/* Perform the requested operation */
	n = StreamReadLine(pDev,&zLine,nLen);
	if( n < 1 ){
		/* EOF or IO error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		const char *zTaglist = 0;
		int nTaglen = 0;
		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){
			/* Allowed tag */
			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);
		}
		/* Process data just read */
		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);
	}
	return PH7_OK;
}
/*
 * string readdir(resource $dir_handle)
 *   Read entry from directory handle.
 * Parameter
 *  $dir_handle
 *   The directory handle resource previously opened with opendir().
 * Return
 *  Returns the filename on success or FALSE on failure.
 */
static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xReadDir == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,0);
	/* Perform the requested operation */
	rc = pStream->xReadDir(pDev->pHandle,pCtx);
	if( rc != PH7_OK ){
		/* Return FALSE */
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * void rewinddir(resource $dir_handle)
 *   Rewind directory handle.
 * Parameter
 *  $dir_handle
 *   The directory handle resource previously opened with opendir().
 * Return
 *  FALSE on failure.
 */
static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xRewindDir == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pStream->xRewindDir(pDev->pHandle);
	return PH7_OK;
 }
/* Forward declaration */
static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);
static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);
/*
 * void closedir(resource $dir_handle)
 *   Close directory handle.
 * Parameter
 *  $dir_handle
 *   The directory handle resource previously opened with opendir().
 * Return
 *  FALSE on failure.
 */
static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xCloseDir == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pStream->xCloseDir(pDev->pHandle);
	/* Release the private stucture */
	ReleaseIOPrivate(pCtx,pDev);
	PH7_MemObjRelease(apArg[0]);
	return PH7_OK;
 }
/*
 * resource opendir(string $path[,resource $context])
 *  Open directory handle.
 * Parameters
 * $path
 *   The directory path that is to be opened.
 * $context
 *   A context stream resource.
 * Return
 *  A directory handle resource on success,or FALSE on failure.
 */
static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zPath;
	io_private *pDev;
	int iLen,rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target path */
	zPath  = ph7_value_to_string(apArg[0],&iLen);
	/* Try to extract a stream */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);
	if( pStream == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"No stream device is associated with the given path(%s)",zPath);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pStream->xOpenDir == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device",
			ph7_function_name(pCtx),pStream->zName
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Allocate a new IO private instance */
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	if( pDev == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Initialize the structure */
	InitIOPrivate(pCtx->pVm,pStream,pDev);
	/* Open the target directory */
	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);
	if( rc != PH7_OK ){
		/* IO error,return FALSE */
		ReleaseIOPrivate(pCtx,pDev);
		ph7_result_bool(pCtx,0);
	}else{
		/* Return the handle as a resource */
		ph7_result_resource(pCtx,pDev);
	}
	return PH7_OK;
}
/*
 * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])
 *  Reads a file and writes it to the output buffer.
 * Parameters
 *  $filename
 *   The filename being read.
 *  $use_include_path
 *   You can use the optional second parameter and set it to
 *   TRUE, if you want to search for the file in the include_path, too.
 *  $context
 *   A context stream resource.
 * Return
 *  The number of bytes read from the file on success or FALSE on failure.
 */
static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int use_include  = FALSE;
	const ph7_io_stream *pStream;
	ph7_int64 n,nRead;
	const char *zFile;
	char zBuf[8192];
	void *pHandle;
	int rc,nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* Point to the target IO stream device */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pStream == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		use_include = ph7_value_to_bool(apArg[1]);
	}
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,
		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);
	if( pHandle == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	nRead = 0;
	for(;;){
		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error,break immediately */
			break;
		}
		/* Output data */
		rc = ph7_context_output(pCtx,zBuf,(int)n);
		if( rc == PH7_ABORT ){
			break;
		}
		/* Increment counter */
		nRead += n;
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Total number of bytes readen */
	ph7_result_int64(pCtx,nRead);
	return PH7_OK;
}
/*
 * string file_get_contents(string $filename[,bool $use_include_path = false
 *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])
 *  Reads entire file into a string.
 * Parameters
 *  $filename
 *   The filename being read.
 *  $use_include_path
 *   You can use the optional second parameter and set it to
 *   TRUE, if you want to search for the file in the include_path, too.
 *  $context
 *   A context stream resource.
 *  $offset
 *   The offset where the reading starts on the original stream.
 *  $maxlen
 *    Maximum length of data read. The default is to read until end of file
 *    is reached. Note that this parameter is applied to the stream processed by the filters.
 * Return
 *   The function returns the read data or FALSE on failure.
 */
static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	ph7_int64 n,nRead,nMaxlen;
	int use_include  = FALSE;
	const char *zFile;
	char zBuf[8192];
	void *pHandle;
	int nLen;

	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* Point to the target IO stream device */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pStream == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nMaxlen = -1;
	if( nArg > 1 ){
		use_include = ph7_value_to_bool(apArg[1]);
	}
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);
	if( pHandle == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 3 ){
		/* Extract the offset */
		n = ph7_value_to_int64(apArg[3]);
		if( n > 0 ){
			if( pStream->xSeek ){
				/* Seek to the desired offset */
				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);
			}
		}
		if( nArg > 4 ){
			/* Maximum data to read */
			nMaxlen = ph7_value_to_int64(apArg[4]);
		}
	}
	/* Perform the requested operation */
	nRead = 0;
	for(;;){
		n = pStream->xRead(pHandle,zBuf,
			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error,break immediately */
			break;
		}
		/* Append data */
		ph7_result_string(pCtx,zBuf,(int)n);
		/* Increment read counter */
		nRead += n;
		if( nMaxlen > 0 && nRead >= nMaxlen ){
			/* Read limit reached */
			break;
		}
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Check if we have read something */
	if( ph7_context_result_buf_length(pCtx) < 1 ){
		/* Nothing read,return FALSE */
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])
 *  Write a string to a file.
 * Parameters
 *  $filename
 *  Path to the file where to write the data.
 * $data
 *  The data to write(Must be a string).
 * $flags
 *  The value of flags can be any combination of the following
 * flags, joined with the binary OR (|) operator.
 *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.
 *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.
 *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.
 * context
 *  A context stream resource.
 * Return
 *  The function returns the number of bytes that were written to the file, or FALSE on failure.
 */
static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int use_include  = FALSE;
	const ph7_io_stream *pStream;
	const char *zFile;
	const char *zData;
	int iOpenFlags;
	void *pHandle;
	int iFlags;
	int nLen;

	if( nArg < 2 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* Point to the target IO stream device */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pStream == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Data to write */
	zData = ph7_value_to_string(apArg[1],&nLen);
	/* Try to open the file in read-write mode */
	iOpenFlags = PH7_IO_OPEN_CREATE|PH7_IO_OPEN_RDWR|PH7_IO_OPEN_TRUNC;
	/* Extract the flags */
	iFlags = 0;
	if( nArg > 2 ){
		iFlags = ph7_value_to_int(apArg[2]);
		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){
			use_include = TRUE;
		}
		if( iFlags & 0x08 /* FILE_APPEND */){
			/* If the file already exists, append the data to the file
			 * instead of overwriting it.
			 */
			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;
			/* Append mode */
			iOpenFlags |= PH7_IO_OPEN_APPEND;
		}
	}
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,
		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);
	if( pHandle == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nLen < 1 ){
		/* Empty data, file is created/truncated */
		ph7_result_int64(pCtx,0);
		PH7_StreamCloseHandle(pStream,pHandle);
		return PH7_OK;
	}
	if( pStream->xWrite ){
		ph7_int64 n;
		if( (iFlags & 0x01/* LOCK_EX */) && pStream->xLock ){
			/* Try to acquire an exclusive lock */
			pStream->xLock(pHandle,1/* LOCK_EX */);
		}
		/* Perform the write operation */
		n = pStream->xWrite(pHandle,(const void *)zData,nLen);
		if( n < 0 ){
			/* IO error,return FALSE */
			ph7_result_bool(pCtx,0);
		}else{
			/* Total number of bytes written */
			ph7_result_int64(pCtx,n);
		}
	}else{
		/* Read-only stream */
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,
			"Read-only stream(%s): Cannot perform write operation",
			pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
	}
	/* Close the handle */
	PH7_StreamCloseHandle(pStream,pHandle);
	return PH7_OK;
}
/*
 * array file(string $filename[,int $flags = 0[,resource $context]])
 *  Reads entire file into an array.
 * Parameters
 *  $filename
 *   The filename being read.
 *  $flags
 *   The optional parameter flags can be one, or more, of the following constants:
 *   FILE_USE_INCLUDE_PATH
 *       Search for the file in the include_path.
 *   FILE_IGNORE_NEW_LINES
 *       Do not add newline at the end of each array element
 *   FILE_SKIP_EMPTY_LINES
 *       Skip empty lines
 *  $context
 *   A context stream resource.
 * Return
 *   The function returns the read data or FALSE on failure.
 */
static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFile,*zPtr,*zEnd,*zBuf;
	ph7_value *pArray,*pLine;
	const ph7_io_stream *pStream;
	int use_include = 0;
	io_private *pDev;
	ph7_int64 n;
	int iFlags;
	int nLen;

	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* Point to the target IO stream device */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pStream == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Allocate a new IO private instance */
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	if( pDev == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Initialize the structure */
	InitIOPrivate(pCtx->pVm,pStream,pDev);
	iFlags = 0;
	if( nArg > 1 ){
		iFlags = ph7_value_to_int(apArg[1]);
	}
	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){
		use_include = TRUE;
	}
	/* Create the array and the working value */
	pArray = ph7_context_new_array(pCtx);
	pLine = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pLine == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Try to open the file in read-only mode */
	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);
	if( pDev->pHandle == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);
		ph7_result_bool(pCtx,0);
		/* Don't worry about freeing memory, everything will be released automatically
		 * as soon we return from this function.
		 */
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		/* Try to extract a line */
		n = StreamReadLine(pDev,&zBuf,-1);
		if( n < 1 ){
			/* EOF or IO error */
			break;
		}
		/* Reset the cursor */
		ph7_value_reset_string_cursor(pLine);
		/* Remove line ending if requested by the caller */
		zPtr = zBuf;
		zEnd = &zBuf[n];
		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){
			/* Ignore trailig lines */
			while( zPtr < zEnd && (zEnd[-1] == '\n'
#ifdef __WINNT__
				|| zEnd[-1] == '\r'
#endif
				)){
					n--;
					zEnd--;
			}
		}
		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){
			/* Ignore empty lines */
			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){
				zPtr++;
			}
			if( zPtr >= zEnd ){
				/* Empty line */
				continue;
			}
		}
		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));
		/* Insert line */
		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pDev->pHandle);
	/* Release the io_private instance */
	ReleaseIOPrivate(pCtx,pDev);
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool copy(string $source,string $dest[,resource $context ] )
 *  Makes a copy of the file source to dest.
 * Parameters
 *  $source
 *   Path to the source file.
 *  $dest
 *   The destination path. If dest is a URL, the copy operation
 *   may fail if the wrapper does not support overwriting of existing files.
 *  $context
 *   A context stream resource.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pSin,*pSout;
	const char *zFile;
	char zBuf[8192];
	void *pIn,*pOut;
	ph7_int64 n;
	int nLen;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_string(apArg[1])){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the source name */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* Point to the target IO stream device */
	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pSin == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Try to open the source file in a read-only mode */
	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);
	if( pIn == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening source: '%s'",zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the destination name */
	zFile = ph7_value_to_string(apArg[1],&nLen);
	/* Point to the target IO stream device */
	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pSout == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		PH7_StreamCloseHandle(pSin,pIn);
		return PH7_OK;
	}
	if( pSout->xWrite == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pSin->zName
			);
		ph7_result_bool(pCtx,0);
		PH7_StreamCloseHandle(pSin,pIn);
		return PH7_OK;
	}
	/* Try to open the destination file in a read-write mode */
	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,
		PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);
	if( pOut == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening destination: '%s'",zFile);
		ph7_result_bool(pCtx,0);
		PH7_StreamCloseHandle(pSin,pIn);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		/* Read from source */
		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error,break immediately */
			break;
		}
		/* Write to dest */
		n = pSout->xWrite(pOut,zBuf,n);
		if( n < 1 ){
			/* IO error,break immediately */
			break;
		}
	}
	/* Close the streams */
	PH7_StreamCloseHandle(pSin,pIn);
	PH7_StreamCloseHandle(pSout,pOut);
	/* Return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * array fstat(resource $handle)
 *  Gets information about a file using an open file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  Returns an array with the statistics of the file or FALSE on failure.
 */
static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pValue;
	const ph7_io_stream *pStream;
	io_private *pDev;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/* Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xStat == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
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
	/* Perform the requested operation */
	pStream->xStat(pDev->pHandle,pArray,pValue);
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	/* Don't worry about freeing memory here,everything will be
	 * released automatically as soon we return from this function.
	 */
	return PH7_OK;
}
/*
 * int fwrite(resource $handle,string $string[,int $length])
 *  Writes the contents of string to the file stream pointed to by handle.
 * Parameters
 *  $handle
 *   The file pointer.
 *  $string
 *   The string that is to be written.
 *  $length
 *   If the length argument is given, writing will stop after length bytes have been written
 *   or the end of string is reached, whichever comes first.
 * Return
 *  Returns the number of bytes written, or FALSE on error.
 */
static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zString;
	io_private *pDev;
	int nLen,n;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/* Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xWrite == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the data to write */
	zString = ph7_value_to_string(apArg[1],&nLen);
	if( nArg > 2 ){
		/* Maximum data length to write */
		n = ph7_value_to_int(apArg[2]);
		if( n >= 0 && n < nLen ){
			nLen = n;
		}
	}
	if( nLen < 1 ){
		/* Nothing to write */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);
	if( n <  0 ){
		/* IO error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* #Bytes written */
		ph7_result_int(pCtx,n);
	}
	return PH7_OK;
}
/*
 * bool flock(resource $handle,int $operation)
 *  Portable advisory file locking.
 * Parameters
 *  $handle
 *   The file pointer.
 *  $operation
 *   operation is one of the following:
 *      LOCK_SH to acquire a shared lock (reader).
 *      LOCK_EX to acquire an exclusive lock (writer).
 *      LOCK_UN to release a lock (shared or exclusive).
 * Return
 *  Returns TRUE on success or FALSE on failure.
 */
static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int nLock;
	int rc;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xLock == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Requested lock operation */
	nLock = ph7_value_to_int(apArg[1]);
	/* Lock operation */
	rc = pStream->xLock(pDev->pHandle,nLock);
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * int fpassthru(resource $handle)
 *  Output all remaining data on a file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  Total number of characters read from handle and passed through
 *  to the output on success or FALSE on failure.
 */
static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_int64 n,nRead;
	char zBuf[8192];
	int rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	nRead = 0;
	for(;;){
		n = StreamRead(pDev,zBuf,sizeof(zBuf));
		if( n < 1 ){
			/* Error or EOF */
			break;
		}
		/* Increment the read counter */
		nRead += n;
		/* Output data */
		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);
		if( rc == PH7_ABORT ){
			/* Consumer callback request an operation abort */
			break;
		}
	}
	/* Total number of bytes readen */
	ph7_result_int64(pCtx,nRead);
	return PH7_OK;
}
/* CSV reader/writer private data */
struct csv_data
{
	int delimiter;    /* Delimiter. Default ',' */
	int enclosure;    /* Enclosure. Default '"'*/
	io_private *pDev; /* Open stream handle */
	int iCount;       /* Counter */
};
/*
 * The following callback is used by the fputcsv() function inorder to iterate
 * throw array entries and output CSV data based on the current key and it's
 * associated data.
 */
static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	struct csv_data *pData = (struct csv_data *)pUserData;
	const char *zData;
	int nLen,c2;
	sxu32 n;
	/* Point to the raw data */
	zData = ph7_value_to_string(pValue,&nLen);
	if( nLen < 1 ){
		/* Nothing to write */
		return PH7_OK;
	}
	if( pData->iCount > 0 ){
		/* Write the delimiter */
		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));
	}
	n = 1;
	c2 = 0;
	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK ||
		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){
			c2 = 1;
			if( n == 0 ){
				c2 = 2;
			}
			/* Write the enclosure */
			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));
			if( c2 > 1 ){
				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));
			}
	}
	/* Write the data */
	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){
		SXUNUSED(pKey); /* cc warning */
		return PH7_ABORT;
	}
	if( c2 > 0 ){
		/* Write the enclosure */
		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));
		if( c2 > 1 ){
			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));
		}
	}
	pData->iCount++;
	return PH7_OK;
}
/*
 * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])
 *  Format line as CSV and write to file pointer.
 * Parameters
 *  $handle
 *   Open file handle.
 * $fields
 *   An array of values.
 * $delimiter
 *   The optional delimiter parameter sets the field delimiter (one character only).
 * $enclosure
 *  The optional enclosure parameter sets the field enclosure (one character only).
 */
static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	struct csv_data sCsv;
	io_private *pDev;
	char *zEol;
	int eolen;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) || !ph7_value_is_array(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xWrite == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Set default csv separator */
	sCsv.delimiter = ',';
	sCsv.enclosure = '"';
	sCsv.pDev = pDev;
	sCsv.iCount = 0;
	if( nArg > 2 ){
		/* User delimiter */
		const char *z;
		int n;
		z = ph7_value_to_string(apArg[2],&n);
		if( n > 0 ){
			sCsv.delimiter = z[0];
		}
		if( nArg > 3 ){
			z = ph7_value_to_string(apArg[3],&n);
			if( n > 0 ){
				sCsv.enclosure = z[0];
			}
		}
	}
	/* Iterate throw array entries and write csv data */
	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);
	/* Write a line ending */
#ifdef __WINNT__
	zEol = "\r\n";
	eolen = (int)sizeof("\r\n")-1;
#else
	/* Assume UNIX LF */
	zEol = "\n";
	eolen = (int)sizeof(char);
#endif
	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);
	return PH7_OK;
}
/*
 * fprintf,vfprintf private data.
 * An instance of the following structure is passed to the formatted
 * input consumer callback defined below.
 */
typedef struct fprintf_data fprintf_data;
struct fprintf_data
{
	io_private *pIO;        /* IO stream */
	ph7_int64 nCount;       /* Total number of bytes written */
};
/*
 * Callback [i.e: Formatted input consumer] for the fprintf function.
 */
static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)
{
	fprintf_data *pFdata = (fprintf_data *)pUserData;
	ph7_int64 n;
	/* Write the formatted data */
	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);
	if( n < 1 ){
		SXUNUSED(pCtx); /* cc warning */
		/* IO error,abort immediately */
		return SXERR_ABORT;
	}
	/* Increment counter */
	pFdata->nCount += n;
	return PH7_OK;
}
/*
 * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])
 *  Write a formatted string to a stream.
 * Parameters
 *  $handle
 *   The file pointer.
 *  $format
 *   String format (see sprintf()).
 * Return
 *  The length of the written string.
 */
static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	fprintf_data sFdata;
	const char *zFormat;
	io_private *pDev;
	int nLen;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Invalid arguments,return zero */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	if( pDev->pStream == 0  || pDev->pStream->xWrite == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device",
			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"
			);
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the string format */
	zFormat = ph7_value_to_string(apArg[1],&nLen);
	if( nLen < 1 ){
		/* Empty string,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Prepare our private data */
	sFdata.nCount = 0;
	sFdata.pIO = pDev;
	/* Format the string */
	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);
	/* Return total number of bytes written */
	ph7_result_int64(pCtx,sFdata.nCount);
	return PH7_OK;
}
/*
 * int vfprintf(resource $handle,string $format,array $args)
 *  Write a formatted string to a stream.
 * Parameters
 *  $handle
 *   The file pointer.
 *  $format
 *   String format (see sprintf()).
 * $args
 *   User arguments.
 * Return
 *  The length of the written string.
 */
static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	fprintf_data sFdata;
	const char *zFormat;
	ph7_hashmap *pMap;
	io_private *pDev;
	SySet sArg;
	int n,nLen;
	if( nArg < 3 || !ph7_value_is_resource(apArg[0]) || !ph7_value_is_string(apArg[1])  || !ph7_value_is_array(apArg[2]) ){
		/* Missing/Invalid arguments,return zero */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	if( pDev->pStream == 0  || pDev->pStream->xWrite == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device",
			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"
			);
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the string format */
	zFormat = ph7_value_to_string(apArg[1],&nLen);
	if( nLen < 1 ){
		/* Empty string,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to hashmap */
	pMap = (ph7_hashmap *)apArg[2]->x.pOther;
	/* Extract arguments from the hashmap */
	n = PH7_HashmapValuesToSet(pMap,&sArg);
	/* Prepare our private data */
	sFdata.nCount = 0;
	sFdata.pIO = pDev;
	/* Format the string */
	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);
	/* Return total number of bytes written*/
	ph7_result_int64(pCtx,sFdata.nCount);
	SySetRelease(&sArg);
	return PH7_OK;
}
/*
 * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.
 * According to the PHP reference manual:
 *  The mode parameter specifies the type of access you require to the stream. It may be any of the following
 *   'r' 	Open for reading only; place the file pointer at the beginning of the file.
 *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.
 *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file
 *          to zero length. If the file does not exist, attempt to create it.
 *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate
 *              the file to zero length. If the file does not exist, attempt to create it.
 *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not
 *         exist, attempt to create it.
 *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does
 *          not exist, attempt to create it.
 *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file
 *         already exists,
 *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file
 *         does not exist attempt to create it. This is equivalent to specifying O_EXCL|O_CREAT flags for
 *         the underlying open(2) system call.
 *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.
 *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated
 *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer
 *          is positioned on the beginning of the file.
 *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file
 *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can
 *          be used after the lock is requested).
 *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.
 */
static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)
{
	const char *zEnd = &zMode[nLen];
	int iFlag = 0;
	int c;
	if( nLen < 1 ){
		/* Open in a read-only mode */
		return PH7_IO_OPEN_RDONLY;
	}
	c = zMode[0];
	if( c == 'r' || c == 'R' ){
		/* Read-only access */
		iFlag = PH7_IO_OPEN_RDONLY;
		zMode++; /* Advance */
		if( zMode < zEnd ){
			c = zMode[0];
			if( c == '+' || c == 'w' || c == 'W' ){
				/* Read+Write access */
				iFlag = PH7_IO_OPEN_RDWR;
			}
		}
	}else if( c == 'w' || c == 'W' ){
		/* Overwrite mode.
		 * If the file does not exists,try to create it
		 */
		iFlag = PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_TRUNC|PH7_IO_OPEN_CREATE;
		zMode++; /* Advance */
		if( zMode < zEnd ){
			c = zMode[0];
			if( c == '+' || c == 'r' || c == 'R' ){
				/* Read+Write access */
				iFlag &= ~PH7_IO_OPEN_WRONLY;
				iFlag |= PH7_IO_OPEN_RDWR;
			}
		}
	}else if( c == 'a' || c == 'A' ){
		/* Append mode (place the file pointer at the end of the file).
		 * Create the file if it does not exists.
		 */
		iFlag = PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_APPEND|PH7_IO_OPEN_CREATE;
		zMode++; /* Advance */
		if( zMode < zEnd ){
			c = zMode[0];
			if( c == '+' ){
				/* Read-Write access */
				iFlag &= ~PH7_IO_OPEN_WRONLY;
				iFlag |= PH7_IO_OPEN_RDWR;
			}
		}
	}else if( c == 'x' || c == 'X' ){
		/* Exclusive access.
		 * If the file already exists,return immediately with a failure code.
		 * Otherwise create a new file.
		 */
		iFlag = PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_EXCL;
		zMode++; /* Advance */
		if( zMode < zEnd ){
			c = zMode[0];
			if( c == '+' || c == 'r' || c == 'R' ){
				/* Read-Write access */
				iFlag &= ~PH7_IO_OPEN_WRONLY;
				iFlag |= PH7_IO_OPEN_RDWR;
			}
		}
	}else if( c == 'c' || c == 'C' ){
		/* Overwrite mode.Create the file if it does not exists.*/
		iFlag = PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE;
		zMode++; /* Advance */
		if( zMode < zEnd ){
			c = zMode[0];
			if( c == '+' ){
				/* Read-Write access */
				iFlag &= ~PH7_IO_OPEN_WRONLY;
				iFlag |= PH7_IO_OPEN_RDWR;
			}
		}
	}else{
		/* Invalid mode. Assume a read only open */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");
		iFlag = PH7_IO_OPEN_RDONLY;
	}
	while( zMode < zEnd ){
		c = zMode[0];
		if( c == 'b' || c == 'B' ){
			iFlag &= ~PH7_IO_OPEN_TEXT;
			iFlag |= PH7_IO_OPEN_BINARY;
		}else if( c == 't' || c == 'T' ){
			iFlag &= ~PH7_IO_OPEN_BINARY;
			iFlag |= PH7_IO_OPEN_TEXT;
		}
		zMode++;
	}
	return iFlag;
}
/*
 * Initialize the IO private structure.
 */
static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)
{
	pOut->pStream = pStream;
	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);
	pOut->nOfft = 0;
	/* Set the magic number */
	pOut->iMagic = IO_PRIVATE_MAGIC;
}
/*
 * Release the IO private structure.
 */
static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)
{
	SyBlobRelease(&pDev->sBuffer);
	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */
	/* Release the whole structure */
	ph7_context_free_chunk(pCtx,pDev);
}
/*
 * Reset the IO private structure.
 */
static void ResetIOPrivate(io_private *pDev)
{
	SyBlobReset(&pDev->sBuffer);
	pDev->nOfft = 0;
}
/* Forward declaration */
static int is_php_stream(const ph7_io_stream *pStream);
/*
 * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])
 *  Open a file,a URL or any other IO stream.
 * Parameters
 *  $filename
 *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search
 *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given
 *   then a regular file is assumed.
 *  $mode
 *   The mode parameter specifies the type of access you require to the stream
 *   See the block comment associated with the StrModeToFlags() for the supported
 *   modes.
 *  $use_include_path
 *   You can use the optional second parameter and set it to
 *   TRUE, if you want to search for the file in the include_path, too.
 *  $context
 *   A context stream resource.
 * Return
 *  File handle on success or FALSE on failure.
 */
static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zUri,*zMode;
	ph7_value *pResource;
	io_private *pDev;
	int iLen,imLen;
	int iOpenFlags;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the URI and the desired access mode */
	zUri  = ph7_value_to_string(apArg[0],&iLen);
	if( nArg > 1 ){
		zMode = ph7_value_to_string(apArg[1],&imLen);
	}else{
		/* Set a default read-only mode */
		zMode = "r";
		imLen = (int)sizeof(char);
	}
	/* Try to extract a stream */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);
	if( pStream == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"No stream device is associated with the given URI(%s)",zUri);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Allocate a new IO private instance */
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	if( pDev == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pResource = 0;
	if( nArg > 3 ){
		pResource = apArg[3];
	}else if( is_php_stream(pStream) ){
		/* TICKET 1433-80: The php:// stream need a ph7_value to access the underlying
		 * virtual machine.
		 */
		pResource = apArg[0];
	}
	/* Initialize the structure */
	InitIOPrivate(pCtx->pVm,pStream,pDev);
	/* Convert open mode to PH7 flags */
	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);
	/* Try to get a handle */
	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,
		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);
	if( pDev->pHandle == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zUri);
		ph7_result_bool(pCtx,0);
		ph7_context_free_chunk(pCtx,pDev);
		return PH7_OK;
	}
	/* All done,return the io_private instance as a resource */
	ph7_result_resource(pCtx,pDev);
	return PH7_OK;
}
/*
 * bool fclose(resource $handle)
 *  Closes an open file pointer
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_vm *pVm;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the VM that own this context */
	pVm = pCtx->pVm;
	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */
	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){
		/* Perform the requested operation */
		PH7_StreamCloseHandle(pStream,pDev->pHandle);
		/* Release the IO private structure */
		ReleaseIOPrivate(pCtx,pDev);
		/* Invalidate the resource handle */
		ph7_value_release(apArg[0]);
	}
	/* Return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
#if !defined(PH7_DISABLE_HASH_FUNC)
/*
 * MD5/SHA1 digest consumer.
 */
static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)
{
	/* Append hex chunk verbatim */
	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);
	return SXRET_OK;
}
/*
 * string md5_file(string $uri[,bool $raw_output = false ])
 *  Calculates the md5 hash of a given file.
 * Parameters
 *  $uri
 *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))
 *  $raw_output
 *   When TRUE, returns the digest in raw binary format with a length of 16.
 * Return
 *  Return the MD5 digest on success or FALSE on failure.
 */
static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	unsigned char zDigest[16];
	int raw_output  = FALSE;
	const char *zFile;
	MD5Context sCtx;
	char zBuf[8192];
	void *pHandle;
	ph7_int64 n;
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* Point to the target IO stream device */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pStream == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);
	if( pHandle == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Init the MD5 context */
	MD5Init(&sCtx);
	/* Perform the requested operation */
	for(;;){
		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error,break immediately */
			break;
		}
		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Extract the digest */
	MD5Final(zDigest,&sCtx);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * string sha1_file(string $uri[,bool $raw_output = false ])
 *  Calculates the SHA1 hash of a given file.
 * Parameters
 *  $uri
 *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))
 *  $raw_output
 *   When TRUE, returns the digest in raw binary format with a length of 20.
 * Return
 *  Return the SHA1 digest on success or FALSE on failure.
 */
static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	unsigned char zDigest[20];
	int raw_output  = FALSE;
	const char *zFile;
	SHA1Context sCtx;
	char zBuf[8192];
	void *pHandle;
	ph7_int64 n;
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* Point to the target IO stream device */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pStream == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);
	if( pHandle == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Init the SHA1 context */
	SHA1Init(&sCtx);
	/* Perform the requested operation */
	for(;;){
		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error,break immediately */
			break;
		}
		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Extract the digest */
	SHA1Final(&sCtx,zDigest);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);
	}
	return PH7_OK;
}
#endif /* PH7_DISABLE_HASH_FUNC */
/*
 * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )
 *  Parse a configuration file.
 * Parameters
 * $filename
 *  The filename of the ini file being parsed.
 * $process_sections
 *  By setting the process_sections parameter to TRUE, you get a multidimensional array
 *  with the section names and settings included.
 *  The default for process_sections is FALSE.
 * $scanner_mode
 *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.
 *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.
 * Return
 *  The settings are returned as an associative array on success.
 *  Otherwise is returned.
 */
static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zFile;
	SyBlob sContents;
	void *pHandle;
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* Point to the target IO stream device */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pStream == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);
	if( pHandle == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);
	/* Read the whole file */
	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);
	if( SyBlobLength(&sContents) < 1 ){
		/* Empty buffer,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Process the raw INI buffer */
		PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),
			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Release the working buffer */
	SyBlobRelease(&sContents);
	return PH7_OK;
}
/* ZIP archive processing moved to vfs_zip.c */
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
	0  /* int (*xExec)(const char *,ph7_context *) */
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
#if !defined(PH7_DISABLE_BUILTIN_FUNC) || !defined(PH7_DISABLE_DISK_IO)
#ifndef PH7_DISABLE_DISK_IO
/*
 * The following defines are mostly used by the UNIX built and have
 * no particular meaning on windows.
 */
#ifndef STDIN_FILENO
#define STDIN_FILENO	0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO	1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO	2
#endif
/*
 * php:// Accessing various I/O streams
 * According to the PHP langage reference manual
 * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input
 * and output streams, the standard input, output and error file descriptors.
 * php://stdin, php://stdout and php://stderr:
 *  Allow direct access to the corresponding input or output stream of the PHP process.
 *  The stream references a duplicate file descriptor, so if you open php://stdin and later
 *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.
 *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.
 * php://output
 *  php://output is a write-only stream that allows you to write to the output buffer
 *  mechanism in the same way as print and echo.
 */
typedef struct ph7_stream_data ph7_stream_data;
/* Supported IO streams */
#define PH7_IO_STREAM_STDIN  1 /* php://stdin */
#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */
#define PH7_IO_STREAM_STDERR 3 /* php://stderr */
#define PH7_IO_STREAM_OUTPUT 4 /* php://output */
 /* The following structure is the private data associated with the php:// stream */
struct ph7_stream_data
{
	ph7_vm *pVm; /* VM that own this instance */
	int iType;   /* Stream type */
	union{
		void *pHandle; /* Stream handle */
		ph7_output_consumer sConsumer; /* VM output consumer */
	}x;
};
/*
 * Allocate a new instance of the ph7_stream_data structure.
 */
static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)
{
	ph7_stream_data *pData;
	if( pVm == 0 ){
		return 0;
	}
	/* Allocate a new instance */
	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));
	if( pData == 0 ){
		return 0;
	}
	/* Zero the structure */
	SyZero(pData,sizeof(ph7_stream_data));
	/* Initialize fields */
	pData->iType = iType;
	if( iType == PH7_IO_STREAM_OUTPUT ){
		/* Point to the default VM consumer routine. */
		pData->x.sConsumer = pVm->sVmConsumer;
	}else{
#ifdef __WINNT__
		DWORD nChannel;
		switch(iType){
		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;
		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;
		default:
			nChannel = STD_INPUT_HANDLE;
			break;
		}
		pData->x.pHandle = GetStdHandle(nChannel);
#else
		/* Assume an UNIX system */
		int ifd = STDIN_FILENO;
		switch(iType){
		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;
		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;
		default:
			break;
		}
		pData->x.pHandle = SX_INT_TO_PTR(ifd);
#endif
	}
	pData->pVm = pVm;
	return pData;
}
/*
 * Implementation of the php:// IO streams routines
 * Status:
 *   Stable.
 */
/* int (*xOpen)(const char *,int,ph7_value *,void **) */
static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)
{
	ph7_stream_data *pData;
	SyString sStream;
	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));
	/* Trim leading and trailing white spaces */
	SyStringFullTrim(&sStream);
	/* Stream to open */
	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){
		iMode = PH7_IO_STREAM_STDIN;
	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){
		iMode = PH7_IO_STREAM_OUTPUT;
	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){
		iMode = PH7_IO_STREAM_STDOUT;
	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){
		iMode = PH7_IO_STREAM_STDERR;
	}else{
		/* unknown stream name */
		return -1;
	}
	/* Create our handle */
	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);
	if( pData == 0 ){
		return -1;
	}
	/* Make the handle public */
	*ppHandle = (void *)pData;
	return PH7_OK;
}
/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */
static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)
{
	ph7_stream_data *pData = (ph7_stream_data *)pHandle;
	if( pData == 0 ){
		return -1;
	}
	if( pData->iType != PH7_IO_STREAM_STDIN ){
		/* Forbidden */
		return -1;
	}
#ifdef __WINNT__
	{
		DWORD nRd;
		BOOL rc;
		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);
		if( !rc ){
			/* IO error */
			return -1;
		}
		return (ph7_int64)nRd;
	}
#elif defined(__UNIXES__)
	{
		ssize_t nRd;
		int fd;
		fd = SX_PTR_TO_INT(pData->x.pHandle);
		nRd = read(fd,pBuffer,(size_t)nDatatoRead);
		if( nRd < 1 ){
			return -1;
		}
		return (ph7_int64)nRd;
	}
#else
	return -1;
#endif
}
/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */
static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)
{
	ph7_stream_data *pData = (ph7_stream_data *)pHandle;
	if( pData == 0 ){
		return -1;
	}
	if( pData->iType == PH7_IO_STREAM_STDIN ){
		/* Forbidden */
		return -1;
	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){
		ph7_output_consumer *pCons = &pData->x.sConsumer;
		int rc;
		/* Call the vm output consumer */
		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);
		if( rc == PH7_ABORT ){
			return -1;
		}
		return nWrite;
	}
#ifdef __WINNT__
	{
		DWORD nWr;
		BOOL rc;
		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);
		if( !rc ){
			/* IO error */
			return -1;
		}
		return (ph7_int64)nWr;
	}
#elif defined(__UNIXES__)
	{
		ssize_t nWr;
		int fd;
		fd = SX_PTR_TO_INT(pData->x.pHandle);
		nWr = write(fd,pBuf,(size_t)nWrite);
		if( nWr < 1 ){
			return -1;
		}
		return (ph7_int64)nWr;
	}
#else
	return -1;
#endif
}
/* void (*xClose)(void *) */
static void PHPStreamData_Close(void *pHandle)
{
	ph7_stream_data *pData = (ph7_stream_data *)pHandle;
	ph7_vm *pVm;
	if( pData == 0 ){
		return;
	}
	pVm = pData->pVm;
	/* Free the instance */
	SyMemBackendFree(&pVm->sAllocator,pData);
}
/*
 * Pipe stream implementation for popen/pclose.
 * This stream wraps the system's popen/pclose APIs to provide
 * PHP-compatible process I/O functionality.
 */
typedef struct pipe_private pipe_private;
struct pipe_private
{
	FILE *pFile;    /* Pipe file handle from popen */
	ph7_vm *pVm;    /* VM that owns this instance */
	int iMode;      /* Open mode: 'r' for read, 'w' for write */
#ifdef __WINNT__
	HANDLE hProcess; /* Process handle on Windows for proper waiting */
	HANDLE hPipe;    /* Pipe handle (for cleanup) */
#endif
};

#ifdef __WINNT__
#include <Windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
/*
 * Custom Windows popen implementation using CreateProcess.
 * This allows us to properly wait for process completion.
 */
static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)
{
	HANDLE hReadPipe = NULL, hWritePipe = NULL;
	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;
	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;
	SECURITY_ATTRIBUTES sa;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	WCHAR *zWideCmd = NULL;
	FILE *pFile = NULL;
	int fd;
	BOOL bRead = (zMode[0] == 'r');

	/* Set up security attributes for pipe inheritance */
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	/* Create pipes for child process I/O */
	if( bRead ){
		/* Reading from child's stdout */
		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){
			return NULL;
		}
		/* Ensure read handle is not inherited */
		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);
		hReadPipe = hChildStdoutRd;
		*phPipe = hChildStdoutRd;
	}else{
		/* Writing to child's stdin */
		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){
			return NULL;
		}
		/* Ensure write handle is not inherited */
		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);
		hWritePipe = hChildStdinWr;
		*phPipe = hChildStdinWr;
	}

	/* Convert command to wide string */
	{
		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);
		if( nLen <= 0 ){
			goto cleanup_pipes;
		}
		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));
		if( !zWideCmd ){
			goto cleanup_pipes;
		}
		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);
	}

	/* Set up process startup info */
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE; /* Hide console window */
	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;
	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	ZeroMemory(&pi, sizeof(pi));

	/* Create the child process */
	if( !CreateProcessW(
		NULL,           /* Application name */
		zWideCmd,       /* Command line */
		NULL,           /* Process security attributes */
		NULL,           /* Thread security attributes */
		TRUE,           /* Inherit handles */
		CREATE_NO_WINDOW, /* Creation flags - no console window */
		NULL,           /* Environment */
		NULL,           /* Current directory */
		&si,            /* Startup info */
		&pi             /* Process info */
	)){
		goto cleanup_all;
	}

	/* Close handles we don't need in parent */
	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);
	if( hChildStdinRd ) CloseHandle(hChildStdinRd);

	/* Close thread handle (we only need process handle) */
	CloseHandle(pi.hThread);

	/* Store process handle for later waiting */
	*phProcess = pi.hProcess;

	/* Convert OS handle to C file descriptor, then to FILE* */
	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),
	                     bRead ? _O_RDONLY | _O_TEXT : _O_WRONLY | _O_TEXT);
	if( fd == -1 ){
		CloseHandle(pi.hProcess);
		*phProcess = NULL;
		goto cleanup_all;
	}

	pFile = _fdopen(fd, zMode);
	if( !pFile ){
		_close(fd); /* This will also close the underlying handle */
		CloseHandle(pi.hProcess);
		*phProcess = NULL;
		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);
		return NULL;
	}

	HeapFree(GetProcessHeap(), 0, zWideCmd);
	return pFile;

cleanup_all:
	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);
cleanup_pipes:
	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);
	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);
	if( hChildStdinRd ) CloseHandle(hChildStdinRd);
	if( hChildStdinWr ) CloseHandle(hChildStdinWr);
	return NULL;
}

/*
 * Custom Windows pclose implementation that properly waits for process completion.
 */
static int WinPclose(FILE *pFile, HANDLE hProcess)
{
	DWORD dwExitCode = 0;
	int status;

	/* Close the FILE* (this closes the pipe) */
	fclose(pFile);

	if( hProcess ){
		/* Wait for the process to complete */
		WaitForSingleObject(hProcess, INFINITE);

		if( GetExitCodeProcess(hProcess, &dwExitCode) ){
			status = (int)dwExitCode;
		}else{
			status = -1;
		}

		/* Close process handle */
		CloseHandle(hProcess);
	}else{
		status = -1;
	}

	return status;
}
#endif /* __WINNT__ */
/*
 * Open a pipe to a process.
 * This is called internally by popen(), not through the stream device interface.
 */
static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)
{
	pipe_private *pPipe;
	FILE *pFile;
	if( pVm == 0 || zCommand == 0 || zMode == 0 ){
		return 0;
	}
	/* Validate mode - only 'r' or 'w' allowed */
	if( zMode[0] != 'r' && zMode[0] != 'w' ){
		return 0;
	}
	/* Open the pipe using system popen */
#ifdef __WINNT__
	{
		/* Build cmd.exe command wrapper */
		const char *zShellPrefix = "cmd.exe /c \"";
		const char *zShellSuffix = "\"";
		size_t nPrefix = strlen(zShellPrefix);
		size_t nSuffix = strlen(zShellSuffix);
		size_t nCmd = strlen(zCommand);
		size_t nQuotes = 0;
		for (size_t i = 0; i < nCmd; ++i) {
			if (zCommand[i] == '"') nQuotes++;
		}
		size_t nCmdEsc = nCmd + nQuotes;
		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));
		if (zCmdEsc == NULL) {
			return 0;
		}
		/* Escape quotes in command */
		size_t j = 0;
		for (size_t i = 0; i < nCmd; ++i) {
			char ch = zCommand[i];
			if (ch == '"') {
				zCmdEsc[j++] = '^';
				zCmdEsc[j++] = '"';
			} else {
				zCmdEsc[j++] = ch;
			}
		}
		zCmdEsc[j] = '\0';
		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;
		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);
		if (zWinCmd == NULL) {
			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);
			return 0;
		}
		memcpy(zWinCmd, zShellPrefix, nPrefix);
		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);
		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);
		zWinCmd[nTotal - 1] = '\0';
		/* Allocate pipe structure early so we can store handles */
		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));
		if( pPipe == 0 ){
			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);
			SyMemBackendFree(&pVm->sAllocator, zWinCmd);
			return 0;
		}
		/* Use our custom WinPopen that properly tracks the process handle */
		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);
		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);
		SyMemBackendFree(&pVm->sAllocator, zWinCmd);
		if( pFile == 0 ){
			SyMemBackendFree(&pVm->sAllocator, pPipe);
			return 0;
		}
		/* Initialize remaining fields */
		pPipe->pFile = pFile;
		pPipe->pVm = pVm;
		pPipe->iMode = zMode[0];
	}
#elif defined(__UNIXES__) /* Unix */
	pFile = popen(zCommand, zMode);
	if( pFile == 0 ){
		return 0;
	}
	/* Allocate pipe private structure */
	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));
	if( pPipe == 0 ){
		/* Out of memory, close the pipe */
		pclose(pFile);
		return 0;
	}
	/* Initialize the structure */
	pPipe->pFile = pFile;
	pPipe->pVm = pVm;
	pPipe->iMode = zMode[0];
#else /* OS_OTHER: no process pipes on this platform */
	(void)pFile;
	return 0;
#endif
	return pPipe;
}
/*
 * Close a pipe and return the exit status of the process.
 * Returns the exit status, or -1 on error.
 */
static int PipeClose(pipe_private *pPipe)
{
	int status;
	ph7_vm *pVm;
	if( pPipe == 0 || pPipe->pFile == 0 ){
		return -1;
	}
	pVm = pPipe->pVm;
	/* Close the pipe and get exit status */
#ifdef __WINNT__
	/* Use our custom WinPclose that properly waits for process completion */
	status = WinPclose(pPipe->pFile, pPipe->hProcess);
#elif defined(__UNIXES__)
	status = pclose(pPipe->pFile);
	/* On Unix, pclose returns the status from waitpid, need to extract exit code */
	if( status != -1 ){
		if( WIFEXITED(status) ){
			status = WEXITSTATUS(status);
		}else if( WIFSIGNALED(status) ){
			/* Process was killed by a signal - use shell convention: 128 + signal number */
			status = 128 + WTERMSIG(status);
		}else{
			/* Unknown termination reason */
			status = -1;
		}
	}
#else /* OS_OTHER: no process pipes on this platform */
	status = -1;
#endif
	/* Free the structure */
	SyMemBackendFree(&pVm->sAllocator, pPipe);
	return status;
}
/*
 * Pipe stream xClose implementation.
 * Note: This is called by fclose(), not pclose().
 * It closes the pipe but does not return the exit status.
 */
static void PipeStream_Close(void *pHandle)
{
	pipe_private *pPipe = (pipe_private *)pHandle;
	if( pPipe ){
		PipeClose(pPipe);
	}
}
/*
 * Pipe stream xRead implementation.
 */
static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)
{
	pipe_private *pPipe = (pipe_private *)pHandle;
	size_t nRead;
	if( pPipe == 0 || pPipe->pFile == 0 ){
		return -1;
	}
	if( pPipe->iMode != 'r' ){
		/* Cannot read from a write-only pipe */
		return -1;
	}
	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);
	if( nRead == 0 ){
		if( feof(pPipe->pFile) ){
			return 0; /* EOF */
		}
		return -1; /* Error */
	}
	return (ph7_int64)nRead;
}
/*
 * Pipe stream xWrite implementation.
 */
static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)
{
	pipe_private *pPipe = (pipe_private *)pHandle;
	size_t nWritten;
	if( pPipe == 0 || pPipe->pFile == 0 ){
		return -1;
	}
	if( pPipe->iMode != 'w' ){
		/* Cannot write to a read-only pipe */
		return -1;
	}
	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);
	if( nWritten == 0 && nWrite > 0 ){
		return -1; /* Error */
	}
	return (ph7_int64)nWritten;
}
/* Export the pipe:// stream (used internally, not registered as a URI scheme) */
static const ph7_io_stream sPipe_Stream = {
	"pipe",
	PH7_IO_STREAM_VERSION,
	0,  /* xOpen - not used, pipes opened via PipeOpen() */
	0,  /* xOpenDir */
	PipeStream_Close,  /* xClose */
	0,  /* xCloseDir */
	PipeStream_Read,   /* xRead */
	0,  /* xReadDir */
	PipeStream_Write,  /* xWrite */
	0,  /* xSeek */
	0,  /* xLock */
	0,  /* xRewindDir */
	0,  /* xTell */
	0,  /* xTrunc */
	0,  /* xSync */
	0   /* xStat */
};
/*
 * Return TRUE if we are dealing with the pipe:// stream.
 * FALSE otherwise.
 */
static int is_pipe_stream(const ph7_io_stream *pStream)
{
	return pStream == &sPipe_Stream;
}
/*
 * resource popen(string $command, string $mode)
 *  Opens process file pointer.
 * Parameters
 *  $command
 *   The command to execute. Passed to the system shell.
 *  $mode
 *   The mode parameter specifies the type of access you require to the stream.
 *   'r' - Open for reading (read from the command's stdout).
 *   'w' - Open for writing (write to the command's stdin).
 * Return
 *  Returns a file pointer on success, or FALSE on error.
 */
static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCommand, *zMode;
	pipe_private *pPipe;
	io_private *pDev;
	int nCmdLen, nModeLen;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Invalid arguments, return FALSE */
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* Extract the command and mode */
	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);
	zMode = ph7_value_to_string(apArg[1], &nModeLen);
	if( nCmdLen < 1 ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	if( nModeLen < 1 || (zMode[0] != 'r' && zMode[0] != 'w') ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* Open the pipe */
	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);
	if( pPipe == 0 ){
		/* Failed to open pipe */
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* Allocate an io_private instance to wrap the pipe */
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);
	if( pDev == 0 ){
		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");
		PipeClose(pPipe);
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* Initialize the io_private structure */
	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);
	pDev->pHandle = pPipe;
	/* Return the io_private instance as a resource */
	ph7_result_resource(pCtx, pDev);
	return PH7_OK;
}
/*
 * int pclose(resource $handle)
 *  Closes a process file pointer opened by popen() and returns the exit code.
 * Parameters
 *  $handle
 *   The file pointer must be valid, and must have been returned by popen().
 * Return
 *  Returns the termination status of the process that was run, or -1 on error.
 */
static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	pipe_private *pPipe;
	io_private *pDev;
	int status;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments, return -1 */
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");
		ph7_result_int(pCtx, -1);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");
		ph7_result_int(pCtx, -1);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0 || !is_pipe_stream(pStream) ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");
		ph7_result_int(pCtx, -1);
		return PH7_OK;
	}
	/* Get the pipe handle */
	pPipe = (pipe_private *)pDev->pHandle;
	/* Close the pipe and get exit status */
	status = PipeClose(pPipe);
	/* Release the IO private structure */
	ReleaseIOPrivate(pCtx, pDev);
	/* Invalidate the resource handle */
	ph7_value_release(apArg[0]);
	/* Return the exit status */
	ph7_result_int(pCtx, status);
	return PH7_OK;
}
/* Export the php:// stream */
static const ph7_io_stream sPHP_Stream = {
	"php",
	PH7_IO_STREAM_VERSION,
	PHPStreamData_Open,  /* xOpen */
	0,   /* xOpenDir */
	PHPStreamData_Close, /* xClose */
	0,  /* xCloseDir */
	PHPStreamData_Read,  /* xRead */
	0,  /* xReadDir */
	PHPStreamData_Write, /* xWrite */
	0,  /* xSeek */
	0,  /* xLock */
	0,  /* xRewindDir */
	0,  /* xTell */
	0,  /* xTrunc */
	0,  /* xSeek */
	0   /* xStat */
};
#endif /* PH7_DISABLE_DISK_IO */
/*
 * Return TRUE if we are dealing with the php:// stream.
 * FALSE otherwise.
 */
static int is_php_stream(const ph7_io_stream *pStream)
{
#ifndef PH7_DISABLE_DISK_IO
	return pStream == &sPHP_Stream;
#else
	SXUNUSED(pStream); /* cc warning */
	return 0;
#endif /* PH7_DISABLE_DISK_IO */
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */
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
		{"getenv",      PH7_vfs_getenv   },
		{"setenv",      PH7_vfs_putenv   },
		{"putenv",      PH7_vfs_putenv   },
		{"touch",       PH7_vfs_touch    },
		{"link",        PH7_vfs_link     },
		{"symlink",     PH7_vfs_symlink  },
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
		{"fread",     PH7_builtin_fread  },
		{"fgetcsv",   PH7_builtin_fgetcsv},
		{"fgetss",    PH7_builtin_fgetss },
		{"readdir",   PH7_builtin_readdir},
		{"rewinddir", PH7_builtin_rewinddir },
		{"closedir",  PH7_builtin_closedir},
		{"opendir",   PH7_builtin_opendir },
		{"readfile",  PH7_builtin_readfile},
		{"file_get_contents", PH7_builtin_file_get_contents},
		{"file_put_contents", PH7_builtin_file_put_contents},
		{"file",      PH7_builtin_file   },
		{"copy",      PH7_builtin_copy   },
		{"fstat",     PH7_builtin_fstat  },
		{"fwrite",    PH7_builtin_fwrite },
		{"fputs",     PH7_builtin_fwrite },
		{"flock",     PH7_builtin_flock  },
		{"fclose",    PH7_builtin_fclose },
		{"fopen",     PH7_builtin_fopen  },
		{"popen",     PH7_builtin_popen  },
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
		{"fnmatch",     PH7_builtin_fnmatch  },
		/* ZIP processing */
		{"zip_open",    PH7_builtin_zip_open },
		{"zip_close",   PH7_builtin_zip_close},
		{"zip_read",    PH7_builtin_zip_read },
		{"zip_entry_open", PH7_builtin_zip_entry_open },
		{"zip_entry_close",PH7_builtin_zip_entry_close},
		{"zip_entry_name", PH7_builtin_zip_entry_name },
		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },
		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },
		{"zip_entry_read", PH7_builtin_zip_entry_read },
		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},
		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}
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
	if( pFileStream ){
		/* Install the file:// stream */
		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);
	}
#endif /* PH7_DISABLE_DISK_IO */

	return SXRET_OK;
}
/*
 * Export the STDIN handle.
 */
PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)
{
#ifndef PH7_DISABLE_DISK_IO
	if( pVm->pStdin == 0  ){
		io_private *pIn;
		/* Allocate an IO private instance */
		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));
		if( pIn == 0 ){
			return 0;
		}
		InitIOPrivate(pVm,&sPHP_Stream,pIn);
		/* Initialize the handle */
		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);
		/* Install the STDIN stream */
		pVm->pStdin = pIn;
		return pIn;
	}else{
		/* NULL or STDIN */
		return pVm->pStdin;
	}
#else
	SXUNUSED(pVm); /* cc warning */
	return 0;
#endif
}
/*
 * Export the STDOUT handle.
 */
PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)
{
#ifndef PH7_DISABLE_DISK_IO
	if( pVm->pStdout == 0  ){
		io_private *pOut;
		/* Allocate an IO private instance */
		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));
		if( pOut == 0 ){
			return 0;
		}
		InitIOPrivate(pVm,&sPHP_Stream,pOut);
		/* Initialize the handle */
		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);
		/* Install the STDOUT stream */
		pVm->pStdout = pOut;
		return pOut;
	}else{
		/* NULL or STDOUT */
		return pVm->pStdout;
	}
#else
	SXUNUSED(pVm); /* cc warning */
	return 0;
#endif
}
/*
 * Export the STDERR handle.
 */
PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)
{
#ifndef PH7_DISABLE_DISK_IO
	if( pVm->pStderr == 0  ){
		io_private *pErr;
		/* Allocate an IO private instance */
		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));
		if( pErr == 0 ){
			return 0;
		}
		InitIOPrivate(pVm,&sPHP_Stream,pErr);
		/* Initialize the handle */
		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);
		/* Install the STDERR stream */
		pVm->pStderr = pErr;
		return pErr;
	}else{
		/* NULL or STDERR */
		return pVm->pStderr;
	}
#else
	SXUNUSED(pVm); /* cc warning */
	return 0;
#endif
}
