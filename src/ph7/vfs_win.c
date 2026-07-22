/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifdef __WINNT__
/*
 * Windows VFS implementation for the PH7 engine.
 * Status:
 *    Stable.
 */
/* What follows here is code that is specific to windows systems. */
#include <Windows.h>
#include <stdio.h> /* For popen/pclose pipe stream support */
#include <io.h>    /* For _open_osfhandle, _close */
#include <fcntl.h> /* For _O_RDONLY, _O_WRONLY, _O_TEXT */
#include <sys/stat.h> /* For _wchmod and the _S_IREAD/_S_IWRITE mode bits */
#include <errno.h> /* For mapping GetLastError() to a POSIX errno */
/* SPDX-SnippetBegin */
/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */
/* SPDX-License-Identifier: blessing */
/*
** Convert a UTF-8 string to microsoft unicode (UTF-16?).
**
** Space to hold the returned string is obtained from HeapAlloc().
** Taken from the sqlite3 source tree
** status: Public Domain
*/
static WCHAR *utf8ToUnicode(const char *zFilename){
  int nChar;
  WCHAR *zWideFilename;

  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, 0, 0);
  zWideFilename = (WCHAR *)HeapAlloc(GetProcessHeap(),0,nChar*sizeof(zWideFilename[0]));
  if( zWideFilename == 0 ){
 	return 0;
  }
  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, zWideFilename, nChar);
  if( nChar==0 ){
    HeapFree(GetProcessHeap(),0,zWideFilename);
    return 0;
  }
  return zWideFilename;
}
/*
** Convert a UTF-8 filename into whatever form the underlying
** operating system wants filenames in.Space to hold the result
** is obtained from HeapAlloc() and must be freed by the calling
** function.
** Taken from the sqlite3 source tree
** status: Public Domain
*/
static void *convertUtf8Filename(const char *zFilename){
  void *zConverted;
  zConverted = utf8ToUnicode(zFilename);
  return zConverted;
}
/*
** Convert microsoft unicode to UTF-8.  Space to hold the returned string is
** obtained from HeapAlloc().
** Taken from the sqlite3 source tree
** status: Public Domain
*/
static char *unicodeToUtf8(const WCHAR *zWideFilename){
  char *zFilename;
  int nByte;

  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, 0, 0, 0, 0);
  zFilename = (char *)HeapAlloc(GetProcessHeap(),0,nByte);
  if( zFilename == 0 ){
  	return 0;
  }
  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, zFilename, nByte,0, 0);
  if( nByte == 0 ){
    HeapFree(GetProcessHeap(),0,zFilename);
    return 0;
  }
  return zFilename;
}
/* SPDX-SnippetEnd */
/* Map the most recent Win32 error to a POSIX errno, so the shared IO-failure
 * reporting (which formats strerror(errno), php-style) yields real text on
 * Windows — the Win32 API sets GetLastError() rather than errno. Call this right
 * after the failing API, before any HeapFree/CloseHandle that could reset it. */
static void WinVfsMapErrno(void)
{
	switch( GetLastError() ){
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
		case ERROR_INVALID_NAME:      errno = ENOENT; break;
		case ERROR_ACCESS_DENIED:
		case ERROR_SHARING_VIOLATION:
		case ERROR_LOCK_VIOLATION:    errno = EACCES; break;
		case ERROR_FILE_EXISTS:
		case ERROR_ALREADY_EXISTS:    errno = EEXIST; break;
		case ERROR_DIR_NOT_EMPTY:     errno = ENOTEMPTY; break;
		default:                      errno = EIO; break;
	}
}
/* php's file:// is the default local-file wrapper: a stat-family builtin given
 * "file://[authority]/path" acts on the plain path. On Windows php also accepts
 * a drive path after the scheme, so strip "file://", an optional "localhost"
 * authority, and a slash sitting in front of a "X:" drive (file:///C:/x). The
 * native calls do not understand the scheme; anything unrecognised is returned
 * intact so the call fails exactly as php does. */
