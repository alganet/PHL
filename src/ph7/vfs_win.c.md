# src/ph7/vfs_win.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 485/643 lines (75.43%)

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
|    - |   18 | `/* SPDX-SnippetBegin */` |
|    - |   19 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|    - |   20 | `/* SPDX-License-Identifier: blessing */` |
|    - |   21 | `/*` |
|    - |   22 | `** Convert a UTF-8 string to microsoft unicode (UTF-16?).` |
|    - |   23 | `**` |
|    - |   24 | `** Space to hold the returned string is obtained from HeapAlloc().` |
|    - |   25 | `** Taken from the sqlite3 source tree` |
|    - |   26 | `** status: Public Domain` |
|    - |   27 | `*/` |
|    2 |   28 | `static WCHAR *utf8ToUnicode(const char *zFilename){` |
|    - |   29 | `  int nChar;` |
|    - |   30 | `  WCHAR *zWideFilename;` |
|    - |   31 |  |
|    2 |   32 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, 0, 0);` |
|    2 |   33 | `  zWideFilename = (WCHAR *)HeapAlloc(GetProcessHeap(),0,nChar*sizeof(zWideFilename[0]));` |
|    2 |   34 | `  if( zWideFilename == 0 ){` |
|  ! 0 |   35 | ` 	return 0;` |
|    - |   36 | `  }` |
|    2 |   37 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, zWideFilename, nChar);` |
|    2 |   38 | `  if( nChar==0 ){` |
|  ! 0 |   39 | `    HeapFree(GetProcessHeap(),0,zWideFilename);` |
|  ! 0 |   40 | `    return 0;` |
|    - |   41 | `  }` |
|    2 |   42 | `  return zWideFilename;` |
|    2 |   43 |  |
|    - |   44 | `/*` |
|    - |   45 | `** Convert a UTF-8 filename into whatever form the underlying` |
|    - |   46 | `** operating system wants filenames in.Space to hold the result` |
|    - |   47 | `** is obtained from HeapAlloc() and must be freed by the calling` |
|    - |   48 | `** function.` |
|    - |   49 | `** Taken from the sqlite3 source tree` |
|    - |   50 | `** status: Public Domain` |
|    - |   51 | `*/` |
|    2 |   52 | `static void *convertUtf8Filename(const char *zFilename){` |
|    - |   53 | `  void *zConverted;` |
|    2 |   54 | `  zConverted = utf8ToUnicode(zFilename);` |
|    2 |   55 | `  return zConverted;` |
|    2 |   56 |  |
|    - |   57 | `/*` |
|    - |   58 | `** Convert microsoft unicode to UTF-8.  Space to hold the returned string is` |
|    - |   59 | `** obtained from HeapAlloc().` |
|    - |   60 | `** Taken from the sqlite3 source tree` |
|    - |   61 | `** status: Public Domain` |
|    - |   62 | `*/` |
|    2 |   63 | `static char *unicodeToUtf8(const WCHAR *zWideFilename){` |
|    - |   64 | `  char *zFilename;` |
|    - |   65 | `  int nByte;` |
|    - |   66 |  |
|    2 |   67 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, 0, 0, 0, 0);` |
|    2 |   68 | `  zFilename = (char *)HeapAlloc(GetProcessHeap(),0,nByte);` |
|    2 |   69 | `  if( zFilename == 0 ){` |
|  ! 0 |   70 | `  	return 0;` |
|    - |   71 | `  }` |
|    2 |   72 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, zFilename, nByte,0, 0);` |
|    2 |   73 | `  if( nByte == 0 ){` |
|  ! 0 |   74 | `    HeapFree(GetProcessHeap(),0,zFilename);` |
|  ! 0 |   75 | `    return 0;` |
|    - |   76 | `  }` |
|    2 |   77 | `  return zFilename;` |
|    2 |   78 |  |
|    - |   79 | `/* SPDX-SnippetEnd */` |
|    - |   80 | `/* int (*xchdir)(const char *) */` |
|    - |   81 | `static int WinVfs_chdir(const char *zPath)` |
|    2 |   82 |  |
|    - |   83 | `	void * pConverted;` |
|    - |   84 | `	BOOL rc;` |
|    2 |   85 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |   86 | `	if( pConverted == 0 ){` |
|  ! 0 |   87 | `		return -1;` |
|    - |   88 | `	}` |
|    2 |   89 | `	rc = SetCurrentDirectoryW((LPCWSTR)pConverted);` |
|    2 |   90 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |   91 | `	return rc ? PH7_OK : -1;` |
|    2 |   92 |  |
|    - |   93 | `/* int (*xGetcwd)(ph7_context *) */` |
|    - |   94 | `static int WinVfs_getcwd(ph7_context *pCtx)` |
|    2 |   95 |  |
|    - |   96 | `	WCHAR zDir[2048];` |
|    - |   97 | `	char *zConverted;` |
|    - |   98 | `	DWORD rc;` |
|    - |   99 | `	/* Get the current directory */` |
|    2 |  100 | `	rc = GetCurrentDirectoryW(sizeof(zDir),zDir);` |
|    2 |  101 | `	if( rc < 1 ){` |
|  ! 0 |  102 | `		return -1;` |
|    - |  103 | `	}` |
|    2 |  104 | `	zConverted = unicodeToUtf8(zDir);` |
|    2 |  105 | `	if( zConverted == 0 ){` |
|  ! 0 |  106 | `		return -1;` |
|    - |  107 | `	}` |
|    2 |  108 | `	ph7_result_string(pCtx,zConverted,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    2 |  109 | `	HeapFree(GetProcessHeap(),0,zConverted);` |
|    2 |  110 | `	return PH7_OK;` |
|    2 |  111 |  |
|    - |  112 | `/* int (*xMkdir)(const char *,int,int) */` |
|    - |  113 | `static int WinVfs_mkdir(const char *zPath,int mode,int recursive)` |
|    1 |  114 |  |
|    - |  115 | `	void * pConverted;` |
|    - |  116 | `	BOOL rc;` |
|    1 |  117 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  118 | `	if( pConverted == 0 ){` |
|  ! 0 |  119 | `		return -1;` |
|    - |  120 | `	}` |
|    1 |  121 | `	mode= 0; /* MSVC warning */` |
|    1 |  122 | `	recursive = 0;` |
|    1 |  123 | `	rc = CreateDirectoryW((LPCWSTR)pConverted,0);` |
|    1 |  124 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  125 | `	return rc ? PH7_OK : -1;` |
|    1 |  126 |  |
|    - |  127 | `/* int (*xRmdir)(const char *) */` |
|    - |  128 | `static int WinVfs_rmdir(const char *zPath)` |
|    1 |  129 |  |
|    - |  130 | `	void * pConverted;` |
|    - |  131 | `	BOOL rc;` |
|    1 |  132 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  133 | `	if( pConverted == 0 ){` |
|  ! 0 |  134 | `		return -1;` |
|    - |  135 | `	}` |
|    1 |  136 | `	rc = RemoveDirectoryW((LPCWSTR)pConverted);` |
|    1 |  137 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  138 | `	return rc ? PH7_OK : -1;` |
|    1 |  139 |  |
|    - |  140 | `/* int (*xIsdir)(const char *) */` |
|    - |  141 | `static int WinVfs_isdir(const char *zPath)` |
|    2 |  142 |  |
|    - |  143 | `	void * pConverted;` |
|    - |  144 | `	DWORD dwAttr;` |
|    2 |  145 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  146 | `	if( pConverted == 0 ){` |
|  ! 0 |  147 | `		return -1;` |
|    - |  148 | `	}` |
|    2 |  149 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    2 |  150 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  151 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  152 | `		return -1;` |
|    - |  153 | `	}` |
|    2 |  154 | `	return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? PH7_OK : -1;` |
|    2 |  155 |  |
|    - |  156 | `/* int (*xRename)(const char *,const char *) */` |
|    - |  157 | `static int WinVfs_Rename(const char *zOld,const char *zNew)` |
|    1 |  158 |  |
|    - |  159 | `	void *pOld,*pNew;` |
|    1 |  160 | `	BOOL rc = 0;` |
|    1 |  161 | `	pOld = convertUtf8Filename(zOld);` |
|    1 |  162 | `	if( pOld == 0 ){` |
|  ! 0 |  163 | `		return -1;` |
|    - |  164 | `	}` |
|    1 |  165 | `	pNew = convertUtf8Filename(zNew);` |
|    1 |  166 | `	if( pNew  ){` |
|    1 |  167 | `		rc = MoveFileW((LPCWSTR)pOld,(LPCWSTR)pNew);` |
|    - |  168 | `	}` |
|    1 |  169 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|    1 |  170 | `	if( pNew ){` |
|    1 |  171 | `		HeapFree(GetProcessHeap(),0,pNew);` |
|    - |  172 | `	}` |
|    1 |  173 | `	return rc ? PH7_OK : - 1;` |
|    1 |  174 |  |
|    - |  175 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|    - |  176 | `static int WinVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|    1 |  177 |  |
|    - |  178 | `	WCHAR zTemp[2048];` |
|    - |  179 | `	void *pPath;` |
|    - |  180 | `	char *zReal;` |
|    - |  181 | `	DWORD n;` |
|    1 |  182 | `	pPath = convertUtf8Filename(zPath);` |
|    1 |  183 | `	if( pPath == 0 ){` |
|  ! 0 |  184 | `		return -1;` |
|    - |  185 | `	}` |
|    1 |  186 | `	n = GetFullPathNameW((LPCWSTR)pPath,0,0,0);` |
|    1 |  187 | `	if( n > 0 ){` |
|    1 |  188 | `		if( n >= sizeof(zTemp) ){` |
|  ! 0 |  189 | `			n = sizeof(zTemp) - 1;` |
|    - |  190 | `		}` |
|    1 |  191 | `		GetFullPathNameW((LPCWSTR)pPath,n,zTemp,0);` |
|    - |  192 | `	}` |
|    1 |  193 | `	HeapFree(GetProcessHeap(),0,pPath);` |
|    1 |  194 | `	if( !n ){` |
|  ! 0 |  195 | `		return -1;` |
|    - |  196 | `	}` |
|    1 |  197 | `	zReal = unicodeToUtf8(zTemp);` |
|    1 |  198 | `	if( zReal == 0 ){` |
|  ! 0 |  199 | `		return -1;` |
|    - |  200 | `	}` |
|    1 |  201 | `	ph7_result_string(pCtx,zReal,-1); /* Will make it's own copy */` |
|    1 |  202 | `	HeapFree(GetProcessHeap(),0,zReal);` |
|    1 |  203 | `	return PH7_OK;` |
|    1 |  204 |  |
|    - |  205 | `/* int (*xSleep)(unsigned int) */` |
|    - |  206 | `static int WinVfs_Sleep(unsigned int uSec)` |
|    1 |  207 |  |
|    1 |  208 | `	Sleep(uSec/1000/*uSec per Millisec */);` |
|    1 |  209 | `	return PH7_OK;` |
|    1 |  210 |  |
|    - |  211 | `/* int (*xUnlink)(const char *) */` |
|    - |  212 | `static int WinVfs_unlink(const char *zPath)` |
|    2 |  213 |  |
|    - |  214 | `	void * pConverted;` |
|    - |  215 | `	BOOL rc;` |
|    2 |  216 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  217 | `	if( pConverted == 0 ){` |
|  ! 0 |  218 | `		return -1;` |
|    - |  219 | `	}` |
|    2 |  220 | `	rc = DeleteFileW((LPCWSTR)pConverted);` |
|    2 |  221 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  222 | `	return rc ? PH7_OK : - 1;` |
|    2 |  223 |  |
|    - |  224 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - |  225 | `static ph7_int64 WinVfs_DiskFreeSpace(const char *zPath)` |
|  ! 0 |  226 |  |
|    - |  227 | `#ifdef _WIN32_WCE` |
|    - |  228 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|    - |  229 | `	SXUNUSED(zPath);` |
|    - |  230 | `	return 0;` |
|    - |  231 | `#else` |
|    - |  232 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|    - |  233 | `	void * pConverted;` |
|    - |  234 | `	WCHAR *p;` |
|    - |  235 | `	BOOL rc;` |
|  ! 0 |  236 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  237 | `	if( pConverted == 0 ){` |
|  ! 0 |  238 | `		return 0;` |
|    - |  239 | `	}` |
|  ! 0 |  240 | `	p = (WCHAR *)pConverted;` |
|  ! 0 |  241 | `	for(;*p;p++){` |
|  ! 0 |  242 | `		if( *p == '\\' \|\| *p == '/'){` |
|  ! 0 |  243 | `			*p = '\0';` |
|  ! 0 |  244 | `			break;` |
|    - |  245 | `		}` |
|  ! 0 |  246 | `	}` |
|  ! 0 |  247 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|  ! 0 |  248 | `	if( !rc ){` |
|  ! 0 |  249 | `		return 0;` |
|    - |  250 | `	}` |
|  ! 0 |  251 | `	return (ph7_int64)dwFreeClusters * dwSectPerClust * dwBytesPerSect;` |
|    - |  252 | `#endif` |
|  ! 0 |  253 |  |
|    - |  254 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - |  255 | `static ph7_int64 WinVfs_DiskTotalSpace(const char *zPath)` |
|  ! 0 |  256 |  |
|    - |  257 | `#ifdef _WIN32_WCE` |
|    - |  258 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|    - |  259 | `	SXUNUSED(zPath);` |
|    - |  260 | `	return 0;` |
|    - |  261 | `#else` |
|    - |  262 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|    - |  263 | `	void * pConverted;` |
|    - |  264 | `	WCHAR *p;` |
|    - |  265 | `	BOOL rc;` |
|  ! 0 |  266 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  267 | `	if( pConverted == 0 ){` |
|  ! 0 |  268 | `		return 0;` |
|    - |  269 | `	}` |
|  ! 0 |  270 | `	p = (WCHAR *)pConverted;` |
|  ! 0 |  271 | `	for(;*p;p++){` |
|  ! 0 |  272 | `		if( *p == '\\' \|\| *p == '/'){` |
|  ! 0 |  273 | `			*p = '\0';` |
|  ! 0 |  274 | `			break;` |
|    - |  275 | `		}` |
|  ! 0 |  276 | `	}` |
|  ! 0 |  277 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|  ! 0 |  278 | `	if( !rc ){` |
|  ! 0 |  279 | `		return 0;` |
|    - |  280 | `	}` |
|  ! 0 |  281 | `	return (ph7_int64)dwTotalClusters * dwSectPerClust * dwBytesPerSect;` |
|    - |  282 | `#endif` |
|  ! 0 |  283 |  |
|    - |  284 | `/* int (*xFileExists)(const char *) */` |
|    - |  285 | `static int WinVfs_FileExists(const char *zPath)` |
|    1 |  286 |  |
|    - |  287 | `	void * pConverted;` |
|    - |  288 | `	DWORD dwAttr;` |
|    1 |  289 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  290 | `	if( pConverted == 0 ){` |
|  ! 0 |  291 | `		return -1;` |
|    - |  292 | `	}` |
|    1 |  293 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  294 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  295 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  296 | `		return -1;` |
|    - |  297 | `	}` |
|    1 |  298 | `	return PH7_OK;` |
|    1 |  299 |  |
|    - |  300 | `/* Open a file in a read-only mode */` |
|    - |  301 | `static HANDLE OpenReadOnly(LPCWSTR pPath)` |
|    2 |  302 |  |
|    2 |  303 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    2 |  304 | `	DWORD dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    2 |  305 | `	DWORD dwAccess = GENERIC_READ;` |
|    2 |  306 | `	DWORD dwCreate = OPEN_EXISTING;` |
|    - |  307 | `	HANDLE pHandle;` |
|    2 |  308 | `	pHandle = CreateFileW(pPath,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    2 |  309 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    1 |  310 | `		return 0;` |
|    - |  311 | `	}` |
|    2 |  312 | `	return pHandle;` |
|    2 |  313 |  |
|    - |  314 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  315 | `static ph7_int64 WinVfs_FileSize(const char *zPath)` |
|    1 |  316 |  |
|    - |  317 | `	DWORD dwLow,dwHigh;` |
|    - |  318 | `	void * pConverted;` |
|    - |  319 | `	ph7_int64 nSize;` |
|    - |  320 | `	HANDLE pHandle;` |
|    - |  321 |  |
|    1 |  322 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  323 | `	if( pConverted == 0 ){` |
|  ! 0 |  324 | `		return -1;` |
|    - |  325 | `	}` |
|    - |  326 | `	/* Open the file in read-only mode */` |
|    1 |  327 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  328 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  329 | `	if( pHandle ){` |
|    1 |  330 | `		dwLow = GetFileSize(pHandle,&dwHigh);` |
|    1 |  331 | `		nSize = dwHigh;` |
|    1 |  332 | `		nSize <<= 32;` |
|    1 |  333 | `		nSize += dwLow;` |
|    1 |  334 | `		CloseHandle(pHandle);` |
|    1 |  335 | `	}else{` |
|  ! 0 |  336 | `		nSize = -1;` |
|    - |  337 | `	}` |
|    1 |  338 | `	return nSize;` |
|    1 |  339 |  |
|    - |  340 | `#define TICKS_PER_SECOND 10000000` |
|    - |  341 | `#define EPOCH_DIFFERENCE 11644473600LL` |
|    - |  342 | `/* Convert Windows timestamp to UNIX timestamp */` |
|    - |  343 | `static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)` |
|    1 |  344 |  |
|    - |  345 | `    ph7_int64 input,temp;` |
|    1 |  346 | `	input = pTime->dwHighDateTime;` |
|    1 |  347 | `	input <<= 32;` |
|    1 |  348 | `	input += pTime->dwLowDateTime;` |
|    1 |  349 | `    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/` |
|    1 |  350 | `    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/` |
|    1 |  351 | `    return temp;` |
|    1 |  352 |  |
|    - |  353 | `/* Convert UNIX timestamp to Windows timestamp */` |
|    - |  354 | `static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)` |
|  ! 0 |  355 |  |
|  ! 0 |  356 | `  ph7_int64 result = EPOCH_DIFFERENCE;` |
|  ! 0 |  357 | `  result += nUnixtime;` |
|  ! 0 |  358 | `  result *= 10000000LL;` |
|  ! 0 |  359 | `  pOut->dwHighDateTime = (DWORD)(nUnixtime>>32);` |
|  ! 0 |  360 | `  pOut->dwLowDateTime = (DWORD)nUnixtime;` |
|  ! 0 |  361 |  |
|    - |  362 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  363 | `static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|    1 |  364 |  |
|    - |  365 | `	FILETIME sTouch,sAccess;` |
|    - |  366 | `	void *pConverted;` |
|    - |  367 | `	void *pHandle;` |
|    1 |  368 | `	BOOL rc = 0;` |
|    1 |  369 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  370 | `	if( pConverted == 0 ){` |
|  ! 0 |  371 | `		return -1;` |
|    - |  372 | `	}` |
|    1 |  373 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  374 | `	if( pHandle ){` |
|    1 |  375 | `		if( touch_time < 0 ){` |
|    1 |  376 | `			GetSystemTimeAsFileTime(&sTouch);` |
|    1 |  377 | `		}else{` |
|  ! 0 |  378 | `			convertUnixTimeToWindowsTime(touch_time,&sTouch);` |
|    - |  379 | `		}` |
|    1 |  380 | `		if( access_time < 0 ){` |
|    - |  381 | `			/* Use the touch time */` |
|    1 |  382 | `			sAccess = sTouch; /* Structure assignment */` |
|    1 |  383 | `		}else{` |
|  ! 0 |  384 | `			convertUnixTimeToWindowsTime(access_time,&sAccess);` |
|    - |  385 | `		}` |
|    1 |  386 | `		rc = SetFileTime(pHandle,&sTouch,&sAccess,0);` |
|    - |  387 | `		/* Close the handle */` |
|    1 |  388 | `		CloseHandle(pHandle);` |
|    - |  389 | `	}` |
|    1 |  390 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  391 | `	return rc ? PH7_OK : -1;` |
|    1 |  392 |  |
|    - |  393 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  394 | `static ph7_int64 WinVfs_FileAtime(const char *zPath)` |
|    1 |  395 |  |
|    - |  396 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  397 | `	void * pConverted;` |
|    - |  398 | `	ph7_int64 atime;` |
|    - |  399 | `	HANDLE pHandle;` |
|    1 |  400 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  401 | `	if( pConverted == 0 ){` |
|  ! 0 |  402 | `		return -1;` |
|    - |  403 | `	}` |
|    - |  404 | `	/* Open the file in read-only mode */` |
|    1 |  405 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  406 | `	if( pHandle ){` |
|    - |  407 | `		BOOL rc;` |
|    1 |  408 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  409 | `		if( rc ){` |
|    1 |  410 | `			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);` |
|    1 |  411 | `		}else{` |
|  ! 0 |  412 | `			atime = -1;` |
|    - |  413 | `		}` |
|    1 |  414 | `		CloseHandle(pHandle);` |
|    1 |  415 | `	}else{` |
|  ! 0 |  416 | `		atime = -1;` |
|    - |  417 | `	}` |
|    1 |  418 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  419 | `	return atime;` |
|    1 |  420 |  |
|    - |  421 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  422 | `static ph7_int64 WinVfs_FileMtime(const char *zPath)` |
|    1 |  423 |  |
|    - |  424 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  425 | `	void * pConverted;` |
|    - |  426 | `	ph7_int64 mtime;` |
|    - |  427 | `	HANDLE pHandle;` |
|    1 |  428 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  429 | `	if( pConverted == 0 ){` |
|  ! 0 |  430 | `		return -1;` |
|    - |  431 | `	}` |
|    - |  432 | `	/* Open the file in read-only mode */` |
|    1 |  433 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  434 | `	if( pHandle ){` |
|    - |  435 | `		BOOL rc;` |
|    1 |  436 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  437 | `		if( rc ){` |
|    1 |  438 | `			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);` |
|    1 |  439 | `		}else{` |
|  ! 0 |  440 | `			mtime = -1;` |
|    - |  441 | `		}` |
|    1 |  442 | `		CloseHandle(pHandle);` |
|    1 |  443 | `	}else{` |
|  ! 0 |  444 | `		mtime = -1;` |
|    - |  445 | `	}` |
|    1 |  446 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  447 | `	return mtime;` |
|    1 |  448 |  |
|    - |  449 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  450 | `static ph7_int64 WinVfs_FileCtime(const char *zPath)` |
|    1 |  451 |  |
|    - |  452 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  453 | `	void * pConverted;` |
|    - |  454 | `	ph7_int64 ctime;` |
|    - |  455 | `	HANDLE pHandle;` |
|    1 |  456 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  457 | `	if( pConverted == 0 ){` |
|  ! 0 |  458 | `		return -1;` |
|    - |  459 | `	}` |
|    - |  460 | `	/* Open the file in read-only mode */` |
|    1 |  461 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  462 | `	if( pHandle ){` |
|    - |  463 | `		BOOL rc;` |
|    1 |  464 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  465 | `		if( rc ){` |
|    1 |  466 | `			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);` |
|    1 |  467 | `		}else{` |
|  ! 0 |  468 | `			ctime = -1;` |
|    - |  469 | `		}` |
|    1 |  470 | `		CloseHandle(pHandle);` |
|    1 |  471 | `	}else{` |
|  ! 0 |  472 | `		ctime = -1;` |
|    - |  473 | `	}` |
|    1 |  474 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  475 | `	return ctime;` |
|    1 |  476 |  |
|    - |  477 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  478 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  479 | `static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|    1 |  480 |  |
|    - |  481 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  482 | `	void *pConverted;` |
|    - |  483 | `	HANDLE pHandle;` |
|    - |  484 | `	BOOL rc;` |
|    1 |  485 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  486 | `	if( pConverted == 0 ){` |
|  ! 0 |  487 | `		return -1;` |
|    - |  488 | `	}` |
|    - |  489 | `	/* Open the file in read-only mode */` |
|    1 |  490 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  491 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  492 | `	if( pHandle == 0 ){` |
|  ! 0 |  493 | `		return -1;` |
|    - |  494 | `	}` |
|    1 |  495 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  496 | `	CloseHandle(pHandle);` |
|    1 |  497 | `	if( !rc ){` |
|  ! 0 |  498 | `		return -1;` |
|    - |  499 | `	}` |
|    - |  500 | `	/* dev */` |
|    1 |  501 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 |  502 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - |  503 | `	/* ino */` |
|    1 |  504 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 |  505 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - |  506 | `	/* mode */` |
|    1 |  507 | `	ph7_value_int(pWorker,0);` |
|    1 |  508 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - |  509 | `	/* nlink */` |
|    1 |  510 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 |  511 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - |  512 | `	/* uid,gid,rdev */` |
|    1 |  513 | `	ph7_value_int(pWorker,0);` |
|    1 |  514 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 |  515 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 |  516 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - |  517 | `	/* size */` |
|    1 |  518 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 |  519 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - |  520 | `	/* atime */` |
|    1 |  521 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 |  522 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - |  523 | `	/* mtime */` |
|    1 |  524 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 |  525 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - |  526 | `	/* ctime */` |
|    1 |  527 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 |  528 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - |  529 | `	/* blksize,blocks */` |
|    1 |  530 | `	ph7_value_int(pWorker,0);` |
|    1 |  531 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 |  532 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 |  533 | `	return PH7_OK;` |
|    1 |  534 |  |
|    - |  535 | `/* int (*xIsfile)(const char *) */` |
|    - |  536 | `static int WinVfs_isfile(const char *zPath)` |
|    2 |  537 |  |
|    - |  538 | `	void * pConverted;` |
|    - |  539 | `	DWORD dwAttr;` |
|    2 |  540 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  541 | `	if( pConverted == 0 ){` |
|  ! 0 |  542 | `		return -1;` |
|    - |  543 | `	}` |
|    2 |  544 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    2 |  545 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  546 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  547 | `		return -1;` |
|    - |  548 | `	}` |
|    2 |  549 | `	return (dwAttr & (FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;` |
|    2 |  550 |  |
|    - |  551 | `/* int (*xIslink)(const char *) */` |
|    - |  552 | `static int WinVfs_islink(const char *zPath)` |
|  ! 0 |  553 |  |
|    - |  554 | `	void * pConverted;` |
|    - |  555 | `	DWORD dwAttr;` |
|  ! 0 |  556 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  557 | `	if( pConverted == 0 ){` |
|  ! 0 |  558 | `		return -1;` |
|    - |  559 | `	}` |
|  ! 0 |  560 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|  ! 0 |  561 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  562 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  563 | `		return -1;` |
|    - |  564 | `	}` |
|  ! 0 |  565 | `	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;` |
|  ! 0 |  566 |  |
|    - |  567 | `/* int (*xWritable)(const char *) */` |
|    - |  568 | `static int WinVfs_iswritable(const char *zPath)` |
|  ! 0 |  569 |  |
|    - |  570 | `	void * pConverted;` |
|    - |  571 | `	DWORD dwAttr;` |
|  ! 0 |  572 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  573 | `	if( pConverted == 0 ){` |
|  ! 0 |  574 | `		return -1;` |
|    - |  575 | `	}` |
|  ! 0 |  576 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|  ! 0 |  577 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  578 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  579 | `		return -1;` |
|    - |  580 | `	}` |
|  ! 0 |  581 | `	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE\|FILE_ATTRIBUTE_NORMAL)) == 0 ){` |
|    - |  582 | `		/* Not a regular file */` |
|  ! 0 |  583 | `		return -1;` |
|    - |  584 | `	}` |
|  ! 0 |  585 | `	if( dwAttr & FILE_ATTRIBUTE_READONLY ){` |
|    - |  586 | `		/* Read-only file */` |
|  ! 0 |  587 | `		return -1;` |
|    - |  588 | `	}` |
|    - |  589 | `	/* File is writable */` |
|  ! 0 |  590 | `	return PH7_OK;` |
|  ! 0 |  591 |  |
|    - |  592 | `/* int (*xExecutable)(const char *) */` |
|    - |  593 | `static int WinVfs_isexecutable(const char *zPath)` |
|  ! 0 |  594 |  |
|    - |  595 | `	void * pConverted;` |
|    - |  596 | `	DWORD dwAttr;` |
|  ! 0 |  597 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  598 | `	if( pConverted == 0 ){` |
|  ! 0 |  599 | `		return -1;` |
|    - |  600 | `	}` |
|  ! 0 |  601 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|  ! 0 |  602 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  603 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  604 | `		return -1;` |
|    - |  605 | `	}` |
|  ! 0 |  606 | `	if( (dwAttr & FILE_ATTRIBUTE_NORMAL) == 0 ){` |
|    - |  607 | `		/* Not a regular file */` |
|  ! 0 |  608 | `		return -1;` |
|    - |  609 | `	}` |
|    - |  610 | `	/* File is executable */` |
|  ! 0 |  611 | `	return PH7_OK;` |
|  ! 0 |  612 |  |
|    - |  613 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  614 | `static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|    1 |  615 |  |
|    - |  616 | `	void * pConverted;` |
|    - |  617 | `	DWORD dwAttr;` |
|    1 |  618 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  619 | `	if( pConverted == 0 ){` |
|    - |  620 | `		/* Expand 'unknown' */` |
|  ! 0 |  621 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  622 | `		return -1;` |
|    - |  623 | `	}` |
|    1 |  624 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  625 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  626 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    - |  627 | `		/* Expand 'unknown' */` |
|  ! 0 |  628 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  629 | `		return -1;` |
|    - |  630 | `	}` |
|    1 |  631 | `	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE) ){` |
|    1 |  632 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|    1 |  633 | `	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){` |
|    1 |  634 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|  ! 0 |  635 | `	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){` |
|  ! 0 |  636 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|  ! 0 |  637 | `	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){` |
|  ! 0 |  638 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|  ! 0 |  639 | `	}else{` |
|  ! 0 |  640 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    - |  641 | `	}` |
|    1 |  642 | `	return PH7_OK;` |
|    1 |  643 |  |
|    - |  644 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  645 | `static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|    2 |  646 |  |
|    - |  647 | `	char zValue[1024];` |
|    - |  648 | `	DWORD n;` |
|    - |  649 | `	/*` |
|    - |  650 | `	 * According to MSDN` |
|    - |  651 | `	 * If lpBuffer is not large enough to hold the data, the return` |
|    - |  652 | `	 * value is the buffer size, in characters, required to hold the` |
|    - |  653 | `	 * string and its terminating null character and the contents` |
|    - |  654 | `	 * of lpBuffer are undefined.` |
|    - |  655 | `	 */` |
|    2 |  656 | `	n = sizeof(zValue);` |
|    2 |  657 | `	SyMemcpy("Undefined",zValue,sizeof("Undefined")-1);` |
|    - |  658 | `	/* Extract the environment value */` |
|    2 |  659 | `	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));` |
|    2 |  660 | `	if( !n ){` |
|    - |  661 | `		/* No such variable*/` |
|  ! 0 |  662 | `		return -1;` |
|    - |  663 | `	}` |
|    2 |  664 | `	ph7_result_string(pCtx,zValue,(int)n);` |
|    2 |  665 | `	return PH7_OK;` |
|    2 |  666 |  |
|    - |  667 | `/* int (*xSetenv)(const char *,const char *) */` |
|    - |  668 | `static int WinVfs_Setenv(const char *zName,const char *zValue)` |
|    1 |  669 |  |
|    - |  670 | `	BOOL rc;` |
|    1 |  671 | `	rc = SetEnvironmentVariableA(zName,zValue);` |
|    1 |  672 | `	return rc ? PH7_OK : -1;` |
|    1 |  673 |  |
|    - |  674 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  675 | `static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|    2 |  676 |  |
|    - |  677 | `	DWORD dwSizeLow,dwSizeHigh;` |
|    - |  678 | `	HANDLE pHandle,pMapHandle;` |
|    - |  679 | `	void *pConverted,*pView;` |
|    - |  680 |  |
|    2 |  681 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  682 | `	if( pConverted == 0 ){` |
|  ! 0 |  683 | `		return -1;` |
|    - |  684 | `	}` |
|    2 |  685 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    2 |  686 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  687 | `	if( pHandle == 0 ){` |
|    1 |  688 | `		return -1;` |
|    - |  689 | `	}` |
|    - |  690 | `	/* Get the file size */` |
|    2 |  691 | `	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);` |
|    - |  692 | `	/* Create the mapping */` |
|    2 |  693 | `	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);` |
|    2 |  694 | `	if( pMapHandle == 0 ){` |
|  ! 0 |  695 | `		CloseHandle(pHandle);` |
|  ! 0 |  696 | `		return -1;` |
|    - |  697 | `	}` |
|    2 |  698 | `	*pSize = ((ph7_int64)dwSizeHigh << 32) \| dwSizeLow;` |
|    - |  699 | `	/* Obtain the view */` |
|    2 |  700 | `	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));` |
|    2 |  701 | `	if( pView ){` |
|    - |  702 | `		/* Let the upper layer point to the view */` |
|    2 |  703 | `		*ppMap = pView;` |
|    - |  704 | `	}` |
|    - |  705 | `	/* Close the handle` |
|    - |  706 | `	 * According to MSDN it's OK the close the HANDLES.` |
|    - |  707 | `	 */` |
|    2 |  708 | `	CloseHandle(pMapHandle);` |
|    2 |  709 | `	CloseHandle(pHandle);` |
|    2 |  710 | `	return pView ? PH7_OK : -1;` |
|    2 |  711 |  |
|    - |  712 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|    - |  713 | `static void WinVfs_Unmap(void *pView,ph7_int64 nSize)` |
|    2 |  714 |  |
|    2 |  715 | `	nSize = 0; /* Compiler warning */` |
|    2 |  716 | `	UnmapViewOfFile(pView);` |
|    2 |  717 |  |
|    - |  718 | `/* void (*xTempDir)(ph7_context *) */` |
|    - |  719 | `static void WinVfs_TempDir(ph7_context *pCtx)` |
|    2 |  720 |  |
|    - |  721 | `	CHAR zTemp[1024];` |
|    - |  722 | `	DWORD n;` |
|    2 |  723 | `	n = GetTempPathA(sizeof(zTemp),zTemp);` |
|    2 |  724 | `	if( n < 1 ){` |
|    - |  725 | `		/* Assume the default windows temp directory */` |
|  ! 0 |  726 | `		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);` |
|  ! 0 |  727 | `	}else{` |
|    2 |  728 | `		ph7_result_string(pCtx,zTemp,(int)n);` |
|    - |  729 | `	}` |
|    2 |  730 |  |
|    - |  731 | `/* unsigned int (*xProcessId)(void) */` |
|    - |  732 | `static unsigned int WinVfs_ProcessId(void)` |
|    1 |  733 |  |
|    1 |  734 | `	DWORD nID = 0;` |
|    - |  735 | `#ifndef __MINGW32__` |
|    1 |  736 | `	nID = GetProcessId(GetCurrentProcess());` |
|    - |  737 | `#endif /* __MINGW32__ */` |
|    1 |  738 | `	return (unsigned int)nID;` |
|    1 |  739 |  |
|    - |  740 | `/* void (*xUsername)(ph7_context *) */` |
|    - |  741 | `static void WinVfs_Username(ph7_context *pCtx)` |
|    1 |  742 |  |
|    - |  743 | `	WCHAR zUser[1024];` |
|    - |  744 | `	DWORD nByte;` |
|    - |  745 | `	BOOL rc;` |
|    1 |  746 | `	nByte = sizeof(zUser);` |
|    1 |  747 | `	rc = GetUserNameW(zUser,&nByte);` |
|    1 |  748 | `	if( !rc ){` |
|    - |  749 | `		/* Set a dummy name */` |
|  ! 0 |  750 | `		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  751 | `	}else{` |
|    - |  752 | `		char *zName;` |
|    1 |  753 | `		zName = unicodeToUtf8(zUser);` |
|    1 |  754 | `		if( zName == 0 ){` |
|  ! 0 |  755 | `			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  756 | `		}else{` |
|    1 |  757 | `			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    1 |  758 | `			HeapFree(GetProcessHeap(),0,zName);` |
|    - |  759 | `		}` |
|    - |  760 | `	}` |
|    - |  761 |  |
|    1 |  762 |  |
|    - |  763 | `/* Export the windows vfs */` |
|    - |  764 | `PH7_PRIVATE const ph7_vfs sWinVfs = {` |
|    - |  765 | `	"Windows_vfs",` |
|    - |  766 | `	PH7_VFS_VERSION,` |
|    - |  767 | `	WinVfs_chdir,    /* int (*xChdir)(const char *) */` |
|    - |  768 |  |
|    - |  769 | `	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|    - |  770 | `	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|    - |  771 | `	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|    - |  772 | `	WinVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|    - |  773 | `	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|    - |  774 | `	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|    - |  775 | `	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */` |
|    - |  776 | `	WinVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|    - |  777 | `	WinVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|    - |  778 |  |
|    - |  779 |  |
|    - |  780 |  |
|    - |  781 | `	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - |  782 | `	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - |  783 | `	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  784 | `	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  785 | `	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  786 | `	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  787 | `	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  788 | `	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  789 | `	WinVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|    - |  790 | `	WinVfs_islink,     /* int (*xIslink)(const char *) */` |
|    - |  791 | `	WinVfs_isfile,     /* int (*xReadable)(const char *) */` |
|    - |  792 | `	WinVfs_iswritable, /* int (*xWritable)(const char *) */` |
|    - |  793 | `	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */` |
|    - |  794 | `	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  795 | `	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  796 | `	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|    - |  797 | `	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  798 | `	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  799 | `	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|    - |  800 |  |
|    - |  801 |  |
|    - |  802 | `	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|    - |  803 | `	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|    - |  804 |  |
|    - |  805 |  |
|    - |  806 | `	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|    - |  807 |  |
|    - |  808 | `};` |
|    - |  809 | `/* Windows file IO */` |
|    - |  810 | `#ifndef INVALID_SET_FILE_POINTER` |
|    - |  811 | `# define INVALID_SET_FILE_POINTER ((DWORD)-1)` |
|    - |  812 | `#endif` |
|    - |  813 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|    - |  814 | `static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|    2 |  815 |  |
|    2 |  816 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    2 |  817 | `	DWORD dwAccess = GENERIC_READ;` |
|    - |  818 | `	DWORD dwShare,dwCreate;` |
|    - |  819 | `	void *pConverted;` |
|    - |  820 | `	HANDLE pHandle;` |
|    - |  821 |  |
|    2 |  822 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  823 | `	if( pConverted == 0 ){` |
|  ! 0 |  824 | `		return -1;` |
|    - |  825 | `	}` |
|    - |  826 | `	/* Set the desired flags according to the open mode */` |
|    2 |  827 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|    - |  828 | `		/* Open existing file, or create if it doesn't exist */` |
|    2 |  829 | `		dwCreate = OPEN_ALWAYS;` |
|    2 |  830 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - |  831 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|    2 |  832 | `			dwCreate = CREATE_ALWAYS;` |
|    2 |  833 | `		}` |
|    2 |  834 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|    - |  835 | `		/* Creates a new file, only if it does not already exist.` |
|    - |  836 | `		* If the file exists, it fails.` |
|    - |  837 | `		*/` |
|  ! 0 |  838 | `		dwCreate = CREATE_NEW;` |
|    2 |  839 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - |  840 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|    - |  841 | `		 * The file must exist.` |
|    - |  842 | `		 */` |
|  ! 0 |  843 | `		dwCreate = TRUNCATE_EXISTING;` |
|  ! 0 |  844 | `	}else{` |
|    - |  845 | `		/* Opens a file, only if it exists. */` |
|    2 |  846 | `		dwCreate = OPEN_EXISTING;` |
|    - |  847 | `	}` |
|    2 |  848 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|    - |  849 | `		/* Read+Write access */` |
|    2 |  850 | `		dwAccess \|= GENERIC_WRITE;` |
|    2 |  851 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|    - |  852 | `		/* Write only access */` |
|    1 |  853 | `		dwAccess = GENERIC_WRITE;` |
|    - |  854 | `	}` |
|    2 |  855 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|    - |  856 | `		/* Append mode */` |
|  ! 0 |  857 | `		dwAccess = FILE_APPEND_DATA;` |
|    - |  858 | `	}` |
|    2 |  859 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|    - |  860 | `		/* File is temporary */` |
|  ! 0 |  861 | `		dwType = FILE_ATTRIBUTE_TEMPORARY;` |
|    - |  862 | `	}` |
|    2 |  863 | `	dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    2 |  864 | `	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    2 |  865 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  866 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    - |  867 | `		SXUNUSED(pResource); /* MSVC warning */` |
|    2 |  868 | `		return -1;` |
|    - |  869 | `	}` |
|    - |  870 | `	/* Make the handle accessible to the upper layer */` |
|    2 |  871 | `	*ppHandle = (void *)pHandle;` |
|    2 |  872 | `	return PH7_OK;` |
|    2 |  873 |  |
|    - |  874 | `/* An instance of the following structure is used to record state information` |
|    - |  875 | ` * while iterating throw directory entries.` |
|    - |  876 | ` */` |
|    - |  877 | `typedef struct WinDir_Info WinDir_Info;` |
|    - |  878 | `struct WinDir_Info` |
|    - |  879 |  |
|    - |  880 | `	HANDLE pDirHandle;` |
|    - |  881 | `	void *pPath;` |
|    - |  882 | `	WIN32_FIND_DATAW sInfo;` |
|    - |  883 | `	int rc;` |
|    - |  884 | `};` |
|    - |  885 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|    - |  886 | `static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|    2 |  887 |  |
|    - |  888 | `	WinDir_Info *pDirInfo;` |
|    - |  889 | `	void *pConverted;` |
|    - |  890 | `	char *zPrep;` |
|    - |  891 | `	sxu32 n;` |
|    - |  892 | `	/* Prepare the path */` |
|    2 |  893 | `	n = SyStrlen(zPath);` |
|    2 |  894 | `	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);` |
|    2 |  895 | `	if( zPrep == 0 ){` |
|  ! 0 |  896 | `		return -1;` |
|    - |  897 | `	}` |
|    2 |  898 | `	SyMemcpy((const void *)zPath,zPrep,n);` |
|    2 |  899 | `	zPrep[n]   = '\\';` |
|    2 |  900 | `	zPrep[n+1] =  '*';` |
|    2 |  901 | `	zPrep[n+2] = 0;` |
|    2 |  902 | `	pConverted = convertUtf8Filename(zPrep);` |
|    2 |  903 | `	HeapFree(GetProcessHeap(),0,zPrep);` |
|    2 |  904 | `	if( pConverted == 0 ){` |
|  ! 0 |  905 | `		return -1;` |
|    - |  906 | `	}` |
|    - |  907 | `	/* Allocate a new instance */` |
|    2 |  908 | `	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));` |
|    2 |  909 | `	if( pDirInfo == 0 ){` |
|  ! 0 |  910 | `		pResource = 0; /* Compiler warning */` |
|  ! 0 |  911 | `		return -1;` |
|    - |  912 | `	}` |
|    2 |  913 | `	pDirInfo->rc = SXRET_OK;` |
|    2 |  914 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);` |
|    2 |  915 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|    - |  916 | `		/* Cannot open directory */` |
|  ! 0 |  917 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  918 | `		HeapFree(GetProcessHeap(),0,pDirInfo);` |
|  ! 0 |  919 | `		return -1;` |
|    - |  920 | `	}` |
|    - |  921 | `	/* Save the path */` |
|    2 |  922 | `	pDirInfo->pPath = pConverted;` |
|    - |  923 | `	/* Save our structure */` |
|    2 |  924 | `	*ppHandle = pDirInfo;` |
|    2 |  925 | `	return PH7_OK;` |
|    2 |  926 |  |
|    - |  927 | `/* void (*xCloseDir)(void *) */` |
|    - |  928 | `static void WinDir_Close(void *pUserData)` |
|    2 |  929 |  |
|    2 |  930 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    2 |  931 | `	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){` |
|    2 |  932 | `		FindClose(pDirInfo->pDirHandle);` |
|    - |  933 | `	}` |
|    2 |  934 | `	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);` |
|    2 |  935 | `	HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    2 |  936 |  |
|    - |  937 | `/* void (*xClose)(void *); */` |
|    - |  938 | `static void WinFile_Close(void *pUserData)` |
|    2 |  939 |  |
|    2 |  940 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    2 |  941 | `	CloseHandle(pHandle);` |
|    2 |  942 |  |
|    - |  943 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|    - |  944 | `static int WinDir_Read(void *pUserData,ph7_context *pCtx)` |
|    2 |  945 |  |
|    2 |  946 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    - |  947 | `	LPWIN32_FIND_DATAW pData;` |
|    - |  948 | `	char *zName;` |
|    - |  949 | `	BOOL rc;` |
|    - |  950 | `	sxu32 n;` |
|    2 |  951 | `	if( pDirInfo->rc != SXRET_OK ){` |
|    - |  952 | `		/* No more entry to process */` |
|    2 |  953 | `		return -1;` |
|    - |  954 | `	}` |
|    2 |  955 | `	pData = &pDirInfo->sInfo;` |
|    - |  956 | `	for(;;){` |
|    2 |  957 | `		zName = unicodeToUtf8(pData->cFileName);` |
|    2 |  958 | `		if( zName == 0 ){` |
|    - |  959 | `			/* Out of memory */` |
|  ! 0 |  960 | `			return -1;` |
|    - |  961 | `		}` |
|    2 |  962 | `		n = SyStrlen(zName);` |
|    - |  963 | `		/* Ignore '.' && '..' */` |
|    2 |  964 | `		if( n > sizeof("..")-1 \|\| zName[0] != '.' \|\| ( n == sizeof("..")-1 && zName[1] != '.') ){` |
|    2 |  965 | `			break;` |
|    - |  966 | `		}` |
|    2 |  967 | `		HeapFree(GetProcessHeap(),0,zName);` |
|    2 |  968 | `		rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|    2 |  969 | `		if( !rc ){` |
|  ! 0 |  970 | `			return -1;` |
|    - |  971 | `		}` |
|    2 |  972 | `	}` |
|    - |  973 | `	/* Return the current file name */` |
|    2 |  974 | `	ph7_result_string(pCtx,zName,-1);` |
|    2 |  975 | `	HeapFree(GetProcessHeap(),0,zName);` |
|    - |  976 | `	/* Point to the next entry */` |
|    2 |  977 | `	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|    2 |  978 | `	if( !rc ){` |
|    2 |  979 | `		pDirInfo->rc = SXERR_EOF;` |
|    - |  980 | `	}` |
|    2 |  981 | `	return PH7_OK;` |
|    2 |  982 |  |
|    - |  983 | `/* void (*xRewindDir)(void *) */` |
|    - |  984 | `static void WinDir_RewindDir(void *pUserData)` |
|    1 |  985 |  |
|    1 |  986 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    1 |  987 | `	FindClose(pDirInfo->pDirHandle);` |
|    1 |  988 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);` |
|    1 |  989 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 |  990 | `		pDirInfo->rc = SXERR_EOF;` |
|  ! 0 |  991 | `	}else{` |
|    1 |  992 | `		pDirInfo->rc = SXRET_OK;` |
|    - |  993 | `	}` |
|    1 |  994 |  |
|    - |  995 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|    - |  996 | `static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)` |
|    2 |  997 |  |
|    2 |  998 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - |  999 | `	DWORD nRd;` |
|    - | 1000 | `	BOOL rc;` |
|    2 | 1001 | `	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    2 | 1002 | `	if( !rc ){` |
|    - | 1003 | `		/* EOF or IO error */` |
|  ! 0 | 1004 | `		return -1;` |
|    - | 1005 | `	}` |
|    2 | 1006 | `	return (ph7_int64)nRd;` |
|    2 | 1007 |  |
|    - | 1008 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|    - | 1009 | `static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)` |
|    2 | 1010 |  |
|    2 | 1011 | `	const char *zData = (const char *)pBuffer;` |
|    2 | 1012 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1013 | `	ph7_int64 nCount;` |
|    - | 1014 | `	DWORD nWr;` |
|    - | 1015 | `	BOOL rc;` |
|    2 | 1016 | `	nWr = 0;` |
|    2 | 1017 | `	nCount = 0;` |
|    - | 1018 | `	for(;;){` |
|    2 | 1019 | `		if( nWrite < 1 ){` |
|    2 | 1020 | `			break;` |
|    - | 1021 | `		}` |
|    2 | 1022 | `		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);` |
|    2 | 1023 | `		if( !rc ){` |
|    - | 1024 | `			/* IO error */` |
|  ! 0 | 1025 | `			break;` |
|    - | 1026 | `		}` |
|    2 | 1027 | `		nWrite -= nWr;` |
|    2 | 1028 | `		nCount += nWr;` |
|    2 | 1029 | `		zData += nWr;` |
|    2 | 1030 | `	}` |
|    2 | 1031 | `	if( nWrite > 0 ){` |
|  ! 0 | 1032 | `		return -1;` |
|    - | 1033 | `	}` |
|    2 | 1034 | `	return nCount;` |
|    2 | 1035 |  |
|    - | 1036 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|    - | 1037 | `static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|    1 | 1038 |  |
|    1 | 1039 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1040 | `	DWORD dwMove,dwNew;` |
|    - | 1041 | `	LONG nHighOfft;` |
|    1 | 1042 | `	switch(whence){` |
|    - | 1043 | `	case 1:/*SEEK_CUR*/` |
|  ! 0 | 1044 | `		dwMove = FILE_CURRENT;` |
|  ! 0 | 1045 | `		break;` |
|    - | 1046 | `	case 2: /* SEEK_END */` |
|  ! 0 | 1047 | `		dwMove = FILE_END;` |
|  ! 0 | 1048 | `		break;` |
|    - | 1049 | `	case 0: /* SEEK_SET */` |
|    - | 1050 | `	default:` |
|    1 | 1051 | `		dwMove = FILE_BEGIN;` |
|    - | 1052 | `		break;` |
|    - | 1053 | `	}` |
|    1 | 1054 | `	nHighOfft = (LONG)(iOfft >> 32);` |
|    1 | 1055 | `	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);` |
|    1 | 1056 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1057 | `		return -1;` |
|    - | 1058 | `	}` |
|    1 | 1059 | `	return PH7_OK;` |
|    1 | 1060 |  |
|    - | 1061 | `/* int (*xLock)(void *,int) */` |
|    - | 1062 | `static int WinFile_Lock(void *pUserData,int lock_type)` |
|    1 | 1063 |  |
|    1 | 1064 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1065 | `	static DWORD dwLo = 0,dwHi = 0; /* xx: MT-SAFE */` |
|    - | 1066 | `	OVERLAPPED sDummy;` |
|    - | 1067 | `	BOOL rc;` |
|    1 | 1068 | `	SyZero(&sDummy,sizeof(sDummy));` |
|    - | 1069 | `	/* Get the file size */` |
|    1 | 1070 | `	if( lock_type < 1 ){` |
|    - | 1071 | `		/* Unlock the file */` |
|    1 | 1072 | `		rc = UnlockFileEx(pHandle,0,dwLo,dwHi,&sDummy);` |
|    1 | 1073 | `	}else{` |
|    1 | 1074 | `		DWORD dwFlags = LOCKFILE_FAIL_IMMEDIATELY; /* Shared non-blocking lock by default*/` |
|    - | 1075 | `		/* Lock the file */` |
|    1 | 1076 | `		if( lock_type == 1 /* LOCK_EXCL */ ){` |
|    1 | 1077 | `			dwFlags \|= LOCKFILE_EXCLUSIVE_LOCK;` |
|    - | 1078 | `		}` |
|    1 | 1079 | `		dwLo = GetFileSize(pHandle,&dwHi);` |
|    1 | 1080 | `		rc = LockFileEx(pHandle,dwFlags,0,dwLo,dwHi,&sDummy);` |
|    - | 1081 | `	}` |
|    1 | 1082 | `	return rc ? PH7_OK : -1 /* Lock error */;` |
|    1 | 1083 |  |
|    - | 1084 | `/* ph7_int64 (*xTell)(void *) */` |
|    - | 1085 | `static ph7_int64 WinFile_Tell(void *pUserData)` |
|    1 | 1086 |  |
|    1 | 1087 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1088 | `	DWORD dwNew;` |
|    1 | 1089 | `	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);` |
|    1 | 1090 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1091 | `		return -1;` |
|    - | 1092 | `	}` |
|    1 | 1093 | `	return (ph7_int64)dwNew;` |
|    1 | 1094 |  |
|    - | 1095 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|    - | 1096 | `static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|    1 | 1097 |  |
|    1 | 1098 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1099 | `	LONG HighOfft;` |
|    - | 1100 | `	DWORD dwNew;` |
|    - | 1101 | `	BOOL rc;` |
|    1 | 1102 | `	HighOfft = (LONG)(nOfft >> 32);` |
|    1 | 1103 | `	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);` |
|    1 | 1104 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1105 | `		return -1;` |
|    - | 1106 | `	}` |
|    1 | 1107 | `	rc = SetEndOfFile(pHandle);` |
|    1 | 1108 | `	return rc ? PH7_OK : -1;` |
|    1 | 1109 |  |
|    - | 1110 | `/* int (*xSync)(void *); */` |
|    - | 1111 | `static int WinFile_Sync(void *pUserData)` |
|    1 | 1112 |  |
|    1 | 1113 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1114 | `	BOOL rc;` |
|    1 | 1115 | `	rc = FlushFileBuffers(pHandle);` |
|    1 | 1116 | `	return rc ? PH7_OK : - 1;` |
|    1 | 1117 |  |
|    - | 1118 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|    - | 1119 | `static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 1120 |  |
|    - | 1121 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    1 | 1122 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1123 | `	BOOL rc;` |
|    1 | 1124 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 | 1125 | `	if( !rc ){` |
|  ! 0 | 1126 | `		return -1;` |
|    - | 1127 | `	}` |
|    - | 1128 | `	/* dev */` |
|    1 | 1129 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 | 1130 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - | 1131 | `	/* ino */` |
|    1 | 1132 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 | 1133 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - | 1134 | `	/* mode */` |
|    1 | 1135 | `	ph7_value_int(pWorker,0);` |
|    1 | 1136 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - | 1137 | `	/* nlink */` |
|    1 | 1138 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 | 1139 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - | 1140 | `	/* uid,gid,rdev */` |
|    1 | 1141 | `	ph7_value_int(pWorker,0);` |
|    1 | 1142 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 | 1143 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 | 1144 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - | 1145 | `	/* size */` |
|    1 | 1146 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 | 1147 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - | 1148 | `	/* atime */` |
|    1 | 1149 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 | 1150 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - | 1151 | `	/* mtime */` |
|    1 | 1152 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 | 1153 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - | 1154 | `	/* ctime */` |
|    1 | 1155 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 | 1156 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - | 1157 | `	/* blksize,blocks */` |
|    1 | 1158 | `	ph7_value_int(pWorker,0);` |
|    1 | 1159 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 | 1160 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 | 1161 | `	return PH7_OK;` |
|    1 | 1162 |  |
|    - | 1163 | `/* Export the file:// stream */` |
|    - | 1164 | `PH7_PRIVATE const ph7_io_stream sWinFileStream = {` |
|    - | 1165 | `	"file", /* Stream name */` |
|    - | 1166 | `	PH7_IO_STREAM_VERSION,` |
|    - | 1167 | `	WinFile_Open,  /* xOpen */` |
|    - | 1168 | `	WinDir_Open,   /* xOpenDir */` |
|    - | 1169 | `	WinFile_Close, /* xClose */` |
|    - | 1170 | `	WinDir_Close,  /* xCloseDir */` |
|    - | 1171 | `	WinFile_Read,  /* xRead */` |
|    - | 1172 | `	WinDir_Read,   /* xReadDir */` |
|    - | 1173 | `	WinFile_Write, /* xWrite */` |
|    - | 1174 | `	WinFile_Seek,  /* xSeek */` |
|    - | 1175 | `	WinFile_Lock,  /* xLock */` |
|    - | 1176 | `	WinDir_RewindDir, /* xRewindDir */` |
|    - | 1177 | `	WinFile_Tell,  /* xTell */` |
|    - | 1178 | `	WinFile_Trunc, /* xTrunc */` |
|    - | 1179 | `	WinFile_Sync,  /* xSeek */` |
|    - | 1180 | `	WinFile_Stat   /* xStat */` |
|    - | 1181 | `};` |
|    - | 1182 | `#endif /* __WINNT__ */` |
|    - | 1183 |  |
