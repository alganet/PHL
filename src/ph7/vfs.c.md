# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2471/3649 lines (67.72%)

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
|      - |   14 | `#endif` |
|      - |   15 | `/*` |
|      - |   16 | ` * This file implement a virtual file systems (VFS) for the PH7 engine.` |
|      - |   17 | ` */` |
|      - |   18 | `/*` |
|      - |   19 | ` * Given a string containing the path of a file or directory, this function` |
|      - |   20 | ` * return the parent directory's path.` |
|      - |   21 | ` */` |
|     62 |   22 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      5 |   23 | `{` |
|     67 |   24 | `	const char *zEnd = &zPath[nByte - 1];` |
|      - |   25 | `	int c,d;` |
|     67 |   26 | `	c = d = '/';` |
|      - |   27 | `#ifdef __WINNT__` |
|      5 |   28 | `	d = '\\';` |
|      - |   29 | `#endif` |
|   1400 |   30 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   1307 |   31 | `		zEnd--;` |
|      5 |   32 | `	}` |
|     67 |   33 | `	*pLen = (int)(zEnd-zPath);` |
|      - |   34 | `#ifdef __WINNT__` |
|      5 |   35 | `	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){` |
|      - |   36 | `		/* Normalize path on windows */` |
|    ! 0 |   37 | `		return "\\";` |
|      - |   38 | `	}` |
|      - |   39 | `#endif` |
|     67 |   40 | `	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){` |
|      - |   41 | `		/* No separator,return "." as the current directory */` |
|      8 |   42 | `		*pLen = sizeof(char);` |
|      8 |   43 | `		return ".";` |
|      - |   44 | `	}` |
|     61 |   45 | `	if( (*pLen) == 0 ){` |
|      2 |   46 | `		*pLen = sizeof(char);` |
|      - |   47 | `#ifdef __WINNT__` |
|    ! 0 |   48 | `		return "\\";` |
|      - |   49 | `#else` |
|      2 |   50 | `		return "/";` |
|      - |   51 | `#endif` |
|      - |   52 | `	}` |
|     59 |   53 | `	return zPath;` |
|     36 |   54 | `}` |
|      - |   55 | `/*` |
|      - |   56 | ` * Compile the VFS implementations when builtins are enabled OR when disk I/O` |
|      - |   57 | ` * is explicitly enabled (i.e. PH7_DISABLE_DISK_IO is NOT defined).` |
|      - |   58 | ` */` |
|      - |   59 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |   60 | `/*` |
|      - |   61 | ` * strerror() trips MSVC's C4996 "may be unsafe" deprecation under /WX. It is a` |
|      - |   62 | ` * standard C function we use deliberately to mirror php's IO error text; wrap it` |
|      - |   63 | ` * once with the deprecation suppressed. The pragma is _MSC_VER-guarded so the` |
|      - |   64 | ` * GCC/-Werror Linux build never sees an unknown-pragma warning.` |
|      - |   65 | ` */` |
|  19896 |   66 | `static const char * VfsStrerror(int iErr)` |
|      5 |   67 | `{` |
|      - |   68 | `#if defined(_MSC_VER)` |
|      - |   69 | `#pragma warning(push)` |
|      - |   70 | `#pragma warning(disable:4996)` |
|      - |   71 | `#endif` |
|  19901 |   72 | `	return strerror(iErr);` |
|      - |   73 | `#if defined(_MSC_VER)` |
|      - |   74 | `#pragma warning(pop)` |
|      - |   75 | `#endif` |
|      5 |   76 | `}` |
|      - |   77 | `/*` |
|      - |   78 | ` * php's non-open IO failures: "unlink(/nope): No such file or directory".` |
|      - |   79 | ` * PH7 returned FALSE in SILENCE for unlink/rmdir/mkdir/rename/chdir/opendir/scandir and` |
|      - |   80 | ` * filesize, so a script could not tell a failed operation from a successful one without` |
|      - |   81 | ` * checking the return value it never got told to check.` |
|      - |   82 | ` */` |
|  19876 |   83 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |   84 | `{` |
|  29819 |   85 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  19876 |   86 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  19881 |   87 | `}` |
|     10 |   88 | `static void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      3 |   89 | `{` |
|     18 |   90 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open stream: %s",` |
|     10 |   91 | `		ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));` |
|     13 |   92 | `}` |
|      - |   93 | `/*` |
|      - |   94 | ` * bool chdir(string $directory)` |
|      - |   95 | ` *  Change the current directory.` |
|      - |   96 | ` * Parameters` |
|      - |   97 | ` *  $directory` |
|      - |   98 | ` *   The new current directory` |
|      - |   99 | ` * Return` |
|      - |  100 | ` *  TRUE on success or FALSE on failure.` |
|      - |  101 | ` */` |
|  13404 |  102 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  103 | `{` |
|      - |  104 | `	const char *zPath;` |
|      - |  105 | `	ph7_vfs *pVfs;` |
|      - |  106 | `	int rc;` |
|  13409 |  107 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  108 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |  109 | `		ph7_result_bool(pCtx,0);` |
|      6 |  110 | `		return PH7_OK;` |
|      - |  111 | `	}` |
|      - |  112 | `	/* Point to the underlying vfs */` |
|  13405 |  113 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13405 |  114 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  115 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  116 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  117 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  118 | `			ph7_function_name(pCtx)` |
|      - |  119 | `			);` |
|    ! 0 |  120 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  121 | `		return PH7_OK;` |
|      - |  122 | `	}` |
|      - |  123 | `	/* Point to the desired directory */` |
|  13405 |  124 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  125 | `	/* Perform the requested operation */` |
|  13405 |  126 | `	errno = 0;` |
|  13405 |  127 | `	rc = pVfs->xChdir(zPath);` |
|  13405 |  128 | `	if( rc != PH7_OK ){` |
|      - |  129 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      4 |  130 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      2 |  131 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      1 |  132 | `	}` |
|      - |  133 | `	/* IO return value */` |
|  13405 |  134 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13405 |  135 | `	return PH7_OK;` |
|   6707 |  136 | `}` |
|      - |  137 | `/*` |
|      - |  138 | ` * bool chroot(string $directory)` |
|      - |  139 | ` *  Change the root directory.` |
|      - |  140 | ` * Parameters` |
|      - |  141 | ` *  $directory` |
|      - |  142 | ` *   The path to change the root directory to` |
|      - |  143 | ` * Return` |
|      - |  144 | ` *  TRUE on success or FALSE on failure.` |
|      - |  145 | ` */` |
|      6 |  146 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  147 | `{` |
|      - |  148 | `	const char *zPath;` |
|      - |  149 | `	ph7_vfs *pVfs;` |
|      - |  150 | `	int rc;` |
|      7 |  151 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  152 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 |  153 | `		ph7_result_bool(pCtx,0);` |
|      5 |  154 | `		return PH7_OK;` |
|      - |  155 | `	}` |
|      - |  156 | `	/* Point to the underlying vfs */` |
|      3 |  157 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  158 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  159 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  160 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  161 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  162 | `			ph7_function_name(pCtx)` |
|      - |  163 | `			);` |
|    ! 0 |  164 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  165 | `		return PH7_OK;` |
|      - |  166 | `	}` |
|      - |  167 | `	/* Point to the desired directory */` |
|      3 |  168 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  169 | `	/* Perform the requested operation */` |
|      3 |  170 | `	rc = pVfs->xChroot(zPath);` |
|      - |  171 | `	/* IO return value */` |
|      3 |  172 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 |  173 | `	return PH7_OK;` |
|      4 |  174 | `}` |
|      - |  175 | `/*` |
|      - |  176 | ` * string getcwd(void)` |
|      - |  177 | ` *  Gets the current working directory.` |
|      - |  178 | ` * Parameters` |
|      - |  179 | ` *  None` |
|      - |  180 | ` * Return` |
|      - |  181 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  182 | ` */` |
|     20 |  183 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  184 | `{` |
|      - |  185 | `	ph7_vfs *pVfs;` |
|      - |  186 | `	int rc;` |
|      - |  187 | `	/* Point to the underlying vfs */` |
|     25 |  188 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     25 |  189 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  190 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  191 | `		SXUNUSED(apArg);` |
|      - |  192 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  193 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  194 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  195 | `			ph7_function_name(pCtx)` |
|      - |  196 | `			);` |
|    ! 0 |  197 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  198 | `		return PH7_OK;` |
|      - |  199 | `	}` |
|     25 |  200 | `	ph7_result_string(pCtx,"",0);` |
|      - |  201 | `	/* Perform the requested operation */` |
|     25 |  202 | `	rc = pVfs->xGetcwd(pCtx);` |
|     25 |  203 | `	if( rc != PH7_OK ){` |
|      - |  204 | `		/* Error,return FALSE */` |
|    ! 0 |  205 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  206 | `	}` |
|     25 |  207 | `	return PH7_OK;` |
|     15 |  208 | `}` |
|      - |  209 | `/*` |
|      - |  210 | ` * bool rmdir(string $directory)` |
|      - |  211 | ` *  Removes directory.` |
|      - |  212 | ` * Parameters` |
|      - |  213 | ` *  $directory` |
|      - |  214 | ` *   The path to the directory` |
|      - |  215 | ` * Return` |
|      - |  216 | ` *  TRUE on success or FALSE on failure.` |
|      - |  217 | ` */` |
|     40 |  218 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  219 | `{` |
|      - |  220 | `	const char *zPath;` |
|      - |  221 | `	ph7_vfs *pVfs;` |
|      - |  222 | `	int rc;` |
|     41 |  223 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  224 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  225 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  226 | `		return PH7_OK;` |
|      - |  227 | `	}` |
|      - |  228 | `	/* Point to the underlying vfs */` |
|     41 |  229 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     41 |  230 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  231 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  232 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  233 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  234 | `			ph7_function_name(pCtx)` |
|      - |  235 | `			);` |
|    ! 0 |  236 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  237 | `		return PH7_OK;` |
|      - |  238 | `	}` |
|      - |  239 | `	/* Point to the desired directory */` |
|     41 |  240 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  241 | `	/* Perform the requested operation */` |
|     41 |  242 | `	errno = 0;` |
|     41 |  243 | `	rc = pVfs->xRmdir(zPath);` |
|     41 |  244 | `	if( rc != PH7_OK ){` |
|      3 |  245 | `		VfsThrowSysWarning(pCtx,zPath);` |
|      1 |  246 | `	}` |
|      - |  247 | `	/* IO return value */` |
|     41 |  248 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     41 |  249 | `	return PH7_OK;` |
|     21 |  250 | `}` |
|      - |  251 | `/*` |
|      - |  252 | ` * bool is_dir(string $filename)` |
|      - |  253 | ` *  Tells whether the given filename is a directory.` |
|      - |  254 | ` * Parameters` |
|      - |  255 | ` *  $filename` |
|      - |  256 | ` *   Path to the file.` |
|      - |  257 | ` * Return` |
|      - |  258 | ` *  TRUE on success or FALSE on failure.` |
|      - |  259 | ` */` |
|   8792 |  260 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  261 | `{` |
|      - |  262 | `	const char *zPath;` |
|      - |  263 | `	ph7_vfs *pVfs;` |
|      - |  264 | `	int rc;` |
|   8797 |  265 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  266 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  267 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  268 | `		return PH7_OK;` |
|      - |  269 | `	}` |
|      - |  270 | `	/* Point to the underlying vfs */` |
|   8797 |  271 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8797 |  272 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  273 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  274 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  275 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  276 | `			ph7_function_name(pCtx)` |
|      - |  277 | `			);` |
|    ! 0 |  278 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  279 | `		return PH7_OK;` |
|      - |  280 | `	}` |
|      - |  281 | `	/* Point to the desired directory */` |
|   8797 |  282 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  283 | `	/* Perform the requested operation */` |
|   8797 |  284 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  285 | `	/* IO return value */` |
|   8797 |  286 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8797 |  287 | `	return PH7_OK;` |
|   4401 |  288 | `}` |
|      - |  289 | `/*` |
|      - |  290 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  291 | ` *  Make a directory.` |
|      - |  292 | ` * Parameters` |
|      - |  293 | ` *  $pathname` |
|      - |  294 | ` *   The directory path.` |
|      - |  295 | ` * $mode` |
|      - |  296 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  297 | ` *  Note:` |
|      - |  298 | ` *   mode is ignored on Windows.` |
|      - |  299 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  300 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  301 | ` *   which you can change using umask().` |
|      - |  302 | ` * $recursive` |
|      - |  303 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  304 | ` *  Defaults to FALSE. (Not used)` |
|      - |  305 | ` * Return` |
|      - |  306 | ` *  TRUE on success or FALSE on failure.` |
|      - |  307 | ` */` |
|     40 |  308 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  309 | `{` |
|     42 |  310 | `	int iRecursive = 0;` |
|      - |  311 | `	const char *zPath;` |
|      - |  312 | `	ph7_vfs *pVfs;` |
|      - |  313 | `	int iMode,rc;` |
|     42 |  314 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  315 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  316 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  317 | `		return PH7_OK;` |
|      - |  318 | `	}` |
|      - |  319 | `	/* Point to the underlying vfs */` |
|     42 |  320 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     42 |  321 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  322 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  323 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  324 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  325 | `			ph7_function_name(pCtx)` |
|      - |  326 | `			);` |
|    ! 0 |  327 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  328 | `		return PH7_OK;` |
|      - |  329 | `	}` |
|      - |  330 | `	/* Point to the desired directory */` |
|     42 |  331 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  332 | `#ifdef __WINNT__` |
|      2 |  333 | `	iMode = 0;` |
|      - |  334 | `#else` |
|      - |  335 | `	/* Assume UNIX */` |
|     40 |  336 | `	iMode = 0777;` |
|      - |  337 | `#endif` |
|     42 |  338 | `	if( nArg > 1 ){` |
|    ! 0 |  339 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  340 | `		if( nArg > 2 ){` |
|    ! 0 |  341 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  342 | `		}` |
|    ! 0 |  343 | `	}` |
|      - |  344 | `	/* Perform the requested operation */` |
|     42 |  345 | `	errno = 0;` |
|     42 |  346 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|     42 |  347 | `	if( rc != PH7_OK ){` |
|      - |  348 | `		/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|    ! 0 |  349 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|    ! 0 |  350 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|    ! 0 |  351 | `	}` |
|      - |  352 | `	/* IO return value */` |
|     42 |  353 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     42 |  354 | `	return PH7_OK;` |
|     22 |  355 | `}` |
|      - |  356 | `/*` |
|      - |  357 | ` * bool rename(string $oldname,string $newname)` |
|      - |  358 | ` *  Attempts to rename oldname to newname.` |
|      - |  359 | ` * Parameters` |
|      - |  360 | ` *  $oldname` |
|      - |  361 | ` *   Old name.` |
|      - |  362 | ` *  $newname` |
|      - |  363 | ` *   New name.` |
|      - |  364 | ` * Return` |
|      - |  365 | ` *  TRUE on success or FALSE on failure.` |
|      - |  366 | ` */` |
|      2 |  367 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  368 | `{` |
|      - |  369 | `	const char *zOld,*zNew;` |
|      - |  370 | `	ph7_vfs *pVfs;` |
|      - |  371 | `	int rc;` |
|      3 |  372 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  373 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  374 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  375 | `		return PH7_OK;` |
|      - |  376 | `	}` |
|      - |  377 | `	/* Point to the underlying vfs */` |
|      3 |  378 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  379 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  380 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  381 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  382 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  383 | `			ph7_function_name(pCtx)` |
|      - |  384 | `			);` |
|    ! 0 |  385 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  386 | `		return PH7_OK;` |
|      - |  387 | `	}` |
|      - |  388 | `	/* Perform the requested operation */` |
|      3 |  389 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  390 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  391 | `	errno = 0;` |
|      3 |  392 | `	rc = pVfs->xRename(zOld,zNew);` |
|      3 |  393 | `	if( rc != PH7_OK ){` |
|      - |  394 | `		/* php names BOTH paths here */` |
|    ! 0 |  395 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    ! 0 |  396 | `			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));` |
|    ! 0 |  397 | `	}` |
|      - |  398 | `	/* IO result */` |
|      3 |  399 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  400 | `	return PH7_OK;` |
|      2 |  401 | `}` |
|      - |  402 | `/*` |
|      - |  403 | ` * string realpath(string $path)` |
|      - |  404 | ` *  Returns canonicalized absolute pathname.` |
|      - |  405 | ` * Parameters` |
|      - |  406 | ` *  $path` |
|      - |  407 | ` *   Target path.` |
|      - |  408 | ` * Return` |
|      - |  409 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  410 | ` */` |
|      4 |  411 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  412 | `{` |
|      - |  413 | `	const char *zPath;` |
|      - |  414 | `	ph7_vfs *pVfs;` |
|      - |  415 | `        int rc;` |
|      5 |  416 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  417 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  418 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  419 | `		return PH7_OK;` |
|      - |  420 | `	}` |
|      - |  421 | `	/* Point to the underlying vfs */` |
|      5 |  422 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  423 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  424 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  425 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  426 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  427 | `			ph7_function_name(pCtx)` |
|      - |  428 | `			);` |
|    ! 0 |  429 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  430 | `		return PH7_OK;` |
|      - |  431 | `	}` |
|      - |  432 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|      5 |  433 | `	ph7_result_string(pCtx,"",0);` |
|      - |  434 | `	/* Perform the requested operation */` |
|      5 |  435 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      5 |  436 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|      5 |  437 | `	if( rc != PH7_OK ){` |
|      2 |  438 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  439 | `	}` |
|      5 |  440 | `	return PH7_OK;` |
|      3 |  441 | `}` |
|      - |  442 | `/*` |
|      - |  443 | ` * int sleep(int $seconds)` |
|      - |  444 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  445 | ` * Parameters` |
|      - |  446 | ` *  $seconds` |
|      - |  447 | ` *   Halt time in seconds.` |
|      - |  448 | ` * Return` |
|      - |  449 | ` *  Zero on success or FALSE on failure.` |
|      - |  450 | ` */` |
|     10 |  451 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  452 | `{` |
|      - |  453 | `	ph7_vfs *pVfs;` |
|      - |  454 | `	int rc,nSleep;` |
|     11 |  455 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  456 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  457 | `		ph7_result_bool(pCtx,0);` |
|      3 |  458 | `		return PH7_OK;` |
|      - |  459 | `	}` |
|      - |  460 | `	/* Point to the underlying vfs */` |
|      9 |  461 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 |  462 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  463 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  464 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  465 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  466 | `			ph7_function_name(pCtx)` |
|      - |  467 | `			);` |
|    ! 0 |  468 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  469 | `		return PH7_OK;` |
|      - |  470 | `	}` |
|      - |  471 | `	/* Amount to sleep */` |
|      9 |  472 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|      9 |  473 | `	if( nSleep < 0 ){` |
|      - |  474 | `		/* Invalid value,return FALSE */` |
|      3 |  475 | `		ph7_result_bool(pCtx,0);` |
|      3 |  476 | `		return PH7_OK;` |
|      - |  477 | `	}` |
|      - |  478 | `	/* Perform the requested operation (Microseconds) */` |
|      7 |  479 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      7 |  480 | `	if( rc != PH7_OK ){` |
|      - |  481 | `		/* Return FALSE */` |
|    ! 0 |  482 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  483 | `	}else{` |
|      - |  484 | `		/* Return zero */` |
|      7 |  485 | `		ph7_result_int(pCtx,0);` |
|      - |  486 | `	}` |
|      7 |  487 | `	return PH7_OK;` |
|      6 |  488 | `}` |
|      - |  489 | `/*` |
|      - |  490 | ` * void usleep(int $micro_seconds)` |
|      - |  491 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  492 | ` * Parameters` |
|      - |  493 | ` *  $micro_seconds` |
|      - |  494 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  495 | ` * Return` |
|      - |  496 | ` *  None.` |
|      - |  497 | ` */` |
|     56 |  498 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  499 | `{` |
|      - |  500 | `	ph7_vfs *pVfs;` |
|      - |  501 | `	int nSleep;` |
|     57 |  502 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  503 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  504 | `		return PH7_OK;` |
|      - |  505 | `	}` |
|      - |  506 | `	/* Point to the underlying vfs */` |
|     57 |  507 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     57 |  508 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  509 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  510 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  511 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  512 | `			ph7_function_name(pCtx)` |
|      - |  513 | `			);` |
|    ! 0 |  514 | `		return PH7_OK;` |
|      - |  515 | `	}` |
|      - |  516 | `	/* Amount to sleep */` |
|     57 |  517 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     57 |  518 | `	if( nSleep < 0 ){` |
|      - |  519 | `		/* Invalid value,return immediately */` |
|      3 |  520 | `		return PH7_OK;` |
|      - |  521 | `	}` |
|      - |  522 | `	/* Perform the requested operation (Microseconds) */` |
|     55 |  523 | `	pVfs->xSleep((unsigned int)nSleep);` |
|     55 |  524 | `	return PH7_OK;` |
|     29 |  525 | `}` |
|      - |  526 | `/*` |
|      - |  527 | ` * bool unlink (string $filename)` |
|      - |  528 | ` *  Delete a file.` |
|      - |  529 | ` * Parameters` |
|      - |  530 | ` *  $filename` |
|      - |  531 | ` *   Path to the file.` |
|      - |  532 | ` * Return` |
|      - |  533 | ` *  TRUE on success or FALSE on failure.` |
|      - |  534 | ` */` |
|  33544 |  535 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  536 | `{` |
|      - |  537 | `	const char *zPath;` |
|      - |  538 | `	ph7_vfs *pVfs;` |
|      - |  539 | `	int rc;` |
|  33549 |  540 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  541 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  542 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  543 | `		return PH7_OK;` |
|      - |  544 | `	}` |
|      - |  545 | `	/* Point to the underlying vfs */` |
|  33549 |  546 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  33549 |  547 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  548 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  549 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  550 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  551 | `			ph7_function_name(pCtx)` |
|      - |  552 | `			);` |
|    ! 0 |  553 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  554 | `		return PH7_OK;` |
|      - |  555 | `	}` |
|      - |  556 | `	/* Point to the desired directory */` |
|  33549 |  557 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  558 | `	/* Perform the requested operation */` |
|  33549 |  559 | `	errno = 0;` |
|  33549 |  560 | `	rc = pVfs->xUnlink(zPath);` |
|  33549 |  561 | `	if( rc != PH7_OK ){` |
|  19879 |  562 | `		VfsThrowSysWarning(pCtx,zPath);` |
|   9937 |  563 | `	}` |
|      - |  564 | `	/* IO return value */` |
|  33549 |  565 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  33549 |  566 | `	return PH7_OK;` |
|  16777 |  567 | `}` |
|      - |  568 | `/*` |
|      - |  569 | ` * bool chmod(string $filename,int $mode)` |
|      - |  570 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - |  571 | ` * Parameters` |
|      - |  572 | ` *  $filename` |
|      - |  573 | ` *   Path to the file.` |
|      - |  574 | ` * $mode` |
|      - |  575 | ` *   Mode (Must be an integer)` |
|      - |  576 | ` * Return` |
|      - |  577 | ` *  TRUE on success or FALSE on failure.` |
|      - |  578 | ` */` |
|    144 |  579 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  580 | `{` |
|      - |  581 | `	const char *zPath;` |
|      - |  582 | `	ph7_vfs *pVfs;` |
|      - |  583 | `	int iMode;` |
|      - |  584 | `	int rc;` |
|    147 |  585 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  586 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  587 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  588 | `		return PH7_OK;` |
|      - |  589 | `	}` |
|      - |  590 | `	/* Point to the underlying vfs */` |
|    147 |  591 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    147 |  592 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  593 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  594 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  595 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  596 | `			ph7_function_name(pCtx)` |
|      - |  597 | `			);` |
|    ! 0 |  598 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  599 | `		return PH7_OK;` |
|      - |  600 | `	}` |
|      - |  601 | `	/* Point to the desired directory */` |
|    147 |  602 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  603 | `	/* Extract the mode */` |
|    147 |  604 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  605 | `	/* Perform the requested operation */` |
|    147 |  606 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  607 | `	/* IO return value */` |
|    147 |  608 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    147 |  609 | `	return PH7_OK;` |
|     75 |  610 | `}` |
|      - |  611 | `/*` |
|      - |  612 | ` * bool chown(string $filename,string $user)` |
|      - |  613 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - |  614 | ` * Parameters` |
|      - |  615 | ` *  $filename` |
|      - |  616 | ` *   Path to the file.` |
|      - |  617 | ` * $user` |
|      - |  618 | ` *   Username.` |
|      - |  619 | ` * Return` |
|      - |  620 | ` *  TRUE on success or FALSE on failure.` |
|      - |  621 | ` */` |
|      6 |  622 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  623 | `{` |
|      - |  624 | `	const char *zPath,*zUser;` |
|      - |  625 | `	ph7_vfs *pVfs;` |
|      - |  626 | `	int rc;` |
|      7 |  627 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  628 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  629 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  630 | `		return PH7_OK;` |
|      - |  631 | `	}` |
|      - |  632 | `	/* Point to the underlying vfs */` |
|      7 |  633 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  634 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - |  635 | `		/* IO routine not implemented,return NULL */` |
|      1 |  636 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  637 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  638 | `			ph7_function_name(pCtx)` |
|      - |  639 | `			);` |
|      1 |  640 | `		ph7_result_bool(pCtx,0);` |
|      1 |  641 | `		return PH7_OK;` |
|      - |  642 | `	}` |
|      - |  643 | `	/* Point to the desired directory */` |
|      6 |  644 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  645 | `	/* Extract the user */` |
|      6 |  646 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - |  647 | `	/* Perform the requested operation */` |
|      6 |  648 | `	errno = 0;` |
|      6 |  649 | `	rc = pVfs->xChown(zPath,zUser);` |
|      6 |  650 | `	if( rc != PH7_OK ){` |
|      - |  651 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  652 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  653 | `		 * "chown(): Operation not permitted". */` |
|      6 |  654 | `		if( rc == -2 ){` |
|      3 |  655 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",` |
|      1 |  656 | `				ph7_function_name(pCtx),zUser);` |
|      1 |  657 | `		}else{` |
|      6 |  658 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  659 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  660 | `		}` |
|      3 |  661 | `	}` |
|      - |  662 | `	/* IO return value */` |
|      6 |  663 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  664 | `	return PH7_OK;` |
|      4 |  665 | `}` |
|      - |  666 | `/*` |
|      - |  667 | ` * bool chgrp(string $filename,string $group)` |
|      - |  668 | ` *  Attempts to change the group of the file filename to group.` |
|      - |  669 | ` * Parameters` |
|      - |  670 | ` *  $filename` |
|      - |  671 | ` *   Path to the file.` |
|      - |  672 | ` * $group` |
|      - |  673 | ` *   groupname.` |
|      - |  674 | ` * Return` |
|      - |  675 | ` *  TRUE on success or FALSE on failure.` |
|      - |  676 | ` */` |
|      6 |  677 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  678 | `{` |
|      - |  679 | `	const char *zPath,*zGroup;` |
|      - |  680 | `	ph7_vfs *pVfs;` |
|      - |  681 | `	int rc;` |
|      7 |  682 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  683 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  684 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  685 | `		return PH7_OK;` |
|      - |  686 | `	}` |
|      - |  687 | `	/* Point to the underlying vfs */` |
|      7 |  688 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  689 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - |  690 | `		/* IO routine not implemented,return NULL */` |
|      1 |  691 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  692 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  693 | `			ph7_function_name(pCtx)` |
|      - |  694 | `			);` |
|      1 |  695 | `		ph7_result_bool(pCtx,0);` |
|      1 |  696 | `		return PH7_OK;` |
|      - |  697 | `	}` |
|      - |  698 | `	/* Point to the desired directory */` |
|      6 |  699 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  700 | `	/* Extract the user */` |
|      6 |  701 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - |  702 | `	/* Perform the requested operation */` |
|      6 |  703 | `	errno = 0;` |
|      6 |  704 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      6 |  705 | `	if( rc != PH7_OK ){` |
|      - |  706 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  707 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  708 | `		 * "chown(): Operation not permitted". */` |
|      6 |  709 | `		if( rc == -2 ){` |
|      3 |  710 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",` |
|      1 |  711 | `				ph7_function_name(pCtx),zGroup);` |
|      1 |  712 | `		}else{` |
|      6 |  713 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  714 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  715 | `		}` |
|      3 |  716 | `	}` |
|      - |  717 | `	/* IO return value */` |
|      6 |  718 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  719 | `	return PH7_OK;` |
|      4 |  720 | `}` |
|      - |  721 | `/*` |
|      - |  722 | ` * int64 disk_free_space(string $directory)` |
|      - |  723 | ` *  Returns available space on filesystem or disk partition.` |
|      - |  724 | ` * Parameters` |
|      - |  725 | ` *  $directory` |
|      - |  726 | ` *   A directory of the filesystem or disk partition.` |
|      - |  727 | ` * Return` |
|      - |  728 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  729 | ` */` |
|      4 |  730 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  731 | `{` |
|      - |  732 | `	const char *zPath;` |
|      - |  733 | `	ph7_int64 iSize;` |
|      - |  734 | `	ph7_vfs *pVfs;` |
|      5 |  735 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  736 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  737 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  738 | `		return PH7_OK;` |
|      - |  739 | `	}` |
|      - |  740 | `	/* Point to the underlying vfs */` |
|      5 |  741 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  742 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - |  743 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  744 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  745 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  746 | `			ph7_function_name(pCtx)` |
|      - |  747 | `			);` |
|    ! 0 |  748 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  749 | `		return PH7_OK;` |
|      - |  750 | `	}` |
|      - |  751 | `	/* Point to the desired directory */` |
|      5 |  752 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  753 | `	/* Perform the requested operation */` |
|      5 |  754 | `	iSize = pVfs->xFreeSpace(zPath);` |
|      - |  755 | `	/* IO return value */` |
|      5 |  756 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  757 | `	return PH7_OK;` |
|      3 |  758 | `}` |
|      - |  759 | `/*` |
|      - |  760 | ` * int64 disk_total_space(string $directory)` |
|      - |  761 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - |  762 | ` * Parameters` |
|      - |  763 | ` *  $directory` |
|      - |  764 | ` *   A directory of the filesystem or disk partition.` |
|      - |  765 | ` * Return` |
|      - |  766 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  767 | ` */` |
|      4 |  768 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  769 | `{` |
|      - |  770 | `	const char *zPath;` |
|      - |  771 | `	ph7_int64 iSize;` |
|      - |  772 | `	ph7_vfs *pVfs;` |
|      5 |  773 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  774 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  775 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  776 | `		return PH7_OK;` |
|      - |  777 | `	}` |
|      - |  778 | `	/* Point to the underlying vfs */` |
|      5 |  779 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  780 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - |  781 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  782 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  783 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  784 | `			ph7_function_name(pCtx)` |
|      - |  785 | `			);` |
|    ! 0 |  786 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  787 | `		return PH7_OK;` |
|      - |  788 | `	}` |
|      - |  789 | `	/* Point to the desired directory */` |
|      5 |  790 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  791 | `	/* Perform the requested operation */` |
|      5 |  792 | `	iSize = pVfs->xTotalSpace(zPath);` |
|      - |  793 | `	/* IO return value */` |
|      5 |  794 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  795 | `	return PH7_OK;` |
|      3 |  796 | `}` |
|      - |  797 | `/*` |
|      - |  798 | ` * bool file_exists(string $filename)` |
|      - |  799 | ` *  Checks whether a file or directory exists.` |
|      - |  800 | ` * Parameters` |
|      - |  801 | ` *  $filename` |
|      - |  802 | ` *   Path to the file.` |
|      - |  803 | ` * Return` |
|      - |  804 | ` *  TRUE on success or FALSE on failure.` |
|      - |  805 | ` */` |
|    178 |  806 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  807 | `{` |
|      - |  808 | `	const char *zPath;` |
|      - |  809 | `	ph7_vfs *pVfs;` |
|      - |  810 | `	int rc;` |
|    181 |  811 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  812 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  813 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  814 | `		return PH7_OK;` |
|      - |  815 | `	}` |
|      - |  816 | `	/* Point to the underlying vfs */` |
|    181 |  817 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    181 |  818 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  819 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  820 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  821 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  822 | `			ph7_function_name(pCtx)` |
|      - |  823 | `			);` |
|    ! 0 |  824 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  825 | `		return PH7_OK;` |
|      - |  826 | `	}` |
|      - |  827 | `	/* Point to the desired directory */` |
|    181 |  828 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  829 | `	/* Perform the requested operation */` |
|    181 |  830 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  831 | `	/* IO return value */` |
|    181 |  832 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    181 |  833 | `	return PH7_OK;` |
|     92 |  834 | `}` |
|      - |  835 | `/*` |
|      - |  836 | ` * int64 file_size(string $filename)` |
|      - |  837 | ` *  Gets the size for the given file.` |
|      - |  838 | ` * Parameters` |
|      - |  839 | ` *  $filename` |
|      - |  840 | ` *   Path to the file.` |
|      - |  841 | ` * Return` |
|      - |  842 | ` *  File size on success or FALSE on failure.` |
|      - |  843 | ` */` |
|     26 |  844 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  845 | `{` |
|      - |  846 | `	const char *zPath;` |
|      - |  847 | `	ph7_int64 iSize;` |
|      - |  848 | `	ph7_vfs *pVfs;` |
|     27 |  849 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  850 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  851 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  852 | `		return PH7_OK;` |
|      - |  853 | `	}` |
|      - |  854 | `	/* Point to the underlying vfs */` |
|     27 |  855 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     27 |  856 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - |  857 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  858 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  859 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  860 | `			ph7_function_name(pCtx)` |
|      - |  861 | `			);` |
|    ! 0 |  862 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  863 | `		return PH7_OK;` |
|      - |  864 | `	}` |
|      - |  865 | `	/* Point to the desired directory */` |
|     27 |  866 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  867 | `	/* Perform the requested operation */` |
|     27 |  868 | `	iSize = pVfs->xFileSize(zPath);` |
|     27 |  869 | `	if( iSize < 0 ){` |
|      - |  870 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - |  871 | `		 * truthy and compares equal to nothing a caller would test for. */` |
|    ! 0 |  872 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): stat failed for %s",` |
|    ! 0 |  873 | `			ph7_function_name(pCtx),zPath);` |
|    ! 0 |  874 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  875 | `		return PH7_OK;` |
|      - |  876 | `	}` |
|      - |  877 | `	/* IO return value */` |
|     27 |  878 | `	ph7_result_int64(pCtx,iSize);` |
|     27 |  879 | `	return PH7_OK;` |
|     14 |  880 | `}` |
|      - |  881 | `/*` |
|      - |  882 | ` * int64 fileatime(string $filename)` |
|      - |  883 | ` *  Gets the last access time of the given file.` |
|      - |  884 | ` * Parameters` |
|      - |  885 | ` *  $filename` |
|      - |  886 | ` *   Path to the file.` |
|      - |  887 | ` * Return` |
|      - |  888 | ` *  File atime on success or FALSE on failure.` |
|      - |  889 | ` */` |
|      2 |  890 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  891 | `{` |
|      - |  892 | `	const char *zPath;` |
|      - |  893 | `	ph7_int64 iTime;` |
|      - |  894 | `	ph7_vfs *pVfs;` |
|      3 |  895 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  896 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  897 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  898 | `		return PH7_OK;` |
|      - |  899 | `	}` |
|      - |  900 | `	/* Point to the underlying vfs */` |
|      3 |  901 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  902 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - |  903 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  904 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  905 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  906 | `			ph7_function_name(pCtx)` |
|      - |  907 | `			);` |
|    ! 0 |  908 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  909 | `		return PH7_OK;` |
|      - |  910 | `	}` |
|      - |  911 | `	/* Point to the desired directory */` |
|      3 |  912 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  913 | `	/* Perform the requested operation */` |
|      3 |  914 | `	iTime = pVfs->xFileAtime(zPath);` |
|      - |  915 | `	/* IO return value */` |
|      3 |  916 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  917 | `	return PH7_OK;` |
|      2 |  918 | `}` |
|      - |  919 | `/*` |
|      - |  920 | ` * int64 filemtime(string $filename)` |
|      - |  921 | ` *  Gets file modification time.` |
|      - |  922 | ` * Parameters` |
|      - |  923 | ` *  $filename` |
|      - |  924 | ` *   Path to the file.` |
|      - |  925 | ` * Return` |
|      - |  926 | ` *  File mtime on success or FALSE on failure.` |
|      - |  927 | ` */` |
|      4 |  928 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  929 | `{` |
|      - |  930 | `	const char *zPath;` |
|      - |  931 | `	ph7_int64 iTime;` |
|      - |  932 | `	ph7_vfs *pVfs;` |
|      5 |  933 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  934 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  935 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  936 | `		return PH7_OK;` |
|      - |  937 | `	}` |
|      - |  938 | `	/* Point to the underlying vfs */` |
|      5 |  939 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  940 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - |  941 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  942 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  943 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  944 | `			ph7_function_name(pCtx)` |
|      - |  945 | `			);` |
|    ! 0 |  946 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  947 | `		return PH7_OK;` |
|      - |  948 | `	}` |
|      - |  949 | `	/* Point to the desired directory */` |
|      5 |  950 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  951 | `	/* Perform the requested operation */` |
|      5 |  952 | `	iTime = pVfs->xFileMtime(zPath);` |
|      - |  953 | `	/* IO return value */` |
|      5 |  954 | `	ph7_result_int64(pCtx,iTime);` |
|      5 |  955 | `	return PH7_OK;` |
|      3 |  956 | `}` |
|      - |  957 | `/*` |
|      - |  958 | ` * int64 filectime(string $filename)` |
|      - |  959 | ` *  Gets inode change time of file.` |
|      - |  960 | ` * Parameters` |
|      - |  961 | ` *  $filename` |
|      - |  962 | ` *   Path to the file.` |
|      - |  963 | ` * Return` |
|      - |  964 | ` *  File ctime on success or FALSE on failure.` |
|      - |  965 | ` */` |
|      2 |  966 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  967 | `{` |
|      - |  968 | `	const char *zPath;` |
|      - |  969 | `	ph7_int64 iTime;` |
|      - |  970 | `	ph7_vfs *pVfs;` |
|      3 |  971 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  972 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  973 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  974 | `		return PH7_OK;` |
|      - |  975 | `	}` |
|      - |  976 | `	/* Point to the underlying vfs */` |
|      3 |  977 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  978 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - |  979 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  980 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  981 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  982 | `			ph7_function_name(pCtx)` |
|      - |  983 | `			);` |
|    ! 0 |  984 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  985 | `		return PH7_OK;` |
|      - |  986 | `	}` |
|      - |  987 | `	/* Point to the desired directory */` |
|      3 |  988 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  989 | `	/* Perform the requested operation */` |
|      3 |  990 | `	iTime = pVfs->xFileCtime(zPath);` |
|      - |  991 | `	/* IO return value */` |
|      3 |  992 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  993 | `	return PH7_OK;` |
|      2 |  994 | `}` |
|      - |  995 | `/*` |
|      - |  996 | ` * bool is_file(string $filename)` |
|      - |  997 | ` *  Tells whether the filename is a regular file.` |
|      - |  998 | ` * Parameters` |
|      - |  999 | ` *  $filename` |
|      - | 1000 | ` *   Path to the file.` |
|      - | 1001 | ` * Return` |
|      - | 1002 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1003 | ` */` |
|   6732 | 1004 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1005 | `{` |
|      - | 1006 | `	const char *zPath;` |
|      - | 1007 | `	ph7_vfs *pVfs;` |
|      - | 1008 | `	int rc;` |
|   6737 | 1009 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1010 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1011 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1012 | `		return PH7_OK;` |
|      - | 1013 | `	}` |
|      - | 1014 | `	/* Point to the underlying vfs */` |
|   6737 | 1015 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6737 | 1016 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1017 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1018 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1019 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1020 | `			ph7_function_name(pCtx)` |
|      - | 1021 | `			);` |
|    ! 0 | 1022 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1023 | `		return PH7_OK;` |
|      - | 1024 | `	}` |
|      - | 1025 | `	/* Point to the desired directory */` |
|   6737 | 1026 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1027 | `	/* Perform the requested operation */` |
|   6737 | 1028 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1029 | `	/* IO return value */` |
|   6737 | 1030 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6737 | 1031 | `	return PH7_OK;` |
|   3371 | 1032 | `}` |
|      - | 1033 | `/*` |
|      - | 1034 | ` * bool is_link(string $filename)` |
|      - | 1035 | ` *  Tells whether the filename is a symbolic link.` |
|      - | 1036 | ` * Parameters` |
|      - | 1037 | ` *  $filename` |
|      - | 1038 | ` *   Path to the file.` |
|      - | 1039 | ` * Return` |
|      - | 1040 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1041 | ` */` |
|      4 | 1042 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1043 | `{` |
|      - | 1044 | `	const char *zPath;` |
|      - | 1045 | `	ph7_vfs *pVfs;` |
|      - | 1046 | `	int rc;` |
|      4 | 1047 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1048 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1049 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1050 | `		return PH7_OK;` |
|      - | 1051 | `	}` |
|      - | 1052 | `	/* Point to the underlying vfs */` |
|      4 | 1053 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 | 1054 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - | 1055 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1056 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1057 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1058 | `			ph7_function_name(pCtx)` |
|      - | 1059 | `			);` |
|    ! 0 | 1060 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1061 | `		return PH7_OK;` |
|      - | 1062 | `	}` |
|      - | 1063 | `	/* Point to the desired directory */` |
|      4 | 1064 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1065 | `	/* Perform the requested operation */` |
|      4 | 1066 | `	rc = pVfs->xIslink(zPath);` |
|      - | 1067 | `	/* IO return value */` |
|      4 | 1068 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 1069 | `	return PH7_OK;` |
|      2 | 1070 | `}` |
|      - | 1071 | `/*` |
|      - | 1072 | ` * bool is_readable(string $filename)` |
|      - | 1073 | ` *  Tells whether a file exists and is readable.` |
|      - | 1074 | ` * Parameters` |
|      - | 1075 | ` *  $filename` |
|      - | 1076 | ` *   Path to the file.` |
|      - | 1077 | ` * Return` |
|      - | 1078 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1079 | ` */` |
|      2 | 1080 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1081 | `{` |
|      - | 1082 | `	const char *zPath;` |
|      - | 1083 | `	ph7_vfs *pVfs;` |
|      - | 1084 | `	int rc;` |
|      3 | 1085 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1086 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1087 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1088 | `		return PH7_OK;` |
|      - | 1089 | `	}` |
|      - | 1090 | `	/* Point to the underlying vfs */` |
|      3 | 1091 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1092 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - | 1093 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1094 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1095 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1096 | `			ph7_function_name(pCtx)` |
|      - | 1097 | `			);` |
|    ! 0 | 1098 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1099 | `		return PH7_OK;` |
|      - | 1100 | `	}` |
|      - | 1101 | `	/* Point to the desired directory */` |
|      3 | 1102 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1103 | `	/* Perform the requested operation */` |
|      3 | 1104 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1105 | `	/* IO return value */` |
|      3 | 1106 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1107 | `	return PH7_OK;` |
|      2 | 1108 | `}` |
|      - | 1109 | `/*` |
|      - | 1110 | ` * bool is_writable(string $filename)` |
|      - | 1111 | ` *  Tells whether the filename is writable.` |
|      - | 1112 | ` * Parameters` |
|      - | 1113 | ` *  $filename` |
|      - | 1114 | ` *   Path to the file.` |
|      - | 1115 | ` * Return` |
|      - | 1116 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1117 | ` */` |
|      4 | 1118 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1119 | `{` |
|      - | 1120 | `	const char *zPath;` |
|      - | 1121 | `	ph7_vfs *pVfs;` |
|      - | 1122 | `	int rc;` |
|      5 | 1123 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1124 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1125 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1126 | `		return PH7_OK;` |
|      - | 1127 | `	}` |
|      - | 1128 | `	/* Point to the underlying vfs */` |
|      5 | 1129 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1130 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1131 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1132 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1133 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1134 | `			ph7_function_name(pCtx)` |
|      - | 1135 | `			);` |
|    ! 0 | 1136 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1137 | `		return PH7_OK;` |
|      - | 1138 | `	}` |
|      - | 1139 | `	/* Point to the desired directory */` |
|      5 | 1140 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1141 | `	/* Perform the requested operation */` |
|      5 | 1142 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1143 | `	/* IO return value */` |
|      5 | 1144 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1145 | `	return PH7_OK;` |
|      3 | 1146 | `}` |
|      - | 1147 | `/*` |
|      - | 1148 | ` * bool is_executable(string $filename)` |
|      - | 1149 | ` *  Tells whether the filename is executable.` |
|      - | 1150 | ` * Parameters` |
|      - | 1151 | ` *  $filename` |
|      - | 1152 | ` *   Path to the file.` |
|      - | 1153 | ` * Return` |
|      - | 1154 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1155 | ` */` |
|      2 | 1156 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1157 | `{` |
|      - | 1158 | `	const char *zPath;` |
|      - | 1159 | `	ph7_vfs *pVfs;` |
|      - | 1160 | `	int rc;` |
|      3 | 1161 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1162 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1163 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1164 | `		return PH7_OK;` |
|      - | 1165 | `	}` |
|      - | 1166 | `	/* Point to the underlying vfs */` |
|      3 | 1167 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1168 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1169 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1170 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1171 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1172 | `			ph7_function_name(pCtx)` |
|      - | 1173 | `			);` |
|    ! 0 | 1174 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1175 | `		return PH7_OK;` |
|      - | 1176 | `	}` |
|      - | 1177 | `	/* Point to the desired directory */` |
|      3 | 1178 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1179 | `	/* Perform the requested operation */` |
|      3 | 1180 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1181 | `	/* IO return value */` |
|      3 | 1182 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1183 | `	return PH7_OK;` |
|      2 | 1184 | `}` |
|      - | 1185 | `/*` |
|      - | 1186 | ` * string filetype(string $filename)` |
|      - | 1187 | ` *  Gets file type.` |
|      - | 1188 | ` * Parameters` |
|      - | 1189 | ` *  $filename` |
|      - | 1190 | ` *   Path to the file.` |
|      - | 1191 | ` * Return` |
|      - | 1192 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1193 | ` *  file, socket and unknown.` |
|      - | 1194 | ` */` |
|      4 | 1195 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1196 | `{` |
|      - | 1197 | `	const char *zPath;` |
|      - | 1198 | `	ph7_vfs *pVfs;` |
|      5 | 1199 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1200 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1201 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1202 | `		return PH7_OK;` |
|      - | 1203 | `	}` |
|      - | 1204 | `	/* Point to the underlying vfs */` |
|      5 | 1205 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1206 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1207 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1208 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1209 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1210 | `			ph7_function_name(pCtx)` |
|      - | 1211 | `			);` |
|    ! 0 | 1212 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1213 | `		return PH7_OK;` |
|      - | 1214 | `	}` |
|      - | 1215 | `	/* Point to the desired directory */` |
|      5 | 1216 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1217 | `	/* Set the empty string as the default return value */` |
|      5 | 1218 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1219 | `	/* Perform the requested operation */` |
|      5 | 1220 | `	pVfs->xFiletype(zPath,pCtx);` |
|      5 | 1221 | `	return PH7_OK;` |
|      3 | 1222 | `}` |
|      - | 1223 | `/*` |
|      - | 1224 | ` * array stat(string $filename)` |
|      - | 1225 | ` *  Gives information about a file.` |
|      - | 1226 | ` * Parameters` |
|      - | 1227 | ` *  $filename` |
|      - | 1228 | ` *   Path to the file.` |
|      - | 1229 | ` * Return` |
|      - | 1230 | ` *  An associative array on success holding the following entries on success` |
|      - | 1231 | ` *  0   dev     device number` |
|      - | 1232 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1233 | ` * 2    mode    inode protection mode` |
|      - | 1234 | ` * 3    nlink   number of links` |
|      - | 1235 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1236 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1237 | ` * 6    rdev    device type, if inode device` |
|      - | 1238 | ` * 7    size    size in bytes` |
|      - | 1239 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1240 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1241 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1242 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1243 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1244 | ` * Note:` |
|      - | 1245 | ` *  FALSE is returned on failure.` |
|      - | 1246 | ` */` |
|     10 | 1247 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1248 | `{` |
|      - | 1249 | `	ph7_value *pArray,*pValue;` |
|      - | 1250 | `	const char *zPath;` |
|      - | 1251 | `	ph7_vfs *pVfs;` |
|      - | 1252 | `	int rc;` |
|     11 | 1253 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1254 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1255 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1256 | `		return PH7_OK;` |
|      - | 1257 | `	}` |
|      - | 1258 | `	/* Point to the underlying vfs */` |
|     11 | 1259 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 1260 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1261 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1262 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1263 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1264 | `			ph7_function_name(pCtx)` |
|      - | 1265 | `			);` |
|    ! 0 | 1266 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1267 | `		return PH7_OK;` |
|      - | 1268 | `	}` |
|      - | 1269 | `	/* Create the array and the working value */` |
|     11 | 1270 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 1271 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 1272 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1273 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1274 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1275 | `		return PH7_OK;` |
|      - | 1276 | `	}` |
|      - | 1277 | `	/* Extract the file path */` |
|     11 | 1278 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1279 | `	/* Perform the requested operation */` |
|     11 | 1280 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     11 | 1281 | `	if( rc != PH7_OK ){` |
|      - | 1282 | `		/* IO error,return FALSE */` |
|    ! 0 | 1283 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1284 | `	}else{` |
|      - | 1285 | `		/* Return the associative array */` |
|     11 | 1286 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1287 | `	}` |
|      - | 1288 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1289 | `	 * automatically as soon we return from this function. */` |
|     11 | 1290 | `	return PH7_OK;` |
|      6 | 1291 | `}` |
|      - | 1292 | `/*` |
|      - | 1293 | ` * array lstat(string $filename)` |
|      - | 1294 | ` *  Gives information about a file or symbolic link.` |
|      - | 1295 | ` * Parameters` |
|      - | 1296 | ` *  $filename` |
|      - | 1297 | ` *   Path to the file.` |
|      - | 1298 | ` * Return` |
|      - | 1299 | ` *  An associative array on success holding the following entries on success` |
|      - | 1300 | ` *  0   dev     device number` |
|      - | 1301 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1302 | ` * 2    mode    inode protection mode` |
|      - | 1303 | ` * 3    nlink   number of links` |
|      - | 1304 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1305 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1306 | ` * 6    rdev    device type, if inode device` |
|      - | 1307 | ` * 7    size    size in bytes` |
|      - | 1308 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1309 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1310 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1311 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1312 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1313 | ` * Note:` |
|      - | 1314 | ` *  FALSE is returned on failure.` |
|      - | 1315 | ` */` |
|      2 | 1316 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1317 | `{` |
|      - | 1318 | `	ph7_value *pArray,*pValue;` |
|      - | 1319 | `	const char *zPath;` |
|      - | 1320 | `	ph7_vfs *pVfs;` |
|      - | 1321 | `	int rc;` |
|      3 | 1322 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1323 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1324 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1325 | `		return PH7_OK;` |
|      - | 1326 | `	}` |
|      - | 1327 | `	/* Point to the underlying vfs */` |
|      3 | 1328 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1329 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1330 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1331 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1332 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1333 | `			ph7_function_name(pCtx)` |
|      - | 1334 | `			);` |
|    ! 0 | 1335 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1336 | `		return PH7_OK;` |
|      - | 1337 | `	}` |
|      - | 1338 | `	/* Create the array and the working value */` |
|      3 | 1339 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 1340 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1341 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1342 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1343 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1344 | `		return PH7_OK;` |
|      - | 1345 | `	}` |
|      - | 1346 | `	/* Extract the file path */` |
|      3 | 1347 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1348 | `	/* Perform the requested operation */` |
|      3 | 1349 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      3 | 1350 | `	if( rc != PH7_OK ){` |
|      - | 1351 | `		/* IO error,return FALSE */` |
|    ! 0 | 1352 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1353 | `	}else{` |
|      - | 1354 | `		/* Return the associative array */` |
|      3 | 1355 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1356 | `	}` |
|      - | 1357 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1358 | `	 * automatically as soon we return from this function. */` |
|      3 | 1359 | `	return PH7_OK;` |
|      2 | 1360 | `}` |
|      - | 1361 | `/*` |
|      - | 1362 | ` * string getenv(string $varname)` |
|      - | 1363 | ` *  Gets the value of an environment variable.` |
|      - | 1364 | ` * Parameters` |
|      - | 1365 | ` *  $varname` |
|      - | 1366 | ` *   The variable name.` |
|      - | 1367 | ` * Return` |
|      - | 1368 | ` *  Returns the value of the environment variable varname, or FALSE if the environment` |
|      - | 1369 | ` * variable varname does not exist.` |
|      - | 1370 | ` */` |
|     52 | 1371 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1372 | `{` |
|      - | 1373 | `	const char *zEnv;` |
|      - | 1374 | `	ph7_vfs *pVfs;` |
|      - | 1375 | `	int iLen;` |
|     57 | 1376 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1377 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1378 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1379 | `		return PH7_OK;` |
|      - | 1380 | `	}` |
|      - | 1381 | `	/* Point to the underlying vfs */` |
|     57 | 1382 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     57 | 1383 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1384 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1385 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1386 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1387 | `			ph7_function_name(pCtx)` |
|      - | 1388 | `			);` |
|    ! 0 | 1389 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1390 | `		return PH7_OK;` |
|      - | 1391 | `	}` |
|      - | 1392 | `	/* Extract the environment variable */` |
|     57 | 1393 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1394 | `	/* Set a boolean FALSE as the default return value */` |
|     57 | 1395 | `	ph7_result_bool(pCtx,0);` |
|     57 | 1396 | `	if( iLen < 1 ){` |
|      - | 1397 | `		/* Empty string */` |
|    ! 0 | 1398 | `		return PH7_OK;` |
|      - | 1399 | `	}` |
|      - | 1400 | `	/* Perform the requested operation */` |
|     57 | 1401 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     57 | 1402 | `	return PH7_OK;` |
|     31 | 1403 | `}` |
|      - | 1404 | `/*` |
|      - | 1405 | ` * bool putenv(string $settings)` |
|      - | 1406 | ` *  Set the value of an environment variable.` |
|      - | 1407 | ` * Parameters` |
|      - | 1408 | ` *  $setting` |
|      - | 1409 | ` *   The setting, like "FOO=BAR"` |
|      - | 1410 | ` * Return` |
|      - | 1411 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1412 | ` */` |
|      6 | 1413 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1414 | `{` |
|      - | 1415 | `	const char *zName,*zValue;` |
|      - | 1416 | `	char *zSettings,*zEnd;` |
|      - | 1417 | `	ph7_vfs *pVfs;` |
|      - | 1418 | `	int iLen,rc;` |
|      7 | 1419 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1420 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1421 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1422 | `		return PH7_OK;` |
|      - | 1423 | `	}` |
|      - | 1424 | `	/* Extract the setting variable */` |
|      7 | 1425 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|      7 | 1426 | `	if( iLen < 1 ){` |
|      - | 1427 | `		/* Empty string,return FALSE */` |
|    ! 0 | 1428 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1429 | `		return PH7_OK;` |
|      - | 1430 | `	}` |
|      - | 1431 | `	/* Parse the setting */` |
|      7 | 1432 | `	zEnd = &zSettings[iLen];` |
|      7 | 1433 | `	zValue = 0;` |
|      7 | 1434 | `	zName = zSettings;` |
|    127 | 1435 | `	while( zSettings < zEnd ){` |
|    127 | 1436 | `		if( zSettings[0] == '=' ){` |
|      - | 1437 | `			/* Null terminate the name */` |
|      7 | 1438 | `			zSettings[0] = 0;` |
|      7 | 1439 | `			zValue = &zSettings[1];` |
|      7 | 1440 | `			break;` |
|      - | 1441 | `		}` |
|    121 | 1442 | `		zSettings++;` |
|      1 | 1443 | `	}` |
|      - | 1444 | `	/* Install the environment variable in the $_Env array */` |
|      7 | 1445 | `	if( zValue == 0 \|\| zName[0] == 0 \|\| zValue >= zEnd \|\| zName >= zValue ){` |
|      - | 1446 | `		/* Invalid settings,retun FALSE */` |
|      5 | 1447 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1448 | `		if( zSettings  < zEnd ){` |
|      5 | 1449 | `			zSettings[0] = '=';` |
|      2 | 1450 | `		}` |
|      5 | 1451 | `		return PH7_OK;` |
|      - | 1452 | `	}` |
|      3 | 1453 | `	ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_ENV_ATTR,zName,zValue,(int)(zEnd-zValue));` |
|      - | 1454 | `	/* Point to the underlying vfs */` |
|      3 | 1455 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1456 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 1457 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1458 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1459 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1460 | `			ph7_function_name(pCtx)` |
|      - | 1461 | `			);` |
|    ! 0 | 1462 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1463 | `		zSettings[0] = '=';` |
|    ! 0 | 1464 | `		return PH7_OK;` |
|      - | 1465 | `	}` |
|      - | 1466 | `	/* Perform the requested operation */` |
|      3 | 1467 | `	rc = pVfs->xSetenv(zName,zValue);` |
|      3 | 1468 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 1469 | `	zSettings[0] = '=';` |
|      3 | 1470 | `	return PH7_OK;` |
|      4 | 1471 | `}` |
|      - | 1472 | `/*` |
|      - | 1473 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 1474 | ` *  Sets access and modification time of file.` |
|      - | 1475 | ` * Note: On windows` |
|      - | 1476 | ` *   If the file does not exists,it will not be created.` |
|      - | 1477 | ` * Parameters` |
|      - | 1478 | ` *  $filename` |
|      - | 1479 | ` *   The name of the file being touched.` |
|      - | 1480 | ` *  $time` |
|      - | 1481 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 1482 | ` * $atime` |
|      - | 1483 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 1484 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 1485 | ` *   present, the current system time is used.` |
|      - | 1486 | ` * Return` |
|      - | 1487 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1488 | `*/` |
|      4 | 1489 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1490 | `{` |
|      - | 1491 | `	ph7_int64 nTime,nAccess;` |
|      - | 1492 | `	const char *zFile;` |
|      - | 1493 | `	ph7_vfs *pVfs;` |
|      - | 1494 | `	int rc;` |
|      5 | 1495 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1496 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1497 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1498 | `		return PH7_OK;` |
|      - | 1499 | `	}` |
|      - | 1500 | `	/* Point to the underlying vfs */` |
|      5 | 1501 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1502 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 1503 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1504 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1505 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1506 | `			ph7_function_name(pCtx)` |
|      - | 1507 | `			);` |
|    ! 0 | 1508 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1509 | `		return PH7_OK;` |
|      - | 1510 | `	}` |
|      - | 1511 | `	/* Perform the requested operation */` |
|      5 | 1512 | `	nTime = nAccess = -1;` |
|      5 | 1513 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|      5 | 1514 | `	if( nArg > 1 ){` |
|      2 | 1515 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      2 | 1516 | `		if( nArg > 2 ){` |
|      2 | 1517 | `			nAccess = ph7_value_to_int64(apArg[1]);` |
|      1 | 1518 | `		}else{` |
|    ! 0 | 1519 | `			nAccess = nTime;` |
|      - | 1520 | `		}` |
|      1 | 1521 | `	}` |
|      5 | 1522 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 1523 | `	/* IO result */` |
|      5 | 1524 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1525 | `	return PH7_OK;` |
|      3 | 1526 | `}` |
|      - | 1527 | `/*` |
|      - | 1528 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 1529 | ` * Status:` |
|      - | 1530 | ` *    Stable.` |
|      - | 1531 | ` */` |
|      - | 1532 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1533 | `/*` |
|      - | 1534 | ` * string dirname(string $path)` |
|      - | 1535 |  |
|      - | 1536 | ` *  Returns parent directory's path.` |
|      - | 1537 | ` * Parameters` |
|      - | 1538 | ` * $path` |
|      - | 1539 | ` *  Target path.` |
|      - | 1540 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1541 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1542 | ` * Return` |
|      - | 1543 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 1544 | ` *  is returned, indicating the current directory.` |
|      - | 1545 | ` */` |
|     20 | 1546 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1547 | `{` |
|      - | 1548 | `	const char *zPath,*zDir;` |
|      - | 1549 | `	int iLen,iDirlen;` |
|     25 | 1550 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1551 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 1552 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1553 | `		return PH7_OK;` |
|      - | 1554 | `	}` |
|      - | 1555 | `	/* Point to the target path */` |
|     25 | 1556 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     25 | 1557 | `	if( iLen < 1 ){` |
|      - | 1558 | `		/* Reuturn "." */` |
|      2 | 1559 | `		ph7_result_string(pCtx,".",sizeof(char));` |
|      2 | 1560 | `		return PH7_OK;` |
|      - | 1561 | `	}` |
|      - | 1562 | `	/* Perform the requested operation */` |
|     23 | 1563 | `	zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1564 | `	/* Return directory name */` |
|     23 | 1565 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     23 | 1566 | `	return PH7_OK;` |
|     15 | 1567 | `}` |
|      - | 1568 | `/*` |
|      - | 1569 | ` * string basename(string $path[, string $suffix ])` |
|      - | 1570 | ` *  Returns trailing name component of path.` |
|      - | 1571 | ` * Parameters` |
|      - | 1572 | ` * $path` |
|      - | 1573 | ` *  Target path.` |
|      - | 1574 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1575 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1576 | ` * $suffix` |
|      - | 1577 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 1578 | ` * Return` |
|      - | 1579 | ` *  The base name of the given path.` |
|      - | 1580 | ` */` |
|     46 | 1581 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1582 | `{` |
|      - | 1583 | `	const char *zPath,*zBase,*zEnd;` |
|      - | 1584 | `	int c,d,iLen;` |
|     47 | 1585 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1586 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1587 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1588 | `		return PH7_OK;` |
|      - | 1589 | `	}` |
|     47 | 1590 | `	c = d = '/';` |
|      - | 1591 | `#ifdef __WINNT__` |
|      1 | 1592 | `	d = '\\';` |
|      - | 1593 | `#endif` |
|      - | 1594 | `	/* Point to the target path */` |
|     47 | 1595 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     47 | 1596 | `	if( iLen < 1 ){` |
|      - | 1597 | `		/* Empty string */` |
|      3 | 1598 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1599 | `		return PH7_OK;` |
|      - | 1600 | `	}` |
|      - | 1601 | `	/* Perform the requested operation */` |
|     45 | 1602 | `	zEnd = &zPath[iLen - 1];` |
|      - | 1603 | `	/* Ignore trailing '/' */` |
|     71 | 1604 | `	while( zEnd > zPath && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      5 | 1605 | `		zEnd--;` |
|      1 | 1606 | `	}` |
|     45 | 1607 | `	if( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ){` |
|      - | 1608 | `		/* Nothing but separators ("/", "///"): php answers the EMPTY string, where the` |
|      - | 1609 | `		 * strip loop above stops one short and left PH7 returning "/". */` |
|      5 | 1610 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1611 | `		return PH7_OK;` |
|      - | 1612 | `	}` |
|     41 | 1613 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    975 | 1614 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    915 | 1615 | `		zEnd--;` |
|      1 | 1616 | `	}` |
|     41 | 1617 | `	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     41 | 1618 | `	zEnd = &zPath[iLen];` |
|     41 | 1619 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1620 | `		const char *zSuffix;` |
|      - | 1621 | `		int nSuffix;` |
|      - | 1622 | `		/* Strip suffix */` |
|      5 | 1623 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      5 | 1624 | `		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1625 | `			zEnd -= nSuffix;` |
|      2 | 1626 | `		}` |
|      2 | 1627 | `	}` |
|      - | 1628 | `	/* Store the basename */` |
|     41 | 1629 | `	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));` |
|     41 | 1630 | `	return PH7_OK;` |
|     24 | 1631 | `}` |
|      - | 1632 | `/*` |
|      - | 1633 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1634 | ` *  Returns information about a file path.` |
|      - | 1635 | ` * Parameter` |
|      - | 1636 | ` *  $path` |
|      - | 1637 | ` *   The path to be parsed.` |
|      - | 1638 | ` *  $options` |
|      - | 1639 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 1640 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 1641 | ` * Return` |
|      - | 1642 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 1643 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 1644 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 1645 | ` */` |
|      - | 1646 | `typedef struct path_info path_info;` |
|      - | 1647 | `struct path_info` |
|      - | 1648 | `{` |
|      - | 1649 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1650 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1651 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1652 | `	SyString sFilename;  /* Filename */` |
|      - | 1653 | `};` |
|      - | 1654 | `/*` |
|      - | 1655 | ` * Extract path fields.` |
|      - | 1656 | ` */` |
|  13342 | 1657 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1658 | `{` |
|  13347 | 1659 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1660 | `	SyString *pCur;` |
|      - | 1661 | `	int c,d;` |
|  13347 | 1662 | `	c = d = '/';` |
|      - | 1663 | `#ifdef __WINNT__` |
|      5 | 1664 | `	d = '\\';` |
|      - | 1665 | `#endif` |
|      - | 1666 | `	/* Zero the structure */` |
|  13347 | 1667 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1668 | `	/* Handle special case */` |
|  13347 | 1669 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1670 | `#ifdef __WINNT__` |
|    ! 0 | 1671 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1672 | `#else` |
|    ! 0 | 1673 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1674 | `#endif` |
|    ! 0 | 1675 | `		return SXRET_OK;` |
|      - | 1676 | `	}` |
|      - | 1677 | `	/* Extract the basename */` |
| 360074 | 1678 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 340061 | 1679 | `		zEnd--;` |
|      5 | 1680 | `	}` |
|  13347 | 1681 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13347 | 1682 | `	zEnd = &zPath[nByte];` |
|      - | 1683 | `	/* dirname */` |
|  13347 | 1684 | `	pCur = &pOut->sDir;` |
|  13347 | 1685 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13347 | 1686 | `	if( pCur->nByte > 1 ){` |
|  26689 | 1687 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1688 | `#ifdef __WINNT__` |
|      5 | 1689 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1690 | `#endif` |
|   6676 | 1691 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1692 | `#ifdef __WINNT__` |
|    ! 0 | 1693 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1694 | `#else` |
|    ! 0 | 1695 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1696 | `#endif` |
|    ! 0 | 1697 | `	}` |
|      - | 1698 | `	/* basename/filename */` |
|  13347 | 1699 | `	pCur = &pOut->sBasename;` |
|  13347 | 1700 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13347 | 1701 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1702 | `#ifdef __WINNT__` |
|      5 | 1703 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1704 | `#endif` |
|  13347 | 1705 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13347 | 1706 | `	if( pCur->nByte > 0 ){` |
|      - | 1707 | `		/* extension */` |
|  13347 | 1708 | `		zEnd--;` |
|  66709 | 1709 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  53367 | 1710 | `			zEnd--;` |
|      5 | 1711 | `		}` |
|  13347 | 1712 | `		if( zEnd > pCur->zString ){` |
|  13345 | 1713 | `			zEnd++; /* Jump leading dot */` |
|  13345 | 1714 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1715 | `			/* Fix filename */` |
|  13345 | 1716 | `			pCur = &pOut->sFilename;` |
|  13345 | 1717 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13345 | 1718 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6670 | 1719 | `			}` |
|   6670 | 1720 | `		}` |
|   6671 | 1721 | `	}` |
|  13347 | 1722 | `	return SXRET_OK;` |
|   6676 | 1723 | `}` |
|      - | 1724 | `/*` |
|      - | 1725 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1726 | ` *  See block comment above.` |
|      - | 1727 | ` */` |
|  13342 | 1728 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1729 | `{` |
|      - | 1730 | `	const char *zPath;` |
|      - | 1731 | `	path_info sInfo;` |
|      - | 1732 | `	SyString *pComp;` |
|      - | 1733 | `	int iLen;` |
|  13347 | 1734 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1735 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1736 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1737 | `		return PH7_OK;` |
|      - | 1738 | `	}` |
|      - | 1739 | `	/* Point to the target path */` |
|  13347 | 1740 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13347 | 1741 | `	if( iLen < 1 ){` |
|      - | 1742 | `		/* Empty string */` |
|    ! 0 | 1743 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1744 | `		return PH7_OK;` |
|      - | 1745 | `	}` |
|      - | 1746 | `	/* Extract path info */` |
|  13347 | 1747 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  20017 | 1748 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1749 | `		/* Return path component */` |
|  13345 | 1750 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13345 | 1751 | `		switch(nComp){` |
|      1 | 1752 | `		case 1: /* PATHINFO_DIRNAME */` |
|      3 | 1753 | `			pComp = &sInfo.sDir;` |
|      3 | 1754 | `			if( pComp->nByte > 0 ){` |
|      3 | 1755 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1756 | `			}else{` |
|      - | 1757 | `				/* Expand the empty string */` |
|    ! 0 | 1758 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1759 | `			}` |
|      3 | 1760 | `			break;` |
|      1 | 1761 | `		case 2: /*PATHINFO_BASENAME*/` |
|      3 | 1762 | `			pComp = &sInfo.sBasename;` |
|      3 | 1763 | `			if( pComp->nByte > 0 ){` |
|      3 | 1764 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1765 | `			}else{` |
|      - | 1766 | `				/* Expand the empty string */` |
|    ! 0 | 1767 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1768 | `			}` |
|      3 | 1769 | `			break;` |
|   3336 | 1770 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6677 | 1771 | `			pComp = &sInfo.sExtension;` |
|   6677 | 1772 | `			if( pComp->nByte > 0 ){` |
|   6675 | 1773 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3340 | 1774 | `			}else{` |
|      - | 1775 | `				/* Expand the empty string */` |
|      3 | 1776 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1777 | `			}` |
|   6677 | 1778 | `			break;` |
|   3332 | 1779 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6669 | 1780 | `			pComp = &sInfo.sFilename;` |
|   6669 | 1781 | `			if( pComp->nByte > 0 ){` |
|   6669 | 1782 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3337 | 1783 | `			}else{` |
|      - | 1784 | `				/* Expand the empty string */` |
|    ! 0 | 1785 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1786 | `			}` |
|   6669 | 1787 | `			break;` |
|    ! 0 | 1788 | `		default:` |
|      - | 1789 | `			/* Expand the empty string */` |
|    ! 0 | 1790 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1791 | `			break;` |
|      - | 1792 | `		}` |
|   6675 | 1793 | `	}else{` |
|      - | 1794 | `		/* Return an associative array */` |
|      - | 1795 | `		ph7_value *pArray,*pValue;` |
|      3 | 1796 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 1797 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1798 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1799 | `			/* Out of mem,return NULL */` |
|    ! 0 | 1800 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1801 | `			return PH7_OK;` |
|      - | 1802 | `		}` |
|      - | 1803 | `		/* dirname */` |
|      3 | 1804 | `		pComp = &sInfo.sDir;` |
|      3 | 1805 | `		if( pComp->nByte > 0 ){` |
|      3 | 1806 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1807 | `			/* Perform the insertion */` |
|      3 | 1808 | `			ph7_array_add_strkey_elem(pArray,"dirname",pValue); /* Will make it's own copy */` |
|      1 | 1809 | `		}` |
|      - | 1810 | `		/* Reset the string cursor */` |
|      3 | 1811 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1812 | `		/* basername */` |
|      3 | 1813 | `		pComp = &sInfo.sBasename;` |
|      3 | 1814 | `		if( pComp->nByte > 0 ){` |
|      3 | 1815 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1816 | `			/* Perform the insertion */` |
|      3 | 1817 | `			ph7_array_add_strkey_elem(pArray,"basename",pValue); /* Will make it's own copy */` |
|      1 | 1818 | `		}` |
|      - | 1819 | `		/* Reset the string cursor */` |
|      3 | 1820 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1821 | `		/* extension */` |
|      3 | 1822 | `		pComp = &sInfo.sExtension;` |
|      3 | 1823 | `		if( pComp->nByte > 0 ){` |
|      3 | 1824 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1825 | `			/* Perform the insertion */` |
|      3 | 1826 | `			ph7_array_add_strkey_elem(pArray,"extension",pValue); /* Will make it's own copy */` |
|      1 | 1827 | `		}` |
|      - | 1828 | `		/* Reset the string cursor */` |
|      3 | 1829 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1830 | `		/* filename */` |
|      3 | 1831 | `		pComp = &sInfo.sFilename;` |
|      3 | 1832 | `		if( pComp->nByte > 0 ){` |
|      3 | 1833 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1834 | `			/* Perform the insertion */` |
|      3 | 1835 | `			ph7_array_add_strkey_elem(pArray,"filename",pValue); /* Will make it's own copy */` |
|      1 | 1836 | `		}` |
|      - | 1837 | `		/* Return the created array */` |
|      3 | 1838 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1839 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 1840 | `		 * automatically as soon we return from this foreign function.` |
|      - | 1841 | `		 */` |
|      - | 1842 | `	}` |
|  13347 | 1843 | `	return PH7_OK;` |
|   6676 | 1844 | `}` |
|      - | 1845 | `/* SPDX-SnippetBegin */` |
|      - | 1846 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 1847 | `/* SPDX-License-Identifier: blessing */` |
|      - | 1848 | `/*` |
|      - | 1849 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 1850 |  |
|      - | 1851 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 1852 | ` * Status: Public Domain` |
|      - | 1853 | ` */` |
|      - | 1854 | `typedef unsigned char u8;` |
|      - | 1855 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 1856 | `** lower-case character.` |
|      - | 1857 | `**` |
|      - | 1858 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 1859 | `** handle case conversions for the UTF character set since the tables` |
|      - | 1860 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 1861 | `*/` |
|      - | 1862 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 1863 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 1864 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 1865 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 1866 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 1867 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 1868 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 1869 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 1870 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 1871 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 1872 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 1873 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 1874 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 1875 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 1876 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 1877 | `    252,253,254,255` |
|      - | 1878 | `};` |
|      - | 1879 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 1880 | `/*` |
|      - | 1881 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 1882 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 1883 | `*/` |
|      - | 1884 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 1885 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 1886 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 1887 | `  }                                                    \` |
|      - | 1888 | `}` |
|      - | 1889 | `/*` |
|      - | 1890 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 1891 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 1892 | `** are the same and false (0) if they are different.` |
|      - | 1893 | `**` |
|      - | 1894 | `** Globbing rules:` |
|      - | 1895 | `**` |
|      - | 1896 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 1897 | `**` |
|      - | 1898 | `**      '?'       Matches exactly one character.` |
|      - | 1899 | `**` |
|      - | 1900 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 1901 | `**                characters.` |
|      - | 1902 | `**` |
|      - | 1903 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 1904 | `**` |
|      - | 1905 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 1906 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 1907 | `** range of characters can be specified using '-'.  Example:` |
|      - | 1908 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 1909 | `** it the last character in the list.` |
|      - | 1910 | `**` |
|      - | 1911 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 1912 | `**` |
|      - | 1913 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 1914 | `**` |
|      - | 1915 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 1916 | `*/` |
|     44 | 1917 | `static int patternCompare(` |
|      - | 1918 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1919 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1920 | `  const int esc,                    /* The escape character */` |
|      - | 1921 | `  int noCase` |
|      1 | 1922 | `){` |
|      - | 1923 | `  int c, c2;` |
|      - | 1924 | `  int invert;` |
|      - | 1925 | `  int seen;` |
|     45 | 1926 | `  u8 matchOne = '?';` |
|     45 | 1927 | `  u8 matchAll = '*';` |
|     45 | 1928 | `  u8 matchSet = '[';` |
|     45 | 1929 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1930 |  |
|     45 | 1931 | `  if( !zPattern \|\| !zString ) return 0;` |
|     81 | 1932 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     73 | 1933 | `    if( !prevEscape && c==matchAll ){` |
|     52 | 1934 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|     27 | 1935 | `               \|\| c == matchOne ){` |
|    ! 0 | 1936 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1937 | `          return 0;` |
|      - | 1938 | `        }` |
|    ! 0 | 1939 | `      }` |
|     27 | 1940 | `      if( c==0 ){` |
|     19 | 1941 | `        return 1;` |
|      9 | 1942 | `      }else if( c==esc ){` |
|    ! 0 | 1943 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1944 | `        if( c==0 ){` |
|    ! 0 | 1945 | `          return 0;` |
|    ! 0 | 1946 | `        }` |
|      9 | 1947 | `      }else if( c==matchSet ){` |
|    ! 0 | 1948 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 1949 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 1950 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 1951 | `        }` |
|    ! 0 | 1952 | `        return *zString!=0;` |
|      - | 1953 | `      }` |
|     11 | 1954 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     11 | 1955 | `        if( noCase ){` |
|      3 | 1956 | `          GlogUpperToLower(c2);` |
|      3 | 1957 | `          GlogUpperToLower(c);` |
|     11 | 1958 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 1959 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 1960 | `            GlogUpperToLower(c2);` |
|      1 | 1961 | `          }` |
|      2 | 1962 | `        }else{` |
|     47 | 1963 | `          while( c2 != 0 && c2 != c ){` |
|     39 | 1964 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 1965 | `          }` |
|      - | 1966 | `        }` |
|     11 | 1967 | `        if( c2==0 ) return 0;` |
|      9 | 1968 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 1969 | `      }` |
|    ! 0 | 1970 | `      return 0;` |
|     47 | 1971 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 1972 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1973 | `        return 0;` |
|    ! 0 | 1974 | `      }` |
|     47 | 1975 | `    }else if( c==matchSet ){` |
|    ! 0 | 1976 | `      int prior_c = 0;` |
|    ! 0 | 1977 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 1978 | `      seen = 0;` |
|    ! 0 | 1979 | `      invert = 0;` |
|    ! 0 | 1980 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 1981 | `      if( c==0 ) return 0;` |
|    ! 0 | 1982 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1983 | `      if( c2=='^' ){` |
|    ! 0 | 1984 | `        invert = 1;` |
|    ! 0 | 1985 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1986 | `      }` |
|    ! 0 | 1987 | `      if( c2==']' ){` |
|    ! 0 | 1988 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 1989 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1990 | `      }` |
|    ! 0 | 1991 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 1992 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 1993 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1994 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 1995 | `          prior_c = 0;` |
|    ! 0 | 1996 | `        }else{` |
|    ! 0 | 1997 | `          if( c==c2 ){` |
|    ! 0 | 1998 | `            seen = 1;` |
|    ! 0 | 1999 | `          }` |
|    ! 0 | 2000 | `          prior_c = c2;` |
|      - | 2001 | `        }` |
|    ! 0 | 2002 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2003 | `      }` |
|    ! 0 | 2004 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 2005 | `        return 0;` |
|    ! 0 | 2006 | `      }` |
|     47 | 2007 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 2008 | `      prevEscape = 1;` |
|    ! 0 | 2009 | `    }else{` |
|     47 | 2010 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     47 | 2011 | `      if( noCase ){` |
|      7 | 2012 | `        GlogUpperToLower(c);` |
|      7 | 2013 | `        GlogUpperToLower(c2);` |
|      3 | 2014 | `      }` |
|     47 | 2015 | `      if( c!=c2 ){` |
|     11 | 2016 | `        return 0;` |
|      - | 2017 | `      }` |
|     37 | 2018 | `      prevEscape = 0;` |
|      - | 2019 | `    }` |
|      1 | 2020 | `  }` |
|      9 | 2021 | `  return *zString==0;` |
|     23 | 2022 | `}` |
|      - | 2023 | `/* SPDX-SnippetEnd */` |
|      - | 2024 | `/*` |
|      - | 2025 | ` * Wrapper around patternCompare() defined above.` |
|      - | 2026 | ` * See block comment above for more information.` |
|      - | 2027 | ` */` |
|     36 | 2028 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 2029 | `{` |
|      - | 2030 | `	int rc;` |
|     37 | 2031 | `	if( iEsc < 0 ){` |
|    ! 0 | 2032 | `		iEsc = '\\';` |
|    ! 0 | 2033 | `	}` |
|     37 | 2034 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     37 | 2035 | `	return rc;` |
|      1 | 2036 | `}` |
|      - | 2037 | `/*` |
|      - | 2038 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 2039 | ` *  Match filename against a pattern.` |
|      - | 2040 | ` * Parameters` |
|      - | 2041 | ` *  $pattern` |
|      - | 2042 | ` *   The shell wildcard pattern.` |
|      - | 2043 | ` * $string` |
|      - | 2044 | ` *  The tested string.` |
|      - | 2045 | ` * $flags` |
|      - | 2046 | ` *   A list of possible flags:` |
|      - | 2047 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 2048 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 2049 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 2050 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 2051 | ` * Return` |
|      - | 2052 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2053 | ` */` |
|      8 | 2054 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2055 | `{` |
|      - | 2056 | `	const char *zString,*zPattern;` |
|      9 | 2057 | `	int iEsc = '\\';` |
|      9 | 2058 | `	int noCase = 0;` |
|      - | 2059 | `	int rc;` |
|      9 | 2060 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2061 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2062 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2063 | `		return PH7_OK;` |
|      - | 2064 | `	}` |
|      - | 2065 | `	/* Extract the pattern and the string */` |
|      9 | 2066 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 2067 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2068 | `	/* Extract the flags if avaialble */` |
|      9 | 2069 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 2070 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 2071 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|    ! 0 | 2072 | `			iEsc = 0;` |
|    ! 0 | 2073 | `		}` |
|      7 | 2074 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
|      3 | 2075 | `			noCase = 1;` |
|      1 | 2076 | `		}` |
|      3 | 2077 | `	}` |
|      - | 2078 | `	/* Go globbing */` |
|      9 | 2079 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 2080 | `	/* Globbing result */` |
|      9 | 2081 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 2082 | `	return PH7_OK;` |
|      5 | 2083 | `}` |
|      - | 2084 | `/*` |
|      - | 2085 | ` * bool strglob(string $pattern,string $string)` |
|      - | 2086 | ` *  Match string against a pattern.` |
|      - | 2087 | ` * Parameters` |
|      - | 2088 | ` *  $pattern` |
|      - | 2089 | ` *   The shell wildcard pattern.` |
|      - | 2090 | ` * $string` |
|      - | 2091 | ` *  The tested string.` |
|      - | 2092 | ` * Return` |
|      - | 2093 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2094 | ` * Note that this a symisc eXtension.` |
|      - | 2095 | ` */` |
|     28 | 2096 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2097 | `{` |
|      - | 2098 | `	const char *zString,*zPattern;` |
|     29 | 2099 | `	int iEsc = '\\';` |
|      - | 2100 | `	int rc;` |
|     29 | 2101 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2102 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2103 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2104 | `		return PH7_OK;` |
|      - | 2105 | `	}` |
|      - | 2106 | `	/* Extract the pattern and the string */` |
|     29 | 2107 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|     29 | 2108 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2109 | `	/* Go globbing */` |
|     29 | 2110 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2111 | `	/* Globbing result */` |
|     29 | 2112 | `	ph7_result_bool(pCtx,rc);` |
|     29 | 2113 | `	return PH7_OK;` |
|     15 | 2114 | `}` |
|      - | 2115 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2116 | `/*` |
|      - | 2117 | ` * bool link(string $target,string $link)` |
|      - | 2118 |  |
|      - | 2119 | ` *  Create a hard link.` |
|      - | 2120 | ` * Parameters` |
|      - | 2121 | ` *  $target` |
|      - | 2122 | ` *   Target of the link.` |
|      - | 2123 | ` *  $link` |
|      - | 2124 | ` *   The link name.` |
|      - | 2125 | ` * Return` |
|      - | 2126 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2127 | ` */` |
|      2 | 2128 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2129 | `{` |
|      - | 2130 | `	const char *zTarget,*zLink;` |
|      - | 2131 | `	ph7_vfs *pVfs;` |
|      - | 2132 | `	int rc;` |
|      3 | 2133 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2134 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2135 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2136 | `		return PH7_OK;` |
|      - | 2137 | `	}` |
|      - | 2138 | `	/* Point to the underlying vfs */` |
|      3 | 2139 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2140 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2141 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2142 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2143 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2144 | `			ph7_function_name(pCtx)` |
|      - | 2145 | `			);` |
|    ! 0 | 2146 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2147 | `		return PH7_OK;` |
|      - | 2148 | `	}` |
|      - | 2149 | `	/* Extract the given arguments */` |
|      3 | 2150 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      3 | 2151 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2152 | `	/* Perform the requested operation */` |
|      3 | 2153 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2154 | `	/* IO result */` |
|      3 | 2155 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 2156 | `	return PH7_OK;` |
|      2 | 2157 | `}` |
|      - | 2158 | `/*` |
|      - | 2159 | ` * bool symlink(string $target,string $link)` |
|      - | 2160 | ` *  Creates a symbolic link.` |
|      - | 2161 | ` * Parameters` |
|      - | 2162 | ` *  $target` |
|      - | 2163 | ` *   Target of the link.` |
|      - | 2164 | ` *  $link` |
|      - | 2165 | ` *   The link name.` |
|      - | 2166 | ` * Return` |
|      - | 2167 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2168 | ` */` |
|      6 | 2169 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2170 | `{` |
|      - | 2171 | `	const char *zTarget,*zLink;` |
|      - | 2172 | `	ph7_vfs *pVfs;` |
|      - | 2173 | `	int rc;` |
|      7 | 2174 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2175 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2176 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2177 | `		return PH7_OK;` |
|      - | 2178 | `	}` |
|      - | 2179 | `	/* Point to the underlying vfs */` |
|      7 | 2180 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 2181 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2182 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2183 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2184 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2185 | `			ph7_function_name(pCtx)` |
|      - | 2186 | `			);` |
|    ! 0 | 2187 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2188 | `		return PH7_OK;` |
|      - | 2189 | `	}` |
|      - | 2190 | `	/* Extract the given arguments */` |
|      7 | 2191 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      7 | 2192 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2193 | `	/* Perform the requested operation */` |
|      7 | 2194 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2195 | `	/* IO result */` |
|      7 | 2196 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      7 | 2197 | `	return PH7_OK;` |
|      4 | 2198 | `}` |
|      - | 2199 | `/*` |
|      - | 2200 | ` * int umask([ int $mask ])` |
|      - | 2201 | ` *  Changes the current umask.` |
|      - | 2202 | ` * Parameters` |
|      - | 2203 | ` *  $mask` |
|      - | 2204 | ` *   The new umask.` |
|      - | 2205 | ` * Return` |
|      - | 2206 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2207 | ` *  Otherwise the old umask is returned.` |
|      - | 2208 | ` */` |
|      8 | 2209 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2210 | `{` |
|      - | 2211 | `	int iOld,iNew;` |
|      - | 2212 | `	ph7_vfs *pVfs;` |
|      - | 2213 | `	/* Point to the underlying vfs */` |
|      9 | 2214 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 2215 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2216 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2217 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2218 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2219 | `			ph7_function_name(pCtx)` |
|      - | 2220 | `			);` |
|    ! 0 | 2221 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2222 | `		return PH7_OK;` |
|      - | 2223 | `	}` |
|      9 | 2224 | `	iNew = 0;` |
|      9 | 2225 | `	if( nArg > 0 ){` |
|      5 | 2226 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2227 | `	}` |
|      - | 2228 | `	/* Perform the requested operation */` |
|      9 | 2229 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2230 | `	/* Old mask */` |
|      9 | 2231 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 2232 | `	return PH7_OK;` |
|      5 | 2233 | `}` |
|      - | 2234 | `/*` |
|      - | 2235 | ` * string sys_get_temp_dir()` |
|      - | 2236 | ` *  Returns directory path used for temporary files.` |
|      - | 2237 | ` * Parameters` |
|      - | 2238 | ` *  None` |
|      - | 2239 | ` * Return` |
|      - | 2240 | ` *  Returns the path of the temporary directory.` |
|      - | 2241 | ` */` |
|    234 | 2242 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2243 | `{` |
|      - | 2244 | `	ph7_vfs *pVfs;` |
|      - | 2245 | `	/* Set the empty string as the default return value */` |
|    238 | 2246 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2247 | `	/* Point to the underlying vfs */` |
|    238 | 2248 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    238 | 2249 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2250 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2251 | `		SXUNUSED(apArg);` |
|      - | 2252 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2253 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2254 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2255 | `			ph7_function_name(pCtx)` |
|      - | 2256 | `			);` |
|    ! 0 | 2257 | `		return PH7_OK;` |
|      - | 2258 | `	}` |
|      - | 2259 | `	/* Perform the requested operation */` |
|    238 | 2260 | `	pVfs->xTempDir(pCtx);` |
|    238 | 2261 | `	return PH7_OK;` |
|    121 | 2262 | `}` |
|      - | 2263 | `/*` |
|      - | 2264 | ` * string get_current_user()` |
|      - | 2265 | ` *  Returns the name of the current working user.` |
|      - | 2266 | ` * Parameters` |
|      - | 2267 | ` *  None` |
|      - | 2268 | ` * Return` |
|      - | 2269 | ` *  Returns the name of the current working user.` |
|      - | 2270 | ` */` |
|      2 | 2271 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2272 | `{` |
|      - | 2273 | `	ph7_vfs *pVfs;` |
|      - | 2274 | `	/* Point to the underlying vfs */` |
|      3 | 2275 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2276 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2277 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2278 | `		SXUNUSED(apArg);` |
|      - | 2279 | `		/* IO routine not implemented */` |
|    ! 0 | 2280 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2281 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2282 | `			ph7_function_name(pCtx)` |
|      - | 2283 | `			);` |
|      - | 2284 | `		/* Set a dummy username */` |
|    ! 0 | 2285 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2286 | `		return PH7_OK;` |
|      - | 2287 | `	}` |
|      - | 2288 | `	/* Perform the requested operation */` |
|      3 | 2289 | `	pVfs->xUsername(pCtx);` |
|      3 | 2290 | `	return PH7_OK;` |
|      2 | 2291 | `}` |
|      - | 2292 | `/*` |
|      - | 2293 | ` * int64 getmypid()` |
|      - | 2294 | ` *  Gets process ID.` |
|      - | 2295 | ` * Parameters` |
|      - | 2296 | ` *  None` |
|      - | 2297 | ` * Return` |
|      - | 2298 | ` *  Returns the process ID.` |
|      - | 2299 | ` */` |
|     80 | 2300 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2301 | `{` |
|      - | 2302 | `	ph7_int64 nProcessId;` |
|      - | 2303 | `	ph7_vfs *pVfs;` |
|      - | 2304 | `	/* Point to the underlying vfs */` |
|     82 | 2305 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     82 | 2306 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2307 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2308 | `		SXUNUSED(apArg);` |
|      - | 2309 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2310 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2311 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2312 | `			ph7_function_name(pCtx)` |
|      - | 2313 | `			);` |
|    ! 0 | 2314 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2315 | `		return PH7_OK;` |
|      - | 2316 | `	}` |
|      - | 2317 | `	/* Perform the requested operation */` |
|     82 | 2318 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2319 | `	/* Set the result */` |
|     82 | 2320 | `	ph7_result_int64(pCtx,nProcessId);` |
|     82 | 2321 | `	return PH7_OK;` |
|     42 | 2322 | `}` |
|      - | 2323 | `/*` |
|      - | 2324 | ` * int getmyuid()` |
|      - | 2325 | ` *  Get user ID.` |
|      - | 2326 | ` * Parameters` |
|      - | 2327 | ` *  None` |
|      - | 2328 | ` * Return` |
|      - | 2329 | ` *  Returns the user ID.` |
|      - | 2330 | ` */` |
|      2 | 2331 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2332 | `{` |
|      - | 2333 | `	ph7_vfs *pVfs;` |
|      - | 2334 | `	int nUid;` |
|      - | 2335 | `	/* Point to the underlying vfs */` |
|      3 | 2336 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2337 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2338 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2339 | `		SXUNUSED(apArg);` |
|      - | 2340 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2341 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2342 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2343 | `			ph7_function_name(pCtx)` |
|      - | 2344 | `			);` |
|    ! 0 | 2345 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2346 | `		return PH7_OK;` |
|      - | 2347 | `	}` |
|      - | 2348 | `	/* Perform the requested operation */` |
|      3 | 2349 | `	nUid = pVfs->xUid();` |
|      - | 2350 | `	/* Set the result */` |
|      3 | 2351 | `	ph7_result_int(pCtx,nUid);` |
|      3 | 2352 | `	return PH7_OK;` |
|      2 | 2353 | `}` |
|      - | 2354 | `/*` |
|      - | 2355 | ` * int getmygid()` |
|      - | 2356 | ` *  Get group ID.` |
|      - | 2357 | ` * Parameters` |
|      - | 2358 | ` *  None` |
|      - | 2359 | ` * Return` |
|      - | 2360 | ` *  Returns the group ID.` |
|      - | 2361 | ` */` |
|      2 | 2362 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2363 | `{` |
|      - | 2364 | `	ph7_vfs *pVfs;` |
|      - | 2365 | `	int nGid;` |
|      - | 2366 | `	/* Point to the underlying vfs */` |
|      3 | 2367 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2368 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 2369 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2370 | `		SXUNUSED(apArg);` |
|      - | 2371 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2372 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2373 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2374 | `			ph7_function_name(pCtx)` |
|      - | 2375 | `			);` |
|    ! 0 | 2376 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2377 | `		return PH7_OK;` |
|      - | 2378 | `	}` |
|      - | 2379 | `	/* Perform the requested operation */` |
|      3 | 2380 | `	nGid = pVfs->xGid();` |
|      - | 2381 | `	/* Set the result */` |
|      3 | 2382 | `	ph7_result_int(pCtx,nGid);` |
|      3 | 2383 | `	return PH7_OK;` |
|      2 | 2384 | `}` |
|      - | 2385 | `#ifdef __WINNT__` |
|      - | 2386 | `#include <Windows.h>` |
|      - | 2387 | `#elif defined(__UNIXES__)` |
|      - | 2388 | `#include <sys/utsname.h>` |
|      - | 2389 | `#endif` |
|      - | 2390 | `/*` |
|      - | 2391 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 2392 | ` *  Returns information about the host operating system.` |
|      - | 2393 | ` * Parameters` |
|      - | 2394 | ` *  $mode` |
|      - | 2395 | ` *   mode is a single character that defines what information is returned:` |
|      - | 2396 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 2397 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 2398 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 2399 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 2400 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 2401 | ` *    'm': Machine type. eg. i386.` |
|      - | 2402 | ` * Return` |
|      - | 2403 | ` *  OS description as a string.` |
|      - | 2404 | ` */` |
|      4 | 2405 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2406 | `{` |
|      - | 2407 | `#if defined(__WINNT__)` |
|      1 | 2408 | `	const char *zName = "Microsoft Windows";` |
|      - | 2409 | `	OSVERSIONINFOW sVer;` |
|      - | 2410 | `#elif defined(__UNIXES__)` |
|      - | 2411 | `	struct utsname sName;` |
|      - | 2412 | `#endif` |
|      5 | 2413 | `	const char *zMode = "a";` |
|      5 | 2414 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 2415 | `		/* Extract the desired mode */` |
|    ! 0 | 2416 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 2417 | `	}` |
|      - | 2418 | `#if defined(__WINNT__)` |
|      1 | 2419 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 2420 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 2421 | `#if defined(_MSC_VER)` |
|      - | 2422 | `#pragma warning(push)` |
|      - | 2423 | `#pragma warning(disable:4996)` |
|      - | 2424 | `#endif` |
|      1 | 2425 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 2426 | `#if defined(_MSC_VER)` |
|      - | 2427 | `#pragma warning(pop)` |
|      - | 2428 | `#endif` |
|    ! 0 | 2429 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 2430 | `		return PH7_OK;` |
|      - | 2431 | `	}` |
|      1 | 2432 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 2433 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 2434 | `			zName = "Microsoft Windows NT";` |
|      1 | 2435 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 2436 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 2437 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 2438 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 2439 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 2440 | `			}` |
|    ! 0 | 2441 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 2442 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 2443 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 2444 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 2445 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 2446 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 2447 | `					default: break;` |
|      - | 2448 | `				}` |
|      - | 2449 | `		}` |
|      - | 2450 | `	}` |
|      1 | 2451 | `	switch(zMode[0]){` |
|      - | 2452 | `	case 's':` |
|      - | 2453 | `		/* Operating system name */` |
|    ! 0 | 2454 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 2455 | `		break;` |
|      - | 2456 | `	case 'n':` |
|      - | 2457 | `		/* Host name */` |
|    ! 0 | 2458 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 2459 | `		break;` |
|      - | 2460 | `	case 'r':` |
|      - | 2461 | `	case 'v':` |
|      - | 2462 | `		/* Version information. */` |
|    ! 0 | 2463 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 2464 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2465 | `			);` |
|    ! 0 | 2466 | `		break;` |
|      - | 2467 | `	case 'm':` |
|      - | 2468 | `		/* Machine name */` |
|    ! 0 | 2469 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 2470 | `		break;` |
|      - | 2471 | `	default:` |
|      1 | 2472 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 2473 | `			zName,` |
|      - | 2474 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2475 | `			);` |
|      - | 2476 | `		break;` |
|      - | 2477 | `	}` |
|      - | 2478 | `#elif defined(__UNIXES__)` |
|      4 | 2479 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 2480 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 2481 | `		return PH7_OK;` |
|      - | 2482 | `	}` |
|      4 | 2483 | `	switch(zMode[0]){` |
|    ! 0 | 2484 | `	case 's':` |
|      - | 2485 | `		/* Operating system name */` |
|    ! 0 | 2486 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 2487 | `		break;` |
|    ! 0 | 2488 | `	case 'n':` |
|      - | 2489 | `		/* Host name */` |
|    ! 0 | 2490 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 2491 | `		break;` |
|    ! 0 | 2492 | `	case 'r':` |
|      - | 2493 | `		/* Release information */` |
|    ! 0 | 2494 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 2495 | `		break;` |
|    ! 0 | 2496 | `	case 'v':` |
|      - | 2497 | `		/* Version information. */` |
|    ! 0 | 2498 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 2499 | `		break;` |
|    ! 0 | 2500 | `	case 'm':` |
|      - | 2501 | `		/* Machine name */` |
|    ! 0 | 2502 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 2503 | `		break;` |
|      2 | 2504 | `	default:` |
|      6 | 2505 | `		ph7_result_string_format(pCtx,` |
|      - | 2506 | `			"%s %s %s %s %s",` |
|      2 | 2507 | `			sName.sysname,` |
|      2 | 2508 | `			sName.release,` |
|      2 | 2509 | `			sName.version,` |
|      2 | 2510 | `			sName.nodename,` |
|      2 | 2511 | `			sName.machine` |
|      - | 2512 | `			);` |
|      4 | 2513 | `		break;` |
|      - | 2514 | `	}` |
|      - | 2515 | `#else` |
|      - | 2516 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 2517 | `#endif` |
|      5 | 2518 | `	return PH7_OK;` |
|      3 | 2519 | `}` |
|      - | 2520 | `/*` |
|      - | 2521 | ` * Section:` |
|      - | 2522 | ` *    IO stream implementation.` |
|      - | 2523 | ` * Status:` |
|      - | 2524 | ` *    Stable.` |
|      - | 2525 | ` */` |
|      - | 2526 | `typedef struct io_private io_private;` |
|      - | 2527 | `struct io_private` |
|      - | 2528 | `{` |
|      - | 2529 | `	const ph7_io_stream *pStream; /* Underlying IO device */` |
|      - | 2530 | `	void *pHandle; /* IO handle */` |
|      - | 2531 | `	/* Unbuffered IO */` |
|      - | 2532 | `	SyBlob sBuffer; /* Working buffer */` |
|      - | 2533 | `	sxu32 nOfft;    /* Current read offset */` |
|      - | 2534 | `	sxu32 iMagic;   /* Sanity check to avoid misuse */` |
|      - | 2535 | `};` |
|      - | 2536 | `#define IO_PRIVATE_MAGIC 0xFEAC14` |
|      - | 2537 | `/* A user-facing handle (fopen/opendir/popen) that has been fclose()'d/closedir()'d/` |
|      - | 2538 | ` * pclose()'d keeps its io_private alive but stamped with this magic, so every` |
|      - | 2539 | ` * ph7_value that still references it observes a closed resource` |
|      - | 2540 | ` * (gettype()=='resource (closed)', is_resource()==false), matching php. */` |
|      - | 2541 | `#define IO_PRIVATE_CLOSED_MAGIC 0xC105ED` |
|      - | 2542 | `/* Stream-device predicates (devices defined later in this file) */` |
|      - | 2543 | `static int is_php_stream(const ph7_io_stream *pStream);` |
|      - | 2544 | `static int is_data_stream(const ph7_io_stream *pStream);` |
|      - | 2545 | `/* Make sure we are dealing with a valid io_private instance */` |
|      - | 2546 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|      - | 2547 | `/* Forward declaration */` |
|      - | 2548 | `static void ResetIOPrivate(io_private *pDev);` |
|      - | 2549 | `/*` |
|      - | 2550 | ` * Return the PHP resource-type name for a raw resource handle.` |
|      - | 2551 | ` * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the` |
|      - | 2552 | ` * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as` |
|      - | 2553 | ` * "stream"; anything else is "Unknown". The magic probe mirrors the` |
|      - | 2554 | ` * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.` |
|      - | 2555 | ` */` |
|      6 | 2556 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      1 | 2557 | `{` |
|      7 | 2558 | `	io_private *pDev = (io_private *)pResource;` |
|      7 | 2559 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|      5 | 2560 | `		return "stream";` |
|      - | 2561 | `	}` |
|      3 | 2562 | `	return "Unknown";` |
|      4 | 2563 | `}` |
|      - | 2564 | `/*` |
|      - | 2565 | ` * Return TRUE if the given resource handle is an io_private that has been closed` |
|      - | 2566 | ` * (fclose/closedir/pclose) yet kept alive so shared copies still observe it. php` |
|      - | 2567 | ` * reports such a value as gettype()=='resource (closed)' and is_resource()==false.` |
|      - | 2568 | ` * The magic probe mirrors IO_PRIVATE_INVALID and is safe on any resource handle:` |
|      - | 2569 | ` * every resource this engine hands out is a struct larger than an io_private, so` |
|      - | 2570 | ` * the iMagic slot is always in bounds and never equals the closed magic by chance.` |
|      - | 2571 | ` */` |
|     54 | 2572 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      3 | 2573 | `{` |
|     57 | 2574 | `	io_private *pDev = (io_private *)pResource;` |
|     57 | 2575 | `	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;` |
|      3 | 2576 | `}` |
|      - | 2577 | `/*` |
|      - | 2578 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - | 2579 | ` *  Truncates a file to a given length.` |
|      - | 2580 | ` * Parameters` |
|      - | 2581 | ` *  $handle` |
|      - | 2582 | ` *   The file pointer.` |
|      - | 2583 | ` *   Note:` |
|      - | 2584 | ` *    The handle must be open for writing.` |
|      - | 2585 | ` * $size` |
|      - | 2586 | ` *   The size to truncate to.` |
|      - | 2587 | ` * Return` |
|      - | 2588 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2589 | ` */` |
|      6 | 2590 | `static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2591 | `{` |
|      - | 2592 | `	const ph7_io_stream *pStream;` |
|      - | 2593 | `	io_private *pDev;` |
|      - | 2594 | `	int rc;` |
|      7 | 2595 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2596 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2597 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2598 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2599 | `		return PH7_OK;` |
|      - | 2600 | `	}` |
|      - | 2601 | `	/* Extract our private data */` |
|      7 | 2602 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2603 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2604 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2605 | `		/*Expecting an IO handle */` |
|    ! 0 | 2606 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2607 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2608 | `		return PH7_OK;` |
|      - | 2609 | `	}` |
|      - | 2610 | `	/* Point to the target IO stream device */` |
|      7 | 2611 | `	pStream = pDev->pStream;` |
|      7 | 2612 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 | 2613 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2614 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2615 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2616 | `			);` |
|    ! 0 | 2617 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2618 | `		return PH7_OK;` |
|      - | 2619 | `	}` |
|      - | 2620 | `	/* Perform the requested operation */` |
|      7 | 2621 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      7 | 2622 | `	if( rc == PH7_OK ){` |
|      - | 2623 | `		/* Discard buffered data */` |
|      7 | 2624 | `		ResetIOPrivate(pDev);` |
|      3 | 2625 | `	}` |
|      - | 2626 | `	/* IO result */` |
|      7 | 2627 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 | 2628 | `	return PH7_OK;` |
|      4 | 2629 | `}` |
|      - | 2630 | `/*` |
|      - | 2631 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - | 2632 | ` *  Seeks on a file pointer.` |
|      - | 2633 | ` * Parameters` |
|      - | 2634 | ` *  $handle` |
|      - | 2635 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - | 2636 | ` * $offset` |
|      - | 2637 | ` *   The offset.` |
|      - | 2638 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - | 2639 | ` *   value in offset and set whence to SEEK_END.` |
|      - | 2640 | ` *   whence` |
|      - | 2641 | ` *   whence values are:` |
|      - | 2642 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - | 2643 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - | 2644 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - | 2645 | ` * Return` |
|      - | 2646 | ` *  0 on success,-1 on failure` |
|      - | 2647 | ` */` |
|     10 | 2648 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2649 | `{` |
|      - | 2650 | `	const ph7_io_stream *pStream;` |
|      - | 2651 | `	io_private *pDev;` |
|      - | 2652 | `	ph7_int64 iOfft;` |
|      - | 2653 | `	int whence;` |
|      - | 2654 | `	int rc;` |
|     12 | 2655 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2656 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2657 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2658 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2659 | `		return PH7_OK;` |
|      - | 2660 | `	}` |
|      - | 2661 | `	/* Extract our private data */` |
|     12 | 2662 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2663 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     12 | 2664 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2665 | `		/*Expecting an IO handle */` |
|    ! 0 | 2666 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2667 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2668 | `		return PH7_OK;` |
|      - | 2669 | `	}` |
|      - | 2670 | `	/* Point to the target IO stream device */` |
|     12 | 2671 | `	pStream = pDev->pStream;` |
|     12 | 2672 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2673 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2674 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2675 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2676 | `			);` |
|    ! 0 | 2677 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2678 | `		return PH7_OK;` |
|      - | 2679 | `	}` |
|      - | 2680 | `	/* Extract the offset */` |
|     12 | 2681 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     12 | 2682 | `	whence = 0;/* SEEK_SET */` |
|     12 | 2683 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      3 | 2684 | `		whence = ph7_value_to_int(apArg[2]);` |
|      1 | 2685 | `	}` |
|      - | 2686 | `	/* Perform the requested operation */` |
|     12 | 2687 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     12 | 2688 | `	if( rc == PH7_OK ){` |
|      - | 2689 | `		/* Ignore buffered data */` |
|     12 | 2690 | `		ResetIOPrivate(pDev);` |
|      5 | 2691 | `	}` |
|      - | 2692 | `	/* IO result */` |
|     12 | 2693 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     12 | 2694 | `	return PH7_OK;` |
|      7 | 2695 | `}` |
|      - | 2696 | `/*` |
|      - | 2697 | ` * int64 ftell(resource $handle)` |
|      - | 2698 | ` *  Returns the current position of the file read/write pointer.` |
|      - | 2699 | ` * Parameters` |
|      - | 2700 | ` *  $handle` |
|      - | 2701 | ` *   The file pointer.` |
|      - | 2702 | ` * Return` |
|      - | 2703 | ` *  Returns the position of the file pointer referenced by handle` |
|      - | 2704 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - | 2705 | ` *  FALSE is returned on failure.` |
|      - | 2706 | ` */` |
|     12 | 2707 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2708 | `{` |
|      - | 2709 | `	const ph7_io_stream *pStream;` |
|      - | 2710 | `	io_private *pDev;` |
|      - | 2711 | `	ph7_int64 iOfft;` |
|     14 | 2712 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2713 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2714 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2715 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2716 | `		return PH7_OK;` |
|      - | 2717 | `	}` |
|      - | 2718 | `	/* Extract our private data */` |
|     14 | 2719 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2720 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     14 | 2721 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2722 | `		/*Expecting an IO handle */` |
|    ! 0 | 2723 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2724 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2725 | `		return PH7_OK;` |
|      - | 2726 | `	}` |
|      - | 2727 | `	/* Point to the target IO stream device */` |
|     14 | 2728 | `	pStream = pDev->pStream;` |
|     14 | 2729 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2730 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2731 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2732 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2733 | `			);` |
|    ! 0 | 2734 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2735 | `		return PH7_OK;` |
|      - | 2736 | `	}` |
|      - | 2737 | `	/* Perform the requested operation */` |
|     14 | 2738 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2739 | `	/* IO result */` |
|     14 | 2740 | `	ph7_result_int64(pCtx,iOfft);` |
|     14 | 2741 | `	return PH7_OK;` |
|      8 | 2742 | `}` |
|      - | 2743 | `/*` |
|      - | 2744 | ` * bool rewind(resource $handle)` |
|      - | 2745 | ` *  Rewind the position of a file pointer.` |
|      - | 2746 | ` * Parameters` |
|      - | 2747 | ` *  $handle` |
|      - | 2748 | ` *   The file pointer.` |
|      - | 2749 | ` * Return` |
|      - | 2750 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2751 | ` */` |
|     14 | 2752 | `static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2753 | `{` |
|      - | 2754 | `	const ph7_io_stream *pStream;` |
|      - | 2755 | `	io_private *pDev;` |
|      - | 2756 | `	int rc;` |
|     15 | 2757 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2758 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2759 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2760 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2761 | `		return PH7_OK;` |
|      - | 2762 | `	}` |
|      - | 2763 | `	/* Extract our private data */` |
|     15 | 2764 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2765 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     15 | 2766 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2767 | `		/*Expecting an IO handle */` |
|    ! 0 | 2768 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2769 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2770 | `		return PH7_OK;` |
|      - | 2771 | `	}` |
|      - | 2772 | `	/* Point to the target IO stream device */` |
|     15 | 2773 | `	pStream = pDev->pStream;` |
|     15 | 2774 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2775 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2776 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2777 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2778 | `			);` |
|    ! 0 | 2779 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2780 | `		return PH7_OK;` |
|      - | 2781 | `	}` |
|      - | 2782 | `	/* Perform the requested operation */` |
|     15 | 2783 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|     15 | 2784 | `	if( rc == PH7_OK ){` |
|      - | 2785 | `		/* Ignore buffered data */` |
|     15 | 2786 | `		ResetIOPrivate(pDev);` |
|      7 | 2787 | `	}` |
|      - | 2788 | `	/* IO result */` |
|     15 | 2789 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     15 | 2790 | `	return PH7_OK;` |
|      8 | 2791 | `}` |
|      - | 2792 | `/*` |
|      - | 2793 | ` * bool fflush(resource $handle)` |
|      - | 2794 | ` *  Flushes the output to a file.` |
|      - | 2795 | ` * Parameters` |
|      - | 2796 | ` *  $handle` |
|      - | 2797 | ` *   The file pointer.` |
|      - | 2798 | ` * Return` |
|      - | 2799 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2800 | ` */` |
|      2 | 2801 | `static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2802 | `{` |
|      - | 2803 | `	const ph7_io_stream *pStream;` |
|      - | 2804 | `	io_private *pDev;` |
|      - | 2805 | `	int rc;` |
|      3 | 2806 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2807 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2808 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2809 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2810 | `		return PH7_OK;` |
|      - | 2811 | `	}` |
|      - | 2812 | `	/* Extract our private data */` |
|      3 | 2813 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2814 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2815 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2816 | `		/*Expecting an IO handle */` |
|    ! 0 | 2817 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2818 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2819 | `		return PH7_OK;` |
|      - | 2820 | `	}` |
|      - | 2821 | `	/* Point to the target IO stream device */` |
|      3 | 2822 | `	pStream = pDev->pStream;` |
|      3 | 2823 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 | 2824 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2825 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2826 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2827 | `			);` |
|    ! 0 | 2828 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2829 | `		return PH7_OK;` |
|      - | 2830 | `	}` |
|      - | 2831 | `	/* Perform the requested operation */` |
|      3 | 2832 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - | 2833 | `	/* IO result */` |
|      3 | 2834 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2835 | `	return PH7_OK;` |
|      2 | 2836 | `}` |
|      - | 2837 | `/*` |
|      - | 2838 | ` * bool feof(resource $handle)` |
|      - | 2839 | ` *  Tests for end-of-file on a file pointer.` |
|      - | 2840 | ` * Parameters` |
|      - | 2841 | ` *  $handle` |
|      - | 2842 | ` *   The file pointer.` |
|      - | 2843 | ` * Return` |
|      - | 2844 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - | 2845 | ` */` |
|  10510 | 2846 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2847 | `{` |
|      - | 2848 | `	const ph7_io_stream *pStream;` |
|      - | 2849 | `	io_private *pDev;` |
|      - | 2850 | `	int rc;` |
|  10515 | 2851 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2852 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2853 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2854 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2855 | `		return PH7_OK;` |
|      - | 2856 | `	}` |
|      - | 2857 | `	/* Extract our private data */` |
|  10515 | 2858 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2859 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10515 | 2860 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2861 | `		/*Expecting an IO handle */` |
|    ! 0 | 2862 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2863 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2864 | `		return PH7_OK;` |
|      - | 2865 | `	}` |
|      - | 2866 | `	/* Point to the target IO stream device */` |
|  10515 | 2867 | `	pStream = pDev->pStream;` |
|  10515 | 2868 | `	if( pStream == 0 ){` |
|    ! 0 | 2869 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2870 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2871 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2872 | `			);` |
|    ! 0 | 2873 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2874 | `		return PH7_OK;` |
|      - | 2875 | `	}` |
|  10515 | 2876 | `	rc = SXERR_EOF;` |
|      - | 2877 | `	/* Perform the requested operation */` |
|  10515 | 2878 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2879 | `		/* Data is available */` |
|   4815 | 2880 | `		rc = PH7_OK;` |
|   2410 | 2881 | `	}else{` |
|      - | 2882 | `		char zBuf[4096];` |
|      - | 2883 | `		ph7_int64 n;` |
|      - | 2884 | `		/* Perform a buffered read */` |
|   5705 | 2885 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5705 | 2886 | `		if( n > 0 ){` |
|      - | 2887 | `			/* Copy buffered data */` |
|   1861 | 2888 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1861 | 2889 | `			rc = PH7_OK;` |
|    928 | 2890 | `		}` |
|      - | 2891 | `	}` |
|      - | 2892 | `	/* EOF or not */` |
|  10515 | 2893 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10515 | 2894 | `	return PH7_OK;` |
|   5260 | 2895 | `}` |
|      - | 2896 | `/*` |
|      - | 2897 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2898 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2899 | ` * [i.e: IO error ] or EOF.` |
|      - | 2900 | ` */` |
|     36 | 2901 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 | 2902 | `{` |
|     38 | 2903 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     38 | 2904 | `	char *zBuf = (char *)pBuf;` |
|      - | 2905 | `	ph7_int64 n,nRead;` |
|     38 | 2906 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     38 | 2907 | `	if( n > 0 ){` |
|      2 | 2908 | `		if( n > nLen ){` |
|    ! 0 | 2909 | `			n = nLen;` |
|    ! 0 | 2910 | `		}` |
|      - | 2911 | `		/* Copy the buffered data */` |
|      2 | 2912 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2913 | `		/* Update the read offset */` |
|      2 | 2914 | `		pDev->nOfft += (sxu32)n;` |
|      2 | 2915 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2916 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      2 | 2917 | `			SyBlobReset(&pDev->sBuffer);` |
|      2 | 2918 | `			pDev->nOfft = 0;` |
|      1 | 2919 | `		}` |
|      2 | 2920 | `		nLen -= n;` |
|      2 | 2921 | `		if( nLen < 1 ){` |
|      - | 2922 | `			/* All done */` |
|    ! 0 | 2923 | `			return n;` |
|      - | 2924 | `		}` |
|      - | 2925 | `		/* Advance the cursor */` |
|      2 | 2926 | `		zBuf += n;` |
|      1 | 2927 | `	}` |
|      - | 2928 | `	/* Read without buffering */` |
|     38 | 2929 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     38 | 2930 | `	if( nRead > 0 ){` |
|     34 | 2931 | `		n += nRead;` |
|     21 | 2932 | `	}else if( n < 1 ){` |
|      - | 2933 | `		/* EOF or IO error */` |
|      3 | 2934 | `		return nRead;` |
|      - | 2935 | `	}` |
|     36 | 2936 | `	return n;` |
|     20 | 2937 | `}` |
|      - | 2938 | `/*` |
|      - | 2939 | ` * Extract a single line from the buffered input.` |
|      - | 2940 | ` */` |
|   6732 | 2941 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 | 2942 | `{` |
|      - | 2943 | `	const char *zIn,*zEnd,*zPtr;` |
|   6737 | 2944 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6737 | 2945 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6737 | 2946 | `	zPtr = zIn;` |
| 391816 | 2947 | `	while( zIn < zEnd ){` |
| 391712 | 2948 | `		if( zIn[0] == '\n' ){` |
|      - | 2949 | `			/* Line found */` |
|   6633 | 2950 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6633 | 2951 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6633 | 2952 | `			*pzLine = zPtr;` |
|   6633 | 2953 | `			return SXRET_OK;` |
|      - | 2954 | `		}` |
| 385084 | 2955 | `		zIn++;` |
|      5 | 2956 | `	}` |
|      - | 2957 | `	/* No line were found */` |
|    109 | 2958 | `	return SXERR_NOTFOUND;` |
|   3371 | 2959 | `}` |
|      - | 2960 | `/*` |
|      - | 2961 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2962 | ` */` |
|   6736 | 2963 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 | 2964 | `{` |
|   6741 | 2965 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2966 | `	char zBuf[8192];` |
|      - | 2967 | `	ph7_int64 n;` |
|      - | 2968 | `	sxi32 rc;` |
|   6741 | 2969 | `	n = 0;` |
|   6741 | 2970 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2971 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     73 | 2972 | `		SyBlobReset(&pDev->sBuffer);` |
|     73 | 2973 | `		pDev->nOfft = 0;` |
|     34 | 2974 | `	}` |
|   6741 | 2975 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2976 | `		/* Check if there is a line */` |
|   6673 | 2977 | `		rc = GetLine(pDev,&n,pzData);` |
|   6673 | 2978 | `		if( rc == SXRET_OK ){` |
|      - | 2979 | `			/* Got line,update the cursor  */` |
|   6573 | 2980 | `			pDev->nOfft += (sxu32)n;` |
|   6573 | 2981 | `			return n;` |
|      - | 2982 | `		}` |
|     50 | 2983 | `	}` |
|      - | 2984 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2985 | `	 * limit is reached.` |
|      - | 2986 | `	 */` |
|     86 | 2987 | `	for(;;){` |
|    177 | 2988 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    177 | 2989 | `		if( n < 1 ){` |
|      - | 2990 | `			/* EOF or IO error */` |
|    113 | 2991 | `			break;` |
|      - | 2992 | `		}` |
|      - | 2993 | `		/* Append the data just read */` |
|     67 | 2994 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2995 | `		/* Try to extract a line */` |
|     67 | 2996 | `		rc = GetLine(pDev,&n,pzData);` |
|     67 | 2997 | `		if( rc == SXRET_OK ){` |
|      - | 2998 | `			/* Got one,return immediately */` |
|     63 | 2999 | `			pDev->nOfft += (sxu32)n;` |
|     63 | 3000 | `			return n;` |
|      - | 3001 | `		}` |
|      5 | 3002 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - | 3003 | `			/* Read limit reached,return the available data */` |
|    ! 0 | 3004 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 | 3005 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 3006 | `			/* Reset the working buffer */` |
|    ! 0 | 3007 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 3008 | `			pDev->nOfft = 0;` |
|    ! 0 | 3009 | `			return n;` |
|      - | 3010 | `		}` |
|      1 | 3011 | `	}` |
|    113 | 3012 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 3013 | `		/* Read limit reached,return the available data */` |
|    109 | 3014 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    109 | 3015 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 3016 | `		/* Reset the working buffer */` |
|    109 | 3017 | `		SyBlobReset(&pDev->sBuffer);` |
|    109 | 3018 | `		pDev->nOfft = 0;` |
|     52 | 3019 | `	}` |
|    113 | 3020 | `	return n;` |
|   3373 | 3021 | `}` |
|      - | 3022 | `/*` |
|      - | 3023 | ` * Open an IO stream handle.` |
|      - | 3024 | ` * Notes on stream:` |
|      - | 3025 | ` * According to the PHP reference manual.` |
|      - | 3026 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - | 3027 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - | 3028 | ` * to an arbitrary locations within the stream.` |
|      - | 3029 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - | 3030 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - | 3031 | ` * on a remote server.` |
|      - | 3032 | ` * A stream is referenced as: scheme://target` |
|      - | 3033 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - | 3034 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - | 3035 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - | 3036 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - | 3037 | ` *  with a path appended.` |
|      - | 3038 | ` *` |
|      - | 3039 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - | 3040 | ` * Please refer to the official documentation for a full discussion.` |
|      - | 3041 | ` * This function return a handle on success. Otherwise null.` |
|      - | 3042 | ` */` |
|  30282 | 3043 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 3044 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 | 3045 | `{` |
|  30287 | 3046 | `	void *pHandle = 0; /* cc warning */` |
|      - | 3047 | `	SyString sFile;` |
|      - | 3048 | `	ph7_value sDummy;` |
|      - | 3049 | `	int rc;` |
|  30287 | 3050 | `	if( pStream == 0 ){` |
|      - | 3051 | `		/* No such stream device */` |
|    ! 0 | 3052 | `		return 0;` |
|      - | 3053 | `	}` |
|  30287 | 3054 | `	if( pResource == 0 ){` |
|      - | 3055 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - | 3056 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - | 3057 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - | 3058 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - | 3059 | `		 * reads it during the call, and file:// ignores it. */` |
|  30265 | 3060 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  30265 | 3061 | `		pResource = &sDummy;` |
|  15130 | 3062 | `	}` |
|  30287 | 3063 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  30287 | 3064 | `	if( use_include ){` |
|   9672 | 3065 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 3066 | `#ifdef __WINNT__` |
|      - | 3067 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 3068 | `#endif` |
|   9650 | 3069 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9646 | 3070 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 3071 | `				/*  Open the file directly */` |
|     27 | 3072 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     14 | 3073 | `		}else{` |
|      - | 3074 | `			SyString *pPath;` |
|      - | 3075 | `			SyBlob sWorker;` |
|      - | 3076 | `#ifdef __WINNT__` |
|      - | 3077 | `			static const int c = '\\';` |
|      - | 3078 | `#else` |
|      - | 3079 | `			static const int c = '/';` |
|      - | 3080 | `#endif` |
|      - | 3081 | `			/* Init the path builder working buffer */` |
|   9650 | 3082 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 3083 | `			/* Build a path from the set of include path */` |
|   9650 | 3084 | `			SySetResetCursor(&pVm->aPaths);` |
|   9650 | 3085 | `			rc = SXERR_IO;` |
|   9656 | 3086 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 3087 | `				/* Build full path */` |
|   9650 | 3088 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 3089 | `				/* Append null terminator */` |
|   9650 | 3090 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 3091 | `					continue;` |
|      - | 3092 | `				}` |
|      - | 3093 | `				/* Try to open the file */` |
|   9650 | 3094 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9650 | 3095 | `				if( rc == PH7_OK ){` |
|   9643 | 3096 | `					if( bPushInclude ){` |
|      - | 3097 | `						/* Mark as included */` |
|   9643 | 3098 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4820 | 3099 | `					}` |
|   9643 | 3100 | `					break;` |
|      - | 3101 | `				}` |
|      - | 3102 | `				/* Reset the working buffer */` |
|      8 | 3103 | `				SyBlobReset(&sWorker);` |
|      - | 3104 | `				/* Check the next path */` |
|      2 | 3105 | `			}` |
|   9650 | 3106 | `			SyBlobRelease(&sWorker);` |
|      - | 3107 | `		}` |
|   9676 | 3108 | `		if( rc == PH7_OK ){` |
|   9669 | 3109 | `			if( bPushInclude ){` |
|      - | 3110 | `				/* Mark as included */` |
|   9669 | 3111 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   4833 | 3112 | `			}` |
|   4833 | 3113 | `		}` |
|   4840 | 3114 | `	}else{` |
|      - | 3115 | `		/* Open the URI direcly */` |
|  20615 | 3116 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 3117 | `	}` |
|  30287 | 3118 | `	if( rc != PH7_OK ){` |
|      - | 3119 | `		/* IO error */` |
|     23 | 3120 | `		return 0;` |
|      - | 3121 | `	}` |
|      - | 3122 | `	/* Return the file handle */` |
|  30269 | 3123 | `	return pHandle;` |
|  15146 | 3124 | `}` |
|      - | 3125 | `/*` |
|      - | 3126 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 3127 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 3128 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 3129 | ` */` |
|   9660 | 3130 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      3 | 3131 | `{` |
|      - | 3132 | `	ph7_int64 nRead;` |
|      - | 3133 | `	char zBuf[8192]; /* 8K */` |
|      - | 3134 | `	int rc;` |
|      - | 3135 | `	/* Perform the requested operation */` |
|   9660 | 3136 | `	for(;;){` |
|  19323 | 3137 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  19323 | 3138 | `		if( nRead < 1 ){` |
|      - | 3139 | `			/* EOF or IO error */` |
|   9663 | 3140 | `			break;` |
|      - | 3141 | `		}` |
|      - | 3142 | `		/* Append contents */` |
|   9663 | 3143 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9663 | 3144 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3145 | `			break;` |
|      - | 3146 | `		}` |
|      3 | 3147 | `	}` |
|   9663 | 3148 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      3 | 3149 | `}` |
|      - | 3150 | `/*` |
|      - | 3151 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 3152 | ` */` |
|  30362 | 3153 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 3154 | `{` |
|  30367 | 3155 | `	if( pStream->xClose ){` |
|  30367 | 3156 | `		pStream->xClose(pHandle);` |
|  15181 | 3157 | `	}` |
|  30367 | 3158 | `}` |
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
|   6726 | 3229 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3230 | `{` |
|      - | 3231 | `	const ph7_io_stream *pStream;` |
|      - | 3232 | `	const char *zLine;` |
|      - | 3233 | `	io_private *pDev;` |
|      - | 3234 | `	ph7_int64 n,nLen;` |
|   6731 | 3235 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3236 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3237 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3238 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3239 | `		return PH7_OK;` |
|      - | 3240 | `	}` |
|      - | 3241 | `	/* Extract our private data */` |
|   6731 | 3242 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3243 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6731 | 3244 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3245 | `		/*Expecting an IO handle */` |
|    ! 0 | 3246 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3247 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3248 | `		return PH7_OK;` |
|      - | 3249 | `	}` |
|      - | 3250 | `	/* Point to the target IO stream device */` |
|   6731 | 3251 | `	pStream = pDev->pStream;` |
|   6731 | 3252 | `	if( pStream == 0  ){` |
|    ! 0 | 3253 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3254 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3255 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3256 | `			);` |
|    ! 0 | 3257 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3258 | `		return PH7_OK;` |
|      - | 3259 | `	}` |
|   6731 | 3260 | `	nLen = -1;` |
|   6731 | 3261 | `	if( nArg > 1 ){` |
|      - | 3262 | `		/* Maximum data to read */` |
|    ! 0 | 3263 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3264 | `	}` |
|      - | 3265 | `	/* Perform the requested operation */` |
|   6731 | 3266 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6731 | 3267 | `	if( n < 1 ){` |
|      - | 3268 | `		/* EOF or IO error,return FALSE */` |
|      7 | 3269 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3270 | `	}else{` |
|      - | 3271 | `		/* Return the freshly extracted line */` |
|   6729 | 3272 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3273 | `	}` |
|   6731 | 3274 | `	return PH7_OK;` |
|   3368 | 3275 | `}` |
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
|      2 | 3288 | `{` |
|      - | 3289 | `	const ph7_io_stream *pStream;` |
|      - | 3290 | `	io_private *pDev;` |
|      - | 3291 | `	ph7_int64 nRead;` |
|      - | 3292 | `	void *pBuf;` |
|      - | 3293 | `	int nLen;` |
|     30 | 3294 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3295 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3296 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3297 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3298 | `		return PH7_OK;` |
|      - | 3299 | `	}` |
|      - | 3300 | `	/* Extract our private data */` |
|     30 | 3301 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3302 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     30 | 3303 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3304 | `		/*Expecting an IO handle */` |
|    ! 0 | 3305 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3306 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3307 | `		return PH7_OK;` |
|      - | 3308 | `	}` |
|      - | 3309 | `	/* Point to the target IO stream device */` |
|     30 | 3310 | `	pStream = pDev->pStream;` |
|     30 | 3311 | `	if( pStream == 0  ){` |
|    ! 0 | 3312 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3313 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3314 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3315 | `			);` |
|    ! 0 | 3316 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3317 | `		return PH7_OK;` |
|      - | 3318 | `	}` |
|     30 | 3319 | `        nLen = 4096;` |
|     30 | 3320 | `	if( nArg > 1 ){` |
|     30 | 3321 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     30 | 3322 | `	  if( nLen < 1 ){` |
|      - | 3323 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3324 | `		nLen = 4096;` |
|    ! 0 | 3325 | `	  }` |
|     14 | 3326 | `        }` |
|      - | 3327 | `	/* Allocate enough buffer */` |
|     30 | 3328 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     30 | 3329 | `	if( pBuf == 0 ){` |
|    ! 0 | 3330 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3331 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3332 | `		return PH7_OK;` |
|      - | 3333 | `	}` |
|      - | 3334 | `	/* Perform the requested operation */` |
|     30 | 3335 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     30 | 3336 | `	if( nRead < 1 ){` |
|      - | 3337 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3338 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3339 | `	}else{` |
|      - | 3340 | `		/* Make a copy of the data just read */` |
|     30 | 3341 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3342 | `	}` |
|      - | 3343 | `	/* Release the buffer */` |
|     30 | 3344 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     30 | 3345 | `	return PH7_OK;` |
|     16 | 3346 | `}` |
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
|  10950 | 3538 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3539 | `{` |
|      - | 3540 | `	const ph7_io_stream *pStream;` |
|      - | 3541 | `	io_private *pDev;` |
|      - | 3542 | `	int rc;` |
|  10955 | 3543 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3544 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3545 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3546 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3547 | `		return PH7_OK;` |
|      - | 3548 | `	}` |
|      - | 3549 | `	/* Extract our private data */` |
|  10955 | 3550 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3551 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10955 | 3552 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3553 | `		/*Expecting an IO handle */` |
|    ! 0 | 3554 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3556 | `		return PH7_OK;` |
|      - | 3557 | `	}` |
|      - | 3558 | `	/* Point to the target IO stream device */` |
|  10955 | 3559 | `	pStream = pDev->pStream;` |
|  10955 | 3560 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3561 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3562 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3563 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3564 | `			);` |
|    ! 0 | 3565 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3566 | `		return PH7_OK;` |
|      - | 3567 | `	}` |
|  10955 | 3568 | `	ph7_result_bool(pCtx,0);` |
|      - | 3569 | `	/* Perform the requested operation */` |
|  10955 | 3570 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  10955 | 3571 | `	if( rc != PH7_OK ){` |
|      - | 3572 | `		/* Return FALSE */` |
|   1065 | 3573 | `		ph7_result_bool(pCtx,0);` |
|    530 | 3574 | `	}` |
|  10955 | 3575 | `	return PH7_OK;` |
|   5480 | 3576 | `}` |
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
|   1064 | 3632 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3633 | `{` |
|      - | 3634 | `	const ph7_io_stream *pStream;` |
|      - | 3635 | `	io_private *pDev;` |
|   1069 | 3636 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3637 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3638 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3639 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3640 | `		return PH7_OK;` |
|      - | 3641 | `	}` |
|      - | 3642 | `	/* Extract our private data */` |
|   1069 | 3643 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3644 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1069 | 3645 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3646 | `		/*Expecting an IO handle */` |
|    ! 0 | 3647 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3648 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3649 | `		return PH7_OK;` |
|      - | 3650 | `	}` |
|      - | 3651 | `	/* Point to the target IO stream device */` |
|   1069 | 3652 | `	pStream = pDev->pStream;` |
|   1069 | 3653 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3654 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3655 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3656 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3657 | `			);` |
|    ! 0 | 3658 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3659 | `		return PH7_OK;` |
|      - | 3660 | `	}` |
|      - | 3661 | `	/* Perform the requested operation */` |
|   1069 | 3662 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3663 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|   1069 | 3664 | `	MarkIOPrivateClosed(pDev);` |
|   1069 | 3665 | `	return PH7_OK;` |
|    537 | 3666 | ` }` |
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
|   1064 | 3678 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3679 | `{` |
|      - | 3680 | `	const ph7_io_stream *pStream;` |
|      - | 3681 | `	const char *zPath;` |
|      - | 3682 | `	io_private *pDev;` |
|      - | 3683 | `	int iLen,rc;` |
|   1069 | 3684 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3685 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3686 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3687 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3688 | `		return PH7_OK;` |
|      - | 3689 | `	}` |
|      - | 3690 | `	/* Extract the target path */` |
|   1069 | 3691 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3692 | `	/* Try to extract a stream */` |
|   1069 | 3693 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1069 | 3694 | `	if( pStream == 0 ){` |
|    ! 0 | 3695 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3696 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3697 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3698 | `		return PH7_OK;` |
|      - | 3699 | `	}` |
|   1069 | 3700 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3701 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3702 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3703 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3704 | `			);` |
|    ! 0 | 3705 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3706 | `		return PH7_OK;` |
|      - | 3707 | `	}` |
|      - | 3708 | `	/* Allocate a new IO private instance */` |
|   1069 | 3709 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1069 | 3710 | `	if( pDev == 0 ){` |
|    ! 0 | 3711 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3712 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3713 | `		return PH7_OK;` |
|      - | 3714 | `	}` |
|      - | 3715 | `	/* Initialize the structure */` |
|   1069 | 3716 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3717 | `	/* Open the target directory */` |
|   1069 | 3718 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1069 | 3719 | `	if( rc != PH7_OK ){` |
|      - | 3720 | `		/* IO error,return FALSE */` |
|    ! 0 | 3721 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3722 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3723 | `	}else{` |
|      - | 3724 | `		/* Return the handle as a resource */` |
|   1069 | 3725 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3726 | `	}` |
|   1069 | 3727 | `	return PH7_OK;` |
|    537 | 3728 | `}` |
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
|   6716 | 3828 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3829 | `{` |
|      - | 3830 | `	const ph7_io_stream *pStream;` |
|      - | 3831 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6721 | 3832 | `	int use_include  = FALSE;` |
|      - | 3833 | `	const char *zFile;` |
|      - | 3834 | `	char zBuf[8192];` |
|      - | 3835 | `	void *pHandle;` |
|      - | 3836 | `	int nLen;` |
|      - | 3837 |  |
|   6721 | 3838 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3839 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3840 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3841 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3842 | `		return PH7_OK;` |
|      - | 3843 | `	}` |
|      - | 3844 | `	/* Extract the file path */` |
|   6721 | 3845 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3846 | `	/* Point to the target IO stream device */` |
|   6721 | 3847 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6721 | 3848 | `	if( pStream == 0 ){` |
|    ! 0 | 3849 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3850 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3851 | `		return PH7_OK;` |
|      - | 3852 | `	}` |
|   6721 | 3853 | `	nMaxlen = -1;` |
|   6721 | 3854 | `	if( nArg > 1 ){` |
|      5 | 3855 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3856 | `	}` |
|      - | 3857 | `	/* Try to open the file in read-only mode */` |
|   6721 | 3858 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6721 | 3859 | `	if( pHandle == 0 ){` |
|    ! 0 | 3860 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3861 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3862 | `		return PH7_OK;` |
|      - | 3863 | `	}` |
|   6721 | 3864 | `	if( nArg > 3 ){` |
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
|   6721 | 3879 | `	nRead = 0;` |
|   6714 | 3880 | `	for(;;){` |
|  20150 | 3881 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6717 | 3882 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  13433 | 3883 | `		if( n < 1 ){` |
|      - | 3884 | `			/* EOF or IO error,break immediately */` |
|   6719 | 3885 | `			break;` |
|      - | 3886 | `		}` |
|      - | 3887 | `		/* Append data */` |
|   6719 | 3888 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3889 | `		/* Increment read counter */` |
|   6719 | 3890 | `		nRead += n;` |
|   6719 | 3891 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3892 | `			/* Read limit reached */` |
|      3 | 3893 | `			break;` |
|      - | 3894 | `		}` |
|      5 | 3895 | `	}` |
|      - | 3896 | `	/* Close the stream */` |
|   6721 | 3897 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3898 | `	/* Check if we have read something */` |
|   6721 | 3899 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3900 | `		/* Nothing read,return FALSE */` |
|      3 | 3901 | `		ph7_result_bool(pCtx,0);` |
|      1 | 3902 | `	}` |
|   6721 | 3903 | `	return PH7_OK;` |
|   3363 | 3904 | `}` |
|      - | 3905 | `/*` |
|      - | 3906 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3907 | ` *  Write a string to a file.` |
|      - | 3908 | ` * Parameters` |
|      - | 3909 | ` *  $filename` |
|      - | 3910 | ` *  Path to the file where to write the data.` |
|      - | 3911 | ` * $data` |
|      - | 3912 | ` *  The data to write(Must be a string).` |
|      - | 3913 | ` * $flags` |
|      - | 3914 | ` *  The value of flags can be any combination of the following` |
|      - | 3915 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3916 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3917 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3918 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3919 | ` * context` |
|      - | 3920 | ` *  A context stream resource.` |
|      - | 3921 | ` * Return` |
|      - | 3922 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3923 | ` */` |
|  13652 | 3924 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3925 | `{` |
|  13657 | 3926 | `	int use_include  = FALSE;` |
|      - | 3927 | `	const ph7_io_stream *pStream;` |
|      - | 3928 | `	const char *zFile;` |
|      - | 3929 | `	const char *zData;` |
|      - | 3930 | `	int iOpenFlags;` |
|      - | 3931 | `	void *pHandle;` |
|      - | 3932 | `	int iFlags;` |
|      - | 3933 | `	int nLen;` |
|      - | 3934 |  |
|  13657 | 3935 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3936 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3937 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3938 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3939 | `		return PH7_OK;` |
|      - | 3940 | `	}` |
|      - | 3941 | `	/* Extract the file path */` |
|  13657 | 3942 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3943 | `	/* Point to the target IO stream device */` |
|  13657 | 3944 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13657 | 3945 | `	if( pStream == 0 ){` |
|    ! 0 | 3946 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3947 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3948 | `		return PH7_OK;` |
|      - | 3949 | `	}` |
|      - | 3950 | `	/* Data to write */` |
|  13657 | 3951 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3952 | `	/* Try to open the file in read-write mode */` |
|  13657 | 3953 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3954 | `	/* Extract the flags */` |
|  13657 | 3955 | `	iFlags = 0;` |
|  13657 | 3956 | `	if( nArg > 2 ){` |
|    ! 0 | 3957 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3958 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3959 | `			use_include = TRUE;` |
|    ! 0 | 3960 | `		}` |
|    ! 0 | 3961 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3962 | `			/* If the file already exists, append the data to the file` |
|      - | 3963 | `			 * instead of overwriting it.` |
|      - | 3964 | `			 */` |
|    ! 0 | 3965 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3966 | `			/* Append mode */` |
|    ! 0 | 3967 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3968 | `		}` |
|    ! 0 | 3969 | `	}` |
|  20483 | 3970 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6826 | 3971 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13657 | 3972 | `	if( pHandle == 0 ){` |
|    ! 0 | 3973 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3974 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3975 | `		return PH7_OK;` |
|      - | 3976 | `	}` |
|  13657 | 3977 | `	if( nLen < 1 ){` |
|      - | 3978 | `		/* Empty data, file is created/truncated */` |
|      7 | 3979 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3980 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3981 | `		return PH7_OK;` |
|      - | 3982 | `	}` |
|  13651 | 3983 | `	if( pStream->xWrite ){` |
|      - | 3984 | `		ph7_int64 n;` |
|  13651 | 3985 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 3986 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3987 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3988 | `		}` |
|      - | 3989 | `		/* Perform the write operation */` |
|  13651 | 3990 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13651 | 3991 | `		if( n < 0 ){` |
|      - | 3992 | `			/* IO error,return FALSE */` |
|    ! 0 | 3993 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3994 | `		}else{` |
|      - | 3995 | `			/* Total number of bytes written */` |
|  13651 | 3996 | `			ph7_result_int64(pCtx,n);` |
|      - | 3997 | `		}` |
|   6828 | 3998 | `	}else{` |
|      - | 3999 | `		/* Read-only stream */` |
|    ! 0 | 4000 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 4001 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 4002 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 4003 | `			);` |
|    ! 0 | 4004 | `		ph7_result_bool(pCtx,0);` |
|      - | 4005 | `	}` |
|      - | 4006 | `	/* Close the handle */` |
|  13651 | 4007 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13651 | 4008 | `	return PH7_OK;` |
|   6831 | 4009 | `}` |
|      - | 4010 | `/*` |
|      - | 4011 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 4012 | ` *  Reads entire file into an array.` |
|      - | 4013 | ` * Parameters` |
|      - | 4014 | ` *  $filename` |
|      - | 4015 | ` *   The filename being read.` |
|      - | 4016 | ` *  $flags` |
|      - | 4017 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 4018 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 4019 | ` *       Search for the file in the include_path.` |
|      - | 4020 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 4021 | ` *       Do not add newline at the end of each array element` |
|      - | 4022 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 4023 | ` *       Skip empty lines` |
|      - | 4024 | ` *  $context` |
|      - | 4025 | ` *   A context stream resource.` |
|      - | 4026 | ` * Return` |
|      - | 4027 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 4028 | ` */` |
|     10 | 4029 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4030 | `{` |
|      - | 4031 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 4032 | `	ph7_value *pArray,*pLine;` |
|      - | 4033 | `	const ph7_io_stream *pStream;` |
|     13 | 4034 | `	int use_include = 0;` |
|      - | 4035 | `	io_private *pDev;` |
|      - | 4036 | `	ph7_int64 n;` |
|      - | 4037 | `	int iFlags;` |
|      - | 4038 | `	int nLen;` |
|      - | 4039 |  |
|     13 | 4040 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4041 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4042 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4043 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4044 | `		return PH7_OK;` |
|      - | 4045 | `	}` |
|      - | 4046 | `	/* Extract the file path */` |
|     13 | 4047 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4048 | `	/* Point to the target IO stream device */` |
|     13 | 4049 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     13 | 4050 | `	if( pStream == 0 ){` |
|    ! 0 | 4051 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4052 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4053 | `		return PH7_OK;` |
|      - | 4054 | `	}` |
|      - | 4055 | `	/* Allocate a new IO private instance */` |
|     13 | 4056 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     13 | 4057 | `	if( pDev == 0 ){` |
|    ! 0 | 4058 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4059 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4060 | `		return PH7_OK;` |
|      - | 4061 | `	}` |
|      - | 4062 | `	/* Initialize the structure */` |
|     13 | 4063 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     13 | 4064 | `	iFlags = 0;` |
|     13 | 4065 | `	if( nArg > 1 ){` |
|    ! 0 | 4066 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 4067 | `	}` |
|     13 | 4068 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 4069 | `		use_include = TRUE;` |
|    ! 0 | 4070 | `	}` |
|      - | 4071 | `	/* Create the array and the working value */` |
|     13 | 4072 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 4073 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     13 | 4074 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 4075 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4076 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4077 | `		return PH7_OK;` |
|      - | 4078 | `	}` |
|      - | 4079 | `	/* Try to open the file in read-only mode */` |
|     13 | 4080 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|     13 | 4081 | `	if( pDev->pHandle == 0 ){` |
|     10 | 4082 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     10 | 4083 | `		ph7_result_bool(pCtx,0);` |
|      - | 4084 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 4085 | `		 * as soon we return from this function.` |
|      - | 4086 | `		 */` |
|     10 | 4087 | `		return PH7_OK;` |
|      - | 4088 | `	}` |
|      - | 4089 | `	/* Perform the requested operation */` |
|      3 | 4090 | `	for(;;){` |
|      - | 4091 | `		/* Try to extract a line */` |
|      7 | 4092 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 4093 | `		if( n < 1 ){` |
|      - | 4094 | `			/* EOF or IO error */` |
|      3 | 4095 | `			break;` |
|      - | 4096 | `		}` |
|      - | 4097 | `		/* Reset the cursor */` |
|      5 | 4098 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 4099 | `		/* Remove line ending if requested by the caller */` |
|      5 | 4100 | `		zPtr = zBuf;` |
|      5 | 4101 | `		zEnd = &zBuf[n];` |
|      5 | 4102 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 4103 | `			/* Ignore trailig lines */` |
|    ! 0 | 4104 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 4105 | `#ifdef __WINNT__` |
|      - | 4106 | `				\|\| zEnd[-1] == '\r'` |
|      - | 4107 | `#endif` |
|      - | 4108 | `				)){` |
|    ! 0 | 4109 | `					n--;` |
|    ! 0 | 4110 | `					zEnd--;` |
|    ! 0 | 4111 | `			}` |
|    ! 0 | 4112 | `		}` |
|      5 | 4113 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 4114 | `			/* Ignore empty lines */` |
|    ! 0 | 4115 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 4116 | `				zPtr++;` |
|    ! 0 | 4117 | `			}` |
|    ! 0 | 4118 | `			if( zPtr >= zEnd ){` |
|      - | 4119 | `				/* Empty line */` |
|    ! 0 | 4120 | `				continue;` |
|      - | 4121 | `			}` |
|    ! 0 | 4122 | `		}` |
|      5 | 4123 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 4124 | `		/* Insert line */` |
|      5 | 4125 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 4126 | `	}` |
|      - | 4127 | `	/* Close the stream */` |
|      3 | 4128 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4129 | `	/* Release the io_private instance */` |
|      3 | 4130 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4131 | `	/* Return the created array */` |
|      3 | 4132 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4133 | `	return PH7_OK;` |
|      8 | 4134 | `}` |
|      - | 4135 | `/*` |
|      - | 4136 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 4137 | ` *  Makes a copy of the file source to dest.` |
|      - | 4138 | ` * Parameters` |
|      - | 4139 | ` *  $source` |
|      - | 4140 | ` *   Path to the source file.` |
|      - | 4141 | ` *  $dest` |
|      - | 4142 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 4143 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 4144 | ` *  $context` |
|      - | 4145 | ` *   A context stream resource.` |
|      - | 4146 | ` * Return` |
|      - | 4147 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4148 | ` */` |
|      4 | 4149 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4150 | `{` |
|      - | 4151 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 4152 | `	const char *zFile;` |
|      - | 4153 | `	char zBuf[8192];` |
|      - | 4154 | `	void *pIn,*pOut;` |
|      - | 4155 | `	ph7_int64 n;` |
|      - | 4156 | `	int nLen;` |
|      6 | 4157 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 4158 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4159 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 4160 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4161 | `		return PH7_OK;` |
|      - | 4162 | `	}` |
|      - | 4163 | `	/* Extract the source name */` |
|      6 | 4164 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4165 | `	/* Point to the target IO stream device */` |
|      6 | 4166 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4167 | `	if( pSin == 0 ){` |
|    ! 0 | 4168 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4169 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4170 | `		return PH7_OK;` |
|      - | 4171 | `	}` |
|      - | 4172 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4173 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4174 | `	if( pIn == 0 ){` |
|      3 | 4175 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 4176 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4177 | `		return PH7_OK;` |
|      - | 4178 | `	}` |
|      - | 4179 | `	/* Extract the destination name */` |
|      3 | 4180 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4181 | `	/* Point to the target IO stream device */` |
|      3 | 4182 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4183 | `	if( pSout == 0 ){` |
|    ! 0 | 4184 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4185 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4186 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4187 | `		return PH7_OK;` |
|      - | 4188 | `	}` |
|      3 | 4189 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4190 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4191 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4192 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4193 | `			);` |
|    ! 0 | 4194 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4195 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4196 | `		return PH7_OK;` |
|      - | 4197 | `	}` |
|      - | 4198 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4199 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4200 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4201 | `	if( pOut == 0 ){` |
|    ! 0 | 4202 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 4203 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4204 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4205 | `		return PH7_OK;` |
|      - | 4206 | `	}` |
|      - | 4207 | `	/* Perform the requested operation */` |
|      2 | 4208 | `	for(;;){` |
|      - | 4209 | `		/* Read from source */` |
|      5 | 4210 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4211 | `		if( n < 1 ){` |
|      - | 4212 | `			/* EOF or IO error,break immediately */` |
|      3 | 4213 | `			break;` |
|      - | 4214 | `		}` |
|      - | 4215 | `		/* Write to dest */` |
|      3 | 4216 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4217 | `		if( n < 1 ){` |
|      - | 4218 | `			/* IO error,break immediately */` |
|    ! 0 | 4219 | `			break;` |
|      - | 4220 | `		}` |
|      1 | 4221 | `	}` |
|      - | 4222 | `	/* Close the streams */` |
|      3 | 4223 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4224 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4225 | `	/* Return TRUE */` |
|      3 | 4226 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4227 | `	return PH7_OK;` |
|      4 | 4228 | `}` |
|      - | 4229 | `/*` |
|      - | 4230 | ` * array fstat(resource $handle)` |
|      - | 4231 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4232 | ` * Parameters` |
|      - | 4233 | ` *  $handle` |
|      - | 4234 | ` *   The file pointer.` |
|      - | 4235 | ` * Return` |
|      - | 4236 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4237 | ` */` |
|      2 | 4238 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4239 | `{` |
|      - | 4240 | `	ph7_value *pArray,*pValue;` |
|      - | 4241 | `	const ph7_io_stream *pStream;` |
|      - | 4242 | `	io_private *pDev;` |
|      3 | 4243 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4244 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4245 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4246 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4247 | `		return PH7_OK;` |
|      - | 4248 | `	}` |
|      - | 4249 | `	/* Extract our private data */` |
|      3 | 4250 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4251 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4252 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4253 | `		/* Expecting an IO handle */` |
|    ! 0 | 4254 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4255 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4256 | `		return PH7_OK;` |
|      - | 4257 | `	}` |
|      - | 4258 | `	/* Point to the target IO stream device */` |
|      3 | 4259 | `	pStream = pDev->pStream;` |
|      3 | 4260 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4261 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4262 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4263 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4264 | `			);` |
|    ! 0 | 4265 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4266 | `		return PH7_OK;` |
|      - | 4267 | `	}` |
|      - | 4268 | `	/* Create the array and the working value */` |
|      3 | 4269 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4270 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4271 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4272 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4273 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4274 | `		return PH7_OK;` |
|      - | 4275 | `	}` |
|      - | 4276 | `	/* Perform the requested operation */` |
|      3 | 4277 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4278 | `	/* Return the freshly created array */` |
|      3 | 4279 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4280 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4281 | `	 * released automatically as soon we return from this function.` |
|      - | 4282 | `	 */` |
|      3 | 4283 | `	return PH7_OK;` |
|      2 | 4284 | `}` |
|      - | 4285 | `/*` |
|      - | 4286 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4287 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4288 | ` * Parameters` |
|      - | 4289 | ` *  $handle` |
|      - | 4290 | ` *   The file pointer.` |
|      - | 4291 | ` *  $string` |
|      - | 4292 | ` *   The string that is to be written.` |
|      - | 4293 | ` *  $length` |
|      - | 4294 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4295 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4296 | ` * Return` |
|      - | 4297 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4298 | ` */` |
|     22 | 4299 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4300 | `{` |
|      - | 4301 | `	const ph7_io_stream *pStream;` |
|      - | 4302 | `	const char *zString;` |
|      - | 4303 | `	io_private *pDev;` |
|      - | 4304 | `	int nLen,n;` |
|     24 | 4305 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4306 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4307 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4308 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4309 | `		return PH7_OK;` |
|      - | 4310 | `	}` |
|      - | 4311 | `	/* Extract our private data */` |
|     24 | 4312 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4313 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     24 | 4314 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4315 | `		/* Expecting an IO handle */` |
|    ! 0 | 4316 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4317 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4318 | `		return PH7_OK;` |
|      - | 4319 | `	}` |
|      - | 4320 | `	/* Point to the target IO stream device */` |
|     24 | 4321 | `	pStream = pDev->pStream;` |
|     24 | 4322 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4323 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4324 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4325 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4326 | `			);` |
|    ! 0 | 4327 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4328 | `		return PH7_OK;` |
|      - | 4329 | `	}` |
|      - | 4330 | `	/* Extract the data to write */` |
|     24 | 4331 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     24 | 4332 | `	if( nArg > 2 ){` |
|      - | 4333 | `		/* Maximum data length to write */` |
|    ! 0 | 4334 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4335 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4336 | `			nLen = n;` |
|    ! 0 | 4337 | `		}` |
|    ! 0 | 4338 | `	}` |
|     24 | 4339 | `	if( nLen < 1 ){` |
|      - | 4340 | `		/* Nothing to write */` |
|    ! 0 | 4341 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4342 | `		return PH7_OK;` |
|      - | 4343 | `	}` |
|      - | 4344 | `	/* Perform the requested operation */` |
|     24 | 4345 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|     24 | 4346 | `	if( n <  0 ){` |
|      - | 4347 | `		/* IO error,return FALSE */` |
|    ! 0 | 4348 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4349 | `	}else{` |
|      - | 4350 | `		/* #Bytes written */` |
|     24 | 4351 | `		ph7_result_int(pCtx,n);` |
|      - | 4352 | `	}` |
|     24 | 4353 | `	return PH7_OK;` |
|     13 | 4354 | `}` |
|      - | 4355 | `/*` |
|      - | 4356 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4357 | ` *  Portable advisory file locking.` |
|      - | 4358 | ` * Parameters` |
|      - | 4359 | ` *  $handle` |
|      - | 4360 | ` *   The file pointer.` |
|      - | 4361 | ` *  $operation` |
|      - | 4362 | ` *   operation is one of the following:` |
|      - | 4363 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4364 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4365 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4366 | ` * Return` |
|      - | 4367 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4368 | ` */` |
|      4 | 4369 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 4370 | `{` |
|      - | 4371 | `	const ph7_io_stream *pStream;` |
|      - | 4372 | `	io_private *pDev;` |
|      - | 4373 | `	int nLock;` |
|      - | 4374 | `	int rc;` |
|      4 | 4375 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4376 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4377 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4378 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4379 | `		return PH7_OK;` |
|      - | 4380 | `	}` |
|      - | 4381 | `	/* Extract our private data */` |
|      4 | 4382 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4383 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      4 | 4384 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4385 | `		/*Expecting an IO handle */` |
|    ! 0 | 4386 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4387 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4388 | `		return PH7_OK;` |
|      - | 4389 | `	}` |
|      - | 4390 | `	/* Point to the target IO stream device */` |
|      4 | 4391 | `	pStream = pDev->pStream;` |
|      4 | 4392 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4393 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4394 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4395 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4396 | `			);` |
|    ! 0 | 4397 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4398 | `		return PH7_OK;` |
|      - | 4399 | `	}` |
|      - | 4400 | `	/* Requested lock operation */` |
|      4 | 4401 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4402 | `	/*` |
|      - | 4403 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally \|LOCK_NB=4)` |
|      - | 4404 | `	 * into the xLock() vtable contract, which is a PUBLIC C API and stays as it is:` |
|      - | 4405 | `	 * negative = unlock, 1 = exclusive, anything else = shared.` |
|      - | 4406 | `	 */` |
|      - | 4407 | `	{` |
|      4 | 4408 | `		int iOp = nLock & ~4 /* strip LOCK_NB */;` |
|      4 | 4409 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      2 | 4410 | `			nLock = -1;` |
|      3 | 4411 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|      2 | 4412 | `			nLock = 1;` |
|      1 | 4413 | `		}else{` |
|    ! 0 | 4414 | `			nLock = 0; /* LOCK_SH */` |
|      - | 4415 | `		}` |
|      - | 4416 | `	}` |
|      - | 4417 | `	/* Lock operation */` |
|      4 | 4418 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4419 | `	/* IO result */` |
|      4 | 4420 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 4421 | `	return PH7_OK;` |
|      2 | 4422 | `}` |
|      - | 4423 | `/*` |
|      - | 4424 | ` * int fpassthru(resource $handle)` |
|      - | 4425 | ` *  Output all remaining data on a file pointer.` |
|      - | 4426 | ` * Parameters` |
|      - | 4427 | ` *  $handle` |
|      - | 4428 | ` *   The file pointer.` |
|      - | 4429 | ` * Return` |
|      - | 4430 | ` *  Total number of characters read from handle and passed through` |
|      - | 4431 | ` *  to the output on success or FALSE on failure.` |
|      - | 4432 | ` */` |
|      2 | 4433 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4434 | `{` |
|      - | 4435 | `	const ph7_io_stream *pStream;` |
|      - | 4436 | `	io_private *pDev;` |
|      - | 4437 | `	ph7_int64 n,nRead;` |
|      - | 4438 | `	char zBuf[8192];` |
|      - | 4439 | `	int rc;` |
|      3 | 4440 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4441 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4442 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4443 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4444 | `		return PH7_OK;` |
|      - | 4445 | `	}` |
|      - | 4446 | `	/* Extract our private data */` |
|      3 | 4447 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4448 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4449 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4450 | `		/*Expecting an IO handle */` |
|    ! 0 | 4451 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4452 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4453 | `		return PH7_OK;` |
|      - | 4454 | `	}` |
|      - | 4455 | `	/* Point to the target IO stream device */` |
|      3 | 4456 | `	pStream = pDev->pStream;` |
|      3 | 4457 | `	if( pStream == 0  ){` |
|    ! 0 | 4458 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4459 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4460 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4461 | `			);` |
|    ! 0 | 4462 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4463 | `		return PH7_OK;` |
|      - | 4464 | `	}` |
|      - | 4465 | `	/* Perform the requested operation */` |
|      3 | 4466 | `	nRead = 0;` |
|      2 | 4467 | `	for(;;){` |
|      5 | 4468 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4469 | `		if( n < 1 ){` |
|      - | 4470 | `			/* Error or EOF */` |
|      3 | 4471 | `			break;` |
|      - | 4472 | `		}` |
|      - | 4473 | `		/* Increment the read counter */` |
|      3 | 4474 | `		nRead += n;` |
|      - | 4475 | `		/* Output data */` |
|      3 | 4476 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4477 | `		if( rc == PH7_ABORT ){` |
|      - | 4478 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4479 | `			break;` |
|      - | 4480 | `		}` |
|      1 | 4481 | `	}` |
|      - | 4482 | `	/* Total number of bytes readen */` |
|      3 | 4483 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4484 | `	return PH7_OK;` |
|      2 | 4485 | `}` |
|      - | 4486 | `/* CSV reader/writer private data */` |
|      - | 4487 | `struct csv_data` |
|      - | 4488 | `{` |
|      - | 4489 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4490 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4491 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4492 | `	int iCount;       /* Counter */` |
|      - | 4493 | `};` |
|      - | 4494 | `/*` |
|      - | 4495 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4496 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4497 | ` * associated data.` |
|      - | 4498 | ` */` |
|      6 | 4499 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4500 | `{` |
|      7 | 4501 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4502 | `	const char *zData;` |
|      - | 4503 | `	int nLen,c2;` |
|      - | 4504 | `	sxu32 n;` |
|      - | 4505 | `	/* Point to the raw data */` |
|      7 | 4506 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4507 | `	if( nLen < 1 ){` |
|      - | 4508 | `		/* Nothing to write */` |
|    ! 0 | 4509 | `		return PH7_OK;` |
|      - | 4510 | `	}` |
|      7 | 4511 | `	if( pData->iCount > 0 ){` |
|      - | 4512 | `		/* Write the delimiter */` |
|      5 | 4513 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4514 | `	}` |
|      7 | 4515 | `	n = 1;` |
|      7 | 4516 | `	c2 = 0;` |
|     10 | 4517 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4518 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4519 | `			c2 = 1;` |
|    ! 0 | 4520 | `			if( n == 0 ){` |
|    ! 0 | 4521 | `				c2 = 2;` |
|    ! 0 | 4522 | `			}` |
|      - | 4523 | `			/* Write the enclosure */` |
|    ! 0 | 4524 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4525 | `			if( c2 > 1 ){` |
|    ! 0 | 4526 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4527 | `			}` |
|    ! 0 | 4528 | `	}` |
|      - | 4529 | `	/* Write the data */` |
|      7 | 4530 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4531 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4532 | `		return PH7_ABORT;` |
|      - | 4533 | `	}` |
|      7 | 4534 | `	if( c2 > 0 ){` |
|      - | 4535 | `		/* Write the enclosure */` |
|    ! 0 | 4536 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4537 | `		if( c2 > 1 ){` |
|    ! 0 | 4538 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4539 | `		}` |
|    ! 0 | 4540 | `	}` |
|      7 | 4541 | `	pData->iCount++;` |
|      7 | 4542 | `	return PH7_OK;` |
|      4 | 4543 | `}` |
|      - | 4544 | `/*` |
|      - | 4545 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4546 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4547 | ` * Parameters` |
|      - | 4548 | ` *  $handle` |
|      - | 4549 | ` *   Open file handle.` |
|      - | 4550 | ` * $fields` |
|      - | 4551 | ` *   An array of values.` |
|      - | 4552 | ` * $delimiter` |
|      - | 4553 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4554 | ` * $enclosure` |
|      - | 4555 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4556 | ` */` |
|      2 | 4557 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4558 | `{` |
|      - | 4559 | `	const ph7_io_stream *pStream;` |
|      - | 4560 | `	struct csv_data sCsv;` |
|      - | 4561 | `	io_private *pDev;` |
|      - | 4562 | `	char *zEol;` |
|      - | 4563 | `	int eolen;` |
|      3 | 4564 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4565 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4566 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4567 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4568 | `		return PH7_OK;` |
|      - | 4569 | `	}` |
|      - | 4570 | `	/* Extract our private data */` |
|      3 | 4571 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4572 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4573 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4574 | `		/*Expecting an IO handle */` |
|    ! 0 | 4575 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4576 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4577 | `		return PH7_OK;` |
|      - | 4578 | `	}` |
|      - | 4579 | `	/* Point to the target IO stream device */` |
|      3 | 4580 | `	pStream = pDev->pStream;` |
|      3 | 4581 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4582 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4583 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4584 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4585 | `			);` |
|    ! 0 | 4586 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4587 | `		return PH7_OK;` |
|      - | 4588 | `	}` |
|      - | 4589 | `	/* Set default csv separator */` |
|      3 | 4590 | `	sCsv.delimiter = ',';` |
|      3 | 4591 | `	sCsv.enclosure = '"';` |
|      3 | 4592 | `	sCsv.pDev = pDev;` |
|      3 | 4593 | `	sCsv.iCount = 0;` |
|      3 | 4594 | `	if( nArg > 2 ){` |
|      - | 4595 | `		/* User delimiter */` |
|      - | 4596 | `		const char *z;` |
|      - | 4597 | `		int n;` |
|      3 | 4598 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4599 | `		if( n > 0 ){` |
|      3 | 4600 | `			sCsv.delimiter = z[0];` |
|      1 | 4601 | `		}` |
|      3 | 4602 | `		if( nArg > 3 ){` |
|      3 | 4603 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4604 | `			if( n > 0 ){` |
|      3 | 4605 | `				sCsv.enclosure = z[0];` |
|      1 | 4606 | `			}` |
|      1 | 4607 | `		}` |
|      1 | 4608 | `	}` |
|      - | 4609 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4610 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4611 | `	/* Write a line ending */` |
|      - | 4612 | `#ifdef __WINNT__` |
|      1 | 4613 | `	zEol = "\r\n";` |
|      1 | 4614 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4615 | `#else` |
|      - | 4616 | `	/* Assume UNIX LF */` |
|      2 | 4617 | `	zEol = "\n";` |
|      2 | 4618 | `	eolen = (int)sizeof(char);` |
|      - | 4619 | `#endif` |
|      3 | 4620 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4621 | `	return PH7_OK;` |
|      2 | 4622 | `}` |
|      - | 4623 | `/*` |
|      - | 4624 | ` * fprintf,vfprintf private data.` |
|      - | 4625 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4626 | ` * input consumer callback defined below.` |
|      - | 4627 | ` */` |
|      - | 4628 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4629 | `struct fprintf_data` |
|      - | 4630 | `{` |
|      - | 4631 | `	io_private *pIO;        /* IO stream */` |
|      - | 4632 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4633 | `};` |
|      - | 4634 | `/*` |
|      - | 4635 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4636 | ` */` |
|     30 | 4637 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4638 | `{` |
|     31 | 4639 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4640 | `	ph7_int64 n;` |
|      - | 4641 | `	/* Write the formatted data */` |
|     31 | 4642 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     31 | 4643 | `	if( n < 1 ){` |
|    ! 0 | 4644 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4645 | `		/* IO error,abort immediately */` |
|    ! 0 | 4646 | `		return SXERR_ABORT;` |
|      - | 4647 | `	}` |
|      - | 4648 | `	/* Increment counter */` |
|     31 | 4649 | `	pFdata->nCount += n;` |
|     31 | 4650 | `	return PH7_OK;` |
|     16 | 4651 | `}` |
|      - | 4652 | `/*` |
|      - | 4653 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4654 | ` *  Write a formatted string to a stream.` |
|      - | 4655 | ` * Parameters` |
|      - | 4656 | ` *  $handle` |
|      - | 4657 | ` *   The file pointer.` |
|      - | 4658 | ` *  $format` |
|      - | 4659 | ` *   String format (see sprintf()).` |
|      - | 4660 | ` * Return` |
|      - | 4661 | ` *  The length of the written string.` |
|      - | 4662 | ` */` |
|     18 | 4663 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4664 | `{` |
|      - | 4665 | `	fprintf_data sFdata;` |
|      - | 4666 | `	const char *zFormat;` |
|      - | 4667 | `	io_private *pDev;` |
|      - | 4668 | `	int nLen;` |
|     19 | 4669 | `	if( nArg < 2 ){` |
|    ! 0 | 4670 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4671 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4672 | `		return PH7_OK;` |
|      - | 4673 | `	}` |
|      - | 4674 | `	{` |
|      - | 4675 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     19 | 4676 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     19 | 4677 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 4678 | `			return rcs;` |
|      - | 4679 | `		}` |
|      - | 4680 | `	}` |
|      - | 4681 | `	/* Extract our private data */` |
|     19 | 4682 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4683 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     19 | 4684 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4685 | `		/*Expecting an IO handle */` |
|    ! 0 | 4686 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4687 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4688 | `		return PH7_OK;` |
|      - | 4689 | `	}` |
|      - | 4690 | `	/* Point to the target IO stream device */` |
|     19 | 4691 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4692 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4693 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4694 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4695 | `			);` |
|    ! 0 | 4696 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4697 | `		return PH7_OK;` |
|      - | 4698 | `	}` |
|      - | 4699 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 4700 | `	{` |
|     19 | 4701 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     19 | 4702 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4703 | `			return rcf;` |
|      - | 4704 | `		}` |
|      - | 4705 | `	}` |
|      - | 4706 | `	/* Extract the string format (scalars/null coerce). */` |
|     19 | 4707 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     19 | 4708 | `	if( nLen < 1 ){` |
|      - | 4709 | `		/* Empty string,return zero */` |
|    ! 0 | 4710 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4711 | `		return PH7_OK;` |
|      - | 4712 | `	}` |
|      - | 4713 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4714 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4715 | `	{` |
|     19 | 4716 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     19 | 4717 | `		if( rcv != PH7_OK ){` |
|      3 | 4718 | `			return rcv;` |
|      - | 4719 | `		}` |
|      - | 4720 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output.` |
|      - | 4721 | `		 * fprintf's values start at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     17 | 4722 | `		rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     17 | 4723 | `		if( rcv != PH7_OK ){` |
|      3 | 4724 | `			return rcv;` |
|      - | 4725 | `		}` |
|      - | 4726 | `	}` |
|      - | 4727 | `	/* Prepare our private data */` |
|     15 | 4728 | `	sFdata.nCount = 0;` |
|     15 | 4729 | `	sFdata.pIO = pDev;` |
|      - | 4730 | `	/* Format the string */` |
|     15 | 4731 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4732 | `	/* Return total number of bytes written */` |
|     15 | 4733 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     15 | 4734 | `	return PH7_OK;` |
|     10 | 4735 | `}` |
|      - | 4736 | `/*` |
|      - | 4737 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4738 | ` *  Write a formatted string to a stream.` |
|      - | 4739 | ` * Parameters` |
|      - | 4740 | ` *  $handle` |
|      - | 4741 | ` *   The file pointer.` |
|      - | 4742 | ` *  $format` |
|      - | 4743 | ` *   String format (see sprintf()).` |
|      - | 4744 | ` * $args` |
|      - | 4745 | ` *   User arguments.` |
|      - | 4746 | ` * Return` |
|      - | 4747 | ` *  The length of the written string.` |
|      - | 4748 | ` */` |
|      6 | 4749 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4750 | `{` |
|      - | 4751 | `	fprintf_data sFdata;` |
|      - | 4752 | `	const char *zFormat;` |
|      - | 4753 | `	ph7_hashmap *pMap;` |
|      - | 4754 | `	io_private *pDev;` |
|      - | 4755 | `	SySet sArg;` |
|      - | 4756 | `	int n,nLen;` |
|      7 | 4757 | `	if( nArg < 3 ){` |
|    ! 0 | 4758 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4759 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4760 | `		return PH7_OK;` |
|      - | 4761 | `	}` |
|      - | 4762 | `	{` |
|      - | 4763 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|      7 | 4764 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|      7 | 4765 | `		if( rcs != PH7_OK ){` |
|      3 | 4766 | `			return rcs;` |
|      - | 4767 | `		}` |
|      - | 4768 | `	}` |
|      - | 4769 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 4770 | `	{` |
|      5 | 4771 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      5 | 4772 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4773 | `			return rcf;` |
|      - | 4774 | `		}` |
|      - | 4775 | `	}` |
|      5 | 4776 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 4777 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4778 | `		char zBuf[64];` |
|    ! 0 | 4779 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4780 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 4781 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 4782 | `	}` |
|      - | 4783 | `	/* Extract our private data */` |
|      5 | 4784 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4785 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4786 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4787 | `		/*Expecting an IO handle */` |
|    ! 0 | 4788 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4789 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4790 | `		return PH7_OK;` |
|      - | 4791 | `	}` |
|      - | 4792 | `	/* Point to the target IO stream device */` |
|      5 | 4793 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4794 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4795 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4796 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4797 | `			);` |
|    ! 0 | 4798 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4799 | `		return PH7_OK;` |
|      - | 4800 | `	}` |
|      - | 4801 | `	/* Extract the string format */` |
|      5 | 4802 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      5 | 4803 | `	if( nLen < 1 ){` |
|      - | 4804 | `		/* Empty string,return zero */` |
|    ! 0 | 4805 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4806 | `		return PH7_OK;` |
|      - | 4807 | `	}` |
|      - | 4808 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4809 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4810 | `	{` |
|      5 | 4811 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      5 | 4812 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4813 | `			return rcv;` |
|      - | 4814 | `		}` |
|      - | 4815 | `	}` |
|      - | 4816 | `	/* Point to hashmap */` |
|      5 | 4817 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4818 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output. */` |
|      - | 4819 | `	{` |
|      5 | 4820 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      5 | 4821 | `		if( rcc != PH7_OK ){` |
|      3 | 4822 | `			return rcc;` |
|      - | 4823 | `		}` |
|      - | 4824 | `	}` |
|      - | 4825 | `	/* Extract arguments from the hashmap */` |
|      3 | 4826 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4827 | `	/* Prepare our private data */` |
|      3 | 4828 | `	sFdata.nCount = 0;` |
|      3 | 4829 | `	sFdata.pIO = pDev;` |
|      - | 4830 | `	/* Format the string */` |
|      3 | 4831 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4832 | `	/* Return total number of bytes written*/` |
|      3 | 4833 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4834 | `	SySetRelease(&sArg);` |
|      3 | 4835 | `	return PH7_OK;` |
|      4 | 4836 | `}` |
|      - | 4837 | `/*` |
|      - | 4838 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4839 | ` * According to the PHP reference manual:` |
|      - | 4840 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4841 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4842 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4843 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4844 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4845 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4846 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4847 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4848 | ` *         exist, attempt to create it.` |
|      - | 4849 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4850 | ` *          not exist, attempt to create it.` |
|      - | 4851 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4852 | ` *         already exists,` |
|      - | 4853 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4854 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4855 | ` *         the underlying open(2) system call.` |
|      - | 4856 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4857 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4858 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4859 | ` *          is positioned on the beginning of the file.` |
|      - | 4860 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4861 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4862 | ` *          be used after the lock is requested).` |
|      - | 4863 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4864 | ` */` |
|    216 | 4865 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      4 | 4866 | `{` |
|    220 | 4867 | `	const char *zEnd = &zMode[nLen];` |
|    220 | 4868 | `	int iFlag = 0;` |
|      - | 4869 | `	int c;` |
|    220 | 4870 | `	if( nLen < 1 ){` |
|      - | 4871 | `		/* Open in a read-only mode */` |
|    ! 0 | 4872 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4873 | `	}` |
|    220 | 4874 | `	c = zMode[0];` |
|    220 | 4875 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4876 | `		/* Read-only access */` |
|     57 | 4877 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     57 | 4878 | `		zMode++; /* Advance */` |
|     57 | 4879 | `		if( zMode < zEnd ){` |
|     17 | 4880 | `			c = zMode[0];` |
|     17 | 4881 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4882 | `				/* Read+Write access */` |
|     17 | 4883 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      8 | 4884 | `			}` |
|     11 | 4885 | `		}` |
|    193 | 4886 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4887 | `		/* Overwrite mode.` |
|      - | 4888 | `		 * If the file does not exists,try to create it` |
|      - | 4889 | `		 */` |
|     34 | 4890 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     34 | 4891 | `		zMode++; /* Advance */` |
|     34 | 4892 | `		if( zMode < zEnd ){` |
|      5 | 4893 | `			c = zMode[0];` |
|      5 | 4894 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4895 | `				/* Read+Write access */` |
|      5 | 4896 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      5 | 4897 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      2 | 4898 | `			}` |
|      4 | 4899 | `		}` |
|    149 | 4900 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4901 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4902 | `		 * Create the file if it does not exists.` |
|      - | 4903 | `		 */` |
|    ! 0 | 4904 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4905 | `		zMode++; /* Advance */` |
|    ! 0 | 4906 | `		if( zMode < zEnd ){` |
|    ! 0 | 4907 | `			c = zMode[0];` |
|    ! 0 | 4908 | `			if( c == '+' ){` |
|      - | 4909 | `				/* Read-Write access */` |
|    ! 0 | 4910 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4911 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4912 | `			}` |
|    ! 0 | 4913 | `		}` |
|    133 | 4914 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4915 | `		/* Exclusive access.` |
|      - | 4916 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4917 | `		 * Otherwise create a new file.` |
|      - | 4918 | `		 */` |
|    133 | 4919 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    133 | 4920 | `		zMode++; /* Advance */` |
|    133 | 4921 | `		if( zMode < zEnd ){` |
|    ! 0 | 4922 | `			c = zMode[0];` |
|    ! 0 | 4923 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4924 | `				/* Read-Write access */` |
|    ! 0 | 4925 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4926 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4927 | `			}` |
|      3 | 4928 | `		}` |
|     65 | 4929 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4930 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4931 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4932 | `		zMode++; /* Advance */` |
|    ! 0 | 4933 | `		if( zMode < zEnd ){` |
|    ! 0 | 4934 | `			c = zMode[0];` |
|    ! 0 | 4935 | `			if( c == '+' ){` |
|      - | 4936 | `				/* Read-Write access */` |
|    ! 0 | 4937 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4938 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4939 | `			}` |
|    ! 0 | 4940 | `		}` |
|    ! 0 | 4941 | `	}else{` |
|      - | 4942 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4943 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4944 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4945 | `	}` |
|    240 | 4946 | `	while( zMode < zEnd ){` |
|     21 | 4947 | `		c = zMode[0];` |
|     21 | 4948 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4949 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4950 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     21 | 4951 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4952 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4953 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4954 | `		}` |
|     21 | 4955 | `		zMode++;` |
|      1 | 4956 | `	}` |
|    220 | 4957 | `	return iFlag;` |
|    112 | 4958 | `}` |
|      - | 4959 | `/*` |
|      - | 4960 | ` * Initialize the IO private structure.` |
|      - | 4961 | ` */` |
|   5226 | 4962 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 4963 | `{` |
|   5231 | 4964 | `	pOut->pStream = pStream;` |
|   5231 | 4965 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   5231 | 4966 | `	pOut->nOfft = 0;` |
|      - | 4967 | `	/* Set the magic number */` |
|   5231 | 4968 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   5231 | 4969 | `}` |
|      - | 4970 | `/*` |
|      - | 4971 | ` * Release the IO private structure.` |
|      - | 4972 | ` */` |
|      2 | 4973 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      1 | 4974 | `{` |
|      3 | 4975 | `	SyBlobRelease(&pDev->sBuffer);` |
|      3 | 4976 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4977 | `	/* Release the whole structure */` |
|      3 | 4978 | `	ph7_context_free_chunk(pCtx,pDev);` |
|      3 | 4979 | `}` |
|      - | 4980 | `/*` |
|      - | 4981 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 4982 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 4983 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 4984 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 4985 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 4986 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 4987 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 4988 | ` */` |
|   5178 | 4989 | `static void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 4990 | `{` |
|   5183 | 4991 | `	SyBlobRelease(&pDev->sBuffer);` |
|   5183 | 4992 | `	pDev->pHandle = 0;` |
|   5183 | 4993 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   5183 | 4994 | `}` |
|      - | 4995 | `/*` |
|      - | 4996 | ` * Reset the IO private structure.` |
|      - | 4997 | ` */` |
|     30 | 4998 | `static void ResetIOPrivate(io_private *pDev)` |
|      2 | 4999 | `{` |
|     32 | 5000 | `	SyBlobReset(&pDev->sBuffer);` |
|     32 | 5001 | `	pDev->nOfft = 0;` |
|     32 | 5002 | `}` |
|      - | 5003 | `/* Forward declaration */` |
|      - | 5004 |  |
|      - | 5005 | `/*` |
|      - | 5006 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 5007 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 5008 | ` * Parameters` |
|      - | 5009 | ` *  $filename` |
|      - | 5010 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 5011 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 5012 | ` *   then a regular file is assumed.` |
|      - | 5013 | ` *  $mode` |
|      - | 5014 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 5015 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 5016 | ` *   modes.` |
|      - | 5017 | ` *  $use_include_path` |
|      - | 5018 | ` *   You can use the optional second parameter and set it to` |
|      - | 5019 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 5020 | ` *  $context` |
|      - | 5021 | ` *   A context stream resource.` |
|      - | 5022 | ` * Return` |
|      - | 5023 | ` *  File handle on success or FALSE on failure.` |
|      - | 5024 | ` */` |
|      - | 5025 | `/*` |
|      - | 5026 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 5027 | ` *                                  int $offset = -1)` |
|      - | 5028 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 5029 | ` */` |
|     10 | 5030 | `static int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5031 | `{` |
|      - | 5032 | `	const ph7_io_stream *pStream;` |
|      - | 5033 | `	io_private *pDev;` |
|     11 | 5034 | `	ph7_int64 nMax = -1;` |
|      - | 5035 | `	char zBuf[4096];` |
|      - | 5036 | `	ph7_int64 nRead;` |
|     11 | 5037 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 5038 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5039 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5040 | `		return PH7_OK;` |
|      - | 5041 | `	}` |
|     11 | 5042 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     11 | 5043 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5044 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5045 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5046 | `		return PH7_OK;` |
|      - | 5047 | `	}` |
|     11 | 5048 | `	pStream = pDev->pStream;` |
|     11 | 5049 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 5050 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5051 | `		return PH7_OK;` |
|      - | 5052 | `	}` |
|     11 | 5053 | `	if( nArg > 1 ){` |
|      5 | 5054 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      2 | 5055 | `	}` |
|     11 | 5056 | `	if( nArg > 2 ){` |
|      5 | 5057 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 5058 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 5059 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 5060 | `		}` |
|      2 | 5061 | `	}` |
|     11 | 5062 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     21 | 5063 | `	while( nMax != 0 ){` |
|     19 | 5064 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     19 | 5065 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 5066 | `			nAsk = nMax;` |
|      1 | 5067 | `		}` |
|     19 | 5068 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     19 | 5069 | `		if( nRead < 1 ){` |
|      9 | 5070 | `			break;` |
|      - | 5071 | `		}` |
|     11 | 5072 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     11 | 5073 | `		if( nMax > 0 ){` |
|      3 | 5074 | `			nMax -= nRead;` |
|      1 | 5075 | `		}` |
|      1 | 5076 | `	}` |
|     11 | 5077 | `	return PH7_OK;` |
|      6 | 5078 | `}` |
|      - | 5079 | `/*` |
|      - | 5080 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 5081 | ` */` |
|      4 | 5082 | `static int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5083 | `{` |
|      - | 5084 | `	ph7_value *pArr,*pV;` |
|      - | 5085 | `	ph7_io_stream **apDev;` |
|      - | 5086 | `	sxu32 n;` |
|      2 | 5087 | `	SXUNUSED(nArg);` |
|      2 | 5088 | `	SXUNUSED(apArg);` |
|      6 | 5089 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 5090 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 5091 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 5092 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5093 | `		return PH7_OK;` |
|      - | 5094 | `	}` |
|      6 | 5095 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 5096 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 5097 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 5098 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 5099 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 5100 | `	}` |
|      6 | 5101 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 5102 | `	return PH7_OK;` |
|      4 | 5103 | `}` |
|      - | 5104 | `/*` |
|      - | 5105 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 5106 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 5107 | ` */` |
|      2 | 5108 | `static int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5109 | `{` |
|      - | 5110 | `	io_private *pDev;` |
|      - | 5111 | `	ph7_value *pArr,*pV;` |
|      3 | 5112 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 5113 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5114 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5115 | `		return PH7_OK;` |
|      - | 5116 | `	}` |
|      3 | 5117 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5118 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5119 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5120 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5121 | `		return PH7_OK;` |
|      - | 5122 | `	}` |
|      3 | 5123 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 5124 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 5125 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 5126 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5127 | `		return PH7_OK;` |
|      - | 5128 | `	}` |
|      3 | 5129 | `	ph7_value_bool(pV,0);` |
|      3 | 5130 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 5131 | `	ph7_value_bool(pV,1);` |
|      3 | 5132 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 5133 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 5134 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 5135 | `	ph7_value_bool(pV,0);` |
|      3 | 5136 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 5137 | `	ph7_value_int(pV,0);` |
|      3 | 5138 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 5139 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 5140 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 5141 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 5142 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 5143 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 5144 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 5145 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 5146 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 5147 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 5148 | `	return PH7_OK;` |
|      2 | 5149 | `}` |
|      - | 5150 | `/*` |
|      - | 5151 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 5152 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 5153 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 5154 | ` * resource, options unconsumed).` |
|      - | 5155 | ` */` |
|      2 | 5156 | `static int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5157 | `{` |
|      3 | 5158 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 5159 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 5160 | `	}else{` |
|    ! 0 | 5161 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 5162 | `		if( pArr == 0 ){` |
|    ! 0 | 5163 | `			ph7_result_null(pCtx);` |
|    ! 0 | 5164 | `			return PH7_OK;` |
|      - | 5165 | `		}` |
|    ! 0 | 5166 | `		ph7_result_value(pCtx,pArr);` |
|      - | 5167 | `	}` |
|      3 | 5168 | `	return PH7_OK;` |
|      2 | 5169 | `}` |
|      - | 5170 | `/*` |
|      - | 5171 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 5172 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 5173 | ` */` |
|      - | 5174 | `#ifdef PH7_ENABLE_NET` |
|      - | 5175 | `typedef struct sock_private sock_private;` |
|      - | 5176 | `struct sock_private` |
|      - | 5177 | `{` |
|      - | 5178 | `	ph7_vm *pVm;` |
|      - | 5179 | `	ph7_socket sock;` |
|      - | 5180 | `	int bEof;` |
|      - | 5181 | `};` |
|     10 | 5182 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 5183 | `{` |
|     10 | 5184 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5185 | `	int n;` |
|     10 | 5186 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      2 | 5187 | `		return 0;` |
|      - | 5188 | `	}` |
|      8 | 5189 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|      8 | 5190 | `	if( n <= 0 ){` |
|      4 | 5191 | `		pSock->bEof = 1;` |
|      4 | 5192 | `		return 0;` |
|      - | 5193 | `	}` |
|      4 | 5194 | `	return (ph7_int64)n;` |
|      5 | 5195 | `}` |
|      4 | 5196 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 5197 | `{` |
|      4 | 5198 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5199 | `	int n;` |
|      4 | 5200 | `	if( pSock == 0 ){` |
|    ! 0 | 5201 | `		return -1;` |
|      - | 5202 | `	}` |
|      4 | 5203 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 5204 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 5205 | `}` |
|      4 | 5206 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 5207 | `{` |
|      4 | 5208 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 5209 | `	if( pSock == 0 ){` |
|    ! 0 | 5210 | `		return;` |
|      - | 5211 | `	}` |
|      4 | 5212 | `	PH7_NetClose(pSock->sock);` |
|      4 | 5213 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 5214 | `}` |
|      - | 5215 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 5216 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 5217 | `{` |
|      - | 5218 | `	sock_private *pSock;` |
|      - | 5219 | `	ph7_socket sock;` |
|      - | 5220 | `	char zHost[256];` |
|      - | 5221 | `	const char *zColon;` |
|    ! 0 | 5222 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 5223 | `	const char *zErr = "";` |
|    ! 0 | 5224 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 5225 | `	SXUNUSED(iMode);` |
|    ! 0 | 5226 | `	if( pVm == 0 ){` |
|    ! 0 | 5227 | `		return -1;` |
|      - | 5228 | `	}` |
|    ! 0 | 5229 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 5230 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 5231 | `		zColon--;` |
|    ! 0 | 5232 | `	}` |
|    ! 0 | 5233 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 5234 | `		return -1;` |
|      - | 5235 | `	}` |
|      - | 5236 | `	{` |
|    ! 0 | 5237 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 5238 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 5239 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 5240 | `		}` |
|    ! 0 | 5241 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 5242 | `		zHost[n] = 0;` |
|      - | 5243 | `	}` |
|      - | 5244 | `	{` |
|    ! 0 | 5245 | `		sxi32 iTmp = 0;` |
|    ! 0 | 5246 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 5247 | `		iPort = (int)iTmp;` |
|      - | 5248 | `	}` |
|    ! 0 | 5249 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 5250 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 5251 | `		return -1;` |
|      - | 5252 | `	}` |
|    ! 0 | 5253 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 5254 | `	if( pSock == 0 ){` |
|    ! 0 | 5255 | `		PH7_NetClose(sock);` |
|    ! 0 | 5256 | `		return -1;` |
|      - | 5257 | `	}` |
|    ! 0 | 5258 | `	pSock->pVm = pVm;` |
|    ! 0 | 5259 | `	pSock->sock = sock;` |
|    ! 0 | 5260 | `	pSock->bEof = 0;` |
|    ! 0 | 5261 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 5262 | `	return PH7_OK;` |
|    ! 0 | 5263 | `}` |
|      - | 5264 | `static const ph7_io_stream sTCP_Stream = {` |
|      - | 5265 | `	"tcp",` |
|      - | 5266 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5267 | `	SockStreamData_Open, /* xOpen */` |
|      - | 5268 | `	0,   /* xOpenDir */` |
|      - | 5269 | `	SockStreamData_Close,/* xClose */` |
|      - | 5270 | `	0,  /* xCloseDir */` |
|      - | 5271 | `	SockStreamData_Read, /* xRead */` |
|      - | 5272 | `	0,  /* xReadDir */` |
|      - | 5273 | `	SockStreamData_Write,/* xWrite */` |
|      - | 5274 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 5275 | `	0,  /* xLock */` |
|      - | 5276 | `	0,  /* xRewindDir */` |
|      - | 5277 | `	0,  /* xTell */` |
|      - | 5278 | `	0,  /* xTrunc */` |
|      - | 5279 | `	0,  /* xSync */` |
|      - | 5280 | `	0   /* xStat */` |
|      - | 5281 | `};` |
|      - | 5282 | `#endif /* PH7_ENABLE_NET */` |
|      - | 5283 | `/*` |
|      - | 5284 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 5285 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 5286 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 5287 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 5288 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 5289 | ` * php streamWrapper protocol method on it.` |
|      - | 5290 | ` */` |
|      - | 5291 | `#define PHL_UWRAP_MAX 8` |
|      - | 5292 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 5293 | `struct uwrap_slot` |
|      - | 5294 | `{` |
|      - | 5295 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 5296 | `	char zScheme[32];         /* protocol name */` |
|      - | 5297 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 5298 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 5299 | `};` |
|      - | 5300 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 5301 | `struct uwrap_handle` |
|      - | 5302 | `{` |
|      - | 5303 | `	ph7_vm *pVm;` |
|      - | 5304 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 5305 | `	int iSlot;` |
|      - | 5306 | `	int bEof;` |
|      - | 5307 | `};` |
|      - | 5308 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 5309 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 5310 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 5311 | `	ph7_value *pResult)` |
|      1 | 5312 | `{` |
|      - | 5313 | `	ph7_class_method *pMeth;` |
|     27 | 5314 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 5315 | `		return -1;` |
|      - | 5316 | `	}` |
|     27 | 5317 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 5318 | `	if( pMeth == 0 ){` |
|    ! 0 | 5319 | `		return -1;` |
|      - | 5320 | `	}` |
|     27 | 5321 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 5322 | `		return -1;` |
|      - | 5323 | `	}` |
|     27 | 5324 | `	return 0;` |
|     14 | 5325 | `}` |
|      8 | 5326 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 5327 | `{` |
|      9 | 5328 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5329 | `	ph7_value sArg,sRet;` |
|      - | 5330 | `	const char *zData;` |
|      9 | 5331 | `	int nData = 0;` |
|      9 | 5332 | `	ph7_int64 n = 0;` |
|      9 | 5333 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 5334 | `		return 0;` |
|      - | 5335 | `	}` |
|      9 | 5336 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 5337 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 5338 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 5339 | `	{` |
|      - | 5340 | `		ph7_value *apArg[1];` |
|      9 | 5341 | `		apArg[0] = &sArg;` |
|      9 | 5342 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5343 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5344 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5345 | `			return -1;` |
|      - | 5346 | `		}` |
|      - | 5347 | `	}` |
|      9 | 5348 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 5349 | `	if( nData > 0 ){` |
|      7 | 5350 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 5351 | `			nData = (int)nRead;` |
|    ! 0 | 5352 | `		}` |
|      7 | 5353 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 5354 | `		n = nData;` |
|      4 | 5355 | `	}else{` |
|      3 | 5356 | `		pH->bEof = 1;` |
|      - | 5357 | `	}` |
|      9 | 5358 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 5359 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 5360 | `	return n;` |
|      5 | 5361 | `}` |
|      2 | 5362 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5363 | `{` |
|      3 | 5364 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5365 | `	ph7_value sArg,sRet;` |
|      - | 5366 | `	ph7_int64 n;` |
|      3 | 5367 | `	if( pH == 0 ){` |
|    ! 0 | 5368 | `		return -1;` |
|      - | 5369 | `	}` |
|      3 | 5370 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 5371 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5372 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 5373 | `	{` |
|      - | 5374 | `		ph7_value *apArg[1];` |
|      3 | 5375 | `		apArg[0] = &sArg;` |
|      3 | 5376 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5377 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5378 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5379 | `			return -1;` |
|      - | 5380 | `		}` |
|      - | 5381 | `	}` |
|      3 | 5382 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5383 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 5384 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5385 | `	return n;` |
|      2 | 5386 | `}` |
|      2 | 5387 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5388 | `{` |
|      3 | 5389 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5390 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 5391 | `	ph7_value *apArg[2];` |
|      - | 5392 | `	int rc;` |
|      3 | 5393 | `	if( pH == 0 ){` |
|    ! 0 | 5394 | `		return -1;` |
|      - | 5395 | `	}` |
|      3 | 5396 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 5397 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 5398 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5399 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 5400 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 5401 | `	apArg[0] = &sOfft;` |
|      3 | 5402 | `	apArg[1] = &sWhence;` |
|      3 | 5403 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 5404 | `	if( rc == 0 ){` |
|      3 | 5405 | `		pH->bEof = 0;` |
|      3 | 5406 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 5407 | `	}` |
|      3 | 5408 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 5409 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 5410 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5411 | `	return rc;` |
|      2 | 5412 | `}` |
|      2 | 5413 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 5414 | `{` |
|      3 | 5415 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5416 | `	ph7_value sRet;` |
|      - | 5417 | `	ph7_int64 n;` |
|      3 | 5418 | `	if( pH == 0 ){` |
|    ! 0 | 5419 | `		return -1;` |
|      - | 5420 | `	}` |
|      3 | 5421 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5422 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 5423 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5424 | `		return -1;` |
|      - | 5425 | `	}` |
|      3 | 5426 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5427 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5428 | `	return n;` |
|      2 | 5429 | `}` |
|      6 | 5430 | `static void UwrapClose(void *pHandle)` |
|      1 | 5431 | `{` |
|      7 | 5432 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 5433 | `	if( pH == 0 ){` |
|    ! 0 | 5434 | `		return;` |
|      - | 5435 | `	}` |
|      7 | 5436 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 5437 | `	if( pH->pObj ){` |
|      7 | 5438 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 5439 | `	}` |
|      7 | 5440 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 5441 | `}` |
|      - | 5442 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 5443 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 5444 | `{` |
|      7 | 5445 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 5446 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 5447 | `	ph7_class *pClass;` |
|      - | 5448 | `	uwrap_handle *pH;` |
|      - | 5449 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 5450 | `	ph7_value *apArg[4];` |
|      - | 5451 | `	int rc;` |
|      7 | 5452 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 5453 | `		return -1;` |
|      - | 5454 | `	}` |
|      7 | 5455 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 5456 | `	if( pClass == 0 ){` |
|    ! 0 | 5457 | `		return -1;` |
|      - | 5458 | `	}` |
|      7 | 5459 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 5460 | `	if( pH == 0 ){` |
|    ! 0 | 5461 | `		return -1;` |
|      - | 5462 | `	}` |
|      7 | 5463 | `	pH->pVm = pVm;` |
|      7 | 5464 | `	pH->iSlot = iSlot;` |
|      7 | 5465 | `	pH->bEof = 0;` |
|      7 | 5466 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 5467 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 5468 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5469 | `		return -1;` |
|      - | 5470 | `	}` |
|      - | 5471 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 5472 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 5473 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 5474 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 5475 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 5476 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 5477 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 5478 | `	 * it as "could not be passed by reference"). */` |
|      - | 5479 | `	{` |
|      7 | 5480 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 5481 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 5482 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5483 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5484 | `			return -1;` |
|      - | 5485 | `		}` |
|      7 | 5486 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 5487 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 5488 | `	}` |
|      - | 5489 | `	{` |
|      - | 5490 | `		SyBlob sUrl;` |
|      7 | 5491 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 5492 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 5493 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 5494 | `		SyBlobRelease(&sUrl);` |
|      - | 5495 | `	}` |
|      9 | 5496 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 5497 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 5498 | `	ph7_value_int(&sOpts,0);` |
|      7 | 5499 | `	apArg[0] = &sPath;` |
|      7 | 5500 | `	apArg[1] = &sMode;` |
|      7 | 5501 | `	apArg[2] = &sOpts;` |
|      7 | 5502 | `	apArg[3] = &sOpened;` |
|      7 | 5503 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 5504 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 5505 | `		rc = -1;` |
|    ! 0 | 5506 | `	}` |
|      7 | 5507 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 5508 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 5509 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 5510 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 5511 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 5512 | `	if( rc != 0 ){` |
|    ! 0 | 5513 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5514 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5515 | `		return -1;` |
|      - | 5516 | `	}` |
|      7 | 5517 | `	*ppHandle = (void *)pH;` |
|      7 | 5518 | `	return PH7_OK;` |
|      4 | 5519 | `}` |
|      - | 5520 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 5521 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 5522 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 5523 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 5524 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 5525 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 5526 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 5527 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 5528 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 5529 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 5530 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 5531 | `PHL_UWRAP_THUNK(7)` |
|      - | 5532 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 5533 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 5534 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 5535 | `};` |
|      - | 5536 | `/*` |
|      - | 5537 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 5538 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5539 | ` */` |
|      2 | 5540 | `static int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5541 | `{` |
|      - | 5542 | `	const char *zScheme,*zClass;` |
|      3 | 5543 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 5544 | `	if( nArg < 2 ){` |
|    ! 0 | 5545 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5546 | `		return PH7_OK;` |
|      - | 5547 | `	}` |
|      3 | 5548 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5549 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 5550 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 5551 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 5552 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5553 | `		return PH7_OK;` |
|      - | 5554 | `	}` |
|      - | 5555 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 5556 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 5557 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 5558 | `	{` |
|      3 | 5559 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 5560 | `		sxu32 n;` |
|     11 | 5561 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 5562 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 5563 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 5564 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5565 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 5566 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5567 | `				return PH7_OK;` |
|      - | 5568 | `			}` |
|      5 | 5569 | `		}` |
|      - | 5570 | `	}` |
|      3 | 5571 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 5572 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 5573 | `			iFree = i;` |
|      3 | 5574 | `			break;` |
|      - | 5575 | `		}` |
|    ! 0 | 5576 | `	}` |
|      3 | 5577 | `	if( iFree < 0 ){` |
|    ! 0 | 5578 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5579 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 5580 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5581 | `		return PH7_OK;` |
|      - | 5582 | `	}` |
|      - | 5583 | `	{` |
|      3 | 5584 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 5585 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 5586 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 5587 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 5588 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 5589 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 5590 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 5591 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 5592 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 5593 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 5594 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 5595 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 5596 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 5597 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 5598 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 5599 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 5600 | `	}` |
|      3 | 5601 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5602 | `	return PH7_OK;` |
|      2 | 5603 | `}` |
|      2 | 5604 | `static int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5605 | `{` |
|      - | 5606 | `	const char *zScheme;` |
|      - | 5607 | `	int nScheme,i;` |
|      3 | 5608 | `	if( nArg < 1 ){` |
|    ! 0 | 5609 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5610 | `		return PH7_OK;` |
|      - | 5611 | `	}` |
|      3 | 5612 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5613 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 5614 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 5615 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 5616 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 5617 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 5618 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 5619 | `			 * what unregister means to a script — recorded. */` |
|      3 | 5620 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 5621 | `			ph7_result_bool(pCtx,1);` |
|      3 | 5622 | `			return PH7_OK;` |
|      - | 5623 | `		}` |
|    ! 0 | 5624 | `	}` |
|    ! 0 | 5625 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 5626 | `	return PH7_OK;` |
|      2 | 5627 | `}` |
|      - | 5628 | `#ifdef PH7_ENABLE_NET` |
|      - | 5629 | `/*` |
|      - | 5630 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 5631 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 5632 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 5633 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 5634 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 5635 | ` */` |
|      6 | 5636 | `static int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 5637 | `{` |
|      6 | 5638 | `	const char *zFunc = ph7_function_name(pCtx);` |
|      6 | 5639 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|      6 | 5640 | `	const char *zTarget,*zErr = "";` |
|      - | 5641 | `	char zHost[256];` |
|      6 | 5642 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 5643 | `	ph7_socket sock;` |
|      - | 5644 | `	io_private *pDev;` |
|      - | 5645 | `	sock_private *pSock;` |
|      6 | 5646 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|      6 | 5647 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|      6 | 5648 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|      6 | 5649 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 5650 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5651 | `		return PH7_OK;` |
|      - | 5652 | `	}` |
|      6 | 5653 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 5654 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 5655 | `	{` |
|      6 | 5656 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|      6 | 5657 | `		const char *zSep = 0;` |
|     32 | 5658 | `		while( z < zEnd - 2 ){` |
|     30 | 5659 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 5660 | `				zSep = z;` |
|      4 | 5661 | `				break;` |
|      - | 5662 | `			}` |
|     26 | 5663 | `			z++;` |
|    ! 0 | 5664 | `		}` |
|      6 | 5665 | `		if( zSep ){` |
|      4 | 5666 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 5667 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5668 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 5669 | `					nTarget,zTarget);` |
|    ! 0 | 5670 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5671 | `				return PH7_OK;` |
|      - | 5672 | `			}` |
|      4 | 5673 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 5674 | `			zTarget = zSep + 3;` |
|      2 | 5675 | `		}` |
|      - | 5676 | `	}` |
|      - | 5677 | `	/* host[:port] */` |
|      - | 5678 | `	{` |
|      6 | 5679 | `		int i = nTarget - 1;` |
|      6 | 5680 | `		int nHost = nTarget;` |
|     48 | 5681 | `		while( i > 0 && zTarget[i] != ':' ){` |
|     42 | 5682 | `			i--;` |
|    ! 0 | 5683 | `		}` |
|      6 | 5684 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 5685 | `			sxi32 iTmp = 0;` |
|      2 | 5686 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 5687 | `			iPort = (int)iTmp;` |
|      2 | 5688 | `			nHost = i;` |
|      1 | 5689 | `		}` |
|      6 | 5690 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 5691 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 5692 | `		}` |
|      6 | 5693 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|      6 | 5694 | `		zHost[nHost] = 0;` |
|      - | 5695 | `	}` |
|      6 | 5696 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      4 | 5697 | `		iPort = ph7_value_to_int(apArg[1]);` |
|      2 | 5698 | `	}` |
|      6 | 5699 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|      6 | 5700 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|      6 | 5701 | `		if( rTimeout > 0 ){` |
|      6 | 5702 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|      3 | 5703 | `		}` |
|      3 | 5704 | `	}` |
|      6 | 5705 | `	if( iPort < 0 ){` |
|    ! 0 | 5706 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5707 | `		return PH7_OK;` |
|      - | 5708 | `	}` |
|      6 | 5709 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|      6 | 5710 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5711 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 5712 | `		{` |
|      2 | 5713 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      2 | 5714 | `			if( pTmp ){` |
|      2 | 5715 | `				if( nArg > iArgErrno ){` |
|      2 | 5716 | `					ph7_value_int(pTmp,iErrno);` |
|      2 | 5717 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      1 | 5718 | `				}` |
|      2 | 5719 | `				if( nArg > iArgErrstr ){` |
|      2 | 5720 | `					ph7_value_string(pTmp,zErr,-1);` |
|      2 | 5721 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      1 | 5722 | `				}` |
|      1 | 5723 | `			}` |
|      - | 5724 | `		}` |
|      - | 5725 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|      3 | 5726 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 5727 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|      2 | 5728 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5729 | `		return PH7_OK;` |
|      - | 5730 | `	}` |
|      - | 5731 | `	{` |
|      4 | 5732 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 5733 | `		if( pTmp ){` |
|      4 | 5734 | `			if( nArg > iArgErrno ){` |
|      4 | 5735 | `				ph7_value_int(pTmp,0);` |
|      4 | 5736 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 5737 | `			}` |
|      4 | 5738 | `			if( nArg > iArgErrstr ){` |
|      4 | 5739 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 5740 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 5741 | `			}` |
|      2 | 5742 | `		}` |
|      - | 5743 | `	}` |
|      - | 5744 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 5745 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 5746 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 5747 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 5748 | `		PH7_NetClose(sock);` |
|    ! 0 | 5749 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5750 | `		return PH7_OK;` |
|      - | 5751 | `	}` |
|      4 | 5752 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 5753 | `	pSock->sock = sock;` |
|      4 | 5754 | `	pSock->bEof = 0;` |
|      4 | 5755 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 5756 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 5757 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 5758 | `	return PH7_OK;` |
|      3 | 5759 | `}` |
|      - | 5760 | `#endif /* PH7_ENABLE_NET */` |
|    216 | 5761 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5762 | `{` |
|      - | 5763 | `	const ph7_io_stream *pStream;` |
|      - | 5764 | `	const char *zUri,*zMode;` |
|      - | 5765 | `	ph7_value *pResource;` |
|      - | 5766 | `	io_private *pDev;` |
|      - | 5767 | `	int iLen,imLen;` |
|      - | 5768 | `	int iOpenFlags;` |
|    220 | 5769 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5770 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5771 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 5772 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5773 | `		return PH7_OK;` |
|      - | 5774 | `	}` |
|      - | 5775 | `	/* Extract the URI and the desired access mode */` |
|    220 | 5776 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    220 | 5777 | `	if( nArg > 1 ){` |
|    220 | 5778 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    112 | 5779 | `	}else{` |
|      - | 5780 | `		/* Set a default read-only mode */` |
|    ! 0 | 5781 | `		zMode = "r";` |
|    ! 0 | 5782 | `		imLen = (int)sizeof(char);` |
|      - | 5783 | `	}` |
|      - | 5784 | `	/* Try to extract a stream */` |
|    220 | 5785 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    220 | 5786 | `	if( pStream == 0 ){` |
|    ! 0 | 5787 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5788 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 5789 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5790 | `		return PH7_OK;` |
|      - | 5791 | `	}` |
|      - | 5792 | `	/* Allocate a new IO private instance */` |
|    220 | 5793 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    220 | 5794 | `	if( pDev == 0 ){` |
|    ! 0 | 5795 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5796 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5797 | `		return PH7_OK;` |
|      - | 5798 | `	}` |
|    220 | 5799 | `	pResource = 0;` |
|    220 | 5800 | `	if( nArg > 3 ){` |
|    ! 0 | 5801 | `		pResource = apArg[3];` |
|    220 | 5802 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 5803 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 5804 | `		 * access the underlying virtual machine.` |
|      - | 5805 | `		 */` |
|     19 | 5806 | `		pResource = apArg[0];` |
|      9 | 5807 | `	}` |
|      - | 5808 | `	/* Initialize the structure */` |
|    220 | 5809 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 5810 | `	/* Convert open mode to PH7 flags */` |
|    220 | 5811 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 5812 | `	/* Try to get a handle */` |
|    328 | 5813 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    108 | 5814 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|    220 | 5815 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 5816 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|    ! 0 | 5817 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5818 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 5819 | `		return PH7_OK;` |
|      - | 5820 | `	}` |
|      - | 5821 | `	/* All done,return the io_private instance as a resource */` |
|    220 | 5822 | `	ph7_result_resource(pCtx,pDev);` |
|    220 | 5823 | `	return PH7_OK;` |
|    112 | 5824 | `}` |
|      - | 5825 | `/*` |
|      - | 5826 | ` * bool fclose(resource $handle)` |
|      - | 5827 | ` *  Closes an open file pointer` |
|      - | 5828 | ` * Parameters` |
|      - | 5829 | ` *  $handle` |
|      - | 5830 | ` *   The file pointer.` |
|      - | 5831 | ` * Return` |
|      - | 5832 | ` *  TRUE on success or FALSE on failure.` |
|      - | 5833 | ` */` |
|    316 | 5834 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5835 | `{` |
|      - | 5836 | `	const ph7_io_stream *pStream;` |
|      - | 5837 | `	io_private *pDev;` |
|      - | 5838 | `	ph7_vm *pVm;` |
|    321 | 5839 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5840 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5841 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5842 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5843 | `		return PH7_OK;` |
|      - | 5844 | `	}` |
|      - | 5845 | `	/* Extract our private data */` |
|    321 | 5846 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5847 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|    321 | 5848 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 5849 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5850 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 5851 | `	}` |
|      - | 5852 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    319 | 5853 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 5854 | `		/*Expecting an IO handle */` |
|    ! 0 | 5855 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5856 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5857 | `		return PH7_OK;` |
|      - | 5858 | `	}` |
|      - | 5859 | `	/* Point to the target IO stream device */` |
|    319 | 5860 | `	pStream = pDev->pStream;` |
|    319 | 5861 | `	if( pStream == 0 ){` |
|    ! 0 | 5862 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5863 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 5864 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 5865 | `			);` |
|    ! 0 | 5866 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5867 | `		return PH7_OK;` |
|      - | 5868 | `	}` |
|      - | 5869 | `	/* Point to the VM that own this context */` |
|    319 | 5870 | `	pVm = pCtx->pVm;` |
|      - | 5871 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    319 | 5872 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 5873 | `		/* Perform the requested operation */` |
|    319 | 5874 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 5875 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|    319 | 5876 | `		MarkIOPrivateClosed(pDev);` |
|    157 | 5877 | `	}` |
|      - | 5878 | `	/* Return TRUE */` |
|    319 | 5879 | `	ph7_result_bool(pCtx,1);` |
|    319 | 5880 | `	return PH7_OK;` |
|    163 | 5881 | `}` |
|      - | 5882 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5883 | `/*` |
|      - | 5884 | ` * MD5/SHA1 digest consumer.` |
|      - | 5885 | ` */` |
|     72 | 5886 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 5887 | `{` |
|      - | 5888 | `	/* Append hex chunk verbatim */` |
|     73 | 5889 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 5890 | `	return SXRET_OK;` |
|      1 | 5891 | `}` |
|      - | 5892 | `/*` |
|      - | 5893 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 5894 | ` *  Calculates the md5 hash of a given file.` |
|      - | 5895 | ` * Parameters` |
|      - | 5896 | ` *  $uri` |
|      - | 5897 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5898 | ` *  $raw_output` |
|      - | 5899 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 5900 | ` * Return` |
|      - | 5901 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 5902 | ` */` |
|      2 | 5903 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5904 | `{` |
|      - | 5905 | `	const ph7_io_stream *pStream;` |
|      - | 5906 | `	unsigned char zDigest[16];` |
|      3 | 5907 | `	int raw_output  = FALSE;` |
|      - | 5908 | `	const char *zFile;` |
|      - | 5909 | `	MD5Context sCtx;` |
|      - | 5910 | `	char zBuf[8192];` |
|      - | 5911 | `	void *pHandle;` |
|      - | 5912 | `	ph7_int64 n;` |
|      - | 5913 | `	int nLen;` |
|      3 | 5914 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5915 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5916 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5917 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5918 | `		return PH7_OK;` |
|      - | 5919 | `	}` |
|      - | 5920 | `	/* Extract the file path */` |
|      3 | 5921 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5922 | `	/* Point to the target IO stream device */` |
|      3 | 5923 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5924 | `	if( pStream == 0 ){` |
|    ! 0 | 5925 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5927 | `		return PH7_OK;` |
|      - | 5928 | `	}` |
|      3 | 5929 | `	if( nArg > 1 ){` |
|    ! 0 | 5930 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5931 | `	}` |
|      - | 5932 | `	/* Try to open the file in read-only mode */` |
|      3 | 5933 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5934 | `	if( pHandle == 0 ){` |
|    ! 0 | 5935 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 5936 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5937 | `		return PH7_OK;` |
|      - | 5938 | `	}` |
|      - | 5939 | `	/* Init the MD5 context */` |
|      3 | 5940 | `	MD5Init(&sCtx);` |
|      - | 5941 | `	/* Perform the requested operation */` |
|      2 | 5942 | `	for(;;){` |
|      5 | 5943 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5944 | `		if( n < 1 ){` |
|      - | 5945 | `			/* EOF or IO error,break immediately */` |
|      3 | 5946 | `			break;` |
|      - | 5947 | `		}` |
|      3 | 5948 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5949 | `	}` |
|      - | 5950 | `	/* Close the stream */` |
|      3 | 5951 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5952 | `	/* Extract the digest */` |
|      3 | 5953 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 5954 | `	if( raw_output ){` |
|      - | 5955 | `		/* Output raw digest */` |
|    ! 0 | 5956 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5957 | `	}else{` |
|      - | 5958 | `		/* Perform a binary to hex conversion */` |
|      3 | 5959 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5960 | `	}` |
|      3 | 5961 | `	return PH7_OK;` |
|      2 | 5962 | `}` |
|      - | 5963 | `/*` |
|      - | 5964 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 5965 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 5966 | ` * Parameters` |
|      - | 5967 | ` *  $uri` |
|      - | 5968 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5969 | ` *  $raw_output` |
|      - | 5970 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 5971 | ` * Return` |
|      - | 5972 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 5973 | ` */` |
|      2 | 5974 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5975 | `{` |
|      - | 5976 | `	const ph7_io_stream *pStream;` |
|      - | 5977 | `	unsigned char zDigest[20];` |
|      3 | 5978 | `	int raw_output  = FALSE;` |
|      - | 5979 | `	const char *zFile;` |
|      - | 5980 | `	SHA1Context sCtx;` |
|      - | 5981 | `	char zBuf[8192];` |
|      - | 5982 | `	void *pHandle;` |
|      - | 5983 | `	ph7_int64 n;` |
|      - | 5984 | `	int nLen;` |
|      3 | 5985 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5986 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5987 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5988 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5989 | `		return PH7_OK;` |
|      - | 5990 | `	}` |
|      - | 5991 | `	/* Extract the file path */` |
|      3 | 5992 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5993 | `	/* Point to the target IO stream device */` |
|      3 | 5994 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5995 | `	if( pStream == 0 ){` |
|    ! 0 | 5996 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5997 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5998 | `		return PH7_OK;` |
|      - | 5999 | `	}` |
|      3 | 6000 | `	if( nArg > 1 ){` |
|    ! 0 | 6001 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 6002 | `	}` |
|      - | 6003 | `	/* Try to open the file in read-only mode */` |
|      3 | 6004 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 6005 | `	if( pHandle == 0 ){` |
|    ! 0 | 6006 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 6007 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6008 | `		return PH7_OK;` |
|      - | 6009 | `	}` |
|      - | 6010 | `	/* Init the SHA1 context */` |
|      3 | 6011 | `	SHA1Init(&sCtx);` |
|      - | 6012 | `	/* Perform the requested operation */` |
|      2 | 6013 | `	for(;;){` |
|      5 | 6014 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 6015 | `		if( n < 1 ){` |
|      - | 6016 | `			/* EOF or IO error,break immediately */` |
|      3 | 6017 | `			break;` |
|      - | 6018 | `		}` |
|      3 | 6019 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 6020 | `	}` |
|      - | 6021 | `	/* Close the stream */` |
|      3 | 6022 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 6023 | `	/* Extract the digest */` |
|      3 | 6024 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 6025 | `	if( raw_output ){` |
|      - | 6026 | `		/* Output raw digest */` |
|    ! 0 | 6027 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 6028 | `	}else{` |
|      - | 6029 | `		/* Perform a binary to hex conversion */` |
|      3 | 6030 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 6031 | `	}` |
|      3 | 6032 | `	return PH7_OK;` |
|      2 | 6033 | `}` |
|      - | 6034 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6035 | `/*` |
|      - | 6036 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 6037 | ` *  Parse a configuration file.` |
|      - | 6038 | ` * Parameters` |
|      - | 6039 | ` * $filename` |
|      - | 6040 | ` *  The filename of the ini file being parsed.` |
|      - | 6041 | ` * $process_sections` |
|      - | 6042 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 6043 | ` *  with the section names and settings included.` |
|      - | 6044 | ` *  The default for process_sections is FALSE.` |
|      - | 6045 | ` * $scanner_mode` |
|      - | 6046 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 6047 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 6048 | ` * Return` |
|      - | 6049 | ` *  The settings are returned as an associative array on success.` |
|      - | 6050 | ` *  Otherwise is returned.` |
|      - | 6051 | ` */` |
|      2 | 6052 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6053 | `{` |
|      - | 6054 | `	const ph7_io_stream *pStream;` |
|      - | 6055 | `	const char *zFile;` |
|      - | 6056 | `	SyBlob sContents;` |
|      - | 6057 | `	void *pHandle;` |
|      - | 6058 | `	int nLen;` |
|      3 | 6059 | `	sxi32 rc = PH7_OK;` |
|      3 | 6060 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6061 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 6062 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 6063 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6064 | `		return PH7_OK;` |
|      - | 6065 | `	}` |
|      - | 6066 | `	/* Extract the file path */` |
|      3 | 6067 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 6068 | `	/* Point to the target IO stream device */` |
|      3 | 6069 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 6070 | `	if( pStream == 0 ){` |
|    ! 0 | 6071 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 6072 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6073 | `		return PH7_OK;` |
|      - | 6074 | `	}` |
|      - | 6075 | `	/* Try to open the file in read-only mode */` |
|      3 | 6076 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 6077 | `	if( pHandle == 0 ){` |
|    ! 0 | 6078 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 6079 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6080 | `		return PH7_OK;` |
|      - | 6081 | `	}` |
|      3 | 6082 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 6083 | `	/* Read the whole file */` |
|      3 | 6084 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 6085 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 6086 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 6087 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6088 | `	}else{` |
|      - | 6089 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 6090 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 6091 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 6092 | `	}` |
|      - | 6093 | `	/* Close the stream */` |
|      3 | 6094 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 6095 | `	/* Release the working buffer */` |
|      3 | 6096 | `	SyBlobRelease(&sContents);` |
|      - | 6097 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 6098 | `	return rc;` |
|      2 | 6099 | `}` |
|      - | 6100 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 6101 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 6102 | `/*` |
|      - | 6103 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 6104 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 6105 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 6106 | ` * to any non-VFS resource).` |
|      - | 6107 | ` */` |
|      - | 6108 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 6109 | `{` |
|      - | 6110 | `	SXUNUSED(pResource);` |
|      - | 6111 | `	return "Unknown";` |
|      - | 6112 | `}` |
|      - | 6113 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 6114 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 6115 | `{` |
|      - | 6116 | `	SXUNUSED(pResource);` |
|      - | 6117 | `	return 0;` |
|      - | 6118 | `}` |
|      - | 6119 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 6120 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 6121 | `#if defined(_MSC_VER)` |
|      - | 6122 | `static const ph7_vfs null_vfs = {` |
|      - | 6123 | `#else` |
|      - | 6124 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 6125 | `#endif` |
|      - | 6126 | `	"null_vfs",` |
|      - | 6127 | `	PH7_VFS_VERSION,` |
|      - | 6128 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 6129 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 6130 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 6131 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 6132 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 6133 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 6134 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 6135 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 6136 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 6137 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 6138 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 6139 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 6140 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 6141 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 6142 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 6143 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 6144 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 6145 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 6146 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 6147 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 6148 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6149 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6150 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 6151 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 6152 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 6153 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 6154 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 6155 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 6156 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 6157 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 6158 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 6159 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 6160 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 6161 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 6162 | `	0, /* int (*xUmask)(int) */` |
|      - | 6163 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 6164 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 6165 | `	0, /* int (*xUid)(void) */` |
|      - | 6166 | `	0, /* int (*xGid)(void) */` |
|      - | 6167 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 6168 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 6169 | `};` |
|      - | 6170 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 6171 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 6172 | `/*` |
|      - | 6173 | ` * Export the builtin vfs.` |
|      - | 6174 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 6175 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 6176 | ` * Note:` |
|      - | 6177 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 6178 | ` * Note:` |
|      - | 6179 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6180 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 6181 | ` */` |
|   3858 | 6182 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 6183 | `{` |
|      - | 6184 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6185 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 6186 | `	return &null_vfs;` |
|      - | 6187 | `#else` |
|      - | 6188 | `#ifdef __WINNT__` |
|      5 | 6189 | `	return &sWinVfs;` |
|      - | 6190 | `#elif defined(__UNIXES__)` |
|   3858 | 6191 | `	return &sUnixVfs;` |
|      - | 6192 | `#else` |
|      - | 6193 | `	return &null_vfs;` |
|      - | 6194 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 6195 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 6196 | `#else` |
|      - | 6197 | `	return &null_vfs;` |
|      - | 6198 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 6199 | `}` |
|      - | 6200 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6201 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6202 | `/*` |
|      - | 6203 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 6204 | ` * no particular meaning on windows.` |
|      - | 6205 | ` */` |
|      - | 6206 | `#ifndef STDIN_FILENO` |
|      - | 6207 | `#define STDIN_FILENO	0` |
|      - | 6208 | `#endif` |
|      - | 6209 | `#ifndef STDOUT_FILENO` |
|      - | 6210 | `#define STDOUT_FILENO	1` |
|      - | 6211 | `#endif` |
|      - | 6212 | `#ifndef STDERR_FILENO` |
|      - | 6213 | `#define STDERR_FILENO	2` |
|      - | 6214 | `#endif` |
|      - | 6215 | `/*` |
|      - | 6216 | ` * php:// Accessing various I/O streams` |
|      - | 6217 | ` * According to the PHP langage reference manual` |
|      - | 6218 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 6219 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 6220 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 6221 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 6222 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 6223 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 6224 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 6225 | ` * php://output` |
|      - | 6226 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 6227 | ` *  mechanism in the same way as print and echo.` |
|      - | 6228 | ` */` |
|      - | 6229 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 6230 | `/* Supported IO streams */` |
|      - | 6231 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 6232 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 6233 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 6234 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 6235 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|      - | 6236 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 6237 | `struct ph7_stream_data` |
|      - | 6238 | `{` |
|      - | 6239 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 6240 | `	int iType;   /* Stream type */` |
|      - | 6241 | `	union{` |
|      - | 6242 | `		void *pHandle; /* Stream handle */` |
|      - | 6243 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 6244 | `	}x;` |
|      - | 6245 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|      - | 6246 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|      - | 6247 | `	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */` |
|      - | 6248 | `};` |
|      - | 6249 | `/*` |
|      - | 6250 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 6251 | ` */` |
|     30 | 6252 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 6253 | `{` |
|      - | 6254 | `	ph7_stream_data *pData;` |
|     31 | 6255 | `	if( pVm == 0 ){` |
|    ! 0 | 6256 | `		return 0;` |
|      - | 6257 | `	}` |
|      - | 6258 | `	/* Allocate a new instance */` |
|     31 | 6259 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|     31 | 6260 | `	if( pData == 0 ){` |
|    ! 0 | 6261 | `		return 0;` |
|      - | 6262 | `	}` |
|      - | 6263 | `	/* Zero the structure */` |
|     31 | 6264 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 6265 | `	/* Initialize fields */` |
|     31 | 6266 | `	pData->iType = iType;` |
|     31 | 6267 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|     31 | 6268 | `	pData->nCur = 0;` |
|     31 | 6269 | `	pData->bReadOnly = 0;` |
|     31 | 6270 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6271 | `		/* Nothing else to set up: the buffer is the stream */` |
|     20 | 6272 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 6273 | `		/* Point to the default VM consumer routine. */` |
|      3 | 6274 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 6275 | `	}else{` |
|      - | 6276 | `#ifdef __WINNT__` |
|      - | 6277 | `		DWORD nChannel;` |
|      1 | 6278 | `		switch(iType){` |
|      1 | 6279 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 6280 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 6281 | `		default:` |
|      1 | 6282 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 6283 | `			break;` |
|      - | 6284 | `		}` |
|      1 | 6285 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 6286 | `#else` |
|      - | 6287 | `		/* Assume an UNIX system */` |
|      6 | 6288 | `		int ifd = STDIN_FILENO;` |
|      6 | 6289 | `		switch(iType){` |
|      2 | 6290 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 6291 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 6292 | `		default:` |
|      2 | 6293 | `			break;` |
|      - | 6294 | `		}` |
|      6 | 6295 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 6296 | `#endif` |
|      - | 6297 | `	}` |
|     31 | 6298 | `	pData->pVm = pVm;` |
|     31 | 6299 | `	return pData;` |
|     16 | 6300 | `}` |
|      - | 6301 | `/*` |
|      - | 6302 | ` * Implementation of the php:// IO streams routines` |
|      - | 6303 | ` * Status:` |
|      - | 6304 | ` *   Stable.` |
|      - | 6305 | ` */` |
|      - | 6306 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|     14 | 6307 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6308 | `{` |
|      - | 6309 | `	ph7_stream_data *pData;` |
|      - | 6310 | `	SyString sStream;` |
|     15 | 6311 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 6312 | `	/* Trim leading and trailing white spaces */` |
|     15 | 6313 | `	SyStringFullTrim(&sStream);` |
|      - | 6314 | `	/* Stream to open */` |
|     15 | 6315 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 6316 | `		iMode = PH7_IO_STREAM_STDIN;` |
|     15 | 6317 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 6318 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|     14 | 6319 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 6320 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|     13 | 6321 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 6322 | `		iMode = PH7_IO_STREAM_STDERR;` |
|     12 | 6323 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|      8 | 6324 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|      - | 6325 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|      - | 6326 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|     13 | 6327 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|      7 | 6328 | `	}else{` |
|      - | 6329 | `		/* unknown stream name */` |
|    ! 0 | 6330 | `		return -1;` |
|      - | 6331 | `	}` |
|      - | 6332 | `	/* Create our handle */` |
|     15 | 6333 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|     15 | 6334 | `	if( pData == 0 ){` |
|    ! 0 | 6335 | `		return -1;` |
|      - | 6336 | `	}` |
|      - | 6337 | `	/* Make the handle public */` |
|     15 | 6338 | `	*ppHandle = (void *)pData;` |
|     15 | 6339 | `	return PH7_OK;` |
|      8 | 6340 | `}` |
|      - | 6341 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|     42 | 6342 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|      1 | 6343 | `{` |
|     43 | 6344 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     43 | 6345 | `	if( pData == 0 ){` |
|    ! 0 | 6346 | `		return -1;` |
|      - | 6347 | `	}` |
|     43 | 6348 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     43 | 6349 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|      - | 6350 | `		sxu32 nRead;` |
|     43 | 6351 | `		if( pData->nCur >= nAvail ){` |
|     15 | 6352 | `			return 0; /* EOF */` |
|      - | 6353 | `		}` |
|     29 | 6354 | `		nRead = nAvail - pData->nCur;` |
|     29 | 6355 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|      7 | 6356 | `			nRead = (sxu32)nDatatoRead;` |
|      3 | 6357 | `		}` |
|     29 | 6358 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|     29 | 6359 | `		pData->nCur += nRead;` |
|     29 | 6360 | `		return (ph7_int64)nRead;` |
|      - | 6361 | `	}` |
|    ! 0 | 6362 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 6363 | `		/* Forbidden */` |
|    ! 0 | 6364 | `		return -1;` |
|      - | 6365 | `	}` |
|      - | 6366 | `#ifdef __WINNT__` |
|      - | 6367 | `	{` |
|      - | 6368 | `		DWORD nRd;` |
|      - | 6369 | `		BOOL rc;` |
|    ! 0 | 6370 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 6371 | `		if( !rc ){` |
|      - | 6372 | `			/* IO error */` |
|    ! 0 | 6373 | `			return -1;` |
|      - | 6374 | `		}` |
|    ! 0 | 6375 | `		return (ph7_int64)nRd;` |
|      - | 6376 | `	}` |
|      - | 6377 | `#elif defined(__UNIXES__)` |
|      - | 6378 | `	{` |
|      - | 6379 | `		ssize_t nRd;` |
|      - | 6380 | `		int fd;` |
|    ! 0 | 6381 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6382 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 6383 | `		if( nRd < 1 ){` |
|    ! 0 | 6384 | `			return -1;` |
|      - | 6385 | `		}` |
|    ! 0 | 6386 | `		return (ph7_int64)nRd;` |
|      - | 6387 | `	}` |
|      - | 6388 | `#else` |
|      - | 6389 | `	return -1;` |
|      - | 6390 | `#endif` |
|     22 | 6391 | `}` |
|      - | 6392 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|     12 | 6393 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 6394 | `{` |
|     13 | 6395 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     13 | 6396 | `	if( pData == 0 ){` |
|    ! 0 | 6397 | `		return -1;` |
|      - | 6398 | `	}` |
|     13 | 6399 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 6400 | `		/* Forbidden */` |
|    ! 0 | 6401 | `		return -1;` |
|     13 | 6402 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6403 | `		sxu32 nLen,nEnd;` |
|     11 | 6404 | `		if( pData->bReadOnly ){` |
|    ! 0 | 6405 | `			return -1;` |
|      - | 6406 | `		}` |
|     11 | 6407 | `		nLen = SyBlobLength(&pData->sMem);` |
|     11 | 6408 | `		if( pData->nCur > nLen ){` |
|      - | 6409 | `			/* seek past end: php zero-fills the gap */` |
|      - | 6410 | `			static const char zZero[64] = {0};` |
|    ! 0 | 6411 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|    ! 0 | 6412 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|    ! 0 | 6413 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6414 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6415 | `					return -1;` |
|      - | 6416 | `				}` |
|    ! 0 | 6417 | `			}` |
|    ! 0 | 6418 | `			nLen = SyBlobLength(&pData->sMem);` |
|    ! 0 | 6419 | `		}` |
|     11 | 6420 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|     11 | 6421 | `		if( pData->nCur < nLen ){` |
|      - | 6422 | `			/* overwrite in place up to the current end */` |
|      3 | 6423 | `			sxu32 nOver = nLen - pData->nCur;` |
|      3 | 6424 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|      3 | 6425 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|      3 | 6426 | `			if( nEnd > nLen ){` |
|    ! 0 | 6427 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|    ! 0 | 6428 | `					return -1;` |
|      - | 6429 | `				}` |
|    ! 0 | 6430 | `			}` |
|      2 | 6431 | `		}else{` |
|      9 | 6432 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|    ! 0 | 6433 | `				return -1;` |
|      - | 6434 | `			}` |
|      - | 6435 | `		}` |
|     11 | 6436 | `		pData->nCur = nEnd;` |
|     11 | 6437 | `		return nWrite;` |
|      3 | 6438 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 6439 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 6440 | `		int rc;` |
|      - | 6441 | `		/* Call the vm output consumer */` |
|      3 | 6442 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 6443 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 6444 | `			return -1;` |
|      - | 6445 | `		}` |
|      3 | 6446 | `		return nWrite;` |
|      - | 6447 | `	}` |
|      - | 6448 | `#ifdef __WINNT__` |
|      - | 6449 | `	{` |
|      - | 6450 | `		DWORD nWr;` |
|      - | 6451 | `		BOOL rc;` |
|    ! 0 | 6452 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 6453 | `		if( !rc ){` |
|      - | 6454 | `			/* IO error */` |
|    ! 0 | 6455 | `			return -1;` |
|      - | 6456 | `		}` |
|    ! 0 | 6457 | `		return (ph7_int64)nWr;` |
|      - | 6458 | `	}` |
|      - | 6459 | `#elif defined(__UNIXES__)` |
|      - | 6460 | `	{` |
|      - | 6461 | `		ssize_t nWr;` |
|      - | 6462 | `		int fd;` |
|    ! 0 | 6463 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6464 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 6465 | `		if( nWr < 1 ){` |
|    ! 0 | 6466 | `			return -1;` |
|      - | 6467 | `		}` |
|    ! 0 | 6468 | `		return (ph7_int64)nWr;` |
|      - | 6469 | `	}` |
|      - | 6470 | `#else` |
|      - | 6471 | `	return -1;` |
|      - | 6472 | `#endif` |
|      7 | 6473 | `}` |
|      - | 6474 | `/* void (*xClose)(void *) */` |
|     20 | 6475 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 6476 | `{` |
|     21 | 6477 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6478 | `	ph7_vm *pVm;` |
|     21 | 6479 | `	if( pData == 0 ){` |
|    ! 0 | 6480 | `		return;` |
|      - | 6481 | `	}` |
|     21 | 6482 | `	pVm = pData->pVm;` |
|     21 | 6483 | `	SyBlobRelease(&pData->sMem);` |
|      - | 6484 | `	/* Free the instance */` |
|     21 | 6485 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|     11 | 6486 | `}` |
|      - | 6487 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|     20 | 6488 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 6489 | `{` |
|     21 | 6490 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6491 | `	ph7_int64 iNew;` |
|     21 | 6492 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6493 | `		return -1;` |
|      - | 6494 | `	}` |
|     21 | 6495 | `	switch(whence){` |
|    ! 0 | 6496 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|      3 | 6497 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|     19 | 6498 | `	default:            iNew = iOfft; break;` |
|      - | 6499 | `	}` |
|     21 | 6500 | `	if( iNew < 0 ){` |
|    ! 0 | 6501 | `		return -1;` |
|      - | 6502 | `	}` |
|     21 | 6503 | `	pData->nCur = (sxu32)iNew;` |
|     21 | 6504 | `	return PH7_OK;` |
|     11 | 6505 | `}` |
|      - | 6506 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|      4 | 6507 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|      1 | 6508 | `{` |
|      5 | 6509 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      5 | 6510 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6511 | `		return -1;` |
|      - | 6512 | `	}` |
|      5 | 6513 | `	return (ph7_int64)pData->nCur;` |
|      3 | 6514 | `}` |
|      - | 6515 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|    ! 0 | 6516 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|    ! 0 | 6517 | `{` |
|    ! 0 | 6518 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 6519 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|    ! 0 | 6520 | `		return -1;` |
|      - | 6521 | `	}` |
|    ! 0 | 6522 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|      - | 6523 | `		/* shrink in place: the blob keeps its allocation */` |
|    ! 0 | 6524 | `		pData->sMem.nByte = (sxu32)nLen;` |
|    ! 0 | 6525 | `	}else{` |
|      - | 6526 | `		static const char zZero[64] = {0};` |
|    ! 0 | 6527 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|    ! 0 | 6528 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|    ! 0 | 6529 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6530 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6531 | `				return -1;` |
|      - | 6532 | `			}` |
|    ! 0 | 6533 | `		}` |
|      - | 6534 | `	}` |
|    ! 0 | 6535 | `	return PH7_OK;` |
|    ! 0 | 6536 | `}` |
|      - | 6537 | `/*` |
|      - | 6538 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|      - | 6539 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|      - | 6540 | ` * base64). Shares the MEMORY machinery above.` |
|      - | 6541 | ` */` |
|      8 | 6542 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 6543 | `{` |
|      9 | 6544 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|      1 | 6545 | `}` |
|     10 | 6546 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6547 | `{` |
|      - | 6548 | `	ph7_stream_data *pData;` |
|     11 | 6549 | `	const char *zIn = zName;` |
|     11 | 6550 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|     11 | 6551 | `	const char *zComma = 0;` |
|     11 | 6552 | `	int bBase64 = 0;` |
|      5 | 6553 | `	SXUNUSED(iMode);` |
|      - | 6554 | `	/* Find the comma separating the mediatype from the payload */` |
|    105 | 6555 | `	while( zIn < zEnd ){` |
|    105 | 6556 | `		if( zIn[0] == ',' ){` |
|     11 | 6557 | `			zComma = zIn;` |
|     11 | 6558 | `			break;` |
|      - | 6559 | `		}` |
|     95 | 6560 | `		zIn++;` |
|      1 | 6561 | `	}` |
|     11 | 6562 | `	if( zComma == 0 ){` |
|    ! 0 | 6563 | `		return -1;` |
|      - | 6564 | `	}` |
|     10 | 6565 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|     10 | 6566 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|      3 | 6567 | `		bBase64 = 1;` |
|      1 | 6568 | `	}` |
|     11 | 6569 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|     11 | 6570 | `	if( pData == 0 ){` |
|    ! 0 | 6571 | `		return -1;` |
|      - | 6572 | `	}` |
|     11 | 6573 | `	pData->bReadOnly = 1;` |
|     11 | 6574 | `	zIn = &zComma[1];` |
|     11 | 6575 | `	if( bBase64 ){` |
|      3 | 6576 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|    ! 0 | 6577 | `			SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6578 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6579 | `			return -1;` |
|      - | 6580 | `		}` |
|      2 | 6581 | `	}else{` |
|      - | 6582 | `		/* percent-decode the payload */` |
|     71 | 6583 | `		while( zIn < zEnd ){` |
|     63 | 6584 | `			char c = zIn[0];` |
|     63 | 6585 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|      3 | 6586 | `				int hi = SyHexToint(zIn[1]);` |
|      3 | 6587 | `				int lo = SyHexToint(zIn[2]);` |
|      3 | 6588 | `				c = (char)((hi << 4) \| lo);` |
|      3 | 6589 | `				zIn += 3;` |
|      2 | 6590 | `			}else{` |
|     61 | 6591 | `				zIn++;` |
|      - | 6592 | `			}` |
|     63 | 6593 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|    ! 0 | 6594 | `				SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6595 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6596 | `				return -1;` |
|      - | 6597 | `			}` |
|      1 | 6598 | `		}` |
|      - | 6599 | `	}` |
|     11 | 6600 | `	*ppHandle = (void *)pData;` |
|     11 | 6601 | `	return PH7_OK;` |
|      6 | 6602 | `}` |
|      - | 6603 | `/* data:// rejects writes outright */` |
|    ! 0 | 6604 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 6605 | `{` |
|    ! 0 | 6606 | `	SXUNUSED(pHandle);` |
|    ! 0 | 6607 | `	SXUNUSED(pBuf);` |
|    ! 0 | 6608 | `	SXUNUSED(nWrite);` |
|    ! 0 | 6609 | `	return -1;` |
|    ! 0 | 6610 | `}` |
|      - | 6611 | `static const ph7_io_stream sDATA_Stream = {` |
|      - | 6612 | `	"data",` |
|      - | 6613 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6614 | `	DataStreamData_Open,  /* xOpen */` |
|      - | 6615 | `	0,   /* xOpenDir */` |
|      - | 6616 | `	PHPStreamData_Close, /* xClose */` |
|      - | 6617 | `	0,  /* xCloseDir */` |
|      - | 6618 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 6619 | `	0,  /* xReadDir */` |
|      - | 6620 | `	DataStreamData_Write, /* xWrite */` |
|      - | 6621 | `	PHPStreamData_Seek,  /* xSeek */` |
|      - | 6622 | `	0,  /* xLock */` |
|      - | 6623 | `	0,  /* xRewindDir */` |
|      - | 6624 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 6625 | `	0,  /* xTrunc */` |
|      - | 6626 | `	0,  /* xSync */` |
|      - | 6627 | `	0   /* xStat */` |
|      - | 6628 | `};` |
|      - | 6629 | `/*` |
|      - | 6630 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 6631 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 6632 | ` * PHP-compatible process I/O functionality.` |
|      - | 6633 | ` */` |
|      - | 6634 | `typedef struct pipe_private pipe_private;` |
|      - | 6635 | `struct pipe_private` |
|      - | 6636 | `{` |
|      - | 6637 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 6638 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 6639 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 6640 | `#ifdef __WINNT__` |
|      - | 6641 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 6642 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 6643 | `#endif` |
|      - | 6644 | `};` |
|      - | 6645 |  |
|      - | 6646 | `#ifdef __WINNT__` |
|      - | 6647 | `#include <Windows.h>` |
|      - | 6648 | `#include <stdio.h>` |
|      - | 6649 | `#include <io.h>` |
|      - | 6650 | `#include <fcntl.h>` |
|      - | 6651 | `/*` |
|      - | 6652 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 6653 | ` * This allows us to properly wait for process completion.` |
|      - | 6654 | ` */` |
|      - | 6655 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      5 | 6656 | `{` |
|      5 | 6657 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      5 | 6658 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      5 | 6659 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 6660 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 6661 | `	STARTUPINFOW si;` |
|      - | 6662 | `	PROCESS_INFORMATION pi;` |
|      5 | 6663 | `	WCHAR *zWideCmd = NULL;` |
|      5 | 6664 | `	FILE *pFile = NULL;` |
|      - | 6665 | `	int fd;` |
|      5 | 6666 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 6667 |  |
|      - | 6668 | `	/* Set up security attributes for pipe inheritance */` |
|      5 | 6669 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      5 | 6670 | `	sa.bInheritHandle = TRUE;` |
|      5 | 6671 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 6672 |  |
|      - | 6673 | `	/* Create pipes for child process I/O */` |
|      5 | 6674 | `	if( bRead ){` |
|      - | 6675 | `		/* Reading from child's stdout */` |
|      5 | 6676 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 6677 | `			return NULL;` |
|      - | 6678 | `		}` |
|      - | 6679 | `		/* Ensure read handle is not inherited */` |
|      5 | 6680 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      5 | 6681 | `		hReadPipe = hChildStdoutRd;` |
|      5 | 6682 | `		*phPipe = hChildStdoutRd;` |
|      5 | 6683 | `	}else{` |
|      - | 6684 | `		/* Writing to child's stdin */` |
|    ! 0 | 6685 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 6686 | `			return NULL;` |
|      - | 6687 | `		}` |
|      - | 6688 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 6689 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 6690 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 6691 | `		*phPipe = hChildStdinWr;` |
|      - | 6692 | `	}` |
|      - | 6693 |  |
|      - | 6694 | `	/* Convert command to wide string */` |
|      - | 6695 | `	{` |
|      5 | 6696 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      5 | 6697 | `		if( nLen <= 0 ){` |
|    ! 0 | 6698 | `			goto cleanup_pipes;` |
|      - | 6699 | `		}` |
|      5 | 6700 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      5 | 6701 | `		if( !zWideCmd ){` |
|    ! 0 | 6702 | `			goto cleanup_pipes;` |
|      - | 6703 | `		}` |
|      5 | 6704 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 6705 | `	}` |
|      - | 6706 |  |
|      - | 6707 | `	/* Set up process startup info */` |
|      5 | 6708 | `	ZeroMemory(&si, sizeof(si));` |
|      5 | 6709 | `	si.cb = sizeof(si);` |
|      5 | 6710 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      5 | 6711 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      5 | 6712 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      5 | 6713 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      5 | 6714 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 6715 |  |
|      5 | 6716 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 6717 |  |
|      - | 6718 | `	/* Create the child process */` |
|      5 | 6719 | `	if( !CreateProcessW(` |
|      - | 6720 | `		NULL,           /* Application name */` |
|      - | 6721 | `		zWideCmd,       /* Command line */` |
|      - | 6722 | `		NULL,           /* Process security attributes */` |
|      - | 6723 | `		NULL,           /* Thread security attributes */` |
|      - | 6724 | `		TRUE,           /* Inherit handles */` |
|      - | 6725 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 6726 | `		NULL,           /* Environment */` |
|      - | 6727 | `		NULL,           /* Current directory */` |
|      - | 6728 | `		&si,            /* Startup info */` |
|      - | 6729 | `		&pi             /* Process info */` |
|      - | 6730 | `	)){` |
|    ! 0 | 6731 | `		goto cleanup_all;` |
|      - | 6732 | `	}` |
|      - | 6733 |  |
|      - | 6734 | `	/* Close handles we don't need in parent */` |
|      5 | 6735 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      5 | 6736 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 6737 |  |
|      - | 6738 | `	/* Close thread handle (we only need process handle) */` |
|      5 | 6739 | `	CloseHandle(pi.hThread);` |
|      - | 6740 |  |
|      - | 6741 | `	/* Store process handle for later waiting */` |
|      5 | 6742 | `	*phProcess = pi.hProcess;` |
|      - | 6743 |  |
|      - | 6744 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      5 | 6745 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 6746 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      5 | 6747 | `	if( fd == -1 ){` |
|    ! 0 | 6748 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6749 | `		*phProcess = NULL;` |
|    ! 0 | 6750 | `		goto cleanup_all;` |
|      - | 6751 | `	}` |
|      - | 6752 |  |
|      5 | 6753 | `	pFile = _fdopen(fd, zMode);` |
|      5 | 6754 | `	if( !pFile ){` |
|    ! 0 | 6755 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 6756 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6757 | `		*phProcess = NULL;` |
|    ! 0 | 6758 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 6759 | `		return NULL;` |
|      - | 6760 | `	}` |
|      - | 6761 |  |
|      5 | 6762 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      5 | 6763 | `	return pFile;` |
|      - | 6764 |  |
|      - | 6765 | `cleanup_all:` |
|    ! 0 | 6766 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 6767 | `cleanup_pipes:` |
|    ! 0 | 6768 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 6769 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 6770 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 6771 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 6772 | `	return NULL;` |
|      5 | 6773 | `}` |
|      - | 6774 |  |
|      - | 6775 | `/*` |
|      - | 6776 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 6777 | ` */` |
|      - | 6778 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      5 | 6779 | `{` |
|      5 | 6780 | `	DWORD dwExitCode = 0;` |
|      - | 6781 | `	int status;` |
|      - | 6782 |  |
|      - | 6783 | `	/* Close the FILE* (this closes the pipe) */` |
|      5 | 6784 | `	fclose(pFile);` |
|      - | 6785 |  |
|      5 | 6786 | `	if( hProcess ){` |
|      - | 6787 | `		/* Wait for the process to complete */` |
|      5 | 6788 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 6789 |  |
|      5 | 6790 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      5 | 6791 | `			status = (int)dwExitCode;` |
|      5 | 6792 | `		}else{` |
|    ! 0 | 6793 | `			status = -1;` |
|      - | 6794 | `		}` |
|      - | 6795 |  |
|      - | 6796 | `		/* Close process handle */` |
|      5 | 6797 | `		CloseHandle(hProcess);` |
|      5 | 6798 | `	}else{` |
|    ! 0 | 6799 | `		status = -1;` |
|      - | 6800 | `	}` |
|      - | 6801 |  |
|      5 | 6802 | `	return status;` |
|      5 | 6803 | `}` |
|      - | 6804 | `#endif /* __WINNT__ */` |
|      - | 6805 | `/*` |
|      - | 6806 | ` * Open a pipe to a process.` |
|      - | 6807 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 6808 | ` */` |
|   3930 | 6809 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      5 | 6810 | `{` |
|      - | 6811 | `	pipe_private *pPipe;` |
|      - | 6812 | `	FILE *pFile;` |
|   3935 | 6813 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 6814 | `		return 0;` |
|      - | 6815 | `	}` |
|      - | 6816 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   3935 | 6817 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 6818 | `		return 0;` |
|      - | 6819 | `	}` |
|      - | 6820 | `	/* Open the pipe using system popen */` |
|      - | 6821 | `#ifdef __WINNT__` |
|      - | 6822 | `	{` |
|      - | 6823 | `		/* Build cmd.exe command wrapper */` |
|      5 | 6824 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      5 | 6825 | `		const char *zShellSuffix = "\"";` |
|      5 | 6826 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      5 | 6827 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      5 | 6828 | `		size_t nCmd = strlen(zCommand);` |
|      5 | 6829 | `		size_t nQuotes = 0;` |
|      5 | 6830 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6831 | `			if (zCommand[i] == '"') nQuotes++;` |
|      5 | 6832 | `		}` |
|      5 | 6833 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      5 | 6834 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      5 | 6835 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 6836 | `			return 0;` |
|      - | 6837 | `		}` |
|      - | 6838 | `		/* Escape quotes in command */` |
|      5 | 6839 | `		size_t j = 0;` |
|      5 | 6840 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6841 | `			char ch = zCommand[i];` |
|      5 | 6842 | `			if (ch == '"') {` |
|      4 | 6843 | `				zCmdEsc[j++] = '^';` |
|      4 | 6844 | `				zCmdEsc[j++] = '"';` |
|      4 | 6845 | `			} else {` |
|      5 | 6846 | `				zCmdEsc[j++] = ch;` |
|      - | 6847 | `			}` |
|      5 | 6848 | `		}` |
|      5 | 6849 | `		zCmdEsc[j] = '\0';` |
|      5 | 6850 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      5 | 6851 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      5 | 6852 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 6853 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6854 | `			return 0;` |
|      - | 6855 | `		}` |
|      5 | 6856 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      5 | 6857 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      5 | 6858 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      5 | 6859 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 6860 | `		/* Allocate pipe structure early so we can store handles */` |
|      5 | 6861 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      5 | 6862 | `		if( pPipe == 0 ){` |
|    ! 0 | 6863 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6864 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 6865 | `			return 0;` |
|      - | 6866 | `		}` |
|      - | 6867 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      5 | 6868 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      5 | 6869 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      5 | 6870 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      5 | 6871 | `		if( pFile == 0 ){` |
|    ! 0 | 6872 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 6873 | `			return 0;` |
|      - | 6874 | `		}` |
|      - | 6875 | `		/* Initialize remaining fields */` |
|      5 | 6876 | `		pPipe->pFile = pFile;` |
|      5 | 6877 | `		pPipe->pVm = pVm;` |
|      5 | 6878 | `		pPipe->iMode = zMode[0];` |
|      - | 6879 | `	}` |
|      - | 6880 | `#elif defined(__UNIXES__) /* Unix */` |
|   3930 | 6881 | `	pFile = popen(zCommand, zMode);` |
|   3930 | 6882 | `	if( pFile == 0 ){` |
|    ! 0 | 6883 | `		return 0;` |
|      - | 6884 | `	}` |
|      - | 6885 | `	/* Allocate pipe private structure */` |
|   3930 | 6886 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   3930 | 6887 | `	if( pPipe == 0 ){` |
|      - | 6888 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 6889 | `		pclose(pFile);` |
|    ! 0 | 6890 | `		return 0;` |
|      - | 6891 | `	}` |
|      - | 6892 | `	/* Initialize the structure */` |
|   3930 | 6893 | `	pPipe->pFile = pFile;` |
|   3930 | 6894 | `	pPipe->pVm = pVm;` |
|   3930 | 6895 | `	pPipe->iMode = zMode[0];` |
|      - | 6896 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6897 | `	(void)pFile;` |
|      - | 6898 | `	return 0;` |
|      - | 6899 | `#endif` |
|   3935 | 6900 | `	return pPipe;` |
|   1970 | 6901 | `}` |
|      - | 6902 | `/*` |
|      - | 6903 | ` * Close a pipe and return the exit status of the process.` |
|      - | 6904 | ` * Returns the exit status, or -1 on error.` |
|      - | 6905 | ` */` |
|   3904 | 6906 | `static int PipeClose(pipe_private *pPipe)` |
|      5 | 6907 | `{` |
|      - | 6908 | `	int status;` |
|      - | 6909 | `	ph7_vm *pVm;` |
|   3909 | 6910 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6911 | `		return -1;` |
|      - | 6912 | `	}` |
|   3909 | 6913 | `	pVm = pPipe->pVm;` |
|      - | 6914 | `	/* Close the pipe and get exit status */` |
|      - | 6915 | `#ifdef __WINNT__` |
|      - | 6916 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      5 | 6917 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 6918 | `#elif defined(__UNIXES__)` |
|   3904 | 6919 | `	status = pclose(pPipe->pFile);` |
|      - | 6920 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   3904 | 6921 | `	if( status != -1 ){` |
|   3904 | 6922 | `		if( WIFEXITED(status) ){` |
|   3904 | 6923 | `			status = WEXITSTATUS(status);` |
|   1952 | 6924 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 6925 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 6926 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 6927 | `		}else{` |
|      - | 6928 | `			/* Unknown termination reason */` |
|    ! 0 | 6929 | `			status = -1;` |
|      - | 6930 | `		}` |
|   1952 | 6931 | `	}` |
|      - | 6932 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6933 | `	status = -1;` |
|      - | 6934 | `#endif` |
|      - | 6935 | `	/* Free the structure */` |
|   3909 | 6936 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   3909 | 6937 | `	return status;` |
|   1957 | 6938 | `}` |
|      - | 6939 | `/*` |
|      - | 6940 | ` * Pipe stream xClose implementation.` |
|      - | 6941 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 6942 | ` * It closes the pipe but does not return the exit status.` |
|      - | 6943 | ` */` |
|    100 | 6944 | `static void PipeStream_Close(void *pHandle)` |
|      4 | 6945 | `{` |
|    104 | 6946 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    104 | 6947 | `	if( pPipe ){` |
|    104 | 6948 | `		PipeClose(pPipe);` |
|     50 | 6949 | `	}` |
|    104 | 6950 | `}` |
|      - | 6951 | `/*` |
|      - | 6952 | ` * Pipe stream xRead implementation.` |
|      - | 6953 | ` */` |
|   5842 | 6954 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      4 | 6955 | `{` |
|   5846 | 6956 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6957 | `	size_t nRead;` |
|   5846 | 6958 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6959 | `		return -1;` |
|      - | 6960 | `	}` |
|   5846 | 6961 | `	if( pPipe->iMode != 'r' ){` |
|      - | 6962 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 6963 | `		return -1;` |
|      - | 6964 | `	}` |
|   5846 | 6965 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   5846 | 6966 | `	if( nRead == 0 ){` |
|   3938 | 6967 | `		if( feof(pPipe->pFile) ){` |
|   3938 | 6968 | `			return 0; /* EOF */` |
|      - | 6969 | `		}` |
|    ! 0 | 6970 | `		return -1; /* Error */` |
|      - | 6971 | `	}` |
|   1912 | 6972 | `	return (ph7_int64)nRead;` |
|   2925 | 6973 | `}` |
|      - | 6974 | `/*` |
|      - | 6975 | ` * Pipe stream xWrite implementation.` |
|      - | 6976 | ` */` |
|      2 | 6977 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 6978 | `{` |
|      2 | 6979 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6980 | `	size_t nWritten;` |
|      2 | 6981 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6982 | `		return -1;` |
|      - | 6983 | `	}` |
|      2 | 6984 | `	if( pPipe->iMode != 'w' ){` |
|      - | 6985 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 6986 | `		return -1;` |
|      - | 6987 | `	}` |
|      2 | 6988 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 6989 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 6990 | `		return -1; /* Error */` |
|      - | 6991 | `	}` |
|      2 | 6992 | `	return (ph7_int64)nWritten;` |
|      1 | 6993 | `}` |
|      - | 6994 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 6995 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 6996 | `	"pipe",` |
|      - | 6997 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6998 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|      - | 6999 | `	0,  /* xOpenDir */` |
|      - | 7000 | `	PipeStream_Close,  /* xClose */` |
|      - | 7001 | `	0,  /* xCloseDir */` |
|      - | 7002 | `	PipeStream_Read,   /* xRead */` |
|      - | 7003 | `	0,  /* xReadDir */` |
|      - | 7004 | `	PipeStream_Write,  /* xWrite */` |
|      - | 7005 | `	0,  /* xSeek */` |
|      - | 7006 | `	0,  /* xLock */` |
|      - | 7007 | `	0,  /* xRewindDir */` |
|      - | 7008 | `	0,  /* xTell */` |
|      - | 7009 | `	0,  /* xTrunc */` |
|      - | 7010 | `	0,  /* xSync */` |
|      - | 7011 | `	0   /* xStat */` |
|      - | 7012 | `};` |
|      - | 7013 | `/*` |
|      - | 7014 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 7015 | ` * FALSE otherwise.` |
|      - | 7016 | ` */` |
|   3800 | 7017 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      5 | 7018 | `{` |
|   3805 | 7019 | `	return pStream == &sPipe_Stream;` |
|      5 | 7020 | `}` |
|      - | 7021 | `/*` |
|      - | 7022 | ` * resource popen(string $command, string $mode)` |
|      - | 7023 | ` *  Opens process file pointer.` |
|      - | 7024 | ` * Parameters` |
|      - | 7025 | ` *  $command` |
|      - | 7026 | ` *   The command to execute. Passed to the system shell.` |
|      - | 7027 | ` *  $mode` |
|      - | 7028 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 7029 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 7030 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 7031 | ` * Return` |
|      - | 7032 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 7033 | ` */` |
|      - | 7034 | `/*` |
|      - | 7035 | ` * string\|false\|null shell_exec(string $command)` |
|      - | 7036 | ` *  Execute a command via the shell and return the complete output as a string.` |
|      - | 7037 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|      - | 7038 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|      - | 7039 | ` */` |
|      4 | 7040 | `static int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7041 | `{` |
|      - | 7042 | `	const char *zCommand;` |
|      - | 7043 | `	pipe_private *pPipe;` |
|      - | 7044 | `	SyBlob sOut;` |
|      - | 7045 | `	char zBuf[4096];` |
|      - | 7046 | `	size_t nRead;` |
|      - | 7047 | `	int nCmdLen;` |
|      6 | 7048 | `	if( nArg < 1 ){` |
|    ! 0 | 7049 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7050 | `		return PH7_OK;` |
|      - | 7051 | `	}` |
|      6 | 7052 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|      6 | 7053 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7054 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7055 | `		return PH7_OK;` |
|      - | 7056 | `	}` |
|      6 | 7057 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|      6 | 7058 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 7059 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7060 | `		return PH7_OK;` |
|      - | 7061 | `	}` |
|      6 | 7062 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|      4 | 7063 | `	for(;;){` |
|     10 | 7064 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|     10 | 7065 | `		if( nRead < 1 ){` |
|      6 | 7066 | `			break;` |
|      - | 7067 | `		}` |
|      6 | 7068 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|      2 | 7069 | `	}` |
|      6 | 7070 | `	PipeClose(pPipe);` |
|      6 | 7071 | `	if( SyBlobLength(&sOut) < 1 ){` |
|      - | 7072 | `		/* php answers NULL, not "", when the command printed nothing */` |
|    ! 0 | 7073 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7074 | `	}else{` |
|      6 | 7075 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|      - | 7076 | `	}` |
|      6 | 7077 | `	SyBlobRelease(&sOut);` |
|      6 | 7078 | `	return PH7_OK;` |
|      4 | 7079 | `}` |
|   3926 | 7080 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7081 | `{` |
|      - | 7082 | `	const char *zCommand, *zMode;` |
|      - | 7083 | `	pipe_private *pPipe;` |
|      - | 7084 | `	io_private *pDev;` |
|      - | 7085 | `	int nCmdLen, nModeLen;` |
|   3931 | 7086 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 7087 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 7088 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 7089 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7090 | `		return PH7_OK;` |
|      - | 7091 | `	}` |
|      - | 7092 | `	/* Extract the command and mode */` |
|   3931 | 7093 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   3931 | 7094 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   3931 | 7095 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7096 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 7097 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7098 | `		return PH7_OK;` |
|      - | 7099 | `	}` |
|   3931 | 7100 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 7101 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 7102 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7103 | `		return PH7_OK;` |
|      - | 7104 | `	}` |
|      - | 7105 | `	/* Open the pipe */` |
|   3931 | 7106 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   3931 | 7107 | `	if( pPipe == 0 ){` |
|      - | 7108 | `		/* Failed to open pipe */` |
|    ! 0 | 7109 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7110 | `		return PH7_OK;` |
|      - | 7111 | `	}` |
|      - | 7112 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   3931 | 7113 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   3931 | 7114 | `	if( pDev == 0 ){` |
|    ! 0 | 7115 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 7116 | `		PipeClose(pPipe);` |
|    ! 0 | 7117 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7118 | `		return PH7_OK;` |
|      - | 7119 | `	}` |
|      - | 7120 | `	/* Initialize the io_private structure */` |
|   3931 | 7121 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   3931 | 7122 | `	pDev->pHandle = pPipe;` |
|      - | 7123 | `	/* Return the io_private instance as a resource */` |
|   3931 | 7124 | `	ph7_result_resource(pCtx, pDev);` |
|   3931 | 7125 | `	return PH7_OK;` |
|   1968 | 7126 | `}` |
|      - | 7127 | `/*` |
|      - | 7128 | ` * int pclose(resource $handle)` |
|      - | 7129 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 7130 | ` * Parameters` |
|      - | 7131 | ` *  $handle` |
|      - | 7132 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 7133 | ` * Return` |
|      - | 7134 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 7135 | ` */` |
|   3800 | 7136 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7137 | `{` |
|      - | 7138 | `	const ph7_io_stream *pStream;` |
|      - | 7139 | `	pipe_private *pPipe;` |
|      - | 7140 | `	io_private *pDev;` |
|      - | 7141 | `	int status;` |
|   3805 | 7142 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 7143 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 7144 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7145 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7146 | `		return PH7_OK;` |
|      - | 7147 | `	}` |
|      - | 7148 | `	/* Extract our private data */` |
|   3805 | 7149 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 7150 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3805 | 7151 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 7152 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7153 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7154 | `		return PH7_OK;` |
|      - | 7155 | `	}` |
|      - | 7156 | `	/* Point to the target IO stream device */` |
|   3805 | 7157 | `	pStream = pDev->pStream;` |
|   3805 | 7158 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 7159 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 7160 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7161 | `		return PH7_OK;` |
|      - | 7162 | `	}` |
|      - | 7163 | `	/* Get the pipe handle */` |
|   3805 | 7164 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 7165 | `	/* Close the pipe and get exit status */` |
|   3805 | 7166 | `	status = PipeClose(pPipe);` |
|      - | 7167 | `	/* Keep the handle alive but flag it closed so shared copies see it */` |
|   3805 | 7168 | `	MarkIOPrivateClosed(pDev);` |
|      - | 7169 | `	/* Return the exit status */` |
|   3805 | 7170 | `	ph7_result_int(pCtx, status);` |
|   3805 | 7171 | `	return PH7_OK;` |
|   1905 | 7172 | `}` |
|      - | 7173 | `/* Export the php:// stream */` |
|      - | 7174 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 7175 | `	"php",` |
|      - | 7176 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7177 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 7178 | `	0,   /* xOpenDir */` |
|      - | 7179 | `	PHPStreamData_Close, /* xClose */` |
|      - | 7180 | `	0,  /* xCloseDir */` |
|      - | 7181 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 7182 | `	0,  /* xReadDir */` |
|      - | 7183 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 7184 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|      - | 7185 | `	0,  /* xLock */` |
|      - | 7186 | `	0,  /* xRewindDir */` |
|      - | 7187 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 7188 | `	PHPStreamData_Trunc, /* xTrunc */` |
|      - | 7189 | `	0,  /* xSync */` |
|      - | 7190 | `	0   /* xStat */` |
|      - | 7191 | `};` |
|      - | 7192 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7193 | `/*` |
|      - | 7194 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 7195 | ` * FALSE otherwise.` |
|      - | 7196 | ` */` |
|    222 | 7197 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      4 | 7198 | `{` |
|      - | 7199 | `#ifndef PH7_DISABLE_DISK_IO` |
|    226 | 7200 | `	return pStream == &sPHP_Stream;` |
|      - | 7201 | `#else` |
|      - | 7202 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7203 | `	return 0;` |
|      - | 7204 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      4 | 7205 | `}` |
|      - | 7206 | `/*` |
|      - | 7207 | ` * Return TRUE if we are dealing with the data:// stream.` |
|      - | 7208 | ` */` |
|    202 | 7209 | `static int is_data_stream(const ph7_io_stream *pStream)` |
|      4 | 7210 | `{` |
|      - | 7211 | `#ifndef PH7_DISABLE_DISK_IO` |
|    206 | 7212 | `	return pStream == &sDATA_Stream;` |
|      - | 7213 | `#else` |
|      - | 7214 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7215 | `	return 0;` |
|      - | 7216 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      4 | 7217 | `}` |
|      - | 7218 | `/*` |
|      - | 7219 | ` * bool stream_isatty(resource $stream)` |
|      - | 7220 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|      - | 7221 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|      - | 7222 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|      - | 7223 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|      - | 7224 | ` */` |
|      6 | 7225 | `static int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7226 | `{` |
|      7 | 7227 | `	int bTty = 0;` |
|      7 | 7228 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7229 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7230 | `		return PH7_OK;` |
|      - | 7231 | `	}` |
|      - | 7232 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7233 | `	{` |
|      7 | 7234 | `		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      7 | 7235 | `		if( !IO_PRIVATE_INVALID(pDev) && is_php_stream(pDev->pStream) ){` |
|      5 | 7236 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|      6 | 7237 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|      4 | 7238 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|      3 | 7239 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|      - | 7240 | `#ifdef __WINNT__` |
|      1 | 7241 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|      - | 7242 | `#else` |
|      4 | 7243 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|      - | 7244 | `#endif` |
|      2 | 7245 | `			}` |
|      2 | 7246 | `		}` |
|      - | 7247 | `	}` |
|      - | 7248 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      7 | 7249 | `	ph7_result_bool(pCtx,bTty);` |
|      7 | 7250 | `	return PH7_OK;` |
|      4 | 7251 | `}` |
|      - | 7252 |  |
|      - | 7253 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7254 | `/*` |
|      - | 7255 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 7256 | ` * [i.e: file://,php://].` |
|      - | 7257 | ` * Note:` |
|      - | 7258 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 7259 | ` *  defined then this function is a no-op.` |
|      - | 7260 | ` */` |
|   3370 | 7261 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 7262 | `{` |
|      - | 7263 | `	/*` |
|      - | 7264 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 7265 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 7266 | `	 */` |
|      - | 7267 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7268 | `	/* VFS: disk I/O related functions */` |
|      - | 7269 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 7270 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 7271 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 7272 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 7273 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 7274 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 7275 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 7276 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 7277 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 7278 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 7279 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 7280 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 7281 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 7282 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 7283 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 7284 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 7285 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 7286 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 7287 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 7288 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 7289 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 7290 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 7291 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 7292 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 7293 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 7294 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 7295 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 7296 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 7297 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 7298 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 7299 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 7300 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 7301 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 7302 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 7303 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 7304 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 7305 | `		{"link",        PH7_vfs_link     },` |
|      - | 7306 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 7307 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 7308 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 7309 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 7310 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 7311 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 7312 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 7313 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 7314 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 7315 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 7316 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 7317 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 7318 | `	};` |
|      - | 7319 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 7320 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 7321 | `	 */` |
|      - | 7322 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 7323 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 7324 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 7325 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 7326 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 7327 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 7328 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 7329 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 7330 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 7331 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 7332 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 7333 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 7334 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 7335 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 7336 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 7337 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 7338 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 7339 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 7340 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 7341 | `		{"file",      PH7_builtin_file   },` |
|      - | 7342 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 7343 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 7344 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 7345 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 7346 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 7347 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 7348 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 7349 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 7350 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 7351 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 7352 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 7353 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 7354 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 7355 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 7356 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 7357 | `#ifdef PH7_ENABLE_NET` |
|      - | 7358 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 7359 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 7360 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 7361 | `#endif` |
|      - | 7362 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 7363 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 7364 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 7365 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 7366 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 7367 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 7368 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 7369 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 7370 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 7371 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7372 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 7373 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 7374 | `	};` |
|   3375 | 7375 | `	const ph7_io_stream *pFileStream = 0;` |
|   3375 | 7376 | `	sxu32 n = 0;` |
|      - | 7377 | `	/* Register disk-related functions */` |
| 165135 | 7378 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 161765 | 7379 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  80885 | 7380 | `	}` |
| 161765 | 7381 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 158395 | 7382 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  79200 | 7383 | `	}` |
|      - | 7384 | `#else` |
|      - | 7385 | `	SXUNUSED(pVm);` |
|      - | 7386 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7387 |  |
|      - | 7388 | `	/*` |
|      - | 7389 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7390 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 7391 | `	 */` |
|      - | 7392 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7393 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 7394 | `		/* Path processing */` |
|      - | 7395 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 7396 | `		{"basename",    PH7_builtin_basename },` |
|      - | 7397 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 7398 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 7399 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 7400 | `		/* ZIP processing */` |
|      - | 7401 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 7402 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 7403 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 7404 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 7405 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 7406 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 7407 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 7408 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 7409 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 7410 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 7411 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 7412 | `	};` |
|  57295 | 7413 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  53925 | 7414 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  26965 | 7415 | `	}` |
|      - | 7416 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7417 |  |
|      - | 7418 | `	/* Install streams if disk I/O is enabled */` |
|      - | 7419 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7420 | `#ifdef __WINNT__` |
|      5 | 7421 | `	pFileStream = &sWinFileStream;` |
|      - | 7422 | `#elif defined(__UNIXES__)` |
|   3370 | 7423 | `	pFileStream = &sUnixFileStream;` |
|      - | 7424 | `#endif` |
|      - | 7425 | `	/* Install the php:// stream */` |
|   3375 | 7426 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3375 | 7427 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 7428 | `#ifdef PH7_ENABLE_NET` |
|   3375 | 7429 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 7430 | `#endif` |
|   3375 | 7431 | `	if( pFileStream ){` |
|      - | 7432 | `		/* Install the file:// stream */` |
|   3375 | 7433 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1685 | 7434 | `	}` |
|      - | 7435 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7436 |  |
|   3375 | 7437 | `	return SXRET_OK;` |
|      5 | 7438 | `}` |
|      - | 7439 | `/*` |
|      - | 7440 | ` * Export the STDIN handle.` |
|      - | 7441 | ` */` |
|      2 | 7442 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 7443 | `{` |
|      - | 7444 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7445 | `	if( pVm->pStdin == 0  ){` |
|      - | 7446 | `		io_private *pIn;` |
|      - | 7447 | `		/* Allocate an IO private instance */` |
|      3 | 7448 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7449 | `		if( pIn == 0 ){` |
|    ! 0 | 7450 | `			return 0;` |
|      - | 7451 | `		}` |
|      3 | 7452 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 7453 | `		/* Initialize the handle */` |
|      3 | 7454 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 7455 | `		/* Install the STDIN stream */` |
|      3 | 7456 | `		pVm->pStdin = pIn;` |
|      3 | 7457 | `		return pIn;` |
|    ! 0 | 7458 | `	}else{` |
|      - | 7459 | `		/* NULL or STDIN */` |
|    ! 0 | 7460 | `		return pVm->pStdin;` |
|      - | 7461 | `	}` |
|      - | 7462 | `#else` |
|      - | 7463 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7464 | `	return 0;` |
|      - | 7465 | `#endif` |
|      2 | 7466 | `}` |
|      - | 7467 | `/*` |
|      - | 7468 | ` * Export the STDOUT handle.` |
|      - | 7469 | ` */` |
|      4 | 7470 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 7471 | `{` |
|      - | 7472 | `#ifndef PH7_DISABLE_DISK_IO` |
|      5 | 7473 | `	if( pVm->pStdout == 0  ){` |
|      - | 7474 | `		io_private *pOut;` |
|      - | 7475 | `		/* Allocate an IO private instance */` |
|      3 | 7476 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7477 | `		if( pOut == 0 ){` |
|    ! 0 | 7478 | `			return 0;` |
|      - | 7479 | `		}` |
|      3 | 7480 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 7481 | `		/* Initialize the handle */` |
|      3 | 7482 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 7483 | `		/* Install the STDOUT stream */` |
|      3 | 7484 | `		pVm->pStdout = pOut;` |
|      3 | 7485 | `		return pOut;` |
|    ! 0 | 7486 | `	}else{` |
|      - | 7487 | `		/* NULL or STDOUT */` |
|      3 | 7488 | `		return pVm->pStdout;` |
|      - | 7489 | `	}` |
|      - | 7490 | `#else` |
|      - | 7491 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7492 | `	return 0;` |
|      - | 7493 | `#endif` |
|      3 | 7494 | `}` |
|      - | 7495 | `/*` |
|      - | 7496 | ` * Export the STDERR handle.` |
|      - | 7497 | ` */` |
|      4 | 7498 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 7499 | `{` |
|      - | 7500 | `#ifndef PH7_DISABLE_DISK_IO` |
|      5 | 7501 | `	if( pVm->pStderr == 0  ){` |
|      - | 7502 | `		io_private *pErr;` |
|      - | 7503 | `		/* Allocate an IO private instance */` |
|      3 | 7504 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7505 | `		if( pErr == 0 ){` |
|    ! 0 | 7506 | `			return 0;` |
|      - | 7507 | `		}` |
|      3 | 7508 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 7509 | `		/* Initialize the handle */` |
|      3 | 7510 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 7511 | `		/* Install the STDERR stream */` |
|      3 | 7512 | `		pVm->pStderr = pErr;` |
|      3 | 7513 | `		return pErr;` |
|    ! 0 | 7514 | `	}else{` |
|      - | 7515 | `		/* NULL or STDERR */` |
|      3 | 7516 | `		return pVm->pStderr;` |
|      - | 7517 | `	}` |
|      - | 7518 | `#else` |
|      - | 7519 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7520 | `	return 0;` |
|      - | 7521 | `#endif` |
|      3 | 7522 | `}` |
|      - | 7523 |  |