static const char * WinVfsLocalPath(const char *zPath)
{
	const char *zRest;
	if( zPath == 0 || SyStrnicmp(zPath,"file://",sizeof("file://")-1) != 0 ){
		return zPath;
	}
	zRest = &zPath[sizeof("file://")-1];
	if( SyStrnicmp(zRest,"localhost/",sizeof("localhost/")-1) == 0 ){
		zRest = &zRest[sizeof("localhost")-1]; /* keep the leading slash */
	}
	if( zRest[0] == '/' && zRest[1] != 0 && zRest[2] == ':' ){
		/* file:///C:/path or file://localhost/C:/path -> C:/path */
		return &zRest[1];
	}
	return zRest;
}
/* int (*xchdir)(const char *) */
static int WinVfs_chdir(const char *zPath)
{
	void * pConverted;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	rc = SetCurrentDirectoryW((LPCWSTR)pConverted);
	if( !rc ){ WinVfsMapErrno(); }
	HeapFree(GetProcessHeap(),0,pConverted);
	return rc ? PH7_OK : -1;
}
/* int (*xGetcwd)(ph7_context *) */
static int WinVfs_getcwd(ph7_context *pCtx)
{
	WCHAR zDir[2048];
	char *zConverted;
	DWORD rc;
	/* Get the current directory */
	rc = GetCurrentDirectoryW(sizeof(zDir),zDir);
	if( rc < 1 ){
		return -1;
	}
	zConverted = unicodeToUtf8(zDir);
	if( zConverted == 0 ){
		return -1;
	}
	ph7_result_string(pCtx,zConverted,-1/*Compute length automatically*/); /* Will make it's own copy */
	HeapFree(GetProcessHeap(),0,zConverted);
	return PH7_OK;
}
/* int (*xMkdir)(const char *,int,int) */
static int WinVfs_mkdir(const char *zPath,int mode,int recursive)
{
	void * pConverted;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	mode= 0; /* MSVC warning */
	recursive = 0;
	rc = CreateDirectoryW((LPCWSTR)pConverted,0);
	if( !rc ){ WinVfsMapErrno(); }
	HeapFree(GetProcessHeap(),0,pConverted);
	return rc ? PH7_OK : -1;
}
/* int (*xRmdir)(const char *) */
static int WinVfs_rmdir(const char *zPath)
{
	void * pConverted;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	rc = RemoveDirectoryW((LPCWSTR)pConverted);
	if( !rc ){ WinVfsMapErrno(); }
	HeapFree(GetProcessHeap(),0,pConverted);
	return rc ? PH7_OK : -1;
}
/* int (*xIsdir)(const char *) */
static int WinVfs_isdir(const char *zPath)
{
	zPath = WinVfsLocalPath(zPath);
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( dwAttr == INVALID_FILE_ATTRIBUTES ){
		return -1;
	}
	return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? PH7_OK : -1;
}
/* int (*xRename)(const char *,const char *) */
static int WinVfs_Rename(const char *zOld,const char *zNew)
{
	void *pOld,*pNew;
	BOOL rc = 0;
	pOld = convertUtf8Filename(zOld);
	if( pOld == 0 ){
		return -1;
	}
	pNew = convertUtf8Filename(zNew);
	if( pNew  ){
		rc = MoveFileW((LPCWSTR)pOld,(LPCWSTR)pNew);
	}
	if( !rc ){ WinVfsMapErrno(); }
	HeapFree(GetProcessHeap(),0,pOld);
	if( pNew ){
		HeapFree(GetProcessHeap(),0,pNew);
	}
	return rc ? PH7_OK : - 1;
}
/* int (*xRealpath)(const char *,ph7_context *) */
static int WinVfs_Realpath(const char *zPath,ph7_context *pCtx)
{
	WCHAR zTemp[2048];
	void *pPath;
	char *zReal;
	DWORD n;
	pPath = convertUtf8Filename(zPath);
	if( pPath == 0 ){
		return -1;
	}
	n = GetFullPathNameW((LPCWSTR)pPath,0,0,0);
	if( n > 0 ){
		if( n >= sizeof(zTemp) ){
			n = sizeof(zTemp) - 1;
		}
		GetFullPathNameW((LPCWSTR)pPath,n,zTemp,0);
	}
	HeapFree(GetProcessHeap(),0,pPath);
	if( !n ){
		return -1;
	}
	zReal = unicodeToUtf8(zTemp);
	if( zReal == 0 ){
		return -1;
	}
	ph7_result_string(pCtx,zReal,-1); /* Will make it's own copy */
	HeapFree(GetProcessHeap(),0,zReal);
	return PH7_OK;
}
/* int (*xSleep)(unsigned int) */
static int WinVfs_Sleep(unsigned int uSec)
{
	Sleep(uSec/1000/*uSec per Millisec */);
	return PH7_OK;
}
/* int (*xUnlink)(const char *) */
static int WinVfs_unlink(const char *zPath)
{
	void * pConverted;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	rc = DeleteFileW((LPCWSTR)pConverted);
	if( !rc ){ WinVfsMapErrno(); }
	HeapFree(GetProcessHeap(),0,pConverted);
	return rc ? PH7_OK : - 1;
}
/* int (*xChmod)(const char *,int) */
static int WinVfs_chmod(const char *zPath,int mode)
{
	void * pConverted;
	int rc;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	/* Windows honors only the read-only attribute: a set owner-write bit (0200)
	 * clears it, otherwise the file is made read-only. This mirrors php, whose
	 * chmod() on Windows likewise maps through _wchmod and returns success. */
	rc = _wchmod((const wchar_t *)pConverted,(mode & 0200) ? (_S_IREAD|_S_IWRITE) : _S_IREAD);
	HeapFree(GetProcessHeap(),0,pConverted);
	return rc == 0 ? PH7_OK : - 1;
}
/* ph7_int64 (*xFreeSpace)(const char *) */
static ph7_int64 WinVfs_DiskFreeSpace(const char *zPath)
{
#ifdef _WIN32_WCE
	/* GetDiskFreeSpace is not supported under WINCE */
	SXUNUSED(zPath);
	return 0;
#else
	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;
	void * pConverted;
	WCHAR *p;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return 0;
	}
	p = (WCHAR *)pConverted;
	for(;*p;p++){
		if( *p == '\\' || *p == '/'){
			*p = '\0';
			break;
		}
	}
	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);
	if( !rc ){
		return 0;
	}
	return (ph7_int64)dwFreeClusters * dwSectPerClust * dwBytesPerSect;
