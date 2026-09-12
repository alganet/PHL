# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2620/3905 lines (67.09%)

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
|  19992 |   68 | `static const char * VfsStrerror(int iErr)` |
|      5 |   69 | `{` |
|      - |   70 | `#if defined(_MSC_VER)` |
|      - |   71 | `#pragma warning(push)` |
|      - |   72 | `#pragma warning(disable:4996)` |
|      - |   73 | `#endif` |
|  19997 |   74 | `	return strerror(iErr);` |
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
|  19972 |   85 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |   86 | `{` |
|  29963 |   87 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  19972 |   88 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  19977 |   89 | `}` |
|     10 |   90 | `static void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      2 |   91 | `{` |
|     17 |   92 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open stream: %s",` |
|     10 |   93 | `		ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));` |
|     12 |   94 | `}` |
|      - |   95 | `/*` |
|      - |   96 | ` * bool chdir(string $directory)` |
|      - |   97 | ` *  Change the current directory.` |
|      - |   98 | ` * Parameters` |
|      - |   99 | ` *  $directory` |
|      - |  100 | ` *   The new current directory` |
|      - |  101 | ` * Return` |
|      - |  102 | ` *  TRUE on success or FALSE on failure.` |
|      - |  103 | ` */` |
|  13448 |  104 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  105 | `{` |
|      - |  106 | `	const char *zPath;` |
|      - |  107 | `	ph7_vfs *pVfs;` |
|      - |  108 | `	int rc;` |
|  13453 |  109 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  110 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |  111 | `		ph7_result_bool(pCtx,0);` |
|      6 |  112 | `		return PH7_OK;` |
|      - |  113 | `	}` |
|      - |  114 | `	/* Point to the underlying vfs */` |
|  13449 |  115 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13449 |  116 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  117 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  118 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  119 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  120 | `			ph7_function_name(pCtx)` |
|      - |  121 | `			);` |
|    ! 0 |  122 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  123 | `		return PH7_OK;` |
|      - |  124 | `	}` |
|      - |  125 | `	/* Point to the desired directory */` |
|  13449 |  126 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  127 | `	/* Perform the requested operation */` |
|  13449 |  128 | `	errno = 0;` |
|  13449 |  129 | `	rc = pVfs->xChdir(zPath);` |
|  13449 |  130 | `	if( rc != PH7_OK ){` |
|      - |  131 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      4 |  132 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      2 |  133 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      1 |  134 | `	}` |
|      - |  135 | `	/* IO return value */` |
|  13449 |  136 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13449 |  137 | `	return PH7_OK;` |
|   6729 |  138 | `}` |
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
|   8820 |  262 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  263 | `{` |
|      - |  264 | `	const char *zPath;` |
|      - |  265 | `	ph7_vfs *pVfs;` |
|      - |  266 | `	int rc;` |
|   8825 |  267 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  268 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  269 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  270 | `		return PH7_OK;` |
|      - |  271 | `	}` |
|      - |  272 | `	/* Point to the underlying vfs */` |
|   8825 |  273 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8825 |  274 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  275 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  276 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  277 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  278 | `			ph7_function_name(pCtx)` |
|      - |  279 | `			);` |
|    ! 0 |  280 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  281 | `		return PH7_OK;` |
|      - |  282 | `	}` |
|      - |  283 | `	/* Point to the desired directory */` |
|   8825 |  284 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  285 | `	/* Perform the requested operation */` |
|   8825 |  286 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  287 | `	/* IO return value */` |
|   8825 |  288 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8825 |  289 | `	return PH7_OK;` |
|   4415 |  290 | `}` |
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
|      3 |  311 | `{` |
|     47 |  312 | `	int iRecursive = 0;` |
|      - |  313 | `	const char *zPath;` |
|      - |  314 | `	ph7_vfs *pVfs;` |
|      - |  315 | `	int iMode,rc;` |
|     47 |  316 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  317 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  318 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  319 | `		return PH7_OK;` |
|      - |  320 | `	}` |
|      - |  321 | `	/* Point to the underlying vfs */` |
|     47 |  322 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     47 |  323 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  324 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  325 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  326 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  327 | `			ph7_function_name(pCtx)` |
|      - |  328 | `			);` |
|    ! 0 |  329 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  330 | `		return PH7_OK;` |
|      - |  331 | `	}` |
|      - |  332 | `	/* Point to the desired directory */` |
|     47 |  333 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  334 | `#ifdef __WINNT__` |
|      3 |  335 | `	iMode = 0;` |
|      - |  336 | `#else` |
|      - |  337 | `	/* Assume UNIX */` |
|     44 |  338 | `	iMode = 0777;` |
|      - |  339 | `#endif` |
|     47 |  340 | `	if( nArg > 1 ){` |
|    ! 0 |  341 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  342 | `		if( nArg > 2 ){` |
|    ! 0 |  343 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  344 | `		}` |
|    ! 0 |  345 | `	}` |
|      - |  346 | `	/* Perform the requested operation */` |
|     25 |  347 | `	errno = 0;` |
|     25 |  348 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|     25 |  349 | `	if( rc != PH7_OK ){` |
|      - |  350 | `		/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|    ! 0 |  351 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|    ! 0 |  352 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|    ! 0 |  353 | `	}` |
|      - |  354 | `	/* IO return value */` |
|     47 |  355 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     47 |  356 | `	return PH7_OK;` |
|     25 |  357 | `}` |
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
|  33688 |  537 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  538 | `{` |
|      - |  539 | `	const char *zPath;` |
|      - |  540 | `	ph7_vfs *pVfs;` |
|      - |  541 | `	int rc;` |
|  33693 |  542 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  543 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  544 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  545 | `		return PH7_OK;` |
|      - |  546 | `	}` |
|      - |  547 | `	/* Point to the underlying vfs */` |
|  33693 |  548 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  33693 |  549 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  550 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  551 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  552 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  553 | `			ph7_function_name(pCtx)` |
|      - |  554 | `			);` |
|    ! 0 |  555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  556 | `		return PH7_OK;` |
|      - |  557 | `	}` |
|      - |  558 | `	/* Point to the desired directory */` |
|  33693 |  559 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  560 | `	/* Perform the requested operation */` |
|  33693 |  561 | `	errno = 0;` |
|  33693 |  562 | `	rc = pVfs->xUnlink(zPath);` |
|  33693 |  563 | `	if( rc != PH7_OK ){` |
|  19971 |  564 | `		VfsThrowSysWarning(pCtx,zPath);` |
|   9983 |  565 | `	}` |
|      - |  566 | `	/* IO return value */` |
|  33693 |  567 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  33693 |  568 | `	return PH7_OK;` |
|  16849 |  569 | `}` |
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
|    146 |  581 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  582 | `{` |
|      - |  583 | `	const char *zPath;` |
|      - |  584 | `	ph7_vfs *pVfs;` |
|      - |  585 | `	int iMode;` |
|      - |  586 | `	int rc;` |
|    148 |  587 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  588 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  590 | `		return PH7_OK;` |
|      - |  591 | `	}` |
|      - |  592 | `	/* Point to the underlying vfs */` |
|    148 |  593 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    148 |  594 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  595 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  596 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  597 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  598 | `			ph7_function_name(pCtx)` |
|      - |  599 | `			);` |
|    ! 0 |  600 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  601 | `		return PH7_OK;` |
|      - |  602 | `	}` |
|      - |  603 | `	/* Point to the desired directory */` |
|    148 |  604 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  605 | `	/* Extract the mode */` |
|    148 |  606 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  607 | `	/* Perform the requested operation */` |
|    148 |  608 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  609 | `	/* IO return value */` |
|    148 |  610 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    148 |  611 | `	return PH7_OK;` |
|     75 |  612 | `}` |
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
|    180 |  808 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  809 | `{` |
|      - |  810 | `	const char *zPath;` |
|      - |  811 | `	ph7_vfs *pVfs;` |
|      - |  812 | `	int rc;` |
|    183 |  813 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  814 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  815 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  816 | `		return PH7_OK;` |
|      - |  817 | `	}` |
|      - |  818 | `	/* Point to the underlying vfs */` |
|    183 |  819 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    183 |  820 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  821 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  822 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  823 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  824 | `			ph7_function_name(pCtx)` |
|      - |  825 | `			);` |
|    ! 0 |  826 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  827 | `		return PH7_OK;` |
|      - |  828 | `	}` |
|      - |  829 | `	/* Point to the desired directory */` |
|    183 |  830 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  831 | `	/* Perform the requested operation */` |
|    183 |  832 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  833 | `	/* IO return value */` |
|    183 |  834 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    183 |  835 | `	return PH7_OK;` |
|     93 |  836 | `}` |
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
|   6760 | 1006 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1007 | `{` |
|      - | 1008 | `	const char *zPath;` |
|      - | 1009 | `	ph7_vfs *pVfs;` |
|      - | 1010 | `	int rc;` |
|   6765 | 1011 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1012 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1013 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1014 | `		return PH7_OK;` |
|      - | 1015 | `	}` |
|      - | 1016 | `	/* Point to the underlying vfs */` |
|   6765 | 1017 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6765 | 1018 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1019 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1020 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1021 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1022 | `			ph7_function_name(pCtx)` |
|      - | 1023 | `			);` |
|    ! 0 | 1024 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1025 | `		return PH7_OK;` |
|      - | 1026 | `	}` |
|      - | 1027 | `	/* Point to the desired directory */` |
|   6765 | 1028 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1029 | `	/* Perform the requested operation */` |
|   6765 | 1030 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1031 | `	/* IO return value */` |
|   6765 | 1032 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6765 | 1033 | `	return PH7_OK;` |
|   3385 | 1034 | `}` |
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
|  13398 | 1659 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1660 | `{` |
|  13403 | 1661 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1662 | `	SyString *pCur;` |
|      - | 1663 | `	int c,d;` |
|  13403 | 1664 | `	c = d = '/';` |
|      - | 1665 | `#ifdef __WINNT__` |
|      5 | 1666 | `	d = '\\';` |
|      - | 1667 | `#endif` |
|      - | 1668 | `	/* Zero the structure */` |
|  13403 | 1669 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1670 | `	/* Handle special case */` |
|  13403 | 1671 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1672 | `#ifdef __WINNT__` |
|    ! 0 | 1673 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1674 | `#else` |
|    ! 0 | 1675 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1676 | `#endif` |
|    ! 0 | 1677 | `		return SXRET_OK;` |
|      - | 1678 | `	}` |
|      - | 1679 | `	/* Extract the basename */` |
| 361570 | 1680 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 341473 | 1681 | `		zEnd--;` |
|      5 | 1682 | `	}` |
|  13403 | 1683 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13403 | 1684 | `	zEnd = &zPath[nByte];` |
|      - | 1685 | `	/* dirname */` |
|  13403 | 1686 | `	pCur = &pOut->sDir;` |
|  13403 | 1687 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13403 | 1688 | `	if( pCur->nByte > 1 ){` |
|  26801 | 1689 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1690 | `#ifdef __WINNT__` |
|      5 | 1691 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1692 | `#endif` |
|   6704 | 1693 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1694 | `#ifdef __WINNT__` |
|    ! 0 | 1695 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1696 | `#else` |
|    ! 0 | 1697 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1698 | `#endif` |
|    ! 0 | 1699 | `	}` |
|      - | 1700 | `	/* basename/filename */` |
|  13403 | 1701 | `	pCur = &pOut->sBasename;` |
|  13403 | 1702 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13403 | 1703 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1704 | `#ifdef __WINNT__` |
|      5 | 1705 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1706 | `#endif` |
|  13403 | 1707 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13403 | 1708 | `	if( pCur->nByte > 0 ){` |
|      - | 1709 | `		/* extension */` |
|  13403 | 1710 | `		zEnd--;` |
|  66989 | 1711 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  53591 | 1712 | `			zEnd--;` |
|      5 | 1713 | `		}` |
|  13403 | 1714 | `		if( zEnd > pCur->zString ){` |
|  13401 | 1715 | `			zEnd++; /* Jump leading dot */` |
|  13401 | 1716 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1717 | `			/* Fix filename */` |
|  13401 | 1718 | `			pCur = &pOut->sFilename;` |
|  13401 | 1719 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13401 | 1720 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6698 | 1721 | `			}` |
|   6698 | 1722 | `		}` |
|   6699 | 1723 | `	}` |
|  13403 | 1724 | `	return SXRET_OK;` |
|   6704 | 1725 | `}` |
|      - | 1726 | `/*` |
|      - | 1727 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1728 | ` *  See block comment above.` |
|      - | 1729 | ` */` |
|  13398 | 1730 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1731 | `{` |
|      - | 1732 | `	const char *zPath;` |
|      - | 1733 | `	path_info sInfo;` |
|      - | 1734 | `	SyString *pComp;` |
|      - | 1735 | `	int iLen;` |
|  13403 | 1736 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1737 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1738 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1739 | `		return PH7_OK;` |
|      - | 1740 | `	}` |
|      - | 1741 | `	/* Point to the target path */` |
|  13403 | 1742 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13403 | 1743 | `	if( iLen < 1 ){` |
|      - | 1744 | `		/* Empty string */` |
|    ! 0 | 1745 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1746 | `		return PH7_OK;` |
|      - | 1747 | `	}` |
|      - | 1748 | `	/* Extract path info */` |
|  13403 | 1749 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  20101 | 1750 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1751 | `		/* Return path component */` |
|  13401 | 1752 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13401 | 1753 | `		switch(nComp){` |
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
|   3350 | 1772 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6705 | 1773 | `			pComp = &sInfo.sExtension;` |
|   6705 | 1774 | `			if( pComp->nByte > 0 ){` |
|   6703 | 1775 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3354 | 1776 | `			}else{` |
|      - | 1777 | `				/* Expand the empty string */` |
|      3 | 1778 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1779 | `			}` |
|   6705 | 1780 | `			break;` |
|   3346 | 1781 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6697 | 1782 | `			pComp = &sInfo.sFilename;` |
|   6697 | 1783 | `			if( pComp->nByte > 0 ){` |
|   6697 | 1784 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3351 | 1785 | `			}else{` |
|      - | 1786 | `				/* Expand the empty string */` |
|    ! 0 | 1787 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1788 | `			}` |
|   6697 | 1789 | `			break;` |
|    ! 0 | 1790 | `		default:` |
|      - | 1791 | `			/* Expand the empty string */` |
|    ! 0 | 1792 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1793 | `			break;` |
|      - | 1794 | `		}` |
|   6703 | 1795 | `	}else{` |
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
|  13403 | 1845 | `	return PH7_OK;` |
|   6704 | 1846 | `}` |
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
|    240 | 2244 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2245 | `{` |
|      - | 2246 | `	ph7_vfs *pVfs;` |
|      - | 2247 | `	/* Set the empty string as the default return value */` |
|    245 | 2248 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2249 | `	/* Point to the underlying vfs */` |
|    245 | 2250 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    245 | 2251 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
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
|    245 | 2262 | `	pVfs->xTempDir(pCtx);` |
|    245 | 2263 | `	return PH7_OK;` |
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
|      3 | 2303 | `{` |
|      - | 2304 | `	ph7_int64 nProcessId;` |
|      - | 2305 | `	ph7_vfs *pVfs;` |
|      - | 2306 | `	/* Point to the underlying vfs */` |
|     87 | 2307 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     87 | 2308 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
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
|     87 | 2320 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2321 | `	/* Set the result */` |
|     87 | 2322 | `	ph7_result_int64(pCtx,nProcessId);` |
|     87 | 2323 | `	return PH7_OK;` |
|     45 | 2324 | `}` |
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
|      3 | 2575 | `{` |
|     67 | 2576 | `	io_private *pDev = (io_private *)pResource;` |
|     67 | 2577 | `	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;` |
|      3 | 2578 | `}` |
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
|  10590 | 2848 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2849 | `{` |
|      - | 2850 | `	const ph7_io_stream *pStream;` |
|      - | 2851 | `	io_private *pDev;` |
|      - | 2852 | `	int rc;` |
|  10595 | 2853 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2854 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2855 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2856 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2857 | `		return PH7_OK;` |
|      - | 2858 | `	}` |
|      - | 2859 | `	/* Extract our private data */` |
|  10595 | 2860 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2861 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10595 | 2862 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2863 | `		/*Expecting an IO handle */` |
|    ! 0 | 2864 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2865 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2866 | `		return PH7_OK;` |
|      - | 2867 | `	}` |
|      - | 2868 | `	/* Point to the target IO stream device */` |
|  10595 | 2869 | `	pStream = pDev->pStream;` |
|  10595 | 2870 | `	if( pStream == 0 ){` |
|    ! 0 | 2871 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2872 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2873 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2874 | `			);` |
|    ! 0 | 2875 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2876 | `		return PH7_OK;` |
|      - | 2877 | `	}` |
|  10595 | 2878 | `	rc = SXERR_EOF;` |
|      - | 2879 | `	/* Perform the requested operation */` |
|  10595 | 2880 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2881 | `		/* Data is available */` |
|   4847 | 2882 | `		rc = PH7_OK;` |
|   2426 | 2883 | `	}else{` |
|      - | 2884 | `		char zBuf[4096];` |
|      - | 2885 | `		ph7_int64 n;` |
|      - | 2886 | `		/* Perform a buffered read */` |
|   5753 | 2887 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5753 | 2888 | `		if( n > 0 ){` |
|      - | 2889 | `			/* Copy buffered data */` |
|   1877 | 2890 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1877 | 2891 | `			rc = PH7_OK;` |
|    936 | 2892 | `		}` |
|      - | 2893 | `	}` |
|      - | 2894 | `	/* EOF or not */` |
|  10595 | 2895 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10595 | 2896 | `	return PH7_OK;` |
|   5300 | 2897 | `}` |
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
|     36 | 2933 | `		n += nRead;` |
|     20 | 2934 | `	}else if( n < 1 ){` |
|      - | 2935 | `		/* EOF or IO error */` |
|      3 | 2936 | `		return nRead;` |
|      - | 2937 | `	}` |
|     37 | 2938 | `	return n;` |
|     21 | 2939 | `}` |
|      - | 2940 | `/*` |
|      - | 2941 | ` * Extract a single line from the buffered input.` |
|      - | 2942 | ` */` |
|   6780 | 2943 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 | 2944 | `{` |
|      - | 2945 | `	const char *zIn,*zEnd,*zPtr;` |
|   6785 | 2946 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6785 | 2947 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6785 | 2948 | `	zPtr = zIn;` |
| 457829 | 2949 | `	while( zIn < zEnd ){` |
| 457725 | 2950 | `		if( zIn[0] == '\n' ){` |
|      - | 2951 | `			/* Line found */` |
|   6681 | 2952 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6681 | 2953 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6681 | 2954 | `			*pzLine = zPtr;` |
|   6681 | 2955 | `			return SXRET_OK;` |
|      - | 2956 | `		}` |
| 451049 | 2957 | `		zIn++;` |
|      5 | 2958 | `	}` |
|      - | 2959 | `	/* No line were found */` |
|    109 | 2960 | `	return SXERR_NOTFOUND;` |
|   3395 | 2961 | `}` |
|      - | 2962 | `/*` |
|      - | 2963 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2964 | ` */` |
|   6784 | 2965 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 | 2966 | `{` |
|   6789 | 2967 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2968 | `	char zBuf[8192];` |
|      - | 2969 | `	ph7_int64 n;` |
|      - | 2970 | `	sxi32 rc;` |
|   6789 | 2971 | `	n = 0;` |
|   6789 | 2972 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2973 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     73 | 2974 | `		SyBlobReset(&pDev->sBuffer);` |
|     73 | 2975 | `		pDev->nOfft = 0;` |
|     34 | 2976 | `	}` |
|   6755 | 2977 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2978 | `		/* Check if there is a line */` |
|   6721 | 2979 | `		rc = GetLine(pDev,&n,pzData);` |
|   6721 | 2980 | `		if( rc == SXRET_OK ){` |
|      - | 2981 | `			/* Got line,update the cursor  */` |
|   6621 | 2982 | `			pDev->nOfft += (sxu32)n;` |
|   6621 | 2983 | `			return n;` |
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
|   3397 | 3023 | `}` |
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
|  30382 | 3045 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 3046 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 | 3047 | `{` |
|  30387 | 3048 | `	void *pHandle = 0; /* cc warning */` |
|      - | 3049 | `	SyString sFile;` |
|      - | 3050 | `	ph7_value sDummy;` |
|      - | 3051 | `	int rc;` |
|  30387 | 3052 | `	if( pStream == 0 ){` |
|      - | 3053 | `		/* No such stream device */` |
|    ! 0 | 3054 | `		return 0;` |
|      - | 3055 | `	}` |
|  30387 | 3056 | `	if( pResource == 0 ){` |
|      - | 3057 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - | 3058 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - | 3059 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - | 3060 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - | 3061 | `		 * reads it during the call, and file:// ignores it. */` |
|  30365 | 3062 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  30365 | 3063 | `		pResource = &sDummy;` |
|  15180 | 3064 | `	}` |
|  30387 | 3065 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  30387 | 3066 | `	if( use_include ){` |
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
|  20699 | 3118 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 3119 | `	}` |
|  30387 | 3120 | `	if( rc != PH7_OK ){` |
|      - | 3121 | `		/* IO error */` |
|     22 | 3122 | `		return 0;` |
|      - | 3123 | `	}` |
|      - | 3124 | `	/* Return the file handle */` |
|  30369 | 3125 | `	return pHandle;` |
|  15196 | 3126 | `}` |
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
|  30492 | 3155 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 3156 | `{` |
|  30497 | 3157 | `	if( pStream->xClose ){` |
|  30497 | 3158 | `		pStream->xClose(pHandle);` |
|  15246 | 3159 | `	}` |
|  30497 | 3160 | `}` |
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
|   6774 | 3231 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3232 | `{` |
|      - | 3233 | `	const ph7_io_stream *pStream;` |
|      - | 3234 | `	const char *zLine;` |
|      - | 3235 | `	io_private *pDev;` |
|      - | 3236 | `	ph7_int64 n,nLen;` |
|   6779 | 3237 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3238 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3239 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3240 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3241 | `		return PH7_OK;` |
|      - | 3242 | `	}` |
|      - | 3243 | `	/* Extract our private data */` |
|   6779 | 3244 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3245 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6779 | 3246 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3247 | `		/*Expecting an IO handle */` |
|    ! 0 | 3248 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3249 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3250 | `		return PH7_OK;` |
|      - | 3251 | `	}` |
|      - | 3252 | `	/* Point to the target IO stream device */` |
|   6779 | 3253 | `	pStream = pDev->pStream;` |
|   6779 | 3254 | `	if( pStream == 0  ){` |
|    ! 0 | 3255 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3256 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3257 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3258 | `			);` |
|    ! 0 | 3259 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3260 | `		return PH7_OK;` |
|      - | 3261 | `	}` |
|   6779 | 3262 | `	nLen = -1;` |
|   6779 | 3263 | `	if( nArg > 1 ){` |
|      - | 3264 | `		/* Maximum data to read */` |
|    ! 0 | 3265 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3266 | `	}` |
|      - | 3267 | `	/* Perform the requested operation */` |
|   6779 | 3268 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6779 | 3269 | `	if( n < 1 ){` |
|      - | 3270 | `		/* EOF or IO error,return FALSE */` |
|      7 | 3271 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3272 | `	}else{` |
|      - | 3273 | `		/* Return the freshly extracted line */` |
|   6777 | 3274 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3275 | `	}` |
|   6779 | 3276 | `	return PH7_OK;` |
|   3392 | 3277 | `}` |
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
|  10978 | 3540 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3541 | `{` |
|      - | 3542 | `	const ph7_io_stream *pStream;` |
|      - | 3543 | `	io_private *pDev;` |
|      - | 3544 | `	int rc;` |
|  10983 | 3545 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3546 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3547 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3548 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3549 | `		return PH7_OK;` |
|      - | 3550 | `	}` |
|      - | 3551 | `	/* Extract our private data */` |
|  10983 | 3552 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3553 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10983 | 3554 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3555 | `		/*Expecting an IO handle */` |
|    ! 0 | 3556 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3557 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3558 | `		return PH7_OK;` |
|      - | 3559 | `	}` |
|      - | 3560 | `	/* Point to the target IO stream device */` |
|  10983 | 3561 | `	pStream = pDev->pStream;` |
|  10983 | 3562 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3563 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3564 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3565 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3566 | `			);` |
|    ! 0 | 3567 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3568 | `		return PH7_OK;` |
|      - | 3569 | `	}` |
|  10983 | 3570 | `	ph7_result_bool(pCtx,0);` |
|      - | 3571 | `	/* Perform the requested operation */` |
|  10983 | 3572 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  10983 | 3573 | `	if( rc != PH7_OK ){` |
|      - | 3574 | `		/* Return FALSE */` |
|   1065 | 3575 | `		ph7_result_bool(pCtx,0);` |
|    530 | 3576 | `	}` |
|  10983 | 3577 | `	return PH7_OK;` |
|   5494 | 3578 | `}` |
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
|   6746 | 3830 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3831 | `{` |
|      - | 3832 | `	const ph7_io_stream *pStream;` |
|      - | 3833 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6751 | 3834 | `	int use_include  = FALSE;` |
|      - | 3835 | `	const char *zFile;` |
|      - | 3836 | `	char zBuf[8192];` |
|      - | 3837 | `	void *pHandle;` |
|      - | 3838 | `	int nLen;` |
|      - | 3839 |  |
|   6751 | 3840 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3841 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3842 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3843 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3844 | `		return PH7_OK;` |
|      - | 3845 | `	}` |
|      - | 3846 | `	/* Extract the file path */` |
|   6751 | 3847 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3848 | `	/* Point to the target IO stream device */` |
|   6751 | 3849 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6751 | 3850 | `	if( pStream == 0 ){` |
|    ! 0 | 3851 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3852 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3853 | `		return PH7_OK;` |
|      - | 3854 | `	}` |
|   6751 | 3855 | `	nMaxlen = -1;` |
|   6751 | 3856 | `	if( nArg > 1 ){` |
|      5 | 3857 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3858 | `	}` |
|      - | 3859 | `	/* Try to open the file in read-only mode */` |
|   6751 | 3860 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6751 | 3861 | `	if( pHandle == 0 ){` |
|    ! 0 | 3862 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3863 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3864 | `		return PH7_OK;` |
|      - | 3865 | `	}` |
|   6751 | 3866 | `	if( nArg > 3 ){` |
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
|   6751 | 3881 | `	nRead = 0;` |
|   6744 | 3882 | `	for(;;){` |
|  20240 | 3883 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6747 | 3884 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  13493 | 3885 | `		if( n < 1 ){` |
|      - | 3886 | `			/* EOF or IO error,break immediately */` |
|   6749 | 3887 | `			break;` |
|      - | 3888 | `		}` |
|      - | 3889 | `		/* Append data */` |
|   6749 | 3890 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3891 | `		/* Increment read counter */` |
|   6749 | 3892 | `		nRead += n;` |
|   6749 | 3893 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3894 | `			/* Read limit reached */` |
|      3 | 3895 | `			break;` |
|      - | 3896 | `		}` |
|      5 | 3897 | `	}` |
|      - | 3898 | `	/* Close the stream */` |
|   6751 | 3899 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3900 | `	/* Check if we have read something */` |
|   6751 | 3901 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3902 | `		/* Nothing read,return FALSE */` |
|      3 | 3903 | `		ph7_result_bool(pCtx,0);` |
|      1 | 3904 | `	}` |
|   6751 | 3905 | `	return PH7_OK;` |
|   3378 | 3906 | `}` |
|      - | 3907 | `/*` |
|      - | 3908 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3909 | ` *  Write a string to a file.` |
|      - | 3910 | ` * Parameters` |
|      - | 3911 | ` *  $filename` |
|      - | 3912 | ` *  Path to the file where to write the data.` |
|      - | 3913 | ` * $data` |
|      - | 3914 | ` *  The data to write(Must be a string).` |
|      - | 3915 | ` * $flags` |
|      - | 3916 | ` *  The value of flags can be any combination of the following` |
|      - | 3917 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3918 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3919 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3920 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3921 | ` * context` |
|      - | 3922 | ` *  A context stream resource.` |
|      - | 3923 | ` * Return` |
|      - | 3924 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3925 | ` */` |
|  13704 | 3926 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3927 | `{` |
|  13709 | 3928 | `	int use_include  = FALSE;` |
|      - | 3929 | `	const ph7_io_stream *pStream;` |
|      - | 3930 | `	const char *zFile;` |
|      - | 3931 | `	const char *zData;` |
|      - | 3932 | `	int iOpenFlags;` |
|      - | 3933 | `	void *pHandle;` |
|      - | 3934 | `	int iFlags;` |
|      - | 3935 | `	int nLen;` |
|      - | 3936 |  |
|  13709 | 3937 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3938 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3939 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3940 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3941 | `		return PH7_OK;` |
|      - | 3942 | `	}` |
|      - | 3943 | `	/* Extract the file path */` |
|  13709 | 3944 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3945 | `	/* Point to the target IO stream device */` |
|  13709 | 3946 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13709 | 3947 | `	if( pStream == 0 ){` |
|    ! 0 | 3948 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3949 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3950 | `		return PH7_OK;` |
|      - | 3951 | `	}` |
|      - | 3952 | `	/* Data to write */` |
|  13709 | 3953 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3954 | `	/* Try to open the file in read-write mode */` |
|  13709 | 3955 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3956 | `	/* Extract the flags */` |
|  13709 | 3957 | `	iFlags = 0;` |
|  13709 | 3958 | `	if( nArg > 2 ){` |
|    ! 0 | 3959 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3960 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3961 | `			use_include = TRUE;` |
|    ! 0 | 3962 | `		}` |
|    ! 0 | 3963 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3964 | `			/* If the file already exists, append the data to the file` |
|      - | 3965 | `			 * instead of overwriting it.` |
|      - | 3966 | `			 */` |
|    ! 0 | 3967 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3968 | `			/* Append mode */` |
|    ! 0 | 3969 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3970 | `		}` |
|    ! 0 | 3971 | `	}` |
|  20561 | 3972 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6852 | 3973 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13709 | 3974 | `	if( pHandle == 0 ){` |
|    ! 0 | 3975 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3976 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3977 | `		return PH7_OK;` |
|      - | 3978 | `	}` |
|  13709 | 3979 | `	if( nLen < 1 ){` |
|      - | 3980 | `		/* Empty data, file is created/truncated */` |
|      7 | 3981 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3982 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3983 | `		return PH7_OK;` |
|      - | 3984 | `	}` |
|  13703 | 3985 | `	if( pStream->xWrite ){` |
|      - | 3986 | `		ph7_int64 n;` |
|  13703 | 3987 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 3988 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3989 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3990 | `		}` |
|      - | 3991 | `		/* Perform the write operation */` |
|  13703 | 3992 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13703 | 3993 | `		if( n < 0 ){` |
|      - | 3994 | `			/* IO error,return FALSE */` |
|    ! 0 | 3995 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3996 | `		}else{` |
|      - | 3997 | `			/* Total number of bytes written */` |
|  13703 | 3998 | `			ph7_result_int64(pCtx,n);` |
|      - | 3999 | `		}` |
|   6854 | 4000 | `	}else{` |
|      - | 4001 | `		/* Read-only stream */` |
|    ! 0 | 4002 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 4003 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 4004 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 4005 | `			);` |
|    ! 0 | 4006 | `		ph7_result_bool(pCtx,0);` |
|      - | 4007 | `	}` |
|      - | 4008 | `	/* Close the handle */` |
|  13703 | 4009 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13703 | 4010 | `	return PH7_OK;` |
|   6857 | 4011 | `}` |
|      - | 4012 | `/*` |
|      - | 4013 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 4014 | ` *  Reads entire file into an array.` |
|      - | 4015 | ` * Parameters` |
|      - | 4016 | ` *  $filename` |
|      - | 4017 | ` *   The filename being read.` |
|      - | 4018 | ` *  $flags` |
|      - | 4019 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 4020 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 4021 | ` *       Search for the file in the include_path.` |
|      - | 4022 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 4023 | ` *       Do not add newline at the end of each array element` |
|      - | 4024 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 4025 | ` *       Skip empty lines` |
|      - | 4026 | ` *  $context` |
|      - | 4027 | ` *   A context stream resource.` |
|      - | 4028 | ` * Return` |
|      - | 4029 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 4030 | ` */` |
|     10 | 4031 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4032 | `{` |
|      - | 4033 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 4034 | `	ph7_value *pArray,*pLine;` |
|      - | 4035 | `	const ph7_io_stream *pStream;` |
|     13 | 4036 | `	int use_include = 0;` |
|      - | 4037 | `	io_private *pDev;` |
|      - | 4038 | `	ph7_int64 n;` |
|      - | 4039 | `	int iFlags;` |
|      - | 4040 | `	int nLen;` |
|      - | 4041 |  |
|     13 | 4042 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4043 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4044 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4045 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4046 | `		return PH7_OK;` |
|      - | 4047 | `	}` |
|      - | 4048 | `	/* Extract the file path */` |
|     13 | 4049 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4050 | `	/* Point to the target IO stream device */` |
|     13 | 4051 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     13 | 4052 | `	if( pStream == 0 ){` |
|    ! 0 | 4053 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4054 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4055 | `		return PH7_OK;` |
|      - | 4056 | `	}` |
|      - | 4057 | `	/* Allocate a new IO private instance */` |
|     13 | 4058 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     13 | 4059 | `	if( pDev == 0 ){` |
|    ! 0 | 4060 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4061 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4062 | `		return PH7_OK;` |
|      - | 4063 | `	}` |
|      - | 4064 | `	/* Initialize the structure */` |
|     13 | 4065 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     13 | 4066 | `	iFlags = 0;` |
|     13 | 4067 | `	if( nArg > 1 ){` |
|    ! 0 | 4068 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 4069 | `	}` |
|      8 | 4070 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 4071 | `		use_include = TRUE;` |
|    ! 0 | 4072 | `	}` |
|      - | 4073 | `	/* Create the array and the working value */` |
|     13 | 4074 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 4075 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     13 | 4076 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 4077 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4078 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4079 | `		return PH7_OK;` |
|      - | 4080 | `	}` |
|      - | 4081 | `	/* Try to open the file in read-only mode */` |
|     13 | 4082 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|     13 | 4083 | `	if( pDev->pHandle == 0 ){` |
|     10 | 4084 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     10 | 4085 | `		ph7_result_bool(pCtx,0);` |
|      - | 4086 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 4087 | `		 * as soon we return from this function.` |
|      - | 4088 | `		 */` |
|     10 | 4089 | `		return PH7_OK;` |
|      - | 4090 | `	}` |
|      - | 4091 | `	/* Perform the requested operation */` |
|      3 | 4092 | `	for(;;){` |
|      - | 4093 | `		/* Try to extract a line */` |
|      7 | 4094 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 4095 | `		if( n < 1 ){` |
|      - | 4096 | `			/* EOF or IO error */` |
|      3 | 4097 | `			break;` |
|      - | 4098 | `		}` |
|      - | 4099 | `		/* Reset the cursor */` |
|      5 | 4100 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 4101 | `		/* Remove line ending if requested by the caller */` |
|      5 | 4102 | `		zPtr = zBuf;` |
|      5 | 4103 | `		zEnd = &zBuf[n];` |
|      5 | 4104 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 4105 | `			/* Ignore trailig lines */` |
|    ! 0 | 4106 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 4107 | `#ifdef __WINNT__` |
|      - | 4108 | `				\|\| zEnd[-1] == '\r'` |
|      - | 4109 | `#endif` |
|      - | 4110 | `				)){` |
|    ! 0 | 4111 | `					n--;` |
|    ! 0 | 4112 | `					zEnd--;` |
|    ! 0 | 4113 | `			}` |
|    ! 0 | 4114 | `		}` |
|      3 | 4115 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 4116 | `			/* Ignore empty lines */` |
|    ! 0 | 4117 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 4118 | `				zPtr++;` |
|    ! 0 | 4119 | `			}` |
|    ! 0 | 4120 | `			if( zPtr >= zEnd ){` |
|      - | 4121 | `				/* Empty line */` |
|    ! 0 | 4122 | `				continue;` |
|      - | 4123 | `			}` |
|    ! 0 | 4124 | `		}` |
|      5 | 4125 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 4126 | `		/* Insert line */` |
|      5 | 4127 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 4128 | `	}` |
|      - | 4129 | `	/* Close the stream */` |
|      3 | 4130 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4131 | `	/* Release the io_private instance */` |
|      3 | 4132 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4133 | `	/* Return the created array */` |
|      3 | 4134 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4135 | `	return PH7_OK;` |
|      8 | 4136 | `}` |
|      - | 4137 | `/*` |
|      - | 4138 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 4139 | ` *  Makes a copy of the file source to dest.` |
|      - | 4140 | ` * Parameters` |
|      - | 4141 | ` *  $source` |
|      - | 4142 | ` *   Path to the source file.` |
|      - | 4143 | ` *  $dest` |
|      - | 4144 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 4145 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 4146 | ` *  $context` |
|      - | 4147 | ` *   A context stream resource.` |
|      - | 4148 | ` * Return` |
|      - | 4149 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4150 | ` */` |
|      4 | 4151 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4152 | `{` |
|      - | 4153 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 4154 | `	const char *zFile;` |
|      - | 4155 | `	char zBuf[8192];` |
|      - | 4156 | `	void *pIn,*pOut;` |
|      - | 4157 | `	ph7_int64 n;` |
|      - | 4158 | `	int nLen;` |
|      6 | 4159 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 4160 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4161 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 4162 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4163 | `		return PH7_OK;` |
|      - | 4164 | `	}` |
|      - | 4165 | `	/* Extract the source name */` |
|      6 | 4166 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4167 | `	/* Point to the target IO stream device */` |
|      6 | 4168 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4169 | `	if( pSin == 0 ){` |
|    ! 0 | 4170 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4171 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4172 | `		return PH7_OK;` |
|      - | 4173 | `	}` |
|      - | 4174 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4175 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4176 | `	if( pIn == 0 ){` |
|      3 | 4177 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 4178 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4179 | `		return PH7_OK;` |
|      - | 4180 | `	}` |
|      - | 4181 | `	/* Extract the destination name */` |
|      3 | 4182 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4183 | `	/* Point to the target IO stream device */` |
|      3 | 4184 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4185 | `	if( pSout == 0 ){` |
|    ! 0 | 4186 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4187 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4188 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4189 | `		return PH7_OK;` |
|      - | 4190 | `	}` |
|      3 | 4191 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4192 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4193 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4194 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4195 | `			);` |
|    ! 0 | 4196 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4197 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4198 | `		return PH7_OK;` |
|      - | 4199 | `	}` |
|      - | 4200 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4201 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4202 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4203 | `	if( pOut == 0 ){` |
|    ! 0 | 4204 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 4205 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4206 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4207 | `		return PH7_OK;` |
|      - | 4208 | `	}` |
|      - | 4209 | `	/* Perform the requested operation */` |
|      2 | 4210 | `	for(;;){` |
|      - | 4211 | `		/* Read from source */` |
|      5 | 4212 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4213 | `		if( n < 1 ){` |
|      - | 4214 | `			/* EOF or IO error,break immediately */` |
|      3 | 4215 | `			break;` |
|      - | 4216 | `		}` |
|      - | 4217 | `		/* Write to dest */` |
|      3 | 4218 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4219 | `		if( n < 1 ){` |
|      - | 4220 | `			/* IO error,break immediately */` |
|    ! 0 | 4221 | `			break;` |
|      - | 4222 | `		}` |
|      1 | 4223 | `	}` |
|      - | 4224 | `	/* Close the streams */` |
|      3 | 4225 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4226 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4227 | `	/* Return TRUE */` |
|      3 | 4228 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4229 | `	return PH7_OK;` |
|      4 | 4230 | `}` |
|      - | 4231 | `/*` |
|      - | 4232 | ` * array fstat(resource $handle)` |
|      - | 4233 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4234 | ` * Parameters` |
|      - | 4235 | ` *  $handle` |
|      - | 4236 | ` *   The file pointer.` |
|      - | 4237 | ` * Return` |
|      - | 4238 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4239 | ` */` |
|      2 | 4240 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4241 | `{` |
|      - | 4242 | `	ph7_value *pArray,*pValue;` |
|      - | 4243 | `	const ph7_io_stream *pStream;` |
|      - | 4244 | `	io_private *pDev;` |
|      3 | 4245 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4246 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4247 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4248 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4249 | `		return PH7_OK;` |
|      - | 4250 | `	}` |
|      - | 4251 | `	/* Extract our private data */` |
|      3 | 4252 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4253 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4254 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4255 | `		/* Expecting an IO handle */` |
|    ! 0 | 4256 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4257 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4258 | `		return PH7_OK;` |
|      - | 4259 | `	}` |
|      - | 4260 | `	/* Point to the target IO stream device */` |
|      3 | 4261 | `	pStream = pDev->pStream;` |
|      3 | 4262 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4263 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4264 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4265 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4266 | `			);` |
|    ! 0 | 4267 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4268 | `		return PH7_OK;` |
|      - | 4269 | `	}` |
|      - | 4270 | `	/* Create the array and the working value */` |
|      3 | 4271 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4272 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4273 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4274 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4275 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4276 | `		return PH7_OK;` |
|      - | 4277 | `	}` |
|      - | 4278 | `	/* Perform the requested operation */` |
|      3 | 4279 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4280 | `	/* Return the freshly created array */` |
|      3 | 4281 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4282 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4283 | `	 * released automatically as soon we return from this function.` |
|      - | 4284 | `	 */` |
|      3 | 4285 | `	return PH7_OK;` |
|      2 | 4286 | `}` |
|      - | 4287 | `/*` |
|      - | 4288 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4289 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4290 | ` * Parameters` |
|      - | 4291 | ` *  $handle` |
|      - | 4292 | ` *   The file pointer.` |
|      - | 4293 | ` *  $string` |
|      - | 4294 | ` *   The string that is to be written.` |
|      - | 4295 | ` *  $length` |
|      - | 4296 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4297 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4298 | ` * Return` |
|      - | 4299 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4300 | ` */` |
|     44 | 4301 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4302 | `{` |
|      - | 4303 | `	const ph7_io_stream *pStream;` |
|      - | 4304 | `	const char *zString;` |
|      - | 4305 | `	io_private *pDev;` |
|      - | 4306 | `	int nLen,n;` |
|     46 | 4307 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4308 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4309 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4310 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4311 | `		return PH7_OK;` |
|      - | 4312 | `	}` |
|      - | 4313 | `	/* Extract our private data */` |
|     46 | 4314 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4315 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     46 | 4316 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4317 | `		/* Expecting an IO handle */` |
|    ! 0 | 4318 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4319 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4320 | `		return PH7_OK;` |
|      - | 4321 | `	}` |
|      - | 4322 | `	/* Point to the target IO stream device */` |
|     46 | 4323 | `	pStream = pDev->pStream;` |
|     46 | 4324 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4325 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4326 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4327 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4328 | `			);` |
|    ! 0 | 4329 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4330 | `		return PH7_OK;` |
|      - | 4331 | `	}` |
|      - | 4332 | `	/* Extract the data to write */` |
|     46 | 4333 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     46 | 4334 | `	if( nArg > 2 ){` |
|      - | 4335 | `		/* Maximum data length to write */` |
|    ! 0 | 4336 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4337 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4338 | `			nLen = n;` |
|    ! 0 | 4339 | `		}` |
|    ! 0 | 4340 | `	}` |
|     46 | 4341 | `	if( nLen < 1 ){` |
|      - | 4342 | `		/* Nothing to write */` |
|    ! 0 | 4343 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4344 | `		return PH7_OK;` |
|      - | 4345 | `	}` |
|      - | 4346 | `	/* Perform the requested operation */` |
|     46 | 4347 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|     46 | 4348 | `	if( n <  0 ){` |
|      - | 4349 | `		/* IO error,return FALSE */` |
|    ! 0 | 4350 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4351 | `	}else{` |
|      - | 4352 | `		/* #Bytes written */` |
|     46 | 4353 | `		ph7_result_int(pCtx,n);` |
|      - | 4354 | `	}` |
|     46 | 4355 | `	return PH7_OK;` |
|     24 | 4356 | `}` |
|      - | 4357 | `/*` |
|      - | 4358 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4359 | ` *  Portable advisory file locking.` |
|      - | 4360 | ` * Parameters` |
|      - | 4361 | ` *  $handle` |
|      - | 4362 | ` *   The file pointer.` |
|      - | 4363 | ` *  $operation` |
|      - | 4364 | ` *   operation is one of the following:` |
|      - | 4365 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4366 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4367 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4368 | ` * Return` |
|      - | 4369 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4370 | ` */` |
|      4 | 4371 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 4372 | `{` |
|      - | 4373 | `	const ph7_io_stream *pStream;` |
|      - | 4374 | `	io_private *pDev;` |
|      - | 4375 | `	int nLock;` |
|      - | 4376 | `	int rc;` |
|      4 | 4377 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4378 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4379 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4380 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4381 | `		return PH7_OK;` |
|      - | 4382 | `	}` |
|      - | 4383 | `	/* Extract our private data */` |
|      4 | 4384 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4385 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      4 | 4386 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4387 | `		/*Expecting an IO handle */` |
|    ! 0 | 4388 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4389 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4390 | `		return PH7_OK;` |
|      - | 4391 | `	}` |
|      - | 4392 | `	/* Point to the target IO stream device */` |
|      4 | 4393 | `	pStream = pDev->pStream;` |
|      4 | 4394 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4395 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4396 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4397 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4398 | `			);` |
|    ! 0 | 4399 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4400 | `		return PH7_OK;` |
|      - | 4401 | `	}` |
|      - | 4402 | `	/* Requested lock operation */` |
|      4 | 4403 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4404 | `	/*` |
|      - | 4405 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally \|LOCK_NB=4)` |
|      - | 4406 | `	 * into the xLock() vtable contract, which is a PUBLIC C API and stays as it is:` |
|      - | 4407 | `	 * negative = unlock, 1 = exclusive, anything else = shared.` |
|      - | 4408 | `	 */` |
|      - | 4409 | `	{` |
|      4 | 4410 | `		int iOp = nLock & ~4 /* strip LOCK_NB */;` |
|      4 | 4411 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      2 | 4412 | `			nLock = -1;` |
|      3 | 4413 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|      2 | 4414 | `			nLock = 1;` |
|      1 | 4415 | `		}else{` |
|    ! 0 | 4416 | `			nLock = 0; /* LOCK_SH */` |
|      - | 4417 | `		}` |
|      - | 4418 | `	}` |
|      - | 4419 | `	/* Lock operation */` |
|      4 | 4420 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4421 | `	/* IO result */` |
|      4 | 4422 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 4423 | `	return PH7_OK;` |
|      2 | 4424 | `}` |
|      - | 4425 | `/*` |
|      - | 4426 | ` * int fpassthru(resource $handle)` |
|      - | 4427 | ` *  Output all remaining data on a file pointer.` |
|      - | 4428 | ` * Parameters` |
|      - | 4429 | ` *  $handle` |
|      - | 4430 | ` *   The file pointer.` |
|      - | 4431 | ` * Return` |
|      - | 4432 | ` *  Total number of characters read from handle and passed through` |
|      - | 4433 | ` *  to the output on success or FALSE on failure.` |
|      - | 4434 | ` */` |
|      2 | 4435 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4436 | `{` |
|      - | 4437 | `	const ph7_io_stream *pStream;` |
|      - | 4438 | `	io_private *pDev;` |
|      - | 4439 | `	ph7_int64 n,nRead;` |
|      - | 4440 | `	char zBuf[8192];` |
|      - | 4441 | `	int rc;` |
|      3 | 4442 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4443 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4444 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4445 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4446 | `		return PH7_OK;` |
|      - | 4447 | `	}` |
|      - | 4448 | `	/* Extract our private data */` |
|      3 | 4449 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4450 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4451 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4452 | `		/*Expecting an IO handle */` |
|    ! 0 | 4453 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4454 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4455 | `		return PH7_OK;` |
|      - | 4456 | `	}` |
|      - | 4457 | `	/* Point to the target IO stream device */` |
|      3 | 4458 | `	pStream = pDev->pStream;` |
|      3 | 4459 | `	if( pStream == 0  ){` |
|    ! 0 | 4460 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4461 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4462 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4463 | `			);` |
|    ! 0 | 4464 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4465 | `		return PH7_OK;` |
|      - | 4466 | `	}` |
|      - | 4467 | `	/* Perform the requested operation */` |
|      3 | 4468 | `	nRead = 0;` |
|      2 | 4469 | `	for(;;){` |
|      5 | 4470 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4471 | `		if( n < 1 ){` |
|      - | 4472 | `			/* Error or EOF */` |
|      3 | 4473 | `			break;` |
|      - | 4474 | `		}` |
|      - | 4475 | `		/* Increment the read counter */` |
|      3 | 4476 | `		nRead += n;` |
|      - | 4477 | `		/* Output data */` |
|      3 | 4478 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4479 | `		if( rc == PH7_ABORT ){` |
|      - | 4480 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4481 | `			break;` |
|      - | 4482 | `		}` |
|      1 | 4483 | `	}` |
|      - | 4484 | `	/* Total number of bytes readen */` |
|      3 | 4485 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4486 | `	return PH7_OK;` |
|      2 | 4487 | `}` |
|      - | 4488 | `/* CSV reader/writer private data */` |
|      - | 4489 | `struct csv_data` |
|      - | 4490 | `{` |
|      - | 4491 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4492 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4493 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4494 | `	int iCount;       /* Counter */` |
|      - | 4495 | `};` |
|      - | 4496 | `/*` |
|      - | 4497 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4498 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4499 | ` * associated data.` |
|      - | 4500 | ` */` |
|      6 | 4501 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4502 | `{` |
|      7 | 4503 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4504 | `	const char *zData;` |
|      - | 4505 | `	int nLen,c2;` |
|      - | 4506 | `	sxu32 n;` |
|      - | 4507 | `	/* Point to the raw data */` |
|      7 | 4508 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4509 | `	if( nLen < 1 ){` |
|      - | 4510 | `		/* Nothing to write */` |
|    ! 0 | 4511 | `		return PH7_OK;` |
|      - | 4512 | `	}` |
|      7 | 4513 | `	if( pData->iCount > 0 ){` |
|      - | 4514 | `		/* Write the delimiter */` |
|      5 | 4515 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4516 | `	}` |
|      7 | 4517 | `	n = 1;` |
|      7 | 4518 | `	c2 = 0;` |
|     10 | 4519 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4520 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4521 | `			c2 = 1;` |
|    ! 0 | 4522 | `			if( n == 0 ){` |
|    ! 0 | 4523 | `				c2 = 2;` |
|    ! 0 | 4524 | `			}` |
|      - | 4525 | `			/* Write the enclosure */` |
|    ! 0 | 4526 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4527 | `			if( c2 > 1 ){` |
|    ! 0 | 4528 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4529 | `			}` |
|    ! 0 | 4530 | `	}` |
|      - | 4531 | `	/* Write the data */` |
|      7 | 4532 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4533 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4534 | `		return PH7_ABORT;` |
|      - | 4535 | `	}` |
|      7 | 4536 | `	if( c2 > 0 ){` |
|      - | 4537 | `		/* Write the enclosure */` |
|    ! 0 | 4538 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4539 | `		if( c2 > 1 ){` |
|    ! 0 | 4540 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4541 | `		}` |
|    ! 0 | 4542 | `	}` |
|      7 | 4543 | `	pData->iCount++;` |
|      7 | 4544 | `	return PH7_OK;` |
|      4 | 4545 | `}` |
|      - | 4546 | `/*` |
|      - | 4547 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4548 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4549 | ` * Parameters` |
|      - | 4550 | ` *  $handle` |
|      - | 4551 | ` *   Open file handle.` |
|      - | 4552 | ` * $fields` |
|      - | 4553 | ` *   An array of values.` |
|      - | 4554 | ` * $delimiter` |
|      - | 4555 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4556 | ` * $enclosure` |
|      - | 4557 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4558 | ` */` |
|      2 | 4559 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4560 | `{` |
|      - | 4561 | `	const ph7_io_stream *pStream;` |
|      - | 4562 | `	struct csv_data sCsv;` |
|      - | 4563 | `	io_private *pDev;` |
|      - | 4564 | `	char *zEol;` |
|      - | 4565 | `	int eolen;` |
|      3 | 4566 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4567 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4568 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4569 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4570 | `		return PH7_OK;` |
|      - | 4571 | `	}` |
|      - | 4572 | `	/* Extract our private data */` |
|      3 | 4573 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4574 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4575 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4576 | `		/*Expecting an IO handle */` |
|    ! 0 | 4577 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4578 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4579 | `		return PH7_OK;` |
|      - | 4580 | `	}` |
|      - | 4581 | `	/* Point to the target IO stream device */` |
|      3 | 4582 | `	pStream = pDev->pStream;` |
|      3 | 4583 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4584 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4585 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4586 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4587 | `			);` |
|    ! 0 | 4588 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4589 | `		return PH7_OK;` |
|      - | 4590 | `	}` |
|      - | 4591 | `	/* Set default csv separator */` |
|      3 | 4592 | `	sCsv.delimiter = ',';` |
|      3 | 4593 | `	sCsv.enclosure = '"';` |
|      3 | 4594 | `	sCsv.pDev = pDev;` |
|      3 | 4595 | `	sCsv.iCount = 0;` |
|      3 | 4596 | `	if( nArg > 2 ){` |
|      - | 4597 | `		/* User delimiter */` |
|      - | 4598 | `		const char *z;` |
|      - | 4599 | `		int n;` |
|      3 | 4600 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4601 | `		if( n > 0 ){` |
|      3 | 4602 | `			sCsv.delimiter = z[0];` |
|      1 | 4603 | `		}` |
|      3 | 4604 | `		if( nArg > 3 ){` |
|      3 | 4605 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4606 | `			if( n > 0 ){` |
|      3 | 4607 | `				sCsv.enclosure = z[0];` |
|      1 | 4608 | `			}` |
|      1 | 4609 | `		}` |
|      1 | 4610 | `	}` |
|      - | 4611 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4612 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4613 | `	/* Write a line ending */` |
|      - | 4614 | `#ifdef __WINNT__` |
|      1 | 4615 | `	zEol = "\r\n";` |
|      1 | 4616 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4617 | `#else` |
|      - | 4618 | `	/* Assume UNIX LF */` |
|      2 | 4619 | `	zEol = "\n";` |
|      2 | 4620 | `	eolen = (int)sizeof(char);` |
|      - | 4621 | `#endif` |
|      3 | 4622 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4623 | `	return PH7_OK;` |
|      2 | 4624 | `}` |
|      - | 4625 | `/*` |
|      - | 4626 | ` * fprintf,vfprintf private data.` |
|      - | 4627 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4628 | ` * input consumer callback defined below.` |
|      - | 4629 | ` */` |
|      - | 4630 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4631 | `struct fprintf_data` |
|      - | 4632 | `{` |
|      - | 4633 | `	io_private *pIO;        /* IO stream */` |
|      - | 4634 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4635 | `};` |
|      - | 4636 | `/*` |
|      - | 4637 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4638 | ` */` |
|     30 | 4639 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4640 | `{` |
|     31 | 4641 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4642 | `	ph7_int64 n;` |
|      - | 4643 | `	/* Write the formatted data */` |
|     31 | 4644 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     31 | 4645 | `	if( n < 1 ){` |
|    ! 0 | 4646 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4647 | `		/* IO error,abort immediately */` |
|    ! 0 | 4648 | `		return SXERR_ABORT;` |
|      - | 4649 | `	}` |
|      - | 4650 | `	/* Increment counter */` |
|     31 | 4651 | `	pFdata->nCount += n;` |
|     31 | 4652 | `	return PH7_OK;` |
|     16 | 4653 | `}` |
|      - | 4654 | `/*` |
|      - | 4655 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4656 | ` *  Write a formatted string to a stream.` |
|      - | 4657 | ` * Parameters` |
|      - | 4658 | ` *  $handle` |
|      - | 4659 | ` *   The file pointer.` |
|      - | 4660 | ` *  $format` |
|      - | 4661 | ` *   String format (see sprintf()).` |
|      - | 4662 | ` * Return` |
|      - | 4663 | ` *  The length of the written string.` |
|      - | 4664 | ` */` |
|     18 | 4665 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4666 | `{` |
|      - | 4667 | `	fprintf_data sFdata;` |
|      - | 4668 | `	const char *zFormat;` |
|      - | 4669 | `	io_private *pDev;` |
|      - | 4670 | `	int nLen;` |
|     19 | 4671 | `	if( nArg < 2 ){` |
|    ! 0 | 4672 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4673 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4674 | `		return PH7_OK;` |
|      - | 4675 | `	}` |
|      - | 4676 | `	{` |
|      - | 4677 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     19 | 4678 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     19 | 4679 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 4680 | `			return rcs;` |
|      - | 4681 | `		}` |
|      - | 4682 | `	}` |
|      - | 4683 | `	/* Extract our private data */` |
|     19 | 4684 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4685 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     19 | 4686 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4687 | `		/*Expecting an IO handle */` |
|    ! 0 | 4688 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4689 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4690 | `		return PH7_OK;` |
|      - | 4691 | `	}` |
|      - | 4692 | `	/* Point to the target IO stream device */` |
|     19 | 4693 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4694 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4695 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4696 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4697 | `			);` |
|    ! 0 | 4698 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4699 | `		return PH7_OK;` |
|      - | 4700 | `	}` |
|      - | 4701 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 4702 | `	{` |
|     19 | 4703 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     19 | 4704 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4705 | `			return rcf;` |
|      - | 4706 | `		}` |
|      - | 4707 | `	}` |
|      - | 4708 | `	/* Extract the string format (scalars/null coerce). */` |
|     19 | 4709 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     19 | 4710 | `	if( nLen < 1 ){` |
|      - | 4711 | `		/* Empty string,return zero */` |
|    ! 0 | 4712 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4713 | `		return PH7_OK;` |
|      - | 4714 | `	}` |
|      - | 4715 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4716 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4717 | `	{` |
|     19 | 4718 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     19 | 4719 | `		if( rcv != PH7_OK ){` |
|      3 | 4720 | `			return rcv;` |
|      - | 4721 | `		}` |
|      - | 4722 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output.` |
|      - | 4723 | `		 * fprintf's values start at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     17 | 4724 | `		rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     17 | 4725 | `		if( rcv != PH7_OK ){` |
|      3 | 4726 | `			return rcv;` |
|      - | 4727 | `		}` |
|      - | 4728 | `	}` |
|      - | 4729 | `	/* Prepare our private data */` |
|     15 | 4730 | `	sFdata.nCount = 0;` |
|     15 | 4731 | `	sFdata.pIO = pDev;` |
|      - | 4732 | `	/* Format the string */` |
|     15 | 4733 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4734 | `	/* Return total number of bytes written */` |
|     15 | 4735 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     15 | 4736 | `	return PH7_OK;` |
|     10 | 4737 | `}` |
|      - | 4738 | `/*` |
|      - | 4739 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4740 | ` *  Write a formatted string to a stream.` |
|      - | 4741 | ` * Parameters` |
|      - | 4742 | ` *  $handle` |
|      - | 4743 | ` *   The file pointer.` |
|      - | 4744 | ` *  $format` |
|      - | 4745 | ` *   String format (see sprintf()).` |
|      - | 4746 | ` * $args` |
|      - | 4747 | ` *   User arguments.` |
|      - | 4748 | ` * Return` |
|      - | 4749 | ` *  The length of the written string.` |
|      - | 4750 | ` */` |
|      6 | 4751 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4752 | `{` |
|      - | 4753 | `	fprintf_data sFdata;` |
|      - | 4754 | `	const char *zFormat;` |
|      - | 4755 | `	ph7_hashmap *pMap;` |
|      - | 4756 | `	io_private *pDev;` |
|      - | 4757 | `	SySet sArg;` |
|      - | 4758 | `	int n,nLen;` |
|      7 | 4759 | `	if( nArg < 3 ){` |
|    ! 0 | 4760 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4761 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4762 | `		return PH7_OK;` |
|      - | 4763 | `	}` |
|      - | 4764 | `	{` |
|      - | 4765 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|      7 | 4766 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|      7 | 4767 | `		if( rcs != PH7_OK ){` |
|      3 | 4768 | `			return rcs;` |
|      - | 4769 | `		}` |
|      - | 4770 | `	}` |
|      - | 4771 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 4772 | `	{` |
|      5 | 4773 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      5 | 4774 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4775 | `			return rcf;` |
|      - | 4776 | `		}` |
|      - | 4777 | `	}` |
|      5 | 4778 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 4779 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4780 | `		char zBuf[64];` |
|    ! 0 | 4781 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4782 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 4783 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 4784 | `	}` |
|      - | 4785 | `	/* Extract our private data */` |
|      5 | 4786 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4787 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4788 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4789 | `		/*Expecting an IO handle */` |
|    ! 0 | 4790 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4791 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4792 | `		return PH7_OK;` |
|      - | 4793 | `	}` |
|      - | 4794 | `	/* Point to the target IO stream device */` |
|      5 | 4795 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4796 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4797 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4798 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4799 | `			);` |
|    ! 0 | 4800 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4801 | `		return PH7_OK;` |
|      - | 4802 | `	}` |
|      - | 4803 | `	/* Extract the string format */` |
|      5 | 4804 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      5 | 4805 | `	if( nLen < 1 ){` |
|      - | 4806 | `		/* Empty string,return zero */` |
|    ! 0 | 4807 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4808 | `		return PH7_OK;` |
|      - | 4809 | `	}` |
|      - | 4810 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4811 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4812 | `	{` |
|      5 | 4813 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      5 | 4814 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4815 | `			return rcv;` |
|      - | 4816 | `		}` |
|      - | 4817 | `	}` |
|      - | 4818 | `	/* Point to hashmap */` |
|      5 | 4819 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4820 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output. */` |
|      - | 4821 | `	{` |
|      5 | 4822 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      5 | 4823 | `		if( rcc != PH7_OK ){` |
|      3 | 4824 | `			return rcc;` |
|      - | 4825 | `		}` |
|      - | 4826 | `	}` |
|      - | 4827 | `	/* Extract arguments from the hashmap */` |
|      3 | 4828 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4829 | `	/* Prepare our private data */` |
|      3 | 4830 | `	sFdata.nCount = 0;` |
|      3 | 4831 | `	sFdata.pIO = pDev;` |
|      - | 4832 | `	/* Format the string */` |
|      3 | 4833 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4834 | `	/* Return total number of bytes written*/` |
|      3 | 4835 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4836 | `	SySetRelease(&sArg);` |
|      3 | 4837 | `	return PH7_OK;` |
|      4 | 4838 | `}` |
|      - | 4839 | `/*` |
|      - | 4840 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4841 | ` * According to the PHP reference manual:` |
|      - | 4842 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4843 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4844 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4845 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4846 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4847 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4848 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4849 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4850 | ` *         exist, attempt to create it.` |
|      - | 4851 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4852 | ` *          not exist, attempt to create it.` |
|      - | 4853 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4854 | ` *         already exists,` |
|      - | 4855 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4856 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4857 | ` *         the underlying open(2) system call.` |
|      - | 4858 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4859 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4860 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4861 | ` *          is positioned on the beginning of the file.` |
|      - | 4862 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4863 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4864 | ` *          be used after the lock is requested).` |
|      - | 4865 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4866 | ` */` |
|    218 | 4867 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      3 | 4868 | `{` |
|    221 | 4869 | `	const char *zEnd = &zMode[nLen];` |
|    221 | 4870 | `	int iFlag = 0;` |
|      - | 4871 | `	int c;` |
|    221 | 4872 | `	if( nLen < 1 ){` |
|      - | 4873 | `		/* Open in a read-only mode */` |
|    ! 0 | 4874 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4875 | `	}` |
|    221 | 4876 | `	c = zMode[0];` |
|    221 | 4877 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4878 | `		/* Read-only access */` |
|     38 | 4879 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     38 | 4880 | `		zMode++; /* Advance */` |
|     38 | 4881 | `		if( zMode < zEnd ){` |
|     17 | 4882 | `			c = zMode[0];` |
|     17 | 4883 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4884 | `				/* Read+Write access */` |
|     17 | 4885 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      8 | 4886 | `			}` |
|     11 | 4887 | `		}` |
|    193 | 4888 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4889 | `		/* Overwrite mode.` |
|      - | 4890 | `		 * If the file does not exists,try to create it` |
|      - | 4891 | `		 */` |
|     20 | 4892 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     20 | 4893 | `		zMode++; /* Advance */` |
|     20 | 4894 | `		if( zMode < zEnd ){` |
|      5 | 4895 | `			c = zMode[0];` |
|      5 | 4896 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4897 | `				/* Read+Write access */` |
|      5 | 4898 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      5 | 4899 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      2 | 4900 | `			}` |
|      4 | 4901 | `		}` |
|    150 | 4902 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4903 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4904 | `		 * Create the file if it does not exists.` |
|      - | 4905 | `		 */` |
|    ! 0 | 4906 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4907 | `		zMode++; /* Advance */` |
|    ! 0 | 4908 | `		if( zMode < zEnd ){` |
|    ! 0 | 4909 | `			c = zMode[0];` |
|    ! 0 | 4910 | `			if( c == '+' ){` |
|      - | 4911 | `				/* Read-Write access */` |
|    ! 0 | 4912 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4913 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4914 | `			}` |
|    ! 0 | 4915 | `		}` |
|    134 | 4916 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4917 | `		/* Exclusive access.` |
|      - | 4918 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4919 | `		 * Otherwise create a new file.` |
|      - | 4920 | `		 */` |
|     68 | 4921 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|     68 | 4922 | `		zMode++; /* Advance */` |
|     68 | 4923 | `		if( zMode < zEnd ){` |
|    ! 0 | 4924 | `			c = zMode[0];` |
|    ! 0 | 4925 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4926 | `				/* Read-Write access */` |
|    ! 0 | 4927 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4928 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4929 | `			}` |
|      2 | 4930 | `		}` |
|     66 | 4931 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4932 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4933 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4934 | `		zMode++; /* Advance */` |
|    ! 0 | 4935 | `		if( zMode < zEnd ){` |
|    ! 0 | 4936 | `			c = zMode[0];` |
|    ! 0 | 4937 | `			if( c == '+' ){` |
|      - | 4938 | `				/* Read-Write access */` |
|    ! 0 | 4939 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4940 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4941 | `			}` |
|    ! 0 | 4942 | `		}` |
|    ! 0 | 4943 | `	}else{` |
|      - | 4944 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4945 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4946 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4947 | `	}` |
|    241 | 4948 | `	while( zMode < zEnd ){` |
|     21 | 4949 | `		c = zMode[0];` |
|     21 | 4950 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4951 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4952 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     21 | 4953 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4954 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4955 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4956 | `		}` |
|     21 | 4957 | `		zMode++;` |
|      1 | 4958 | `	}` |
|    221 | 4959 | `	return iFlag;` |
|    112 | 4960 | `}` |
|      - | 4961 | `/*` |
|      - | 4962 | ` * Initialize the IO private structure.` |
|      - | 4963 | ` */` |
|   5300 | 4964 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 4965 | `{` |
|   5305 | 4966 | `	pOut->pStream = pStream;` |
|   5305 | 4967 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   5305 | 4968 | `	pOut->nOfft = 0;` |
|      - | 4969 | `	/* Set the magic number */` |
|   5305 | 4970 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   5305 | 4971 | `}` |
|      - | 4972 | `/*` |
|      - | 4973 | ` * Release the IO private structure.` |
|      - | 4974 | ` */` |
|      2 | 4975 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      1 | 4976 | `{` |
|      3 | 4977 | `	SyBlobRelease(&pDev->sBuffer);` |
|      3 | 4978 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4979 | `	/* Release the whole structure */` |
|      3 | 4980 | `	ph7_context_free_chunk(pCtx,pDev);` |
|      3 | 4981 | `}` |
|      - | 4982 | `/*` |
|      - | 4983 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 4984 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 4985 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 4986 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 4987 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 4988 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 4989 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 4990 | ` */` |
|   5242 | 4991 | `static void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 4992 | `{` |
|   5247 | 4993 | `	SyBlobRelease(&pDev->sBuffer);` |
|   5247 | 4994 | `	pDev->pHandle = 0;` |
|   5247 | 4995 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   5247 | 4996 | `}` |
|      - | 4997 | `/*` |
|      - | 4998 | ` * Reset the IO private structure.` |
|      - | 4999 | ` */` |
|     30 | 5000 | `static void ResetIOPrivate(io_private *pDev)` |
|      2 | 5001 | `{` |
|     32 | 5002 | `	SyBlobReset(&pDev->sBuffer);` |
|     32 | 5003 | `	pDev->nOfft = 0;` |
|     32 | 5004 | `}` |
|      - | 5005 | `/* Forward declaration */` |
|      - | 5006 |  |
|      - | 5007 | `/*` |
|      - | 5008 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 5009 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 5010 | ` * Parameters` |
|      - | 5011 | ` *  $filename` |
|      - | 5012 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 5013 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 5014 | ` *   then a regular file is assumed.` |
|      - | 5015 | ` *  $mode` |
|      - | 5016 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 5017 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 5018 | ` *   modes.` |
|      - | 5019 | ` *  $use_include_path` |
|      - | 5020 | ` *   You can use the optional second parameter and set it to` |
|      - | 5021 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 5022 | ` *  $context` |
|      - | 5023 | ` *   A context stream resource.` |
|      - | 5024 | ` * Return` |
|      - | 5025 | ` *  File handle on success or FALSE on failure.` |
|      - | 5026 | ` */` |
|      - | 5027 | `/*` |
|      - | 5028 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 5029 | ` *                                  int $offset = -1)` |
|      - | 5030 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 5031 | ` */` |
|     28 | 5032 | `static int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5033 | `{` |
|      - | 5034 | `	const ph7_io_stream *pStream;` |
|      - | 5035 | `	io_private *pDev;` |
|     29 | 5036 | `	ph7_int64 nMax = -1;` |
|      - | 5037 | `	char zBuf[4096];` |
|      - | 5038 | `	ph7_int64 nRead;` |
|     29 | 5039 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 5040 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5041 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5042 | `		return PH7_OK;` |
|      - | 5043 | `	}` |
|     29 | 5044 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     29 | 5045 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5046 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5047 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5048 | `		return PH7_OK;` |
|      - | 5049 | `	}` |
|     29 | 5050 | `	pStream = pDev->pStream;` |
|     29 | 5051 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 5052 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5053 | `		return PH7_OK;` |
|      - | 5054 | `	}` |
|     29 | 5055 | `	if( nArg > 1 ){` |
|      5 | 5056 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      2 | 5057 | `	}` |
|     17 | 5058 | `	if( nArg > 2 ){` |
|      5 | 5059 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 5060 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 5061 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 5062 | `		}` |
|      2 | 5063 | `	}` |
|     29 | 5064 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     52 | 5065 | `	while( nMax != 0 ){` |
|     50 | 5066 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     50 | 5067 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 5068 | `			nAsk = nMax;` |
|      1 | 5069 | `		}` |
|     50 | 5070 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     50 | 5071 | `		if( nRead < 1 ){` |
|     27 | 5072 | `			break;` |
|      - | 5073 | `		}` |
|     24 | 5074 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     24 | 5075 | `		if( nMax > 0 ){` |
|      3 | 5076 | `			nMax -= nRead;` |
|      1 | 5077 | `		}` |
|      1 | 5078 | `	}` |
|     29 | 5079 | `	return PH7_OK;` |
|     15 | 5080 | `}` |
|      - | 5081 | `/*` |
|      - | 5082 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 5083 | ` */` |
|      4 | 5084 | `static int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5085 | `{` |
|      - | 5086 | `	ph7_value *pArr,*pV;` |
|      - | 5087 | `	ph7_io_stream **apDev;` |
|      - | 5088 | `	sxu32 n;` |
|      2 | 5089 | `	SXUNUSED(nArg);` |
|      2 | 5090 | `	SXUNUSED(apArg);` |
|      6 | 5091 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 5092 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 5093 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 5094 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5095 | `		return PH7_OK;` |
|      - | 5096 | `	}` |
|      6 | 5097 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 5098 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 5099 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 5100 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 5101 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 5102 | `	}` |
|      6 | 5103 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 5104 | `	return PH7_OK;` |
|      4 | 5105 | `}` |
|      - | 5106 | `/*` |
|      - | 5107 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 5108 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 5109 | ` */` |
|      2 | 5110 | `static int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5111 | `{` |
|      - | 5112 | `	io_private *pDev;` |
|      - | 5113 | `	ph7_value *pArr,*pV;` |
|      3 | 5114 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 5115 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5116 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5117 | `		return PH7_OK;` |
|      - | 5118 | `	}` |
|      3 | 5119 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5120 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5121 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5122 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5123 | `		return PH7_OK;` |
|      - | 5124 | `	}` |
|      3 | 5125 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 5126 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 5127 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 5128 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5129 | `		return PH7_OK;` |
|      - | 5130 | `	}` |
|      3 | 5131 | `	ph7_value_bool(pV,0);` |
|      3 | 5132 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 5133 | `	ph7_value_bool(pV,1);` |
|      3 | 5134 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 5135 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 5136 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 5137 | `	ph7_value_bool(pV,0);` |
|      3 | 5138 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 5139 | `	ph7_value_int(pV,0);` |
|      3 | 5140 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 5141 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 5142 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 5143 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 5144 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 5145 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 5146 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 5147 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 5148 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 5149 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 5150 | `	return PH7_OK;` |
|      2 | 5151 | `}` |
|      - | 5152 | `/*` |
|      - | 5153 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 5154 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 5155 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 5156 | ` * resource, options unconsumed).` |
|      - | 5157 | ` */` |
|      2 | 5158 | `static int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5159 | `{` |
|      3 | 5160 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 5161 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 5162 | `	}else{` |
|    ! 0 | 5163 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 5164 | `		if( pArr == 0 ){` |
|    ! 0 | 5165 | `			ph7_result_null(pCtx);` |
|    ! 0 | 5166 | `			return PH7_OK;` |
|      - | 5167 | `		}` |
|    ! 0 | 5168 | `		ph7_result_value(pCtx,pArr);` |
|      - | 5169 | `	}` |
|      3 | 5170 | `	return PH7_OK;` |
|      2 | 5171 | `}` |
|      - | 5172 | `/*` |
|      - | 5173 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 5174 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 5175 | ` */` |
|      - | 5176 | `#ifdef PH7_ENABLE_NET` |
|      - | 5177 | `typedef struct sock_private sock_private;` |
|      - | 5178 | `struct sock_private` |
|      - | 5179 | `{` |
|      - | 5180 | `	ph7_vm *pVm;` |
|      - | 5181 | `	ph7_socket sock;` |
|      - | 5182 | `	int bEof;` |
|      - | 5183 | `};` |
|     13 | 5184 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 5185 | `{` |
|     13 | 5186 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5187 | `	int n;` |
|     13 | 5188 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      1 | 5189 | `		return 0;` |
|      - | 5190 | `	}` |
|     12 | 5191 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     12 | 5192 | `	if( n <= 0 ){` |
|      4 | 5193 | `		pSock->bEof = 1;` |
|      4 | 5194 | `		return 0;` |
|      - | 5195 | `	}` |
|      8 | 5196 | `	return (ph7_int64)n;` |
|      5 | 5197 | `}` |
|      4 | 5198 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 5199 | `{` |
|      4 | 5200 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5201 | `	int n;` |
|      4 | 5202 | `	if( pSock == 0 ){` |
|    ! 0 | 5203 | `		return -1;` |
|      - | 5204 | `	}` |
|      4 | 5205 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 5206 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 5207 | `}` |
|      4 | 5208 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 5209 | `{` |
|      4 | 5210 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 5211 | `	if( pSock == 0 ){` |
|    ! 0 | 5212 | `		return;` |
|      - | 5213 | `	}` |
|      4 | 5214 | `	PH7_NetClose(pSock->sock);` |
|      4 | 5215 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 5216 | `}` |
|      - | 5217 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 5218 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 5219 | `{` |
|      - | 5220 | `	sock_private *pSock;` |
|      - | 5221 | `	ph7_socket sock;` |
|      - | 5222 | `	char zHost[256];` |
|      - | 5223 | `	const char *zColon;` |
|    ! 0 | 5224 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 5225 | `	const char *zErr = "";` |
|    ! 0 | 5226 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 5227 | `	SXUNUSED(iMode);` |
|    ! 0 | 5228 | `	if( pVm == 0 ){` |
|    ! 0 | 5229 | `		return -1;` |
|      - | 5230 | `	}` |
|    ! 0 | 5231 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 5232 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 5233 | `		zColon--;` |
|    ! 0 | 5234 | `	}` |
|    ! 0 | 5235 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 5236 | `		return -1;` |
|      - | 5237 | `	}` |
|      - | 5238 | `	{` |
|    ! 0 | 5239 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 5240 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 5241 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 5242 | `		}` |
|    ! 0 | 5243 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 5244 | `		zHost[n] = 0;` |
|      - | 5245 | `	}` |
|      - | 5246 | `	{` |
|    ! 0 | 5247 | `		sxi32 iTmp = 0;` |
|    ! 0 | 5248 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 5249 | `		iPort = (int)iTmp;` |
|      - | 5250 | `	}` |
|    ! 0 | 5251 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 5252 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 5253 | `		return -1;` |
|      - | 5254 | `	}` |
|    ! 0 | 5255 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 5256 | `	if( pSock == 0 ){` |
|    ! 0 | 5257 | `		PH7_NetClose(sock);` |
|    ! 0 | 5258 | `		return -1;` |
|      - | 5259 | `	}` |
|    ! 0 | 5260 | `	pSock->pVm = pVm;` |
|    ! 0 | 5261 | `	pSock->sock = sock;` |
|    ! 0 | 5262 | `	pSock->bEof = 0;` |
|    ! 0 | 5263 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 5264 | `	return PH7_OK;` |
|    ! 0 | 5265 | `}` |
|      - | 5266 | `static const ph7_io_stream sTCP_Stream = {` |
|      - | 5267 | `	"tcp",` |
|      - | 5268 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5269 | `	SockStreamData_Open, /* xOpen */` |
|      - | 5270 | `	0,   /* xOpenDir */` |
|      - | 5271 | `	SockStreamData_Close,/* xClose */` |
|      - | 5272 | `	0,  /* xCloseDir */` |
|      - | 5273 | `	SockStreamData_Read, /* xRead */` |
|      - | 5274 | `	0,  /* xReadDir */` |
|      - | 5275 | `	SockStreamData_Write,/* xWrite */` |
|      - | 5276 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 5277 | `	0,  /* xLock */` |
|      - | 5278 | `	0,  /* xRewindDir */` |
|      - | 5279 | `	0,  /* xTell */` |
|      - | 5280 | `	0,  /* xTrunc */` |
|      - | 5281 | `	0,  /* xSync */` |
|      - | 5282 | `	0   /* xStat */` |
|      - | 5283 | `};` |
|      - | 5284 | `#endif /* PH7_ENABLE_NET */` |
|      - | 5285 | `/*` |
|      - | 5286 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 5287 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 5288 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 5289 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 5290 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 5291 | ` * php streamWrapper protocol method on it.` |
|      - | 5292 | ` */` |
|      - | 5293 | `#define PHL_UWRAP_MAX 8` |
|      - | 5294 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 5295 | `struct uwrap_slot` |
|      - | 5296 | `{` |
|      - | 5297 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 5298 | `	char zScheme[32];         /* protocol name */` |
|      - | 5299 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 5300 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 5301 | `};` |
|      - | 5302 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 5303 | `struct uwrap_handle` |
|      - | 5304 | `{` |
|      - | 5305 | `	ph7_vm *pVm;` |
|      - | 5306 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 5307 | `	int iSlot;` |
|      - | 5308 | `	int bEof;` |
|      - | 5309 | `};` |
|      - | 5310 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 5311 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 5312 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 5313 | `	ph7_value *pResult)` |
|      1 | 5314 | `{` |
|      - | 5315 | `	ph7_class_method *pMeth;` |
|     27 | 5316 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 5317 | `		return -1;` |
|      - | 5318 | `	}` |
|     27 | 5319 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 5320 | `	if( pMeth == 0 ){` |
|    ! 0 | 5321 | `		return -1;` |
|      - | 5322 | `	}` |
|     27 | 5323 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 5324 | `		return -1;` |
|      - | 5325 | `	}` |
|     27 | 5326 | `	return 0;` |
|     14 | 5327 | `}` |
|      8 | 5328 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 5329 | `{` |
|      9 | 5330 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5331 | `	ph7_value sArg,sRet;` |
|      - | 5332 | `	const char *zData;` |
|      9 | 5333 | `	int nData = 0;` |
|      9 | 5334 | `	ph7_int64 n = 0;` |
|      9 | 5335 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 5336 | `		return 0;` |
|      - | 5337 | `	}` |
|      9 | 5338 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 5339 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 5340 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 5341 | `	{` |
|      - | 5342 | `		ph7_value *apArg[1];` |
|      9 | 5343 | `		apArg[0] = &sArg;` |
|      9 | 5344 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5345 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5346 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5347 | `			return -1;` |
|      - | 5348 | `		}` |
|      - | 5349 | `	}` |
|      9 | 5350 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 5351 | `	if( nData > 0 ){` |
|      7 | 5352 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 5353 | `			nData = (int)nRead;` |
|    ! 0 | 5354 | `		}` |
|      7 | 5355 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 5356 | `		n = nData;` |
|      4 | 5357 | `	}else{` |
|      3 | 5358 | `		pH->bEof = 1;` |
|      - | 5359 | `	}` |
|      9 | 5360 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 5361 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 5362 | `	return n;` |
|      5 | 5363 | `}` |
|      2 | 5364 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5365 | `{` |
|      3 | 5366 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5367 | `	ph7_value sArg,sRet;` |
|      - | 5368 | `	ph7_int64 n;` |
|      3 | 5369 | `	if( pH == 0 ){` |
|    ! 0 | 5370 | `		return -1;` |
|      - | 5371 | `	}` |
|      3 | 5372 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 5373 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5374 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 5375 | `	{` |
|      - | 5376 | `		ph7_value *apArg[1];` |
|      3 | 5377 | `		apArg[0] = &sArg;` |
|      3 | 5378 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5379 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5380 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5381 | `			return -1;` |
|      - | 5382 | `		}` |
|      - | 5383 | `	}` |
|      3 | 5384 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5385 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 5386 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5387 | `	return n;` |
|      2 | 5388 | `}` |
|      2 | 5389 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5390 | `{` |
|      3 | 5391 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5392 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 5393 | `	ph7_value *apArg[2];` |
|      - | 5394 | `	int rc;` |
|      3 | 5395 | `	if( pH == 0 ){` |
|    ! 0 | 5396 | `		return -1;` |
|      - | 5397 | `	}` |
|      3 | 5398 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 5399 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 5400 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5401 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 5402 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 5403 | `	apArg[0] = &sOfft;` |
|      3 | 5404 | `	apArg[1] = &sWhence;` |
|      3 | 5405 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 5406 | `	if( rc == 0 ){` |
|      3 | 5407 | `		pH->bEof = 0;` |
|      3 | 5408 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 5409 | `	}` |
|      3 | 5410 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 5411 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 5412 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5413 | `	return rc;` |
|      2 | 5414 | `}` |
|      2 | 5415 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 5416 | `{` |
|      3 | 5417 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5418 | `	ph7_value sRet;` |
|      - | 5419 | `	ph7_int64 n;` |
|      3 | 5420 | `	if( pH == 0 ){` |
|    ! 0 | 5421 | `		return -1;` |
|      - | 5422 | `	}` |
|      3 | 5423 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5424 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 5425 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5426 | `		return -1;` |
|      - | 5427 | `	}` |
|      3 | 5428 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5429 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5430 | `	return n;` |
|      2 | 5431 | `}` |
|      6 | 5432 | `static void UwrapClose(void *pHandle)` |
|      1 | 5433 | `{` |
|      7 | 5434 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 5435 | `	if( pH == 0 ){` |
|    ! 0 | 5436 | `		return;` |
|      - | 5437 | `	}` |
|      7 | 5438 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 5439 | `	if( pH->pObj ){` |
|      7 | 5440 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 5441 | `	}` |
|      7 | 5442 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 5443 | `}` |
|      - | 5444 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 5445 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 5446 | `{` |
|      7 | 5447 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 5448 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 5449 | `	ph7_class *pClass;` |
|      - | 5450 | `	uwrap_handle *pH;` |
|      - | 5451 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 5452 | `	ph7_value *apArg[4];` |
|      - | 5453 | `	int rc;` |
|      7 | 5454 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 5455 | `		return -1;` |
|      - | 5456 | `	}` |
|      7 | 5457 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 5458 | `	if( pClass == 0 ){` |
|    ! 0 | 5459 | `		return -1;` |
|      - | 5460 | `	}` |
|      7 | 5461 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 5462 | `	if( pH == 0 ){` |
|    ! 0 | 5463 | `		return -1;` |
|      - | 5464 | `	}` |
|      7 | 5465 | `	pH->pVm = pVm;` |
|      7 | 5466 | `	pH->iSlot = iSlot;` |
|      7 | 5467 | `	pH->bEof = 0;` |
|      7 | 5468 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 5469 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 5470 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5471 | `		return -1;` |
|      - | 5472 | `	}` |
|      - | 5473 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 5474 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 5475 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 5476 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 5477 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 5478 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 5479 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 5480 | `	 * it as "could not be passed by reference"). */` |
|      - | 5481 | `	{` |
|      7 | 5482 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 5483 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 5484 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5485 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5486 | `			return -1;` |
|      - | 5487 | `		}` |
|      7 | 5488 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 5489 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 5490 | `	}` |
|      - | 5491 | `	{` |
|      - | 5492 | `		SyBlob sUrl;` |
|      7 | 5493 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 5494 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 5495 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 5496 | `		SyBlobRelease(&sUrl);` |
|      - | 5497 | `	}` |
|      9 | 5498 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 5499 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 5500 | `	ph7_value_int(&sOpts,0);` |
|      7 | 5501 | `	apArg[0] = &sPath;` |
|      7 | 5502 | `	apArg[1] = &sMode;` |
|      7 | 5503 | `	apArg[2] = &sOpts;` |
|      7 | 5504 | `	apArg[3] = &sOpened;` |
|      7 | 5505 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 5506 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 5507 | `		rc = -1;` |
|    ! 0 | 5508 | `	}` |
|      7 | 5509 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 5510 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 5511 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 5512 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 5513 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 5514 | `	if( rc != 0 ){` |
|    ! 0 | 5515 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5516 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5517 | `		return -1;` |
|      - | 5518 | `	}` |
|      7 | 5519 | `	*ppHandle = (void *)pH;` |
|      7 | 5520 | `	return PH7_OK;` |
|      4 | 5521 | `}` |
|      - | 5522 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 5523 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 5524 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 5525 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 5526 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 5527 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 5528 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 5529 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 5530 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 5531 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 5532 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 5533 | `PHL_UWRAP_THUNK(7)` |
|      - | 5534 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 5535 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 5536 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 5537 | `};` |
|      - | 5538 | `/*` |
|      - | 5539 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 5540 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5541 | ` */` |
|      2 | 5542 | `static int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5543 | `{` |
|      - | 5544 | `	const char *zScheme,*zClass;` |
|      3 | 5545 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 5546 | `	if( nArg < 2 ){` |
|    ! 0 | 5547 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5548 | `		return PH7_OK;` |
|      - | 5549 | `	}` |
|      3 | 5550 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5551 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 5552 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 5553 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 5554 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5555 | `		return PH7_OK;` |
|      - | 5556 | `	}` |
|      - | 5557 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 5558 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 5559 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 5560 | `	{` |
|      3 | 5561 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 5562 | `		sxu32 n;` |
|     11 | 5563 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 5564 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 5565 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 5566 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5567 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 5568 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5569 | `				return PH7_OK;` |
|      - | 5570 | `			}` |
|      5 | 5571 | `		}` |
|      - | 5572 | `	}` |
|      3 | 5573 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 5574 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 5575 | `			iFree = i;` |
|      3 | 5576 | `			break;` |
|      - | 5577 | `		}` |
|    ! 0 | 5578 | `	}` |
|      3 | 5579 | `	if( iFree < 0 ){` |
|    ! 0 | 5580 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5581 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 5582 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5583 | `		return PH7_OK;` |
|      - | 5584 | `	}` |
|      - | 5585 | `	{` |
|      3 | 5586 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 5587 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 5588 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 5589 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 5590 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 5591 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 5592 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 5593 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 5594 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 5595 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 5596 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 5597 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 5598 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 5599 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 5600 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 5601 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 5602 | `	}` |
|      3 | 5603 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5604 | `	return PH7_OK;` |
|      2 | 5605 | `}` |
|      2 | 5606 | `static int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5607 | `{` |
|      - | 5608 | `	const char *zScheme;` |
|      - | 5609 | `	int nScheme,i;` |
|      3 | 5610 | `	if( nArg < 1 ){` |
|    ! 0 | 5611 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5612 | `		return PH7_OK;` |
|      - | 5613 | `	}` |
|      3 | 5614 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5615 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 5616 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 5617 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 5618 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 5619 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 5620 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 5621 | `			 * what unregister means to a script — recorded. */` |
|      3 | 5622 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 5623 | `			ph7_result_bool(pCtx,1);` |
|      3 | 5624 | `			return PH7_OK;` |
|      - | 5625 | `		}` |
|    ! 0 | 5626 | `	}` |
|    ! 0 | 5627 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 5628 | `	return PH7_OK;` |
|      2 | 5629 | `}` |
|      - | 5630 | `#ifdef PH7_ENABLE_NET` |
|      - | 5631 | `/*` |
|      - | 5632 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 5633 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 5634 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 5635 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 5636 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 5637 | ` */` |
|      6 | 5638 | `static int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 5639 | `{` |
|      6 | 5640 | `	const char *zFunc = ph7_function_name(pCtx);` |
|      6 | 5641 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|      6 | 5642 | `	const char *zTarget,*zErr = "";` |
|      - | 5643 | `	char zHost[256];` |
|      6 | 5644 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 5645 | `	ph7_socket sock;` |
|      - | 5646 | `	io_private *pDev;` |
|      - | 5647 | `	sock_private *pSock;` |
|      6 | 5648 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|      6 | 5649 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|      6 | 5650 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|      6 | 5651 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 5652 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5653 | `		return PH7_OK;` |
|      - | 5654 | `	}` |
|      6 | 5655 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 5656 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 5657 | `	{` |
|      6 | 5658 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|      6 | 5659 | `		const char *zSep = 0;` |
|     32 | 5660 | `		while( z < zEnd - 2 ){` |
|     30 | 5661 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 5662 | `				zSep = z;` |
|      4 | 5663 | `				break;` |
|      - | 5664 | `			}` |
|     26 | 5665 | `			z++;` |
|    ! 0 | 5666 | `		}` |
|      5 | 5667 | `		if( zSep ){` |
|      4 | 5668 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 5669 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5670 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 5671 | `					nTarget,zTarget);` |
|    ! 0 | 5672 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5673 | `				return PH7_OK;` |
|      - | 5674 | `			}` |
|      4 | 5675 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 5676 | `			zTarget = zSep + 3;` |
|      2 | 5677 | `		}` |
|      - | 5678 | `	}` |
|      - | 5679 | `	/* host[:port] */` |
|      - | 5680 | `	{` |
|      6 | 5681 | `		int i = nTarget - 1;` |
|      6 | 5682 | `		int nHost = nTarget;` |
|     48 | 5683 | `		while( i > 0 && zTarget[i] != ':' ){` |
|     42 | 5684 | `			i--;` |
|    ! 0 | 5685 | `		}` |
|      6 | 5686 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 5687 | `			sxi32 iTmp = 0;` |
|      2 | 5688 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 5689 | `			iPort = (int)iTmp;` |
|      2 | 5690 | `			nHost = i;` |
|      1 | 5691 | `		}` |
|      5 | 5692 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 5693 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 5694 | `		}` |
|      5 | 5695 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|      5 | 5696 | `		zHost[nHost] = 0;` |
|      - | 5697 | `	}` |
|      5 | 5698 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      4 | 5699 | `		iPort = ph7_value_to_int(apArg[1]);` |
|      2 | 5700 | `	}` |
|      6 | 5701 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|      6 | 5702 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|      6 | 5703 | `		if( rTimeout > 0 ){` |
|      6 | 5704 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|      3 | 5705 | `		}` |
|      3 | 5706 | `	}` |
|      6 | 5707 | `	if( iPort < 0 ){` |
|    ! 0 | 5708 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5709 | `		return PH7_OK;` |
|      - | 5710 | `	}` |
|      6 | 5711 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|      6 | 5712 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5713 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 5714 | `		{` |
|      2 | 5715 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      2 | 5716 | `			if( pTmp ){` |
|      2 | 5717 | `				if( nArg > iArgErrno ){` |
|      2 | 5718 | `					ph7_value_int(pTmp,iErrno);` |
|      2 | 5719 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      1 | 5720 | `				}` |
|      2 | 5721 | `				if( nArg > iArgErrstr ){` |
|      2 | 5722 | `					ph7_value_string(pTmp,zErr,-1);` |
|      2 | 5723 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      1 | 5724 | `				}` |
|      1 | 5725 | `			}` |
|      - | 5726 | `		}` |
|      - | 5727 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|      3 | 5728 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 5729 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|      2 | 5730 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5731 | `		return PH7_OK;` |
|      - | 5732 | `	}` |
|      - | 5733 | `	{` |
|      4 | 5734 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 5735 | `		if( pTmp ){` |
|      4 | 5736 | `			if( nArg > iArgErrno ){` |
|      4 | 5737 | `				ph7_value_int(pTmp,0);` |
|      4 | 5738 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 5739 | `			}` |
|      4 | 5740 | `			if( nArg > iArgErrstr ){` |
|      4 | 5741 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 5742 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 5743 | `			}` |
|      2 | 5744 | `		}` |
|      - | 5745 | `	}` |
|      - | 5746 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 5747 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 5748 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 5749 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 5750 | `		PH7_NetClose(sock);` |
|    ! 0 | 5751 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5752 | `		return PH7_OK;` |
|      - | 5753 | `	}` |
|      4 | 5754 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 5755 | `	pSock->sock = sock;` |
|      4 | 5756 | `	pSock->bEof = 0;` |
|      4 | 5757 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 5758 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 5759 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 5760 | `	return PH7_OK;` |
|      3 | 5761 | `}` |
|      - | 5762 | `#endif /* PH7_ENABLE_NET */` |
|    218 | 5763 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5764 | `{` |
|      - | 5765 | `	const ph7_io_stream *pStream;` |
|      - | 5766 | `	const char *zUri,*zMode;` |
|      - | 5767 | `	ph7_value *pResource;` |
|      - | 5768 | `	io_private *pDev;` |
|      - | 5769 | `	int iLen,imLen;` |
|      - | 5770 | `	int iOpenFlags;` |
|    221 | 5771 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5772 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5773 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 5774 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5775 | `		return PH7_OK;` |
|      - | 5776 | `	}` |
|      - | 5777 | `	/* Extract the URI and the desired access mode */` |
|    221 | 5778 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    221 | 5779 | `	if( nArg > 1 ){` |
|    221 | 5780 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    112 | 5781 | `	}else{` |
|      - | 5782 | `		/* Set a default read-only mode */` |
|    ! 0 | 5783 | `		zMode = "r";` |
|    ! 0 | 5784 | `		imLen = (int)sizeof(char);` |
|      - | 5785 | `	}` |
|      - | 5786 | `	/* Try to extract a stream */` |
|    221 | 5787 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    221 | 5788 | `	if( pStream == 0 ){` |
|    ! 0 | 5789 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5790 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 5791 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5792 | `		return PH7_OK;` |
|      - | 5793 | `	}` |
|      - | 5794 | `	/* Allocate a new IO private instance */` |
|    221 | 5795 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    221 | 5796 | `	if( pDev == 0 ){` |
|    ! 0 | 5797 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5798 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5799 | `		return PH7_OK;` |
|      - | 5800 | `	}` |
|    221 | 5801 | `	pResource = 0;` |
|    221 | 5802 | `	if( nArg > 3 ){` |
|    ! 0 | 5803 | `		pResource = apArg[3];` |
|    221 | 5804 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 5805 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 5806 | `		 * access the underlying virtual machine.` |
|      - | 5807 | `		 */` |
|     19 | 5808 | `		pResource = apArg[0];` |
|      9 | 5809 | `	}` |
|      - | 5810 | `	/* Initialize the structure */` |
|    221 | 5811 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 5812 | `	/* Convert open mode to PH7 flags */` |
|    221 | 5813 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 5814 | `	/* Try to get a handle */` |
|    330 | 5815 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    109 | 5816 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|    221 | 5817 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 5818 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|    ! 0 | 5819 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5820 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 5821 | `		return PH7_OK;` |
|      - | 5822 | `	}` |
|      - | 5823 | `	/* All done,return the io_private instance as a resource */` |
|    221 | 5824 | `	ph7_result_resource(pCtx,pDev);` |
|    221 | 5825 | `	return PH7_OK;` |
|    112 | 5826 | `}` |
|      - | 5827 | `/*` |
|      - | 5828 | ` * bool fclose(resource $handle)` |
|      - | 5829 | ` *  Closes an open file pointer` |
|      - | 5830 | ` * Parameters` |
|      - | 5831 | ` *  $handle` |
|      - | 5832 | ` *   The file pointer.` |
|      - | 5833 | ` * Return` |
|      - | 5834 | ` *  TRUE on success or FALSE on failure.` |
|      - | 5835 | ` */` |
|    348 | 5836 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5837 | `{` |
|      - | 5838 | `	const ph7_io_stream *pStream;` |
|      - | 5839 | `	io_private *pDev;` |
|      - | 5840 | `	ph7_vm *pVm;` |
|    353 | 5841 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5842 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5843 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5844 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5845 | `		return PH7_OK;` |
|      - | 5846 | `	}` |
|      - | 5847 | `	/* Extract our private data */` |
|    353 | 5848 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5849 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|    353 | 5850 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 5851 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5852 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 5853 | `	}` |
|      - | 5854 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    351 | 5855 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 5856 | `		/*Expecting an IO handle */` |
|    ! 0 | 5857 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5858 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5859 | `		return PH7_OK;` |
|      - | 5860 | `	}` |
|      - | 5861 | `	/* Point to the target IO stream device */` |
|    351 | 5862 | `	pStream = pDev->pStream;` |
|    351 | 5863 | `	if( pStream == 0 ){` |
|    ! 0 | 5864 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5865 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 5866 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 5867 | `			);` |
|    ! 0 | 5868 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5869 | `		return PH7_OK;` |
|      - | 5870 | `	}` |
|      - | 5871 | `	/* Point to the VM that own this context */` |
|    351 | 5872 | `	pVm = pCtx->pVm;` |
|      - | 5873 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    351 | 5874 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 5875 | `		/* Perform the requested operation */` |
|    351 | 5876 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 5877 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|    351 | 5878 | `		MarkIOPrivateClosed(pDev);` |
|    173 | 5879 | `	}` |
|      - | 5880 | `	/* Return TRUE */` |
|    351 | 5881 | `	ph7_result_bool(pCtx,1);` |
|    351 | 5882 | `	return PH7_OK;` |
|    179 | 5883 | `}` |
|      - | 5884 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5885 | `/*` |
|      - | 5886 | ` * MD5/SHA1 digest consumer.` |
|      - | 5887 | ` */` |
|     72 | 5888 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 5889 | `{` |
|      - | 5890 | `	/* Append hex chunk verbatim */` |
|     73 | 5891 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 5892 | `	return SXRET_OK;` |
|      1 | 5893 | `}` |
|      - | 5894 | `/*` |
|      - | 5895 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 5896 | ` *  Calculates the md5 hash of a given file.` |
|      - | 5897 | ` * Parameters` |
|      - | 5898 | ` *  $uri` |
|      - | 5899 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5900 | ` *  $raw_output` |
|      - | 5901 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 5902 | ` * Return` |
|      - | 5903 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 5904 | ` */` |
|      2 | 5905 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5906 | `{` |
|      - | 5907 | `	const ph7_io_stream *pStream;` |
|      - | 5908 | `	unsigned char zDigest[16];` |
|      3 | 5909 | `	int raw_output  = FALSE;` |
|      - | 5910 | `	const char *zFile;` |
|      - | 5911 | `	MD5Context sCtx;` |
|      - | 5912 | `	char zBuf[8192];` |
|      - | 5913 | `	void *pHandle;` |
|      - | 5914 | `	ph7_int64 n;` |
|      - | 5915 | `	int nLen;` |
|      3 | 5916 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5917 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5918 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5919 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5920 | `		return PH7_OK;` |
|      - | 5921 | `	}` |
|      - | 5922 | `	/* Extract the file path */` |
|      3 | 5923 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5924 | `	/* Point to the target IO stream device */` |
|      3 | 5925 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5926 | `	if( pStream == 0 ){` |
|    ! 0 | 5927 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5928 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5929 | `		return PH7_OK;` |
|      - | 5930 | `	}` |
|      3 | 5931 | `	if( nArg > 1 ){` |
|    ! 0 | 5932 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5933 | `	}` |
|      - | 5934 | `	/* Try to open the file in read-only mode */` |
|      3 | 5935 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5936 | `	if( pHandle == 0 ){` |
|    ! 0 | 5937 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 5938 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5939 | `		return PH7_OK;` |
|      - | 5940 | `	}` |
|      - | 5941 | `	/* Init the MD5 context */` |
|      3 | 5942 | `	MD5Init(&sCtx);` |
|      - | 5943 | `	/* Perform the requested operation */` |
|      2 | 5944 | `	for(;;){` |
|      5 | 5945 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5946 | `		if( n < 1 ){` |
|      - | 5947 | `			/* EOF or IO error,break immediately */` |
|      3 | 5948 | `			break;` |
|      - | 5949 | `		}` |
|      3 | 5950 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5951 | `	}` |
|      - | 5952 | `	/* Close the stream */` |
|      3 | 5953 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5954 | `	/* Extract the digest */` |
|      3 | 5955 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 5956 | `	if( raw_output ){` |
|      - | 5957 | `		/* Output raw digest */` |
|    ! 0 | 5958 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5959 | `	}else{` |
|      - | 5960 | `		/* Perform a binary to hex conversion */` |
|      3 | 5961 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5962 | `	}` |
|      3 | 5963 | `	return PH7_OK;` |
|      2 | 5964 | `}` |
|      - | 5965 | `/*` |
|      - | 5966 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 5967 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 5968 | ` * Parameters` |
|      - | 5969 | ` *  $uri` |
|      - | 5970 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5971 | ` *  $raw_output` |
|      - | 5972 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 5973 | ` * Return` |
|      - | 5974 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 5975 | ` */` |
|      2 | 5976 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5977 | `{` |
|      - | 5978 | `	const ph7_io_stream *pStream;` |
|      - | 5979 | `	unsigned char zDigest[20];` |
|      3 | 5980 | `	int raw_output  = FALSE;` |
|      - | 5981 | `	const char *zFile;` |
|      - | 5982 | `	SHA1Context sCtx;` |
|      - | 5983 | `	char zBuf[8192];` |
|      - | 5984 | `	void *pHandle;` |
|      - | 5985 | `	ph7_int64 n;` |
|      - | 5986 | `	int nLen;` |
|      3 | 5987 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5988 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5989 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5990 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5991 | `		return PH7_OK;` |
|      - | 5992 | `	}` |
|      - | 5993 | `	/* Extract the file path */` |
|      3 | 5994 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5995 | `	/* Point to the target IO stream device */` |
|      3 | 5996 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5997 | `	if( pStream == 0 ){` |
|    ! 0 | 5998 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5999 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6000 | `		return PH7_OK;` |
|      - | 6001 | `	}` |
|      3 | 6002 | `	if( nArg > 1 ){` |
|    ! 0 | 6003 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 6004 | `	}` |
|      - | 6005 | `	/* Try to open the file in read-only mode */` |
|      3 | 6006 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 6007 | `	if( pHandle == 0 ){` |
|    ! 0 | 6008 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 6009 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6010 | `		return PH7_OK;` |
|      - | 6011 | `	}` |
|      - | 6012 | `	/* Init the SHA1 context */` |
|      3 | 6013 | `	SHA1Init(&sCtx);` |
|      - | 6014 | `	/* Perform the requested operation */` |
|      2 | 6015 | `	for(;;){` |
|      5 | 6016 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 6017 | `		if( n < 1 ){` |
|      - | 6018 | `			/* EOF or IO error,break immediately */` |
|      3 | 6019 | `			break;` |
|      - | 6020 | `		}` |
|      3 | 6021 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 6022 | `	}` |
|      - | 6023 | `	/* Close the stream */` |
|      3 | 6024 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 6025 | `	/* Extract the digest */` |
|      3 | 6026 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 6027 | `	if( raw_output ){` |
|      - | 6028 | `		/* Output raw digest */` |
|    ! 0 | 6029 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 6030 | `	}else{` |
|      - | 6031 | `		/* Perform a binary to hex conversion */` |
|      3 | 6032 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 6033 | `	}` |
|      3 | 6034 | `	return PH7_OK;` |
|      2 | 6035 | `}` |
|      - | 6036 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6037 | `/*` |
|      - | 6038 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 6039 | ` *  Parse a configuration file.` |
|      - | 6040 | ` * Parameters` |
|      - | 6041 | ` * $filename` |
|      - | 6042 | ` *  The filename of the ini file being parsed.` |
|      - | 6043 | ` * $process_sections` |
|      - | 6044 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 6045 | ` *  with the section names and settings included.` |
|      - | 6046 | ` *  The default for process_sections is FALSE.` |
|      - | 6047 | ` * $scanner_mode` |
|      - | 6048 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 6049 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 6050 | ` * Return` |
|      - | 6051 | ` *  The settings are returned as an associative array on success.` |
|      - | 6052 | ` *  Otherwise is returned.` |
|      - | 6053 | ` */` |
|      2 | 6054 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6055 | `{` |
|      - | 6056 | `	const ph7_io_stream *pStream;` |
|      - | 6057 | `	const char *zFile;` |
|      - | 6058 | `	SyBlob sContents;` |
|      - | 6059 | `	void *pHandle;` |
|      - | 6060 | `	int nLen;` |
|      3 | 6061 | `	sxi32 rc = PH7_OK;` |
|      3 | 6062 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6063 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 6064 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 6065 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6066 | `		return PH7_OK;` |
|      - | 6067 | `	}` |
|      - | 6068 | `	/* Extract the file path */` |
|      3 | 6069 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 6070 | `	/* Point to the target IO stream device */` |
|      3 | 6071 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 6072 | `	if( pStream == 0 ){` |
|    ! 0 | 6073 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 6074 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6075 | `		return PH7_OK;` |
|      - | 6076 | `	}` |
|      - | 6077 | `	/* Try to open the file in read-only mode */` |
|      3 | 6078 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 6079 | `	if( pHandle == 0 ){` |
|    ! 0 | 6080 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 6081 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6082 | `		return PH7_OK;` |
|      - | 6083 | `	}` |
|      3 | 6084 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 6085 | `	/* Read the whole file */` |
|      3 | 6086 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 6087 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 6088 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 6089 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6090 | `	}else{` |
|      - | 6091 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 6092 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 6093 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 6094 | `	}` |
|      - | 6095 | `	/* Close the stream */` |
|      3 | 6096 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 6097 | `	/* Release the working buffer */` |
|      3 | 6098 | `	SyBlobRelease(&sContents);` |
|      - | 6099 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 6100 | `	return rc;` |
|      2 | 6101 | `}` |
|      - | 6102 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 6103 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 6104 | `/*` |
|      - | 6105 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 6106 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 6107 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 6108 | ` * to any non-VFS resource).` |
|      - | 6109 | ` */` |
|      - | 6110 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 6111 | `{` |
|      - | 6112 | `	SXUNUSED(pResource);` |
|      - | 6113 | `	return "Unknown";` |
|      - | 6114 | `}` |
|      - | 6115 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 6116 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 6117 | `{` |
|      - | 6118 | `	SXUNUSED(pResource);` |
|      - | 6119 | `	return 0;` |
|      - | 6120 | `}` |
|      - | 6121 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 6122 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 6123 | `#if defined(_MSC_VER)` |
|      - | 6124 | `static const ph7_vfs null_vfs = {` |
|      - | 6125 | `#else` |
|      - | 6126 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 6127 | `#endif` |
|      - | 6128 | `	"null_vfs",` |
|      - | 6129 | `	PH7_VFS_VERSION,` |
|      - | 6130 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 6131 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 6132 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 6133 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 6134 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 6135 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 6136 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 6137 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 6138 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 6139 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 6140 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 6141 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 6142 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 6143 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 6144 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 6145 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 6146 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 6147 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 6148 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 6149 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 6150 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6151 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6152 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 6153 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 6154 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 6155 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 6156 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 6157 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 6158 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 6159 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 6160 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 6161 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 6162 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 6163 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 6164 | `	0, /* int (*xUmask)(int) */` |
|      - | 6165 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 6166 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 6167 | `	0, /* int (*xUid)(void) */` |
|      - | 6168 | `	0, /* int (*xGid)(void) */` |
|      - | 6169 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 6170 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 6171 | `};` |
|      - | 6172 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 6173 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 6174 | `/*` |
|      - | 6175 | ` * Export the builtin vfs.` |
|      - | 6176 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 6177 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 6178 | ` * Note:` |
|      - | 6179 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 6180 | ` * Note:` |
|      - | 6181 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6182 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 6183 | ` */` |
|   3902 | 6184 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 6185 | `{` |
|      - | 6186 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6187 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 6188 | `	return &null_vfs;` |
|      - | 6189 | `#else` |
|      - | 6190 | `#ifdef __WINNT__` |
|      5 | 6191 | `	return &sWinVfs;` |
|      - | 6192 | `#elif defined(__UNIXES__)` |
|   3902 | 6193 | `	return &sUnixVfs;` |
|      - | 6194 | `#else` |
|      - | 6195 | `	return &null_vfs;` |
|      - | 6196 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 6197 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 6198 | `#else` |
|      - | 6199 | `	return &null_vfs;` |
|      - | 6200 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 6201 | `}` |
|      - | 6202 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6203 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6204 | `/*` |
|      - | 6205 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 6206 | ` * no particular meaning on windows.` |
|      - | 6207 | ` */` |
|      - | 6208 | `#ifndef STDIN_FILENO` |
|      - | 6209 | `#define STDIN_FILENO	0` |
|      - | 6210 | `#endif` |
|      - | 6211 | `#ifndef STDOUT_FILENO` |
|      - | 6212 | `#define STDOUT_FILENO	1` |
|      - | 6213 | `#endif` |
|      - | 6214 | `#ifndef STDERR_FILENO` |
|      - | 6215 | `#define STDERR_FILENO	2` |
|      - | 6216 | `#endif` |
|      - | 6217 | `/*` |
|      - | 6218 | ` * php:// Accessing various I/O streams` |
|      - | 6219 | ` * According to the PHP langage reference manual` |
|      - | 6220 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 6221 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 6222 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 6223 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 6224 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 6225 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 6226 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 6227 | ` * php://output` |
|      - | 6228 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 6229 | ` *  mechanism in the same way as print and echo.` |
|      - | 6230 | ` */` |
|      - | 6231 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 6232 | `/* Supported IO streams */` |
|      - | 6233 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 6234 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 6235 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 6236 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 6237 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|      - | 6238 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 6239 | `struct ph7_stream_data` |
|      - | 6240 | `{` |
|      - | 6241 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 6242 | `	int iType;   /* Stream type */` |
|      - | 6243 | `	union{` |
|      - | 6244 | `		void *pHandle; /* Stream handle */` |
|      - | 6245 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 6246 | `	}x;` |
|      - | 6247 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|      - | 6248 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|      - | 6249 | `	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */` |
|      - | 6250 | `};` |
|      - | 6251 | `/*` |
|      - | 6252 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 6253 | ` */` |
|     40 | 6254 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 6255 | `{` |
|      - | 6256 | `	ph7_stream_data *pData;` |
|     41 | 6257 | `	if( pVm == 0 ){` |
|    ! 0 | 6258 | `		return 0;` |
|      - | 6259 | `	}` |
|      - | 6260 | `	/* Allocate a new instance */` |
|     41 | 6261 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|     41 | 6262 | `	if( pData == 0 ){` |
|    ! 0 | 6263 | `		return 0;` |
|      - | 6264 | `	}` |
|      - | 6265 | `	/* Zero the structure */` |
|     41 | 6266 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 6267 | `	/* Initialize fields */` |
|     41 | 6268 | `	pData->iType = iType;` |
|     41 | 6269 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|     41 | 6270 | `	pData->nCur = 0;` |
|     41 | 6271 | `	pData->bReadOnly = 0;` |
|     41 | 6272 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6273 | `		/* Nothing else to set up: the buffer is the stream */` |
|     30 | 6274 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 6275 | `		/* Point to the default VM consumer routine. */` |
|      3 | 6276 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 6277 | `	}else{` |
|      - | 6278 | `#ifdef __WINNT__` |
|      - | 6279 | `		DWORD nChannel;` |
|      1 | 6280 | `		switch(iType){` |
|      1 | 6281 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 6282 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 6283 | `		default:` |
|      1 | 6284 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 6285 | `			break;` |
|      - | 6286 | `		}` |
|      1 | 6287 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 6288 | `#else` |
|      - | 6289 | `		/* Assume an UNIX system */` |
|     16 | 6290 | `		int ifd = STDIN_FILENO;` |
|     16 | 6291 | `		switch(iType){` |
|      6 | 6292 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      8 | 6293 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 6294 | `		default:` |
|      2 | 6295 | `			break;` |
|      - | 6296 | `		}` |
|     16 | 6297 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 6298 | `#endif` |
|      - | 6299 | `	}` |
|     41 | 6300 | `	pData->pVm = pVm;` |
|     41 | 6301 | `	return pData;` |
|     21 | 6302 | `}` |
|      - | 6303 | `/*` |
|      - | 6304 | ` * Implementation of the php:// IO streams routines` |
|      - | 6305 | ` * Status:` |
|      - | 6306 | ` *   Stable.` |
|      - | 6307 | ` */` |
|      - | 6308 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|     14 | 6309 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6310 | `{` |
|      - | 6311 | `	ph7_stream_data *pData;` |
|      - | 6312 | `	SyString sStream;` |
|     15 | 6313 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 6314 | `	/* Trim leading and trailing white spaces */` |
|     15 | 6315 | `	SyStringFullTrim(&sStream);` |
|      - | 6316 | `	/* Stream to open */` |
|     15 | 6317 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 6318 | `		iMode = PH7_IO_STREAM_STDIN;` |
|     15 | 6319 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 6320 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|     14 | 6321 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 6322 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|     13 | 6323 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 6324 | `		iMode = PH7_IO_STREAM_STDERR;` |
|     12 | 6325 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|      8 | 6326 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|      - | 6327 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|      - | 6328 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|     13 | 6329 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|      7 | 6330 | `	}else{` |
|      - | 6331 | `		/* unknown stream name */` |
|    ! 0 | 6332 | `		return -1;` |
|      - | 6333 | `	}` |
|      - | 6334 | `	/* Create our handle */` |
|     15 | 6335 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|     15 | 6336 | `	if( pData == 0 ){` |
|    ! 0 | 6337 | `		return -1;` |
|      - | 6338 | `	}` |
|      - | 6339 | `	/* Make the handle public */` |
|     15 | 6340 | `	*ppHandle = (void *)pData;` |
|     15 | 6341 | `	return PH7_OK;` |
|      8 | 6342 | `}` |
|      - | 6343 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|     42 | 6344 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|      1 | 6345 | `{` |
|     43 | 6346 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     43 | 6347 | `	if( pData == 0 ){` |
|    ! 0 | 6348 | `		return -1;` |
|      - | 6349 | `	}` |
|     43 | 6350 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     43 | 6351 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|      - | 6352 | `		sxu32 nRead;` |
|     43 | 6353 | `		if( pData->nCur >= nAvail ){` |
|     15 | 6354 | `			return 0; /* EOF */` |
|      - | 6355 | `		}` |
|     29 | 6356 | `		nRead = nAvail - pData->nCur;` |
|     29 | 6357 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|      7 | 6358 | `			nRead = (sxu32)nDatatoRead;` |
|      3 | 6359 | `		}` |
|     29 | 6360 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|     29 | 6361 | `		pData->nCur += nRead;` |
|     29 | 6362 | `		return (ph7_int64)nRead;` |
|      - | 6363 | `	}` |
|    ! 0 | 6364 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 6365 | `		/* Forbidden */` |
|    ! 0 | 6366 | `		return -1;` |
|      - | 6367 | `	}` |
|      - | 6368 | `#ifdef __WINNT__` |
|      - | 6369 | `	{` |
|      - | 6370 | `		DWORD nRd;` |
|      - | 6371 | `		BOOL rc;` |
|    ! 0 | 6372 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 6373 | `		if( !rc ){` |
|      - | 6374 | `			/* IO error */` |
|    ! 0 | 6375 | `			return -1;` |
|      - | 6376 | `		}` |
|    ! 0 | 6377 | `		return (ph7_int64)nRd;` |
|      - | 6378 | `	}` |
|      - | 6379 | `#elif defined(__UNIXES__)` |
|      - | 6380 | `	{` |
|      - | 6381 | `		ssize_t nRd;` |
|      - | 6382 | `		int fd;` |
|    ! 0 | 6383 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6384 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 6385 | `		if( nRd < 1 ){` |
|    ! 0 | 6386 | `			return -1;` |
|      - | 6387 | `		}` |
|    ! 0 | 6388 | `		return (ph7_int64)nRd;` |
|      - | 6389 | `	}` |
|      - | 6390 | `#else` |
|      - | 6391 | `	return -1;` |
|      - | 6392 | `#endif` |
|     22 | 6393 | `}` |
|      - | 6394 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|     22 | 6395 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 6396 | `{` |
|     23 | 6397 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     23 | 6398 | `	if( pData == 0 ){` |
|    ! 0 | 6399 | `		return -1;` |
|      - | 6400 | `	}` |
|     23 | 6401 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 6402 | `		/* Forbidden */` |
|    ! 0 | 6403 | `		return -1;` |
|     23 | 6404 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6405 | `		sxu32 nLen,nEnd;` |
|     11 | 6406 | `		if( pData->bReadOnly ){` |
|    ! 0 | 6407 | `			return -1;` |
|      - | 6408 | `		}` |
|     11 | 6409 | `		nLen = SyBlobLength(&pData->sMem);` |
|     11 | 6410 | `		if( pData->nCur > nLen ){` |
|      - | 6411 | `			/* seek past end: php zero-fills the gap */` |
|      - | 6412 | `			static const char zZero[64] = {0};` |
|    ! 0 | 6413 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|    ! 0 | 6414 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|    ! 0 | 6415 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6416 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6417 | `					return -1;` |
|      - | 6418 | `				}` |
|    ! 0 | 6419 | `			}` |
|    ! 0 | 6420 | `			nLen = SyBlobLength(&pData->sMem);` |
|    ! 0 | 6421 | `		}` |
|     11 | 6422 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|     11 | 6423 | `		if( pData->nCur < nLen ){` |
|      - | 6424 | `			/* overwrite in place up to the current end */` |
|      3 | 6425 | `			sxu32 nOver = nLen - pData->nCur;` |
|      3 | 6426 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|      4 | 6427 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|      4 | 6428 | `			if( nEnd > nLen ){` |
|    ! 0 | 6429 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|    ! 0 | 6430 | `					return -1;` |
|      - | 6431 | `				}` |
|    ! 0 | 6432 | `			}` |
|      2 | 6433 | `		}else{` |
|      9 | 6434 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|    ! 0 | 6435 | `				return -1;` |
|      - | 6436 | `			}` |
|      - | 6437 | `		}` |
|     11 | 6438 | `		pData->nCur = nEnd;` |
|     11 | 6439 | `		return nWrite;` |
|     13 | 6440 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 6441 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 6442 | `		int rc;` |
|      - | 6443 | `		/* Call the vm output consumer */` |
|      3 | 6444 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 6445 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 6446 | `			return -1;` |
|      - | 6447 | `		}` |
|      3 | 6448 | `		return nWrite;` |
|      - | 6449 | `	}` |
|      - | 6450 | `#ifdef __WINNT__` |
|      - | 6451 | `	{` |
|      - | 6452 | `		DWORD nWr;` |
|      - | 6453 | `		BOOL rc;` |
|    ! 0 | 6454 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 6455 | `		if( !rc ){` |
|      - | 6456 | `			/* IO error */` |
|    ! 0 | 6457 | `			return -1;` |
|      - | 6458 | `		}` |
|    ! 0 | 6459 | `		return (ph7_int64)nWr;` |
|      - | 6460 | `	}` |
|      - | 6461 | `#elif defined(__UNIXES__)` |
|      - | 6462 | `	{` |
|      - | 6463 | `		ssize_t nWr;` |
|      - | 6464 | `		int fd;` |
|     10 | 6465 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|     10 | 6466 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|     10 | 6467 | `		if( nWr < 1 ){` |
|    ! 0 | 6468 | `			return -1;` |
|      - | 6469 | `		}` |
|     10 | 6470 | `		return (ph7_int64)nWr;` |
|      - | 6471 | `	}` |
|      - | 6472 | `#else` |
|      - | 6473 | `	return -1;` |
|      - | 6474 | `#endif` |
|     12 | 6475 | `}` |
|      - | 6476 | `/* void (*xClose)(void *) */` |
|     20 | 6477 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 6478 | `{` |
|     21 | 6479 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6480 | `	ph7_vm *pVm;` |
|     21 | 6481 | `	if( pData == 0 ){` |
|    ! 0 | 6482 | `		return;` |
|      - | 6483 | `	}` |
|     21 | 6484 | `	pVm = pData->pVm;` |
|     21 | 6485 | `	SyBlobRelease(&pData->sMem);` |
|      - | 6486 | `	/* Free the instance */` |
|     21 | 6487 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|     11 | 6488 | `}` |
|      - | 6489 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|     20 | 6490 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 6491 | `{` |
|     21 | 6492 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6493 | `	ph7_int64 iNew;` |
|     21 | 6494 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6495 | `		return -1;` |
|      - | 6496 | `	}` |
|     21 | 6497 | `	switch(whence){` |
|    ! 0 | 6498 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|      3 | 6499 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|     19 | 6500 | `	default:            iNew = iOfft; break;` |
|      - | 6501 | `	}` |
|     21 | 6502 | `	if( iNew < 0 ){` |
|    ! 0 | 6503 | `		return -1;` |
|      - | 6504 | `	}` |
|     21 | 6505 | `	pData->nCur = (sxu32)iNew;` |
|     21 | 6506 | `	return PH7_OK;` |
|     11 | 6507 | `}` |
|      - | 6508 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|      4 | 6509 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|      1 | 6510 | `{` |
|      5 | 6511 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      5 | 6512 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6513 | `		return -1;` |
|      - | 6514 | `	}` |
|      5 | 6515 | `	return (ph7_int64)pData->nCur;` |
|      3 | 6516 | `}` |
|      - | 6517 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|    ! 0 | 6518 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|    ! 0 | 6519 | `{` |
|    ! 0 | 6520 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 6521 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|    ! 0 | 6522 | `		return -1;` |
|      - | 6523 | `	}` |
|    ! 0 | 6524 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|      - | 6525 | `		/* shrink in place: the blob keeps its allocation */` |
|    ! 0 | 6526 | `		pData->sMem.nByte = (sxu32)nLen;` |
|    ! 0 | 6527 | `	}else{` |
|      - | 6528 | `		static const char zZero[64] = {0};` |
|    ! 0 | 6529 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|    ! 0 | 6530 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|    ! 0 | 6531 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6532 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6533 | `				return -1;` |
|      - | 6534 | `			}` |
|    ! 0 | 6535 | `		}` |
|      - | 6536 | `	}` |
|    ! 0 | 6537 | `	return PH7_OK;` |
|    ! 0 | 6538 | `}` |
|      - | 6539 | `/*` |
|      - | 6540 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|      - | 6541 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|      - | 6542 | ` * base64). Shares the MEMORY machinery above.` |
|      - | 6543 | ` */` |
|      8 | 6544 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 6545 | `{` |
|      9 | 6546 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|      1 | 6547 | `}` |
|     10 | 6548 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6549 | `{` |
|      - | 6550 | `	ph7_stream_data *pData;` |
|     11 | 6551 | `	const char *zIn = zName;` |
|     11 | 6552 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|     11 | 6553 | `	const char *zComma = 0;` |
|     11 | 6554 | `	int bBase64 = 0;` |
|      5 | 6555 | `	SXUNUSED(iMode);` |
|      - | 6556 | `	/* Find the comma separating the mediatype from the payload */` |
|    105 | 6557 | `	while( zIn < zEnd ){` |
|    105 | 6558 | `		if( zIn[0] == ',' ){` |
|     11 | 6559 | `			zComma = zIn;` |
|     11 | 6560 | `			break;` |
|      - | 6561 | `		}` |
|     95 | 6562 | `		zIn++;` |
|      1 | 6563 | `	}` |
|     11 | 6564 | `	if( zComma == 0 ){` |
|    ! 0 | 6565 | `		return -1;` |
|      - | 6566 | `	}` |
|     10 | 6567 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|     10 | 6568 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|      3 | 6569 | `		bBase64 = 1;` |
|      1 | 6570 | `	}` |
|     11 | 6571 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|     11 | 6572 | `	if( pData == 0 ){` |
|    ! 0 | 6573 | `		return -1;` |
|      - | 6574 | `	}` |
|     11 | 6575 | `	pData->bReadOnly = 1;` |
|     11 | 6576 | `	zIn = &zComma[1];` |
|     11 | 6577 | `	if( bBase64 ){` |
|      3 | 6578 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|    ! 0 | 6579 | `			SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6580 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6581 | `			return -1;` |
|      - | 6582 | `		}` |
|      2 | 6583 | `	}else{` |
|      - | 6584 | `		/* percent-decode the payload */` |
|     71 | 6585 | `		while( zIn < zEnd ){` |
|     63 | 6586 | `			char c = zIn[0];` |
|     63 | 6587 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|      3 | 6588 | `				int hi = SyHexToint(zIn[1]);` |
|      3 | 6589 | `				int lo = SyHexToint(zIn[2]);` |
|      3 | 6590 | `				c = (char)((hi << 4) \| lo);` |
|      3 | 6591 | `				zIn += 3;` |
|      2 | 6592 | `			}else{` |
|     61 | 6593 | `				zIn++;` |
|      - | 6594 | `			}` |
|     63 | 6595 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|    ! 0 | 6596 | `				SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6597 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6598 | `				return -1;` |
|      - | 6599 | `			}` |
|      1 | 6600 | `		}` |
|      - | 6601 | `	}` |
|     11 | 6602 | `	*ppHandle = (void *)pData;` |
|     11 | 6603 | `	return PH7_OK;` |
|      6 | 6604 | `}` |
|      - | 6605 | `/* data:// rejects writes outright */` |
|    ! 0 | 6606 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 6607 | `{` |
|    ! 0 | 6608 | `	SXUNUSED(pHandle);` |
|    ! 0 | 6609 | `	SXUNUSED(pBuf);` |
|    ! 0 | 6610 | `	SXUNUSED(nWrite);` |
|    ! 0 | 6611 | `	return -1;` |
|    ! 0 | 6612 | `}` |
|      - | 6613 | `static const ph7_io_stream sDATA_Stream = {` |
|      - | 6614 | `	"data",` |
|      - | 6615 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6616 | `	DataStreamData_Open,  /* xOpen */` |
|      - | 6617 | `	0,   /* xOpenDir */` |
|      - | 6618 | `	PHPStreamData_Close, /* xClose */` |
|      - | 6619 | `	0,  /* xCloseDir */` |
|      - | 6620 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 6621 | `	0,  /* xReadDir */` |
|      - | 6622 | `	DataStreamData_Write, /* xWrite */` |
|      - | 6623 | `	PHPStreamData_Seek,  /* xSeek */` |
|      - | 6624 | `	0,  /* xLock */` |
|      - | 6625 | `	0,  /* xRewindDir */` |
|      - | 6626 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 6627 | `	0,  /* xTrunc */` |
|      - | 6628 | `	0,  /* xSync */` |
|      - | 6629 | `	0   /* xStat */` |
|      - | 6630 | `};` |
|      - | 6631 | `/*` |
|      - | 6632 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 6633 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 6634 | ` * PHP-compatible process I/O functionality.` |
|      - | 6635 | ` */` |
|      - | 6636 | `typedef struct pipe_private pipe_private;` |
|      - | 6637 | `struct pipe_private` |
|      - | 6638 | `{` |
|      - | 6639 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 6640 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 6641 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 6642 | `#ifdef __WINNT__` |
|      - | 6643 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 6644 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 6645 | `#endif` |
|      - | 6646 | `};` |
|      - | 6647 |  |
|      - | 6648 | `#ifdef __WINNT__` |
|      - | 6649 | `#include <Windows.h>` |
|      - | 6650 | `#include <stdio.h>` |
|      - | 6651 | `#include <io.h>` |
|      - | 6652 | `#include <fcntl.h>` |
|      - | 6653 | `/*` |
|      - | 6654 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 6655 | ` * This allows us to properly wait for process completion.` |
|      - | 6656 | ` */` |
|      - | 6657 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      5 | 6658 | `{` |
|      5 | 6659 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      5 | 6660 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      5 | 6661 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 6662 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 6663 | `	STARTUPINFOW si;` |
|      - | 6664 | `	PROCESS_INFORMATION pi;` |
|      5 | 6665 | `	WCHAR *zWideCmd = NULL;` |
|      5 | 6666 | `	FILE *pFile = NULL;` |
|      - | 6667 | `	int fd;` |
|      5 | 6668 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 6669 |  |
|      - | 6670 | `	/* Set up security attributes for pipe inheritance */` |
|      5 | 6671 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      5 | 6672 | `	sa.bInheritHandle = TRUE;` |
|      5 | 6673 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 6674 |  |
|      - | 6675 | `	/* Create pipes for child process I/O */` |
|      5 | 6676 | `	if( bRead ){` |
|      - | 6677 | `		/* Reading from child's stdout */` |
|      5 | 6678 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 6679 | `			return NULL;` |
|      - | 6680 | `		}` |
|      - | 6681 | `		/* Ensure read handle is not inherited */` |
|      5 | 6682 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      5 | 6683 | `		hReadPipe = hChildStdoutRd;` |
|      5 | 6684 | `		*phPipe = hChildStdoutRd;` |
|      5 | 6685 | `	}else{` |
|      - | 6686 | `		/* Writing to child's stdin */` |
|    ! 0 | 6687 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 6688 | `			return NULL;` |
|      - | 6689 | `		}` |
|      - | 6690 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 6691 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 6692 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 6693 | `		*phPipe = hChildStdinWr;` |
|      - | 6694 | `	}` |
|      - | 6695 |  |
|      - | 6696 | `	/* Convert command to wide string */` |
|      - | 6697 | `	{` |
|      5 | 6698 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      5 | 6699 | `		if( nLen <= 0 ){` |
|    ! 0 | 6700 | `			goto cleanup_pipes;` |
|      - | 6701 | `		}` |
|      5 | 6702 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      5 | 6703 | `		if( !zWideCmd ){` |
|    ! 0 | 6704 | `			goto cleanup_pipes;` |
|      - | 6705 | `		}` |
|      5 | 6706 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 6707 | `	}` |
|      - | 6708 |  |
|      - | 6709 | `	/* Set up process startup info */` |
|      5 | 6710 | `	ZeroMemory(&si, sizeof(si));` |
|      5 | 6711 | `	si.cb = sizeof(si);` |
|      5 | 6712 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      5 | 6713 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      5 | 6714 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      5 | 6715 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      5 | 6716 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 6717 |  |
|      5 | 6718 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 6719 |  |
|      - | 6720 | `	/* Create the child process */` |
|      5 | 6721 | `	if( !CreateProcessW(` |
|      - | 6722 | `		NULL,           /* Application name */` |
|      - | 6723 | `		zWideCmd,       /* Command line */` |
|      - | 6724 | `		NULL,           /* Process security attributes */` |
|      - | 6725 | `		NULL,           /* Thread security attributes */` |
|      - | 6726 | `		TRUE,           /* Inherit handles */` |
|      - | 6727 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 6728 | `		NULL,           /* Environment */` |
|      - | 6729 | `		NULL,           /* Current directory */` |
|      - | 6730 | `		&si,            /* Startup info */` |
|      - | 6731 | `		&pi             /* Process info */` |
|      - | 6732 | `	)){` |
|    ! 0 | 6733 | `		goto cleanup_all;` |
|      - | 6734 | `	}` |
|      - | 6735 |  |
|      - | 6736 | `	/* Close handles we don't need in parent */` |
|      5 | 6737 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      5 | 6738 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 6739 |  |
|      - | 6740 | `	/* Close thread handle (we only need process handle) */` |
|      5 | 6741 | `	CloseHandle(pi.hThread);` |
|      - | 6742 |  |
|      - | 6743 | `	/* Store process handle for later waiting */` |
|      5 | 6744 | `	*phProcess = pi.hProcess;` |
|      - | 6745 |  |
|      - | 6746 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      5 | 6747 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 6748 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      5 | 6749 | `	if( fd == -1 ){` |
|    ! 0 | 6750 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6751 | `		*phProcess = NULL;` |
|    ! 0 | 6752 | `		goto cleanup_all;` |
|      - | 6753 | `	}` |
|      - | 6754 |  |
|      5 | 6755 | `	pFile = _fdopen(fd, zMode);` |
|      5 | 6756 | `	if( !pFile ){` |
|    ! 0 | 6757 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 6758 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6759 | `		*phProcess = NULL;` |
|    ! 0 | 6760 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 6761 | `		return NULL;` |
|      - | 6762 | `	}` |
|      - | 6763 |  |
|      5 | 6764 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      5 | 6765 | `	return pFile;` |
|      - | 6766 |  |
|      - | 6767 | `cleanup_all:` |
|    ! 0 | 6768 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 6769 | `cleanup_pipes:` |
|    ! 0 | 6770 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 6771 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 6772 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 6773 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 6774 | `	return NULL;` |
|      5 | 6775 | `}` |
|      - | 6776 |  |
|      - | 6777 | `/*` |
|      - | 6778 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 6779 | ` */` |
|      - | 6780 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      5 | 6781 | `{` |
|      5 | 6782 | `	DWORD dwExitCode = 0;` |
|      - | 6783 | `	int status;` |
|      - | 6784 |  |
|      - | 6785 | `	/* Close the FILE* (this closes the pipe) */` |
|      5 | 6786 | `	fclose(pFile);` |
|      - | 6787 |  |
|      5 | 6788 | `	if( hProcess ){` |
|      - | 6789 | `		/* Wait for the process to complete */` |
|      5 | 6790 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 6791 |  |
|      5 | 6792 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      5 | 6793 | `			status = (int)dwExitCode;` |
|      5 | 6794 | `		}else{` |
|    ! 0 | 6795 | `			status = -1;` |
|      - | 6796 | `		}` |
|      - | 6797 |  |
|      - | 6798 | `		/* Close process handle */` |
|      5 | 6799 | `		CloseHandle(hProcess);` |
|      5 | 6800 | `	}else{` |
|    ! 0 | 6801 | `		status = -1;` |
|      - | 6802 | `	}` |
|      - | 6803 |  |
|      5 | 6804 | `	return status;` |
|      5 | 6805 | `}` |
|      - | 6806 | `#endif /* __WINNT__ */` |
|      - | 6807 | `/*` |
|      - | 6808 | ` * Open a pipe to a process.` |
|      - | 6809 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 6810 | ` */` |
|   3964 | 6811 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      5 | 6812 | `{` |
|      - | 6813 | `	pipe_private *pPipe;` |
|      - | 6814 | `	FILE *pFile;` |
|   3969 | 6815 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 6816 | `		return 0;` |
|      - | 6817 | `	}` |
|      - | 6818 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   3969 | 6819 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 6820 | `		return 0;` |
|      - | 6821 | `	}` |
|      - | 6822 | `	/* Open the pipe using system popen */` |
|      - | 6823 | `#ifdef __WINNT__` |
|      - | 6824 | `	{` |
|      - | 6825 | `		/* Build cmd.exe command wrapper */` |
|      5 | 6826 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      5 | 6827 | `		const char *zShellSuffix = "\"";` |
|      5 | 6828 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      5 | 6829 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      5 | 6830 | `		size_t nCmd = strlen(zCommand);` |
|      5 | 6831 | `		size_t nQuotes = 0;` |
|      5 | 6832 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6833 | `			if (zCommand[i] == '"') nQuotes++;` |
|      5 | 6834 | `		}` |
|      5 | 6835 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      5 | 6836 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      5 | 6837 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 6838 | `			return 0;` |
|      - | 6839 | `		}` |
|      - | 6840 | `		/* Escape quotes in command */` |
|      5 | 6841 | `		size_t j = 0;` |
|      5 | 6842 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6843 | `			char ch = zCommand[i];` |
|      5 | 6844 | `			if (ch == '"') {` |
|      4 | 6845 | `				zCmdEsc[j++] = '^';` |
|      4 | 6846 | `				zCmdEsc[j++] = '"';` |
|      4 | 6847 | `			} else {` |
|      5 | 6848 | `				zCmdEsc[j++] = ch;` |
|      - | 6849 | `			}` |
|      5 | 6850 | `		}` |
|      5 | 6851 | `		zCmdEsc[j] = '\0';` |
|      5 | 6852 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      5 | 6853 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      5 | 6854 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 6855 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6856 | `			return 0;` |
|      - | 6857 | `		}` |
|      5 | 6858 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      5 | 6859 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      5 | 6860 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      5 | 6861 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 6862 | `		/* Allocate pipe structure early so we can store handles */` |
|      5 | 6863 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      5 | 6864 | `		if( pPipe == 0 ){` |
|    ! 0 | 6865 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6866 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 6867 | `			return 0;` |
|      - | 6868 | `		}` |
|      - | 6869 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      5 | 6870 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      5 | 6871 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      5 | 6872 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      5 | 6873 | `		if( pFile == 0 ){` |
|    ! 0 | 6874 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 6875 | `			return 0;` |
|      - | 6876 | `		}` |
|      - | 6877 | `		/* Initialize remaining fields */` |
|      5 | 6878 | `		pPipe->pFile = pFile;` |
|      5 | 6879 | `		pPipe->pVm = pVm;` |
|      5 | 6880 | `		pPipe->iMode = zMode[0];` |
|      - | 6881 | `	}` |
|      - | 6882 | `#elif defined(__UNIXES__) /* Unix */` |
|   3964 | 6883 | `	pFile = popen(zCommand, zMode);` |
|   3964 | 6884 | `	if( pFile == 0 ){` |
|    ! 0 | 6885 | `		return 0;` |
|      - | 6886 | `	}` |
|      - | 6887 | `	/* Allocate pipe private structure */` |
|   3964 | 6888 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   3964 | 6889 | `	if( pPipe == 0 ){` |
|      - | 6890 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 6891 | `		pclose(pFile);` |
|    ! 0 | 6892 | `		return 0;` |
|      - | 6893 | `	}` |
|      - | 6894 | `	/* Initialize the structure */` |
|   3964 | 6895 | `	pPipe->pFile = pFile;` |
|   3964 | 6896 | `	pPipe->pVm = pVm;` |
|   3964 | 6897 | `	pPipe->iMode = zMode[0];` |
|      - | 6898 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6899 | `	(void)pFile;` |
|      - | 6900 | `	return 0;` |
|      - | 6901 | `#endif` |
|   3969 | 6902 | `	return pPipe;` |
|   1987 | 6903 | `}` |
|      - | 6904 | `/*` |
|      - | 6905 | ` * Close a pipe and return the exit status of the process.` |
|      - | 6906 | ` * Returns the exit status, or -1 on error.` |
|      - | 6907 | ` */` |
|   3938 | 6908 | `static int PipeClose(pipe_private *pPipe)` |
|      5 | 6909 | `{` |
|      - | 6910 | `	int status;` |
|      - | 6911 | `	ph7_vm *pVm;` |
|   3943 | 6912 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6913 | `		return -1;` |
|      - | 6914 | `	}` |
|   3943 | 6915 | `	pVm = pPipe->pVm;` |
|      - | 6916 | `	/* Close the pipe and get exit status */` |
|      - | 6917 | `#ifdef __WINNT__` |
|      - | 6918 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      5 | 6919 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 6920 | `#elif defined(__UNIXES__)` |
|   3938 | 6921 | `	status = pclose(pPipe->pFile);` |
|      - | 6922 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   3938 | 6923 | `	if( status != -1 ){` |
|   3938 | 6924 | `		if( WIFEXITED(status) ){` |
|   3938 | 6925 | `			status = WEXITSTATUS(status);` |
|   1969 | 6926 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 6927 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 6928 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 6929 | `		}else{` |
|      - | 6930 | `			/* Unknown termination reason */` |
|    ! 0 | 6931 | `			status = -1;` |
|      - | 6932 | `		}` |
|   1969 | 6933 | `	}` |
|      - | 6934 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6935 | `	status = -1;` |
|      - | 6936 | `#endif` |
|      - | 6937 | `	/* Free the structure */` |
|   3943 | 6938 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   3943 | 6939 | `	return status;` |
|   1974 | 6940 | `}` |
|      - | 6941 | `/*` |
|      - | 6942 | ` * Pipe stream xClose implementation.` |
|      - | 6943 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 6944 | ` * It closes the pipe but does not return the exit status.` |
|      - | 6945 | ` */` |
|    102 | 6946 | `static void PipeStream_Close(void *pHandle)` |
|      4 | 6947 | `{` |
|    106 | 6948 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    106 | 6949 | `	if( pPipe ){` |
|    106 | 6950 | `		PipeClose(pPipe);` |
|     51 | 6951 | `	}` |
|    106 | 6952 | `}` |
|      - | 6953 | `/*` |
|      - | 6954 | ` * Pipe stream xRead implementation.` |
|      - | 6955 | ` */` |
|   5890 | 6956 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      4 | 6957 | `{` |
|   5894 | 6958 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6959 | `	size_t nRead;` |
|   5894 | 6960 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6961 | `		return -1;` |
|      - | 6962 | `	}` |
|   5894 | 6963 | `	if( pPipe->iMode != 'r' ){` |
|      - | 6964 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 6965 | `		return -1;` |
|      - | 6966 | `	}` |
|   5894 | 6967 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   5894 | 6968 | `	if( nRead == 0 ){` |
|   3970 | 6969 | `		if( feof(pPipe->pFile) ){` |
|   3970 | 6970 | `			return 0; /* EOF */` |
|      - | 6971 | `		}` |
|    ! 0 | 6972 | `		return -1; /* Error */` |
|      - | 6973 | `	}` |
|   1928 | 6974 | `	return (ph7_int64)nRead;` |
|   2949 | 6975 | `}` |
|      - | 6976 | `/*` |
|      - | 6977 | ` * Pipe stream xWrite implementation.` |
|      - | 6978 | ` */` |
|      4 | 6979 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 6980 | `{` |
|      4 | 6981 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6982 | `	size_t nWritten;` |
|      4 | 6983 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6984 | `		return -1;` |
|      - | 6985 | `	}` |
|      4 | 6986 | `	if( pPipe->iMode != 'w' ){` |
|      - | 6987 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 6988 | `		return -1;` |
|      - | 6989 | `	}` |
|      4 | 6990 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      4 | 6991 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 6992 | `		return -1; /* Error */` |
|      - | 6993 | `	}` |
|      4 | 6994 | `	return (ph7_int64)nWritten;` |
|      2 | 6995 | `}` |
|      - | 6996 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 6997 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 6998 | `	"pipe",` |
|      - | 6999 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7000 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|      - | 7001 | `	0,  /* xOpenDir */` |
|      - | 7002 | `	PipeStream_Close,  /* xClose */` |
|      - | 7003 | `	0,  /* xCloseDir */` |
|      - | 7004 | `	PipeStream_Read,   /* xRead */` |
|      - | 7005 | `	0,  /* xReadDir */` |
|      - | 7006 | `	PipeStream_Write,  /* xWrite */` |
|      - | 7007 | `	0,  /* xSeek */` |
|      - | 7008 | `	0,  /* xLock */` |
|      - | 7009 | `	0,  /* xRewindDir */` |
|      - | 7010 | `	0,  /* xTell */` |
|      - | 7011 | `	0,  /* xTrunc */` |
|      - | 7012 | `	0,  /* xSync */` |
|      - | 7013 | `	0   /* xStat */` |
|      - | 7014 | `};` |
|      - | 7015 | `/*` |
|      - | 7016 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 7017 | ` * FALSE otherwise.` |
|      - | 7018 | ` */` |
|   3832 | 7019 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      5 | 7020 | `{` |
|   3837 | 7021 | `	return pStream == &sPipe_Stream;` |
|      5 | 7022 | `}` |
|      - | 7023 | `/*` |
|      - | 7024 | ` * resource popen(string $command, string $mode)` |
|      - | 7025 | ` *  Opens process file pointer.` |
|      - | 7026 | ` * Parameters` |
|      - | 7027 | ` *  $command` |
|      - | 7028 | ` *   The command to execute. Passed to the system shell.` |
|      - | 7029 | ` *  $mode` |
|      - | 7030 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 7031 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 7032 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 7033 | ` * Return` |
|      - | 7034 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 7035 | ` */` |
|      - | 7036 | `/*` |
|      - | 7037 | ` * string\|false\|null shell_exec(string $command)` |
|      - | 7038 | ` *  Execute a command via the shell and return the complete output as a string.` |
|      - | 7039 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|      - | 7040 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|      - | 7041 | ` */` |
|      4 | 7042 | `static int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7043 | `{` |
|      - | 7044 | `	const char *zCommand;` |
|      - | 7045 | `	pipe_private *pPipe;` |
|      - | 7046 | `	SyBlob sOut;` |
|      - | 7047 | `	char zBuf[4096];` |
|      - | 7048 | `	size_t nRead;` |
|      - | 7049 | `	int nCmdLen;` |
|      6 | 7050 | `	if( nArg < 1 ){` |
|    ! 0 | 7051 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7052 | `		return PH7_OK;` |
|      - | 7053 | `	}` |
|      6 | 7054 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|      6 | 7055 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7056 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7057 | `		return PH7_OK;` |
|      - | 7058 | `	}` |
|      6 | 7059 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|      6 | 7060 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 7061 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7062 | `		return PH7_OK;` |
|      - | 7063 | `	}` |
|      6 | 7064 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|      4 | 7065 | `	for(;;){` |
|     10 | 7066 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|     10 | 7067 | `		if( nRead < 1 ){` |
|      6 | 7068 | `			break;` |
|      - | 7069 | `		}` |
|      6 | 7070 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|      2 | 7071 | `	}` |
|      6 | 7072 | `	PipeClose(pPipe);` |
|      6 | 7073 | `	if( SyBlobLength(&sOut) < 1 ){` |
|      - | 7074 | `		/* php answers NULL, not "", when the command printed nothing */` |
|    ! 0 | 7075 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7076 | `	}else{` |
|      6 | 7077 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|      - | 7078 | `	}` |
|      6 | 7079 | `	SyBlobRelease(&sOut);` |
|      6 | 7080 | `	return PH7_OK;` |
|      4 | 7081 | `}` |
|   3960 | 7082 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7083 | `{` |
|      - | 7084 | `	const char *zCommand, *zMode;` |
|      - | 7085 | `	pipe_private *pPipe;` |
|      - | 7086 | `	io_private *pDev;` |
|      - | 7087 | `	int nCmdLen, nModeLen;` |
|   3965 | 7088 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 7089 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 7090 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 7091 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7092 | `		return PH7_OK;` |
|      - | 7093 | `	}` |
|      - | 7094 | `	/* Extract the command and mode */` |
|   3965 | 7095 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   3965 | 7096 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   3965 | 7097 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7098 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 7099 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7100 | `		return PH7_OK;` |
|      - | 7101 | `	}` |
|   3965 | 7102 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 7103 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 7104 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7105 | `		return PH7_OK;` |
|      - | 7106 | `	}` |
|      - | 7107 | `	/* Open the pipe */` |
|   3965 | 7108 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   3965 | 7109 | `	if( pPipe == 0 ){` |
|      - | 7110 | `		/* Failed to open pipe */` |
|    ! 0 | 7111 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7112 | `		return PH7_OK;` |
|      - | 7113 | `	}` |
|      - | 7114 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   3965 | 7115 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   3965 | 7116 | `	if( pDev == 0 ){` |
|    ! 0 | 7117 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 7118 | `		PipeClose(pPipe);` |
|    ! 0 | 7119 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7120 | `		return PH7_OK;` |
|      - | 7121 | `	}` |
|      - | 7122 | `	/* Initialize the io_private structure */` |
|   3965 | 7123 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   3965 | 7124 | `	pDev->pHandle = pPipe;` |
|      - | 7125 | `	/* Return the io_private instance as a resource */` |
|   3965 | 7126 | `	ph7_result_resource(pCtx, pDev);` |
|   3965 | 7127 | `	return PH7_OK;` |
|   1985 | 7128 | `}` |
|      - | 7129 | `/*` |
|      - | 7130 | ` * int pclose(resource $handle)` |
|      - | 7131 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 7132 | ` * Parameters` |
|      - | 7133 | ` *  $handle` |
|      - | 7134 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 7135 | ` * Return` |
|      - | 7136 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 7137 | ` */` |
|   3832 | 7138 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7139 | `{` |
|      - | 7140 | `	const ph7_io_stream *pStream;` |
|      - | 7141 | `	pipe_private *pPipe;` |
|      - | 7142 | `	io_private *pDev;` |
|      - | 7143 | `	int status;` |
|   3837 | 7144 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 7145 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 7146 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7147 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7148 | `		return PH7_OK;` |
|      - | 7149 | `	}` |
|      - | 7150 | `	/* Extract our private data */` |
|   3837 | 7151 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 7152 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3837 | 7153 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 7154 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7155 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7156 | `		return PH7_OK;` |
|      - | 7157 | `	}` |
|      - | 7158 | `	/* Point to the target IO stream device */` |
|   3837 | 7159 | `	pStream = pDev->pStream;` |
|   3837 | 7160 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 7161 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 7162 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7163 | `		return PH7_OK;` |
|      - | 7164 | `	}` |
|      - | 7165 | `	/* Get the pipe handle */` |
|   3837 | 7166 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 7167 | `	/* Close the pipe and get exit status */` |
|   3837 | 7168 | `	status = PipeClose(pPipe);` |
|      - | 7169 | `	/* Keep the handle alive but flag it closed so shared copies see it */` |
|   3837 | 7170 | `	MarkIOPrivateClosed(pDev);` |
|      - | 7171 | `	/* Return the exit status */` |
|   3837 | 7172 | `	ph7_result_int(pCtx, status);` |
|   3837 | 7173 | `	return PH7_OK;` |
|   1921 | 7174 | `}` |
|      - | 7175 | `/*` |
|      - | 7176 | ` * proc_open() / proc_close() / proc_get_status() / proc_terminate()` |
|      - | 7177 | ` *   Run a command via fork()/exec() with fine-grained control over its` |
|      - | 7178 | ` *   standard descriptors (php's process-control family). The returned` |
|      - | 7179 | ` *   "process" resource wraps a small proc_private whose leading bytes mirror` |
|      - | 7180 | ` *   io_private (a distinct magic) so is_resource()/gettype() probes stay in` |
|      - | 7181 | ` *   bounds and report it as a live, non-stream resource.` |
|      - | 7182 | ` */` |
|      - | 7183 | `#ifdef __UNIXES__` |
|      - | 7184 | `#define PROC_PRIVATE_MAGIC 0x9C0DE5` |
|      - | 7185 | `#define PROC_MAX_DESC 16` |
|      - | 7186 | `typedef struct proc_private proc_private;` |
|      - | 7187 | `struct proc_private` |
|      - | 7188 | `{` |
|      - | 7189 | `	io_private base;   /* io_private-compatible header (base.iMagic == PROC_PRIVATE_MAGIC) */` |
|      - | 7190 | `	int pid;           /* child process id */` |
|      - | 7191 | `	int running;       /* TRUE until reaped by proc_close/proc_get_status */` |
|      - | 7192 | `	int exit_code;     /* cached exit status once reaped */` |
|      - | 7193 | `};` |
|      - | 7194 | `/* Wrap a raw fd as an fopen-style stream resource (a php pipe end). */` |
|     28 | 7195 | `static io_private * ProcWrapFd(ph7_vm *pVm,int fd)` |
|      - | 7196 | `{` |
|     28 | 7197 | `	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|     28 | 7198 | `	if( pDev == 0 ){` |
|    ! 0 | 7199 | `		return 0;` |
|      - | 7200 | `	}` |
|     28 | 7201 | `	InitIOPrivate(pVm,&sUnixFileStream,pDev);` |
|     28 | 7202 | `	pDev->pHandle = SX_INT_TO_PTR(fd);` |
|     28 | 7203 | `	return pDev;` |
|     14 | 7204 | `}` |
|      - | 7205 | `/* One parsed descriptor-spec entry. */` |
|      - | 7206 | `struct proc_desc` |
|      - | 7207 | `{` |
|      - | 7208 | `	int child_fd;      /* the array key: which fd the child sees */` |
|      - | 7209 | `	int kind;          /* 0=pipe, 1=file, 2=redirect */` |
|      - | 7210 | `	/* pipe */` |
|      - | 7211 | `	int child_end;     /* fd the child must have at child_fd */` |
|      - | 7212 | `	int parent_end;    /* fd the parent keeps (wrapped into $pipes), -1 if none */` |
|      - | 7213 | `	/* file */` |
|      - | 7214 | `	int file_fd;       /* opened fd for a ['file',path,mode] spec */` |
|      - | 7215 | `	/* redirect */` |
|      - | 7216 | `	int redirect_to;   /* target child fd for a ['redirect',N] spec */` |
|      - | 7217 | `};` |
|     10 | 7218 | `static int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7219 | `{` |
|      - | 7220 | `	struct proc_desc aDesc[PROC_MAX_DESC];` |
|     10 | 7221 | `	int nDesc = 0;` |
|      - | 7222 | `	ph7_value *pSpec, *pPipes, *pEntry, *pType, *pParam;` |
|      - | 7223 | `	ph7_hashmap *pSpecMap;` |
|      - | 7224 | `	ph7_hashmap_node *pNode;` |
|     10 | 7225 | `	ph7_vm *pVm = pCtx->pVm;` |
|     10 | 7226 | `	char **azArgv = 0;      /* exec argv when the command is an array */` |
|     10 | 7227 | `	int nArgv = 0;` |
|     10 | 7228 | `	const char *zCmd = 0;   /* exec command when it is a string (via /bin/sh -c) */` |
|     10 | 7229 | `	const char *zCwd = 0;` |
|     10 | 7230 | `	char **azEnv = 0;       /* constructed envp when an env array is supplied */` |
|     10 | 7231 | `	int nEnv = 0;` |
|      - | 7232 | `	proc_private *pProc;` |
|      - | 7233 | `	pid_t pid;` |
|      - | 7234 | `	int i, rc;` |
|     10 | 7235 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[1]) ){` |
|    ! 0 | 7236 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() expects a command and a descriptor spec");` |
|    ! 0 | 7237 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7238 | `		return PH7_OK;` |
|      - | 7239 | `	}` |
|      - | 7240 | `	/* --- Command: array (execvp) or string (/bin/sh -c) --- */` |
|     10 | 7241 | `	if( ph7_value_is_array(apArg[0]) ){` |
|     10 | 7242 | `		ph7_hashmap *pCmdMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     10 | 7243 | `		int nCount = (int)ph7_array_count(apArg[0]);` |
|     10 | 7244 | `		azArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|     10 | 7245 | `		if( azArgv == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     10 | 7246 | `		pNode = pCmdMap->pFirst;` |
|     20 | 7247 | `		for( i = 0 ; i < nCount && pNode ; ++i ){` |
|     10 | 7248 | `			ph7_value *pv = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|      - | 7249 | `			int nLen; const char *zs;` |
|     10 | 7250 | `			PH7_MemObjInit(pVm,pv);` |
|     10 | 7251 | `			PH7_HashmapExtractNodeValue(pNode,pv,FALSE);` |
|     10 | 7252 | `			zs = ph7_value_to_string(pv,&nLen);` |
|     10 | 7253 | `			azArgv[nArgv] = (char *)SyMemBackendDup(&pVm->sAllocator,zs,(sxu32)nLen+1);` |
|     10 | 7254 | `			if( azArgv[nArgv] ){ azArgv[nArgv][nLen] = 0; nArgv++; }` |
|     10 | 7255 | `			PH7_MemObjRelease(pv);` |
|     10 | 7256 | `			SyMemBackendFree(&pVm->sAllocator,pv);` |
|     10 | 7257 | `			pNode = pNode->pPrev; /* hashmap insertion-order walk */` |
|      5 | 7258 | `		}` |
|     10 | 7259 | `		azArgv[nArgv] = 0;` |
|      5 | 7260 | `	}else{` |
|      - | 7261 | `		int nLen;` |
|    ! 0 | 7262 | `		zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7263 | `	}` |
|      - | 7264 | `	/* --- Optional cwd (arg 4) and env (arg 5) --- */` |
|     10 | 7265 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|    ! 0 | 7266 | `		int nLen; zCwd = ph7_value_to_string(apArg[3],&nLen);` |
|    ! 0 | 7267 | `		if( nLen < 1 ){ zCwd = 0; }` |
|    ! 0 | 7268 | `	}` |
|      5 | 7269 | `	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){` |
|    ! 0 | 7270 | `		ph7_hashmap *pEnvMap = (ph7_hashmap *)apArg[4]->x.pOther;` |
|    ! 0 | 7271 | `		int nCount = (int)ph7_array_count(apArg[4]);` |
|    ! 0 | 7272 | `		azEnv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|    ! 0 | 7273 | `		if( azEnv ){` |
|    ! 0 | 7274 | `			pNode = pEnvMap->pFirst;` |
|    ! 0 | 7275 | `			for( i = 0 ; i < nCount && pNode ; ++i ){` |
|      - | 7276 | `				ph7_value sKey, sVal; int nk, nv; const char *zk, *zv; char *zPair;` |
|    ! 0 | 7277 | `				PH7_MemObjInit(pVm,&sKey); PH7_MemObjInit(pVm,&sVal);` |
|    ! 0 | 7278 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|    ! 0 | 7279 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|    ! 0 | 7280 | `				zk = ph7_value_to_string(&sKey,&nk);` |
|    ! 0 | 7281 | `				zv = ph7_value_to_string(&sVal,&nv);` |
|    ! 0 | 7282 | `				zPair = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nk+nv+2));` |
|    ! 0 | 7283 | `				if( zPair ){` |
|    ! 0 | 7284 | `					SyMemcpy(zk,zPair,(sxu32)nk); zPair[nk] = '=';` |
|    ! 0 | 7285 | `					SyMemcpy(zv,&zPair[nk+1],(sxu32)nv); zPair[nk+1+nv] = 0;` |
|    ! 0 | 7286 | `					azEnv[nEnv++] = zPair;` |
|    ! 0 | 7287 | `				}` |
|    ! 0 | 7288 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|    ! 0 | 7289 | `				pNode = pNode->pPrev;` |
|    ! 0 | 7290 | `			}` |
|    ! 0 | 7291 | `			azEnv[nEnv] = 0;` |
|    ! 0 | 7292 | `		}` |
|    ! 0 | 7293 | `	}` |
|      - | 7294 | `	/* --- Parse the descriptor spec, creating pipes as we go --- */` |
|     10 | 7295 | `	pSpec = apArg[1];` |
|     10 | 7296 | `	pSpecMap = (ph7_hashmap *)pSpec->x.pOther;` |
|     10 | 7297 | `	pNode = pSpecMap->pFirst;` |
|     40 | 7298 | `	for( i = 0 ; i < (int)pSpecMap->nEntry && pNode && nDesc < PROC_MAX_DESC ; ++i ){` |
|     30 | 7299 | `		ph7_value sKey; struct proc_desc *pD = &aDesc[nDesc];` |
|     30 | 7300 | `		PH7_MemObjInit(pVm,&sKey);` |
|     30 | 7301 | `		PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|     30 | 7302 | `		pD->child_fd = ph7_value_to_int(&sKey);` |
|     30 | 7303 | `		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1;` |
|     30 | 7304 | `		PH7_MemObjRelease(&sKey);` |
|     30 | 7305 | `		pEntry = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|     30 | 7306 | `		PH7_MemObjInit(pVm,pEntry);` |
|     30 | 7307 | `		PH7_HashmapExtractNodeValue(pNode,pEntry,FALSE);` |
|     30 | 7308 | `		if( ph7_value_is_array(pEntry) ){` |
|      - | 7309 | `			int nLen; const char *zType;` |
|     30 | 7310 | `			pType = ph7_array_fetch(pEntry,"0",1);` |
|     30 | 7311 | `			zType = pType ? ph7_value_to_string(pType,&nLen) : "";` |
|     30 | 7312 | `			if( SyStrncmp(zType,"pipe",4) == 0 ){` |
|      - | 7313 | `				int fds[2];` |
|     28 | 7314 | `				if( pipe(fds) == 0 ){` |
|     28 | 7315 | `					pParam = ph7_array_fetch(pEntry,"1",1);` |
|      - | 7316 | `					{` |
|     28 | 7317 | `						int nMode; const char *zMode = pParam ? ph7_value_to_string(pParam,&nMode) : "r";` |
|     28 | 7318 | `						pD->kind = 0;` |
|     28 | 7319 | `						if( zMode[0] == 'w' \|\| zMode[0] == 'a' ){` |
|      - | 7320 | `							/* child writes -> parent reads: child gets write end */` |
|     18 | 7321 | `							pD->child_end = fds[1]; pD->parent_end = fds[0];` |
|      9 | 7322 | `						}else{` |
|      - | 7323 | `							/* child reads -> parent writes: child gets read end */` |
|     10 | 7324 | `							pD->child_end = fds[0]; pD->parent_end = fds[1];` |
|      - | 7325 | `						}` |
|     28 | 7326 | `						nDesc++;` |
|      - | 7327 | `					}` |
|     14 | 7328 | `				}` |
|     16 | 7329 | `			}else if( SyStrncmp(zType,"file",4) == 0 ){` |
|    ! 0 | 7330 | `				int nLen2, nLen3; const char *zPath, *zMode; int oflag = O_RDONLY;` |
|    ! 0 | 7331 | `				ph7_value *pPath = ph7_array_fetch(pEntry,"1",1);` |
|    ! 0 | 7332 | `				ph7_value *pMode = ph7_array_fetch(pEntry,"2",1);` |
|    ! 0 | 7333 | `				zPath = pPath ? ph7_value_to_string(pPath,&nLen2) : "";` |
|    ! 0 | 7334 | `				zMode = pMode ? ph7_value_to_string(pMode,&nLen3) : "r";` |
|    ! 0 | 7335 | `				if( zMode[0] == 'w' ){ oflag = O_WRONLY\|O_CREAT\|O_TRUNC; }` |
|    ! 0 | 7336 | `				else if( zMode[0] == 'a' ){ oflag = O_WRONLY\|O_CREAT\|O_APPEND; }` |
|    ! 0 | 7337 | `				pD->kind = 1;` |
|    ! 0 | 7338 | `				pD->file_fd = open(zPath,oflag,0644);` |
|    ! 0 | 7339 | `				nDesc++;` |
|      2 | 7340 | `			}else if( SyStrncmp(zType,"redirect",8) == 0 ){` |
|      2 | 7341 | `				pParam = ph7_array_fetch(pEntry,"1",1);` |
|      2 | 7342 | `				pD->kind = 2;` |
|      2 | 7343 | `				pD->redirect_to = pParam ? ph7_value_to_int(pParam) : 1;` |
|      2 | 7344 | `				nDesc++;` |
|      1 | 7345 | `			}` |
|     15 | 7346 | `		}` |
|     30 | 7347 | `		PH7_MemObjRelease(pEntry);` |
|     30 | 7348 | `		SyMemBackendFree(&pVm->sAllocator,pEntry);` |
|     30 | 7349 | `		pNode = pNode->pPrev;` |
|     15 | 7350 | `	}` |
|      - | 7351 | `	/* --- Fork the child --- */` |
|     10 | 7352 | `	pid = fork();` |
|     15 | 7353 | `	if( pid < 0 ){` |
|    ! 0 | 7354 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open(): fork() failed");` |
|    ! 0 | 7355 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7356 | `		return PH7_OK;` |
|      - | 7357 | `	}` |
|     20 | 7358 | `	if( pid == 0 ){` |
|      - | 7359 | `		/* Child: wire up descriptors then exec */` |
|     40 | 7360 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|     30 | 7361 | `			struct proc_desc *pD = &aDesc[i];` |
|     30 | 7362 | `			if( pD->kind == 0 ){` |
|     28 | 7363 | `				dup2(pD->child_end,pD->child_fd);` |
|     28 | 7364 | `				close(pD->parent_end);` |
|     28 | 7365 | `				close(pD->child_end);` |
|     16 | 7366 | `			}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|    ! 0 | 7367 | `				dup2(pD->file_fd,pD->child_fd);` |
|    ! 0 | 7368 | `				close(pD->file_fd);` |
|    ! 0 | 7369 | `			}` |
|     15 | 7370 | `		}` |
|      - | 7371 | `		/* Redirects run after the pipes are in place (e.g. 2>&1) */` |
|     40 | 7372 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|     30 | 7373 | `			if( aDesc[i].kind == 2 ){ dup2(aDesc[i].redirect_to,aDesc[i].child_fd); }` |
|     15 | 7374 | `		}` |
|     10 | 7375 | `		if( zCwd ){ if( chdir(zCwd) != 0 ){ _exit(127); } }` |
|     10 | 7376 | `		if( azEnv ){` |
|    ! 0 | 7377 | `			if( azArgv ){ execve(azArgv[0],azArgv,azEnv); }` |
|    ! 0 | 7378 | `			else{ char *av[4]; av[0]=(char*)"sh"; av[1]=(char*)"-c"; av[2]=(char*)zCmd; av[3]=0; execve("/bin/sh",av,azEnv); }` |
|    ! 0 | 7379 | `		}else{` |
|     10 | 7380 | `			if( azArgv ){ execvp(azArgv[0],azArgv); }` |
|    ! 0 | 7381 | `			else{ execl("/bin/sh","sh","-c",zCmd,(char *)0); }` |
|      - | 7382 | `		}` |
|      5 | 7383 | `		_exit(127); /* exec failed */` |
|      - | 7384 | `	}` |
|      - | 7385 | `	/* Parent: close the child ends, wrap the parent ends into $pipes */` |
|     10 | 7386 | `	pPipes = ph7_context_new_array(pCtx);` |
|     40 | 7387 | `	for( i = 0 ; i < nDesc ; ++i ){` |
|     30 | 7388 | `		struct proc_desc *pD = &aDesc[i];` |
|     30 | 7389 | `		if( pD->kind == 0 ){` |
|      - | 7390 | `			io_private *pEnd;` |
|      - | 7391 | `			ph7_value *pRes;` |
|     28 | 7392 | `			close(pD->child_end);` |
|     28 | 7393 | `			pEnd = ProcWrapFd(pVm,pD->parent_end);` |
|     28 | 7394 | `			pRes = ph7_context_new_scalar(pCtx);` |
|     28 | 7395 | `			if( pEnd && pRes && pPipes ){` |
|     28 | 7396 | `				ph7_value_resource(pRes,pEnd);` |
|     28 | 7397 | `				ph7_array_add_intkey_elem(pPipes,pD->child_fd,pRes);` |
|     14 | 7398 | `			}` |
|     28 | 7399 | `			if( pRes ){ ph7_context_release_value(pCtx,pRes); }` |
|     16 | 7400 | `		}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|    ! 0 | 7401 | `			close(pD->file_fd);` |
|    ! 0 | 7402 | `		}` |
|     15 | 7403 | `	}` |
|     10 | 7404 | `	if( pPipes ){` |
|     10 | 7405 | `		PH7_VmStoreArgByRef(pVm,apArg[2],pPipes);` |
|      5 | 7406 | `	}` |
|      - | 7407 | `	/* Free the exec argv/env copies now that the child owns its own image */` |
|     10 | 7408 | `	if( azArgv ){` |
|     20 | 7409 | `		for( i = 0 ; i < nArgv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azArgv[i]); }` |
|     10 | 7410 | `		SyMemBackendFree(&pVm->sAllocator,azArgv);` |
|      5 | 7411 | `	}` |
|     15 | 7412 | `	if( azEnv ){` |
|    ! 0 | 7413 | `		for( i = 0 ; i < nEnv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azEnv[i]); }` |
|    ! 0 | 7414 | `		SyMemBackendFree(&pVm->sAllocator,azEnv);` |
|    ! 0 | 7415 | `	}` |
|      - | 7416 | `	/* Build the process resource */` |
|     10 | 7417 | `	pProc = (proc_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(proc_private));` |
|     10 | 7418 | `	if( pProc == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     10 | 7419 | `	SyZero(pProc,sizeof(proc_private));` |
|     10 | 7420 | `	pProc->base.iMagic = PROC_PRIVATE_MAGIC;` |
|     10 | 7421 | `	pProc->pid = (int)pid;` |
|     10 | 7422 | `	pProc->running = 1;` |
|     10 | 7423 | `	pProc->exit_code = 0;` |
|     10 | 7424 | `	ph7_result_resource(pCtx,pProc);` |
|      5 | 7425 | `	(void)rc;` |
|     10 | 7426 | `	return PH7_OK;` |
|      5 | 7427 | `}` |
|      - | 7428 | `/* Reap the child if it has not been reaped yet, caching the exit code. */` |
|     10 | 7429 | `static void ProcReap(proc_private *pProc,int block)` |
|      - | 7430 | `{` |
|     10 | 7431 | `	int status = 0;` |
|      - | 7432 | `	pid_t r;` |
|     10 | 7433 | `	if( !pProc->running ){ return; }` |
|     10 | 7434 | `	r = waitpid((pid_t)pProc->pid,&status,block ? 0 : WNOHANG);` |
|     10 | 7435 | `	if( r == (pid_t)pProc->pid ){` |
|     10 | 7436 | `		pProc->running = 0;` |
|     10 | 7437 | `		if( WIFEXITED(status) ){ pProc->exit_code = WEXITSTATUS(status); }` |
|    ! 0 | 7438 | `		else if( WIFSIGNALED(status) ){ pProc->exit_code = 128 + WTERMSIG(status); }` |
|      5 | 7439 | `	}` |
|      5 | 7440 | `}` |
|     10 | 7441 | `static int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7442 | `{` |
|      - | 7443 | `	proc_private *pProc;` |
|     10 | 7444 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7445 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 7446 | `		return PH7_OK;` |
|      - | 7447 | `	}` |
|     10 | 7448 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|     10 | 7449 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|    ! 0 | 7450 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 7451 | `		return PH7_OK;` |
|      - | 7452 | `	}` |
|     10 | 7453 | `	ProcReap(pProc,1/*block until it exits*/);` |
|     10 | 7454 | `	ph7_result_int(pCtx,pProc->exit_code);` |
|     10 | 7455 | `	return PH7_OK;` |
|      5 | 7456 | `}` |
|    ! 0 | 7457 | `static int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7458 | `{` |
|      - | 7459 | `	proc_private *pProc;` |
|    ! 0 | 7460 | `	int sig = 15; /* SIGTERM */` |
|    ! 0 | 7461 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7462 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7463 | `		return PH7_OK;` |
|      - | 7464 | `	}` |
|    ! 0 | 7465 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|    ! 0 | 7466 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|    ! 0 | 7467 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7468 | `		return PH7_OK;` |
|      - | 7469 | `	}` |
|    ! 0 | 7470 | `	if( nArg > 1 ){ sig = ph7_value_to_int(apArg[1]); }` |
|    ! 0 | 7471 | `	if( pProc->running ){ kill((pid_t)pProc->pid,sig); }` |
|    ! 0 | 7472 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 7473 | `	return PH7_OK;` |
|    ! 0 | 7474 | `}` |
|    ! 0 | 7475 | `static int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7476 | `{` |
|      - | 7477 | `	proc_private *pProc;` |
|      - | 7478 | `	ph7_value *pArray, *pVal;` |
|    ! 0 | 7479 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7480 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7481 | `		return PH7_OK;` |
|      - | 7482 | `	}` |
|    ! 0 | 7483 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|    ! 0 | 7484 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|    ! 0 | 7485 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7486 | `		return PH7_OK;` |
|      - | 7487 | `	}` |
|    ! 0 | 7488 | `	ProcReap(pProc,0/*non-blocking poll*/);` |
|    ! 0 | 7489 | `	pArray = ph7_context_new_array(pCtx);` |
|    ! 0 | 7490 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 7491 | `	if( pArray == 0 \|\| pVal == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    ! 0 | 7492 | `	ph7_value_int(pVal,pProc->pid);` |
|    ! 0 | 7493 | `	ph7_array_add_strkey_elem(pArray,"pid",pVal);` |
|    ! 0 | 7494 | `	ph7_value_bool(pVal,pProc->running);` |
|    ! 0 | 7495 | `	ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|    ! 0 | 7496 | `	ph7_value_bool(pVal,0);` |
|    ! 0 | 7497 | `	ph7_array_add_strkey_elem(pArray,"signaled",pVal);` |
|    ! 0 | 7498 | `	ph7_array_add_strkey_elem(pArray,"stopped",pVal);` |
|    ! 0 | 7499 | `	ph7_value_int(pVal,pProc->running ? -1 : pProc->exit_code);` |
|    ! 0 | 7500 | `	ph7_array_add_strkey_elem(pArray,"exitcode",pVal);` |
|    ! 0 | 7501 | `	ph7_value_int(pVal,0);` |
|    ! 0 | 7502 | `	ph7_array_add_strkey_elem(pArray,"termsig",pVal);` |
|    ! 0 | 7503 | `	ph7_array_add_strkey_elem(pArray,"stopsig",pVal);` |
|    ! 0 | 7504 | `	ph7_context_release_value(pCtx,pVal);` |
|    ! 0 | 7505 | `	ph7_result_value(pCtx,pArray);` |
|    ! 0 | 7506 | `	return PH7_OK;` |
|    ! 0 | 7507 | `}` |
|      - | 7508 | `#else /* !__UNIXES__ */` |
|      - | 7509 | `static int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7510 | `{` |
|      - | 7511 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 7512 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() is not available on this platform");` |
|    ! 0 | 7513 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 7514 | `	return PH7_OK;` |
|    ! 0 | 7515 | `}` |
|      - | 7516 | `static int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7517 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_int(pCtx,-1); return PH7_OK; }` |
|      - | 7518 | `static int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7519 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|      - | 7520 | `static int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7521 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|      - | 7522 | `#endif /* __UNIXES__ */` |
|      - | 7523 | `/* Export the php:// stream */` |
|      - | 7524 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 7525 | `	"php",` |
|      - | 7526 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7527 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 7528 | `	0,   /* xOpenDir */` |
|      - | 7529 | `	PHPStreamData_Close, /* xClose */` |
|      - | 7530 | `	0,  /* xCloseDir */` |
|      - | 7531 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 7532 | `	0,  /* xReadDir */` |
|      - | 7533 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 7534 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|      - | 7535 | `	0,  /* xLock */` |
|      - | 7536 | `	0,  /* xRewindDir */` |
|      - | 7537 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 7538 | `	PHPStreamData_Trunc, /* xTrunc */` |
|      - | 7539 | `	0,  /* xSync */` |
|      - | 7540 | `	0   /* xStat */` |
|      - | 7541 | `};` |
|      - | 7542 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7543 | `/*` |
|      - | 7544 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 7545 | ` * FALSE otherwise.` |
|      - | 7546 | ` */` |
|    224 | 7547 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      3 | 7548 | `{` |
|      - | 7549 | `#ifndef PH7_DISABLE_DISK_IO` |
|    227 | 7550 | `	return pStream == &sPHP_Stream;` |
|      - | 7551 | `#else` |
|      - | 7552 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7553 | `	return 0;` |
|      - | 7554 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      3 | 7555 | `}` |
|      - | 7556 | `/*` |
|      - | 7557 | ` * Return TRUE if we are dealing with the data:// stream.` |
|      - | 7558 | ` */` |
|    204 | 7559 | `static int is_data_stream(const ph7_io_stream *pStream)` |
|      3 | 7560 | `{` |
|      - | 7561 | `#ifndef PH7_DISABLE_DISK_IO` |
|    207 | 7562 | `	return pStream == &sDATA_Stream;` |
|      - | 7563 | `#else` |
|      - | 7564 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7565 | `	return 0;` |
|      - | 7566 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      3 | 7567 | `}` |
|      - | 7568 | `/*` |
|      - | 7569 | ` * bool stream_isatty(resource $stream)` |
|      - | 7570 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|      - | 7571 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|      - | 7572 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|      - | 7573 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|      - | 7574 | ` */` |
|      6 | 7575 | `static int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7576 | `{` |
|      7 | 7577 | `	int bTty = 0;` |
|      7 | 7578 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7579 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7580 | `		return PH7_OK;` |
|      - | 7581 | `	}` |
|      - | 7582 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7583 | `	{` |
|      7 | 7584 | `		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      7 | 7585 | `		if( !IO_PRIVATE_INVALID(pDev) && is_php_stream(pDev->pStream) ){` |
|      5 | 7586 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|      6 | 7587 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|      4 | 7588 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|      3 | 7589 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|      - | 7590 | `#ifdef __WINNT__` |
|      1 | 7591 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|      - | 7592 | `#else` |
|      4 | 7593 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|      - | 7594 | `#endif` |
|      2 | 7595 | `			}` |
|      2 | 7596 | `		}` |
|      - | 7597 | `	}` |
|      - | 7598 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      7 | 7599 | `	ph7_result_bool(pCtx,bTty);` |
|      7 | 7600 | `	return PH7_OK;` |
|      4 | 7601 | `}` |
|      - | 7602 |  |
|      - | 7603 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7604 | `/*` |
|      - | 7605 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 7606 | ` * [i.e: file://,php://].` |
|      - | 7607 | ` * Note:` |
|      - | 7608 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 7609 | ` *  defined then this function is a no-op.` |
|      - | 7610 | ` */` |
|   3414 | 7611 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 7612 | `{` |
|      - | 7613 | `	/*` |
|      - | 7614 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 7615 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 7616 | `	 */` |
|      - | 7617 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7618 | `	/* VFS: disk I/O related functions */` |
|      - | 7619 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 7620 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 7621 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 7622 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 7623 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 7624 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 7625 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 7626 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 7627 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 7628 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 7629 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 7630 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 7631 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 7632 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 7633 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 7634 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 7635 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 7636 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 7637 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 7638 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 7639 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 7640 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 7641 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 7642 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 7643 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 7644 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 7645 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 7646 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 7647 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 7648 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 7649 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 7650 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 7651 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 7652 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 7653 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 7654 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 7655 | `		{"link",        PH7_vfs_link     },` |
|      - | 7656 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 7657 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 7658 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 7659 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 7660 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 7661 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 7662 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 7663 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 7664 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 7665 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 7666 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 7667 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 7668 | `	};` |
|      - | 7669 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 7670 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 7671 | `	 */` |
|      - | 7672 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 7673 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 7674 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 7675 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 7676 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 7677 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 7678 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 7679 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 7680 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 7681 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 7682 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 7683 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 7684 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 7685 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 7686 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 7687 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 7688 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 7689 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 7690 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 7691 | `		{"file",      PH7_builtin_file   },` |
|      - | 7692 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 7693 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 7694 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 7695 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 7696 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 7697 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 7698 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 7699 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 7700 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 7701 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 7702 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 7703 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 7704 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 7705 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 7706 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 7707 | `#ifdef PH7_ENABLE_NET` |
|      - | 7708 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 7709 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 7710 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 7711 | `#endif` |
|      - | 7712 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 7713 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 7714 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 7715 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 7716 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 7717 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 7718 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 7719 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 7720 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 7721 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 7722 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 7723 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 7724 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 7725 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7726 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 7727 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 7728 | `	};` |
|   3419 | 7729 | `	const ph7_io_stream *pFileStream = 0;` |
|   3419 | 7730 | `	sxu32 n = 0;` |
|      - | 7731 | `	/* Register disk-related functions */` |
| 167291 | 7732 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 163877 | 7733 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  81941 | 7734 | `	}` |
| 177533 | 7735 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 174119 | 7736 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  87062 | 7737 | `	}` |
|      - | 7738 | `#else` |
|      - | 7739 | `	SXUNUSED(pVm);` |
|      - | 7740 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7741 |  |
|      - | 7742 | `	/*` |
|      - | 7743 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7744 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 7745 | `	 */` |
|      - | 7746 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7747 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 7748 | `		/* Path processing */` |
|      - | 7749 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 7750 | `		{"basename",    PH7_builtin_basename },` |
|      - | 7751 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 7752 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 7753 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 7754 | `		/* ZIP processing */` |
|      - | 7755 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 7756 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 7757 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 7758 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 7759 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 7760 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 7761 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 7762 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 7763 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 7764 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 7765 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 7766 | `	};` |
|  58043 | 7767 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  54629 | 7768 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  27317 | 7769 | `	}` |
|      - | 7770 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7771 |  |
|      - | 7772 | `	/* Install streams if disk I/O is enabled */` |
|      - | 7773 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7774 | `#ifdef __WINNT__` |
|      5 | 7775 | `	pFileStream = &sWinFileStream;` |
|      - | 7776 | `#elif defined(__UNIXES__)` |
|   3414 | 7777 | `	pFileStream = &sUnixFileStream;` |
|      - | 7778 | `#endif` |
|      - | 7779 | `	/* Install the php:// stream */` |
|   3419 | 7780 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3419 | 7781 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 7782 | `#ifdef PH7_ENABLE_NET` |
|   3419 | 7783 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 7784 | `#endif` |
|   3419 | 7785 | `	if( pFileStream ){` |
|      - | 7786 | `		/* Install the file:// stream */` |
|   3419 | 7787 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1707 | 7788 | `	}` |
|      - | 7789 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7790 |  |
|   3419 | 7791 | `	return SXRET_OK;` |
|      5 | 7792 | `}` |
|      - | 7793 | `/*` |
|      - | 7794 | ` * Export the STDIN handle.` |
|      - | 7795 | ` */` |
|      2 | 7796 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 7797 | `{` |
|      - | 7798 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7799 | `	if( pVm->pStdin == 0  ){` |
|      - | 7800 | `		io_private *pIn;` |
|      - | 7801 | `		/* Allocate an IO private instance */` |
|      3 | 7802 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7803 | `		if( pIn == 0 ){` |
|    ! 0 | 7804 | `			return 0;` |
|      - | 7805 | `		}` |
|      3 | 7806 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 7807 | `		/* Initialize the handle */` |
|      3 | 7808 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 7809 | `		/* Install the STDIN stream */` |
|      3 | 7810 | `		pVm->pStdin = pIn;` |
|      3 | 7811 | `		return pIn;` |
|    ! 0 | 7812 | `	}else{` |
|      - | 7813 | `		/* NULL or STDIN */` |
|    ! 0 | 7814 | `		return pVm->pStdin;` |
|      - | 7815 | `	}` |
|      - | 7816 | `#else` |
|      - | 7817 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7818 | `	return 0;` |
|      - | 7819 | `#endif` |
|      2 | 7820 | `}` |
|      - | 7821 | `/*` |
|      - | 7822 | ` * Export the STDOUT handle.` |
|      - | 7823 | ` */` |
|      8 | 7824 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 7825 | `{` |
|      - | 7826 | `#ifndef PH7_DISABLE_DISK_IO` |
|      9 | 7827 | `	if( pVm->pStdout == 0  ){` |
|      - | 7828 | `		io_private *pOut;` |
|      - | 7829 | `		/* Allocate an IO private instance */` |
|      7 | 7830 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      7 | 7831 | `		if( pOut == 0 ){` |
|    ! 0 | 7832 | `			return 0;` |
|      - | 7833 | `		}` |
|      7 | 7834 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 7835 | `		/* Initialize the handle */` |
|      7 | 7836 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 7837 | `		/* Install the STDOUT stream */` |
|      7 | 7838 | `		pVm->pStdout = pOut;` |
|      7 | 7839 | `		return pOut;` |
|    ! 0 | 7840 | `	}else{` |
|      - | 7841 | `		/* NULL or STDOUT */` |
|      3 | 7842 | `		return pVm->pStdout;` |
|      - | 7843 | `	}` |
|      - | 7844 | `#else` |
|      - | 7845 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7846 | `	return 0;` |
|      - | 7847 | `#endif` |
|      5 | 7848 | `}` |
|      - | 7849 | `/*` |
|      - | 7850 | ` * Export the STDERR handle.` |
|      - | 7851 | ` */` |
|     10 | 7852 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 7853 | `{` |
|      - | 7854 | `#ifndef PH7_DISABLE_DISK_IO` |
|     11 | 7855 | `	if( pVm->pStderr == 0  ){` |
|      - | 7856 | `		io_private *pErr;` |
|      - | 7857 | `		/* Allocate an IO private instance */` |
|      9 | 7858 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      9 | 7859 | `		if( pErr == 0 ){` |
|    ! 0 | 7860 | `			return 0;` |
|      - | 7861 | `		}` |
|      9 | 7862 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 7863 | `		/* Initialize the handle */` |
|      9 | 7864 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 7865 | `		/* Install the STDERR stream */` |
|      9 | 7866 | `		pVm->pStderr = pErr;` |
|      9 | 7867 | `		return pErr;` |
|    ! 0 | 7868 | `	}else{` |
|      - | 7869 | `		/* NULL or STDERR */` |
|      3 | 7870 | `		return pVm->pStderr;` |
|      - | 7871 | `	}` |
|      - | 7872 | `#else` |
|      - | 7873 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7874 | `	return 0;` |
|      - | 7875 | `#endif` |
|      6 | 7876 | `}` |
|      - | 7877 |  |
