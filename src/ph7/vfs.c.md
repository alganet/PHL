# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 799/1175 lines (68.00%)

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
|     72 |   24 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      5 |   25 | `{` |
|     77 |   26 | `	const char *zEnd = &zPath[nByte - 1];` |
|      - |   27 | `	int c,d;` |
|     77 |   28 | `	c = d = '/';` |
|      - |   29 | `#ifdef __WINNT__` |
|      5 |   30 | `	d = '\\';` |
|      - |   31 | `#endif` |
|   1739 |   32 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   1631 |   33 | `		zEnd--;` |
|      5 |   34 | `	}` |
|     77 |   35 | `	*pLen = (int)(zEnd-zPath);` |
|      - |   36 | `#ifdef __WINNT__` |
|      5 |   37 | `	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){` |
|      - |   38 | `		/* Normalize path on windows */` |
|    ! 0 |   39 | `		return "\\";` |
|      - |   40 | `	}` |
|      - |   41 | `#endif` |
|     77 |   42 | `	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){` |
|      - |   43 | `		/* No separator,return "." as the current directory */` |
|      8 |   44 | `		*pLen = sizeof(char);` |
|      8 |   45 | `		return ".";` |
|      - |   46 | `	}` |
|     71 |   47 | `	if( (*pLen) == 0 ){` |
|      2 |   48 | `		*pLen = sizeof(char);` |
|      - |   49 | `#ifdef __WINNT__` |
|    ! 0 |   50 | `		return "\\";` |
|      - |   51 | `#else` |
|      2 |   52 | `		return "/";` |
|      - |   53 | `#endif` |
|      - |   54 | `	}` |
|     69 |   55 | `	return zPath;` |
|     41 |   56 | `}` |
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
|  19606 |   68 | `PH7_PRIVATE const char * VfsStrerror(int iErr)` |
|      5 |   69 | `{` |
|      - |   70 | `#if defined(_MSC_VER)` |
|      - |   71 | `#pragma warning(push)` |
|      - |   72 | `#pragma warning(disable:4996)` |
|      - |   73 | `#endif` |
|  19611 |   74 | `	return strerror(iErr);` |
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
|  19584 |   85 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |   86 | `{` |
|  29381 |   87 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  19584 |   88 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  19589 |   89 | `}` |
|     12 |   90 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
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
|  12936 |  104 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  105 | `{` |
|      - |  106 | `	const char *zPath;` |
|      - |  107 | `	ph7_vfs *pVfs;` |
|      - |  108 | `	int rc;` |
|  12941 |  109 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  110 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |  111 | `		ph7_result_bool(pCtx,0);` |
|      6 |  112 | `		return PH7_OK;` |
|      - |  113 | `	}` |
|      - |  114 | `	/* Point to the underlying vfs */` |
|  12937 |  115 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  12937 |  116 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  117 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  118 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  119 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  120 | `			ph7_function_name(pCtx)` |
|      - |  121 | `			);` |
|    ! 0 |  122 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  123 | `		return PH7_OK;` |
|      - |  124 | `	}` |
|      - |  125 | `	/* Point to the desired directory */` |
|  12937 |  126 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  127 | `	/* Perform the requested operation */` |
|  12937 |  128 | `	errno = 0;` |
|  12937 |  129 | `	rc = pVfs->xChdir(zPath);` |
|  12937 |  130 | `	if( rc != PH7_OK ){` |
|      - |  131 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      4 |  132 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      2 |  133 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      1 |  134 | `	}` |
|      - |  135 | `	/* IO return value */` |
|  12937 |  136 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  12937 |  137 | `	return PH7_OK;` |
|   6473 |  138 | `}` |
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
|   8646 |  262 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  263 | `{` |
|      - |  264 | `	const char *zPath;` |
|      - |  265 | `	ph7_vfs *pVfs;` |
|      - |  266 | `	int rc;` |
|   8651 |  267 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  268 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  269 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  270 | `		return PH7_OK;` |
|      - |  271 | `	}` |
|      - |  272 | `	/* Point to the underlying vfs */` |
|   8651 |  273 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8651 |  274 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  275 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  276 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  277 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  278 | `			ph7_function_name(pCtx)` |
|      - |  279 | `			);` |
|    ! 0 |  280 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  281 | `		return PH7_OK;` |
|      - |  282 | `	}` |
|      - |  283 | `	/* Point to the desired directory */` |
|   8651 |  284 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  285 | `	/* Perform the requested operation */` |
|   8651 |  286 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  287 | `	/* IO return value */` |
|   8651 |  288 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8651 |  289 | `	return PH7_OK;` |
|   4328 |  290 | `}` |
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
|      3 |  311 | `{` |
|     49 |  312 | `	int iRecursive = 0;` |
|      - |  313 | `	const char *zPath;` |
|      - |  314 | `	ph7_vfs *pVfs;` |
|      - |  315 | `	int iMode,rc;` |
|     49 |  316 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  317 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  318 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  319 | `		return PH7_OK;` |
|      - |  320 | `	}` |
|      - |  321 | `	/* Point to the underlying vfs */` |
|     49 |  322 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     49 |  323 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  324 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  325 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  326 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  327 | `			ph7_function_name(pCtx)` |
|      - |  328 | `			);` |
|    ! 0 |  329 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  330 | `		return PH7_OK;` |
|      - |  331 | `	}` |
|      - |  332 | `	/* Point to the desired directory */` |
|     49 |  333 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  334 | `#ifdef __WINNT__` |
|      3 |  335 | `	iMode = 0;` |
|      - |  336 | `#else` |
|      - |  337 | `	/* Assume UNIX */` |
|     46 |  338 | `	iMode = 0777;` |
|      - |  339 | `#endif` |
|     49 |  340 | `	if( nArg > 1 ){` |
|    ! 0 |  341 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  342 | `		if( nArg > 2 ){` |
|    ! 0 |  343 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  344 | `		}` |
|    ! 0 |  345 | `	}` |
|      - |  346 | `	/* Perform the requested operation */` |
|     49 |  347 | `	errno = 0;` |
|     49 |  348 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|     49 |  349 | `	if( rc != PH7_OK ){` |
|      - |  350 | `		/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|    ! 0 |  351 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|    ! 0 |  352 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|    ! 0 |  353 | `	}` |
|      - |  354 | `	/* IO return value */` |
|     49 |  355 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     49 |  356 | `	return PH7_OK;` |
|     26 |  357 | `}` |
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
|  32768 |  537 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  538 | `{` |
|      - |  539 | `	const char *zPath;` |
|      - |  540 | `	ph7_vfs *pVfs;` |
|      - |  541 | `	int rc;` |
|  32773 |  542 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  543 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  544 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  545 | `		return PH7_OK;` |
|      - |  546 | `	}` |
|      - |  547 | `	/* Point to the underlying vfs */` |
|  32773 |  548 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  32773 |  549 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  550 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  551 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  552 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  553 | `			ph7_function_name(pCtx)` |
|      - |  554 | `			);` |
|    ! 0 |  555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  556 | `		return PH7_OK;` |
|      - |  557 | `	}` |
|      - |  558 | `	/* Point to the desired directory */` |
|  32773 |  559 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  560 | `	/* Perform the requested operation */` |
|  32773 |  561 | `	errno = 0;` |
|  32773 |  562 | `	rc = pVfs->xUnlink(zPath);` |
|  32773 |  563 | `	if( rc != PH7_OK ){` |
|  19583 |  564 | `		VfsThrowSysWarning(pCtx,zPath);` |
|   9789 |  565 | `	}` |
|      - |  566 | `	/* IO return value */` |
|  32773 |  567 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  32773 |  568 | `	return PH7_OK;` |
|  16389 |  569 | `}` |
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
|    140 |  581 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  582 | `{` |
|      - |  583 | `	const char *zPath;` |
|      - |  584 | `	ph7_vfs *pVfs;` |
|      - |  585 | `	int iMode;` |
|      - |  586 | `	int rc;` |
|    142 |  587 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  588 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  590 | `		return PH7_OK;` |
|      - |  591 | `	}` |
|      - |  592 | `	/* Point to the underlying vfs */` |
|    142 |  593 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    142 |  594 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  595 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  596 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  597 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  598 | `			ph7_function_name(pCtx)` |
|      - |  599 | `			);` |
|    ! 0 |  600 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  601 | `		return PH7_OK;` |
|      - |  602 | `	}` |
|      - |  603 | `	/* Point to the desired directory */` |
|    142 |  604 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  605 | `	/* Extract the mode */` |
|    142 |  606 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  607 | `	/* Perform the requested operation */` |
|    142 |  608 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  609 | `	/* IO return value */` |
|    142 |  610 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    142 |  611 | `	return PH7_OK;` |
|     72 |  612 | `}` |
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
|    160 |  808 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  809 | `{` |
|      - |  810 | `	const char *zPath;` |
|      - |  811 | `	ph7_vfs *pVfs;` |
|      - |  812 | `	int rc;` |
|    162 |  813 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  814 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  815 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  816 | `		return PH7_OK;` |
|      - |  817 | `	}` |
|      - |  818 | `	/* Point to the underlying vfs */` |
|    162 |  819 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    162 |  820 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  821 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  822 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  823 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  824 | `			ph7_function_name(pCtx)` |
|      - |  825 | `			);` |
|    ! 0 |  826 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  827 | `		return PH7_OK;` |
|      - |  828 | `	}` |
|      - |  829 | `	/* Point to the desired directory */` |
|    162 |  830 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  831 | `	/* Perform the requested operation */` |
|    162 |  832 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  833 | `	/* IO return value */` |
|    162 |  834 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    162 |  835 | `	return PH7_OK;` |
|     82 |  836 | `}` |
|      - |  837 | `/*` |
|      - |  838 | ` * int64 file_size(string $filename)` |
|      - |  839 | ` *  Gets the size for the given file.` |
|      - |  840 | ` * Parameters` |
|      - |  841 | ` *  $filename` |
|      - |  842 | ` *   Path to the file.` |
|      - |  843 | ` * Return` |
|      - |  844 | ` *  File size on success or FALSE on failure.` |
|      - |  845 | ` */` |
|     10 |  846 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  847 | `{` |
|      - |  848 | `	const char *zPath;` |
|      - |  849 | `	ph7_int64 iSize;` |
|      - |  850 | `	ph7_vfs *pVfs;` |
|     11 |  851 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  852 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  853 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  854 | `		return PH7_OK;` |
|      - |  855 | `	}` |
|      - |  856 | `	/* Point to the underlying vfs */` |
|     11 |  857 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  858 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - |  859 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  860 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  861 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  862 | `			ph7_function_name(pCtx)` |
|      - |  863 | `			);` |
|    ! 0 |  864 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  865 | `		return PH7_OK;` |
|      - |  866 | `	}` |
|      - |  867 | `	/* Point to the desired directory */` |
|     11 |  868 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  869 | `	/* Perform the requested operation */` |
|     11 |  870 | `	iSize = pVfs->xFileSize(zPath);` |
|     11 |  871 | `	if( iSize < 0 ){` |
|      - |  872 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - |  873 | `		 * truthy and compares equal to nothing a caller would test for. */` |
|    ! 0 |  874 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): stat failed for %s",` |
|    ! 0 |  875 | `			ph7_function_name(pCtx),zPath);` |
|    ! 0 |  876 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  877 | `		return PH7_OK;` |
|      - |  878 | `	}` |
|      - |  879 | `	/* IO return value */` |
|     11 |  880 | `	ph7_result_int64(pCtx,iSize);` |
|     11 |  881 | `	return PH7_OK;` |
|      6 |  882 | `}` |
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
|   6584 | 1006 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1007 | `{` |
|      - | 1008 | `	const char *zPath;` |
|      - | 1009 | `	ph7_vfs *pVfs;` |
|      - | 1010 | `	int rc;` |
|   6589 | 1011 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1012 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1013 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1014 | `		return PH7_OK;` |
|      - | 1015 | `	}` |
|      - | 1016 | `	/* Point to the underlying vfs */` |
|   6589 | 1017 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6589 | 1018 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1019 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1020 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1021 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1022 | `			ph7_function_name(pCtx)` |
|      - | 1023 | `			);` |
|    ! 0 | 1024 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1025 | `		return PH7_OK;` |
|      - | 1026 | `	}` |
|      - | 1027 | `	/* Point to the desired directory */` |
|   6589 | 1028 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1029 | `	/* Perform the requested operation */` |
|   6589 | 1030 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1031 | `	/* IO return value */` |
|   6589 | 1032 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6589 | 1033 | `	return PH7_OK;` |
|   3297 | 1034 | `}` |
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
|     22 | 1548 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1549 | `{` |
|      - | 1550 | `	const char *zPath,*zDir;` |
|      - | 1551 | `	int iLen,iDirlen;` |
|     27 | 1552 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1553 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 1554 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1555 | `		return PH7_OK;` |
|      - | 1556 | `	}` |
|      - | 1557 | `	/* Point to the target path */` |
|     27 | 1558 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     27 | 1559 | `	if( iLen < 1 ){` |
|      - | 1560 | `		/* Reuturn "." */` |
|      2 | 1561 | `		ph7_result_string(pCtx,".",sizeof(char));` |
|      2 | 1562 | `		return PH7_OK;` |
|      - | 1563 | `	}` |
|      - | 1564 | `	/* Perform the requested operation */` |
|     25 | 1565 | `	zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1566 | `	/* Return directory name */` |
|     25 | 1567 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     25 | 1568 | `	return PH7_OK;` |
|     16 | 1569 | `}` |
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
|     71 | 1606 | `	while( zEnd > zPath && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      5 | 1607 | `		zEnd--;` |
|      1 | 1608 | `	}` |
|     45 | 1609 | `	if( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ){` |
|      - | 1610 | `		/* Nothing but separators ("/", "///"): php answers the EMPTY string, where the` |
|      - | 1611 | `		 * strip loop above stops one short and left PH7 returning "/". */` |
|      5 | 1612 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1613 | `		return PH7_OK;` |
|      - | 1614 | `	}` |
|     41 | 1615 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    975 | 1616 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
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
|  13038 | 1659 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1660 | `{` |
|  13043 | 1661 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1662 | `	SyString *pCur;` |
|      - | 1663 | `	int c,d;` |
|  13043 | 1664 | `	c = d = '/';` |
|      - | 1665 | `#ifdef __WINNT__` |
|      5 | 1666 | `	d = '\\';` |
|      - | 1667 | `#endif` |
|      - | 1668 | `	/* Zero the structure */` |
|  13043 | 1669 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1670 | `	/* Handle special case */` |
|  13043 | 1671 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1672 | `#ifdef __WINNT__` |
|    ! 0 | 1673 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1674 | `#else` |
|    ! 0 | 1675 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1676 | `#endif` |
|    ! 0 | 1677 | `		return SXRET_OK;` |
|      - | 1678 | `	}` |
|      - | 1679 | `	/* Extract the basename */` |
| 351402 | 1680 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 331845 | 1681 | `		zEnd--;` |
|      5 | 1682 | `	}` |
|  13043 | 1683 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13043 | 1684 | `	zEnd = &zPath[nByte];` |
|      - | 1685 | `	/* dirname */` |
|  13043 | 1686 | `	pCur = &pOut->sDir;` |
|  13043 | 1687 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13043 | 1688 | `	if( pCur->nByte > 1 ){` |
|  26081 | 1689 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1690 | `#ifdef __WINNT__` |
|      5 | 1691 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1692 | `#endif` |
|   6524 | 1693 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1694 | `#ifdef __WINNT__` |
|    ! 0 | 1695 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1696 | `#else` |
|    ! 0 | 1697 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1698 | `#endif` |
|    ! 0 | 1699 | `	}` |
|      - | 1700 | `	/* basename/filename */` |
|  13043 | 1701 | `	pCur = &pOut->sBasename;` |
|  13043 | 1702 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13043 | 1703 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1704 | `#ifdef __WINNT__` |
|      5 | 1705 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1706 | `#endif` |
|  13043 | 1707 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13043 | 1708 | `	if( pCur->nByte > 0 ){` |
|      - | 1709 | `		/* extension */` |
|  13043 | 1710 | `		zEnd--;` |
|  65189 | 1711 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  52151 | 1712 | `			zEnd--;` |
|      5 | 1713 | `		}` |
|  13043 | 1714 | `		if( zEnd > pCur->zString ){` |
|  13041 | 1715 | `			zEnd++; /* Jump leading dot */` |
|  13041 | 1716 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1717 | `			/* Fix filename */` |
|  13041 | 1718 | `			pCur = &pOut->sFilename;` |
|  13041 | 1719 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13041 | 1720 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6518 | 1721 | `			}` |
|   6518 | 1722 | `		}` |
|   6519 | 1723 | `	}` |
|  13043 | 1724 | `	return SXRET_OK;` |
|   6524 | 1725 | `}` |
|      - | 1726 | `/*` |
|      - | 1727 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1728 | ` *  See block comment above.` |
|      - | 1729 | ` */` |
|  13038 | 1730 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1731 | `{` |
|      - | 1732 | `	const char *zPath;` |
|      - | 1733 | `	path_info sInfo;` |
|      - | 1734 | `	SyString *pComp;` |
|      - | 1735 | `	int iLen;` |
|  13043 | 1736 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1737 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1738 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1739 | `		return PH7_OK;` |
|      - | 1740 | `	}` |
|      - | 1741 | `	/* Point to the target path */` |
|  13043 | 1742 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13043 | 1743 | `	if( iLen < 1 ){` |
|      - | 1744 | `		/* Empty string */` |
|    ! 0 | 1745 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1746 | `		return PH7_OK;` |
|      - | 1747 | `	}` |
|      - | 1748 | `	/* Extract path info */` |
|  13043 | 1749 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  19561 | 1750 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1751 | `		/* Return path component */` |
|  13041 | 1752 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13041 | 1753 | `		switch(nComp){` |
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
|   3260 | 1772 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6525 | 1773 | `			pComp = &sInfo.sExtension;` |
|   6525 | 1774 | `			if( pComp->nByte > 0 ){` |
|   6523 | 1775 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3264 | 1776 | `			}else{` |
|      - | 1777 | `				/* Expand the empty string */` |
|      3 | 1778 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1779 | `			}` |
|   6525 | 1780 | `			break;` |
|   3256 | 1781 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6517 | 1782 | `			pComp = &sInfo.sFilename;` |
|   6517 | 1783 | `			if( pComp->nByte > 0 ){` |
|   6517 | 1784 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3261 | 1785 | `			}else{` |
|      - | 1786 | `				/* Expand the empty string */` |
|    ! 0 | 1787 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1788 | `			}` |
|   6517 | 1789 | `			break;` |
|    ! 0 | 1790 | `		default:` |
|      - | 1791 | `			/* Expand the empty string */` |
|    ! 0 | 1792 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1793 | `			break;` |
|      - | 1794 | `		}` |
|   6523 | 1795 | `	}else{` |
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
|  13043 | 1845 | `	return PH7_OK;` |
|   6524 | 1846 | `}` |
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
|     47 | 2012 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     47 | 2013 | `      if( noCase ){` |
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
|      7 | 2076 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
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
|    214 | 2244 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2245 | `{` |
|      - | 2246 | `	ph7_vfs *pVfs;` |
|      - | 2247 | `	/* Set the empty string as the default return value */` |
|    217 | 2248 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2249 | `	/* Point to the underlying vfs */` |
|    217 | 2250 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    217 | 2251 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
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
|    217 | 2262 | `	pVfs->xTempDir(pCtx);` |
|    217 | 2263 | `	return PH7_OK;` |
|    110 | 2264 | `}` |
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
|      - | 2522 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 2523 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 2524 | `#if defined(_MSC_VER)` |
|      - | 2525 | `static const ph7_vfs null_vfs = {` |
|      - | 2526 | `#else` |
|      - | 2527 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 2528 | `#endif` |
|      - | 2529 | `	"null_vfs",` |
|      - | 2530 | `	PH7_VFS_VERSION,` |
|      - | 2531 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 2532 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 2533 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 2534 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 2535 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 2536 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 2537 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 2538 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 2539 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 2540 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 2541 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 2542 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 2543 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 2544 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 2545 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 2546 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 2547 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 2548 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 2549 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 2550 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 2551 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 2552 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 2553 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 2554 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 2555 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 2556 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 2557 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 2558 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 2559 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 2560 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 2561 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 2562 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 2563 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 2564 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 2565 | `	0, /* int (*xUmask)(int) */` |
|      - | 2566 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 2567 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 2568 | `	0, /* int (*xUid)(void) */` |
|      - | 2569 | `	0, /* int (*xGid)(void) */` |
|      - | 2570 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 2571 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 2572 | `};` |
|      - | 2573 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 2574 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 2575 | `/*` |
|      - | 2576 | ` * Export the builtin vfs.` |
|      - | 2577 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 2578 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 2579 | ` * Note:` |
|      - | 2580 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 2581 | ` * Note:` |
|      - | 2582 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2583 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 2584 | ` */` |
|   3884 | 2585 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 2586 | `{` |
|      - | 2587 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 2588 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 2589 | `	return &null_vfs;` |
|      - | 2590 | `#else` |
|      - | 2591 | `#ifdef __WINNT__` |
|      5 | 2592 | `	return &sWinVfs;` |
|      - | 2593 | `#elif defined(__UNIXES__)` |
|   3884 | 2594 | `	return &sUnixVfs;` |
|      - | 2595 | `#else` |
|      - | 2596 | `	return &null_vfs;` |
|      - | 2597 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 2598 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 2599 | `#else` |
|      - | 2600 | `	return &null_vfs;` |
|      - | 2601 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 2602 | `}` |
|      - | 2603 | `/*` |
|      - | 2604 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 2605 | ` * [i.e: file://,php://].` |
|      - | 2606 | ` * Note:` |
|      - | 2607 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 2608 | ` *  defined then this function is a no-op.` |
|      - | 2609 | ` */` |
|   3412 | 2610 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 2611 | `{` |
|      - | 2612 | `	/*` |
|      - | 2613 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 2614 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 2615 | `	 */` |
|      - | 2616 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 2617 | `	/* VFS: disk I/O related functions */` |
|      - | 2618 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 2619 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 2620 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 2621 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 2622 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 2623 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 2624 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 2625 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 2626 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 2627 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 2628 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 2629 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 2630 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 2631 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 2632 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 2633 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 2634 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 2635 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 2636 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 2637 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 2638 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 2639 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 2640 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 2641 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 2642 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 2643 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 2644 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 2645 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 2646 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 2647 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 2648 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 2649 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 2650 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 2651 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 2652 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 2653 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 2654 | `		{"link",        PH7_vfs_link     },` |
|      - | 2655 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 2656 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 2657 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 2658 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 2659 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 2660 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 2661 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 2662 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 2663 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 2664 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 2665 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 2666 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 2667 | `	};` |
|      - | 2668 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 2669 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 2670 | `	 */` |
|      - | 2671 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 2672 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 2673 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 2674 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 2675 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 2676 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 2677 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 2678 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 2679 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 2680 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 2681 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 2682 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 2683 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 2684 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 2685 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 2686 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 2687 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 2688 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 2689 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 2690 | `		{"file",      PH7_builtin_file   },` |
|      - | 2691 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 2692 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 2693 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 2694 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 2695 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 2696 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 2697 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 2698 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 2699 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 2700 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 2701 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 2702 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 2703 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 2704 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 2705 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 2706 | `#ifdef PH7_ENABLE_NET` |
|      - | 2707 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 2708 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 2709 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 2710 | `#endif` |
|      - | 2711 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 2712 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 2713 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 2714 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 2715 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 2716 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 2717 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 2718 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 2719 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 2720 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 2721 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 2722 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 2723 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 2724 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 2725 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 2726 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 2727 | `	};` |
|   3417 | 2728 | `	const ph7_io_stream *pFileStream = 0;` |
|   3417 | 2729 | `	sxu32 n = 0;` |
|      - | 2730 | `	/* Register disk-related functions */` |
| 167193 | 2731 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 163781 | 2732 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  81893 | 2733 | `	}` |
| 177429 | 2734 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 174017 | 2735 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  87011 | 2736 | `	}` |
|      - | 2737 | `#else` |
|      - | 2738 | `	SXUNUSED(pVm);` |
|      - | 2739 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 2740 |  |
|      - | 2741 | `	/*` |
|      - | 2742 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2743 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 2744 | `	 */` |
|      - | 2745 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2746 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 2747 | `		/* Path processing */` |
|      - | 2748 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 2749 | `		{"basename",    PH7_builtin_basename },` |
|      - | 2750 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 2751 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 2752 | `		{"fnmatch",     PH7_builtin_fnmatch  }` |
|      - | 2753 | `	};` |
|  20477 | 2754 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  17065 | 2755 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|   8535 | 2756 | `	}` |
|      - | 2757 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2758 |  |
|      - | 2759 | `	/* Install streams if disk I/O is enabled */` |
|      - | 2760 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 2761 | `#ifdef __WINNT__` |
|      5 | 2762 | `	pFileStream = &sWinFileStream;` |
|      - | 2763 | `#elif defined(__UNIXES__)` |
|   3412 | 2764 | `	pFileStream = &sUnixFileStream;` |
|      - | 2765 | `#endif` |
|      - | 2766 | `	/* Install the php:// stream */` |
|   3417 | 2767 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3417 | 2768 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 2769 | `#ifdef PH7_ENABLE_NET` |
|   3417 | 2770 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 2771 | `#endif` |
|   3417 | 2772 | `	if( pFileStream ){` |
|      - | 2773 | `		/* Install the file:// stream */` |
|   3417 | 2774 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1706 | 2775 | `	}` |
|      - | 2776 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 2777 |  |
|   3417 | 2778 | `	return SXRET_OK;` |
|      5 | 2779 | `}` |
|      - | 2780 |  |
