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
|    - |   18 | `/*` |
|    - |   19 | `** Convert a UTF-8 string to microsoft unicode (UTF-16?).` |
|    - |   20 | `**` |
|    - |   21 | `** Space to hold the returned string is obtained from HeapAlloc().` |
|    - |   22 | `** Taken from the sqlite3 source tree` |
|    - |   23 | `** status: Public Domain` |
|    - |   24 | `*/` |
|    2 |   25 | `static WCHAR *utf8ToUnicode(const char *zFilename){` |
|    - |   26 | `  int nChar;` |
|    - |   27 | `  WCHAR *zWideFilename;` |
|    - |   28 |  |
|    2 |   29 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, 0, 0);` |
|    2 |   30 | `  zWideFilename = (WCHAR *)HeapAlloc(GetProcessHeap(),0,nChar*sizeof(zWideFilename[0]));` |
|    2 |   31 | `  if( zWideFilename == 0 ){` |
|  ! 0 |   32 | ` 	return 0;` |
|    - |   33 | `  }` |
|    2 |   34 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, zWideFilename, nChar);` |
|    2 |   35 | `  if( nChar==0 ){` |
|  ! 0 |   36 | `    HeapFree(GetProcessHeap(),0,zWideFilename);` |
|  ! 0 |   37 | `    return 0;` |
|    - |   38 | `  }` |
|    2 |   39 | `  return zWideFilename;` |
|    2 |   40 |  |
|    - |   41 | `/*` |
|    - |   42 | `** Convert a UTF-8 filename into whatever form the underlying` |
|    - |   43 | `** operating system wants filenames in.Space to hold the result` |
|    - |   44 | `** is obtained from HeapAlloc() and must be freed by the calling` |
|    - |   45 | `** function.` |
|    - |   46 | `** Taken from the sqlite3 source tree` |
|    - |   47 | `** status: Public Domain` |
|    - |   48 | `*/` |
|    2 |   49 | `static void *convertUtf8Filename(const char *zFilename){` |
|    - |   50 | `  void *zConverted;` |
|    2 |   51 | `  zConverted = utf8ToUnicode(zFilename);` |
|    2 |   52 | `  return zConverted;` |
|    2 |   53 |  |
|    - |   54 | `/*` |
|    - |   55 | `** Convert microsoft unicode to UTF-8.  Space to hold the returned string is` |
|    - |   56 | `** obtained from HeapAlloc().` |
|    - |   57 | `** Taken from the sqlite3 source tree` |
|    - |   58 | `** status: Public Domain` |
|    - |   59 | `*/` |
|    2 |   60 | `static char *unicodeToUtf8(const WCHAR *zWideFilename){` |
|    - |   61 | `  char *zFilename;` |
|    - |   62 | `  int nByte;` |
|    - |   63 |  |
|    2 |   64 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, 0, 0, 0, 0);` |
|    2 |   65 | `  zFilename = (char *)HeapAlloc(GetProcessHeap(),0,nByte);` |
|    2 |   66 | `  if( zFilename == 0 ){` |
|  ! 0 |   67 | `  	return 0;` |
|    - |   68 | `  }` |
|    2 |   69 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, zFilename, nByte,0, 0);` |
|    2 |   70 | `  if( nByte == 0 ){` |
|  ! 0 |   71 | `    HeapFree(GetProcessHeap(),0,zFilename);` |
|  ! 0 |   72 | `    return 0;` |
|    - |   73 | `  }` |
|    2 |   74 | `  return zFilename;` |
|    2 |   75 |  |
|    - |   76 | `/* int (*xchdir)(const char *) */` |
|    - |   77 | `static int WinVfs_chdir(const char *zPath)` |
|    2 |   78 |  |
|    - |   79 | `	void * pConverted;` |
|    - |   80 | `	BOOL rc;` |
|    2 |   81 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |   82 | `	if( pConverted == 0 ){` |
|  ! 0 |   83 | `		return -1;` |
|    - |   84 | `	}` |
|    2 |   85 | `	rc = SetCurrentDirectoryW((LPCWSTR)pConverted);` |
|    2 |   86 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |   87 | `	return rc ? PH7_OK : -1;` |
|    2 |   88 |  |
|    - |   89 | `/* int (*xGetcwd)(ph7_context *) */` |
|    - |   90 | `static int WinVfs_getcwd(ph7_context *pCtx)` |
|    2 |   91 |  |
|    - |   92 | `	WCHAR zDir[2048];` |
|    - |   93 | `	char *zConverted;` |
|    - |   94 | `	DWORD rc;` |
|    - |   95 | `	/* Get the current directory */` |
|    2 |   96 | `	rc = GetCurrentDirectoryW(sizeof(zDir),zDir);` |
|    2 |   97 | `	if( rc < 1 ){` |
|  ! 0 |   98 | `		return -1;` |
|    - |   99 | `	}` |
|    2 |  100 | `	zConverted = unicodeToUtf8(zDir);` |
|    2 |  101 | `	if( zConverted == 0 ){` |
|  ! 0 |  102 | `		return -1;` |
|    - |  103 | `	}` |
|    2 |  104 | `	ph7_result_string(pCtx,zConverted,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    2 |  105 | `	HeapFree(GetProcessHeap(),0,zConverted);` |
|    2 |  106 | `	return PH7_OK;` |
|    2 |  107 |  |
|    - |  108 | `/* int (*xMkdir)(const char *,int,int) */` |
|    - |  109 | `static int WinVfs_mkdir(const char *zPath,int mode,int recursive)` |
|    1 |  110 |  |
|    - |  111 | `	void * pConverted;` |
|    - |  112 | `	BOOL rc;` |
|    1 |  113 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  114 | `	if( pConverted == 0 ){` |
|  ! 0 |  115 | `		return -1;` |
|    - |  116 | `	}` |
|    1 |  117 | `	mode= 0; /* MSVC warning */` |
|    1 |  118 | `	recursive = 0;` |
|    1 |  119 | `	rc = CreateDirectoryW((LPCWSTR)pConverted,0);` |
|    1 |  120 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  121 | `	return rc ? PH7_OK : -1;` |
|    1 |  122 |  |
|    - |  123 | `/* int (*xRmdir)(const char *) */` |
|    - |  124 | `static int WinVfs_rmdir(const char *zPath)` |
|    1 |  125 |  |
|    - |  126 | `	void * pConverted;` |
|    - |  127 | `	BOOL rc;` |
|    1 |  128 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  129 | `	if( pConverted == 0 ){` |
|  ! 0 |  130 | `		return -1;` |
|    - |  131 | `	}` |
|    1 |  132 | `	rc = RemoveDirectoryW((LPCWSTR)pConverted);` |
|    1 |  133 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  134 | `	return rc ? PH7_OK : -1;` |
|    1 |  135 |  |
|    - |  136 | `/* int (*xIsdir)(const char *) */` |
|    - |  137 | `static int WinVfs_isdir(const char *zPath)` |
|    2 |  138 |  |
|    - |  139 | `	void * pConverted;` |
|    - |  140 | `	DWORD dwAttr;` |
|    2 |  141 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  142 | `	if( pConverted == 0 ){` |
|  ! 0 |  143 | `		return -1;` |
|    - |  144 | `	}` |
|    2 |  145 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    2 |  146 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  147 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  148 | `		return -1;` |
|    - |  149 | `	}` |
|    2 |  150 | `	return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? PH7_OK : -1;` |
|    2 |  151 |  |
|    - |  152 | `/* int (*xRename)(const char *,const char *) */` |
|    - |  153 | `static int WinVfs_Rename(const char *zOld,const char *zNew)` |
|    1 |  154 |  |
|    - |  155 | `	void *pOld,*pNew;` |
|    1 |  156 | `	BOOL rc = 0;` |
|    1 |  157 | `	pOld = convertUtf8Filename(zOld);` |
|    1 |  158 | `	if( pOld == 0 ){` |
|  ! 0 |  159 | `		return -1;` |
|    - |  160 | `	}` |
|    1 |  161 | `	pNew = convertUtf8Filename(zNew);` |
|    1 |  162 | `	if( pNew  ){` |
|    1 |  163 | `		rc = MoveFileW((LPCWSTR)pOld,(LPCWSTR)pNew);` |
|    - |  164 | `	}` |
|    1 |  165 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|    1 |  166 | `	if( pNew ){` |
|    1 |  167 | `		HeapFree(GetProcessHeap(),0,pNew);` |
|    - |  168 | `	}` |
|    1 |  169 | `	return rc ? PH7_OK : - 1;` |
|    1 |  170 |  |
|    - |  171 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|    - |  172 | `static int WinVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|    1 |  173 |  |
|    - |  174 | `	WCHAR zTemp[2048];` |
|    - |  175 | `	void *pPath;` |
|    - |  176 | `	char *zReal;` |
|    - |  177 | `	DWORD n;` |
|    1 |  178 | `	pPath = convertUtf8Filename(zPath);` |
|    1 |  179 | `	if( pPath == 0 ){` |
|  ! 0 |  180 | `		return -1;` |
|    - |  181 | `	}` |
|    1 |  182 | `	n = GetFullPathNameW((LPCWSTR)pPath,0,0,0);` |
|    1 |  183 | `	if( n > 0 ){` |
|    1 |  184 | `		if( n >= sizeof(zTemp) ){` |
|  ! 0 |  185 | `			n = sizeof(zTemp) - 1;` |
|    - |  186 | `		}` |
|    1 |  187 | `		GetFullPathNameW((LPCWSTR)pPath,n,zTemp,0);` |
|    - |  188 | `	}` |
|    1 |  189 | `	HeapFree(GetProcessHeap(),0,pPath);` |
|    1 |  190 | `	if( !n ){` |
|  ! 0 |  191 | `		return -1;` |
|    - |  192 | `	}` |
|    1 |  193 | `	zReal = unicodeToUtf8(zTemp);` |
|    1 |  194 | `	if( zReal == 0 ){` |
|  ! 0 |  195 | `		return -1;` |
|    - |  196 | `	}` |
|    1 |  197 | `	ph7_result_string(pCtx,zReal,-1); /* Will make it's own copy */` |
|    1 |  198 | `	HeapFree(GetProcessHeap(),0,zReal);` |
|    1 |  199 | `	return PH7_OK;` |
|    1 |  200 |  |
|    - |  201 | `/* int (*xSleep)(unsigned int) */` |
|    - |  202 | `static int WinVfs_Sleep(unsigned int uSec)` |
|    1 |  203 |  |
|    1 |  204 | `	Sleep(uSec/1000/*uSec per Millisec */);` |
|    1 |  205 | `	return PH7_OK;` |
|    1 |  206 |  |
|    - |  207 | `/* int (*xUnlink)(const char *) */` |
|    - |  208 | `static int WinVfs_unlink(const char *zPath)` |
|    2 |  209 |  |
|    - |  210 | `	void * pConverted;` |
|    - |  211 | `	BOOL rc;` |
|    2 |  212 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  213 | `	if( pConverted == 0 ){` |
|  ! 0 |  214 | `		return -1;` |
|    - |  215 | `	}` |
|    2 |  216 | `	rc = DeleteFileW((LPCWSTR)pConverted);` |
|    2 |  217 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  218 | `	return rc ? PH7_OK : - 1;` |
|    2 |  219 |  |
|    - |  220 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - |  221 | `static ph7_int64 WinVfs_DiskFreeSpace(const char *zPath)` |
|  ! 0 |  222 |  |
|    - |  223 | `#ifdef _WIN32_WCE` |
|    - |  224 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|    - |  225 | `	SXUNUSED(zPath);` |
|    - |  226 | `	return 0;` |
|    - |  227 | `#else` |
|    - |  228 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|    - |  229 | `	void * pConverted;` |
|    - |  230 | `	WCHAR *p;` |
|    - |  231 | `	BOOL rc;` |
|  ! 0 |  232 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  233 | `	if( pConverted == 0 ){` |
|  ! 0 |  234 | `		return 0;` |
|    - |  235 | `	}` |
|  ! 0 |  236 | `	p = (WCHAR *)pConverted;` |
|  ! 0 |  237 | `	for(;*p;p++){` |
|  ! 0 |  238 | `		if( *p == '\\' \|\| *p == '/'){` |
|  ! 0 |  239 | `			*p = '\0';` |
|  ! 0 |  240 | `			break;` |
|    - |  241 | `		}` |
|  ! 0 |  242 | `	}` |
|  ! 0 |  243 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|  ! 0 |  244 | `	if( !rc ){` |
|  ! 0 |  245 | `		return 0;` |
|    - |  246 | `	}` |
|  ! 0 |  247 | `	return (ph7_int64)dwFreeClusters * dwSectPerClust * dwBytesPerSect;` |
|    - |  248 | `#endif` |
|  ! 0 |  249 |  |
|    - |  250 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - |  251 | `static ph7_int64 WinVfs_DiskTotalSpace(const char *zPath)` |
|  ! 0 |  252 |  |
|    - |  253 | `#ifdef _WIN32_WCE` |
|    - |  254 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|    - |  255 | `	SXUNUSED(zPath);` |
|    - |  256 | `	return 0;` |
|    - |  257 | `#else` |
|    - |  258 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|    - |  259 | `	void * pConverted;` |
|    - |  260 | `	WCHAR *p;` |
|    - |  261 | `	BOOL rc;` |
|  ! 0 |  262 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  263 | `	if( pConverted == 0 ){` |
|  ! 0 |  264 | `		return 0;` |
|    - |  265 | `	}` |
|  ! 0 |  266 | `	p = (WCHAR *)pConverted;` |
|  ! 0 |  267 | `	for(;*p;p++){` |
|  ! 0 |  268 | `		if( *p == '\\' \|\| *p == '/'){` |
|  ! 0 |  269 | `			*p = '\0';` |
|  ! 0 |  270 | `			break;` |
|    - |  271 | `		}` |
|  ! 0 |  272 | `	}` |
|  ! 0 |  273 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|  ! 0 |  274 | `	if( !rc ){` |
|  ! 0 |  275 | `		return 0;` |
|    - |  276 | `	}` |
|  ! 0 |  277 | `	return (ph7_int64)dwTotalClusters * dwSectPerClust * dwBytesPerSect;` |
|    - |  278 | `#endif` |
|  ! 0 |  279 |  |
|    - |  280 | `/* int (*xFileExists)(const char *) */` |
|    - |  281 | `static int WinVfs_FileExists(const char *zPath)` |
|    1 |  282 |  |
|    - |  283 | `	void * pConverted;` |
|    - |  284 | `	DWORD dwAttr;` |
|    1 |  285 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  286 | `	if( pConverted == 0 ){` |
|  ! 0 |  287 | `		return -1;` |
|    - |  288 | `	}` |
|    1 |  289 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  290 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  291 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  292 | `		return -1;` |
|    - |  293 | `	}` |
|    1 |  294 | `	return PH7_OK;` |
|    1 |  295 |  |
|    - |  296 | `/* Open a file in a read-only mode */` |
|    - |  297 | `static HANDLE OpenReadOnly(LPCWSTR pPath)` |
|    2 |  298 |  |
|    2 |  299 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    2 |  300 | `	DWORD dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    2 |  301 | `	DWORD dwAccess = GENERIC_READ;` |
|    2 |  302 | `	DWORD dwCreate = OPEN_EXISTING;` |
|    - |  303 | `	HANDLE pHandle;` |
|    2 |  304 | `	pHandle = CreateFileW(pPath,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    2 |  305 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    1 |  306 | `		return 0;` |
|    - |  307 | `	}` |
|    2 |  308 | `	return pHandle;` |
|    2 |  309 |  |
|    - |  310 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  311 | `static ph7_int64 WinVfs_FileSize(const char *zPath)` |
|    1 |  312 |  |
|    - |  313 | `	DWORD dwLow,dwHigh;` |
|    - |  314 | `	void * pConverted;` |
|    - |  315 | `	ph7_int64 nSize;` |
|    - |  316 | `	HANDLE pHandle;` |
|    - |  317 |  |
|    1 |  318 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  319 | `	if( pConverted == 0 ){` |
|  ! 0 |  320 | `		return -1;` |
|    - |  321 | `	}` |
|    - |  322 | `	/* Open the file in read-only mode */` |
|    1 |  323 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  324 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  325 | `	if( pHandle ){` |
|    1 |  326 | `		dwLow = GetFileSize(pHandle,&dwHigh);` |
|    1 |  327 | `		nSize = dwHigh;` |
|    1 |  328 | `		nSize <<= 32;` |
|    1 |  329 | `		nSize += dwLow;` |
|    1 |  330 | `		CloseHandle(pHandle);` |
|    1 |  331 | `	}else{` |
|  ! 0 |  332 | `		nSize = -1;` |
|    - |  333 | `	}` |
|    1 |  334 | `	return nSize;` |
|    1 |  335 |  |
|    - |  336 | `#define TICKS_PER_SECOND 10000000` |
|    - |  337 | `#define EPOCH_DIFFERENCE 11644473600LL` |
|    - |  338 | `/* Convert Windows timestamp to UNIX timestamp */` |
|    - |  339 | `static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)` |
|    1 |  340 |  |
|    - |  341 | `    ph7_int64 input,temp;` |
|    1 |  342 | `	input = pTime->dwHighDateTime;` |
|    1 |  343 | `	input <<= 32;` |
|    1 |  344 | `	input += pTime->dwLowDateTime;` |
|    1 |  345 | `    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/` |
|    1 |  346 | `    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/` |
|    1 |  347 | `    return temp;` |
|    1 |  348 |  |
|    - |  349 | `/* Convert UNIX timestamp to Windows timestamp */` |
|    - |  350 | `static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)` |
|  ! 0 |  351 |  |
|  ! 0 |  352 | `  ph7_int64 result = EPOCH_DIFFERENCE;` |
|  ! 0 |  353 | `  result += nUnixtime;` |
|  ! 0 |  354 | `  result *= 10000000LL;` |
|  ! 0 |  355 | `  pOut->dwHighDateTime = (DWORD)(nUnixtime>>32);` |
|  ! 0 |  356 | `  pOut->dwLowDateTime = (DWORD)nUnixtime;` |
|  ! 0 |  357 |  |
|    - |  358 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  359 | `static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|    1 |  360 |  |
|    - |  361 | `	FILETIME sTouch,sAccess;` |
|    - |  362 | `	void *pConverted;` |
|    - |  363 | `	void *pHandle;` |
|    1 |  364 | `	BOOL rc = 0;` |
|    1 |  365 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  366 | `	if( pConverted == 0 ){` |
|  ! 0 |  367 | `		return -1;` |
|    - |  368 | `	}` |
|    1 |  369 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  370 | `	if( pHandle ){` |
|    1 |  371 | `		if( touch_time < 0 ){` |
|    1 |  372 | `			GetSystemTimeAsFileTime(&sTouch);` |
|    1 |  373 | `		}else{` |
|  ! 0 |  374 | `			convertUnixTimeToWindowsTime(touch_time,&sTouch);` |
|    - |  375 | `		}` |
|    1 |  376 | `		if( access_time < 0 ){` |
|    - |  377 | `			/* Use the touch time */` |
|    1 |  378 | `			sAccess = sTouch; /* Structure assignment */` |
|    1 |  379 | `		}else{` |
|  ! 0 |  380 | `			convertUnixTimeToWindowsTime(access_time,&sAccess);` |
|    - |  381 | `		}` |
|    1 |  382 | `		rc = SetFileTime(pHandle,&sTouch,&sAccess,0);` |
|    - |  383 | `		/* Close the handle */` |
|    1 |  384 | `		CloseHandle(pHandle);` |
|    - |  385 | `	}` |
|    1 |  386 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  387 | `	return rc ? PH7_OK : -1;` |
|    1 |  388 |  |
|    - |  389 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  390 | `static ph7_int64 WinVfs_FileAtime(const char *zPath)` |
|    1 |  391 |  |
|    - |  392 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  393 | `	void * pConverted;` |
|    - |  394 | `	ph7_int64 atime;` |
|    - |  395 | `	HANDLE pHandle;` |
|    1 |  396 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  397 | `	if( pConverted == 0 ){` |
|  ! 0 |  398 | `		return -1;` |
|    - |  399 | `	}` |
|    - |  400 | `	/* Open the file in read-only mode */` |
|    1 |  401 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  402 | `	if( pHandle ){` |
|    - |  403 | `		BOOL rc;` |
|    1 |  404 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  405 | `		if( rc ){` |
|    1 |  406 | `			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);` |
|    1 |  407 | `		}else{` |
|  ! 0 |  408 | `			atime = -1;` |
|    - |  409 | `		}` |
|    1 |  410 | `		CloseHandle(pHandle);` |
|    1 |  411 | `	}else{` |
|  ! 0 |  412 | `		atime = -1;` |
|    - |  413 | `	}` |
|    1 |  414 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  415 | `	return atime;` |
|    1 |  416 |  |
|    - |  417 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  418 | `static ph7_int64 WinVfs_FileMtime(const char *zPath)` |
|    1 |  419 |  |
|    - |  420 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  421 | `	void * pConverted;` |
|    - |  422 | `	ph7_int64 mtime;` |
|    - |  423 | `	HANDLE pHandle;` |
|    1 |  424 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  425 | `	if( pConverted == 0 ){` |
|  ! 0 |  426 | `		return -1;` |
|    - |  427 | `	}` |
|    - |  428 | `	/* Open the file in read-only mode */` |
|    1 |  429 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  430 | `	if( pHandle ){` |
|    - |  431 | `		BOOL rc;` |
|    1 |  432 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  433 | `		if( rc ){` |
|    1 |  434 | `			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);` |
|    1 |  435 | `		}else{` |
|  ! 0 |  436 | `			mtime = -1;` |
|    - |  437 | `		}` |
|    1 |  438 | `		CloseHandle(pHandle);` |
|    1 |  439 | `	}else{` |
|  ! 0 |  440 | `		mtime = -1;` |
|    - |  441 | `	}` |
|    1 |  442 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  443 | `	return mtime;` |
|    1 |  444 |  |
|    - |  445 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  446 | `static ph7_int64 WinVfs_FileCtime(const char *zPath)` |
|    1 |  447 |  |
|    - |  448 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  449 | `	void * pConverted;` |
|    - |  450 | `	ph7_int64 ctime;` |
|    - |  451 | `	HANDLE pHandle;` |
|    1 |  452 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  453 | `	if( pConverted == 0 ){` |
|  ! 0 |  454 | `		return -1;` |
|    - |  455 | `	}` |
|    - |  456 | `	/* Open the file in read-only mode */` |
|    1 |  457 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  458 | `	if( pHandle ){` |
|    - |  459 | `		BOOL rc;` |
|    1 |  460 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  461 | `		if( rc ){` |
|    1 |  462 | `			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);` |
|    1 |  463 | `		}else{` |
|  ! 0 |  464 | `			ctime = -1;` |
|    - |  465 | `		}` |
|    1 |  466 | `		CloseHandle(pHandle);` |
|    1 |  467 | `	}else{` |
|  ! 0 |  468 | `		ctime = -1;` |
|    - |  469 | `	}` |
|    1 |  470 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  471 | `	return ctime;` |
|    1 |  472 |  |
|    - |  473 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  474 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  475 | `static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|    1 |  476 |  |
|    - |  477 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    - |  478 | `	void *pConverted;` |
|    - |  479 | `	HANDLE pHandle;` |
|    - |  480 | `	BOOL rc;` |
|    1 |  481 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  482 | `	if( pConverted == 0 ){` |
|  ! 0 |  483 | `		return -1;` |
|    - |  484 | `	}` |
|    - |  485 | `	/* Open the file in read-only mode */` |
|    1 |  486 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    1 |  487 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  488 | `	if( pHandle == 0 ){` |
|  ! 0 |  489 | `		return -1;` |
|    - |  490 | `	}` |
|    1 |  491 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 |  492 | `	CloseHandle(pHandle);` |
|    1 |  493 | `	if( !rc ){` |
|  ! 0 |  494 | `		return -1;` |
|    - |  495 | `	}` |
|    - |  496 | `	/* dev */` |
|    1 |  497 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 |  498 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - |  499 | `	/* ino */` |
|    1 |  500 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 |  501 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - |  502 | `	/* mode */` |
|    1 |  503 | `	ph7_value_int(pWorker,0);` |
|    1 |  504 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - |  505 | `	/* nlink */` |
|    1 |  506 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 |  507 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - |  508 | `	/* uid,gid,rdev */` |
|    1 |  509 | `	ph7_value_int(pWorker,0);` |
|    1 |  510 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 |  511 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 |  512 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - |  513 | `	/* size */` |
|    1 |  514 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 |  515 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - |  516 | `	/* atime */` |
|    1 |  517 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 |  518 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - |  519 | `	/* mtime */` |
|    1 |  520 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 |  521 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - |  522 | `	/* ctime */` |
|    1 |  523 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 |  524 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - |  525 | `	/* blksize,blocks */` |
|    1 |  526 | `	ph7_value_int(pWorker,0);` |
|    1 |  527 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 |  528 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 |  529 | `	return PH7_OK;` |
|    1 |  530 |  |
|    - |  531 | `/* int (*xIsfile)(const char *) */` |
|    - |  532 | `static int WinVfs_isfile(const char *zPath)` |
|    2 |  533 |  |
|    - |  534 | `	void * pConverted;` |
|    - |  535 | `	DWORD dwAttr;` |
|    2 |  536 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  537 | `	if( pConverted == 0 ){` |
|  ! 0 |  538 | `		return -1;` |
|    - |  539 | `	}` |
|    2 |  540 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    2 |  541 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  542 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    1 |  543 | `		return -1;` |
|    - |  544 | `	}` |
|    2 |  545 | `	return (dwAttr & (FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;` |
|    2 |  546 |  |
|    - |  547 | `/* int (*xIslink)(const char *) */` |
|    - |  548 | `static int WinVfs_islink(const char *zPath)` |
|  ! 0 |  549 |  |
|    - |  550 | `	void * pConverted;` |
|    - |  551 | `	DWORD dwAttr;` |
|  ! 0 |  552 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  553 | `	if( pConverted == 0 ){` |
|  ! 0 |  554 | `		return -1;` |
|    - |  555 | `	}` |
|  ! 0 |  556 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|  ! 0 |  557 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  558 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  559 | `		return -1;` |
|    - |  560 | `	}` |
|  ! 0 |  561 | `	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;` |
|  ! 0 |  562 |  |
|    - |  563 | `/* int (*xWritable)(const char *) */` |
|    - |  564 | `static int WinVfs_iswritable(const char *zPath)` |
|  ! 0 |  565 |  |
|    - |  566 | `	void * pConverted;` |
|    - |  567 | `	DWORD dwAttr;` |
|  ! 0 |  568 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  569 | `	if( pConverted == 0 ){` |
|  ! 0 |  570 | `		return -1;` |
|    - |  571 | `	}` |
|  ! 0 |  572 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|  ! 0 |  573 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  574 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  575 | `		return -1;` |
|    - |  576 | `	}` |
|  ! 0 |  577 | `	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE\|FILE_ATTRIBUTE_NORMAL)) == 0 ){` |
|    - |  578 | `		/* Not a regular file */` |
|  ! 0 |  579 | `		return -1;` |
|    - |  580 | `	}` |
|  ! 0 |  581 | `	if( dwAttr & FILE_ATTRIBUTE_READONLY ){` |
|    - |  582 | `		/* Read-only file */` |
|  ! 0 |  583 | `		return -1;` |
|    - |  584 | `	}` |
|    - |  585 | `	/* File is writable */` |
|  ! 0 |  586 | `	return PH7_OK;` |
|  ! 0 |  587 |  |
|    - |  588 | `/* int (*xExecutable)(const char *) */` |
|    - |  589 | `static int WinVfs_isexecutable(const char *zPath)` |
|  ! 0 |  590 |  |
|    - |  591 | `	void * pConverted;` |
|    - |  592 | `	DWORD dwAttr;` |
|  ! 0 |  593 | `	pConverted = convertUtf8Filename(zPath);` |
|  ! 0 |  594 | `	if( pConverted == 0 ){` |
|  ! 0 |  595 | `		return -1;` |
|    - |  596 | `	}` |
|  ! 0 |  597 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|  ! 0 |  598 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  599 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|  ! 0 |  600 | `		return -1;` |
|    - |  601 | `	}` |
|  ! 0 |  602 | `	if( (dwAttr & FILE_ATTRIBUTE_NORMAL) == 0 ){` |
|    - |  603 | `		/* Not a regular file */` |
|  ! 0 |  604 | `		return -1;` |
|    - |  605 | `	}` |
|    - |  606 | `	/* File is executable */` |
|  ! 0 |  607 | `	return PH7_OK;` |
|  ! 0 |  608 |  |
|    - |  609 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  610 | `static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|    1 |  611 |  |
|    - |  612 | `	void * pConverted;` |
|    - |  613 | `	DWORD dwAttr;` |
|    1 |  614 | `	pConverted = convertUtf8Filename(zPath);` |
|    1 |  615 | `	if( pConverted == 0 ){` |
|    - |  616 | `		/* Expand 'unknown' */` |
|  ! 0 |  617 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  618 | `		return -1;` |
|    - |  619 | `	}` |
|    1 |  620 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    1 |  621 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    1 |  622 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    - |  623 | `		/* Expand 'unknown' */` |
|  ! 0 |  624 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|  ! 0 |  625 | `		return -1;` |
|    - |  626 | `	}` |
|    1 |  627 | `	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE) ){` |
|    1 |  628 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|    1 |  629 | `	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){` |
|    1 |  630 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|  ! 0 |  631 | `	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){` |
|  ! 0 |  632 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|  ! 0 |  633 | `	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){` |
|  ! 0 |  634 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|  ! 0 |  635 | `	}else{` |
|  ! 0 |  636 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    - |  637 | `	}` |
|    1 |  638 | `	return PH7_OK;` |
|    1 |  639 |  |
|    - |  640 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  641 | `static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|    2 |  642 |  |
|    - |  643 | `	char zValue[1024];` |
|    - |  644 | `	DWORD n;` |
|    - |  645 | `	/*` |
|    - |  646 | `	 * According to MSDN` |
|    - |  647 | `	 * If lpBuffer is not large enough to hold the data, the return` |
|    - |  648 | `	 * value is the buffer size, in characters, required to hold the` |
|    - |  649 | `	 * string and its terminating null character and the contents` |
|    - |  650 | `	 * of lpBuffer are undefined.` |
|    - |  651 | `	 */` |
|    2 |  652 | `	n = sizeof(zValue);` |
|    2 |  653 | `	SyMemcpy("Undefined",zValue,sizeof("Undefined")-1);` |
|    - |  654 | `	/* Extract the environment value */` |
|    2 |  655 | `	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));` |
|    2 |  656 | `	if( !n ){` |
|    - |  657 | `		/* No such variable*/` |
|  ! 0 |  658 | `		return -1;` |
|    - |  659 | `	}` |
|    2 |  660 | `	ph7_result_string(pCtx,zValue,(int)n);` |
|    2 |  661 | `	return PH7_OK;` |
|    2 |  662 |  |
|    - |  663 | `/* int (*xSetenv)(const char *,const char *) */` |
|    - |  664 | `static int WinVfs_Setenv(const char *zName,const char *zValue)` |
|    1 |  665 |  |
|    - |  666 | `	BOOL rc;` |
|    1 |  667 | `	rc = SetEnvironmentVariableA(zName,zValue);` |
|    1 |  668 | `	return rc ? PH7_OK : -1;` |
|    1 |  669 |  |
|    - |  670 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  671 | `static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|    2 |  672 |  |
|    - |  673 | `	DWORD dwSizeLow,dwSizeHigh;` |
|    - |  674 | `	HANDLE pHandle,pMapHandle;` |
|    - |  675 | `	void *pConverted,*pView;` |
|    - |  676 |  |
|    2 |  677 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  678 | `	if( pConverted == 0 ){` |
|  ! 0 |  679 | `		return -1;` |
|    - |  680 | `	}` |
|    2 |  681 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|    2 |  682 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  683 | `	if( pHandle == 0 ){` |
|    1 |  684 | `		return -1;` |
|    - |  685 | `	}` |
|    - |  686 | `	/* Get the file size */` |
|    2 |  687 | `	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);` |
|    - |  688 | `	/* Create the mapping */` |
|    2 |  689 | `	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);` |
|    2 |  690 | `	if( pMapHandle == 0 ){` |
|  ! 0 |  691 | `		CloseHandle(pHandle);` |
|  ! 0 |  692 | `		return -1;` |
|    - |  693 | `	}` |
|    2 |  694 | `	*pSize = ((ph7_int64)dwSizeHigh << 32) \| dwSizeLow;` |
|    - |  695 | `	/* Obtain the view */` |
|    2 |  696 | `	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));` |
|    2 |  697 | `	if( pView ){` |
|    - |  698 | `		/* Let the upper layer point to the view */` |
|    2 |  699 | `		*ppMap = pView;` |
|    - |  700 | `	}` |
|    - |  701 | `	/* Close the handle` |
|    - |  702 | `	 * According to MSDN it's OK the close the HANDLES.` |
|    - |  703 | `	 */` |
|    2 |  704 | `	CloseHandle(pMapHandle);` |
|    2 |  705 | `	CloseHandle(pHandle);` |
|    2 |  706 | `	return pView ? PH7_OK : -1;` |
|    2 |  707 |  |
|    - |  708 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|    - |  709 | `static void WinVfs_Unmap(void *pView,ph7_int64 nSize)` |
|    2 |  710 |  |
|    2 |  711 | `	nSize = 0; /* Compiler warning */` |
|    2 |  712 | `	UnmapViewOfFile(pView);` |
|    2 |  713 |  |
|    - |  714 | `/* void (*xTempDir)(ph7_context *) */` |
|    - |  715 | `static void WinVfs_TempDir(ph7_context *pCtx)` |
|    2 |  716 |  |
|    - |  717 | `	CHAR zTemp[1024];` |
|    - |  718 | `	DWORD n;` |
|    2 |  719 | `	n = GetTempPathA(sizeof(zTemp),zTemp);` |
|    2 |  720 | `	if( n < 1 ){` |
|    - |  721 | `		/* Assume the default windows temp directory */` |
|  ! 0 |  722 | `		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);` |
|  ! 0 |  723 | `	}else{` |
|    2 |  724 | `		ph7_result_string(pCtx,zTemp,(int)n);` |
|    - |  725 | `	}` |
|    2 |  726 |  |
|    - |  727 | `/* unsigned int (*xProcessId)(void) */` |
|    - |  728 | `static unsigned int WinVfs_ProcessId(void)` |
|    1 |  729 |  |
|    1 |  730 | `	DWORD nID = 0;` |
|    - |  731 | `#ifndef __MINGW32__` |
|    1 |  732 | `	nID = GetProcessId(GetCurrentProcess());` |
|    - |  733 | `#endif /* __MINGW32__ */` |
|    1 |  734 | `	return (unsigned int)nID;` |
|    1 |  735 |  |
|    - |  736 | `/* void (*xUsername)(ph7_context *) */` |
|    - |  737 | `static void WinVfs_Username(ph7_context *pCtx)` |
|    1 |  738 |  |
|    - |  739 | `	WCHAR zUser[1024];` |
|    - |  740 | `	DWORD nByte;` |
|    - |  741 | `	BOOL rc;` |
|    1 |  742 | `	nByte = sizeof(zUser);` |
|    1 |  743 | `	rc = GetUserNameW(zUser,&nByte);` |
|    1 |  744 | `	if( !rc ){` |
|    - |  745 | `		/* Set a dummy name */` |
|  ! 0 |  746 | `		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  747 | `	}else{` |
|    - |  748 | `		char *zName;` |
|    1 |  749 | `		zName = unicodeToUtf8(zUser);` |
|    1 |  750 | `		if( zName == 0 ){` |
|  ! 0 |  751 | `			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|  ! 0 |  752 | `		}else{` |
|    1 |  753 | `			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|    1 |  754 | `			HeapFree(GetProcessHeap(),0,zName);` |
|    - |  755 | `		}` |
|    - |  756 | `	}` |
|    - |  757 |  |
|    1 |  758 |  |
|    - |  759 | `/* Export the windows vfs */` |
|    - |  760 | `PH7_PRIVATE const ph7_vfs sWinVfs = {` |
|    - |  761 | `	"Windows_vfs",` |
|    - |  762 | `	PH7_VFS_VERSION,` |
|    - |  763 | `	WinVfs_chdir,    /* int (*xChdir)(const char *) */` |
|    - |  764 |  |
|    - |  765 | `	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|    - |  766 | `	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|    - |  767 | `	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|    - |  768 | `	WinVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|    - |  769 | `	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|    - |  770 | `	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|    - |  771 | `	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */` |
|    - |  772 | `	WinVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|    - |  773 | `	WinVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|    - |  774 |  |
|    - |  775 |  |
|    - |  776 |  |
|    - |  777 | `	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    - |  778 | `	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    - |  779 | `	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|    - |  780 | `	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|    - |  781 | `	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|    - |  782 | `	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|    - |  783 | `	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  784 | `	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    - |  785 | `	WinVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|    - |  786 | `	WinVfs_islink,     /* int (*xIslink)(const char *) */` |
|    - |  787 | `	WinVfs_isfile,     /* int (*xReadable)(const char *) */` |
|    - |  788 | `	WinVfs_iswritable, /* int (*xWritable)(const char *) */` |
|    - |  789 | `	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */` |
|    - |  790 | `	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|    - |  791 | `	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|    - |  792 | `	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|    - |  793 | `	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    - |  794 | `	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|    - |  795 | `	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|    - |  796 |  |
|    - |  797 |  |
|    - |  798 | `	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|    - |  799 | `	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|    - |  800 |  |
|    - |  801 |  |
|    - |  802 | `	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|    - |  803 |  |
|    - |  804 | `};` |
|    - |  805 | `/* Windows file IO */` |
|    - |  806 | `#ifndef INVALID_SET_FILE_POINTER` |
|    - |  807 | `# define INVALID_SET_FILE_POINTER ((DWORD)-1)` |
|    - |  808 | `#endif` |
|    - |  809 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|    - |  810 | `static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|    2 |  811 |  |
|    2 |  812 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|    2 |  813 | `	DWORD dwAccess = GENERIC_READ;` |
|    - |  814 | `	DWORD dwShare,dwCreate;` |
|    - |  815 | `	void *pConverted;` |
|    - |  816 | `	HANDLE pHandle;` |
|    - |  817 |  |
|    2 |  818 | `	pConverted = convertUtf8Filename(zPath);` |
|    2 |  819 | `	if( pConverted == 0 ){` |
|  ! 0 |  820 | `		return -1;` |
|    - |  821 | `	}` |
|    - |  822 | `	/* Set the desired flags according to the open mode */` |
|    2 |  823 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|    - |  824 | `		/* Open existing file, or create if it doesn't exist */` |
|    2 |  825 | `		dwCreate = OPEN_ALWAYS;` |
|    2 |  826 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - |  827 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|    2 |  828 | `			dwCreate = CREATE_ALWAYS;` |
|    2 |  829 | `		}` |
|    2 |  830 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|    - |  831 | `		/* Creates a new file, only if it does not already exist.` |
|    - |  832 | `		* If the file exists, it fails.` |
|    - |  833 | `		*/` |
|  ! 0 |  834 | `		dwCreate = CREATE_NEW;` |
|    2 |  835 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|    - |  836 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|    - |  837 | `		 * The file must exist.` |
|    - |  838 | `		 */` |
|  ! 0 |  839 | `		dwCreate = TRUNCATE_EXISTING;` |
|  ! 0 |  840 | `	}else{` |
|    - |  841 | `		/* Opens a file, only if it exists. */` |
|    2 |  842 | `		dwCreate = OPEN_EXISTING;` |
|    - |  843 | `	}` |
|    2 |  844 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|    - |  845 | `		/* Read+Write access */` |
|    2 |  846 | `		dwAccess \|= GENERIC_WRITE;` |
|    2 |  847 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|    - |  848 | `		/* Write only access */` |
|    1 |  849 | `		dwAccess = GENERIC_WRITE;` |
|    - |  850 | `	}` |
|    2 |  851 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|    - |  852 | `		/* Append mode */` |
|  ! 0 |  853 | `		dwAccess = FILE_APPEND_DATA;` |
|    - |  854 | `	}` |
|    2 |  855 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|    - |  856 | `		/* File is temporary */` |
|  ! 0 |  857 | `		dwType = FILE_ATTRIBUTE_TEMPORARY;` |
|    - |  858 | `	}` |
|    2 |  859 | `	dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|    2 |  860 | `	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|    2 |  861 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    2 |  862 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|    - |  863 | `		SXUNUSED(pResource); /* MSVC warning */` |
|    1 |  864 | `		return -1;` |
|    - |  865 | `	}` |
|    - |  866 | `	/* Make the handle accessible to the upper layer */` |
|    2 |  867 | `	*ppHandle = (void *)pHandle;` |
|    2 |  868 | `	return PH7_OK;` |
|    2 |  869 |  |
|    - |  870 | `/* An instance of the following structure is used to record state information` |
|    - |  871 | ` * while iterating throw directory entries.` |
|    - |  872 | ` */` |
|    - |  873 | `typedef struct WinDir_Info WinDir_Info;` |
|    - |  874 | `struct WinDir_Info` |
|    - |  875 |  |
|    - |  876 | `	HANDLE pDirHandle;` |
|    - |  877 | `	void *pPath;` |
|    - |  878 | `	WIN32_FIND_DATAW sInfo;` |
|    - |  879 | `	int rc;` |
|    - |  880 | `};` |
|    - |  881 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|    - |  882 | `static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|    2 |  883 |  |
|    - |  884 | `	WinDir_Info *pDirInfo;` |
|    - |  885 | `	void *pConverted;` |
|    - |  886 | `	char *zPrep;` |
|    - |  887 | `	sxu32 n;` |
|    - |  888 | `	/* Prepare the path */` |
|    2 |  889 | `	n = SyStrlen(zPath);` |
|    2 |  890 | `	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);` |
|    2 |  891 | `	if( zPrep == 0 ){` |
|  ! 0 |  892 | `		return -1;` |
|    - |  893 | `	}` |
|    2 |  894 | `	SyMemcpy((const void *)zPath,zPrep,n);` |
|    2 |  895 | `	zPrep[n]   = '\\';` |
|    2 |  896 | `	zPrep[n+1] =  '*';` |
|    2 |  897 | `	zPrep[n+2] = 0;` |
|    2 |  898 | `	pConverted = convertUtf8Filename(zPrep);` |
|    2 |  899 | `	HeapFree(GetProcessHeap(),0,zPrep);` |
|    2 |  900 | `	if( pConverted == 0 ){` |
|  ! 0 |  901 | `		return -1;` |
|    - |  902 | `	}` |
|    - |  903 | `	/* Allocate a new instance */` |
|    2 |  904 | `	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));` |
|    2 |  905 | `	if( pDirInfo == 0 ){` |
|  ! 0 |  906 | `		pResource = 0; /* Compiler warning */` |
|  ! 0 |  907 | `		return -1;` |
|    - |  908 | `	}` |
|    2 |  909 | `	pDirInfo->rc = SXRET_OK;` |
|    2 |  910 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);` |
|    2 |  911 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|    - |  912 | `		/* Cannot open directory */` |
|  ! 0 |  913 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|  ! 0 |  914 | `		HeapFree(GetProcessHeap(),0,pDirInfo);` |
|  ! 0 |  915 | `		return -1;` |
|    - |  916 | `	}` |
|    - |  917 | `	/* Save the path */` |
|    2 |  918 | `	pDirInfo->pPath = pConverted;` |
|    - |  919 | `	/* Save our structure */` |
|    2 |  920 | `	*ppHandle = pDirInfo;` |
|    2 |  921 | `	return PH7_OK;` |
|    2 |  922 |  |
|    - |  923 | `/* void (*xCloseDir)(void *) */` |
|    - |  924 | `static void WinDir_Close(void *pUserData)` |
|    2 |  925 |  |
|    2 |  926 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    2 |  927 | `	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){` |
|    2 |  928 | `		FindClose(pDirInfo->pDirHandle);` |
|    - |  929 | `	}` |
|    2 |  930 | `	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);` |
|    2 |  931 | `	HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    2 |  932 |  |
|    - |  933 | `/* void (*xClose)(void *); */` |
|    - |  934 | `static void WinFile_Close(void *pUserData)` |
|    2 |  935 |  |
|    2 |  936 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    2 |  937 | `	CloseHandle(pHandle);` |
|    2 |  938 |  |
|    - |  939 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|    - |  940 | `static int WinDir_Read(void *pUserData,ph7_context *pCtx)` |
|    2 |  941 |  |
|    2 |  942 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    - |  943 | `	LPWIN32_FIND_DATAW pData;` |
|    - |  944 | `	char *zName;` |
|    - |  945 | `	BOOL rc;` |
|    - |  946 | `	sxu32 n;` |
|    2 |  947 | `	if( pDirInfo->rc != SXRET_OK ){` |
|    - |  948 | `		/* No more entry to process */` |
|    2 |  949 | `		return -1;` |
|    - |  950 | `	}` |
|    2 |  951 | `	pData = &pDirInfo->sInfo;` |
|    - |  952 | `	for(;;){` |
|    2 |  953 | `		zName = unicodeToUtf8(pData->cFileName);` |
|    2 |  954 | `		if( zName == 0 ){` |
|    - |  955 | `			/* Out of memory */` |
|  ! 0 |  956 | `			return -1;` |
|    - |  957 | `		}` |
|    2 |  958 | `		n = SyStrlen(zName);` |
|    - |  959 | `		/* Ignore '.' && '..' */` |
|    2 |  960 | `		if( n > sizeof("..")-1 \|\| zName[0] != '.' \|\| ( n == sizeof("..")-1 && zName[1] != '.') ){` |
|    2 |  961 | `			break;` |
|    - |  962 | `		}` |
|    2 |  963 | `		HeapFree(GetProcessHeap(),0,zName);` |
|    2 |  964 | `		rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|    2 |  965 | `		if( !rc ){` |
|  ! 0 |  966 | `			return -1;` |
|    - |  967 | `		}` |
|    2 |  968 | `	}` |
|    - |  969 | `	/* Return the current file name */` |
|    2 |  970 | `	ph7_result_string(pCtx,zName,-1);` |
|    2 |  971 | `	HeapFree(GetProcessHeap(),0,zName);` |
|    - |  972 | `	/* Point to the next entry */` |
|    2 |  973 | `	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|    2 |  974 | `	if( !rc ){` |
|    2 |  975 | `		pDirInfo->rc = SXERR_EOF;` |
|    - |  976 | `	}` |
|    2 |  977 | `	return PH7_OK;` |
|    2 |  978 |  |
|    - |  979 | `/* void (*xRewindDir)(void *) */` |
|    - |  980 | `static void WinDir_RewindDir(void *pUserData)` |
|    1 |  981 |  |
|    1 |  982 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|    1 |  983 | `	FindClose(pDirInfo->pDirHandle);` |
|    1 |  984 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);` |
|    1 |  985 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|  ! 0 |  986 | `		pDirInfo->rc = SXERR_EOF;` |
|  ! 0 |  987 | `	}else{` |
|    1 |  988 | `		pDirInfo->rc = SXRET_OK;` |
|    - |  989 | `	}` |
|    1 |  990 |  |
|    - |  991 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|    - |  992 | `static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)` |
|    2 |  993 |  |
|    2 |  994 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - |  995 | `	DWORD nRd;` |
|    - |  996 | `	BOOL rc;` |
|    2 |  997 | `	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    2 |  998 | `	if( !rc ){` |
|    - |  999 | `		/* EOF or IO error */` |
|  ! 0 | 1000 | `		return -1;` |
|    - | 1001 | `	}` |
|    2 | 1002 | `	return (ph7_int64)nRd;` |
|    2 | 1003 |  |
|    - | 1004 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|    - | 1005 | `static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)` |
|    2 | 1006 |  |
|    2 | 1007 | `	const char *zData = (const char *)pBuffer;` |
|    2 | 1008 | `	HANDLE pHandle = (HANDLE)pOS;` |
|    - | 1009 | `	ph7_int64 nCount;` |
|    - | 1010 | `	DWORD nWr;` |
|    - | 1011 | `	BOOL rc;` |
|    2 | 1012 | `	nWr = 0;` |
|    2 | 1013 | `	nCount = 0;` |
|    - | 1014 | `	for(;;){` |
|    2 | 1015 | `		if( nWrite < 1 ){` |
|    2 | 1016 | `			break;` |
|    - | 1017 | `		}` |
|    2 | 1018 | `		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);` |
|    2 | 1019 | `		if( !rc ){` |
|    - | 1020 | `			/* IO error */` |
|  ! 0 | 1021 | `			break;` |
|    - | 1022 | `		}` |
|    2 | 1023 | `		nWrite -= nWr;` |
|    2 | 1024 | `		nCount += nWr;` |
|    2 | 1025 | `		zData += nWr;` |
|    2 | 1026 | `	}` |
|    2 | 1027 | `	if( nWrite > 0 ){` |
|  ! 0 | 1028 | `		return -1;` |
|    - | 1029 | `	}` |
|    2 | 1030 | `	return nCount;` |
|    2 | 1031 |  |
|    - | 1032 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|    - | 1033 | `static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|    1 | 1034 |  |
|    1 | 1035 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1036 | `	DWORD dwMove,dwNew;` |
|    - | 1037 | `	LONG nHighOfft;` |
|    1 | 1038 | `	switch(whence){` |
|    - | 1039 | `	case 1:/*SEEK_CUR*/` |
|  ! 0 | 1040 | `		dwMove = FILE_CURRENT;` |
|  ! 0 | 1041 | `		break;` |
|    - | 1042 | `	case 2: /* SEEK_END */` |
|  ! 0 | 1043 | `		dwMove = FILE_END;` |
|  ! 0 | 1044 | `		break;` |
|    - | 1045 | `	case 0: /* SEEK_SET */` |
|    - | 1046 | `	default:` |
|    1 | 1047 | `		dwMove = FILE_BEGIN;` |
|    - | 1048 | `		break;` |
|    - | 1049 | `	}` |
|    1 | 1050 | `	nHighOfft = (LONG)(iOfft >> 32);` |
|    1 | 1051 | `	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);` |
|    1 | 1052 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1053 | `		return -1;` |
|    - | 1054 | `	}` |
|    1 | 1055 | `	return PH7_OK;` |
|    1 | 1056 |  |
|    - | 1057 | `/* int (*xLock)(void *,int) */` |
|    - | 1058 | `static int WinFile_Lock(void *pUserData,int lock_type)` |
|    1 | 1059 |  |
|    1 | 1060 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1061 | `	static DWORD dwLo = 0,dwHi = 0; /* xx: MT-SAFE */` |
|    - | 1062 | `	OVERLAPPED sDummy;` |
|    - | 1063 | `	BOOL rc;` |
|    1 | 1064 | `	SyZero(&sDummy,sizeof(sDummy));` |
|    - | 1065 | `	/* Get the file size */` |
|    1 | 1066 | `	if( lock_type < 1 ){` |
|    - | 1067 | `		/* Unlock the file */` |
|    1 | 1068 | `		rc = UnlockFileEx(pHandle,0,dwLo,dwHi,&sDummy);` |
|    1 | 1069 | `	}else{` |
|    1 | 1070 | `		DWORD dwFlags = LOCKFILE_FAIL_IMMEDIATELY; /* Shared non-blocking lock by default*/` |
|    - | 1071 | `		/* Lock the file */` |
|    1 | 1072 | `		if( lock_type == 1 /* LOCK_EXCL */ ){` |
|    1 | 1073 | `			dwFlags \|= LOCKFILE_EXCLUSIVE_LOCK;` |
|    - | 1074 | `		}` |
|    1 | 1075 | `		dwLo = GetFileSize(pHandle,&dwHi);` |
|    1 | 1076 | `		rc = LockFileEx(pHandle,dwFlags,0,dwLo,dwHi,&sDummy);` |
|    - | 1077 | `	}` |
|    1 | 1078 | `	return rc ? PH7_OK : -1 /* Lock error */;` |
|    1 | 1079 |  |
|    - | 1080 | `/* ph7_int64 (*xTell)(void *) */` |
|    - | 1081 | `static ph7_int64 WinFile_Tell(void *pUserData)` |
|    1 | 1082 |  |
|    1 | 1083 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1084 | `	DWORD dwNew;` |
|    1 | 1085 | `	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);` |
|    1 | 1086 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1087 | `		return -1;` |
|    - | 1088 | `	}` |
|    1 | 1089 | `	return (ph7_int64)dwNew;` |
|    1 | 1090 |  |
|    - | 1091 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|    - | 1092 | `static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|    1 | 1093 |  |
|    1 | 1094 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1095 | `	LONG HighOfft;` |
|    - | 1096 | `	DWORD dwNew;` |
|    - | 1097 | `	BOOL rc;` |
|    1 | 1098 | `	HighOfft = (LONG)(nOfft >> 32);` |
|    1 | 1099 | `	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);` |
|    1 | 1100 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|  ! 0 | 1101 | `		return -1;` |
|    - | 1102 | `	}` |
|    1 | 1103 | `	rc = SetEndOfFile(pHandle);` |
|    1 | 1104 | `	return rc ? PH7_OK : -1;` |
|    1 | 1105 |  |
|    - | 1106 | `/* int (*xSync)(void *); */` |
|    - | 1107 | `static int WinFile_Sync(void *pUserData)` |
|    1 | 1108 |  |
|    1 | 1109 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1110 | `	BOOL rc;` |
|    1 | 1111 | `	rc = FlushFileBuffers(pHandle);` |
|    1 | 1112 | `	return rc ? PH7_OK : - 1;` |
|    1 | 1113 |  |
|    - | 1114 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|    - | 1115 | `static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|    1 | 1116 |  |
|    - | 1117 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|    1 | 1118 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|    - | 1119 | `	BOOL rc;` |
|    1 | 1120 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|    1 | 1121 | `	if( !rc ){` |
|  ! 0 | 1122 | `		return -1;` |
|    - | 1123 | `	}` |
|    - | 1124 | `	/* dev */` |
|    1 | 1125 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|    1 | 1126 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|    - | 1127 | `	/* ino */` |
|    1 | 1128 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|    1 | 1129 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|    - | 1130 | `	/* mode */` |
|    1 | 1131 | `	ph7_value_int(pWorker,0);` |
|    1 | 1132 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|    - | 1133 | `	/* nlink */` |
|    1 | 1134 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|    1 | 1135 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|    - | 1136 | `	/* uid,gid,rdev */` |
|    1 | 1137 | `	ph7_value_int(pWorker,0);` |
|    1 | 1138 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    1 | 1139 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    1 | 1140 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|    - | 1141 | `	/* size */` |
|    1 | 1142 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|    1 | 1143 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|    - | 1144 | `	/* atime */` |
|    1 | 1145 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|    1 | 1146 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|    - | 1147 | `	/* mtime */` |
|    1 | 1148 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|    1 | 1149 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|    - | 1150 | `	/* ctime */` |
|    1 | 1151 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|    1 | 1152 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|    - | 1153 | `	/* blksize,blocks */` |
|    1 | 1154 | `	ph7_value_int(pWorker,0);` |
|    1 | 1155 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    1 | 1156 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    1 | 1157 | `	return PH7_OK;` |
|    1 | 1158 |  |
|    - | 1159 | `/* Export the file:// stream */` |
|    - | 1160 | `PH7_PRIVATE const ph7_io_stream sWinFileStream = {` |
|    - | 1161 | `	"file", /* Stream name */` |
|    - | 1162 | `	PH7_IO_STREAM_VERSION,` |
|    - | 1163 | `	WinFile_Open,  /* xOpen */` |
|    - | 1164 | `	WinDir_Open,   /* xOpenDir */` |
|    - | 1165 | `	WinFile_Close, /* xClose */` |
|    - | 1166 | `	WinDir_Close,  /* xCloseDir */` |
|    - | 1167 | `	WinFile_Read,  /* xRead */` |
|    - | 1168 | `	WinDir_Read,   /* xReadDir */` |
|    - | 1169 | `	WinFile_Write, /* xWrite */` |
|    - | 1170 | `	WinFile_Seek,  /* xSeek */` |
|    - | 1171 | `	WinFile_Lock,  /* xLock */` |
|    - | 1172 | `	WinDir_RewindDir, /* xRewindDir */` |
|    - | 1173 | `	WinFile_Tell,  /* xTell */` |
|    - | 1174 | `	WinFile_Trunc, /* xTrunc */` |
|    - | 1175 | `	WinFile_Sync,  /* xSeek */` |
|    - | 1176 | `	WinFile_Stat   /* xStat */` |
|    - | 1177 | `};` |
|    - | 1178 | `#endif /* __WINNT__ */` |
|    - | 1179 |  |
