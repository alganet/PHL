# src/ph7/vfs_win.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 764/876 lines (87.21%)

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
|    4 |   96 | `		case ERROR_ALREADY_EXISTS:    errno = EEXIST; break;` |
|    1 |   97 | `		case ERROR_DIR_NOT_EMPTY:     errno = ENOTEMPTY; break;` |
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
|    5 |  191 | `		return -1;` |
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
|    5 |  217 | `{` |
|    - |  218 | `	WCHAR zTemp[2048];` |
|    - |  219 | `	void *pPath;` |
|    - |  220 | `	char *zReal;` |
|    - |  221 | `	DWORD n;` |
|    5 |  222 | `	pPath = convertUtf8Filename(zPath);` |
|    5 |  223 | `	if( pPath == 0 ){` |
|  ! 0 |  224 | `		return -1;` |
|    - |  225 | `	}` |
|    5 |  226 | `	n = GetFullPathNameW((LPCWSTR)pPath,0,0,0);` |
|    5 |  227 | `	if( n > 0 ){` |
|    5 |  228 | `		if( n >= sizeof(zTemp) ){` |
|  ! 0 |  229 | `			n = sizeof(zTemp) - 1;` |
|    - |  230 | `		}` |
|    5 |  231 | `		GetFullPathNameW((LPCWSTR)pPath,n,zTemp,0);` |
|    - |  232 | `	}` |
|    - |  233 | `	/* GetFullPathNameW only NORMALIZES -- it answers happily for a path that does` |
|    - |  234 | `	 * not exist, and php's realpath() is false for one on every platform. */` |
|    5 |  235 | `	if( n > 0 && GetFileAttributesW((LPCWSTR)pPath) == INVALID_FILE_ATTRIBUTES ){` |
|    2 |  236 | `		n = 0;` |
|    - |  237 | `	}` |
|    5 |  238 | `	HeapFree(GetProcessHeap(),0,pPath);` |
|    5 |  239 | `	if( !n ){` |
|    2 |  240 | `		return -1;` |
|    - |  241 | `	}` |
|    5 |  242 | `	zReal = unicodeToUtf8(zTemp);` |
|    5 |  243 | `	if( zReal == 0 ){` |
|  ! 0 |  244 | `		return -1;` |
|    - |  245 | `	}` |
|    - |  246 | `	{` |
|    - |  247 | `		/* GetFullPathNameW KEEPS a trailing separator where php's realpath()` |
|    - |  248 | `		 * drops it everywhere but a drive root -- and so must this, because` |
|    - |  249 | `		 * realpath($d . '/') has to name the same string as realpath($d). */` |
|    5 |  250 | `		sxu32 nReal = SyStrlen(zReal);` |
|    5 |  251 | `		while( nReal > 3 && (zReal[nReal-1] == '\\' \|\| zReal[nReal-1] == '/') ){` |
|    1 |  252 | `			nReal--;` |
|    1 |  253 | `		}` |
|    5 |  254 | `		ph7_result_string(pCtx,zReal,(int)nReal); /* Will make it's own copy */` |
|    - |  255 | `	}` |
|    5 |  256 | `	HeapFree(GetProcessHeap(),0,zReal);` |
|    5 |  257 | `	return PH7_OK;` |
|    5 |  258 | `}` |
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
|    4 |  384 | `{` |
|    - |  385 | `	HANDLE pHandle;` |
|    4 |  386 | `	pHandle = CreateFileW(pPath,FILE_READ_ATTRIBUTES,` |
|    - |  387 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_EXISTING,` |
|    - |  388 | `		FILE_ATTRIBUTE_NORMAL\|FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    4 |  389 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|    3 |  390 | `		return 0;` |
|    - |  391 | `	}` |
|    4 |  392 | `	return pHandle;` |
|    4 |  393 | `}` |
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
|  ! 0 |  404 | `		return 0;` |
|    - |  405 | `	}` |
|    5 |  406 | `	return pHandle;` |
|    5 |  407 | `}` |
|    - |  408 | `/*` |
|    - |  409 | ` * Does this path NAME an executable? Windows has no execute permission, so php` |
|    - |  410 | ` * reads one off the extension, matched case-insensitively against the END of` |
|    - |  411 | `` * the name -- `dot.exe.txt` is not one, and a file called exactly `.exe` is.`` |
|    - |  412 | ` *` |
|    - |  413 | ` * php uses TWO different lists for it, and they disagree: the stat MODE takes` |
|    - |  414 | `` * `.exe`, `.com`, `.bat` and `.cmd`, while is_executable() takes `.exe` and`` |
|    - |  415 | `` * `.com` alone -- so `a.bat` stats as 33279, execute bits and all, and`` |
|    - |  416 | `` * `is_executable('a.bat')` is false. Both read off php 8.5.8 on the gate guest;`` |
|    - |  417 | ` * bScripts is which of the two lists is being asked.` |
|    - |  418 | ` */` |
|    - |  419 | `static int WinPathIsExec(const char *zPath,int bScripts)` |
|    2 |  420 | `{` |
|    - |  421 | `	static const char * const azExt[] = { ".exe", ".com", ".bat", ".cmd" };` |
|    - |  422 | `	sxu32 n,i,nExt;` |
|    2 |  423 | `	if( zPath == 0 ){` |
|    1 |  424 | `		return 0;` |
|    - |  425 | `	}` |
|    2 |  426 | `	n = SyStrlen(zPath);` |
|    2 |  427 | `	if( n < 4 ){` |
|  ! 0 |  428 | `		return 0;` |
|    - |  429 | `	}` |
|    2 |  430 | `	nExt = bScripts ? (sxu32)SX_ARRAYSIZE(azExt) : 2;` |
|    2 |  431 | `	for( i = 0 ; i < nExt ; ++i ){` |
|    2 |  432 | `		if( SyStrnicmp(&zPath[n-4],azExt[i],4) == 0 ){` |
|  ! 0 |  433 | `			return 1;` |
|    - |  434 | `		}` |
|    2 |  435 | `	}` |
|    2 |  436 | `	return 0;` |
|    2 |  437 | `}` |
|    - |  438 | `/*` |
|    - |  439 | `` * The `mode` of a Windows stat answer. Windows keeps no such field, so php`` |
|    - |  440 | ` * SYNTHESISES one from the file ATTRIBUTES: a directory is S_IFDIR with the` |
|    - |  441 | ` * three execute bits, anything else is S_IFREG, the read-only attribute is the` |
|    - |  442 | ` * difference between 0444 and 0666, and an executable NAME adds the execute` |
|    - |  443 | ` * bits back. php 8.5.8 on the gate guest, over the whole table: 33206 for an` |
|    - |  444 | ` * ordinary file, 33060 read-only, 33279 executable, 33133 read-only and` |
|    - |  445 | ` * executable, 16895 for a directory and 16749 for a read-only one.` |
|    - |  446 | ` *` |
|    - |  447 | ``  * PHL reported 0 for every one of them, so `($st['mode'] & 0170000) == 0100000` `` |
|    - |  448 | ` * -- how a portable is-this-a-file is written -- was false for everything, and` |
|    - |  449 | ` * fileperms() answered 0 on a whole platform.` |
|    - |  450 | ` *` |
|    - |  451 | ` * zPath is NULL where there is no name to read: fstat() has only a handle, and` |
|    - |  452 | ` * php's answer there carries no execute bit even for an .exe.` |
|    - |  453 | ` */` |
|    - |  454 | `static int WinStatMode(DWORD dwAttr,const char *zPath)` |
|    3 |  455 | `{` |
|    - |  456 | `	int iMode;` |
|    3 |  457 | `	if( dwAttr & FILE_ATTRIBUTE_DIRECTORY ){` |
|    3 |  458 | `		iMode = 0040000 \| 0111;   /* S_IFDIR, and a directory is always enterable */` |
|    3 |  459 | `	}else{` |
|    2 |  460 | `		iMode = 0100000;          /* S_IFREG */` |
|    - |  461 | `	}` |
|    3 |  462 | `	iMode \|= (dwAttr & FILE_ATTRIBUTE_READONLY) ? 0444 : 0666;` |
|    3 |  463 | `	if( (dwAttr & FILE_ATTRIBUTE_DIRECTORY) == 0 && WinPathIsExec(zPath,TRUE) ){` |
|  ! 0 |  464 | `		iMode \|= 0111;` |
|    - |  465 | `	}` |
|    3 |  466 | `	return iMode;` |
|    3 |  467 | `}` |
|    - |  468 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  469 | `static ph7_int64 WinVfs_FileSize(const char *zPath)` |
|    4 |  470 | `{` |
|    4 |  471 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  472 | `	DWORD dwLow,dwHigh;` |
|    - |  473 | `	void * pConverted;` |
|    - |  474 | `	ph7_int64 nSize;` |
|    - |  475 | `	HANDLE pHandle;` |
|    - |  476 |  |
|    4 |  477 | `	pConverted = convertUtf8Filename(zPath);` |
|    4 |  478 | `	if( pConverted == 0 ){` |
|  ! 0 |  479 | `		return -1;` |
|    - |  480 | `	}` |
|    - |  481 | `	/* Through the STAT opener: a DIRECTORY has a size on Windows (php reports` |
|    - |  482 | `	 * its allocation -- 16384 for C:\Windows) and a plain read-only open cannot` |
|    - |  483 | `	 * get a handle to one at all, so filesize() answered false for every` |
|    - |  484 | `	 * directory here. */` |
|    4 |  485 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    4 |  486 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    4 |  487 | `	if( pHandle ){` |
|    4 |  488 | `		dwLow = GetFileSize(pHandle,&dwHigh);` |
|    4 |  489 | `		nSize = dwHigh;` |
|    4 |  490 | `		nSize <<= 32;` |
|    4 |  491 | `		nSize += dwLow;` |
|    4 |  492 | `		CloseHandle(pHandle);` |
|    4 |  493 | `	}else{` |
|    2 |  494 | `		nSize = -1;` |
|    - |  495 | `	}` |
|    4 |  496 | `	return nSize;` |
|    4 |  497 | `}` |
|    - |  498 | `#define TICKS_PER_SECOND 10000000` |
|    - |  499 | `#define EPOCH_DIFFERENCE 11644473600LL` |
|    - |  500 | `/* Convert Windows timestamp to UNIX timestamp */` |
|    - |  501 | `static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)` |
|    4 |  502 | `{` |
|    - |  503 | `    ph7_int64 input,temp;` |
|    4 |  504 | `	input = pTime->dwHighDateTime;` |
|    4 |  505 | `	input <<= 32;` |
|    4 |  506 | `	input += pTime->dwLowDateTime;` |
|    4 |  507 | `    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/` |
|    4 |  508 | `    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/` |
|    4 |  509 | `    return temp;` |
|    4 |  510 | `}` |
|    - |  511 | `/* Convert UNIX timestamp to Windows timestamp */` |
|    - |  512 | `static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)` |
|    4 |  513 | `{` |
|    - |  514 | ``  /* The converted value has to be the one that is STORED: this computed `result` and`` |
|    - |  515 | `   * then wrote the raw unix seconds into the FILETIME, so a requested 1000000000` |
|    - |  516 | `   * landed as 100 seconds past the 1601 epoch and read back as -11644473500. */` |
|    4 |  517 | `  ph7_int64 result = EPOCH_DIFFERENCE;` |
|    4 |  518 | `  result += nUnixtime;` |
|    4 |  519 | `  result *= TICKS_PER_SECOND;` |
|    4 |  520 | `  pOut->dwHighDateTime = (DWORD)((sxu64)result >> 32);` |
|    4 |  521 | `  pOut->dwLowDateTime = (DWORD)((sxu64)result & 0xFFFFFFFFu);` |
|    4 |  522 | `}` |
|    - |  523 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  524 | `static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|    4 |  525 | `{` |
|    - |  526 | `	FILETIME sTouch,sAccess;` |
|    - |  527 | `	void *pConverted;` |
|    - |  528 | `	void *pHandle;` |
|    4 |  529 | `	BOOL rc = 0;` |
|    - |  530 | `	/* Accept file:// like every other path-taking entry in this VFS (the POSIX` |
|    - |  531 | `	 * driver already does) — this was the only one that skipped the mapping. */` |
|    4 |  532 | `	zPath = WinVfsLocalPath(zPath);` |
|    4 |  533 | `	pConverted = convertUtf8Filename(zPath);` |
|    4 |  534 | `	if( pConverted == 0 ){` |
|  ! 0 |  535 | `		return -1;` |
|    - |  536 | `	}` |
|    - |  537 | `	/* php's touch() CREATES a missing file (OPEN_ALWAYS), and SetFileTime needs write` |
|    - |  538 | `	 * access — the read-only handle this used could not stamp an existing file either.` |
|    - |  539 | `	 * FILE_FLAG_BACKUP_SEMANTICS is what lets a DIRECTORY be opened at all (php's` |
|    - |  540 | `	 * win32 utime passes it for the same reason, and the POSIX driver's utime() works` |
|    - |  541 | `	 * on directories); it does not change what OPEN_ALWAYS creates for a missing path.` |
|    - |  542 | `	 * Mirrors the POSIX driver's utime + open(O_CREAT) fallback. */` |
|    4 |  543 | `	pHandle = CreateFileW((LPCWSTR)pConverted,FILE_WRITE_ATTRIBUTES,` |
|    - |  544 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_ALWAYS,` |
|    - |  545 | `		FILE_ATTRIBUTE_NORMAL\|FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    4 |  546 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|    - |  547 | `		/* The caller reports strerror(errno), and this API sets GetLastError()` |
|    - |  548 | `		 * instead -- so a failed touch() read whatever errno already held` |
|    - |  549 | `		 * ("No error" for a path that does not exist). */` |
|    1 |  550 | `		WinVfsMapErrno();` |
|    1 |  551 | `		pHandle = 0;` |
|    - |  552 | `	}` |
|    4 |  553 | `	if( pHandle ){` |
|    - |  554 | `		/* Both stamps are real values: the builtin resolves php's "now" default, so a` |
|    - |  555 | `		 * NEGATIVE timestamp (legal to php) is no longer read as "not given". */` |
|    4 |  556 | `		convertUnixTimeToWindowsTime(touch_time,&sTouch);` |
|    4 |  557 | `		convertUnixTimeToWindowsTime(access_time,&sAccess);` |
|    - |  558 | `		/* SetFileTime(hFile, creation, lastAccess, lastWrite): the modification stamp` |
|    - |  559 | `		 * belongs in the LAST slot. It used to be passed as the CREATION time with` |
|    - |  560 | `		 * lastWrite left NULL, so touch($f, $mtime) changed a stamp nothing reads and` |
|    - |  561 | `		 * left filemtime() reporting whatever the file already had. Creation stays` |
|    - |  562 | `		 * untouched, like php. */` |
|    4 |  563 | `		rc = SetFileTime(pHandle,0,&sAccess,&sTouch);` |
|    4 |  564 | `		if( !rc ){` |
|  ! 0 |  565 | `			WinVfsMapErrno();` |
|    - |  566 | `		}` |
|    - |  567 | `		/* Close the handle */` |
|    4 |  568 | `		CloseHandle(pHandle);` |
|    - |  569 | `	}` |
|    4 |  570 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    4 |  571 | `	return rc ? PH7_OK : -1;` |
|    4 |  572 | `}` |
|    - |  573 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  574 | `static ph7_int64 WinVfs_FileAtime(const char *zPath)` |
|    1 |  575 | `{` |
|    1 |  576 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  577 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  578 | `	void * pConverted;` |
|    - |  579 | `	ph7_int64 atime;` |
|    - |  580 | `	HANDLE pHandle;` |
|    1 |  581 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  582 | `	if( pConverted == 0 ){` |
|  ! 0 |  583 | `		return -1;` |
|    - |  584 | `	}` |
|    - |  585 | `	/* Open for a stat read (directories included) */` |
|    1 |  586 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    1 |  587 | `	if( pHandle ){` |
|    - |  588 | `		BOOL rc;` |
|    1 |  589 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  590 | `		if( rc ){` |
|    1 |  591 | `			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);` |
|    1 |  592 | `		}else{` |
|  ! 0 |  593 | `			atime = -1;` |
|    - |  594 | `		}` |
|    1 |  595 | `		CloseHandle(pHandle);` |
|    1 |  596 | `	}else{` |
|    1 |  597 | `		atime = -1;` |
|    - |  598 | `	}` |
|    1 |  599 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  600 | `	return atime;` |
|    1 |  601 | `}` |
|    - |  602 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  603 | `static ph7_int64 WinVfs_FileMtime(const char *zPath)` |
|    4 |  604 | `{` |
|    4 |  605 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  606 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  607 | `	void * pConverted;` |
|    - |  608 | `	ph7_int64 mtime;` |
|    - |  609 | `	HANDLE pHandle;` |
|    4 |  610 | `	pConverted = convertUtf8Filename(zPath);` |
|    4 |  611 | `	if( pConverted == 0 ){` |
|  ! 0 |  612 | `		return -1;` |
|    - |  613 | `	}` |
|    - |  614 | `	/* Open for a stat read (directories included) */` |
|    4 |  615 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    4 |  616 | `	if( pHandle ){` |
|    - |  617 | `		BOOL rc;` |
|    4 |  618 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    4 |  619 | `		if( rc ){` |
|    4 |  620 | `			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);` |
|    4 |  621 | `		}else{` |
|  ! 0 |  622 | `			mtime = -1;` |
|    - |  623 | `		}` |
|    4 |  624 | `		CloseHandle(pHandle);` |
|    4 |  625 | `	}else{` |
|    1 |  626 | `		mtime = -1;` |
|    - |  627 | `	}` |
|    4 |  628 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    4 |  629 | `	return mtime;` |
|    4 |  630 | `}` |
|    - |  631 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  632 | `static ph7_int64 WinVfs_FileCtime(const char *zPath)` |
|    1 |  633 | `{` |
|    1 |  634 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  635 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  636 | `	void * pConverted;` |
|    - |  637 | `	ph7_int64 ctime;` |
|    - |  638 | `	HANDLE pHandle;` |
|    1 |  639 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  640 | `	if( pConverted == 0 ){` |
|  ! 0 |  641 | `		return -1;` |
|    - |  642 | `	}` |
|    - |  643 | `	/* Open for a stat read (directories included) */` |
|    1 |  644 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    1 |  645 | `	if( pHandle ){` |
|    - |  646 | `		BOOL rc;` |
|    1 |  647 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  648 | `		if( rc ){` |
|    1 |  649 | `			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);` |
|    1 |  650 | `		}else{` |
|  ! 0 |  651 | `			ctime = -1;` |
|    - |  652 | `		}` |
|    1 |  653 | `		CloseHandle(pHandle);` |
|    1 |  654 | `	}else{` |
|    1 |  655 | `		ctime = -1;` |
|    - |  656 | `	}` |
|    1 |  657 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  658 | `	return ctime;` |
|    1 |  659 | `}` |
|    - |  660 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  661 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  662 | `static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|    3 |  663 | `{` |
|    3 |  664 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  665 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  666 | `	void *pConverted;` |
|    - |  667 | `	HANDLE pHandle;` |
|    - |  668 | `	BOOL rc;` |
|    3 |  669 | `	pConverted = convertUtf8Filename(zPath);` |
|    3 |  670 | `	if( pConverted == 0 ){` |
|  ! 0 |  671 | `		return -1;` |
|    - |  672 | `	}` |
|    - |  673 | `	/* Through the STAT opener: a plain read-only open cannot get a handle to a` |
|    - |  674 | `	 * DIRECTORY, so stat() answered FALSE for every one of them here -- and` |
|    - |  675 | `	 * with it fileperms(), fileowner(), filegroup() and fileinode(). */` |
|    3 |  676 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    3 |  677 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    3 |  678 | `	if( pHandle == 0 ){` |
|    3 |  679 | `		return -1;` |
|    - |  680 | `	}` |
|    3 |  681 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    3 |  682 | `	CloseHandle(pHandle);` |
|    3 |  683 | `	if( !rc ){` |
|  ! 0 |  684 | `		return -1;` |
|    - |  685 | `	}` |
|    - |  686 | `	/* dev */` |
|    3 |  687 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    3 |  688 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - |  689 | `	/* ino */` |
|    3 |  690 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    3 |  691 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - |  692 | `	/* mode */` |
|    3 |  693 | `	ph7_value_int(pWorker,WinStatMode(sInfo.dwFileAttributes,zPath));` |
|    3 |  694 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - |  695 | `	/* nlink */` |
|    3 |  696 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    3 |  697 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - |  698 | `	/* uid,gid,rdev */` |
|    3 |  699 | `	ph7_value_int(pWorker,0);` |
|    3 |  700 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    3 |  701 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    3 |  702 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - |  703 | `	/* size */` |
|    3 |  704 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    3 |  705 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - |  706 | `	/* atime */` |
|    3 |  707 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    3 |  708 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - |  709 | `	/* mtime */` |
|    3 |  710 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    3 |  711 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - |  712 | `	/* ctime */` |
|    3 |  713 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    3 |  714 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - |  715 | `	/* blksize,blocks: php reports -1 for both on Windows -- on every file and` |
|    - |  716 | `	 * every stream -- having neither field to fill them from. */` |
|    3 |  717 | `	ph7_value_int(pWorker,-1);` |
|    3 |  718 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    3 |  719 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    3 |  720 | `	return PH7_OK;` |
|    3 |  721 | `}` |
|    - |  722 | `/* int (*xIsfile)(const char *) */` |
|    - |  723 | `static int WinVfs_isfile(const char *zPath)` |
|    5 |  724 | `{` |
|    5 |  725 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  726 | `	void * pConverted;` |
|    - |  727 | `	DWORD dwAttr;` |
|    5 |  728 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  729 | `	if( pConverted == 0 ){` |
|  ! 0 |  730 | `		return -1;` |
|    - |  731 | `	}` |
|    5 |  732 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  733 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  734 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  735 | `		return -1;` |
|    - |  736 | `	}` |
|    5 |  737 | `	return (dwAttr & (FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;` |
|    5 |  738 | `}` |
|    - |  739 | `/* int (*xIslink)(const char *) */` |
|    - |  740 | `static int WinVfs_islink(const char *zPath)` |
|    2 |  741 | `{` |
|    2 |  742 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  743 | `	void * pConverted;` |
|    - |  744 | `	DWORD dwAttr;` |
|    2 |  745 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  746 | `	if( pConverted == 0 ){` |
|  ! 0 |  747 | `		return -1;` |
|    - |  748 | `	}` |
|    2 |  749 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    2 |  750 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  751 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  752 | `		return -1;` |
|    - |  753 | `	}` |
|    2 |  754 | `	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;` |
|    2 |  755 | `}` |
|    - |  756 | `/* int (*xWritable)(const char *) */` |
|    - |  757 | `static int WinVfs_iswritable(const char *zPath)` |
|    5 |  758 | `{` |
|    5 |  759 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  760 | `	void * pConverted;` |
|    - |  761 | `	DWORD dwAttr;` |
|    5 |  762 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  763 | `	if( pConverted == 0 ){` |
|  ! 0 |  764 | `		return -1;` |
|    - |  765 | `	}` |
|    5 |  766 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  767 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  768 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  769 | `		return -1;` |
|    - |  770 | `	}` |
|    5 |  771 | `	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_DIRECTORY)) == 0 ){` |
|    - |  772 | `		/* Not a regular file and not a directory. A DIRECTORY is the one this` |
|    - |  773 | `		 * used to refuse and php answers TRUE for: php asks _waccess(W_OK),` |
|    - |  774 | `		 * which on Windows is the read-only attribute and nothing else, so` |
|    - |  775 | ``		 * `is_writable(sys_get_temp_dir())` was false here and true there. */`` |
|  ! 0 |  776 | `		return -1;` |
|    - |  777 | `	}` |
|    5 |  778 | `	if( dwAttr & FILE_ATTRIBUTE_READONLY ){` |
|    - |  779 | `		/* Read-only file. Windows keeps the bit on directories too and ignores` |
|    - |  780 | `		 * it there, which is why php's answer for one is effectively "yes";` |
|    - |  781 | `		 * this follows _waccess rather than second-guessing it. */` |
|    1 |  782 | `		return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? PH7_OK : -1;` |
|    - |  783 | `	}` |
|    - |  784 | `	/* File is writable */` |
|    5 |  785 | `	return PH7_OK;` |
|    5 |  786 | `}` |
|    - |  787 | `/* int (*xExecutable)(const char *) */` |
|    - |  788 | `static int WinVfs_isexecutable(const char *zPath)` |
|    2 |  789 | `{` |
|    2 |  790 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  791 | `	void * pConverted;` |
|    - |  792 | `	DWORD dwAttr;` |
|    2 |  793 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  794 | `	if( pConverted == 0 ){` |
|  ! 0 |  795 | `		return -1;` |
|    - |  796 | `	}` |
|    2 |  797 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    2 |  798 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  799 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    2 |  800 | `		return -1;` |
|    - |  801 | `	}` |
|    1 |  802 | `	if( dwAttr & FILE_ATTRIBUTE_DIRECTORY ){` |
|    - |  803 | `		/* php answers FALSE for a directory on Windows whatever its mode bits` |
|    - |  804 | `		 * say -- and its mode bits DO carry the execute triplet. */` |
|  ! 0 |  805 | `		return -1;` |
|    - |  806 | `	}` |
|    - |  807 | `	/* Windows has no execute permission: php reads one off the NAME, and this` |
|    - |  808 | `	 * used to test FILE_ATTRIBUTE_NORMAL -- a flag no real file carries -- so` |
|    - |  809 | `	 * is_executable() was false for everything on the platform. The list here` |
|    - |  810 | ``	 * is php's SHORTER one: a `.bat` carries the mode's execute bits and is`` |
|    - |  811 | `	 * still not executable to this question. */` |
|    1 |  812 | `	return WinPathIsExec(zPath,FALSE) ? PH7_OK : -1;` |
|    2 |  813 | `}` |
|    - |  814 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  815 | `static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|    1 |  816 | `{` |
|    1 |  817 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  818 | `	void * pConverted;` |
|    - |  819 | `	DWORD dwAttr;` |
|    1 |  820 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  821 | `	if( pConverted == 0 ){` |
|    - |  822 | `		/* Expand 'unknown' */` |
|  ! 0 |  823 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  824 | `		return -1;` |
|    - |  825 | `	}` |
|    1 |  826 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  827 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  828 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    - |  829 | `		/* Expand 'unknown' */` |
|    1 |  830 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    1 |  831 | `		return -1;` |
|    - |  832 | `	}` |
|    1 |  833 | `	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE) ){` |
|    1 |  834 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|    1 |  835 | `	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){` |
|    1 |  836 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|  ! 0 |  837 | `	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){` |
|  ! 0 |  838 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|  ! 0 |  839 | `	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){` |
|  ! 0 |  840 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|  ! 0 |  841 | `	}else{` |
|  ! 0 |  842 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    - |  843 | `	}` |
|    1 |  844 | `	return PH7_OK;` |
|    1 |  845 | `}` |
|    - |  846 | `/*` |
|    - |  847 | ` * int (*xReadlink)(const char *,ph7_context *)` |
|    - |  848 | ` *` |
|    - |  849 | ` * Windows has no readlink(2). php resolves the handle instead` |
|    - |  850 | ` * (GetFinalPathNameByHandleW) and strips the \\?\ prefix it comes back with, so` |
|    - |  851 | ` * the answer here is the target's CANONICAL path where a unix readlink() would` |
|    - |  852 | ` * hand back the link's raw text -- a relative symlink therefore reads back` |
|    - |  853 | ` * absolute on this platform.` |
|    - |  854 | ` */` |
|    - |  855 | `static int WinVfs_Readlink(const char *zPath,ph7_context *pCtx)` |
|    1 |  856 | `{` |
|    - |  857 | `	void *pConverted;` |
|    - |  858 | `	HANDLE pHandle;` |
|    - |  859 | `	WCHAR zTarget[1024];` |
|    - |  860 | `	char zUtf8[1024*4];` |
|    - |  861 | `	DWORD nLen;` |
|    - |  862 | `	int nOut;` |
|    1 |  863 | `	zPath = WinVfsLocalPath(zPath);` |
|    1 |  864 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  865 | `	if( pConverted == 0 ){` |
|  ! 0 |  866 | `		return -1;` |
|    - |  867 | `	}` |
|    - |  868 | `	/* No reparse-point screen: php_win32_ioutil_readlink_w falls back to the` |
|    - |  869 | `	 * handle's final path when the name is not a link, so a plain file reads` |
|    - |  870 | `	 * back as itself on this platform rather than failing. */` |
|    1 |  871 | `	pHandle = CreateFileW((LPCWSTR)pConverted,0,` |
|    - |  872 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_EXISTING,` |
|    - |  873 | `		FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    1 |  874 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  875 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 |  876 | `		return -1;` |
|    - |  877 | `	}` |
|    1 |  878 | `	nLen = GetFinalPathNameByHandleW(pHandle,zTarget,` |
|    - |  879 | `		(DWORD)(sizeof(zTarget)/sizeof(zTarget[0])) - 1,0);` |
|    1 |  880 | `	CloseHandle(pHandle);` |
|    1 |  881 | `	if( nLen < 1 \|\| nLen >= (DWORD)(sizeof(zTarget)/sizeof(zTarget[0])) ){` |
|  ! 0 |  882 | `		return -1;` |
|    - |  883 | `	}` |
|    1 |  884 | `	zTarget[nLen] = 0;` |
|    1 |  885 | `	nOut = WideCharToMultiByte(CP_UTF8,0,zTarget,(int)nLen,zUtf8,(int)sizeof(zUtf8),0,0);` |
|    1 |  886 | `	if( nOut < 1 ){` |
|  ! 0 |  887 | `		return -1;` |
|    - |  888 | `	}` |
|    - |  889 | `	/* Drop the \\?\ prefix the API always prepends, as php does. */` |
|    1 |  890 | `	if( nOut > 4 && zUtf8[0] == '\\' && zUtf8[1] == '\\' && zUtf8[2] == '?' && zUtf8[3] == '\\' ){` |
|    1 |  891 | `		ph7_result_string(pCtx,&zUtf8[4],nOut - 4);` |
|    1 |  892 | `	}else{` |
|  ! 0 |  893 | `		ph7_result_string(pCtx,zUtf8,nOut);` |
|    - |  894 | `	}` |
|    1 |  895 | `	return PH7_OK;` |
|    1 |  896 | `}` |
|    - |  897 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  898 | `static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|    5 |  899 | `{` |
|    - |  900 | `	char zValue[1024];` |
|    5 |  901 | `	char *zBuf = zValue;` |
|    - |  902 | `	DWORD n;` |
|    - |  903 | `	/*` |
|    - |  904 | `	 * According to MSDN, when lpBuffer is not large enough to hold the data the` |
|    - |  905 | `	 * return value is the size REQUIRED (terminator included) and the buffer's` |
|    - |  906 | `	 * contents are UNDEFINED. Handing that length back was a stack over-read of` |
|    - |  907 | `	 * everything past the buffer for any value longer than it -- and on Windows` |
|    - |  908 | `	 * PATH alone routinely is.` |
|    - |  909 | `	 */` |
|    5 |  910 | `	SetLastError(0);` |
|    5 |  911 | `	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));` |
|    5 |  912 | `	if( !n ){` |
|    - |  913 | `		/* 0 is both "absent" and "empty": only the error code tells them apart,` |
|    - |  914 | `		 * and php reports an empty variable as "" the way POSIX does. */` |
|    2 |  915 | `		if( GetLastError() == ERROR_ENVVAR_NOT_FOUND ){` |
|    2 |  916 | `			return -1;` |
|    - |  917 | `		}` |
|    2 |  918 | `		ph7_result_string(pCtx,"",0);` |
|    2 |  919 | `		return PH7_OK;` |
|    - |  920 | `	}` |
|    5 |  921 | `	if( n >= sizeof(zValue) ){` |
|  ! 0 |  922 | `		DWORD nWant = n;` |
|  ! 0 |  923 | `		zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nWant,0,TRUE);` |
|  ! 0 |  924 | `		if( zBuf == 0 ){` |
|  ! 0 |  925 | `			return -1;` |
|    - |  926 | `		}` |
|  ! 0 |  927 | `		n = GetEnvironmentVariableA(zVar,zBuf,nWant);` |
|  ! 0 |  928 | `		if( !n \|\| n >= nWant ){` |
|    - |  929 | `			/* It changed underneath us; report it as absent rather than guess. */` |
|  ! 0 |  930 | `			return -1;` |
|    - |  931 | `		}` |
|    - |  932 | `	}` |
|    5 |  933 | `	ph7_result_string(pCtx,zBuf,(int)n);` |
|    5 |  934 | `	return PH7_OK;` |
|    5 |  935 | `}` |
|    - |  936 | `/* int (*xSetenv)(const char *,const char *) */` |
|    - |  937 | `static int WinVfs_Setenv(const char *zName,const char *zValue)` |
|    3 |  938 | `{` |
|    - |  939 | `	BOOL rc;` |
|    - |  940 | `	/* A NULL value REMOVES the variable, which is php's putenv("NAME") with no` |
|    - |  941 | `	 * '='. An EMPTY value is a variable of its own, on Windows as elsewhere. */` |
|    3 |  942 | `	rc = SetEnvironmentVariableA(zName,zValue);` |
|    3 |  943 | `	return rc ? PH7_OK : -1;` |
|    3 |  944 | `}` |
|    - |  945 | `/* int (*xEnviron)(ph7_context *) */` |
|    - |  946 | `static int WinVfs_Environ(ph7_context *pCtx)` |
|    1 |  947 | `{` |
|    - |  948 | `	ph7_value *pArray,*pKey,*pVal;` |
|    - |  949 | `	LPCH zBlock,zEntry;` |
|    1 |  950 | `	pArray = ph7_context_new_array(pCtx);` |
|    1 |  951 | `	pKey = ph7_context_new_scalar(pCtx);` |
|    1 |  952 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    1 |  953 | `	if( pArray == 0 \|\| pKey == 0 \|\| pVal == 0 ){` |
|  ! 0 |  954 | `		return -1;` |
|    - |  955 | `	}` |
|    1 |  956 | `	zBlock = GetEnvironmentStringsA();` |
|    1 |  957 | `	if( zBlock == 0 ){` |
|  ! 0 |  958 | `		return -1;` |
|    - |  959 | `	}` |
|    1 |  960 | `	for( zEntry = zBlock ; *zEntry ; zEntry += lstrlenA(zEntry) + 1 ){` |
|    1 |  961 | `		const char *zEq = zEntry;` |
|    - |  962 | `		/* The block leads with the "=C:=C:\dir" drive-cursor entries, whose name` |
|    - |  963 | `		 * is empty: php does not report them and neither does this. */` |
|    1 |  964 | `		if( *zEq == '=' ){` |
|    1 |  965 | `			continue;` |
|    - |  966 | `		}` |
|    1 |  967 | `		while( *zEq && *zEq != '=' ){` |
|    1 |  968 | `			zEq++;` |
|    1 |  969 | `		}` |
|    1 |  970 | `		if( *zEq != '=' ){` |
|  ! 0 |  971 | `			continue;` |
|    - |  972 | `		}` |
|    1 |  973 | `		ph7_value_string(pKey,zEntry,(int)(zEq - zEntry));` |
|    1 |  974 | `		ph7_value_string(pVal,zEq+1,-1);` |
|    1 |  975 | `		ph7_array_add_elem(pArray,pKey,pVal);` |
|    1 |  976 | `		ph7_value_reset_string_cursor(pKey);` |
|    1 |  977 | `		ph7_value_reset_string_cursor(pVal);` |
|    1 |  978 | `	}` |
|    1 |  979 | `	FreeEnvironmentStringsA(zBlock);` |
|    1 |  980 | `	ph7_result_value(pCtx,pArray);` |
|    1 |  981 | `	return PH7_OK;` |
|    1 |  982 | `}` |
|    - |  983 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  984 | `static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|    5 |  985 | `{` |
|    - |  986 | `	DWORD dwSizeLow,dwSizeHigh;` |
|    - |  987 | `	HANDLE pHandle,pMapHandle;` |
|    - |  988 | `	void *pConverted,*pView;` |
|    - |  989 |  |
|    5 |  990 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  991 | `	if( pConverted == 0 ){` |
|  ! 0 |  992 | `		return -1;` |
|    - |  993 | `	}` |
|    5 |  994 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    5 |  995 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  996 | `	if( pHandle == 0 ){` |
|  ! 0 |  997 | `		return -1;` |
|    - |  998 | `	}` |
|    - |  999 | `	/* Get the file size */` |
|    5 | 1000 | `	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);` |
|    5 | 1001 | `	if( dwSizeLow == 0 && dwSizeHigh == 0 ){` |
|    - | 1002 | `		/* A 0-byte file is a legal PHP program that prints nothing.` |
|    - | 1003 | `		 * CreateFileMapping refuses a zero-length file (ERROR_FILE_INVALID), so` |
|    - | 1004 | ``		 * this came back as an IO error and `phl empty.php` said "Could not open`` |
|    - | 1005 | `		 * input file" where php runs it. Answer an EMPTY view; the caller does` |
|    - | 1006 | `		 * not unmap one (api.c). */` |
|    1 | 1007 | `		CloseHandle(pHandle);` |
|    1 | 1008 | `		*ppMap = (void *)"";` |
|    1 | 1009 | `		*pSize = 0;` |
|    1 | 1010 | `		return PH7_OK;` |
|    - | 1011 | `	}` |
|    - | 1012 | `	/* Create the mapping */` |
|    5 | 1013 | `	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);` |
|    5 | 1014 | `	if( pMapHandle == 0 ){` |
|  ! 0 | 1015 | `		CloseHandle(pHandle);` |
|  ! 0 | 1016 | `		return -1;` |
|    - | 1017 | `	}` |
|    5 | 1018 | `	*pSize = ((ph7_int64)dwSizeHigh << 32) \| dwSizeLow;` |
|    - | 1019 | `	/* Obtain the view */` |
|    5 | 1020 | `	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));` |
|    5 | 1021 | `	if( pView ){` |
|    - | 1022 | `		/* Let the upper layer point to the view */` |
|    5 | 1023 | `		*ppMap = pView;` |
|    - | 1024 | `	}` |
|    - | 1025 | `	/* Close the handle` |
|    - | 1026 | `	 * According to MSDN it's OK the close the HANDLES.` |
|    - | 1027 | `	 */` |
|    5 | 1028 | `	CloseHandle(pMapHandle);` |
|    5 | 1029 | `	CloseHandle(pHandle);` |
|    5 | 1030 | `	return pView ? PH7_OK : -1;` |
|    5 | 1031 | `}` |
|    - | 1032 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|    - | 1033 | `static void WinVfs_Unmap(void *pView,ph7_int64 nSize)` |
|    5 | 1034 | `{` |
|    5 | 1035 | `	nSize = 0; /* Compiler warning */` |
|    5 | 1036 | `	UnmapViewOfFile(pView);` |
|    5 | 1037 | `}` |
|    - | 1038 | `/* void (*xTempDir)(ph7_context *) */` |
|    - | 1039 | `static void WinVfs_TempDir(ph7_context *pCtx)` |
|    5 | 1040 | `{` |
|    - | 1041 | `	CHAR zTemp[1024];` |
|    - | 1042 | `	DWORD n;` |
|    5 | 1043 | `	n = GetTempPathA(sizeof(zTemp),zTemp);` |
|    5 | 1044 | `	if( n < 1 ){` |
|    - | 1045 | `		/* Assume the default windows temp directory */` |
|  ! 0 | 1046 | `		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);` |
|  ! 0 | 1047 | `	}else{` |
|    - | 1048 | `		/* GetTempPath() always ends its answer with a separator and php's` |
|    - | 1049 | `		 * sys_get_temp_dir() never does -- the unix side already trims one (see` |
|    - | 1050 | `		 * UnixVfs_TempDir), and a caller joining "/name" to this one was getting` |
|    - | 1051 | `		 * "...\Temp\/name". */` |
|    5 | 1052 | `		while( n > 1 && (zTemp[n-1] == '\\' \|\| zTemp[n-1] == '/') ){` |
|    5 | 1053 | `			n--;` |
|    5 | 1054 | `		}` |
|    5 | 1055 | `		ph7_result_string(pCtx,zTemp,(int)n);` |
|    - | 1056 | `	}` |
|    5 | 1057 | `}` |
|    - | 1058 | `/* unsigned int (*xProcessId)(void) */` |
|    - | 1059 | `static unsigned int WinVfs_ProcessId(void)` |
|    5 | 1060 | `{` |
|    5 | 1061 | `	DWORD nID = 0;` |
|    - | 1062 | `#ifndef __MINGW32__` |
|    5 | 1063 | `	nID = GetProcessId(GetCurrentProcess());` |
|    - | 1064 | `#endif /* __MINGW32__ */` |
|    5 | 1065 | `	return (unsigned int)nID;` |
|    5 | 1066 | `}` |
|    - | 1067 | `/* void (*xUsername)(ph7_context *) */` |
|    - | 1068 | `static void WinVfs_Username(ph7_context *pCtx)` |
|    1 | 1069 | `{` |
|    - | 1070 | `	WCHAR zUser[1024];` |
|    - | 1071 | `	DWORD nByte;` |
|    - | 1072 | `	BOOL rc;` |
|    1 | 1073 | `	nByte = sizeof(zUser);` |
|    1 | 1074 | `	rc = GetUserNameW(zUser,&nByte);` |
|    1 | 1075 | `	if( !rc ){` |
|    - | 1076 | `		/* Set a dummy name */` |
|  ! 0 | 1077 | `		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 | 1078 | `	}else{` |
|    - | 1079 | `		char *zName;` |
|    1 | 1080 | `		zName = unicodeToUtf8(zUser);` |
|    1 | 1081 | `		if( zName == 0 ){` |
|  ! 0 | 1082 | `			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 | 1083 | `		}else{` |
|    1 | 1084 | `			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    1 | 1085 | `			HeapFree(GetProcessHeap(),0,zName);` |
|    - | 1086 | `		}` |
|    - | 1087 | `	}` |
|    - | 1088 |  |
|    1 | 1089 | `}` |
|    - | 1090 | `/* int (*xChroot)(const char *) — Windows has no chroot; fail cleanly so chroot()` |
|    - | 1091 | ` * returns false. (php has no chroot symbol at all; PHL exposes it as an extension` |
|    - | 1092 | ` * and this reports the failure without a "not implemented in the VFS" warning.) */` |
|    - | 1093 | `static int WinVfs_chroot(const char *zPath)` |
|  ! 0 | 1094 | `{` |
|    - | 1095 | `	(void)zPath;` |
|  ! 0 | 1096 | `	return -1;` |
|  ! 0 | 1097 | `}` |
|    - | 1098 | `#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE` |
|    - | 1099 | `#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2` |
|    - | 1100 | `#endif` |
|    - | 1101 | `#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY` |
|    - | 1102 | `#define SYMBOLIC_LINK_FLAG_DIRECTORY 0x1` |
|    - | 1103 | `#endif` |
|    - | 1104 | `/* int (*xLink)(const char *,const char *,int) — hard link (iSym==0) via` |
|    - | 1105 | ` * CreateHardLink or symbolic link (iSym!=0) via CreateSymbolicLink, mirroring` |
|    - | 1106 | ` * php on Windows. Developer-mode symlink creation is allowed. */` |
|    - | 1107 | `static int WinVfs_Link(const char *zOld,const char *zNew,int iSym)` |
|    1 | 1108 | `{` |
|    - | 1109 | `	void *pOld, *pNew;` |
|    - | 1110 | `	BOOL rc;` |
|    1 | 1111 | `	pOld = convertUtf8Filename(zOld);` |
|    1 | 1112 | `	if( pOld == 0 ){` |
|  ! 0 | 1113 | `		return -1;` |
|    - | 1114 | `	}` |
|    1 | 1115 | `	pNew = convertUtf8Filename(zNew);` |
|    1 | 1116 | `	if( pNew == 0 ){` |
|  ! 0 | 1117 | `		HeapFree(GetProcessHeap(),0,pOld);` |
|  ! 0 | 1118 | `		return -1;` |
|    - | 1119 | `	}` |
|    1 | 1120 | `	if( iSym ){` |
|    1 | 1121 | `		DWORD dwFlags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;` |
|    1 | 1122 | `		DWORD attr = GetFileAttributesW((LPCWSTR)pOld);` |
|    1 | 1123 | `		if( attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) ){` |
|  ! 0 | 1124 | `			dwFlags \|= SYMBOLIC_LINK_FLAG_DIRECTORY;` |
|    - | 1125 | `		}` |
|    1 | 1126 | `		rc = CreateSymbolicLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,dwFlags) ? TRUE : FALSE;` |
|    1 | 1127 | `	}else{` |
|    1 | 1128 | `		rc = CreateHardLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,0);` |
|    - | 1129 | `	}` |
|    1 | 1130 | `	HeapFree(GetProcessHeap(),0,pNew);` |
|    1 | 1131 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|    1 | 1132 | `	return rc ? PH7_OK : -1;` |
|    1 | 1133 | `}` |
|    - | 1134 | `/* int (*xUmask)(int) — Windows has no umask; php's umask() returns 0 there. */` |
|    - | 1135 | `static int WinVfs_Umask(int iMask)` |
|    1 | 1136 | `{` |
|    - | 1137 | `	(void)iMask;` |
|    1 | 1138 | `	return 0;` |
|    1 | 1139 | `}` |
|    - | 1140 | `/* int (*xUid)(void) / int (*xGid)(void) — no uid/gid on Windows; php's` |
|    - | 1141 | ` * getmyuid()/getmygid() both return 0. */` |
|    - | 1142 | `static int WinVfs_Uid(void)` |
|    1 | 1143 | `{` |
|    1 | 1144 | `	return 0;` |
|    1 | 1145 | `}` |
|    - | 1146 | `static int WinVfs_Gid(void)` |
|    1 | 1147 | `{` |
|    1 | 1148 | `	return 0;` |
|    1 | 1149 | `}` |
|    - | 1150 | `/* Export the windows vfs */` |
|    - | 1151 | `PH7_PRIVATE const ph7_vfs sWinVfs = {` |
|    - | 1152 | `	"Windows_vfs",` |
|    - | 1153 | `	PH7_VFS_VERSION,` |
|    - | 1154 | `	WinVfs_chdir,    /* int (*xChdir)(const char *) */` |
|    - | 1155 | `	WinVfs_chroot,   /* int (*xChroot)(const char *); */` |
|    - | 1156 | `	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|    - | 1157 | `	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|    - | 1158 | `	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|    - | 1159 | `	WinVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|    - | 1160 | `	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|    - | 1161 | `	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|    - | 1162 | `	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */` |
|    - | 1163 | `	WinVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|    - | 1164 | `	WinVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|    - | 1165 | `	WinVfs_chmod, /*int (*xChmod)(const char *,int)*/` |
|    - | 1166 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|    - | 1167 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|    - | 1168 | `	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - | 1169 | `	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - | 1170 | `	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|    - | 1171 | `	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - | 1172 | `	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - | 1173 | `	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - | 1174 | `	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - | 1175 | `	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - | 1176 | `	WinVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|    - | 1177 | `	WinVfs_islink,     /* int (*xIslink)(const char *) */` |
|    - | 1178 | `	WinVfs_isfile,     /* int (*xReadable)(const char *) */` |
|    - | 1179 | `	WinVfs_iswritable, /* int (*xWritable)(const char *) */` |
|    - | 1180 | `	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */` |
|    - | 1181 | `	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|    - | 1182 | `	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|    - | 1183 | `	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|    - | 1184 | `	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - | 1185 | `	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - | 1186 | `	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|    - | 1187 | `	WinVfs_Link,       /* int (*xLink)(const char *,const char *,int) */` |
|    - | 1188 | `	WinVfs_Umask,      /* int (*xUmask)(int) */` |
|    - | 1189 | `	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|    - | 1190 | `	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|    - | 1191 | `	WinVfs_Uid, /* int (*xUid)(void) */` |
|    - | 1192 | `	WinVfs_Gid, /* int (*xGid)(void) */` |
|    - | 1193 | `	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|    - | 1194 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|    - | 1195 | `	WinVfs_Readlink, /* int (*xReadlink)(const char *,ph7_context *) */` |
|    - | 1196 | `	WinVfs_Environ  /* int (*xEnviron)(ph7_context *) */` |
|    - | 1197 | `};` |
|    - | 1198 | `/* Windows file IO */` |
|    - | 1199 | `#ifndef INVALID_SET_FILE_POINTER` |
|    - | 1200 | `# define INVALID_SET_FILE_POINTER ((DWORD)-1)` |
|    - | 1201 | `#endif` |
|    - | 1202 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|    - | 1203 | `static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|    5 | 1204 | `{` |
|    5 | 1205 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    5 | 1206 | `	DWORD dwAccess = GENERIC_READ;` |
|    - | 1207 | `	DWORD dwShare,dwCreate;` |
|    - | 1208 | `	void *pConverted;` |
|    - | 1209 | `	HANDLE pHandle;` |
|    - | 1210 |  |
|    5 | 1211 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 | 1212 | `	if( pConverted == 0 ){` |
|  ! 0 | 1213 | `		errno = ENOMEM;` |
|  ! 0 | 1214 | `		return -1;` |
|    - | 1215 | `	}` |
|    - | 1216 | `	/* Set the desired flags according to the open mode */` |
|    5 | 1217 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|    - | 1218 | `		/* Open existing file, or create if it doesn't exist */` |
|    5 | 1219 | `		dwCreate = OPEN_ALWAYS;` |
|    5 | 1220 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - | 1221 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|    5 | 1222 | `			dwCreate = CREATE_ALWAYS;` |
|    5 | 1223 | `		}` |
|    5 | 1224 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|    - | 1225 | `		/* Creates a new file, only if it does not already exist.` |
|    - | 1226 | `		* If the file exists, it fails.` |
|    - | 1227 | `		*/` |
|    5 | 1228 | `		dwCreate = CREATE_NEW;` |
|    5 | 1229 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - | 1230 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|    - | 1231 | `		 * The file must exist.` |
|    - | 1232 | `		 */` |
|  ! 0 | 1233 | `		dwCreate = TRUNCATE_EXISTING;` |
|  ! 0 | 1234 | `	}else{` |
|    - | 1235 | `		/* Opens a file, only if it exists. */` |
|    5 | 1236 | `		dwCreate = OPEN_EXISTING;` |
|    - | 1237 | `	}` |
|    5 | 1238 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|    - | 1239 | `		/* Read+Write access */` |
|    2 | 1240 | `		dwAccess \|= GENERIC_WRITE;` |
|    5 | 1241 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|    - | 1242 | `		/* Write only access */` |
|    5 | 1243 | `		dwAccess = GENERIC_WRITE;` |
|    - | 1244 | `	}` |
|    5 | 1245 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|    - | 1246 | `		/* Append mode. On Windows the "every write lands at the END" rule is` |
|    - | 1247 | `		 * FILE_APPEND_DATA *without* FILE_WRITE_DATA, so the mask has to be` |
|    - | 1248 | ``		 * BUILT rather than replaced: `a+` is read-write, and overwriting the`` |
|    - | 1249 | `		 * whole mask left such a handle with no READ access at all -- every` |
|    - | 1250 | `		 * read on one answered false where php reads the file. (Invisible until` |
|    - | 1251 | ``		 * the mode grammar started seeing the `+` of `ab+`.) */`` |
|    5 | 1252 | `		dwAccess = (iOpenMode & PH7_IO_OPEN_RDWR)` |
|    - | 1253 | `			? (GENERIC_READ\|FILE_APPEND_DATA) : FILE_APPEND_DATA;` |
|    - | 1254 | `	}` |
|    5 | 1255 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|    - | 1256 | `		/* File is temporary */` |
|  ! 0 | 1257 | `		dwType = FILE_ATTRIBUTE_TEMPORARY;` |
|    - | 1258 | `	}` |
|    5 | 1259 | `	dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    5 | 1260 | `	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    5 | 1261 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    - | 1262 | `		/* Mapped BEFORE the HeapFree: the caller's warning is worded` |
|    - | 1263 | `		 * "Failed to open stream: %s" from strerror(errno), and CreateFileW` |
|    - | 1264 | `		 * reports through GetLastError() only -- so every failed open on` |
|    - | 1265 | `		 * Windows read "No error" (or, worse, whatever errno an unrelated` |
|    - | 1266 | `		 * earlier call had left behind) where php names the real reason. */` |
|    5 | 1267 | `		WinVfsMapErrno();` |
|    5 | 1268 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|    - | 1269 | `		SXUNUSED(pResource); /* MSVC warning */` |
|    5 | 1270 | `		return -1;` |
|    - | 1271 | `	}` |
|    5 | 1272 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    - | 1273 | `	/* Make the handle accessible to the upper layer */` |
|    5 | 1274 | `	*ppHandle = (void *)pHandle;` |
|    5 | 1275 | `	return PH7_OK;` |
|    5 | 1276 | `}` |
|    - | 1277 | `/* An instance of the following structure is used to record state information` |
|    - | 1278 | ` * while iterating throw directory entries.` |
|    - | 1279 | ` */` |
|    - | 1280 | `typedef struct WinDir_Info WinDir_Info;` |
|    - | 1281 | `struct WinDir_Info` |
|    - | 1282 | `{` |
|    - | 1283 | `	HANDLE pDirHandle;` |
|    - | 1284 | `	void *pPath;` |
|    - | 1285 | `	WIN32_FIND_DATAW sInfo;` |
|    - | 1286 | `	int rc;` |
|    - | 1287 | `};` |
|    - | 1288 | `/* The Win32 code the last failed opendir() stopped on, for the warning php` |
|    - | 1289 | ` * raises from it ahead of its own "Failed to open directory". */` |
|    - | 1290 | `static DWORD dwOpenDirErr = 0;` |
|    - | 1291 | `/*` |
|    - | 1292 | ` * php_win32_docref1_from_error()'s text for the last failed opendir(): the` |
|    - | 1293 | ` * system message with its trailing line breaks and periods stripped, and then` |
|    - | 1294 | ` * two MORE characters cut -- php's own bug, which is why a missing directory is` |
|    - | 1295 | ` * "The system cannot find the file specifi". 0 when there is nothing to say.` |
|    - | 1296 | ` */` |
|    - | 1297 | `PH7_PRIVATE unsigned long PH7_WinOpenDirReason(char *zBuf,int nBuf)` |
|    3 | 1298 | `{` |
|    3 | 1299 | `	WCHAR *zMsg = 0;` |
|    - | 1300 | `	DWORD n;` |
|    - | 1301 | `	int nOut;` |
|    3 | 1302 | `	if( dwOpenDirErr == 0 \|\| nBuf < 1 ){` |
|  ! 0 | 1303 | `		return 0;` |
|    - | 1304 | `	}` |
|    3 | 1305 | `	n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER\|FORMAT_MESSAGE_FROM_SYSTEM\|FORMAT_MESSAGE_IGNORE_INSERTS,` |
|    - | 1306 | `		0,dwOpenDirErr,MAKELANGID(LANG_NEUTRAL,SUBLANG_NEUTRAL),(LPWSTR)&zMsg,0,0);` |
|    3 | 1307 | `	zBuf[0] = 0;` |
|    3 | 1308 | `	if( n > 0 && zMsg ){` |
|    3 | 1309 | `		while( n > 0 && (zMsg[n-1] == L'\r' \|\| zMsg[n-1] == L'\n' \|\| zMsg[n-1] == L'.') ){` |
|    3 | 1310 | `			n--;` |
|    3 | 1311 | `		}` |
|    3 | 1312 | `		nOut = WideCharToMultiByte(CP_UTF8,0,zMsg,(int)n,zBuf,nBuf-1,0,0);` |
|    3 | 1313 | `		if( nOut < 0 ){` |
|  ! 0 | 1314 | `			nOut = 0;` |
|    - | 1315 | `		}` |
|    3 | 1316 | `		zBuf[nOut] = 0;` |
|    3 | 1317 | `		if( nOut >= 2 ){` |
|    3 | 1318 | `			zBuf[nOut-2] = 0;` |
|    - | 1319 | `		}` |
|    - | 1320 | `	}` |
|    3 | 1321 | `	if( zMsg ){` |
|    3 | 1322 | `		LocalFree(zMsg);` |
|    - | 1323 | `	}` |
|    3 | 1324 | `	return (unsigned long)dwOpenDirErr;` |
|    3 | 1325 | `}` |
|    - | 1326 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|    - | 1327 | `static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|    5 | 1328 | `{` |
|    - | 1329 | `	WinDir_Info *pDirInfo;` |
|    - | 1330 | `	void *pConverted;` |
|    - | 1331 | `	char *zPrep;` |
|    - | 1332 | `	sxu32 n;` |
|    5 | 1333 | `	dwOpenDirErr = 0;` |
|    - | 1334 | `	/* Prepare the path */` |
|    5 | 1335 | `	n = SyStrlen(zPath);` |
|    - | 1336 | `	{` |
|    - | 1337 | `		/* php resolves the path BEFORE it lists it, and a name that does not` |
|    - | 1338 | `		 * resolve fails there, with that lookup's code: 2 for a missing leaf, 3` |
|    - | 1339 | `		 * for a missing parent, 267 for a file in the way. Look the path itself` |
|    - | 1340 | `		 * up the same way (a drive root cannot be, and needs no asking). */` |
|    5 | 1341 | `		sxu32 nProbe = n;` |
|    5 | 1342 | `		while( nProbe > 0 && (zPath[nProbe-1] == '/' \|\| zPath[nProbe-1] == '\\') ){` |
|  ! 0 | 1343 | `			nProbe--;` |
|  ! 0 | 1344 | `		}` |
|    5 | 1345 | `		if( nProbe > 0 && zPath[nProbe-1] != ':' ){` |
|    - | 1346 | `			WIN32_FIND_DATAW sProbe;` |
|    - | 1347 | `			HANDLE hProbe;` |
|    5 | 1348 | `			zPrep = (char *)HeapAlloc(GetProcessHeap(),0,nProbe+1);` |
|    5 | 1349 | `			if( zPrep == 0 ){` |
|  ! 0 | 1350 | `				errno = ENOMEM;` |
|  ! 0 | 1351 | `				return -1;` |
|    - | 1352 | `			}` |
|    5 | 1353 | `			SyMemcpy((const void *)zPath,zPrep,nProbe);` |
|    5 | 1354 | `			zPrep[nProbe] = 0;` |
|    5 | 1355 | `			pConverted = convertUtf8Filename(zPrep);` |
|    5 | 1356 | `			HeapFree(GetProcessHeap(),0,zPrep);` |
|    5 | 1357 | `			if( pConverted == 0 ){` |
|  ! 0 | 1358 | `				errno = ENOMEM;` |
|  ! 0 | 1359 | `				return -1;` |
|    - | 1360 | `			}` |
|    5 | 1361 | `			hProbe = FindFirstFileW((LPCWSTR)pConverted,&sProbe);` |
|    5 | 1362 | `			if( hProbe == INVALID_HANDLE_VALUE ){` |
|    4 | 1363 | `				dwOpenDirErr = GetLastError();` |
|    4 | 1364 | `			}else{` |
|    5 | 1365 | `				FindClose(hProbe);` |
|    - | 1366 | `			}` |
|    5 | 1367 | `			HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 | 1368 | `			if( dwOpenDirErr != 0 ){` |
|    4 | 1369 | `				errno = ENOENT;` |
|    4 | 1370 | `				return -1;` |
|    - | 1371 | `			}` |
|    - | 1372 | `		}` |
|    - | 1373 | `	}` |
|    5 | 1374 | `	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);` |
|    5 | 1375 | `	if( zPrep == 0 ){` |
|  ! 0 | 1376 | `		errno = ENOMEM;` |
|  ! 0 | 1377 | `		return -1;` |
|    - | 1378 | `	}` |
|    5 | 1379 | `	SyMemcpy((const void *)zPath,zPrep,n);` |
|    5 | 1380 | `	zPrep[n]   = '\\';` |
|    5 | 1381 | `	zPrep[n+1] =  '*';` |
|    5 | 1382 | `	zPrep[n+2] = 0;` |
|    5 | 1383 | `	pConverted = convertUtf8Filename(zPrep);` |
|    5 | 1384 | `	HeapFree(GetProcessHeap(),0,zPrep);` |
|    5 | 1385 | `	if( pConverted == 0 ){` |
|  ! 0 | 1386 | `		errno = ENOMEM;` |
|  ! 0 | 1387 | `		return -1;` |
|    - | 1388 | `	}` |
|    - | 1389 | `	/* Allocate a new instance */` |
|    5 | 1390 | `	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));` |
|    5 | 1391 | `	if( pDirInfo == 0 ){` |
|  ! 0 | 1392 | `		errno = ENOMEM;` |
|  ! 0 | 1393 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 | 1394 | `		pResource = 0; /* Compiler warning */` |
|  ! 0 | 1395 | `		return -1;` |
|    - | 1396 | `	}` |
|    5 | 1397 | `	pDirInfo->rc = SXRET_OK;` |
|    5 | 1398 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);` |
|    5 | 1399 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|    - | 1400 | `		/* Cannot open directory -- same reason as WinFile_Open above. A path that` |
|    - | 1401 | `		 * resolved but is not a directory is 267, which php reports as ENOENT. */` |
|    1 | 1402 | `		dwOpenDirErr = GetLastError();` |
|    1 | 1403 | `		WinVfsMapErrno();` |
|    1 | 1404 | `		if( dwOpenDirErr == ERROR_DIRECTORY ){` |
|    1 | 1405 | `			errno = ENOENT;` |
|    - | 1406 | `		}` |
|    1 | 1407 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 | 1408 | `		HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    1 | 1409 | `		return -1;` |
|    - | 1410 | `	}` |
|    - | 1411 | `	/* Save the path */` |
|    5 | 1412 | `	pDirInfo->pPath = pConverted;` |
|    - | 1413 | `	/* Save our structure */` |
|    5 | 1414 | `	*ppHandle = pDirInfo;` |
|    5 | 1415 | `	return PH7_OK;` |
|    5 | 1416 | `}` |
|    - | 1417 | `/* void (*xCloseDir)(void *) */` |
|    - | 1418 | `static void WinDir_Close(void *pUserData)` |
|    5 | 1419 | `{` |
|    5 | 1420 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    5 | 1421 | `	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){` |
|    5 | 1422 | `		FindClose(pDirInfo->pDirHandle);` |
|    - | 1423 | `	}` |
|    5 | 1424 | `	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);` |
|    5 | 1425 | `	HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    5 | 1426 | `}` |
|    - | 1427 | `/* void (*xClose)(void *); */` |
|    - | 1428 | `static void WinFile_Close(void *pUserData)` |
|    5 | 1429 | `{` |
|    5 | 1430 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    5 | 1431 | `	CloseHandle(pHandle);` |
|    5 | 1432 | `}` |
|    - | 1433 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|    - | 1434 | `static int WinDir_Read(void *pUserData,ph7_context *pCtx)` |
|    5 | 1435 | `{` |
|    5 | 1436 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    - | 1437 | `	LPWIN32_FIND_DATAW pData;` |
|    - | 1438 | `	char *zName;` |
|    - | 1439 | `	BOOL rc;` |
|    5 | 1440 | `	if( pDirInfo->rc != SXRET_OK ){` |
|    - | 1441 | `		/* No more entry to process */` |
|    5 | 1442 | `		return -1;` |
|    - | 1443 | `	}` |
|    5 | 1444 | `	pData = &pDirInfo->sInfo;` |
|    - | 1445 | `	/* php parity: readdir()/scandir() include the '.' and '..' entries, so unlike` |
|    - | 1446 | `	 * the historical PH7 behaviour we return them instead of skipping. */` |
|    5 | 1447 | `	zName = unicodeToUtf8(pData->cFileName);` |
|    5 | 1448 | `	if( zName == 0 ){` |
|    - | 1449 | `		/* Out of memory */` |
|  ! 0 | 1450 | `		return -1;` |
|    - | 1451 | `	}` |
|    - | 1452 | `	/* Return the current file name */` |
|    5 | 1453 | `	ph7_result_string(pCtx,zName,-1);` |
|    5 | 1454 | `	HeapFree(GetProcessHeap(),0,zName);` |
|    - | 1455 | `	/* Point to the next entry */` |
|    5 | 1456 | `	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|    5 | 1457 | `	if( !rc ){` |
|    5 | 1458 | `		pDirInfo->rc = SXERR_EOF;` |
|    - | 1459 | `	}` |
|    5 | 1460 | `	return PH7_OK;` |
|    5 | 1461 | `}` |
|    - | 1462 | `/* void (*xRewindDir)(void *) */` |
|    - | 1463 | `static void WinDir_RewindDir(void *pUserData)` |
|    4 | 1464 | `{` |
|    4 | 1465 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    4 | 1466 | `	FindClose(pDirInfo->pDirHandle);` |
|    4 | 1467 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);` |
|    4 | 1468 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 | 1469 | `		pDirInfo->rc = SXERR_EOF;` |
|  ! 0 | 1470 | `	}else{` |
|    4 | 1471 | `		pDirInfo->rc = SXRET_OK;` |
|    - | 1472 | `	}` |
|    4 | 1473 | `}` |
|    - | 1474 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|    - | 1475 | `static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)` |
|    5 | 1476 | `{` |
|    5 | 1477 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1478 | `	DWORD nRd;` |
|    - | 1479 | `	BOOL rc;` |
|    5 | 1480 | `	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    5 | 1481 | `	if( !rc ){` |
|    - | 1482 | `		/* EOF or IO error */` |
|    1 | 1483 | `		return -1;` |
|    - | 1484 | `	}` |
|    5 | 1485 | `	return (ph7_int64)nRd;` |
|    5 | 1486 | `}` |
|    - | 1487 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|    - | 1488 | `static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)` |
|    5 | 1489 | `{` |
|    5 | 1490 | `	const char *zData = (const char *)pBuffer;` |
|    5 | 1491 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1492 | `	ph7_int64 nCount;` |
|    - | 1493 | `	DWORD nWr;` |
|    - | 1494 | `	BOOL rc;` |
|    5 | 1495 | `	nWr = 0;` |
|    5 | 1496 | `	nCount = 0;` |
|    - | 1497 | `	for(;;){` |
|    5 | 1498 | `		if( nWrite < 1 ){` |
|    5 | 1499 | `			break;` |
|    - | 1500 | `		}` |
|    5 | 1501 | `		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);` |
|    5 | 1502 | `		if( !rc ){` |
|    - | 1503 | `			/* IO error — surface a POSIX errno for the caller's diagnostic` |
|    - | 1504 | `			 * (e.g. a byte-range lock violation reports EACCES like php). */` |
|    1 | 1505 | `			WinVfsMapErrno();` |
|    1 | 1506 | `			break;` |
|    - | 1507 | `		}` |
|    5 | 1508 | `		nWrite -= nWr;` |
|    5 | 1509 | `		nCount += nWr;` |
|    5 | 1510 | `		zData += nWr;` |
|    5 | 1511 | `	}` |
|    5 | 1512 | `	if( nWrite > 0 ){` |
|    1 | 1513 | `		return -1;` |
|    - | 1514 | `	}` |
|    5 | 1515 | `	return nCount;` |
|    5 | 1516 | `}` |
|    - | 1517 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|    - | 1518 | `static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|    4 | 1519 | `{` |
|    4 | 1520 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1521 | `	DWORD dwMove,dwNew;` |
|    - | 1522 | `	LONG nHighOfft;` |
|    4 | 1523 | `	switch(whence){` |
|    - | 1524 | `	case 1:/*SEEK_CUR*/` |
|    2 | 1525 | `		dwMove = FILE_CURRENT;` |
|    2 | 1526 | `		break;` |
|    - | 1527 | `	case 2: /* SEEK_END */` |
|    1 | 1528 | `		dwMove = FILE_END;` |
|    1 | 1529 | `		break;` |
|    - | 1530 | `	case 0: /* SEEK_SET */` |
|    - | 1531 | `	default:` |
|    4 | 1532 | `		dwMove = FILE_BEGIN;` |
|    - | 1533 | `		break;` |
|    - | 1534 | `	}` |
|    4 | 1535 | `	nHighOfft = (LONG)(iOfft >> 32);` |
|    4 | 1536 | `	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);` |
|    4 | 1537 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1538 | `		return -1;` |
|    - | 1539 | `	}` |
|    4 | 1540 | `	return PH7_OK;` |
|    4 | 1541 | `}` |
|    - | 1542 | `/* int (*xLock)(void *,int) — see the lock_type value space in ph7.h */` |
|    - | 1543 | `static int WinFile_Lock(void *pUserData,int lock_type)` |
|    1 | 1544 | `{` |
|    1 | 1545 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1546 | `	OVERLAPPED sDummy;` |
|    - | 1547 | `	BOOL rc;` |
|    1 | 1548 | `	SyZero(&sDummy,sizeof(sDummy));` |
|    - | 1549 | `	/* Lock/unlock the whole file. php locks the maximal byte range, so the lock` |
|    - | 1550 | `	 * is effective even for an empty or freshly-truncated file — the previous` |
|    - | 1551 | `	 * code locked only GetFileSize() bytes (i.e. nothing for a 0-byte file). */` |
|    1 | 1552 | `	if( lock_type < 0 ){` |
|    - | 1553 | `		/* Unlock the file */` |
|    1 | 1554 | `		rc = UnlockFileEx(pHandle,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);` |
|    1 | 1555 | `	}else{` |
|    - | 1556 | `		/* LOCKFILE_FAIL_IMMEDIATELY belongs to the NON-BLOCKING requests only: it` |
|    - | 1557 | `		 * used to be passed unconditionally, so a plain flock($f,LOCK_EX) answered` |
|    - | 1558 | `		 * false under contention on Windows where php (and POSIX) waits. */` |
|    1 | 1559 | `		DWORD dwFlags = 0;` |
|    1 | 1560 | `		if( lock_type == PH7_IO_LOCK_EX \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    1 | 1561 | `			dwFlags \|= LOCKFILE_EXCLUSIVE_LOCK;` |
|    - | 1562 | `		}` |
|    1 | 1563 | `		if( lock_type == PH7_IO_LOCK_SH_NB \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    1 | 1564 | `			dwFlags \|= LOCKFILE_FAIL_IMMEDIATELY;` |
|    - | 1565 | `		}` |
|    1 | 1566 | `		rc = LockFileEx(pHandle,dwFlags,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);` |
|    1 | 1567 | `		if( !rc && GetLastError() == ERROR_LOCK_VIOLATION ){` |
|    - | 1568 | `			/* Refused because another holder has the range: php's $would_block. */` |
|    1 | 1569 | `			return SXERR_BUSY;` |
|    - | 1570 | `		}` |
|    - | 1571 | `	}` |
|    1 | 1572 | `	return rc ? PH7_OK : -1 /* Lock error */;` |
|    1 | 1573 | `}` |
|    - | 1574 | `/* ph7_int64 (*xTell)(void *) */` |
|    - | 1575 | `static ph7_int64 WinFile_Tell(void *pUserData)` |
|    5 | 1576 | `{` |
|    5 | 1577 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1578 | `	DWORD dwNew;` |
|    5 | 1579 | `	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);` |
|    5 | 1580 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1581 | `		return -1;` |
|    - | 1582 | `	}` |
|    5 | 1583 | `	return (ph7_int64)dwNew;` |
|    5 | 1584 | `}` |
|    - | 1585 | `/* int (*xTrunc)(void *,ph7_int64)` |
|    - | 1586 | ` *` |
|    - | 1587 | ` * SetEndOfFile() truncates at the CURRENT file pointer, so the size has to be` |
|    - | 1588 | ` * seeked to first — and the pointer is then left there. POSIX ftruncate() does` |
|    - | 1589 | ` * not move it, and neither does php on either platform, so the caller's` |
|    - | 1590 | `` * position is saved across the pair: without this, `ftruncate($h,6)` after an`` |
|    - | 1591 | ` * fgets() moved the handle to 6 on Windows and left it where it was everywhere` |
|    - | 1592 | ` * else, and every position-shaped answer after it (ftell, the next read, a` |
|    - | 1593 | ` * write's landing point) diverged by platform.` |
|    - | 1594 | ` */` |
|    - | 1595 | `static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|    1 | 1596 | `{` |
|    1 | 1597 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1598 | `	LONG HighOfft,HighCur;` |
|    - | 1599 | `	DWORD dwNew,dwCur;` |
|    - | 1600 | `	BOOL rc;` |
|    1 | 1601 | `	HighCur = 0;` |
|    1 | 1602 | `	dwCur = SetFilePointer(pHandle,0,&HighCur,FILE_CURRENT);` |
|    1 | 1603 | `	if( dwCur == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ){` |
|  ! 0 | 1604 | `		return -1;` |
|    - | 1605 | `	}` |
|    1 | 1606 | `	HighOfft = (LONG)(nOfft >> 32);` |
|    1 | 1607 | `	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);` |
|    1 | 1608 | `	if( dwNew == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR ){` |
|  ! 0 | 1609 | `		return -1;` |
|    - | 1610 | `	}` |
|    1 | 1611 | `	rc = SetEndOfFile(pHandle);` |
|    - | 1612 | `	/* Put the caller back where it was, whether the truncation worked or not. */` |
|    1 | 1613 | `	SetFilePointer(pHandle,(LONG)dwCur,&HighCur,FILE_BEGIN);` |
|    1 | 1614 | `	return rc ? PH7_OK : -1;` |
|    1 | 1615 | `}` |
|    - | 1616 | `/* int (*xSync)(void *); */` |
|    - | 1617 | `static int WinFile_Sync(void *pUserData)` |
|    1 | 1618 | `{` |
|    1 | 1619 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1620 | `	BOOL rc;` |
|    1 | 1621 | `	rc = FlushFileBuffers(pHandle);` |
|    1 | 1622 | `	if( !rc && GetLastError() == ERROR_ACCESS_DENIED ){` |
|    - | 1623 | `		/* A handle with no WRITE access: FlushFileBuffers refuses it, and php's` |
|    - | 1624 | `		 * fflush() -- a C-library fflush over a read-only stream -- succeeds.` |
|    - | 1625 | ``		 * `fflush($h)` on a file opened 'r' answered false here and true`` |
|    - | 1626 | `		 * everywhere else, which is a platform difference and not an answer. */` |
|    1 | 1627 | `		return PH7_OK;` |
|    - | 1628 | `	}` |
|    1 | 1629 | `	return rc ? PH7_OK : - 1;` |
|    1 | 1630 | `}` |
|    - | 1631 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|    - | 1632 | `static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 1633 | `{` |
|    - | 1634 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    1 | 1635 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1636 | `	BOOL rc;` |
|    1 | 1637 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 | 1638 | `	if( !rc ){` |
|  ! 0 | 1639 | `		return -1;` |
|    - | 1640 | `	}` |
|    - | 1641 | `	/* dev */` |
|    1 | 1642 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 | 1643 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - | 1644 | `	/* ino */` |
|    1 | 1645 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 | 1646 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - | 1647 | `	/* mode. A handle carries no NAME, so php's fstat() answers 33206 even for an` |
|    - | 1648 | `	 * open .exe -- the extension arm belongs to the path-based stat alone. */` |
|    1 | 1649 | `	ph7_value_int(pWorker,WinStatMode(sInfo.dwFileAttributes,0));` |
|    1 | 1650 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - | 1651 | `	/* nlink */` |
|    1 | 1652 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 | 1653 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - | 1654 | `	/* uid,gid,rdev */` |
|    1 | 1655 | `	ph7_value_int(pWorker,0);` |
|    1 | 1656 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 | 1657 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 | 1658 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - | 1659 | `	/* size */` |
|    1 | 1660 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 | 1661 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - | 1662 | `	/* atime */` |
|    1 | 1663 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 | 1664 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - | 1665 | `	/* mtime */` |
|    1 | 1666 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 | 1667 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - | 1668 | `	/* ctime */` |
|    1 | 1669 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 | 1670 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - | 1671 | `	/* blksize,blocks: php reports -1 for both on Windows -- on every file and` |
|    - | 1672 | `	 * every stream -- having neither field to fill them from. */` |
|    1 | 1673 | `	ph7_value_int(pWorker,-1);` |
|    1 | 1674 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 | 1675 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 | 1676 | `	return PH7_OK;` |
|    1 | 1677 | `}` |
|    - | 1678 | `/* Export the file:// stream */` |
|    - | 1679 | `/*` |
|    - | 1680 | ` * php copies a plain file with MapViewOfFile(), and a view has to START on an` |
|    - | 1681 | ` * allocation granule, so it maps from the granule the position sits in. At the` |
|    - | 1682 | ` * end of the file that is a view of zero requested bytes -- which php's copy` |
|    - | 1683 | ` * loop reads as a failure -- unless the end sits ON a granule (an empty file` |
|    - | 1684 | ` * included), where php refuses the zero-length view and its read loop answers` |
|    - | 1685 | ` * 0. Linux's mmap() refuses every zero-length map, so there it is always 0.` |
|    - | 1686 | ` * nAhead is what the handle has read ahead of the script's position.` |
|    - | 1687 | ` */` |
|    - | 1688 | `PH7_PRIVATE int PH7_WinFileMapsEmptyView(void *pHandle,ph7_int64 nAhead)` |
|    1 | 1689 | `{` |
|    - | 1690 | `	LARGE_INTEGER iSize,iZero,iPos;` |
|    - | 1691 | `	SYSTEM_INFO sInfo;` |
|    1 | 1692 | `	iZero.QuadPart = 0;` |
|    - | 1693 | `	if( !GetFileSizeEx((HANDLE)pHandle,&iSize)` |
|    - | 1694 | `	 \|\| !SetFilePointerEx((HANDLE)pHandle,iZero,&iPos,FILE_CURRENT)` |
|    1 | 1695 | `	 \|\| iPos.QuadPart - nAhead < iSize.QuadPart ){` |
|    1 | 1696 | `		return 0;` |
|    - | 1697 | `	}` |
|    1 | 1698 | `	GetSystemInfo(&sInfo);` |
|    1 | 1699 | `	return sInfo.dwAllocationGranularity != 0` |
|    - | 1700 | `		&& (iSize.QuadPart % (ph7_int64)sInfo.dwAllocationGranularity) != 0;` |
|    1 | 1701 | `}` |
|    - | 1702 | `PH7_PRIVATE const ph7_io_stream sWinFileStream = {` |
|    - | 1703 | `	"file", /* Stream name */` |
|    - | 1704 | `	PH7_IO_STREAM_VERSION,` |
|    - | 1705 | `	WinFile_Open,  /* xOpen */` |
|    - | 1706 | `	WinDir_Open,   /* xOpenDir */` |
|    - | 1707 | `	WinFile_Close, /* xClose */` |
|    - | 1708 | `	WinDir_Close,  /* xCloseDir */` |
|    - | 1709 | `	WinFile_Read,  /* xRead */` |
|    - | 1710 | `	WinDir_Read,   /* xReadDir */` |
|    - | 1711 | `	WinFile_Write, /* xWrite */` |
|    - | 1712 | `	WinFile_Seek,  /* xSeek */` |
|    - | 1713 | `	WinFile_Lock,  /* xLock */` |
|    - | 1714 | `	WinDir_RewindDir, /* xRewindDir */` |
|    - | 1715 | `	WinFile_Tell,  /* xTell */` |
|    - | 1716 | `	WinFile_Trunc, /* xTrunc */` |
|    - | 1717 | `	WinFile_Sync,  /* xSeek */` |
|    - | 1718 | `	WinFile_Stat   /* xStat */` |
|    - | 1719 | `};` |
|    - | 1720 | `#endif /* __WINNT__ */` |
|    - | 1721 |  |
