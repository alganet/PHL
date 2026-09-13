# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2625/3905 lines (67.22%)

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
|     70 |   24 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      5 |   25 | `{` |
|     75 |   26 | `	const char *zEnd = &zPath[nByte - 1];` |
|      - |   27 | `	int c,d;` |
|     75 |   28 | `	c = d = '/';` |
|      - |   29 | `#ifdef __WINNT__` |
|      5 |   30 | `	d = '\\';` |
|      - |   31 | `#endif` |
|   1702 |   32 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   1601 |   33 | `		zEnd--;` |
|      5 |   34 | `	}` |
|     75 |   35 | `	*pLen = (int)(zEnd-zPath);` |
|      - |   36 | `#ifdef __WINNT__` |
|      5 |   37 | `	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){` |
|      - |   38 | `		/* Normalize path on windows */` |
|    ! 0 |   39 | `		return "\\";` |
|      - |   40 | `	}` |
|      - |   41 | `#endif` |
|     75 |   42 | `	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){` |
|      - |   43 | `		/* No separator,return "." as the current directory */` |
|      8 |   44 | `		*pLen = sizeof(char);` |
|      8 |   45 | `		return ".";` |
|      - |   46 | `	}` |
|     69 |   47 | `	if( (*pLen) == 0 ){` |
|      2 |   48 | `		*pLen = sizeof(char);` |
|      - |   49 | `#ifdef __WINNT__` |
|    ! 0 |   50 | `		return "\\";` |
|      - |   51 | `#else` |
|      2 |   52 | `		return "/";` |
|      - |   53 | `#endif` |
|      - |   54 | `	}` |
|     67 |   55 | `	return zPath;` |
|     40 |   56 | `}` |
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
|  20024 |   68 | `static const char * VfsStrerror(int iErr)` |
|      5 |   69 | `{` |
|      - |   70 | `#if defined(_MSC_VER)` |
|      - |   71 | `#pragma warning(push)` |
|      - |   72 | `#pragma warning(disable:4996)` |
|      - |   73 | `#endif` |
|  20029 |   74 | `	return strerror(iErr);` |
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
|  20002 |   85 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |   86 | `{` |
|  30008 |   87 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  20002 |   88 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  20007 |   89 | `}` |
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
|  13460 |  104 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  105 | `{` |
|      - |  106 | `	const char *zPath;` |
|      - |  107 | `	ph7_vfs *pVfs;` |
|      - |  108 | `	int rc;` |
|  13465 |  109 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  110 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |  111 | `		ph7_result_bool(pCtx,0);` |
|      6 |  112 | `		return PH7_OK;` |
|      - |  113 | `	}` |
|      - |  114 | `	/* Point to the underlying vfs */` |
|  13461 |  115 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13461 |  116 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  117 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  118 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  119 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  120 | `			ph7_function_name(pCtx)` |
|      - |  121 | `			);` |
|    ! 0 |  122 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  123 | `		return PH7_OK;` |
|      - |  124 | `	}` |
|      - |  125 | `	/* Point to the desired directory */` |
|  13461 |  126 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  127 | `	/* Perform the requested operation */` |
|  13461 |  128 | `	errno = 0;` |
|  13461 |  129 | `	rc = pVfs->xChdir(zPath);` |
|  13461 |  130 | `	if( rc != PH7_OK ){` |
|      - |  131 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      4 |  132 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      2 |  133 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      1 |  134 | `	}` |
|      - |  135 | `	/* IO return value */` |
|  13461 |  136 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13461 |  137 | `	return PH7_OK;` |
|   6735 |  138 | `}` |
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
|     46 |  220 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  221 | `{` |
|      - |  222 | `	const char *zPath;` |
|      - |  223 | `	ph7_vfs *pVfs;` |
|      - |  224 | `	int rc;` |
|     49 |  225 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  226 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  227 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  228 | `		return PH7_OK;` |
|      - |  229 | `	}` |
|      - |  230 | `	/* Point to the underlying vfs */` |
|     49 |  231 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     49 |  232 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  233 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  234 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  235 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  236 | `			ph7_function_name(pCtx)` |
|      - |  237 | `			);` |
|    ! 0 |  238 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  239 | `		return PH7_OK;` |
|      - |  240 | `	}` |
|      - |  241 | `	/* Point to the desired directory */` |
|     49 |  242 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  243 | `	/* Perform the requested operation */` |
|     49 |  244 | `	errno = 0;` |
|     49 |  245 | `	rc = pVfs->xRmdir(zPath);` |
|     49 |  246 | `	if( rc != PH7_OK ){` |
|      8 |  247 | `		VfsThrowSysWarning(pCtx,zPath);` |
|      3 |  248 | `	}` |
|      - |  249 | `	/* IO return value */` |
|     49 |  250 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     49 |  251 | `	return PH7_OK;` |
|     26 |  252 | `}` |
|      - |  253 | `/*` |
|      - |  254 | ` * bool is_dir(string $filename)` |
|      - |  255 | ` *  Tells whether the given filename is a directory.` |
|      - |  256 | ` * Parameters` |
|      - |  257 | ` *  $filename` |
|      - |  258 | ` *   Path to the file.` |
|      - |  259 | ` * Return` |
|      - |  260 | ` *  TRUE on success or FALSE on failure.` |
|      - |  261 | ` */` |
|   8832 |  262 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  263 | `{` |
|      - |  264 | `	const char *zPath;` |
|      - |  265 | `	ph7_vfs *pVfs;` |
|      - |  266 | `	int rc;` |
|   8837 |  267 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  268 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  269 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  270 | `		return PH7_OK;` |
|      - |  271 | `	}` |
|      - |  272 | `	/* Point to the underlying vfs */` |
|   8837 |  273 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8837 |  274 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  275 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  276 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  277 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  278 | `			ph7_function_name(pCtx)` |
|      - |  279 | `			);` |
|    ! 0 |  280 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  281 | `		return PH7_OK;` |
|      - |  282 | `	}` |
|      - |  283 | `	/* Point to the desired directory */` |
|   8837 |  284 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  285 | `	/* Perform the requested operation */` |
|   8837 |  286 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  287 | `	/* IO return value */` |
|   8837 |  288 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8837 |  289 | `	return PH7_OK;` |
|   4421 |  290 | `}` |
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
|     46 |  310 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  311 | `{` |
|     50 |  312 | `	int iRecursive = 0;` |
|      - |  313 | `	const char *zPath;` |
|      - |  314 | `	ph7_vfs *pVfs;` |
|      - |  315 | `	int iMode,rc;` |
|     50 |  316 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  317 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  318 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  319 | `		return PH7_OK;` |
|      - |  320 | `	}` |
|      - |  321 | `	/* Point to the underlying vfs */` |
|     50 |  322 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     50 |  323 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  324 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  325 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  326 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  327 | `			ph7_function_name(pCtx)` |
|      - |  328 | `			);` |
|    ! 0 |  329 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  330 | `		return PH7_OK;` |
|      - |  331 | `	}` |
|      - |  332 | `	/* Point to the desired directory */` |
|     50 |  333 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  334 | `#ifdef __WINNT__` |
|      4 |  335 | `	iMode = 0;` |
|      - |  336 | `#else` |
|      - |  337 | `	/* Assume UNIX */` |
|     46 |  338 | `	iMode = 0777;` |
|      - |  339 | `#endif` |
|     50 |  340 | `	if( nArg > 1 ){` |
|    ! 0 |  341 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  342 | `		if( nArg > 2 ){` |
|    ! 0 |  343 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  344 | `		}` |
|    ! 0 |  345 | `	}` |
|      - |  346 | `	/* Perform the requested operation */` |
|     27 |  347 | `	errno = 0;` |
|     27 |  348 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|     27 |  349 | `	if( rc != PH7_OK ){` |
|      - |  350 | `		/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|    ! 0 |  351 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|    ! 0 |  352 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|    ! 0 |  353 | `	}` |
|      - |  354 | `	/* IO return value */` |
|     50 |  355 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     50 |  356 | `	return PH7_OK;` |
|     27 |  357 | `}` |
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
|  33734 |  537 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  538 | `{` |
|      - |  539 | `	const char *zPath;` |
|      - |  540 | `	ph7_vfs *pVfs;` |
|      - |  541 | `	int rc;` |
|  33739 |  542 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  543 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  544 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  545 | `		return PH7_OK;` |
|      - |  546 | `	}` |
|      - |  547 | `	/* Point to the underlying vfs */` |
|  33739 |  548 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  33739 |  549 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  550 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  551 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  552 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  553 | `			ph7_function_name(pCtx)` |
|      - |  554 | `			);` |
|    ! 0 |  555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  556 | `		return PH7_OK;` |
|      - |  557 | `	}` |
|      - |  558 | `	/* Point to the desired directory */` |
|  33739 |  559 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  560 | `	/* Perform the requested operation */` |
|  33739 |  561 | `	errno = 0;` |
|  33739 |  562 | `	rc = pVfs->xUnlink(zPath);` |
|  33739 |  563 | `	if( rc != PH7_OK ){` |
|  20001 |  564 | `		VfsThrowSysWarning(pCtx,zPath);` |
|   9998 |  565 | `	}` |
|      - |  566 | `	/* IO return value */` |
|  33739 |  567 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  33739 |  568 | `	return PH7_OK;` |
|  16872 |  569 | `}` |
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
|      3 |  582 | `{` |
|      - |  583 | `	const char *zPath;` |
|      - |  584 | `	ph7_vfs *pVfs;` |
|      - |  585 | `	int iMode;` |
|      - |  586 | `	int rc;` |
|    151 |  587 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  588 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  590 | `		return PH7_OK;` |
|      - |  591 | `	}` |
|      - |  592 | `	/* Point to the underlying vfs */` |
|    151 |  593 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    151 |  594 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  595 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  596 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  597 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  598 | `			ph7_function_name(pCtx)` |
|      - |  599 | `			);` |
|    ! 0 |  600 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  601 | `		return PH7_OK;` |
|      - |  602 | `	}` |
|      - |  603 | `	/* Point to the desired directory */` |
|    151 |  604 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  605 | `	/* Extract the mode */` |
|    151 |  606 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  607 | `	/* Perform the requested operation */` |
|    151 |  608 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  609 | `	/* IO return value */` |
|    151 |  610 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    151 |  611 | `	return PH7_OK;` |
|     77 |  612 | `}` |
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
|      3 |  809 | `{` |
|      - |  810 | `	const char *zPath;` |
|      - |  811 | `	ph7_vfs *pVfs;` |
|      - |  812 | `	int rc;` |
|    185 |  813 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  814 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  815 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  816 | `		return PH7_OK;` |
|      - |  817 | `	}` |
|      - |  818 | `	/* Point to the underlying vfs */` |
|    185 |  819 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    185 |  820 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  821 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  822 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  823 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  824 | `			ph7_function_name(pCtx)` |
|      - |  825 | `			);` |
|    ! 0 |  826 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  827 | `		return PH7_OK;` |
|      - |  828 | `	}` |
|      - |  829 | `	/* Point to the desired directory */` |
|    185 |  830 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  831 | `	/* Perform the requested operation */` |
|    185 |  832 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  833 | `	/* IO return value */` |
|    185 |  834 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    185 |  835 | `	return PH7_OK;` |
|     94 |  836 | `}` |
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
|   6768 | 1006 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1007 | `{` |
|      - | 1008 | `	const char *zPath;` |
|      - | 1009 | `	ph7_vfs *pVfs;` |
|      - | 1010 | `	int rc;` |
|   6773 | 1011 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1012 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1013 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1014 | `		return PH7_OK;` |
|      - | 1015 | `	}` |
|      - | 1016 | `	/* Point to the underlying vfs */` |
|   6773 | 1017 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6773 | 1018 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1019 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1020 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1021 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1022 | `			ph7_function_name(pCtx)` |
|      - | 1023 | `			);` |
|    ! 0 | 1024 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1025 | `		return PH7_OK;` |
|      - | 1026 | `	}` |
|      - | 1027 | `	/* Point to the desired directory */` |
|   6773 | 1028 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1029 | `	/* Perform the requested operation */` |
|   6773 | 1030 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1031 | `	/* IO return value */` |
|   6773 | 1032 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6773 | 1033 | `	return PH7_OK;` |
|   3389 | 1034 | `}` |
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
|  13414 | 1659 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1660 | `{` |
|  13419 | 1661 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1662 | `	SyString *pCur;` |
|      - | 1663 | `	int c,d;` |
|  13419 | 1664 | `	c = d = '/';` |
|      - | 1665 | `#ifdef __WINNT__` |
|      5 | 1666 | `	d = '\\';` |
|      - | 1667 | `#endif` |
|      - | 1668 | `	/* Zero the structure */` |
|  13419 | 1669 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1670 | `	/* Handle special case */` |
|  13419 | 1671 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1672 | `#ifdef __WINNT__` |
|    ! 0 | 1673 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1674 | `#else` |
|    ! 0 | 1675 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1676 | `#endif` |
|    ! 0 | 1677 | `		return SXRET_OK;` |
|      - | 1678 | `	}` |
|      - | 1679 | `	/* Extract the basename */` |
| 362038 | 1680 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 341917 | 1681 | `		zEnd--;` |
|      5 | 1682 | `	}` |
|  13419 | 1683 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13419 | 1684 | `	zEnd = &zPath[nByte];` |
|      - | 1685 | `	/* dirname */` |
|  13419 | 1686 | `	pCur = &pOut->sDir;` |
|  13419 | 1687 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13419 | 1688 | `	if( pCur->nByte > 1 ){` |
|  26833 | 1689 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1690 | `#ifdef __WINNT__` |
|      5 | 1691 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1692 | `#endif` |
|   6712 | 1693 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1694 | `#ifdef __WINNT__` |
|    ! 0 | 1695 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1696 | `#else` |
|    ! 0 | 1697 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1698 | `#endif` |
|    ! 0 | 1699 | `	}` |
|      - | 1700 | `	/* basename/filename */` |
|  13419 | 1701 | `	pCur = &pOut->sBasename;` |
|  13419 | 1702 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13419 | 1703 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1704 | `#ifdef __WINNT__` |
|      5 | 1705 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1706 | `#endif` |
|  13419 | 1707 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13419 | 1708 | `	if( pCur->nByte > 0 ){` |
|      - | 1709 | `		/* extension */` |
|  13419 | 1710 | `		zEnd--;` |
|  67069 | 1711 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  53655 | 1712 | `			zEnd--;` |
|      5 | 1713 | `		}` |
|  13419 | 1714 | `		if( zEnd > pCur->zString ){` |
|  13417 | 1715 | `			zEnd++; /* Jump leading dot */` |
|  13417 | 1716 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1717 | `			/* Fix filename */` |
|  13417 | 1718 | `			pCur = &pOut->sFilename;` |
|  13417 | 1719 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13417 | 1720 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6706 | 1721 | `			}` |
|   6706 | 1722 | `		}` |
|   6707 | 1723 | `	}` |
|  13419 | 1724 | `	return SXRET_OK;` |
|   6712 | 1725 | `}` |
|      - | 1726 | `/*` |
|      - | 1727 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1728 | ` *  See block comment above.` |
|      - | 1729 | ` */` |
|  13414 | 1730 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1731 | `{` |
|      - | 1732 | `	const char *zPath;` |
|      - | 1733 | `	path_info sInfo;` |
|      - | 1734 | `	SyString *pComp;` |
|      - | 1735 | `	int iLen;` |
|  13419 | 1736 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1737 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1738 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1739 | `		return PH7_OK;` |
|      - | 1740 | `	}` |
|      - | 1741 | `	/* Point to the target path */` |
|  13419 | 1742 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13419 | 1743 | `	if( iLen < 1 ){` |
|      - | 1744 | `		/* Empty string */` |
|    ! 0 | 1745 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1746 | `		return PH7_OK;` |
|      - | 1747 | `	}` |
|      - | 1748 | `	/* Extract path info */` |
|  13419 | 1749 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  20125 | 1750 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1751 | `		/* Return path component */` |
|  13417 | 1752 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13417 | 1753 | `		switch(nComp){` |
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
|   3354 | 1772 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6713 | 1773 | `			pComp = &sInfo.sExtension;` |
|   6713 | 1774 | `			if( pComp->nByte > 0 ){` |
|   6711 | 1775 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3358 | 1776 | `			}else{` |
|      - | 1777 | `				/* Expand the empty string */` |
|      3 | 1778 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1779 | `			}` |
|   6713 | 1780 | `			break;` |
|   3350 | 1781 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6705 | 1782 | `			pComp = &sInfo.sFilename;` |
|   6705 | 1783 | `			if( pComp->nByte > 0 ){` |
|   6705 | 1784 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3355 | 1785 | `			}else{` |
|      - | 1786 | `				/* Expand the empty string */` |
|    ! 0 | 1787 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1788 | `			}` |
|   6705 | 1789 | `			break;` |
|    ! 0 | 1790 | `		default:` |
|      - | 1791 | `			/* Expand the empty string */` |
|    ! 0 | 1792 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1793 | `			break;` |
|      - | 1794 | `		}` |
|   6711 | 1795 | `	}else{` |
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
|  13419 | 1845 | `	return PH7_OK;` |
|   6712 | 1846 | `}` |
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
|      5 | 2245 | `{` |
|      - | 2246 | `	ph7_vfs *pVfs;` |
|      - | 2247 | `	/* Set the empty string as the default return value */` |
|    247 | 2248 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2249 | `	/* Point to the underlying vfs */` |
|    247 | 2250 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    247 | 2251 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
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
|    247 | 2262 | `	pVfs->xTempDir(pCtx);` |
|    247 | 2263 | `	return PH7_OK;` |
|    126 | 2264 | `}` |
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
|  10630 | 2848 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2849 | `{` |
|      - | 2850 | `	const ph7_io_stream *pStream;` |
|      - | 2851 | `	io_private *pDev;` |
|      - | 2852 | `	int rc;` |
|  10635 | 2853 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2854 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2855 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2856 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2857 | `		return PH7_OK;` |
|      - | 2858 | `	}` |
|      - | 2859 | `	/* Extract our private data */` |
|  10635 | 2860 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2861 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10635 | 2862 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2863 | `		/*Expecting an IO handle */` |
|    ! 0 | 2864 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2865 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2866 | `		return PH7_OK;` |
|      - | 2867 | `	}` |
|      - | 2868 | `	/* Point to the target IO stream device */` |
|  10635 | 2869 | `	pStream = pDev->pStream;` |
|  10635 | 2870 | `	if( pStream == 0 ){` |
|    ! 0 | 2871 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2872 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2873 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2874 | `			);` |
|    ! 0 | 2875 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2876 | `		return PH7_OK;` |
|      - | 2877 | `	}` |
|  10635 | 2878 | `	rc = SXERR_EOF;` |
|      - | 2879 | `	/* Perform the requested operation */` |
|  10635 | 2880 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2881 | `		/* Data is available */` |
|   4869 | 2882 | `		rc = PH7_OK;` |
|   2437 | 2883 | `	}else{` |
|      - | 2884 | `		char zBuf[4096];` |
|      - | 2885 | `		ph7_int64 n;` |
|      - | 2886 | `		/* Perform a buffered read */` |
|   5771 | 2887 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5771 | 2888 | `		if( n > 0 ){` |
|      - | 2889 | `			/* Copy buffered data */` |
|   1885 | 2890 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1885 | 2891 | `			rc = PH7_OK;` |
|    940 | 2892 | `		}` |
|      - | 2893 | `	}` |
|      - | 2894 | `	/* EOF or not */` |
|  10635 | 2895 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10635 | 2896 | `	return PH7_OK;` |
|   5320 | 2897 | `}` |
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
|   6810 | 2943 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 | 2944 | `{` |
|      - | 2945 | `	const char *zIn,*zEnd,*zPtr;` |
|   6815 | 2946 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6815 | 2947 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6815 | 2948 | `	zPtr = zIn;` |
| 458195 | 2949 | `	while( zIn < zEnd ){` |
| 458091 | 2950 | `		if( zIn[0] == '\n' ){` |
|      - | 2951 | `			/* Line found */` |
|   6711 | 2952 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6711 | 2953 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6711 | 2954 | `			*pzLine = zPtr;` |
|   6711 | 2955 | `			return SXRET_OK;` |
|      - | 2956 | `		}` |
| 451385 | 2957 | `		zIn++;` |
|      5 | 2958 | `	}` |
|      - | 2959 | `	/* No line were found */` |
|    109 | 2960 | `	return SXERR_NOTFOUND;` |
|   3410 | 2961 | `}` |
|      - | 2962 | `/*` |
|      - | 2963 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2964 | ` */` |
|   6814 | 2965 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 | 2966 | `{` |
|   6819 | 2967 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2968 | `	char zBuf[8192];` |
|      - | 2969 | `	ph7_int64 n;` |
|      - | 2970 | `	sxi32 rc;` |
|   6819 | 2971 | `	n = 0;` |
|   6819 | 2972 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2973 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     73 | 2974 | `		SyBlobReset(&pDev->sBuffer);` |
|     73 | 2975 | `		pDev->nOfft = 0;` |
|     34 | 2976 | `	}` |
|   6785 | 2977 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2978 | `		/* Check if there is a line */` |
|   6751 | 2979 | `		rc = GetLine(pDev,&n,pzData);` |
|   6751 | 2980 | `		if( rc == SXRET_OK ){` |
|      - | 2981 | `			/* Got line,update the cursor  */` |
|   6651 | 2982 | `			pDev->nOfft += (sxu32)n;` |
|   6651 | 2983 | `			return n;` |
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
|     66 | 2996 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2997 | `		/* Try to extract a line */` |
|     66 | 2998 | `		rc = GetLine(pDev,&n,pzData);` |
|     66 | 2999 | `		if( rc == SXRET_OK ){` |
|      - | 3000 | `			/* Got one,return immediately */` |
|     62 | 3001 | `			pDev->nOfft += (sxu32)n;` |
|     62 | 3002 | `			return n;` |
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
|   3412 | 3023 | `}` |
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
|  30424 | 3045 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 3046 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 | 3047 | `{` |
|  30429 | 3048 | `	void *pHandle = 0; /* cc warning */` |
|      - | 3049 | `	SyString sFile;` |
|      - | 3050 | `	ph7_value sDummy;` |
|      - | 3051 | `	int rc;` |
|  30429 | 3052 | `	if( pStream == 0 ){` |
|      - | 3053 | `		/* No such stream device */` |
|    ! 0 | 3054 | `		return 0;` |
|      - | 3055 | `	}` |
|  30429 | 3056 | `	if( pResource == 0 ){` |
|      - | 3057 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - | 3058 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - | 3059 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - | 3060 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - | 3061 | `		 * reads it during the call, and file:// ignores it. */` |
|  30407 | 3062 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  30407 | 3063 | `		pResource = &sDummy;` |
|  15201 | 3064 | `	}` |
|  30429 | 3065 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  30429 | 3066 | `	if( use_include ){` |
|   9696 | 3067 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 3068 | `#ifdef __WINNT__` |
|      - | 3069 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 3070 | `#endif` |
|   9660 | 3071 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9654 | 3072 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 3073 | `				/*  Open the file directly */` |
|     46 | 3074 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     46 | 3075 | `				if( rc == PH7_OK && bPushInclude ){` |
|      - | 3076 | `					/* Mark as included */` |
|     46 | 3077 | `					PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|     21 | 3078 | `				}` |
|     25 | 3079 | `		}else{` |
|      - | 3080 | `			SyString *pPath;` |
|      - | 3081 | `			SyBlob sWorker;` |
|      - | 3082 | `#ifdef __WINNT__` |
|      - | 3083 | `			static const int c = '\\';` |
|      - | 3084 | `#else` |
|      - | 3085 | `			static const int c = '/';` |
|      - | 3086 | `#endif` |
|      - | 3087 | `			/* Init the path builder working buffer */` |
|   9656 | 3088 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 3089 | `			/* Build a path from the set of include path */` |
|   9656 | 3090 | `			SySetResetCursor(&pVm->aPaths);` |
|   9656 | 3091 | `			rc = SXERR_IO;` |
|   9662 | 3092 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 3093 | `				/* Build full path */` |
|   9656 | 3094 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 3095 | `				/* Append null terminator */` |
|   9656 | 3096 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 3097 | `					continue;` |
|      - | 3098 | `				}` |
|      - | 3099 | `				/* Try to open the file */` |
|   9656 | 3100 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9656 | 3101 | `				if( rc == PH7_OK ){` |
|   9650 | 3102 | `					if( bPushInclude ){` |
|      - | 3103 | `						/* Mark as included */` |
|   9650 | 3104 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4824 | 3105 | `					}` |
|   9650 | 3106 | `					break;` |
|      - | 3107 | `				}` |
|      - | 3108 | `				/* Reset the working buffer */` |
|      8 | 3109 | `				SyBlobReset(&sWorker);` |
|      - | 3110 | `				/* Check the next path */` |
|      2 | 3111 | `			}` |
|   9656 | 3112 | `			SyBlobRelease(&sWorker);` |
|      - | 3113 | `		}` |
|   4852 | 3114 | `	}else{` |
|      - | 3115 | `		/* Open the URI direcly */` |
|  20733 | 3116 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 3117 | `	}` |
|  30429 | 3118 | `	if( rc != PH7_OK ){` |
|      - | 3119 | `		/* IO error */` |
|     24 | 3120 | `		return 0;` |
|      - | 3121 | `	}` |
|      - | 3122 | `	/* Return the file handle */` |
|  30409 | 3123 | `	return pHandle;` |
|  15217 | 3124 | `}` |
|      - | 3125 | `/*` |
|      - | 3126 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 3127 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 3128 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 3129 | ` */` |
|   9680 | 3130 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      4 | 3131 | `{` |
|      - | 3132 | `	ph7_int64 nRead;` |
|      - | 3133 | `	char zBuf[8192]; /* 8K */` |
|      - | 3134 | `	int rc;` |
|      - | 3135 | `	/* Perform the requested operation */` |
|   9680 | 3136 | `	for(;;){` |
|  19364 | 3137 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  19364 | 3138 | `		if( nRead < 1 ){` |
|      - | 3139 | `			/* EOF or IO error */` |
|   9684 | 3140 | `			break;` |
|      - | 3141 | `		}` |
|      - | 3142 | `		/* Append contents */` |
|   9684 | 3143 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9684 | 3144 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3145 | `			break;` |
|      - | 3146 | `		}` |
|      4 | 3147 | `	}` |
|   9684 | 3148 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      4 | 3149 | `}` |
|      - | 3150 | `/*` |
|      - | 3151 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 3152 | ` */` |
|  30532 | 3153 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 3154 | `{` |
|  30537 | 3155 | `	if( pStream->xClose ){` |
|  30537 | 3156 | `		pStream->xClose(pHandle);` |
|  15266 | 3157 | `	}` |
|  30537 | 3158 | `}` |
|      - | 3159 | `/*` |
|      - | 3160 | ` * string fgetc(resource $handle)` |
|      - | 3161 | ` *  Gets a character from the given file pointer.` |
|      - | 3162 | ` * Parameters` |
|      - | 3163 | ` *  $handle` |
|      - | 3164 | ` *   The file pointer.` |
|      - | 3165 | ` * Return` |
|      - | 3166 | ` *  Returns a string containing a single character read from the file` |
|      - | 3167 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3168 | ` * WARNING` |
|      - | 3169 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3170 | ` */` |
|      4 | 3171 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3172 | `{` |
|      - | 3173 | `	const ph7_io_stream *pStream;` |
|      - | 3174 | `	io_private *pDev;` |
|      - | 3175 | `	int c,n;` |
|      5 | 3176 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3177 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3178 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3179 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3180 | `		return PH7_OK;` |
|      - | 3181 | `	}` |
|      - | 3182 | `	/* Extract our private data */` |
|      5 | 3183 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3184 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3185 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3186 | `		/*Expecting an IO handle */` |
|    ! 0 | 3187 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3189 | `		return PH7_OK;` |
|      - | 3190 | `	}` |
|      - | 3191 | `	/* Point to the target IO stream device */` |
|      5 | 3192 | `	pStream = pDev->pStream;` |
|      5 | 3193 | `	if( pStream == 0  ){` |
|    ! 0 | 3194 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3195 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3196 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3197 | `			);` |
|    ! 0 | 3198 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3199 | `		return PH7_OK;` |
|      - | 3200 | `	}` |
|      - | 3201 | `	/* Perform the requested operation */` |
|      5 | 3202 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3203 | `	/* IO result */` |
|      5 | 3204 | `	if( n < 1 ){` |
|      - | 3205 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3206 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3207 | `	}else{` |
|      - | 3208 | `		/* Return the string holding the character */` |
|      5 | 3209 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3210 | `	}` |
|      5 | 3211 | `	return PH7_OK;` |
|      3 | 3212 | `}` |
|      - | 3213 | `/*` |
|      - | 3214 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3215 | ` *  Gets line from file pointer.` |
|      - | 3216 | ` * Parameters` |
|      - | 3217 | ` *  $handle` |
|      - | 3218 | ` *   The file pointer.` |
|      - | 3219 | ` * $length` |
|      - | 3220 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3221 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3222 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3223 | ` *  the end of the line.` |
|      - | 3224 | ` * Return` |
|      - | 3225 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3226 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3227 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3228 | ` */` |
|   6804 | 3229 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3230 | `{` |
|      - | 3231 | `	const ph7_io_stream *pStream;` |
|      - | 3232 | `	const char *zLine;` |
|      - | 3233 | `	io_private *pDev;` |
|      - | 3234 | `	ph7_int64 n,nLen;` |
|   6809 | 3235 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3236 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3237 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3238 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3239 | `		return PH7_OK;` |
|      - | 3240 | `	}` |
|      - | 3241 | `	/* Extract our private data */` |
|   6809 | 3242 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3243 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6809 | 3244 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3245 | `		/*Expecting an IO handle */` |
|    ! 0 | 3246 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3247 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3248 | `		return PH7_OK;` |
|      - | 3249 | `	}` |
|      - | 3250 | `	/* Point to the target IO stream device */` |
|   6809 | 3251 | `	pStream = pDev->pStream;` |
|   6809 | 3252 | `	if( pStream == 0  ){` |
|    ! 0 | 3253 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3254 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3255 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3256 | `			);` |
|    ! 0 | 3257 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3258 | `		return PH7_OK;` |
|      - | 3259 | `	}` |
|   6809 | 3260 | `	nLen = -1;` |
|   6809 | 3261 | `	if( nArg > 1 ){` |
|      - | 3262 | `		/* Maximum data to read */` |
|    ! 0 | 3263 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3264 | `	}` |
|      - | 3265 | `	/* Perform the requested operation */` |
|   6809 | 3266 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6809 | 3267 | `	if( n < 1 ){` |
|      - | 3268 | `		/* EOF or IO error,return FALSE */` |
|      7 | 3269 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3270 | `	}else{` |
|      - | 3271 | `		/* Return the freshly extracted line */` |
|   6807 | 3272 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3273 | `	}` |
|   6809 | 3274 | `	return PH7_OK;` |
|   3407 | 3275 | `}` |
|      - | 3276 | `/*` |
|      - | 3277 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3278 | ` *  Binary-safe file read.` |
|      - | 3279 | ` * Parameters` |
|      - | 3280 | ` *  $handle` |
|      - | 3281 | ` *   The file pointer.` |
|      - | 3282 | ` * $length` |
|      - | 3283 | ` *  Up to length number of bytes read.` |
|      - | 3284 | ` * Return` |
|      - | 3285 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3286 | ` */` |
|     28 | 3287 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3288 | `{` |
|      - | 3289 | `	const ph7_io_stream *pStream;` |
|      - | 3290 | `	io_private *pDev;` |
|      - | 3291 | `	ph7_int64 nRead;` |
|      - | 3292 | `	void *pBuf;` |
|      - | 3293 | `	int nLen;` |
|     31 | 3294 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3295 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3296 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3297 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3298 | `		return PH7_OK;` |
|      - | 3299 | `	}` |
|      - | 3300 | `	/* Extract our private data */` |
|     31 | 3301 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3302 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     31 | 3303 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3304 | `		/*Expecting an IO handle */` |
|    ! 0 | 3305 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3306 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3307 | `		return PH7_OK;` |
|      - | 3308 | `	}` |
|      - | 3309 | `	/* Point to the target IO stream device */` |
|     31 | 3310 | `	pStream = pDev->pStream;` |
|     31 | 3311 | `	if( pStream == 0  ){` |
|    ! 0 | 3312 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3313 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3314 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3315 | `			);` |
|    ! 0 | 3316 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3317 | `		return PH7_OK;` |
|      - | 3318 | `	}` |
|     31 | 3319 | `        nLen = 4096;` |
|     31 | 3320 | `	if( nArg > 1 ){` |
|     31 | 3321 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     31 | 3322 | `	  if( nLen < 1 ){` |
|      - | 3323 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3324 | `		nLen = 4096;` |
|    ! 0 | 3325 | `	  }` |
|     14 | 3326 | `        }` |
|      - | 3327 | `	/* Allocate enough buffer */` |
|     31 | 3328 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     31 | 3329 | `	if( pBuf == 0 ){` |
|    ! 0 | 3330 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3331 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3332 | `		return PH7_OK;` |
|      - | 3333 | `	}` |
|      - | 3334 | `	/* Perform the requested operation */` |
|     31 | 3335 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     31 | 3336 | `	if( nRead < 1 ){` |
|      - | 3337 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3338 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3339 | `	}else{` |
|      - | 3340 | `		/* Make a copy of the data just read */` |
|     31 | 3341 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3342 | `	}` |
|      - | 3343 | `	/* Release the buffer */` |
|     31 | 3344 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     31 | 3345 | `	return PH7_OK;` |
|     17 | 3346 | `}` |
|      - | 3347 | `/*` |
|      - | 3348 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3349 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3350 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3351 | ` * Parameters` |
|      - | 3352 | ` * $handle` |
|      - | 3353 | ` *   The file pointer.` |
|      - | 3354 | ` * $length` |
|      - | 3355 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3356 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3357 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3358 | ` *  the end of the line.` |
|      - | 3359 | ` * $delimiter` |
|      - | 3360 | ` *   Set the field delimiter (one character only).` |
|      - | 3361 | ` * $enclosure` |
|      - | 3362 | ` *   Set the field enclosure character (one character only).` |
|      - | 3363 | ` * $escape` |
|      - | 3364 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3365 | ` * Return` |
|      - | 3366 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3367 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3368 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3369 | ` */` |
|      2 | 3370 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3371 | `{` |
|      - | 3372 | `	const ph7_io_stream *pStream;` |
|      - | 3373 | `	const char *zLine;` |
|      - | 3374 | `	io_private *pDev;` |
|      - | 3375 | `	ph7_int64 n,nLen;` |
|      3 | 3376 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3377 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3378 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3379 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3380 | `		return PH7_OK;` |
|      - | 3381 | `	}` |
|      - | 3382 | `	/* Extract our private data */` |
|      3 | 3383 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3384 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3385 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3386 | `		/*Expecting an IO handle */` |
|    ! 0 | 3387 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3388 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3389 | `		return PH7_OK;` |
|      - | 3390 | `	}` |
|      - | 3391 | `	/* Point to the target IO stream device */` |
|      3 | 3392 | `	pStream = pDev->pStream;` |
|      3 | 3393 | `	if( pStream == 0  ){` |
|    ! 0 | 3394 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3395 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3396 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3397 | `			);` |
|    ! 0 | 3398 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3399 | `		return PH7_OK;` |
|      - | 3400 | `	}` |
|      3 | 3401 | `	nLen = -1;` |
|      3 | 3402 | `	if( nArg > 1 ){` |
|      - | 3403 | `		/* Maximum data to read */` |
|      3 | 3404 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3405 | `	}` |
|      - | 3406 | `	/* Perform the requested operation */` |
|      3 | 3407 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3408 | `	if( n < 1 ){` |
|      - | 3409 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3410 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3411 | `	}else{` |
|      - | 3412 | `		ph7_value *pArray;` |
|      3 | 3413 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3414 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3415 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3416 | `		if( nArg > 2 ){` |
|      - | 3417 | `			const char *zPtr;` |
|      - | 3418 | `			int i;` |
|      3 | 3419 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3420 | `				/* Extract the delimiter */` |
|      3 | 3421 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3422 | `				if( i > 0 ){` |
|      3 | 3423 | `					delim = zPtr[0];` |
|      1 | 3424 | `				}` |
|      1 | 3425 | `			}` |
|      3 | 3426 | `			if( nArg > 3 ){` |
|      3 | 3427 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3428 | `					/* Extract the enclosure */` |
|      3 | 3429 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3430 | `					if( i > 0 ){` |
|      3 | 3431 | `						encl = zPtr[0];` |
|      1 | 3432 | `					}` |
|      1 | 3433 | `				}` |
|      3 | 3434 | `				if( nArg > 4 ){` |
|      3 | 3435 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3436 | `						/* Extract the escape character */` |
|      3 | 3437 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3438 | `						if( i > 0 ){` |
|      3 | 3439 | `							escape = zPtr[0];` |
|      1 | 3440 | `						}` |
|      1 | 3441 | `					}` |
|      1 | 3442 | `				}` |
|      1 | 3443 | `			}` |
|      1 | 3444 | `		}` |
|      - | 3445 | `		/* Create our array */` |
|      3 | 3446 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3447 | `		if( pArray == 0 ){` |
|    ! 0 | 3448 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3449 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3450 | `			return PH7_OK;` |
|      - | 3451 | `		}` |
|      - | 3452 | `		/* Parse the raw input */` |
|      3 | 3453 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3454 | `		/* Return the freshly created array  */` |
|      3 | 3455 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3456 | `	}` |
|      3 | 3457 | `	return PH7_OK;` |
|      2 | 3458 | `}` |
|      - | 3459 | `/*` |
|      - | 3460 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3461 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3462 | ` * Parameters` |
|      - | 3463 | ` * $handle` |
|      - | 3464 | ` *   The file pointer.` |
|      - | 3465 | ` * $length` |
|      - | 3466 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3467 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3468 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3469 | ` *  the end of the line.` |
|      - | 3470 | ` * $allowable_tags` |
|      - | 3471 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3472 | ` * Return` |
|      - | 3473 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3474 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3475 | ` */` |
|      2 | 3476 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3477 | `{` |
|      - | 3478 | `	const ph7_io_stream *pStream;` |
|      - | 3479 | `	const char *zLine;` |
|      - | 3480 | `	io_private *pDev;` |
|      - | 3481 | `	ph7_int64 n,nLen;` |
|      3 | 3482 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3483 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3484 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3485 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3486 | `		return PH7_OK;` |
|      - | 3487 | `	}` |
|      - | 3488 | `	/* Extract our private data */` |
|      3 | 3489 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3490 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3491 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3492 | `		/*Expecting an IO handle */` |
|    ! 0 | 3493 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3494 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3495 | `		return PH7_OK;` |
|      - | 3496 | `	}` |
|      - | 3497 | `	/* Point to the target IO stream device */` |
|      3 | 3498 | `	pStream = pDev->pStream;` |
|      3 | 3499 | `	if( pStream == 0  ){` |
|    ! 0 | 3500 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3501 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3502 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3503 | `			);` |
|    ! 0 | 3504 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3505 | `		return PH7_OK;` |
|      - | 3506 | `	}` |
|      3 | 3507 | `	nLen = -1;` |
|      3 | 3508 | `	if( nArg > 1 ){` |
|      - | 3509 | `		/* Maximum data to read */` |
|    ! 0 | 3510 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3511 | `	}` |
|      - | 3512 | `	/* Perform the requested operation */` |
|      3 | 3513 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3514 | `	if( n < 1 ){` |
|      - | 3515 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3516 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3517 | `	}else{` |
|      3 | 3518 | `		const char *zTaglist = 0;` |
|      3 | 3519 | `		int nTaglen = 0;` |
|      3 | 3520 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3521 | `			/* Allowed tag */` |
|    ! 0 | 3522 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3523 | `		}` |
|      - | 3524 | `		/* Process data just read */` |
|      3 | 3525 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3526 | `	}` |
|      3 | 3527 | `	return PH7_OK;` |
|      2 | 3528 | `}` |
|      - | 3529 | `/*` |
|      - | 3530 | ` * string readdir(resource $dir_handle)` |
|      - | 3531 | ` *   Read entry from directory handle.` |
|      - | 3532 | ` * Parameter` |
|      - | 3533 | ` *  $dir_handle` |
|      - | 3534 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3535 | ` * Return` |
|      - | 3536 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3537 | ` */` |
|  10994 | 3538 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3539 | `{` |
|      - | 3540 | `	const ph7_io_stream *pStream;` |
|      - | 3541 | `	io_private *pDev;` |
|      - | 3542 | `	int rc;` |
|  10999 | 3543 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3544 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3545 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3546 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3547 | `		return PH7_OK;` |
|      - | 3548 | `	}` |
|      - | 3549 | `	/* Extract our private data */` |
|  10999 | 3550 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3551 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10999 | 3552 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3553 | `		/*Expecting an IO handle */` |
|    ! 0 | 3554 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3556 | `		return PH7_OK;` |
|      - | 3557 | `	}` |
|      - | 3558 | `	/* Point to the target IO stream device */` |
|  10999 | 3559 | `	pStream = pDev->pStream;` |
|  10999 | 3560 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3561 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3562 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3563 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3564 | `			);` |
|    ! 0 | 3565 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3566 | `		return PH7_OK;` |
|      - | 3567 | `	}` |
|  10999 | 3568 | `	ph7_result_bool(pCtx,0);` |
|      - | 3569 | `	/* Perform the requested operation */` |
|  10999 | 3570 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  10999 | 3571 | `	if( rc != PH7_OK ){` |
|      - | 3572 | `		/* Return FALSE */` |
|   1067 | 3573 | `		ph7_result_bool(pCtx,0);` |
|    531 | 3574 | `	}` |
|  10999 | 3575 | `	return PH7_OK;` |
|   5502 | 3576 | `}` |
|      - | 3577 | `/*` |
|      - | 3578 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3579 | ` *   Rewind directory handle.` |
|      - | 3580 | ` * Parameter` |
|      - | 3581 | ` *  $dir_handle` |
|      - | 3582 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3583 | ` * Return` |
|      - | 3584 | ` *  FALSE on failure.` |
|      - | 3585 | ` */` |
|      2 | 3586 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3587 | `{` |
|      - | 3588 | `	const ph7_io_stream *pStream;` |
|      - | 3589 | `	io_private *pDev;` |
|      3 | 3590 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3591 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3592 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3593 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3594 | `		return PH7_OK;` |
|      - | 3595 | `	}` |
|      - | 3596 | `	/* Extract our private data */` |
|      3 | 3597 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3598 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3599 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3600 | `		/*Expecting an IO handle */` |
|    ! 0 | 3601 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3602 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3603 | `		return PH7_OK;` |
|      - | 3604 | `	}` |
|      - | 3605 | `	/* Point to the target IO stream device */` |
|      3 | 3606 | `	pStream = pDev->pStream;` |
|      3 | 3607 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3608 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3609 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3610 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3611 | `			);` |
|    ! 0 | 3612 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3613 | `		return PH7_OK;` |
|      - | 3614 | `	}` |
|      - | 3615 | `	/* Perform the requested operation */` |
|      3 | 3616 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3617 | `	return PH7_OK;` |
|      2 | 3618 | ` }` |
|      - | 3619 | `/* Forward declaration */` |
|      - | 3620 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3621 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3622 | `static void MarkIOPrivateClosed(io_private *pDev);` |
|      - | 3623 | `/*` |
|      - | 3624 | ` * void closedir(resource $dir_handle)` |
|      - | 3625 | ` *   Close directory handle.` |
|      - | 3626 | ` * Parameter` |
|      - | 3627 | ` *  $dir_handle` |
|      - | 3628 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3629 | ` * Return` |
|      - | 3630 | ` *  FALSE on failure.` |
|      - | 3631 | ` */` |
|   1066 | 3632 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3633 | `{` |
|      - | 3634 | `	const ph7_io_stream *pStream;` |
|      - | 3635 | `	io_private *pDev;` |
|   1071 | 3636 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3637 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3638 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3639 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3640 | `		return PH7_OK;` |
|      - | 3641 | `	}` |
|      - | 3642 | `	/* Extract our private data */` |
|   1071 | 3643 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3644 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1071 | 3645 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3646 | `		/*Expecting an IO handle */` |
|    ! 0 | 3647 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3648 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3649 | `		return PH7_OK;` |
|      - | 3650 | `	}` |
|      - | 3651 | `	/* Point to the target IO stream device */` |
|   1071 | 3652 | `	pStream = pDev->pStream;` |
|   1071 | 3653 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3654 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3655 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3656 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3657 | `			);` |
|    ! 0 | 3658 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3659 | `		return PH7_OK;` |
|      - | 3660 | `	}` |
|      - | 3661 | `	/* Perform the requested operation */` |
|   1071 | 3662 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3663 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|   1071 | 3664 | `	MarkIOPrivateClosed(pDev);` |
|   1071 | 3665 | `	return PH7_OK;` |
|    538 | 3666 | ` }` |
|      - | 3667 | `/*` |
|      - | 3668 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3669 | ` *  Open directory handle.` |
|      - | 3670 | ` * Parameters` |
|      - | 3671 | ` * $path` |
|      - | 3672 | ` *   The directory path that is to be opened.` |
|      - | 3673 | ` * $context` |
|      - | 3674 | ` *   A context stream resource.` |
|      - | 3675 | ` * Return` |
|      - | 3676 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3677 | ` */` |
|   1066 | 3678 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3679 | `{` |
|      - | 3680 | `	const ph7_io_stream *pStream;` |
|      - | 3681 | `	const char *zPath;` |
|      - | 3682 | `	io_private *pDev;` |
|      - | 3683 | `	int iLen,rc;` |
|   1071 | 3684 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3685 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3686 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3687 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3688 | `		return PH7_OK;` |
|      - | 3689 | `	}` |
|      - | 3690 | `	/* Extract the target path */` |
|   1071 | 3691 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3692 | `	/* Try to extract a stream */` |
|   1071 | 3693 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1071 | 3694 | `	if( pStream == 0 ){` |
|    ! 0 | 3695 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3696 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3697 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3698 | `		return PH7_OK;` |
|      - | 3699 | `	}` |
|   1071 | 3700 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3701 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3702 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3703 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3704 | `			);` |
|    ! 0 | 3705 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3706 | `		return PH7_OK;` |
|      - | 3707 | `	}` |
|      - | 3708 | `	/* Allocate a new IO private instance */` |
|   1071 | 3709 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1071 | 3710 | `	if( pDev == 0 ){` |
|    ! 0 | 3711 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3712 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3713 | `		return PH7_OK;` |
|      - | 3714 | `	}` |
|      - | 3715 | `	/* Initialize the structure */` |
|   1071 | 3716 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3717 | `	/* Open the target directory */` |
|   1071 | 3718 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1071 | 3719 | `	if( rc != PH7_OK ){` |
|      - | 3720 | `		/* IO error,return FALSE */` |
|    ! 0 | 3721 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3722 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3723 | `	}else{` |
|      - | 3724 | `		/* Return the handle as a resource */` |
|   1071 | 3725 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3726 | `	}` |
|   1071 | 3727 | `	return PH7_OK;` |
|    538 | 3728 | `}` |
|      - | 3729 | `/*` |
|      - | 3730 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3731 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3732 | ` * Parameters` |
|      - | 3733 | ` *  $filename` |
|      - | 3734 | ` *   The filename being read.` |
|      - | 3735 | ` *  $use_include_path` |
|      - | 3736 | ` *   You can use the optional second parameter and set it to` |
|      - | 3737 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3738 | ` *  $context` |
|      - | 3739 | ` *   A context stream resource.` |
|      - | 3740 | ` * Return` |
|      - | 3741 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3742 | ` */` |
|      - | 3743 | `/*` |
|      - | 3744 | ` * php's IO failures are E_WARNINGs naming the function, the path and the system reason:` |
|      - | 3745 | ` *   file_get_contents(/nope): Failed to open stream: No such file or directory` |
|      - | 3746 | ` * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of` |
|      - | 3747 | ` * them -- wrong severity, wrong text, and no reason. errno still holds the failing` |
|      - | 3748 | ` * syscall's code at this point (a successful call never clears it), which is where the` |
|      - | 3749 | ` * trailing reason comes from.` |
|      - | 3750 | ` */` |
|      2 | 3751 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3752 | `{` |
|      3 | 3753 | `	int use_include  = FALSE;` |
|      - | 3754 | `	const ph7_io_stream *pStream;` |
|      - | 3755 | `	ph7_int64 n,nRead;` |
|      - | 3756 | `	const char *zFile;` |
|      - | 3757 | `	char zBuf[8192];` |
|      - | 3758 | `	void *pHandle;` |
|      - | 3759 | `	int rc,nLen;` |
|      3 | 3760 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3761 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3762 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3763 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3764 | `		return PH7_OK;` |
|      - | 3765 | `	}` |
|      - | 3766 | `	/* Extract the file path */` |
|      3 | 3767 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3768 | `	/* Point to the target IO stream device */` |
|      3 | 3769 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3770 | `	if( pStream == 0 ){` |
|    ! 0 | 3771 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3772 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3773 | `		return PH7_OK;` |
|      - | 3774 | `	}` |
|      3 | 3775 | `	if( nArg > 1 ){` |
|    ! 0 | 3776 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3777 | `	}` |
|      - | 3778 | `	/* Try to open the file in read-only mode */` |
|      4 | 3779 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3780 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3781 | `	if( pHandle == 0 ){` |
|    ! 0 | 3782 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3783 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3784 | `		return PH7_OK;` |
|      - | 3785 | `	}` |
|      - | 3786 | `	/* Perform the requested operation */` |
|      3 | 3787 | `	nRead = 0;` |
|      2 | 3788 | `	for(;;){` |
|      5 | 3789 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3790 | `		if( n < 1 ){` |
|      - | 3791 | `			/* EOF or IO error,break immediately */` |
|      3 | 3792 | `			break;` |
|      - | 3793 | `		}` |
|      - | 3794 | `		/* Output data */` |
|      3 | 3795 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3796 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3797 | `			break;` |
|      - | 3798 | `		}` |
|      - | 3799 | `		/* Increment counter */` |
|      3 | 3800 | `		nRead += n;` |
|      1 | 3801 | `	}` |
|      - | 3802 | `	/* Close the stream */` |
|      3 | 3803 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3804 | `	/* Total number of bytes readen */` |
|      3 | 3805 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3806 | `	return PH7_OK;` |
|      2 | 3807 | `}` |
|      - | 3808 | `/*` |
|      - | 3809 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3810 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3811 | ` *  Reads entire file into a string.` |
|      - | 3812 | ` * Parameters` |
|      - | 3813 | ` *  $filename` |
|      - | 3814 | ` *   The filename being read.` |
|      - | 3815 | ` *  $use_include_path` |
|      - | 3816 | ` *   You can use the optional second parameter and set it to` |
|      - | 3817 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3818 | ` *  $context` |
|      - | 3819 | ` *   A context stream resource.` |
|      - | 3820 | ` *  $offset` |
|      - | 3821 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3822 | ` *  $maxlen` |
|      - | 3823 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3824 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3825 | ` * Return` |
|      - | 3826 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3827 | ` */` |
|   6760 | 3828 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3829 | `{` |
|      - | 3830 | `	const ph7_io_stream *pStream;` |
|      - | 3831 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6765 | 3832 | `	int use_include  = FALSE;` |
|      - | 3833 | `	const char *zFile;` |
|      - | 3834 | `	char zBuf[8192];` |
|      - | 3835 | `	void *pHandle;` |
|      - | 3836 | `	int nLen;` |
|      - | 3837 |  |
|   6765 | 3838 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3839 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3840 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3841 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3842 | `		return PH7_OK;` |
|      - | 3843 | `	}` |
|      - | 3844 | `	/* Extract the file path */` |
|   6765 | 3845 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3846 | `	/* Point to the target IO stream device */` |
|   6765 | 3847 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6765 | 3848 | `	if( pStream == 0 ){` |
|    ! 0 | 3849 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3850 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3851 | `		return PH7_OK;` |
|      - | 3852 | `	}` |
|   6765 | 3853 | `	nMaxlen = -1;` |
|   6765 | 3854 | `	if( nArg > 1 ){` |
|      5 | 3855 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3856 | `	}` |
|      - | 3857 | `	/* Try to open the file in read-only mode */` |
|   6765 | 3858 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6765 | 3859 | `	if( pHandle == 0 ){` |
|      3 | 3860 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 3861 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3862 | `		return PH7_OK;` |
|      - | 3863 | `	}` |
|   6763 | 3864 | `	if( nArg > 3 ){` |
|      - | 3865 | `		/* Extract the offset */` |
|      5 | 3866 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3867 | `		if( n > 0 ){` |
|    ! 0 | 3868 | `			if( pStream->xSeek ){` |
|      - | 3869 | `				/* Seek to the desired offset */` |
|    ! 0 | 3870 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3871 | `			}` |
|    ! 0 | 3872 | `		}` |
|      5 | 3873 | `		if( nArg > 4 ){` |
|      - | 3874 | `			/* Maximum data to read */` |
|      5 | 3875 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3876 | `		}` |
|      2 | 3877 | `	}` |
|      - | 3878 | `	/* Perform the requested operation */` |
|   6763 | 3879 | `	nRead = 0;` |
|   6755 | 3880 | `	for(;;){` |
|  20273 | 3881 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6758 | 3882 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  13515 | 3883 | `		if( n < 1 ){` |
|      - | 3884 | `			/* EOF or IO error,break immediately */` |
|   6761 | 3885 | `			break;` |
|      - | 3886 | `		}` |
|      - | 3887 | `		/* Append data */` |
|   6759 | 3888 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3889 | `		/* Increment read counter */` |
|   6759 | 3890 | `		nRead += n;` |
|   6759 | 3891 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3892 | `			/* Read limit reached */` |
|      3 | 3893 | `			break;` |
|      - | 3894 | `		}` |
|      5 | 3895 | `	}` |
|      - | 3896 | `	/* Close the stream */` |
|   6763 | 3897 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3898 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|      - | 3899 | `	 * open failure, handled above); the read loop never set a string result, so` |
|      - | 3900 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   6763 | 3901 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      6 | 3902 | `		ph7_result_string(pCtx,"",0);` |
|      2 | 3903 | `	}` |
|   6763 | 3904 | `	return PH7_OK;` |
|   3385 | 3905 | `}` |
|      - | 3906 | `/*` |
|      - | 3907 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3908 | ` *  Write a string to a file.` |
|      - | 3909 | ` * Parameters` |
|      - | 3910 | ` *  $filename` |
|      - | 3911 | ` *  Path to the file where to write the data.` |
|      - | 3912 | ` * $data` |
|      - | 3913 | ` *  The data to write(Must be a string).` |
|      - | 3914 | ` * $flags` |
|      - | 3915 | ` *  The value of flags can be any combination of the following` |
|      - | 3916 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3917 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3918 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3919 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3920 | ` * context` |
|      - | 3921 | ` *  A context stream resource.` |
|      - | 3922 | ` * Return` |
|      - | 3923 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3924 | ` */` |
|  13722 | 3925 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3926 | `{` |
|  13727 | 3927 | `	int use_include  = FALSE;` |
|      - | 3928 | `	const ph7_io_stream *pStream;` |
|      - | 3929 | `	const char *zFile;` |
|      - | 3930 | `	const char *zData;` |
|      - | 3931 | `	int iOpenFlags;` |
|      - | 3932 | `	void *pHandle;` |
|      - | 3933 | `	int iFlags;` |
|      - | 3934 | `	int nLen;` |
|      - | 3935 |  |
|  13727 | 3936 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3937 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3938 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3939 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3940 | `		return PH7_OK;` |
|      - | 3941 | `	}` |
|      - | 3942 | `	/* Extract the file path */` |
|  13727 | 3943 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3944 | `	/* Point to the target IO stream device */` |
|  13727 | 3945 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13727 | 3946 | `	if( pStream == 0 ){` |
|    ! 0 | 3947 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3948 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3949 | `		return PH7_OK;` |
|      - | 3950 | `	}` |
|      - | 3951 | `	/* Data to write */` |
|  13727 | 3952 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3953 | `	/* Try to open the file in read-write mode */` |
|  13727 | 3954 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3955 | `	/* Extract the flags */` |
|  13727 | 3956 | `	iFlags = 0;` |
|  13727 | 3957 | `	if( nArg > 2 ){` |
|    ! 0 | 3958 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3959 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3960 | `			use_include = TRUE;` |
|    ! 0 | 3961 | `		}` |
|    ! 0 | 3962 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3963 | `			/* If the file already exists, append the data to the file` |
|      - | 3964 | `			 * instead of overwriting it.` |
|      - | 3965 | `			 */` |
|    ! 0 | 3966 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3967 | `			/* Append mode */` |
|    ! 0 | 3968 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3969 | `		}` |
|    ! 0 | 3970 | `	}` |
|  20588 | 3971 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6861 | 3972 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13727 | 3973 | `	if( pHandle == 0 ){` |
|    ! 0 | 3974 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3975 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3976 | `		return PH7_OK;` |
|      - | 3977 | `	}` |
|  13727 | 3978 | `	if( nLen < 1 ){` |
|      - | 3979 | `		/* Empty data, file is created/truncated */` |
|     10 | 3980 | `		ph7_result_int64(pCtx,0);` |
|     10 | 3981 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     10 | 3982 | `		return PH7_OK;` |
|      - | 3983 | `	}` |
|  13719 | 3984 | `	if( pStream->xWrite ){` |
|      - | 3985 | `		ph7_int64 n;` |
|  13719 | 3986 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 3987 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3988 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3989 | `		}` |
|      - | 3990 | `		/* Perform the write operation */` |
|  13719 | 3991 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13719 | 3992 | `		if( n < 0 ){` |
|      - | 3993 | `			/* IO error,return FALSE — with php's write-failure diagnostic. */` |
|      1 | 3994 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3995 | `				"%s(): Write of %d bytes failed with errno=%d %s",` |
|    ! 0 | 3996 | `				ph7_function_name(pCtx),(int)nLen,errno,VfsStrerror(errno));` |
|      1 | 3997 | `			ph7_result_bool(pCtx,0);` |
|      1 | 3998 | `		}else{` |
|      - | 3999 | `			/* Total number of bytes written */` |
|  13719 | 4000 | `			ph7_result_int64(pCtx,n);` |
|      - | 4001 | `		}` |
|   6862 | 4002 | `	}else{` |
|      - | 4003 | `		/* Read-only stream */` |
|    ! 0 | 4004 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 4005 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 4006 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 4007 | `			);` |
|    ! 0 | 4008 | `		ph7_result_bool(pCtx,0);` |
|      - | 4009 | `	}` |
|      - | 4010 | `	/* Close the handle */` |
|  13719 | 4011 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13719 | 4012 | `	return PH7_OK;` |
|   6866 | 4013 | `}` |
|      - | 4014 | `/*` |
|      - | 4015 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 4016 | ` *  Reads entire file into an array.` |
|      - | 4017 | ` * Parameters` |
|      - | 4018 | ` *  $filename` |
|      - | 4019 | ` *   The filename being read.` |
|      - | 4020 | ` *  $flags` |
|      - | 4021 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 4022 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 4023 | ` *       Search for the file in the include_path.` |
|      - | 4024 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 4025 | ` *       Do not add newline at the end of each array element` |
|      - | 4026 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 4027 | ` *       Skip empty lines` |
|      - | 4028 | ` *  $context` |
|      - | 4029 | ` *   A context stream resource.` |
|      - | 4030 | ` * Return` |
|      - | 4031 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 4032 | ` */` |
|     10 | 4033 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4034 | `{` |
|      - | 4035 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 4036 | `	ph7_value *pArray,*pLine;` |
|      - | 4037 | `	const ph7_io_stream *pStream;` |
|     13 | 4038 | `	int use_include = 0;` |
|      - | 4039 | `	io_private *pDev;` |
|      - | 4040 | `	ph7_int64 n;` |
|      - | 4041 | `	int iFlags;` |
|      - | 4042 | `	int nLen;` |
|      - | 4043 |  |
|     13 | 4044 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4045 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4046 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4047 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4048 | `		return PH7_OK;` |
|      - | 4049 | `	}` |
|      - | 4050 | `	/* Extract the file path */` |
|     13 | 4051 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4052 | `	/* Point to the target IO stream device */` |
|     13 | 4053 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     13 | 4054 | `	if( pStream == 0 ){` |
|    ! 0 | 4055 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4056 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4057 | `		return PH7_OK;` |
|      - | 4058 | `	}` |
|      - | 4059 | `	/* Allocate a new IO private instance */` |
|     13 | 4060 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     13 | 4061 | `	if( pDev == 0 ){` |
|    ! 0 | 4062 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4063 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4064 | `		return PH7_OK;` |
|      - | 4065 | `	}` |
|      - | 4066 | `	/* Initialize the structure */` |
|     13 | 4067 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     13 | 4068 | `	iFlags = 0;` |
|     13 | 4069 | `	if( nArg > 1 ){` |
|    ! 0 | 4070 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 4071 | `	}` |
|      8 | 4072 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 4073 | `		use_include = TRUE;` |
|    ! 0 | 4074 | `	}` |
|      - | 4075 | `	/* Create the array and the working value */` |
|     13 | 4076 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 4077 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     13 | 4078 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 4079 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4080 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4081 | `		return PH7_OK;` |
|      - | 4082 | `	}` |
|      - | 4083 | `	/* Try to open the file in read-only mode */` |
|     13 | 4084 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|     13 | 4085 | `	if( pDev->pHandle == 0 ){` |
|     10 | 4086 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     10 | 4087 | `		ph7_result_bool(pCtx,0);` |
|      - | 4088 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 4089 | `		 * as soon we return from this function.` |
|      - | 4090 | `		 */` |
|     10 | 4091 | `		return PH7_OK;` |
|      - | 4092 | `	}` |
|      - | 4093 | `	/* Perform the requested operation */` |
|      3 | 4094 | `	for(;;){` |
|      - | 4095 | `		/* Try to extract a line */` |
|      7 | 4096 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 4097 | `		if( n < 1 ){` |
|      - | 4098 | `			/* EOF or IO error */` |
|      3 | 4099 | `			break;` |
|      - | 4100 | `		}` |
|      - | 4101 | `		/* Reset the cursor */` |
|      5 | 4102 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 4103 | `		/* Remove line ending if requested by the caller */` |
|      5 | 4104 | `		zPtr = zBuf;` |
|      5 | 4105 | `		zEnd = &zBuf[n];` |
|      5 | 4106 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 4107 | `			/* Ignore trailig lines */` |
|    ! 0 | 4108 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 4109 | `#ifdef __WINNT__` |
|      - | 4110 | `				\|\| zEnd[-1] == '\r'` |
|      - | 4111 | `#endif` |
|      - | 4112 | `				)){` |
|    ! 0 | 4113 | `					n--;` |
|    ! 0 | 4114 | `					zEnd--;` |
|    ! 0 | 4115 | `			}` |
|    ! 0 | 4116 | `		}` |
|      3 | 4117 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 4118 | `			/* Ignore empty lines */` |
|    ! 0 | 4119 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 4120 | `				zPtr++;` |
|    ! 0 | 4121 | `			}` |
|    ! 0 | 4122 | `			if( zPtr >= zEnd ){` |
|      - | 4123 | `				/* Empty line */` |
|    ! 0 | 4124 | `				continue;` |
|      - | 4125 | `			}` |
|    ! 0 | 4126 | `		}` |
|      5 | 4127 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 4128 | `		/* Insert line */` |
|      5 | 4129 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 4130 | `	}` |
|      - | 4131 | `	/* Close the stream */` |
|      3 | 4132 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4133 | `	/* Release the io_private instance */` |
|      3 | 4134 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4135 | `	/* Return the created array */` |
|      3 | 4136 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4137 | `	return PH7_OK;` |
|      8 | 4138 | `}` |
|      - | 4139 | `/*` |
|      - | 4140 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 4141 | ` *  Makes a copy of the file source to dest.` |
|      - | 4142 | ` * Parameters` |
|      - | 4143 | ` *  $source` |
|      - | 4144 | ` *   Path to the source file.` |
|      - | 4145 | ` *  $dest` |
|      - | 4146 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 4147 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 4148 | ` *  $context` |
|      - | 4149 | ` *   A context stream resource.` |
|      - | 4150 | ` * Return` |
|      - | 4151 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4152 | ` */` |
|      4 | 4153 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4154 | `{` |
|      - | 4155 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 4156 | `	const char *zFile;` |
|      - | 4157 | `	char zBuf[8192];` |
|      - | 4158 | `	void *pIn,*pOut;` |
|      - | 4159 | `	ph7_int64 n;` |
|      - | 4160 | `	int nLen;` |
|      6 | 4161 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 4162 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4163 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 4164 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4165 | `		return PH7_OK;` |
|      - | 4166 | `	}` |
|      - | 4167 | `	/* Extract the source name */` |
|      6 | 4168 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4169 | `	/* Point to the target IO stream device */` |
|      6 | 4170 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4171 | `	if( pSin == 0 ){` |
|    ! 0 | 4172 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4173 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4174 | `		return PH7_OK;` |
|      - | 4175 | `	}` |
|      - | 4176 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4177 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4178 | `	if( pIn == 0 ){` |
|      3 | 4179 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 4180 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4181 | `		return PH7_OK;` |
|      - | 4182 | `	}` |
|      - | 4183 | `	/* Extract the destination name */` |
|      3 | 4184 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4185 | `	/* Point to the target IO stream device */` |
|      3 | 4186 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4187 | `	if( pSout == 0 ){` |
|    ! 0 | 4188 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4189 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4190 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4191 | `		return PH7_OK;` |
|      - | 4192 | `	}` |
|      3 | 4193 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4194 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4195 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4196 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4197 | `			);` |
|    ! 0 | 4198 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4199 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4200 | `		return PH7_OK;` |
|      - | 4201 | `	}` |
|      - | 4202 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4203 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4204 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4205 | `	if( pOut == 0 ){` |
|    ! 0 | 4206 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 4207 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4208 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4209 | `		return PH7_OK;` |
|      - | 4210 | `	}` |
|      - | 4211 | `	/* Perform the requested operation */` |
|      2 | 4212 | `	for(;;){` |
|      - | 4213 | `		/* Read from source */` |
|      5 | 4214 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4215 | `		if( n < 1 ){` |
|      - | 4216 | `			/* EOF or IO error,break immediately */` |
|      3 | 4217 | `			break;` |
|      - | 4218 | `		}` |
|      - | 4219 | `		/* Write to dest */` |
|      3 | 4220 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4221 | `		if( n < 1 ){` |
|      - | 4222 | `			/* IO error,break immediately */` |
|    ! 0 | 4223 | `			break;` |
|      - | 4224 | `		}` |
|      1 | 4225 | `	}` |
|      - | 4226 | `	/* Close the streams */` |
|      3 | 4227 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4228 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4229 | `	/* Return TRUE */` |
|      3 | 4230 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4231 | `	return PH7_OK;` |
|      4 | 4232 | `}` |
|      - | 4233 | `/*` |
|      - | 4234 | ` * array fstat(resource $handle)` |
|      - | 4235 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4236 | ` * Parameters` |
|      - | 4237 | ` *  $handle` |
|      - | 4238 | ` *   The file pointer.` |
|      - | 4239 | ` * Return` |
|      - | 4240 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4241 | ` */` |
|      2 | 4242 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4243 | `{` |
|      - | 4244 | `	ph7_value *pArray,*pValue;` |
|      - | 4245 | `	const ph7_io_stream *pStream;` |
|      - | 4246 | `	io_private *pDev;` |
|      3 | 4247 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4248 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4249 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4250 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4251 | `		return PH7_OK;` |
|      - | 4252 | `	}` |
|      - | 4253 | `	/* Extract our private data */` |
|      3 | 4254 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4255 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4256 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4257 | `		/* Expecting an IO handle */` |
|    ! 0 | 4258 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4259 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4260 | `		return PH7_OK;` |
|      - | 4261 | `	}` |
|      - | 4262 | `	/* Point to the target IO stream device */` |
|      3 | 4263 | `	pStream = pDev->pStream;` |
|      3 | 4264 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4265 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4266 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4267 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4268 | `			);` |
|    ! 0 | 4269 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4270 | `		return PH7_OK;` |
|      - | 4271 | `	}` |
|      - | 4272 | `	/* Create the array and the working value */` |
|      3 | 4273 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4274 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4275 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4276 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4277 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4278 | `		return PH7_OK;` |
|      - | 4279 | `	}` |
|      - | 4280 | `	/* Perform the requested operation */` |
|      3 | 4281 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4282 | `	/* Return the freshly created array */` |
|      3 | 4283 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4284 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4285 | `	 * released automatically as soon we return from this function.` |
|      - | 4286 | `	 */` |
|      3 | 4287 | `	return PH7_OK;` |
|      2 | 4288 | `}` |
|      - | 4289 | `/*` |
|      - | 4290 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4291 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4292 | ` * Parameters` |
|      - | 4293 | ` *  $handle` |
|      - | 4294 | ` *   The file pointer.` |
|      - | 4295 | ` *  $string` |
|      - | 4296 | ` *   The string that is to be written.` |
|      - | 4297 | ` *  $length` |
|      - | 4298 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4299 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4300 | ` * Return` |
|      - | 4301 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4302 | ` */` |
|     44 | 4303 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4304 | `{` |
|      - | 4305 | `	const ph7_io_stream *pStream;` |
|      - | 4306 | `	const char *zString;` |
|      - | 4307 | `	io_private *pDev;` |
|      - | 4308 | `	int nLen,n;` |
|     46 | 4309 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4310 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4311 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4312 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4313 | `		return PH7_OK;` |
|      - | 4314 | `	}` |
|      - | 4315 | `	/* Extract our private data */` |
|     46 | 4316 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4317 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     46 | 4318 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4319 | `		/* Expecting an IO handle */` |
|    ! 0 | 4320 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4321 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4322 | `		return PH7_OK;` |
|      - | 4323 | `	}` |
|      - | 4324 | `	/* Point to the target IO stream device */` |
|     46 | 4325 | `	pStream = pDev->pStream;` |
|     46 | 4326 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4327 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4328 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4329 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4330 | `			);` |
|    ! 0 | 4331 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4332 | `		return PH7_OK;` |
|      - | 4333 | `	}` |
|      - | 4334 | `	/* Extract the data to write */` |
|     46 | 4335 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     46 | 4336 | `	if( nArg > 2 ){` |
|      - | 4337 | `		/* Maximum data length to write */` |
|    ! 0 | 4338 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4339 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4340 | `			nLen = n;` |
|    ! 0 | 4341 | `		}` |
|    ! 0 | 4342 | `	}` |
|     46 | 4343 | `	if( nLen < 1 ){` |
|      - | 4344 | `		/* Nothing to write */` |
|    ! 0 | 4345 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4346 | `		return PH7_OK;` |
|      - | 4347 | `	}` |
|      - | 4348 | `	/* Perform the requested operation */` |
|     46 | 4349 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|     46 | 4350 | `	if( n <  0 ){` |
|      - | 4351 | `		/* IO error,return FALSE */` |
|    ! 0 | 4352 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4353 | `	}else{` |
|      - | 4354 | `		/* #Bytes written */` |
|     46 | 4355 | `		ph7_result_int(pCtx,n);` |
|      - | 4356 | `	}` |
|     46 | 4357 | `	return PH7_OK;` |
|     24 | 4358 | `}` |
|      - | 4359 | `/*` |
|      - | 4360 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4361 | ` *  Portable advisory file locking.` |
|      - | 4362 | ` * Parameters` |
|      - | 4363 | ` *  $handle` |
|      - | 4364 | ` *   The file pointer.` |
|      - | 4365 | ` *  $operation` |
|      - | 4366 | ` *   operation is one of the following:` |
|      - | 4367 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4368 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4369 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4370 | ` * Return` |
|      - | 4371 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4372 | ` */` |
|      4 | 4373 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4374 | `{` |
|      - | 4375 | `	const ph7_io_stream *pStream;` |
|      - | 4376 | `	io_private *pDev;` |
|      - | 4377 | `	int nLock;` |
|      - | 4378 | `	int rc;` |
|      5 | 4379 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4380 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4381 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4382 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4383 | `		return PH7_OK;` |
|      - | 4384 | `	}` |
|      - | 4385 | `	/* Extract our private data */` |
|      5 | 4386 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4387 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4388 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4389 | `		/*Expecting an IO handle */` |
|    ! 0 | 4390 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4391 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4392 | `		return PH7_OK;` |
|      - | 4393 | `	}` |
|      - | 4394 | `	/* Point to the target IO stream device */` |
|      5 | 4395 | `	pStream = pDev->pStream;` |
|      5 | 4396 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4397 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4398 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4399 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4400 | `			);` |
|    ! 0 | 4401 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4402 | `		return PH7_OK;` |
|      - | 4403 | `	}` |
|      - | 4404 | `	/* Requested lock operation */` |
|      5 | 4405 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4406 | `	/*` |
|      - | 4407 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally \|LOCK_NB=4)` |
|      - | 4408 | `	 * into the xLock() vtable contract, which is a PUBLIC C API and stays as it is:` |
|      - | 4409 | `	 * negative = unlock, 1 = exclusive, anything else = shared.` |
|      - | 4410 | `	 */` |
|      - | 4411 | `	{` |
|      5 | 4412 | `		int iOp = nLock & ~4 /* strip LOCK_NB */;` |
|      5 | 4413 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      3 | 4414 | `			nLock = -1;` |
|      4 | 4415 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|      3 | 4416 | `			nLock = 1;` |
|      2 | 4417 | `		}else{` |
|    ! 0 | 4418 | `			nLock = 0; /* LOCK_SH */` |
|      - | 4419 | `		}` |
|      - | 4420 | `	}` |
|      - | 4421 | `	/* Lock operation */` |
|      5 | 4422 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4423 | `	/* IO result */` |
|      5 | 4424 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 4425 | `	return PH7_OK;` |
|      3 | 4426 | `}` |
|      - | 4427 | `/*` |
|      - | 4428 | ` * int fpassthru(resource $handle)` |
|      - | 4429 | ` *  Output all remaining data on a file pointer.` |
|      - | 4430 | ` * Parameters` |
|      - | 4431 | ` *  $handle` |
|      - | 4432 | ` *   The file pointer.` |
|      - | 4433 | ` * Return` |
|      - | 4434 | ` *  Total number of characters read from handle and passed through` |
|      - | 4435 | ` *  to the output on success or FALSE on failure.` |
|      - | 4436 | ` */` |
|      2 | 4437 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4438 | `{` |
|      - | 4439 | `	const ph7_io_stream *pStream;` |
|      - | 4440 | `	io_private *pDev;` |
|      - | 4441 | `	ph7_int64 n,nRead;` |
|      - | 4442 | `	char zBuf[8192];` |
|      - | 4443 | `	int rc;` |
|      3 | 4444 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4445 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4446 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4447 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4448 | `		return PH7_OK;` |
|      - | 4449 | `	}` |
|      - | 4450 | `	/* Extract our private data */` |
|      3 | 4451 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4452 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4453 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4454 | `		/*Expecting an IO handle */` |
|    ! 0 | 4455 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4456 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4457 | `		return PH7_OK;` |
|      - | 4458 | `	}` |
|      - | 4459 | `	/* Point to the target IO stream device */` |
|      3 | 4460 | `	pStream = pDev->pStream;` |
|      3 | 4461 | `	if( pStream == 0  ){` |
|    ! 0 | 4462 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4463 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4464 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4465 | `			);` |
|    ! 0 | 4466 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4467 | `		return PH7_OK;` |
|      - | 4468 | `	}` |
|      - | 4469 | `	/* Perform the requested operation */` |
|      3 | 4470 | `	nRead = 0;` |
|      2 | 4471 | `	for(;;){` |
|      5 | 4472 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4473 | `		if( n < 1 ){` |
|      - | 4474 | `			/* Error or EOF */` |
|      3 | 4475 | `			break;` |
|      - | 4476 | `		}` |
|      - | 4477 | `		/* Increment the read counter */` |
|      3 | 4478 | `		nRead += n;` |
|      - | 4479 | `		/* Output data */` |
|      3 | 4480 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4481 | `		if( rc == PH7_ABORT ){` |
|      - | 4482 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4483 | `			break;` |
|      - | 4484 | `		}` |
|      1 | 4485 | `	}` |
|      - | 4486 | `	/* Total number of bytes readen */` |
|      3 | 4487 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4488 | `	return PH7_OK;` |
|      2 | 4489 | `}` |
|      - | 4490 | `/* CSV reader/writer private data */` |
|      - | 4491 | `struct csv_data` |
|      - | 4492 | `{` |
|      - | 4493 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4494 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4495 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4496 | `	int iCount;       /* Counter */` |
|      - | 4497 | `};` |
|      - | 4498 | `/*` |
|      - | 4499 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4500 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4501 | ` * associated data.` |
|      - | 4502 | ` */` |
|      6 | 4503 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4504 | `{` |
|      7 | 4505 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4506 | `	const char *zData;` |
|      - | 4507 | `	int nLen,c2;` |
|      - | 4508 | `	sxu32 n;` |
|      - | 4509 | `	/* Point to the raw data */` |
|      7 | 4510 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4511 | `	if( nLen < 1 ){` |
|      - | 4512 | `		/* Nothing to write */` |
|    ! 0 | 4513 | `		return PH7_OK;` |
|      - | 4514 | `	}` |
|      7 | 4515 | `	if( pData->iCount > 0 ){` |
|      - | 4516 | `		/* Write the delimiter */` |
|      5 | 4517 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4518 | `	}` |
|      7 | 4519 | `	n = 1;` |
|      7 | 4520 | `	c2 = 0;` |
|     10 | 4521 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4522 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4523 | `			c2 = 1;` |
|    ! 0 | 4524 | `			if( n == 0 ){` |
|    ! 0 | 4525 | `				c2 = 2;` |
|    ! 0 | 4526 | `			}` |
|      - | 4527 | `			/* Write the enclosure */` |
|    ! 0 | 4528 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4529 | `			if( c2 > 1 ){` |
|    ! 0 | 4530 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4531 | `			}` |
|    ! 0 | 4532 | `	}` |
|      - | 4533 | `	/* Write the data */` |
|      7 | 4534 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4535 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4536 | `		return PH7_ABORT;` |
|      - | 4537 | `	}` |
|      7 | 4538 | `	if( c2 > 0 ){` |
|      - | 4539 | `		/* Write the enclosure */` |
|    ! 0 | 4540 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4541 | `		if( c2 > 1 ){` |
|    ! 0 | 4542 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4543 | `		}` |
|    ! 0 | 4544 | `	}` |
|      7 | 4545 | `	pData->iCount++;` |
|      7 | 4546 | `	return PH7_OK;` |
|      4 | 4547 | `}` |
|      - | 4548 | `/*` |
|      - | 4549 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4550 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4551 | ` * Parameters` |
|      - | 4552 | ` *  $handle` |
|      - | 4553 | ` *   Open file handle.` |
|      - | 4554 | ` * $fields` |
|      - | 4555 | ` *   An array of values.` |
|      - | 4556 | ` * $delimiter` |
|      - | 4557 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4558 | ` * $enclosure` |
|      - | 4559 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4560 | ` */` |
|      2 | 4561 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4562 | `{` |
|      - | 4563 | `	const ph7_io_stream *pStream;` |
|      - | 4564 | `	struct csv_data sCsv;` |
|      - | 4565 | `	io_private *pDev;` |
|      - | 4566 | `	char *zEol;` |
|      - | 4567 | `	int eolen;` |
|      3 | 4568 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4569 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4570 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4571 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4572 | `		return PH7_OK;` |
|      - | 4573 | `	}` |
|      - | 4574 | `	/* Extract our private data */` |
|      3 | 4575 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4576 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4577 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4578 | `		/*Expecting an IO handle */` |
|    ! 0 | 4579 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4580 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4581 | `		return PH7_OK;` |
|      - | 4582 | `	}` |
|      - | 4583 | `	/* Point to the target IO stream device */` |
|      3 | 4584 | `	pStream = pDev->pStream;` |
|      3 | 4585 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4586 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4587 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4588 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4589 | `			);` |
|    ! 0 | 4590 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4591 | `		return PH7_OK;` |
|      - | 4592 | `	}` |
|      - | 4593 | `	/* Set default csv separator */` |
|      3 | 4594 | `	sCsv.delimiter = ',';` |
|      3 | 4595 | `	sCsv.enclosure = '"';` |
|      3 | 4596 | `	sCsv.pDev = pDev;` |
|      3 | 4597 | `	sCsv.iCount = 0;` |
|      3 | 4598 | `	if( nArg > 2 ){` |
|      - | 4599 | `		/* User delimiter */` |
|      - | 4600 | `		const char *z;` |
|      - | 4601 | `		int n;` |
|      3 | 4602 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4603 | `		if( n > 0 ){` |
|      3 | 4604 | `			sCsv.delimiter = z[0];` |
|      1 | 4605 | `		}` |
|      3 | 4606 | `		if( nArg > 3 ){` |
|      3 | 4607 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4608 | `			if( n > 0 ){` |
|      3 | 4609 | `				sCsv.enclosure = z[0];` |
|      1 | 4610 | `			}` |
|      1 | 4611 | `		}` |
|      1 | 4612 | `	}` |
|      - | 4613 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4614 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4615 | `	/* Write a line ending */` |
|      - | 4616 | `#ifdef __WINNT__` |
|      1 | 4617 | `	zEol = "\r\n";` |
|      1 | 4618 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4619 | `#else` |
|      - | 4620 | `	/* Assume UNIX LF */` |
|      2 | 4621 | `	zEol = "\n";` |
|      2 | 4622 | `	eolen = (int)sizeof(char);` |
|      - | 4623 | `#endif` |
|      3 | 4624 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4625 | `	return PH7_OK;` |
|      2 | 4626 | `}` |
|      - | 4627 | `/*` |
|      - | 4628 | ` * fprintf,vfprintf private data.` |
|      - | 4629 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4630 | ` * input consumer callback defined below.` |
|      - | 4631 | ` */` |
|      - | 4632 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4633 | `struct fprintf_data` |
|      - | 4634 | `{` |
|      - | 4635 | `	io_private *pIO;        /* IO stream */` |
|      - | 4636 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4637 | `};` |
|      - | 4638 | `/*` |
|      - | 4639 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4640 | ` */` |
|     30 | 4641 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4642 | `{` |
|     31 | 4643 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4644 | `	ph7_int64 n;` |
|      - | 4645 | `	/* Write the formatted data */` |
|     31 | 4646 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     31 | 4647 | `	if( n < 1 ){` |
|    ! 0 | 4648 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4649 | `		/* IO error,abort immediately */` |
|    ! 0 | 4650 | `		return SXERR_ABORT;` |
|      - | 4651 | `	}` |
|      - | 4652 | `	/* Increment counter */` |
|     31 | 4653 | `	pFdata->nCount += n;` |
|     31 | 4654 | `	return PH7_OK;` |
|     16 | 4655 | `}` |
|      - | 4656 | `/*` |
|      - | 4657 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4658 | ` *  Write a formatted string to a stream.` |
|      - | 4659 | ` * Parameters` |
|      - | 4660 | ` *  $handle` |
|      - | 4661 | ` *   The file pointer.` |
|      - | 4662 | ` *  $format` |
|      - | 4663 | ` *   String format (see sprintf()).` |
|      - | 4664 | ` * Return` |
|      - | 4665 | ` *  The length of the written string.` |
|      - | 4666 | ` */` |
|     18 | 4667 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4668 | `{` |
|      - | 4669 | `	fprintf_data sFdata;` |
|      - | 4670 | `	const char *zFormat;` |
|      - | 4671 | `	io_private *pDev;` |
|      - | 4672 | `	int nLen;` |
|     19 | 4673 | `	if( nArg < 2 ){` |
|    ! 0 | 4674 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4675 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4676 | `		return PH7_OK;` |
|      - | 4677 | `	}` |
|      - | 4678 | `	{` |
|      - | 4679 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     19 | 4680 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     19 | 4681 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 4682 | `			return rcs;` |
|      - | 4683 | `		}` |
|      - | 4684 | `	}` |
|      - | 4685 | `	/* Extract our private data */` |
|     19 | 4686 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4687 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     19 | 4688 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4689 | `		/*Expecting an IO handle */` |
|    ! 0 | 4690 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4691 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4692 | `		return PH7_OK;` |
|      - | 4693 | `	}` |
|      - | 4694 | `	/* Point to the target IO stream device */` |
|     19 | 4695 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4696 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4697 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4698 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4699 | `			);` |
|    ! 0 | 4700 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4701 | `		return PH7_OK;` |
|      - | 4702 | `	}` |
|      - | 4703 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 4704 | `	{` |
|     19 | 4705 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     19 | 4706 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4707 | `			return rcf;` |
|      - | 4708 | `		}` |
|      - | 4709 | `	}` |
|      - | 4710 | `	/* Extract the string format (scalars/null coerce). */` |
|     19 | 4711 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     19 | 4712 | `	if( nLen < 1 ){` |
|      - | 4713 | `		/* Empty string,return zero */` |
|    ! 0 | 4714 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4715 | `		return PH7_OK;` |
|      - | 4716 | `	}` |
|      - | 4717 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4718 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4719 | `	{` |
|     19 | 4720 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     19 | 4721 | `		if( rcv != PH7_OK ){` |
|      3 | 4722 | `			return rcv;` |
|      - | 4723 | `		}` |
|      - | 4724 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output.` |
|      - | 4725 | `		 * fprintf's values start at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     17 | 4726 | `		rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     17 | 4727 | `		if( rcv != PH7_OK ){` |
|      3 | 4728 | `			return rcv;` |
|      - | 4729 | `		}` |
|      - | 4730 | `	}` |
|      - | 4731 | `	/* Prepare our private data */` |
|     15 | 4732 | `	sFdata.nCount = 0;` |
|     15 | 4733 | `	sFdata.pIO = pDev;` |
|      - | 4734 | `	/* Format the string */` |
|     15 | 4735 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4736 | `	/* Return total number of bytes written */` |
|     15 | 4737 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     15 | 4738 | `	return PH7_OK;` |
|     10 | 4739 | `}` |
|      - | 4740 | `/*` |
|      - | 4741 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4742 | ` *  Write a formatted string to a stream.` |
|      - | 4743 | ` * Parameters` |
|      - | 4744 | ` *  $handle` |
|      - | 4745 | ` *   The file pointer.` |
|      - | 4746 | ` *  $format` |
|      - | 4747 | ` *   String format (see sprintf()).` |
|      - | 4748 | ` * $args` |
|      - | 4749 | ` *   User arguments.` |
|      - | 4750 | ` * Return` |
|      - | 4751 | ` *  The length of the written string.` |
|      - | 4752 | ` */` |
|      6 | 4753 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4754 | `{` |
|      - | 4755 | `	fprintf_data sFdata;` |
|      - | 4756 | `	const char *zFormat;` |
|      - | 4757 | `	ph7_hashmap *pMap;` |
|      - | 4758 | `	io_private *pDev;` |
|      - | 4759 | `	SySet sArg;` |
|      - | 4760 | `	int n,nLen;` |
|      7 | 4761 | `	if( nArg < 3 ){` |
|    ! 0 | 4762 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4763 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4764 | `		return PH7_OK;` |
|      - | 4765 | `	}` |
|      - | 4766 | `	{` |
|      - | 4767 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|      7 | 4768 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|      7 | 4769 | `		if( rcs != PH7_OK ){` |
|      3 | 4770 | `			return rcs;` |
|      - | 4771 | `		}` |
|      - | 4772 | `	}` |
|      - | 4773 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 4774 | `	{` |
|      5 | 4775 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      5 | 4776 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4777 | `			return rcf;` |
|      - | 4778 | `		}` |
|      - | 4779 | `	}` |
|      5 | 4780 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 4781 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4782 | `		char zBuf[64];` |
|    ! 0 | 4783 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4784 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 4785 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 4786 | `	}` |
|      - | 4787 | `	/* Extract our private data */` |
|      5 | 4788 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4789 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4790 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4791 | `		/*Expecting an IO handle */` |
|    ! 0 | 4792 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4793 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4794 | `		return PH7_OK;` |
|      - | 4795 | `	}` |
|      - | 4796 | `	/* Point to the target IO stream device */` |
|      5 | 4797 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4798 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4799 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4800 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4801 | `			);` |
|    ! 0 | 4802 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4803 | `		return PH7_OK;` |
|      - | 4804 | `	}` |
|      - | 4805 | `	/* Extract the string format */` |
|      5 | 4806 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      5 | 4807 | `	if( nLen < 1 ){` |
|      - | 4808 | `		/* Empty string,return zero */` |
|    ! 0 | 4809 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4810 | `		return PH7_OK;` |
|      - | 4811 | `	}` |
|      - | 4812 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4813 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4814 | `	{` |
|      5 | 4815 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      5 | 4816 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4817 | `			return rcv;` |
|      - | 4818 | `		}` |
|      - | 4819 | `	}` |
|      - | 4820 | `	/* Point to hashmap */` |
|      5 | 4821 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4822 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output. */` |
|      - | 4823 | `	{` |
|      5 | 4824 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      5 | 4825 | `		if( rcc != PH7_OK ){` |
|      3 | 4826 | `			return rcc;` |
|      - | 4827 | `		}` |
|      - | 4828 | `	}` |
|      - | 4829 | `	/* Extract arguments from the hashmap */` |
|      3 | 4830 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4831 | `	/* Prepare our private data */` |
|      3 | 4832 | `	sFdata.nCount = 0;` |
|      3 | 4833 | `	sFdata.pIO = pDev;` |
|      - | 4834 | `	/* Format the string */` |
|      3 | 4835 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4836 | `	/* Return total number of bytes written*/` |
|      3 | 4837 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4838 | `	SySetRelease(&sArg);` |
|      3 | 4839 | `	return PH7_OK;` |
|      4 | 4840 | `}` |
|      - | 4841 | `/*` |
|      - | 4842 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4843 | ` * According to the PHP reference manual:` |
|      - | 4844 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4845 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4846 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4847 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4848 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4849 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4850 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4851 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4852 | ` *         exist, attempt to create it.` |
|      - | 4853 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4854 | ` *          not exist, attempt to create it.` |
|      - | 4855 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4856 | ` *         already exists,` |
|      - | 4857 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4858 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4859 | ` *         the underlying open(2) system call.` |
|      - | 4860 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4861 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4862 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4863 | ` *          is positioned on the beginning of the file.` |
|      - | 4864 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4865 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4866 | ` *          be used after the lock is requested).` |
|      - | 4867 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4868 | ` */` |
|    220 | 4869 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      4 | 4870 | `{` |
|    224 | 4871 | `	const char *zEnd = &zMode[nLen];` |
|    224 | 4872 | `	int iFlag = 0;` |
|      - | 4873 | `	int c;` |
|    224 | 4874 | `	if( nLen < 1 ){` |
|      - | 4875 | `		/* Open in a read-only mode */` |
|    ! 0 | 4876 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4877 | `	}` |
|    224 | 4878 | `	c = zMode[0];` |
|    224 | 4879 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4880 | `		/* Read-only access */` |
|     38 | 4881 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     38 | 4882 | `		zMode++; /* Advance */` |
|     38 | 4883 | `		if( zMode < zEnd ){` |
|     17 | 4884 | `			c = zMode[0];` |
|     17 | 4885 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4886 | `				/* Read+Write access */` |
|     17 | 4887 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      8 | 4888 | `			}` |
|     11 | 4889 | `		}` |
|    196 | 4890 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4891 | `		/* Overwrite mode.` |
|      - | 4892 | `		 * If the file does not exists,try to create it` |
|      - | 4893 | `		 */` |
|     20 | 4894 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     20 | 4895 | `		zMode++; /* Advance */` |
|     20 | 4896 | `		if( zMode < zEnd ){` |
|      5 | 4897 | `			c = zMode[0];` |
|      5 | 4898 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4899 | `				/* Read+Write access */` |
|      5 | 4900 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      5 | 4901 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      2 | 4902 | `			}` |
|      4 | 4903 | `		}` |
|    153 | 4904 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4905 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4906 | `		 * Create the file if it does not exists.` |
|      - | 4907 | `		 */` |
|    ! 0 | 4908 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4909 | `		zMode++; /* Advance */` |
|    ! 0 | 4910 | `		if( zMode < zEnd ){` |
|    ! 0 | 4911 | `			c = zMode[0];` |
|    ! 0 | 4912 | `			if( c == '+' ){` |
|      - | 4913 | `				/* Read-Write access */` |
|    ! 0 | 4914 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4915 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4916 | `			}` |
|    ! 0 | 4917 | `		}` |
|    137 | 4918 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4919 | `		/* Exclusive access.` |
|      - | 4920 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4921 | `		 * Otherwise create a new file.` |
|      - | 4922 | `		 */` |
|     70 | 4923 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|     70 | 4924 | `		zMode++; /* Advance */` |
|     70 | 4925 | `		if( zMode < zEnd ){` |
|    ! 0 | 4926 | `			c = zMode[0];` |
|    ! 0 | 4927 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4928 | `				/* Read-Write access */` |
|    ! 0 | 4929 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4930 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4931 | `			}` |
|      3 | 4932 | `		}` |
|     67 | 4933 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4934 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4935 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4936 | `		zMode++; /* Advance */` |
|    ! 0 | 4937 | `		if( zMode < zEnd ){` |
|    ! 0 | 4938 | `			c = zMode[0];` |
|    ! 0 | 4939 | `			if( c == '+' ){` |
|      - | 4940 | `				/* Read-Write access */` |
|    ! 0 | 4941 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4942 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4943 | `			}` |
|    ! 0 | 4944 | `		}` |
|    ! 0 | 4945 | `	}else{` |
|      - | 4946 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4947 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4948 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4949 | `	}` |
|    244 | 4950 | `	while( zMode < zEnd ){` |
|     21 | 4951 | `		c = zMode[0];` |
|     21 | 4952 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4953 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4954 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     21 | 4955 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4956 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4957 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4958 | `		}` |
|     21 | 4959 | `		zMode++;` |
|      1 | 4960 | `	}` |
|    224 | 4961 | `	return iFlag;` |
|    114 | 4962 | `}` |
|      - | 4963 | `/*` |
|      - | 4964 | ` * Initialize the IO private structure.` |
|      - | 4965 | ` */` |
|   5314 | 4966 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 4967 | `{` |
|   5319 | 4968 | `	pOut->pStream = pStream;` |
|   5319 | 4969 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   5319 | 4970 | `	pOut->nOfft = 0;` |
|      - | 4971 | `	/* Set the magic number */` |
|   5319 | 4972 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   5319 | 4973 | `}` |
|      - | 4974 | `/*` |
|      - | 4975 | ` * Release the IO private structure.` |
|      - | 4976 | ` */` |
|      2 | 4977 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      1 | 4978 | `{` |
|      3 | 4979 | `	SyBlobRelease(&pDev->sBuffer);` |
|      3 | 4980 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4981 | `	/* Release the whole structure */` |
|      3 | 4982 | `	ph7_context_free_chunk(pCtx,pDev);` |
|      3 | 4983 | `}` |
|      - | 4984 | `/*` |
|      - | 4985 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 4986 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 4987 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 4988 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 4989 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 4990 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 4991 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 4992 | ` */` |
|   5256 | 4993 | `static void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 4994 | `{` |
|   5261 | 4995 | `	SyBlobRelease(&pDev->sBuffer);` |
|   5261 | 4996 | `	pDev->pHandle = 0;` |
|   5261 | 4997 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   5261 | 4998 | `}` |
|      - | 4999 | `/*` |
|      - | 5000 | ` * Reset the IO private structure.` |
|      - | 5001 | ` */` |
|     30 | 5002 | `static void ResetIOPrivate(io_private *pDev)` |
|      2 | 5003 | `{` |
|     32 | 5004 | `	SyBlobReset(&pDev->sBuffer);` |
|     32 | 5005 | `	pDev->nOfft = 0;` |
|     32 | 5006 | `}` |
|      - | 5007 | `/* Forward declaration */` |
|      - | 5008 |  |
|      - | 5009 | `/*` |
|      - | 5010 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 5011 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 5012 | ` * Parameters` |
|      - | 5013 | ` *  $filename` |
|      - | 5014 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 5015 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 5016 | ` *   then a regular file is assumed.` |
|      - | 5017 | ` *  $mode` |
|      - | 5018 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 5019 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 5020 | ` *   modes.` |
|      - | 5021 | ` *  $use_include_path` |
|      - | 5022 | ` *   You can use the optional second parameter and set it to` |
|      - | 5023 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 5024 | ` *  $context` |
|      - | 5025 | ` *   A context stream resource.` |
|      - | 5026 | ` * Return` |
|      - | 5027 | ` *  File handle on success or FALSE on failure.` |
|      - | 5028 | ` */` |
|      - | 5029 | `/*` |
|      - | 5030 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 5031 | ` *                                  int $offset = -1)` |
|      - | 5032 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 5033 | ` */` |
|     28 | 5034 | `static int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5035 | `{` |
|      - | 5036 | `	const ph7_io_stream *pStream;` |
|      - | 5037 | `	io_private *pDev;` |
|     29 | 5038 | `	ph7_int64 nMax = -1;` |
|      - | 5039 | `	char zBuf[4096];` |
|      - | 5040 | `	ph7_int64 nRead;` |
|     29 | 5041 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 5042 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5043 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5044 | `		return PH7_OK;` |
|      - | 5045 | `	}` |
|     29 | 5046 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     29 | 5047 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5048 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5049 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5050 | `		return PH7_OK;` |
|      - | 5051 | `	}` |
|     29 | 5052 | `	pStream = pDev->pStream;` |
|     29 | 5053 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 5054 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5055 | `		return PH7_OK;` |
|      - | 5056 | `	}` |
|     29 | 5057 | `	if( nArg > 1 ){` |
|      5 | 5058 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      2 | 5059 | `	}` |
|     17 | 5060 | `	if( nArg > 2 ){` |
|      5 | 5061 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 5062 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 5063 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 5064 | `		}` |
|      2 | 5065 | `	}` |
|     29 | 5066 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     49 | 5067 | `	while( nMax != 0 ){` |
|     47 | 5068 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     47 | 5069 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 5070 | `			nAsk = nMax;` |
|      1 | 5071 | `		}` |
|     47 | 5072 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     47 | 5073 | `		if( nRead < 1 ){` |
|     27 | 5074 | `			break;` |
|      - | 5075 | `		}` |
|     21 | 5076 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     21 | 5077 | `		if( nMax > 0 ){` |
|      3 | 5078 | `			nMax -= nRead;` |
|      1 | 5079 | `		}` |
|      1 | 5080 | `	}` |
|     29 | 5081 | `	return PH7_OK;` |
|     15 | 5082 | `}` |
|      - | 5083 | `/*` |
|      - | 5084 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 5085 | ` */` |
|      4 | 5086 | `static int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5087 | `{` |
|      - | 5088 | `	ph7_value *pArr,*pV;` |
|      - | 5089 | `	ph7_io_stream **apDev;` |
|      - | 5090 | `	sxu32 n;` |
|      2 | 5091 | `	SXUNUSED(nArg);` |
|      2 | 5092 | `	SXUNUSED(apArg);` |
|      6 | 5093 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 5094 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 5095 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 5096 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5097 | `		return PH7_OK;` |
|      - | 5098 | `	}` |
|      6 | 5099 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 5100 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 5101 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 5102 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 5103 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 5104 | `	}` |
|      6 | 5105 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 5106 | `	return PH7_OK;` |
|      4 | 5107 | `}` |
|      - | 5108 | `/*` |
|      - | 5109 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 5110 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 5111 | ` */` |
|      2 | 5112 | `static int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5113 | `{` |
|      - | 5114 | `	io_private *pDev;` |
|      - | 5115 | `	ph7_value *pArr,*pV;` |
|      3 | 5116 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 5117 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5118 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5119 | `		return PH7_OK;` |
|      - | 5120 | `	}` |
|      3 | 5121 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5122 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5123 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5124 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5125 | `		return PH7_OK;` |
|      - | 5126 | `	}` |
|      3 | 5127 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 5128 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 5129 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 5130 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5131 | `		return PH7_OK;` |
|      - | 5132 | `	}` |
|      3 | 5133 | `	ph7_value_bool(pV,0);` |
|      3 | 5134 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 5135 | `	ph7_value_bool(pV,1);` |
|      3 | 5136 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 5137 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 5138 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 5139 | `	ph7_value_bool(pV,0);` |
|      3 | 5140 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 5141 | `	ph7_value_int(pV,0);` |
|      3 | 5142 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 5143 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 5144 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 5145 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 5146 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 5147 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 5148 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 5149 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 5150 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 5151 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 5152 | `	return PH7_OK;` |
|      2 | 5153 | `}` |
|      - | 5154 | `/*` |
|      - | 5155 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 5156 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 5157 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 5158 | ` * resource, options unconsumed).` |
|      - | 5159 | ` */` |
|      2 | 5160 | `static int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5161 | `{` |
|      3 | 5162 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 5163 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 5164 | `	}else{` |
|    ! 0 | 5165 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 5166 | `		if( pArr == 0 ){` |
|    ! 0 | 5167 | `			ph7_result_null(pCtx);` |
|    ! 0 | 5168 | `			return PH7_OK;` |
|      - | 5169 | `		}` |
|    ! 0 | 5170 | `		ph7_result_value(pCtx,pArr);` |
|      - | 5171 | `	}` |
|      3 | 5172 | `	return PH7_OK;` |
|      2 | 5173 | `}` |
|      - | 5174 | `/*` |
|      - | 5175 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 5176 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 5177 | ` */` |
|      - | 5178 | `#ifdef PH7_ENABLE_NET` |
|      - | 5179 | `typedef struct sock_private sock_private;` |
|      - | 5180 | `struct sock_private` |
|      - | 5181 | `{` |
|      - | 5182 | `	ph7_vm *pVm;` |
|      - | 5183 | `	ph7_socket sock;` |
|      - | 5184 | `	int bEof;` |
|      - | 5185 | `};` |
|     10 | 5186 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 5187 | `{` |
|     10 | 5188 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5189 | `	int n;` |
|     10 | 5190 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      2 | 5191 | `		return 0;` |
|      - | 5192 | `	}` |
|      8 | 5193 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|      8 | 5194 | `	if( n <= 0 ){` |
|      4 | 5195 | `		pSock->bEof = 1;` |
|      4 | 5196 | `		return 0;` |
|      - | 5197 | `	}` |
|      4 | 5198 | `	return (ph7_int64)n;` |
|      5 | 5199 | `}` |
|      4 | 5200 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 5201 | `{` |
|      4 | 5202 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5203 | `	int n;` |
|      4 | 5204 | `	if( pSock == 0 ){` |
|    ! 0 | 5205 | `		return -1;` |
|      - | 5206 | `	}` |
|      4 | 5207 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 5208 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 5209 | `}` |
|      4 | 5210 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 5211 | `{` |
|      4 | 5212 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 5213 | `	if( pSock == 0 ){` |
|    ! 0 | 5214 | `		return;` |
|      - | 5215 | `	}` |
|      4 | 5216 | `	PH7_NetClose(pSock->sock);` |
|      4 | 5217 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 5218 | `}` |
|      - | 5219 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 5220 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 5221 | `{` |
|      - | 5222 | `	sock_private *pSock;` |
|      - | 5223 | `	ph7_socket sock;` |
|      - | 5224 | `	char zHost[256];` |
|      - | 5225 | `	const char *zColon;` |
|    ! 0 | 5226 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 5227 | `	const char *zErr = "";` |
|    ! 0 | 5228 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 5229 | `	SXUNUSED(iMode);` |
|    ! 0 | 5230 | `	if( pVm == 0 ){` |
|    ! 0 | 5231 | `		return -1;` |
|      - | 5232 | `	}` |
|    ! 0 | 5233 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 5234 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 5235 | `		zColon--;` |
|    ! 0 | 5236 | `	}` |
|    ! 0 | 5237 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 5238 | `		return -1;` |
|      - | 5239 | `	}` |
|      - | 5240 | `	{` |
|    ! 0 | 5241 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 5242 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 5243 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 5244 | `		}` |
|    ! 0 | 5245 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 5246 | `		zHost[n] = 0;` |
|      - | 5247 | `	}` |
|      - | 5248 | `	{` |
|    ! 0 | 5249 | `		sxi32 iTmp = 0;` |
|    ! 0 | 5250 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 5251 | `		iPort = (int)iTmp;` |
|      - | 5252 | `	}` |
|    ! 0 | 5253 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 5254 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 5255 | `		return -1;` |
|      - | 5256 | `	}` |
|    ! 0 | 5257 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 5258 | `	if( pSock == 0 ){` |
|    ! 0 | 5259 | `		PH7_NetClose(sock);` |
|    ! 0 | 5260 | `		return -1;` |
|      - | 5261 | `	}` |
|    ! 0 | 5262 | `	pSock->pVm = pVm;` |
|    ! 0 | 5263 | `	pSock->sock = sock;` |
|    ! 0 | 5264 | `	pSock->bEof = 0;` |
|    ! 0 | 5265 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 5266 | `	return PH7_OK;` |
|    ! 0 | 5267 | `}` |
|      - | 5268 | `static const ph7_io_stream sTCP_Stream = {` |
|      - | 5269 | `	"tcp",` |
|      - | 5270 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5271 | `	SockStreamData_Open, /* xOpen */` |
|      - | 5272 | `	0,   /* xOpenDir */` |
|      - | 5273 | `	SockStreamData_Close,/* xClose */` |
|      - | 5274 | `	0,  /* xCloseDir */` |
|      - | 5275 | `	SockStreamData_Read, /* xRead */` |
|      - | 5276 | `	0,  /* xReadDir */` |
|      - | 5277 | `	SockStreamData_Write,/* xWrite */` |
|      - | 5278 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 5279 | `	0,  /* xLock */` |
|      - | 5280 | `	0,  /* xRewindDir */` |
|      - | 5281 | `	0,  /* xTell */` |
|      - | 5282 | `	0,  /* xTrunc */` |
|      - | 5283 | `	0,  /* xSync */` |
|      - | 5284 | `	0   /* xStat */` |
|      - | 5285 | `};` |
|      - | 5286 | `#endif /* PH7_ENABLE_NET */` |
|      - | 5287 | `/*` |
|      - | 5288 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 5289 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 5290 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 5291 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 5292 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 5293 | ` * php streamWrapper protocol method on it.` |
|      - | 5294 | ` */` |
|      - | 5295 | `#define PHL_UWRAP_MAX 8` |
|      - | 5296 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 5297 | `struct uwrap_slot` |
|      - | 5298 | `{` |
|      - | 5299 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 5300 | `	char zScheme[32];         /* protocol name */` |
|      - | 5301 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 5302 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 5303 | `};` |
|      - | 5304 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 5305 | `struct uwrap_handle` |
|      - | 5306 | `{` |
|      - | 5307 | `	ph7_vm *pVm;` |
|      - | 5308 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 5309 | `	int iSlot;` |
|      - | 5310 | `	int bEof;` |
|      - | 5311 | `};` |
|      - | 5312 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 5313 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 5314 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 5315 | `	ph7_value *pResult)` |
|      1 | 5316 | `{` |
|      - | 5317 | `	ph7_class_method *pMeth;` |
|     27 | 5318 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 5319 | `		return -1;` |
|      - | 5320 | `	}` |
|     27 | 5321 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 5322 | `	if( pMeth == 0 ){` |
|    ! 0 | 5323 | `		return -1;` |
|      - | 5324 | `	}` |
|     27 | 5325 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 5326 | `		return -1;` |
|      - | 5327 | `	}` |
|     27 | 5328 | `	return 0;` |
|     14 | 5329 | `}` |
|      8 | 5330 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 5331 | `{` |
|      9 | 5332 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5333 | `	ph7_value sArg,sRet;` |
|      - | 5334 | `	const char *zData;` |
|      9 | 5335 | `	int nData = 0;` |
|      9 | 5336 | `	ph7_int64 n = 0;` |
|      9 | 5337 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 5338 | `		return 0;` |
|      - | 5339 | `	}` |
|      9 | 5340 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 5341 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 5342 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 5343 | `	{` |
|      - | 5344 | `		ph7_value *apArg[1];` |
|      9 | 5345 | `		apArg[0] = &sArg;` |
|      9 | 5346 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5347 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5348 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5349 | `			return -1;` |
|      - | 5350 | `		}` |
|      - | 5351 | `	}` |
|      9 | 5352 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 5353 | `	if( nData > 0 ){` |
|      7 | 5354 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 5355 | `			nData = (int)nRead;` |
|    ! 0 | 5356 | `		}` |
|      7 | 5357 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 5358 | `		n = nData;` |
|      4 | 5359 | `	}else{` |
|      3 | 5360 | `		pH->bEof = 1;` |
|      - | 5361 | `	}` |
|      9 | 5362 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 5363 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 5364 | `	return n;` |
|      5 | 5365 | `}` |
|      2 | 5366 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5367 | `{` |
|      3 | 5368 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5369 | `	ph7_value sArg,sRet;` |
|      - | 5370 | `	ph7_int64 n;` |
|      3 | 5371 | `	if( pH == 0 ){` |
|    ! 0 | 5372 | `		return -1;` |
|      - | 5373 | `	}` |
|      3 | 5374 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 5375 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5376 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 5377 | `	{` |
|      - | 5378 | `		ph7_value *apArg[1];` |
|      3 | 5379 | `		apArg[0] = &sArg;` |
|      3 | 5380 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5381 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5382 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5383 | `			return -1;` |
|      - | 5384 | `		}` |
|      - | 5385 | `	}` |
|      3 | 5386 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5387 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 5388 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5389 | `	return n;` |
|      2 | 5390 | `}` |
|      2 | 5391 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5392 | `{` |
|      3 | 5393 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5394 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 5395 | `	ph7_value *apArg[2];` |
|      - | 5396 | `	int rc;` |
|      3 | 5397 | `	if( pH == 0 ){` |
|    ! 0 | 5398 | `		return -1;` |
|      - | 5399 | `	}` |
|      3 | 5400 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 5401 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 5402 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5403 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 5404 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 5405 | `	apArg[0] = &sOfft;` |
|      3 | 5406 | `	apArg[1] = &sWhence;` |
|      3 | 5407 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 5408 | `	if( rc == 0 ){` |
|      3 | 5409 | `		pH->bEof = 0;` |
|      3 | 5410 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 5411 | `	}` |
|      3 | 5412 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 5413 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 5414 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5415 | `	return rc;` |
|      2 | 5416 | `}` |
|      2 | 5417 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 5418 | `{` |
|      3 | 5419 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5420 | `	ph7_value sRet;` |
|      - | 5421 | `	ph7_int64 n;` |
|      3 | 5422 | `	if( pH == 0 ){` |
|    ! 0 | 5423 | `		return -1;` |
|      - | 5424 | `	}` |
|      3 | 5425 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5426 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 5427 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5428 | `		return -1;` |
|      - | 5429 | `	}` |
|      3 | 5430 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5431 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5432 | `	return n;` |
|      2 | 5433 | `}` |
|      6 | 5434 | `static void UwrapClose(void *pHandle)` |
|      1 | 5435 | `{` |
|      7 | 5436 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 5437 | `	if( pH == 0 ){` |
|    ! 0 | 5438 | `		return;` |
|      - | 5439 | `	}` |
|      7 | 5440 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 5441 | `	if( pH->pObj ){` |
|      7 | 5442 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 5443 | `	}` |
|      7 | 5444 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 5445 | `}` |
|      - | 5446 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 5447 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 5448 | `{` |
|      7 | 5449 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 5450 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 5451 | `	ph7_class *pClass;` |
|      - | 5452 | `	uwrap_handle *pH;` |
|      - | 5453 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 5454 | `	ph7_value *apArg[4];` |
|      - | 5455 | `	int rc;` |
|      7 | 5456 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 5457 | `		return -1;` |
|      - | 5458 | `	}` |
|      7 | 5459 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 5460 | `	if( pClass == 0 ){` |
|    ! 0 | 5461 | `		return -1;` |
|      - | 5462 | `	}` |
|      7 | 5463 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 5464 | `	if( pH == 0 ){` |
|    ! 0 | 5465 | `		return -1;` |
|      - | 5466 | `	}` |
|      7 | 5467 | `	pH->pVm = pVm;` |
|      7 | 5468 | `	pH->iSlot = iSlot;` |
|      7 | 5469 | `	pH->bEof = 0;` |
|      7 | 5470 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 5471 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 5472 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5473 | `		return -1;` |
|      - | 5474 | `	}` |
|      - | 5475 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 5476 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 5477 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 5478 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 5479 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 5480 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 5481 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 5482 | `	 * it as "could not be passed by reference"). */` |
|      - | 5483 | `	{` |
|      7 | 5484 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 5485 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 5486 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5487 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5488 | `			return -1;` |
|      - | 5489 | `		}` |
|      7 | 5490 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 5491 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 5492 | `	}` |
|      - | 5493 | `	{` |
|      - | 5494 | `		SyBlob sUrl;` |
|      7 | 5495 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 5496 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 5497 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 5498 | `		SyBlobRelease(&sUrl);` |
|      - | 5499 | `	}` |
|      9 | 5500 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 5501 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 5502 | `	ph7_value_int(&sOpts,0);` |
|      7 | 5503 | `	apArg[0] = &sPath;` |
|      7 | 5504 | `	apArg[1] = &sMode;` |
|      7 | 5505 | `	apArg[2] = &sOpts;` |
|      7 | 5506 | `	apArg[3] = &sOpened;` |
|      7 | 5507 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 5508 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 5509 | `		rc = -1;` |
|    ! 0 | 5510 | `	}` |
|      7 | 5511 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 5512 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 5513 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 5514 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 5515 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 5516 | `	if( rc != 0 ){` |
|    ! 0 | 5517 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5518 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5519 | `		return -1;` |
|      - | 5520 | `	}` |
|      7 | 5521 | `	*ppHandle = (void *)pH;` |
|      7 | 5522 | `	return PH7_OK;` |
|      4 | 5523 | `}` |
|      - | 5524 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 5525 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 5526 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 5527 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 5528 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 5529 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 5530 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 5531 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 5532 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 5533 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 5534 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 5535 | `PHL_UWRAP_THUNK(7)` |
|      - | 5536 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 5537 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 5538 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 5539 | `};` |
|      - | 5540 | `/*` |
|      - | 5541 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 5542 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5543 | ` */` |
|      2 | 5544 | `static int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5545 | `{` |
|      - | 5546 | `	const char *zScheme,*zClass;` |
|      3 | 5547 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 5548 | `	if( nArg < 2 ){` |
|    ! 0 | 5549 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5550 | `		return PH7_OK;` |
|      - | 5551 | `	}` |
|      3 | 5552 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5553 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 5554 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 5555 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 5556 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5557 | `		return PH7_OK;` |
|      - | 5558 | `	}` |
|      - | 5559 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 5560 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 5561 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 5562 | `	{` |
|      3 | 5563 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 5564 | `		sxu32 n;` |
|     11 | 5565 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 5566 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 5567 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 5568 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5569 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 5570 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5571 | `				return PH7_OK;` |
|      - | 5572 | `			}` |
|      5 | 5573 | `		}` |
|      - | 5574 | `	}` |
|      3 | 5575 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 5576 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 5577 | `			iFree = i;` |
|      3 | 5578 | `			break;` |
|      - | 5579 | `		}` |
|    ! 0 | 5580 | `	}` |
|      3 | 5581 | `	if( iFree < 0 ){` |
|    ! 0 | 5582 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5583 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 5584 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5585 | `		return PH7_OK;` |
|      - | 5586 | `	}` |
|      - | 5587 | `	{` |
|      3 | 5588 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 5589 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 5590 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 5591 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 5592 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 5593 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 5594 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 5595 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 5596 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 5597 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 5598 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 5599 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 5600 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 5601 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 5602 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 5603 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 5604 | `	}` |
|      3 | 5605 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5606 | `	return PH7_OK;` |
|      2 | 5607 | `}` |
|      2 | 5608 | `static int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5609 | `{` |
|      - | 5610 | `	const char *zScheme;` |
|      - | 5611 | `	int nScheme,i;` |
|      3 | 5612 | `	if( nArg < 1 ){` |
|    ! 0 | 5613 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5614 | `		return PH7_OK;` |
|      - | 5615 | `	}` |
|      3 | 5616 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5617 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 5618 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 5619 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 5620 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 5621 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 5622 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 5623 | `			 * what unregister means to a script — recorded. */` |
|      3 | 5624 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 5625 | `			ph7_result_bool(pCtx,1);` |
|      3 | 5626 | `			return PH7_OK;` |
|      - | 5627 | `		}` |
|    ! 0 | 5628 | `	}` |
|    ! 0 | 5629 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 5630 | `	return PH7_OK;` |
|      2 | 5631 | `}` |
|      - | 5632 | `#ifdef PH7_ENABLE_NET` |
|      - | 5633 | `/*` |
|      - | 5634 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 5635 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 5636 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 5637 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 5638 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 5639 | ` */` |
|      6 | 5640 | `static int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 5641 | `{` |
|      6 | 5642 | `	const char *zFunc = ph7_function_name(pCtx);` |
|      6 | 5643 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|      6 | 5644 | `	const char *zTarget,*zErr = "";` |
|      - | 5645 | `	char zHost[256];` |
|      6 | 5646 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 5647 | `	ph7_socket sock;` |
|      - | 5648 | `	io_private *pDev;` |
|      - | 5649 | `	sock_private *pSock;` |
|      6 | 5650 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|      6 | 5651 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|      6 | 5652 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|      6 | 5653 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 5654 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5655 | `		return PH7_OK;` |
|      - | 5656 | `	}` |
|      6 | 5657 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 5658 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 5659 | `	{` |
|      6 | 5660 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|      6 | 5661 | `		const char *zSep = 0;` |
|     32 | 5662 | `		while( z < zEnd - 2 ){` |
|     30 | 5663 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 5664 | `				zSep = z;` |
|      4 | 5665 | `				break;` |
|      - | 5666 | `			}` |
|     26 | 5667 | `			z++;` |
|    ! 0 | 5668 | `		}` |
|      5 | 5669 | `		if( zSep ){` |
|      4 | 5670 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 5671 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5672 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 5673 | `					nTarget,zTarget);` |
|    ! 0 | 5674 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5675 | `				return PH7_OK;` |
|      - | 5676 | `			}` |
|      4 | 5677 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 5678 | `			zTarget = zSep + 3;` |
|      2 | 5679 | `		}` |
|      - | 5680 | `	}` |
|      - | 5681 | `	/* host[:port] */` |
|      - | 5682 | `	{` |
|      6 | 5683 | `		int i = nTarget - 1;` |
|      6 | 5684 | `		int nHost = nTarget;` |
|     48 | 5685 | `		while( i > 0 && zTarget[i] != ':' ){` |
|     42 | 5686 | `			i--;` |
|    ! 0 | 5687 | `		}` |
|      6 | 5688 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 5689 | `			sxi32 iTmp = 0;` |
|      2 | 5690 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 5691 | `			iPort = (int)iTmp;` |
|      2 | 5692 | `			nHost = i;` |
|      1 | 5693 | `		}` |
|      5 | 5694 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 5695 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 5696 | `		}` |
|      5 | 5697 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|      5 | 5698 | `		zHost[nHost] = 0;` |
|      - | 5699 | `	}` |
|      5 | 5700 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      4 | 5701 | `		iPort = ph7_value_to_int(apArg[1]);` |
|      2 | 5702 | `	}` |
|      6 | 5703 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|      6 | 5704 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|      6 | 5705 | `		if( rTimeout > 0 ){` |
|      6 | 5706 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|      3 | 5707 | `		}` |
|      3 | 5708 | `	}` |
|      6 | 5709 | `	if( iPort < 0 ){` |
|    ! 0 | 5710 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5711 | `		return PH7_OK;` |
|      - | 5712 | `	}` |
|      6 | 5713 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|      6 | 5714 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5715 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 5716 | `		{` |
|      2 | 5717 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      2 | 5718 | `			if( pTmp ){` |
|      2 | 5719 | `				if( nArg > iArgErrno ){` |
|      2 | 5720 | `					ph7_value_int(pTmp,iErrno);` |
|      2 | 5721 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      1 | 5722 | `				}` |
|      2 | 5723 | `				if( nArg > iArgErrstr ){` |
|      2 | 5724 | `					ph7_value_string(pTmp,zErr,-1);` |
|      2 | 5725 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      1 | 5726 | `				}` |
|      1 | 5727 | `			}` |
|      - | 5728 | `		}` |
|      - | 5729 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|      3 | 5730 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 5731 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|      2 | 5732 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5733 | `		return PH7_OK;` |
|      - | 5734 | `	}` |
|      - | 5735 | `	{` |
|      4 | 5736 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 5737 | `		if( pTmp ){` |
|      4 | 5738 | `			if( nArg > iArgErrno ){` |
|      4 | 5739 | `				ph7_value_int(pTmp,0);` |
|      4 | 5740 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 5741 | `			}` |
|      4 | 5742 | `			if( nArg > iArgErrstr ){` |
|      4 | 5743 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 5744 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 5745 | `			}` |
|      2 | 5746 | `		}` |
|      - | 5747 | `	}` |
|      - | 5748 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 5749 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 5750 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 5751 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 5752 | `		PH7_NetClose(sock);` |
|    ! 0 | 5753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5754 | `		return PH7_OK;` |
|      - | 5755 | `	}` |
|      4 | 5756 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 5757 | `	pSock->sock = sock;` |
|      4 | 5758 | `	pSock->bEof = 0;` |
|      4 | 5759 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 5760 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 5761 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 5762 | `	return PH7_OK;` |
|      3 | 5763 | `}` |
|      - | 5764 | `#endif /* PH7_ENABLE_NET */` |
|    220 | 5765 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5766 | `{` |
|      - | 5767 | `	const ph7_io_stream *pStream;` |
|      - | 5768 | `	const char *zUri,*zMode;` |
|      - | 5769 | `	ph7_value *pResource;` |
|      - | 5770 | `	io_private *pDev;` |
|      - | 5771 | `	int iLen,imLen;` |
|      - | 5772 | `	int iOpenFlags;` |
|    224 | 5773 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5774 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5775 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 5776 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5777 | `		return PH7_OK;` |
|      - | 5778 | `	}` |
|      - | 5779 | `	/* Extract the URI and the desired access mode */` |
|    224 | 5780 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    224 | 5781 | `	if( nArg > 1 ){` |
|    224 | 5782 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    114 | 5783 | `	}else{` |
|      - | 5784 | `		/* Set a default read-only mode */` |
|    ! 0 | 5785 | `		zMode = "r";` |
|    ! 0 | 5786 | `		imLen = (int)sizeof(char);` |
|      - | 5787 | `	}` |
|      - | 5788 | `	/* Try to extract a stream */` |
|    224 | 5789 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    224 | 5790 | `	if( pStream == 0 ){` |
|    ! 0 | 5791 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5792 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 5793 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5794 | `		return PH7_OK;` |
|      - | 5795 | `	}` |
|      - | 5796 | `	/* Allocate a new IO private instance */` |
|    224 | 5797 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    224 | 5798 | `	if( pDev == 0 ){` |
|    ! 0 | 5799 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5800 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5801 | `		return PH7_OK;` |
|      - | 5802 | `	}` |
|    224 | 5803 | `	pResource = 0;` |
|    224 | 5804 | `	if( nArg > 3 ){` |
|    ! 0 | 5805 | `		pResource = apArg[3];` |
|    224 | 5806 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 5807 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 5808 | `		 * access the underlying virtual machine.` |
|      - | 5809 | `		 */` |
|     19 | 5810 | `		pResource = apArg[0];` |
|      9 | 5811 | `	}` |
|      - | 5812 | `	/* Initialize the structure */` |
|    224 | 5813 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 5814 | `	/* Convert open mode to PH7 flags */` |
|    224 | 5815 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 5816 | `	/* Try to get a handle */` |
|    334 | 5817 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    110 | 5818 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|    224 | 5819 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 5820 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|    ! 0 | 5821 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5822 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 5823 | `		return PH7_OK;` |
|      - | 5824 | `	}` |
|      - | 5825 | `	/* All done,return the io_private instance as a resource */` |
|    224 | 5826 | `	ph7_result_resource(pCtx,pDev);` |
|    224 | 5827 | `	return PH7_OK;` |
|    114 | 5828 | `}` |
|      - | 5829 | `/*` |
|      - | 5830 | ` * bool fclose(resource $handle)` |
|      - | 5831 | ` *  Closes an open file pointer` |
|      - | 5832 | ` * Parameters` |
|      - | 5833 | ` *  $handle` |
|      - | 5834 | ` *   The file pointer.` |
|      - | 5835 | ` * Return` |
|      - | 5836 | ` *  TRUE on success or FALSE on failure.` |
|      - | 5837 | ` */` |
|    350 | 5838 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5839 | `{` |
|      - | 5840 | `	const ph7_io_stream *pStream;` |
|      - | 5841 | `	io_private *pDev;` |
|      - | 5842 | `	ph7_vm *pVm;` |
|    355 | 5843 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5844 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5845 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5846 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5847 | `		return PH7_OK;` |
|      - | 5848 | `	}` |
|      - | 5849 | `	/* Extract our private data */` |
|    355 | 5850 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5851 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|    355 | 5852 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 5853 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5854 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 5855 | `	}` |
|      - | 5856 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    353 | 5857 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 5858 | `		/*Expecting an IO handle */` |
|    ! 0 | 5859 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5860 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5861 | `		return PH7_OK;` |
|      - | 5862 | `	}` |
|      - | 5863 | `	/* Point to the target IO stream device */` |
|    353 | 5864 | `	pStream = pDev->pStream;` |
|    353 | 5865 | `	if( pStream == 0 ){` |
|    ! 0 | 5866 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5867 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 5868 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 5869 | `			);` |
|    ! 0 | 5870 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5871 | `		return PH7_OK;` |
|      - | 5872 | `	}` |
|      - | 5873 | `	/* Point to the VM that own this context */` |
|    353 | 5874 | `	pVm = pCtx->pVm;` |
|      - | 5875 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    353 | 5876 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 5877 | `		/* Perform the requested operation */` |
|    353 | 5878 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 5879 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|    353 | 5880 | `		MarkIOPrivateClosed(pDev);` |
|    174 | 5881 | `	}` |
|      - | 5882 | `	/* Return TRUE */` |
|    353 | 5883 | `	ph7_result_bool(pCtx,1);` |
|    353 | 5884 | `	return PH7_OK;` |
|    180 | 5885 | `}` |
|      - | 5886 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5887 | `/*` |
|      - | 5888 | ` * MD5/SHA1 digest consumer.` |
|      - | 5889 | ` */` |
|     72 | 5890 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 5891 | `{` |
|      - | 5892 | `	/* Append hex chunk verbatim */` |
|     73 | 5893 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 5894 | `	return SXRET_OK;` |
|      1 | 5895 | `}` |
|      - | 5896 | `/*` |
|      - | 5897 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 5898 | ` *  Calculates the md5 hash of a given file.` |
|      - | 5899 | ` * Parameters` |
|      - | 5900 | ` *  $uri` |
|      - | 5901 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5902 | ` *  $raw_output` |
|      - | 5903 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 5904 | ` * Return` |
|      - | 5905 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 5906 | ` */` |
|      2 | 5907 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5908 | `{` |
|      - | 5909 | `	const ph7_io_stream *pStream;` |
|      - | 5910 | `	unsigned char zDigest[16];` |
|      3 | 5911 | `	int raw_output  = FALSE;` |
|      - | 5912 | `	const char *zFile;` |
|      - | 5913 | `	MD5Context sCtx;` |
|      - | 5914 | `	char zBuf[8192];` |
|      - | 5915 | `	void *pHandle;` |
|      - | 5916 | `	ph7_int64 n;` |
|      - | 5917 | `	int nLen;` |
|      3 | 5918 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5919 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5920 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5921 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5922 | `		return PH7_OK;` |
|      - | 5923 | `	}` |
|      - | 5924 | `	/* Extract the file path */` |
|      3 | 5925 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5926 | `	/* Point to the target IO stream device */` |
|      3 | 5927 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5928 | `	if( pStream == 0 ){` |
|    ! 0 | 5929 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5930 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5931 | `		return PH7_OK;` |
|      - | 5932 | `	}` |
|      3 | 5933 | `	if( nArg > 1 ){` |
|    ! 0 | 5934 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5935 | `	}` |
|      - | 5936 | `	/* Try to open the file in read-only mode */` |
|      3 | 5937 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5938 | `	if( pHandle == 0 ){` |
|    ! 0 | 5939 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 5940 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5941 | `		return PH7_OK;` |
|      - | 5942 | `	}` |
|      - | 5943 | `	/* Init the MD5 context */` |
|      3 | 5944 | `	MD5Init(&sCtx);` |
|      - | 5945 | `	/* Perform the requested operation */` |
|      2 | 5946 | `	for(;;){` |
|      5 | 5947 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5948 | `		if( n < 1 ){` |
|      - | 5949 | `			/* EOF or IO error,break immediately */` |
|      3 | 5950 | `			break;` |
|      - | 5951 | `		}` |
|      3 | 5952 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5953 | `	}` |
|      - | 5954 | `	/* Close the stream */` |
|      3 | 5955 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5956 | `	/* Extract the digest */` |
|      3 | 5957 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 5958 | `	if( raw_output ){` |
|      - | 5959 | `		/* Output raw digest */` |
|    ! 0 | 5960 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5961 | `	}else{` |
|      - | 5962 | `		/* Perform a binary to hex conversion */` |
|      3 | 5963 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5964 | `	}` |
|      3 | 5965 | `	return PH7_OK;` |
|      2 | 5966 | `}` |
|      - | 5967 | `/*` |
|      - | 5968 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 5969 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 5970 | ` * Parameters` |
|      - | 5971 | ` *  $uri` |
|      - | 5972 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5973 | ` *  $raw_output` |
|      - | 5974 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 5975 | ` * Return` |
|      - | 5976 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 5977 | ` */` |
|      2 | 5978 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5979 | `{` |
|      - | 5980 | `	const ph7_io_stream *pStream;` |
|      - | 5981 | `	unsigned char zDigest[20];` |
|      3 | 5982 | `	int raw_output  = FALSE;` |
|      - | 5983 | `	const char *zFile;` |
|      - | 5984 | `	SHA1Context sCtx;` |
|      - | 5985 | `	char zBuf[8192];` |
|      - | 5986 | `	void *pHandle;` |
|      - | 5987 | `	ph7_int64 n;` |
|      - | 5988 | `	int nLen;` |
|      3 | 5989 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5990 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5991 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5992 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5993 | `		return PH7_OK;` |
|      - | 5994 | `	}` |
|      - | 5995 | `	/* Extract the file path */` |
|      3 | 5996 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5997 | `	/* Point to the target IO stream device */` |
|      3 | 5998 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5999 | `	if( pStream == 0 ){` |
|    ! 0 | 6000 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 6001 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6002 | `		return PH7_OK;` |
|      - | 6003 | `	}` |
|      3 | 6004 | `	if( nArg > 1 ){` |
|    ! 0 | 6005 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 6006 | `	}` |
|      - | 6007 | `	/* Try to open the file in read-only mode */` |
|      3 | 6008 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 6009 | `	if( pHandle == 0 ){` |
|    ! 0 | 6010 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 6011 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6012 | `		return PH7_OK;` |
|      - | 6013 | `	}` |
|      - | 6014 | `	/* Init the SHA1 context */` |
|      3 | 6015 | `	SHA1Init(&sCtx);` |
|      - | 6016 | `	/* Perform the requested operation */` |
|      2 | 6017 | `	for(;;){` |
|      5 | 6018 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 6019 | `		if( n < 1 ){` |
|      - | 6020 | `			/* EOF or IO error,break immediately */` |
|      3 | 6021 | `			break;` |
|      - | 6022 | `		}` |
|      3 | 6023 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 6024 | `	}` |
|      - | 6025 | `	/* Close the stream */` |
|      3 | 6026 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 6027 | `	/* Extract the digest */` |
|      3 | 6028 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 6029 | `	if( raw_output ){` |
|      - | 6030 | `		/* Output raw digest */` |
|    ! 0 | 6031 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 6032 | `	}else{` |
|      - | 6033 | `		/* Perform a binary to hex conversion */` |
|      3 | 6034 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 6035 | `	}` |
|      3 | 6036 | `	return PH7_OK;` |
|      2 | 6037 | `}` |
|      - | 6038 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6039 | `/*` |
|      - | 6040 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 6041 | ` *  Parse a configuration file.` |
|      - | 6042 | ` * Parameters` |
|      - | 6043 | ` * $filename` |
|      - | 6044 | ` *  The filename of the ini file being parsed.` |
|      - | 6045 | ` * $process_sections` |
|      - | 6046 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 6047 | ` *  with the section names and settings included.` |
|      - | 6048 | ` *  The default for process_sections is FALSE.` |
|      - | 6049 | ` * $scanner_mode` |
|      - | 6050 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 6051 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 6052 | ` * Return` |
|      - | 6053 | ` *  The settings are returned as an associative array on success.` |
|      - | 6054 | ` *  Otherwise is returned.` |
|      - | 6055 | ` */` |
|      2 | 6056 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6057 | `{` |
|      - | 6058 | `	const ph7_io_stream *pStream;` |
|      - | 6059 | `	const char *zFile;` |
|      - | 6060 | `	SyBlob sContents;` |
|      - | 6061 | `	void *pHandle;` |
|      - | 6062 | `	int nLen;` |
|      3 | 6063 | `	sxi32 rc = PH7_OK;` |
|      3 | 6064 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6065 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 6066 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 6067 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6068 | `		return PH7_OK;` |
|      - | 6069 | `	}` |
|      - | 6070 | `	/* Extract the file path */` |
|      3 | 6071 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 6072 | `	/* Point to the target IO stream device */` |
|      3 | 6073 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 6074 | `	if( pStream == 0 ){` |
|    ! 0 | 6075 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 6076 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6077 | `		return PH7_OK;` |
|      - | 6078 | `	}` |
|      - | 6079 | `	/* Try to open the file in read-only mode */` |
|      3 | 6080 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 6081 | `	if( pHandle == 0 ){` |
|    ! 0 | 6082 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 6083 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6084 | `		return PH7_OK;` |
|      - | 6085 | `	}` |
|      3 | 6086 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 6087 | `	/* Read the whole file */` |
|      3 | 6088 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 6089 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 6090 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 6091 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6092 | `	}else{` |
|      - | 6093 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 6094 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 6095 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 6096 | `	}` |
|      - | 6097 | `	/* Close the stream */` |
|      3 | 6098 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 6099 | `	/* Release the working buffer */` |
|      3 | 6100 | `	SyBlobRelease(&sContents);` |
|      - | 6101 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 6102 | `	return rc;` |
|      2 | 6103 | `}` |
|      - | 6104 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 6105 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 6106 | `/*` |
|      - | 6107 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 6108 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 6109 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 6110 | ` * to any non-VFS resource).` |
|      - | 6111 | ` */` |
|      - | 6112 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 6113 | `{` |
|      - | 6114 | `	SXUNUSED(pResource);` |
|      - | 6115 | `	return "Unknown";` |
|      - | 6116 | `}` |
|      - | 6117 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 6118 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 6119 | `{` |
|      - | 6120 | `	SXUNUSED(pResource);` |
|      - | 6121 | `	return 0;` |
|      - | 6122 | `}` |
|      - | 6123 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 6124 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 6125 | `#if defined(_MSC_VER)` |
|      - | 6126 | `static const ph7_vfs null_vfs = {` |
|      - | 6127 | `#else` |
|      - | 6128 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 6129 | `#endif` |
|      - | 6130 | `	"null_vfs",` |
|      - | 6131 | `	PH7_VFS_VERSION,` |
|      - | 6132 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 6133 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 6134 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 6135 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 6136 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 6137 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 6138 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 6139 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 6140 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 6141 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 6142 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 6143 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 6144 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 6145 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 6146 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 6147 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 6148 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 6149 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 6150 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 6151 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 6152 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6153 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6154 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 6155 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 6156 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 6157 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 6158 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 6159 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 6160 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 6161 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 6162 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 6163 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 6164 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 6165 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 6166 | `	0, /* int (*xUmask)(int) */` |
|      - | 6167 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 6168 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 6169 | `	0, /* int (*xUid)(void) */` |
|      - | 6170 | `	0, /* int (*xGid)(void) */` |
|      - | 6171 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 6172 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 6173 | `};` |
|      - | 6174 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 6175 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 6176 | `/*` |
|      - | 6177 | ` * Export the builtin vfs.` |
|      - | 6178 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 6179 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 6180 | ` * Note:` |
|      - | 6181 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 6182 | ` * Note:` |
|      - | 6183 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6184 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 6185 | ` */` |
|   3912 | 6186 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 6187 | `{` |
|      - | 6188 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6189 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 6190 | `	return &null_vfs;` |
|      - | 6191 | `#else` |
|      - | 6192 | `#ifdef __WINNT__` |
|      5 | 6193 | `	return &sWinVfs;` |
|      - | 6194 | `#elif defined(__UNIXES__)` |
|   3912 | 6195 | `	return &sUnixVfs;` |
|      - | 6196 | `#else` |
|      - | 6197 | `	return &null_vfs;` |
|      - | 6198 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 6199 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 6200 | `#else` |
|      - | 6201 | `	return &null_vfs;` |
|      - | 6202 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 6203 | `}` |
|      - | 6204 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6205 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6206 | `/*` |
|      - | 6207 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 6208 | ` * no particular meaning on windows.` |
|      - | 6209 | ` */` |
|      - | 6210 | `#ifndef STDIN_FILENO` |
|      - | 6211 | `#define STDIN_FILENO	0` |
|      - | 6212 | `#endif` |
|      - | 6213 | `#ifndef STDOUT_FILENO` |
|      - | 6214 | `#define STDOUT_FILENO	1` |
|      - | 6215 | `#endif` |
|      - | 6216 | `#ifndef STDERR_FILENO` |
|      - | 6217 | `#define STDERR_FILENO	2` |
|      - | 6218 | `#endif` |
|      - | 6219 | `/*` |
|      - | 6220 | ` * php:// Accessing various I/O streams` |
|      - | 6221 | ` * According to the PHP langage reference manual` |
|      - | 6222 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 6223 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 6224 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 6225 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 6226 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 6227 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 6228 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 6229 | ` * php://output` |
|      - | 6230 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 6231 | ` *  mechanism in the same way as print and echo.` |
|      - | 6232 | ` */` |
|      - | 6233 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 6234 | `/* Supported IO streams */` |
|      - | 6235 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 6236 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 6237 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 6238 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 6239 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|      - | 6240 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 6241 | `struct ph7_stream_data` |
|      - | 6242 | `{` |
|      - | 6243 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 6244 | `	int iType;   /* Stream type */` |
|      - | 6245 | `	union{` |
|      - | 6246 | `		void *pHandle; /* Stream handle */` |
|      - | 6247 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 6248 | `	}x;` |
|      - | 6249 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|      - | 6250 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|      - | 6251 | `	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */` |
|      - | 6252 | `};` |
|      - | 6253 | `/*` |
|      - | 6254 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 6255 | ` */` |
|     40 | 6256 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 6257 | `{` |
|      - | 6258 | `	ph7_stream_data *pData;` |
|     41 | 6259 | `	if( pVm == 0 ){` |
|    ! 0 | 6260 | `		return 0;` |
|      - | 6261 | `	}` |
|      - | 6262 | `	/* Allocate a new instance */` |
|     41 | 6263 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|     41 | 6264 | `	if( pData == 0 ){` |
|    ! 0 | 6265 | `		return 0;` |
|      - | 6266 | `	}` |
|      - | 6267 | `	/* Zero the structure */` |
|     41 | 6268 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 6269 | `	/* Initialize fields */` |
|     41 | 6270 | `	pData->iType = iType;` |
|     41 | 6271 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|     41 | 6272 | `	pData->nCur = 0;` |
|     41 | 6273 | `	pData->bReadOnly = 0;` |
|     41 | 6274 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6275 | `		/* Nothing else to set up: the buffer is the stream */` |
|     30 | 6276 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 6277 | `		/* Point to the default VM consumer routine. */` |
|      3 | 6278 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 6279 | `	}else{` |
|      - | 6280 | `#ifdef __WINNT__` |
|      - | 6281 | `		DWORD nChannel;` |
|      1 | 6282 | `		switch(iType){` |
|      1 | 6283 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 6284 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 6285 | `		default:` |
|      1 | 6286 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 6287 | `			break;` |
|      - | 6288 | `		}` |
|      1 | 6289 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 6290 | `#else` |
|      - | 6291 | `		/* Assume an UNIX system */` |
|     16 | 6292 | `		int ifd = STDIN_FILENO;` |
|     16 | 6293 | `		switch(iType){` |
|      6 | 6294 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      8 | 6295 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 6296 | `		default:` |
|      2 | 6297 | `			break;` |
|      - | 6298 | `		}` |
|     16 | 6299 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 6300 | `#endif` |
|      - | 6301 | `	}` |
|     41 | 6302 | `	pData->pVm = pVm;` |
|     41 | 6303 | `	return pData;` |
|     21 | 6304 | `}` |
|      - | 6305 | `/*` |
|      - | 6306 | ` * Implementation of the php:// IO streams routines` |
|      - | 6307 | ` * Status:` |
|      - | 6308 | ` *   Stable.` |
|      - | 6309 | ` */` |
|      - | 6310 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|     14 | 6311 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6312 | `{` |
|      - | 6313 | `	ph7_stream_data *pData;` |
|      - | 6314 | `	SyString sStream;` |
|     15 | 6315 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 6316 | `	/* Trim leading and trailing white spaces */` |
|     15 | 6317 | `	SyStringFullTrim(&sStream);` |
|      - | 6318 | `	/* Stream to open */` |
|     15 | 6319 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 6320 | `		iMode = PH7_IO_STREAM_STDIN;` |
|     15 | 6321 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 6322 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|     14 | 6323 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 6324 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|     13 | 6325 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 6326 | `		iMode = PH7_IO_STREAM_STDERR;` |
|     12 | 6327 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|      8 | 6328 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|      - | 6329 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|      - | 6330 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|     13 | 6331 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|      7 | 6332 | `	}else{` |
|      - | 6333 | `		/* unknown stream name */` |
|    ! 0 | 6334 | `		return -1;` |
|      - | 6335 | `	}` |
|      - | 6336 | `	/* Create our handle */` |
|     15 | 6337 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|     15 | 6338 | `	if( pData == 0 ){` |
|    ! 0 | 6339 | `		return -1;` |
|      - | 6340 | `	}` |
|      - | 6341 | `	/* Make the handle public */` |
|     15 | 6342 | `	*ppHandle = (void *)pData;` |
|     15 | 6343 | `	return PH7_OK;` |
|      8 | 6344 | `}` |
|      - | 6345 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|     42 | 6346 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|      1 | 6347 | `{` |
|     43 | 6348 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     43 | 6349 | `	if( pData == 0 ){` |
|    ! 0 | 6350 | `		return -1;` |
|      - | 6351 | `	}` |
|     43 | 6352 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     43 | 6353 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|      - | 6354 | `		sxu32 nRead;` |
|     43 | 6355 | `		if( pData->nCur >= nAvail ){` |
|     15 | 6356 | `			return 0; /* EOF */` |
|      - | 6357 | `		}` |
|     29 | 6358 | `		nRead = nAvail - pData->nCur;` |
|     29 | 6359 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|      7 | 6360 | `			nRead = (sxu32)nDatatoRead;` |
|      3 | 6361 | `		}` |
|     29 | 6362 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|     29 | 6363 | `		pData->nCur += nRead;` |
|     29 | 6364 | `		return (ph7_int64)nRead;` |
|      - | 6365 | `	}` |
|    ! 0 | 6366 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 6367 | `		/* Forbidden */` |
|    ! 0 | 6368 | `		return -1;` |
|      - | 6369 | `	}` |
|      - | 6370 | `#ifdef __WINNT__` |
|      - | 6371 | `	{` |
|      - | 6372 | `		DWORD nRd;` |
|      - | 6373 | `		BOOL rc;` |
|    ! 0 | 6374 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 6375 | `		if( !rc ){` |
|      - | 6376 | `			/* IO error */` |
|    ! 0 | 6377 | `			return -1;` |
|      - | 6378 | `		}` |
|    ! 0 | 6379 | `		return (ph7_int64)nRd;` |
|      - | 6380 | `	}` |
|      - | 6381 | `#elif defined(__UNIXES__)` |
|      - | 6382 | `	{` |
|      - | 6383 | `		ssize_t nRd;` |
|      - | 6384 | `		int fd;` |
|    ! 0 | 6385 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6386 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 6387 | `		if( nRd < 1 ){` |
|    ! 0 | 6388 | `			return -1;` |
|      - | 6389 | `		}` |
|    ! 0 | 6390 | `		return (ph7_int64)nRd;` |
|      - | 6391 | `	}` |
|      - | 6392 | `#else` |
|      - | 6393 | `	return -1;` |
|      - | 6394 | `#endif` |
|     22 | 6395 | `}` |
|      - | 6396 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|     22 | 6397 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 6398 | `{` |
|     23 | 6399 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     23 | 6400 | `	if( pData == 0 ){` |
|    ! 0 | 6401 | `		return -1;` |
|      - | 6402 | `	}` |
|     23 | 6403 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 6404 | `		/* Forbidden */` |
|    ! 0 | 6405 | `		return -1;` |
|     23 | 6406 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6407 | `		sxu32 nLen,nEnd;` |
|     11 | 6408 | `		if( pData->bReadOnly ){` |
|    ! 0 | 6409 | `			return -1;` |
|      - | 6410 | `		}` |
|     11 | 6411 | `		nLen = SyBlobLength(&pData->sMem);` |
|     11 | 6412 | `		if( pData->nCur > nLen ){` |
|      - | 6413 | `			/* seek past end: php zero-fills the gap */` |
|      - | 6414 | `			static const char zZero[64] = {0};` |
|    ! 0 | 6415 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|    ! 0 | 6416 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|    ! 0 | 6417 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6418 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6419 | `					return -1;` |
|      - | 6420 | `				}` |
|    ! 0 | 6421 | `			}` |
|    ! 0 | 6422 | `			nLen = SyBlobLength(&pData->sMem);` |
|    ! 0 | 6423 | `		}` |
|     11 | 6424 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|     11 | 6425 | `		if( pData->nCur < nLen ){` |
|      - | 6426 | `			/* overwrite in place up to the current end */` |
|      3 | 6427 | `			sxu32 nOver = nLen - pData->nCur;` |
|      3 | 6428 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|      4 | 6429 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|      4 | 6430 | `			if( nEnd > nLen ){` |
|    ! 0 | 6431 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|    ! 0 | 6432 | `					return -1;` |
|      - | 6433 | `				}` |
|    ! 0 | 6434 | `			}` |
|      2 | 6435 | `		}else{` |
|      9 | 6436 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|    ! 0 | 6437 | `				return -1;` |
|      - | 6438 | `			}` |
|      - | 6439 | `		}` |
|     11 | 6440 | `		pData->nCur = nEnd;` |
|     11 | 6441 | `		return nWrite;` |
|     13 | 6442 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 6443 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 6444 | `		int rc;` |
|      - | 6445 | `		/* Call the vm output consumer */` |
|      3 | 6446 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 6447 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 6448 | `			return -1;` |
|      - | 6449 | `		}` |
|      3 | 6450 | `		return nWrite;` |
|      - | 6451 | `	}` |
|      - | 6452 | `#ifdef __WINNT__` |
|      - | 6453 | `	{` |
|      - | 6454 | `		DWORD nWr;` |
|      - | 6455 | `		BOOL rc;` |
|    ! 0 | 6456 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 6457 | `		if( !rc ){` |
|      - | 6458 | `			/* IO error */` |
|    ! 0 | 6459 | `			return -1;` |
|      - | 6460 | `		}` |
|    ! 0 | 6461 | `		return (ph7_int64)nWr;` |
|      - | 6462 | `	}` |
|      - | 6463 | `#elif defined(__UNIXES__)` |
|      - | 6464 | `	{` |
|      - | 6465 | `		ssize_t nWr;` |
|      - | 6466 | `		int fd;` |
|     10 | 6467 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|     10 | 6468 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|     10 | 6469 | `		if( nWr < 1 ){` |
|    ! 0 | 6470 | `			return -1;` |
|      - | 6471 | `		}` |
|     10 | 6472 | `		return (ph7_int64)nWr;` |
|      - | 6473 | `	}` |
|      - | 6474 | `#else` |
|      - | 6475 | `	return -1;` |
|      - | 6476 | `#endif` |
|     12 | 6477 | `}` |
|      - | 6478 | `/* void (*xClose)(void *) */` |
|     20 | 6479 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 6480 | `{` |
|     21 | 6481 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6482 | `	ph7_vm *pVm;` |
|     21 | 6483 | `	if( pData == 0 ){` |
|    ! 0 | 6484 | `		return;` |
|      - | 6485 | `	}` |
|     21 | 6486 | `	pVm = pData->pVm;` |
|     21 | 6487 | `	SyBlobRelease(&pData->sMem);` |
|      - | 6488 | `	/* Free the instance */` |
|     21 | 6489 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|     11 | 6490 | `}` |
|      - | 6491 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|     20 | 6492 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 6493 | `{` |
|     21 | 6494 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6495 | `	ph7_int64 iNew;` |
|     21 | 6496 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6497 | `		return -1;` |
|      - | 6498 | `	}` |
|     21 | 6499 | `	switch(whence){` |
|    ! 0 | 6500 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|      3 | 6501 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|     19 | 6502 | `	default:            iNew = iOfft; break;` |
|      - | 6503 | `	}` |
|     21 | 6504 | `	if( iNew < 0 ){` |
|    ! 0 | 6505 | `		return -1;` |
|      - | 6506 | `	}` |
|     21 | 6507 | `	pData->nCur = (sxu32)iNew;` |
|     21 | 6508 | `	return PH7_OK;` |
|     11 | 6509 | `}` |
|      - | 6510 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|      4 | 6511 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|      1 | 6512 | `{` |
|      5 | 6513 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      5 | 6514 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6515 | `		return -1;` |
|      - | 6516 | `	}` |
|      5 | 6517 | `	return (ph7_int64)pData->nCur;` |
|      3 | 6518 | `}` |
|      - | 6519 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|    ! 0 | 6520 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|    ! 0 | 6521 | `{` |
|    ! 0 | 6522 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 6523 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|    ! 0 | 6524 | `		return -1;` |
|      - | 6525 | `	}` |
|    ! 0 | 6526 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|      - | 6527 | `		/* shrink in place: the blob keeps its allocation */` |
|    ! 0 | 6528 | `		pData->sMem.nByte = (sxu32)nLen;` |
|    ! 0 | 6529 | `	}else{` |
|      - | 6530 | `		static const char zZero[64] = {0};` |
|    ! 0 | 6531 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|    ! 0 | 6532 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|    ! 0 | 6533 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6534 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6535 | `				return -1;` |
|      - | 6536 | `			}` |
|    ! 0 | 6537 | `		}` |
|      - | 6538 | `	}` |
|    ! 0 | 6539 | `	return PH7_OK;` |
|    ! 0 | 6540 | `}` |
|      - | 6541 | `/*` |
|      - | 6542 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|      - | 6543 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|      - | 6544 | ` * base64). Shares the MEMORY machinery above.` |
|      - | 6545 | ` */` |
|      8 | 6546 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 6547 | `{` |
|      9 | 6548 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|      1 | 6549 | `}` |
|     10 | 6550 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6551 | `{` |
|      - | 6552 | `	ph7_stream_data *pData;` |
|     11 | 6553 | `	const char *zIn = zName;` |
|     11 | 6554 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|     11 | 6555 | `	const char *zComma = 0;` |
|     11 | 6556 | `	int bBase64 = 0;` |
|      5 | 6557 | `	SXUNUSED(iMode);` |
|      - | 6558 | `	/* Find the comma separating the mediatype from the payload */` |
|    105 | 6559 | `	while( zIn < zEnd ){` |
|    105 | 6560 | `		if( zIn[0] == ',' ){` |
|     11 | 6561 | `			zComma = zIn;` |
|     11 | 6562 | `			break;` |
|      - | 6563 | `		}` |
|     95 | 6564 | `		zIn++;` |
|      1 | 6565 | `	}` |
|     11 | 6566 | `	if( zComma == 0 ){` |
|    ! 0 | 6567 | `		return -1;` |
|      - | 6568 | `	}` |
|     10 | 6569 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|     10 | 6570 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|      3 | 6571 | `		bBase64 = 1;` |
|      1 | 6572 | `	}` |
|     11 | 6573 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|     11 | 6574 | `	if( pData == 0 ){` |
|    ! 0 | 6575 | `		return -1;` |
|      - | 6576 | `	}` |
|     11 | 6577 | `	pData->bReadOnly = 1;` |
|     11 | 6578 | `	zIn = &zComma[1];` |
|     11 | 6579 | `	if( bBase64 ){` |
|      3 | 6580 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|    ! 0 | 6581 | `			SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6582 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6583 | `			return -1;` |
|      - | 6584 | `		}` |
|      2 | 6585 | `	}else{` |
|      - | 6586 | `		/* percent-decode the payload */` |
|     71 | 6587 | `		while( zIn < zEnd ){` |
|     63 | 6588 | `			char c = zIn[0];` |
|     63 | 6589 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|      3 | 6590 | `				int hi = SyHexToint(zIn[1]);` |
|      3 | 6591 | `				int lo = SyHexToint(zIn[2]);` |
|      3 | 6592 | `				c = (char)((hi << 4) \| lo);` |
|      3 | 6593 | `				zIn += 3;` |
|      2 | 6594 | `			}else{` |
|     61 | 6595 | `				zIn++;` |
|      - | 6596 | `			}` |
|     63 | 6597 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|    ! 0 | 6598 | `				SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6599 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6600 | `				return -1;` |
|      - | 6601 | `			}` |
|      1 | 6602 | `		}` |
|      - | 6603 | `	}` |
|     11 | 6604 | `	*ppHandle = (void *)pData;` |
|     11 | 6605 | `	return PH7_OK;` |
|      6 | 6606 | `}` |
|      - | 6607 | `/* data:// rejects writes outright */` |
|    ! 0 | 6608 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 6609 | `{` |
|    ! 0 | 6610 | `	SXUNUSED(pHandle);` |
|    ! 0 | 6611 | `	SXUNUSED(pBuf);` |
|    ! 0 | 6612 | `	SXUNUSED(nWrite);` |
|    ! 0 | 6613 | `	return -1;` |
|    ! 0 | 6614 | `}` |
|      - | 6615 | `static const ph7_io_stream sDATA_Stream = {` |
|      - | 6616 | `	"data",` |
|      - | 6617 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6618 | `	DataStreamData_Open,  /* xOpen */` |
|      - | 6619 | `	0,   /* xOpenDir */` |
|      - | 6620 | `	PHPStreamData_Close, /* xClose */` |
|      - | 6621 | `	0,  /* xCloseDir */` |
|      - | 6622 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 6623 | `	0,  /* xReadDir */` |
|      - | 6624 | `	DataStreamData_Write, /* xWrite */` |
|      - | 6625 | `	PHPStreamData_Seek,  /* xSeek */` |
|      - | 6626 | `	0,  /* xLock */` |
|      - | 6627 | `	0,  /* xRewindDir */` |
|      - | 6628 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 6629 | `	0,  /* xTrunc */` |
|      - | 6630 | `	0,  /* xSync */` |
|      - | 6631 | `	0   /* xStat */` |
|      - | 6632 | `};` |
|      - | 6633 | `/*` |
|      - | 6634 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 6635 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 6636 | ` * PHP-compatible process I/O functionality.` |
|      - | 6637 | ` */` |
|      - | 6638 | `typedef struct pipe_private pipe_private;` |
|      - | 6639 | `struct pipe_private` |
|      - | 6640 | `{` |
|      - | 6641 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 6642 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 6643 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 6644 | `#ifdef __WINNT__` |
|      - | 6645 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 6646 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 6647 | `#endif` |
|      - | 6648 | `};` |
|      - | 6649 |  |
|      - | 6650 | `#ifdef __WINNT__` |
|      - | 6651 | `#include <Windows.h>` |
|      - | 6652 | `#include <stdio.h>` |
|      - | 6653 | `#include <io.h>` |
|      - | 6654 | `#include <fcntl.h>` |
|      - | 6655 | `/*` |
|      - | 6656 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 6657 | ` * This allows us to properly wait for process completion.` |
|      - | 6658 | ` */` |
|      - | 6659 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      5 | 6660 | `{` |
|      5 | 6661 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      5 | 6662 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      5 | 6663 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 6664 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 6665 | `	STARTUPINFOW si;` |
|      - | 6666 | `	PROCESS_INFORMATION pi;` |
|      5 | 6667 | `	WCHAR *zWideCmd = NULL;` |
|      5 | 6668 | `	FILE *pFile = NULL;` |
|      - | 6669 | `	int fd;` |
|      5 | 6670 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 6671 |  |
|      - | 6672 | `	/* Set up security attributes for pipe inheritance */` |
|      5 | 6673 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      5 | 6674 | `	sa.bInheritHandle = TRUE;` |
|      5 | 6675 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 6676 |  |
|      - | 6677 | `	/* Create pipes for child process I/O */` |
|      5 | 6678 | `	if( bRead ){` |
|      - | 6679 | `		/* Reading from child's stdout */` |
|      5 | 6680 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 6681 | `			return NULL;` |
|      - | 6682 | `		}` |
|      - | 6683 | `		/* Ensure read handle is not inherited */` |
|      5 | 6684 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      5 | 6685 | `		hReadPipe = hChildStdoutRd;` |
|      5 | 6686 | `		*phPipe = hChildStdoutRd;` |
|      5 | 6687 | `	}else{` |
|      - | 6688 | `		/* Writing to child's stdin */` |
|    ! 0 | 6689 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 6690 | `			return NULL;` |
|      - | 6691 | `		}` |
|      - | 6692 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 6693 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 6694 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 6695 | `		*phPipe = hChildStdinWr;` |
|      - | 6696 | `	}` |
|      - | 6697 |  |
|      - | 6698 | `	/* Convert command to wide string */` |
|      - | 6699 | `	{` |
|      5 | 6700 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      5 | 6701 | `		if( nLen <= 0 ){` |
|    ! 0 | 6702 | `			goto cleanup_pipes;` |
|      - | 6703 | `		}` |
|      5 | 6704 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      5 | 6705 | `		if( !zWideCmd ){` |
|    ! 0 | 6706 | `			goto cleanup_pipes;` |
|      - | 6707 | `		}` |
|      5 | 6708 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 6709 | `	}` |
|      - | 6710 |  |
|      - | 6711 | `	/* Set up process startup info */` |
|      5 | 6712 | `	ZeroMemory(&si, sizeof(si));` |
|      5 | 6713 | `	si.cb = sizeof(si);` |
|      5 | 6714 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      5 | 6715 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      5 | 6716 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      5 | 6717 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      5 | 6718 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 6719 |  |
|      5 | 6720 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 6721 |  |
|      - | 6722 | `	/* Create the child process */` |
|      5 | 6723 | `	if( !CreateProcessW(` |
|      - | 6724 | `		NULL,           /* Application name */` |
|      - | 6725 | `		zWideCmd,       /* Command line */` |
|      - | 6726 | `		NULL,           /* Process security attributes */` |
|      - | 6727 | `		NULL,           /* Thread security attributes */` |
|      - | 6728 | `		TRUE,           /* Inherit handles */` |
|      - | 6729 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 6730 | `		NULL,           /* Environment */` |
|      - | 6731 | `		NULL,           /* Current directory */` |
|      - | 6732 | `		&si,            /* Startup info */` |
|      - | 6733 | `		&pi             /* Process info */` |
|      - | 6734 | `	)){` |
|    ! 0 | 6735 | `		goto cleanup_all;` |
|      - | 6736 | `	}` |
|      - | 6737 |  |
|      - | 6738 | `	/* Close handles we don't need in parent */` |
|      5 | 6739 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      5 | 6740 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 6741 |  |
|      - | 6742 | `	/* Close thread handle (we only need process handle) */` |
|      5 | 6743 | `	CloseHandle(pi.hThread);` |
|      - | 6744 |  |
|      - | 6745 | `	/* Store process handle for later waiting */` |
|      5 | 6746 | `	*phProcess = pi.hProcess;` |
|      - | 6747 |  |
|      - | 6748 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      5 | 6749 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 6750 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      5 | 6751 | `	if( fd == -1 ){` |
|    ! 0 | 6752 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6753 | `		*phProcess = NULL;` |
|    ! 0 | 6754 | `		goto cleanup_all;` |
|      - | 6755 | `	}` |
|      - | 6756 |  |
|      5 | 6757 | `	pFile = _fdopen(fd, zMode);` |
|      5 | 6758 | `	if( !pFile ){` |
|    ! 0 | 6759 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 6760 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6761 | `		*phProcess = NULL;` |
|    ! 0 | 6762 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 6763 | `		return NULL;` |
|      - | 6764 | `	}` |
|      - | 6765 |  |
|      5 | 6766 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      5 | 6767 | `	return pFile;` |
|      - | 6768 |  |
|      - | 6769 | `cleanup_all:` |
|    ! 0 | 6770 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 6771 | `cleanup_pipes:` |
|    ! 0 | 6772 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 6773 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 6774 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 6775 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 6776 | `	return NULL;` |
|      5 | 6777 | `}` |
|      - | 6778 |  |
|      - | 6779 | `/*` |
|      - | 6780 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 6781 | ` */` |
|      - | 6782 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      5 | 6783 | `{` |
|      5 | 6784 | `	DWORD dwExitCode = 0;` |
|      - | 6785 | `	int status;` |
|      - | 6786 |  |
|      - | 6787 | `	/* Close the FILE* (this closes the pipe) */` |
|      5 | 6788 | `	fclose(pFile);` |
|      - | 6789 |  |
|      5 | 6790 | `	if( hProcess ){` |
|      - | 6791 | `		/* Wait for the process to complete */` |
|      5 | 6792 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 6793 |  |
|      5 | 6794 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      5 | 6795 | `			status = (int)dwExitCode;` |
|      5 | 6796 | `		}else{` |
|    ! 0 | 6797 | `			status = -1;` |
|      - | 6798 | `		}` |
|      - | 6799 |  |
|      - | 6800 | `		/* Close process handle */` |
|      5 | 6801 | `		CloseHandle(hProcess);` |
|      5 | 6802 | `	}else{` |
|    ! 0 | 6803 | `		status = -1;` |
|      - | 6804 | `	}` |
|      - | 6805 |  |
|      5 | 6806 | `	return status;` |
|      5 | 6807 | `}` |
|      - | 6808 | `#endif /* __WINNT__ */` |
|      - | 6809 | `/*` |
|      - | 6810 | ` * Open a pipe to a process.` |
|      - | 6811 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 6812 | ` */` |
|   3974 | 6813 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      5 | 6814 | `{` |
|      - | 6815 | `	pipe_private *pPipe;` |
|      - | 6816 | `	FILE *pFile;` |
|   3979 | 6817 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 6818 | `		return 0;` |
|      - | 6819 | `	}` |
|      - | 6820 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   3979 | 6821 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 6822 | `		return 0;` |
|      - | 6823 | `	}` |
|      - | 6824 | `	/* Open the pipe using system popen */` |
|      - | 6825 | `#ifdef __WINNT__` |
|      - | 6826 | `	{` |
|      - | 6827 | `		/* Build cmd.exe command wrapper */` |
|      5 | 6828 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      5 | 6829 | `		const char *zShellSuffix = "\"";` |
|      5 | 6830 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      5 | 6831 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      5 | 6832 | `		size_t nCmd = strlen(zCommand);` |
|      5 | 6833 | `		size_t nQuotes = 0;` |
|      5 | 6834 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6835 | `			if (zCommand[i] == '"') nQuotes++;` |
|      5 | 6836 | `		}` |
|      5 | 6837 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      5 | 6838 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      5 | 6839 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 6840 | `			return 0;` |
|      - | 6841 | `		}` |
|      - | 6842 | `		/* Escape quotes in command */` |
|      5 | 6843 | `		size_t j = 0;` |
|      5 | 6844 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6845 | `			char ch = zCommand[i];` |
|      5 | 6846 | `			if (ch == '"') {` |
|      4 | 6847 | `				zCmdEsc[j++] = '^';` |
|      4 | 6848 | `				zCmdEsc[j++] = '"';` |
|      4 | 6849 | `			} else {` |
|      5 | 6850 | `				zCmdEsc[j++] = ch;` |
|      - | 6851 | `			}` |
|      5 | 6852 | `		}` |
|      5 | 6853 | `		zCmdEsc[j] = '\0';` |
|      5 | 6854 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      5 | 6855 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      5 | 6856 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 6857 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6858 | `			return 0;` |
|      - | 6859 | `		}` |
|      5 | 6860 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      5 | 6861 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      5 | 6862 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      5 | 6863 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 6864 | `		/* Allocate pipe structure early so we can store handles */` |
|      5 | 6865 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      5 | 6866 | `		if( pPipe == 0 ){` |
|    ! 0 | 6867 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6868 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 6869 | `			return 0;` |
|      - | 6870 | `		}` |
|      - | 6871 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      5 | 6872 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      5 | 6873 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      5 | 6874 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      5 | 6875 | `		if( pFile == 0 ){` |
|    ! 0 | 6876 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 6877 | `			return 0;` |
|      - | 6878 | `		}` |
|      - | 6879 | `		/* Initialize remaining fields */` |
|      5 | 6880 | `		pPipe->pFile = pFile;` |
|      5 | 6881 | `		pPipe->pVm = pVm;` |
|      5 | 6882 | `		pPipe->iMode = zMode[0];` |
|      - | 6883 | `	}` |
|      - | 6884 | `#elif defined(__UNIXES__) /* Unix */` |
|   3974 | 6885 | `	pFile = popen(zCommand, zMode);` |
|   3974 | 6886 | `	if( pFile == 0 ){` |
|    ! 0 | 6887 | `		return 0;` |
|      - | 6888 | `	}` |
|      - | 6889 | `	/* Allocate pipe private structure */` |
|   3974 | 6890 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   3974 | 6891 | `	if( pPipe == 0 ){` |
|      - | 6892 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 6893 | `		pclose(pFile);` |
|    ! 0 | 6894 | `		return 0;` |
|      - | 6895 | `	}` |
|      - | 6896 | `	/* Initialize the structure */` |
|   3974 | 6897 | `	pPipe->pFile = pFile;` |
|   3974 | 6898 | `	pPipe->pVm = pVm;` |
|   3974 | 6899 | `	pPipe->iMode = zMode[0];` |
|      - | 6900 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6901 | `	(void)pFile;` |
|      - | 6902 | `	return 0;` |
|      - | 6903 | `#endif` |
|   3979 | 6904 | `	return pPipe;` |
|   1992 | 6905 | `}` |
|      - | 6906 | `/*` |
|      - | 6907 | ` * Close a pipe and return the exit status of the process.` |
|      - | 6908 | ` * Returns the exit status, or -1 on error.` |
|      - | 6909 | ` */` |
|   3948 | 6910 | `static int PipeClose(pipe_private *pPipe)` |
|      5 | 6911 | `{` |
|      - | 6912 | `	int status;` |
|      - | 6913 | `	ph7_vm *pVm;` |
|   3953 | 6914 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6915 | `		return -1;` |
|      - | 6916 | `	}` |
|   3953 | 6917 | `	pVm = pPipe->pVm;` |
|      - | 6918 | `	/* Close the pipe and get exit status */` |
|      - | 6919 | `#ifdef __WINNT__` |
|      - | 6920 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      5 | 6921 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 6922 | `#elif defined(__UNIXES__)` |
|   3948 | 6923 | `	status = pclose(pPipe->pFile);` |
|      - | 6924 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   3948 | 6925 | `	if( status != -1 ){` |
|   3948 | 6926 | `		if( WIFEXITED(status) ){` |
|   3948 | 6927 | `			status = WEXITSTATUS(status);` |
|   1974 | 6928 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 6929 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 6930 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 6931 | `		}else{` |
|      - | 6932 | `			/* Unknown termination reason */` |
|    ! 0 | 6933 | `			status = -1;` |
|      - | 6934 | `		}` |
|   1974 | 6935 | `	}` |
|      - | 6936 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6937 | `	status = -1;` |
|      - | 6938 | `#endif` |
|      - | 6939 | `	/* Free the structure */` |
|   3953 | 6940 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   3953 | 6941 | `	return status;` |
|   1979 | 6942 | `}` |
|      - | 6943 | `/*` |
|      - | 6944 | ` * Pipe stream xClose implementation.` |
|      - | 6945 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 6946 | ` * It closes the pipe but does not return the exit status.` |
|      - | 6947 | ` */` |
|    102 | 6948 | `static void PipeStream_Close(void *pHandle)` |
|      4 | 6949 | `{` |
|    106 | 6950 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    106 | 6951 | `	if( pPipe ){` |
|    106 | 6952 | `		PipeClose(pPipe);` |
|     51 | 6953 | `	}` |
|    106 | 6954 | `}` |
|      - | 6955 | `/*` |
|      - | 6956 | ` * Pipe stream xRead implementation.` |
|      - | 6957 | ` */` |
|   5908 | 6958 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      4 | 6959 | `{` |
|   5912 | 6960 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6961 | `	size_t nRead;` |
|   5912 | 6962 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6963 | `		return -1;` |
|      - | 6964 | `	}` |
|   5912 | 6965 | `	if( pPipe->iMode != 'r' ){` |
|      - | 6966 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 6967 | `		return -1;` |
|      - | 6968 | `	}` |
|   5912 | 6969 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   5912 | 6970 | `	if( nRead == 0 ){` |
|   3980 | 6971 | `		if( feof(pPipe->pFile) ){` |
|   3980 | 6972 | `			return 0; /* EOF */` |
|      - | 6973 | `		}` |
|    ! 0 | 6974 | `		return -1; /* Error */` |
|      - | 6975 | `	}` |
|   1936 | 6976 | `	return (ph7_int64)nRead;` |
|   2958 | 6977 | `}` |
|      - | 6978 | `/*` |
|      - | 6979 | ` * Pipe stream xWrite implementation.` |
|      - | 6980 | ` */` |
|      4 | 6981 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 6982 | `{` |
|      4 | 6983 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6984 | `	size_t nWritten;` |
|      4 | 6985 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6986 | `		return -1;` |
|      - | 6987 | `	}` |
|      4 | 6988 | `	if( pPipe->iMode != 'w' ){` |
|      - | 6989 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 6990 | `		return -1;` |
|      - | 6991 | `	}` |
|      4 | 6992 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      4 | 6993 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 6994 | `		return -1; /* Error */` |
|      - | 6995 | `	}` |
|      4 | 6996 | `	return (ph7_int64)nWritten;` |
|      2 | 6997 | `}` |
|      - | 6998 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 6999 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 7000 | `	"pipe",` |
|      - | 7001 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7002 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|      - | 7003 | `	0,  /* xOpenDir */` |
|      - | 7004 | `	PipeStream_Close,  /* xClose */` |
|      - | 7005 | `	0,  /* xCloseDir */` |
|      - | 7006 | `	PipeStream_Read,   /* xRead */` |
|      - | 7007 | `	0,  /* xReadDir */` |
|      - | 7008 | `	PipeStream_Write,  /* xWrite */` |
|      - | 7009 | `	0,  /* xSeek */` |
|      - | 7010 | `	0,  /* xLock */` |
|      - | 7011 | `	0,  /* xRewindDir */` |
|      - | 7012 | `	0,  /* xTell */` |
|      - | 7013 | `	0,  /* xTrunc */` |
|      - | 7014 | `	0,  /* xSync */` |
|      - | 7015 | `	0   /* xStat */` |
|      - | 7016 | `};` |
|      - | 7017 | `/*` |
|      - | 7018 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 7019 | ` * FALSE otherwise.` |
|      - | 7020 | ` */` |
|   3842 | 7021 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      5 | 7022 | `{` |
|   3847 | 7023 | `	return pStream == &sPipe_Stream;` |
|      5 | 7024 | `}` |
|      - | 7025 | `/*` |
|      - | 7026 | ` * resource popen(string $command, string $mode)` |
|      - | 7027 | ` *  Opens process file pointer.` |
|      - | 7028 | ` * Parameters` |
|      - | 7029 | ` *  $command` |
|      - | 7030 | ` *   The command to execute. Passed to the system shell.` |
|      - | 7031 | ` *  $mode` |
|      - | 7032 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 7033 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 7034 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 7035 | ` * Return` |
|      - | 7036 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 7037 | ` */` |
|      - | 7038 | `/*` |
|      - | 7039 | ` * string\|false\|null shell_exec(string $command)` |
|      - | 7040 | ` *  Execute a command via the shell and return the complete output as a string.` |
|      - | 7041 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|      - | 7042 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|      - | 7043 | ` */` |
|      4 | 7044 | `static int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7045 | `{` |
|      - | 7046 | `	const char *zCommand;` |
|      - | 7047 | `	pipe_private *pPipe;` |
|      - | 7048 | `	SyBlob sOut;` |
|      - | 7049 | `	char zBuf[4096];` |
|      - | 7050 | `	size_t nRead;` |
|      - | 7051 | `	int nCmdLen;` |
|      6 | 7052 | `	if( nArg < 1 ){` |
|    ! 0 | 7053 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7054 | `		return PH7_OK;` |
|      - | 7055 | `	}` |
|      6 | 7056 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|      6 | 7057 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7058 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7059 | `		return PH7_OK;` |
|      - | 7060 | `	}` |
|      6 | 7061 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|      6 | 7062 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 7063 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7064 | `		return PH7_OK;` |
|      - | 7065 | `	}` |
|      6 | 7066 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|      4 | 7067 | `	for(;;){` |
|     10 | 7068 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|     10 | 7069 | `		if( nRead < 1 ){` |
|      6 | 7070 | `			break;` |
|      - | 7071 | `		}` |
|      6 | 7072 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|      2 | 7073 | `	}` |
|      6 | 7074 | `	PipeClose(pPipe);` |
|      6 | 7075 | `	if( SyBlobLength(&sOut) < 1 ){` |
|      - | 7076 | `		/* php answers NULL, not "", when the command printed nothing */` |
|    ! 0 | 7077 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7078 | `	}else{` |
|      6 | 7079 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|      - | 7080 | `	}` |
|      6 | 7081 | `	SyBlobRelease(&sOut);` |
|      6 | 7082 | `	return PH7_OK;` |
|      4 | 7083 | `}` |
|   3970 | 7084 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7085 | `{` |
|      - | 7086 | `	const char *zCommand, *zMode;` |
|      - | 7087 | `	pipe_private *pPipe;` |
|      - | 7088 | `	io_private *pDev;` |
|      - | 7089 | `	int nCmdLen, nModeLen;` |
|   3975 | 7090 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 7091 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 7092 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 7093 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7094 | `		return PH7_OK;` |
|      - | 7095 | `	}` |
|      - | 7096 | `	/* Extract the command and mode */` |
|   3975 | 7097 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   3975 | 7098 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   3975 | 7099 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7100 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 7101 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7102 | `		return PH7_OK;` |
|      - | 7103 | `	}` |
|   3975 | 7104 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 7105 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 7106 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7107 | `		return PH7_OK;` |
|      - | 7108 | `	}` |
|      - | 7109 | `	/* Open the pipe */` |
|   3975 | 7110 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   3975 | 7111 | `	if( pPipe == 0 ){` |
|      - | 7112 | `		/* Failed to open pipe */` |
|    ! 0 | 7113 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7114 | `		return PH7_OK;` |
|      - | 7115 | `	}` |
|      - | 7116 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   3975 | 7117 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   3975 | 7118 | `	if( pDev == 0 ){` |
|    ! 0 | 7119 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 7120 | `		PipeClose(pPipe);` |
|    ! 0 | 7121 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7122 | `		return PH7_OK;` |
|      - | 7123 | `	}` |
|      - | 7124 | `	/* Initialize the io_private structure */` |
|   3975 | 7125 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   3975 | 7126 | `	pDev->pHandle = pPipe;` |
|      - | 7127 | `	/* Return the io_private instance as a resource */` |
|   3975 | 7128 | `	ph7_result_resource(pCtx, pDev);` |
|   3975 | 7129 | `	return PH7_OK;` |
|   1990 | 7130 | `}` |
|      - | 7131 | `/*` |
|      - | 7132 | ` * int pclose(resource $handle)` |
|      - | 7133 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 7134 | ` * Parameters` |
|      - | 7135 | ` *  $handle` |
|      - | 7136 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 7137 | ` * Return` |
|      - | 7138 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 7139 | ` */` |
|   3842 | 7140 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7141 | `{` |
|      - | 7142 | `	const ph7_io_stream *pStream;` |
|      - | 7143 | `	pipe_private *pPipe;` |
|      - | 7144 | `	io_private *pDev;` |
|      - | 7145 | `	int status;` |
|   3847 | 7146 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 7147 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 7148 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7149 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7150 | `		return PH7_OK;` |
|      - | 7151 | `	}` |
|      - | 7152 | `	/* Extract our private data */` |
|   3847 | 7153 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 7154 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3847 | 7155 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 7156 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7157 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7158 | `		return PH7_OK;` |
|      - | 7159 | `	}` |
|      - | 7160 | `	/* Point to the target IO stream device */` |
|   3847 | 7161 | `	pStream = pDev->pStream;` |
|   3847 | 7162 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 7163 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 7164 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7165 | `		return PH7_OK;` |
|      - | 7166 | `	}` |
|      - | 7167 | `	/* Get the pipe handle */` |
|   3847 | 7168 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 7169 | `	/* Close the pipe and get exit status */` |
|   3847 | 7170 | `	status = PipeClose(pPipe);` |
|      - | 7171 | `	/* Keep the handle alive but flag it closed so shared copies see it */` |
|   3847 | 7172 | `	MarkIOPrivateClosed(pDev);` |
|      - | 7173 | `	/* Return the exit status */` |
|   3847 | 7174 | `	ph7_result_int(pCtx, status);` |
|   3847 | 7175 | `	return PH7_OK;` |
|   1926 | 7176 | `}` |
|      - | 7177 | `/*` |
|      - | 7178 | ` * proc_open() / proc_close() / proc_get_status() / proc_terminate()` |
|      - | 7179 | ` *   Run a command via fork()/exec() with fine-grained control over its` |
|      - | 7180 | ` *   standard descriptors (php's process-control family). The returned` |
|      - | 7181 | ` *   "process" resource wraps a small proc_private whose leading bytes mirror` |
|      - | 7182 | ` *   io_private (a distinct magic) so is_resource()/gettype() probes stay in` |
|      - | 7183 | ` *   bounds and report it as a live, non-stream resource.` |
|      - | 7184 | ` */` |
|      - | 7185 | `#ifdef __UNIXES__` |
|      - | 7186 | `#define PROC_PRIVATE_MAGIC 0x9C0DE5` |
|      - | 7187 | `#define PROC_MAX_DESC 16` |
|      - | 7188 | `typedef struct proc_private proc_private;` |
|      - | 7189 | `struct proc_private` |
|      - | 7190 | `{` |
|      - | 7191 | `	io_private base;   /* io_private-compatible header (base.iMagic == PROC_PRIVATE_MAGIC) */` |
|      - | 7192 | `	int pid;           /* child process id */` |
|      - | 7193 | `	int running;       /* TRUE until reaped by proc_close/proc_get_status */` |
|      - | 7194 | `	int exit_code;     /* cached exit status once reaped */` |
|      - | 7195 | `};` |
|      - | 7196 | `/* Wrap a raw fd as an fopen-style stream resource (a php pipe end). */` |
|     28 | 7197 | `static io_private * ProcWrapFd(ph7_vm *pVm,int fd)` |
|      - | 7198 | `{` |
|     28 | 7199 | `	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|     28 | 7200 | `	if( pDev == 0 ){` |
|    ! 0 | 7201 | `		return 0;` |
|      - | 7202 | `	}` |
|     28 | 7203 | `	InitIOPrivate(pVm,&sUnixFileStream,pDev);` |
|     28 | 7204 | `	pDev->pHandle = SX_INT_TO_PTR(fd);` |
|     28 | 7205 | `	return pDev;` |
|     14 | 7206 | `}` |
|      - | 7207 | `/* One parsed descriptor-spec entry. */` |
|      - | 7208 | `struct proc_desc` |
|      - | 7209 | `{` |
|      - | 7210 | `	int child_fd;      /* the array key: which fd the child sees */` |
|      - | 7211 | `	int kind;          /* 0=pipe, 1=file, 2=redirect */` |
|      - | 7212 | `	/* pipe */` |
|      - | 7213 | `	int child_end;     /* fd the child must have at child_fd */` |
|      - | 7214 | `	int parent_end;    /* fd the parent keeps (wrapped into $pipes), -1 if none */` |
|      - | 7215 | `	/* file */` |
|      - | 7216 | `	int file_fd;       /* opened fd for a ['file',path,mode] spec */` |
|      - | 7217 | `	/* redirect */` |
|      - | 7218 | `	int redirect_to;   /* target child fd for a ['redirect',N] spec */` |
|      - | 7219 | `};` |
|     10 | 7220 | `static int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7221 | `{` |
|      - | 7222 | `	struct proc_desc aDesc[PROC_MAX_DESC];` |
|     10 | 7223 | `	int nDesc = 0;` |
|      - | 7224 | `	ph7_value *pSpec, *pPipes, *pEntry, *pType, *pParam;` |
|      - | 7225 | `	ph7_hashmap *pSpecMap;` |
|      - | 7226 | `	ph7_hashmap_node *pNode;` |
|     10 | 7227 | `	ph7_vm *pVm = pCtx->pVm;` |
|     10 | 7228 | `	char **azArgv = 0;      /* exec argv when the command is an array */` |
|     10 | 7229 | `	int nArgv = 0;` |
|     10 | 7230 | `	const char *zCmd = 0;   /* exec command when it is a string (via /bin/sh -c) */` |
|     10 | 7231 | `	const char *zCwd = 0;` |
|     10 | 7232 | `	char **azEnv = 0;       /* constructed envp when an env array is supplied */` |
|     10 | 7233 | `	int nEnv = 0;` |
|      - | 7234 | `	proc_private *pProc;` |
|      - | 7235 | `	pid_t pid;` |
|      - | 7236 | `	int i, rc;` |
|     10 | 7237 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[1]) ){` |
|    ! 0 | 7238 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() expects a command and a descriptor spec");` |
|    ! 0 | 7239 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7240 | `		return PH7_OK;` |
|      - | 7241 | `	}` |
|      - | 7242 | `	/* --- Command: array (execvp) or string (/bin/sh -c) --- */` |
|     10 | 7243 | `	if( ph7_value_is_array(apArg[0]) ){` |
|     10 | 7244 | `		ph7_hashmap *pCmdMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     10 | 7245 | `		int nCount = (int)ph7_array_count(apArg[0]);` |
|     10 | 7246 | `		azArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|     10 | 7247 | `		if( azArgv == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     10 | 7248 | `		pNode = pCmdMap->pFirst;` |
|     20 | 7249 | `		for( i = 0 ; i < nCount && pNode ; ++i ){` |
|     10 | 7250 | `			ph7_value *pv = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|      - | 7251 | `			int nLen; const char *zs;` |
|     10 | 7252 | `			PH7_MemObjInit(pVm,pv);` |
|     10 | 7253 | `			PH7_HashmapExtractNodeValue(pNode,pv,FALSE);` |
|     10 | 7254 | `			zs = ph7_value_to_string(pv,&nLen);` |
|     10 | 7255 | `			azArgv[nArgv] = (char *)SyMemBackendDup(&pVm->sAllocator,zs,(sxu32)nLen+1);` |
|     10 | 7256 | `			if( azArgv[nArgv] ){ azArgv[nArgv][nLen] = 0; nArgv++; }` |
|     10 | 7257 | `			PH7_MemObjRelease(pv);` |
|     10 | 7258 | `			SyMemBackendFree(&pVm->sAllocator,pv);` |
|     10 | 7259 | `			pNode = pNode->pPrev; /* hashmap insertion-order walk */` |
|      5 | 7260 | `		}` |
|     10 | 7261 | `		azArgv[nArgv] = 0;` |
|      5 | 7262 | `	}else{` |
|      - | 7263 | `		int nLen;` |
|    ! 0 | 7264 | `		zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7265 | `	}` |
|      - | 7266 | `	/* --- Optional cwd (arg 4) and env (arg 5) --- */` |
|     10 | 7267 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|    ! 0 | 7268 | `		int nLen; zCwd = ph7_value_to_string(apArg[3],&nLen);` |
|    ! 0 | 7269 | `		if( nLen < 1 ){ zCwd = 0; }` |
|    ! 0 | 7270 | `	}` |
|      5 | 7271 | `	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){` |
|    ! 0 | 7272 | `		ph7_hashmap *pEnvMap = (ph7_hashmap *)apArg[4]->x.pOther;` |
|    ! 0 | 7273 | `		int nCount = (int)ph7_array_count(apArg[4]);` |
|    ! 0 | 7274 | `		azEnv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|    ! 0 | 7275 | `		if( azEnv ){` |
|    ! 0 | 7276 | `			pNode = pEnvMap->pFirst;` |
|    ! 0 | 7277 | `			for( i = 0 ; i < nCount && pNode ; ++i ){` |
|      - | 7278 | `				ph7_value sKey, sVal; int nk, nv; const char *zk, *zv; char *zPair;` |
|    ! 0 | 7279 | `				PH7_MemObjInit(pVm,&sKey); PH7_MemObjInit(pVm,&sVal);` |
|    ! 0 | 7280 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|    ! 0 | 7281 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|    ! 0 | 7282 | `				zk = ph7_value_to_string(&sKey,&nk);` |
|    ! 0 | 7283 | `				zv = ph7_value_to_string(&sVal,&nv);` |
|    ! 0 | 7284 | `				zPair = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nk+nv+2));` |
|    ! 0 | 7285 | `				if( zPair ){` |
|    ! 0 | 7286 | `					SyMemcpy(zk,zPair,(sxu32)nk); zPair[nk] = '=';` |
|    ! 0 | 7287 | `					SyMemcpy(zv,&zPair[nk+1],(sxu32)nv); zPair[nk+1+nv] = 0;` |
|    ! 0 | 7288 | `					azEnv[nEnv++] = zPair;` |
|    ! 0 | 7289 | `				}` |
|    ! 0 | 7290 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|    ! 0 | 7291 | `				pNode = pNode->pPrev;` |
|    ! 0 | 7292 | `			}` |
|    ! 0 | 7293 | `			azEnv[nEnv] = 0;` |
|    ! 0 | 7294 | `		}` |
|    ! 0 | 7295 | `	}` |
|      - | 7296 | `	/* --- Parse the descriptor spec, creating pipes as we go --- */` |
|     10 | 7297 | `	pSpec = apArg[1];` |
|     10 | 7298 | `	pSpecMap = (ph7_hashmap *)pSpec->x.pOther;` |
|     10 | 7299 | `	pNode = pSpecMap->pFirst;` |
|     40 | 7300 | `	for( i = 0 ; i < (int)pSpecMap->nEntry && pNode && nDesc < PROC_MAX_DESC ; ++i ){` |
|     30 | 7301 | `		ph7_value sKey; struct proc_desc *pD = &aDesc[nDesc];` |
|     30 | 7302 | `		PH7_MemObjInit(pVm,&sKey);` |
|     30 | 7303 | `		PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|     30 | 7304 | `		pD->child_fd = ph7_value_to_int(&sKey);` |
|     30 | 7305 | `		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1;` |
|     30 | 7306 | `		PH7_MemObjRelease(&sKey);` |
|     30 | 7307 | `		pEntry = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|     30 | 7308 | `		PH7_MemObjInit(pVm,pEntry);` |
|     30 | 7309 | `		PH7_HashmapExtractNodeValue(pNode,pEntry,FALSE);` |
|     30 | 7310 | `		if( ph7_value_is_array(pEntry) ){` |
|      - | 7311 | `			int nLen; const char *zType;` |
|     30 | 7312 | `			pType = ph7_array_fetch(pEntry,"0",1);` |
|     30 | 7313 | `			zType = pType ? ph7_value_to_string(pType,&nLen) : "";` |
|     30 | 7314 | `			if( SyStrncmp(zType,"pipe",4) == 0 ){` |
|      - | 7315 | `				int fds[2];` |
|     28 | 7316 | `				if( pipe(fds) == 0 ){` |
|     28 | 7317 | `					pParam = ph7_array_fetch(pEntry,"1",1);` |
|      - | 7318 | `					{` |
|     28 | 7319 | `						int nMode; const char *zMode = pParam ? ph7_value_to_string(pParam,&nMode) : "r";` |
|     28 | 7320 | `						pD->kind = 0;` |
|     28 | 7321 | `						if( zMode[0] == 'w' \|\| zMode[0] == 'a' ){` |
|      - | 7322 | `							/* child writes -> parent reads: child gets write end */` |
|     18 | 7323 | `							pD->child_end = fds[1]; pD->parent_end = fds[0];` |
|      9 | 7324 | `						}else{` |
|      - | 7325 | `							/* child reads -> parent writes: child gets read end */` |
|     10 | 7326 | `							pD->child_end = fds[0]; pD->parent_end = fds[1];` |
|      - | 7327 | `						}` |
|     28 | 7328 | `						nDesc++;` |
|      - | 7329 | `					}` |
|     14 | 7330 | `				}` |
|     16 | 7331 | `			}else if( SyStrncmp(zType,"file",4) == 0 ){` |
|    ! 0 | 7332 | `				int nLen2, nLen3; const char *zPath, *zMode; int oflag = O_RDONLY;` |
|    ! 0 | 7333 | `				ph7_value *pPath = ph7_array_fetch(pEntry,"1",1);` |
|    ! 0 | 7334 | `				ph7_value *pMode = ph7_array_fetch(pEntry,"2",1);` |
|    ! 0 | 7335 | `				zPath = pPath ? ph7_value_to_string(pPath,&nLen2) : "";` |
|    ! 0 | 7336 | `				zMode = pMode ? ph7_value_to_string(pMode,&nLen3) : "r";` |
|    ! 0 | 7337 | `				if( zMode[0] == 'w' ){ oflag = O_WRONLY\|O_CREAT\|O_TRUNC; }` |
|    ! 0 | 7338 | `				else if( zMode[0] == 'a' ){ oflag = O_WRONLY\|O_CREAT\|O_APPEND; }` |
|    ! 0 | 7339 | `				pD->kind = 1;` |
|    ! 0 | 7340 | `				pD->file_fd = open(zPath,oflag,0644);` |
|    ! 0 | 7341 | `				nDesc++;` |
|      2 | 7342 | `			}else if( SyStrncmp(zType,"redirect",8) == 0 ){` |
|      2 | 7343 | `				pParam = ph7_array_fetch(pEntry,"1",1);` |
|      2 | 7344 | `				pD->kind = 2;` |
|      2 | 7345 | `				pD->redirect_to = pParam ? ph7_value_to_int(pParam) : 1;` |
|      2 | 7346 | `				nDesc++;` |
|      1 | 7347 | `			}` |
|     15 | 7348 | `		}` |
|     30 | 7349 | `		PH7_MemObjRelease(pEntry);` |
|     30 | 7350 | `		SyMemBackendFree(&pVm->sAllocator,pEntry);` |
|     30 | 7351 | `		pNode = pNode->pPrev;` |
|     15 | 7352 | `	}` |
|      - | 7353 | `	/* --- Fork the child --- */` |
|     10 | 7354 | `	pid = fork();` |
|     15 | 7355 | `	if( pid < 0 ){` |
|    ! 0 | 7356 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open(): fork() failed");` |
|    ! 0 | 7357 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7358 | `		return PH7_OK;` |
|      - | 7359 | `	}` |
|     20 | 7360 | `	if( pid == 0 ){` |
|      - | 7361 | `		/* Child: wire up descriptors then exec */` |
|     40 | 7362 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|     30 | 7363 | `			struct proc_desc *pD = &aDesc[i];` |
|     30 | 7364 | `			if( pD->kind == 0 ){` |
|     28 | 7365 | `				dup2(pD->child_end,pD->child_fd);` |
|     28 | 7366 | `				close(pD->parent_end);` |
|     28 | 7367 | `				close(pD->child_end);` |
|     16 | 7368 | `			}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|    ! 0 | 7369 | `				dup2(pD->file_fd,pD->child_fd);` |
|    ! 0 | 7370 | `				close(pD->file_fd);` |
|    ! 0 | 7371 | `			}` |
|     15 | 7372 | `		}` |
|      - | 7373 | `		/* Redirects run after the pipes are in place (e.g. 2>&1) */` |
|     40 | 7374 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|     30 | 7375 | `			if( aDesc[i].kind == 2 ){ dup2(aDesc[i].redirect_to,aDesc[i].child_fd); }` |
|     15 | 7376 | `		}` |
|     10 | 7377 | `		if( zCwd ){ if( chdir(zCwd) != 0 ){ _exit(127); } }` |
|     10 | 7378 | `		if( azEnv ){` |
|    ! 0 | 7379 | `			if( azArgv ){ execve(azArgv[0],azArgv,azEnv); }` |
|    ! 0 | 7380 | `			else{ char *av[4]; av[0]=(char*)"sh"; av[1]=(char*)"-c"; av[2]=(char*)zCmd; av[3]=0; execve("/bin/sh",av,azEnv); }` |
|    ! 0 | 7381 | `		}else{` |
|     10 | 7382 | `			if( azArgv ){ execvp(azArgv[0],azArgv); }` |
|    ! 0 | 7383 | `			else{ execl("/bin/sh","sh","-c",zCmd,(char *)0); }` |
|      - | 7384 | `		}` |
|      5 | 7385 | `		_exit(127); /* exec failed */` |
|      - | 7386 | `	}` |
|      - | 7387 | `	/* Parent: close the child ends, wrap the parent ends into $pipes */` |
|     10 | 7388 | `	pPipes = ph7_context_new_array(pCtx);` |
|     40 | 7389 | `	for( i = 0 ; i < nDesc ; ++i ){` |
|     30 | 7390 | `		struct proc_desc *pD = &aDesc[i];` |
|     30 | 7391 | `		if( pD->kind == 0 ){` |
|      - | 7392 | `			io_private *pEnd;` |
|      - | 7393 | `			ph7_value *pRes;` |
|     28 | 7394 | `			close(pD->child_end);` |
|     28 | 7395 | `			pEnd = ProcWrapFd(pVm,pD->parent_end);` |
|     28 | 7396 | `			pRes = ph7_context_new_scalar(pCtx);` |
|     28 | 7397 | `			if( pEnd && pRes && pPipes ){` |
|     28 | 7398 | `				ph7_value_resource(pRes,pEnd);` |
|     28 | 7399 | `				ph7_array_add_intkey_elem(pPipes,pD->child_fd,pRes);` |
|     14 | 7400 | `			}` |
|     28 | 7401 | `			if( pRes ){ ph7_context_release_value(pCtx,pRes); }` |
|     16 | 7402 | `		}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|    ! 0 | 7403 | `			close(pD->file_fd);` |
|    ! 0 | 7404 | `		}` |
|     15 | 7405 | `	}` |
|     10 | 7406 | `	if( pPipes ){` |
|     10 | 7407 | `		PH7_VmStoreArgByRef(pVm,apArg[2],pPipes);` |
|      5 | 7408 | `	}` |
|      - | 7409 | `	/* Free the exec argv/env copies now that the child owns its own image */` |
|     10 | 7410 | `	if( azArgv ){` |
|     20 | 7411 | `		for( i = 0 ; i < nArgv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azArgv[i]); }` |
|     10 | 7412 | `		SyMemBackendFree(&pVm->sAllocator,azArgv);` |
|      5 | 7413 | `	}` |
|     15 | 7414 | `	if( azEnv ){` |
|    ! 0 | 7415 | `		for( i = 0 ; i < nEnv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azEnv[i]); }` |
|    ! 0 | 7416 | `		SyMemBackendFree(&pVm->sAllocator,azEnv);` |
|    ! 0 | 7417 | `	}` |
|      - | 7418 | `	/* Build the process resource */` |
|     10 | 7419 | `	pProc = (proc_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(proc_private));` |
|     10 | 7420 | `	if( pProc == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     10 | 7421 | `	SyZero(pProc,sizeof(proc_private));` |
|     10 | 7422 | `	pProc->base.iMagic = PROC_PRIVATE_MAGIC;` |
|     10 | 7423 | `	pProc->pid = (int)pid;` |
|     10 | 7424 | `	pProc->running = 1;` |
|     10 | 7425 | `	pProc->exit_code = 0;` |
|     10 | 7426 | `	ph7_result_resource(pCtx,pProc);` |
|      5 | 7427 | `	(void)rc;` |
|     10 | 7428 | `	return PH7_OK;` |
|      5 | 7429 | `}` |
|      - | 7430 | `/* Reap the child if it has not been reaped yet, caching the exit code. */` |
|     10 | 7431 | `static void ProcReap(proc_private *pProc,int block)` |
|      - | 7432 | `{` |
|     10 | 7433 | `	int status = 0;` |
|      - | 7434 | `	pid_t r;` |
|     10 | 7435 | `	if( !pProc->running ){ return; }` |
|     10 | 7436 | `	r = waitpid((pid_t)pProc->pid,&status,block ? 0 : WNOHANG);` |
|     10 | 7437 | `	if( r == (pid_t)pProc->pid ){` |
|     10 | 7438 | `		pProc->running = 0;` |
|     10 | 7439 | `		if( WIFEXITED(status) ){ pProc->exit_code = WEXITSTATUS(status); }` |
|    ! 0 | 7440 | `		else if( WIFSIGNALED(status) ){ pProc->exit_code = 128 + WTERMSIG(status); }` |
|      5 | 7441 | `	}` |
|      5 | 7442 | `}` |
|     10 | 7443 | `static int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7444 | `{` |
|      - | 7445 | `	proc_private *pProc;` |
|     10 | 7446 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7447 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 7448 | `		return PH7_OK;` |
|      - | 7449 | `	}` |
|     10 | 7450 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|     10 | 7451 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|    ! 0 | 7452 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 7453 | `		return PH7_OK;` |
|      - | 7454 | `	}` |
|     10 | 7455 | `	ProcReap(pProc,1/*block until it exits*/);` |
|     10 | 7456 | `	ph7_result_int(pCtx,pProc->exit_code);` |
|     10 | 7457 | `	return PH7_OK;` |
|      5 | 7458 | `}` |
|    ! 0 | 7459 | `static int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7460 | `{` |
|      - | 7461 | `	proc_private *pProc;` |
|    ! 0 | 7462 | `	int sig = 15; /* SIGTERM */` |
|    ! 0 | 7463 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7464 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7465 | `		return PH7_OK;` |
|      - | 7466 | `	}` |
|    ! 0 | 7467 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|    ! 0 | 7468 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|    ! 0 | 7469 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7470 | `		return PH7_OK;` |
|      - | 7471 | `	}` |
|    ! 0 | 7472 | `	if( nArg > 1 ){ sig = ph7_value_to_int(apArg[1]); }` |
|    ! 0 | 7473 | `	if( pProc->running ){ kill((pid_t)pProc->pid,sig); }` |
|    ! 0 | 7474 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 7475 | `	return PH7_OK;` |
|    ! 0 | 7476 | `}` |
|    ! 0 | 7477 | `static int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 7478 | `{` |
|      - | 7479 | `	proc_private *pProc;` |
|      - | 7480 | `	ph7_value *pArray, *pVal;` |
|    ! 0 | 7481 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7482 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7483 | `		return PH7_OK;` |
|      - | 7484 | `	}` |
|    ! 0 | 7485 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|    ! 0 | 7486 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|    ! 0 | 7487 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7488 | `		return PH7_OK;` |
|      - | 7489 | `	}` |
|    ! 0 | 7490 | `	ProcReap(pProc,0/*non-blocking poll*/);` |
|    ! 0 | 7491 | `	pArray = ph7_context_new_array(pCtx);` |
|    ! 0 | 7492 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 7493 | `	if( pArray == 0 \|\| pVal == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    ! 0 | 7494 | `	ph7_value_int(pVal,pProc->pid);` |
|    ! 0 | 7495 | `	ph7_array_add_strkey_elem(pArray,"pid",pVal);` |
|    ! 0 | 7496 | `	ph7_value_bool(pVal,pProc->running);` |
|    ! 0 | 7497 | `	ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|    ! 0 | 7498 | `	ph7_value_bool(pVal,0);` |
|    ! 0 | 7499 | `	ph7_array_add_strkey_elem(pArray,"signaled",pVal);` |
|    ! 0 | 7500 | `	ph7_array_add_strkey_elem(pArray,"stopped",pVal);` |
|    ! 0 | 7501 | `	ph7_value_int(pVal,pProc->running ? -1 : pProc->exit_code);` |
|    ! 0 | 7502 | `	ph7_array_add_strkey_elem(pArray,"exitcode",pVal);` |
|    ! 0 | 7503 | `	ph7_value_int(pVal,0);` |
|    ! 0 | 7504 | `	ph7_array_add_strkey_elem(pArray,"termsig",pVal);` |
|    ! 0 | 7505 | `	ph7_array_add_strkey_elem(pArray,"stopsig",pVal);` |
|    ! 0 | 7506 | `	ph7_context_release_value(pCtx,pVal);` |
|    ! 0 | 7507 | `	ph7_result_value(pCtx,pArray);` |
|    ! 0 | 7508 | `	return PH7_OK;` |
|    ! 0 | 7509 | `}` |
|      - | 7510 | `#else /* !__UNIXES__ */` |
|      - | 7511 | `static int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7512 | `{` |
|      - | 7513 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 7514 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() is not available on this platform");` |
|    ! 0 | 7515 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 7516 | `	return PH7_OK;` |
|    ! 0 | 7517 | `}` |
|      - | 7518 | `static int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7519 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_int(pCtx,-1); return PH7_OK; }` |
|      - | 7520 | `static int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7521 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|      - | 7522 | `static int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 7523 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|      - | 7524 | `#endif /* __UNIXES__ */` |
|      - | 7525 | `/* Export the php:// stream */` |
|      - | 7526 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 7527 | `	"php",` |
|      - | 7528 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7529 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 7530 | `	0,   /* xOpenDir */` |
|      - | 7531 | `	PHPStreamData_Close, /* xClose */` |
|      - | 7532 | `	0,  /* xCloseDir */` |
|      - | 7533 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 7534 | `	0,  /* xReadDir */` |
|      - | 7535 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 7536 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|      - | 7537 | `	0,  /* xLock */` |
|      - | 7538 | `	0,  /* xRewindDir */` |
|      - | 7539 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 7540 | `	PHPStreamData_Trunc, /* xTrunc */` |
|      - | 7541 | `	0,  /* xSync */` |
|      - | 7542 | `	0   /* xStat */` |
|      - | 7543 | `};` |
|      - | 7544 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7545 | `/*` |
|      - | 7546 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 7547 | ` * FALSE otherwise.` |
|      - | 7548 | ` */` |
|    226 | 7549 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      4 | 7550 | `{` |
|      - | 7551 | `#ifndef PH7_DISABLE_DISK_IO` |
|    230 | 7552 | `	return pStream == &sPHP_Stream;` |
|      - | 7553 | `#else` |
|      - | 7554 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7555 | `	return 0;` |
|      - | 7556 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      4 | 7557 | `}` |
|      - | 7558 | `/*` |
|      - | 7559 | ` * Return TRUE if we are dealing with the data:// stream.` |
|      - | 7560 | ` */` |
|    206 | 7561 | `static int is_data_stream(const ph7_io_stream *pStream)` |
|      4 | 7562 | `{` |
|      - | 7563 | `#ifndef PH7_DISABLE_DISK_IO` |
|    210 | 7564 | `	return pStream == &sDATA_Stream;` |
|      - | 7565 | `#else` |
|      - | 7566 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7567 | `	return 0;` |
|      - | 7568 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      4 | 7569 | `}` |
|      - | 7570 | `/*` |
|      - | 7571 | ` * bool stream_isatty(resource $stream)` |
|      - | 7572 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|      - | 7573 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|      - | 7574 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|      - | 7575 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|      - | 7576 | ` */` |
|      6 | 7577 | `static int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7578 | `{` |
|      7 | 7579 | `	int bTty = 0;` |
|      7 | 7580 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7581 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7582 | `		return PH7_OK;` |
|      - | 7583 | `	}` |
|      - | 7584 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7585 | `	{` |
|      7 | 7586 | `		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      7 | 7587 | `		if( !IO_PRIVATE_INVALID(pDev) && is_php_stream(pDev->pStream) ){` |
|      5 | 7588 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|      6 | 7589 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|      4 | 7590 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|      3 | 7591 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|      - | 7592 | `#ifdef __WINNT__` |
|      1 | 7593 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|      - | 7594 | `#else` |
|      4 | 7595 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|      - | 7596 | `#endif` |
|      2 | 7597 | `			}` |
|      2 | 7598 | `		}` |
|      - | 7599 | `	}` |
|      - | 7600 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      7 | 7601 | `	ph7_result_bool(pCtx,bTty);` |
|      7 | 7602 | `	return PH7_OK;` |
|      4 | 7603 | `}` |
|      - | 7604 |  |
|      - | 7605 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7606 | `/*` |
|      - | 7607 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 7608 | ` * [i.e: file://,php://].` |
|      - | 7609 | ` * Note:` |
|      - | 7610 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 7611 | ` *  defined then this function is a no-op.` |
|      - | 7612 | ` */` |
|   3424 | 7613 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 7614 | `{` |
|      - | 7615 | `	/*` |
|      - | 7616 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 7617 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 7618 | `	 */` |
|      - | 7619 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7620 | `	/* VFS: disk I/O related functions */` |
|      - | 7621 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 7622 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 7623 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 7624 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 7625 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 7626 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 7627 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 7628 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 7629 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 7630 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 7631 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 7632 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 7633 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 7634 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 7635 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 7636 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 7637 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 7638 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 7639 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 7640 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 7641 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 7642 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 7643 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 7644 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 7645 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 7646 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 7647 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 7648 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 7649 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 7650 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 7651 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 7652 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 7653 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 7654 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 7655 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 7656 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 7657 | `		{"link",        PH7_vfs_link     },` |
|      - | 7658 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 7659 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 7660 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 7661 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 7662 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 7663 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 7664 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 7665 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 7666 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 7667 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 7668 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 7669 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 7670 | `	};` |
|      - | 7671 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 7672 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 7673 | `	 */` |
|      - | 7674 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 7675 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 7676 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 7677 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 7678 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 7679 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 7680 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 7681 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 7682 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 7683 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 7684 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 7685 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 7686 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 7687 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 7688 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 7689 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 7690 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 7691 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 7692 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 7693 | `		{"file",      PH7_builtin_file   },` |
|      - | 7694 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 7695 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 7696 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 7697 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 7698 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 7699 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 7700 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 7701 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 7702 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 7703 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 7704 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 7705 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 7706 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 7707 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 7708 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 7709 | `#ifdef PH7_ENABLE_NET` |
|      - | 7710 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 7711 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 7712 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 7713 | `#endif` |
|      - | 7714 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 7715 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 7716 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 7717 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 7718 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 7719 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 7720 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 7721 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 7722 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 7723 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 7724 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 7725 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 7726 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 7727 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7728 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 7729 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 7730 | `	};` |
|   3429 | 7731 | `	const ph7_io_stream *pFileStream = 0;` |
|   3429 | 7732 | `	sxu32 n = 0;` |
|      - | 7733 | `	/* Register disk-related functions */` |
| 167781 | 7734 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 164357 | 7735 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  82181 | 7736 | `	}` |
| 178053 | 7737 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 174629 | 7738 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  87317 | 7739 | `	}` |
|      - | 7740 | `#else` |
|      - | 7741 | `	SXUNUSED(pVm);` |
|      - | 7742 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7743 |  |
|      - | 7744 | `	/*` |
|      - | 7745 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7746 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 7747 | `	 */` |
|      - | 7748 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7749 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 7750 | `		/* Path processing */` |
|      - | 7751 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 7752 | `		{"basename",    PH7_builtin_basename },` |
|      - | 7753 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 7754 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 7755 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 7756 | `		/* ZIP processing */` |
|      - | 7757 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 7758 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 7759 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 7760 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 7761 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 7762 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 7763 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 7764 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 7765 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 7766 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 7767 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 7768 | `	};` |
|  58213 | 7769 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  54789 | 7770 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  27397 | 7771 | `	}` |
|      - | 7772 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7773 |  |
|      - | 7774 | `	/* Install streams if disk I/O is enabled */` |
|      - | 7775 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7776 | `#ifdef __WINNT__` |
|      5 | 7777 | `	pFileStream = &sWinFileStream;` |
|      - | 7778 | `#elif defined(__UNIXES__)` |
|   3424 | 7779 | `	pFileStream = &sUnixFileStream;` |
|      - | 7780 | `#endif` |
|      - | 7781 | `	/* Install the php:// stream */` |
|   3429 | 7782 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3429 | 7783 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 7784 | `#ifdef PH7_ENABLE_NET` |
|   3429 | 7785 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 7786 | `#endif` |
|   3429 | 7787 | `	if( pFileStream ){` |
|      - | 7788 | `		/* Install the file:// stream */` |
|   3429 | 7789 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1712 | 7790 | `	}` |
|      - | 7791 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7792 |  |
|   3429 | 7793 | `	return SXRET_OK;` |
|      5 | 7794 | `}` |
|      - | 7795 | `/*` |
|      - | 7796 | ` * Export the STDIN handle.` |
|      - | 7797 | ` */` |
|      2 | 7798 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 7799 | `{` |
|      - | 7800 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7801 | `	if( pVm->pStdin == 0  ){` |
|      - | 7802 | `		io_private *pIn;` |
|      - | 7803 | `		/* Allocate an IO private instance */` |
|      3 | 7804 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7805 | `		if( pIn == 0 ){` |
|    ! 0 | 7806 | `			return 0;` |
|      - | 7807 | `		}` |
|      3 | 7808 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 7809 | `		/* Initialize the handle */` |
|      3 | 7810 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 7811 | `		/* Install the STDIN stream */` |
|      3 | 7812 | `		pVm->pStdin = pIn;` |
|      3 | 7813 | `		return pIn;` |
|    ! 0 | 7814 | `	}else{` |
|      - | 7815 | `		/* NULL or STDIN */` |
|    ! 0 | 7816 | `		return pVm->pStdin;` |
|      - | 7817 | `	}` |
|      - | 7818 | `#else` |
|      - | 7819 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7820 | `	return 0;` |
|      - | 7821 | `#endif` |
|      2 | 7822 | `}` |
|      - | 7823 | `/*` |
|      - | 7824 | ` * Export the STDOUT handle.` |
|      - | 7825 | ` */` |
|      8 | 7826 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 7827 | `{` |
|      - | 7828 | `#ifndef PH7_DISABLE_DISK_IO` |
|      9 | 7829 | `	if( pVm->pStdout == 0  ){` |
|      - | 7830 | `		io_private *pOut;` |
|      - | 7831 | `		/* Allocate an IO private instance */` |
|      7 | 7832 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      7 | 7833 | `		if( pOut == 0 ){` |
|    ! 0 | 7834 | `			return 0;` |
|      - | 7835 | `		}` |
|      7 | 7836 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 7837 | `		/* Initialize the handle */` |
|      7 | 7838 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 7839 | `		/* Install the STDOUT stream */` |
|      7 | 7840 | `		pVm->pStdout = pOut;` |
|      7 | 7841 | `		return pOut;` |
|    ! 0 | 7842 | `	}else{` |
|      - | 7843 | `		/* NULL or STDOUT */` |
|      3 | 7844 | `		return pVm->pStdout;` |
|      - | 7845 | `	}` |
|      - | 7846 | `#else` |
|      - | 7847 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7848 | `	return 0;` |
|      - | 7849 | `#endif` |
|      5 | 7850 | `}` |
|      - | 7851 | `/*` |
|      - | 7852 | ` * Export the STDERR handle.` |
|      - | 7853 | ` */` |
|     10 | 7854 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 7855 | `{` |
|      - | 7856 | `#ifndef PH7_DISABLE_DISK_IO` |
|     11 | 7857 | `	if( pVm->pStderr == 0  ){` |
|      - | 7858 | `		io_private *pErr;` |
|      - | 7859 | `		/* Allocate an IO private instance */` |
|      9 | 7860 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      9 | 7861 | `		if( pErr == 0 ){` |
|    ! 0 | 7862 | `			return 0;` |
|      - | 7863 | `		}` |
|      9 | 7864 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 7865 | `		/* Initialize the handle */` |
|      9 | 7866 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 7867 | `		/* Install the STDERR stream */` |
|      9 | 7868 | `		pVm->pStderr = pErr;` |
|      9 | 7869 | `		return pErr;` |
|    ! 0 | 7870 | `	}else{` |
|      - | 7871 | `		/* NULL or STDERR */` |
|      3 | 7872 | `		return pVm->pStderr;` |
|      - | 7873 | `	}` |
|      - | 7874 | `#else` |
|      - | 7875 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7876 | `	return 0;` |
|      - | 7877 | `#endif` |
|      6 | 7878 | `}` |
|      - | 7879 |  |
