# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2623/3905 lines (67.17%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <stdio.h>` |
|      - |    8 | `#include <errno.h>` |
|      - |    9 | `#include <string.h>` |
|      - |   10 |  |
|      - |   11 | `#ifdef __UNIXES__` |
|      - |   12 | `#include <unistd.h>` |
|      - |   13 | `#include <sys/wait.h>` |
|      - |   14 | `#include <fcntl.h>` |
|      - |   15 | `#include <signal.h>` |
|      - |   16 | `#endif` |
|      - |   17 | `/*` |
|      - |   18 | ` * This file implement a virtual file systems (VFS) for the PH7 engine.` |
|      - |   19 | ` */` |
|      - |   20 | `/*` |
|      - |   21 | ` * Given a string containing the path of a file or directory, this function` |
|      - |   22 | ` * return the parent directory's path.` |
|      - |   23 | ` */` |
|     62 |   24 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      5 |   25 | `{` |
|     67 |   26 | `	const char *zEnd = &zPath[nByte - 1];` |
|      - |   27 | `	int c,d;` |
|     67 |   28 | `	c = d = '/';` |
|      - |   29 | `#ifdef __WINNT__` |
|      5 |   30 | `	d = '\\';` |
|      - |   31 | `#endif` |
|   1396 |   32 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   1307 |   33 | `		zEnd--;` |
|      5 |   34 | `	}` |
|     67 |   35 | `	*pLen = (int)(zEnd-zPath);` |
|      - |   36 | `#ifdef __WINNT__` |
|      5 |   37 | `	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){` |
|      - |   38 | `		/* Normalize path on windows */` |
|    ! 0 |   39 | `		return "\\";` |
|      - |   40 | `	}` |
|      - |   41 | `#endif` |
|     67 |   42 | `	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){` |
|      - |   43 | `		/* No separator,return "." as the current directory */` |
|      8 |   44 | `		*pLen = sizeof(char);` |
|      8 |   45 | `		return ".";` |
|      - |   46 | `	}` |
|     61 |   47 | `	if( (*pLen) == 0 ){` |
|      2 |   48 | `		*pLen = sizeof(char);` |
|      - |   49 | `#ifdef __WINNT__` |
|    ! 0 |   50 | `		return "\\";` |
|      - |   51 | `#else` |
|      2 |   52 | `		return "/";` |
|      - |   53 | `#endif` |
|      - |   54 | `	}` |
|     59 |   55 | `	return zPath;` |
|     36 |   56 | `}` |
|      - |   57 | `/*` |
|      - |   58 | ` * Compile the VFS implementations when builtins are enabled OR when disk I/O` |
|      - |   59 | ` * is explicitly enabled (i.e. PH7_DISABLE_DISK_IO is NOT defined).` |
|      - |   60 | ` */` |
|      - |   61 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |   62 | `/*` |
|      - |   63 | ` * strerror() trips MSVC's C4996 "may be unsafe" deprecation under /WX. It is a` |
|      - |   64 | ` * standard C function we use deliberately to mirror php's IO error text; wrap it` |
|      - |   65 | ` * once with the deprecation suppressed. The pragma is _MSC_VER-guarded so the` |
|      - |   66 | ` * GCC/-Werror Linux build never sees an unknown-pragma warning.` |
|      - |   67 | ` */` |
|  20018 |   68 | `static const char * VfsStrerror(int iErr)` |
|      5 |   69 | `{` |
|      - |   70 | `#if defined(_MSC_VER)` |
|      - |   71 | `#pragma warning(push)` |
|      - |   72 | `#pragma warning(disable:4996)` |
|      - |   73 | `#endif` |
|  20023 |   74 | `	return strerror(iErr);` |
|      - |   75 | `#if defined(_MSC_VER)` |
|      - |   76 | `#pragma warning(pop)` |
|      - |   77 | `#endif` |
|      5 |   78 | `}` |
|      - |   79 | `/*` |
|      - |   80 | ` * php's non-open IO failures: "unlink(/nope): No such file or directory".` |
|      - |   81 | ` * PH7 returned FALSE in SILENCE for unlink/rmdir/mkdir/rename/chdir/opendir/scandir and` |
|      - |   82 | ` * filesize, so a script could not tell a failed operation from a successful one without` |
|      - |   83 | ` * checking the return value it never got told to check.` |
|      - |   84 | ` */` |
|  19996 |   85 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |   86 | `{` |
|  29999 |   87 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  19996 |   88 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  20001 |   89 | `}` |
|     12 |   90 | `static void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      3 |   91 | `{` |
|     21 |   92 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open stream: %s",` |
|     12 |   93 | `		ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));` |
|     15 |   94 | `}` |
|      - |   95 | `/*` |
|      - |   96 | ` * bool chdir(string $directory)` |
|      - |   97 | ` *  Change the current directory.` |
|      - |   98 | ` * Parameters` |
|      - |   99 | ` *  $directory` |
|      - |  100 | ` *   The new current directory` |
|      - |  101 | ` * Return` |
|      - |  102 | ` *  TRUE on success or FALSE on failure.` |
|      - |  103 | ` */` |
|  13454 |  104 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  105 | `{` |
|      - |  106 | `	const char *zPath;` |
|      - |  107 | `	ph7_vfs *pVfs;` |
|      - |  108 | `	int rc;` |
|  13459 |  109 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  110 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |  111 | `		ph7_result_bool(pCtx,0);` |
|      6 |  112 | `		return PH7_OK;` |
|      - |  113 | `	}` |
|      - |  114 | `	/* Point to the underlying vfs */` |
|  13455 |  115 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13455 |  116 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  117 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  118 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  119 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  120 | `			ph7_function_name(pCtx)` |
|      - |  121 | `			);` |
|    ! 0 |  122 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  123 | `		return PH7_OK;` |
|      - |  124 | `	}` |
|      - |  125 | `	/* Point to the desired directory */` |
|  13455 |  126 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  127 | `	/* Perform the requested operation */` |
|  13455 |  128 | `	errno = 0;` |
|  13455 |  129 | `	rc = pVfs->xChdir(zPath);` |
|  13455 |  130 | `	if( rc != PH7_OK ){` |
|      - |  131 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      4 |  132 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      2 |  133 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      1 |  134 | `	}` |
|      - |  135 | `	/* IO return value */` |
|  13455 |  136 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13455 |  137 | `	return PH7_OK;` |
|   6732 |  138 | `}` |
|      - |  139 | `/*` |
|      - |  140 | ` * bool chroot(string $directory)` |
|      - |  141 | ` *  Change the root directory.` |
|      - |  142 | ` * Parameters` |
|      - |  143 | ` *  $directory` |
|      - |  144 | ` *   The path to change the root directory to` |
|      - |  145 | ` * Return` |
|      - |  146 | ` *  TRUE on success or FALSE on failure.` |
|      - |  147 | ` */` |
|      6 |  148 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  149 | `{` |
|      - |  150 | `	const char *zPath;` |
|      - |  151 | `	ph7_vfs *pVfs;` |
|      - |  152 | `	int rc;` |
|      7 |  153 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  154 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 |  155 | `		ph7_result_bool(pCtx,0);` |
|      5 |  156 | `		return PH7_OK;` |
|      - |  157 | `	}` |
|      - |  158 | `	/* Point to the underlying vfs */` |
|      3 |  159 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  160 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  161 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  162 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  163 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  164 | `			ph7_function_name(pCtx)` |
|      - |  165 | `			);` |
|    ! 0 |  166 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  167 | `		return PH7_OK;` |
|      - |  168 | `	}` |
|      - |  169 | `	/* Point to the desired directory */` |
|      3 |  170 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  171 | `	/* Perform the requested operation */` |
|      3 |  172 | `	rc = pVfs->xChroot(zPath);` |
|      - |  173 | `	/* IO return value */` |
|      3 |  174 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 |  175 | `	return PH7_OK;` |
|      4 |  176 | `}` |
|      - |  177 | `/*` |
|      - |  178 | ` * string getcwd(void)` |
|      - |  179 | ` *  Gets the current working directory.` |
|      - |  180 | ` * Parameters` |
|      - |  181 | ` *  None` |
|      - |  182 | ` * Return` |
|      - |  183 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  184 | ` */` |
|     20 |  185 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  186 | `{` |
|      - |  187 | `	ph7_vfs *pVfs;` |
|      - |  188 | `	int rc;` |
|      - |  189 | `	/* Point to the underlying vfs */` |
|     25 |  190 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     25 |  191 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  192 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  193 | `		SXUNUSED(apArg);` |
|      - |  194 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  195 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  196 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  197 | `			ph7_function_name(pCtx)` |
|      - |  198 | `			);` |
|    ! 0 |  199 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  200 | `		return PH7_OK;` |
|      - |  201 | `	}` |
|     25 |  202 | `	ph7_result_string(pCtx,"",0);` |
|      - |  203 | `	/* Perform the requested operation */` |
|     25 |  204 | `	rc = pVfs->xGetcwd(pCtx);` |
|     25 |  205 | `	if( rc != PH7_OK ){` |
|      - |  206 | `		/* Error,return FALSE */` |
|    ! 0 |  207 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  208 | `	}` |
|     25 |  209 | `	return PH7_OK;` |
|     15 |  210 | `}` |
|      - |  211 | `/*` |
|      - |  212 | ` * bool rmdir(string $directory)` |
|      - |  213 | ` *  Removes directory.` |
|      - |  214 | ` * Parameters` |
|      - |  215 | ` *  $directory` |
|      - |  216 | ` *   The path to the directory` |
|      - |  217 | ` * Return` |
|      - |  218 | ` *  TRUE on success or FALSE on failure.` |
|      - |  219 | ` */` |
|     44 |  220 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  221 | `{` |
|      - |  222 | `	const char *zPath;` |
|      - |  223 | `	ph7_vfs *pVfs;` |
|      - |  224 | `	int rc;` |
|     46 |  225 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  226 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  227 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  228 | `		return PH7_OK;` |
|      - |  229 | `	}` |
|      - |  230 | `	/* Point to the underlying vfs */` |
|     46 |  231 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     46 |  232 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  233 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  234 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  235 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  236 | `			ph7_function_name(pCtx)` |
|      - |  237 | `			);` |
|    ! 0 |  238 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  239 | `		return PH7_OK;` |
|      - |  240 | `	}` |
|      - |  241 | `	/* Point to the desired directory */` |
|     46 |  242 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  243 | `	/* Perform the requested operation */` |
|     46 |  244 | `	errno = 0;` |
|     46 |  245 | `	rc = pVfs->xRmdir(zPath);` |
|     46 |  246 | `	if( rc != PH7_OK ){` |
|      8 |  247 | `		VfsThrowSysWarning(pCtx,zPath);` |
|      3 |  248 | `	}` |
|      - |  249 | `	/* IO return value */` |
|     46 |  250 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     46 |  251 | `	return PH7_OK;` |
|     24 |  252 | `}` |
|      - |  253 | `/*` |
|      - |  254 | ` * bool is_dir(string $filename)` |
|      - |  255 | ` *  Tells whether the given filename is a directory.` |
|      - |  256 | ` * Parameters` |
|      - |  257 | ` *  $filename` |
|      - |  258 | ` *   Path to the file.` |
|      - |  259 | ` * Return` |
|      - |  260 | ` *  TRUE on success or FALSE on failure.` |
|      - |  261 | ` */` |
|   8826 |  262 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  263 | `{` |
|      - |  264 | `	const char *zPath;` |
|      - |  265 | `	ph7_vfs *pVfs;` |
|      - |  266 | `	int rc;` |
|   8831 |  267 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  268 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  269 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  270 | `		return PH7_OK;` |
|      - |  271 | `	}` |
|      - |  272 | `	/* Point to the underlying vfs */` |
|   8831 |  273 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8831 |  274 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  275 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  276 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  277 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  278 | `			ph7_function_name(pCtx)` |
|      - |  279 | `			);` |
|    ! 0 |  280 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  281 | `		return PH7_OK;` |
|      - |  282 | `	}` |
|      - |  283 | `	/* Point to the desired directory */` |
|   8831 |  284 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  285 | `	/* Perform the requested operation */` |
|   8831 |  286 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  287 | `	/* IO return value */` |
|   8831 |  288 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8831 |  289 | `	return PH7_OK;` |
|   4418 |  290 | `}` |
|      - |  291 | `/*` |
|      - |  292 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  293 | ` *  Make a directory.` |
|      - |  294 | ` * Parameters` |
|      - |  295 | ` *  $pathname` |
|      - |  296 | ` *   The directory path.` |
|      - |  297 | ` * $mode` |
|      - |  298 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  299 | ` *  Note:` |
|      - |  300 | ` *   mode is ignored on Windows.` |
|      - |  301 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  302 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  303 | ` *   which you can change using umask().` |
|      - |  304 | ` * $recursive` |
|      - |  305 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  306 | ` *  Defaults to FALSE. (Not used)` |
|      - |  307 | ` * Return` |
|      - |  308 | ` *  TRUE on success or FALSE on failure.` |
|      - |  309 | ` */` |
|     44 |  310 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  311 | `{` |
|     46 |  312 | `	int iRecursive = 0;` |
|      - |  313 | `	const char *zPath;` |
|      - |  314 | `	ph7_vfs *pVfs;` |
|      - |  315 | `	int iMode,rc;` |
|     46 |  316 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  317 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  318 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  319 | `		return PH7_OK;` |
|      - |  320 | `	}` |
|      - |  321 | `	/* Point to the underlying vfs */` |
|     46 |  322 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     46 |  323 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  324 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  325 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  326 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  327 | `			ph7_function_name(pCtx)` |
|      - |  328 | `			);` |
|    ! 0 |  329 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  330 | `		return PH7_OK;` |
|      - |  331 | `	}` |
|      - |  332 | `	/* Point to the desired directory */` |
|     46 |  333 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  334 | `#ifdef __WINNT__` |
|      2 |  335 | `	iMode = 0;` |
|      - |  336 | `#else` |
|      - |  337 | `	/* Assume UNIX */` |
|     44 |  338 | `	iMode = 0777;` |
|      - |  339 | `#endif` |
|     46 |  340 | `	if( nArg > 1 ){` |
|    ! 0 |  341 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  342 | `		if( nArg > 2 ){` |
|    ! 0 |  343 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  344 | `		}` |
|    ! 0 |  345 | `	}` |
|      - |  346 | `	/* Perform the requested operation */` |
|     24 |  347 | `	errno = 0;` |
|     24 |  348 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|     24 |  349 | `	if( rc != PH7_OK ){` |
|      - |  350 | `		/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|    ! 0 |  351 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|    ! 0 |  352 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|    ! 0 |  353 | `	}` |
|      - |  354 | `	/* IO return value */` |
|     46 |  355 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     46 |  356 | `	return PH7_OK;` |
|     24 |  357 | `}` |
|      - |  358 | `/*` |
|      - |  359 | ` * bool rename(string $oldname,string $newname)` |
|      - |  360 | ` *  Attempts to rename oldname to newname.` |
|      - |  361 | ` * Parameters` |
|      - |  362 | ` *  $oldname` |
|      - |  363 | ` *   Old name.` |
|      - |  364 | ` *  $newname` |
|      - |  365 | ` *   New name.` |
|      - |  366 | ` * Return` |
|      - |  367 | ` *  TRUE on success or FALSE on failure.` |
|      - |  368 | ` */` |
|      2 |  369 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  370 | `{` |
|      - |  371 | `	const char *zOld,*zNew;` |
|      - |  372 | `	ph7_vfs *pVfs;` |
|      - |  373 | `	int rc;` |
|      3 |  374 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  375 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  376 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  377 | `		return PH7_OK;` |
|      - |  378 | `	}` |
|      - |  379 | `	/* Point to the underlying vfs */` |
|      3 |  380 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  381 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  382 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  383 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  384 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  385 | `			ph7_function_name(pCtx)` |
|      - |  386 | `			);` |
|    ! 0 |  387 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  388 | `		return PH7_OK;` |
|      - |  389 | `	}` |
|      - |  390 | `	/* Perform the requested operation */` |
|      3 |  391 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  392 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  393 | `	errno = 0;` |
|      3 |  394 | `	rc = pVfs->xRename(zOld,zNew);` |
|      3 |  395 | `	if( rc != PH7_OK ){` |
|      - |  396 | `		/* php names BOTH paths here */` |
|    ! 0 |  397 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    ! 0 |  398 | `			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));` |
|    ! 0 |  399 | `	}` |
|      - |  400 | `	/* IO result */` |
|      3 |  401 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  402 | `	return PH7_OK;` |
|      2 |  403 | `}` |
|      - |  404 | `/*` |
|      - |  405 | ` * string realpath(string $path)` |
|      - |  406 | ` *  Returns canonicalized absolute pathname.` |
|      - |  407 | ` * Parameters` |
|      - |  408 | ` *  $path` |
|      - |  409 | ` *   Target path.` |
|      - |  410 | ` * Return` |
|      - |  411 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  412 | ` */` |
|      6 |  413 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  414 | `{` |
|      - |  415 | `	const char *zPath;` |
|      - |  416 | `	ph7_vfs *pVfs;` |
|      - |  417 | `        int rc;` |
|      8 |  418 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  419 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  420 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  421 | `		return PH7_OK;` |
|      - |  422 | `	}` |
|      - |  423 | `	/* Point to the underlying vfs */` |
|      8 |  424 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 |  425 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  426 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  427 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  428 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  429 | `			ph7_function_name(pCtx)` |
|      - |  430 | `			);` |
|    ! 0 |  431 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  432 | `		return PH7_OK;` |
|      - |  433 | `	}` |
|      - |  434 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|      8 |  435 | `	ph7_result_string(pCtx,"",0);` |
|      - |  436 | `	/* Perform the requested operation */` |
|      8 |  437 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      8 |  438 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|      8 |  439 | `	if( rc != PH7_OK ){` |
|      2 |  440 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  441 | `	}` |
|      8 |  442 | `	return PH7_OK;` |
|      5 |  443 | `}` |
|      - |  444 | `/*` |
|      - |  445 | ` * int sleep(int $seconds)` |
|      - |  446 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  447 | ` * Parameters` |
|      - |  448 | ` *  $seconds` |
|      - |  449 | ` *   Halt time in seconds.` |
|      - |  450 | ` * Return` |
|      - |  451 | ` *  Zero on success or FALSE on failure.` |
|      - |  452 | ` */` |
|     10 |  453 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  454 | `{` |
|      - |  455 | `	ph7_vfs *pVfs;` |
|      - |  456 | `	int rc,nSleep;` |
|     11 |  457 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  458 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  459 | `		ph7_result_bool(pCtx,0);` |
|      3 |  460 | `		return PH7_OK;` |
|      - |  461 | `	}` |
|      - |  462 | `	/* Point to the underlying vfs */` |
|      9 |  463 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 |  464 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  465 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  466 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  467 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  468 | `			ph7_function_name(pCtx)` |
|      - |  469 | `			);` |
|    ! 0 |  470 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  471 | `		return PH7_OK;` |
|      - |  472 | `	}` |
|      - |  473 | `	/* Amount to sleep */` |
|      9 |  474 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|      9 |  475 | `	if( nSleep < 0 ){` |
|      - |  476 | `		/* Invalid value,return FALSE */` |
|      3 |  477 | `		ph7_result_bool(pCtx,0);` |
|      3 |  478 | `		return PH7_OK;` |
|      - |  479 | `	}` |
|      - |  480 | `	/* Perform the requested operation (Microseconds) */` |
|      7 |  481 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      7 |  482 | `	if( rc != PH7_OK ){` |
|      - |  483 | `		/* Return FALSE */` |
|    ! 0 |  484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  485 | `	}else{` |
|      - |  486 | `		/* Return zero */` |
|      7 |  487 | `		ph7_result_int(pCtx,0);` |
|      - |  488 | `	}` |
|      7 |  489 | `	return PH7_OK;` |
|      6 |  490 | `}` |
|      - |  491 | `/*` |
|      - |  492 | ` * void usleep(int $micro_seconds)` |
|      - |  493 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  494 | ` * Parameters` |
|      - |  495 | ` *  $micro_seconds` |
|      - |  496 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  497 | ` * Return` |
|      - |  498 | ` *  None.` |
|      - |  499 | ` */` |
|     56 |  500 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  501 | `{` |
|      - |  502 | `	ph7_vfs *pVfs;` |
|      - |  503 | `	int nSleep;` |
|     57 |  504 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  505 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  506 | `		return PH7_OK;` |
|      - |  507 | `	}` |
|      - |  508 | `	/* Point to the underlying vfs */` |
|     57 |  509 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     57 |  510 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  511 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  512 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  513 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  514 | `			ph7_function_name(pCtx)` |
|      - |  515 | `			);` |
|    ! 0 |  516 | `		return PH7_OK;` |
|      - |  517 | `	}` |
|      - |  518 | `	/* Amount to sleep */` |
|     57 |  519 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     57 |  520 | `	if( nSleep < 0 ){` |
|      - |  521 | `		/* Invalid value,return immediately */` |
|      3 |  522 | `		return PH7_OK;` |
|      - |  523 | `	}` |
|      - |  524 | `	/* Perform the requested operation (Microseconds) */` |
|     55 |  525 | `	pVfs->xSleep((unsigned int)nSleep);` |
|     55 |  526 | `	return PH7_OK;` |
|     29 |  527 | `}` |
|      - |  528 | `/*` |
|      - |  529 | ` * bool unlink (string $filename)` |
|      - |  530 | ` *  Delete a file.` |
|      - |  531 | ` * Parameters` |
|      - |  532 | ` *  $filename` |
|      - |  533 | ` *   Path to the file.` |
|      - |  534 | ` * Return` |
|      - |  535 | ` *  TRUE on success or FALSE on failure.` |
|      - |  536 | ` */` |
|  33720 |  537 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  538 | `{` |
|      - |  539 | `	const char *zPath;` |
|      - |  540 | `	ph7_vfs *pVfs;` |
|      - |  541 | `	int rc;` |
|  33725 |  542 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  543 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  544 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  545 | `		return PH7_OK;` |
|      - |  546 | `	}` |
|      - |  547 | `	/* Point to the underlying vfs */` |
|  33725 |  548 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  33725 |  549 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  550 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  551 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  552 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  553 | `			ph7_function_name(pCtx)` |
|      - |  554 | `			);` |
|    ! 0 |  555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  556 | `		return PH7_OK;` |
|      - |  557 | `	}` |
|      - |  558 | `	/* Point to the desired directory */` |
|  33725 |  559 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  560 | `	/* Perform the requested operation */` |
|  33725 |  561 | `	errno = 0;` |
|  33725 |  562 | `	rc = pVfs->xUnlink(zPath);` |
|  33725 |  563 | `	if( rc != PH7_OK ){` |
|  19995 |  564 | `		VfsThrowSysWarning(pCtx,zPath);` |
|   9995 |  565 | `	}` |
|      - |  566 | `	/* IO return value */` |
|  33725 |  567 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  33725 |  568 | `	return PH7_OK;` |
|  16865 |  569 | `}` |
|      - |  570 | `/*` |
|      - |  571 | ` * bool chmod(string $filename,int $mode)` |
|      - |  572 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - |  573 | ` * Parameters` |
|      - |  574 | ` *  $filename` |
|      - |  575 | ` *   Path to the file.` |
|      - |  576 | ` * $mode` |
|      - |  577 | ` *   Mode (Must be an integer)` |
|      - |  578 | ` * Return` |
|      - |  579 | ` *  TRUE on success or FALSE on failure.` |
|      - |  580 | ` */` |
|    148 |  581 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  582 | `{` |
|      - |  583 | `	const char *zPath;` |
|      - |  584 | `	ph7_vfs *pVfs;` |
|      - |  585 | `	int iMode;` |
|      - |  586 | `	int rc;` |
|    152 |  587 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  588 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  590 | `		return PH7_OK;` |
|      - |  591 | `	}` |
|      - |  592 | `	/* Point to the underlying vfs */` |
|    152 |  593 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    152 |  594 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  595 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  596 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  597 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  598 | `			ph7_function_name(pCtx)` |
|      - |  599 | `			);` |
|    ! 0 |  600 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  601 | `		return PH7_OK;` |
|      - |  602 | `	}` |
|      - |  603 | `	/* Point to the desired directory */` |
|    152 |  604 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  605 | `	/* Extract the mode */` |
|    152 |  606 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  607 | `	/* Perform the requested operation */` |
|    152 |  608 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  609 | `	/* IO return value */` |
|    152 |  610 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    152 |  611 | `	return PH7_OK;` |
|     78 |  612 | `}` |
|      - |  613 | `/*` |
|      - |  614 | ` * bool chown(string $filename,string $user)` |
|      - |  615 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - |  616 | ` * Parameters` |
|      - |  617 | ` *  $filename` |
|      - |  618 | ` *   Path to the file.` |
|      - |  619 | ` * $user` |
|      - |  620 | ` *   Username.` |
|      - |  621 | ` * Return` |
|      - |  622 | ` *  TRUE on success or FALSE on failure.` |
|      - |  623 | ` */` |
|      6 |  624 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  625 | `{` |
|      - |  626 | `	const char *zPath,*zUser;` |
|      - |  627 | `	ph7_vfs *pVfs;` |
|      - |  628 | `	int rc;` |
|      7 |  629 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  630 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  631 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  632 | `		return PH7_OK;` |
|      - |  633 | `	}` |
|      - |  634 | `	/* Point to the underlying vfs */` |
|      7 |  635 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  636 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - |  637 | `		/* IO routine not implemented,return NULL */` |
|      1 |  638 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  639 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  640 | `			ph7_function_name(pCtx)` |
|      - |  641 | `			);` |
|      1 |  642 | `		ph7_result_bool(pCtx,0);` |
|      1 |  643 | `		return PH7_OK;` |
|      - |  644 | `	}` |
|      - |  645 | `	/* Point to the desired directory */` |
|      6 |  646 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  647 | `	/* Extract the user */` |
|      6 |  648 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - |  649 | `	/* Perform the requested operation */` |
|      6 |  650 | `	errno = 0;` |
|      6 |  651 | `	rc = pVfs->xChown(zPath,zUser);` |
|      6 |  652 | `	if( rc != PH7_OK ){` |
|      - |  653 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  654 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  655 | `		 * "chown(): Operation not permitted". */` |
|      6 |  656 | `		if( rc == -2 ){` |
|      3 |  657 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",` |
|      1 |  658 | `				ph7_function_name(pCtx),zUser);` |
|      1 |  659 | `		}else{` |
|      6 |  660 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  661 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  662 | `		}` |
|      3 |  663 | `	}` |
|      - |  664 | `	/* IO return value */` |
|      6 |  665 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  666 | `	return PH7_OK;` |
|      4 |  667 | `}` |
|      - |  668 | `/*` |
|      - |  669 | ` * bool chgrp(string $filename,string $group)` |
|      - |  670 | ` *  Attempts to change the group of the file filename to group.` |
|      - |  671 | ` * Parameters` |
|      - |  672 | ` *  $filename` |
|      - |  673 | ` *   Path to the file.` |
|      - |  674 | ` * $group` |
|      - |  675 | ` *   groupname.` |
|      - |  676 | ` * Return` |
|      - |  677 | ` *  TRUE on success or FALSE on failure.` |
|      - |  678 | ` */` |
|      6 |  679 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  680 | `{` |
|      - |  681 | `	const char *zPath,*zGroup;` |
|      - |  682 | `	ph7_vfs *pVfs;` |
|      - |  683 | `	int rc;` |
|      7 |  684 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  685 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  686 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  687 | `		return PH7_OK;` |
|      - |  688 | `	}` |
|      - |  689 | `	/* Point to the underlying vfs */` |
|      7 |  690 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  691 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - |  692 | `		/* IO routine not implemented,return NULL */` |
|      1 |  693 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  694 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  695 | `			ph7_function_name(pCtx)` |
|      - |  696 | `			);` |
|      1 |  697 | `		ph7_result_bool(pCtx,0);` |
|      1 |  698 | `		return PH7_OK;` |
|      - |  699 | `	}` |
|      - |  700 | `	/* Point to the desired directory */` |
|      6 |  701 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  702 | `	/* Extract the user */` |
|      6 |  703 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - |  704 | `	/* Perform the requested operation */` |
|      6 |  705 | `	errno = 0;` |
|      6 |  706 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      6 |  707 | `	if( rc != PH7_OK ){` |
|      - |  708 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  709 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  710 | `		 * "chown(): Operation not permitted". */` |
|      6 |  711 | `		if( rc == -2 ){` |
|      3 |  712 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",` |
|      1 |  713 | `				ph7_function_name(pCtx),zGroup);` |
|      1 |  714 | `		}else{` |
|      6 |  715 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  716 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  717 | `		}` |
|      3 |  718 | `	}` |
|      - |  719 | `	/* IO return value */` |
|      6 |  720 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  721 | `	return PH7_OK;` |
|      4 |  722 | `}` |
|      - |  723 | `/*` |
|      - |  724 | ` * int64 disk_free_space(string $directory)` |
|      - |  725 | ` *  Returns available space on filesystem or disk partition.` |
|      - |  726 | ` * Parameters` |
|      - |  727 | ` *  $directory` |
|      - |  728 | ` *   A directory of the filesystem or disk partition.` |
|      - |  729 | ` * Return` |
|      - |  730 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  731 | ` */` |
|      4 |  732 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  733 | `{` |
|      - |  734 | `	const char *zPath;` |
|      - |  735 | `	ph7_int64 iSize;` |
|      - |  736 | `	ph7_vfs *pVfs;` |
|      5 |  737 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  738 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  739 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  740 | `		return PH7_OK;` |
|      - |  741 | `	}` |
|      - |  742 | `	/* Point to the underlying vfs */` |
|      5 |  743 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  744 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - |  745 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  746 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  747 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  748 | `			ph7_function_name(pCtx)` |
|      - |  749 | `			);` |
|    ! 0 |  750 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  751 | `		return PH7_OK;` |
|      - |  752 | `	}` |
|      - |  753 | `	/* Point to the desired directory */` |
|      5 |  754 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  755 | `	/* Perform the requested operation */` |
|      5 |  756 | `	iSize = pVfs->xFreeSpace(zPath);` |
|      - |  757 | `	/* IO return value */` |
|      5 |  758 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  759 | `	return PH7_OK;` |
|      3 |  760 | `}` |
|      - |  761 | `/*` |
|      - |  762 | ` * int64 disk_total_space(string $directory)` |
|      - |  763 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - |  764 | ` * Parameters` |
|      - |  765 | ` *  $directory` |
|      - |  766 | ` *   A directory of the filesystem or disk partition.` |
|      - |  767 | ` * Return` |
|      - |  768 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  769 | ` */` |
|      4 |  770 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  771 | `{` |
|      - |  772 | `	const char *zPath;` |
|      - |  773 | `	ph7_int64 iSize;` |
|      - |  774 | `	ph7_vfs *pVfs;` |
|      5 |  775 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  776 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  777 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  778 | `		return PH7_OK;` |
|      - |  779 | `	}` |
|      - |  780 | `	/* Point to the underlying vfs */` |
|      5 |  781 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  782 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - |  783 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  784 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  785 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  786 | `			ph7_function_name(pCtx)` |
|      - |  787 | `			);` |
|    ! 0 |  788 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  789 | `		return PH7_OK;` |
|      - |  790 | `	}` |
|      - |  791 | `	/* Point to the desired directory */` |
|      5 |  792 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  793 | `	/* Perform the requested operation */` |
|      5 |  794 | `	iSize = pVfs->xTotalSpace(zPath);` |
|      - |  795 | `	/* IO return value */` |
|      5 |  796 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  797 | `	return PH7_OK;` |
|      3 |  798 | `}` |
|      - |  799 | `/*` |
|      - |  800 | ` * bool file_exists(string $filename)` |
|      - |  801 | ` *  Checks whether a file or directory exists.` |
|      - |  802 | ` * Parameters` |
|      - |  803 | ` *  $filename` |
|      - |  804 | ` *   Path to the file.` |
|      - |  805 | ` * Return` |
|      - |  806 | ` *  TRUE on success or FALSE on failure.` |
|      - |  807 | ` */` |
|    182 |  808 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  809 | `{` |
|      - |  810 | `	const char *zPath;` |
|      - |  811 | `	ph7_vfs *pVfs;` |
|      - |  812 | `	int rc;` |
|    186 |  813 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  814 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  815 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  816 | `		return PH7_OK;` |
|      - |  817 | `	}` |
|      - |  818 | `	/* Point to the underlying vfs */` |
|    186 |  819 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    186 |  820 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  821 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  822 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  823 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  824 | `			ph7_function_name(pCtx)` |
|      - |  825 | `			);` |
|    ! 0 |  826 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  827 | `		return PH7_OK;` |
|      - |  828 | `	}` |
|      - |  829 | `	/* Point to the desired directory */` |
|    186 |  830 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  831 | `	/* Perform the requested operation */` |
|    186 |  832 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  833 | `	/* IO return value */` |
|    186 |  834 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    186 |  835 | `	return PH7_OK;` |
|     95 |  836 | `}` |
|      - |  837 | `/*` |
|      - |  838 | ` * int64 file_size(string $filename)` |
|      - |  839 | ` *  Gets the size for the given file.` |
|      - |  840 | ` * Parameters` |
|      - |  841 | ` *  $filename` |
|      - |  842 | ` *   Path to the file.` |
|      - |  843 | ` * Return` |
|      - |  844 | ` *  File size on success or FALSE on failure.` |
|      - |  845 | ` */` |
|     26 |  846 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  847 | `{` |
|      - |  848 | `	const char *zPath;` |
|      - |  849 | `	ph7_int64 iSize;` |
|      - |  850 | `	ph7_vfs *pVfs;` |
|     27 |  851 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  852 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  853 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  854 | `		return PH7_OK;` |
|      - |  855 | `	}` |
|      - |  856 | `	/* Point to the underlying vfs */` |
|     27 |  857 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     27 |  858 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - |  859 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  860 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  861 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  862 | `			ph7_function_name(pCtx)` |
|      - |  863 | `			);` |
|    ! 0 |  864 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  865 | `		return PH7_OK;` |
|      - |  866 | `	}` |
|      - |  867 | `	/* Point to the desired directory */` |
|     27 |  868 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  869 | `	/* Perform the requested operation */` |
|     27 |  870 | `	iSize = pVfs->xFileSize(zPath);` |
|     27 |  871 | `	if( iSize < 0 ){` |
|      - |  872 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - |  873 | `		 * truthy and compares equal to nothing a caller would test for. */` |
|    ! 0 |  874 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): stat failed for %s",` |
|    ! 0 |  875 | `			ph7_function_name(pCtx),zPath);` |
|    ! 0 |  876 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  877 | `		return PH7_OK;` |
|      - |  878 | `	}` |
|      - |  879 | `	/* IO return value */` |
|     27 |  880 | `	ph7_result_int64(pCtx,iSize);` |
|     27 |  881 | `	return PH7_OK;` |
|     14 |  882 | `}` |
|      - |  883 | `/*` |
|      - |  884 | ` * int64 fileatime(string $filename)` |
|      - |  885 | ` *  Gets the last access time of the given file.` |
|      - |  886 | ` * Parameters` |
|      - |  887 | ` *  $filename` |
|      - |  888 | ` *   Path to the file.` |
|      - |  889 | ` * Return` |
|      - |  890 | ` *  File atime on success or FALSE on failure.` |
|      - |  891 | ` */` |
|      2 |  892 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  893 | `{` |
|      - |  894 | `	const char *zPath;` |
|      - |  895 | `	ph7_int64 iTime;` |
|      - |  896 | `	ph7_vfs *pVfs;` |
|      3 |  897 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  898 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  899 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  900 | `		return PH7_OK;` |
|      - |  901 | `	}` |
|      - |  902 | `	/* Point to the underlying vfs */` |
|      3 |  903 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  904 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - |  905 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  906 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  907 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  908 | `			ph7_function_name(pCtx)` |
|      - |  909 | `			);` |
|    ! 0 |  910 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  911 | `		return PH7_OK;` |
|      - |  912 | `	}` |
|      - |  913 | `	/* Point to the desired directory */` |
|      3 |  914 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  915 | `	/* Perform the requested operation */` |
|      3 |  916 | `	iTime = pVfs->xFileAtime(zPath);` |
|      - |  917 | `	/* IO return value */` |
|      3 |  918 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  919 | `	return PH7_OK;` |
|      2 |  920 | `}` |
|      - |  921 | `/*` |
|      - |  922 | ` * int64 filemtime(string $filename)` |
|      - |  923 | ` *  Gets file modification time.` |
|      - |  924 | ` * Parameters` |
|      - |  925 | ` *  $filename` |
|      - |  926 | ` *   Path to the file.` |
|      - |  927 | ` * Return` |
|      - |  928 | ` *  File mtime on success or FALSE on failure.` |
|      - |  929 | ` */` |
|      4 |  930 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  931 | `{` |
|      - |  932 | `	const char *zPath;` |
|      - |  933 | `	ph7_int64 iTime;` |
|      - |  934 | `	ph7_vfs *pVfs;` |
|      5 |  935 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  936 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  937 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  938 | `		return PH7_OK;` |
|      - |  939 | `	}` |
|      - |  940 | `	/* Point to the underlying vfs */` |
|      5 |  941 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  942 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - |  943 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  944 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  945 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  946 | `			ph7_function_name(pCtx)` |
|      - |  947 | `			);` |
|    ! 0 |  948 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  949 | `		return PH7_OK;` |
|      - |  950 | `	}` |
|      - |  951 | `	/* Point to the desired directory */` |
|      5 |  952 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  953 | `	/* Perform the requested operation */` |
|      5 |  954 | `	iTime = pVfs->xFileMtime(zPath);` |
|      - |  955 | `	/* IO return value */` |
|      5 |  956 | `	ph7_result_int64(pCtx,iTime);` |
|      5 |  957 | `	return PH7_OK;` |
|      3 |  958 | `}` |
|      - |  959 | `/*` |
|      - |  960 | ` * int64 filectime(string $filename)` |
|      - |  961 | ` *  Gets inode change time of file.` |
|      - |  962 | ` * Parameters` |
|      - |  963 | ` *  $filename` |
|      - |  964 | ` *   Path to the file.` |
|      - |  965 | ` * Return` |
|      - |  966 | ` *  File ctime on success or FALSE on failure.` |
|      - |  967 | ` */` |
|      2 |  968 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  969 | `{` |
|      - |  970 | `	const char *zPath;` |
|      - |  971 | `	ph7_int64 iTime;` |
|      - |  972 | `	ph7_vfs *pVfs;` |
|      3 |  973 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  974 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  975 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  976 | `		return PH7_OK;` |
|      - |  977 | `	}` |
|      - |  978 | `	/* Point to the underlying vfs */` |
|      3 |  979 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  980 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - |  981 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  982 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  983 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  984 | `			ph7_function_name(pCtx)` |
|      - |  985 | `			);` |
|    ! 0 |  986 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  987 | `		return PH7_OK;` |
|      - |  988 | `	}` |
|      - |  989 | `	/* Point to the desired directory */` |
|      3 |  990 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  991 | `	/* Perform the requested operation */` |
|      3 |  992 | `	iTime = pVfs->xFileCtime(zPath);` |
|      - |  993 | `	/* IO return value */` |
|      3 |  994 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  995 | `	return PH7_OK;` |
|      2 |  996 | `}` |
|      - |  997 | `/*` |
|      - |  998 | ` * bool is_file(string $filename)` |
|      - |  999 | ` *  Tells whether the filename is a regular file.` |
|      - | 1000 | ` * Parameters` |
|      - | 1001 | ` *  $filename` |
|      - | 1002 | ` *   Path to the file.` |
|      - | 1003 | ` * Return` |
|      - | 1004 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1005 | ` */` |
|   6766 | 1006 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1007 | `{` |
|      - | 1008 | `	const char *zPath;` |
|      - | 1009 | `	ph7_vfs *pVfs;` |
|      - | 1010 | `	int rc;` |
|   6771 | 1011 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1012 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1013 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1014 | `		return PH7_OK;` |
|      - | 1015 | `	}` |
|      - | 1016 | `	/* Point to the underlying vfs */` |
|   6771 | 1017 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6771 | 1018 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1019 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1020 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1021 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1022 | `			ph7_function_name(pCtx)` |
|      - | 1023 | `			);` |
|    ! 0 | 1024 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1025 | `		return PH7_OK;` |
|      - | 1026 | `	}` |
|      - | 1027 | `	/* Point to the desired directory */` |
|   6771 | 1028 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1029 | `	/* Perform the requested operation */` |
|   6771 | 1030 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1031 | `	/* IO return value */` |
|   6771 | 1032 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6771 | 1033 | `	return PH7_OK;` |
|   3388 | 1034 | `}` |
|      - | 1035 | `/*` |
|      - | 1036 | ` * bool is_link(string $filename)` |
|      - | 1037 | ` *  Tells whether the filename is a symbolic link.` |
|      - | 1038 | ` * Parameters` |
|      - | 1039 | ` *  $filename` |
|      - | 1040 | ` *   Path to the file.` |
|      - | 1041 | ` * Return` |
|      - | 1042 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1043 | ` */` |
|      4 | 1044 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1045 | `{` |
|      - | 1046 | `	const char *zPath;` |
|      - | 1047 | `	ph7_vfs *pVfs;` |
|      - | 1048 | `	int rc;` |
|      4 | 1049 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1050 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1051 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1052 | `		return PH7_OK;` |
|      - | 1053 | `	}` |
|      - | 1054 | `	/* Point to the underlying vfs */` |
|      4 | 1055 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 | 1056 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - | 1057 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1058 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1059 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1060 | `			ph7_function_name(pCtx)` |
|      - | 1061 | `			);` |
|    ! 0 | 1062 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1063 | `		return PH7_OK;` |
|      - | 1064 | `	}` |
|      - | 1065 | `	/* Point to the desired directory */` |
|      4 | 1066 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1067 | `	/* Perform the requested operation */` |
|      4 | 1068 | `	rc = pVfs->xIslink(zPath);` |
|      - | 1069 | `	/* IO return value */` |
|      4 | 1070 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 1071 | `	return PH7_OK;` |
|      2 | 1072 | `}` |
|      - | 1073 | `/*` |
|      - | 1074 | ` * bool is_readable(string $filename)` |
|      - | 1075 | ` *  Tells whether a file exists and is readable.` |
|      - | 1076 | ` * Parameters` |
|      - | 1077 | ` *  $filename` |
|      - | 1078 | ` *   Path to the file.` |
|      - | 1079 | ` * Return` |
|      - | 1080 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1081 | ` */` |
|      2 | 1082 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1083 | `{` |
|      - | 1084 | `	const char *zPath;` |
|      - | 1085 | `	ph7_vfs *pVfs;` |
|      - | 1086 | `	int rc;` |
|      3 | 1087 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1088 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1089 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1090 | `		return PH7_OK;` |
|      - | 1091 | `	}` |
|      - | 1092 | `	/* Point to the underlying vfs */` |
|      3 | 1093 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1094 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - | 1095 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1096 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1097 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1098 | `			ph7_function_name(pCtx)` |
|      - | 1099 | `			);` |
|    ! 0 | 1100 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1101 | `		return PH7_OK;` |
|      - | 1102 | `	}` |
|      - | 1103 | `	/* Point to the desired directory */` |
|      3 | 1104 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1105 | `	/* Perform the requested operation */` |
|      3 | 1106 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1107 | `	/* IO return value */` |
|      3 | 1108 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1109 | `	return PH7_OK;` |
|      2 | 1110 | `}` |
|      - | 1111 | `/*` |
|      - | 1112 | ` * bool is_writable(string $filename)` |
|      - | 1113 | ` *  Tells whether the filename is writable.` |
|      - | 1114 | ` * Parameters` |
|      - | 1115 | ` *  $filename` |
|      - | 1116 | ` *   Path to the file.` |
|      - | 1117 | ` * Return` |
|      - | 1118 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1119 | ` */` |
|      4 | 1120 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1121 | `{` |
|      - | 1122 | `	const char *zPath;` |
|      - | 1123 | `	ph7_vfs *pVfs;` |
|      - | 1124 | `	int rc;` |
|      5 | 1125 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1126 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1127 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1128 | `		return PH7_OK;` |
|      - | 1129 | `	}` |
|      - | 1130 | `	/* Point to the underlying vfs */` |
|      5 | 1131 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1132 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1133 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1134 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1135 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1136 | `			ph7_function_name(pCtx)` |
|      - | 1137 | `			);` |
|    ! 0 | 1138 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1139 | `		return PH7_OK;` |
|      - | 1140 | `	}` |
|      - | 1141 | `	/* Point to the desired directory */` |
|      5 | 1142 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1143 | `	/* Perform the requested operation */` |
|      5 | 1144 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1145 | `	/* IO return value */` |
|      5 | 1146 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1147 | `	return PH7_OK;` |
|      3 | 1148 | `}` |
|      - | 1149 | `/*` |
|      - | 1150 | ` * bool is_executable(string $filename)` |
|      - | 1151 | ` *  Tells whether the filename is executable.` |
|      - | 1152 | ` * Parameters` |
|      - | 1153 | ` *  $filename` |
|      - | 1154 | ` *   Path to the file.` |
|      - | 1155 | ` * Return` |
|      - | 1156 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1157 | ` */` |
|      2 | 1158 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1159 | `{` |
|      - | 1160 | `	const char *zPath;` |
|      - | 1161 | `	ph7_vfs *pVfs;` |
|      - | 1162 | `	int rc;` |
|      3 | 1163 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1164 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1165 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1166 | `		return PH7_OK;` |
|      - | 1167 | `	}` |
|      - | 1168 | `	/* Point to the underlying vfs */` |
|      3 | 1169 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1170 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1171 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1172 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1173 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1174 | `			ph7_function_name(pCtx)` |
|      - | 1175 | `			);` |
|    ! 0 | 1176 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1177 | `		return PH7_OK;` |
|      - | 1178 | `	}` |
|      - | 1179 | `	/* Point to the desired directory */` |
|      3 | 1180 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1181 | `	/* Perform the requested operation */` |
|      3 | 1182 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1183 | `	/* IO return value */` |
|      3 | 1184 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1185 | `	return PH7_OK;` |
|      2 | 1186 | `}` |
|      - | 1187 | `/*` |
|      - | 1188 | ` * string filetype(string $filename)` |
|      - | 1189 | ` *  Gets file type.` |
|      - | 1190 | ` * Parameters` |
|      - | 1191 | ` *  $filename` |
|      - | 1192 | ` *   Path to the file.` |
|      - | 1193 | ` * Return` |
|      - | 1194 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1195 | ` *  file, socket and unknown.` |
|      - | 1196 | ` */` |
|      4 | 1197 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1198 | `{` |
|      - | 1199 | `	const char *zPath;` |
|      - | 1200 | `	ph7_vfs *pVfs;` |
|      5 | 1201 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1202 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1203 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1204 | `		return PH7_OK;` |
|      - | 1205 | `	}` |
|      - | 1206 | `	/* Point to the underlying vfs */` |
|      5 | 1207 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1208 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1209 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1210 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1211 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1212 | `			ph7_function_name(pCtx)` |
|      - | 1213 | `			);` |
|    ! 0 | 1214 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1215 | `		return PH7_OK;` |
|      - | 1216 | `	}` |
|      - | 1217 | `	/* Point to the desired directory */` |
|      5 | 1218 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1219 | `	/* Set the empty string as the default return value */` |
|      5 | 1220 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1221 | `	/* Perform the requested operation */` |
|      5 | 1222 | `	pVfs->xFiletype(zPath,pCtx);` |
|      5 | 1223 | `	return PH7_OK;` |
|      3 | 1224 | `}` |
|      - | 1225 | `/*` |
|      - | 1226 | ` * array stat(string $filename)` |
|      - | 1227 | ` *  Gives information about a file.` |
|      - | 1228 | ` * Parameters` |
|      - | 1229 | ` *  $filename` |
|      - | 1230 | ` *   Path to the file.` |
|      - | 1231 | ` * Return` |
|      - | 1232 | ` *  An associative array on success holding the following entries on success` |
|      - | 1233 | ` *  0   dev     device number` |
|      - | 1234 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1235 | ` * 2    mode    inode protection mode` |
|      - | 1236 | ` * 3    nlink   number of links` |
|      - | 1237 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1238 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1239 | ` * 6    rdev    device type, if inode device` |
|      - | 1240 | ` * 7    size    size in bytes` |
|      - | 1241 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1242 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1243 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1244 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1245 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1246 | ` * Note:` |
|      - | 1247 | ` *  FALSE is returned on failure.` |
|      - | 1248 | ` */` |
|     10 | 1249 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1250 | `{` |
|      - | 1251 | `	ph7_value *pArray,*pValue;` |
|      - | 1252 | `	const char *zPath;` |
|      - | 1253 | `	ph7_vfs *pVfs;` |
|      - | 1254 | `	int rc;` |
|     11 | 1255 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1256 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1257 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1258 | `		return PH7_OK;` |
|      - | 1259 | `	}` |
|      - | 1260 | `	/* Point to the underlying vfs */` |
|     11 | 1261 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 1262 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1263 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1264 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1265 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1266 | `			ph7_function_name(pCtx)` |
|      - | 1267 | `			);` |
|    ! 0 | 1268 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1269 | `		return PH7_OK;` |
|      - | 1270 | `	}` |
|      - | 1271 | `	/* Create the array and the working value */` |
|     11 | 1272 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 1273 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 1274 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1275 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1276 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1277 | `		return PH7_OK;` |
|      - | 1278 | `	}` |
|      - | 1279 | `	/* Extract the file path */` |
|     11 | 1280 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1281 | `	/* Perform the requested operation */` |
|     11 | 1282 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     11 | 1283 | `	if( rc != PH7_OK ){` |
|      - | 1284 | `		/* IO error,return FALSE */` |
|    ! 0 | 1285 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1286 | `	}else{` |
|      - | 1287 | `		/* Return the associative array */` |
|     11 | 1288 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1289 | `	}` |
|      - | 1290 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1291 | `	 * automatically as soon we return from this function. */` |
|     11 | 1292 | `	return PH7_OK;` |
|      6 | 1293 | `}` |
|      - | 1294 | `/*` |
|      - | 1295 | ` * array lstat(string $filename)` |
|      - | 1296 | ` *  Gives information about a file or symbolic link.` |
|      - | 1297 | ` * Parameters` |
|      - | 1298 | ` *  $filename` |
|      - | 1299 | ` *   Path to the file.` |
|      - | 1300 | ` * Return` |
|      - | 1301 | ` *  An associative array on success holding the following entries on success` |
|      - | 1302 | ` *  0   dev     device number` |
|      - | 1303 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1304 | ` * 2    mode    inode protection mode` |
|      - | 1305 | ` * 3    nlink   number of links` |
|      - | 1306 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1307 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1308 | ` * 6    rdev    device type, if inode device` |
|      - | 1309 | ` * 7    size    size in bytes` |
|      - | 1310 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1311 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1312 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1313 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1314 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1315 | ` * Note:` |
|      - | 1316 | ` *  FALSE is returned on failure.` |
|      - | 1317 | ` */` |
|      2 | 1318 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1319 | `{` |
|      - | 1320 | `	ph7_value *pArray,*pValue;` |
|      - | 1321 | `	const char *zPath;` |
|      - | 1322 | `	ph7_vfs *pVfs;` |
|      - | 1323 | `	int rc;` |
|      3 | 1324 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1325 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1326 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1327 | `		return PH7_OK;` |
|      - | 1328 | `	}` |
|      - | 1329 | `	/* Point to the underlying vfs */` |
|      3 | 1330 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1331 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1332 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1333 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1334 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1335 | `			ph7_function_name(pCtx)` |
|      - | 1336 | `			);` |
|    ! 0 | 1337 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1338 | `		return PH7_OK;` |
|      - | 1339 | `	}` |
|      - | 1340 | `	/* Create the array and the working value */` |
|      3 | 1341 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 1342 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1343 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1344 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1345 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1346 | `		return PH7_OK;` |
|      - | 1347 | `	}` |
|      - | 1348 | `	/* Extract the file path */` |
|      3 | 1349 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1350 | `	/* Perform the requested operation */` |
|      3 | 1351 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      3 | 1352 | `	if( rc != PH7_OK ){` |
|      - | 1353 | `		/* IO error,return FALSE */` |
|    ! 0 | 1354 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1355 | `	}else{` |
|      - | 1356 | `		/* Return the associative array */` |
|      3 | 1357 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1358 | `	}` |
|      - | 1359 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1360 | `	 * automatically as soon we return from this function. */` |
|      3 | 1361 | `	return PH7_OK;` |
|      2 | 1362 | `}` |
|      - | 1363 | `/*` |
|      - | 1364 | ` * string getenv(string $varname)` |
|      - | 1365 | ` *  Gets the value of an environment variable.` |
|      - | 1366 | ` * Parameters` |
|      - | 1367 | ` *  $varname` |
|      - | 1368 | ` *   The variable name.` |
|      - | 1369 | ` * Return` |
|      - | 1370 | ` *  Returns the value of the environment variable varname, or FALSE if the environment` |
|      - | 1371 | ` * variable varname does not exist.` |
|      - | 1372 | ` */` |
|     56 | 1373 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1374 | `{` |
|      - | 1375 | `	const char *zEnv;` |
|      - | 1376 | `	ph7_vfs *pVfs;` |
|      - | 1377 | `	int iLen;` |
|     60 | 1378 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1379 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1380 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1381 | `		return PH7_OK;` |
|      - | 1382 | `	}` |
|      - | 1383 | `	/* Point to the underlying vfs */` |
|     60 | 1384 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     60 | 1385 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1386 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1387 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1388 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1389 | `			ph7_function_name(pCtx)` |
|      - | 1390 | `			);` |
|    ! 0 | 1391 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1392 | `		return PH7_OK;` |
|      - | 1393 | `	}` |
|      - | 1394 | `	/* Extract the environment variable */` |
|     60 | 1395 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1396 | `	/* Set a boolean FALSE as the default return value */` |
|     60 | 1397 | `	ph7_result_bool(pCtx,0);` |
|     60 | 1398 | `	if( iLen < 1 ){` |
|      - | 1399 | `		/* Empty string */` |
|    ! 0 | 1400 | `		return PH7_OK;` |
|      - | 1401 | `	}` |
|      - | 1402 | `	/* Perform the requested operation */` |
|     60 | 1403 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     60 | 1404 | `	return PH7_OK;` |
|     32 | 1405 | `}` |
|      - | 1406 | `/*` |
|      - | 1407 | ` * bool putenv(string $settings)` |
|      - | 1408 | ` *  Set the value of an environment variable.` |
|      - | 1409 | ` * Parameters` |
|      - | 1410 | ` *  $setting` |
|      - | 1411 | ` *   The setting, like "FOO=BAR"` |
|      - | 1412 | ` * Return` |
|      - | 1413 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1414 | ` */` |
|      6 | 1415 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1416 | `{` |
|      - | 1417 | `	const char *zName,*zValue;` |
|      - | 1418 | `	char *zSettings,*zEnd;` |
|      - | 1419 | `	ph7_vfs *pVfs;` |
|      - | 1420 | `	int iLen,rc;` |
|      7 | 1421 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1422 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1423 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1424 | `		return PH7_OK;` |
|      - | 1425 | `	}` |
|      - | 1426 | `	/* Extract the setting variable */` |
|      7 | 1427 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|      7 | 1428 | `	if( iLen < 1 ){` |
|      - | 1429 | `		/* Empty string,return FALSE */` |
|    ! 0 | 1430 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1431 | `		return PH7_OK;` |
|      - | 1432 | `	}` |
|      - | 1433 | `	/* Parse the setting */` |
|      7 | 1434 | `	zEnd = &zSettings[iLen];` |
|      7 | 1435 | `	zValue = 0;` |
|      7 | 1436 | `	zName = zSettings;` |
|    127 | 1437 | `	while( zSettings < zEnd ){` |
|    127 | 1438 | `		if( zSettings[0] == '=' ){` |
|      - | 1439 | `			/* Null terminate the name */` |
|      7 | 1440 | `			zSettings[0] = 0;` |
|      7 | 1441 | `			zValue = &zSettings[1];` |
|      7 | 1442 | `			break;` |
|      - | 1443 | `		}` |
|    121 | 1444 | `		zSettings++;` |
|      1 | 1445 | `	}` |
|      - | 1446 | `	/* Install the environment variable in the $_Env array */` |
|      7 | 1447 | `	if( zValue == 0 \|\| zName[0] == 0 \|\| zValue >= zEnd \|\| zName >= zValue ){` |
|      - | 1448 | `		/* Invalid settings,retun FALSE */` |
|      5 | 1449 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1450 | `		if( zSettings  < zEnd ){` |
|      5 | 1451 | `			zSettings[0] = '=';` |
|      2 | 1452 | `		}` |
|      5 | 1453 | `		return PH7_OK;` |
|      - | 1454 | `	}` |
|      3 | 1455 | `	ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_ENV_ATTR,zName,zValue,(int)(zEnd-zValue));` |
|      - | 1456 | `	/* Point to the underlying vfs */` |
|      3 | 1457 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1458 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 1459 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1460 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1461 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1462 | `			ph7_function_name(pCtx)` |
|      - | 1463 | `			);` |
|    ! 0 | 1464 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1465 | `		zSettings[0] = '=';` |
|    ! 0 | 1466 | `		return PH7_OK;` |
|      - | 1467 | `	}` |
|      - | 1468 | `	/* Perform the requested operation */` |
|      3 | 1469 | `	rc = pVfs->xSetenv(zName,zValue);` |
|      3 | 1470 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 1471 | `	zSettings[0] = '=';` |
|      3 | 1472 | `	return PH7_OK;` |
|      4 | 1473 | `}` |
|      - | 1474 | `/*` |
|      - | 1475 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 1476 | ` *  Sets access and modification time of file.` |
|      - | 1477 | ` * Note: On windows` |
|      - | 1478 | ` *   If the file does not exists,it will not be created.` |
|      - | 1479 | ` * Parameters` |
|      - | 1480 | ` *  $filename` |
|      - | 1481 | ` *   The name of the file being touched.` |
|      - | 1482 | ` *  $time` |
|      - | 1483 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 1484 | ` * $atime` |
|      - | 1485 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 1486 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 1487 | ` *   present, the current system time is used.` |
|      - | 1488 | ` * Return` |
|      - | 1489 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1490 | `*/` |
|      4 | 1491 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1492 | `{` |
|      - | 1493 | `	ph7_int64 nTime,nAccess;` |
|      - | 1494 | `	const char *zFile;` |
|      - | 1495 | `	ph7_vfs *pVfs;` |
|      - | 1496 | `	int rc;` |
|      5 | 1497 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1498 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1499 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1500 | `		return PH7_OK;` |
|      - | 1501 | `	}` |
|      - | 1502 | `	/* Point to the underlying vfs */` |
|      5 | 1503 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1504 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 1505 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1506 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1507 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1508 | `			ph7_function_name(pCtx)` |
|      - | 1509 | `			);` |
|    ! 0 | 1510 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1511 | `		return PH7_OK;` |
|      - | 1512 | `	}` |
|      - | 1513 | `	/* Perform the requested operation */` |
|      5 | 1514 | `	nTime = nAccess = -1;` |
|      5 | 1515 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|      5 | 1516 | `	if( nArg > 1 ){` |
|      2 | 1517 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      2 | 1518 | `		if( nArg > 2 ){` |
|      2 | 1519 | `			nAccess = ph7_value_to_int64(apArg[1]);` |
|      1 | 1520 | `		}else{` |
|    ! 0 | 1521 | `			nAccess = nTime;` |
|      - | 1522 | `		}` |
|      1 | 1523 | `	}` |
|      5 | 1524 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 1525 | `	/* IO result */` |
|      5 | 1526 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1527 | `	return PH7_OK;` |
|      3 | 1528 | `}` |
|      - | 1529 | `/*` |
|      - | 1530 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 1531 | ` * Status:` |
|      - | 1532 | ` *    Stable.` |
|      - | 1533 | ` */` |
|      - | 1534 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1535 | `/*` |
|      - | 1536 | ` * string dirname(string $path)` |
|      - | 1537 |  |
|      - | 1538 | ` *  Returns parent directory's path.` |
|      - | 1539 | ` * Parameters` |
|      - | 1540 | ` * $path` |
|      - | 1541 | ` *  Target path.` |
|      - | 1542 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1543 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1544 | ` * Return` |
|      - | 1545 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 1546 | ` *  is returned, indicating the current directory.` |
|      - | 1547 | ` */` |
|     20 | 1548 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1549 | `{` |
|      - | 1550 | `	const char *zPath,*zDir;` |
|      - | 1551 | `	int iLen,iDirlen;` |
|     25 | 1552 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1553 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 1554 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1555 | `		return PH7_OK;` |
|      - | 1556 | `	}` |
|      - | 1557 | `	/* Point to the target path */` |
|     25 | 1558 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     25 | 1559 | `	if( iLen < 1 ){` |
|      - | 1560 | `		/* Reuturn "." */` |
|      2 | 1561 | `		ph7_result_string(pCtx,".",sizeof(char));` |
|      2 | 1562 | `		return PH7_OK;` |
|      - | 1563 | `	}` |
|      - | 1564 | `	/* Perform the requested operation */` |
|     23 | 1565 | `	zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1566 | `	/* Return directory name */` |
|     23 | 1567 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     23 | 1568 | `	return PH7_OK;` |
|     15 | 1569 | `}` |
|      - | 1570 | `/*` |
|      - | 1571 | ` * string basename(string $path[, string $suffix ])` |
|      - | 1572 | ` *  Returns trailing name component of path.` |
|      - | 1573 | ` * Parameters` |
|      - | 1574 | ` * $path` |
|      - | 1575 | ` *  Target path.` |
|      - | 1576 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1577 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1578 | ` * $suffix` |
|      - | 1579 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 1580 | ` * Return` |
|      - | 1581 | ` *  The base name of the given path.` |
|      - | 1582 | ` */` |
|     46 | 1583 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1584 | `{` |
|      - | 1585 | `	const char *zPath,*zBase,*zEnd;` |
|      - | 1586 | `	int c,d,iLen;` |
|     47 | 1587 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1588 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1589 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1590 | `		return PH7_OK;` |
|      - | 1591 | `	}` |
|     47 | 1592 | `	c = d = '/';` |
|      - | 1593 | `#ifdef __WINNT__` |
|      1 | 1594 | `	d = '\\';` |
|      - | 1595 | `#endif` |
|      - | 1596 | `	/* Point to the target path */` |
|     47 | 1597 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     47 | 1598 | `	if( iLen < 1 ){` |
|      - | 1599 | `		/* Empty string */` |
|      3 | 1600 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1601 | `		return PH7_OK;` |
|      - | 1602 | `	}` |
|      - | 1603 | `	/* Perform the requested operation */` |
|     45 | 1604 | `	zEnd = &zPath[iLen - 1];` |
|      - | 1605 | `	/* Ignore trailing '/' */` |
|     69 | 1606 | `	while( zEnd > zPath && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      5 | 1607 | `		zEnd--;` |
|      1 | 1608 | `	}` |
|     45 | 1609 | `	if( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ){` |
|      - | 1610 | `		/* Nothing but separators ("/", "///"): php answers the EMPTY string, where the` |
|      - | 1611 | `		 * strip loop above stops one short and left PH7 returning "/". */` |
|      5 | 1612 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1613 | `		return PH7_OK;` |
|      - | 1614 | `	}` |
|     41 | 1615 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    973 | 1616 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    915 | 1617 | `		zEnd--;` |
|      1 | 1618 | `	}` |
|     41 | 1619 | `	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     41 | 1620 | `	zEnd = &zPath[iLen];` |
|     41 | 1621 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1622 | `		const char *zSuffix;` |
|      - | 1623 | `		int nSuffix;` |
|      - | 1624 | `		/* Strip suffix */` |
|      5 | 1625 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      5 | 1626 | `		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1627 | `			zEnd -= nSuffix;` |
|      2 | 1628 | `		}` |
|      2 | 1629 | `	}` |
|      - | 1630 | `	/* Store the basename */` |
|     41 | 1631 | `	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));` |
|     41 | 1632 | `	return PH7_OK;` |
|     24 | 1633 | `}` |
|      - | 1634 | `/*` |
|      - | 1635 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1636 | ` *  Returns information about a file path.` |
|      - | 1637 | ` * Parameter` |
|      - | 1638 | ` *  $path` |
|      - | 1639 | ` *   The path to be parsed.` |
|      - | 1640 | ` *  $options` |
|      - | 1641 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 1642 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 1643 | ` * Return` |
|      - | 1644 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 1645 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 1646 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 1647 | ` */` |
|      - | 1648 | `typedef struct path_info path_info;` |
|      - | 1649 | `struct path_info` |
|      - | 1650 | `{` |
|      - | 1651 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1652 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1653 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1654 | `	SyString sFilename;  /* Filename */` |
|      - | 1655 | `};` |
|      - | 1656 | `/*` |
|      - | 1657 | ` * Extract path fields.` |
|      - | 1658 | ` */` |
|  13410 | 1659 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1660 | `{` |
|  13415 | 1661 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1662 | `	SyString *pCur;` |
|      - | 1663 | `	int c,d;` |
|  13415 | 1664 | `	c = d = '/';` |
|      - | 1665 | `#ifdef __WINNT__` |
|      5 | 1666 | `	d = '\\';` |
|      - | 1667 | `#endif` |
|      - | 1668 | `	/* Zero the structure */` |
|  13415 | 1669 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1670 | `	/* Handle special case */` |
|  13415 | 1671 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1672 | `#ifdef __WINNT__` |
|    ! 0 | 1673 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1674 | `#else` |
|    ! 0 | 1675 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1676 | `#endif` |
|    ! 0 | 1677 | `		return SXRET_OK;` |
|      - | 1678 | `	}` |
|      - | 1679 | `	/* Extract the basename */` |
| 361908 | 1680 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 341793 | 1681 | `		zEnd--;` |
|      5 | 1682 | `	}` |
|  13415 | 1683 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13415 | 1684 | `	zEnd = &zPath[nByte];` |
|      - | 1685 | `	/* dirname */` |
|  13415 | 1686 | `	pCur = &pOut->sDir;` |
|  13415 | 1687 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13415 | 1688 | `	if( pCur->nByte > 1 ){` |
|  26825 | 1689 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1690 | `#ifdef __WINNT__` |
|      5 | 1691 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1692 | `#endif` |
|   6710 | 1693 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1694 | `#ifdef __WINNT__` |
|    ! 0 | 1695 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1696 | `#else` |
|    ! 0 | 1697 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1698 | `#endif` |
|    ! 0 | 1699 | `	}` |
|      - | 1700 | `	/* basename/filename */` |
|  13415 | 1701 | `	pCur = &pOut->sBasename;` |
|  13415 | 1702 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13415 | 1703 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1704 | `#ifdef __WINNT__` |
|      5 | 1705 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1706 | `#endif` |
|  13415 | 1707 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13415 | 1708 | `	if( pCur->nByte > 0 ){` |
|      - | 1709 | `		/* extension */` |
|  13415 | 1710 | `		zEnd--;` |
|  67049 | 1711 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  53639 | 1712 | `			zEnd--;` |
|      5 | 1713 | `		}` |
|  13415 | 1714 | `		if( zEnd > pCur->zString ){` |
|  13413 | 1715 | `			zEnd++; /* Jump leading dot */` |
|  13413 | 1716 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1717 | `			/* Fix filename */` |
|  13413 | 1718 | `			pCur = &pOut->sFilename;` |
|  13413 | 1719 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13413 | 1720 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6704 | 1721 | `			}` |
|   6704 | 1722 | `		}` |
|   6705 | 1723 | `	}` |
|  13415 | 1724 | `	return SXRET_OK;` |
|   6710 | 1725 | `}` |
|      - | 1726 | `/*` |
|      - | 1727 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1728 | ` *  See block comment above.` |
|      - | 1729 | ` */` |
|  13410 | 1730 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1731 | `{` |
|      - | 1732 | `	const char *zPath;` |
|      - | 1733 | `	path_info sInfo;` |
|      - | 1734 | `	SyString *pComp;` |
|      - | 1735 | `	int iLen;` |
|  13415 | 1736 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1737 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1738 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1739 | `		return PH7_OK;` |
|      - | 1740 | `	}` |
|      - | 1741 | `	/* Point to the target path */` |
|  13415 | 1742 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13415 | 1743 | `	if( iLen < 1 ){` |
|      - | 1744 | `		/* Empty string */` |
|    ! 0 | 1745 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1746 | `		return PH7_OK;` |
|      - | 1747 | `	}` |
|      - | 1748 | `	/* Extract path info */` |
|  13415 | 1749 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  20119 | 1750 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1751 | `		/* Return path component */` |
|  13413 | 1752 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13413 | 1753 | `		switch(nComp){` |
|      1 | 1754 | `		case 1: /* PATHINFO_DIRNAME */` |
|      3 | 1755 | `			pComp = &sInfo.sDir;` |
|      3 | 1756 | `			if( pComp->nByte > 0 ){` |
|      3 | 1757 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1758 | `			}else{` |
|      - | 1759 | `				/* Expand the empty string */` |
|    ! 0 | 1760 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1761 | `			}` |
|      3 | 1762 | `			break;` |
|      1 | 1763 | `		case 2: /*PATHINFO_BASENAME*/` |
|      3 | 1764 | `			pComp = &sInfo.sBasename;` |
|      3 | 1765 | `			if( pComp->nByte > 0 ){` |
|      3 | 1766 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1767 | `			}else{` |
|      - | 1768 | `				/* Expand the empty string */` |
|    ! 0 | 1769 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1770 | `			}` |
|      3 | 1771 | `			break;` |
|   3353 | 1772 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6711 | 1773 | `			pComp = &sInfo.sExtension;` |
|   6711 | 1774 | `			if( pComp->nByte > 0 ){` |
|   6709 | 1775 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3357 | 1776 | `			}else{` |
|      - | 1777 | `				/* Expand the empty string */` |
|      3 | 1778 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1779 | `			}` |
|   6711 | 1780 | `			break;` |
|   3349 | 1781 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6703 | 1782 | `			pComp = &sInfo.sFilename;` |
|   6703 | 1783 | `			if( pComp->nByte > 0 ){` |
|   6703 | 1784 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3354 | 1785 | `			}else{` |
|      - | 1786 | `				/* Expand the empty string */` |
|    ! 0 | 1787 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1788 | `			}` |
|   6703 | 1789 | `			break;` |
|    ! 0 | 1790 | `		default:` |
|      - | 1791 | `			/* Expand the empty string */` |
|    ! 0 | 1792 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1793 | `			break;` |
|      - | 1794 | `		}` |
|   6709 | 1795 | `	}else{` |
|      - | 1796 | `		/* Return an associative array */` |
|      - | 1797 | `		ph7_value *pArray,*pValue;` |
|      3 | 1798 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 1799 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1800 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1801 | `			/* Out of mem,return NULL */` |
|    ! 0 | 1802 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1803 | `			return PH7_OK;` |
|      - | 1804 | `		}` |
|      - | 1805 | `		/* dirname */` |
|      3 | 1806 | `		pComp = &sInfo.sDir;` |
|      3 | 1807 | `		if( pComp->nByte > 0 ){` |
|      3 | 1808 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1809 | `			/* Perform the insertion */` |
|      3 | 1810 | `			ph7_array_add_strkey_elem(pArray,"dirname",pValue); /* Will make it's own copy */` |
|      1 | 1811 | `		}` |
|      - | 1812 | `		/* Reset the string cursor */` |
|      3 | 1813 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1814 | `		/* basername */` |
|      3 | 1815 | `		pComp = &sInfo.sBasename;` |
|      3 | 1816 | `		if( pComp->nByte > 0 ){` |
|      3 | 1817 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1818 | `			/* Perform the insertion */` |
|      3 | 1819 | `			ph7_array_add_strkey_elem(pArray,"basename",pValue); /* Will make it's own copy */` |
|      1 | 1820 | `		}` |
|      - | 1821 | `		/* Reset the string cursor */` |
|      3 | 1822 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1823 | `		/* extension */` |
|      3 | 1824 | `		pComp = &sInfo.sExtension;` |
|      3 | 1825 | `		if( pComp->nByte > 0 ){` |
|      3 | 1826 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1827 | `			/* Perform the insertion */` |
|      3 | 1828 | `			ph7_array_add_strkey_elem(pArray,"extension",pValue); /* Will make it's own copy */` |
|      1 | 1829 | `		}` |
|      - | 1830 | `		/* Reset the string cursor */` |
|      3 | 1831 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1832 | `		/* filename */` |
|      3 | 1833 | `		pComp = &sInfo.sFilename;` |
|      3 | 1834 | `		if( pComp->nByte > 0 ){` |
|      3 | 1835 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1836 | `			/* Perform the insertion */` |
|      3 | 1837 | `			ph7_array_add_strkey_elem(pArray,"filename",pValue); /* Will make it's own copy */` |
|      1 | 1838 | `		}` |
|      - | 1839 | `		/* Return the created array */` |
|      3 | 1840 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1841 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 1842 | `		 * automatically as soon we return from this foreign function.` |
|      - | 1843 | `		 */` |
|      - | 1844 | `	}` |
|  13415 | 1845 | `	return PH7_OK;` |
|   6710 | 1846 | `}` |
|      - | 1847 | `/* SPDX-SnippetBegin */` |
|      - | 1848 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 1849 | `/* SPDX-License-Identifier: blessing */` |
|      - | 1850 | `/*` |
|      - | 1851 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 1852 |  |
|      - | 1853 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 1854 | ` * Status: Public Domain` |
|      - | 1855 | ` */` |
|      - | 1856 | `typedef unsigned char u8;` |
|      - | 1857 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 1858 | `** lower-case character.` |
|      - | 1859 | `**` |
|      - | 1860 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 1861 | `** handle case conversions for the UTF character set since the tables` |
|      - | 1862 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 1863 | `*/` |
|      - | 1864 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 1865 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 1866 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 1867 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 1868 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 1869 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 1870 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 1871 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 1872 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 1873 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 1874 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 1875 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 1876 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 1877 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 1878 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 1879 | `    252,253,254,255` |
|      - | 1880 | `};` |
|      - | 1881 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 1882 | `/*` |
|      - | 1883 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 1884 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 1885 | `*/` |
|      - | 1886 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 1887 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 1888 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 1889 | `  }                                                    \` |
|      - | 1890 | `}` |
|      - | 1891 | `/*` |
|      - | 1892 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 1893 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 1894 | `** are the same and false (0) if they are different.` |
|      - | 1895 | `**` |
|      - | 1896 | `** Globbing rules:` |
|      - | 1897 | `**` |
|      - | 1898 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 1899 | `**` |
|      - | 1900 | `**      '?'       Matches exactly one character.` |
|      - | 1901 | `**` |
|      - | 1902 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 1903 | `**                characters.` |
|      - | 1904 | `**` |
|      - | 1905 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 1906 | `**` |
|      - | 1907 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 1908 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 1909 | `** range of characters can be specified using '-'.  Example:` |
|      - | 1910 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 1911 | `** it the last character in the list.` |
|      - | 1912 | `**` |
|      - | 1913 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 1914 | `**` |
|      - | 1915 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 1916 | `**` |
|      - | 1917 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 1918 | `*/` |
|     44 | 1919 | `static int patternCompare(` |
|      - | 1920 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1921 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1922 | `  const int esc,                    /* The escape character */` |
|      - | 1923 | `  int noCase` |
|      1 | 1924 | `){` |
|      - | 1925 | `  int c, c2;` |
|      - | 1926 | `  int invert;` |
|      - | 1927 | `  int seen;` |
|     45 | 1928 | `  u8 matchOne = '?';` |
|     45 | 1929 | `  u8 matchAll = '*';` |
|     45 | 1930 | `  u8 matchSet = '[';` |
|     45 | 1931 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1932 |  |
|     45 | 1933 | `  if( !zPattern \|\| !zString ) return 0;` |
|     81 | 1934 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     73 | 1935 | `    if( !prevEscape && c==matchAll ){` |
|     52 | 1936 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|     27 | 1937 | `               \|\| c == matchOne ){` |
|    ! 0 | 1938 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1939 | `          return 0;` |
|      - | 1940 | `        }` |
|    ! 0 | 1941 | `      }` |
|     27 | 1942 | `      if( c==0 ){` |
|     19 | 1943 | `        return 1;` |
|      9 | 1944 | `      }else if( c==esc ){` |
|    ! 0 | 1945 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1946 | `        if( c==0 ){` |
|    ! 0 | 1947 | `          return 0;` |
|    ! 0 | 1948 | `        }` |
|      9 | 1949 | `      }else if( c==matchSet ){` |
|    ! 0 | 1950 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 1951 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 1952 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 1953 | `        }` |
|    ! 0 | 1954 | `        return *zString!=0;` |
|      - | 1955 | `      }` |
|     11 | 1956 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     11 | 1957 | `        if( noCase ){` |
|      3 | 1958 | `          GlogUpperToLower(c2);` |
|      3 | 1959 | `          GlogUpperToLower(c);` |
|     11 | 1960 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 1961 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 1962 | `            GlogUpperToLower(c2);` |
|      1 | 1963 | `          }` |
|      2 | 1964 | `        }else{` |
|     47 | 1965 | `          while( c2 != 0 && c2 != c ){` |
|     39 | 1966 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 1967 | `          }` |
|      - | 1968 | `        }` |
|     11 | 1969 | `        if( c2==0 ) return 0;` |
|      9 | 1970 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 1971 | `      }` |
|    ! 0 | 1972 | `      return 0;` |
|     47 | 1973 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 1974 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1975 | `        return 0;` |
|    ! 0 | 1976 | `      }` |
|     47 | 1977 | `    }else if( c==matchSet ){` |
|    ! 0 | 1978 | `      int prior_c = 0;` |
|    ! 0 | 1979 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 1980 | `      seen = 0;` |
|    ! 0 | 1981 | `      invert = 0;` |
|    ! 0 | 1982 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 1983 | `      if( c==0 ) return 0;` |
|    ! 0 | 1984 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1985 | `      if( c2=='^' ){` |
|    ! 0 | 1986 | `        invert = 1;` |
|    ! 0 | 1987 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1988 | `      }` |
|    ! 0 | 1989 | `      if( c2==']' ){` |
|    ! 0 | 1990 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 1991 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1992 | `      }` |
|    ! 0 | 1993 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 1994 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 1995 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1996 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 1997 | `          prior_c = 0;` |
|    ! 0 | 1998 | `        }else{` |
|    ! 0 | 1999 | `          if( c==c2 ){` |
|    ! 0 | 2000 | `            seen = 1;` |
|    ! 0 | 2001 | `          }` |
|    ! 0 | 2002 | `          prior_c = c2;` |
|      - | 2003 | `        }` |
|    ! 0 | 2004 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2005 | `      }` |
|    ! 0 | 2006 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 2007 | `        return 0;` |
|    ! 0 | 2008 | `      }` |
|     47 | 2009 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 2010 | `      prevEscape = 1;` |
|    ! 0 | 2011 | `    }else{` |
|     27 | 2012 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     27 | 2013 | `      if( noCase ){` |
|      7 | 2014 | `        GlogUpperToLower(c);` |
|      7 | 2015 | `        GlogUpperToLower(c2);` |
|      3 | 2016 | `      }` |
|     47 | 2017 | `      if( c!=c2 ){` |
|     11 | 2018 | `        return 0;` |
|      - | 2019 | `      }` |
|     37 | 2020 | `      prevEscape = 0;` |
|      - | 2021 | `    }` |
|      1 | 2022 | `  }` |
|      9 | 2023 | `  return *zString==0;` |
|     23 | 2024 | `}` |
|      - | 2025 | `/* SPDX-SnippetEnd */` |
|      - | 2026 | `/*` |
|      - | 2027 | ` * Wrapper around patternCompare() defined above.` |
|      - | 2028 | ` * See block comment above for more information.` |
|      - | 2029 | ` */` |
|     36 | 2030 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 2031 | `{` |
|      - | 2032 | `	int rc;` |
|     37 | 2033 | `	if( iEsc < 0 ){` |
|    ! 0 | 2034 | `		iEsc = '\\';` |
|    ! 0 | 2035 | `	}` |
|     37 | 2036 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     37 | 2037 | `	return rc;` |
|      1 | 2038 | `}` |
|      - | 2039 | `/*` |
|      - | 2040 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 2041 | ` *  Match filename against a pattern.` |
|      - | 2042 | ` * Parameters` |
|      - | 2043 | ` *  $pattern` |
|      - | 2044 | ` *   The shell wildcard pattern.` |
|      - | 2045 | ` * $string` |
|      - | 2046 | ` *  The tested string.` |
|      - | 2047 | ` * $flags` |
|      - | 2048 | ` *   A list of possible flags:` |
|      - | 2049 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 2050 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 2051 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 2052 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 2053 | ` * Return` |
|      - | 2054 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2055 | ` */` |
|      8 | 2056 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2057 | `{` |
|      - | 2058 | `	const char *zString,*zPattern;` |
|      9 | 2059 | `	int iEsc = '\\';` |
|      9 | 2060 | `	int noCase = 0;` |
|      - | 2061 | `	int rc;` |
|      9 | 2062 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2063 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2064 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2065 | `		return PH7_OK;` |
|      - | 2066 | `	}` |
|      - | 2067 | `	/* Extract the pattern and the string */` |
|      9 | 2068 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 2069 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2070 | `	/* Extract the flags if avaialble */` |
|      9 | 2071 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 2072 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 2073 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|    ! 0 | 2074 | `			iEsc = 0;` |
|    ! 0 | 2075 | `		}` |
|      5 | 2076 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
|      3 | 2077 | `			noCase = 1;` |
|      1 | 2078 | `		}` |
|      3 | 2079 | `	}` |
|      - | 2080 | `	/* Go globbing */` |
|      9 | 2081 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 2082 | `	/* Globbing result */` |
|      9 | 2083 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 2084 | `	return PH7_OK;` |
|      5 | 2085 | `}` |
|      - | 2086 | `/*` |
|      - | 2087 | ` * bool strglob(string $pattern,string $string)` |
|      - | 2088 | ` *  Match string against a pattern.` |
|      - | 2089 | ` * Parameters` |
|      - | 2090 | ` *  $pattern` |
|      - | 2091 | ` *   The shell wildcard pattern.` |
|      - | 2092 | ` * $string` |
|      - | 2093 | ` *  The tested string.` |
|      - | 2094 | ` * Return` |
|      - | 2095 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2096 | ` * Note that this a symisc eXtension.` |
|      - | 2097 | ` */` |
|     28 | 2098 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2099 | `{` |
|      - | 2100 | `	const char *zString,*zPattern;` |
|     29 | 2101 | `	int iEsc = '\\';` |
|      - | 2102 | `	int rc;` |
|     29 | 2103 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2104 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2105 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2106 | `		return PH7_OK;` |
|      - | 2107 | `	}` |
|      - | 2108 | `	/* Extract the pattern and the string */` |
|     29 | 2109 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|     29 | 2110 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2111 | `	/* Go globbing */` |
|     29 | 2112 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2113 | `	/* Globbing result */` |
|     29 | 2114 | `	ph7_result_bool(pCtx,rc);` |
|     29 | 2115 | `	return PH7_OK;` |
|     15 | 2116 | `}` |
|      - | 2117 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2118 | `/*` |
|      - | 2119 | ` * bool link(string $target,string $link)` |
|      - | 2120 |  |
|      - | 2121 | ` *  Create a hard link.` |
|      - | 2122 | ` * Parameters` |
|      - | 2123 | ` *  $target` |
|      - | 2124 | ` *   Target of the link.` |
|      - | 2125 | ` *  $link` |
|      - | 2126 | ` *   The link name.` |
|      - | 2127 | ` * Return` |
|      - | 2128 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2129 | ` */` |
|      2 | 2130 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2131 | `{` |
|      - | 2132 | `	const char *zTarget,*zLink;` |
|      - | 2133 | `	ph7_vfs *pVfs;` |
|      - | 2134 | `	int rc;` |
|      3 | 2135 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2136 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2137 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2138 | `		return PH7_OK;` |
|      - | 2139 | `	}` |
|      - | 2140 | `	/* Point to the underlying vfs */` |
|      3 | 2141 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2142 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2143 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2144 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2145 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2146 | `			ph7_function_name(pCtx)` |
|      - | 2147 | `			);` |
|    ! 0 | 2148 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2149 | `		return PH7_OK;` |
|      - | 2150 | `	}` |
|      - | 2151 | `	/* Extract the given arguments */` |
|      3 | 2152 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      3 | 2153 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2154 | `	/* Perform the requested operation */` |
|      3 | 2155 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2156 | `	/* IO result */` |
|      3 | 2157 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 2158 | `	return PH7_OK;` |
|      2 | 2159 | `}` |
|      - | 2160 | `/*` |
|      - | 2161 | ` * bool symlink(string $target,string $link)` |
|      - | 2162 | ` *  Creates a symbolic link.` |
|      - | 2163 | ` * Parameters` |
|      - | 2164 | ` *  $target` |
|      - | 2165 | ` *   Target of the link.` |
|      - | 2166 | ` *  $link` |
|      - | 2167 | ` *   The link name.` |
|      - | 2168 | ` * Return` |
|      - | 2169 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2170 | ` */` |
|      6 | 2171 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2172 | `{` |
|      - | 2173 | `	const char *zTarget,*zLink;` |
|      - | 2174 | `	ph7_vfs *pVfs;` |
|      - | 2175 | `	int rc;` |
|      7 | 2176 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2177 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2178 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2179 | `		return PH7_OK;` |
|      - | 2180 | `	}` |
|      - | 2181 | `	/* Point to the underlying vfs */` |
|      7 | 2182 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 2183 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2184 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2185 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2186 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2187 | `			ph7_function_name(pCtx)` |
|      - | 2188 | `			);` |
|    ! 0 | 2189 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2190 | `		return PH7_OK;` |
|      - | 2191 | `	}` |
|      - | 2192 | `	/* Extract the given arguments */` |
|      7 | 2193 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      7 | 2194 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2195 | `	/* Perform the requested operation */` |
|      7 | 2196 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2197 | `	/* IO result */` |
|      7 | 2198 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      7 | 2199 | `	return PH7_OK;` |
|      4 | 2200 | `}` |
|      - | 2201 | `/*` |
|      - | 2202 | ` * int umask([ int $mask ])` |
|      - | 2203 | ` *  Changes the current umask.` |
|      - | 2204 | ` * Parameters` |
|      - | 2205 | ` *  $mask` |
|      - | 2206 | ` *   The new umask.` |
|      - | 2207 | ` * Return` |
|      - | 2208 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2209 | ` *  Otherwise the old umask is returned.` |
|      - | 2210 | ` */` |
|      8 | 2211 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2212 | `{` |
|      - | 2213 | `	int iOld,iNew;` |
|      - | 2214 | `	ph7_vfs *pVfs;` |
|      - | 2215 | `	/* Point to the underlying vfs */` |
|      9 | 2216 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 2217 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2218 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2219 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2220 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2221 | `			ph7_function_name(pCtx)` |
|      - | 2222 | `			);` |
|    ! 0 | 2223 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2224 | `		return PH7_OK;` |
|      - | 2225 | `	}` |
|      9 | 2226 | `	iNew = 0;` |
|      9 | 2227 | `	if( nArg > 0 ){` |
|      5 | 2228 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2229 | `	}` |
|      - | 2230 | `	/* Perform the requested operation */` |
|      9 | 2231 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2232 | `	/* Old mask */` |
|      9 | 2233 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 2234 | `	return PH7_OK;` |
|      5 | 2235 | `}` |
|      - | 2236 | `/*` |
|      - | 2237 | ` * string sys_get_temp_dir()` |
|      - | 2238 | ` *  Returns directory path used for temporary files.` |
|      - | 2239 | ` * Parameters` |
|      - | 2240 | ` *  None` |
|      - | 2241 | ` * Return` |
|      - | 2242 | ` *  Returns the path of the temporary directory.` |
|      - | 2243 | ` */` |
|    242 | 2244 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2245 | `{` |
|      - | 2246 | `	ph7_vfs *pVfs;` |
|      - | 2247 | `	/* Set the empty string as the default return value */` |
|    246 | 2248 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2249 | `	/* Point to the underlying vfs */` |
|    246 | 2250 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    246 | 2251 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2252 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2253 | `		SXUNUSED(apArg);` |
|      - | 2254 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2255 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2256 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2257 | `			ph7_function_name(pCtx)` |
|      - | 2258 | `			);` |
|    ! 0 | 2259 | `		return PH7_OK;` |
|      - | 2260 | `	}` |
|      - | 2261 | `	/* Perform the requested operation */` |
|    246 | 2262 | `	pVfs->xTempDir(pCtx);` |
|    246 | 2263 | `	return PH7_OK;` |
|    125 | 2264 | `}` |
|      - | 2265 | `/*` |
|      - | 2266 | ` * string get_current_user()` |
|      - | 2267 | ` *  Returns the name of the current working user.` |
|      - | 2268 | ` * Parameters` |
|      - | 2269 | ` *  None` |
|      - | 2270 | ` * Return` |
|      - | 2271 | ` *  Returns the name of the current working user.` |
|      - | 2272 | ` */` |
|      2 | 2273 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2274 | `{` |
|      - | 2275 | `	ph7_vfs *pVfs;` |
|      - | 2276 | `	/* Point to the underlying vfs */` |
|      3 | 2277 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2278 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2279 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2280 | `		SXUNUSED(apArg);` |
|      - | 2281 | `		/* IO routine not implemented */` |
|    ! 0 | 2282 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2283 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2284 | `			ph7_function_name(pCtx)` |
|      - | 2285 | `			);` |
|      - | 2286 | `		/* Set a dummy username */` |
|    ! 0 | 2287 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2288 | `		return PH7_OK;` |
|      - | 2289 | `	}` |
|      - | 2290 | `	/* Perform the requested operation */` |
|      3 | 2291 | `	pVfs->xUsername(pCtx);` |
|      3 | 2292 | `	return PH7_OK;` |
|      2 | 2293 | `}` |
|      - | 2294 | `/*` |
|      - | 2295 | ` * int64 getmypid()` |
|      - | 2296 | ` *  Gets process ID.` |
|      - | 2297 | ` * Parameters` |
|      - | 2298 | ` *  None` |
|      - | 2299 | ` * Return` |
|      - | 2300 | ` *  Returns the process ID.` |
|      - | 2301 | ` */` |
|     84 | 2302 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2303 | `{` |
|      - | 2304 | `	ph7_int64 nProcessId;` |
|      - | 2305 | `	ph7_vfs *pVfs;` |
|      - | 2306 | `	/* Point to the underlying vfs */` |
|     86 | 2307 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     86 | 2308 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2309 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2310 | `		SXUNUSED(apArg);` |
|      - | 2311 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2312 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2313 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2314 | `			ph7_function_name(pCtx)` |
|      - | 2315 | `			);` |
|    ! 0 | 2316 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2317 | `		return PH7_OK;` |
|      - | 2318 | `	}` |
|      - | 2319 | `	/* Perform the requested operation */` |
|     86 | 2320 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2321 | `	/* Set the result */` |
|     86 | 2322 | `	ph7_result_int64(pCtx,nProcessId);` |
|     86 | 2323 | `	return PH7_OK;` |
|     44 | 2324 | `}` |
|      - | 2325 | `/*` |
|      - | 2326 | ` * int getmyuid()` |
|      - | 2327 | ` *  Get user ID.` |
|      - | 2328 | ` * Parameters` |
|      - | 2329 | ` *  None` |
|      - | 2330 | ` * Return` |
|      - | 2331 | ` *  Returns the user ID.` |
|      - | 2332 | ` */` |
|      2 | 2333 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2334 | `{` |
|      - | 2335 | `	ph7_vfs *pVfs;` |
|      - | 2336 | `	int nUid;` |
|      - | 2337 | `	/* Point to the underlying vfs */` |
|      3 | 2338 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2339 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2340 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2341 | `		SXUNUSED(apArg);` |
|      - | 2342 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2343 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2344 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2345 | `			ph7_function_name(pCtx)` |
|      - | 2346 | `			);` |
|    ! 0 | 2347 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2348 | `		return PH7_OK;` |
|      - | 2349 | `	}` |
|      - | 2350 | `	/* Perform the requested operation */` |
|      3 | 2351 | `	nUid = pVfs->xUid();` |
|      - | 2352 | `	/* Set the result */` |
|      3 | 2353 | `	ph7_result_int(pCtx,nUid);` |
|      3 | 2354 | `	return PH7_OK;` |
|      2 | 2355 | `}` |
|      - | 2356 | `/*` |
|      - | 2357 | ` * int getmygid()` |
|      - | 2358 | ` *  Get group ID.` |
|      - | 2359 | ` * Parameters` |
|      - | 2360 | ` *  None` |
|      - | 2361 | ` * Return` |
|      - | 2362 | ` *  Returns the group ID.` |
|      - | 2363 | ` */` |
|      2 | 2364 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2365 | `{` |
|      - | 2366 | `	ph7_vfs *pVfs;` |
|      - | 2367 | `	int nGid;` |
|      - | 2368 | `	/* Point to the underlying vfs */` |
|      3 | 2369 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2370 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 2371 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2372 | `		SXUNUSED(apArg);` |
|      - | 2373 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2374 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2375 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2376 | `			ph7_function_name(pCtx)` |
|      - | 2377 | `			);` |
|    ! 0 | 2378 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2379 | `		return PH7_OK;` |
|      - | 2380 | `	}` |
|      - | 2381 | `	/* Perform the requested operation */` |
|      3 | 2382 | `	nGid = pVfs->xGid();` |
|      - | 2383 | `	/* Set the result */` |
|      3 | 2384 | `	ph7_result_int(pCtx,nGid);` |
|      3 | 2385 | `	return PH7_OK;` |
|      2 | 2386 | `}` |
|      - | 2387 | `#ifdef __WINNT__` |
|      - | 2388 | `#include <Windows.h>` |
|      - | 2389 | `#elif defined(__UNIXES__)` |
|      - | 2390 | `#include <sys/utsname.h>` |
|      - | 2391 | `#endif` |
|      - | 2392 | `/*` |
|      - | 2393 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 2394 | ` *  Returns information about the host operating system.` |
|      - | 2395 | ` * Parameters` |
|      - | 2396 | ` *  $mode` |
|      - | 2397 | ` *   mode is a single character that defines what information is returned:` |
|      - | 2398 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 2399 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 2400 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 2401 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 2402 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 2403 | ` *    'm': Machine type. eg. i386.` |
|      - | 2404 | ` * Return` |
|      - | 2405 | ` *  OS description as a string.` |
|      - | 2406 | ` */` |
|      4 | 2407 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2408 | `{` |
|      - | 2409 | `#if defined(__WINNT__)` |
|      1 | 2410 | `	const char *zName = "Microsoft Windows";` |
|      - | 2411 | `	OSVERSIONINFOW sVer;` |
|      - | 2412 | `#elif defined(__UNIXES__)` |
|      - | 2413 | `	struct utsname sName;` |
|      - | 2414 | `#endif` |
|      5 | 2415 | `	const char *zMode = "a";` |
|      5 | 2416 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 2417 | `		/* Extract the desired mode */` |
|    ! 0 | 2418 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 2419 | `	}` |
|      - | 2420 | `#if defined(__WINNT__)` |
|      1 | 2421 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 2422 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 2423 | `#if defined(_MSC_VER)` |
|      - | 2424 | `#pragma warning(push)` |
|      - | 2425 | `#pragma warning(disable:4996)` |
|      - | 2426 | `#endif` |
|      1 | 2427 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 2428 | `#if defined(_MSC_VER)` |
|      - | 2429 | `#pragma warning(pop)` |
|      - | 2430 | `#endif` |
|    ! 0 | 2431 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 2432 | `		return PH7_OK;` |
|      - | 2433 | `	}` |
|      1 | 2434 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 2435 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 2436 | `			zName = "Microsoft Windows NT";` |
|      1 | 2437 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 2438 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 2439 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 2440 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 2441 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 2442 | `			}` |
|    ! 0 | 2443 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 2444 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 2445 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 2446 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 2447 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 2448 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 2449 | `					default: break;` |
|      - | 2450 | `				}` |
|      - | 2451 | `		}` |
|      - | 2452 | `	}` |
|      1 | 2453 | `	switch(zMode[0]){` |
|      - | 2454 | `	case 's':` |
|      - | 2455 | `		/* Operating system name */` |
|    ! 0 | 2456 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 2457 | `		break;` |
|      - | 2458 | `	case 'n':` |
|      - | 2459 | `		/* Host name */` |
|    ! 0 | 2460 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 2461 | `		break;` |
|      - | 2462 | `	case 'r':` |
|      - | 2463 | `	case 'v':` |
|      - | 2464 | `		/* Version information. */` |
|    ! 0 | 2465 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 2466 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2467 | `			);` |
|    ! 0 | 2468 | `		break;` |
|      - | 2469 | `	case 'm':` |
|      - | 2470 | `		/* Machine name */` |
|    ! 0 | 2471 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 2472 | `		break;` |
|      - | 2473 | `	default:` |
|      1 | 2474 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 2475 | `			zName,` |
|      - | 2476 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2477 | `			);` |
|      - | 2478 | `		break;` |
|      - | 2479 | `	}` |
|      - | 2480 | `#elif defined(__UNIXES__)` |
|      4 | 2481 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 2482 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 2483 | `		return PH7_OK;` |
|      - | 2484 | `	}` |
|      4 | 2485 | `	switch(zMode[0]){` |
|    ! 0 | 2486 | `	case 's':` |
|      - | 2487 | `		/* Operating system name */` |
|    ! 0 | 2488 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 2489 | `		break;` |
|    ! 0 | 2490 | `	case 'n':` |
|      - | 2491 | `		/* Host name */` |
|    ! 0 | 2492 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 2493 | `		break;` |
|    ! 0 | 2494 | `	case 'r':` |
|      - | 2495 | `		/* Release information */` |
|    ! 0 | 2496 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 2497 | `		break;` |
|    ! 0 | 2498 | `	case 'v':` |
|      - | 2499 | `		/* Version information. */` |
|    ! 0 | 2500 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 2501 | `		break;` |
|    ! 0 | 2502 | `	case 'm':` |
|      - | 2503 | `		/* Machine name */` |
|    ! 0 | 2504 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 2505 | `		break;` |
|      2 | 2506 | `	default:` |
|      6 | 2507 | `		ph7_result_string_format(pCtx,` |
|      - | 2508 | `			"%s %s %s %s %s",` |
|      2 | 2509 | `			sName.sysname,` |
|      2 | 2510 | `			sName.release,` |
|      2 | 2511 | `			sName.version,` |
|      2 | 2512 | `			sName.nodename,` |
|      2 | 2513 | `			sName.machine` |
|      - | 2514 | `			);` |
|      4 | 2515 | `		break;` |
|      - | 2516 | `	}` |
|      - | 2517 | `#else` |
|      - | 2518 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 2519 | `#endif` |
|      5 | 2520 | `	return PH7_OK;` |
|      3 | 2521 | `}` |
|      - | 2522 | `/*` |
|      - | 2523 | ` * Section:` |
|      - | 2524 | ` *    IO stream implementation.` |
|      - | 2525 | ` * Status:` |
|      - | 2526 | ` *    Stable.` |
|      - | 2527 | ` */` |
|      - | 2528 | `typedef struct io_private io_private;` |
|      - | 2529 | `struct io_private` |
|      - | 2530 | `{` |
|      - | 2531 | `	const ph7_io_stream *pStream; /* Underlying IO device */` |
|      - | 2532 | `	void *pHandle; /* IO handle */` |
|      - | 2533 | `	/* Unbuffered IO */` |
|      - | 2534 | `	SyBlob sBuffer; /* Working buffer */` |
|      - | 2535 | `	sxu32 nOfft;    /* Current read offset */` |
|      - | 2536 | `	sxu32 iMagic;   /* Sanity check to avoid misuse */` |
|      - | 2537 | `};` |
|      - | 2538 | `#define IO_PRIVATE_MAGIC 0xFEAC14` |
|      - | 2539 | `/* A user-facing handle (fopen/opendir/popen) that has been fclose()'d/closedir()'d/` |
|      - | 2540 | ` * pclose()'d keeps its io_private alive but stamped with this magic, so every` |
|      - | 2541 | ` * ph7_value that still references it observes a closed resource` |
|      - | 2542 | ` * (gettype()=='resource (closed)', is_resource()==false), matching php. */` |
|      - | 2543 | `#define IO_PRIVATE_CLOSED_MAGIC 0xC105ED` |
|      - | 2544 | `/* Stream-device predicates (devices defined later in this file) */` |
|      - | 2545 | `static int is_php_stream(const ph7_io_stream *pStream);` |
|      - | 2546 | `static int is_data_stream(const ph7_io_stream *pStream);` |
|      - | 2547 | `/* Make sure we are dealing with a valid io_private instance */` |
|      - | 2548 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|      - | 2549 | `/* Forward declaration */` |
|      - | 2550 | `static void ResetIOPrivate(io_private *pDev);` |
|      - | 2551 | `/*` |
|      - | 2552 | ` * Return the PHP resource-type name for a raw resource handle.` |
|      - | 2553 | ` * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the` |
|      - | 2554 | ` * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as` |
|      - | 2555 | ` * "stream"; anything else is "Unknown". The magic probe mirrors the` |
|      - | 2556 | ` * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.` |
|      - | 2557 | ` */` |
|      6 | 2558 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      1 | 2559 | `{` |
|      7 | 2560 | `	io_private *pDev = (io_private *)pResource;` |
|      7 | 2561 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|      5 | 2562 | `		return "stream";` |
|      - | 2563 | `	}` |
|      3 | 2564 | `	return "Unknown";` |
|      4 | 2565 | `}` |
|      - | 2566 | `/*` |
|      - | 2567 | ` * Return TRUE if the given resource handle is an io_private that has been closed` |
|      - | 2568 | ` * (fclose/closedir/pclose) yet kept alive so shared copies still observe it. php` |
|      - | 2569 | ` * reports such a value as gettype()=='resource (closed)' and is_resource()==false.` |
|      - | 2570 | ` * The magic probe mirrors IO_PRIVATE_INVALID and is safe on any resource handle:` |
|      - | 2571 | ` * every resource this engine hands out is a struct larger than an io_private, so` |
|      - | 2572 | ` * the iMagic slot is always in bounds and never equals the closed magic by chance.` |
|      - | 2573 | ` */` |
|     64 | 2574 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      2 | 2575 | `{` |
|     66 | 2576 | `	io_private *pDev = (io_private *)pResource;` |
|     66 | 2577 | `	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;` |
|      2 | 2578 | `}` |
|      - | 2579 | `/*` |
|      - | 2580 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - | 2581 | ` *  Truncates a file to a given length.` |
|      - | 2582 | ` * Parameters` |
|      - | 2583 | ` *  $handle` |
|      - | 2584 | ` *   The file pointer.` |
|      - | 2585 | ` *   Note:` |
|      - | 2586 | ` *    The handle must be open for writing.` |
|      - | 2587 | ` * $size` |
|      - | 2588 | ` *   The size to truncate to.` |
|      - | 2589 | ` * Return` |
|      - | 2590 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2591 | ` */` |
|      6 | 2592 | `static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2593 | `{` |
|      - | 2594 | `	const ph7_io_stream *pStream;` |
|      - | 2595 | `	io_private *pDev;` |
|      - | 2596 | `	int rc;` |
|      7 | 2597 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2598 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2599 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2600 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2601 | `		return PH7_OK;` |
|      - | 2602 | `	}` |
|      - | 2603 | `	/* Extract our private data */` |
|      7 | 2604 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2605 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2606 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2607 | `		/*Expecting an IO handle */` |
|    ! 0 | 2608 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2609 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2610 | `		return PH7_OK;` |
|      - | 2611 | `	}` |
|      - | 2612 | `	/* Point to the target IO stream device */` |
|      7 | 2613 | `	pStream = pDev->pStream;` |
|      7 | 2614 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 | 2615 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2616 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2617 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2618 | `			);` |
|    ! 0 | 2619 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2620 | `		return PH7_OK;` |
|      - | 2621 | `	}` |
|      - | 2622 | `	/* Perform the requested operation */` |
|      7 | 2623 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      7 | 2624 | `	if( rc == PH7_OK ){` |
|      - | 2625 | `		/* Discard buffered data */` |
|      7 | 2626 | `		ResetIOPrivate(pDev);` |
|      3 | 2627 | `	}` |
|      - | 2628 | `	/* IO result */` |
|      7 | 2629 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 | 2630 | `	return PH7_OK;` |
|      4 | 2631 | `}` |
|      - | 2632 | `/*` |
|      - | 2633 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - | 2634 | ` *  Seeks on a file pointer.` |
|      - | 2635 | ` * Parameters` |
|      - | 2636 | ` *  $handle` |
|      - | 2637 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - | 2638 | ` * $offset` |
|      - | 2639 | ` *   The offset.` |
|      - | 2640 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - | 2641 | ` *   value in offset and set whence to SEEK_END.` |
|      - | 2642 | ` *   whence` |
|      - | 2643 | ` *   whence values are:` |
|      - | 2644 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - | 2645 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - | 2646 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - | 2647 | ` * Return` |
|      - | 2648 | ` *  0 on success,-1 on failure` |
|      - | 2649 | ` */` |
|     10 | 2650 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2651 | `{` |
|      - | 2652 | `	const ph7_io_stream *pStream;` |
|      - | 2653 | `	io_private *pDev;` |
|      - | 2654 | `	ph7_int64 iOfft;` |
|      - | 2655 | `	int whence;` |
|      - | 2656 | `	int rc;` |
|     12 | 2657 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2658 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2659 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2660 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2661 | `		return PH7_OK;` |
|      - | 2662 | `	}` |
|      - | 2663 | `	/* Extract our private data */` |
|     12 | 2664 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2665 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     12 | 2666 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2667 | `		/*Expecting an IO handle */` |
|    ! 0 | 2668 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2669 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2670 | `		return PH7_OK;` |
|      - | 2671 | `	}` |
|      - | 2672 | `	/* Point to the target IO stream device */` |
|     12 | 2673 | `	pStream = pDev->pStream;` |
|     12 | 2674 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2675 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2676 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2677 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2678 | `			);` |
|    ! 0 | 2679 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2680 | `		return PH7_OK;` |
|      - | 2681 | `	}` |
|      - | 2682 | `	/* Extract the offset */` |
|     12 | 2683 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     12 | 2684 | `	whence = 0;/* SEEK_SET */` |
|     12 | 2685 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      3 | 2686 | `		whence = ph7_value_to_int(apArg[2]);` |
|      1 | 2687 | `	}` |
|      - | 2688 | `	/* Perform the requested operation */` |
|     12 | 2689 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     12 | 2690 | `	if( rc == PH7_OK ){` |
|      - | 2691 | `		/* Ignore buffered data */` |
|     12 | 2692 | `		ResetIOPrivate(pDev);` |
|      5 | 2693 | `	}` |
|      - | 2694 | `	/* IO result */` |
|     12 | 2695 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     12 | 2696 | `	return PH7_OK;` |
|      7 | 2697 | `}` |
|      - | 2698 | `/*` |
|      - | 2699 | ` * int64 ftell(resource $handle)` |
|      - | 2700 | ` *  Returns the current position of the file read/write pointer.` |
|      - | 2701 | ` * Parameters` |
|      - | 2702 | ` *  $handle` |
|      - | 2703 | ` *   The file pointer.` |
|      - | 2704 | ` * Return` |
|      - | 2705 | ` *  Returns the position of the file pointer referenced by handle` |
|      - | 2706 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - | 2707 | ` *  FALSE is returned on failure.` |
|      - | 2708 | ` */` |
|     12 | 2709 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2710 | `{` |
|      - | 2711 | `	const ph7_io_stream *pStream;` |
|      - | 2712 | `	io_private *pDev;` |
|      - | 2713 | `	ph7_int64 iOfft;` |
|     14 | 2714 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2715 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2716 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2717 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2718 | `		return PH7_OK;` |
|      - | 2719 | `	}` |
|      - | 2720 | `	/* Extract our private data */` |
|     14 | 2721 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2722 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     14 | 2723 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2724 | `		/*Expecting an IO handle */` |
|    ! 0 | 2725 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2726 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2727 | `		return PH7_OK;` |
|      - | 2728 | `	}` |
|      - | 2729 | `	/* Point to the target IO stream device */` |
|     14 | 2730 | `	pStream = pDev->pStream;` |
|     14 | 2731 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2732 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2733 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2734 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2735 | `			);` |
|    ! 0 | 2736 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2737 | `		return PH7_OK;` |
|      - | 2738 | `	}` |
|      - | 2739 | `	/* Perform the requested operation */` |
|     14 | 2740 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2741 | `	/* IO result */` |
|     14 | 2742 | `	ph7_result_int64(pCtx,iOfft);` |
|     14 | 2743 | `	return PH7_OK;` |
|      8 | 2744 | `}` |
|      - | 2745 | `/*` |
|      - | 2746 | ` * bool rewind(resource $handle)` |
|      - | 2747 | ` *  Rewind the position of a file pointer.` |
|      - | 2748 | ` * Parameters` |
|      - | 2749 | ` *  $handle` |
|      - | 2750 | ` *   The file pointer.` |
|      - | 2751 | ` * Return` |
|      - | 2752 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2753 | ` */` |
|     14 | 2754 | `static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2755 | `{` |
|      - | 2756 | `	const ph7_io_stream *pStream;` |
|      - | 2757 | `	io_private *pDev;` |
|      - | 2758 | `	int rc;` |
|     15 | 2759 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2760 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2761 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2762 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2763 | `		return PH7_OK;` |
|      - | 2764 | `	}` |
|      - | 2765 | `	/* Extract our private data */` |
|     15 | 2766 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2767 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     15 | 2768 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2769 | `		/*Expecting an IO handle */` |
|    ! 0 | 2770 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2771 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2772 | `		return PH7_OK;` |
|      - | 2773 | `	}` |
|      - | 2774 | `	/* Point to the target IO stream device */` |
|     15 | 2775 | `	pStream = pDev->pStream;` |
|     15 | 2776 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2777 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2778 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2779 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2780 | `			);` |
|    ! 0 | 2781 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2782 | `		return PH7_OK;` |
|      - | 2783 | `	}` |
|      - | 2784 | `	/* Perform the requested operation */` |
|     15 | 2785 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|     15 | 2786 | `	if( rc == PH7_OK ){` |
|      - | 2787 | `		/* Ignore buffered data */` |
|     15 | 2788 | `		ResetIOPrivate(pDev);` |
|      7 | 2789 | `	}` |
|      - | 2790 | `	/* IO result */` |
|     15 | 2791 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     15 | 2792 | `	return PH7_OK;` |
|      8 | 2793 | `}` |
|      - | 2794 | `/*` |
|      - | 2795 | ` * bool fflush(resource $handle)` |
|      - | 2796 | ` *  Flushes the output to a file.` |
|      - | 2797 | ` * Parameters` |
|      - | 2798 | ` *  $handle` |
|      - | 2799 | ` *   The file pointer.` |
|      - | 2800 | ` * Return` |
|      - | 2801 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2802 | ` */` |
|      2 | 2803 | `static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2804 | `{` |
|      - | 2805 | `	const ph7_io_stream *pStream;` |
|      - | 2806 | `	io_private *pDev;` |
|      - | 2807 | `	int rc;` |
|      3 | 2808 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2809 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2810 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2811 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2812 | `		return PH7_OK;` |
|      - | 2813 | `	}` |
|      - | 2814 | `	/* Extract our private data */` |
|      3 | 2815 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2816 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2817 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2818 | `		/*Expecting an IO handle */` |
|    ! 0 | 2819 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2820 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2821 | `		return PH7_OK;` |
|      - | 2822 | `	}` |
|      - | 2823 | `	/* Point to the target IO stream device */` |
|      3 | 2824 | `	pStream = pDev->pStream;` |
|      3 | 2825 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 | 2826 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2827 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2828 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2829 | `			);` |
|    ! 0 | 2830 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2831 | `		return PH7_OK;` |
|      - | 2832 | `	}` |
|      - | 2833 | `	/* Perform the requested operation */` |
|      3 | 2834 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - | 2835 | `	/* IO result */` |
|      3 | 2836 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2837 | `	return PH7_OK;` |
|      2 | 2838 | `}` |
|      - | 2839 | `/*` |
|      - | 2840 | ` * bool feof(resource $handle)` |
|      - | 2841 | ` *  Tests for end-of-file on a file pointer.` |
|      - | 2842 | ` * Parameters` |
|      - | 2843 | ` *  $handle` |
|      - | 2844 | ` *   The file pointer.` |
|      - | 2845 | ` * Return` |
|      - | 2846 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - | 2847 | ` */` |
|  10618 | 2848 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2849 | `{` |
|      - | 2850 | `	const ph7_io_stream *pStream;` |
|      - | 2851 | `	io_private *pDev;` |
|      - | 2852 | `	int rc;` |
|  10623 | 2853 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2854 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2855 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2856 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2857 | `		return PH7_OK;` |
|      - | 2858 | `	}` |
|      - | 2859 | `	/* Extract our private data */` |
|  10623 | 2860 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2861 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10623 | 2862 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2863 | `		/*Expecting an IO handle */` |
|    ! 0 | 2864 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2865 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2866 | `		return PH7_OK;` |
|      - | 2867 | `	}` |
|      - | 2868 | `	/* Point to the target IO stream device */` |
|  10623 | 2869 | `	pStream = pDev->pStream;` |
|  10623 | 2870 | `	if( pStream == 0 ){` |
|    ! 0 | 2871 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2872 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2873 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2874 | `			);` |
|    ! 0 | 2875 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2876 | `		return PH7_OK;` |
|      - | 2877 | `	}` |
|  10623 | 2878 | `	rc = SXERR_EOF;` |
|      - | 2879 | `	/* Perform the requested operation */` |
|  10623 | 2880 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2881 | `		/* Data is available */` |
|   4863 | 2882 | `		rc = PH7_OK;` |
|   2434 | 2883 | `	}else{` |
|      - | 2884 | `		char zBuf[4096];` |
|      - | 2885 | `		ph7_int64 n;` |
|      - | 2886 | `		/* Perform a buffered read */` |
|   5765 | 2887 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5765 | 2888 | `		if( n > 0 ){` |
|      - | 2889 | `			/* Copy buffered data */` |
|   1883 | 2890 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1883 | 2891 | `			rc = PH7_OK;` |
|    939 | 2892 | `		}` |
|      - | 2893 | `	}` |
|      - | 2894 | `	/* EOF or not */` |
|  10623 | 2895 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10623 | 2896 | `	return PH7_OK;` |
|   5314 | 2897 | `}` |
|      - | 2898 | `/*` |
|      - | 2899 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2900 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2901 | ` * [i.e: IO error ] or EOF.` |
|      - | 2902 | ` */` |
|     36 | 2903 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      3 | 2904 | `{` |
|     39 | 2905 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     39 | 2906 | `	char *zBuf = (char *)pBuf;` |
|      - | 2907 | `	ph7_int64 n,nRead;` |
|     39 | 2908 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     39 | 2909 | `	if( n > 0 ){` |
|      2 | 2910 | `		if( n > nLen ){` |
|    ! 0 | 2911 | `			n = nLen;` |
|    ! 0 | 2912 | `		}` |
|      - | 2913 | `		/* Copy the buffered data */` |
|      2 | 2914 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2915 | `		/* Update the read offset */` |
|      2 | 2916 | `		pDev->nOfft += (sxu32)n;` |
|      2 | 2917 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2918 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      2 | 2919 | `			SyBlobReset(&pDev->sBuffer);` |
|      2 | 2920 | `			pDev->nOfft = 0;` |
|      1 | 2921 | `		}` |
|      2 | 2922 | `		nLen -= n;` |
|      2 | 2923 | `		if( nLen < 1 ){` |
|      - | 2924 | `			/* All done */` |
|    ! 0 | 2925 | `			return n;` |
|      - | 2926 | `		}` |
|      - | 2927 | `		/* Advance the cursor */` |
|      2 | 2928 | `		zBuf += n;` |
|      1 | 2929 | `	}` |
|      - | 2930 | `	/* Read without buffering */` |
|     39 | 2931 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     39 | 2932 | `	if( nRead > 0 ){` |
|     35 | 2933 | `		n += nRead;` |
|     21 | 2934 | `	}else if( n < 1 ){` |
|      - | 2935 | `		/* EOF or IO error */` |
|      3 | 2936 | `		return nRead;` |
|      - | 2937 | `	}` |
|     37 | 2938 | `	return n;` |
|     21 | 2939 | `}` |
|      - | 2940 | `/*` |
|      - | 2941 | ` * Extract a single line from the buffered input.` |
|      - | 2942 | ` */` |
|   6802 | 2943 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 | 2944 | `{` |
|      - | 2945 | `	const char *zIn,*zEnd,*zPtr;` |
|   6807 | 2946 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6807 | 2947 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6807 | 2948 | `	zPtr = zIn;` |
| 458001 | 2949 | `	while( zIn < zEnd ){` |
| 457897 | 2950 | `		if( zIn[0] == '\n' ){` |
|      - | 2951 | `			/* Line found */` |
|   6703 | 2952 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6703 | 2953 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6703 | 2954 | `			*pzLine = zPtr;` |
|   6703 | 2955 | `			return SXRET_OK;` |
|      - | 2956 | `		}` |
| 451199 | 2957 | `		zIn++;` |
|      5 | 2958 | `	}` |
|      - | 2959 | `	/* No line were found */` |
|    109 | 2960 | `	return SXERR_NOTFOUND;` |
|   3406 | 2961 | `}` |
|      - | 2962 | `/*` |
|      - | 2963 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2964 | ` */` |
|   6806 | 2965 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 | 2966 | `{` |
|   6811 | 2967 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2968 | `	char zBuf[8192];` |
|      - | 2969 | `	ph7_int64 n;` |
|      - | 2970 | `	sxi32 rc;` |
|   6811 | 2971 | `	n = 0;` |
|   6811 | 2972 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2973 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     73 | 2974 | `		SyBlobReset(&pDev->sBuffer);` |
|     73 | 2975 | `		pDev->nOfft = 0;` |
|     34 | 2976 | `	}` |
|   6777 | 2977 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2978 | `		/* Check if there is a line */` |
|   6743 | 2979 | `		rc = GetLine(pDev,&n,pzData);` |
|   6743 | 2980 | `		if( rc == SXRET_OK ){` |
|      - | 2981 | `			/* Got line,update the cursor  */` |
|   6643 | 2982 | `			pDev->nOfft += (sxu32)n;` |
|   6643 | 2983 | `			return n;` |
|      - | 2984 | `		}` |
|     50 | 2985 | `	}` |
|      - | 2986 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2987 | `	 * limit is reached.` |
|      - | 2988 | `	 */` |
|     86 | 2989 | `	for(;;){` |
|    177 | 2990 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    177 | 2991 | `		if( n < 1 ){` |
|      - | 2992 | `			/* EOF or IO error */` |
|    113 | 2993 | `			break;` |
|      - | 2994 | `		}` |
|      - | 2995 | `		/* Append the data just read */` |
|     67 | 2996 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2997 | `		/* Try to extract a line */` |
|     67 | 2998 | `		rc = GetLine(pDev,&n,pzData);` |
|     67 | 2999 | `		if( rc == SXRET_OK ){` |
|      - | 3000 | `			/* Got one,return immediately */` |
|     63 | 3001 | `			pDev->nOfft += (sxu32)n;` |
|     63 | 3002 | `			return n;` |
|      - | 3003 | `		}` |
|      5 | 3004 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - | 3005 | `			/* Read limit reached,return the available data */` |
|    ! 0 | 3006 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 | 3007 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 3008 | `			/* Reset the working buffer */` |
|    ! 0 | 3009 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 3010 | `			pDev->nOfft = 0;` |
|    ! 0 | 3011 | `			return n;` |
|      - | 3012 | `		}` |
|      1 | 3013 | `	}` |
|    113 | 3014 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 3015 | `		/* Read limit reached,return the available data */` |
|    109 | 3016 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    109 | 3017 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 3018 | `		/* Reset the working buffer */` |
|    109 | 3019 | `		SyBlobReset(&pDev->sBuffer);` |
|    109 | 3020 | `		pDev->nOfft = 0;` |
|     52 | 3021 | `	}` |
|    113 | 3022 | `	return n;` |
|   3408 | 3023 | `}` |
|      - | 3024 | `/*` |
|      - | 3025 | ` * Open an IO stream handle.` |
|      - | 3026 | ` * Notes on stream:` |
|      - | 3027 | ` * According to the PHP reference manual.` |
|      - | 3028 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - | 3029 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - | 3030 | ` * to an arbitrary locations within the stream.` |
|      - | 3031 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - | 3032 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - | 3033 | ` * on a remote server.` |
|      - | 3034 | ` * A stream is referenced as: scheme://target` |
|      - | 3035 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - | 3036 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - | 3037 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - | 3038 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - | 3039 | ` *  with a path appended.` |
|      - | 3040 | ` *` |
|      - | 3041 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - | 3042 | ` * Please refer to the official documentation for a full discussion.` |
|      - | 3043 | ` * This function return a handle on success. Otherwise null.` |
|      - | 3044 | ` */` |
|  30406 | 3045 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 3046 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 | 3047 | `{` |
|  30411 | 3048 | `	void *pHandle = 0; /* cc warning */` |
|      - | 3049 | `	SyString sFile;` |
|      - | 3050 | `	ph7_value sDummy;` |
|      - | 3051 | `	int rc;` |
|  30411 | 3052 | `	if( pStream == 0 ){` |
|      - | 3053 | `		/* No such stream device */` |
|    ! 0 | 3054 | `		return 0;` |
|      - | 3055 | `	}` |
|  30411 | 3056 | `	if( pResource == 0 ){` |
|      - | 3057 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - | 3058 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - | 3059 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - | 3060 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - | 3061 | `		 * reads it during the call, and file:// ignores it. */` |
|  30389 | 3062 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  30389 | 3063 | `		pResource = &sDummy;` |
|  15192 | 3064 | `	}` |
|  30411 | 3065 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  30411 | 3066 | `	if( use_include ){` |
|   9688 | 3067 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 3068 | `#ifdef __WINNT__` |
|      - | 3069 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 3070 | `#endif` |
|   9652 | 3071 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9648 | 3072 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 3073 | `				/*  Open the file directly */` |
|     44 | 3074 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     24 | 3075 | `		}else{` |
|      - | 3076 | `			SyString *pPath;` |
|      - | 3077 | `			SyBlob sWorker;` |
|      - | 3078 | `#ifdef __WINNT__` |
|      - | 3079 | `			static const int c = '\\';` |
|      - | 3080 | `#else` |
|      - | 3081 | `			static const int c = '/';` |
|      - | 3082 | `#endif` |
|      - | 3083 | `			/* Init the path builder working buffer */` |
|   9650 | 3084 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 3085 | `			/* Build a path from the set of include path */` |
|   9650 | 3086 | `			SySetResetCursor(&pVm->aPaths);` |
|   9650 | 3087 | `			rc = SXERR_IO;` |
|   9656 | 3088 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 3089 | `				/* Build full path */` |
|   9650 | 3090 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 3091 | `				/* Append null terminator */` |
|   9650 | 3092 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 3093 | `					continue;` |
|      - | 3094 | `				}` |
|      - | 3095 | `				/* Try to open the file */` |
|   9650 | 3096 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9650 | 3097 | `				if( rc == PH7_OK ){` |
|   9643 | 3098 | `					if( bPushInclude ){` |
|      - | 3099 | `						/* Mark as included */` |
|   9643 | 3100 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4821 | 3101 | `					}` |
|   9643 | 3102 | `					break;` |
|      - | 3103 | `				}` |
|      - | 3104 | `				/* Reset the working buffer */` |
|      8 | 3105 | `				SyBlobReset(&sWorker);` |
|      - | 3106 | `				/* Check the next path */` |
|      2 | 3107 | `			}` |
|   9650 | 3108 | `			SyBlobRelease(&sWorker);` |
|      - | 3109 | `		}` |
|   9692 | 3110 | `		if( rc == PH7_OK ){` |
|   9686 | 3111 | `			if( bPushInclude ){` |
|      - | 3112 | `				/* Mark as included */` |
|   9686 | 3113 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   4841 | 3114 | `			}` |
|   4841 | 3115 | `		}` |
|   4848 | 3116 | `	}else{` |
|      - | 3117 | `		/* Open the URI direcly */` |
|  20723 | 3118 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 3119 | `	}` |
|  30411 | 3120 | `	if( rc != PH7_OK ){` |
|      - | 3121 | `		/* IO error */` |
|     25 | 3122 | `		return 0;` |
|      - | 3123 | `	}` |
|      - | 3124 | `	/* Return the file handle */` |
|  30391 | 3125 | `	return pHandle;` |
|  15208 | 3126 | `}` |
|      - | 3127 | `/*` |
|      - | 3128 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 3129 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 3130 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 3131 | ` */` |
|   9676 | 3132 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      4 | 3133 | `{` |
|      - | 3134 | `	ph7_int64 nRead;` |
|      - | 3135 | `	char zBuf[8192]; /* 8K */` |
|      - | 3136 | `	int rc;` |
|      - | 3137 | `	/* Perform the requested operation */` |
|   9676 | 3138 | `	for(;;){` |
|  19356 | 3139 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  19356 | 3140 | `		if( nRead < 1 ){` |
|      - | 3141 | `			/* EOF or IO error */` |
|   9680 | 3142 | `			break;` |
|      - | 3143 | `		}` |
|      - | 3144 | `		/* Append contents */` |
|   9680 | 3145 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9680 | 3146 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3147 | `			break;` |
|      - | 3148 | `		}` |
|      4 | 3149 | `	}` |
|   9680 | 3150 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      4 | 3151 | `}` |
|      - | 3152 | `/*` |
|      - | 3153 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 3154 | ` */` |
|  30514 | 3155 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 3156 | `{` |
|  30519 | 3157 | `	if( pStream->xClose ){` |
|  30519 | 3158 | `		pStream->xClose(pHandle);` |
|  15257 | 3159 | `	}` |
|  30519 | 3160 | `}` |
|      - | 3161 | `/*` |
|      - | 3162 | ` * string fgetc(resource $handle)` |
|      - | 3163 | ` *  Gets a character from the given file pointer.` |
|      - | 3164 | ` * Parameters` |
|      - | 3165 | ` *  $handle` |
|      - | 3166 | ` *   The file pointer.` |
|      - | 3167 | ` * Return` |
|      - | 3168 | ` *  Returns a string containing a single character read from the file` |
|      - | 3169 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3170 | ` * WARNING` |
|      - | 3171 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3172 | ` */` |
|      4 | 3173 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3174 | `{` |
|      - | 3175 | `	const ph7_io_stream *pStream;` |
|      - | 3176 | `	io_private *pDev;` |
|      - | 3177 | `	int c,n;` |
|      5 | 3178 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3179 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3180 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3181 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3182 | `		return PH7_OK;` |
|      - | 3183 | `	}` |
|      - | 3184 | `	/* Extract our private data */` |
|      5 | 3185 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3186 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3187 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3188 | `		/*Expecting an IO handle */` |
|    ! 0 | 3189 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3190 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3191 | `		return PH7_OK;` |
|      - | 3192 | `	}` |
|      - | 3193 | `	/* Point to the target IO stream device */` |
|      5 | 3194 | `	pStream = pDev->pStream;` |
|      5 | 3195 | `	if( pStream == 0  ){` |
|    ! 0 | 3196 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3197 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3198 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3199 | `			);` |
|    ! 0 | 3200 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3201 | `		return PH7_OK;` |
|      - | 3202 | `	}` |
|      - | 3203 | `	/* Perform the requested operation */` |
|      5 | 3204 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3205 | `	/* IO result */` |
|      5 | 3206 | `	if( n < 1 ){` |
|      - | 3207 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3208 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3209 | `	}else{` |
|      - | 3210 | `		/* Return the string holding the character */` |
|      5 | 3211 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3212 | `	}` |
|      5 | 3213 | `	return PH7_OK;` |
|      3 | 3214 | `}` |
|      - | 3215 | `/*` |
|      - | 3216 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3217 | ` *  Gets line from file pointer.` |
|      - | 3218 | ` * Parameters` |
|      - | 3219 | ` *  $handle` |
|      - | 3220 | ` *   The file pointer.` |
|      - | 3221 | ` * $length` |
|      - | 3222 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3223 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3224 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3225 | ` *  the end of the line.` |
|      - | 3226 | ` * Return` |
|      - | 3227 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3228 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3229 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3230 | ` */` |
|   6796 | 3231 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3232 | `{` |
|      - | 3233 | `	const ph7_io_stream *pStream;` |
|      - | 3234 | `	const char *zLine;` |
|      - | 3235 | `	io_private *pDev;` |
|      - | 3236 | `	ph7_int64 n,nLen;` |
|   6801 | 3237 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3238 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3239 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3240 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3241 | `		return PH7_OK;` |
|      - | 3242 | `	}` |
|      - | 3243 | `	/* Extract our private data */` |
|   6801 | 3244 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3245 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6801 | 3246 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3247 | `		/*Expecting an IO handle */` |
|    ! 0 | 3248 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3249 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3250 | `		return PH7_OK;` |
|      - | 3251 | `	}` |
|      - | 3252 | `	/* Point to the target IO stream device */` |
|   6801 | 3253 | `	pStream = pDev->pStream;` |
|   6801 | 3254 | `	if( pStream == 0  ){` |
|    ! 0 | 3255 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3256 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3257 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3258 | `			);` |
|    ! 0 | 3259 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3260 | `		return PH7_OK;` |
|      - | 3261 | `	}` |
|   6801 | 3262 | `	nLen = -1;` |
|   6801 | 3263 | `	if( nArg > 1 ){` |
|      - | 3264 | `		/* Maximum data to read */` |
|    ! 0 | 3265 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3266 | `	}` |
|      - | 3267 | `	/* Perform the requested operation */` |
|   6801 | 3268 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6801 | 3269 | `	if( n < 1 ){` |
|      - | 3270 | `		/* EOF or IO error,return FALSE */` |
|      7 | 3271 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3272 | `	}else{` |
|      - | 3273 | `		/* Return the freshly extracted line */` |
|   6799 | 3274 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3275 | `	}` |
|   6801 | 3276 | `	return PH7_OK;` |
|   3403 | 3277 | `}` |
|      - | 3278 | `/*` |
|      - | 3279 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3280 | ` *  Binary-safe file read.` |
|      - | 3281 | ` * Parameters` |
|      - | 3282 | ` *  $handle` |
|      - | 3283 | ` *   The file pointer.` |
|      - | 3284 | ` * $length` |
|      - | 3285 | ` *  Up to length number of bytes read.` |
|      - | 3286 | ` * Return` |
|      - | 3287 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3288 | ` */` |
|     28 | 3289 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3290 | `{` |
|      - | 3291 | `	const ph7_io_stream *pStream;` |
|      - | 3292 | `	io_private *pDev;` |
|      - | 3293 | `	ph7_int64 nRead;` |
|      - | 3294 | `	void *pBuf;` |
|      - | 3295 | `	int nLen;` |
|     31 | 3296 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3297 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3298 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3299 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3300 | `		return PH7_OK;` |
|      - | 3301 | `	}` |
|      - | 3302 | `	/* Extract our private data */` |
|     31 | 3303 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3304 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     31 | 3305 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3306 | `		/*Expecting an IO handle */` |
|    ! 0 | 3307 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3308 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3309 | `		return PH7_OK;` |
|      - | 3310 | `	}` |
|      - | 3311 | `	/* Point to the target IO stream device */` |
|     31 | 3312 | `	pStream = pDev->pStream;` |
|     31 | 3313 | `	if( pStream == 0  ){` |
|    ! 0 | 3314 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3315 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3316 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3317 | `			);` |
|    ! 0 | 3318 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3319 | `		return PH7_OK;` |
|      - | 3320 | `	}` |
|     31 | 3321 | `        nLen = 4096;` |
|     31 | 3322 | `	if( nArg > 1 ){` |
|     31 | 3323 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     31 | 3324 | `	  if( nLen < 1 ){` |
|      - | 3325 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3326 | `		nLen = 4096;` |
|    ! 0 | 3327 | `	  }` |
|     14 | 3328 | `        }` |
|      - | 3329 | `	/* Allocate enough buffer */` |
|     31 | 3330 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     31 | 3331 | `	if( pBuf == 0 ){` |
|    ! 0 | 3332 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3333 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3334 | `		return PH7_OK;` |
|      - | 3335 | `	}` |
|      - | 3336 | `	/* Perform the requested operation */` |
|     31 | 3337 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     31 | 3338 | `	if( nRead < 1 ){` |
|      - | 3339 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3340 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3341 | `	}else{` |
|      - | 3342 | `		/* Make a copy of the data just read */` |
|     31 | 3343 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3344 | `	}` |
|      - | 3345 | `	/* Release the buffer */` |
|     31 | 3346 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     31 | 3347 | `	return PH7_OK;` |
|     17 | 3348 | `}` |
|      - | 3349 | `/*` |
|      - | 3350 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3351 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3352 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3353 | ` * Parameters` |
|      - | 3354 | ` * $handle` |
|      - | 3355 | ` *   The file pointer.` |
|      - | 3356 | ` * $length` |
|      - | 3357 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3358 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3359 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3360 | ` *  the end of the line.` |
|      - | 3361 | ` * $delimiter` |
|      - | 3362 | ` *   Set the field delimiter (one character only).` |
|      - | 3363 | ` * $enclosure` |
|      - | 3364 | ` *   Set the field enclosure character (one character only).` |
|      - | 3365 | ` * $escape` |
|      - | 3366 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3367 | ` * Return` |
|      - | 3368 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3369 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3370 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3371 | ` */` |
|      2 | 3372 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3373 | `{` |
|      - | 3374 | `	const ph7_io_stream *pStream;` |
|      - | 3375 | `	const char *zLine;` |
|      - | 3376 | `	io_private *pDev;` |
|      - | 3377 | `	ph7_int64 n,nLen;` |
|      3 | 3378 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3379 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3380 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3381 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3382 | `		return PH7_OK;` |
|      - | 3383 | `	}` |
|      - | 3384 | `	/* Extract our private data */` |
|      3 | 3385 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3386 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3387 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3388 | `		/*Expecting an IO handle */` |
|    ! 0 | 3389 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3390 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3391 | `		return PH7_OK;` |
|      - | 3392 | `	}` |
|      - | 3393 | `	/* Point to the target IO stream device */` |
|      3 | 3394 | `	pStream = pDev->pStream;` |
|      3 | 3395 | `	if( pStream == 0  ){` |
|    ! 0 | 3396 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3397 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3398 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3399 | `			);` |
|    ! 0 | 3400 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3401 | `		return PH7_OK;` |
|      - | 3402 | `	}` |
|      3 | 3403 | `	nLen = -1;` |
|      3 | 3404 | `	if( nArg > 1 ){` |
|      - | 3405 | `		/* Maximum data to read */` |
|      3 | 3406 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3407 | `	}` |
|      - | 3408 | `	/* Perform the requested operation */` |
|      3 | 3409 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3410 | `	if( n < 1 ){` |
|      - | 3411 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3412 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3413 | `	}else{` |
|      - | 3414 | `		ph7_value *pArray;` |
|      3 | 3415 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3416 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3417 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3418 | `		if( nArg > 2 ){` |
|      - | 3419 | `			const char *zPtr;` |
|      - | 3420 | `			int i;` |
|      3 | 3421 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3422 | `				/* Extract the delimiter */` |
|      3 | 3423 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3424 | `				if( i > 0 ){` |
|      3 | 3425 | `					delim = zPtr[0];` |
|      1 | 3426 | `				}` |
|      1 | 3427 | `			}` |
|      3 | 3428 | `			if( nArg > 3 ){` |
|      3 | 3429 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3430 | `					/* Extract the enclosure */` |
|      3 | 3431 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3432 | `					if( i > 0 ){` |
|      3 | 3433 | `						encl = zPtr[0];` |
|      1 | 3434 | `					}` |
|      1 | 3435 | `				}` |
|      3 | 3436 | `				if( nArg > 4 ){` |
|      3 | 3437 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3438 | `						/* Extract the escape character */` |
|      3 | 3439 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3440 | `						if( i > 0 ){` |
|      3 | 3441 | `							escape = zPtr[0];` |
|      1 | 3442 | `						}` |
|      1 | 3443 | `					}` |
|      1 | 3444 | `				}` |
|      1 | 3445 | `			}` |
|      1 | 3446 | `		}` |
|      - | 3447 | `		/* Create our array */` |
|      3 | 3448 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3449 | `		if( pArray == 0 ){` |
|    ! 0 | 3450 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3451 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3452 | `			return PH7_OK;` |
|      - | 3453 | `		}` |
|      - | 3454 | `		/* Parse the raw input */` |
|      3 | 3455 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3456 | `		/* Return the freshly created array  */` |
|      3 | 3457 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3458 | `	}` |
|      3 | 3459 | `	return PH7_OK;` |
|      2 | 3460 | `}` |
|      - | 3461 | `/*` |
|      - | 3462 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3463 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3464 | ` * Parameters` |
|      - | 3465 | ` * $handle` |
|      - | 3466 | ` *   The file pointer.` |
|      - | 3467 | ` * $length` |
|      - | 3468 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3469 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3470 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3471 | ` *  the end of the line.` |
|      - | 3472 | ` * $allowable_tags` |
|      - | 3473 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3474 | ` * Return` |
|      - | 3475 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3476 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3477 | ` */` |
|      2 | 3478 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3479 | `{` |
|      - | 3480 | `	const ph7_io_stream *pStream;` |
|      - | 3481 | `	const char *zLine;` |
|      - | 3482 | `	io_private *pDev;` |
|      - | 3483 | `	ph7_int64 n,nLen;` |
|      3 | 3484 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3485 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3486 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3487 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3488 | `		return PH7_OK;` |
|      - | 3489 | `	}` |
|      - | 3490 | `	/* Extract our private data */` |
|      3 | 3491 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3492 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3493 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3494 | `		/*Expecting an IO handle */` |
|    ! 0 | 3495 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3496 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3497 | `		return PH7_OK;` |
|      - | 3498 | `	}` |
|      - | 3499 | `	/* Point to the target IO stream device */` |
|      3 | 3500 | `	pStream = pDev->pStream;` |
|      3 | 3501 | `	if( pStream == 0  ){` |
|    ! 0 | 3502 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3503 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3504 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3505 | `			);` |
|    ! 0 | 3506 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3507 | `		return PH7_OK;` |
|      - | 3508 | `	}` |
|      3 | 3509 | `	nLen = -1;` |
|      3 | 3510 | `	if( nArg > 1 ){` |
|      - | 3511 | `		/* Maximum data to read */` |
|    ! 0 | 3512 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3513 | `	}` |
|      - | 3514 | `	/* Perform the requested operation */` |
|      3 | 3515 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3516 | `	if( n < 1 ){` |
|      - | 3517 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3518 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3519 | `	}else{` |
|      3 | 3520 | `		const char *zTaglist = 0;` |
|      3 | 3521 | `		int nTaglen = 0;` |
|      3 | 3522 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3523 | `			/* Allowed tag */` |
|    ! 0 | 3524 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3525 | `		}` |
|      - | 3526 | `		/* Process data just read */` |
|      3 | 3527 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3528 | `	}` |
|      3 | 3529 | `	return PH7_OK;` |
|      2 | 3530 | `}` |
|      - | 3531 | `/*` |
|      - | 3532 | ` * string readdir(resource $dir_handle)` |
|      - | 3533 | ` *   Read entry from directory handle.` |
|      - | 3534 | ` * Parameter` |
|      - | 3535 | ` *  $dir_handle` |
|      - | 3536 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3537 | ` * Return` |
|      - | 3538 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3539 | ` */` |
|  10984 | 3540 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3541 | `{` |
|      - | 3542 | `	const ph7_io_stream *pStream;` |
|      - | 3543 | `	io_private *pDev;` |
|      - | 3544 | `	int rc;` |
|  10989 | 3545 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3546 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3547 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3548 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3549 | `		return PH7_OK;` |
|      - | 3550 | `	}` |
|      - | 3551 | `	/* Extract our private data */` |
|  10989 | 3552 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3553 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10989 | 3554 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3555 | `		/*Expecting an IO handle */` |
|    ! 0 | 3556 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3557 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3558 | `		return PH7_OK;` |
|      - | 3559 | `	}` |
|      - | 3560 | `	/* Point to the target IO stream device */` |
|  10989 | 3561 | `	pStream = pDev->pStream;` |
|  10989 | 3562 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3563 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3564 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3565 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3566 | `			);` |
|    ! 0 | 3567 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3568 | `		return PH7_OK;` |
|      - | 3569 | `	}` |
|  10989 | 3570 | `	ph7_result_bool(pCtx,0);` |
|      - | 3571 | `	/* Perform the requested operation */` |
|  10989 | 3572 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  10989 | 3573 | `	if( rc != PH7_OK ){` |
|      - | 3574 | `		/* Return FALSE */` |
|   1065 | 3575 | `		ph7_result_bool(pCtx,0);` |
|    530 | 3576 | `	}` |
|  10989 | 3577 | `	return PH7_OK;` |
|   5497 | 3578 | `}` |
|      - | 3579 | `/*` |
|      - | 3580 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3581 | ` *   Rewind directory handle.` |
|      - | 3582 | ` * Parameter` |
|      - | 3583 | ` *  $dir_handle` |
|      - | 3584 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3585 | ` * Return` |
|      - | 3586 | ` *  FALSE on failure.` |
|      - | 3587 | ` */` |
|      2 | 3588 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3589 | `{` |
|      - | 3590 | `	const ph7_io_stream *pStream;` |
|      - | 3591 | `	io_private *pDev;` |
|      3 | 3592 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3593 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3594 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3595 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3596 | `		return PH7_OK;` |
|      - | 3597 | `	}` |
|      - | 3598 | `	/* Extract our private data */` |
|      3 | 3599 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3600 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3601 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3602 | `		/*Expecting an IO handle */` |
|    ! 0 | 3603 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3604 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3605 | `		return PH7_OK;` |
|      - | 3606 | `	}` |
|      - | 3607 | `	/* Point to the target IO stream device */` |
|      3 | 3608 | `	pStream = pDev->pStream;` |
|      3 | 3609 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3610 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3611 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3612 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3613 | `			);` |
|    ! 0 | 3614 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3615 | `		return PH7_OK;` |
|      - | 3616 | `	}` |
|      - | 3617 | `	/* Perform the requested operation */` |
|      3 | 3618 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3619 | `	return PH7_OK;` |
|      2 | 3620 | ` }` |
|      - | 3621 | `/* Forward declaration */` |
|      - | 3622 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3623 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3624 | `static void MarkIOPrivateClosed(io_private *pDev);` |
|      - | 3625 | `/*` |
|      - | 3626 | ` * void closedir(resource $dir_handle)` |
|      - | 3627 | ` *   Close directory handle.` |
|      - | 3628 | ` * Parameter` |
|      - | 3629 | ` *  $dir_handle` |
|      - | 3630 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3631 | ` * Return` |
|      - | 3632 | ` *  FALSE on failure.` |
|      - | 3633 | ` */` |
|   1064 | 3634 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3635 | `{` |
|      - | 3636 | `	const ph7_io_stream *pStream;` |
|      - | 3637 | `	io_private *pDev;` |
|   1069 | 3638 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3639 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3640 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3641 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3642 | `		return PH7_OK;` |
|      - | 3643 | `	}` |
|      - | 3644 | `	/* Extract our private data */` |
|   1069 | 3645 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3646 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1069 | 3647 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3648 | `		/*Expecting an IO handle */` |
|    ! 0 | 3649 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3650 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3651 | `		return PH7_OK;` |
|      - | 3652 | `	}` |
|      - | 3653 | `	/* Point to the target IO stream device */` |
|   1069 | 3654 | `	pStream = pDev->pStream;` |
|   1069 | 3655 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3656 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3657 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3658 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3659 | `			);` |
|    ! 0 | 3660 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3661 | `		return PH7_OK;` |
|      - | 3662 | `	}` |
|      - | 3663 | `	/* Perform the requested operation */` |
|   1069 | 3664 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3665 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|   1069 | 3666 | `	MarkIOPrivateClosed(pDev);` |
|   1069 | 3667 | `	return PH7_OK;` |
|    537 | 3668 | ` }` |
|      - | 3669 | `/*` |
|      - | 3670 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3671 | ` *  Open directory handle.` |
|      - | 3672 | ` * Parameters` |
|      - | 3673 | ` * $path` |
|      - | 3674 | ` *   The directory path that is to be opened.` |
|      - | 3675 | ` * $context` |
|      - | 3676 | ` *   A context stream resource.` |
|      - | 3677 | ` * Return` |
|      - | 3678 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3679 | ` */` |
|   1064 | 3680 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3681 | `{` |
|      - | 3682 | `	const ph7_io_stream *pStream;` |
|      - | 3683 | `	const char *zPath;` |
|      - | 3684 | `	io_private *pDev;` |
|      - | 3685 | `	int iLen,rc;` |
|   1069 | 3686 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3687 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3688 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3689 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3690 | `		return PH7_OK;` |
|      - | 3691 | `	}` |
|      - | 3692 | `	/* Extract the target path */` |
|   1069 | 3693 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3694 | `	/* Try to extract a stream */` |
|   1069 | 3695 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1069 | 3696 | `	if( pStream == 0 ){` |
|    ! 0 | 3697 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3698 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3699 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3700 | `		return PH7_OK;` |
|      - | 3701 | `	}` |
|   1069 | 3702 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3703 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3704 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3705 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3706 | `			);` |
|    ! 0 | 3707 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3708 | `		return PH7_OK;` |
|      - | 3709 | `	}` |
|      - | 3710 | `	/* Allocate a new IO private instance */` |
|   1069 | 3711 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1069 | 3712 | `	if( pDev == 0 ){` |
|    ! 0 | 3713 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3714 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3715 | `		return PH7_OK;` |
|      - | 3716 | `	}` |
|      - | 3717 | `	/* Initialize the structure */` |
|   1069 | 3718 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3719 | `	/* Open the target directory */` |
|   1069 | 3720 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1069 | 3721 | `	if( rc != PH7_OK ){` |
|      - | 3722 | `		/* IO error,return FALSE */` |
|    ! 0 | 3723 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3724 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3725 | `	}else{` |
|      - | 3726 | `		/* Return the handle as a resource */` |
|   1069 | 3727 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3728 | `	}` |
|   1069 | 3729 | `	return PH7_OK;` |
|    537 | 3730 | `}` |
|      - | 3731 | `/*` |
|      - | 3732 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3733 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3734 | ` * Parameters` |
|      - | 3735 | ` *  $filename` |
|      - | 3736 | ` *   The filename being read.` |
|      - | 3737 | ` *  $use_include_path` |
|      - | 3738 | ` *   You can use the optional second parameter and set it to` |
|      - | 3739 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3740 | ` *  $context` |
|      - | 3741 | ` *   A context stream resource.` |
|      - | 3742 | ` * Return` |
|      - | 3743 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3744 | ` */` |
|      - | 3745 | `/*` |
|      - | 3746 | ` * php's IO failures are E_WARNINGs naming the function, the path and the system reason:` |
|      - | 3747 | ` *   file_get_contents(/nope): Failed to open stream: No such file or directory` |
|      - | 3748 | ` * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of` |
|      - | 3749 | ` * them -- wrong severity, wrong text, and no reason. errno still holds the failing` |
|      - | 3750 | ` * syscall's code at this point (a successful call never clears it), which is where the` |
|      - | 3751 | ` * trailing reason comes from.` |
|      - | 3752 | ` */` |
|      2 | 3753 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3754 | `{` |
|      3 | 3755 | `	int use_include  = FALSE;` |
|      - | 3756 | `	const ph7_io_stream *pStream;` |
|      - | 3757 | `	ph7_int64 n,nRead;` |
|      - | 3758 | `	const char *zFile;` |
|      - | 3759 | `	char zBuf[8192];` |
|      - | 3760 | `	void *pHandle;` |
|      - | 3761 | `	int rc,nLen;` |
|      3 | 3762 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3763 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3764 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3765 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3766 | `		return PH7_OK;` |
|      - | 3767 | `	}` |
|      - | 3768 | `	/* Extract the file path */` |
|      3 | 3769 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3770 | `	/* Point to the target IO stream device */` |
|      3 | 3771 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3772 | `	if( pStream == 0 ){` |
|    ! 0 | 3773 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3774 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3775 | `		return PH7_OK;` |
|      - | 3776 | `	}` |
|      3 | 3777 | `	if( nArg > 1 ){` |
|    ! 0 | 3778 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3779 | `	}` |
|      - | 3780 | `	/* Try to open the file in read-only mode */` |
|      4 | 3781 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3782 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3783 | `	if( pHandle == 0 ){` |
|    ! 0 | 3784 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3785 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3786 | `		return PH7_OK;` |
|      - | 3787 | `	}` |
|      - | 3788 | `	/* Perform the requested operation */` |
|      3 | 3789 | `	nRead = 0;` |
|      2 | 3790 | `	for(;;){` |
|      5 | 3791 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3792 | `		if( n < 1 ){` |
|      - | 3793 | `			/* EOF or IO error,break immediately */` |
|      3 | 3794 | `			break;` |
|      - | 3795 | `		}` |
|      - | 3796 | `		/* Output data */` |
|      3 | 3797 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3798 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3799 | `			break;` |
|      - | 3800 | `		}` |
|      - | 3801 | `		/* Increment counter */` |
|      3 | 3802 | `		nRead += n;` |
|      1 | 3803 | `	}` |
|      - | 3804 | `	/* Close the stream */` |
|      3 | 3805 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3806 | `	/* Total number of bytes readen */` |
|      3 | 3807 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3808 | `	return PH7_OK;` |
|      2 | 3809 | `}` |
|      - | 3810 | `/*` |
|      - | 3811 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3812 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3813 | ` *  Reads entire file into a string.` |
|      - | 3814 | ` * Parameters` |
|      - | 3815 | ` *  $filename` |
|      - | 3816 | ` *   The filename being read.` |
|      - | 3817 | ` *  $use_include_path` |
|      - | 3818 | ` *   You can use the optional second parameter and set it to` |
|      - | 3819 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3820 | ` *  $context` |
|      - | 3821 | ` *   A context stream resource.` |
|      - | 3822 | ` *  $offset` |
|      - | 3823 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3824 | ` *  $maxlen` |
|      - | 3825 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3826 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3827 | ` * Return` |
|      - | 3828 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3829 | ` */` |
|   6758 | 3830 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3831 | `{` |
|      - | 3832 | `	const ph7_io_stream *pStream;` |
|      - | 3833 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6763 | 3834 | `	int use_include  = FALSE;` |
|      - | 3835 | `	const char *zFile;` |
|      - | 3836 | `	char zBuf[8192];` |
|      - | 3837 | `	void *pHandle;` |
|      - | 3838 | `	int nLen;` |
|      - | 3839 |  |
|   6763 | 3840 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3841 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3842 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3843 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3844 | `		return PH7_OK;` |
|      - | 3845 | `	}` |
|      - | 3846 | `	/* Extract the file path */` |
|   6763 | 3847 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3848 | `	/* Point to the target IO stream device */` |
|   6763 | 3849 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6763 | 3850 | `	if( pStream == 0 ){` |
|    ! 0 | 3851 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3852 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3853 | `		return PH7_OK;` |
|      - | 3854 | `	}` |
|   6763 | 3855 | `	nMaxlen = -1;` |
|   6763 | 3856 | `	if( nArg > 1 ){` |
|      5 | 3857 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3858 | `	}` |
|      - | 3859 | `	/* Try to open the file in read-only mode */` |
|   6763 | 3860 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6763 | 3861 | `	if( pHandle == 0 ){` |
|      3 | 3862 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 3863 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3864 | `		return PH7_OK;` |
|      - | 3865 | `	}` |
|   6761 | 3866 | `	if( nArg > 3 ){` |
|      - | 3867 | `		/* Extract the offset */` |
|      5 | 3868 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3869 | `		if( n > 0 ){` |
|    ! 0 | 3870 | `			if( pStream->xSeek ){` |
|      - | 3871 | `				/* Seek to the desired offset */` |
|    ! 0 | 3872 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3873 | `			}` |
|    ! 0 | 3874 | `		}` |
|      5 | 3875 | `		if( nArg > 4 ){` |
|      - | 3876 | `			/* Maximum data to read */` |
|      5 | 3877 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3878 | `		}` |
|      2 | 3879 | `	}` |
|      - | 3880 | `	/* Perform the requested operation */` |
|   6761 | 3881 | `	nRead = 0;` |
|   6753 | 3882 | `	for(;;){` |
|  20267 | 3883 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6756 | 3884 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  13511 | 3885 | `		if( n < 1 ){` |
|      - | 3886 | `			/* EOF or IO error,break immediately */` |
|   6759 | 3887 | `			break;` |
|      - | 3888 | `		}` |
|      - | 3889 | `		/* Append data */` |
|   6757 | 3890 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3891 | `		/* Increment read counter */` |
|   6757 | 3892 | `		nRead += n;` |
|   6757 | 3893 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3894 | `			/* Read limit reached */` |
|      3 | 3895 | `			break;` |
|      - | 3896 | `		}` |
|      5 | 3897 | `	}` |
|      - | 3898 | `	/* Close the stream */` |
|   6761 | 3899 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3900 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|      - | 3901 | `	 * open failure, handled above); the read loop never set a string result, so` |
|      - | 3902 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   6761 | 3903 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      6 | 3904 | `		ph7_result_string(pCtx,"",0);` |
|      2 | 3905 | `	}` |
|   6761 | 3906 | `	return PH7_OK;` |
|   3384 | 3907 | `}` |
|      - | 3908 | `/*` |
|      - | 3909 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3910 | ` *  Write a string to a file.` |
|      - | 3911 | ` * Parameters` |
|      - | 3912 | ` *  $filename` |
|      - | 3913 | ` *  Path to the file where to write the data.` |
|      - | 3914 | ` * $data` |
|      - | 3915 | ` *  The data to write(Must be a string).` |
|      - | 3916 | ` * $flags` |
|      - | 3917 | ` *  The value of flags can be any combination of the following` |
|      - | 3918 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3919 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3920 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3921 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3922 | ` * context` |
|      - | 3923 | ` *  A context stream resource.` |
|      - | 3924 | ` * Return` |
|      - | 3925 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3926 | ` */` |
|  13714 | 3927 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3928 | `{` |
|  13719 | 3929 | `	int use_include  = FALSE;` |
|      - | 3930 | `	const ph7_io_stream *pStream;` |
|      - | 3931 | `	const char *zFile;` |
|      - | 3932 | `	const char *zData;` |
|      - | 3933 | `	int iOpenFlags;` |
|      - | 3934 | `	void *pHandle;` |
|      - | 3935 | `	int iFlags;` |
|      - | 3936 | `	int nLen;` |
|      - | 3937 |  |
|  13719 | 3938 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3939 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3940 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3941 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3942 | `		return PH7_OK;` |
|      - | 3943 | `	}` |
|      - | 3944 | `	/* Extract the file path */` |
|  13719 | 3945 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3946 | `	/* Point to the target IO stream device */` |
|  13719 | 3947 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13719 | 3948 | `	if( pStream == 0 ){` |
|    ! 0 | 3949 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3950 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3951 | `		return PH7_OK;` |
|      - | 3952 | `	}` |
|      - | 3953 | `	/* Data to write */` |
|  13719 | 3954 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3955 | `	/* Try to open the file in read-write mode */` |
|  13719 | 3956 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3957 | `	/* Extract the flags */` |
|  13719 | 3958 | `	iFlags = 0;` |
|  13719 | 3959 | `	if( nArg > 2 ){` |
|    ! 0 | 3960 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3961 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3962 | `			use_include = TRUE;` |
|    ! 0 | 3963 | `		}` |
|    ! 0 | 3964 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3965 | `			/* If the file already exists, append the data to the file` |
|      - | 3966 | `			 * instead of overwriting it.` |
|      - | 3967 | `			 */` |
|    ! 0 | 3968 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3969 | `			/* Append mode */` |
|    ! 0 | 3970 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3971 | `		}` |
|    ! 0 | 3972 | `	}` |
|  20576 | 3973 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6857 | 3974 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13719 | 3975 | `	if( pHandle == 0 ){` |
|    ! 0 | 3976 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3977 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3978 | `		return PH7_OK;` |
|      - | 3979 | `	}` |
|  13719 | 3980 | `	if( nLen < 1 ){` |
|      - | 3981 | `		/* Empty data, file is created/truncated */` |
|     10 | 3982 | `		ph7_result_int64(pCtx,0);` |
|     10 | 3983 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     10 | 3984 | `		return PH7_OK;` |
|      - | 3985 | `	}` |
|  13711 | 3986 | `	if( pStream->xWrite ){` |
|      - | 3987 | `		ph7_int64 n;` |
|  13711 | 3988 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 3989 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3990 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3991 | `		}` |
|      - | 3992 | `		/* Perform the write operation */` |
|  13711 | 3993 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13711 | 3994 | `		if( n < 0 ){` |
|      - | 3995 | `			/* IO error,return FALSE */` |
|    ! 0 | 3996 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3997 | `		}else{` |
|      - | 3998 | `			/* Total number of bytes written */` |
|  13711 | 3999 | `			ph7_result_int64(pCtx,n);` |
|      - | 4000 | `		}` |
|   6858 | 4001 | `	}else{` |
|      - | 4002 | `		/* Read-only stream */` |
|    ! 0 | 4003 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 4004 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 4005 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 4006 | `			);` |
|    ! 0 | 4007 | `		ph7_result_bool(pCtx,0);` |
|      - | 4008 | `	}` |
|      - | 4009 | `	/* Close the handle */` |
|  13711 | 4010 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13711 | 4011 | `	return PH7_OK;` |
|   6862 | 4012 | `}` |
|      - | 4013 | `/*` |
|      - | 4014 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 4015 | ` *  Reads entire file into an array.` |
|      - | 4016 | ` * Parameters` |
|      - | 4017 | ` *  $filename` |
|      - | 4018 | ` *   The filename being read.` |
|      - | 4019 | ` *  $flags` |
|      - | 4020 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 4021 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 4022 | ` *       Search for the file in the include_path.` |
|      - | 4023 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 4024 | ` *       Do not add newline at the end of each array element` |
|      - | 4025 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 4026 | ` *       Skip empty lines` |
|      - | 4027 | ` *  $context` |
|      - | 4028 | ` *   A context stream resource.` |
|      - | 4029 | ` * Return` |
|      - | 4030 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 4031 | ` */` |
|     10 | 4032 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4033 | `{` |
|      - | 4034 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 4035 | `	ph7_value *pArray,*pLine;` |
|      - | 4036 | `	const ph7_io_stream *pStream;` |
|     13 | 4037 | `	int use_include = 0;` |
|      - | 4038 | `	io_private *pDev;` |
|      - | 4039 | `	ph7_int64 n;` |
|      - | 4040 | `	int iFlags;` |
|      - | 4041 | `	int nLen;` |
|      - | 4042 |  |
|     13 | 4043 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4044 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4045 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4046 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4047 | `		return PH7_OK;` |
|      - | 4048 | `	}` |
|      - | 4049 | `	/* Extract the file path */` |
|     13 | 4050 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4051 | `	/* Point to the target IO stream device */` |
|     13 | 4052 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     13 | 4053 | `	if( pStream == 0 ){` |
|    ! 0 | 4054 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4055 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4056 | `		return PH7_OK;` |
|      - | 4057 | `	}` |
|      - | 4058 | `	/* Allocate a new IO private instance */` |
|     13 | 4059 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     13 | 4060 | `	if( pDev == 0 ){` |
|    ! 0 | 4061 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4062 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4063 | `		return PH7_OK;` |
|      - | 4064 | `	}` |
|      - | 4065 | `	/* Initialize the structure */` |
|     13 | 4066 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     13 | 4067 | `	iFlags = 0;` |
|     13 | 4068 | `	if( nArg > 1 ){` |
|    ! 0 | 4069 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 4070 | `	}` |
|      8 | 4071 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 4072 | `		use_include = TRUE;` |
|    ! 0 | 4073 | `	}` |
|      - | 4074 | `	/* Create the array and the working value */` |
|     13 | 4075 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 4076 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     13 | 4077 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 4078 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4079 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4080 | `		return PH7_OK;` |
|      - | 4081 | `	}` |
|      - | 4082 | `	/* Try to open the file in read-only mode */` |
|     13 | 4083 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|     13 | 4084 | `	if( pDev->pHandle == 0 ){` |
|     10 | 4085 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     10 | 4086 | `		ph7_result_bool(pCtx,0);` |
|      - | 4087 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 4088 | `		 * as soon we return from this function.` |
|      - | 4089 | `		 */` |
|     10 | 4090 | `		return PH7_OK;` |
|      - | 4091 | `	}` |
|      - | 4092 | `	/* Perform the requested operation */` |
|      3 | 4093 | `	for(;;){` |
|      - | 4094 | `		/* Try to extract a line */` |
|      7 | 4095 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 4096 | `		if( n < 1 ){` |
|      - | 4097 | `			/* EOF or IO error */` |
|      3 | 4098 | `			break;` |
|      - | 4099 | `		}` |
|      - | 4100 | `		/* Reset the cursor */` |
|      5 | 4101 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 4102 | `		/* Remove line ending if requested by the caller */` |
|      5 | 4103 | `		zPtr = zBuf;` |
|      5 | 4104 | `		zEnd = &zBuf[n];` |
|      5 | 4105 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 4106 | `			/* Ignore trailig lines */` |
|    ! 0 | 4107 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 4108 | `#ifdef __WINNT__` |
|      - | 4109 | `				\|\| zEnd[-1] == '\r'` |
|      - | 4110 | `#endif` |
|      - | 4111 | `				)){` |
|    ! 0 | 4112 | `					n--;` |
|    ! 0 | 4113 | `					zEnd--;` |
|    ! 0 | 4114 | `			}` |
|    ! 0 | 4115 | `		}` |
|      3 | 4116 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 4117 | `			/* Ignore empty lines */` |
|    ! 0 | 4118 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 4119 | `				zPtr++;` |
|    ! 0 | 4120 | `			}` |
|    ! 0 | 4121 | `			if( zPtr >= zEnd ){` |
|      - | 4122 | `				/* Empty line */` |
|    ! 0 | 4123 | `				continue;` |
|      - | 4124 | `			}` |
|    ! 0 | 4125 | `		}` |
|      5 | 4126 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 4127 | `		/* Insert line */` |
|      5 | 4128 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 4129 | `	}` |
|      - | 4130 | `	/* Close the stream */` |
|      3 | 4131 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4132 | `	/* Release the io_private instance */` |
|      3 | 4133 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4134 | `	/* Return the created array */` |
|      3 | 4135 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4136 | `	return PH7_OK;` |
|      8 | 4137 | `}` |
|      - | 4138 | `/*` |
|      - | 4139 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 4140 | ` *  Makes a copy of the file source to dest.` |
|      - | 4141 | ` * Parameters` |
|      - | 4142 | ` *  $source` |
|      - | 4143 | ` *   Path to the source file.` |
|      - | 4144 | ` *  $dest` |
|      - | 4145 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 4146 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 4147 | ` *  $context` |
|      - | 4148 | ` *   A context stream resource.` |
|      - | 4149 | ` * Return` |
|      - | 4150 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4151 | ` */` |
|      4 | 4152 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4153 | `{` |
|      - | 4154 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 4155 | `	const char *zFile;` |
|      - | 4156 | `	char zBuf[8192];` |
|      - | 4157 | `	void *pIn,*pOut;` |
|      - | 4158 | `	ph7_int64 n;` |
|      - | 4159 | `	int nLen;` |
|      6 | 4160 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 4161 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4162 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 4163 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4164 | `		return PH7_OK;` |
|      - | 4165 | `	}` |
|      - | 4166 | `	/* Extract the source name */` |
|      6 | 4167 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4168 | `	/* Point to the target IO stream device */` |
|      6 | 4169 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4170 | `	if( pSin == 0 ){` |
|    ! 0 | 4171 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4172 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4173 | `		return PH7_OK;` |
|      - | 4174 | `	}` |
|      - | 4175 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4176 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4177 | `	if( pIn == 0 ){` |
|      3 | 4178 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 4179 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4180 | `		return PH7_OK;` |
|      - | 4181 | `	}` |
|      - | 4182 | `	/* Extract the destination name */` |
|      3 | 4183 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4184 | `	/* Point to the target IO stream device */` |
|      3 | 4185 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4186 | `	if( pSout == 0 ){` |
|    ! 0 | 4187 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4189 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4190 | `		return PH7_OK;` |
|      - | 4191 | `	}` |
|      3 | 4192 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4193 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4194 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4195 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4196 | `			);` |
|    ! 0 | 4197 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4198 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4199 | `		return PH7_OK;` |
|      - | 4200 | `	}` |
|      - | 4201 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4202 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4203 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4204 | `	if( pOut == 0 ){` |
|    ! 0 | 4205 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 4206 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4207 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4208 | `		return PH7_OK;` |
|      - | 4209 | `	}` |
|      - | 4210 | `	/* Perform the requested operation */` |
|      2 | 4211 | `	for(;;){` |
|      - | 4212 | `		/* Read from source */` |
|      5 | 4213 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4214 | `		if( n < 1 ){` |
|      - | 4215 | `			/* EOF or IO error,break immediately */` |
|      3 | 4216 | `			break;` |
|      - | 4217 | `		}` |
|      - | 4218 | `		/* Write to dest */` |
|      3 | 4219 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4220 | `		if( n < 1 ){` |
|      - | 4221 | `			/* IO error,break immediately */` |
|    ! 0 | 4222 | `			break;` |
|      - | 4223 | `		}` |
|      1 | 4224 | `	}` |
|      - | 4225 | `	/* Close the streams */` |
|      3 | 4226 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4227 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4228 | `	/* Return TRUE */` |
|      3 | 4229 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4230 | `	return PH7_OK;` |
|      4 | 4231 | `}` |
|      - | 4232 | `/*` |
|      - | 4233 | ` * array fstat(resource $handle)` |
|      - | 4234 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4235 | ` * Parameters` |
|      - | 4236 | ` *  $handle` |
|      - | 4237 | ` *   The file pointer.` |
|      - | 4238 | ` * Return` |
|      - | 4239 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4240 | ` */` |
|      2 | 4241 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4242 | `{` |
|      - | 4243 | `	ph7_value *pArray,*pValue;` |
|      - | 4244 | `	const ph7_io_stream *pStream;` |
|      - | 4245 | `	io_private *pDev;` |
|      3 | 4246 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4247 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4248 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4249 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4250 | `		return PH7_OK;` |
|      - | 4251 | `	}` |
|      - | 4252 | `	/* Extract our private data */` |
|      3 | 4253 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4254 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4255 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4256 | `		/* Expecting an IO handle */` |
|    ! 0 | 4257 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4258 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4259 | `		return PH7_OK;` |
|      - | 4260 | `	}` |
|      - | 4261 | `	/* Point to the target IO stream device */` |
|      3 | 4262 | `	pStream = pDev->pStream;` |
|      3 | 4263 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4264 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4265 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4266 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4267 | `			);` |
|    ! 0 | 4268 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4269 | `		return PH7_OK;` |
|      - | 4270 | `	}` |
|      - | 4271 | `	/* Create the array and the working value */` |
|      3 | 4272 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4273 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4274 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4275 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4276 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4277 | `		return PH7_OK;` |
|      - | 4278 | `	}` |
|      - | 4279 | `	/* Perform the requested operation */` |
|      3 | 4280 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4281 | `	/* Return the freshly created array */` |
|      3 | 4282 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4283 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4284 | `	 * released automatically as soon we return from this function.` |
|      - | 4285 | `	 */` |
|      3 | 4286 | `	return PH7_OK;` |
|      2 | 4287 | `}` |
|      - | 4288 | `/*` |
|      - | 4289 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4290 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4291 | ` * Parameters` |
|      - | 4292 | ` *  $handle` |
|      - | 4293 | ` *   The file pointer.` |
|      - | 4294 | ` *  $string` |
|      - | 4295 | ` *   The string that is to be written.` |
|      - | 4296 | ` *  $length` |
|      - | 4297 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4298 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4299 | ` * Return` |
|      - | 4300 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4301 | ` */` |
|     44 | 4302 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4303 | `{` |
|      - | 4304 | `	const ph7_io_stream *pStream;` |
|      - | 4305 | `	const char *zString;` |
|      - | 4306 | `	io_private *pDev;` |
|      - | 4307 | `	int nLen,n;` |
|     46 | 4308 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4309 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4310 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4311 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4312 | `		return PH7_OK;` |
|      - | 4313 | `	}` |
|      - | 4314 | `	/* Extract our private data */` |
|     46 | 4315 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4316 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     46 | 4317 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4318 | `		/* Expecting an IO handle */` |
|    ! 0 | 4319 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4320 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4321 | `		return PH7_OK;` |
|      - | 4322 | `	}` |
|      - | 4323 | `	/* Point to the target IO stream device */` |
|     46 | 4324 | `	pStream = pDev->pStream;` |
|     46 | 4325 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4326 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4327 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4328 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4329 | `			);` |
|    ! 0 | 4330 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4331 | `		return PH7_OK;` |
|      - | 4332 | `	}` |
|      - | 4333 | `	/* Extract the data to write */` |
|     46 | 4334 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     46 | 4335 | `	if( nArg > 2 ){` |
|      - | 4336 | `		/* Maximum data length to write */` |
|    ! 0 | 4337 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4338 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4339 | `			nLen = n;` |
|    ! 0 | 4340 | `		}` |
|    ! 0 | 4341 | `	}` |
|     46 | 4342 | `	if( nLen < 1 ){` |
|      - | 4343 | `		/* Nothing to write */` |
|    ! 0 | 4344 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4345 | `		return PH7_OK;` |
|      - | 4346 | `	}` |
|      - | 4347 | `	/* Perform the requested operation */` |
|     46 | 4348 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|     46 | 4349 | `	if( n <  0 ){` |
|      - | 4350 | `		/* IO error,return FALSE */` |
|    ! 0 | 4351 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4352 | `	}else{` |
|      - | 4353 | `		/* #Bytes written */` |
|     46 | 4354 | `		ph7_result_int(pCtx,n);` |
|      - | 4355 | `	}` |
|     46 | 4356 | `	return PH7_OK;` |
|     24 | 4357 | `}` |
|      - | 4358 | `/*` |
|      - | 4359 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4360 | ` *  Portable advisory file locking.` |
|      - | 4361 | ` * Parameters` |
|      - | 4362 | ` *  $handle` |
|      - | 4363 | ` *   The file pointer.` |
|      - | 4364 | ` *  $operation` |
|      - | 4365 | ` *   operation is one of the following:` |
|      - | 4366 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4367 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4368 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4369 | ` * Return` |
|      - | 4370 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4371 | ` */` |
|      4 | 4372 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 4373 | `{` |
|      - | 4374 | `	const ph7_io_stream *pStream;` |
|      - | 4375 | `	io_private *pDev;` |
|      - | 4376 | `	int nLock;` |
|      - | 4377 | `	int rc;` |
|      4 | 4378 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4379 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4380 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4381 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4382 | `		return PH7_OK;` |
|      - | 4383 | `	}` |
|      - | 4384 | `	/* Extract our private data */` |
|      4 | 4385 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4386 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      4 | 4387 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4388 | `		/*Expecting an IO handle */` |
|    ! 0 | 4389 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4390 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4391 | `		return PH7_OK;` |
|      - | 4392 | `	}` |
|      - | 4393 | `	/* Point to the target IO stream device */` |
|      4 | 4394 | `	pStream = pDev->pStream;` |
|      4 | 4395 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4396 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4397 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4398 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4399 | `			);` |
|    ! 0 | 4400 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4401 | `		return PH7_OK;` |
|      - | 4402 | `	}` |
|      - | 4403 | `	/* Requested lock operation */` |
|      4 | 4404 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4405 | `	/*` |
|      - | 4406 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally \|LOCK_NB=4)` |
|      - | 4407 | `	 * into the xLock() vtable contract, which is a PUBLIC C API and stays as it is:` |
|      - | 4408 | `	 * negative = unlock, 1 = exclusive, anything else = shared.` |
|      - | 4409 | `	 */` |
|      - | 4410 | `	{` |
|      4 | 4411 | `		int iOp = nLock & ~4 /* strip LOCK_NB */;` |
|      4 | 4412 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      2 | 4413 | `			nLock = -1;` |
|      3 | 4414 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|      2 | 4415 | `			nLock = 1;` |
|      1 | 4416 | `		}else{` |
|    ! 0 | 4417 | `			nLock = 0; /* LOCK_SH */` |
|      - | 4418 | `		}` |
|      - | 4419 | `	}` |
|      - | 4420 | `	/* Lock operation */` |
|      4 | 4421 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4422 | `	/* IO result */` |
|      4 | 4423 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 4424 | `	return PH7_OK;` |
|      2 | 4425 | `}` |
|      - | 4426 | `/*` |
|      - | 4427 | ` * int fpassthru(resource $handle)` |
|      - | 4428 | ` *  Output all remaining data on a file pointer.` |
|      - | 4429 | ` * Parameters` |
|      - | 4430 | ` *  $handle` |
|      - | 4431 | ` *   The file pointer.` |
|      - | 4432 | ` * Return` |
|      - | 4433 | ` *  Total number of characters read from handle and passed through` |
|      - | 4434 | ` *  to the output on success or FALSE on failure.` |
|      - | 4435 | ` */` |
|      2 | 4436 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4437 | `{` |
|      - | 4438 | `	const ph7_io_stream *pStream;` |
|      - | 4439 | `	io_private *pDev;` |
|      - | 4440 | `	ph7_int64 n,nRead;` |
|      - | 4441 | `	char zBuf[8192];` |
|      - | 4442 | `	int rc;` |
|      3 | 4443 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4444 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4445 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4446 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4447 | `		return PH7_OK;` |
|      - | 4448 | `	}` |
|      - | 4449 | `	/* Extract our private data */` |
|      3 | 4450 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4451 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4452 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4453 | `		/*Expecting an IO handle */` |
|    ! 0 | 4454 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4455 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4456 | `		return PH7_OK;` |
|      - | 4457 | `	}` |
|      - | 4458 | `	/* Point to the target IO stream device */` |
|      3 | 4459 | `	pStream = pDev->pStream;` |
|      3 | 4460 | `	if( pStream == 0  ){` |
|    ! 0 | 4461 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4462 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4463 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4464 | `			);` |
|    ! 0 | 4465 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4466 | `		return PH7_OK;` |
|      - | 4467 | `	}` |
|      - | 4468 | `	/* Perform the requested operation */` |
|      3 | 4469 | `	nRead = 0;` |
|      2 | 4470 | `	for(;;){` |
|      5 | 4471 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4472 | `		if( n < 1 ){` |
|      - | 4473 | `			/* Error or EOF */` |
|      3 | 4474 | `			break;` |
|      - | 4475 | `		}` |
|      - | 4476 | `		/* Increment the read counter */` |
|      3 | 4477 | `		nRead += n;` |
|      - | 4478 | `		/* Output data */` |
|      3 | 4479 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4480 | `		if( rc == PH7_ABORT ){` |
|      - | 4481 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4482 | `			break;` |
|      - | 4483 | `		}` |
|      1 | 4484 | `	}` |
|      - | 4485 | `	/* Total number of bytes readen */` |
|      3 | 4486 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4487 | `	return PH7_OK;` |
|      2 | 4488 | `}` |
|      - | 4489 | `/* CSV reader/writer private data */` |
|      - | 4490 | `struct csv_data` |
|      - | 4491 | `{` |
|      - | 4492 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4493 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4494 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4495 | `	int iCount;       /* Counter */` |
|      - | 4496 | `};` |
|      - | 4497 | `/*` |
|      - | 4498 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4499 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4500 | ` * associated data.` |
|      - | 4501 | ` */` |
|      6 | 4502 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4503 | `{` |
|      7 | 4504 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4505 | `	const char *zData;` |
|      - | 4506 | `	int nLen,c2;` |
|      - | 4507 | `	sxu32 n;` |
|      - | 4508 | `	/* Point to the raw data */` |
|      7 | 4509 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4510 | `	if( nLen < 1 ){` |
|      - | 4511 | `		/* Nothing to write */` |
|    ! 0 | 4512 | `		return PH7_OK;` |
|      - | 4513 | `	}` |
|      7 | 4514 | `	if( pData->iCount > 0 ){` |
|      - | 4515 | `		/* Write the delimiter */` |
|      5 | 4516 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4517 | `	}` |
|      7 | 4518 | `	n = 1;` |
|      7 | 4519 | `	c2 = 0;` |
|     10 | 4520 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4521 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4522 | `			c2 = 1;` |
|    ! 0 | 4523 | `			if( n == 0 ){` |
|    ! 0 | 4524 | `				c2 = 2;` |
|    ! 0 | 4525 | `			}` |
|      - | 4526 | `			/* Write the enclosure */` |
|    ! 0 | 4527 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4528 | `			if( c2 > 1 ){` |
|    ! 0 | 4529 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4530 | `			}` |
|    ! 0 | 4531 | `	}` |
|      - | 4532 | `	/* Write the data */` |
|      7 | 4533 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4534 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4535 | `		return PH7_ABORT;` |
|      - | 4536 | `	}` |
|      7 | 4537 | `	if( c2 > 0 ){` |
|      - | 4538 | `		/* Write the enclosure */` |
|    ! 0 | 4539 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4540 | `		if( c2 > 1 ){` |
|    ! 0 | 4541 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4542 | `		}` |
|    ! 0 | 4543 | `	}` |
|      7 | 4544 | `	pData->iCount++;` |
|      7 | 4545 | `	return PH7_OK;` |
|      4 | 4546 | `}` |
|      - | 4547 | `/*` |
|      - | 4548 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4549 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4550 | ` * Parameters` |
|      - | 4551 | ` *  $handle` |
|      - | 4552 | ` *   Open file handle.` |
|      - | 4553 | ` * $fields` |
|      - | 4554 | ` *   An array of values.` |
|      - | 4555 | ` * $delimiter` |
|      - | 4556 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4557 | ` * $enclosure` |
|      - | 4558 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4559 | ` */` |
|      2 | 4560 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4561 | `{` |
|      - | 4562 | `	const ph7_io_stream *pStream;` |
|      - | 4563 | `	struct csv_data sCsv;` |
|      - | 4564 | `	io_private *pDev;` |
|      - | 4565 | `	char *zEol;` |
|      - | 4566 | `	int eolen;` |
|      3 | 4567 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4568 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4569 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4570 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4571 | `		return PH7_OK;` |
|      - | 4572 | `	}` |
|      - | 4573 | `	/* Extract our private data */` |
|      3 | 4574 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4575 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4576 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4577 | `		/*Expecting an IO handle */` |
|    ! 0 | 4578 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4579 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4580 | `		return PH7_OK;` |
|      - | 4581 | `	}` |
|      - | 4582 | `	/* Point to the target IO stream device */` |
|      3 | 4583 | `	pStream = pDev->pStream;` |
|      3 | 4584 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4585 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4586 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4587 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4588 | `			);` |
|    ! 0 | 4589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4590 | `		return PH7_OK;` |
|      - | 4591 | `	}` |
|      - | 4592 | `	/* Set default csv separator */` |
|      3 | 4593 | `	sCsv.delimiter = ',';` |
|      3 | 4594 | `	sCsv.enclosure = '"';` |
|      3 | 4595 | `	sCsv.pDev = pDev;` |
|      3 | 4596 | `	sCsv.iCount = 0;` |
|      3 | 4597 | `	if( nArg > 2 ){` |
|      - | 4598 | `		/* User delimiter */` |
|      - | 4599 | `		const char *z;` |
|      - | 4600 | `		int n;` |
|      3 | 4601 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4602 | `		if( n > 0 ){` |
|      3 | 4603 | `			sCsv.delimiter = z[0];` |
|      1 | 4604 | `		}` |
|      3 | 4605 | `		if( nArg > 3 ){` |
|      3 | 4606 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4607 | `			if( n > 0 ){` |
|      3 | 4608 | `				sCsv.enclosure = z[0];` |
|      1 | 4609 | `			}` |
|      1 | 4610 | `		}` |
|      1 | 4611 | `	}` |
|      - | 4612 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4613 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4614 | `	/* Write a line ending */` |
|      - | 4615 | `#ifdef __WINNT__` |
|      1 | 4616 | `	zEol = "\r\n";` |
|      1 | 4617 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4618 | `#else` |
|      - | 4619 | `	/* Assume UNIX LF */` |
|      2 | 4620 | `	zEol = "\n";` |
|      2 | 4621 | `	eolen = (int)sizeof(char);` |
|      - | 4622 | `#endif` |
|      3 | 4623 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4624 | `	return PH7_OK;` |
|      2 | 4625 | `}` |
|      - | 4626 | `/*` |
|      - | 4627 | ` * fprintf,vfprintf private data.` |
|      - | 4628 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4629 | ` * input consumer callback defined below.` |
|      - | 4630 | ` */` |
|      - | 4631 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4632 | `struct fprintf_data` |
|      - | 4633 | `{` |
|      - | 4634 | `	io_private *pIO;        /* IO stream */` |
|      - | 4635 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4636 | `};` |
|      - | 4637 | `/*` |
|      - | 4638 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4639 | ` */` |
|     30 | 4640 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4641 | `{` |
|     31 | 4642 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4643 | `	ph7_int64 n;` |
|      - | 4644 | `	/* Write the formatted data */` |
|     31 | 4645 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     31 | 4646 | `	if( n < 1 ){` |
|    ! 0 | 4647 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4648 | `		/* IO error,abort immediately */` |
|    ! 0 | 4649 | `		return SXERR_ABORT;` |
|      - | 4650 | `	}` |
|      - | 4651 | `	/* Increment counter */` |
|     31 | 4652 | `	pFdata->nCount += n;` |
|     31 | 4653 | `	return PH7_OK;` |
|     16 | 4654 | `}` |
|      - | 4655 | `/*` |
|      - | 4656 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4657 | ` *  Write a formatted string to a stream.` |
|      - | 4658 | ` * Parameters` |
|      - | 4659 | ` *  $handle` |
|      - | 4660 | ` *   The file pointer.` |
|      - | 4661 | ` *  $format` |
|      - | 4662 | ` *   String format (see sprintf()).` |
|      - | 4663 | ` * Return` |
|      - | 4664 | ` *  The length of the written string.` |
|      - | 4665 | ` */` |
|     18 | 4666 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4667 | `{` |
|      - | 4668 | `	fprintf_data sFdata;` |
|      - | 4669 | `	const char *zFormat;` |
|      - | 4670 | `	io_private *pDev;` |
|      - | 4671 | `	int nLen;` |
|     19 | 4672 | `	if( nArg < 2 ){` |
|    ! 0 | 4673 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4674 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4675 | `		return PH7_OK;` |
|      - | 4676 | `	}` |
|      - | 4677 | `	{` |
|      - | 4678 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     19 | 4679 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     19 | 4680 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 4681 | `			return rcs;` |
|      - | 4682 | `		}` |
|      - | 4683 | `	}` |
|      - | 4684 | `	/* Extract our private data */` |
|     19 | 4685 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4686 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     19 | 4687 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4688 | `		/*Expecting an IO handle */` |
|    ! 0 | 4689 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4690 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4691 | `		return PH7_OK;` |
|      - | 4692 | `	}` |
|      - | 4693 | `	/* Point to the target IO stream device */` |
|     19 | 4694 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4695 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4696 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4697 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4698 | `			);` |
|    ! 0 | 4699 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4700 | `		return PH7_OK;` |
|      - | 4701 | `	}` |
|      - | 4702 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 4703 | `	{` |
|     19 | 4704 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     19 | 4705 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4706 | `			return rcf;` |
|      - | 4707 | `		}` |
|      - | 4708 | `	}` |
|      - | 4709 | `	/* Extract the string format (scalars/null coerce). */` |
|     19 | 4710 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     19 | 4711 | `	if( nLen < 1 ){` |
|      - | 4712 | `		/* Empty string,return zero */` |
|    ! 0 | 4713 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4714 | `		return PH7_OK;` |
|      - | 4715 | `	}` |
|      - | 4716 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4717 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4718 | `	{` |
|     19 | 4719 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     19 | 4720 | `		if( rcv != PH7_OK ){` |
|      3 | 4721 | `			return rcv;` |
|      - | 4722 | `		}` |
|      - | 4723 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output.` |
|      - | 4724 | `		 * fprintf's values start at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     17 | 4725 | `		rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     17 | 4726 | `		if( rcv != PH7_OK ){` |
|      3 | 4727 | `			return rcv;` |
|      - | 4728 | `		}` |
|      - | 4729 | `	}` |
|      - | 4730 | `	/* Prepare our private data */` |
|     15 | 4731 | `	sFdata.nCount = 0;` |
|     15 | 4732 | `	sFdata.pIO = pDev;` |
|      - | 4733 | `	/* Format the string */` |
|     15 | 4734 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4735 | `	/* Return total number of bytes written */` |
|     15 | 4736 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     15 | 4737 | `	return PH7_OK;` |
|     10 | 4738 | `}` |
|      - | 4739 | `/*` |
|      - | 4740 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4741 | ` *  Write a formatted string to a stream.` |
|      - | 4742 | ` * Parameters` |
|      - | 4743 | ` *  $handle` |
|      - | 4744 | ` *   The file pointer.` |
|      - | 4745 | ` *  $format` |
|      - | 4746 | ` *   String format (see sprintf()).` |
|      - | 4747 | ` * $args` |
|      - | 4748 | ` *   User arguments.` |
|      - | 4749 | ` * Return` |
|      - | 4750 | ` *  The length of the written string.` |
|      - | 4751 | ` */` |
|      6 | 4752 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4753 | `{` |
|      - | 4754 | `	fprintf_data sFdata;` |
|      - | 4755 | `	const char *zFormat;` |
|      - | 4756 | `	ph7_hashmap *pMap;` |
|      - | 4757 | `	io_private *pDev;` |
|      - | 4758 | `	SySet sArg;` |
|      - | 4759 | `	int n,nLen;` |
|      7 | 4760 | `	if( nArg < 3 ){` |
|    ! 0 | 4761 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4762 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4763 | `		return PH7_OK;` |
|      - | 4764 | `	}` |
|      - | 4765 | `	{` |
|      - | 4766 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|      7 | 4767 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|      7 | 4768 | `		if( rcs != PH7_OK ){` |
|      3 | 4769 | `			return rcs;` |
|      - | 4770 | `		}` |
|      - | 4771 | `	}` |
|      - | 4772 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 4773 | `	{` |
|      5 | 4774 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      5 | 4775 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4776 | `			return rcf;` |
|      - | 4777 | `		}` |
|      - | 4778 | `	}` |
|      5 | 4779 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 4780 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4781 | `		char zBuf[64];` |
|    ! 0 | 4782 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4783 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 4784 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 4785 | `	}` |
|      - | 4786 | `	/* Extract our private data */` |
|      5 | 4787 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4788 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4789 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4790 | `		/*Expecting an IO handle */` |
|    ! 0 | 4791 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4792 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4793 | `		return PH7_OK;` |
|      - | 4794 | `	}` |
|      - | 4795 | `	/* Point to the target IO stream device */` |
|      5 | 4796 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4797 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4798 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4799 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4800 | `			);` |
|    ! 0 | 4801 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4802 | `		return PH7_OK;` |
|      - | 4803 | `	}` |
|      - | 4804 | `	/* Extract the string format */` |
|      5 | 4805 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      5 | 4806 | `	if( nLen < 1 ){` |
|      - | 4807 | `		/* Empty string,return zero */` |
|    ! 0 | 4808 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4809 | `		return PH7_OK;` |
|      - | 4810 | `	}` |
|      - | 4811 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4812 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4813 | `	{` |
|      5 | 4814 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      5 | 4815 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4816 | `			return rcv;` |
|      - | 4817 | `		}` |
|      - | 4818 | `	}` |
|      - | 4819 | `	/* Point to hashmap */` |
|      5 | 4820 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4821 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output. */` |
|      - | 4822 | `	{` |
|      5 | 4823 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      5 | 4824 | `		if( rcc != PH7_OK ){` |
|      3 | 4825 | `			return rcc;` |
|      - | 4826 | `		}` |
|      - | 4827 | `	}` |
|      - | 4828 | `	/* Extract arguments from the hashmap */` |
|      3 | 4829 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4830 | `	/* Prepare our private data */` |
|      3 | 4831 | `	sFdata.nCount = 0;` |
|      3 | 4832 | `	sFdata.pIO = pDev;` |
|      - | 4833 | `	/* Format the string */` |
|      3 | 4834 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4835 | `	/* Return total number of bytes written*/` |
|      3 | 4836 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4837 | `	SySetRelease(&sArg);` |
|      3 | 4838 | `	return PH7_OK;` |
|      4 | 4839 | `}` |
|      - | 4840 | `/*` |
|      - | 4841 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4842 | ` * According to the PHP reference manual:` |
|      - | 4843 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4844 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4845 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4846 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4847 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4848 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4849 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4850 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4851 | ` *         exist, attempt to create it.` |
|      - | 4852 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4853 | ` *          not exist, attempt to create it.` |
|      - | 4854 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4855 | ` *         already exists,` |
|      - | 4856 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4857 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4858 | ` *         the underlying open(2) system call.` |
|      - | 4859 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4860 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4861 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4862 | ` *          is positioned on the beginning of the file.` |
|      - | 4863 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4864 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4865 | ` *          be used after the lock is requested).` |
|      - | 4866 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4867 | ` */` |
|    220 | 4868 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      5 | 4869 | `{` |
|    225 | 4870 | `	const char *zEnd = &zMode[nLen];` |
|    225 | 4871 | `	int iFlag = 0;` |
|      - | 4872 | `	int c;` |
|    225 | 4873 | `	if( nLen < 1 ){` |
|      - | 4874 | `		/* Open in a read-only mode */` |
|    ! 0 | 4875 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4876 | `	}` |
|    225 | 4877 | `	c = zMode[0];` |
|    225 | 4878 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4879 | `		/* Read-only access */` |
|     37 | 4880 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     37 | 4881 | `		zMode++; /* Advance */` |
|     37 | 4882 | `		if( zMode < zEnd ){` |
|     17 | 4883 | `			c = zMode[0];` |
|     17 | 4884 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4885 | `				/* Read+Write access */` |
|     17 | 4886 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      8 | 4887 | `			}` |
|     10 | 4888 | `		}` |
|    198 | 4889 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4890 | `		/* Overwrite mode.` |
|      - | 4891 | `		 * If the file does not exists,try to create it` |
|      - | 4892 | `		 */` |
|     20 | 4893 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     20 | 4894 | `		zMode++; /* Advance */` |
|     20 | 4895 | `		if( zMode < zEnd ){` |
|      5 | 4896 | `			c = zMode[0];` |
|      5 | 4897 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4898 | `				/* Read+Write access */` |
|      5 | 4899 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      5 | 4900 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      2 | 4901 | `			}` |
|      4 | 4902 | `		}` |
|    154 | 4903 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4904 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4905 | `		 * Create the file if it does not exists.` |
|      - | 4906 | `		 */` |
|    ! 0 | 4907 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4908 | `		zMode++; /* Advance */` |
|    ! 0 | 4909 | `		if( zMode < zEnd ){` |
|    ! 0 | 4910 | `			c = zMode[0];` |
|    ! 0 | 4911 | `			if( c == '+' ){` |
|      - | 4912 | `				/* Read-Write access */` |
|    ! 0 | 4913 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4914 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4915 | `			}` |
|    ! 0 | 4916 | `		}` |
|    138 | 4917 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4918 | `		/* Exclusive access.` |
|      - | 4919 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4920 | `		 * Otherwise create a new file.` |
|      - | 4921 | `		 */` |
|     71 | 4922 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|     71 | 4923 | `		zMode++; /* Advance */` |
|     71 | 4924 | `		if( zMode < zEnd ){` |
|    ! 0 | 4925 | `			c = zMode[0];` |
|    ! 0 | 4926 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4927 | `				/* Read-Write access */` |
|    ! 0 | 4928 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4929 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4930 | `			}` |
|      4 | 4931 | `		}` |
|     67 | 4932 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4933 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4934 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4935 | `		zMode++; /* Advance */` |
|    ! 0 | 4936 | `		if( zMode < zEnd ){` |
|    ! 0 | 4937 | `			c = zMode[0];` |
|    ! 0 | 4938 | `			if( c == '+' ){` |
|      - | 4939 | `				/* Read-Write access */` |
|    ! 0 | 4940 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4941 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4942 | `			}` |
|    ! 0 | 4943 | `		}` |
|    ! 0 | 4944 | `	}else{` |
|      - | 4945 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4946 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4947 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4948 | `	}` |
|    245 | 4949 | `	while( zMode < zEnd ){` |
|     21 | 4950 | `		c = zMode[0];` |
|     21 | 4951 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4952 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4953 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     21 | 4954 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4955 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4956 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4957 | `		}` |
|     21 | 4958 | `		zMode++;` |
|      1 | 4959 | `	}` |
|    225 | 4960 | `	return iFlag;` |
|    115 | 4961 | `}` |
|      - | 4962 | `/*` |
|      - | 4963 | ` * Initialize the IO private structure.` |
|      - | 4964 | ` */` |
|   5308 | 4965 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 4966 | `{` |
|   5313 | 4967 | `	pOut->pStream = pStream;` |
|   5313 | 4968 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   5313 | 4969 | `	pOut->nOfft = 0;` |
|      - | 4970 | `	/* Set the magic number */` |
|   5313 | 4971 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   5313 | 4972 | `}` |
|      - | 4973 | `/*` |
|      - | 4974 | ` * Release the IO private structure.` |
|      - | 4975 | ` */` |
|      2 | 4976 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      1 | 4977 | `{` |
|      3 | 4978 | `	SyBlobRelease(&pDev->sBuffer);` |
|      3 | 4979 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4980 | `	/* Release the whole structure */` |
|      3 | 4981 | `	ph7_context_free_chunk(pCtx,pDev);` |
|      3 | 4982 | `}` |
|      - | 4983 | `/*` |
|      - | 4984 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 4985 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 4986 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 4987 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 4988 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 4989 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 4990 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 4991 | ` */` |
|   5250 | 4992 | `static void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 4993 | `{` |
|   5255 | 4994 | `	SyBlobRelease(&pDev->sBuffer);` |
|   5255 | 4995 | `	pDev->pHandle = 0;` |
|   5255 | 4996 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   5255 | 4997 | `}` |
|      - | 4998 | `/*` |
|      - | 4999 | ` * Reset the IO private structure.` |
|      - | 5000 | ` */` |
|     30 | 5001 | `static void ResetIOPrivate(io_private *pDev)` |
|      2 | 5002 | `{` |
|     32 | 5003 | `	SyBlobReset(&pDev->sBuffer);` |
|     32 | 5004 | `	pDev->nOfft = 0;` |
|     32 | 5005 | `}` |
|      - | 5006 | `/* Forward declaration */` |
|      - | 5007 |  |
|      - | 5008 | `/*` |
|      - | 5009 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 5010 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 5011 | ` * Parameters` |
|      - | 5012 | ` *  $filename` |
|      - | 5013 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 5014 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 5015 | ` *   then a regular file is assumed.` |
|      - | 5016 | ` *  $mode` |
|      - | 5017 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 5018 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 5019 | ` *   modes.` |
|      - | 5020 | ` *  $use_include_path` |
|      - | 5021 | ` *   You can use the optional second parameter and set it to` |
|      - | 5022 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 5023 | ` *  $context` |
|      - | 5024 | ` *   A context stream resource.` |
|      - | 5025 | ` * Return` |
|      - | 5026 | ` *  File handle on success or FALSE on failure.` |
|      - | 5027 | ` */` |
|      - | 5028 | `/*` |
|      - | 5029 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 5030 | ` *                                  int $offset = -1)` |
|      - | 5031 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 5032 | ` */` |
|     28 | 5033 | `static int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5034 | `{` |
|      - | 5035 | `	const ph7_io_stream *pStream;` |
|      - | 5036 | `	io_private *pDev;` |
|     29 | 5037 | `	ph7_int64 nMax = -1;` |
|      - | 5038 | `	char zBuf[4096];` |
|      - | 5039 | `	ph7_int64 nRead;` |
|     29 | 5040 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 5041 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5042 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5043 | `		return PH7_OK;` |
|      - | 5044 | `	}` |
|     29 | 5045 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     29 | 5046 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5047 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5048 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5049 | `		return PH7_OK;` |
|      - | 5050 | `	}` |
|     29 | 5051 | `	pStream = pDev->pStream;` |
|     29 | 5052 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 5053 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5054 | `		return PH7_OK;` |
|      - | 5055 | `	}` |
|     29 | 5056 | `	if( nArg > 1 ){` |
|      5 | 5057 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      2 | 5058 | `	}` |
|     17 | 5059 | `	if( nArg > 2 ){` |
|      5 | 5060 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 5061 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 5062 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 5063 | `		}` |
|      2 | 5064 | `	}` |
|     29 | 5065 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     50 | 5066 | `	while( nMax != 0 ){` |
|     48 | 5067 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     48 | 5068 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 5069 | `			nAsk = nMax;` |
|      1 | 5070 | `		}` |
|     48 | 5071 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     48 | 5072 | `		if( nRead < 1 ){` |
|     27 | 5073 | `			break;` |
|      - | 5074 | `		}` |
|     22 | 5075 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     22 | 5076 | `		if( nMax > 0 ){` |
|      3 | 5077 | `			nMax -= nRead;` |
|      1 | 5078 | `		}` |
|      1 | 5079 | `	}` |
|     29 | 5080 | `	return PH7_OK;` |
|     15 | 5081 | `}` |
|      - | 5082 | `/*` |
|      - | 5083 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 5084 | ` */` |
|      4 | 5085 | `static int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5086 | `{` |
|      - | 5087 | `	ph7_value *pArr,*pV;` |
|      - | 5088 | `	ph7_io_stream **apDev;` |
|      - | 5089 | `	sxu32 n;` |
|      2 | 5090 | `	SXUNUSED(nArg);` |
|      2 | 5091 | `	SXUNUSED(apArg);` |
|      6 | 5092 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 5093 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 5094 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 5095 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5096 | `		return PH7_OK;` |
|      - | 5097 | `	}` |
|      6 | 5098 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 5099 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 5100 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 5101 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 5102 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 5103 | `	}` |
|      6 | 5104 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 5105 | `	return PH7_OK;` |
|      4 | 5106 | `}` |
|      - | 5107 | `/*` |
|      - | 5108 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 5109 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 5110 | ` */` |
|      2 | 5111 | `static int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5112 | `{` |
|      - | 5113 | `	io_private *pDev;` |
|      - | 5114 | `	ph7_value *pArr,*pV;` |
|      3 | 5115 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 5116 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5117 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5118 | `		return PH7_OK;` |
|      - | 5119 | `	}` |
|      3 | 5120 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5121 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5122 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5123 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5124 | `		return PH7_OK;` |
|      - | 5125 | `	}` |
|      3 | 5126 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 5127 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 5128 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 5129 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5130 | `		return PH7_OK;` |
|      - | 5131 | `	}` |
|      3 | 5132 | `	ph7_value_bool(pV,0);` |
|      3 | 5133 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 5134 | `	ph7_value_bool(pV,1);` |
|      3 | 5135 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 5136 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 5137 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 5138 | `	ph7_value_bool(pV,0);` |
|      3 | 5139 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 5140 | `	ph7_value_int(pV,0);` |
|      3 | 5141 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 5142 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 5143 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 5144 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 5145 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 5146 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 5147 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 5148 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 5149 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 5150 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 5151 | `	return PH7_OK;` |
|      2 | 5152 | `}` |
|      - | 5153 | `/*` |
|      - | 5154 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 5155 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 5156 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 5157 | ` * resource, options unconsumed).` |
|      - | 5158 | ` */` |
|      2 | 5159 | `static int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5160 | `{` |
|      3 | 5161 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 5162 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 5163 | `	}else{` |
|    ! 0 | 5164 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 5165 | `		if( pArr == 0 ){` |
|    ! 0 | 5166 | `			ph7_result_null(pCtx);` |
|    ! 0 | 5167 | `			return PH7_OK;` |
|      - | 5168 | `		}` |
|    ! 0 | 5169 | `		ph7_result_value(pCtx,pArr);` |
|      - | 5170 | `	}` |
|      3 | 5171 | `	return PH7_OK;` |
|      2 | 5172 | `}` |
|      - | 5173 | `/*` |
|      - | 5174 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 5175 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 5176 | ` */` |
|      - | 5177 | `#ifdef PH7_ENABLE_NET` |
|      - | 5178 | `typedef struct sock_private sock_private;` |
|      - | 5179 | `struct sock_private` |
|      - | 5180 | `{` |
|      - | 5181 | `	ph7_vm *pVm;` |
|      - | 5182 | `	ph7_socket sock;` |
|      - | 5183 | `	int bEof;` |
|      - | 5184 | `};` |
|     11 | 5185 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 5186 | `{` |
|     11 | 5187 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5188 | `	int n;` |
|     11 | 5189 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      2 | 5190 | `		return 0;` |
|      - | 5191 | `	}` |
|      9 | 5192 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|      9 | 5193 | `	if( n <= 0 ){` |
|      4 | 5194 | `		pSock->bEof = 1;` |
|      4 | 5195 | `		return 0;` |
|      - | 5196 | `	}` |
|      5 | 5197 | `	return (ph7_int64)n;` |
|      5 | 5198 | `}` |
|      4 | 5199 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 5200 | `{` |
|      4 | 5201 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5202 | `	int n;` |
|      4 | 5203 | `	if( pSock == 0 ){` |
|    ! 0 | 5204 | `		return -1;` |
|      - | 5205 | `	}` |
|      4 | 5206 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 5207 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 5208 | `}` |
|      4 | 5209 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 5210 | `{` |
|      4 | 5211 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 5212 | `	if( pSock == 0 ){` |
|    ! 0 | 5213 | `		return;` |
|      - | 5214 | `	}` |
|      4 | 5215 | `	PH7_NetClose(pSock->sock);` |
|      4 | 5216 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 5217 | `}` |
|      - | 5218 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 5219 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 5220 | `{` |
|      - | 5221 | `	sock_private *pSock;` |
|      - | 5222 | `	ph7_socket sock;` |
|      - | 5223 | `	char zHost[256];` |
|      - | 5224 | `	const char *zColon;` |
|    ! 0 | 5225 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 5226 | `	const char *zErr = "";` |
|    ! 0 | 5227 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 5228 | `	SXUNUSED(iMode);` |
|    ! 0 | 5229 | `	if( pVm == 0 ){` |
|    ! 0 | 5230 | `		return -1;` |
|      - | 5231 | `	}` |
|    ! 0 | 5232 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 5233 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 5234 | `		zColon--;` |
|    ! 0 | 5235 | `	}` |
|    ! 0 | 5236 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 5237 | `		return -1;` |
|      - | 5238 | `	}` |
|      - | 5239 | `	{` |
|    ! 0 | 5240 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 5241 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 5242 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 5243 | `		}` |
|    ! 0 | 5244 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 5245 | `		zHost[n] = 0;` |
|      - | 5246 | `	}` |
|      - | 5247 | `	{` |
|    ! 0 | 5248 | `		sxi32 iTmp = 0;` |
|    ! 0 | 5249 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 5250 | `		iPort = (int)iTmp;` |
|      - | 5251 | `	}` |
|    ! 0 | 5252 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 5253 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 5254 | `		return -1;` |
|      - | 5255 | `	}` |
|    ! 0 | 5256 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 5257 | `	if( pSock == 0 ){` |
|    ! 0 | 5258 | `		PH7_NetClose(sock);` |
|    ! 0 | 5259 | `		return -1;` |
|      - | 5260 | `	}` |
|    ! 0 | 5261 | `	pSock->pVm = pVm;` |
|    ! 0 | 5262 | `	pSock->sock = sock;` |
|    ! 0 | 5263 | `	pSock->bEof = 0;` |
|    ! 0 | 5264 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 5265 | `	return PH7_OK;` |
|    ! 0 | 5266 | `}` |
|      - | 5267 | `static const ph7_io_stream sTCP_Stream = {` |
|      - | 5268 | `	"tcp",` |
|      - | 5269 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5270 | `	SockStreamData_Open, /* xOpen */` |
|      - | 5271 | `	0,   /* xOpenDir */` |
|      - | 5272 | `	SockStreamData_Close,/* xClose */` |
|      - | 5273 | `	0,  /* xCloseDir */` |
|      - | 5274 | `	SockStreamData_Read, /* xRead */` |
|      - | 5275 | `	0,  /* xReadDir */` |
|      - | 5276 | `	SockStreamData_Write,/* xWrite */` |
|      - | 5277 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 5278 | `	0,  /* xLock */` |
|      - | 5279 | `	0,  /* xRewindDir */` |
|      - | 5280 | `	0,  /* xTell */` |
|      - | 5281 | `	0,  /* xTrunc */` |
|      - | 5282 | `	0,  /* xSync */` |
|      - | 5283 | `	0   /* xStat */` |
|      - | 5284 | `};` |
|      - | 5285 | `#endif /* PH7_ENABLE_NET */` |
|      - | 5286 | `/*` |
|      - | 5287 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 5288 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 5289 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 5290 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 5291 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 5292 | ` * php streamWrapper protocol method on it.` |
|      - | 5293 | ` */` |
|      - | 5294 | `#define PHL_UWRAP_MAX 8` |
|      - | 5295 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 5296 | `struct uwrap_slot` |
|      - | 5297 | `{` |
|      - | 5298 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 5299 | `	char zScheme[32];         /* protocol name */` |
|      - | 5300 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 5301 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 5302 | `};` |
|      - | 5303 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 5304 | `struct uwrap_handle` |
|      - | 5305 | `{` |
|      - | 5306 | `	ph7_vm *pVm;` |
|      - | 5307 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 5308 | `	int iSlot;` |
|      - | 5309 | `	int bEof;` |
|      - | 5310 | `};` |
|      - | 5311 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 5312 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 5313 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 5314 | `	ph7_value *pResult)` |
|      1 | 5315 | `{` |
|      - | 5316 | `	ph7_class_method *pMeth;` |
|     27 | 5317 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 5318 | `		return -1;` |
|      - | 5319 | `	}` |
|     27 | 5320 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 5321 | `	if( pMeth == 0 ){` |
|    ! 0 | 5322 | `		return -1;` |
|      - | 5323 | `	}` |
|     27 | 5324 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 5325 | `		return -1;` |
|      - | 5326 | `	}` |
|     27 | 5327 | `	return 0;` |
|     14 | 5328 | `}` |
|      8 | 5329 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 5330 | `{` |
|      9 | 5331 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5332 | `	ph7_value sArg,sRet;` |
|      - | 5333 | `	const char *zData;` |
|      9 | 5334 | `	int nData = 0;` |
|      9 | 5335 | `	ph7_int64 n = 0;` |
|      9 | 5336 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 5337 | `		return 0;` |
|      - | 5338 | `	}` |
|      9 | 5339 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 5340 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 5341 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 5342 | `	{` |
|      - | 5343 | `		ph7_value *apArg[1];` |
|      9 | 5344 | `		apArg[0] = &sArg;` |
|      9 | 5345 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5346 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5347 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5348 | `			return -1;` |
|      - | 5349 | `		}` |
|      - | 5350 | `	}` |
|      9 | 5351 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 5352 | `	if( nData > 0 ){` |
|      7 | 5353 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 5354 | `			nData = (int)nRead;` |
|    ! 0 | 5355 | `		}` |
|      7 | 5356 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 5357 | `		n = nData;` |
|      4 | 5358 | `	}else{` |
|      3 | 5359 | `		pH->bEof = 1;` |
|      - | 5360 | `	}` |
|      9 | 5361 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 5362 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 5363 | `	return n;` |
|      5 | 5364 | `}` |
|      2 | 5365 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5366 | `{` |
|      3 | 5367 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5368 | `	ph7_value sArg,sRet;` |
|      - | 5369 | `	ph7_int64 n;` |
|      3 | 5370 | `	if( pH == 0 ){` |
|    ! 0 | 5371 | `		return -1;` |
|      - | 5372 | `	}` |
|      3 | 5373 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 5374 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5375 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 5376 | `	{` |
|      - | 5377 | `		ph7_value *apArg[1];` |
|      3 | 5378 | `		apArg[0] = &sArg;` |
|      3 | 5379 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5380 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5381 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5382 | `			return -1;` |
|      - | 5383 | `		}` |
|      - | 5384 | `	}` |
|      3 | 5385 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5386 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 5387 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5388 | `	return n;` |
|      2 | 5389 | `}` |
|      2 | 5390 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5391 | `{` |
|      3 | 5392 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5393 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 5394 | `	ph7_value *apArg[2];` |
|      - | 5395 | `	int rc;` |
|      3 | 5396 | `	if( pH == 0 ){` |
|    ! 0 | 5397 | `		return -1;` |
|      - | 5398 | `	}` |
|      3 | 5399 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 5400 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 5401 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5402 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 5403 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 5404 | `	apArg[0] = &sOfft;` |
|      3 | 5405 | `	apArg[1] = &sWhence;` |
|      3 | 5406 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 5407 | `	if( rc == 0 ){` |
|      3 | 5408 | `		pH->bEof = 0;` |
|      3 | 5409 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 5410 | `	}` |
|      3 | 5411 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 5412 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 5413 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5414 | `	return rc;` |
|      2 | 5415 | `}` |
|      2 | 5416 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 5417 | `{` |
|      3 | 5418 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5419 | `	ph7_value sRet;` |
|      - | 5420 | `	ph7_int64 n;` |
|      3 | 5421 | `	if( pH == 0 ){` |
|    ! 0 | 5422 | `		return -1;` |
|      - | 5423 | `	}` |
|      3 | 5424 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5425 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 5426 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5427 | `		return -1;` |
|      - | 5428 | `	}` |
|      3 | 5429 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5430 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5431 | `	return n;` |
|      2 | 5432 | `}` |
|      6 | 5433 | `static void UwrapClose(void *pHandle)` |
|      1 | 5434 | `{` |
|      7 | 5435 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 5436 | `	if( pH == 0 ){` |
|    ! 0 | 5437 | `		return;` |
|      - | 5438 | `	}` |
|      7 | 5439 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 5440 | `	if( pH->pObj ){` |
|      7 | 5441 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 5442 | `	}` |
|      7 | 5443 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 5444 | `}` |
|      - | 5445 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 5446 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 5447 | `{` |
|      7 | 5448 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 5449 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 5450 | `	ph7_class *pClass;` |
|      - | 5451 | `	uwrap_handle *pH;` |
|      - | 5452 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 5453 | `	ph7_value *apArg[4];` |
|      - | 5454 | `	int rc;` |
|      7 | 5455 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 5456 | `		return -1;` |
|      - | 5457 | `	}` |
|      7 | 5458 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 5459 | `	if( pClass == 0 ){` |
|    ! 0 | 5460 | `		return -1;` |
|      - | 5461 | `	}` |
|      7 | 5462 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 5463 | `	if( pH == 0 ){` |
|    ! 0 | 5464 | `		return -1;` |
|      - | 5465 | `	}` |
|      7 | 5466 | `	pH->pVm = pVm;` |
|      7 | 5467 | `	pH->iSlot = iSlot;` |
|      7 | 5468 | `	pH->bEof = 0;` |
|      7 | 5469 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 5470 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 5471 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5472 | `		return -1;` |
|      - | 5473 | `	}` |
|      - | 5474 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 5475 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 5476 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 5477 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 5478 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 5479 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 5480 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 5481 | `	 * it as "could not be passed by reference"). */` |
|      - | 5482 | `	{` |
|      7 | 5483 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 5484 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 5485 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5486 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5487 | `			return -1;` |
|      - | 5488 | `		}` |
|      7 | 5489 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 5490 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 5491 | `	}` |
|      - | 5492 | `	{` |
|      - | 5493 | `		SyBlob sUrl;` |
|      7 | 5494 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 5495 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 5496 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 5497 | `		SyBlobRelease(&sUrl);` |
|      - | 5498 | `	}` |
|      9 | 5499 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 5500 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 5501 | `	ph7_value_int(&sOpts,0);` |
|      7 | 5502 | `	apArg[0] = &sPath;` |
|      7 | 5503 | `	apArg[1] = &sMode;` |
|      7 | 5504 | `	apArg[2] = &sOpts;` |
|      7 | 5505 | `	apArg[3] = &sOpened;` |
|      7 | 5506 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 5507 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 5508 | `		rc = -1;` |
|    ! 0 | 5509 | `	}` |
|      7 | 5510 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 5511 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 5512 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 5513 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 5514 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 5515 | `	if( rc != 0 ){` |
|    ! 0 | 5516 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5517 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5518 | `		return -1;` |
|      - | 5519 | `	}` |
|      7 | 5520 | `	*ppHandle = (void *)pH;` |
|      7 | 5521 | `	return PH7_OK;` |
|      4 | 5522 | `}` |
|      - | 5523 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 5524 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 5525 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 5526 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 5527 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 5528 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 5529 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 5530 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 5531 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 5532 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 5533 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 5534 | `PHL_UWRAP_THUNK(7)` |
|      - | 5535 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 5536 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 5537 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 5538 | `};` |
|      - | 5539 | `/*` |
|      - | 5540 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 5541 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5542 | ` */` |
|      2 | 5543 | `static int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5544 | `{` |
|      - | 5545 | `	const char *zScheme,*zClass;` |
|      3 | 5546 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 5547 | `	if( nArg < 2 ){` |
|    ! 0 | 5548 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5549 | `		return PH7_OK;` |
|      - | 5550 | `	}` |
|      3 | 5551 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5552 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 5553 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 5554 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 5555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5556 | `		return PH7_OK;` |
|      - | 5557 | `	}` |
|      - | 5558 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 5559 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 5560 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 5561 | `	{` |
|      3 | 5562 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 5563 | `		sxu32 n;` |
|     11 | 5564 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 5565 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 5566 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 5567 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5568 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 5569 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5570 | `				return PH7_OK;` |
|      - | 5571 | `			}` |
|      5 | 5572 | `		}` |
|      - | 5573 | `	}` |
|      3 | 5574 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 5575 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 5576 | `			iFree = i;` |
|      3 | 5577 | `			break;` |
|      - | 5578 | `		}` |
|    ! 0 | 5579 | `	}` |
|      3 | 5580 | `	if( iFree < 0 ){` |
|    ! 0 | 5581 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5582 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 5583 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5584 | `		return PH7_OK;` |
|      - | 5585 | `	}` |
|      - | 5586 | `	{` |
|      3 | 5587 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 5588 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 5589 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 5590 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 5591 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 5592 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 5593 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 5594 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 5595 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 5596 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 5597 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 5598 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 5599 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 5600 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 5601 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 5602 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 5603 | `	}` |
|      3 | 5604 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5605 | `	return PH7_OK;` |
|      2 | 5606 | `}` |
|      2 | 5607 | `static int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5608 | `{` |
|      - | 5609 | `	const char *zScheme;` |
|      - | 5610 | `	int nScheme,i;` |
|      3 | 5611 | `	if( nArg < 1 ){` |
|    ! 0 | 5612 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5613 | `		return PH7_OK;` |
|      - | 5614 | `	}` |
|      3 | 5615 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5616 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 5617 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 5618 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 5619 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 5620 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 5621 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 5622 | `			 * what unregister means to a script — recorded. */` |
|      3 | 5623 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 5624 | `			ph7_result_bool(pCtx,1);` |
|      3 | 5625 | `			return PH7_OK;` |
|      - | 5626 | `		}` |
|    ! 0 | 5627 | `	}` |
|    ! 0 | 5628 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 5629 | `	return PH7_OK;` |
|      2 | 5630 | `}` |
|      - | 5631 | `#ifdef PH7_ENABLE_NET` |
|      - | 5632 | `/*` |
|      - | 5633 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 5634 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 5635 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 5636 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 5637 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 5638 | ` */` |
|      6 | 5639 | `static int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 5640 | `{` |
|      6 | 5641 | `	const char *zFunc = ph7_function_name(pCtx);` |
|      6 | 5642 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|      6 | 5643 | `	const char *zTarget,*zErr = "";` |
|      - | 5644 | `	char zHost[256];` |
|      6 | 5645 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 5646 | `	ph7_socket sock;` |
|      - | 5647 | `	io_private *pDev;` |
|      - | 5648 | `	sock_private *pSock;` |
|      6 | 5649 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|      6 | 5650 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|      6 | 5651 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|      6 | 5652 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 5653 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5654 | `		return PH7_OK;` |
|      - | 5655 | `	}` |
|      6 | 5656 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 5657 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 5658 | `	{` |
|      6 | 5659 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|      6 | 5660 | `		const char *zSep = 0;` |
|     32 | 5661 | `		while( z < zEnd - 2 ){` |
|     30 | 5662 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 5663 | `				zSep = z;` |
|      4 | 5664 | `				break;` |
|      - | 5665 | `			}` |
|     26 | 5666 | `			z++;` |
|    ! 0 | 5667 | `		}` |
|      5 | 5668 | `		if( zSep ){` |
|      4 | 5669 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 5670 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5671 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 5672 | `					nTarget,zTarget);` |
|    ! 0 | 5673 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5674 | `				return PH7_OK;` |
|      - | 5675 | `			}` |
|      4 | 5676 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 5677 | `			zTarget = zSep + 3;` |
|      2 | 5678 | `		}` |
|      - | 5679 | `	}` |
|      - | 5680 | `	/* host[:port] */` |
|      - | 5681 | `	{` |
|      6 | 5682 | `		int i = nTarget - 1;` |
|      6 | 5683 | `		int nHost = nTarget;` |
|     48 | 5684 | `		while( i > 0 && zTarget[i] != ':' ){` |
|     42 | 5685 | `			i--;` |
|    ! 0 | 5686 | `		}` |
|      6 | 5687 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 5688 | `			sxi32 iTmp = 0;` |
|      2 | 5689 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 5690 | `			iPort = (int)iTmp;` |
|      2 | 5691 | `			nHost = i;` |
|      1 | 5692 | `		}` |
|      5 | 5693 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 5694 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 5695 | `		}` |
|      5 | 5696 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|      5 | 5697 | `		zHost[nHost] = 0;` |
|      - | 5698 | `	}` |
|      5 | 5699 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      4 | 5700 | `		iPort = ph7_value_to_int(apArg[1]);` |
|      2 | 5701 | `	}` |
|      6 | 5702 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|      6 | 5703 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|      6 | 5704 | `		if( rTimeout > 0 ){` |
|      6 | 5705 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|      3 | 5706 | `		}` |
|      3 | 5707 | `	}` |
|      6 | 5708 | `	if( iPort < 0 ){` |
|    ! 0 | 5709 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5710 | `		return PH7_OK;` |
|      - | 5711 | `	}` |
|      6 | 5712 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|      6 | 5713 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5714 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 5715 | `		{` |
|      2 | 5716 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      2 | 5717 | `			if( pTmp ){` |
|      2 | 5718 | `				if( nArg > iArgErrno ){` |
|      2 | 5719 | `					ph7_value_int(pTmp,iErrno);` |
|      2 | 5720 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      1 | 5721 | `				}` |
|      2 | 5722 | `				if( nArg > iArgErrstr ){` |
|      2 | 5723 | `					ph7_value_string(pTmp,zErr,-1);` |
|      2 | 5724 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      1 | 5725 | `				}` |
|      1 | 5726 | `			}` |
|      - | 5727 | `		}` |
|      - | 5728 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|      3 | 5729 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 5730 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|      2 | 5731 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5732 | `		return PH7_OK;` |
|      - | 5733 | `	}` |
|      - | 5734 | `	{` |
|      4 | 5735 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 5736 | `		if( pTmp ){` |
|      4 | 5737 | `			if( nArg > iArgErrno ){` |
|      4 | 5738 | `				ph7_value_int(pTmp,0);` |
|      4 | 5739 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 5740 | `			}` |
|      4 | 5741 | `			if( nArg > iArgErrstr ){` |
|      4 | 5742 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 5743 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 5744 | `			}` |
|      2 | 5745 | `		}` |
|      - | 5746 | `	}` |
|      - | 5747 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 5748 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 5749 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 5750 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 5751 | `		PH7_NetClose(sock);` |
|    ! 0 | 5752 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5753 | `		return PH7_OK;` |
|      - | 5754 | `	}` |
|      4 | 5755 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 5756 | `	pSock->sock = sock;` |
|      4 | 5757 | `	pSock->bEof = 0;` |
|      4 | 5758 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 5759 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 5760 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 5761 | `	return PH7_OK;` |
|      3 | 5762 | `}` |
|      - | 5763 | `#endif /* PH7_ENABLE_NET */` |
|    220 | 5764 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5765 | `{` |
|      - | 5766 | `	const ph7_io_stream *pStream;` |
|      - | 5767 | `	const char *zUri,*zMode;` |
|      - | 5768 | `	ph7_value *pResource;` |
|      - | 5769 | `	io_private *pDev;` |
|      - | 5770 | `	int iLen,imLen;` |
|      - | 5771 | `	int iOpenFlags;` |
|    225 | 5772 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5773 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5774 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 5775 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5776 | `		return PH7_OK;` |
|      - | 5777 | `	}` |
|      - | 5778 | `	/* Extract the URI and the desired access mode */` |
|    225 | 5779 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    225 | 5780 | `	if( nArg > 1 ){` |
|    225 | 5781 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    115 | 5782 | `	}else{` |
|      - | 5783 | `		/* Set a default read-only mode */` |
|    ! 0 | 5784 | `		zMode = "r";` |
|    ! 0 | 5785 | `		imLen = (int)sizeof(char);` |
|      - | 5786 | `	}` |
|      - | 5787 | `	/* Try to extract a stream */` |
|    225 | 5788 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    225 | 5789 | `	if( pStream == 0 ){` |
|    ! 0 | 5790 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5791 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 5792 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5793 | `		return PH7_OK;` |
|      - | 5794 | `	}` |
|      - | 5795 | `	/* Allocate a new IO private instance */` |
|    225 | 5796 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    225 | 5797 | `	if( pDev == 0 ){` |
|    ! 0 | 5798 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5799 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5800 | `		return PH7_OK;` |
|      - | 5801 | `	}` |
|    225 | 5802 | `	pResource = 0;` |
|    225 | 5803 | `	if( nArg > 3 ){` |
|    ! 0 | 5804 | `		pResource = apArg[3];` |
|    225 | 5805 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 5806 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 5807 | `		 * access the underlying virtual machine.` |
|      - | 5808 | `		 */` |
|     19 | 5809 | `		pResource = apArg[0];` |
|      9 | 5810 | `	}` |
|      - | 5811 | `	/* Initialize the structure */` |
|    225 | 5812 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 5813 | `	/* Convert open mode to PH7 flags */` |
|    225 | 5814 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 5815 | `	/* Try to get a handle */` |
|    335 | 5816 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    110 | 5817 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|    225 | 5818 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 5819 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|    ! 0 | 5820 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5821 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 5822 | `		return PH7_OK;` |
|      - | 5823 | `	}` |
|      - | 5824 | `	/* All done,return the io_private instance as a resource */` |
|    225 | 5825 | `	ph7_result_resource(pCtx,pDev);` |
|    225 | 5826 | `	return PH7_OK;` |
|    115 | 5827 | `}` |
|      - | 5828 | `/*` |
|      - | 5829 | ` * bool fclose(resource $handle)` |
|      - | 5830 | ` *  Closes an open file pointer` |
|      - | 5831 | ` * Parameters` |
|      - | 5832 | ` *  $handle` |
|      - | 5833 | ` *   The file pointer.` |
|      - | 5834 | ` * Return` |
|      - | 5835 | ` *  TRUE on success or FALSE on failure.` |
|      - | 5836 | ` */` |
|    350 | 5837 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5838 | `{` |
|      - | 5839 | `	const ph7_io_stream *pStream;` |
|      - | 5840 | `	io_private *pDev;` |
|      - | 5841 | `	ph7_vm *pVm;` |
|    355 | 5842 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5843 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5844 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5845 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5846 | `		return PH7_OK;` |
|      - | 5847 | `	}` |
|      - | 5848 | `	/* Extract our private data */` |
|    355 | 5849 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5850 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|    355 | 5851 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 5852 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5853 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 5854 | `	}` |
|      - | 5855 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    353 | 5856 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 5857 | `		/*Expecting an IO handle */` |
|    ! 0 | 5858 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5859 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5860 | `		return PH7_OK;` |
|      - | 5861 | `	}` |
|      - | 5862 | `	/* Point to the target IO stream device */` |
|    353 | 5863 | `	pStream = pDev->pStream;` |
|    353 | 5864 | `	if( pStream == 0 ){` |
|    ! 0 | 5865 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5866 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 5867 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 5868 | `			);` |
|    ! 0 | 5869 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5870 | `		return PH7_OK;` |
|      - | 5871 | `	}` |
|      - | 5872 | `	/* Point to the VM that own this context */` |
|    353 | 5873 | `	pVm = pCtx->pVm;` |
|      - | 5874 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    353 | 5875 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 5876 | `		/* Perform the requested operation */` |
|    353 | 5877 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 5878 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|    353 | 5879 | `		MarkIOPrivateClosed(pDev);` |
|    174 | 5880 | `	}` |
|      - | 5881 | `	/* Return TRUE */` |
|    353 | 5882 | `	ph7_result_bool(pCtx,1);` |
|    353 | 5883 | `	return PH7_OK;` |
|    180 | 5884 | `}` |
|      - | 5885 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5886 | `/*` |
|      - | 5887 | ` * MD5/SHA1 digest consumer.` |
|      - | 5888 | ` */` |
|     72 | 5889 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 5890 | `{` |
|      - | 5891 | `	/* Append hex chunk verbatim */` |
|     73 | 5892 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 5893 | `	return SXRET_OK;` |
|      1 | 5894 | `}` |
|      - | 5895 | `/*` |
|      - | 5896 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 5897 | ` *  Calculates the md5 hash of a given file.` |
|      - | 5898 | ` * Parameters` |
|      - | 5899 | ` *  $uri` |
|      - | 5900 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5901 | ` *  $raw_output` |
|      - | 5902 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 5903 | ` * Return` |
|      - | 5904 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 5905 | ` */` |
|      2 | 5906 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5907 | `{` |
|      - | 5908 | `	const ph7_io_stream *pStream;` |
|      - | 5909 | `	unsigned char zDigest[16];` |
|      3 | 5910 | `	int raw_output  = FALSE;` |
|      - | 5911 | `	const char *zFile;` |
|      - | 5912 | `	MD5Context sCtx;` |
|      - | 5913 | `	char zBuf[8192];` |
|      - | 5914 | `	void *pHandle;` |
|      - | 5915 | `	ph7_int64 n;` |
|      - | 5916 | `	int nLen;` |
|      3 | 5917 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5918 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5919 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5920 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5921 | `		return PH7_OK;` |
|      - | 5922 | `	}` |
|      - | 5923 | `	/* Extract the file path */` |
|      3 | 5924 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5925 | `	/* Point to the target IO stream device */` |
|      3 | 5926 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5927 | `	if( pStream == 0 ){` |
|    ! 0 | 5928 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5929 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5930 | `		return PH7_OK;` |
|      - | 5931 | `	}` |
|      3 | 5932 | `	if( nArg > 1 ){` |
|    ! 0 | 5933 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5934 | `	}` |
|      - | 5935 | `	/* Try to open the file in read-only mode */` |
|      3 | 5936 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5937 | `	if( pHandle == 0 ){` |
|    ! 0 | 5938 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 5939 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5940 | `		return PH7_OK;` |
|      - | 5941 | `	}` |
|      - | 5942 | `	/* Init the MD5 context */` |
|      3 | 5943 | `	MD5Init(&sCtx);` |
|      - | 5944 | `	/* Perform the requested operation */` |
|      2 | 5945 | `	for(;;){` |
|      5 | 5946 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5947 | `		if( n < 1 ){` |
|      - | 5948 | `			/* EOF or IO error,break immediately */` |
|      3 | 5949 | `			break;` |
|      - | 5950 | `		}` |
|      3 | 5951 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5952 | `	}` |
|      - | 5953 | `	/* Close the stream */` |
|      3 | 5954 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5955 | `	/* Extract the digest */` |
|      3 | 5956 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 5957 | `	if( raw_output ){` |
|      - | 5958 | `		/* Output raw digest */` |
|    ! 0 | 5959 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5960 | `	}else{` |
|      - | 5961 | `		/* Perform a binary to hex conversion */` |
|      3 | 5962 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5963 | `	}` |
|      3 | 5964 | `	return PH7_OK;` |
|      2 | 5965 | `}` |
|      - | 5966 | `/*` |
|      - | 5967 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 5968 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 5969 | ` * Parameters` |
|      - | 5970 | ` *  $uri` |
|      - | 5971 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5972 | ` *  $raw_output` |
|      - | 5973 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 5974 | ` * Return` |
|      - | 5975 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 5976 | ` */` |
|      2 | 5977 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5978 | `{` |
|      - | 5979 | `	const ph7_io_stream *pStream;` |
|      - | 5980 | `	unsigned char zDigest[20];` |
|      3 | 5981 | `	int raw_output  = FALSE;` |
|      - | 5982 | `	const char *zFile;` |
|      - | 5983 | `	SHA1Context sCtx;` |
|      - | 5984 | `	char zBuf[8192];` |
|      - | 5985 | `	void *pHandle;` |
|      - | 5986 | `	ph7_int64 n;` |
|      - | 5987 | `	int nLen;` |
|      3 | 5988 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5989 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5990 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5991 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5992 | `		return PH7_OK;` |
|      - | 5993 | `	}` |
|      - | 5994 | `	/* Extract the file path */` |
|      3 | 5995 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5996 | `	/* Point to the target IO stream device */` |
|      3 | 5997 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5998 | `	if( pStream == 0 ){` |
|    ! 0 | 5999 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 6000 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6001 | `		return PH7_OK;` |
|      - | 6002 | `	}` |
|      3 | 6003 | `	if( nArg > 1 ){` |
|    ! 0 | 6004 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 6005 | `	}` |
|      - | 6006 | `	/* Try to open the file in read-only mode */` |
|      3 | 6007 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 6008 | `	if( pHandle == 0 ){` |
|    ! 0 | 6009 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 6010 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6011 | `		return PH7_OK;` |
|      - | 6012 | `	}` |
|      - | 6013 | `	/* Init the SHA1 context */` |
|      3 | 6014 | `	SHA1Init(&sCtx);` |
|      - | 6015 | `	/* Perform the requested operation */` |
|      2 | 6016 | `	for(;;){` |
|      5 | 6017 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 6018 | `		if( n < 1 ){` |
|      - | 6019 | `			/* EOF or IO error,break immediately */` |
|      3 | 6020 | `			break;` |
|      - | 6021 | `		}` |
|      3 | 6022 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 6023 | `	}` |
|      - | 6024 | `	/* Close the stream */` |
|      3 | 6025 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 6026 | `	/* Extract the digest */` |
|      3 | 6027 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 6028 | `	if( raw_output ){` |
|      - | 6029 | `		/* Output raw digest */` |
|    ! 0 | 6030 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 6031 | `	}else{` |
|      - | 6032 | `		/* Perform a binary to hex conversion */` |
|      3 | 6033 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 6034 | `	}` |
|      3 | 6035 | `	return PH7_OK;` |
|      2 | 6036 | `}` |
|      - | 6037 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6038 | `/*` |
|      - | 6039 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 6040 | ` *  Parse a configuration file.` |
|      - | 6041 | ` * Parameters` |
|      - | 6042 | ` * $filename` |
|      - | 6043 | ` *  The filename of the ini file being parsed.` |
|      - | 6044 | ` * $process_sections` |
|      - | 6045 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 6046 | ` *  with the section names and settings included.` |
|      - | 6047 | ` *  The default for process_sections is FALSE.` |
|      - | 6048 | ` * $scanner_mode` |
|      - | 6049 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 6050 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 6051 | ` * Return` |
|      - | 6052 | ` *  The settings are returned as an associative array on success.` |
|      - | 6053 | ` *  Otherwise is returned.` |
|      - | 6054 | ` */` |
|      2 | 6055 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6056 | `{` |
|      - | 6057 | `	const ph7_io_stream *pStream;` |
|      - | 6058 | `	const char *zFile;` |
|      - | 6059 | `	SyBlob sContents;` |
|      - | 6060 | `	void *pHandle;` |
|      - | 6061 | `	int nLen;` |
|      3 | 6062 | `	sxi32 rc = PH7_OK;` |
|      3 | 6063 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6064 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 6065 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 6066 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6067 | `		return PH7_OK;` |
|      - | 6068 | `	}` |
|      - | 6069 | `	/* Extract the file path */` |
|      3 | 6070 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 6071 | `	/* Point to the target IO stream device */` |
|      3 | 6072 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 6073 | `	if( pStream == 0 ){` |
|    ! 0 | 6074 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 6075 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6076 | `		return PH7_OK;` |
|      - | 6077 | `	}` |
|      - | 6078 | `	/* Try to open the file in read-only mode */` |
|      3 | 6079 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 6080 | `	if( pHandle == 0 ){` |
|    ! 0 | 6081 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 6082 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6083 | `		return PH7_OK;` |
|      - | 6084 | `	}` |
|      3 | 6085 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 6086 | `	/* Read the whole file */` |
|      3 | 6087 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 6088 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 6089 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 6090 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6091 | `	}else{` |
|      - | 6092 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 6093 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 6094 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 6095 | `	}` |
|      - | 6096 | `	/* Close the stream */` |
|      3 | 6097 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 6098 | `	/* Release the working buffer */` |
|      3 | 6099 | `	SyBlobRelease(&sContents);` |
|      - | 6100 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 6101 | `	return rc;` |
|      2 | 6102 | `}` |
|      - | 6103 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 6104 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 6105 | `/*` |
|      - | 6106 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 6107 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 6108 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 6109 | ` * to any non-VFS resource).` |
|      - | 6110 | ` */` |
|      - | 6111 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 6112 | `{` |
|      - | 6113 | `	SXUNUSED(pResource);` |
|      - | 6114 | `	return "Unknown";` |
|      - | 6115 | `}` |
|      - | 6116 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 6117 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 6118 | `{` |
|      - | 6119 | `	SXUNUSED(pResource);` |
|      - | 6120 | `	return 0;` |
|      - | 6121 | `}` |
|      - | 6122 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 6123 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 6124 | `#if defined(_MSC_VER)` |
|      - | 6125 | `static const ph7_vfs null_vfs = {` |
|      - | 6126 | `#else` |
|      - | 6127 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 6128 | `#endif` |
|      - | 6129 | `	"null_vfs",` |
|      - | 6130 | `	PH7_VFS_VERSION,` |
|      - | 6131 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 6132 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 6133 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 6134 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 6135 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 6136 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 6137 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 6138 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 6139 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 6140 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 6141 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 6142 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 6143 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 6144 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 6145 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 6146 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 6147 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 6148 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 6149 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 6150 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 6151 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6152 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6153 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 6154 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 6155 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 6156 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 6157 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 6158 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 6159 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 6160 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 6161 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 6162 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 6163 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 6164 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 6165 | `	0, /* int (*xUmask)(int) */` |
|      - | 6166 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 6167 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 6168 | `	0, /* int (*xUid)(void) */` |
|      - | 6169 | `	0, /* int (*xGid)(void) */` |
|      - | 6170 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 6171 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 6172 | `};` |
|      - | 6173 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 6174 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 6175 | `/*` |
|      - | 6176 | ` * Export the builtin vfs.` |
|      - | 6177 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 6178 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 6179 | ` * Note:` |
|      - | 6180 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 6181 | ` * Note:` |
|      - | 6182 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6183 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 6184 | ` */` |
|   3908 | 6185 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 6186 | `{` |
|      - | 6187 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6188 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 6189 | `	return &null_vfs;` |
|      - | 6190 | `#else` |
|      - | 6191 | `#ifdef __WINNT__` |
|      5 | 6192 | `	return &sWinVfs;` |
|      - | 6193 | `#elif defined(__UNIXES__)` |
|   3908 | 6194 | `	return &sUnixVfs;` |
|      - | 6195 | `#else` |
|      - | 6196 | `	return &null_vfs;` |
|      - | 6197 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 6198 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 6199 | `#else` |
|      - | 6200 | `	return &null_vfs;` |
|      - | 6201 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 6202 | `}` |
|      - | 6203 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6204 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6205 | `/*` |
|      - | 6206 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 6207 | ` * no particular meaning on windows.` |
|      - | 6208 | ` */` |
|      - | 6209 | `#ifndef STDIN_FILENO` |
|      - | 6210 | `#define STDIN_FILENO	0` |
|      - | 6211 | `#endif` |
|      - | 6212 | `#ifndef STDOUT_FILENO` |
|      - | 6213 | `#define STDOUT_FILENO	1` |
|      - | 6214 | `#endif` |
|      - | 6215 | `#ifndef STDERR_FILENO` |
|      - | 6216 | `#define STDERR_FILENO	2` |
|      - | 6217 | `#endif` |
|      - | 6218 | `/*` |
|      - | 6219 | ` * php:// Accessing various I/O streams` |
|      - | 6220 | ` * According to the PHP langage reference manual` |
|      - | 6221 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 6222 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 6223 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 6224 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 6225 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 6226 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 6227 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 6228 | ` * php://output` |
|      - | 6229 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 6230 | ` *  mechanism in the same way as print and echo.` |
|      - | 6231 | ` */` |
|      - | 6232 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 6233 | `/* Supported IO streams */` |
|      - | 6234 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 6235 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 6236 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 6237 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 6238 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|      - | 6239 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 6240 | `struct ph7_stream_data` |
|      - | 6241 | `{` |
|      - | 6242 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 6243 | `	int iType;   /* Stream type */` |
|      - | 6244 | `	union{` |
|      - | 6245 | `		void *pHandle; /* Stream handle */` |
|      - | 6246 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 6247 | `	}x;` |
|      - | 6248 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|      - | 6249 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|      - | 6250 | `	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */` |
|      - | 6251 | `};` |
|      - | 6252 | `/*` |
|      - | 6253 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 6254 | ` */` |
|     40 | 6255 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 6256 | `{` |
|      - | 6257 | `	ph7_stream_data *pData;` |
|     41 | 6258 | `	if( pVm == 0 ){` |
|    ! 0 | 6259 | `		return 0;` |
|      - | 6260 | `	}` |
|      - | 6261 | `	/* Allocate a new instance */` |
|     41 | 6262 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|     41 | 6263 | `	if( pData == 0 ){` |
|    ! 0 | 6264 | `		return 0;` |
|      - | 6265 | `	}` |
|      - | 6266 | `	/* Zero the structure */` |
|     41 | 6267 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 6268 | `	/* Initialize fields */` |
|     41 | 6269 | `	pData->iType = iType;` |
|     41 | 6270 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|     41 | 6271 | `	pData->nCur = 0;` |
|     41 | 6272 | `	pData->bReadOnly = 0;` |
|     41 | 6273 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6274 | `		/* Nothing else to set up: the buffer is the stream */` |
|     30 | 6275 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 6276 | `		/* Point to the default VM consumer routine. */` |
|      3 | 6277 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 6278 | `	}else{` |
|      - | 6279 | `#ifdef __WINNT__` |
|      - | 6280 | `		DWORD nChannel;` |
|      1 | 6281 | `		switch(iType){` |
|      1 | 6282 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 6283 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 6284 | `		default:` |
|      1 | 6285 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 6286 | `			break;` |
|      - | 6287 | `		}` |
|      1 | 6288 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 6289 | `#else` |
|      - | 6290 | `		/* Assume an UNIX system */` |
|     16 | 6291 | `		int ifd = STDIN_FILENO;` |
|     16 | 6292 | `		switch(iType){` |
|      6 | 6293 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      8 | 6294 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 6295 | `		default:` |
|      2 | 6296 | `			break;` |
|      - | 6297 | `		}` |
|     16 | 6298 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 6299 | `#endif` |
|      - | 6300 | `	}` |
|     41 | 6301 | `	pData->pVm = pVm;` |
|     41 | 6302 | `	return pData;` |
|     21 | 6303 | `}` |
|      - | 6304 | `/*` |
|      - | 6305 | ` * Implementation of the php:// IO streams routines` |
|      - | 6306 | ` * Status:` |
|      - | 6307 | ` *   Stable.` |
|      - | 6308 | ` */` |
|      - | 6309 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|     14 | 6310 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6311 | `{` |
|      - | 6312 | `	ph7_stream_data *pData;` |
|      - | 6313 | `	SyString sStream;` |
|     15 | 6314 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 6315 | `	/* Trim leading and trailing white spaces */` |
|     15 | 6316 | `	SyStringFullTrim(&sStream);` |
|      - | 6317 | `	/* Stream to open */` |
|     15 | 6318 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 6319 | `		iMode = PH7_IO_STREAM_STDIN;` |
|     15 | 6320 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 6321 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|     14 | 6322 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 6323 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|     13 | 6324 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 6325 | `		iMode = PH7_IO_STREAM_STDERR;` |
|     12 | 6326 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|      8 | 6327 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|      - | 6328 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|      - | 6329 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|     13 | 6330 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|      7 | 6331 | `	}else{` |
|      - | 6332 | `		/* unknown stream name */` |
|    ! 0 | 6333 | `		return -1;` |
|      - | 6334 | `	}` |
|      - | 6335 | `	/* Create our handle */` |
|     15 | 6336 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|     15 | 6337 | `	if( pData == 0 ){` |
|    ! 0 | 6338 | `		return -1;` |
|      - | 6339 | `	}` |
|      - | 6340 | `	/* Make the handle public */` |
|     15 | 6341 | `	*ppHandle = (void *)pData;` |
|     15 | 6342 | `	return PH7_OK;` |
|      8 | 6343 | `}` |
|      - | 6344 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|     42 | 6345 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|      1 | 6346 | `{` |
|     43 | 6347 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     43 | 6348 | `	if( pData == 0 ){` |
|    ! 0 | 6349 | `		return -1;` |
|      - | 6350 | `	}` |
|     43 | 6351 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     43 | 6352 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|      - | 6353 | `		sxu32 nRead;` |
|     43 | 6354 | `		if( pData->nCur >= nAvail ){` |
|     15 | 6355 | `			return 0; /* EOF */` |
|      - | 6356 | `		}` |
|     29 | 6357 | `		nRead = nAvail - pData->nCur;` |
|     29 | 6358 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|      7 | 6359 | `			nRead = (sxu32)nDatatoRead;` |
|      3 | 6360 | `		}` |
|     29 | 6361 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|     29 | 6362 | `		pData->nCur += nRead;` |
|     29 | 6363 | `		return (ph7_int64)nRead;` |
|      - | 6364 | `	}` |
|    ! 0 | 6365 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 6366 | `		/* Forbidden */` |
|    ! 0 | 6367 | `		return -1;` |
|      - | 6368 | `	}` |
|      - | 6369 | `#ifdef __WINNT__` |
|      - | 6370 | `	{` |
|      - | 6371 | `		DWORD nRd;` |
|      - | 6372 | `		BOOL rc;` |
|    ! 0 | 6373 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 6374 | `		if( !rc ){` |
|      - | 6375 | `			/* IO error */` |
|    ! 0 | 6376 | `			return -1;` |
|      - | 6377 | `		}` |
|    ! 0 | 6378 | `		return (ph7_int64)nRd;` |
|      - | 6379 | `	}` |
|      - | 6380 | `#elif defined(__UNIXES__)` |
|      - | 6381 | `	{` |
|      - | 6382 | `		ssize_t nRd;` |
|      - | 6383 | `		int fd;` |
|    ! 0 | 6384 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6385 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 6386 | `		if( nRd < 1 ){` |
|    ! 0 | 6387 | `			return -1;` |
|      - | 6388 | `		}` |
|    ! 0 | 6389 | `		return (ph7_int64)nRd;` |
|      - | 6390 | `	}` |
|      - | 6391 | `#else` |
|      - | 6392 | `	return -1;` |
|      - | 6393 | `#endif` |
|     22 | 6394 | `}` |
|      - | 6395 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|     22 | 6396 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 6397 | `{` |
|     23 | 6398 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     23 | 6399 | `	if( pData == 0 ){` |
|    ! 0 | 6400 | `		return -1;` |
|      - | 6401 | `	}` |
|     23 | 6402 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 6403 | `		/* Forbidden */` |
|    ! 0 | 6404 | `		return -1;` |
|     23 | 6405 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6406 | `		sxu32 nLen,nEnd;` |
|     11 | 6407 | `		if( pData->bReadOnly ){` |
|    ! 0 | 6408 | `			return -1;` |
|      - | 6409 | `		}` |
|     11 | 6410 | `		nLen = SyBlobLength(&pData->sMem);` |
|     11 | 6411 | `		if( pData->nCur > nLen ){` |
|      - | 6412 | `			/* seek past end: php zero-fills the gap */` |
|      - | 6413 | `			static const char zZero[64] = {0};` |
|    ! 0 | 6414 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|    ! 0 | 6415 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|    ! 0 | 6416 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6417 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6418 | `					return -1;` |
|      - | 6419 | `				}` |
|    ! 0 | 6420 | `			}` |
|    ! 0 | 6421 | `			nLen = SyBlobLength(&pData->sMem);` |
|    ! 0 | 6422 | `		}` |
|     11 | 6423 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|     11 | 6424 | `		if( pData->nCur < nLen ){` |
|      - | 6425 | `			/* overwrite in place up to the current end */` |
|      3 | 6426 | `			sxu32 nOver = nLen - pData->nCur;` |
|      3 | 6427 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|      4 | 6428 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|      4 | 6429 | `			if( nEnd > nLen ){` |
|    ! 0 | 6430 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|    ! 0 | 6431 | `					return -1;` |
|      - | 6432 | `				}` |
|    ! 0 | 6433 | `			}` |
|      2 | 6434 | `		}else{` |
|      9 | 6435 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|    ! 0 | 6436 | `				return -1;` |
|      - | 6437 | `			}` |
|      - | 6438 | `		}` |
|     11 | 6439 | `		pData->nCur = nEnd;` |
|     11 | 6440 | `		return nWrite;` |
|     13 | 6441 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 6442 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 6443 | `		int rc;` |
|      - | 6444 | `		/* Call the vm output consumer */` |
|      3 | 6445 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 6446 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 6447 | `			return -1;` |
|      - | 6448 | `		}` |
|      3 | 6449 | `		return nWrite;` |
|      - | 6450 | `	}` |
|      - | 6451 | `#ifdef __WINNT__` |
|      - | 6452 | `	{` |
|      - | 6453 | `		DWORD nWr;` |
|      - | 6454 | `		BOOL rc;` |
|    ! 0 | 6455 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 6456 | `		if( !rc ){` |
|      - | 6457 | `			/* IO error */` |
|    ! 0 | 6458 | `			return -1;` |
|      - | 6459 | `		}` |
|    ! 0 | 6460 | `		return (ph7_int64)nWr;` |
|      - | 6461 | `	}` |
|      - | 6462 | `#elif defined(__UNIXES__)` |
|      - | 6463 | `	{` |
|      - | 6464 | `		ssize_t nWr;` |
|      - | 6465 | `		int fd;` |
|     10 | 6466 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|     10 | 6467 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|     10 | 6468 | `		if( nWr < 1 ){` |
|    ! 0 | 6469 | `			return -1;` |
|      - | 6470 | `		}` |
|     10 | 6471 | `		return (ph7_int64)nWr;` |
|      - | 6472 | `	}` |
|      - | 6473 | `#else` |
|      - | 6474 | `	return -1;` |
|      - | 6475 | `#endif` |
|     12 | 6476 | `}` |
|      - | 6477 | `/* void (*xClose)(void *) */` |
|     20 | 6478 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 6479 | `{` |
|     21 | 6480 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6481 | `	ph7_vm *pVm;` |
|     21 | 6482 | `	if( pData == 0 ){` |
|    ! 0 | 6483 | `		return;` |
|      - | 6484 | `	}` |
|     21 | 6485 | `	pVm = pData->pVm;` |
|     21 | 6486 | `	SyBlobRelease(&pData->sMem);` |
|      - | 6487 | `	/* Free the instance */` |
|     21 | 6488 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|     11 | 6489 | `}` |
|      - | 6490 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|     20 | 6491 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 6492 | `{` |
|     21 | 6493 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6494 | `	ph7_int64 iNew;` |
|     21 | 6495 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6496 | `		return -1;` |
|      - | 6497 | `	}` |
|     21 | 6498 | `	switch(whence){` |
|    ! 0 | 6499 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|      3 | 6500 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|     19 | 6501 | `	default:            iNew = iOfft; break;` |
|      - | 6502 | `	}` |
|     21 | 6503 | `	if( iNew < 0 ){` |
|    ! 0 | 6504 | `		return -1;` |
|      - | 6505 | `	}` |
|     21 | 6506 | `	pData->nCur = (sxu32)iNew;` |
|     21 | 6507 | `	return PH7_OK;` |
|     11 | 6508 | `}` |
|      - | 6509 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|      4 | 6510 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|      1 | 6511 | `{` |
|      5 | 6512 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      5 | 6513 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6514 | `		return -1;` |
|      - | 6515 | `	}` |
|      5 | 6516 | `	return (ph7_int64)pData->nCur;` |
|      3 | 6517 | `}` |
|      - | 6518 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|    ! 0 | 6519 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|    ! 0 | 6520 | `{` |
|    ! 0 | 6521 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 6522 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|    ! 0 | 6523 | `		return -1;` |
|      - | 6524 | `	}` |
|    ! 0 | 6525 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|      - | 6526 | `		/* shrink in place: the blob keeps its allocation */` |
|    ! 0 | 6527 | `		pData->sMem.nByte = (sxu32)nLen;` |
|    ! 0 | 6528 | `	}else{` |
|      - | 6529 | `		static const char zZero[64] = {0};` |
|    ! 0 | 6530 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|    ! 0 | 6531 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|    ! 0 | 6532 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6533 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6534 | `				return -1;` |
|      - | 6535 | `			}` |
|    ! 0 | 6536 | `		}` |
|      - | 6537 | `	}` |
|    ! 0 | 6538 | `	return PH7_OK;` |
|    ! 0 | 6539 | `}` |
|      - | 6540 | `/*` |
|      - | 6541 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|      - | 6542 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|      - | 6543 | ` * base64). Shares the MEMORY machinery above.` |
|      - | 6544 | ` */` |
|      8 | 6545 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 6546 | `{` |
|      9 | 6547 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|      1 | 6548 | `}` |
|     10 | 6549 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6550 | `{` |
|      - | 6551 | `	ph7_stream_data *pData;` |
|     11 | 6552 | `	const char *zIn = zName;` |
|     11 | 6553 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|     11 | 6554 | `	const char *zComma = 0;` |
|     11 | 6555 | `	int bBase64 = 0;` |
|      5 | 6556 | `	SXUNUSED(iMode);` |
|      - | 6557 | `	/* Find the comma separating the mediatype from the payload */` |
|    105 | 6558 | `	while( zIn < zEnd ){` |
|    105 | 6559 | `		if( zIn[0] == ',' ){` |
|     11 | 6560 | `			zComma = zIn;` |
|     11 | 6561 | `			break;` |
|      - | 6562 | `		}` |
|     95 | 6563 | `		zIn++;` |
|      1 | 6564 | `	}` |
|     11 | 6565 | `	if( zComma == 0 ){` |
|    ! 0 | 6566 | `		return -1;` |
|      - | 6567 | `	}` |
|     10 | 6568 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|     10 | 6569 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|      3 | 6570 | `		bBase64 = 1;` |
|      1 | 6571 | `	}` |
|     11 | 6572 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|     11 | 6573 | `	if( pData == 0 ){` |
|    ! 0 | 6574 | `		return -1;` |
|      - | 6575 | `	}` |
|     11 | 6576 | `	pData->bReadOnly = 1;` |
|     11 | 6577 | `	zIn = &zComma[1];` |
|     11 | 6578 | `	if( bBase64 ){` |
|      3 | 6579 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|    ! 0 | 6580 | `			SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6581 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6582 | `			return -1;` |
|      - | 6583 | `		}` |
|      2 | 6584 | `	}else{` |
|      - | 6585 | `		/* percent-decode the payload */` |
|     71 | 6586 | `		while( zIn < zEnd ){` |
|     63 | 6587 | `			char c = zIn[0];` |
|     63 | 6588 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|      3 | 6589 | `				int hi = SyHexToint(zIn[1]);` |
|      3 | 6590 | `				int lo = SyHexToint(zIn[2]);` |
|      3 | 6591 | `				c = (char)((hi << 4) \| lo);` |
|      3 | 6592 | `				zIn += 3;` |
|      2 | 6593 | `			}else{` |
|     61 | 6594 | `				zIn++;` |
|      - | 6595 | `			}` |
|     63 | 6596 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|    ! 0 | 6597 | `				SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6598 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6599 | `				return -1;` |
|      - | 6600 | `			}` |
|      1 | 6601 | `		}` |
|      - | 6602 | `	}` |
|     11 | 6603 | `	*ppHandle = (void *)pData;` |
|     11 | 6604 | `	return PH7_OK;` |
|      6 | 6605 | `}` |
|      - | 6606 | `/* data:// rejects writes outright */` |
|    ! 0 | 6607 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 6608 | `{` |
|    ! 0 | 6609 | `	SXUNUSED(pHandle);` |
|    ! 0 | 6610 | `	SXUNUSED(pBuf);` |
|    ! 0 | 6611 | `	SXUNUSED(nWrite);` |
|    ! 0 | 6612 | `	return -1;` |
|    ! 0 | 6613 | `}` |
|      - | 6614 | `static const ph7_io_stream sDATA_Stream = {` |
|      - | 6615 | `	"data",` |
|      - | 6616 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6617 | `	DataStreamData_Open,  /* xOpen */` |
|      - | 6618 | `	0,   /* xOpenDir */` |
|      - | 6619 | `	PHPStreamData_Close, /* xClose */` |
|      - | 6620 | `	0,  /* xCloseDir */` |
|      - | 6621 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 6622 | `	0,  /* xReadDir */` |
|      - | 6623 | `	DataStreamData_Write, /* xWrite */` |
|      - | 6624 | `	PHPStreamData_Seek,  /* xSeek */` |
|      - | 6625 | `	0,  /* xLock */` |
|      - | 6626 | `	0,  /* xRewindDir */` |
|      - | 6627 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 6628 | `	0,  /* xTrunc */` |
|      - | 6629 | `	0,  /* xSync */` |
|      - | 6630 | `	0   /* xStat */` |
|      - | 6631 | `};` |
|      - | 6632 | `/*` |
|      - | 6633 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 6634 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 6635 | ` * PHP-compatible process I/O functionality.` |
|      - | 6636 | ` */` |
|      - | 6637 | `typedef struct pipe_private pipe_private;` |
|      - | 6638 | `struct pipe_private` |
|      - | 6639 | `{` |
|      - | 6640 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 6641 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 6642 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 6643 | `#ifdef __WINNT__` |
|      - | 6644 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 6645 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 6646 | `#endif` |
|      - | 6647 | `};` |
|      - | 6648 |  |
|      - | 6649 | `#ifdef __WINNT__` |
|      - | 6650 | `#include <Windows.h>` |
|      - | 6651 | `#include <stdio.h>` |
|      - | 6652 | `#include <io.h>` |
|      - | 6653 | `#include <fcntl.h>` |
|      - | 6654 | `/*` |
|      - | 6655 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 6656 | ` * This allows us to properly wait for process completion.` |
|      - | 6657 | ` */` |
|      - | 6658 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      5 | 6659 | `{` |
|      5 | 6660 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      5 | 6661 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      5 | 6662 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 6663 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 6664 | `	STARTUPINFOW si;` |
|      - | 6665 | `	PROCESS_INFORMATION pi;` |
|      5 | 6666 | `	WCHAR *zWideCmd = NULL;` |
|      5 | 6667 | `	FILE *pFile = NULL;` |
|      - | 6668 | `	int fd;` |
|      5 | 6669 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 6670 |  |
|      - | 6671 | `	/* Set up security attributes for pipe inheritance */` |
|      5 | 6672 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      5 | 6673 | `	sa.bInheritHandle = TRUE;` |
|      5 | 6674 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 6675 |  |
|      - | 6676 | `	/* Create pipes for child process I/O */` |
|      5 | 6677 | `	if( bRead ){` |
|      - | 6678 | `		/* Reading from child's stdout */` |
|      5 | 6679 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 6680 | `			return NULL;` |
|      - | 6681 | `		}` |
|      - | 6682 | `		/* Ensure read handle is not inherited */` |
|      5 | 6683 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      5 | 6684 | `		hReadPipe = hChildStdoutRd;` |
|      5 | 6685 | `		*phPipe = hChildStdoutRd;` |
|      5 | 6686 | `	}else{` |
|      - | 6687 | `		/* Writing to child's stdin */` |
|    ! 0 | 6688 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 6689 | `			return NULL;` |
|      - | 6690 | `		}` |
|      - | 6691 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 6692 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 6693 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 6694 | `		*phPipe = hChildStdinWr;` |
|      - | 6695 | `	}` |
|      - | 6696 |  |
|      - | 6697 | `	/* Convert command to wide string */` |
|      - | 6698 | `	{` |
|      5 | 6699 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      5 | 6700 | `		if( nLen <= 0 ){` |
|    ! 0 | 6701 | `			goto cleanup_pipes;` |
|      - | 6702 | `		}` |
|      5 | 6703 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      5 | 6704 | `		if( !zWideCmd ){` |
|    ! 0 | 6705 | `			goto cleanup_pipes;` |
|      - | 6706 | `		}` |
|      5 | 6707 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 6708 | `	}` |
|      - | 6709 |  |
|      - | 6710 | `	/* Set up process startup info */` |
|      5 | 6711 | `	ZeroMemory(&si, sizeof(si));` |
|      5 | 6712 | `	si.cb = sizeof(si);` |
|      5 | 6713 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      5 | 6714 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      5 | 6715 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      5 | 6716 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      5 | 6717 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 6718 |  |
|      5 | 6719 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 6720 |  |
|      - | 6721 | `	/* Create the child process */` |
|      5 | 6722 | `	if( !CreateProcessW(` |
|      - | 6723 | `		NULL,           /* Application name */` |
|      - | 6724 | `		zWideCmd,       /* Command line */` |
|      - | 6725 | `		NULL,           /* Process security attributes */` |
|      - | 6726 | `		NULL,           /* Thread security attributes */` |
|      - | 6727 | `		TRUE,           /* Inherit handles */` |
|      - | 6728 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 6729 | `		NULL,           /* Environment */` |
|      - | 6730 | `		NULL,           /* Current directory */` |
|      - | 6731 | `		&si,            /* Startup info */` |
|      - | 6732 | `		&pi             /* Process info */` |
|      - | 6733 | `	)){` |
|    ! 0 | 6734 | `		goto cleanup_all;` |
|      - | 6735 | `	}` |
|      - | 6736 |  |
|      - | 6737 | `	/* Close handles we don't need in parent */` |
|      5 | 6738 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      5 | 6739 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 6740 |  |
|      - | 6741 | `	/* Close thread handle (we only need process handle) */` |
|      5 | 6742 | `	CloseHandle(pi.hThread);` |
|      - | 6743 |  |
|      - | 6744 | `	/* Store process handle for later waiting */` |
|      5 | 6745 | `	*phProcess = pi.hProcess;` |
|      - | 6746 |  |
|      - | 6747 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      5 | 6748 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 6749 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      5 | 6750 | `	if( fd == -1 ){` |
|    ! 0 | 6751 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6752 | `		*phProcess = NULL;` |
|    ! 0 | 6753 | `		goto cleanup_all;` |
|      - | 6754 | `	}` |
|      - | 6755 |  |
|      5 | 6756 | `	pFile = _fdopen(fd, zMode);` |
|      5 | 6757 | `	if( !pFile ){` |
|    ! 0 | 6758 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 6759 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6760 | `		*phProcess = NULL;` |
|    ! 0 | 6761 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 6762 | `		return NULL;` |
|      - | 6763 | `	}` |
|      - | 6764 |  |
|      5 | 6765 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      5 | 6766 | `	return pFile;` |
|      - | 6767 |  |
|      - | 6768 | `cleanup_all:` |
|    ! 0 | 6769 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 6770 | `cleanup_pipes:` |
|    ! 0 | 6771 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 6772 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 6773 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 6774 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 6775 | `	return NULL;` |
|      5 | 6776 | `}` |
|      - | 6777 |  |
|      - | 6778 | `/*` |
|      - | 6779 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 6780 | ` */` |
|      - | 6781 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      5 | 6782 | `{` |
|      5 | 6783 | `	DWORD dwExitCode = 0;` |
|      - | 6784 | `	int status;` |
|      - | 6785 |  |
|      - | 6786 | `	/* Close the FILE* (this closes the pipe) */` |
|      5 | 6787 | `	fclose(pFile);` |
|      - | 6788 |  |
|      5 | 6789 | `	if( hProcess ){` |
|      - | 6790 | `		/* Wait for the process to complete */` |
|      5 | 6791 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 6792 |  |
|      5 | 6793 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      5 | 6794 | `			status = (int)dwExitCode;` |
|      5 | 6795 | `		}else{` |
|    ! 0 | 6796 | `			status = -1;` |
|      - | 6797 | `		}` |
|      - | 6798 |  |
|      - | 6799 | `		/* Close process handle */` |
|      5 | 6800 | `		CloseHandle(hProcess);` |
|      5 | 6801 | `	}else{` |
|    ! 0 | 6802 | `		status = -1;` |
|      - | 6803 | `	}` |
|      - | 6804 |  |
|      5 | 6805 | `	return status;` |
|      5 | 6806 | `}` |
|      - | 6807 | `#endif /* __WINNT__ */` |
|      - | 6808 | `/*` |
|      - | 6809 | ` * Open a pipe to a process.` |
|      - | 6810 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 6811 | ` */` |
|   3970 | 6812 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      5 | 6813 | `{` |
|      - | 6814 | `	pipe_private *pPipe;` |
|      - | 6815 | `	FILE *pFile;` |
|   3975 | 6816 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 6817 | `		return 0;` |
|      - | 6818 | `	}` |
|      - | 6819 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   3975 | 6820 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 6821 | `		return 0;` |
|      - | 6822 | `	}` |
|      - | 6823 | `	/* Open the pipe using system popen */` |
|      - | 6824 | `#ifdef __WINNT__` |
|      - | 6825 | `	{` |
|      - | 6826 | `		/* Build cmd.exe command wrapper */` |
|      5 | 6827 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      5 | 6828 | `		const char *zShellSuffix = "\"";` |
|      5 | 6829 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      5 | 6830 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      5 | 6831 | `		size_t nCmd = strlen(zCommand);` |
|      5 | 6832 | `		size_t nQuotes = 0;` |
|      5 | 6833 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6834 | `			if (zCommand[i] == '"') nQuotes++;` |
|      5 | 6835 | `		}` |
|      5 | 6836 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      5 | 6837 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      5 | 6838 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 6839 | `			return 0;` |
|      - | 6840 | `		}` |
|      - | 6841 | `		/* Escape quotes in command */` |
|      5 | 6842 | `		size_t j = 0;` |
|      5 | 6843 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6844 | `			char ch = zCommand[i];` |
|      5 | 6845 | `			if (ch == '"') {` |
|      4 | 6846 | `				zCmdEsc[j++] = '^';` |
|      4 | 6847 | `				zCmdEsc[j++] = '"';` |
|      4 | 6848 | `			} else {` |
|      5 | 6849 | `				zCmdEsc[j++] = ch;` |
|      - | 6850 | `			}` |
|      5 | 6851 | `		}` |
|      5 | 6852 | `		zCmdEsc[j] = '\0';` |
|      5 | 6853 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      5 | 6854 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      5 | 6855 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 6856 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6857 | `			return 0;` |
|      - | 6858 | `		}` |
|      5 | 6859 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      5 | 6860 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      5 | 6861 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      5 | 6862 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 6863 | `		/* Allocate pipe structure early so we can store handles */` |
|      5 | 6864 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      5 | 6865 | `		if( pPipe == 0 ){` |
|    ! 0 | 6866 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6867 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 6868 | `			return 0;` |
|      - | 6869 | `		}` |
|      - | 6870 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      5 | 6871 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      5 | 6872 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      5 | 6873 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      5 | 6874 | `		if( pFile == 0 ){` |
|    ! 0 | 6875 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 6876 | `			return 0;` |
|      - | 6877 | `		}` |
|      - | 6878 | `		/* Initialize remaining fields */` |
|      5 | 6879 | `		pPipe->pFile = pFile;` |
|      5 | 6880 | `		pPipe->pVm = pVm;` |
|      5 | 6881 | `		pPipe->iMode = zMode[0];` |
|      - | 6882 | `	}` |
|      - | 6883 | `#elif defined(__UNIXES__) /* Unix */` |
|   3970 | 6884 | `	pFile = popen(zCommand, zMode);` |
|   3970 | 6885 | `	if( pFile == 0 ){` |
|    ! 0 | 6886 | `		return 0;` |
|      - | 6887 | `	}` |
|      - | 6888 | `	/* Allocate pipe private structure */` |
|   3970 | 6889 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   3970 | 6890 | `	if( pPipe == 0 ){` |
|      - | 6891 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 6892 | `		pclose(pFile);` |
|    ! 0 | 6893 | `		return 0;` |
|      - | 6894 | `	}` |
|      - | 6895 | `	/* Initialize the structure */` |
|   3970 | 6896 | `	pPipe->pFile = pFile;` |
|   3970 | 6897 | `	pPipe->pVm = pVm;` |
|   3970 | 6898 | `	pPipe->iMode = zMode[0];` |
|      - | 6899 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6900 | `	(void)pFile;` |
|      - | 6901 | `	return 0;` |
|      - | 6902 | `#endif` |
|   3975 | 6903 | `	return pPipe;` |
|   1990 | 6904 | `}` |
|      - | 6905 | `/*` |
|      - | 6906 | ` * Close a pipe and return the exit status of the process.` |
|      - | 6907 | ` * Returns the exit status, or -1 on error.` |
|      - | 6908 | ` */` |
|   3944 | 6909 | `static int PipeClose(pipe_private *pPipe)` |
|      5 | 6910 | `{` |
|      - | 6911 | `	int status;` |
|      - | 6912 | `	ph7_vm *pVm;` |
|   3949 | 6913 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6914 | `		return -1;` |
|      - | 6915 | `	}` |
|   3949 | 6916 | `	pVm = pPipe->pVm;` |
|      - | 6917 | `	/* Close the pipe and get exit status */` |
|      - | 6918 | `#ifdef __WINNT__` |
|      - | 6919 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      5 | 6920 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 6921 | `#elif defined(__UNIXES__)` |
|   3944 | 6922 | `	status = pclose(pPipe->pFile);` |
|      - | 6923 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   3944 | 6924 | `	if( status != -1 ){` |
|   3944 | 6925 | `		if( WIFEXITED(status) ){` |
|   3944 | 6926 | `			status = WEXITSTATUS(status);` |
|   1972 | 6927 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 6928 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 6929 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 6930 | `		}else{` |
|      - | 6931 | `			/* Unknown termination reason */` |
|    ! 0 | 6932 | `			status = -1;` |
|      - | 6933 | `		}` |
|   1972 | 6934 | `	}` |
|      - | 6935 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6936 | `	status = -1;` |
|      - | 6937 | `#endif` |
|      - | 6938 | `	/* Free the structure */` |
|   3949 | 6939 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   3949 | 6940 | `	return status;` |
|   1977 | 6941 | `}` |
|      - | 6942 | `/*` |
|      - | 6943 | ` * Pipe stream xClose implementation.` |
|      - | 6944 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 6945 | ` * It closes the pipe but does not return the exit status.` |
|      - | 6946 | ` */` |
|    102 | 6947 | `static void PipeStream_Close(void *pHandle)` |
|      4 | 6948 | `{` |
|    106 | 6949 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    106 | 6950 | `	if( pPipe ){` |
|    106 | 6951 | `		PipeClose(pPipe);` |
|     51 | 6952 | `	}` |
|    106 | 6953 | `}` |
|      - | 6954 | `/*` |
|      - | 6955 | ` * Pipe stream xRead implementation.` |
|      - | 6956 | ` */` |
|   5902 | 6957 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      4 | 6958 | `{` |
|   5906 | 6959 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6960 | `	size_t nRead;` |
|   5906 | 6961 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6962 | `		return -1;` |
|      - | 6963 | `	}` |
|   5906 | 6964 | `	if( pPipe->iMode != 'r' ){` |
|      - | 6965 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 6966 | `		return -1;` |
|      - | 6967 | `	}` |
|   5906 | 6968 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   5906 | 6969 | `	if( nRead == 0 ){` |
|   3976 | 6970 | `		if( feof(pPipe->pFile) ){` |
|   3976 | 6971 | `			return 0; /* EOF */` |
|      - | 6972 | `		}` |
|    ! 0 | 6973 | `		return -1; /* Error */` |
|      - | 6974 | `	}` |
|   1934 | 6975 | `	return (ph7_int64)nRead;` |
|   2955 | 6976 | `}` |
|      - | 6977 | `/*` |
|      - | 6978 | ` * Pipe stream xWrite implementation.` |
|      - | 6979 | ` */` |
|      4 | 6980 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 6981 | `{` |
|      4 | 6982 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6983 | `	size_t nWritten;` |
|      4 | 6984 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6985 | `		return -1;` |
|      - | 6986 | `	}` |
|      4 | 6987 | `	if( pPipe->iMode != 'w' ){` |
|      - | 6988 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 6989 | `		return -1;` |
|      - | 6990 | `	}` |
|      4 | 6991 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      4 | 6992 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 6993 | `		return -1; /* Error */` |
|      - | 6994 | `	}` |
|      4 | 6995 | `	return (ph7_int64)nWritten;` |
|      2 | 6996 | `}` |
|      - | 6997 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 6998 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 6999 | `	"pipe",` |
|      - | 7000 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7001 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|      - | 7002 | `	0,  /* xOpenDir */` |
|      - | 7003 | `	PipeStream_Close,  /* xClose */` |
|      - | 7004 | `	0,  /* xCloseDir */` |
|      - | 7005 | `	PipeStream_Read,   /* xRead */` |
|      - | 7006 | `	0,  /* xReadDir */` |
|      - | 7007 | `	PipeStream_Write,  /* xWrite */` |
|      - | 7008 | `	0,  /* xSeek */` |
|      - | 7009 | `	0,  /* xLock */` |
|      - | 7010 | `	0,  /* xRewindDir */` |
|      - | 7011 | `	0,  /* xTell */` |
|      - | 7012 | `	0,  /* xTrunc */` |
|      - | 7013 | `	0,  /* xSync */` |
|      - | 7014 | `	0   /* xStat */` |
|      - | 7015 | `};` |
|      - | 7016 | `/*` |
|      - | 7017 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 7018 | ` * FALSE otherwise.` |
|      - | 7019 | ` */` |
|   3838 | 7020 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      5 | 7021 | `{` |
|   3843 | 7022 | `	return pStream == &sPipe_Stream;` |
|      5 | 7023 | `}` |
|      - | 7024 | `/*` |
|      - | 7025 | ` * resource popen(string $command, string $mode)` |
|      - | 7026 | ` *  Opens process file pointer.` |
|      - | 7027 | ` * Parameters` |
|      - | 7028 | ` *  $command` |
|      - | 7029 | ` *   The command to execute. Passed to the system shell.` |
|      - | 7030 | ` *  $mode` |
|      - | 7031 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 7032 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 7033 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 7034 | ` * Return` |
|      - | 7035 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 7036 | ` */` |
|      - | 7037 | `/*` |
|      - | 7038 | ` * string\|false\|null shell_exec(string $command)` |
|      - | 7039 | ` *  Execute a command via the shell and return the complete output as a string.` |
|      - | 7040 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|      - | 7041 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|      - | 7042 | ` */` |
|      4 | 7043 | `static int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7044 | `{` |
|      - | 7045 | `	const char *zCommand;` |
|      - | 7046 | `	pipe_private *pPipe;` |
|      - | 7047 | `	SyBlob sOut;` |
|      - | 7048 | `	char zBuf[4096];` |
|      - | 7049 | `	size_t nRead;` |
|      - | 7050 | `	int nCmdLen;` |
|      6 | 7051 | `	if( nArg < 1 ){` |
|    ! 0 | 7052 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7053 | `		return PH7_OK;` |
|      - | 7054 | `	}` |
|      6 | 7055 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|      6 | 7056 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7057 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7058 | `		return PH7_OK;` |
|      - | 7059 | `	}` |
|      6 | 7060 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|      6 | 7061 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 7062 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7063 | `		return PH7_OK;` |
|      - | 7064 | `	}` |
|      6 | 7065 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|      4 | 7066 | `	for(;;){` |
|     10 | 7067 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|     10 | 7068 | `		if( nRead < 1 ){` |
|      6 | 7069 | `			break;` |
|      - | 7070 | `		}` |
|      6 | 7071 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|      2 | 7072 | `	}` |
|      6 | 7073 | `	PipeClose(pPipe);` |
|      6 | 7074 | `	if( SyBlobLength(&sOut) < 1 ){` |
|      - | 7075 | `		/* php answers NULL, not "", when the command printed nothing */` |
|    ! 0 | 7076 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7077 | `	}else{` |
|      6 | 7078 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|      - | 7079 | `	}` |
|      6 | 7080 | `	SyBlobRelease(&sOut);` |
|      6 | 7081 | `	return PH7_OK;` |
|      4 | 7082 | `}` |
|   3966 | 7083 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7084 | `{` |
|      - | 7085 | `	const char *zCommand, *zMode;` |
|      - | 7086 | `	pipe_private *pPipe;` |
|      - | 7087 | `	io_private *pDev;` |
|      - | 7088 | `	int nCmdLen, nModeLen;` |
|   3971 | 7089 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 7090 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 7091 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 7092 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7093 | `		return PH7_OK;` |
|      - | 7094 | `	}` |
|      - | 7095 | `	/* Extract the command and mode */` |
|   3971 | 7096 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   3971 | 7097 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   3971 | 7098 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7099 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 7100 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7101 | `		return PH7_OK;` |
|      - | 7102 | `	}` |
|   3971 | 7103 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 7104 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 7105 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7106 | `		return PH7_OK;` |
|      - | 7107 | `	}` |
|      - | 7108 | `	/* Open the pipe */` |
|   3971 | 7109 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   3971 | 7110 | `	if( pPipe == 0 ){` |
|      - | 7111 | `		/* Failed to open pipe */` |
|    ! 0 | 7112 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7113 | `		return PH7_OK;` |
|      - | 7114 | `	}` |
|      - | 7115 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   3971 | 7116 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   3971 | 7117 | `	if( pDev == 0 ){` |
|    ! 0 | 7118 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 7119 | `		PipeClose(pPipe);` |
|    ! 0 | 7120 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7121 | `		return PH7_OK;` |
|      - | 7122 | `	}` |
|      - | 7123 | `	/* Initialize the io_private structure */` |
|   3971 | 7124 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   3971 | 7125 | `	pDev->pHandle = pPipe;` |
|      - | 7126 | `	/* Return the io_private instance as a resource */` |
|   3971 | 7127 | `	ph7_result_resource(pCtx, pDev);` |
|   3971 | 7128 | `	return PH7_OK;` |
|   1988 | 7129 | `}` |
|      - | 7130 | `/*` |
|      - | 7131 | ` * int pclose(resource $handle)` |
|      - | 7132 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 7133 | ` * Parameters` |
|      - | 7134 | ` *  $handle` |
|      - | 7135 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 7136 | ` * Return` |
|      - | 7137 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 7138 | ` */` |
|   3838 | 7139 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7140 | `{` |
|      - | 7141 | `	const ph7_io_stream *pStream;` |
|      - | 7142 | `	pipe_private *pPipe;` |
|      - | 7143 | `	io_private *pDev;` |
|      - | 7144 | `	int status;` |
|   3843 | 7145 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 7146 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 7147 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7148 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7149 | `		return PH7_OK;` |
|      - | 7150 | `	}` |
|      - | 7151 | `	/* Extract our private data */` |
|   3843 | 7152 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 7153 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3843 | 7154 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 7155 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7156 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7157 | `		return PH7_OK;` |
|      - | 7158 | `	}` |
|      - | 7159 | `	/* Point to the target IO stream device */` |
|   3843 | 7160 | `	pStream = pDev->pStream;` |
|   3843 | 7161 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 7162 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 7163 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7164 | `		return PH7_OK;` |
|      - | 7165 | `	}` |
|      - | 7166 | `	/* Get the pipe handle */` |
|   3843 | 7167 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 7168 | `	/* Close the pipe and get exit status */` |
|   3843 | 7169 | `	status = PipeClose(pPipe);` |
|      - | 7170 | `	/* Keep the handle alive but flag it closed so shared copies see it */` |
|   3843 | 7171 | `	MarkIOPrivateClosed(pDev);` |
|      - | 7172 | `	/* Return the exit status */` |
|   3843 | 7173 | `	ph7_result_int(pCtx, status);` |
|   3843 | 7174 | `	return PH7_OK;` |
|   1924 | 7175 | `}` |
|      - | 7176 | `/*` |
|      - | 7177 | ` * proc_open() / proc_close() / proc_get_status() / proc_terminate()` |
|      - | 7178 | ` *   Run a command via fork()/exec() with fine-grained control over its` |
|      - | 7179 | ` *   standard descriptors (php's process-control family). The returned` |
|      - | 7180 | ` *   "process" resource wraps a small proc_private whose leading bytes mirror` |
|      - | 7181 | ` *   io_private (a distinct magic) so is_resource()/gettype() probes stay in` |
|      - | 7182 | ` *   bounds and report it as a live, non-stream resource.` |
|      - | 7183 | ` */` |
|      - | 7184 | `#ifdef __UNIXES__` |
|      - | 7185 | `#define PROC_PRIVATE_MAGIC 0x9C0DE5` |
|      - | 7186 | `#define PROC_MAX_DESC 16` |
|      - | 7187 | `typedef struct proc_private proc_private;` |
|      - | 7188 | `struct proc_private` |
|      - | 7189 | `{` |
|      - | 7190 | `	io_private base;   /* io_private-compatible header (base.iMagic == PROC_PRIVATE_MAGIC) */` |
|      - | 7191 | `	int pid;           /* child process id */` |
|      - | 7192 | `	int running;       /* TRUE until reaped by proc_close/proc_get_status */` |
|      - | 7193 | `	int exit_code;     /* cached exit status once reaped */` |
|      - | 7194 | `};` |
|      - | 7195 | `/* Wrap a raw fd as an fopen-style stream resource (a php pipe end). */` |
|     28 | 7196 | `static io_private * ProcWrapFd(ph7_vm *pVm,int fd)` |
|      - | 7197 | `{` |
|     28 | 7198 | `	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|     28 | 7199 | `	if( pDev == 0 ){` |
|    ! 0 | 7200 | `		return 0;` |
|      - | 7201 | `	}` |
|     28 | 7202 | `	InitIOPrivate(pVm,&sUnixFileStream,pDev);` |
|     28 | 7203 | `	pDev->pHandle = SX_INT_TO_PTR(fd);` |
|     28 | 7204 | `	return pDev;` |
|     14 | 7205 | `}` |
|      - | 7206 | `/* One parsed descriptor-spec entry. */` |
|      - | 7207 | `struct proc_desc` |
|      - | 7208 | `{` |
|      - | 7209 | `	int child_fd;      /* the array key: which fd the child sees */` |
|      - | 7210 | `	int kind;          /* 0=pipe, 1=file, 2=redirect */` |
|      - | 7211 | `	/* pipe */` |
|      - | 7212 | `	int child_end;     /* fd the child must have at child_fd */` |
|      - | 7213 | `	int parent_end;    /* fd the parent keeps (wrapped into $pipes), -1 if none */` |
|      - | 7214 | `	/* file */` |
|      - | 7215 | `	int file_fd;       /* opened fd for a ['file',path,mode] spec */` |
|      - | 7216 | `	/* redirect */` |
|      - | 7217 | `	int redirect_to;   /* target child fd for a ['redirect',N] spec */` |
|      - | 7218 | `};` |
|     10 | 7219 | `static int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7220 | `{` |
|      - | 7221 | `	struct proc_desc aDesc[PROC_MAX_DESC];` |
|     10 | 7222 | `	int nDesc = 0;` |
|      - | 7223 | `	ph7_value *pSpec, *pPipes, *pEntry, *pType, *pParam;` |
|      - | 7224 | `	ph7_hashmap *pSpecMap;` |
|      - | 7225 | `	ph7_hashmap_node *pNode;` |
|     10 | 7226 | `	ph7_vm *pVm = pCtx->pVm;` |
|     10 | 7227 | `	char **azArgv = 0;      /* exec argv when the command is an array */` |
|     10 | 7228 | `	int nArgv = 0;` |
|     10 | 7229 | `	const char *zCmd = 0;   /* exec command when it is a string (via /bin/sh -c) */` |
|     10 | 7230 | `	const char *zCwd = 0;` |
|     10 | 7231 | `	char **azEnv = 0;       /* constructed envp when an env array is supplied */` |
|     10 | 7232 | `	int nEnv = 0;` |
|      - | 7233 | `	proc_private *pProc;` |
|      - | 7234 | `	pid_t pid;` |
|      - | 7235 | `	int i, rc;` |
|     10 | 7236 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[1]) ){` |
|    ! 0 | 7237 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() expects a command and a descriptor spec");` |
|    ! 0 | 7238 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7239 | `		return PH7_OK;` |
|      - | 7240 | `	}` |
|      - | 7241 | `	/* --- Command: array (execvp) or string (/bin/sh -c) --- */` |
|     10 | 7242 | `	if( ph7_value_is_array(apArg[0]) ){` |
|     10 | 7243 | `		ph7_hashmap *pCmdMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     10 | 7244 | `		int nCount = (int)ph7_array_count(apArg[0]);` |
|     10 | 7245 | `		azArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|     10 | 7246 | `		if( azArgv == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     10 | 7247 | `		pNode = pCmdMap->pFirst;` |
|     20 | 7248 | `		for( i = 0 ; i < nCount && pNode ; ++i ){` |
|     10 | 7249 | `			ph7_value *pv = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|      - | 7250 | `			int nLen; const char *zs;` |
|     10 | 7251 | `			PH7_MemObjInit(pVm,pv);` |
|     10 | 7252 | `			PH7_HashmapExtractNodeValue(pNode,pv,FALSE);` |
|     10 | 7253 | `			zs = ph7_value_to_string(pv,&nLen);` |
|     10 | 7254 | `			azArgv[nArgv] = (char *)SyMemBackendDup(&pVm->sAllocator,zs,(sxu32)nLen+1);` |
|     10 | 7255 | `			if( azArgv[nArgv] ){ azArgv[nArgv][nLen] = 0; nArgv++; }` |
|     10 | 7256 | `			PH7_MemObjRelease(pv);` |
|     10 | 7257 | `			SyMemBackendFree(&pVm->sAllocator,pv);` |
|     10 | 7258 | `			pNode = pNode->pPrev; /* hashmap insertion-order walk */` |
|      5 | 7259 | `		}` |
|     10 | 7260 | `		azArgv[nArgv] = 0;` |
|      5 | 7261 | `	}else{` |
|      - | 7262 | `		int nLen;` |
|    ! 0 | 7263 | `		zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7264 | `	}` |
|      - | 7265 | `	/* --- Optional cwd (arg 4) and env (arg 5) --- */` |
|     10 | 7266 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|    ! 0 | 7267 | `		int nLen; zCwd = ph7_value_to_string(apArg[3],&nLen);` |
|    ! 0 | 7268 | `		if( nLen < 1 ){ zCwd = 0; }` |
|    ! 0 | 7269 | `	}` |
|      5 | 7270 | `	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){` |
|    ! 0 | 7271 | `		ph7_hashmap *pEnvMap = (ph7_hashmap *)apArg[4]->x.pOther;` |
|    ! 0 | 7272 | `		int nCount = (int)ph7_array_count(apArg[4]);` |
|    ! 0 | 7273 | `		azEnv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|    ! 0 | 7274 | `		if( azEnv ){` |
|    ! 0 | 7275 | `			pNode = pEnvMap->pFirst;` |
|    ! 0 | 7276 | `			for( i = 0 ; i < nCount && pNode ; ++i ){` |
|      - | 7277 | `				ph7_value sKey, sVal; int nk, nv; const char *zk, *zv; char *zPair;` |
|    ! 0 | 7278 | `				PH7_MemObjInit(pVm,&sKey); PH7_MemObjInit(pVm,&sVal);` |
|    ! 0 | 7279 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|    ! 0 | 7280 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|    ! 0 | 7281 | `				zk = ph7_value_to_string(&sKey,&nk);` |
|    ! 0 | 7282 | `				zv = ph7_value_to_string(&sVal,&nv);` |
|    ! 0 | 7283 | `				zPair = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nk+nv+2));` |
|    ! 0 | 7284 | `				if( zPair ){` |
|    ! 0 | 7285 | `					SyMemcpy(zk,zPair,(sxu32)nk); zPair[nk] = '=';` |
|    ! 0 | 7286 | `					SyMemcpy(zv,&zPair[nk+1],(sxu32)nv); zPair[nk+1+nv] = 0;` |
|    ! 0 | 7287 | `					azEnv[nEnv++] = zPair;` |
|    ! 0 | 7288 | `				}` |
|    ! 0 | 7289 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|    ! 0 | 7290 | `				pNode = pNode->pPrev;` |
|    ! 0 | 7291 | `			}` |
|    ! 0 | 7292 | `			azEnv[nEnv] = 0;` |
|    ! 0 | 7293 | `		}` |
|    ! 0 | 7294 | `	}` |
|      - | 7295 | `	/* --- Parse the descriptor spec, creating pipes as we go --- */` |
|     10 | 7296 | `	pSpec = apArg[1];` |
|     10 | 7297 | `	pSpecMap = (ph7_hashmap *)pSpec->x.pOther;` |
|     10 | 7298 | `	pNode = pSpecMap->pFirst;` |
|     40 | 7299 | `	for( i = 0 ; i < (int)pSpecMap->nEntry && pNode && nDesc < PROC_MAX_DESC ; ++i ){` |
|     30 | 7300 | `		ph7_value sKey; struct proc_desc *pD = &aDesc[nDesc];` |
|     30 | 7301 | `		PH7_MemObjInit(pVm,&sKey);` |
|     30 | 7302 | `		PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|     30 | 7303 | `		pD->child_fd = ph7_value_to_int(&sKey);` |
|     30 | 7304 | `		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1;` |
|     30 | 7305 | `		PH7_MemObjRelease(&sKey);` |
|     30 | 7306 | `		pEntry = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|     30 | 7307 | `		PH7_MemObjInit(pVm,pEntry);` |
|     30 | 7308 | `		PH7_HashmapExtractNodeValue(pNode,pEntry,FALSE);` |
|     30 | 7309 | `		if( ph7_value_is_array(pEntry) ){` |
|      - | 7310 | `			int nLen; const char *zType;` |
|     30 | 7311 | `			pType = ph7_array_fetch(pEntry,"0",1);` |
|     30 | 7312 | `			zType = pType ? ph7_value_to_string(pType,&nLen) : "";` |
|     30 | 7313 | `			if( SyStrncmp(zType,"pipe",4) == 0 ){` |
|      - | 7314 | `				int fds[2];` |
|     28 | 7315 | `				if( pipe(fds) == 0 ){` |
|     28 | 7316 | `					pParam = ph7_array_fetch(pEntry,"1",1);` |
|      - | 7317 | `					{` |
|     28 | 7318 | `						int nMode; const char *zMode = pParam ? ph7_value_to_string(pParam,&nMode) : "r";` |
|     28 | 7319 | `						pD->kind = 0;` |
|     28 | 7320 | `						if( zMode[0] == 'w' \|\| zMode[0] == 'a' ){` |
|      - | 7321 | `							/* child writes -> parent reads: child gets write end */` |
|     18 | 7322 | `							pD->child_end = fds[1]; pD->parent_end = fds[0];` |
|      9 | 7323 | `						}else{` |
|      - | 7324 | `							/* child reads -> parent writes: child gets read end */` |
|     10 | 7325 | `							pD->child_end = fds[0]; pD->parent_end = fds[1];` |
|      - | 7326 | `						}` |
|     28 | 7327 | `						nDesc++;` |
|      - | 7328 | `					}` |
|     14 | 7329 | `				}` |
|     16 | 7330 | `			}else if( SyStrncmp(zType,"file",4) == 0 ){` |
|    ! 0 | 7331 | `				int nLen2, nLen3; const char *zPath, *zMode; int oflag = O_RDONLY;` |
|    ! 0 | 7332 | `				ph7_value *pPath = ph7_array_fetch(pEntry,"1",1);` |
|    ! 0 | 7333 | `				ph7_value *pMode = ph7_array_fetch(pEntry,"2",1);` |
|    ! 0 | 7334 | `				zPath = pPath ? ph7_value_to_string(pPath,&nLen2) : "";` |
|    ! 0 | 7335 | `				zMode = pMode ? ph7_value_to_string(pMode,&nLen3) : "r";` |
|    ! 0 | 7336 | `				if( zMode[0] == 'w' ){ oflag = O_WRONLY\|O_CREAT\|O_TRUNC; }` |
|    ! 0 | 7337 | `				else if( zMode[0] == 'a' ){ oflag = O_WRONLY\|O_CREAT\|O_APPEND; }` |
|    ! 0 | 7338 | `				pD->kind = 1;` |
|    ! 0 | 7339 | `				pD->file_fd = open(zPath,oflag,0644);` |
|    ! 0 | 7340 | `				nDesc++;` |
|      2 | 7341 | `			}else if( SyStrncmp(zType,"redirect",8) == 0 ){` |
|      2 | 7342 | `				pParam = ph7_array_fetch(pEntry,"1",1);` |
|      2 | 7343 | `				pD->kind = 2;` |
|      2 | 7344 | `				pD->redirect_to = pParam ? ph7_value_to_int(pParam) : 1;` |
|      2 | 7345 | `				nDesc++;` |
|      1 | 7346 | `			}` |
|     15 | 7347 | `		}` |
|     30 | 7348 | `		PH7_MemObjRelease(pEntry);` |
|     30 | 7349 | `		SyMemBackendFree(&pVm->sAllocator,pEntry);` |
|     30 | 7350 | `		pNode = pNode->pPrev;` |
|     15 | 7351 | `	}` |
|      - | 7352 | `	/* --- Fork the child --- */` |
|     10 | 7353 | `	pid = fork();` |
|     15 | 7354 | `	if( pid < 0 ){` |
|    ! 0 | 7355 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open(): fork() failed");` |
|    ! 0 | 7356 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7357 | `		return PH7_OK;` |
|      - | 7358 | `	}` |
|     20 | 7359 | `	if( pid == 0 ){` |
|      - | 7360 | `		/* Child: wire up descriptors then exec */` |
|     40 | 7361 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|     30 | 7362 | `			struct proc_desc *pD = &aDesc[i];` |
|     30 | 7363 | `			if( pD->kind == 0 ){` |
|     28 | 7364 | `				dup2(pD->child_end,pD->child_fd);` |
|     28 | 7365 | `				close(pD->parent_end);` |
|     28 | 7366 | `				close(pD->child_end);` |
|     16 | 7367 | `			}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|    ! 0 | 7368 | `				dup2(pD->file_fd,pD->child_fd);` |
|    ! 0 | 7369 | `				close(pD->file_fd);` |
|    ! 0 | 7370 | `			}` |
|     15 | 7371 | `		}` |
|      - | 7372 | `		/* Redirects run after the pipes are in place (e.g. 2>&1) */` |
|     40 | 7373 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|     30 | 7374 | `			if( aDesc[i].kind == 2 ){ dup2(aDesc[i].redirect_to,aDesc[i].child_fd); }` |
|     15 | 7375 | `		}` |
|     10 | 7376 | `		if( zCwd ){ if( chdir(zCwd) != 0 ){ _exit(127); } }` |
|     10 | 7377 | `		if( azEnv ){` |
|    ! 0 | 7378 | `			if( azArgv ){ execve(azArgv[0],azArgv,azEnv); }` |
|    ! 0 | 7379 | `			else{ char *av[4]; av[0]=(char*)"sh"; av[1]=(char*)"-c"; av[2]=(char*)zCmd; av[3]=0; execve("/bin/sh",av,azEnv); }` |
|    ! 0 | 7380 | `		}else{` |
|     10 | 7381 | `			if( azArgv ){ execvp(azArgv[0],azArgv); }` |
|    ! 0 | 7382 | `			else{ execl("/bin/sh","sh","-c",zCmd,(char *)0); }` |
|      - | 7383 | `		}` |
|      5 | 7384 | `		_exit(127); /* exec failed */` |
|      - | 7385 | `	}` |
|      - | 7386 | `	/* Parent: close the child ends, wrap the parent ends into $pipes */` |
|     10 | 7387 | `	pPipes = ph7_context_new_array(pCtx);` |
|     40 | 7388 | `	for( i = 0 ; i < nDesc ; ++i ){` |
|     30 | 7389 | `		struct proc_desc *pD = &aDesc[i];` |
|     30 | 7390 | `		if( pD->kind == 0 ){` |
|      - | 7391 | `			io_private *pEnd;` |
|      - | 7392 | `			ph7_value *pRes;` |
|     28 | 7393 | `			close(pD->child_end);` |
|     28 | 7394 | `			pEnd = ProcWrapFd(pVm,pD->parent_end);` |
|     28 | 7395 | `			pRes = ph7_context_new_scalar(pCtx);` |
|     28 | 7396 | `			if( pEnd && pRes && pPipes ){` |
|     28 | 7397 | `				ph7_value_resource(pRes,pEnd);` |
|     28 | 7398 | `				ph7_array_add_intkey_elem(pPipes,pD->child_fd,pRes);` |
|     14 | 7399 | `			}` |
|     28 | 7400 | `			if( pRes ){ ph7_context_release_value(pCtx,pRes); }` |
|     16 | 7401 | `		}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|    ! 0 | 7402 | `			close(pD->file_fd);` |
|    ! 0 | 7403 | `		}` |
|     15 | 7404 | `	}` |
|     10 | 7405 | `	if( pPipes ){` |
|     10 | 7406 | `		PH7_VmStoreArgByRef(pVm,apArg[2],pPipes);` |
|      5 | 7407 | `	}` |
|      - | 7408 | `	/* Free the exec argv/env copies now that the child owns its own image */` |
|     10 | 7409 | `	if( azArgv ){` |
|     20 | 7410 | `		for( i = 0 ; i < nArgv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azArgv[i]); }` |
|     10 | 7411 | `		SyMemBackendFree(&pVm->sAllocator,azArgv);` |
|      5 | 7412 | `	}` |
|     15 | 7413 | `	if( azEnv ){` |
|    ! 0 | 7414 | `		for( i = 0 ; i < nEnv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azEnv[i]); }` |
|    ! 0 | 7415 | `		SyMemBackendFree(&pVm->sAllocator,azEnv);` |
|    ! 0 | 7416 | `	}` |
|      - | 7417 | `	/* Build the process resource */` |
|     10 | 7418 | `	pProc = (proc_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(proc_private));` |
|     10 | 7419 | `	if( pProc == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     10 | 7420 | `	SyZero(pProc,sizeof(proc_private));` |
|     10 | 7421 | `	pProc->base.iMagic = PROC_PRIVATE_MAGIC;` |
|     10 | 7422 | `	pProc->pid = (int)pid;` |
|     10 | 7423 | `	pProc->running = 1;` |
|     10 | 7424 | `	pProc->exit_code = 0;` |
|     10 | 7425 | `	ph7_result_resource(pCtx,pProc);` |
|      5 | 7426 | `	(void)rc;` |
|     10 | 7427 | `	return PH7_OK;` |
|      5 | 7428 | `}` |
|      - | 7429 | `/* Reap the child if it has not been reaped yet, caching the exit code. */` |
|     10 | 7430 | `static void ProcReap(proc_private *pProc,int block)` |
|      - | 7431 | `{` |
|     10 | 7432 | `	int status = 0;` |
|      - | 7433 | `	pid_t r;` |
|     10 | 7434 | `	if( !pProc->running ){ return; }` |
|     10 | 7435 | `	r = waitpid((pid_t)pProc->pid,&status,block ? 0 : WNOHANG);` |
|     10 | 7436 | `	if( r == (pid_t)pProc->pid ){` |
|     10 | 7437 | `		pProc->running = 0;` |
|     10 | 7438 | `		if( WIFEXITED(status) ){ pProc->exit_code = WEXITSTATUS(status); }` |
|    ! 0 | 7439 | `		else if( WIFSIGNALED(status) ){ pProc->exit_code = 128 + WTERMSIG(status); }` |
|      5 | 7440 | `	}` |
|      5 | 7441 | `}` |
|     10 | 7442 | `static int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7443 | `{` |
|      - | 7444 | `	proc_private *pProc;` |
|     10 | 7445 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7446 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 7447 | `		return PH7_OK;` |
|      - | 7448 | `	}` |
|     10 | 7449 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|     10 | 7450 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|    ! 0 | 7451 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 7452 | `		return PH7_OK;` |
|      - | 7453 | `	}` |
|     10 | 7454 | `	ProcReap(pProc,1/*block until it exits*/);` |
|     10 | 7455 | `	ph7_result_int(pCtx,pProc->exit_code);` |
|     10 | 7456 | `	return PH7_OK;` |
|      5 | 7457 | `}` |
|    ! 0 | 7458 | `static int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7459 | `{` |
|      - | 7460 | `	proc_private *pProc;` |
|    ! 0 | 7461 | `	int sig = 15; /* SIGTERM */` |
|    ! 0 | 7462 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7463 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7464 | `		return PH7_OK;` |
|      - | 7465 | `	}` |
|    ! 0 | 7466 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|    ! 0 | 7467 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|    ! 0 | 7468 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7469 | `		return PH7_OK;` |
|      - | 7470 | `	}` |
|    ! 0 | 7471 | `	if( nArg > 1 ){ sig = ph7_value_to_int(apArg[1]); }` |
|    ! 0 | 7472 | `	if( pProc->running ){ kill((pid_t)pProc->pid,sig); }` |
|    ! 0 | 7473 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 7474 | `	return PH7_OK;` |
|    ! 0 | 7475 | `}` |
|    ! 0 | 7476 | `static int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7477 | `{` |
|      - | 7478 | `	proc_private *pProc;` |
|      - | 7479 | `	ph7_value *pArray, *pVal;` |
|    ! 0 | 7480 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7481 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7482 | `		return PH7_OK;` |
|      - | 7483 | `	}` |
|    ! 0 | 7484 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|    ! 0 | 7485 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|    ! 0 | 7486 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7487 | `		return PH7_OK;` |
|      - | 7488 | `	}` |
|    ! 0 | 7489 | `	ProcReap(pProc,0/*non-blocking poll*/);` |
|    ! 0 | 7490 | `	pArray = ph7_context_new_array(pCtx);` |
|    ! 0 | 7491 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 7492 | `	if( pArray == 0 \|\| pVal == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    ! 0 | 7493 | `	ph7_value_int(pVal,pProc->pid);` |
|    ! 0 | 7494 | `	ph7_array_add_strkey_elem(pArray,"pid",pVal);` |
|    ! 0 | 7495 | `	ph7_value_bool(pVal,pProc->running);` |
|    ! 0 | 7496 | `	ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|    ! 0 | 7497 | `	ph7_value_bool(pVal,0);` |
|    ! 0 | 7498 | `	ph7_array_add_strkey_elem(pArray,"signaled",pVal);` |
|    ! 0 | 7499 | `	ph7_array_add_strkey_elem(pArray,"stopped",pVal);` |
|    ! 0 | 7500 | `	ph7_value_int(pVal,pProc->running ? -1 : pProc->exit_code);` |
|    ! 0 | 7501 | `	ph7_array_add_strkey_elem(pArray,"exitcode",pVal);` |
|    ! 0 | 7502 | `	ph7_value_int(pVal,0);` |
|    ! 0 | 7503 | `	ph7_array_add_strkey_elem(pArray,"termsig",pVal);` |
|    ! 0 | 7504 | `	ph7_array_add_strkey_elem(pArray,"stopsig",pVal);` |
|    ! 0 | 7505 | `	ph7_context_release_value(pCtx,pVal);` |
|    ! 0 | 7506 | `	ph7_result_value(pCtx,pArray);` |
|    ! 0 | 7507 | `	return PH7_OK;` |
|    ! 0 | 7508 | `}` |
|      - | 7509 | `#else /* !__UNIXES__ */` |
|      - | 7510 | `static int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7511 | `{` |
|      - | 7512 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 7513 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() is not available on this platform");` |
|    ! 0 | 7514 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 7515 | `	return PH7_OK;` |
|    ! 0 | 7516 | `}` |
|      - | 7517 | `static int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7518 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_int(pCtx,-1); return PH7_OK; }` |
|      - | 7519 | `static int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7520 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|      - | 7521 | `static int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7522 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|      - | 7523 | `#endif /* __UNIXES__ */` |
|      - | 7524 | `/* Export the php:// stream */` |
|      - | 7525 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 7526 | `	"php",` |
|      - | 7527 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7528 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 7529 | `	0,   /* xOpenDir */` |
|      - | 7530 | `	PHPStreamData_Close, /* xClose */` |
|      - | 7531 | `	0,  /* xCloseDir */` |
|      - | 7532 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 7533 | `	0,  /* xReadDir */` |
|      - | 7534 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 7535 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|      - | 7536 | `	0,  /* xLock */` |
|      - | 7537 | `	0,  /* xRewindDir */` |
|      - | 7538 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 7539 | `	PHPStreamData_Trunc, /* xTrunc */` |
|      - | 7540 | `	0,  /* xSync */` |
|      - | 7541 | `	0   /* xStat */` |
|      - | 7542 | `};` |
|      - | 7543 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7544 | `/*` |
|      - | 7545 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 7546 | ` * FALSE otherwise.` |
|      - | 7547 | ` */` |
|    226 | 7548 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      5 | 7549 | `{` |
|      - | 7550 | `#ifndef PH7_DISABLE_DISK_IO` |
|    231 | 7551 | `	return pStream == &sPHP_Stream;` |
|      - | 7552 | `#else` |
|      - | 7553 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7554 | `	return 0;` |
|      - | 7555 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      5 | 7556 | `}` |
|      - | 7557 | `/*` |
|      - | 7558 | ` * Return TRUE if we are dealing with the data:// stream.` |
|      - | 7559 | ` */` |
|    206 | 7560 | `static int is_data_stream(const ph7_io_stream *pStream)` |
|      5 | 7561 | `{` |
|      - | 7562 | `#ifndef PH7_DISABLE_DISK_IO` |
|    211 | 7563 | `	return pStream == &sDATA_Stream;` |
|      - | 7564 | `#else` |
|      - | 7565 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7566 | `	return 0;` |
|      - | 7567 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      5 | 7568 | `}` |
|      - | 7569 | `/*` |
|      - | 7570 | ` * bool stream_isatty(resource $stream)` |
|      - | 7571 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|      - | 7572 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|      - | 7573 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|      - | 7574 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|      - | 7575 | ` */` |
|      6 | 7576 | `static int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7577 | `{` |
|      7 | 7578 | `	int bTty = 0;` |
|      7 | 7579 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7580 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7581 | `		return PH7_OK;` |
|      - | 7582 | `	}` |
|      - | 7583 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7584 | `	{` |
|      7 | 7585 | `		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      7 | 7586 | `		if( !IO_PRIVATE_INVALID(pDev) && is_php_stream(pDev->pStream) ){` |
|      5 | 7587 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|      6 | 7588 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|      4 | 7589 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|      3 | 7590 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|      - | 7591 | `#ifdef __WINNT__` |
|      1 | 7592 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|      - | 7593 | `#else` |
|      4 | 7594 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|      - | 7595 | `#endif` |
|      2 | 7596 | `			}` |
|      2 | 7597 | `		}` |
|      - | 7598 | `	}` |
|      - | 7599 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      7 | 7600 | `	ph7_result_bool(pCtx,bTty);` |
|      7 | 7601 | `	return PH7_OK;` |
|      4 | 7602 | `}` |
|      - | 7603 |  |
|      - | 7604 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7605 | `/*` |
|      - | 7606 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 7607 | ` * [i.e: file://,php://].` |
|      - | 7608 | ` * Note:` |
|      - | 7609 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 7610 | ` *  defined then this function is a no-op.` |
|      - | 7611 | ` */` |
|   3420 | 7612 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 7613 | `{` |
|      - | 7614 | `	/*` |
|      - | 7615 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 7616 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 7617 | `	 */` |
|      - | 7618 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7619 | `	/* VFS: disk I/O related functions */` |
|      - | 7620 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 7621 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 7622 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 7623 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 7624 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 7625 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 7626 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 7627 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 7628 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 7629 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 7630 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 7631 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 7632 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 7633 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 7634 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 7635 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 7636 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 7637 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 7638 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 7639 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 7640 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 7641 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 7642 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 7643 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 7644 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 7645 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 7646 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 7647 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 7648 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 7649 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 7650 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 7651 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 7652 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 7653 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 7654 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 7655 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 7656 | `		{"link",        PH7_vfs_link     },` |
|      - | 7657 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 7658 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 7659 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 7660 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 7661 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 7662 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 7663 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 7664 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 7665 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 7666 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 7667 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 7668 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 7669 | `	};` |
|      - | 7670 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 7671 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 7672 | `	 */` |
|      - | 7673 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 7674 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 7675 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 7676 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 7677 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 7678 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 7679 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 7680 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 7681 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 7682 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 7683 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 7684 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 7685 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 7686 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 7687 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 7688 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 7689 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 7690 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 7691 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 7692 | `		{"file",      PH7_builtin_file   },` |
|      - | 7693 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 7694 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 7695 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 7696 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 7697 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 7698 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 7699 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 7700 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 7701 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 7702 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 7703 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 7704 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 7705 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 7706 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 7707 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 7708 | `#ifdef PH7_ENABLE_NET` |
|      - | 7709 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 7710 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 7711 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 7712 | `#endif` |
|      - | 7713 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 7714 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 7715 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 7716 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 7717 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 7718 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 7719 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 7720 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 7721 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 7722 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 7723 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 7724 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 7725 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 7726 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7727 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 7728 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 7729 | `	};` |
|   3425 | 7730 | `	const ph7_io_stream *pFileStream = 0;` |
|   3425 | 7731 | `	sxu32 n = 0;` |
|      - | 7732 | `	/* Register disk-related functions */` |
| 167585 | 7733 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 164165 | 7734 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  82085 | 7735 | `	}` |
| 177845 | 7736 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 174425 | 7737 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  87215 | 7738 | `	}` |
|      - | 7739 | `#else` |
|      - | 7740 | `	SXUNUSED(pVm);` |
|      - | 7741 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7742 |  |
|      - | 7743 | `	/*` |
|      - | 7744 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7745 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 7746 | `	 */` |
|      - | 7747 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7748 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 7749 | `		/* Path processing */` |
|      - | 7750 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 7751 | `		{"basename",    PH7_builtin_basename },` |
|      - | 7752 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 7753 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 7754 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 7755 | `		/* ZIP processing */` |
|      - | 7756 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 7757 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 7758 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 7759 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 7760 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 7761 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 7762 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 7763 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 7764 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 7765 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 7766 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 7767 | `	};` |
|  58145 | 7768 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  54725 | 7769 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  27365 | 7770 | `	}` |
|      - | 7771 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7772 |  |
|      - | 7773 | `	/* Install streams if disk I/O is enabled */` |
|      - | 7774 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7775 | `#ifdef __WINNT__` |
|      5 | 7776 | `	pFileStream = &sWinFileStream;` |
|      - | 7777 | `#elif defined(__UNIXES__)` |
|   3420 | 7778 | `	pFileStream = &sUnixFileStream;` |
|      - | 7779 | `#endif` |
|      - | 7780 | `	/* Install the php:// stream */` |
|   3425 | 7781 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3425 | 7782 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 7783 | `#ifdef PH7_ENABLE_NET` |
|   3425 | 7784 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 7785 | `#endif` |
|   3425 | 7786 | `	if( pFileStream ){` |
|      - | 7787 | `		/* Install the file:// stream */` |
|   3425 | 7788 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1710 | 7789 | `	}` |
|      - | 7790 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7791 |  |
|   3425 | 7792 | `	return SXRET_OK;` |
|      5 | 7793 | `}` |
|      - | 7794 | `/*` |
|      - | 7795 | ` * Export the STDIN handle.` |
|      - | 7796 | ` */` |
|      2 | 7797 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 7798 | `{` |
|      - | 7799 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7800 | `	if( pVm->pStdin == 0  ){` |
|      - | 7801 | `		io_private *pIn;` |
|      - | 7802 | `		/* Allocate an IO private instance */` |
|      3 | 7803 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7804 | `		if( pIn == 0 ){` |
|    ! 0 | 7805 | `			return 0;` |
|      - | 7806 | `		}` |
|      3 | 7807 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 7808 | `		/* Initialize the handle */` |
|      3 | 7809 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 7810 | `		/* Install the STDIN stream */` |
|      3 | 7811 | `		pVm->pStdin = pIn;` |
|      3 | 7812 | `		return pIn;` |
|    ! 0 | 7813 | `	}else{` |
|      - | 7814 | `		/* NULL or STDIN */` |
|    ! 0 | 7815 | `		return pVm->pStdin;` |
|      - | 7816 | `	}` |
|      - | 7817 | `#else` |
|      - | 7818 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7819 | `	return 0;` |
|      - | 7820 | `#endif` |
|      2 | 7821 | `}` |
|      - | 7822 | `/*` |
|      - | 7823 | ` * Export the STDOUT handle.` |
|      - | 7824 | ` */` |
|      8 | 7825 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 7826 | `{` |
|      - | 7827 | `#ifndef PH7_DISABLE_DISK_IO` |
|      9 | 7828 | `	if( pVm->pStdout == 0  ){` |
|      - | 7829 | `		io_private *pOut;` |
|      - | 7830 | `		/* Allocate an IO private instance */` |
|      7 | 7831 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      7 | 7832 | `		if( pOut == 0 ){` |
|    ! 0 | 7833 | `			return 0;` |
|      - | 7834 | `		}` |
|      7 | 7835 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 7836 | `		/* Initialize the handle */` |
|      7 | 7837 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 7838 | `		/* Install the STDOUT stream */` |
|      7 | 7839 | `		pVm->pStdout = pOut;` |
|      7 | 7840 | `		return pOut;` |
|    ! 0 | 7841 | `	}else{` |
|      - | 7842 | `		/* NULL or STDOUT */` |
|      3 | 7843 | `		return pVm->pStdout;` |
|      - | 7844 | `	}` |
|      - | 7845 | `#else` |
|      - | 7846 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7847 | `	return 0;` |
|      - | 7848 | `#endif` |
|      5 | 7849 | `}` |
|      - | 7850 | `/*` |
|      - | 7851 | ` * Export the STDERR handle.` |
|      - | 7852 | ` */` |
|     10 | 7853 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 7854 | `{` |
|      - | 7855 | `#ifndef PH7_DISABLE_DISK_IO` |
|     11 | 7856 | `	if( pVm->pStderr == 0  ){` |
|      - | 7857 | `		io_private *pErr;` |
|      - | 7858 | `		/* Allocate an IO private instance */` |
|      9 | 7859 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      9 | 7860 | `		if( pErr == 0 ){` |
|    ! 0 | 7861 | `			return 0;` |
|      - | 7862 | `		}` |
|      9 | 7863 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 7864 | `		/* Initialize the handle */` |
|      9 | 7865 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 7866 | `		/* Install the STDERR stream */` |
|      9 | 7867 | `		pVm->pStderr = pErr;` |
|      9 | 7868 | `		return pErr;` |
|    ! 0 | 7869 | `	}else{` |
|      - | 7870 | `		/* NULL or STDERR */` |
|      3 | 7871 | `		return pVm->pStderr;` |
|      - | 7872 | `	}` |
|      - | 7873 | `#else` |
|      - | 7874 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7875 | `	return 0;` |
|      - | 7876 | `#endif` |
|      6 | 7877 | `}` |
|      - | 7878 |  |