#endif
}
/* ph7_int64 (*xTotalSpace)(const char *) */
static ph7_int64 WinVfs_DiskTotalSpace(const char *zPath)
{
#ifdef _WIN32_WCE
	/* GetDiskFreeSpace is not supported under WINCE */
	SXUNUSED(zPath);
	return 0;
#else
	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;
	void * pConverted;
	WCHAR *p;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return 0;
	}
	p = (WCHAR *)pConverted;
	for(;*p;p++){
		if( *p == '\\' || *p == '/'){
			*p = '\0';
			break;
		}
	}
	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);
	if( !rc ){
		return 0;
	}
	return (ph7_int64)dwTotalClusters * dwSectPerClust * dwBytesPerSect;
#endif
}
/* int (*xFileExists)(const char *) */
static int WinVfs_FileExists(const char *zPath)
{
	zPath = WinVfsLocalPath(zPath);
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( dwAttr == INVALID_FILE_ATTRIBUTES ){
		return -1;
	}
	return PH7_OK;
}
/* Open a file in a read-only mode */
static HANDLE OpenReadOnly(LPCWSTR pPath)
{
	DWORD dwType = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS;
	DWORD dwShare = FILE_SHARE_READ | FILE_SHARE_WRITE;
	DWORD dwAccess = GENERIC_READ;
	DWORD dwCreate = OPEN_EXISTING;
	HANDLE pHandle;
	pHandle = CreateFileW(pPath,dwAccess,dwShare,0,dwCreate,dwType,0);
	if( pHandle == INVALID_HANDLE_VALUE){
		return 0;
	}
	return pHandle;
}
/* ph7_int64 (*xFileSize)(const char *) */
static ph7_int64 WinVfs_FileSize(const char *zPath)
{
	zPath = WinVfsLocalPath(zPath);
	DWORD dwLow,dwHigh;
	void * pConverted;
	ph7_int64 nSize;
	HANDLE pHandle;

	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	/* Open the file in read-only mode */
	pHandle = OpenReadOnly((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( pHandle ){
		dwLow = GetFileSize(pHandle,&dwHigh);
		nSize = dwHigh;
		nSize <<= 32;
		nSize += dwLow;
		CloseHandle(pHandle);
	}else{
		nSize = -1;
	}
	return nSize;
}
#define TICKS_PER_SECOND 10000000
#define EPOCH_DIFFERENCE 11644473600LL
/* Convert Windows timestamp to UNIX timestamp */
static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)
{
    ph7_int64 input,temp;
	input = pTime->dwHighDateTime;
	input <<= 32;
	input += pTime->dwLowDateTime;
    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/
    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/
    return temp;
}
/* Convert UNIX timestamp to Windows timestamp */
static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)
{
  ph7_int64 result = EPOCH_DIFFERENCE;
  result += nUnixtime;
  result *= 10000000LL;
  pOut->dwHighDateTime = (DWORD)(nUnixtime>>32);
  pOut->dwLowDateTime = (DWORD)nUnixtime;
}
/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */
static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)
{
	FILETIME sTouch,sAccess;
	void *pConverted;
	void *pHandle;
	BOOL rc = 0;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	pHandle = OpenReadOnly((LPCWSTR)pConverted);
	if( pHandle ){
		if( touch_time < 0 ){
			GetSystemTimeAsFileTime(&sTouch);
		}else{
			convertUnixTimeToWindowsTime(touch_time,&sTouch);
		}
		if( access_time < 0 ){
			/* Use the touch time */
			sAccess = sTouch; /* Structure assignment */
		}else{
			convertUnixTimeToWindowsTime(access_time,&sAccess);
		}
		rc = SetFileTime(pHandle,&sTouch,&sAccess,0);
		/* Close the handle */
		CloseHandle(pHandle);
	}
	HeapFree(GetProcessHeap(),0,pConverted);
	return rc ? PH7_OK : -1;
}
/* ph7_int64 (*xFileAtime)(const char *) */
static ph7_int64 WinVfs_FileAtime(const char *zPath)
{
	zPath = WinVfsLocalPath(zPath);
	BY_HANDLE_FILE_INFORMATION sInfo;
	void * pConverted;
	ph7_int64 atime;
	HANDLE pHandle;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	/* Open the file in read-only mode */
	pHandle = OpenReadOnly((LPCWSTR)pConverted);
	if( pHandle ){
		BOOL rc;
		rc = GetFileInformationByHandle(pHandle,&sInfo);
		if( rc ){
			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);
		}else{
			atime = -1;
		}
		CloseHandle(pHandle);
	}else{
		atime = -1;
	}
	HeapFree(GetProcessHeap(),0,pConverted);
	return atime;
}
/* ph7_int64 (*xFileMtime)(const char *) */
static ph7_int64 WinVfs_FileMtime(const char *zPath)
{
	zPath = WinVfsLocalPath(zPath);
	BY_HANDLE_FILE_INFORMATION sInfo;
	void * pConverted;
	ph7_int64 mtime;
	HANDLE pHandle;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	/* Open the file in read-only mode */
	pHandle = OpenReadOnly((LPCWSTR)pConverted);
	if( pHandle ){
		BOOL rc;
		rc = GetFileInformationByHandle(pHandle,&sInfo);
		if( rc ){
			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);
		}else{
			mtime = -1;
		}
		CloseHandle(pHandle);
	}else{
		mtime = -1;
	}
	HeapFree(GetProcessHeap(),0,pConverted);
	return mtime;
}
/* ph7_int64 (*xFileCtime)(const char *) */
static ph7_int64 WinVfs_FileCtime(const char *zPath)
{
	zPath = WinVfsLocalPath(zPath);
	BY_HANDLE_FILE_INFORMATION sInfo;
	void * pConverted;
	ph7_int64 ctime;
	HANDLE pHandle;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	/* Open the file in read-only mode */
	pHandle = OpenReadOnly((LPCWSTR)pConverted);
	if( pHandle ){
		BOOL rc;
		rc = GetFileInformationByHandle(pHandle,&sInfo);
		if( rc ){
			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);
		}else{
			ctime = -1;
		}
		CloseHandle(pHandle);
	}else{
		ctime = -1;
	}
	HeapFree(GetProcessHeap(),0,pConverted);
	return ctime;
}
/* int (*xStat)(const char *,ph7_value *,ph7_value *) */
/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */
static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)
{
	zPath = WinVfsLocalPath(zPath);
	BY_HANDLE_FILE_INFORMATION sInfo;
	void *pConverted;
	HANDLE pHandle;
	BOOL rc;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	/* Open the file in read-only mode */
	pHandle = OpenReadOnly((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( pHandle == 0 ){
		return -1;
	}
	rc = GetFileInformationByHandle(pHandle,&sInfo);
	CloseHandle(pHandle);
	if( !rc ){
		return -1;
	}
	/* dev */
	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);
	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */
	/* ino */
	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) | sInfo.nFileIndexLow));
	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */
	/* mode */
	ph7_value_int(pWorker,0);
	ph7_array_add_strkey_elem(pArray,"mode",pWorker);
	/* nlink */
	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);
	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */
	/* uid,gid,rdev */
	ph7_value_int(pWorker,0);
	ph7_array_add_strkey_elem(pArray,"uid",pWorker);
	ph7_array_add_strkey_elem(pArray,"gid",pWorker);
	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);
	/* size */
	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) | sInfo.nFileSizeLow));
	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */
	/* atime */
	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));
	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */
	/* mtime */
	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));
	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */
	/* ctime */
	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));
	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */
	/* blksize,blocks */
	ph7_value_int(pWorker,0);
	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);
	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);
	return PH7_OK;
}
/* int (*xIsfile)(const char *) */
static int WinVfs_isfile(const char *zPath)
{
	zPath = WinVfsLocalPath(zPath);
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( dwAttr == INVALID_FILE_ATTRIBUTES ){
		return -1;
	}
	return (dwAttr & (FILE_ATTRIBUTE_NORMAL|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;
}
/* int (*xIslink)(const char *) */
static int WinVfs_islink(const char *zPath)
{
	zPath = WinVfsLocalPath(zPath);
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( dwAttr == INVALID_FILE_ATTRIBUTES ){
		return -1;
	}
	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;
}
/* int (*xWritable)(const char *) */
static int WinVfs_iswritable(const char *zPath)
{
	zPath = WinVfsLocalPath(zPath);
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( dwAttr == INVALID_FILE_ATTRIBUTES ){
		return -1;
	}
	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_NORMAL)) == 0 ){
		/* Not a regular file */
		return -1;
	}
	if( dwAttr & FILE_ATTRIBUTE_READONLY ){
		/* Read-only file */
		return -1;
	}
	/* File is writable */
	return PH7_OK;
}
/* int (*xExecutable)(const char *) */
static int WinVfs_isexecutable(const char *zPath)
{
	zPath = WinVfsLocalPath(zPath);
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( dwAttr == INVALID_FILE_ATTRIBUTES ){
		return -1;
	}
	if( (dwAttr & FILE_ATTRIBUTE_NORMAL) == 0 ){
		/* Not a regular file */
		return -1;
	}
	/* File is executable */
	return PH7_OK;
}
/* int (*xFiletype)(const char *,ph7_context *) */
static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)
{
	zPath = WinVfsLocalPath(zPath);
	void * pConverted;
	DWORD dwAttr;
	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		/* Expand 'unknown' */
		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);
		return -1;
	}
	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( dwAttr == INVALID_FILE_ATTRIBUTES ){
		/* Expand 'unknown' */
		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);
		return -1;
	}
	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_NORMAL|FILE_ATTRIBUTE_ARCHIVE) ){
		ph7_result_string(pCtx,"file",sizeof("file")-1);
	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){
		ph7_result_string(pCtx,"dir",sizeof("dir")-1);
	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){
		ph7_result_string(pCtx,"link",sizeof("link")-1);
	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){
		ph7_result_string(pCtx,"block",sizeof("block")-1);
	}else{
		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);
	}
	return PH7_OK;
}
/* int (*xGetenv)(const char *,ph7_context *) */
static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)
{
	char zValue[1024];
	DWORD n;
	/*
	 * According to MSDN
	 * If lpBuffer is not large enough to hold the data, the return
	 * value is the buffer size, in characters, required to hold the
	 * string and its terminating null character and the contents
	 * of lpBuffer are undefined.
	 */
	n = sizeof(zValue);
	SyMemcpy("Undefined",zValue,sizeof("Undefined")-1);
	/* Extract the environment value */
	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));
	if( !n ){
		/* No such variable*/
		return -1;
	}
	ph7_result_string(pCtx,zValue,(int)n);
	return PH7_OK;
}
/* int (*xSetenv)(const char *,const char *) */
static int WinVfs_Setenv(const char *zName,const char *zValue)
{
	BOOL rc;
	rc = SetEnvironmentVariableA(zName,zValue);
	return rc ? PH7_OK : -1;
}
/* int (*xMmap)(const char *,void **,ph7_int64 *) */
static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)
{
	DWORD dwSizeLow,dwSizeHigh;
	HANDLE pHandle,pMapHandle;
	void *pConverted,*pView;

	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	pHandle = OpenReadOnly((LPCWSTR)pConverted);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( pHandle == 0 ){
		return -1;
	}
	/* Get the file size */
	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);
	/* Create the mapping */
	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);
	if( pMapHandle == 0 ){
		CloseHandle(pHandle);
		return -1;
	}
	*pSize = ((ph7_int64)dwSizeHigh << 32) | dwSizeLow;
	/* Obtain the view */
	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));
	if( pView ){
		/* Let the upper layer point to the view */
		*ppMap = pView;
	}
	/* Close the handle
	 * According to MSDN it's OK the close the HANDLES.
	 */
	CloseHandle(pMapHandle);
	CloseHandle(pHandle);
	return pView ? PH7_OK : -1;
}
/* void (*xUnmap)(void *,ph7_int64)  */
static void WinVfs_Unmap(void *pView,ph7_int64 nSize)
{
	nSize = 0; /* Compiler warning */
	UnmapViewOfFile(pView);
}
/* void (*xTempDir)(ph7_context *) */
static void WinVfs_TempDir(ph7_context *pCtx)
{
	CHAR zTemp[1024];
	DWORD n;
	n = GetTempPathA(sizeof(zTemp),zTemp);
	if( n < 1 ){
		/* Assume the default windows temp directory */
		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);
	}else{
		ph7_result_string(pCtx,zTemp,(int)n);
	}
}
/* unsigned int (*xProcessId)(void) */
static unsigned int WinVfs_ProcessId(void)
{
	DWORD nID = 0;
#ifndef __MINGW32__
	nID = GetProcessId(GetCurrentProcess());
#endif /* __MINGW32__ */
	return (unsigned int)nID;
}
/* void (*xUsername)(ph7_context *) */
static void WinVfs_Username(ph7_context *pCtx)
{
	WCHAR zUser[1024];
	DWORD nByte;
	BOOL rc;
	nByte = sizeof(zUser);
	rc = GetUserNameW(zUser,&nByte);
	if( !rc ){
		/* Set a dummy name */
		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);
	}else{
		char *zName;
		zName = unicodeToUtf8(zUser);
		if( zName == 0 ){
			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);
		}else{
			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */
			HeapFree(GetProcessHeap(),0,zName);
		}
	}

}
/* int (*xChroot)(const char *) — Windows has no chroot; fail cleanly so chroot()
 * returns false. (php has no chroot symbol at all; PHL exposes it as an extension
 * and this reports the failure without a "not implemented in the VFS" warning.) */
