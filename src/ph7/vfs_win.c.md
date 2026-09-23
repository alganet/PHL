# src/ph7/vfs_win.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 640/734 lines (87.19%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#ifdef __WINNT__` |
|    - |    8 | `/*` |
|    - |    9 | ` * Windows VFS implementation for the PH7 engine.` |
|    - |   10 | ` * Status:` |
|    - |   11 | ` *    Stable.` |
|    - |   12 | ` */` |
|    - |   13 | `/* What follows here is code that is specific to windows systems. */` |
|    - |   14 | `#include <Windows.h>` |
|    - |   15 | `#include <stdio.h> /* For popen/pclose pipe stream support */` |
|    - |   16 | `#include <io.h>    /* For _open_osfhandle, _close */` |
|    - |   17 | `#include <fcntl.h> /* For _O_RDONLY, _O_WRONLY, _O_TEXT */` |
|    - |   18 | `#include <sys/stat.h> /* For _wchmod and the _S_IREAD/_S_IWRITE mode bits */` |
|    - |   19 | `#include <errno.h> /* For mapping GetLastError() to a POSIX errno */` |
|    - |   20 | `/* SPDX-SnippetBegin */` |
|    - |   21 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|    - |   22 | `/* SPDX-License-Identifier: blessing */` |
|    - |   23 | `/*` |
|    - |   24 | `** Convert a UTF-8 string to microsoft unicode (UTF-16?).` |
|    - |   25 | `**` |
|    - |   26 | `** Space to hold the returned string is obtained from HeapAlloc().` |
|    - |   27 | `** Taken from the sqlite3 source tree` |
|    - |   28 | `** status: Public Domain` |
|    - |   29 | `*/` |
|    5 |   30 | `static WCHAR *utf8ToUnicode(const char *zFilename){` |
|    - |   31 | `  int nChar;` |
|    - |   32 | `  WCHAR *zWideFilename;` |
|    - |   33 |  |
|    5 |   34 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, 0, 0);` |
|    5 |   35 | `  zWideFilename = (WCHAR *)HeapAlloc(GetProcessHeap(),0,nChar*sizeof(zWideFilename[0]));` |
|    5 |   36 | `  if( zWideFilename == 0 ){` |
|  ! 0 |   37 | ` 	return 0;` |
|    - |   38 | `  }` |
|    5 |   39 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, zWideFilename, nChar);` |
|    5 |   40 | `  if( nChar==0 ){` |
|  ! 0 |   41 | `    HeapFree(GetProcessHeap(),0,zWideFilename);` |
|  ! 0 |   42 | `    return 0;` |
|    - |   43 | `  }` |
|    5 |   44 | `  return zWideFilename;` |
|    5 |   45 | `}` |
|    - |   46 | `/*` |
|    - |   47 | `** Convert a UTF-8 filename into whatever form the underlying` |
|    - |   48 | `** operating system wants filenames in.Space to hold the result` |
|    - |   49 | `** is obtained from HeapAlloc() and must be freed by the calling` |
|    - |   50 | `** function.` |
|    - |   51 | `** Taken from the sqlite3 source tree` |
|    - |   52 | `** status: Public Domain` |
|    - |   53 | `*/` |
|    5 |   54 | `static void *convertUtf8Filename(const char *zFilename){` |
|    - |   55 | `  void *zConverted;` |
|    5 |   56 | `  zConverted = utf8ToUnicode(zFilename);` |
|    5 |   57 | `  return zConverted;` |
|    5 |   58 | `}` |
|    - |   59 | `/*` |
|    - |   60 | `** Convert microsoft unicode to UTF-8.  Space to hold the returned string is` |
|    - |   61 | `** obtained from HeapAlloc().` |
|    - |   62 | `** Taken from the sqlite3 source tree` |
|    - |   63 | `** status: Public Domain` |
|    - |   64 | `*/` |
|    5 |   65 | `static char *unicodeToUtf8(const WCHAR *zWideFilename){` |
|    - |   66 | `  char *zFilename;` |
|    - |   67 | `  int nByte;` |
|    - |   68 |  |
|    5 |   69 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, 0, 0, 0, 0);` |
|    5 |   70 | `  zFilename = (char *)HeapAlloc(GetProcessHeap(),0,nByte);` |
|    5 |   71 | `  if( zFilename == 0 ){` |
|  ! 0 |   72 | `  	return 0;` |
|    - |   73 | `  }` |
|    5 |   74 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, zFilename, nByte,0, 0);` |
|    5 |   75 | `  if( nByte == 0 ){` |
|  ! 0 |   76 | `    HeapFree(GetProcessHeap(),0,zFilename);` |
|  ! 0 |   77 | `    return 0;` |
|    - |   78 | `  }` |
|    5 |   79 | `  return zFilename;` |
|    5 |   80 | `}` |
|    - |   81 | `/* SPDX-SnippetEnd */` |
|    - |   82 | `/* Map the most recent Win32 error to a POSIX errno, so the shared IO-failure` |
|    - |   83 | ` * reporting (which formats strerror(errno), php-style) yields real text on` |
|    - |   84 | ` * Windows — the Win32 API sets GetLastError() rather than errno. Call this right` |
|    - |   85 | ` * after the failing API, before any HeapFree/CloseHandle that could reset it. */` |
|    - |   86 | `static void WinVfsMapErrno(void)` |
|    5 |   87 | `{` |
|    5 |   88 | `	switch( GetLastError() ){` |
|    - |   89 | `		case ERROR_FILE_NOT_FOUND:` |
|    - |   90 | `		case ERROR_PATH_NOT_FOUND:` |
|    5 |   91 | `		case ERROR_INVALID_NAME:      errno = ENOENT; break;` |
|    - |   92 | `		case ERROR_ACCESS_DENIED:` |
|    - |   93 | `		case ERROR_SHARING_VIOLATION:` |
|    1 |   94 | `		case ERROR_LOCK_VIOLATION:    errno = EACCES; break;` |
|    - |   95 | `		case ERROR_FILE_EXISTS:` |
|  ! 0 |   96 | `		case ERROR_ALREADY_EXISTS:    errno = EEXIST; break;` |
|  ! 0 |   97 | `		case ERROR_DIR_NOT_EMPTY:     errno = ENOTEMPTY; break;` |
|  ! 0 |   98 | `		default:                      errno = EIO; break;` |
|    - |   99 | `	}` |
|    5 |  100 | `}` |
|    - |  101 | `/* php's file:// is the default local-file wrapper: a stat-family builtin given` |
|    - |  102 | ` * "file://[authority]/path" acts on the plain path. On Windows php also accepts` |
|    - |  103 | ` * a drive path after the scheme, so strip "file://", an optional "localhost"` |
|    - |  104 | ` * authority, and a slash sitting in front of a "X:" drive (file:///C:/x). The` |
|    - |  105 | ` * native calls do not understand the scheme; anything unrecognised is returned` |
|    - |  106 | ` * intact so the call fails exactly as php does. */` |
|    - |  107 | `static const char * WinVfsLocalPath(const char *zPath)` |
|    5 |  108 | `{` |
|    - |  109 | `	const char *zRest;` |
|    5 |  110 | `	if( zPath == 0 \|\| SyStrnicmp(zPath,"file://",sizeof("file://")-1) != 0 ){` |
|    5 |  111 | `		return zPath;` |
|    - |  112 | `	}` |
|    1 |  113 | `	zRest = &zPath[sizeof("file://")-1];` |
|    1 |  114 | `	if( SyStrnicmp(zRest,"localhost/",sizeof("localhost/")-1) == 0 ){` |
|  ! 0 |  115 | `		zRest = &zRest[sizeof("localhost")-1]; /* keep the leading slash */` |
|    - |  116 | `	}` |
|    1 |  117 | `	if( zRest[0] == '/' && zRest[1] != 0 && zRest[2] == ':' ){` |
|    - |  118 | `		/* file:///C:/path or file://localhost/C:/path -> C:/path */` |
|  ! 0 |  119 | `		return &zRest[1];` |
|    - |  120 | `	}` |
|    1 |  121 | `	return zRest;` |
|    5 |  122 | `}` |
|    - |  123 | `/* int (*xchdir)(const char *) */` |
|    - |  124 | `static int WinVfs_chdir(const char *zPath)` |
|    5 |  125 | `{` |
|    - |  126 | `	void * pConverted;` |
|    - |  127 | `	BOOL rc;` |
|    5 |  128 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  129 | `	if( pConverted == 0 ){` |
|  ! 0 |  130 | `		return -1;` |
|    - |  131 | `	}` |
|    5 |  132 | `	rc = SetCurrentDirectoryW((LPCWSTR)pConverted);` |
|    5 |  133 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    5 |  134 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  135 | `	return rc ? PH7_OK : -1;` |
|    5 |  136 | `}` |
|    - |  137 | `/* int (*xGetcwd)(ph7_context *) */` |
|    - |  138 | `static int WinVfs_getcwd(ph7_context *pCtx)` |
|    5 |  139 | `{` |
|    - |  140 | `	WCHAR zDir[2048];` |
|    - |  141 | `	char *zConverted;` |
|    - |  142 | `	DWORD rc;` |
|    - |  143 | `	/* Get the current directory */` |
|    5 |  144 | `	rc = GetCurrentDirectoryW(sizeof(zDir),zDir);` |
|    5 |  145 | `	if( rc < 1 ){` |
|  ! 0 |  146 | `		return -1;` |
|    - |  147 | `	}` |
|    5 |  148 | `	zConverted = unicodeToUtf8(zDir);` |
|    5 |  149 | `	if( zConverted == 0 ){` |
|  ! 0 |  150 | `		return -1;` |
|    - |  151 | `	}` |
|    5 |  152 | `	ph7_result_string(pCtx,zConverted,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    5 |  153 | `	HeapFree(GetProcessHeap(),0,zConverted);` |
|    5 |  154 | `	return PH7_OK;` |
|    5 |  155 | `}` |
|    - |  156 | `/* int (*xMkdir)(const char *,int,int) */` |
|    - |  157 | `static int WinVfs_mkdir(const char *zPath,int mode,int recursive)` |
|    4 |  158 | `{` |
|    - |  159 | `	void * pConverted;` |
|    - |  160 | `	BOOL rc;` |
|    4 |  161 | `	pConverted = convertUtf8Filename(zPath);` |
|    4 |  162 | `	if( pConverted == 0 ){` |
|  ! 0 |  163 | `		return -1;` |
|    - |  164 | `	}` |
|    4 |  165 | `	mode= 0; /* MSVC warning */` |
|    4 |  166 | `	recursive = 0;` |
|    4 |  167 | `	rc = CreateDirectoryW((LPCWSTR)pConverted,0);` |
|    4 |  168 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    4 |  169 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    4 |  170 | `	return rc ? PH7_OK : -1;` |
|    4 |  171 | `}` |
|    - |  172 | `/* int (*xRmdir)(const char *) */` |
|    - |  173 | `static int WinVfs_rmdir(const char *zPath)` |
|    3 |  174 | `{` |
|    - |  175 | `	void * pConverted;` |
|    - |  176 | `	BOOL rc;` |
|    3 |  177 | `	pConverted = convertUtf8Filename(zPath);` |
|    3 |  178 | `	if( pConverted == 0 ){` |
|  ! 0 |  179 | `		return -1;` |
|    - |  180 | `	}` |
|    3 |  181 | `	rc = RemoveDirectoryW((LPCWSTR)pConverted);` |
|    3 |  182 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    3 |  183 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    3 |  184 | `	return rc ? PH7_OK : -1;` |
|    3 |  185 | `}` |
|    - |  186 | `/* int (*xIsdir)(const char *) */` |
|    - |  187 | `static int WinVfs_isdir(const char *zPath)` |
|    5 |  188 | `{` |
|    5 |  189 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  190 | `	void * pConverted;` |
|    - |  191 | `	DWORD dwAttr;` |
|    5 |  192 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  193 | `	if( pConverted == 0 ){` |
|  ! 0 |  194 | `		return -1;` |
|    - |  195 | `	}` |
|    5 |  196 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  197 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  198 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  199 | `		return -1;` |
|    - |  200 | `	}` |
|    5 |  201 | `	return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? PH7_OK : -1;` |
|    5 |  202 | `}` |
|    - |  203 | `/* int (*xRename)(const char *,const char *) */` |
|    - |  204 | `static int WinVfs_Rename(const char *zOld,const char *zNew)` |
|    1 |  205 | `{` |
|    - |  206 | `	void *pOld,*pNew;` |
|    1 |  207 | `	BOOL rc = 0;` |
|    1 |  208 | `	pOld = convertUtf8Filename(zOld);` |
|    1 |  209 | `	if( pOld == 0 ){` |
|  ! 0 |  210 | `		return -1;` |
|    - |  211 | `	}` |
|    1 |  212 | `	pNew = convertUtf8Filename(zNew);` |
|    1 |  213 | `	if( pNew  ){` |
|    1 |  214 | `		rc = MoveFileW((LPCWSTR)pOld,(LPCWSTR)pNew);` |
|    - |  215 | `	}` |
|    1 |  216 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    1 |  217 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|    1 |  218 | `	if( pNew ){` |
|    1 |  219 | `		HeapFree(GetProcessHeap(),0,pNew);` |
|    - |  220 | `	}` |
|    1 |  221 | `	return rc ? PH7_OK : - 1;` |
|    1 |  222 | `}` |
|    - |  223 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|    - |  224 | `static int WinVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|    2 |  225 | `{` |
|    - |  226 | `	WCHAR zTemp[2048];` |
|    - |  227 | `	void *pPath;` |
|    - |  228 | `	char *zReal;` |
|    - |  229 | `	DWORD n;` |
|    2 |  230 | `	pPath = convertUtf8Filename(zPath);` |
|    2 |  231 | `	if( pPath == 0 ){` |
|  ! 0 |  232 | `		return -1;` |
|    - |  233 | `	}` |
|    2 |  234 | `	n = GetFullPathNameW((LPCWSTR)pPath,0,0,0);` |
|    2 |  235 | `	if( n > 0 ){` |
|    2 |  236 | `		if( n >= sizeof(zTemp) ){` |
|  ! 0 |  237 | `			n = sizeof(zTemp) - 1;` |
|    - |  238 | `		}` |
|    2 |  239 | `		GetFullPathNameW((LPCWSTR)pPath,n,zTemp,0);` |
|    - |  240 | `	}` |
|    - |  241 | `	/* GetFullPathNameW only NORMALIZES -- it answers happily for a path that does` |
|    - |  242 | `	 * not exist, and php's realpath() is false for one on every platform. */` |
|    2 |  243 | `	if( n > 0 && GetFileAttributesW((LPCWSTR)pPath) == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  244 | `		n = 0;` |
|    - |  245 | `	}` |
|    2 |  246 | `	HeapFree(GetProcessHeap(),0,pPath);` |
|    2 |  247 | `	if( !n ){` |
|    1 |  248 | `		return -1;` |
|    - |  249 | `	}` |
|    2 |  250 | `	zReal = unicodeToUtf8(zTemp);` |
|    2 |  251 | `	if( zReal == 0 ){` |
|  ! 0 |  252 | `		return -1;` |
|    - |  253 | `	}` |
|    2 |  254 | `	ph7_result_string(pCtx,zReal,-1); /* Will make it's own copy */` |
|    2 |  255 | `	HeapFree(GetProcessHeap(),0,zReal);` |
|    2 |  256 | `	return PH7_OK;` |
|    2 |  257 | `}` |
|    - |  258 | `/* int (*xSleep)(unsigned int) */` |
|    - |  259 | `static int WinVfs_Sleep(unsigned int uSec)` |
|    1 |  260 | `{` |
|    1 |  261 | `	Sleep(uSec/1000/*uSec per Millisec */);` |
|    1 |  262 | `	return PH7_OK;` |
|    1 |  263 | `}` |
|    - |  264 | `/* int (*xUnlink)(const char *) */` |
|    - |  265 | `static int WinVfs_unlink(const char *zPath)` |
|    5 |  266 | `{` |
|    - |  267 | `	void * pConverted;` |
|    - |  268 | `	BOOL rc;` |
|    5 |  269 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  270 | `	if( pConverted == 0 ){` |
|  ! 0 |  271 | `		return -1;` |
|    - |  272 | `	}` |
|    5 |  273 | `	rc = DeleteFileW((LPCWSTR)pConverted);` |
|    5 |  274 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    5 |  275 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  276 | `	return rc ? PH7_OK : - 1;` |
|    5 |  277 | `}` |
|    - |  278 | `/* int (*xChmod)(const char *,int) */` |
|    - |  279 | `static int WinVfs_chmod(const char *zPath,int mode)` |
|    2 |  280 | `{` |
|    - |  281 | `	void * pConverted;` |
|    - |  282 | `	int rc;` |
|    2 |  283 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  284 | `	if( pConverted == 0 ){` |
|  ! 0 |  285 | `		return -1;` |
|    - |  286 | `	}` |
|    - |  287 | `	/* Windows honors only the read-only attribute: a set owner-write bit (0200)` |
|    - |  288 | `	 * clears it, otherwise the file is made read-only. This mirrors php, whose` |
|    - |  289 | `	 * chmod() on Windows likewise maps through _wchmod and returns success. */` |
|    2 |  290 | `	rc = _wchmod((const wchar_t *)pConverted,(mode & 0200) ? (_S_IREAD\|_S_IWRITE) : _S_IREAD);` |
|    2 |  291 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  292 | `	return rc == 0 ? PH7_OK : - 1;` |
|    2 |  293 | `}` |
|    - |  294 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - |  295 | `static ph7_int64 WinVfs_DiskFreeSpace(const char *zPath)` |
|    1 |  296 | `{` |
|    - |  297 | `#ifdef _WIN32_WCE` |
|    - |  298 | `	/* GetDiskFreeSpaceEx is not supported under WINCE */` |
|    - |  299 | `	SXUNUSED(zPath);` |
|    - |  300 | `	return -1;` |
|    - |  301 | `#else` |
|    - |  302 | `	/* php's own call (win32/ioutil + php_disk_free_space): GetDiskFreeSpaceExW on` |
|    - |  303 | `	 * the DIRECTORY it was given, failing when that directory does not exist.` |
|    - |  304 | `	 * PH7 called the older GetDiskFreeSpaceW, which only accepts a volume ROOT, so` |
|    - |  305 | `	 * it first truncated the path at its first separator: every subpath answered` |
|    - |  306 | `	 * for the drive rather than for the volume actually mounted there, and a path` |
|    - |  307 | `	 * that does not exist at all answered the drive's numbers instead of failing.` |
|    - |  308 | `	 * The failure value is -1, the convention the unix VFS already uses and the` |
|    - |  309 | `	 * one disk_free_space() reads as php's FALSE. */` |
|    - |  310 | `	ULARGE_INTEGER uFreeToCaller,uTotal,uTotalFree;` |
|    - |  311 | `	void * pConverted;` |
|    - |  312 | `	BOOL rc;` |
|    1 |  313 | `	pConverted = convertUtf8Filename(WinVfsLocalPath(zPath));` |
|    1 |  314 | `	if( pConverted == 0 ){` |
|  ! 0 |  315 | `		return -1;` |
|    - |  316 | `	}` |
|    1 |  317 | `	rc = GetDiskFreeSpaceExW((LPCWSTR)pConverted,&uFreeToCaller,&uTotal,&uTotalFree);` |
|    1 |  318 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    1 |  319 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  320 | `	if( !rc ){` |
|    1 |  321 | `		return -1;` |
|    - |  322 | `	}` |
|    1 |  323 | `	return (ph7_int64)uFreeToCaller.QuadPart;` |
|    - |  324 | `#endif` |
|    1 |  325 | `}` |
|    - |  326 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - |  327 | `static ph7_int64 WinVfs_DiskTotalSpace(const char *zPath)` |
|    1 |  328 | `{` |
|    - |  329 | `#ifdef _WIN32_WCE` |
|    - |  330 | `	/* GetDiskFreeSpaceEx is not supported under WINCE */` |
|    - |  331 | `	SXUNUSED(zPath);` |
|    - |  332 | `	return -1;` |
|    - |  333 | `#else` |
|    - |  334 | `	/* php's own call (win32/ioutil + php_disk_free_space): GetDiskFreeSpaceExW on` |
|    - |  335 | `	 * the DIRECTORY it was given, failing when that directory does not exist.` |
|    - |  336 | `	 * PH7 called the older GetDiskFreeSpaceW, which only accepts a volume ROOT, so` |
|    - |  337 | `	 * it first truncated the path at its first separator: every subpath answered` |
|    - |  338 | `	 * for the drive rather than for the volume actually mounted there, and a path` |
|    - |  339 | `	 * that does not exist at all answered the drive's numbers instead of failing.` |
|    - |  340 | `	 * The failure value is -1, the convention the unix VFS already uses and the` |
|    - |  341 | `	 * one disk_free_space() reads as php's FALSE. */` |
|    - |  342 | `	ULARGE_INTEGER uFreeToCaller,uTotal,uTotalFree;` |
|    - |  343 | `	void * pConverted;` |
|    - |  344 | `	BOOL rc;` |
|    1 |  345 | `	pConverted = convertUtf8Filename(WinVfsLocalPath(zPath));` |
|    1 |  346 | `	if( pConverted == 0 ){` |
|  ! 0 |  347 | `		return -1;` |
|    - |  348 | `	}` |
|    1 |  349 | `	rc = GetDiskFreeSpaceExW((LPCWSTR)pConverted,&uFreeToCaller,&uTotal,&uTotalFree);` |
|    1 |  350 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    1 |  351 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  352 | `	if( !rc ){` |
|    1 |  353 | `		return -1;` |
|    - |  354 | `	}` |
|    1 |  355 | `	return (ph7_int64)uTotal.QuadPart;` |
|    - |  356 | `#endif` |
|    1 |  357 | `}` |
|    - |  358 | `/* int (*xFileExists)(const char *) */` |
|    - |  359 | `static int WinVfs_FileExists(const char *zPath)` |
|    5 |  360 | `{` |
|    5 |  361 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  362 | `	void * pConverted;` |
|    - |  363 | `	DWORD dwAttr;` |
|    5 |  364 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  365 | `	if( pConverted == 0 ){` |
|  ! 0 |  366 | `		return -1;` |
|    - |  367 | `	}` |
|    5 |  368 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  369 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  370 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    4 |  371 | `		return -1;` |
|    - |  372 | `	}` |
|    5 |  373 | `	return PH7_OK;` |
|    5 |  374 | `}` |
|    - |  375 | `/* Open a path for a STAT-family read. Same as OpenReadOnly but with` |
|    - |  376 | ` * FILE_FLAG_BACKUP_SEMANTICS, which is the only way CreateFileW will open a` |
|    - |  377 | ` * DIRECTORY — without it filemtime()/fileatime()/filectime() answered -1 for every` |
|    - |  378 | ` * directory on Windows (a plain read-only open fails with ERROR_ACCESS_DENIED),` |
|    - |  379 | ` * where php reports the timestamp. FILE_READ_ATTRIBUTES is all` |
|    - |  380 | ` * GetFileInformationByHandle needs, so this also works on a file another process` |
|    - |  381 | ` * holds open for writing. */` |
|    - |  382 | `static HANDLE OpenForStat(LPCWSTR pPath)` |
|    1 |  383 | `{` |
|    - |  384 | `	HANDLE pHandle;` |
|    1 |  385 | `	pHandle = CreateFileW(pPath,FILE_READ_ATTRIBUTES,` |
|    - |  386 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_EXISTING,` |
|    - |  387 | `		FILE_ATTRIBUTE_NORMAL\|FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    1 |  388 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|    1 |  389 | `		return 0;` |
|    - |  390 | `	}` |
|    1 |  391 | `	return pHandle;` |
|    1 |  392 | `}` |
|    - |  393 | `/* Open a file in a read-only mode */` |
|    - |  394 | `static HANDLE OpenReadOnly(LPCWSTR pPath)` |
|    5 |  395 | `{` |
|    5 |  396 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    5 |  397 | `	DWORD dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    5 |  398 | `	DWORD dwAccess = GENERIC_READ;` |
|    5 |  399 | `	DWORD dwCreate = OPEN_EXISTING;` |
|    - |  400 | `	HANDLE pHandle;` |
|    5 |  401 | `	pHandle = CreateFileW(pPath,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    5 |  402 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    1 |  403 | `		return 0;` |
|    - |  404 | `	}` |
|    5 |  405 | `	return pHandle;` |
|    5 |  406 | `}` |
|    - |  407 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  408 | `static ph7_int64 WinVfs_FileSize(const char *zPath)` |
|    1 |  409 | `{` |
|    1 |  410 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  411 | `	DWORD dwLow,dwHigh;` |
|    - |  412 | `	void * pConverted;` |
|    - |  413 | `	ph7_int64 nSize;` |
|    - |  414 | `	HANDLE pHandle;` |
|    - |  415 |  |
|    1 |  416 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  417 | `	if( pConverted == 0 ){` |
|  ! 0 |  418 | `		return -1;` |
|    - |  419 | `	}` |
|    - |  420 | `	/* Open the file in read-only mode */` |
|    1 |  421 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  422 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  423 | `	if( pHandle ){` |
|    1 |  424 | `		dwLow = GetFileSize(pHandle,&dwHigh);` |
|    1 |  425 | `		nSize = dwHigh;` |
|    1 |  426 | `		nSize <<= 32;` |
|    1 |  427 | `		nSize += dwLow;` |
|    1 |  428 | `		CloseHandle(pHandle);` |
|    1 |  429 | `	}else{` |
|    1 |  430 | `		nSize = -1;` |
|    - |  431 | `	}` |
|    1 |  432 | `	return nSize;` |
|    1 |  433 | `}` |
|    - |  434 | `#define TICKS_PER_SECOND 10000000` |
|    - |  435 | `#define EPOCH_DIFFERENCE 11644473600LL` |
|    - |  436 | `/* Convert Windows timestamp to UNIX timestamp */` |
|    - |  437 | `static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)` |
|    1 |  438 | `{` |
|    - |  439 | `    ph7_int64 input,temp;` |
|    1 |  440 | `	input = pTime->dwHighDateTime;` |
|    1 |  441 | `	input <<= 32;` |
|    1 |  442 | `	input += pTime->dwLowDateTime;` |
|    1 |  443 | `    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/` |
|    1 |  444 | `    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/` |
|    1 |  445 | `    return temp;` |
|    1 |  446 | `}` |
|    - |  447 | `/* Convert UNIX timestamp to Windows timestamp */` |
|    - |  448 | `static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)` |
|    1 |  449 | `{` |
|    - |  450 | ``  /* The converted value has to be the one that is STORED: this computed `result` and`` |
|    - |  451 | `   * then wrote the raw unix seconds into the FILETIME, so a requested 1000000000` |
|    - |  452 | `   * landed as 100 seconds past the 1601 epoch and read back as -11644473500. */` |
|    1 |  453 | `  ph7_int64 result = EPOCH_DIFFERENCE;` |
|    1 |  454 | `  result += nUnixtime;` |
|    1 |  455 | `  result *= TICKS_PER_SECOND;` |
|    1 |  456 | `  pOut->dwHighDateTime = (DWORD)((sxu64)result >> 32);` |
|    1 |  457 | `  pOut->dwLowDateTime = (DWORD)((sxu64)result & 0xFFFFFFFFu);` |
|    1 |  458 | `}` |
|    - |  459 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  460 | `static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|    1 |  461 | `{` |
|    - |  462 | `	FILETIME sTouch,sAccess;` |
|    - |  463 | `	void *pConverted;` |
|    - |  464 | `	void *pHandle;` |
|    1 |  465 | `	BOOL rc = 0;` |
|    - |  466 | `	/* Accept file:// like every other path-taking entry in this VFS (the POSIX` |
|    - |  467 | `	 * driver already does) — this was the only one that skipped the mapping. */` |
|    1 |  468 | `	zPath = WinVfsLocalPath(zPath);` |
|    1 |  469 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  470 | `	if( pConverted == 0 ){` |
|  ! 0 |  471 | `		return -1;` |
|    - |  472 | `	}` |
|    - |  473 | `	/* php's touch() CREATES a missing file (OPEN_ALWAYS), and SetFileTime needs write` |
|    - |  474 | `	 * access — the read-only handle this used could not stamp an existing file either.` |
|    - |  475 | `	 * FILE_FLAG_BACKUP_SEMANTICS is what lets a DIRECTORY be opened at all (php's` |
|    - |  476 | `	 * win32 utime passes it for the same reason, and the POSIX driver's utime() works` |
|    - |  477 | `	 * on directories); it does not change what OPEN_ALWAYS creates for a missing path.` |
|    - |  478 | `	 * Mirrors the POSIX driver's utime + open(O_CREAT) fallback. */` |
|    1 |  479 | `	pHandle = CreateFileW((LPCWSTR)pConverted,FILE_WRITE_ATTRIBUTES,` |
|    - |  480 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_ALWAYS,` |
|    - |  481 | `		FILE_ATTRIBUTE_NORMAL\|FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    1 |  482 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 |  483 | `		pHandle = 0;` |
|    - |  484 | `	}` |
|    1 |  485 | `	if( pHandle ){` |
|    - |  486 | `		/* Both stamps are real values: the builtin resolves php's "now" default, so a` |
|    - |  487 | `		 * NEGATIVE timestamp (legal to php) is no longer read as "not given". */` |
|    1 |  488 | `		convertUnixTimeToWindowsTime(touch_time,&sTouch);` |
|    1 |  489 | `		convertUnixTimeToWindowsTime(access_time,&sAccess);` |
|    - |  490 | `		/* SetFileTime(hFile, creation, lastAccess, lastWrite): the modification stamp` |
|    - |  491 | `		 * belongs in the LAST slot. It used to be passed as the CREATION time with` |
|    - |  492 | `		 * lastWrite left NULL, so touch($f, $mtime) changed a stamp nothing reads and` |
|    - |  493 | `		 * left filemtime() reporting whatever the file already had. Creation stays` |
|    - |  494 | `		 * untouched, like php. */` |
|    1 |  495 | `		rc = SetFileTime(pHandle,0,&sAccess,&sTouch);` |
|    - |  496 | `		/* Close the handle */` |
|    1 |  497 | `		CloseHandle(pHandle);` |
|    - |  498 | `	}` |
|    1 |  499 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  500 | `	return rc ? PH7_OK : -1;` |
|    1 |  501 | `}` |
|    - |  502 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  503 | `static ph7_int64 WinVfs_FileAtime(const char *zPath)` |
|    1 |  504 | `{` |
|    1 |  505 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  506 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  507 | `	void * pConverted;` |
|    - |  508 | `	ph7_int64 atime;` |
|    - |  509 | `	HANDLE pHandle;` |
|    1 |  510 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  511 | `	if( pConverted == 0 ){` |
|  ! 0 |  512 | `		return -1;` |
|    - |  513 | `	}` |
|    - |  514 | `	/* Open for a stat read (directories included) */` |
|    1 |  515 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    1 |  516 | `	if( pHandle ){` |
|    - |  517 | `		BOOL rc;` |
|    1 |  518 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  519 | `		if( rc ){` |
|    1 |  520 | `			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);` |
|    1 |  521 | `		}else{` |
|  ! 0 |  522 | `			atime = -1;` |
|    - |  523 | `		}` |
|    1 |  524 | `		CloseHandle(pHandle);` |
|    1 |  525 | `	}else{` |
|    1 |  526 | `		atime = -1;` |
|    - |  527 | `	}` |
|    1 |  528 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  529 | `	return atime;` |
|    1 |  530 | `}` |
|    - |  531 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  532 | `static ph7_int64 WinVfs_FileMtime(const char *zPath)` |
|    1 |  533 | `{` |
|    1 |  534 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  535 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  536 | `	void * pConverted;` |
|    - |  537 | `	ph7_int64 mtime;` |
|    - |  538 | `	HANDLE pHandle;` |
|    1 |  539 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  540 | `	if( pConverted == 0 ){` |
|  ! 0 |  541 | `		return -1;` |
|    - |  542 | `	}` |
|    - |  543 | `	/* Open for a stat read (directories included) */` |
|    1 |  544 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    1 |  545 | `	if( pHandle ){` |
|    - |  546 | `		BOOL rc;` |
|    1 |  547 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  548 | `		if( rc ){` |
|    1 |  549 | `			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);` |
|    1 |  550 | `		}else{` |
|  ! 0 |  551 | `			mtime = -1;` |
|    - |  552 | `		}` |
|    1 |  553 | `		CloseHandle(pHandle);` |
|    1 |  554 | `	}else{` |
|    1 |  555 | `		mtime = -1;` |
|    - |  556 | `	}` |
|    1 |  557 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  558 | `	return mtime;` |
|    1 |  559 | `}` |
|    - |  560 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  561 | `static ph7_int64 WinVfs_FileCtime(const char *zPath)` |
|    1 |  562 | `{` |
|    1 |  563 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  564 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  565 | `	void * pConverted;` |
|    - |  566 | `	ph7_int64 ctime;` |
|    - |  567 | `	HANDLE pHandle;` |
|    1 |  568 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  569 | `	if( pConverted == 0 ){` |
|  ! 0 |  570 | `		return -1;` |
|    - |  571 | `	}` |
|    - |  572 | `	/* Open for a stat read (directories included) */` |
|    1 |  573 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    1 |  574 | `	if( pHandle ){` |
|    - |  575 | `		BOOL rc;` |
|    1 |  576 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  577 | `		if( rc ){` |
|    1 |  578 | `			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);` |
|    1 |  579 | `		}else{` |
|  ! 0 |  580 | `			ctime = -1;` |
|    - |  581 | `		}` |
|    1 |  582 | `		CloseHandle(pHandle);` |
|    1 |  583 | `	}else{` |
|    1 |  584 | `		ctime = -1;` |
|    - |  585 | `	}` |
|    1 |  586 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  587 | `	return ctime;` |
|    1 |  588 | `}` |
|    - |  589 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  590 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  591 | `static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|    1 |  592 | `{` |
|    1 |  593 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  594 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  595 | `	void *pConverted;` |
|    - |  596 | `	HANDLE pHandle;` |
|    - |  597 | `	BOOL rc;` |
|    1 |  598 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  599 | `	if( pConverted == 0 ){` |
|  ! 0 |  600 | `		return -1;` |
|    - |  601 | `	}` |
|    - |  602 | `	/* Open the file in read-only mode */` |
|    1 |  603 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  604 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  605 | `	if( pHandle == 0 ){` |
|    1 |  606 | `		return -1;` |
|    - |  607 | `	}` |
|    1 |  608 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  609 | `	CloseHandle(pHandle);` |
|    1 |  610 | `	if( !rc ){` |
|  ! 0 |  611 | `		return -1;` |
|    - |  612 | `	}` |
|    - |  613 | `	/* dev */` |
|    1 |  614 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 |  615 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - |  616 | `	/* ino */` |
|    1 |  617 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 |  618 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - |  619 | `	/* mode */` |
|    1 |  620 | `	ph7_value_int(pWorker,0);` |
|    1 |  621 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - |  622 | `	/* nlink */` |
|    1 |  623 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 |  624 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - |  625 | `	/* uid,gid,rdev */` |
|    1 |  626 | `	ph7_value_int(pWorker,0);` |
|    1 |  627 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 |  628 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 |  629 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - |  630 | `	/* size */` |
|    1 |  631 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 |  632 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - |  633 | `	/* atime */` |
|    1 |  634 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 |  635 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - |  636 | `	/* mtime */` |
|    1 |  637 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 |  638 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - |  639 | `	/* ctime */` |
|    1 |  640 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 |  641 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - |  642 | `	/* blksize,blocks */` |
|    1 |  643 | `	ph7_value_int(pWorker,0);` |
|    1 |  644 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 |  645 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 |  646 | `	return PH7_OK;` |
|    1 |  647 | `}` |
|    - |  648 | `/* int (*xIsfile)(const char *) */` |
|    - |  649 | `static int WinVfs_isfile(const char *zPath)` |
|    5 |  650 | `{` |
|    5 |  651 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  652 | `	void * pConverted;` |
|    - |  653 | `	DWORD dwAttr;` |
|    5 |  654 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  655 | `	if( pConverted == 0 ){` |
|  ! 0 |  656 | `		return -1;` |
|    - |  657 | `	}` |
|    5 |  658 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  659 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  660 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  661 | `		return -1;` |
|    - |  662 | `	}` |
|    5 |  663 | `	return (dwAttr & (FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;` |
|    5 |  664 | `}` |
|    - |  665 | `/* int (*xIslink)(const char *) */` |
|    - |  666 | `static int WinVfs_islink(const char *zPath)` |
|    1 |  667 | `{` |
|    1 |  668 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  669 | `	void * pConverted;` |
|    - |  670 | `	DWORD dwAttr;` |
|    1 |  671 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  672 | `	if( pConverted == 0 ){` |
|  ! 0 |  673 | `		return -1;` |
|    - |  674 | `	}` |
|    1 |  675 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  676 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  677 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  678 | `		return -1;` |
|    - |  679 | `	}` |
|    1 |  680 | `	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;` |
|    1 |  681 | `}` |
|    - |  682 | `/* int (*xWritable)(const char *) */` |
|    - |  683 | `static int WinVfs_iswritable(const char *zPath)` |
|    1 |  684 | `{` |
|    1 |  685 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  686 | `	void * pConverted;` |
|    - |  687 | `	DWORD dwAttr;` |
|    1 |  688 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  689 | `	if( pConverted == 0 ){` |
|  ! 0 |  690 | `		return -1;` |
|    - |  691 | `	}` |
|    1 |  692 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  693 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  694 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  695 | `		return -1;` |
|    - |  696 | `	}` |
|    1 |  697 | `	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE\|FILE_ATTRIBUTE_NORMAL)) == 0 ){` |
|    - |  698 | `		/* Not a regular file */` |
|  ! 0 |  699 | `		return -1;` |
|    - |  700 | `	}` |
|    1 |  701 | `	if( dwAttr & FILE_ATTRIBUTE_READONLY ){` |
|    - |  702 | `		/* Read-only file */` |
|  ! 0 |  703 | `		return -1;` |
|    - |  704 | `	}` |
|    - |  705 | `	/* File is writable */` |
|    1 |  706 | `	return PH7_OK;` |
|    1 |  707 | `}` |
|    - |  708 | `/* int (*xExecutable)(const char *) */` |
|    - |  709 | `static int WinVfs_isexecutable(const char *zPath)` |
|    1 |  710 | `{` |
|    1 |  711 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  712 | `	void * pConverted;` |
|    - |  713 | `	DWORD dwAttr;` |
|    1 |  714 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  715 | `	if( pConverted == 0 ){` |
|  ! 0 |  716 | `		return -1;` |
|    - |  717 | `	}` |
|    1 |  718 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  719 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  720 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  721 | `		return -1;` |
|    - |  722 | `	}` |
|    1 |  723 | `	if( (dwAttr & FILE_ATTRIBUTE_NORMAL) == 0 ){` |
|    - |  724 | `		/* Not a regular file */` |
|    1 |  725 | `		return -1;` |
|    - |  726 | `	}` |
|    - |  727 | `	/* File is executable */` |
|  ! 0 |  728 | `	return PH7_OK;` |
|    1 |  729 | `}` |
|    - |  730 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  731 | `static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|    1 |  732 | `{` |
|    1 |  733 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  734 | `	void * pConverted;` |
|    - |  735 | `	DWORD dwAttr;` |
|    1 |  736 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  737 | `	if( pConverted == 0 ){` |
|    - |  738 | `		/* Expand 'unknown' */` |
|  ! 0 |  739 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  740 | `		return -1;` |
|    - |  741 | `	}` |
|    1 |  742 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  743 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  744 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    - |  745 | `		/* Expand 'unknown' */` |
|    1 |  746 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    1 |  747 | `		return -1;` |
|    - |  748 | `	}` |
|    1 |  749 | `	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE) ){` |
|    1 |  750 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|    1 |  751 | `	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){` |
|    1 |  752 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|  ! 0 |  753 | `	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){` |
|  ! 0 |  754 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|  ! 0 |  755 | `	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){` |
|  ! 0 |  756 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|  ! 0 |  757 | `	}else{` |
|  ! 0 |  758 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    - |  759 | `	}` |
|    1 |  760 | `	return PH7_OK;` |
|    1 |  761 | `}` |
|    - |  762 | `/*` |
|    - |  763 | ` * int (*xReadlink)(const char *,ph7_context *)` |
|    - |  764 | ` *` |
|    - |  765 | ` * Windows has no readlink(2). php resolves the handle instead` |
|    - |  766 | ` * (GetFinalPathNameByHandleW) and strips the \\?\ prefix it comes back with, so` |
|    - |  767 | ` * the answer here is the target's CANONICAL path where a unix readlink() would` |
|    - |  768 | ` * hand back the link's raw text -- a relative symlink therefore reads back` |
|    - |  769 | ` * absolute on this platform.` |
|    - |  770 | ` */` |
|    - |  771 | `static int WinVfs_Readlink(const char *zPath,ph7_context *pCtx)` |
|    1 |  772 | `{` |
|    - |  773 | `	void *pConverted;` |
|    - |  774 | `	HANDLE pHandle;` |
|    - |  775 | `	WCHAR zTarget[1024];` |
|    - |  776 | `	char zUtf8[1024*4];` |
|    - |  777 | `	DWORD nLen;` |
|    - |  778 | `	int nOut;` |
|    1 |  779 | `	zPath = WinVfsLocalPath(zPath);` |
|    1 |  780 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  781 | `	if( pConverted == 0 ){` |
|  ! 0 |  782 | `		return -1;` |
|    - |  783 | `	}` |
|    - |  784 | `	/* No reparse-point screen: php_win32_ioutil_readlink_w falls back to the` |
|    - |  785 | `	 * handle's final path when the name is not a link, so a plain file reads` |
|    - |  786 | `	 * back as itself on this platform rather than failing. */` |
|    1 |  787 | `	pHandle = CreateFileW((LPCWSTR)pConverted,0,` |
|    - |  788 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_EXISTING,` |
|    - |  789 | `		FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    1 |  790 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  791 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 |  792 | `		return -1;` |
|    - |  793 | `	}` |
|    1 |  794 | `	nLen = GetFinalPathNameByHandleW(pHandle,zTarget,` |
|    - |  795 | `		(DWORD)(sizeof(zTarget)/sizeof(zTarget[0])) - 1,0);` |
|    1 |  796 | `	CloseHandle(pHandle);` |
|    1 |  797 | `	if( nLen < 1 \|\| nLen >= (DWORD)(sizeof(zTarget)/sizeof(zTarget[0])) ){` |
|  ! 0 |  798 | `		return -1;` |
|    - |  799 | `	}` |
|    1 |  800 | `	zTarget[nLen] = 0;` |
|    1 |  801 | `	nOut = WideCharToMultiByte(CP_UTF8,0,zTarget,(int)nLen,zUtf8,(int)sizeof(zUtf8),0,0);` |
|    1 |  802 | `	if( nOut < 1 ){` |
|  ! 0 |  803 | `		return -1;` |
|    - |  804 | `	}` |
|    - |  805 | `	/* Drop the \\?\ prefix the API always prepends, as php does. */` |
|    1 |  806 | `	if( nOut > 4 && zUtf8[0] == '\\' && zUtf8[1] == '\\' && zUtf8[2] == '?' && zUtf8[3] == '\\' ){` |
|    1 |  807 | `		ph7_result_string(pCtx,&zUtf8[4],nOut - 4);` |
|    1 |  808 | `	}else{` |
|  ! 0 |  809 | `		ph7_result_string(pCtx,zUtf8,nOut);` |
|    - |  810 | `	}` |
|    1 |  811 | `	return PH7_OK;` |
|    1 |  812 | `}` |
|    - |  813 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  814 | `static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|    4 |  815 | `{` |
|    - |  816 | `	char zValue[1024];` |
|    - |  817 | `	DWORD n;` |
|    - |  818 | `	/*` |
|    - |  819 | `	 * According to MSDN` |
|    - |  820 | `	 * If lpBuffer is not large enough to hold the data, the return` |
|    - |  821 | `	 * value is the buffer size, in characters, required to hold the` |
|    - |  822 | `	 * string and its terminating null character and the contents` |
|    - |  823 | `	 * of lpBuffer are undefined.` |
|    - |  824 | `	 */` |
|    4 |  825 | `	n = sizeof(zValue);` |
|    4 |  826 | `	SyMemcpy("Undefined",zValue,sizeof("Undefined")-1);` |
|    - |  827 | `	/* Extract the environment value */` |
|    4 |  828 | `	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));` |
|    4 |  829 | `	if( !n ){` |
|    - |  830 | `		/* No such variable*/` |
|    1 |  831 | `		return -1;` |
|    - |  832 | `	}` |
|    4 |  833 | `	ph7_result_string(pCtx,zValue,(int)n);` |
|    4 |  834 | `	return PH7_OK;` |
|    4 |  835 | `}` |
|    - |  836 | `/* int (*xSetenv)(const char *,const char *) */` |
|    - |  837 | `static int WinVfs_Setenv(const char *zName,const char *zValue)` |
|    1 |  838 | `{` |
|    - |  839 | `	BOOL rc;` |
|    1 |  840 | `	rc = SetEnvironmentVariableA(zName,zValue);` |
|    1 |  841 | `	return rc ? PH7_OK : -1;` |
|    1 |  842 | `}` |
|    - |  843 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  844 | `static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|    5 |  845 | `{` |
|    - |  846 | `	DWORD dwSizeLow,dwSizeHigh;` |
|    - |  847 | `	HANDLE pHandle,pMapHandle;` |
|    - |  848 | `	void *pConverted,*pView;` |
|    - |  849 |  |
|    5 |  850 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  851 | `	if( pConverted == 0 ){` |
|  ! 0 |  852 | `		return -1;` |
|    - |  853 | `	}` |
|    5 |  854 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    5 |  855 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  856 | `	if( pHandle == 0 ){` |
|  ! 0 |  857 | `		return -1;` |
|    - |  858 | `	}` |
|    - |  859 | `	/* Get the file size */` |
|    5 |  860 | `	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);` |
|    - |  861 | `	/* Create the mapping */` |
|    5 |  862 | `	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);` |
|    5 |  863 | `	if( pMapHandle == 0 ){` |
|  ! 0 |  864 | `		CloseHandle(pHandle);` |
|  ! 0 |  865 | `		return -1;` |
|    - |  866 | `	}` |
|    5 |  867 | `	*pSize = ((ph7_int64)dwSizeHigh << 32) \| dwSizeLow;` |
|    - |  868 | `	/* Obtain the view */` |
|    5 |  869 | `	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));` |
|    5 |  870 | `	if( pView ){` |
|    - |  871 | `		/* Let the upper layer point to the view */` |
|    5 |  872 | `		*ppMap = pView;` |
|    - |  873 | `	}` |
|    - |  874 | `	/* Close the handle` |
|    - |  875 | `	 * According to MSDN it's OK the close the HANDLES.` |
|    - |  876 | `	 */` |
|    5 |  877 | `	CloseHandle(pMapHandle);` |
|    5 |  878 | `	CloseHandle(pHandle);` |
|    5 |  879 | `	return pView ? PH7_OK : -1;` |
|    5 |  880 | `}` |
|    - |  881 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|    - |  882 | `static void WinVfs_Unmap(void *pView,ph7_int64 nSize)` |
|    5 |  883 | `{` |
|    5 |  884 | `	nSize = 0; /* Compiler warning */` |
|    5 |  885 | `	UnmapViewOfFile(pView);` |
|    5 |  886 | `}` |
|    - |  887 | `/* void (*xTempDir)(ph7_context *) */` |
|    - |  888 | `static void WinVfs_TempDir(ph7_context *pCtx)` |
|    4 |  889 | `{` |
|    - |  890 | `	CHAR zTemp[1024];` |
|    - |  891 | `	DWORD n;` |
|    4 |  892 | `	n = GetTempPathA(sizeof(zTemp),zTemp);` |
|    4 |  893 | `	if( n < 1 ){` |
|    - |  894 | `		/* Assume the default windows temp directory */` |
|  ! 0 |  895 | `		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);` |
|  ! 0 |  896 | `	}else{` |
|    4 |  897 | `		ph7_result_string(pCtx,zTemp,(int)n);` |
|    - |  898 | `	}` |
|    4 |  899 | `}` |
|    - |  900 | `/* unsigned int (*xProcessId)(void) */` |
|    - |  901 | `static unsigned int WinVfs_ProcessId(void)` |
|    3 |  902 | `{` |
|    3 |  903 | `	DWORD nID = 0;` |
|    - |  904 | `#ifndef __MINGW32__` |
|    3 |  905 | `	nID = GetProcessId(GetCurrentProcess());` |
|    - |  906 | `#endif /* __MINGW32__ */` |
|    3 |  907 | `	return (unsigned int)nID;` |
|    3 |  908 | `}` |
|    - |  909 | `/* void (*xUsername)(ph7_context *) */` |
|    - |  910 | `static void WinVfs_Username(ph7_context *pCtx)` |
|    1 |  911 | `{` |
|    - |  912 | `	WCHAR zUser[1024];` |
|    - |  913 | `	DWORD nByte;` |
|    - |  914 | `	BOOL rc;` |
|    1 |  915 | `	nByte = sizeof(zUser);` |
|    1 |  916 | `	rc = GetUserNameW(zUser,&nByte);` |
|    1 |  917 | `	if( !rc ){` |
|    - |  918 | `		/* Set a dummy name */` |
|  ! 0 |  919 | `		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  920 | `	}else{` |
|    - |  921 | `		char *zName;` |
|    1 |  922 | `		zName = unicodeToUtf8(zUser);` |
|    1 |  923 | `		if( zName == 0 ){` |
|  ! 0 |  924 | `			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  925 | `		}else{` |
|    1 |  926 | `			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    1 |  927 | `			HeapFree(GetProcessHeap(),0,zName);` |
|    - |  928 | `		}` |
|    - |  929 | `	}` |
|    - |  930 |  |
|    1 |  931 | `}` |
|    - |  932 | `/* int (*xChroot)(const char *) — Windows has no chroot; fail cleanly so chroot()` |
|    - |  933 | ` * returns false. (php has no chroot symbol at all; PHL exposes it as an extension` |
|    - |  934 | ` * and this reports the failure without a "not implemented in the VFS" warning.) */` |
|    - |  935 | `static int WinVfs_chroot(const char *zPath)` |
|  ! 0 |  936 | `{` |
|    - |  937 | `	(void)zPath;` |
|  ! 0 |  938 | `	return -1;` |
|  ! 0 |  939 | `}` |
|    - |  940 | `#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE` |
|    - |  941 | `#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2` |
|    - |  942 | `#endif` |
|    - |  943 | `#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY` |
|    - |  944 | `#define SYMBOLIC_LINK_FLAG_DIRECTORY 0x1` |
|    - |  945 | `#endif` |
|    - |  946 | `/* int (*xLink)(const char *,const char *,int) — hard link (iSym==0) via` |
|    - |  947 | ` * CreateHardLink or symbolic link (iSym!=0) via CreateSymbolicLink, mirroring` |
|    - |  948 | ` * php on Windows. Developer-mode symlink creation is allowed. */` |
|    - |  949 | `static int WinVfs_Link(const char *zOld,const char *zNew,int iSym)` |
|    1 |  950 | `{` |
|    - |  951 | `	void *pOld, *pNew;` |
|    - |  952 | `	BOOL rc;` |
|    1 |  953 | `	pOld = convertUtf8Filename(zOld);` |
|    1 |  954 | `	if( pOld == 0 ){` |
|  ! 0 |  955 | `		return -1;` |
|    - |  956 | `	}` |
|    1 |  957 | `	pNew = convertUtf8Filename(zNew);` |
|    1 |  958 | `	if( pNew == 0 ){` |
|  ! 0 |  959 | `		HeapFree(GetProcessHeap(),0,pOld);` |
|  ! 0 |  960 | `		return -1;` |
|    - |  961 | `	}` |
|    1 |  962 | `	if( iSym ){` |
|    1 |  963 | `		DWORD dwFlags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;` |
|    1 |  964 | `		DWORD attr = GetFileAttributesW((LPCWSTR)pOld);` |
|    1 |  965 | `		if( attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) ){` |
|  ! 0 |  966 | `			dwFlags \|= SYMBOLIC_LINK_FLAG_DIRECTORY;` |
|    - |  967 | `		}` |
|    1 |  968 | `		rc = CreateSymbolicLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,dwFlags) ? TRUE : FALSE;` |
|    1 |  969 | `	}else{` |
|    1 |  970 | `		rc = CreateHardLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,0);` |
|    - |  971 | `	}` |
|    1 |  972 | `	HeapFree(GetProcessHeap(),0,pNew);` |
|    1 |  973 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|    1 |  974 | `	return rc ? PH7_OK : -1;` |
|    1 |  975 | `}` |
|    - |  976 | `/* int (*xUmask)(int) — Windows has no umask; php's umask() returns 0 there. */` |
|    - |  977 | `static int WinVfs_Umask(int iMask)` |
|    1 |  978 | `{` |
|    - |  979 | `	(void)iMask;` |
|    1 |  980 | `	return 0;` |
|    1 |  981 | `}` |
|    - |  982 | `/* int (*xUid)(void) / int (*xGid)(void) — no uid/gid on Windows; php's` |
|    - |  983 | ` * getmyuid()/getmygid() both return 0. */` |
|    - |  984 | `static int WinVfs_Uid(void)` |
|    1 |  985 | `{` |
|    1 |  986 | `	return 0;` |
|    1 |  987 | `}` |
|    - |  988 | `static int WinVfs_Gid(void)` |
|    1 |  989 | `{` |
|    1 |  990 | `	return 0;` |
|    1 |  991 | `}` |
|    - |  992 | `/* Export the windows vfs */` |
|    - |  993 | `PH7_PRIVATE const ph7_vfs sWinVfs = {` |
|    - |  994 | `	"Windows_vfs",` |
|    - |  995 | `	PH7_VFS_VERSION,` |
|    - |  996 | `	WinVfs_chdir,    /* int (*xChdir)(const char *) */` |
|    - |  997 | `	WinVfs_chroot,   /* int (*xChroot)(const char *); */` |
|    - |  998 | `	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|    - |  999 | `	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|    - | 1000 | `	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|    - | 1001 | `	WinVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|    - | 1002 | `	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|    - | 1003 | `	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|    - | 1004 | `	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */` |
|    - | 1005 | `	WinVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|    - | 1006 | `	WinVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|    - | 1007 | `	WinVfs_chmod, /*int (*xChmod)(const char *,int)*/` |
|    - | 1008 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|    - | 1009 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|    - | 1010 | `	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - | 1011 | `	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - | 1012 | `	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|    - | 1013 | `	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - | 1014 | `	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - | 1015 | `	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - | 1016 | `	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - | 1017 | `	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - | 1018 | `	WinVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|    - | 1019 | `	WinVfs_islink,     /* int (*xIslink)(const char *) */` |
|    - | 1020 | `	WinVfs_isfile,     /* int (*xReadable)(const char *) */` |
|    - | 1021 | `	WinVfs_iswritable, /* int (*xWritable)(const char *) */` |
|    - | 1022 | `	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */` |
|    - | 1023 | `	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|    - | 1024 | `	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|    - | 1025 | `	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|    - | 1026 | `	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - | 1027 | `	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - | 1028 | `	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|    - | 1029 | `	WinVfs_Link,       /* int (*xLink)(const char *,const char *,int) */` |
|    - | 1030 | `	WinVfs_Umask,      /* int (*xUmask)(int) */` |
|    - | 1031 | `	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|    - | 1032 | `	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|    - | 1033 | `	WinVfs_Uid, /* int (*xUid)(void) */` |
|    - | 1034 | `	WinVfs_Gid, /* int (*xGid)(void) */` |
|    - | 1035 | `	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|    - | 1036 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|    - | 1037 | `	WinVfs_Readlink /* int (*xReadlink)(const char *,ph7_context *) */` |
|    - | 1038 | `};` |
|    - | 1039 | `/* Windows file IO */` |
|    - | 1040 | `#ifndef INVALID_SET_FILE_POINTER` |
|    - | 1041 | `# define INVALID_SET_FILE_POINTER ((DWORD)-1)` |
|    - | 1042 | `#endif` |
|    - | 1043 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|    - | 1044 | `static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|    5 | 1045 | `{` |
|    5 | 1046 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    5 | 1047 | `	DWORD dwAccess = GENERIC_READ;` |
|    - | 1048 | `	DWORD dwShare,dwCreate;` |
|    - | 1049 | `	void *pConverted;` |
|    - | 1050 | `	HANDLE pHandle;` |
|    - | 1051 |  |
|    5 | 1052 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 | 1053 | `	if( pConverted == 0 ){` |
|  ! 0 | 1054 | `		return -1;` |
|    - | 1055 | `	}` |
|    - | 1056 | `	/* Set the desired flags according to the open mode */` |
|    5 | 1057 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|    - | 1058 | `		/* Open existing file, or create if it doesn't exist */` |
|    5 | 1059 | `		dwCreate = OPEN_ALWAYS;` |
|    5 | 1060 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - | 1061 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|    5 | 1062 | `			dwCreate = CREATE_ALWAYS;` |
|    5 | 1063 | `		}` |
|    5 | 1064 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|    - | 1065 | `		/* Creates a new file, only if it does not already exist.` |
|    - | 1066 | `		* If the file exists, it fails.` |
|    - | 1067 | `		*/` |
|    2 | 1068 | `		dwCreate = CREATE_NEW;` |
|    5 | 1069 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - | 1070 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|    - | 1071 | `		 * The file must exist.` |
|    - | 1072 | `		 */` |
|  ! 0 | 1073 | `		dwCreate = TRUNCATE_EXISTING;` |
|  ! 0 | 1074 | `	}else{` |
|    - | 1075 | `		/* Opens a file, only if it exists. */` |
|    5 | 1076 | `		dwCreate = OPEN_EXISTING;` |
|    - | 1077 | `	}` |
|    5 | 1078 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|    - | 1079 | `		/* Read+Write access */` |
|    5 | 1080 | `		dwAccess \|= GENERIC_WRITE;` |
|    5 | 1081 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|    - | 1082 | `		/* Write only access */` |
|    2 | 1083 | `		dwAccess = GENERIC_WRITE;` |
|    - | 1084 | `	}` |
|    5 | 1085 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|    - | 1086 | `		/* Append mode */` |
|    1 | 1087 | `		dwAccess = FILE_APPEND_DATA;` |
|    - | 1088 | `	}` |
|    5 | 1089 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|    - | 1090 | `		/* File is temporary */` |
|  ! 0 | 1091 | `		dwType = FILE_ATTRIBUTE_TEMPORARY;` |
|    - | 1092 | `	}` |
|    5 | 1093 | `	dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    5 | 1094 | `	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    5 | 1095 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 | 1096 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    - | 1097 | `		SXUNUSED(pResource); /* MSVC warning */` |
|    4 | 1098 | `		return -1;` |
|    - | 1099 | `	}` |
|    - | 1100 | `	/* Make the handle accessible to the upper layer */` |
|    5 | 1101 | `	*ppHandle = (void *)pHandle;` |
|    5 | 1102 | `	return PH7_OK;` |
|    5 | 1103 | `}` |
|    - | 1104 | `/* An instance of the following structure is used to record state information` |
|    - | 1105 | ` * while iterating throw directory entries.` |
|    - | 1106 | ` */` |
|    - | 1107 | `typedef struct WinDir_Info WinDir_Info;` |
|    - | 1108 | `struct WinDir_Info` |
|    - | 1109 | `{` |
|    - | 1110 | `	HANDLE pDirHandle;` |
|    - | 1111 | `	void *pPath;` |
|    - | 1112 | `	WIN32_FIND_DATAW sInfo;` |
|    - | 1113 | `	int rc;` |
|    - | 1114 | `};` |
|    - | 1115 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|    - | 1116 | `static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|    5 | 1117 | `{` |
|    - | 1118 | `	WinDir_Info *pDirInfo;` |
|    - | 1119 | `	void *pConverted;` |
|    - | 1120 | `	char *zPrep;` |
|    - | 1121 | `	sxu32 n;` |
|    - | 1122 | `	/* Prepare the path */` |
|    5 | 1123 | `	n = SyStrlen(zPath);` |
|    5 | 1124 | `	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);` |
|    5 | 1125 | `	if( zPrep == 0 ){` |
|  ! 0 | 1126 | `		return -1;` |
|    - | 1127 | `	}` |
|    5 | 1128 | `	SyMemcpy((const void *)zPath,zPrep,n);` |
|    5 | 1129 | `	zPrep[n]   = '\\';` |
|    5 | 1130 | `	zPrep[n+1] =  '*';` |
|    5 | 1131 | `	zPrep[n+2] = 0;` |
|    5 | 1132 | `	pConverted = convertUtf8Filename(zPrep);` |
|    5 | 1133 | `	HeapFree(GetProcessHeap(),0,zPrep);` |
|    5 | 1134 | `	if( pConverted == 0 ){` |
|  ! 0 | 1135 | `		return -1;` |
|    - | 1136 | `	}` |
|    - | 1137 | `	/* Allocate a new instance */` |
|    5 | 1138 | `	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));` |
|    5 | 1139 | `	if( pDirInfo == 0 ){` |
|  ! 0 | 1140 | `		pResource = 0; /* Compiler warning */` |
|  ! 0 | 1141 | `		return -1;` |
|    - | 1142 | `	}` |
|    5 | 1143 | `	pDirInfo->rc = SXRET_OK;` |
|    5 | 1144 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);` |
|    5 | 1145 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|    - | 1146 | `		/* Cannot open directory */` |
|    2 | 1147 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 | 1148 | `		HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    2 | 1149 | `		return -1;` |
|    - | 1150 | `	}` |
|    - | 1151 | `	/* Save the path */` |
|    5 | 1152 | `	pDirInfo->pPath = pConverted;` |
|    - | 1153 | `	/* Save our structure */` |
|    5 | 1154 | `	*ppHandle = pDirInfo;` |
|    5 | 1155 | `	return PH7_OK;` |
|    5 | 1156 | `}` |
|    - | 1157 | `/* void (*xCloseDir)(void *) */` |
|    - | 1158 | `static void WinDir_Close(void *pUserData)` |
|    5 | 1159 | `{` |
|    5 | 1160 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    5 | 1161 | `	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){` |
|    5 | 1162 | `		FindClose(pDirInfo->pDirHandle);` |
|    - | 1163 | `	}` |
|    5 | 1164 | `	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);` |
|    5 | 1165 | `	HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    5 | 1166 | `}` |
|    - | 1167 | `/* void (*xClose)(void *); */` |
|    - | 1168 | `static void WinFile_Close(void *pUserData)` |
|    5 | 1169 | `{` |
|    5 | 1170 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    5 | 1171 | `	CloseHandle(pHandle);` |
|    5 | 1172 | `}` |
|    - | 1173 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|    - | 1174 | `static int WinDir_Read(void *pUserData,ph7_context *pCtx)` |
|    5 | 1175 | `{` |
|    5 | 1176 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    - | 1177 | `	LPWIN32_FIND_DATAW pData;` |
|    - | 1178 | `	char *zName;` |
|    - | 1179 | `	BOOL rc;` |
|    5 | 1180 | `	if( pDirInfo->rc != SXRET_OK ){` |
|    - | 1181 | `		/* No more entry to process */` |
|    5 | 1182 | `		return -1;` |
|    - | 1183 | `	}` |
|    5 | 1184 | `	pData = &pDirInfo->sInfo;` |
|    - | 1185 | `	/* php parity: readdir()/scandir() include the '.' and '..' entries, so unlike` |
|    - | 1186 | `	 * the historical PH7 behaviour we return them instead of skipping. */` |
|    5 | 1187 | `	zName = unicodeToUtf8(pData->cFileName);` |
|    5 | 1188 | `	if( zName == 0 ){` |
|    - | 1189 | `		/* Out of memory */` |
|  ! 0 | 1190 | `		return -1;` |
|    - | 1191 | `	}` |
|    - | 1192 | `	/* Return the current file name */` |
|    5 | 1193 | `	ph7_result_string(pCtx,zName,-1);` |
|    5 | 1194 | `	HeapFree(GetProcessHeap(),0,zName);` |
|    - | 1195 | `	/* Point to the next entry */` |
|    5 | 1196 | `	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|    5 | 1197 | `	if( !rc ){` |
|    5 | 1198 | `		pDirInfo->rc = SXERR_EOF;` |
|    - | 1199 | `	}` |
|    5 | 1200 | `	return PH7_OK;` |
|    5 | 1201 | `}` |
|    - | 1202 | `/* void (*xRewindDir)(void *) */` |
|    - | 1203 | `static void WinDir_RewindDir(void *pUserData)` |
|    2 | 1204 | `{` |
|    2 | 1205 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    2 | 1206 | `	FindClose(pDirInfo->pDirHandle);` |
|    2 | 1207 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);` |
|    2 | 1208 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 | 1209 | `		pDirInfo->rc = SXERR_EOF;` |
|  ! 0 | 1210 | `	}else{` |
|    2 | 1211 | `		pDirInfo->rc = SXRET_OK;` |
|    - | 1212 | `	}` |
|    2 | 1213 | `}` |
|    - | 1214 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|    - | 1215 | `static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)` |
|    5 | 1216 | `{` |
|    5 | 1217 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1218 | `	DWORD nRd;` |
|    - | 1219 | `	BOOL rc;` |
|    5 | 1220 | `	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    5 | 1221 | `	if( !rc ){` |
|    - | 1222 | `		/* EOF or IO error */` |
|  ! 0 | 1223 | `		return -1;` |
|    - | 1224 | `	}` |
|    5 | 1225 | `	return (ph7_int64)nRd;` |
|    5 | 1226 | `}` |
|    - | 1227 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|    - | 1228 | `static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)` |
|    5 | 1229 | `{` |
|    5 | 1230 | `	const char *zData = (const char *)pBuffer;` |
|    5 | 1231 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1232 | `	ph7_int64 nCount;` |
|    - | 1233 | `	DWORD nWr;` |
|    - | 1234 | `	BOOL rc;` |
|    5 | 1235 | `	nWr = 0;` |
|    5 | 1236 | `	nCount = 0;` |
|    - | 1237 | `	for(;;){` |
|    5 | 1238 | `		if( nWrite < 1 ){` |
|    5 | 1239 | `			break;` |
|    - | 1240 | `		}` |
|    5 | 1241 | `		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);` |
|    5 | 1242 | `		if( !rc ){` |
|    - | 1243 | `			/* IO error — surface a POSIX errno for the caller's diagnostic` |
|    - | 1244 | `			 * (e.g. a byte-range lock violation reports EACCES like php). */` |
|    1 | 1245 | `			WinVfsMapErrno();` |
|    1 | 1246 | `			break;` |
|    - | 1247 | `		}` |
|    5 | 1248 | `		nWrite -= nWr;` |
|    5 | 1249 | `		nCount += nWr;` |
|    5 | 1250 | `		zData += nWr;` |
|    5 | 1251 | `	}` |
|    5 | 1252 | `	if( nWrite > 0 ){` |
|    1 | 1253 | `		return -1;` |
|    - | 1254 | `	}` |
|    5 | 1255 | `	return nCount;` |
|    5 | 1256 | `}` |
|    - | 1257 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|    - | 1258 | `static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|    1 | 1259 | `{` |
|    1 | 1260 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1261 | `	DWORD dwMove,dwNew;` |
|    - | 1262 | `	LONG nHighOfft;` |
|    1 | 1263 | `	switch(whence){` |
|    - | 1264 | `	case 1:/*SEEK_CUR*/` |
|  ! 0 | 1265 | `		dwMove = FILE_CURRENT;` |
|  ! 0 | 1266 | `		break;` |
|    - | 1267 | `	case 2: /* SEEK_END */` |
|  ! 0 | 1268 | `		dwMove = FILE_END;` |
|  ! 0 | 1269 | `		break;` |
|    - | 1270 | `	case 0: /* SEEK_SET */` |
|    - | 1271 | `	default:` |
|    1 | 1272 | `		dwMove = FILE_BEGIN;` |
|    - | 1273 | `		break;` |
|    - | 1274 | `	}` |
|    1 | 1275 | `	nHighOfft = (LONG)(iOfft >> 32);` |
|    1 | 1276 | `	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);` |
|    1 | 1277 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1278 | `		return -1;` |
|    - | 1279 | `	}` |
|    1 | 1280 | `	return PH7_OK;` |
|    1 | 1281 | `}` |
|    - | 1282 | `/* int (*xLock)(void *,int) — see the lock_type value space in ph7.h */` |
|    - | 1283 | `static int WinFile_Lock(void *pUserData,int lock_type)` |
|    1 | 1284 | `{` |
|    1 | 1285 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1286 | `	OVERLAPPED sDummy;` |
|    - | 1287 | `	BOOL rc;` |
|    1 | 1288 | `	SyZero(&sDummy,sizeof(sDummy));` |
|    - | 1289 | `	/* Lock/unlock the whole file. php locks the maximal byte range, so the lock` |
|    - | 1290 | `	 * is effective even for an empty or freshly-truncated file — the previous` |
|    - | 1291 | `	 * code locked only GetFileSize() bytes (i.e. nothing for a 0-byte file). */` |
|    1 | 1292 | `	if( lock_type < 0 ){` |
|    - | 1293 | `		/* Unlock the file */` |
|    1 | 1294 | `		rc = UnlockFileEx(pHandle,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);` |
|    1 | 1295 | `	}else{` |
|    - | 1296 | `		/* LOCKFILE_FAIL_IMMEDIATELY belongs to the NON-BLOCKING requests only: it` |
|    - | 1297 | `		 * used to be passed unconditionally, so a plain flock($f,LOCK_EX) answered` |
|    - | 1298 | `		 * false under contention on Windows where php (and POSIX) waits. */` |
|    1 | 1299 | `		DWORD dwFlags = 0;` |
|    1 | 1300 | `		if( lock_type == PH7_IO_LOCK_EX \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    1 | 1301 | `			dwFlags \|= LOCKFILE_EXCLUSIVE_LOCK;` |
|    - | 1302 | `		}` |
|    1 | 1303 | `		if( lock_type == PH7_IO_LOCK_SH_NB \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    1 | 1304 | `			dwFlags \|= LOCKFILE_FAIL_IMMEDIATELY;` |
|    - | 1305 | `		}` |
|    1 | 1306 | `		rc = LockFileEx(pHandle,dwFlags,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);` |
|    1 | 1307 | `		if( !rc && GetLastError() == ERROR_LOCK_VIOLATION ){` |
|    - | 1308 | `			/* Refused because another holder has the range: php's $would_block. */` |
|    1 | 1309 | `			return SXERR_BUSY;` |
|    - | 1310 | `		}` |
|    - | 1311 | `	}` |
|    1 | 1312 | `	return rc ? PH7_OK : -1 /* Lock error */;` |
|    1 | 1313 | `}` |
|    - | 1314 | `/* ph7_int64 (*xTell)(void *) */` |
|    - | 1315 | `static ph7_int64 WinFile_Tell(void *pUserData)` |
|    1 | 1316 | `{` |
|    1 | 1317 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1318 | `	DWORD dwNew;` |
|    1 | 1319 | `	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);` |
|    1 | 1320 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1321 | `		return -1;` |
|    - | 1322 | `	}` |
|    1 | 1323 | `	return (ph7_int64)dwNew;` |
|    1 | 1324 | `}` |
|    - | 1325 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|    - | 1326 | `static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|    1 | 1327 | `{` |
|    1 | 1328 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1329 | `	LONG HighOfft;` |
|    - | 1330 | `	DWORD dwNew;` |
|    - | 1331 | `	BOOL rc;` |
|    1 | 1332 | `	HighOfft = (LONG)(nOfft >> 32);` |
|    1 | 1333 | `	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);` |
|    1 | 1334 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1335 | `		return -1;` |
|    - | 1336 | `	}` |
|    1 | 1337 | `	rc = SetEndOfFile(pHandle);` |
|    1 | 1338 | `	return rc ? PH7_OK : -1;` |
|    1 | 1339 | `}` |
|    - | 1340 | `/* int (*xSync)(void *); */` |
|    - | 1341 | `static int WinFile_Sync(void *pUserData)` |
|    1 | 1342 | `{` |
|    1 | 1343 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1344 | `	BOOL rc;` |
|    1 | 1345 | `	rc = FlushFileBuffers(pHandle);` |
|    1 | 1346 | `	return rc ? PH7_OK : - 1;` |
|    1 | 1347 | `}` |
|    - | 1348 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|    - | 1349 | `static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 1350 | `{` |
|    - | 1351 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    1 | 1352 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1353 | `	BOOL rc;` |
|    1 | 1354 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 | 1355 | `	if( !rc ){` |
|  ! 0 | 1356 | `		return -1;` |
|    - | 1357 | `	}` |
|    - | 1358 | `	/* dev */` |
|    1 | 1359 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 | 1360 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - | 1361 | `	/* ino */` |
|    1 | 1362 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 | 1363 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - | 1364 | `	/* mode */` |
|    1 | 1365 | `	ph7_value_int(pWorker,0);` |
|    1 | 1366 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - | 1367 | `	/* nlink */` |
|    1 | 1368 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 | 1369 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - | 1370 | `	/* uid,gid,rdev */` |
|    1 | 1371 | `	ph7_value_int(pWorker,0);` |
|    1 | 1372 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 | 1373 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 | 1374 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - | 1375 | `	/* size */` |
|    1 | 1376 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 | 1377 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - | 1378 | `	/* atime */` |
|    1 | 1379 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 | 1380 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - | 1381 | `	/* mtime */` |
|    1 | 1382 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 | 1383 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - | 1384 | `	/* ctime */` |
|    1 | 1385 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 | 1386 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - | 1387 | `	/* blksize,blocks */` |
|    1 | 1388 | `	ph7_value_int(pWorker,0);` |
|    1 | 1389 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 | 1390 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 | 1391 | `	return PH7_OK;` |
|    1 | 1392 | `}` |
|    - | 1393 | `/* Export the file:// stream */` |
|    - | 1394 | `PH7_PRIVATE const ph7_io_stream sWinFileStream = {` |
|    - | 1395 | `	"file", /* Stream name */` |
|    - | 1396 | `	PH7_IO_STREAM_VERSION,` |
|    - | 1397 | `	WinFile_Open,  /* xOpen */` |
|    - | 1398 | `	WinDir_Open,   /* xOpenDir */` |
|    - | 1399 | `	WinFile_Close, /* xClose */` |
|    - | 1400 | `	WinDir_Close,  /* xCloseDir */` |
|    - | 1401 | `	WinFile_Read,  /* xRead */` |
|    - | 1402 | `	WinDir_Read,   /* xReadDir */` |
|    - | 1403 | `	WinFile_Write, /* xWrite */` |
|    - | 1404 | `	WinFile_Seek,  /* xSeek */` |
|    - | 1405 | `	WinFile_Lock,  /* xLock */` |
|    - | 1406 | `	WinDir_RewindDir, /* xRewindDir */` |
|    - | 1407 | `	WinFile_Tell,  /* xTell */` |
|    - | 1408 | `	WinFile_Trunc, /* xTrunc */` |
|    - | 1409 | `	WinFile_Sync,  /* xSeek */` |
|    - | 1410 | `	WinFile_Stat   /* xStat */` |
|    - | 1411 | `};` |
|    - | 1412 | `#endif /* __WINNT__ */` |
|    - | 1413 |  |
