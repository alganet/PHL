# src/ph7/vfs_win.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 602/717 lines (83.96%)

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
|    2 |  241 | `	HeapFree(GetProcessHeap(),0,pPath);` |
|    2 |  242 | `	if( !n ){` |
|  ! 0 |  243 | `		return -1;` |
|    - |  244 | `	}` |
|    2 |  245 | `	zReal = unicodeToUtf8(zTemp);` |
|    2 |  246 | `	if( zReal == 0 ){` |
|  ! 0 |  247 | `		return -1;` |
|    - |  248 | `	}` |
|    2 |  249 | `	ph7_result_string(pCtx,zReal,-1); /* Will make it's own copy */` |
|    2 |  250 | `	HeapFree(GetProcessHeap(),0,zReal);` |
|    2 |  251 | `	return PH7_OK;` |
|    2 |  252 | `}` |
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
|    2 |  275 | `{` |
|    - |  276 | `	void * pConverted;` |
|    - |  277 | `	int rc;` |
|    2 |  278 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  279 | `	if( pConverted == 0 ){` |
|  ! 0 |  280 | `		return -1;` |
|    - |  281 | `	}` |
|    - |  282 | `	/* Windows honors only the read-only attribute: a set owner-write bit (0200)` |
|    - |  283 | `	 * clears it, otherwise the file is made read-only. This mirrors php, whose` |
|    - |  284 | `	 * chmod() on Windows likewise maps through _wchmod and returns success. */` |
|    2 |  285 | `	rc = _wchmod((const wchar_t *)pConverted,(mode & 0200) ? (_S_IREAD\|_S_IWRITE) : _S_IREAD);` |
|    2 |  286 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  287 | `	return rc == 0 ? PH7_OK : - 1;` |
|    2 |  288 | `}` |
|    - |  289 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - |  290 | `static ph7_int64 WinVfs_DiskFreeSpace(const char *zPath)` |
|    1 |  291 | `{` |
|    - |  292 | `#ifdef _WIN32_WCE` |
|    - |  293 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|    - |  294 | `	SXUNUSED(zPath);` |
|    - |  295 | `	return 0;` |
|    - |  296 | `#else` |
|    - |  297 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|    - |  298 | `	void * pConverted;` |
|    - |  299 | `	WCHAR *p;` |
|    - |  300 | `	BOOL rc;` |
|    1 |  301 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  302 | `	if( pConverted == 0 ){` |
|  ! 0 |  303 | `		return 0;` |
|    - |  304 | `	}` |
|    1 |  305 | `	p = (WCHAR *)pConverted;` |
|    1 |  306 | `	for(;*p;p++){` |
|    1 |  307 | `		if( *p == '\\' \|\| *p == '/'){` |
|    1 |  308 | `			*p = '\0';` |
|    1 |  309 | `			break;` |
|    - |  310 | `		}` |
|    1 |  311 | `	}` |
|    1 |  312 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|    1 |  313 | `	if( !rc ){` |
|  ! 0 |  314 | `		return 0;` |
|    - |  315 | `	}` |
|    1 |  316 | `	return (ph7_int64)dwFreeClusters * dwSectPerClust * dwBytesPerSect;` |
|    - |  317 | `#endif` |
|    1 |  318 | `}` |
|    - |  319 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - |  320 | `static ph7_int64 WinVfs_DiskTotalSpace(const char *zPath)` |
|    1 |  321 | `{` |
|    - |  322 | `#ifdef _WIN32_WCE` |
|    - |  323 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|    - |  324 | `	SXUNUSED(zPath);` |
|    - |  325 | `	return 0;` |
|    - |  326 | `#else` |
|    - |  327 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|    - |  328 | `	void * pConverted;` |
|    - |  329 | `	WCHAR *p;` |
|    - |  330 | `	BOOL rc;` |
|    1 |  331 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  332 | `	if( pConverted == 0 ){` |
|  ! 0 |  333 | `		return 0;` |
|    - |  334 | `	}` |
|    1 |  335 | `	p = (WCHAR *)pConverted;` |
|    1 |  336 | `	for(;*p;p++){` |
|    1 |  337 | `		if( *p == '\\' \|\| *p == '/'){` |
|    1 |  338 | `			*p = '\0';` |
|    1 |  339 | `			break;` |
|    - |  340 | `		}` |
|    1 |  341 | `	}` |
|    1 |  342 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|    1 |  343 | `	if( !rc ){` |
|  ! 0 |  344 | `		return 0;` |
|    - |  345 | `	}` |
|    1 |  346 | `	return (ph7_int64)dwTotalClusters * dwSectPerClust * dwBytesPerSect;` |
|    - |  347 | `#endif` |
|    1 |  348 | `}` |
|    - |  349 | `/* int (*xFileExists)(const char *) */` |
|    - |  350 | `static int WinVfs_FileExists(const char *zPath)` |
|    5 |  351 | `{` |
|    5 |  352 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  353 | `	void * pConverted;` |
|    - |  354 | `	DWORD dwAttr;` |
|    5 |  355 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  356 | `	if( pConverted == 0 ){` |
|  ! 0 |  357 | `		return -1;` |
|    - |  358 | `	}` |
|    5 |  359 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  360 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  361 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    4 |  362 | `		return -1;` |
|    - |  363 | `	}` |
|    5 |  364 | `	return PH7_OK;` |
|    5 |  365 | `}` |
|    - |  366 | `/* Open a path for a STAT-family read. Same as OpenReadOnly but with` |
|    - |  367 | ` * FILE_FLAG_BACKUP_SEMANTICS, which is the only way CreateFileW will open a` |
|    - |  368 | ` * DIRECTORY — without it filemtime()/fileatime()/filectime() answered -1 for every` |
|    - |  369 | ` * directory on Windows (a plain read-only open fails with ERROR_ACCESS_DENIED),` |
|    - |  370 | ` * where php reports the timestamp. FILE_READ_ATTRIBUTES is all` |
|    - |  371 | ` * GetFileInformationByHandle needs, so this also works on a file another process` |
|    - |  372 | ` * holds open for writing. */` |
|    - |  373 | `static HANDLE OpenForStat(LPCWSTR pPath)` |
|    1 |  374 | `{` |
|    - |  375 | `	HANDLE pHandle;` |
|    1 |  376 | `	pHandle = CreateFileW(pPath,FILE_READ_ATTRIBUTES,` |
|    - |  377 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_EXISTING,` |
|    - |  378 | `		FILE_ATTRIBUTE_NORMAL\|FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    1 |  379 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 |  380 | `		return 0;` |
|    - |  381 | `	}` |
|    1 |  382 | `	return pHandle;` |
|    1 |  383 | `}` |
|    - |  384 | `/* Open a file in a read-only mode */` |
|    - |  385 | `static HANDLE OpenReadOnly(LPCWSTR pPath)` |
|    5 |  386 | `{` |
|    5 |  387 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    5 |  388 | `	DWORD dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    5 |  389 | `	DWORD dwAccess = GENERIC_READ;` |
|    5 |  390 | `	DWORD dwCreate = OPEN_EXISTING;` |
|    - |  391 | `	HANDLE pHandle;` |
|    5 |  392 | `	pHandle = CreateFileW(pPath,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    5 |  393 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|  ! 0 |  394 | `		return 0;` |
|    - |  395 | `	}` |
|    5 |  396 | `	return pHandle;` |
|    5 |  397 | `}` |
|    - |  398 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  399 | `static ph7_int64 WinVfs_FileSize(const char *zPath)` |
|    1 |  400 | `{` |
|    1 |  401 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  402 | `	DWORD dwLow,dwHigh;` |
|    - |  403 | `	void * pConverted;` |
|    - |  404 | `	ph7_int64 nSize;` |
|    - |  405 | `	HANDLE pHandle;` |
|    - |  406 |  |
|    1 |  407 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  408 | `	if( pConverted == 0 ){` |
|  ! 0 |  409 | `		return -1;` |
|    - |  410 | `	}` |
|    - |  411 | `	/* Open the file in read-only mode */` |
|    1 |  412 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  413 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  414 | `	if( pHandle ){` |
|    1 |  415 | `		dwLow = GetFileSize(pHandle,&dwHigh);` |
|    1 |  416 | `		nSize = dwHigh;` |
|    1 |  417 | `		nSize <<= 32;` |
|    1 |  418 | `		nSize += dwLow;` |
|    1 |  419 | `		CloseHandle(pHandle);` |
|    1 |  420 | `	}else{` |
|  ! 0 |  421 | `		nSize = -1;` |
|    - |  422 | `	}` |
|    1 |  423 | `	return nSize;` |
|    1 |  424 | `}` |
|    - |  425 | `#define TICKS_PER_SECOND 10000000` |
|    - |  426 | `#define EPOCH_DIFFERENCE 11644473600LL` |
|    - |  427 | `/* Convert Windows timestamp to UNIX timestamp */` |
|    - |  428 | `static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)` |
|    1 |  429 | `{` |
|    - |  430 | `    ph7_int64 input,temp;` |
|    1 |  431 | `	input = pTime->dwHighDateTime;` |
|    1 |  432 | `	input <<= 32;` |
|    1 |  433 | `	input += pTime->dwLowDateTime;` |
|    1 |  434 | `    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/` |
|    1 |  435 | `    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/` |
|    1 |  436 | `    return temp;` |
|    1 |  437 | `}` |
|    - |  438 | `/* Convert UNIX timestamp to Windows timestamp */` |
|    - |  439 | `static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)` |
|    1 |  440 | `{` |
|    - |  441 | ``  /* The converted value has to be the one that is STORED: this computed `result` and`` |
|    - |  442 | `   * then wrote the raw unix seconds into the FILETIME, so a requested 1000000000` |
|    - |  443 | `   * landed as 100 seconds past the 1601 epoch and read back as -11644473500. */` |
|    1 |  444 | `  ph7_int64 result = EPOCH_DIFFERENCE;` |
|    1 |  445 | `  result += nUnixtime;` |
|    1 |  446 | `  result *= TICKS_PER_SECOND;` |
|    1 |  447 | `  pOut->dwHighDateTime = (DWORD)((sxu64)result >> 32);` |
|    1 |  448 | `  pOut->dwLowDateTime = (DWORD)((sxu64)result & 0xFFFFFFFFu);` |
|    1 |  449 | `}` |
|    - |  450 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  451 | `static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|    1 |  452 | `{` |
|    - |  453 | `	FILETIME sTouch,sAccess;` |
|    - |  454 | `	void *pConverted;` |
|    - |  455 | `	void *pHandle;` |
|    1 |  456 | `	BOOL rc = 0;` |
|    - |  457 | `	/* Accept file:// like every other path-taking entry in this VFS (the POSIX` |
|    - |  458 | `	 * driver already does) — this was the only one that skipped the mapping. */` |
|    1 |  459 | `	zPath = WinVfsLocalPath(zPath);` |
|    1 |  460 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  461 | `	if( pConverted == 0 ){` |
|  ! 0 |  462 | `		return -1;` |
|    - |  463 | `	}` |
|    - |  464 | `	/* php's touch() CREATES a missing file (OPEN_ALWAYS), and SetFileTime needs write` |
|    - |  465 | `	 * access — the read-only handle this used could not stamp an existing file either.` |
|    - |  466 | `	 * FILE_FLAG_BACKUP_SEMANTICS is what lets a DIRECTORY be opened at all (php's` |
|    - |  467 | `	 * win32 utime passes it for the same reason, and the POSIX driver's utime() works` |
|    - |  468 | `	 * on directories); it does not change what OPEN_ALWAYS creates for a missing path.` |
|    - |  469 | `	 * Mirrors the POSIX driver's utime + open(O_CREAT) fallback. */` |
|    1 |  470 | `	pHandle = CreateFileW((LPCWSTR)pConverted,FILE_WRITE_ATTRIBUTES,` |
|    - |  471 | `		FILE_SHARE_READ\|FILE_SHARE_WRITE\|FILE_SHARE_DELETE,0,OPEN_ALWAYS,` |
|    - |  472 | `		FILE_ATTRIBUTE_NORMAL\|FILE_FLAG_BACKUP_SEMANTICS,0);` |
|    1 |  473 | `	if( pHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 |  474 | `		pHandle = 0;` |
|    - |  475 | `	}` |
|    1 |  476 | `	if( pHandle ){` |
|    - |  477 | `		/* Both stamps are real values: the builtin resolves php's "now" default, so a` |
|    - |  478 | `		 * NEGATIVE timestamp (legal to php) is no longer read as "not given". */` |
|    1 |  479 | `		convertUnixTimeToWindowsTime(touch_time,&sTouch);` |
|    1 |  480 | `		convertUnixTimeToWindowsTime(access_time,&sAccess);` |
|    - |  481 | `		/* SetFileTime(hFile, creation, lastAccess, lastWrite): the modification stamp` |
|    - |  482 | `		 * belongs in the LAST slot. It used to be passed as the CREATION time with` |
|    - |  483 | `		 * lastWrite left NULL, so touch($f, $mtime) changed a stamp nothing reads and` |
|    - |  484 | `		 * left filemtime() reporting whatever the file already had. Creation stays` |
|    - |  485 | `		 * untouched, like php. */` |
|    1 |  486 | `		rc = SetFileTime(pHandle,0,&sAccess,&sTouch);` |
|    - |  487 | `		/* Close the handle */` |
|    1 |  488 | `		CloseHandle(pHandle);` |
|    - |  489 | `	}` |
|    1 |  490 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  491 | `	return rc ? PH7_OK : -1;` |
|    1 |  492 | `}` |
|    - |  493 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  494 | `static ph7_int64 WinVfs_FileAtime(const char *zPath)` |
|    1 |  495 | `{` |
|    1 |  496 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  497 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  498 | `	void * pConverted;` |
|    - |  499 | `	ph7_int64 atime;` |
|    - |  500 | `	HANDLE pHandle;` |
|    1 |  501 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  502 | `	if( pConverted == 0 ){` |
|  ! 0 |  503 | `		return -1;` |
|    - |  504 | `	}` |
|    - |  505 | `	/* Open for a stat read (directories included) */` |
|    1 |  506 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    1 |  507 | `	if( pHandle ){` |
|    - |  508 | `		BOOL rc;` |
|    1 |  509 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  510 | `		if( rc ){` |
|    1 |  511 | `			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);` |
|    1 |  512 | `		}else{` |
|  ! 0 |  513 | `			atime = -1;` |
|    - |  514 | `		}` |
|    1 |  515 | `		CloseHandle(pHandle);` |
|    1 |  516 | `	}else{` |
|  ! 0 |  517 | `		atime = -1;` |
|    - |  518 | `	}` |
|    1 |  519 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  520 | `	return atime;` |
|    1 |  521 | `}` |
|    - |  522 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  523 | `static ph7_int64 WinVfs_FileMtime(const char *zPath)` |
|    1 |  524 | `{` |
|    1 |  525 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  526 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  527 | `	void * pConverted;` |
|    - |  528 | `	ph7_int64 mtime;` |
|    - |  529 | `	HANDLE pHandle;` |
|    1 |  530 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  531 | `	if( pConverted == 0 ){` |
|  ! 0 |  532 | `		return -1;` |
|    - |  533 | `	}` |
|    - |  534 | `	/* Open for a stat read (directories included) */` |
|    1 |  535 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    1 |  536 | `	if( pHandle ){` |
|    - |  537 | `		BOOL rc;` |
|    1 |  538 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  539 | `		if( rc ){` |
|    1 |  540 | `			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);` |
|    1 |  541 | `		}else{` |
|  ! 0 |  542 | `			mtime = -1;` |
|    - |  543 | `		}` |
|    1 |  544 | `		CloseHandle(pHandle);` |
|    1 |  545 | `	}else{` |
|  ! 0 |  546 | `		mtime = -1;` |
|    - |  547 | `	}` |
|    1 |  548 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  549 | `	return mtime;` |
|    1 |  550 | `}` |
|    - |  551 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  552 | `static ph7_int64 WinVfs_FileCtime(const char *zPath)` |
|    1 |  553 | `{` |
|    1 |  554 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  555 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  556 | `	void * pConverted;` |
|    - |  557 | `	ph7_int64 ctime;` |
|    - |  558 | `	HANDLE pHandle;` |
|    1 |  559 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  560 | `	if( pConverted == 0 ){` |
|  ! 0 |  561 | `		return -1;` |
|    - |  562 | `	}` |
|    - |  563 | `	/* Open for a stat read (directories included) */` |
|    1 |  564 | `	pHandle = OpenForStat((LPCWSTR)pConverted);` |
|    1 |  565 | `	if( pHandle ){` |
|    - |  566 | `		BOOL rc;` |
|    1 |  567 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  568 | `		if( rc ){` |
|    1 |  569 | `			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);` |
|    1 |  570 | `		}else{` |
|  ! 0 |  571 | `			ctime = -1;` |
|    - |  572 | `		}` |
|    1 |  573 | `		CloseHandle(pHandle);` |
|    1 |  574 | `	}else{` |
|  ! 0 |  575 | `		ctime = -1;` |
|    - |  576 | `	}` |
|    1 |  577 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  578 | `	return ctime;` |
|    1 |  579 | `}` |
|    - |  580 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  581 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  582 | `static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|    1 |  583 | `{` |
|    1 |  584 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  585 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  586 | `	void *pConverted;` |
|    - |  587 | `	HANDLE pHandle;` |
|    - |  588 | `	BOOL rc;` |
|    1 |  589 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  590 | `	if( pConverted == 0 ){` |
|  ! 0 |  591 | `		return -1;` |
|    - |  592 | `	}` |
|    - |  593 | `	/* Open the file in read-only mode */` |
|    1 |  594 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  595 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  596 | `	if( pHandle == 0 ){` |
|  ! 0 |  597 | `		return -1;` |
|    - |  598 | `	}` |
|    1 |  599 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  600 | `	CloseHandle(pHandle);` |
|    1 |  601 | `	if( !rc ){` |
|  ! 0 |  602 | `		return -1;` |
|    - |  603 | `	}` |
|    - |  604 | `	/* dev */` |
|    1 |  605 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 |  606 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - |  607 | `	/* ino */` |
|    1 |  608 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 |  609 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - |  610 | `	/* mode */` |
|    1 |  611 | `	ph7_value_int(pWorker,0);` |
|    1 |  612 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - |  613 | `	/* nlink */` |
|    1 |  614 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 |  615 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - |  616 | `	/* uid,gid,rdev */` |
|    1 |  617 | `	ph7_value_int(pWorker,0);` |
|    1 |  618 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 |  619 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 |  620 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - |  621 | `	/* size */` |
|    1 |  622 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 |  623 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - |  624 | `	/* atime */` |
|    1 |  625 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 |  626 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - |  627 | `	/* mtime */` |
|    1 |  628 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 |  629 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - |  630 | `	/* ctime */` |
|    1 |  631 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 |  632 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - |  633 | `	/* blksize,blocks */` |
|    1 |  634 | `	ph7_value_int(pWorker,0);` |
|    1 |  635 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 |  636 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 |  637 | `	return PH7_OK;` |
|    1 |  638 | `}` |
|    - |  639 | `/* int (*xIsfile)(const char *) */` |
|    - |  640 | `static int WinVfs_isfile(const char *zPath)` |
|    5 |  641 | `{` |
|    5 |  642 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  643 | `	void * pConverted;` |
|    - |  644 | `	DWORD dwAttr;` |
|    5 |  645 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  646 | `	if( pConverted == 0 ){` |
|  ! 0 |  647 | `		return -1;` |
|    - |  648 | `	}` |
|    5 |  649 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    5 |  650 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  651 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  652 | `		return -1;` |
|    - |  653 | `	}` |
|    5 |  654 | `	return (dwAttr & (FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;` |
|    5 |  655 | `}` |
|    - |  656 | `/* int (*xIslink)(const char *) */` |
|    - |  657 | `static int WinVfs_islink(const char *zPath)` |
|  ! 0 |  658 | `{` |
|  ! 0 |  659 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  660 | `	void * pConverted;` |
|    - |  661 | `	DWORD dwAttr;` |
|  ! 0 |  662 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  663 | `	if( pConverted == 0 ){` |
|  ! 0 |  664 | `		return -1;` |
|    - |  665 | `	}` |
|  ! 0 |  666 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|  ! 0 |  667 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  668 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  669 | `		return -1;` |
|    - |  670 | `	}` |
|  ! 0 |  671 | `	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;` |
|  ! 0 |  672 | `}` |
|    - |  673 | `/* int (*xWritable)(const char *) */` |
|    - |  674 | `static int WinVfs_iswritable(const char *zPath)` |
|    1 |  675 | `{` |
|    1 |  676 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  677 | `	void * pConverted;` |
|    - |  678 | `	DWORD dwAttr;` |
|    1 |  679 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  680 | `	if( pConverted == 0 ){` |
|  ! 0 |  681 | `		return -1;` |
|    - |  682 | `	}` |
|    1 |  683 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  684 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  685 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  686 | `		return -1;` |
|    - |  687 | `	}` |
|    1 |  688 | `	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE\|FILE_ATTRIBUTE_NORMAL)) == 0 ){` |
|    - |  689 | `		/* Not a regular file */` |
|  ! 0 |  690 | `		return -1;` |
|    - |  691 | `	}` |
|    1 |  692 | `	if( dwAttr & FILE_ATTRIBUTE_READONLY ){` |
|    - |  693 | `		/* Read-only file */` |
|  ! 0 |  694 | `		return -1;` |
|    - |  695 | `	}` |
|    - |  696 | `	/* File is writable */` |
|    1 |  697 | `	return PH7_OK;` |
|    1 |  698 | `}` |
|    - |  699 | `/* int (*xExecutable)(const char *) */` |
|    - |  700 | `static int WinVfs_isexecutable(const char *zPath)` |
|    1 |  701 | `{` |
|    1 |  702 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  703 | `	void * pConverted;` |
|    - |  704 | `	DWORD dwAttr;` |
|    1 |  705 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  706 | `	if( pConverted == 0 ){` |
|  ! 0 |  707 | `		return -1;` |
|    - |  708 | `	}` |
|    1 |  709 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  710 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  711 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  712 | `		return -1;` |
|    - |  713 | `	}` |
|    1 |  714 | `	if( (dwAttr & FILE_ATTRIBUTE_NORMAL) == 0 ){` |
|    - |  715 | `		/* Not a regular file */` |
|    1 |  716 | `		return -1;` |
|    - |  717 | `	}` |
|    - |  718 | `	/* File is executable */` |
|  ! 0 |  719 | `	return PH7_OK;` |
|    1 |  720 | `}` |
|    - |  721 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  722 | `static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|    1 |  723 | `{` |
|    1 |  724 | `	zPath = WinVfsLocalPath(zPath);` |
|    - |  725 | `	void * pConverted;` |
|    - |  726 | `	DWORD dwAttr;` |
|    1 |  727 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  728 | `	if( pConverted == 0 ){` |
|    - |  729 | `		/* Expand 'unknown' */` |
|  ! 0 |  730 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  731 | `		return -1;` |
|    - |  732 | `	}` |
|    1 |  733 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  734 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  735 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    - |  736 | `		/* Expand 'unknown' */` |
|  ! 0 |  737 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  738 | `		return -1;` |
|    - |  739 | `	}` |
|    1 |  740 | `	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE) ){` |
|    1 |  741 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|    1 |  742 | `	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){` |
|    1 |  743 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|  ! 0 |  744 | `	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){` |
|  ! 0 |  745 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|  ! 0 |  746 | `	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){` |
|  ! 0 |  747 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|  ! 0 |  748 | `	}else{` |
|  ! 0 |  749 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    - |  750 | `	}` |
|    1 |  751 | `	return PH7_OK;` |
|    1 |  752 | `}` |
|    - |  753 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  754 | `static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|    4 |  755 | `{` |
|    - |  756 | `	char zValue[1024];` |
|    - |  757 | `	DWORD n;` |
|    - |  758 | `	/*` |
|    - |  759 | `	 * According to MSDN` |
|    - |  760 | `	 * If lpBuffer is not large enough to hold the data, the return` |
|    - |  761 | `	 * value is the buffer size, in characters, required to hold the` |
|    - |  762 | `	 * string and its terminating null character and the contents` |
|    - |  763 | `	 * of lpBuffer are undefined.` |
|    - |  764 | `	 */` |
|    4 |  765 | `	n = sizeof(zValue);` |
|    4 |  766 | `	SyMemcpy("Undefined",zValue,sizeof("Undefined")-1);` |
|    - |  767 | `	/* Extract the environment value */` |
|    4 |  768 | `	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));` |
|    4 |  769 | `	if( !n ){` |
|    - |  770 | `		/* No such variable*/` |
|  ! 0 |  771 | `		return -1;` |
|    - |  772 | `	}` |
|    4 |  773 | `	ph7_result_string(pCtx,zValue,(int)n);` |
|    4 |  774 | `	return PH7_OK;` |
|    4 |  775 | `}` |
|    - |  776 | `/* int (*xSetenv)(const char *,const char *) */` |
|    - |  777 | `static int WinVfs_Setenv(const char *zName,const char *zValue)` |
|    1 |  778 | `{` |
|    - |  779 | `	BOOL rc;` |
|    1 |  780 | `	rc = SetEnvironmentVariableA(zName,zValue);` |
|    1 |  781 | `	return rc ? PH7_OK : -1;` |
|    1 |  782 | `}` |
|    - |  783 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  784 | `static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|    5 |  785 | `{` |
|    - |  786 | `	DWORD dwSizeLow,dwSizeHigh;` |
|    - |  787 | `	HANDLE pHandle,pMapHandle;` |
|    - |  788 | `	void *pConverted,*pView;` |
|    - |  789 |  |
|    5 |  790 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  791 | `	if( pConverted == 0 ){` |
|  ! 0 |  792 | `		return -1;` |
|    - |  793 | `	}` |
|    5 |  794 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    5 |  795 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 |  796 | `	if( pHandle == 0 ){` |
|  ! 0 |  797 | `		return -1;` |
|    - |  798 | `	}` |
|    - |  799 | `	/* Get the file size */` |
|    5 |  800 | `	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);` |
|    - |  801 | `	/* Create the mapping */` |
|    5 |  802 | `	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);` |
|    5 |  803 | `	if( pMapHandle == 0 ){` |
|  ! 0 |  804 | `		CloseHandle(pHandle);` |
|  ! 0 |  805 | `		return -1;` |
|    - |  806 | `	}` |
|    5 |  807 | `	*pSize = ((ph7_int64)dwSizeHigh << 32) \| dwSizeLow;` |
|    - |  808 | `	/* Obtain the view */` |
|    5 |  809 | `	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));` |
|    5 |  810 | `	if( pView ){` |
|    - |  811 | `		/* Let the upper layer point to the view */` |
|    5 |  812 | `		*ppMap = pView;` |
|    - |  813 | `	}` |
|    - |  814 | `	/* Close the handle` |
|    - |  815 | `	 * According to MSDN it's OK the close the HANDLES.` |
|    - |  816 | `	 */` |
|    5 |  817 | `	CloseHandle(pMapHandle);` |
|    5 |  818 | `	CloseHandle(pHandle);` |
|    5 |  819 | `	return pView ? PH7_OK : -1;` |
|    5 |  820 | `}` |
|    - |  821 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|    - |  822 | `static void WinVfs_Unmap(void *pView,ph7_int64 nSize)` |
|    5 |  823 | `{` |
|    5 |  824 | `	nSize = 0; /* Compiler warning */` |
|    5 |  825 | `	UnmapViewOfFile(pView);` |
|    5 |  826 | `}` |
|    - |  827 | `/* void (*xTempDir)(ph7_context *) */` |
|    - |  828 | `static void WinVfs_TempDir(ph7_context *pCtx)` |
|    4 |  829 | `{` |
|    - |  830 | `	CHAR zTemp[1024];` |
|    - |  831 | `	DWORD n;` |
|    4 |  832 | `	n = GetTempPathA(sizeof(zTemp),zTemp);` |
|    4 |  833 | `	if( n < 1 ){` |
|    - |  834 | `		/* Assume the default windows temp directory */` |
|  ! 0 |  835 | `		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);` |
|  ! 0 |  836 | `	}else{` |
|    4 |  837 | `		ph7_result_string(pCtx,zTemp,(int)n);` |
|    - |  838 | `	}` |
|    4 |  839 | `}` |
|    - |  840 | `/* unsigned int (*xProcessId)(void) */` |
|    - |  841 | `static unsigned int WinVfs_ProcessId(void)` |
|    3 |  842 | `{` |
|    3 |  843 | `	DWORD nID = 0;` |
|    - |  844 | `#ifndef __MINGW32__` |
|    3 |  845 | `	nID = GetProcessId(GetCurrentProcess());` |
|    - |  846 | `#endif /* __MINGW32__ */` |
|    3 |  847 | `	return (unsigned int)nID;` |
|    3 |  848 | `}` |
|    - |  849 | `/* void (*xUsername)(ph7_context *) */` |
|    - |  850 | `static void WinVfs_Username(ph7_context *pCtx)` |
|    1 |  851 | `{` |
|    - |  852 | `	WCHAR zUser[1024];` |
|    - |  853 | `	DWORD nByte;` |
|    - |  854 | `	BOOL rc;` |
|    1 |  855 | `	nByte = sizeof(zUser);` |
|    1 |  856 | `	rc = GetUserNameW(zUser,&nByte);` |
|    1 |  857 | `	if( !rc ){` |
|    - |  858 | `		/* Set a dummy name */` |
|  ! 0 |  859 | `		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  860 | `	}else{` |
|    - |  861 | `		char *zName;` |
|    1 |  862 | `		zName = unicodeToUtf8(zUser);` |
|    1 |  863 | `		if( zName == 0 ){` |
|  ! 0 |  864 | `			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  865 | `		}else{` |
|    1 |  866 | `			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    1 |  867 | `			HeapFree(GetProcessHeap(),0,zName);` |
|    - |  868 | `		}` |
|    - |  869 | `	}` |
|    - |  870 |  |
|    1 |  871 | `}` |
|    - |  872 | `/* int (*xChroot)(const char *) — Windows has no chroot; fail cleanly so chroot()` |
|    - |  873 | ` * returns false. (php has no chroot symbol at all; PHL exposes it as an extension` |
|    - |  874 | ` * and this reports the failure without a "not implemented in the VFS" warning.) */` |
|    - |  875 | `static int WinVfs_chroot(const char *zPath)` |
|    1 |  876 | `{` |
|    - |  877 | `	(void)zPath;` |
|    1 |  878 | `	return -1;` |
|    1 |  879 | `}` |
|    - |  880 | `#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE` |
|    - |  881 | `#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2` |
|    - |  882 | `#endif` |
|    - |  883 | `#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY` |
|    - |  884 | `#define SYMBOLIC_LINK_FLAG_DIRECTORY 0x1` |
|    - |  885 | `#endif` |
|    - |  886 | `/* int (*xLink)(const char *,const char *,int) — hard link (iSym==0) via` |
|    - |  887 | ` * CreateHardLink or symbolic link (iSym!=0) via CreateSymbolicLink, mirroring` |
|    - |  888 | ` * php on Windows. Developer-mode symlink creation is allowed. */` |
|    - |  889 | `static int WinVfs_Link(const char *zOld,const char *zNew,int iSym)` |
|    1 |  890 | `{` |
|    - |  891 | `	void *pOld, *pNew;` |
|    - |  892 | `	BOOL rc;` |
|    1 |  893 | `	pOld = convertUtf8Filename(zOld);` |
|    1 |  894 | `	if( pOld == 0 ){` |
|  ! 0 |  895 | `		return -1;` |
|    - |  896 | `	}` |
|    1 |  897 | `	pNew = convertUtf8Filename(zNew);` |
|    1 |  898 | `	if( pNew == 0 ){` |
|  ! 0 |  899 | `		HeapFree(GetProcessHeap(),0,pOld);` |
|  ! 0 |  900 | `		return -1;` |
|    - |  901 | `	}` |
|    1 |  902 | `	if( iSym ){` |
|    1 |  903 | `		DWORD dwFlags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;` |
|    1 |  904 | `		DWORD attr = GetFileAttributesW((LPCWSTR)pOld);` |
|    1 |  905 | `		if( attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) ){` |
|  ! 0 |  906 | `			dwFlags \|= SYMBOLIC_LINK_FLAG_DIRECTORY;` |
|    - |  907 | `		}` |
|    1 |  908 | `		rc = CreateSymbolicLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,dwFlags) ? TRUE : FALSE;` |
|    1 |  909 | `	}else{` |
|    1 |  910 | `		rc = CreateHardLinkW((LPCWSTR)pNew,(LPCWSTR)pOld,0);` |
|    - |  911 | `	}` |
|    1 |  912 | `	HeapFree(GetProcessHeap(),0,pNew);` |
|    1 |  913 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|    1 |  914 | `	return rc ? PH7_OK : -1;` |
|    1 |  915 | `}` |
|    - |  916 | `/* int (*xUmask)(int) — Windows has no umask; php's umask() returns 0 there. */` |
|    - |  917 | `static int WinVfs_Umask(int iMask)` |
|    1 |  918 | `{` |
|    - |  919 | `	(void)iMask;` |
|    1 |  920 | `	return 0;` |
|    1 |  921 | `}` |
|    - |  922 | `/* int (*xUid)(void) / int (*xGid)(void) — no uid/gid on Windows; php's` |
|    - |  923 | ` * getmyuid()/getmygid() both return 0. */` |
|    - |  924 | `static int WinVfs_Uid(void)` |
|    1 |  925 | `{` |
|    1 |  926 | `	return 0;` |
|    1 |  927 | `}` |
|    - |  928 | `static int WinVfs_Gid(void)` |
|    1 |  929 | `{` |
|    1 |  930 | `	return 0;` |
|    1 |  931 | `}` |
|    - |  932 | `/* Export the windows vfs */` |
|    - |  933 | `PH7_PRIVATE const ph7_vfs sWinVfs = {` |
|    - |  934 | `	"Windows_vfs",` |
|    - |  935 | `	PH7_VFS_VERSION,` |
|    - |  936 | `	WinVfs_chdir,    /* int (*xChdir)(const char *) */` |
|    - |  937 | `	WinVfs_chroot,   /* int (*xChroot)(const char *); */` |
|    - |  938 | `	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|    - |  939 | `	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|    - |  940 | `	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|    - |  941 | `	WinVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|    - |  942 | `	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|    - |  943 | `	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|    - |  944 | `	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */` |
|    - |  945 | `	WinVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|    - |  946 | `	WinVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|    - |  947 | `	WinVfs_chmod, /*int (*xChmod)(const char *,int)*/` |
|    - |  948 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|    - |  949 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|    - |  950 | `	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - |  951 | `	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - |  952 | `	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  953 | `	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  954 | `	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  955 | `	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  956 | `	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  957 | `	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  958 | `	WinVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|    - |  959 | `	WinVfs_islink,     /* int (*xIslink)(const char *) */` |
|    - |  960 | `	WinVfs_isfile,     /* int (*xReadable)(const char *) */` |
|    - |  961 | `	WinVfs_iswritable, /* int (*xWritable)(const char *) */` |
|    - |  962 | `	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */` |
|    - |  963 | `	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  964 | `	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  965 | `	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|    - |  966 | `	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  967 | `	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  968 | `	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|    - |  969 | `	WinVfs_Link,       /* int (*xLink)(const char *,const char *,int) */` |
|    - |  970 | `	WinVfs_Umask,      /* int (*xUmask)(int) */` |
|    - |  971 | `	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|    - |  972 | `	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|    - |  973 | `	WinVfs_Uid, /* int (*xUid)(void) */` |
|    - |  974 | `	WinVfs_Gid, /* int (*xGid)(void) */` |
|    - |  975 | `	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|    - |  976 | `	0 /* int (*xExec)(const char *,ph7_context *) */` |
|    - |  977 | `};` |
|    - |  978 | `/* Windows file IO */` |
|    - |  979 | `#ifndef INVALID_SET_FILE_POINTER` |
|    - |  980 | `# define INVALID_SET_FILE_POINTER ((DWORD)-1)` |
|    - |  981 | `#endif` |
|    - |  982 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|    - |  983 | `static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|    5 |  984 | `{` |
|    5 |  985 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    5 |  986 | `	DWORD dwAccess = GENERIC_READ;` |
|    - |  987 | `	DWORD dwShare,dwCreate;` |
|    - |  988 | `	void *pConverted;` |
|    - |  989 | `	HANDLE pHandle;` |
|    - |  990 |  |
|    5 |  991 | `	pConverted = convertUtf8Filename(zPath);` |
|    5 |  992 | `	if( pConverted == 0 ){` |
|  ! 0 |  993 | `		return -1;` |
|    - |  994 | `	}` |
|    - |  995 | `	/* Set the desired flags according to the open mode */` |
|    5 |  996 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|    - |  997 | `		/* Open existing file, or create if it doesn't exist */` |
|    5 |  998 | `		dwCreate = OPEN_ALWAYS;` |
|    5 |  999 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - | 1000 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|    5 | 1001 | `			dwCreate = CREATE_ALWAYS;` |
|    5 | 1002 | `		}` |
|    5 | 1003 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|    - | 1004 | `		/* Creates a new file, only if it does not already exist.` |
|    - | 1005 | `		* If the file exists, it fails.` |
|    - | 1006 | `		*/` |
|    2 | 1007 | `		dwCreate = CREATE_NEW;` |
|    5 | 1008 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - | 1009 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|    - | 1010 | `		 * The file must exist.` |
|    - | 1011 | `		 */` |
|  ! 0 | 1012 | `		dwCreate = TRUNCATE_EXISTING;` |
|  ! 0 | 1013 | `	}else{` |
|    - | 1014 | `		/* Opens a file, only if it exists. */` |
|    5 | 1015 | `		dwCreate = OPEN_EXISTING;` |
|    - | 1016 | `	}` |
|    5 | 1017 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|    - | 1018 | `		/* Read+Write access */` |
|    5 | 1019 | `		dwAccess \|= GENERIC_WRITE;` |
|    5 | 1020 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|    - | 1021 | `		/* Write only access */` |
|    2 | 1022 | `		dwAccess = GENERIC_WRITE;` |
|    - | 1023 | `	}` |
|    5 | 1024 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|    - | 1025 | `		/* Append mode */` |
|  ! 0 | 1026 | `		dwAccess = FILE_APPEND_DATA;` |
|    - | 1027 | `	}` |
|    5 | 1028 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|    - | 1029 | `		/* File is temporary */` |
|  ! 0 | 1030 | `		dwType = FILE_ATTRIBUTE_TEMPORARY;` |
|    - | 1031 | `	}` |
|    5 | 1032 | `	dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    5 | 1033 | `	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    5 | 1034 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    5 | 1035 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    - | 1036 | `		SXUNUSED(pResource); /* MSVC warning */` |
|    4 | 1037 | `		return -1;` |
|    - | 1038 | `	}` |
|    - | 1039 | `	/* Make the handle accessible to the upper layer */` |
|    5 | 1040 | `	*ppHandle = (void *)pHandle;` |
|    5 | 1041 | `	return PH7_OK;` |
|    5 | 1042 | `}` |
|    - | 1043 | `/* An instance of the following structure is used to record state information` |
|    - | 1044 | ` * while iterating throw directory entries.` |
|    - | 1045 | ` */` |
|    - | 1046 | `typedef struct WinDir_Info WinDir_Info;` |
|    - | 1047 | `struct WinDir_Info` |
|    - | 1048 | `{` |
|    - | 1049 | `	HANDLE pDirHandle;` |
|    - | 1050 | `	void *pPath;` |
|    - | 1051 | `	WIN32_FIND_DATAW sInfo;` |
|    - | 1052 | `	int rc;` |
|    - | 1053 | `};` |
|    - | 1054 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|    - | 1055 | `static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|    5 | 1056 | `{` |
|    - | 1057 | `	WinDir_Info *pDirInfo;` |
|    - | 1058 | `	void *pConverted;` |
|    - | 1059 | `	char *zPrep;` |
|    - | 1060 | `	sxu32 n;` |
|    - | 1061 | `	/* Prepare the path */` |
|    5 | 1062 | `	n = SyStrlen(zPath);` |
|    5 | 1063 | `	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);` |
|    5 | 1064 | `	if( zPrep == 0 ){` |
|  ! 0 | 1065 | `		return -1;` |
|    - | 1066 | `	}` |
|    5 | 1067 | `	SyMemcpy((const void *)zPath,zPrep,n);` |
|    5 | 1068 | `	zPrep[n]   = '\\';` |
|    5 | 1069 | `	zPrep[n+1] =  '*';` |
|    5 | 1070 | `	zPrep[n+2] = 0;` |
|    5 | 1071 | `	pConverted = convertUtf8Filename(zPrep);` |
|    5 | 1072 | `	HeapFree(GetProcessHeap(),0,zPrep);` |
|    5 | 1073 | `	if( pConverted == 0 ){` |
|  ! 0 | 1074 | `		return -1;` |
|    - | 1075 | `	}` |
|    - | 1076 | `	/* Allocate a new instance */` |
|    5 | 1077 | `	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));` |
|    5 | 1078 | `	if( pDirInfo == 0 ){` |
|  ! 0 | 1079 | `		pResource = 0; /* Compiler warning */` |
|  ! 0 | 1080 | `		return -1;` |
|    - | 1081 | `	}` |
|    5 | 1082 | `	pDirInfo->rc = SXRET_OK;` |
|    5 | 1083 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);` |
|    5 | 1084 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|    - | 1085 | `		/* Cannot open directory */` |
|  ! 0 | 1086 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 | 1087 | `		HeapFree(GetProcessHeap(),0,pDirInfo);` |
|  ! 0 | 1088 | `		return -1;` |
|    - | 1089 | `	}` |
|    - | 1090 | `	/* Save the path */` |
|    5 | 1091 | `	pDirInfo->pPath = pConverted;` |
|    - | 1092 | `	/* Save our structure */` |
|    5 | 1093 | `	*ppHandle = pDirInfo;` |
|    5 | 1094 | `	return PH7_OK;` |
|    5 | 1095 | `}` |
|    - | 1096 | `/* void (*xCloseDir)(void *) */` |
|    - | 1097 | `static void WinDir_Close(void *pUserData)` |
|    5 | 1098 | `{` |
|    5 | 1099 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    5 | 1100 | `	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){` |
|    5 | 1101 | `		FindClose(pDirInfo->pDirHandle);` |
|    - | 1102 | `	}` |
|    5 | 1103 | `	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);` |
|    5 | 1104 | `	HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    5 | 1105 | `}` |
|    - | 1106 | `/* void (*xClose)(void *); */` |
|    - | 1107 | `static void WinFile_Close(void *pUserData)` |
|    5 | 1108 | `{` |
|    5 | 1109 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    5 | 1110 | `	CloseHandle(pHandle);` |
|    5 | 1111 | `}` |
|    - | 1112 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|    - | 1113 | `static int WinDir_Read(void *pUserData,ph7_context *pCtx)` |
|    5 | 1114 | `{` |
|    5 | 1115 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    - | 1116 | `	LPWIN32_FIND_DATAW pData;` |
|    - | 1117 | `	char *zName;` |
|    - | 1118 | `	BOOL rc;` |
|    5 | 1119 | `	if( pDirInfo->rc != SXRET_OK ){` |
|    - | 1120 | `		/* No more entry to process */` |
|    5 | 1121 | `		return -1;` |
|    - | 1122 | `	}` |
|    5 | 1123 | `	pData = &pDirInfo->sInfo;` |
|    - | 1124 | `	/* php parity: readdir()/scandir() include the '.' and '..' entries, so unlike` |
|    - | 1125 | `	 * the historical PH7 behaviour we return them instead of skipping. */` |
|    5 | 1126 | `	zName = unicodeToUtf8(pData->cFileName);` |
|    5 | 1127 | `	if( zName == 0 ){` |
|    - | 1128 | `		/* Out of memory */` |
|  ! 0 | 1129 | `		return -1;` |
|    - | 1130 | `	}` |
|    - | 1131 | `	/* Return the current file name */` |
|    5 | 1132 | `	ph7_result_string(pCtx,zName,-1);` |
|    5 | 1133 | `	HeapFree(GetProcessHeap(),0,zName);` |
|    - | 1134 | `	/* Point to the next entry */` |
|    5 | 1135 | `	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|    5 | 1136 | `	if( !rc ){` |
|    5 | 1137 | `		pDirInfo->rc = SXERR_EOF;` |
|    - | 1138 | `	}` |
|    5 | 1139 | `	return PH7_OK;` |
|    5 | 1140 | `}` |
|    - | 1141 | `/* void (*xRewindDir)(void *) */` |
|    - | 1142 | `static void WinDir_RewindDir(void *pUserData)` |
|    1 | 1143 | `{` |
|    1 | 1144 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    1 | 1145 | `	FindClose(pDirInfo->pDirHandle);` |
|    1 | 1146 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);` |
|    1 | 1147 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 | 1148 | `		pDirInfo->rc = SXERR_EOF;` |
|  ! 0 | 1149 | `	}else{` |
|    1 | 1150 | `		pDirInfo->rc = SXRET_OK;` |
|    - | 1151 | `	}` |
|    1 | 1152 | `}` |
|    - | 1153 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|    - | 1154 | `static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)` |
|    5 | 1155 | `{` |
|    5 | 1156 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1157 | `	DWORD nRd;` |
|    - | 1158 | `	BOOL rc;` |
|    5 | 1159 | `	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    5 | 1160 | `	if( !rc ){` |
|    - | 1161 | `		/* EOF or IO error */` |
|  ! 0 | 1162 | `		return -1;` |
|    - | 1163 | `	}` |
|    5 | 1164 | `	return (ph7_int64)nRd;` |
|    5 | 1165 | `}` |
|    - | 1166 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|    - | 1167 | `static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)` |
|    5 | 1168 | `{` |
|    5 | 1169 | `	const char *zData = (const char *)pBuffer;` |
|    5 | 1170 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1171 | `	ph7_int64 nCount;` |
|    - | 1172 | `	DWORD nWr;` |
|    - | 1173 | `	BOOL rc;` |
|    5 | 1174 | `	nWr = 0;` |
|    5 | 1175 | `	nCount = 0;` |
|    - | 1176 | `	for(;;){` |
|    5 | 1177 | `		if( nWrite < 1 ){` |
|    5 | 1178 | `			break;` |
|    - | 1179 | `		}` |
|    5 | 1180 | `		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);` |
|    5 | 1181 | `		if( !rc ){` |
|    - | 1182 | `			/* IO error — surface a POSIX errno for the caller's diagnostic` |
|    - | 1183 | `			 * (e.g. a byte-range lock violation reports EACCES like php). */` |
|    1 | 1184 | `			WinVfsMapErrno();` |
|    1 | 1185 | `			break;` |
|    - | 1186 | `		}` |
|    5 | 1187 | `		nWrite -= nWr;` |
|    5 | 1188 | `		nCount += nWr;` |
|    5 | 1189 | `		zData += nWr;` |
|    5 | 1190 | `	}` |
|    5 | 1191 | `	if( nWrite > 0 ){` |
|    1 | 1192 | `		return -1;` |
|    - | 1193 | `	}` |
|    5 | 1194 | `	return nCount;` |
|    5 | 1195 | `}` |
|    - | 1196 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|    - | 1197 | `static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|    1 | 1198 | `{` |
|    1 | 1199 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1200 | `	DWORD dwMove,dwNew;` |
|    - | 1201 | `	LONG nHighOfft;` |
|    1 | 1202 | `	switch(whence){` |
|    - | 1203 | `	case 1:/*SEEK_CUR*/` |
|  ! 0 | 1204 | `		dwMove = FILE_CURRENT;` |
|  ! 0 | 1205 | `		break;` |
|    - | 1206 | `	case 2: /* SEEK_END */` |
|  ! 0 | 1207 | `		dwMove = FILE_END;` |
|  ! 0 | 1208 | `		break;` |
|    - | 1209 | `	case 0: /* SEEK_SET */` |
|    - | 1210 | `	default:` |
|    1 | 1211 | `		dwMove = FILE_BEGIN;` |
|    - | 1212 | `		break;` |
|    - | 1213 | `	}` |
|    1 | 1214 | `	nHighOfft = (LONG)(iOfft >> 32);` |
|    1 | 1215 | `	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);` |
|    1 | 1216 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1217 | `		return -1;` |
|    - | 1218 | `	}` |
|    1 | 1219 | `	return PH7_OK;` |
|    1 | 1220 | `}` |
|    - | 1221 | `/* int (*xLock)(void *,int) — see the lock_type value space in ph7.h */` |
|    - | 1222 | `static int WinFile_Lock(void *pUserData,int lock_type)` |
|    1 | 1223 | `{` |
|    1 | 1224 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1225 | `	OVERLAPPED sDummy;` |
|    - | 1226 | `	BOOL rc;` |
|    1 | 1227 | `	SyZero(&sDummy,sizeof(sDummy));` |
|    - | 1228 | `	/* Lock/unlock the whole file. php locks the maximal byte range, so the lock` |
|    - | 1229 | `	 * is effective even for an empty or freshly-truncated file — the previous` |
|    - | 1230 | `	 * code locked only GetFileSize() bytes (i.e. nothing for a 0-byte file). */` |
|    1 | 1231 | `	if( lock_type < 0 ){` |
|    - | 1232 | `		/* Unlock the file */` |
|    1 | 1233 | `		rc = UnlockFileEx(pHandle,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);` |
|    1 | 1234 | `	}else{` |
|    - | 1235 | `		/* LOCKFILE_FAIL_IMMEDIATELY belongs to the NON-BLOCKING requests only: it` |
|    - | 1236 | `		 * used to be passed unconditionally, so a plain flock($f,LOCK_EX) answered` |
|    - | 1237 | `		 * false under contention on Windows where php (and POSIX) waits. */` |
|    1 | 1238 | `		DWORD dwFlags = 0;` |
|    1 | 1239 | `		if( lock_type == PH7_IO_LOCK_EX \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    1 | 1240 | `			dwFlags \|= LOCKFILE_EXCLUSIVE_LOCK;` |
|    - | 1241 | `		}` |
|    1 | 1242 | `		if( lock_type == PH7_IO_LOCK_SH_NB \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    1 | 1243 | `			dwFlags \|= LOCKFILE_FAIL_IMMEDIATELY;` |
|    - | 1244 | `		}` |
|    1 | 1245 | `		rc = LockFileEx(pHandle,dwFlags,0,0xFFFFFFFF,0xFFFFFFFF,&sDummy);` |
|    1 | 1246 | `		if( !rc && GetLastError() == ERROR_LOCK_VIOLATION ){` |
|    - | 1247 | `			/* Refused because another holder has the range: php's $would_block. */` |
|    1 | 1248 | `			return SXERR_BUSY;` |
|    - | 1249 | `		}` |
|    - | 1250 | `	}` |
|    1 | 1251 | `	return rc ? PH7_OK : -1 /* Lock error */;` |
|    1 | 1252 | `}` |
|    - | 1253 | `/* ph7_int64 (*xTell)(void *) */` |
|    - | 1254 | `static ph7_int64 WinFile_Tell(void *pUserData)` |
|    1 | 1255 | `{` |
|    1 | 1256 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1257 | `	DWORD dwNew;` |
|    1 | 1258 | `	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);` |
|    1 | 1259 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1260 | `		return -1;` |
|    - | 1261 | `	}` |
|    1 | 1262 | `	return (ph7_int64)dwNew;` |
|    1 | 1263 | `}` |
|    - | 1264 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|    - | 1265 | `static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|    1 | 1266 | `{` |
|    1 | 1267 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1268 | `	LONG HighOfft;` |
|    - | 1269 | `	DWORD dwNew;` |
|    - | 1270 | `	BOOL rc;` |
|    1 | 1271 | `	HighOfft = (LONG)(nOfft >> 32);` |
|    1 | 1272 | `	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);` |
|    1 | 1273 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1274 | `		return -1;` |
|    - | 1275 | `	}` |
|    1 | 1276 | `	rc = SetEndOfFile(pHandle);` |
|    1 | 1277 | `	return rc ? PH7_OK : -1;` |
|    1 | 1278 | `}` |
|    - | 1279 | `/* int (*xSync)(void *); */` |
|    - | 1280 | `static int WinFile_Sync(void *pUserData)` |
|    1 | 1281 | `{` |
|    1 | 1282 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1283 | `	BOOL rc;` |
|    1 | 1284 | `	rc = FlushFileBuffers(pHandle);` |
|    1 | 1285 | `	return rc ? PH7_OK : - 1;` |
|    1 | 1286 | `}` |
|    - | 1287 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|    - | 1288 | `static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 1289 | `{` |
|    - | 1290 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    1 | 1291 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1292 | `	BOOL rc;` |
|    1 | 1293 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 | 1294 | `	if( !rc ){` |
|  ! 0 | 1295 | `		return -1;` |
|    - | 1296 | `	}` |
|    - | 1297 | `	/* dev */` |
|    1 | 1298 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 | 1299 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - | 1300 | `	/* ino */` |
|    1 | 1301 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 | 1302 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - | 1303 | `	/* mode */` |
|    1 | 1304 | `	ph7_value_int(pWorker,0);` |
|    1 | 1305 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - | 1306 | `	/* nlink */` |
|    1 | 1307 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 | 1308 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - | 1309 | `	/* uid,gid,rdev */` |
|    1 | 1310 | `	ph7_value_int(pWorker,0);` |
|    1 | 1311 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 | 1312 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 | 1313 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - | 1314 | `	/* size */` |
|    1 | 1315 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 | 1316 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - | 1317 | `	/* atime */` |
|    1 | 1318 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 | 1319 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - | 1320 | `	/* mtime */` |
|    1 | 1321 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 | 1322 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - | 1323 | `	/* ctime */` |
|    1 | 1324 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 | 1325 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - | 1326 | `	/* blksize,blocks */` |
|    1 | 1327 | `	ph7_value_int(pWorker,0);` |
|    1 | 1328 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 | 1329 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 | 1330 | `	return PH7_OK;` |
|    1 | 1331 | `}` |
|    - | 1332 | `/* Export the file:// stream */` |
|    - | 1333 | `PH7_PRIVATE const ph7_io_stream sWinFileStream = {` |
|    - | 1334 | `	"file", /* Stream name */` |
|    - | 1335 | `	PH7_IO_STREAM_VERSION,` |
|    - | 1336 | `	WinFile_Open,  /* xOpen */` |
|    - | 1337 | `	WinDir_Open,   /* xOpenDir */` |
|    - | 1338 | `	WinFile_Close, /* xClose */` |
|    - | 1339 | `	WinDir_Close,  /* xCloseDir */` |
|    - | 1340 | `	WinFile_Read,  /* xRead */` |
|    - | 1341 | `	WinDir_Read,   /* xReadDir */` |
|    - | 1342 | `	WinFile_Write, /* xWrite */` |
|    - | 1343 | `	WinFile_Seek,  /* xSeek */` |
|    - | 1344 | `	WinFile_Lock,  /* xLock */` |
|    - | 1345 | `	WinDir_RewindDir, /* xRewindDir */` |
|    - | 1346 | `	WinFile_Tell,  /* xTell */` |
|    - | 1347 | `	WinFile_Trunc, /* xTrunc */` |
|    - | 1348 | `	WinFile_Sync,  /* xSeek */` |
|    - | 1349 | `	WinFile_Stat   /* xStat */` |
|    - | 1350 | `};` |
|    - | 1351 | `#endif /* __WINNT__ */` |
|    - | 1352 |  |