static int WinVfs_chroot(const char *zPath)
{
	(void)zPath;
	return -1;
}
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif
#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY
#define SYMBOLIC_LINK_FLAG_DIRECTORY 0x1
#endif
/* int (*xLink)(const char *,const char *,int) — hard link (iSym==0) via
 * CreateHardLink or symbolic link (iSym!=0) via CreateSymbolicLink, mirroring
 * php on Windows. Developer-mode symlink creation is allowed. */
static int WinVfs_Link(const char *zOld,const char *zNew,int iSym)
{
	void *pOld, *pNew;
	BOOL rc;
	pOld = convertUtf8Filename(zOld);
	if( pOld == 0 ){
		return -1;
	}
	pNew = convertUtf8Filename(zNew);
	if( pNew == 0 ){
		HeapFree(GetProcessHeap(),0,pOld);
		return -1;
	}
	if( iSym ){
		DWORD dwFlags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
		DWORD attr = GetFileAttributesW((LPCWSTR)pOld);
		if( attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) ){
			dwFlags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
		}
		rc = CreateSymbolicLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,dwFlags) ? TRUE : FALSE;
	}else{
		rc = CreateHardLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,0);
	}
	HeapFree(GetProcessHeap(),0,pNew);
	HeapFree(GetProcessHeap(),0,pOld);
	return rc ? PH7_OK : -1;
}
/* int (*xUmask)(int) — Windows has no umask; php's umask() returns 0 there. */
static int WinVfs_Umask(int iMask)
{
	(void)iMask;
	return 0;
}
/* int (*xUid)(void) / int (*xGid)(void) — no uid/gid on Windows; php's
 * getmyuid()/getmygid() both return 0. */
