# src/ph7/vfs_win.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 487/710 lines (68.59%)

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
|  ! 0 |   94 | `		case ERROR_LOCK_VIOLATION:    errno = EACCES; break;` |
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
|  ! 0 |  113 | `	zRest = &zPath[sizeof("file://")-1];` |
|  ! 0 |  114 | `	if( SyStrnicmp(zRest,"localhost/",sizeof("localhost/")-1) == 0 ){` |
|  ! 0 |  115 | `		zRest = &zRest[sizeof("localhost")-1]; /* keep the leading slash */` |
|    - |  116 | `	}` |
|  ! 0 |  117 | `	if( zRest[0] == '/' && zRest[1] != 0 && zRest[2] == ':' ){` |
|    - |  118 | `		/* file:///C:/path or file://localhost/C:/path -> C:/path */` |
|  ! 0 |  119 | `		return &zRest[1];` |
|    - |  120 | `	}` |
|  ! 0 |  121 | `	return zRest;` |
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
|    2 |  158 | `{` |
|    - |  159 | `	void * pConverted;` |
|    - |  160 | `	BOOL rc;` |
|    2 |  161 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  162 | `	if( pConverted == 0 ){` |
|  ! 0 |  163 | `		return -1;` |
|    - |  164 | `	}` |
|    2 |  165 | `	mode= 0; /* MSVC warning */` |
|    2 |  166 | `	recursive = 0;` |
|    2 |  167 | `	rc = CreateDirectoryW((LPCWSTR)pConverted,0);` |
|    2 |  168 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    2 |  169 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  170 | `	return rc ? PH7_OK : -1;` |
|    2 |  171 | `}` |
|    - |  172 | `/* int (*xRmdir)(const char *) */` |
|    - |  173 | `static int WinVfs_rmdir(const char *zPath)` |
|    1 |  174 | `{` |
|    - |  175 | `	void * pConverted;` |
|    - |  176 | `	BOOL rc;` |
|    1 |  177 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  178 | `	if( pConverted == 0 ){` |
|  ! 0 |  179 | `		return -1;` |
|    - |  180 | `	}` |
|    1 |  181 | `	rc = RemoveDirectoryW((LPCWSTR)pConverted);` |
|    1 |  182 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    1 |  183 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  184 | `	return rc ? PH7_OK : -1;` |
|    1 |  185 | `}` |
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
|    1 |  225 | `{` |
|    - |  226 | `	WCHAR zTemp[2048];` |
|    - |  227 | `	void *pPath;` |
|    - |  228 | `	char *zReal;` |
|    - |  229 | `	DWORD n;` |
|    1 |  230 | `	pPath = convertUtf8Filename(zPath);` |
|    1 |  231 | `	if( pPath == 0 ){` |
|  ! 0 |  232 | `		return -1;` |
|    - |  233 | `	}` |
|    1 |  234 | `	n = GetFullPathNameW((LPCWSTR)pPath,0,0,0);` |
|    1 |  235 | `	if( n > 0 ){` |
|    1 |  236 | `		if( n >= sizeof(zTemp) ){` |
|  ! 0 |  237 | `			n = sizeof(zTemp) - 1;` |
|    - |  238 | `		}` |
|    1 |  239 | `		GetFullPathNameW((LPCWSTR)pPath,n,zTemp,0);` |
|    - |  240 | `	}` |
|    1 |  241 | `	HeapFree(GetProcessHeap(),0,pPath);` |
|    1 |  242 | `	if( !n ){` |
|  ! 0 |  243 | `		return -1;` |
|    - |  244 | `	}` |
|    1 |  245 | `	zReal = unicodeToUtf8(zTemp);` |
|    1 |  246 | `	if( zReal == 0 ){` |
|  ! 0 |  247 | `		return -1;` |
|    - |  248 | `	}` |
|    1 |  249 | `	ph7_result_string(pCtx,zReal,-1); /* Will make it's own copy */` |
|    1 |  250 | `	HeapFree(GetProcessHeap(),0,zReal);` |
|    1 |  251 | `	return PH7_OK;` |
|    1 |  252 | `}` |
|    - |  253 | `/* int (*xSleep)(unsigned int) */` |
|    - |  254 | `static int WinVfs_Sleep(unsigned int uSec)` |
|    1 |  255 | `{` |
|    1 |  256 | `	Sleep(uSec/1000/*uSec per Millisec */);` |
|    1 |  257 | `	return PH7_OK;` |
|    1 |  258 | `}` |
|    - |  259 | `/* int (*xUnlink)(const char *) */` |
|    - |  260 | `static int WinVfs_unlink(const char *zPath)` |
|    5 |  261 | `{` |
|    - |  262 | `	void * pConverted;` |
|    - |  263 | `	BOOL rc;` |
|    5 |  264 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  265 | `	if( pConverted == 0 ){` |
|  ! 0 |  266 | `		return -1;` |
|    - |  267 | `	}` |
|    5 |  268 | `	rc = DeleteFileW((LPCWSTR)pConverted);` |
|    5 |  269 | `	if( !rc ){ WinVfsMapErrno(); }` |
|    5 |  270 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  271 | `	return rc ? PH7_OK : - 1;` |
|    5 |  272 | `}` |
|    - |  273 | `/* int (*xChmod)(const char *,int) */` |
|    - |  274 | `static int WinVfs_chmod(const char *zPath,int mode)` |
|  ! 0 |  275 | `{` |
|    - |  276 | `	void * pConverted;` |
|    - |  277 | `	int rc;` |
|  ! 0 |  278 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  279 | `	if( pConverted == 0 ){` |
|  ! 0 |  280 | `		return -1;` |
|    - |  281 | `	}` |
|    - |  282 | `	/* Windows honors only the read-only attribute: a set owner-write bit (0200)` |
|    - |  283 | `	 * clears it, otherwise the file is made read-only. This mirrors php, whose` |
|    - |  284 | `	 * chmod() on Windows likewise maps through _wchmod and returns success. */` |
|  ! 0 |  285 | `	rc = _wchmod((const wchar_t *)pConverted,(mode & 0200) ? (_S_IREAD\|_S_IWRITE) : _S_IREAD);` |
|  ! 0 |  286 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  287 | `	return rc == 0 ? PH7_OK : - 1;` |
|  ! 0 |  288 | `}` |
|    - |  289 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - |  290 | `static ph7_int64 WinVfs_DiskFreeSpace(const char *zPath)` |
|  ! 0 |  291 | `{` |
|    - |  292 | `#ifdef _WIN32_WCE` |
|    - |  293 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|    - |  294 | `	SXUNUSED(zPath);` |
|    - |  295 | `	return 0;` |
|    - |  296 | `#else` |
|    - |  297 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|    - |  298 | `	void * pConverted;` |
|    - |  299 | `	WCHAR *p;` |
|    - |  300 | `	BOOL rc;` |
|  ! 0 |  301 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  302 | `	if( pConverted == 0 ){` |
|  ! 0 |  303 | `		return 0;` |
|    - |  304 | `	}` |
|  ! 0 |  305 | `	p = (WCHAR *)pConverted;` |
|  ! 0 |  306 | `	for(;*p;p++){` |
|  ! 0 |  307 | `		if( *p == '\\' \|\| *p == '/'){` |
|  ! 0 |  308 | `			*p = '\0';` |
|  ! 0 |  309 | `			break;` |
|    - |  310 | `		}` |
|  ! 0 |  311 | `	}` |
|  ! 0 |  312 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|  ! 0 |  313 | `	if( !rc ){` |
|  ! 0 |  314 | `		return 0;` |
|    - |  315 | `	}` |
|  ! 0 |  316 | `	return (ph7_int64)dwFreeClusters * dwSectPerClust * dwBytesPerSect;` |
|    - |  317 | `#endif` |
|  ! 0 |  318 | `}` |
|    - |  319 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - |  320 | `static ph7_int64 WinVfs_DiskTotalSpace(const char *zPath)` |
|  ! 0 |  321 | `{` |
|    - |  322 | `#ifdef _WIN32_WCE` |
|    - |  323 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|    - |  324 | `	SXUNUSED(zPath);` |
|    - |  325 | `	return 0;` |
|    - |  326 | `#else` |
|    - |  327 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|    - |  328 | `	void * pConverted;` |
|    - |  329 | `	WCHAR *p;` |
|    - |  330 | `	BOOL rc;` |
|  ! 0 |  331 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  332 | `	if( pConverted == 0 ){` |
|  ! 0 |  333 | `		return 0;` |
|    - |  334 | `	}` |
|  ! 0 |  335 | `	p = (WCHAR *)pConverted;` |
|  ! 0 |  336 | `	for(;*p;p++){` |
|  ! 0 |  337 | `		if( *p == '\\' \|\| *p == '/'){` |
|  ! 0 |  338 | `			*p = '\0';` |
|  ! 0 |  339 | `			break;` |
|    - |  340 | `		}` |
|  ! 0 |  341 | `	}` |
|  ! 0 |  342 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|  ! 0 |  343 | `	if( !rc ){` |
|  ! 0 |  344 | `		return 0;` |
|    - |  345 | `	}` |
|  ! 0 |  346 | `	return (ph7_int64)dwTotalClusters * dwSectPerClust * dwBytesPerSect;` |
|    - |  347 | `#endif` |
|  ! 0 |  348 | `}` |
|    - |  349 | `/* int (*xFileExists)(const char *) */` |
|    - |  350 | `static int WinVfs_FileExists(const char *zPath)` |
|    2 |  351 | `{` |
|    2 |  352 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  353 | `	void * pConverted;` |
|    - |  354 | `	DWORD dwAttr;` |
|    2 |  355 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  356 | `	if( pConverted == 0 ){` |
|  ! 0 |  357 | `		return -1;` |
|    - |  358 | `	}` |
|    2 |  359 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    2 |  360 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  361 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    2 |  362 | `		return -1;` |
|    - |  363 | `	}` |
|    2 |  364 | `	return PH7_OK;` |
|    2 |  365 | `}` |
|    - |  366 | `/* Open a file in a read-only mode */` |
|    - |  367 | `static HANDLE OpenReadOnly(LPCWSTR pPath)` |
|    5 |  368 | `{` |
|    5 |  369 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    5 |  370 | `	DWORD dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    5 |  371 | `	DWORD dwAccess = GENERIC_READ;` |
|    5 |  372 | `	DWORD dwCreate = OPEN_EXISTING;` |
|    - |  373 | `	HANDLE pHandle;` |
|    5 |  374 | `	pHandle = CreateFileW(pPath,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    5 |  375 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    1 |  376 | `		return 0;` |
|    - |  377 | `	}` |
|    5 |  378 | `	return pHandle;` |
|    5 |  379 | `}` |
|    - |  380 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  381 | `static ph7_int64 WinVfs_FileSize(const char *zPath)` |
|    1 |  382 | `{` |
|    1 |  383 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  384 | `	DWORD dwLow,dwHigh;` |
|    - |  385 | `	void * pConverted;` |
|    - |  386 | `	ph7_int64 nSize;` |
|    - |  387 | `	HANDLE pHandle;` |
|    - |  388 |  |
|    1 |  389 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  390 | `	if( pConverted == 0 ){` |
|  ! 0 |  391 | `		return -1;` |
|    - |  392 | `	}` |
|    - |  393 | `	/* Open the file in read-only mode */` |
|    1 |  394 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  395 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  396 | `	if( pHandle ){` |
|    1 |  397 | `		dwLow = GetFileSize(pHandle,&dwHigh);` |
|    1 |  398 | `		nSize = dwHigh;` |
|    1 |  399 | `		nSize <<= 32;` |
|    1 |  400 | `		nSize += dwLow;` |
|    1 |  401 | `		CloseHandle(pHandle);` |
|    1 |  402 | `	}else{` |
|  ! 0 |  403 | `		nSize = -1;` |
|    - |  404 | `	}` |
|    1 |  405 | `	return nSize;` |
|    1 |  406 | `}` |
|    - |  407 | `#define TICKS_PER_SECOND 10000000` |
|    - |  408 | `#define EPOCH_DIFFERENCE 11644473600LL` |
|    - |  409 | `/* Convert Windows timestamp to UNIX timestamp */` |
|    - |  410 | `static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)` |
|    1 |  411 | `{` |
|    - |  412 | `    ph7_int64 input,temp;` |
|    1 |  413 | `	input = pTime->dwHighDateTime;` |
|    1 |  414 | `	input <<= 32;` |
|    1 |  415 | `	input += pTime->dwLowDateTime;` |
|    1 |  416 | `    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/` |
|    1 |  417 | `    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/` |
|    1 |  418 | `    return temp;` |
|    1 |  419 | `}` |
|    - |  420 | `/* Convert UNIX timestamp to Windows timestamp */` |
|    - |  421 | `static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)` |
|  ! 0 |  422 | `{` |
|  ! 0 |  423 | `  ph7_int64 result = EPOCH_DIFFERENCE;` |
|  ! 0 |  424 | `  result += nUnixtime;` |
|  ! 0 |  425 | `  result *= 10000000LL;` |
|  ! 0 |  426 | `  pOut->dwHighDateTime = (DWORD)(nUnixtime>>32);` |
|  ! 0 |  427 | `  pOut->dwLowDateTime = (DWORD)nUnixtime;` |
|  ! 0 |  428 | `}` |
|    - |  429 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  430 | `static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|    1 |  431 | `{` |
|    - |  432 | `	FILETIME sTouch,sAccess;` |
|    - |  433 | `	void *pConverted;` |
|    - |  434 | `	void *pHandle;` |
|    1 |  435 | `	BOOL rc = 0;` |
|    1 |  436 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  437 | `	if( pConverted == 0 ){` |
|  ! 0 |  438 | `		return -1;` |
|    - |  439 | `	}` |
|    1 |  440 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  441 | `	if( pHandle ){` |
|    1 |  442 | `		if( touch_time < 0 ){` |
|    1 |  443 | `			GetSystemTimeAsFileTime(&sTouch);` |
|    1 |  444 | `		}else{` |
|  ! 0 |  445 | `			convertUnixTimeToWindowsTime(touch_time,&sTouch);` |
|    - |  446 | `		}` |
|    1 |  447 | `		if( access_time < 0 ){` |
|    - |  448 | `			/* Use the touch time */` |
|    1 |  449 | `			sAccess = sTouch; /* Structure assignment */` |
|    1 |  450 | `		}else{` |
|  ! 0 |  451 | `			convertUnixTimeToWindowsTime(access_time,&sAccess);` |
|    - |  452 | `		}` |
|    1 |  453 | `		rc = SetFileTime(pHandle,&sTouch,&sAccess,0);` |
|    - |  454 | `		/* Close the handle */` |
|    1 |  455 | `		CloseHandle(pHandle);` |
|    - |  456 | `	}` |
|    1 |  457 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  458 | `	return rc ? PH7_OK : -1;` |
|    1 |  459 | `}` |
|    - |  460 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  461 | `static ph7_int64 WinVfs_FileAtime(const char *zPath)` |
|    1 |  462 | `{` |
|    1 |  463 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  464 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  465 | `	void * pConverted;` |
|    - |  466 | `	ph7_int64 atime;` |
|    - |  467 | `	HANDLE pHandle;` |
|    1 |  468 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  469 | `	if( pConverted == 0 ){` |
|  ! 0 |  470 | `		return -1;` |
|    - |  471 | `	}` |
|    - |  472 | `	/* Open the file in read-only mode */` |
|    1 |  473 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  474 | `	if( pHandle ){` |
|    - |  475 | `		BOOL rc;` |
|    1 |  476 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  477 | `		if( rc ){` |
|    1 |  478 | `			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);` |
|    1 |  479 | `		}else{` |
|  ! 0 |  480 | `			atime = -1;` |
|    - |  481 | `		}` |
|    1 |  482 | `		CloseHandle(pHandle);` |
|    1 |  483 | `	}else{` |
|  ! 0 |  484 | `		atime = -1;` |
|    - |  485 | `	}` |
|    1 |  486 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  487 | `	return atime;` |
|    1 |  488 | `}` |
|    - |  489 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  490 | `static ph7_int64 WinVfs_FileMtime(const char *zPath)` |
|    1 |  491 | `{` |
|    1 |  492 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  493 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  494 | `	void * pConverted;` |
|    - |  495 | `	ph7_int64 mtime;` |
|    - |  496 | `	HANDLE pHandle;` |
|    1 |  497 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  498 | `	if( pConverted == 0 ){` |
|  ! 0 |  499 | `		return -1;` |
|    - |  500 | `	}` |
|    - |  501 | `	/* Open the file in read-only mode */` |
|    1 |  502 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  503 | `	if( pHandle ){` |
|    - |  504 | `		BOOL rc;` |
|    1 |  505 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  506 | `		if( rc ){` |
|    1 |  507 | `			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);` |
|    1 |  508 | `		}else{` |
|  ! 0 |  509 | `			mtime = -1;` |
|    - |  510 | `		}` |
|    1 |  511 | `		CloseHandle(pHandle);` |
|    1 |  512 | `	}else{` |
|  ! 0 |  513 | `		mtime = -1;` |
|    - |  514 | `	}` |
|    1 |  515 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  516 | `	return mtime;` |
|    1 |  517 | `}` |
|    - |  518 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  519 | `static ph7_int64 WinVfs_FileCtime(const char *zPath)` |
|    1 |  520 | `{` |
|    1 |  521 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  522 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  523 | `	void * pConverted;` |
|    - |  524 | `	ph7_int64 ctime;` |
|    - |  525 | `	HANDLE pHandle;` |
|    1 |  526 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  527 | `	if( pConverted == 0 ){` |
|  ! 0 |  528 | `		return -1;` |
|    - |  529 | `	}` |
|    - |  530 | `	/* Open the file in read-only mode */` |
|    1 |  531 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  532 | `	if( pHandle ){` |
|    - |  533 | `		BOOL rc;` |
|    1 |  534 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  535 | `		if( rc ){` |
|    1 |  536 | `			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);` |
|    1 |  537 | `		}else{` |
|  ! 0 |  538 | `			ctime = -1;` |
|    - |  539 | `		}` |
|    1 |  540 | `		CloseHandle(pHandle);` |
|    1 |  541 | `	}else{` |
|  ! 0 |  542 | `		ctime = -1;` |
|    - |  543 | `	}` |
|    1 |  544 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  545 | `	return ctime;` |
|    1 |  546 | `}` |
|    - |  547 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  548 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  549 | `static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|    1 |  550 | `{` |
|    1 |  551 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  552 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  553 | `	void *pConverted;` |
|    - |  554 | `	HANDLE pHandle;` |
|    - |  555 | `	BOOL rc;` |
|    1 |  556 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  557 | `	if( pConverted == 0 ){` |
|  ! 0 |  558 | `		return -1;` |
|    - |  559 | `	}` |
|    - |  560 | `	/* Open the file in read-only mode */` |
|    1 |  561 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  562 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  563 | `	if( pHandle == 0 ){` |
|  ! 0 |  564 | `		return -1;` |
|    - |  565 | `	}` |
|    1 |  566 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  567 | `	CloseHandle(pHandle);` |
|    1 |  568 | `	if( !rc ){` |
|  ! 0 |  569 | `		return -1;` |
|    - |  570 | `	}` |
|    - |  571 | `	/* dev */` |
|    1 |  572 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 |  573 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - |  574 | `	/* ino */` |
|    1 |  575 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 |  576 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - |  577 | `	/* mode */` |
|    1 |  578 | `	ph7_value_int(pWorker,0);` |
|    1 |  579 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - |  580 | `	/* nlink */` |
|    1 |  581 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 |  582 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - |  583 | `	/* uid,gid,rdev */` |
|    1 |  584 | `	ph7_value_int(pWorker,0);` |
|    1 |  585 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 |  586 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 |  587 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - |  588 | `	/* size */` |
|    1 |  589 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 |  590 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - |  591 | `	/* atime */` |
|    1 |  592 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 |  593 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - |  594 | `	/* mtime */` |
|    1 |  595 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 |  596 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - |  597 | `	/* ctime */` |
|    1 |  598 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 |  599 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - |  600 | `	/* blksize,blocks */` |
|    1 |  601 | `	ph7_value_int(pWorker,0);` |
|    1 |  602 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 |  603 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 |  604 | `	return PH7_OK;` |
|    1 |  605 | `}` |
|    - |  606 | `/* int (*xIsfile)(const char *) */` |
|    - |  607 | `static int WinVfs_isfile(const char *zPath)` |
|    5 |  608 | `{` |
|    5 |  609 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  610 | `	void * pConverted;` |
|    - |  611 | `	DWORD dwAttr;` |
|    5 |  612 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  613 | `	if( pConverted == 0 ){` |
|  ! 0 |  614 | `		return -1;` |
|    - |  615 | `	}` |
|    5 |  616 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  617 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  618 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  619 | `		return -1;` |
|    - |  620 | `	}` |
|    5 |  621 | `	return (dwAttr & (FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;` |
|    5 |  622 | `}` |
|    - |  623 | `/* int (*xIslink)(const char *) */` |
|    - |  624 | `static int WinVfs_islink(const char *zPath)` |
|  ! 0 |  625 | `{` |
|  ! 0 |  626 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  627 | `	void * pConverted;` |
|    - |  628 | `	DWORD dwAttr;` |
|  ! 0 |  629 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  630 | `	if( pConverted == 0 ){` |
|  ! 0 |  631 | `		return -1;` |
|    - |  632 | `	}` |
|  ! 0 |  633 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|  ! 0 |  634 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  635 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  636 | `		return -1;` |
|    - |  637 | `	}` |
|  ! 0 |  638 | `	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;` |
|  ! 0 |  639 | `}` |
|    - |  640 | `/* int (*xWritable)(const char *) */` |
|    - |  641 | `static int WinVfs_iswritable(const char *zPath)` |
|  ! 0 |  642 | `{` |
|  ! 0 |  643 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  644 | `	void * pConverted;` |
|    - |  645 | `	DWORD dwAttr;` |
|  ! 0 |  646 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  647 | `	if( pConverted == 0 ){` |
|  ! 0 |  648 | `		return -1;` |
|    - |  649 | `	}` |
|  ! 0 |  650 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|  ! 0 |  651 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  652 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  653 | `		return -1;` |
|    - |  654 | `	}` |
|  ! 0 |  655 | `	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE\|FILE_ATTRIBUTE_NORMAL)) == 0 ){` |
|    - |  656 | `		/* Not a regular file */` |
|  ! 0 |  657 | `		return -1;` |
|    - |  658 | `	}` |
|  ! 0 |  659 | `	if( dwAttr & FILE_ATTRIBUTE_READONLY ){` |
|    - |  660 | `		/* Read-only file */` |
|  ! 0 |  661 | `		return -1;` |
|    - |  662 | `	}` |
|    - |  663 | `	/* File is writable */` |
|  ! 0 |  664 | `	return PH7_OK;` |
|  ! 0 |  665 | `}` |
|    - |  666 | `/* int (*xExecutable)(const char *) */` |
|    - |  667 | `static int WinVfs_isexecutable(const char *zPath)` |
|  ! 0 |  668 | `{` |
|  ! 0 |  669 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  670 | `	void * pConverted;` |
|    - |  671 | `	DWORD dwAttr;` |
|  ! 0 |  672 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  673 | `	if( pConverted == 0 ){` |
|  ! 0 |  674 | `		return -1;` |
|    - |  675 | `	}` |
|  ! 0 |  676 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|  ! 0 |  677 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  678 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  679 | `		return -1;` |
|    - |  680 | `	}` |
|  ! 0 |  681 | `	if( (dwAttr & FILE_ATTRIBUTE_NORMAL) == 0 ){` |
|    - |  682 | `		/* Not a regular file */` |
|  ! 0 |  683 | `		return -1;` |
|    - |  684 | `	}` |
|    - |  685 | `	/* File is executable */` |
|  ! 0 |  686 | `	return PH7_OK;` |
|  ! 0 |  687 | `}` |
|    - |  688 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  689 | `static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|    1 |  690 | `{` |
|    1 |  691 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  692 | `	void * pConverted;` |
|    - |  693 | `	DWORD dwAttr;` |
|    1 |  694 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  695 | `	if( pConverted == 0 ){` |
|    - |  696 | `		/* Expand 'unknown' */` |
|  ! 0 |  697 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  698 | `		return -1;` |
|    - |  699 | `	}` |
|    1 |  700 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  701 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  702 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    - |  703 | `		/* Expand 'unknown' */` |
|  ! 0 |  704 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  705 | `		return -1;` |
|    - |  706 | `	}` |
|    1 |  707 | `	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE) ){` |
|    1 |  708 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|    1 |  709 | `	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){` |
|    1 |  710 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|  ! 0 |  711 | `	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){` |
|  ! 0 |  712 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|  ! 0 |  713 | `	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){` |
|  ! 0 |  714 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|  ! 0 |  715 | `	}else{` |
|  ! 0 |  716 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    - |  717 | `	}` |
|    1 |  718 | `	return PH7_OK;` |
|    1 |  719 | `}` |
|    - |  720 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  721 | `static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|    5 |  722 | `{` |
|    - |  723 | `	char zValue[1024];` |
|    - |  724 | `	DWORD n;` |
|    - |  725 | `	/*` |
|    - |  726 | `	 * According to MSDN` |
|    - |  727 | `	 * If lpBuffer is not large enough to hold the data, the return` |
|    - |  728 | `	 * value is the buffer size, in characters, required to hold the` |
|    - |  729 | `	 * string and its terminating null character and the contents` |
|    - |  730 | `	 * of lpBuffer are undefined.` |
|    - |  731 | `	 */` |
|    5 |  732 | `	n = sizeof(zValue);` |
|    5 |  733 | `	SyMemcpy("Undefined",zValue,sizeof("Undefined")-1);` |
|    - |  734 | `	/* Extract the environment value */` |
|    5 |  735 | `	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));` |
|    5 |  736 | `	if( !n ){` |
|    - |  737 | `		/* No such variable*/` |
|  ! 0 |  738 | `		return -1;` |
|    - |  739 | `	}` |
|    5 |  740 | `	ph7_result_string(pCtx,zValue,(int)n);` |
|    5 |  741 | `	return PH7_OK;` |
|    5 |  742 | `}` |
|    - |  743 | `/* int (*xSetenv)(const char *,const char *) */` |
|    - |  744 | `static int WinVfs_Setenv(const char *zName,const char *zValue)` |
|    1 |  745 | `{` |
|    - |  746 | `	BOOL rc;` |
|    1 |  747 | `	rc = SetEnvironmentVariableA(zName,zValue);` |
|    1 |  748 | `	return rc ? PH7_OK : -1;` |
|    1 |  749 | `}` |
|    - |  750 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  751 | `static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|    5 |  752 | `{` |
|    - |  753 | `	DWORD dwSizeLow,dwSizeHigh;` |
|    - |  754 | `	HANDLE pHandle,pMapHandle;` |
|    - |  755 | `	void *pConverted,*pView;` |
|    - |  756 |  |
|    5 |  757 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  758 | `	if( pConverted == 0 ){` |
|  ! 0 |  759 | `		return -1;` |
|    - |  760 | `	}` |
|    5 |  761 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    5 |  762 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  763 | `	if( pHandle == 0 ){` |
|    1 |  764 | `		return -1;` |
|    - |  765 | `	}` |
|    - |  766 | `	/* Get the file size */` |
|    5 |  767 | `	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);` |
|    - |  768 | `	/* Create the mapping */` |
|    5 |  769 | `	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);` |
|    5 |  770 | `	if( pMapHandle == 0 ){` |
|  ! 0 |  771 | `		CloseHandle(pHandle);` |
|  ! 0 |  772 | `		return -1;` |
|    - |  773 | `	}` |
|    5 |  774 | `	*pSize = ((ph7_int64)dwSizeHigh << 32) \| dwSizeLow;` |
|    - |  775 | `	/* Obtain the view */` |
|    5 |  776 | `	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));` |
|    5 |  777 | `	if( pView ){` |
|    - |  778 | `		/* Let the upper layer point to the view */` |
|    5 |  779 | `		*ppMap = pView;` |
|    - |  780 | `	}` |
|    - |  781 | `	/* Close the handle` |
|    - |  782 | `	 * According to MSDN it's OK the close the HANDLES.` |
|    - |  783 | `	 */` |
|    5 |  784 | `	CloseHandle(pMapHandle);` |
|    5 |  785 | `	CloseHandle(pHandle);` |
|    5 |  786 | `	return pView ? PH7_OK : -1;` |
|    5 |  787 | `}` |
|    - |  788 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|    - |  789 | `static void WinVfs_Unmap(void *pView,ph7_int64 nSize)` |
|    5 |  790 | `{` |
|    5 |  791 | `	nSize = 0; /* Compiler warning */` |
|    5 |  792 | `	UnmapViewOfFile(pView);` |
|    5 |  793 | `}` |
|    - |  794 | `/* void (*xTempDir)(ph7_context *) */` |
|    - |  795 | `static void WinVfs_TempDir(ph7_context *pCtx)` |
|    3 |  796 | `{` |
|    - |  797 | `	CHAR zTemp[1024];` |
|    - |  798 | `	DWORD n;` |
|    3 |  799 | `	n = GetTempPathA(sizeof(zTemp),zTemp);` |
|    3 |  800 | `	if( n < 1 ){` |
|    - |  801 | `		/* Assume the default windows temp directory */` |
|  ! 0 |  802 | `		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);` |
|  ! 0 |  803 | `	}else{` |
|    3 |  804 | `		ph7_result_string(pCtx,zTemp,(int)n);` |
|    - |  805 | `	}` |
|    3 |  806 | `}` |
|    - |  807 | `/* unsigned int (*xProcessId)(void) */` |
|    - |  808 | `static unsigned int WinVfs_ProcessId(void)` |
|    2 |  809 | `{` |
|    2 |  810 | `	DWORD nID = 0;` |
|    - |  811 | `#ifndef __MINGW32__` |
|    2 |  812 | `	nID = GetProcessId(GetCurrentProcess());` |
|    - |  813 | `#endif /* __MINGW32__ */` |
|    2 |  814 | `	return (unsigned int)nID;` |
|    2 |  815 | `}` |
|    - |  816 | `/* void (*xUsername)(ph7_context *) */` |
|    - |  817 | `static void WinVfs_Username(ph7_context *pCtx)` |
|    1 |  818 | `{` |
|    - |  819 | `	WCHAR zUser[1024];` |
|    - |  820 | `	DWORD nByte;` |
|    - |  821 | `	BOOL rc;` |
|    1 |  822 | `	nByte = sizeof(zUser);` |
|    1 |  823 | `	rc = GetUserNameW(zUser,&nByte);` |
|    1 |  824 | `	if( !rc ){` |
|    - |  825 | `		/* Set a dummy name */` |
|  ! 0 |  826 | `		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  827 | `	}else{` |
|    - |  828 | `		char *zName;` |
|    1 |  829 | `		zName = unicodeToUtf8(zUser);` |
|    1 |  830 | `		if( zName == 0 ){` |
|  ! 0 |  831 | `			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  832 | `		}else{` |
|    1 |  833 | `			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    1 |  834 | `			HeapFree(GetProcessHeap(),0,zName);` |
|    - |  835 | `		}` |
|    - |  836 | `	}` |
|    - |  837 |  |
|    1 |  838 | `}` |
|    - |  839 | `/* int (*xChroot)(const char *) — Windows has no chroot; fail cleanly so chroot()` |
|    - |  840 | ` * returns false. (php has no chroot symbol at all; PHL exposes it as an extension` |
|    - |  841 | ` * and this reports the failure without a "not implemented in the VFS" warning.) */` |
|    - |  842 | `static int WinVfs_chroot(const char *zPath)` |
|  ! 0 |  843 | `{` |
|    - |  844 | `	(void)zPath;` |
|  ! 0 |  845 | `	return -1;` |
|  ! 0 |  846 | `}` |
|    - |  847 | `#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE` |
|    - |  848 | `#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2` |
|    - |  849 | `#endif` |
|    - |  850 | `#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY` |
|    - |  851 | `#define SYMBOLIC_LINK_FLAG_DIRECTORY 0x1` |
|    - |  852 | `#endif` |
|    - |  853 | `/* int (*xLink)(const char *,const char *,int) — hard link (iSym==0) via` |
|    - |  854 | ` * CreateHardLink or symbolic link (iSym!=0) via CreateSymbolicLink, mirroring` |
|    - |  855 | ` * php on Windows. Developer-mode symlink creation is allowed. */` |
|    - |  856 | `static int WinVfs_Link(const char *zOld,const char *zNew,int iSym)` |
|  ! 0 |  857 | `{` |
|    - |  858 | `	void *pOld, *pNew;` |
|    - |  859 | `	BOOL rc;` |
|  ! 0 |  860 | `	pOld = convertUtf8Filename(zOld);` |
|  ! 0 |  861 | `	if( pOld == 0 ){` |
|  ! 0 |  862 | `		return -1;` |
|    - |  863 | `	}` |
|  ! 0 |  864 | `	pNew = convertUtf8Filename(zNew);` |
|  ! 0 |  865 | `	if( pNew == 0 ){` |
|  ! 0 |  866 | `		HeapFree(GetProcessHeap(),0,pOld);` |
|  ! 0 |  867 | `		return -1;` |
|    - |  868 | `	}` |
|  ! 0 |  869 | `	if( iSym ){` |
|  ! 0 |  870 | `		DWORD dwFlags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;` |
|  ! 0 |  871 | `		DWORD attr = GetFileAttributesW((LPCWSTR)pOld);` |
|  ! 0 |  872 | `		if( attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) ){` |
|  ! 0 |  873 | `			dwFlags \|= SYMBOLIC_LINK_FLAG_DIRECTORY;` |
|    - |  874 | `		}` |
|  ! 0 |  875 | `		rc = CreateSymbolicLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,dwFlags) ? TRUE : FALSE;` |
|  ! 0 |  876 | `	}else{` |
|  ! 0 |  877 | `		rc = CreateHardLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,0);` |
|    - |  878 | `	}` |
|  ! 0 |  879 | `	HeapFree(GetProcessHeap(),0,pNew);` |
|  ! 0 |  880 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|  ! 0 |  881 | `	return rc ? PH7_OK : -1;` |
|  ! 0 |  882 | `}` |
|    - |  883 | `/* int (*xUmask)(int) — Windows has no umask; php's umask() returns 0 there. */` |
|    - |  884 | `static int WinVfs_Umask(int iMask)` |
|  ! 0 |  885 | `{` |
|    - |  886 | `	(void)iMask;` |
|  ! 0 |  887 | `	return 0;` |
|  ! 0 |  888 | `}` |
|    - |  889 | `/* int (*xUid)(void) / int (*xGid)(void) — no uid/gid on Windows; php's` |
|    - |  890 | ` * getmyuid()/getmygid() both return 0. */` |
|    - |  891 | `static int WinVfs_Uid(void)` |
|  ! 0 |  892 | `{` |
|  ! 0 |  893 | `	return 0;` |
|  ! 0 |  894 | `}` |
|    - |  895 | `static int WinVfs_Gid(void)` |
|  ! 0 |  896 | `{` |
|  ! 0 |  897 | `	return 0;` |
|  ! 0 |  898 | `}` |
|    - |  899 | `/* Export the windows vfs */` |
|    - |  900 | `PH7_PRIVATE const ph7_vfs sWinVfs = {` |
|    - |  901 | `	"Windows_vfs",` |
|    - |  902 | `	PH7_VFS_VERSION,` |
|    - |  903 | `	WinVfs_chdir,    /* int (*xChdir)(const char *) */` |
|    - |  904 | `	WinVfs_chroot,   /* int (*xChroot)(const char *); */` |
|    - |  905 | `	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|    - |  906 | `	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|    - |  907 | `	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|    - |  908 | `	WinVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|    - |  909 | `	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|    - |  910 | `	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|    - |  911 | `	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */` |
|    - |  912 | `	WinVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|    - |  913 | `	WinVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|    - |  914 | `	WinVfs_chmod, /*int (*xChmod)(const char *,int)*/` |
|    - |  915 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|    - |  916 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|    - |  917 | `	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - |  918 | `	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - |  919 | `	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  920 | `	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  921 | `	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  922 | `	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  923 | `	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  924 | `	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  925 | `	WinVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|    - |  926 | `	WinVfs_islink,     /* int (*xIslink)(const char *) */` |
|    - |  927 | `	WinVfs_isfile,     /* int (*xReadable)(const char *) */` |
|    - |  928 | `	WinVfs_iswritable, /* int (*xWritable)(const char *) */` |
|    - |  929 | `	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */` |
|    - |  930 | `	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  931 | `	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  932 | `	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|    - |  933 | `	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  934 | `	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  935 | `	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|    - |  936 | `	WinVfs_Link,       /* int (*xLink)(const char *,const char *,int) */` |
|    - |  937 | `	WinVfs_Umask,      /* int (*xUmask)(int) */` |
|    - |  938 | `	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|    - |  939 | `	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|    - |  940 | `	WinVfs_Uid, /* int (*xUid)(void) */` |
|    - |  941 | `	WinVfs_Gid, /* int (*xGid)(void) */` |
|    - |  942 | `	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|    - |  943 | `	0 /* int (*xExec)(const char *,ph7_context *) */` |
|    - |  944 | `};` |
|    - |  945 | `/* Windows file IO */` |
|    - |  946 | `#ifndef INVALID_SET_FILE_POINTER` |
|    - |  947 | `# define INVALID_SET_FILE_POINTER ((DWORD)-1)` |
|    - |  948 | `#endif` |
|    - |  949 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|    - |  950 | `static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|    5 |  951 | `{` |
|    5 |  952 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    5 |  953 | `	DWORD dwAccess = GENERIC_READ;` |
|    - |  954 | `	DWORD dwShare,dwCreate;` |
|    - |  955 | `	void *pConverted;` |
|    - |  956 | `	HANDLE pHandle;` |
|    - |  957 |  |
|    5 |  958 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  959 | `	if( pConverted == 0 ){` |
|  ! 0 |  960 | `		return -1;` |
|    - |  961 | `	}` |
|    - |  962 | `	/* Set the desired flags according to the open mode */` |
|    5 |  963 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|    - |  964 | `		/* Open existing file, or create if it doesn't exist */` |
|    5 |  965 | `		dwCreate = OPEN_ALWAYS;` |
|    5 |  966 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - |  967 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|    5 |  968 | `			dwCreate = CREATE_ALWAYS;` |
|    5 |  969 | `		}` |
|    5 |  970 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|    - |  971 | `		/* Creates a new file, only if it does not already exist.` |
|    - |  972 | `		* If the file exists, it fails.` |
|    - |  973 | `		*/` |
|  ! 0 |  974 | `		dwCreate = CREATE_NEW;` |
|    5 |  975 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - |  976 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|    - |  977 | `		 * The file must exist.` |
|    - |  978 | `		 */` |
|  ! 0 |  979 | `		dwCreate = TRUNCATE_EXISTING;` |
|  ! 0 |  980 | `	}else{` |
|    - |  981 | `		/* Opens a file, only if it exists. */` |
|    5 |  982 | `		dwCreate = OPEN_EXISTING;` |
|    - |  983 | `	}` |
|    5 |  984 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|    - |  985 | `		/* Read+Write access */` |
|    5 |  986 | `		dwAccess \|= GENERIC_WRITE;` |
|    5 |  987 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|    - |  988 | `		/* Write only access */` |
|    1 |  989 | `		dwAccess = GENERIC_WRITE;` |
|    - |  990 | `	}` |
|    5 |  991 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|    - |  992 | `		/* Append mode */` |
|  ! 0 |  993 | `		dwAccess = FILE_APPEND_DATA;` |
|    - |  994 | `	}` |
|    5 |  995 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|    - |  996 | `		/* File is temporary */` |
|  ! 0 |  997 | `		dwType = FILE_ATTRIBUTE_TEMPORARY;` |
|    - |  998 | `	}` |
|    5 |  999 | `	dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    5 | 1000 | `	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    5 | 1001 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 | 1002 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    - | 1003 | `		SXUNUSED(pResource); /* MSVC warning */` |
|    4 | 1004 | `		return -1;` |
|    - | 1005 | `	}` |
|    - | 1006 | `	/* Make the handle accessible to the upper layer */` |
|    5 | 1007 | `	*ppHandle = (void *)pHandle;` |
|    5 | 1008 | `	return PH7_OK;` |
|    5 | 1009 | `}` |
|    - | 1010 | `/* An instance of the following structure is used to record state information` |
|    - | 1011 | ` * while iterating throw directory entries.` |
|    - | 1012 | ` */` |
|    - | 1013 | `typedef struct WinDir_Info WinDir_Info;` |
|    - | 1014 | `struct WinDir_Info` |
|    - | 1015 | `{` |
|    - | 1016 | `	HANDLE pDirHandle;` |
|    - | 1017 | `	void *pPath;` |
|    - | 1018 | `	WIN32_FIND_DATAW sInfo;` |
|    - | 1019 | `	int rc;` |
|    - | 1020 | `};` |
|    - | 1021 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|    - | 1022 | `static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|    5 | 1023 | `{` |
|    - | 1024 | `	WinDir_Info *pDirInfo;` |
|    - | 1025 | `	void *pConverted;` |
|    - | 1026 | `	char *zPrep;` |
|    - | 1027 | `	sxu32 n;` |
|    - | 1028 | `	/* Prepare the path */` |
|    5 | 1029 | `	n = SyStrlen(zPath);` |
|    5 | 1030 | `	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);` |
|    5 | 1031 | `	if( zPrep == 0 ){` |
|  ! 0 | 1032 | `		return -1;` |
|    - | 1033 | `	}` |
|    5 | 1034 | `	SyMemcpy((const void *)zPath,zPrep,n);` |
|    5 | 1035 | `	zPrep[n]   = '\\';` |
|    5 | 1036 | `	zPrep[n+1] =  '*';` |
|    5 | 1037 | `	zPrep[n+2] = 0;` |
|    5 | 1038 | `	pConverted = convertUtf8Filename(zPrep);` |
|    5 | 1039 | `	HeapFree(GetProcessHeap(),0,zPrep);` |
|    5 | 1040 | `	if( pConverted == 0 ){` |
|  ! 0 | 1041 | `		return -1;` |
|    - | 1042 | `	}` |
|    - | 1043 | `	/* Allocate a new instance */` |
|    5 | 1044 | `	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));` |
|    5 | 1045 | `	if( pDirInfo == 0 ){` |
|  ! 0 | 1046 | `		pResource = 0; /* Compiler warning */` |
|  ! 0 | 1047 | `		return -1;` |
|    - | 1048 | `	}` |
|    5 | 1049 | `	pDirInfo->rc = SXRET_OK;` |
|    5 | 1050 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);` |
|    5 | 1051 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|    - | 1052 | `		/* Cannot open directory */` |
|  ! 0 | 1053 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 | 1054 | `		HeapFree(GetProcessHeap(),0,pDirInfo);` |
|  ! 0 | 1055 | `		return -1;` |
|    - | 1056 | `	}` |
|    - | 1057 | `	/* Save the path */` |
|    5 | 1058 | `	pDirInfo->pPath = pConverted;` |
|    - | 1059 | `	/* Save our structure */` |
|    5 | 1060 | `	*ppHandle = pDirInfo;` |
|    5 | 1061 | `	return PH7_OK;` |
|    5 | 1062 | `}` |
|    - | 1063 | `/* void (*xCloseDir)(void *) */` |
|    - | 1064 | `static void WinDir_Close(void *pUserData)` |
|    5 | 1065 | `{` |
|    5 | 1066 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    5 | 1067 | `	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){` |
|    5 | 1068 | `		FindClose(pDirInfo->pDirHandle);` |
|    - | 1069 | `	}` |
|    5 | 1070 | `	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);` |
|    5 | 1071 | `	HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    5 | 1072 | `}` |
|    - | 1073 | `/* void (*xClose)(void *); */` |
|    - | 1074 | `static void WinFile_Close(void *pUserData)` |
|    5 | 1075 | `{` |
|    5 | 1076 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    5 | 1077 | `	CloseHandle(pHandle);` |
|    5 | 1078 | `}` |
|    - | 1079 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|    - | 1080 | `static int WinDir_Read(void *pUserData,ph7_context *pCtx)` |
|    5 | 1081 | `{` |
|    5 | 1082 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    - | 1083 | `	LPWIN32_FIND_DATAW pData;` |
|    - | 1084 | `	char *zName;` |
|    - | 1085 | `	BOOL rc;` |
|    5 | 1086 | `	if( pDirInfo->rc != SXRET_OK ){` |
|    - | 1087 | `		/* No more entry to process */` |
|    5 | 1088 | `		return -1;` |
|    - | 1089 | `	}` |
|    5 | 1090 | `	pData = &pDirInfo->sInfo;` |
|    - | 1091 | `	/* php parity: readdir()/scandir() include the '.' and '..' entries, so unlike` |
|    - | 1092 | `	 * the historical PH7 behaviour we return them instead of skipping. */` |
|    5 | 1093 | `	zName = unicodeToUtf8(pData->cFileName);` |
|    5 | 1094 | `	if( zName == 0 ){` |
|    - | 1095 | `		/* Out of memory */` |
|  ! 0 | 1096 | `		return -1;` |
|    - | 1097 | `	}` |
|    - | 1098 | `	/* Return the current file name */` |
|    5 | 1099 | `	ph7_result_string(pCtx,zName,-1);` |
|    5 | 1100 | `	HeapFree(GetProcessHeap(),0,zName);` |
|    - | 1101 | `	/* Point to the next entry */` |
|    5 | 1102 | `	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|    5 | 1103 | `	if( !rc ){` |
|    5 | 1104 | `		pDirInfo->rc = SXERR_EOF;` |
|    - | 1105 | `	}` |
|    5 | 1106 | `	return PH7_OK;` |
|    5 | 1107 | `}` |
|    - | 1108 | `/* void (*xRewindDir)(void *) */` |
|    - | 1109 | `static void WinDir_RewindDir(void *pUserData)` |
|    1 | 1110 | `{` |
|    1 | 1111 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    1 | 1112 | `	FindClose(pDirInfo->pDirHandle);` |
|    1 | 1113 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);` |
|    1 | 1114 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 | 1115 | `		pDirInfo->rc = SXERR_EOF;` |
|  ! 0 | 1116 | `	}else{` |
|    1 | 1117 | `		pDirInfo->rc = SXRET_OK;` |
|    - | 1118 | `	}` |
|    1 | 1119 | `}` |
|    - | 1120 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|    - | 1121 | `static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)` |
|    5 | 1122 | `{` |
|    5 | 1123 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1124 | `	DWORD nRd;` |
|    - | 1125 | `	BOOL rc;` |
|    5 | 1126 | `	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    5 | 1127 | `	if( !rc ){` |
|    - | 1128 | `		/* EOF or IO error */` |
|  ! 0 | 1129 | `		return -1;` |
|    - | 1130 | `	}` |
|    5 | 1131 | `	return (ph7_int64)nRd;` |
|    5 | 1132 | `}` |
|    - | 1133 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|    - | 1134 | `static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)` |
|    5 | 1135 | `{` |
|    5 | 1136 | `	const char *zData = (const char *)pBuffer;` |
|    5 | 1137 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1138 | `	ph7_int64 nCount;` |
|    - | 1139 | `	DWORD nWr;` |
|    - | 1140 | `	BOOL rc;` |
|    5 | 1141 | `	nWr = 0;` |
|    5 | 1142 | `	nCount = 0;` |
|    - | 1143 | `	for(;;){` |
|    5 | 1144 | `		if( nWrite < 1 ){` |
|    5 | 1145 | `			break;` |
|    - | 1146 | `		}` |
|    5 | 1147 | `		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);` |
|    5 | 1148 | `		if( !rc ){` |
|    - | 1149 | `			/* IO error — surface a POSIX errno for the caller's diagnostic` |
|    - | 1150 | `			 * (e.g. a byte-range lock violation reports EACCES like php). */` |
|  ! 0 | 1151 | `			WinVfsMapErrno();` |
|  ! 0 | 1152 | `			break;` |
|    - | 1153 | `		}` |
|    5 | 1154 | `		nWrite -= nWr;` |
|    5 | 1155 | `		nCount += nWr;` |
|    5 | 1156 | `		zData += nWr;` |
|    5 | 1157 | `	}` |
|    5 | 1158 | `	if( nWrite > 0 ){` |
|  ! 0 | 1159 | `		return -1;` |
|    - | 1160 | `	}` |
|    5 | 1161 | `	return nCount;` |
|    5 | 1162 | `}` |
|    - | 1163 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|    - | 1164 | `static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|    1 | 1165 | `{` |
|    1 | 1166 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1167 | `	DWORD dwMove,dwNew;` |
|    - | 1168 | `	LONG nHighOfft;` |
|    1 | 1169 | `	switch(whence){` |
|    - | 1170 | `	case 1:/*SEEK_CUR*/` |
|  ! 0 | 1171 | `		dwMove = FILE_CURRENT;` |
|  ! 0 | 1172 | `		break;` |
|    - | 1173 | `	case 2: /* SEEK_END */` |
|  ! 0 | 1174 | `		dwMove = FILE_END;` |
|  ! 0 | 1175 | `		break;` |
|    - | 1176 | `	case 0: /* SEEK_SET */` |
|    - | 1177 | `	default:` |
|    1 | 1178 | `		dwMove = FILE_BEGIN;` |
|    - | 1179 | `		break;` |
|    - | 1180 | `	}` |
|    1 | 1181 | `	nHighOfft = (LONG)(iOfft >> 32);` |
|    1 | 1182 | `	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);` |
|    1 | 1183 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1184 | `		return -1;` |
|    - | 1185 | `	}` |
|    1 | 1186 | `	return PH7_OK;` |
|    1 | 1187 | `}` |
|    - | 1188 | `/* int (*xLock)(void *,int) */` |
|    - | 1189 | `static int WinFile_Lock(void *pUserData,int lock_type)` |
|  ! 0 | 1190 | `{` |
|  ! 0 | 1191 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1192 | `	OVERLAPPED sDummy;` |
|    - | 1193 | `	BOOL rc;` |
|  ! 0 | 1194 | `	SyZero(&sDummy,sizeof(sDummy));` |
|    - | 1195 | `	/* Lock/unlock the whole file. php locks the maximal byte range, so the lock` |
|    - | 1196 | `	 * is effective even for an empty or freshly-truncated file — the previous` |
|    - | 1197 | `	 * code locked only GetFileSize() bytes (i.e. nothing for a 0-byte file). */` |
|  ! 0 | 1198 | `	if( lock_type < 1 ){` |
|    - | 1199 | `		/* Unlock the file */` |
|  ! 0 | 1200 | `		rc = UnlockFileEx(pHandle,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);` |
|  ! 0 | 1201 | `	}else{` |
|  ! 0 | 1202 | `		DWORD dwFlags = LOCKFILE_FAIL_IMMEDIATELY; /* Shared non-blocking lock by default*/` |
|    - | 1203 | `		/* Lock the file */` |
|  ! 0 | 1204 | `		if( lock_type == 1 /* LOCK_EXCL */ ){` |
|  ! 0 | 1205 | `			dwFlags \|= LOCKFILE_EXCLUSIVE_LOCK;` |
|    - | 1206 | `		}` |
|  ! 0 | 1207 | `		rc = LockFileEx(pHandle,dwFlags,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);` |
|    - | 1208 | `	}` |
|  ! 0 | 1209 | `	return rc ? PH7_OK : -1 /* Lock error */;` |
|  ! 0 | 1210 | `}` |
|    - | 1211 | `/* ph7_int64 (*xTell)(void *) */` |
|    - | 1212 | `static ph7_int64 WinFile_Tell(void *pUserData)` |
|    1 | 1213 | `{` |
|    1 | 1214 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1215 | `	DWORD dwNew;` |
|    1 | 1216 | `	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);` |
|    1 | 1217 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1218 | `		return -1;` |
|    - | 1219 | `	}` |
|    1 | 1220 | `	return (ph7_int64)dwNew;` |
|    1 | 1221 | `}` |
|    - | 1222 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|    - | 1223 | `static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|    1 | 1224 | `{` |
|    1 | 1225 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1226 | `	LONG HighOfft;` |
|    - | 1227 | `	DWORD dwNew;` |
|    - | 1228 | `	BOOL rc;` |
|    1 | 1229 | `	HighOfft = (LONG)(nOfft >> 32);` |
|    1 | 1230 | `	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);` |
|    1 | 1231 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1232 | `		return -1;` |
|    - | 1233 | `	}` |
|    1 | 1234 | `	rc = SetEndOfFile(pHandle);` |
|    1 | 1235 | `	return rc ? PH7_OK : -1;` |
|    1 | 1236 | `}` |
|    - | 1237 | `/* int (*xSync)(void *); */` |
|    - | 1238 | `static int WinFile_Sync(void *pUserData)` |
|    1 | 1239 | `{` |
|    1 | 1240 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1241 | `	BOOL rc;` |
|    1 | 1242 | `	rc = FlushFileBuffers(pHandle);` |
|    1 | 1243 | `	return rc ? PH7_OK : - 1;` |
|    1 | 1244 | `}` |
|    - | 1245 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|    - | 1246 | `static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 1247 | `{` |
|    - | 1248 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    1 | 1249 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1250 | `	BOOL rc;` |
|    1 | 1251 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 | 1252 | `	if( !rc ){` |
|  ! 0 | 1253 | `		return -1;` |
|    - | 1254 | `	}` |
|    - | 1255 | `	/* dev */` |
|    1 | 1256 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 | 1257 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - | 1258 | `	/* ino */` |
|    1 | 1259 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 | 1260 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - | 1261 | `	/* mode */` |
|    1 | 1262 | `	ph7_value_int(pWorker,0);` |
|    1 | 1263 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - | 1264 | `	/* nlink */` |
|    1 | 1265 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 | 1266 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - | 1267 | `	/* uid,gid,rdev */` |
|    1 | 1268 | `	ph7_value_int(pWorker,0);` |
|    1 | 1269 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 | 1270 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 | 1271 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - | 1272 | `	/* size */` |
|    1 | 1273 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 | 1274 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - | 1275 | `	/* atime */` |
|    1 | 1276 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 | 1277 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - | 1278 | `	/* mtime */` |
|    1 | 1279 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 | 1280 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - | 1281 | `	/* ctime */` |
|    1 | 1282 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 | 1283 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - | 1284 | `	/* blksize,blocks */` |
|    1 | 1285 | `	ph7_value_int(pWorker,0);` |
|    1 | 1286 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 | 1287 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 | 1288 | `	return PH7_OK;` |
|    1 | 1289 | `}` |
|    - | 1290 | `/* Export the file:// stream */` |
|    - | 1291 | `PH7_PRIVATE const ph7_io_stream sWinFileStream = {` |
|    - | 1292 | `	"file", /* Stream name */` |
|    - | 1293 | `	PH7_IO_STREAM_VERSION,` |
|    - | 1294 | `	WinFile_Open,  /* xOpen */` |
|    - | 1295 | `	WinDir_Open,   /* xOpenDir */` |
|    - | 1296 | `	WinFile_Close, /* xClose */` |
|    - | 1297 | `	WinDir_Close,  /* xCloseDir */` |
|    - | 1298 | `	WinFile_Read,  /* xRead */` |
|    - | 1299 | `	WinDir_Read,   /* xReadDir */` |
|    - | 1300 | `	WinFile_Write, /* xWrite */` |
|    - | 1301 | `	WinFile_Seek,  /* xSeek */` |
|    - | 1302 | `	WinFile_Lock,  /* xLock */` |
|    - | 1303 | `	WinDir_RewindDir, /* xRewindDir */` |
|    - | 1304 | `	WinFile_Tell,  /* xTell */` |
|    - | 1305 | `	WinFile_Trunc, /* xTrunc */` |
|    - | 1306 | `	WinFile_Sync,  /* xSeek */` |
|    - | 1307 | `	WinFile_Stat   /* xStat */` |
|    - | 1308 | `};` |
|    - | 1309 | `#endif /* __WINNT__ */` |
|    - | 1310 |  |
