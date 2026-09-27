# src/ph7/vfs_win.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 732/843 lines (86.83%)

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
|    2 |   94 | `		case ERROR_LOCK_VIOLATION:    errno = EACCES; break;` |
|    - |   95 | `		case ERROR_FILE_EXISTS:` |
|    1 |   96 | `		case ERROR_ALREADY_EXISTS:    errno = EEXIST; break;` |
|    2 |   97 | `		case ERROR_DIR_NOT_EMPTY:     errno = ENOTEMPTY; break;` |
|    1 |   98 | `		default:                      errno = EIO; break;` |
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
|    - |  109 | `	/* The engine's one file:// strip, drive rule included. */` |
|    5 |  110 | `	return PH7_VmFileUrlLocalPath(zPath);` |
|    5 |  111 | `}` |
|    - |  112 | `/* int (*xchdir)(const char *) */` |
|    - |  113 | `static int WinVfs_chdir(const char *zPath)` |
|    5 |  114 | `{` |
|    - |  115 | `	void * pConverted;` |
|    - |  116 | `	BOOL rc;` |
|    5 |  117 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  118 | `	if( pConverted == 0 ){` |
|  ! 0 |  119 | `		return -1;` |
|    - |  120 | `	}` |
|    5 |  121 | `	rc = SetCurrentDirectoryW((LPCWSTR)pConverted);` |
|    5 |  122 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    5 |  123 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  124 | `	return rc ? PH7_OK : -1;` |
|    5 |  125 | `}` |
|    - |  126 | `/* int (*xGetcwd)(ph7_context *) */` |
|    - |  127 | `static int WinVfs_getcwd(ph7_context *pCtx)` |
|    5 |  128 | `{` |
|    - |  129 | `	WCHAR zDir[2048];` |
|    - |  130 | `	char *zConverted;` |
|    - |  131 | `	DWORD rc;` |
|    - |  132 | `	/* Get the current directory */` |
|    5 |  133 | `	rc = GetCurrentDirectoryW(sizeof(zDir),zDir);` |
|    5 |  134 | `	if( rc < 1 ){` |
|  ! 0 |  135 | `		return -1;` |
|    - |  136 | `	}` |
|    5 |  137 | `	zConverted = unicodeToUtf8(zDir);` |
|    5 |  138 | `	if( zConverted == 0 ){` |
|  ! 0 |  139 | `		return -1;` |
|    - |  140 | `	}` |
|    5 |  141 | `	ph7_result_string(pCtx,zConverted,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    5 |  142 | `	HeapFree(GetProcessHeap(),0,zConverted);` |
|    5 |  143 | `	return PH7_OK;` |
|    5 |  144 | `}` |
|    - |  145 | `/* int (*xMkdir)(const char *,int,int)` |
|    - |  146 | ` * ONE level. php builds a tree in the WRAPPER rather than in the syscall, and so` |
|    - |  147 | `` * does PHL now (VfsMkdirRecursive in vfs.c), so `recursive` never arrives set. */`` |
|    - |  148 | `static int WinVfs_mkdir(const char *zPath,int mode,int recursive)` |
|    5 |  149 | `{` |
|    - |  150 | `	void * pConverted;` |
|    - |  151 | `	BOOL rc;` |
|    5 |  152 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  153 | `	if( pConverted == 0 ){` |
|  ! 0 |  154 | `		errno = ENOMEM;` |
|  ! 0 |  155 | `		return -1;` |
|    - |  156 | `	}` |
|    5 |  157 | `	mode= 0; /* MSVC warning */` |
|    5 |  158 | `	recursive = 0;` |
|    5 |  159 | `	rc = CreateDirectoryW((LPCWSTR)pConverted,0);` |
|    5 |  160 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    5 |  161 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  162 | `	return rc ? PH7_OK : -1;` |
|    5 |  163 | `}` |
|    - |  164 | `/* int (*xRmdir)(const char *) */` |
|    - |  165 | `static int WinVfs_rmdir(const char *zPath)` |
|    5 |  166 | `{` |
|    - |  167 | `	void * pConverted;` |
|    - |  168 | `	BOOL rc;` |
|    5 |  169 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  170 | `	if( pConverted == 0 ){` |
|  ! 0 |  171 | `		return -1;` |
|    - |  172 | `	}` |
|    5 |  173 | `	rc = RemoveDirectoryW((LPCWSTR)pConverted);` |
|    5 |  174 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    5 |  175 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  176 | `	return rc ? PH7_OK : -1;` |
|    5 |  177 | `}` |
|    - |  178 | `/* int (*xIsdir)(const char *) */` |
|    - |  179 | `static int WinVfs_isdir(const char *zPath)` |
|    5 |  180 | `{` |
|    5 |  181 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  182 | `	void * pConverted;` |
|    - |  183 | `	DWORD dwAttr;` |
|    5 |  184 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  185 | `	if( pConverted == 0 ){` |
|  ! 0 |  186 | `		return -1;` |
|    - |  187 | `	}` |
|    5 |  188 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  189 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  190 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    4 |  191 | `		return -1;` |
|    - |  192 | `	}` |
|    5 |  193 | `	return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? PH7_OK : -1;` |
|    5 |  194 | `}` |
|    - |  195 | `/* int (*xRename)(const char *,const char *) */` |
|    - |  196 | `static int WinVfs_Rename(const char *zOld,const char *zNew)` |
|    1 |  197 | `{` |
|    - |  198 | `	void *pOld,*pNew;` |
|    1 |  199 | `	BOOL rc = 0;` |
|    1 |  200 | `	pOld = convertUtf8Filename(zOld);` |
|    1 |  201 | `	if( pOld == 0 ){` |
|  ! 0 |  202 | `		return -1;` |
|    - |  203 | `	}` |
|    1 |  204 | `	pNew = convertUtf8Filename(zNew);` |
|    1 |  205 | `	if( pNew  ){` |
|    1 |  206 | `		rc = MoveFileW((LPCWSTR)pOld,(LPCWSTR)pNew);` |
|    - |  207 | `	}` |
|    1 |  208 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    1 |  209 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|    1 |  210 | `	if( pNew ){` |
|    1 |  211 | `		HeapFree(GetProcessHeap(),0,pNew);` |
|    - |  212 | `	}` |
|    1 |  213 | `	return rc ? PH7_OK : - 1;` |
|    1 |  214 | `}` |
|    - |  215 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|    - |  216 | `static int WinVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|    3 |  217 | `{` |
|    - |  218 | `	WCHAR zTemp[2048];` |
|    - |  219 | `	void *pPath;` |
|    - |  220 | `	char *zReal;` |
|    - |  221 | `	DWORD n;` |
|    3 |  222 | `	pPath = convertUtf8Filename(zPath);` |
|    3 |  223 | `	if( pPath == 0 ){` |
|  ! 0 |  224 | `		return -1;` |
|    - |  225 | `	}` |
|    3 |  226 | `	n = GetFullPathNameW((LPCWSTR)pPath,0,0,0);` |
|    3 |  227 | `	if( n > 0 ){` |
|    3 |  228 | `		if( n >= sizeof(zTemp) ){` |
|  ! 0 |  229 | `			n = sizeof(zTemp) - 1;` |
|    - |  230 | `		}` |
|    3 |  231 | `		GetFullPathNameW((LPCWSTR)pPath,n,zTemp,0);` |
|    - |  232 | `	}` |
|    - |  233 | `	/* GetFullPathNameW only NORMALIZES -- it answers happily for a path that does` |
|    - |  234 | `	 * not exist, and php's realpath() is false for one on every platform. */` |
|    3 |  235 | `	if( n > 0 && GetFileAttributesW((LPCWSTR)pPath) == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  236 | `		n = 0;` |
|    - |  237 | `	}` |
|    3 |  238 | `	HeapFree(GetProcessHeap(),0,pPath);` |
|    3 |  239 | `	if( !n ){` |
|    1 |  240 | `		return -1;` |
|    - |  241 | `	}` |
|    3 |  242 | `	zReal = unicodeToUtf8(zTemp);` |
|    3 |  243 | `	if( zReal == 0 ){` |
|  ! 0 |  244 | `		return -1;` |
|    - |  245 | `	}` |
|    - |  246 | `	{` |
|    - |  247 | `		/* GetFullPathNameW KEEPS a trailing separator where php's realpath()` |
|    - |  248 | `		 * drops it everywhere but a drive root -- and so must this, because` |
|    - |  249 | `		 * realpath($d . '/') has to name the same string as realpath($d). */` |
|    3 |  250 | `		sxu32 nReal = SyStrlen(zReal);` |
|    3 |  251 | `		while( nReal > 3 && (zReal[nReal-1] == '\\' \|\| zReal[nReal-1] == '/') ){` |
|    1 |  252 | `			nReal--;` |
|    1 |  253 | `		}` |
|    3 |  254 | `		ph7_result_string(pCtx,zReal,(int)nReal); /* Will make it's own copy */` |
|    - |  255 | `	}` |
|    3 |  256 | `	HeapFree(GetProcessHeap(),0,zReal);` |
|    3 |  257 | `	return PH7_OK;` |
|    3 |  258 | `}` |
|    - |  259 | `/* int (*xSleep)(unsigned int) */` |
|    - |  260 | `static int WinVfs_Sleep(unsigned int uSec)` |
|    4 |  261 | `{` |
|    4 |  262 | `	Sleep(uSec/1000/*uSec per Millisec */);` |
|    4 |  263 | `	return PH7_OK;` |
|    4 |  264 | `}` |
|    - |  265 | `/* int (*xUnlink)(const char *) */` |
|    - |  266 | `static int WinVfs_unlink(const char *zPath)` |
|    5 |  267 | `{` |
|    - |  268 | `	void * pConverted;` |
|    - |  269 | `	BOOL rc;` |
|    5 |  270 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  271 | `	if( pConverted == 0 ){` |
|  ! 0 |  272 | `		return -1;` |
|    - |  273 | `	}` |
|    5 |  274 | `	rc = DeleteFileW((LPCWSTR)pConverted);` |
|    5 |  275 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    5 |  276 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  277 | `	return rc ? PH7_OK : - 1;` |
|    5 |  278 | `}` |
|    - |  279 | `/* int (*xChmod)(const char *,int) */` |
|    - |  280 | `static int WinVfs_chmod(const char *zPath,int mode)` |
|    5 |  281 | `{` |
|    - |  282 | `	void * pConverted;` |
|    - |  283 | `	int rc;` |
|    5 |  284 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  285 | `	if( pConverted == 0 ){` |
|  ! 0 |  286 | `		return -1;` |
|    - |  287 | `	}` |
|    - |  288 | `	/* Windows honors only the read-only attribute: a set owner-write bit (0200)` |
|    - |  289 | `	 * clears it, otherwise the file is made read-only. This mirrors php, whose` |
|    - |  290 | `	 * chmod() on Windows likewise maps through _wchmod and returns success. */` |
|    5 |  291 | `	rc = _wchmod((const wchar_t *)pConverted,(mode & 0200) ? (_S_IREAD\|_S_IWRITE) : _S_IREAD);` |
|    5 |  292 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  293 | `	return rc == 0 ? PH7_OK : - 1;` |
|    5 |  294 | `}` |
|    - |  295 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - |  296 | `static ph7_int64 WinVfs_DiskFreeSpace(const char *zPath)` |
|    1 |  297 | `{` |
|    - |  298 | `#ifdef _WIN32_WCE` |
|    - |  299 | `	/* GetDiskFreeSpaceEx is not supported under WINCE */` |
|    - |  300 | `	SXUNUSED(zPath);` |
|    - |  301 | `	return -1;` |
|    - |  302 | `#else` |
|    - |  303 | `	/* php's own call (win32/ioutil + php_disk_free_space): GetDiskFreeSpaceExW on` |
|    - |  304 | `	 * the DIRECTORY it was given, failing when that directory does not exist.` |
|    - |  305 | `	 * PH7 called the older GetDiskFreeSpaceW, which only accepts a volume ROOT, so` |
|    - |  306 | `	 * it first truncated the path at its first separator: every subpath answered` |
|    - |  307 | `	 * for the drive rather than for the volume actually mounted there, and a path` |
|    - |  308 | `	 * that does not exist at all answered the drive's numbers instead of failing.` |
|    - |  309 | `	 * The failure value is -1, the convention the unix VFS already uses and the` |
|    - |  310 | `	 * one disk_free_space() reads as php's FALSE. */` |
|    - |  311 | `	ULARGE_INTEGER uFreeToCaller,uTotal,uTotalFree;` |
|    - |  312 | `	void * pConverted;` |
|    - |  313 | `	BOOL rc;` |
|    1 |  314 | `	pConverted = convertUtf8Filename(WinVfsLocalPath(zPath));` |
|    1 |  315 | `	if( pConverted == 0 ){` |
|  ! 0 |  316 | `		return -1;` |
|    - |  317 | `	}` |
|    1 |  318 | `	rc = GetDiskFreeSpaceExW((LPCWSTR)pConverted,&uFreeToCaller,&uTotal,&uTotalFree);` |
|    1 |  319 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    1 |  320 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  321 | `	if( !rc ){` |
|    1 |  322 | `		return -1;` |
|    - |  323 | `	}` |
|    1 |  324 | `	return (ph7_int64)uFreeToCaller.QuadPart;` |
|    - |  325 | `#endif` |
|    1 |  326 | `}` |
|    - |  327 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - |  328 | `static ph7_int64 WinVfs_DiskTotalSpace(const char *zPath)` |
|    1 |  329 | `{` |
|    - |  330 | `#ifdef _WIN32_WCE` |
|    - |  331 | `	/* GetDiskFreeSpaceEx is not supported under WINCE */` |
|    - |  332 | `	SXUNUSED(zPath);` |
|    - |  333 | `	return -1;` |
|    - |  334 | `#else` |
|    - |  335 | `	/* php's own call (win32/ioutil + php_disk_free_space): GetDiskFreeSpaceExW on` |
|    - |  336 | `	 * the DIRECTORY it was given, failing when that directory does not exist.` |
|    - |  337 | `	 * PH7 called the older GetDiskFreeSpaceW, which only accepts a volume ROOT, so` |
|    - |  338 | `	 * it first truncated the path at its first separator: every subpath answered` |
|    - |  339 | `	 * for the drive rather than for the volume actually mounted there, and a path` |
|    - |  340 | `	 * that does not exist at all answered the drive's numbers instead of failing.` |
|    - |  341 | `	 * The failure value is -1, the convention the unix VFS already uses and the` |
|    - |  342 | `	 * one disk_free_space() reads as php's FALSE. */` |
|    - |  343 | `	ULARGE_INTEGER uFreeToCaller,uTotal,uTotalFree;` |
|    - |  344 | `	void * pConverted;` |
|    - |  345 | `	BOOL rc;` |
|    1 |  346 | `	pConverted = convertUtf8Filename(WinVfsLocalPath(zPath));` |
|    1 |  347 | `	if( pConverted == 0 ){` |
|  ! 0 |  348 | `		return -1;` |
|    - |  349 | `	}` |
|    1 |  350 | `	rc = GetDiskFreeSpaceExW((LPCWSTR)pConverted,&uFreeToCaller,&uTotal,&uTotalFree);` |
|    1 |  351 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    1 |  352 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  353 | `	if( !rc ){` |
|    1 |  354 | `		return -1;` |
|    - |  355 | `	}` |
|    1 |  356 | `	return (ph7_int64)uTotal.QuadPart;` |
|    - |  357 | `#endif` |
|    1 |  358 | `}` |
|    - |  359 | `/* int (*xFileExists)(const char *) */` |
|    - |  360 | `static int WinVfs_FileExists(const char *zPath)` |
|    5 |  361 | `{` |
|    5 |  362 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  363 | `	void * pConverted;` |
|    - |  364 | `	DWORD dwAttr;` |
|    5 |  365 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  366 | `	if( pConverted == 0 ){` |
|  ! 0 |  367 | `		return -1;` |
|    - |  368 | `	}` |
|    5 |  369 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  370 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  371 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    5 |  372 | `		return -1;` |
|    - |  373 | `	}` |
|    5 |  374 | `	return PH7_OK;` |
|    5 |  375 | `}` |
|    - |  376 | `/* Open a path for a STAT-family read. Same as OpenReadOnly but with` |
|    - |  377 | ` * FILE_FLAG_BACKUP_SEMANTICS, which is the only way CreateFileW will open a` |
|    - |  378 | ` * DIRECTORY — without it filemtime()/fileatime()/filectime() answered -1 for every` |
|    - |  379 | ` * directory on Windows (a plain read-only open fails with ERROR_ACCESS_DENIED),` |
|    - |  380 | ` * where php reports the timestamp. FILE_READ_ATTRIBUTES is all` |
|    - |  381 | ` * GetFileInformationByHandle needs, so this also works on a file another process` |
|    - |  382 | ` * holds open for writing. */` |
|    - |  383 | `static HANDLE OpenForStat(LPCWSTR pPath)` |
|    3 |  384 | `{` |
|    - |  385 | `	HANDLE pHandle;` |
|    3 |  386 | `	pHandle = CreateFileW(pPath,FILE_READ_ATTRIBUTES,` |
|    - |  387 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_EXISTING,` |
|    - |  388 | `		FILE_ATTRIBUTE_NORMAL\|FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    3 |  389 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|    1 |  390 | `		return 0;` |
|    - |  391 | `	}` |
|    3 |  392 | `	return pHandle;` |
|    3 |  393 | `}` |
|    - |  394 | `/* Open a file in a read-only mode */` |
|    - |  395 | `static HANDLE OpenReadOnly(LPCWSTR pPath)` |
|    5 |  396 | `{` |
|    5 |  397 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    5 |  398 | `	DWORD dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    5 |  399 | `	DWORD dwAccess = GENERIC_READ;` |
|    5 |  400 | `	DWORD dwCreate = OPEN_EXISTING;` |
|    - |  401 | `	HANDLE pHandle;` |
|    5 |  402 | `	pHandle = CreateFileW(pPath,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    5 |  403 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    1 |  404 | `		return 0;` |
|    - |  405 | `	}` |
|    5 |  406 | `	return pHandle;` |
|    5 |  407 | `}` |
|    - |  408 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  409 | `static ph7_int64 WinVfs_FileSize(const char *zPath)` |
|    3 |  410 | `{` |
|    3 |  411 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  412 | `	DWORD dwLow,dwHigh;` |
|    - |  413 | `	void * pConverted;` |
|    - |  414 | `	ph7_int64 nSize;` |
|    - |  415 | `	HANDLE pHandle;` |
|    - |  416 |  |
|    3 |  417 | `	pConverted = convertUtf8Filename(zPath);` |
|    3 |  418 | `	if( pConverted == 0 ){` |
|  ! 0 |  419 | `		return -1;` |
|    - |  420 | `	}` |
|    - |  421 | `	/* Open the file in read-only mode */` |
|    3 |  422 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    3 |  423 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    3 |  424 | `	if( pHandle ){` |
|    3 |  425 | `		dwLow = GetFileSize(pHandle,&dwHigh);` |
|    3 |  426 | `		nSize = dwHigh;` |
|    3 |  427 | `		nSize <<= 32;` |
|    3 |  428 | `		nSize += dwLow;` |
|    3 |  429 | `		CloseHandle(pHandle);` |
|    3 |  430 | `	}else{` |
|    1 |  431 | `		nSize = -1;` |
|    - |  432 | `	}` |
|    3 |  433 | `	return nSize;` |
|    3 |  434 | `}` |
|    - |  435 | `#define TICKS_PER_SECOND 10000000` |
|    - |  436 | `#define EPOCH_DIFFERENCE 11644473600LL` |
|    - |  437 | `/* Convert Windows timestamp to UNIX timestamp */` |
|    - |  438 | `static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)` |
|    3 |  439 | `{` |
|    - |  440 | `    ph7_int64 input,temp;` |
|    3 |  441 | `	input = pTime->dwHighDateTime;` |
|    3 |  442 | `	input <<= 32;` |
|    3 |  443 | `	input += pTime->dwLowDateTime;` |
|    3 |  444 | `    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/` |
|    3 |  445 | `    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/` |
|    3 |  446 | `    return temp;` |
|    3 |  447 | `}` |
|    - |  448 | `/* Convert UNIX timestamp to Windows timestamp */` |
|    - |  449 | `static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)` |
|    3 |  450 | `{` |
|    - |  451 | ``  /* The converted value has to be the one that is STORED: this computed `result` and`` |
|    - |  452 | `   * then wrote the raw unix seconds into the FILETIME, so a requested 1000000000` |
|    - |  453 | `   * landed as 100 seconds past the 1601 epoch and read back as -11644473500. */` |
|    3 |  454 | `  ph7_int64 result = EPOCH_DIFFERENCE;` |
|    3 |  455 | `  result += nUnixtime;` |
|    3 |  456 | `  result *= TICKS_PER_SECOND;` |
|    3 |  457 | `  pOut->dwHighDateTime = (DWORD)((sxu64)result >> 32);` |
|    3 |  458 | `  pOut->dwLowDateTime = (DWORD)((sxu64)result & 0xFFFFFFFFu);` |
|    3 |  459 | `}` |
|    - |  460 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  461 | `static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|    3 |  462 | `{` |
|    - |  463 | `	FILETIME sTouch,sAccess;` |
|    - |  464 | `	void *pConverted;` |
|    - |  465 | `	void *pHandle;` |
|    3 |  466 | `	BOOL rc = 0;` |
|    - |  467 | `	/* Accept file:// like every other path-taking entry in this VFS (the POSIX` |
|    - |  468 | `	 * driver already does) — this was the only one that skipped the mapping. */` |
|    3 |  469 | `	zPath = WinVfsLocalPath(zPath);` |
|    3 |  470 | `	pConverted = convertUtf8Filename(zPath);` |
|    3 |  471 | `	if( pConverted == 0 ){` |
|  ! 0 |  472 | `		return -1;` |
|    - |  473 | `	}` |
|    - |  474 | `	/* php's touch() CREATES a missing file (OPEN_ALWAYS), and SetFileTime needs write` |
|    - |  475 | `	 * access — the read-only handle this used could not stamp an existing file either.` |
|    - |  476 | `	 * FILE_FLAG_BACKUP_SEMANTICS is what lets a DIRECTORY be opened at all (php's` |
|    - |  477 | `	 * win32 utime passes it for the same reason, and the POSIX driver's utime() works` |
|    - |  478 | `	 * on directories); it does not change what OPEN_ALWAYS creates for a missing path.` |
|    - |  479 | `	 * Mirrors the POSIX driver's utime + open(O_CREAT) fallback. */` |
|    3 |  480 | `	pHandle = CreateFileW((LPCWSTR)pConverted,FILE_WRITE_ATTRIBUTES,` |
|    - |  481 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_ALWAYS,` |
|    - |  482 | `		FILE_ATTRIBUTE_NORMAL\|FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    3 |  483 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 |  484 | `		pHandle = 0;` |
|    - |  485 | `	}` |
|    3 |  486 | `	if( pHandle ){` |
|    - |  487 | `		/* Both stamps are real values: the builtin resolves php's "now" default, so a` |
|    - |  488 | `		 * NEGATIVE timestamp (legal to php) is no longer read as "not given". */` |
|    3 |  489 | `		convertUnixTimeToWindowsTime(touch_time,&sTouch);` |
|    3 |  490 | `		convertUnixTimeToWindowsTime(access_time,&sAccess);` |
|    - |  491 | `		/* SetFileTime(hFile, creation, lastAccess, lastWrite): the modification stamp` |
|    - |  492 | `		 * belongs in the LAST slot. It used to be passed as the CREATION time with` |
|    - |  493 | `		 * lastWrite left NULL, so touch($f, $mtime) changed a stamp nothing reads and` |
|    - |  494 | `		 * left filemtime() reporting whatever the file already had. Creation stays` |
|    - |  495 | `		 * untouched, like php. */` |
|    3 |  496 | `		rc = SetFileTime(pHandle,0,&sAccess,&sTouch);` |
|    - |  497 | `		/* Close the handle */` |
|    3 |  498 | `		CloseHandle(pHandle);` |
|    - |  499 | `	}` |
|    3 |  500 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    3 |  501 | `	return rc ? PH7_OK : -1;` |
|    3 |  502 | `}` |
|    - |  503 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  504 | `static ph7_int64 WinVfs_FileAtime(const char *zPath)` |
|    1 |  505 | `{` |
|    1 |  506 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  507 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  508 | `	void * pConverted;` |
|    - |  509 | `	ph7_int64 atime;` |
|    - |  510 | `	HANDLE pHandle;` |
|    1 |  511 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  512 | `	if( pConverted == 0 ){` |
|  ! 0 |  513 | `		return -1;` |
|    - |  514 | `	}` |
|    - |  515 | `	/* Open for a stat read (directories included) */` |
|    1 |  516 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    1 |  517 | `	if( pHandle ){` |
|    - |  518 | `		BOOL rc;` |
|    1 |  519 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  520 | `		if( rc ){` |
|    1 |  521 | `			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);` |
|    1 |  522 | `		}else{` |
|  ! 0 |  523 | `			atime = -1;` |
|    - |  524 | `		}` |
|    1 |  525 | `		CloseHandle(pHandle);` |
|    1 |  526 | `	}else{` |
|    1 |  527 | `		atime = -1;` |
|    - |  528 | `	}` |
|    1 |  529 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  530 | `	return atime;` |
|    1 |  531 | `}` |
|    - |  532 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  533 | `static ph7_int64 WinVfs_FileMtime(const char *zPath)` |
|    3 |  534 | `{` |
|    3 |  535 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  536 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  537 | `	void * pConverted;` |
|    - |  538 | `	ph7_int64 mtime;` |
|    - |  539 | `	HANDLE pHandle;` |
|    3 |  540 | `	pConverted = convertUtf8Filename(zPath);` |
|    3 |  541 | `	if( pConverted == 0 ){` |
|  ! 0 |  542 | `		return -1;` |
|    - |  543 | `	}` |
|    - |  544 | `	/* Open for a stat read (directories included) */` |
|    3 |  545 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    3 |  546 | `	if( pHandle ){` |
|    - |  547 | `		BOOL rc;` |
|    3 |  548 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    3 |  549 | `		if( rc ){` |
|    3 |  550 | `			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);` |
|    3 |  551 | `		}else{` |
|  ! 0 |  552 | `			mtime = -1;` |
|    - |  553 | `		}` |
|    3 |  554 | `		CloseHandle(pHandle);` |
|    3 |  555 | `	}else{` |
|    1 |  556 | `		mtime = -1;` |
|    - |  557 | `	}` |
|    3 |  558 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    3 |  559 | `	return mtime;` |
|    3 |  560 | `}` |
|    - |  561 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  562 | `static ph7_int64 WinVfs_FileCtime(const char *zPath)` |
|    1 |  563 | `{` |
|    1 |  564 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  565 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  566 | `	void * pConverted;` |
|    - |  567 | `	ph7_int64 ctime;` |
|    - |  568 | `	HANDLE pHandle;` |
|    1 |  569 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  570 | `	if( pConverted == 0 ){` |
|  ! 0 |  571 | `		return -1;` |
|    - |  572 | `	}` |
|    - |  573 | `	/* Open for a stat read (directories included) */` |
|    1 |  574 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    1 |  575 | `	if( pHandle ){` |
|    - |  576 | `		BOOL rc;` |
|    1 |  577 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  578 | `		if( rc ){` |
|    1 |  579 | `			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);` |
|    1 |  580 | `		}else{` |
|  ! 0 |  581 | `			ctime = -1;` |
|    - |  582 | `		}` |
|    1 |  583 | `		CloseHandle(pHandle);` |
|    1 |  584 | `	}else{` |
|    1 |  585 | `		ctime = -1;` |
|    - |  586 | `	}` |
|    1 |  587 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  588 | `	return ctime;` |
|    1 |  589 | `}` |
|    - |  590 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  591 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  592 | `static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|    1 |  593 | `{` |
|    1 |  594 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  595 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  596 | `	void *pConverted;` |
|    - |  597 | `	HANDLE pHandle;` |
|    - |  598 | `	BOOL rc;` |
|    1 |  599 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  600 | `	if( pConverted == 0 ){` |
|  ! 0 |  601 | `		return -1;` |
|    - |  602 | `	}` |
|    - |  603 | `	/* Open the file in read-only mode */` |
|    1 |  604 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  605 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  606 | `	if( pHandle == 0 ){` |
|    1 |  607 | `		return -1;` |
|    - |  608 | `	}` |
|    1 |  609 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  610 | `	CloseHandle(pHandle);` |
|    1 |  611 | `	if( !rc ){` |
|  ! 0 |  612 | `		return -1;` |
|    - |  613 | `	}` |
|    - |  614 | `	/* dev */` |
|    1 |  615 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 |  616 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - |  617 | `	/* ino */` |
|    1 |  618 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 |  619 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - |  620 | `	/* mode */` |
|    1 |  621 | `	ph7_value_int(pWorker,0);` |
|    1 |  622 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - |  623 | `	/* nlink */` |
|    1 |  624 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 |  625 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - |  626 | `	/* uid,gid,rdev */` |
|    1 |  627 | `	ph7_value_int(pWorker,0);` |
|    1 |  628 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 |  629 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 |  630 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - |  631 | `	/* size */` |
|    1 |  632 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 |  633 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - |  634 | `	/* atime */` |
|    1 |  635 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 |  636 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - |  637 | `	/* mtime */` |
|    1 |  638 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 |  639 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - |  640 | `	/* ctime */` |
|    1 |  641 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 |  642 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - |  643 | `	/* blksize,blocks */` |
|    1 |  644 | `	ph7_value_int(pWorker,0);` |
|    1 |  645 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 |  646 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 |  647 | `	return PH7_OK;` |
|    1 |  648 | `}` |
|    - |  649 | `/* int (*xIsfile)(const char *) */` |
|    - |  650 | `static int WinVfs_isfile(const char *zPath)` |
|    5 |  651 | `{` |
|    5 |  652 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  653 | `	void * pConverted;` |
|    - |  654 | `	DWORD dwAttr;` |
|    5 |  655 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  656 | `	if( pConverted == 0 ){` |
|  ! 0 |  657 | `		return -1;` |
|    - |  658 | `	}` |
|    5 |  659 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  660 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  661 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  662 | `		return -1;` |
|    - |  663 | `	}` |
|    5 |  664 | `	return (dwAttr & (FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;` |
|    5 |  665 | `}` |
|    - |  666 | `/* int (*xIslink)(const char *) */` |
|    - |  667 | `static int WinVfs_islink(const char *zPath)` |
|    1 |  668 | `{` |
|    1 |  669 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  670 | `	void * pConverted;` |
|    - |  671 | `	DWORD dwAttr;` |
|    1 |  672 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  673 | `	if( pConverted == 0 ){` |
|  ! 0 |  674 | `		return -1;` |
|    - |  675 | `	}` |
|    1 |  676 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  677 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  678 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  679 | `		return -1;` |
|    - |  680 | `	}` |
|    1 |  681 | `	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;` |
|    1 |  682 | `}` |
|    - |  683 | `/* int (*xWritable)(const char *) */` |
|    - |  684 | `static int WinVfs_iswritable(const char *zPath)` |
|    1 |  685 | `{` |
|    1 |  686 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  687 | `	void * pConverted;` |
|    - |  688 | `	DWORD dwAttr;` |
|    1 |  689 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  690 | `	if( pConverted == 0 ){` |
|  ! 0 |  691 | `		return -1;` |
|    - |  692 | `	}` |
|    1 |  693 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  694 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  695 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  696 | `		return -1;` |
|    - |  697 | `	}` |
|    1 |  698 | `	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE\|FILE_ATTRIBUTE_NORMAL)) == 0 ){` |
|    - |  699 | `		/* Not a regular file */` |
|  ! 0 |  700 | `		return -1;` |
|    - |  701 | `	}` |
|    1 |  702 | `	if( dwAttr & FILE_ATTRIBUTE_READONLY ){` |
|    - |  703 | `		/* Read-only file */` |
|  ! 0 |  704 | `		return -1;` |
|    - |  705 | `	}` |
|    - |  706 | `	/* File is writable */` |
|    1 |  707 | `	return PH7_OK;` |
|    1 |  708 | `}` |
|    - |  709 | `/* int (*xExecutable)(const char *) */` |
|    - |  710 | `static int WinVfs_isexecutable(const char *zPath)` |
|    2 |  711 | `{` |
|    2 |  712 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  713 | `	void * pConverted;` |
|    - |  714 | `	DWORD dwAttr;` |
|    2 |  715 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  716 | `	if( pConverted == 0 ){` |
|  ! 0 |  717 | `		return -1;` |
|    - |  718 | `	}` |
|    2 |  719 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    2 |  720 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  721 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    2 |  722 | `		return -1;` |
|    - |  723 | `	}` |
|    1 |  724 | `	if( (dwAttr & FILE_ATTRIBUTE_NORMAL) == 0 ){` |
|    - |  725 | `		/* Not a regular file */` |
|    1 |  726 | `		return -1;` |
|    - |  727 | `	}` |
|    - |  728 | `	/* File is executable */` |
|  ! 0 |  729 | `	return PH7_OK;` |
|    2 |  730 | `}` |
|    - |  731 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  732 | `static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|    1 |  733 | `{` |
|    1 |  734 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  735 | `	void * pConverted;` |
|    - |  736 | `	DWORD dwAttr;` |
|    1 |  737 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  738 | `	if( pConverted == 0 ){` |
|    - |  739 | `		/* Expand 'unknown' */` |
|  ! 0 |  740 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  741 | `		return -1;` |
|    - |  742 | `	}` |
|    1 |  743 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  744 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  745 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    - |  746 | `		/* Expand 'unknown' */` |
|    1 |  747 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    1 |  748 | `		return -1;` |
|    - |  749 | `	}` |
|    1 |  750 | `	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE) ){` |
|    1 |  751 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|    1 |  752 | `	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){` |
|    1 |  753 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|  ! 0 |  754 | `	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){` |
|  ! 0 |  755 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|  ! 0 |  756 | `	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){` |
|  ! 0 |  757 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|  ! 0 |  758 | `	}else{` |
|  ! 0 |  759 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    - |  760 | `	}` |
|    1 |  761 | `	return PH7_OK;` |
|    1 |  762 | `}` |
|    - |  763 | `/*` |
|    - |  764 | ` * int (*xReadlink)(const char *,ph7_context *)` |
|    - |  765 | ` *` |
|    - |  766 | ` * Windows has no readlink(2). php resolves the handle instead` |
|    - |  767 | ` * (GetFinalPathNameByHandleW) and strips the \\?\ prefix it comes back with, so` |
|    - |  768 | ` * the answer here is the target's CANONICAL path where a unix readlink() would` |
|    - |  769 | ` * hand back the link's raw text -- a relative symlink therefore reads back` |
|    - |  770 | ` * absolute on this platform.` |
|    - |  771 | ` */` |
|    - |  772 | `static int WinVfs_Readlink(const char *zPath,ph7_context *pCtx)` |
|    1 |  773 | `{` |
|    - |  774 | `	void *pConverted;` |
|    - |  775 | `	HANDLE pHandle;` |
|    - |  776 | `	WCHAR zTarget[1024];` |
|    - |  777 | `	char zUtf8[1024*4];` |
|    - |  778 | `	DWORD nLen;` |
|    - |  779 | `	int nOut;` |
|    1 |  780 | `	zPath = WinVfsLocalPath(zPath);` |
|    1 |  781 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  782 | `	if( pConverted == 0 ){` |
|  ! 0 |  783 | `		return -1;` |
|    - |  784 | `	}` |
|    - |  785 | `	/* No reparse-point screen: php_win32_ioutil_readlink_w falls back to the` |
|    - |  786 | `	 * handle's final path when the name is not a link, so a plain file reads` |
|    - |  787 | `	 * back as itself on this platform rather than failing. */` |
|    1 |  788 | `	pHandle = CreateFileW((LPCWSTR)pConverted,0,` |
|    - |  789 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_EXISTING,` |
|    - |  790 | `		FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    1 |  791 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  792 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 |  793 | `		return -1;` |
|    - |  794 | `	}` |
|    1 |  795 | `	nLen = GetFinalPathNameByHandleW(pHandle,zTarget,` |
|    - |  796 | `		(DWORD)(sizeof(zTarget)/sizeof(zTarget[0])) - 1,0);` |
|    1 |  797 | `	CloseHandle(pHandle);` |
|    1 |  798 | `	if( nLen < 1 \|\| nLen >= (DWORD)(sizeof(zTarget)/sizeof(zTarget[0])) ){` |
|  ! 0 |  799 | `		return -1;` |
|    - |  800 | `	}` |
|    1 |  801 | `	zTarget[nLen] = 0;` |
|    1 |  802 | `	nOut = WideCharToMultiByte(CP_UTF8,0,zTarget,(int)nLen,zUtf8,(int)sizeof(zUtf8),0,0);` |
|    1 |  803 | `	if( nOut < 1 ){` |
|  ! 0 |  804 | `		return -1;` |
|    - |  805 | `	}` |
|    - |  806 | `	/* Drop the \\?\ prefix the API always prepends, as php does. */` |
|    1 |  807 | `	if( nOut > 4 && zUtf8[0] == '\\' && zUtf8[1] == '\\' && zUtf8[2] == '?' && zUtf8[3] == '\\' ){` |
|    1 |  808 | `		ph7_result_string(pCtx,&zUtf8[4],nOut - 4);` |
|    1 |  809 | `	}else{` |
|  ! 0 |  810 | `		ph7_result_string(pCtx,zUtf8,nOut);` |
|    - |  811 | `	}` |
|    1 |  812 | `	return PH7_OK;` |
|    1 |  813 | `}` |
|    - |  814 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  815 | `static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|    4 |  816 | `{` |
|    - |  817 | `	char zValue[1024];` |
|    4 |  818 | `	char *zBuf = zValue;` |
|    - |  819 | `	DWORD n;` |
|    - |  820 | `	/*` |
|    - |  821 | `	 * According to MSDN, when lpBuffer is not large enough to hold the data the` |
|    - |  822 | `	 * return value is the size REQUIRED (terminator included) and the buffer's` |
|    - |  823 | `	 * contents are UNDEFINED. Handing that length back was a stack over-read of` |
|    - |  824 | `	 * everything past the buffer for any value longer than it -- and on Windows` |
|    - |  825 | `	 * PATH alone routinely is.` |
|    - |  826 | `	 */` |
|    4 |  827 | `	SetLastError(0);` |
|    4 |  828 | `	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));` |
|    4 |  829 | `	if( !n ){` |
|    - |  830 | `		/* 0 is both "absent" and "empty": only the error code tells them apart,` |
|    - |  831 | `		 * and php reports an empty variable as "" the way POSIX does. */` |
|    1 |  832 | `		if( GetLastError() == ERROR_ENVVAR_NOT_FOUND ){` |
|    1 |  833 | `			return -1;` |
|    - |  834 | `		}` |
|    1 |  835 | `		ph7_result_string(pCtx,"",0);` |
|    1 |  836 | `		return PH7_OK;` |
|    - |  837 | `	}` |
|    4 |  838 | `	if( n >= sizeof(zValue) ){` |
|  ! 0 |  839 | `		DWORD nWant = n;` |
|  ! 0 |  840 | `		zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nWant,0,TRUE);` |
|  ! 0 |  841 | `		if( zBuf == 0 ){` |
|  ! 0 |  842 | `			return -1;` |
|    - |  843 | `		}` |
|  ! 0 |  844 | `		n = GetEnvironmentVariableA(zVar,zBuf,nWant);` |
|  ! 0 |  845 | `		if( !n \|\| n >= nWant ){` |
|    - |  846 | `			/* It changed underneath us; report it as absent rather than guess. */` |
|  ! 0 |  847 | `			return -1;` |
|    - |  848 | `		}` |
|    - |  849 | `	}` |
|    4 |  850 | `	ph7_result_string(pCtx,zBuf,(int)n);` |
|    4 |  851 | `	return PH7_OK;` |
|    4 |  852 | `}` |
|    - |  853 | `/* int (*xSetenv)(const char *,const char *) */` |
|    - |  854 | `static int WinVfs_Setenv(const char *zName,const char *zValue)` |
|    1 |  855 | `{` |
|    - |  856 | `	BOOL rc;` |
|    - |  857 | `	/* A NULL value REMOVES the variable, which is php's putenv("NAME") with no` |
|    - |  858 | `	 * '='. An EMPTY value is a variable of its own, on Windows as elsewhere. */` |
|    1 |  859 | `	rc = SetEnvironmentVariableA(zName,zValue);` |
|    1 |  860 | `	return rc ? PH7_OK : -1;` |
|    1 |  861 | `}` |
|    - |  862 | `/* int (*xEnviron)(ph7_context *) */` |
|    - |  863 | `static int WinVfs_Environ(ph7_context *pCtx)` |
|    1 |  864 | `{` |
|    - |  865 | `	ph7_value *pArray,*pKey,*pVal;` |
|    - |  866 | `	LPCH zBlock,zEntry;` |
|    1 |  867 | `	pArray = ph7_context_new_array(pCtx);` |
|    1 |  868 | `	pKey = ph7_context_new_scalar(pCtx);` |
|    1 |  869 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    1 |  870 | `	if( pArray == 0 \|\| pKey == 0 \|\| pVal == 0 ){` |
|  ! 0 |  871 | `		return -1;` |
|    - |  872 | `	}` |
|    1 |  873 | `	zBlock = GetEnvironmentStringsA();` |
|    1 |  874 | `	if( zBlock == 0 ){` |
|  ! 0 |  875 | `		return -1;` |
|    - |  876 | `	}` |
|    1 |  877 | `	for( zEntry = zBlock ; *zEntry ; zEntry += lstrlenA(zEntry) + 1 ){` |
|    1 |  878 | `		const char *zEq = zEntry;` |
|    - |  879 | `		/* The block leads with the "=C:=C:\dir" drive-cursor entries, whose name` |
|    - |  880 | `		 * is empty: php does not report them and neither does this. */` |
|    1 |  881 | `		if( *zEq == '=' ){` |
|    1 |  882 | `			continue;` |
|    - |  883 | `		}` |
|    1 |  884 | `		while( *zEq && *zEq != '=' ){` |
|    1 |  885 | `			zEq++;` |
|    1 |  886 | `		}` |
|    1 |  887 | `		if( *zEq != '=' ){` |
|  ! 0 |  888 | `			continue;` |
|    - |  889 | `		}` |
|    1 |  890 | `		ph7_value_string(pKey,zEntry,(int)(zEq - zEntry));` |
|    1 |  891 | `		ph7_value_string(pVal,zEq+1,-1);` |
|    1 |  892 | `		ph7_array_add_elem(pArray,pKey,pVal);` |
|    1 |  893 | `		ph7_value_reset_string_cursor(pKey);` |
|    1 |  894 | `		ph7_value_reset_string_cursor(pVal);` |
|    1 |  895 | `	}` |
|    1 |  896 | `	FreeEnvironmentStringsA(zBlock);` |
|    1 |  897 | `	ph7_result_value(pCtx,pArray);` |
|    1 |  898 | `	return PH7_OK;` |
|    1 |  899 | `}` |
|    - |  900 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  901 | `static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|    5 |  902 | `{` |
|    - |  903 | `	DWORD dwSizeLow,dwSizeHigh;` |
|    - |  904 | `	HANDLE pHandle,pMapHandle;` |
|    - |  905 | `	void *pConverted,*pView;` |
|    - |  906 |  |
|    5 |  907 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  908 | `	if( pConverted == 0 ){` |
|  ! 0 |  909 | `		return -1;` |
|    - |  910 | `	}` |
|    5 |  911 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    5 |  912 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  913 | `	if( pHandle == 0 ){` |
|  ! 0 |  914 | `		return -1;` |
|    - |  915 | `	}` |
|    - |  916 | `	/* Get the file size */` |
|    5 |  917 | `	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);` |
|    - |  918 | `	/* Create the mapping */` |
|    5 |  919 | `	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);` |
|    5 |  920 | `	if( pMapHandle == 0 ){` |
|  ! 0 |  921 | `		CloseHandle(pHandle);` |
|  ! 0 |  922 | `		return -1;` |
|    - |  923 | `	}` |
|    5 |  924 | `	*pSize = ((ph7_int64)dwSizeHigh << 32) \| dwSizeLow;` |
|    - |  925 | `	/* Obtain the view */` |
|    5 |  926 | `	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));` |
|    5 |  927 | `	if( pView ){` |
|    - |  928 | `		/* Let the upper layer point to the view */` |
|    5 |  929 | `		*ppMap = pView;` |
|    - |  930 | `	}` |
|    - |  931 | `	/* Close the handle` |
|    - |  932 | `	 * According to MSDN it's OK the close the HANDLES.` |
|    - |  933 | `	 */` |
|    5 |  934 | `	CloseHandle(pMapHandle);` |
|    5 |  935 | `	CloseHandle(pHandle);` |
|    5 |  936 | `	return pView ? PH7_OK : -1;` |
|    5 |  937 | `}` |
|    - |  938 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|    - |  939 | `static void WinVfs_Unmap(void *pView,ph7_int64 nSize)` |
|    5 |  940 | `{` |
|    5 |  941 | `	nSize = 0; /* Compiler warning */` |
|    5 |  942 | `	UnmapViewOfFile(pView);` |
|    5 |  943 | `}` |
|    - |  944 | `/* void (*xTempDir)(ph7_context *) */` |
|    - |  945 | `static void WinVfs_TempDir(ph7_context *pCtx)` |
|    5 |  946 | `{` |
|    - |  947 | `	CHAR zTemp[1024];` |
|    - |  948 | `	DWORD n;` |
|    5 |  949 | `	n = GetTempPathA(sizeof(zTemp),zTemp);` |
|    5 |  950 | `	if( n < 1 ){` |
|    - |  951 | `		/* Assume the default windows temp directory */` |
|  ! 0 |  952 | `		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);` |
|  ! 0 |  953 | `	}else{` |
|    - |  954 | `		/* GetTempPath() always ends its answer with a separator and php's` |
|    - |  955 | `		 * sys_get_temp_dir() never does -- the unix side already trims one (see` |
|    - |  956 | `		 * UnixVfs_TempDir), and a caller joining "/name" to this one was getting` |
|    - |  957 | `		 * "...\Temp\/name". */` |
|    5 |  958 | `		while( n > 1 && (zTemp[n-1] == '\\' \|\| zTemp[n-1] == '/') ){` |
|    5 |  959 | `			n--;` |
|    5 |  960 | `		}` |
|    5 |  961 | `		ph7_result_string(pCtx,zTemp,(int)n);` |
|    - |  962 | `	}` |
|    5 |  963 | `}` |
|    - |  964 | `/* unsigned int (*xProcessId)(void) */` |
|    - |  965 | `static unsigned int WinVfs_ProcessId(void)` |
|    5 |  966 | `{` |
|    5 |  967 | `	DWORD nID = 0;` |
|    - |  968 | `#ifndef __MINGW32__` |
|    5 |  969 | `	nID = GetProcessId(GetCurrentProcess());` |
|    - |  970 | `#endif /* __MINGW32__ */` |
|    5 |  971 | `	return (unsigned int)nID;` |
|    5 |  972 | `}` |
|    - |  973 | `/* void (*xUsername)(ph7_context *) */` |
|    - |  974 | `static void WinVfs_Username(ph7_context *pCtx)` |
|    1 |  975 | `{` |
|    - |  976 | `	WCHAR zUser[1024];` |
|    - |  977 | `	DWORD nByte;` |
|    - |  978 | `	BOOL rc;` |
|    1 |  979 | `	nByte = sizeof(zUser);` |
|    1 |  980 | `	rc = GetUserNameW(zUser,&nByte);` |
|    1 |  981 | `	if( !rc ){` |
|    - |  982 | `		/* Set a dummy name */` |
|  ! 0 |  983 | `		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  984 | `	}else{` |
|    - |  985 | `		char *zName;` |
|    1 |  986 | `		zName = unicodeToUtf8(zUser);` |
|    1 |  987 | `		if( zName == 0 ){` |
|  ! 0 |  988 | `			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  989 | `		}else{` |
|    1 |  990 | `			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    1 |  991 | `			HeapFree(GetProcessHeap(),0,zName);` |
|    - |  992 | `		}` |
|    - |  993 | `	}` |
|    - |  994 |  |
|    1 |  995 | `}` |
|    - |  996 | `/* int (*xChroot)(const char *) — Windows has no chroot; fail cleanly so chroot()` |
|    - |  997 | ` * returns false. (php has no chroot symbol at all; PHL exposes it as an extension` |
|    - |  998 | ` * and this reports the failure without a "not implemented in the VFS" warning.) */` |
|    - |  999 | `static int WinVfs_chroot(const char *zPath)` |
|  ! 0 | 1000 | `{` |
|    - | 1001 | `	(void)zPath;` |
|  ! 0 | 1002 | `	return -1;` |
|  ! 0 | 1003 | `}` |
|    - | 1004 | `#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE` |
|    - | 1005 | `#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2` |
|    - | 1006 | `#endif` |
|    - | 1007 | `#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY` |
|    - | 1008 | `#define SYMBOLIC_LINK_FLAG_DIRECTORY 0x1` |
|    - | 1009 | `#endif` |
|    - | 1010 | `/* int (*xLink)(const char *,const char *,int) — hard link (iSym==0) via` |
|    - | 1011 | ` * CreateHardLink or symbolic link (iSym!=0) via CreateSymbolicLink, mirroring` |
|    - | 1012 | ` * php on Windows. Developer-mode symlink creation is allowed. */` |
|    - | 1013 | `static int WinVfs_Link(const char *zOld,const char *zNew,int iSym)` |
|    1 | 1014 | `{` |
|    - | 1015 | `	void *pOld, *pNew;` |
|    - | 1016 | `	BOOL rc;` |
|    1 | 1017 | `	pOld = convertUtf8Filename(zOld);` |
|    1 | 1018 | `	if( pOld == 0 ){` |
|  ! 0 | 1019 | `		return -1;` |
|    - | 1020 | `	}` |
|    1 | 1021 | `	pNew = convertUtf8Filename(zNew);` |
|    1 | 1022 | `	if( pNew == 0 ){` |
|  ! 0 | 1023 | `		HeapFree(GetProcessHeap(),0,pOld);` |
|  ! 0 | 1024 | `		return -1;` |
|    - | 1025 | `	}` |
|    1 | 1026 | `	if( iSym ){` |
|    1 | 1027 | `		DWORD dwFlags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;` |
|    1 | 1028 | `		DWORD attr = GetFileAttributesW((LPCWSTR)pOld);` |
|    1 | 1029 | `		if( attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) ){` |
|  ! 0 | 1030 | `			dwFlags \|= SYMBOLIC_LINK_FLAG_DIRECTORY;` |
|    - | 1031 | `		}` |
|    1 | 1032 | `		rc = CreateSymbolicLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,dwFlags) ? TRUE : FALSE;` |
|    1 | 1033 | `	}else{` |
|    1 | 1034 | `		rc = CreateHardLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,0);` |
|    - | 1035 | `	}` |
|    1 | 1036 | `	HeapFree(GetProcessHeap(),0,pNew);` |
|    1 | 1037 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|    1 | 1038 | `	return rc ? PH7_OK : -1;` |
|    1 | 1039 | `}` |
|    - | 1040 | `/* int (*xUmask)(int) — Windows has no umask; php's umask() returns 0 there. */` |
|    - | 1041 | `static int WinVfs_Umask(int iMask)` |
|    1 | 1042 | `{` |
|    - | 1043 | `	(void)iMask;` |
|    1 | 1044 | `	return 0;` |
|    1 | 1045 | `}` |
|    - | 1046 | `/* int (*xUid)(void) / int (*xGid)(void) — no uid/gid on Windows; php's` |
|    - | 1047 | ` * getmyuid()/getmygid() both return 0. */` |
|    - | 1048 | `static int WinVfs_Uid(void)` |
|    1 | 1049 | `{` |
|    1 | 1050 | `	return 0;` |
|    1 | 1051 | `}` |
|    - | 1052 | `static int WinVfs_Gid(void)` |
|    1 | 1053 | `{` |
|    1 | 1054 | `	return 0;` |
|    1 | 1055 | `}` |
|    - | 1056 | `/* Export the windows vfs */` |
|    - | 1057 | `PH7_PRIVATE const ph7_vfs sWinVfs = {` |
|    - | 1058 | `	"Windows_vfs",` |
|    - | 1059 | `	PH7_VFS_VERSION,` |
|    - | 1060 | `	WinVfs_chdir,    /* int (*xChdir)(const char *) */` |
|    - | 1061 | `	WinVfs_chroot,   /* int (*xChroot)(const char *); */` |
|    - | 1062 | `	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|    - | 1063 | `	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|    - | 1064 | `	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|    - | 1065 | `	WinVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|    - | 1066 | `	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|    - | 1067 | `	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|    - | 1068 | `	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */` |
|    - | 1069 | `	WinVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|    - | 1070 | `	WinVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|    - | 1071 | `	WinVfs_chmod, /*int (*xChmod)(const char *,int)*/` |
|    - | 1072 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|    - | 1073 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|    - | 1074 | `	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - | 1075 | `	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - | 1076 | `	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|    - | 1077 | `	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - | 1078 | `	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - | 1079 | `	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - | 1080 | `	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - | 1081 | `	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - | 1082 | `	WinVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|    - | 1083 | `	WinVfs_islink,     /* int (*xIslink)(const char *) */` |
|    - | 1084 | `	WinVfs_isfile,     /* int (*xReadable)(const char *) */` |
|    - | 1085 | `	WinVfs_iswritable, /* int (*xWritable)(const char *) */` |
|    - | 1086 | `	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */` |
|    - | 1087 | `	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|    - | 1088 | `	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|    - | 1089 | `	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|    - | 1090 | `	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - | 1091 | `	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - | 1092 | `	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|    - | 1093 | `	WinVfs_Link,       /* int (*xLink)(const char *,const char *,int) */` |
|    - | 1094 | `	WinVfs_Umask,      /* int (*xUmask)(int) */` |
|    - | 1095 | `	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|    - | 1096 | `	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|    - | 1097 | `	WinVfs_Uid, /* int (*xUid)(void) */` |
|    - | 1098 | `	WinVfs_Gid, /* int (*xGid)(void) */` |
|    - | 1099 | `	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|    - | 1100 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|    - | 1101 | `	WinVfs_Readlink, /* int (*xReadlink)(const char *,ph7_context *) */` |
|    - | 1102 | `	WinVfs_Environ  /* int (*xEnviron)(ph7_context *) */` |
|    - | 1103 | `};` |
|    - | 1104 | `/* Windows file IO */` |
|    - | 1105 | `#ifndef INVALID_SET_FILE_POINTER` |
|    - | 1106 | `# define INVALID_SET_FILE_POINTER ((DWORD)-1)` |
|    - | 1107 | `#endif` |
|    - | 1108 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|    - | 1109 | `static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|    5 | 1110 | `{` |
|    5 | 1111 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    5 | 1112 | `	DWORD dwAccess = GENERIC_READ;` |
|    - | 1113 | `	DWORD dwShare,dwCreate;` |
|    - | 1114 | `	void *pConverted;` |
|    - | 1115 | `	HANDLE pHandle;` |
|    - | 1116 |  |
|    5 | 1117 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 | 1118 | `	if( pConverted == 0 ){` |
|  ! 0 | 1119 | `		errno = ENOMEM;` |
|  ! 0 | 1120 | `		return -1;` |
|    - | 1121 | `	}` |
|    - | 1122 | `	/* Set the desired flags according to the open mode */` |
|    5 | 1123 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|    - | 1124 | `		/* Open existing file, or create if it doesn't exist */` |
|    5 | 1125 | `		dwCreate = OPEN_ALWAYS;` |
|    5 | 1126 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - | 1127 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|    5 | 1128 | `			dwCreate = CREATE_ALWAYS;` |
|    5 | 1129 | `		}` |
|    5 | 1130 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|    - | 1131 | `		/* Creates a new file, only if it does not already exist.` |
|    - | 1132 | `		* If the file exists, it fails.` |
|    - | 1133 | `		*/` |
|    5 | 1134 | `		dwCreate = CREATE_NEW;` |
|    5 | 1135 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - | 1136 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|    - | 1137 | `		 * The file must exist.` |
|    - | 1138 | `		 */` |
|  ! 0 | 1139 | `		dwCreate = TRUNCATE_EXISTING;` |
|  ! 0 | 1140 | `	}else{` |
|    - | 1141 | `		/* Opens a file, only if it exists. */` |
|    5 | 1142 | `		dwCreate = OPEN_EXISTING;` |
|    - | 1143 | `	}` |
|    5 | 1144 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|    - | 1145 | `		/* Read+Write access */` |
|    5 | 1146 | `		dwAccess \|= GENERIC_WRITE;` |
|    5 | 1147 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|    - | 1148 | `		/* Write only access */` |
|    5 | 1149 | `		dwAccess = GENERIC_WRITE;` |
|    - | 1150 | `	}` |
|    5 | 1151 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|    - | 1152 | `		/* Append mode */` |
|    1 | 1153 | `		dwAccess = FILE_APPEND_DATA;` |
|    - | 1154 | `	}` |
|    5 | 1155 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|    - | 1156 | `		/* File is temporary */` |
|  ! 0 | 1157 | `		dwType = FILE_ATTRIBUTE_TEMPORARY;` |
|    - | 1158 | `	}` |
|    5 | 1159 | `	dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    5 | 1160 | `	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    5 | 1161 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    - | 1162 | `		/* Mapped BEFORE the HeapFree: the caller's warning is worded` |
|    - | 1163 | `		 * "Failed to open stream: %s" from strerror(errno), and CreateFileW` |
|    - | 1164 | `		 * reports through GetLastError() only -- so every failed open on` |
|    - | 1165 | `		 * Windows read "No error" (or, worse, whatever errno an unrelated` |
|    - | 1166 | `		 * earlier call had left behind) where php names the real reason. */` |
|    5 | 1167 | `		WinVfsMapErrno();` |
|    5 | 1168 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|    - | 1169 | `		SXUNUSED(pResource); /* MSVC warning */` |
|    5 | 1170 | `		return -1;` |
|    - | 1171 | `	}` |
|    5 | 1172 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    - | 1173 | `	/* Make the handle accessible to the upper layer */` |
|    5 | 1174 | `	*ppHandle = (void *)pHandle;` |
|    5 | 1175 | `	return PH7_OK;` |
|    5 | 1176 | `}` |
|    - | 1177 | `/* An instance of the following structure is used to record state information` |
|    - | 1178 | ` * while iterating throw directory entries.` |
|    - | 1179 | ` */` |
|    - | 1180 | `typedef struct WinDir_Info WinDir_Info;` |
|    - | 1181 | `struct WinDir_Info` |
|    - | 1182 | `{` |
|    - | 1183 | `	HANDLE pDirHandle;` |
|    - | 1184 | `	void *pPath;` |
|    - | 1185 | `	WIN32_FIND_DATAW sInfo;` |
|    - | 1186 | `	int rc;` |
|    - | 1187 | `};` |
|    - | 1188 | `/* The Win32 code the last failed opendir() stopped on, for the warning php` |
|    - | 1189 | ` * raises from it ahead of its own "Failed to open directory". */` |
|    - | 1190 | `static DWORD dwOpenDirErr = 0;` |
|    - | 1191 | `/*` |
|    - | 1192 | ` * php_win32_docref1_from_error()'s text for the last failed opendir(): the` |
|    - | 1193 | ` * system message with its trailing line breaks and periods stripped, and then` |
|    - | 1194 | ` * two MORE characters cut -- php's own bug, which is why a missing directory is` |
|    - | 1195 | ` * "The system cannot find the file specifi". 0 when there is nothing to say.` |
|    - | 1196 | ` */` |
|    - | 1197 | `PH7_PRIVATE unsigned long PH7_WinOpenDirReason(char *zBuf,int nBuf)` |
|    5 | 1198 | `{` |
|    5 | 1199 | `	WCHAR *zMsg = 0;` |
|    - | 1200 | `	DWORD n;` |
|    - | 1201 | `	int nOut;` |
|    5 | 1202 | `	if( dwOpenDirErr == 0 \|\| nBuf < 1 ){` |
|  ! 0 | 1203 | `		return 0;` |
|    - | 1204 | `	}` |
|    5 | 1205 | `	n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER\|FORMAT_MESSAGE_FROM_SYSTEM\|FORMAT_MESSAGE_IGNORE_INSERTS,` |
|    - | 1206 | `		0,dwOpenDirErr,MAKELANGID(LANG_NEUTRAL,SUBLANG_NEUTRAL),(LPWSTR)&zMsg,0,0);` |
|    5 | 1207 | `	zBuf[0] = 0;` |
|    5 | 1208 | `	if( n > 0 && zMsg ){` |
|    5 | 1209 | `		while( n > 0 && (zMsg[n-1] == L'\r' \|\| zMsg[n-1] == L'\n' \|\| zMsg[n-1] == L'.') ){` |
|    5 | 1210 | `			n--;` |
|    5 | 1211 | `		}` |
|    5 | 1212 | `		nOut = WideCharToMultiByte(CP_UTF8,0,zMsg,(int)n,zBuf,nBuf-1,0,0);` |
|    5 | 1213 | `		if( nOut < 0 ){` |
|  ! 0 | 1214 | `			nOut = 0;` |
|    - | 1215 | `		}` |
|    5 | 1216 | `		zBuf[nOut] = 0;` |
|    5 | 1217 | `		if( nOut >= 2 ){` |
|    5 | 1218 | `			zBuf[nOut-2] = 0;` |
|    - | 1219 | `		}` |
|    - | 1220 | `	}` |
|    5 | 1221 | `	if( zMsg ){` |
|    5 | 1222 | `		LocalFree(zMsg);` |
|    - | 1223 | `	}` |
|    5 | 1224 | `	return (unsigned long)dwOpenDirErr;` |
|    5 | 1225 | `}` |
|    - | 1226 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|    - | 1227 | `static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|    5 | 1228 | `{` |
|    - | 1229 | `	WinDir_Info *pDirInfo;` |
|    - | 1230 | `	void *pConverted;` |
|    - | 1231 | `	char *zPrep;` |
|    - | 1232 | `	sxu32 n;` |
|    5 | 1233 | `	dwOpenDirErr = 0;` |
|    - | 1234 | `	/* Prepare the path */` |
|    5 | 1235 | `	n = SyStrlen(zPath);` |
|    - | 1236 | `	{` |
|    - | 1237 | `		/* php resolves the path BEFORE it lists it, and a name that does not` |
|    - | 1238 | `		 * resolve fails there, with that lookup's code: 2 for a missing leaf, 3` |
|    - | 1239 | `		 * for a missing parent, 267 for a file in the way. Look the path itself` |
|    - | 1240 | `		 * up the same way (a drive root cannot be, and needs no asking). */` |
|    5 | 1241 | `		sxu32 nProbe = n;` |
|    5 | 1242 | `		while( nProbe > 0 && (zPath[nProbe-1] == '/' \|\| zPath[nProbe-1] == '\\') ){` |
|  ! 0 | 1243 | `			nProbe--;` |
|  ! 0 | 1244 | `		}` |
|    5 | 1245 | `		if( nProbe > 0 && zPath[nProbe-1] != ':' ){` |
|    - | 1246 | `			WIN32_FIND_DATAW sProbe;` |
|    - | 1247 | `			HANDLE hProbe;` |
|    5 | 1248 | `			zPrep = (char *)HeapAlloc(GetProcessHeap(),0,nProbe+1);` |
|    5 | 1249 | `			if( zPrep == 0 ){` |
|  ! 0 | 1250 | `				errno = ENOMEM;` |
|  ! 0 | 1251 | `				return -1;` |
|    - | 1252 | `			}` |
|    5 | 1253 | `			SyMemcpy((const void *)zPath,zPrep,nProbe);` |
|    5 | 1254 | `			zPrep[nProbe] = 0;` |
|    5 | 1255 | `			pConverted = convertUtf8Filename(zPrep);` |
|    5 | 1256 | `			HeapFree(GetProcessHeap(),0,zPrep);` |
|    5 | 1257 | `			if( pConverted == 0 ){` |
|  ! 0 | 1258 | `				errno = ENOMEM;` |
|  ! 0 | 1259 | `				return -1;` |
|    - | 1260 | `			}` |
|    5 | 1261 | `			hProbe = FindFirstFileW((LPCWSTR)pConverted,&sProbe);` |
|    5 | 1262 | `			if( hProbe == INVALID_HANDLE_VALUE ){` |
|    5 | 1263 | `				dwOpenDirErr = GetLastError();` |
|    5 | 1264 | `			}else{` |
|    5 | 1265 | `				FindClose(hProbe);` |
|    - | 1266 | `			}` |
|    5 | 1267 | `			HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 | 1268 | `			if( dwOpenDirErr != 0 ){` |
|    5 | 1269 | `				errno = ENOENT;` |
|    5 | 1270 | `				return -1;` |
|    - | 1271 | `			}` |
|    - | 1272 | `		}` |
|    - | 1273 | `	}` |
|    5 | 1274 | `	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);` |
|    5 | 1275 | `	if( zPrep == 0 ){` |
|  ! 0 | 1276 | `		errno = ENOMEM;` |
|  ! 0 | 1277 | `		return -1;` |
|    - | 1278 | `	}` |
|    5 | 1279 | `	SyMemcpy((const void *)zPath,zPrep,n);` |
|    5 | 1280 | `	zPrep[n]   = '\\';` |
|    5 | 1281 | `	zPrep[n+1] =  '*';` |
|    5 | 1282 | `	zPrep[n+2] = 0;` |
|    5 | 1283 | `	pConverted = convertUtf8Filename(zPrep);` |
|    5 | 1284 | `	HeapFree(GetProcessHeap(),0,zPrep);` |
|    5 | 1285 | `	if( pConverted == 0 ){` |
|  ! 0 | 1286 | `		errno = ENOMEM;` |
|  ! 0 | 1287 | `		return -1;` |
|    - | 1288 | `	}` |
|    - | 1289 | `	/* Allocate a new instance */` |
|    5 | 1290 | `	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));` |
|    5 | 1291 | `	if( pDirInfo == 0 ){` |
|  ! 0 | 1292 | `		errno = ENOMEM;` |
|  ! 0 | 1293 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 | 1294 | `		pResource = 0; /* Compiler warning */` |
|  ! 0 | 1295 | `		return -1;` |
|    - | 1296 | `	}` |
|    5 | 1297 | `	pDirInfo->rc = SXRET_OK;` |
|    5 | 1298 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);` |
|    5 | 1299 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|    - | 1300 | `		/* Cannot open directory -- same reason as WinFile_Open above. A path that` |
|    - | 1301 | `		 * resolved but is not a directory is 267, which php reports as ENOENT. */` |
|    1 | 1302 | `		dwOpenDirErr = GetLastError();` |
|    1 | 1303 | `		WinVfsMapErrno();` |
|    1 | 1304 | `		if( dwOpenDirErr == ERROR_DIRECTORY ){` |
|    1 | 1305 | `			errno = ENOENT;` |
|    - | 1306 | `		}` |
|    1 | 1307 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 | 1308 | `		HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    1 | 1309 | `		return -1;` |
|    - | 1310 | `	}` |
|    - | 1311 | `	/* Save the path */` |
|    5 | 1312 | `	pDirInfo->pPath = pConverted;` |
|    - | 1313 | `	/* Save our structure */` |
|    5 | 1314 | `	*ppHandle = pDirInfo;` |
|    5 | 1315 | `	return PH7_OK;` |
|    5 | 1316 | `}` |
|    - | 1317 | `/* void (*xCloseDir)(void *) */` |
|    - | 1318 | `static void WinDir_Close(void *pUserData)` |
|    5 | 1319 | `{` |
|    5 | 1320 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    5 | 1321 | `	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){` |
|    5 | 1322 | `		FindClose(pDirInfo->pDirHandle);` |
|    - | 1323 | `	}` |
|    5 | 1324 | `	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);` |
|    5 | 1325 | `	HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    5 | 1326 | `}` |
|    - | 1327 | `/* void (*xClose)(void *); */` |
|    - | 1328 | `static void WinFile_Close(void *pUserData)` |
|    5 | 1329 | `{` |
|    5 | 1330 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    5 | 1331 | `	CloseHandle(pHandle);` |
|    5 | 1332 | `}` |
|    - | 1333 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|    - | 1334 | `static int WinDir_Read(void *pUserData,ph7_context *pCtx)` |
|    5 | 1335 | `{` |
|    5 | 1336 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    - | 1337 | `	LPWIN32_FIND_DATAW pData;` |
|    - | 1338 | `	char *zName;` |
|    - | 1339 | `	BOOL rc;` |
|    5 | 1340 | `	if( pDirInfo->rc != SXRET_OK ){` |
|    - | 1341 | `		/* No more entry to process */` |
|    5 | 1342 | `		return -1;` |
|    - | 1343 | `	}` |
|    5 | 1344 | `	pData = &pDirInfo->sInfo;` |
|    - | 1345 | `	/* php parity: readdir()/scandir() include the '.' and '..' entries, so unlike` |
|    - | 1346 | `	 * the historical PH7 behaviour we return them instead of skipping. */` |
|    5 | 1347 | `	zName = unicodeToUtf8(pData->cFileName);` |
|    5 | 1348 | `	if( zName == 0 ){` |
|    - | 1349 | `		/* Out of memory */` |
|  ! 0 | 1350 | `		return -1;` |
|    - | 1351 | `	}` |
|    - | 1352 | `	/* Return the current file name */` |
|    5 | 1353 | `	ph7_result_string(pCtx,zName,-1);` |
|    5 | 1354 | `	HeapFree(GetProcessHeap(),0,zName);` |
|    - | 1355 | `	/* Point to the next entry */` |
|    5 | 1356 | `	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|    5 | 1357 | `	if( !rc ){` |
|    5 | 1358 | `		pDirInfo->rc = SXERR_EOF;` |
|    - | 1359 | `	}` |
|    5 | 1360 | `	return PH7_OK;` |
|    5 | 1361 | `}` |
|    - | 1362 | `/* void (*xRewindDir)(void *) */` |
|    - | 1363 | `static void WinDir_RewindDir(void *pUserData)` |
|    2 | 1364 | `{` |
|    2 | 1365 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    2 | 1366 | `	FindClose(pDirInfo->pDirHandle);` |
|    2 | 1367 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);` |
|    2 | 1368 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 | 1369 | `		pDirInfo->rc = SXERR_EOF;` |
|  ! 0 | 1370 | `	}else{` |
|    2 | 1371 | `		pDirInfo->rc = SXRET_OK;` |
|    - | 1372 | `	}` |
|    2 | 1373 | `}` |
|    - | 1374 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|    - | 1375 | `static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)` |
|    5 | 1376 | `{` |
|    5 | 1377 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1378 | `	DWORD nRd;` |
|    - | 1379 | `	BOOL rc;` |
|    5 | 1380 | `	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    5 | 1381 | `	if( !rc ){` |
|    - | 1382 | `		/* EOF or IO error */` |
|    1 | 1383 | `		return -1;` |
|    - | 1384 | `	}` |
|    5 | 1385 | `	return (ph7_int64)nRd;` |
|    5 | 1386 | `}` |
|    - | 1387 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|    - | 1388 | `static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)` |
|    5 | 1389 | `{` |
|    5 | 1390 | `	const char *zData = (const char *)pBuffer;` |
|    5 | 1391 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1392 | `	ph7_int64 nCount;` |
|    - | 1393 | `	DWORD nWr;` |
|    - | 1394 | `	BOOL rc;` |
|    5 | 1395 | `	nWr = 0;` |
|    5 | 1396 | `	nCount = 0;` |
|    - | 1397 | `	for(;;){` |
|    5 | 1398 | `		if( nWrite < 1 ){` |
|    5 | 1399 | `			break;` |
|    - | 1400 | `		}` |
|    5 | 1401 | `		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);` |
|    5 | 1402 | `		if( !rc ){` |
|    - | 1403 | `			/* IO error — surface a POSIX errno for the caller's diagnostic` |
|    - | 1404 | `			 * (e.g. a byte-range lock violation reports EACCES like php). */` |
|    1 | 1405 | `			WinVfsMapErrno();` |
|    1 | 1406 | `			break;` |
|    - | 1407 | `		}` |
|    5 | 1408 | `		nWrite -= nWr;` |
|    5 | 1409 | `		nCount += nWr;` |
|    5 | 1410 | `		zData += nWr;` |
|    5 | 1411 | `	}` |
|    5 | 1412 | `	if( nWrite > 0 ){` |
|    1 | 1413 | `		return -1;` |
|    - | 1414 | `	}` |
|    5 | 1415 | `	return nCount;` |
|    5 | 1416 | `}` |
|    - | 1417 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|    - | 1418 | `static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|    2 | 1419 | `{` |
|    2 | 1420 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1421 | `	DWORD dwMove,dwNew;` |
|    - | 1422 | `	LONG nHighOfft;` |
|    2 | 1423 | `	switch(whence){` |
|    - | 1424 | `	case 1:/*SEEK_CUR*/` |
|    1 | 1425 | `		dwMove = FILE_CURRENT;` |
|    1 | 1426 | `		break;` |
|    - | 1427 | `	case 2: /* SEEK_END */` |
|  ! 0 | 1428 | `		dwMove = FILE_END;` |
|  ! 0 | 1429 | `		break;` |
|    - | 1430 | `	case 0: /* SEEK_SET */` |
|    - | 1431 | `	default:` |
|    2 | 1432 | `		dwMove = FILE_BEGIN;` |
|    - | 1433 | `		break;` |
|    - | 1434 | `	}` |
|    2 | 1435 | `	nHighOfft = (LONG)(iOfft >> 32);` |
|    2 | 1436 | `	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);` |
|    2 | 1437 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1438 | `		return -1;` |
|    - | 1439 | `	}` |
|    2 | 1440 | `	return PH7_OK;` |
|    2 | 1441 | `}` |
|    - | 1442 | `/* int (*xLock)(void *,int) — see the lock_type value space in ph7.h */` |
|    - | 1443 | `static int WinFile_Lock(void *pUserData,int lock_type)` |
|    1 | 1444 | `{` |
|    1 | 1445 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1446 | `	OVERLAPPED sDummy;` |
|    - | 1447 | `	BOOL rc;` |
|    1 | 1448 | `	SyZero(&sDummy,sizeof(sDummy));` |
|    - | 1449 | `	/* Lock/unlock the whole file. php locks the maximal byte range, so the lock` |
|    - | 1450 | `	 * is effective even for an empty or freshly-truncated file — the previous` |
|    - | 1451 | `	 * code locked only GetFileSize() bytes (i.e. nothing for a 0-byte file). */` |
|    1 | 1452 | `	if( lock_type < 0 ){` |
|    - | 1453 | `		/* Unlock the file */` |
|    1 | 1454 | `		rc = UnlockFileEx(pHandle,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);` |
|    1 | 1455 | `	}else{` |
|    - | 1456 | `		/* LOCKFILE_FAIL_IMMEDIATELY belongs to the NON-BLOCKING requests only: it` |
|    - | 1457 | `		 * used to be passed unconditionally, so a plain flock($f,LOCK_EX) answered` |
|    - | 1458 | `		 * false under contention on Windows where php (and POSIX) waits. */` |
|    1 | 1459 | `		DWORD dwFlags = 0;` |
|    1 | 1460 | `		if( lock_type == PH7_IO_LOCK_EX \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    1 | 1461 | `			dwFlags \|= LOCKFILE_EXCLUSIVE_LOCK;` |
|    - | 1462 | `		}` |
|    1 | 1463 | `		if( lock_type == PH7_IO_LOCK_SH_NB \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    1 | 1464 | `			dwFlags \|= LOCKFILE_FAIL_IMMEDIATELY;` |
|    - | 1465 | `		}` |
|    1 | 1466 | `		rc = LockFileEx(pHandle,dwFlags,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);` |
|    1 | 1467 | `		if( !rc && GetLastError() == ERROR_LOCK_VIOLATION ){` |
|    - | 1468 | `			/* Refused because another holder has the range: php's $would_block. */` |
|    1 | 1469 | `			return SXERR_BUSY;` |
|    - | 1470 | `		}` |
|    - | 1471 | `	}` |
|    1 | 1472 | `	return rc ? PH7_OK : -1 /* Lock error */;` |
|    1 | 1473 | `}` |
|    - | 1474 | `/* ph7_int64 (*xTell)(void *) */` |
|    - | 1475 | `static ph7_int64 WinFile_Tell(void *pUserData)` |
|    3 | 1476 | `{` |
|    3 | 1477 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1478 | `	DWORD dwNew;` |
|    3 | 1479 | `	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);` |
|    3 | 1480 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1481 | `		return -1;` |
|    - | 1482 | `	}` |
|    3 | 1483 | `	return (ph7_int64)dwNew;` |
|    3 | 1484 | `}` |
|    - | 1485 | `/* int (*xTrunc)(void *,ph7_int64)` |
|    - | 1486 | ` *` |
|    - | 1487 | ` * SetEndOfFile() truncates at the CURRENT file pointer, so the size has to be` |
|    - | 1488 | ` * seeked to first — and the pointer is then left there. POSIX ftruncate() does` |
|    - | 1489 | ` * not move it, and neither does php on either platform, so the caller's` |
|    - | 1490 | `` * position is saved across the pair: without this, `ftruncate($h,6)` after an`` |
|    - | 1491 | ` * fgets() moved the handle to 6 on Windows and left it where it was everywhere` |
|    - | 1492 | ` * else, and every position-shaped answer after it (ftell, the next read, a` |
|    - | 1493 | ` * write's landing point) diverged by platform.` |
|    - | 1494 | ` */` |
|    - | 1495 | `static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|    1 | 1496 | `{` |
|    1 | 1497 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1498 | `	LONG HighOfft,HighCur;` |
|    - | 1499 | `	DWORD dwNew,dwCur;` |
|    - | 1500 | `	BOOL rc;` |
|    1 | 1501 | `	HighCur = 0;` |
|    1 | 1502 | `	dwCur = SetFilePointer(pHandle,0,&HighCur,FILE_CURRENT);` |
|    1 | 1503 | `	if( dwCur == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ){` |
|  ! 0 | 1504 | `		return -1;` |
|    - | 1505 | `	}` |
|    1 | 1506 | `	HighOfft = (LONG)(nOfft >> 32);` |
|    1 | 1507 | `	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);` |
|    1 | 1508 | `	if( dwNew == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ){` |
|  ! 0 | 1509 | `		return -1;` |
|    - | 1510 | `	}` |
|    1 | 1511 | `	rc = SetEndOfFile(pHandle);` |
|    - | 1512 | `	/* Put the caller back where it was, whether the truncation worked or not. */` |
|    1 | 1513 | `	SetFilePointer(pHandle,(LONG)dwCur,&HighCur,FILE_BEGIN);` |
|    1 | 1514 | `	return rc ? PH7_OK : -1;` |
|    1 | 1515 | `}` |
|    - | 1516 | `/* int (*xSync)(void *); */` |
|    - | 1517 | `static int WinFile_Sync(void *pUserData)` |
|    1 | 1518 | `{` |
|    1 | 1519 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1520 | `	BOOL rc;` |
|    1 | 1521 | `	rc = FlushFileBuffers(pHandle);` |
|    1 | 1522 | `	return rc ? PH7_OK : - 1;` |
|    1 | 1523 | `}` |
|    - | 1524 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|    - | 1525 | `static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 1526 | `{` |
|    - | 1527 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    1 | 1528 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1529 | `	BOOL rc;` |
|    1 | 1530 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 | 1531 | `	if( !rc ){` |
|  ! 0 | 1532 | `		return -1;` |
|    - | 1533 | `	}` |
|    - | 1534 | `	/* dev */` |
|    1 | 1535 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 | 1536 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - | 1537 | `	/* ino */` |
|    1 | 1538 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 | 1539 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - | 1540 | `	/* mode */` |
|    1 | 1541 | `	ph7_value_int(pWorker,0);` |
|    1 | 1542 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - | 1543 | `	/* nlink */` |
|    1 | 1544 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 | 1545 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - | 1546 | `	/* uid,gid,rdev */` |
|    1 | 1547 | `	ph7_value_int(pWorker,0);` |
|    1 | 1548 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 | 1549 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 | 1550 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - | 1551 | `	/* size */` |
|    1 | 1552 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 | 1553 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - | 1554 | `	/* atime */` |
|    1 | 1555 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 | 1556 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - | 1557 | `	/* mtime */` |
|    1 | 1558 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 | 1559 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - | 1560 | `	/* ctime */` |
|    1 | 1561 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 | 1562 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - | 1563 | `	/* blksize,blocks */` |
|    1 | 1564 | `	ph7_value_int(pWorker,0);` |
|    1 | 1565 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 | 1566 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 | 1567 | `	return PH7_OK;` |
|    1 | 1568 | `}` |
|    - | 1569 | `/* Export the file:// stream */` |
|    - | 1570 | `/*` |
|    - | 1571 | ` * php copies a plain file with MapViewOfFile(), and a view has to START on an` |
|    - | 1572 | ` * allocation granule, so it maps from the granule the position sits in. At the` |
|    - | 1573 | ` * end of the file that is a view of zero requested bytes -- which php's copy` |
|    - | 1574 | ` * loop reads as a failure -- unless the end sits ON a granule (an empty file` |
|    - | 1575 | ` * included), where php refuses the zero-length view and its read loop answers` |
|    - | 1576 | ` * 0. Linux's mmap() refuses every zero-length map, so there it is always 0.` |
|    - | 1577 | ` * nAhead is what the handle has read ahead of the script's position.` |
|    - | 1578 | ` */` |
|    - | 1579 | `PH7_PRIVATE int PH7_WinFileMapsEmptyView(void *pHandle,ph7_int64 nAhead)` |
|    1 | 1580 | `{` |
|    - | 1581 | `	LARGE_INTEGER iSize,iZero,iPos;` |
|    - | 1582 | `	SYSTEM_INFO sInfo;` |
|    1 | 1583 | `	iZero.QuadPart = 0;` |
|    - | 1584 | `	if( !GetFileSizeEx((HANDLE)pHandle,&iSize)` |
|    - | 1585 | `	 \|\| !SetFilePointerEx((HANDLE)pHandle,iZero,&iPos,FILE_CURRENT)` |
|    1 | 1586 | `	 \|\| iPos.QuadPart - nAhead < iSize.QuadPart ){` |
|    1 | 1587 | `		return 0;` |
|    - | 1588 | `	}` |
|    1 | 1589 | `	GetSystemInfo(&sInfo);` |
|    1 | 1590 | `	return sInfo.dwAllocationGranularity != 0` |
|    - | 1591 | `		&& (iSize.QuadPart % (ph7_int64)sInfo.dwAllocationGranularity) != 0;` |
|    1 | 1592 | `}` |
|    - | 1593 | `PH7_PRIVATE const ph7_io_stream sWinFileStream = {` |
|    - | 1594 | `	"file", /* Stream name */` |
|    - | 1595 | `	PH7_IO_STREAM_VERSION,` |
|    - | 1596 | `	WinFile_Open,  /* xOpen */` |
|    - | 1597 | `	WinDir_Open,   /* xOpenDir */` |
|    - | 1598 | `	WinFile_Close, /* xClose */` |
|    - | 1599 | `	WinDir_Close,  /* xCloseDir */` |
|    - | 1600 | `	WinFile_Read,  /* xRead */` |
|    - | 1601 | `	WinDir_Read,   /* xReadDir */` |
|    - | 1602 | `	WinFile_Write, /* xWrite */` |
|    - | 1603 | `	WinFile_Seek,  /* xSeek */` |
|    - | 1604 | `	WinFile_Lock,  /* xLock */` |
|    - | 1605 | `	WinDir_RewindDir, /* xRewindDir */` |
|    - | 1606 | `	WinFile_Tell,  /* xTell */` |
|    - | 1607 | `	WinFile_Trunc, /* xTrunc */` |
|    - | 1608 | `	WinFile_Sync,  /* xSeek */` |
|    - | 1609 | `	WinFile_Stat   /* xStat */` |
|    - | 1610 | `};` |
|    - | 1611 | `#endif /* __WINNT__ */` |
|    - | 1612 |  |