static int WinVfs_Uid(void)
{
	return 0;
}
static int WinVfs_Gid(void)
{
	return 0;
}
/* Export the windows vfs */
PH7_PRIVATE const ph7_vfs sWinVfs = {
	"Windows_vfs",
	PH7_VFS_VERSION,
	WinVfs_chdir,    /* int (*xChdir)(const char *) */
	WinVfs_chroot,   /* int (*xChroot)(const char *); */
	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */
	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */
	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */
	WinVfs_isdir,    /* int (*xIsdir)(const char *) */
	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */
	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/
	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */
	WinVfs_unlink,   /* int (*xUnlink)(const char *) */
	WinVfs_FileExists, /* int (*xFileExists)(const char *) */
	WinVfs_chmod, /*int (*xChmod)(const char *,int)*/
	0, /*int (*xChown)(const char *,const char *)*/
	0, /*int (*xChgrp)(const char *,const char *)*/
	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */
	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */
	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */
	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */
	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */
	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */
	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */
	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */
	WinVfs_isfile,     /* int (*xIsfile)(const char *) */
	WinVfs_islink,     /* int (*xIslink)(const char *) */
	WinVfs_isfile,     /* int (*xReadable)(const char *) */
	WinVfs_iswritable, /* int (*xWritable)(const char *) */
	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */
	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */
	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */
	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */
	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */
	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */
	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */
	WinVfs_Link,       /* int (*xLink)(const char *,const char *,int) */
	WinVfs_Umask,      /* int (*xUmask)(int) */
	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */
	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */
	WinVfs_Uid, /* int (*xUid)(void) */
	WinVfs_Gid, /* int (*xGid)(void) */
	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */
	0 /* int (*xExec)(const char *,ph7_context *) */
};
/* Windows file IO */
#ifndef INVALID_SET_FILE_POINTER
# define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif
/* int (*xOpen)(const char *,int,ph7_value *,void **) */
static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)
{
	DWORD dwType = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS;
	DWORD dwAccess = GENERIC_READ;
	DWORD dwShare,dwCreate;
	void *pConverted;
	HANDLE pHandle;

	pConverted = convertUtf8Filename(zPath);
	if( pConverted == 0 ){
		return -1;
	}
	/* Set the desired flags according to the open mode */
	if( iOpenMode & PH7_IO_OPEN_CREATE ){
		/* Open existing file, or create if it doesn't exist */
		dwCreate = OPEN_ALWAYS;
		if( iOpenMode & PH7_IO_OPEN_TRUNC ){
			/* If the specified file exists and is writable, the function overwrites the file */
			dwCreate = CREATE_ALWAYS;
		}
	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){
		/* Creates a new file, only if it does not already exist.
		* If the file exists, it fails.
		*/
		dwCreate = CREATE_NEW;
	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){
		/* Opens a file and truncates it so that its size is zero bytes
		 * The file must exist.
		 */
		dwCreate = TRUNCATE_EXISTING;
	}else{
		/* Opens a file, only if it exists. */
		dwCreate = OPEN_EXISTING;
	}
	if( iOpenMode & PH7_IO_OPEN_RDWR ){
		/* Read+Write access */
		dwAccess |= GENERIC_WRITE;
	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){
		/* Write only access */
		dwAccess = GENERIC_WRITE;
	}
	if( iOpenMode & PH7_IO_OPEN_APPEND ){
		/* Append mode */
		dwAccess = FILE_APPEND_DATA;
	}
	if( iOpenMode & PH7_IO_OPEN_TEMP ){
		/* File is temporary */
		dwType = FILE_ATTRIBUTE_TEMPORARY;
	}
	dwShare = FILE_SHARE_READ | FILE_SHARE_WRITE;
	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);
	HeapFree(GetProcessHeap(),0,pConverted);
	if( pHandle == INVALID_HANDLE_VALUE){
		SXUNUSED(pResource); /* MSVC warning */
		return -1;
	}
	/* Make the handle accessible to the upper layer */
	*ppHandle = (void *)pHandle;
	return PH7_OK;
}
/* An instance of the following structure is used to record state information
 * while iterating throw directory entries.
 */
typedef struct WinDir_Info WinDir_Info;
struct WinDir_Info
{
	HANDLE pDirHandle;
	void *pPath;
	WIN32_FIND_DATAW sInfo;
	int rc;
};
/* int (*xOpenDir)(const char *,ph7_value *,void **) */
static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)
{
	WinDir_Info *pDirInfo;
	void *pConverted;
	char *zPrep;
	sxu32 n;
	/* Prepare the path */
	n = SyStrlen(zPath);
	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);
	if( zPrep == 0 ){
		return -1;
	}
	SyMemcpy((const void *)zPath,zPrep,n);
	zPrep[n]   = '\\';
	zPrep[n+1] =  '*';
	zPrep[n+2] = 0;
	pConverted = convertUtf8Filename(zPrep);
	HeapFree(GetProcessHeap(),0,zPrep);
	if( pConverted == 0 ){
		return -1;
	}
	/* Allocate a new instance */
	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));
	if( pDirInfo == 0 ){
		pResource = 0; /* Compiler warning */
		return -1;
	}
	pDirInfo->rc = SXRET_OK;
	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);
	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){
		/* Cannot open directory */
		HeapFree(GetProcessHeap(),0,pConverted);
		HeapFree(GetProcessHeap(),0,pDirInfo);
		return -1;
	}
	/* Save the path */
	pDirInfo->pPath = pConverted;
	/* Save our structure */
	*ppHandle = pDirInfo;
	return PH7_OK;
}
/* void (*xCloseDir)(void *) */
static void WinDir_Close(void *pUserData)
{
	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;
	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){
		FindClose(pDirInfo->pDirHandle);
	}
	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);
	HeapFree(GetProcessHeap(),0,pDirInfo);
}
/* void (*xClose)(void *); */
static void WinFile_Close(void *pUserData)
{
	HANDLE pHandle = (HANDLE)pUserData;
	CloseHandle(pHandle);
}
/* int (*xReadDir)(void *,ph7_context *) */
static int WinDir_Read(void *pUserData,ph7_context *pCtx)
{
	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;
	LPWIN32_FIND_DATAW pData;
	char *zName;
	BOOL rc;
	if( pDirInfo->rc != SXRET_OK ){
		/* No more entry to process */
		return -1;
	}
	pData = &pDirInfo->sInfo;
	/* php parity: readdir()/scandir() include the '.' and '..' entries, so unlike
	 * the historical PH7 behaviour we return them instead of skipping. */
	zName = unicodeToUtf8(pData->cFileName);
	if( zName == 0 ){
		/* Out of memory */
		return -1;
	}
	/* Return the current file name */
	ph7_result_string(pCtx,zName,-1);
	HeapFree(GetProcessHeap(),0,zName);
	/* Point to the next entry */
	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);
	if( !rc ){
		pDirInfo->rc = SXERR_EOF;
	}
	return PH7_OK;
}
/* void (*xRewindDir)(void *) */
static void WinDir_RewindDir(void *pUserData)
{
	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;
	FindClose(pDirInfo->pDirHandle);
	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);
	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){
		pDirInfo->rc = SXERR_EOF;
	}else{
		pDirInfo->rc = SXRET_OK;
	}
}
/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */
static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)
{
	HANDLE pHandle = (HANDLE)pOS;
	DWORD nRd;
	BOOL rc;
	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);
	if( !rc ){
		/* EOF or IO error */
		return -1;
	}
	return (ph7_int64)nRd;
}
/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */
static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)
{
	const char *zData = (const char *)pBuffer;
	HANDLE pHandle = (HANDLE)pOS;
	ph7_int64 nCount;
	DWORD nWr;
	BOOL rc;
	nWr = 0;
	nCount = 0;
	for(;;){
		if( nWrite < 1 ){
			break;
		}
		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);
		if( !rc ){
			/* IO error — surface a POSIX errno for the caller's diagnostic
			 * (e.g. a byte-range lock violation reports EACCES like php). */
			WinVfsMapErrno();
			break;
		}
		nWrite -= nWr;
		nCount += nWr;
		zData += nWr;
	}
	if( nWrite > 0 ){
		return -1;
	}
	return nCount;
}
/* int (*xSeek)(void *,ph7_int64,int) */
static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)
{
	HANDLE pHandle = (HANDLE)pUserData;
	DWORD dwMove,dwNew;
	LONG nHighOfft;
	switch(whence){
	case 1:/*SEEK_CUR*/
		dwMove = FILE_CURRENT;
		break;
	case 2: /* SEEK_END */
		dwMove = FILE_END;
		break;
	case 0: /* SEEK_SET */
	default:
		dwMove = FILE_BEGIN;
		break;
	}
	nHighOfft = (LONG)(iOfft >> 32);
	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);
	if( dwNew == INVALID_SET_FILE_POINTER ){
		return -1;
	}
	return PH7_OK;
}
/* int (*xLock)(void *,int) */
static int WinFile_Lock(void *pUserData,int lock_type)
{
	HANDLE pHandle = (HANDLE)pUserData;
	OVERLAPPED sDummy;
	BOOL rc;
	SyZero(&sDummy,sizeof(sDummy));
	/* Lock/unlock the whole file. php locks the maximal byte range, so the lock
	 * is effective even for an empty or freshly-truncated file — the previous
	 * code locked only GetFileSize() bytes (i.e. nothing for a 0-byte file). */
	if( lock_type < 1 ){
		/* Unlock the file */
		rc = UnlockFileEx(pHandle,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);
	}else{
		DWORD dwFlags = LOCKFILE_FAIL_IMMEDIATELY; /* Shared non-blocking lock by default*/
		/* Lock the file */
		if( lock_type == 1 /* LOCK_EXCL */ ){
			dwFlags |= LOCKFILE_EXCLUSIVE_LOCK;
		}
		rc = LockFileEx(pHandle,dwFlags,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);
	}
	return rc ? PH7_OK : -1 /* Lock error */;
}
/* ph7_int64 (*xTell)(void *) */
static ph7_int64 WinFile_Tell(void *pUserData)
{
	HANDLE pHandle = (HANDLE)pUserData;
	DWORD dwNew;
	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);
	if( dwNew == INVALID_SET_FILE_POINTER ){
		return -1;
	}
	return (ph7_int64)dwNew;
}
/* int (*xTrunc)(void *,ph7_int64) */
static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)
{
	HANDLE pHandle = (HANDLE)pUserData;
	LONG HighOfft;
	DWORD dwNew;
	BOOL rc;
	HighOfft = (LONG)(nOfft >> 32);
	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);
	if( dwNew == INVALID_SET_FILE_POINTER ){
		return -1;
	}
	rc = SetEndOfFile(pHandle);
	return rc ? PH7_OK : -1;
}
/* int (*xSync)(void *); */
static int WinFile_Sync(void *pUserData)
{
	HANDLE pHandle = (HANDLE)pUserData;
	BOOL rc;
	rc = FlushFileBuffers(pHandle);
	return rc ? PH7_OK : - 1;
}
/* int (*xStat)(void *,ph7_value *,ph7_value *) */
static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)
{
	BY_HANDLE_FILE_INFORMATION sInfo;
	HANDLE pHandle = (HANDLE)pUserData;
	BOOL rc;
	rc = GetFileInformationByHandle(pHandle,&sInfo);
	if( !rc ){
		return -1;
	}
	/* dev */
	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);
	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */
	/* ino */
	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) | sInfo.nFileIndexLow));
	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */
	/* mode */
	ph7_value_int(pWorker,0);
	ph7_array_add_strkey_elem(pArray,"mode",pWorker);
	/* nlink */
	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);
	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */
	/* uid,gid,rdev */
	ph7_value_int(pWorker,0);
	ph7_array_add_strkey_elem(pArray,"uid",pWorker);
	ph7_array_add_strkey_elem(pArray,"gid",pWorker);
	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);
	/* size */
	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) | sInfo.nFileSizeLow));
	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */
	/* atime */
	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));
	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */
	/* mtime */
	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));
	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */
	/* ctime */
	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));
	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */
	/* blksize,blocks */
	ph7_value_int(pWorker,0);
	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);
	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);
	return PH7_OK;
}
/* Export the file:// stream */
PH7_PRIVATE const ph7_io_stream sWinFileStream = {
	"file", /* Stream name */
	PH7_IO_STREAM_VERSION,
	WinFile_Open,  /* xOpen */
	WinDir_Open,   /* xOpenDir */
	WinFile_Close, /* xClose */
	WinDir_Close,  /* xCloseDir */
	WinFile_Read,  /* xRead */
	WinDir_Read,   /* xReadDir */
	WinFile_Write, /* xWrite */
	WinFile_Seek,  /* xSeek */
	WinFile_Lock,  /* xLock */
	WinDir_RewindDir, /* xRewindDir */
	WinFile_Tell,  /* xTell */
	WinFile_Trunc, /* xTrunc */
	WinFile_Sync,  /* xSeek */
	WinFile_Stat   /* xStat */
};
#endif /* __WINNT__ */
