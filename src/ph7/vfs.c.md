# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2459/3639 lines (67.57%)

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
|  19828 |   66 | `static const char * VfsStrerror(int iErr)` |
|      5 |   67 | `{` |
|      - |   68 | `#if defined(_MSC_VER)` |
|      - |   69 | `#pragma warning(push)` |
|      - |   70 | `#pragma warning(disable:4996)` |
|      - |   71 | `#endif` |
|  19833 |   72 | `	return strerror(iErr);` |
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
|  19814 |   83 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |   84 | `{` |
|  29726 |   85 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  19814 |   86 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  19819 |   87 | `}` |
|      4 |   88 | `static void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      2 |   89 | `{` |
|      8 |   90 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open stream: %s",` |
|      4 |   91 | `		ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));` |
|      6 |   92 | `}` |
|      - |   93 | `/*` |
|      - |   94 | ` * bool chdir(string $directory)` |
|      - |   95 | ` *  Change the current directory.` |
|      - |   96 | ` * Parameters` |
|      - |   97 | ` *  $directory` |
|      - |   98 | ` *   The new current directory` |
|      - |   99 | ` * Return` |
|      - |  100 | ` *  TRUE on success or FALSE on failure.` |
|      - |  101 | ` */` |
|  13386 |  102 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  103 | `{` |
|      - |  104 | `	const char *zPath;` |
|      - |  105 | `	ph7_vfs *pVfs;` |
|      - |  106 | `	int rc;` |
|  13391 |  107 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  108 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |  109 | `		ph7_result_bool(pCtx,0);` |
|      6 |  110 | `		return PH7_OK;` |
|      - |  111 | `	}` |
|      - |  112 | `	/* Point to the underlying vfs */` |
|  13387 |  113 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13387 |  114 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  115 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  116 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  117 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  118 | `			ph7_function_name(pCtx)` |
|      - |  119 | `			);` |
|    ! 0 |  120 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  121 | `		return PH7_OK;` |
|      - |  122 | `	}` |
|      - |  123 | `	/* Point to the desired directory */` |
|  13387 |  124 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  125 | `	/* Perform the requested operation */` |
|  13387 |  126 | `	errno = 0;` |
|  13387 |  127 | `	rc = pVfs->xChdir(zPath);` |
|  13387 |  128 | `	if( rc != PH7_OK ){` |
|      - |  129 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      4 |  130 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      2 |  131 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      1 |  132 | `	}` |
|      - |  133 | `	/* IO return value */` |
|  13387 |  134 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13387 |  135 | `	return PH7_OK;` |
|   6698 |  136 | `}` |
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
|     36 |  218 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  219 | `{` |
|      - |  220 | `	const char *zPath;` |
|      - |  221 | `	ph7_vfs *pVfs;` |
|      - |  222 | `	int rc;` |
|     37 |  223 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  224 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  225 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  226 | `		return PH7_OK;` |
|      - |  227 | `	}` |
|      - |  228 | `	/* Point to the underlying vfs */` |
|     37 |  229 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     37 |  230 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  231 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  232 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  233 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  234 | `			ph7_function_name(pCtx)` |
|      - |  235 | `			);` |
|    ! 0 |  236 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  237 | `		return PH7_OK;` |
|      - |  238 | `	}` |
|      - |  239 | `	/* Point to the desired directory */` |
|     37 |  240 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  241 | `	/* Perform the requested operation */` |
|     37 |  242 | `	errno = 0;` |
|     37 |  243 | `	rc = pVfs->xRmdir(zPath);` |
|     37 |  244 | `	if( rc != PH7_OK ){` |
|      3 |  245 | `		VfsThrowSysWarning(pCtx,zPath);` |
|      1 |  246 | `	}` |
|      - |  247 | `	/* IO return value */` |
|     37 |  248 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     37 |  249 | `	return PH7_OK;` |
|     19 |  250 | `}` |
|      - |  251 | `/*` |
|      - |  252 | ` * bool is_dir(string $filename)` |
|      - |  253 | ` *  Tells whether the given filename is a directory.` |
|      - |  254 | ` * Parameters` |
|      - |  255 | ` *  $filename` |
|      - |  256 | ` *   Path to the file.` |
|      - |  257 | ` * Return` |
|      - |  258 | ` *  TRUE on success or FALSE on failure.` |
|      - |  259 | ` */` |
|   8758 |  260 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  261 | `{` |
|      - |  262 | `	const char *zPath;` |
|      - |  263 | `	ph7_vfs *pVfs;` |
|      - |  264 | `	int rc;` |
|   8763 |  265 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  266 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  267 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  268 | `		return PH7_OK;` |
|      - |  269 | `	}` |
|      - |  270 | `	/* Point to the underlying vfs */` |
|   8763 |  271 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8763 |  272 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  273 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  274 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  275 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  276 | `			ph7_function_name(pCtx)` |
|      - |  277 | `			);` |
|    ! 0 |  278 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  279 | `		return PH7_OK;` |
|      - |  280 | `	}` |
|      - |  281 | `	/* Point to the desired directory */` |
|   8763 |  282 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  283 | `	/* Perform the requested operation */` |
|   8763 |  284 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  285 | `	/* IO return value */` |
|   8763 |  286 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8763 |  287 | `	return PH7_OK;` |
|   4384 |  288 | `}` |
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
|     36 |  308 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  309 | `{` |
|     38 |  310 | `	int iRecursive = 0;` |
|      - |  311 | `	const char *zPath;` |
|      - |  312 | `	ph7_vfs *pVfs;` |
|      - |  313 | `	int iMode,rc;` |
|     38 |  314 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  315 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  316 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  317 | `		return PH7_OK;` |
|      - |  318 | `	}` |
|      - |  319 | `	/* Point to the underlying vfs */` |
|     38 |  320 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     38 |  321 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  322 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  323 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  324 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  325 | `			ph7_function_name(pCtx)` |
|      - |  326 | `			);` |
|    ! 0 |  327 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  328 | `		return PH7_OK;` |
|      - |  329 | `	}` |
|      - |  330 | `	/* Point to the desired directory */` |
|     38 |  331 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  332 | `#ifdef __WINNT__` |
|      2 |  333 | `	iMode = 0;` |
|      - |  334 | `#else` |
|      - |  335 | `	/* Assume UNIX */` |
|     36 |  336 | `	iMode = 0777;` |
|      - |  337 | `#endif` |
|     38 |  338 | `	if( nArg > 1 ){` |
|    ! 0 |  339 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  340 | `		if( nArg > 2 ){` |
|    ! 0 |  341 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  342 | `		}` |
|    ! 0 |  343 | `	}` |
|      - |  344 | `	/* Perform the requested operation */` |
|     38 |  345 | `	errno = 0;` |
|     38 |  346 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|     38 |  347 | `	if( rc != PH7_OK ){` |
|      - |  348 | `		/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|    ! 0 |  349 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|    ! 0 |  350 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|    ! 0 |  351 | `	}` |
|      - |  352 | `	/* IO return value */` |
|     38 |  353 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     38 |  354 | `	return PH7_OK;` |
|     20 |  355 | `}` |
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
|  33458 |  535 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  536 | `{` |
|      - |  537 | `	const char *zPath;` |
|      - |  538 | `	ph7_vfs *pVfs;` |
|      - |  539 | `	int rc;` |
|  33463 |  540 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  541 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  542 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  543 | `		return PH7_OK;` |
|      - |  544 | `	}` |
|      - |  545 | `	/* Point to the underlying vfs */` |
|  33463 |  546 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  33463 |  547 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  548 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  549 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  550 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  551 | `			ph7_function_name(pCtx)` |
|      - |  552 | `			);` |
|    ! 0 |  553 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  554 | `		return PH7_OK;` |
|      - |  555 | `	}` |
|      - |  556 | `	/* Point to the desired directory */` |
|  33463 |  557 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  558 | `	/* Perform the requested operation */` |
|  33463 |  559 | `	errno = 0;` |
|  33463 |  560 | `	rc = pVfs->xUnlink(zPath);` |
|  33463 |  561 | `	if( rc != PH7_OK ){` |
|  19817 |  562 | `		VfsThrowSysWarning(pCtx,zPath);` |
|   9906 |  563 | `	}` |
|      - |  564 | `	/* IO return value */` |
|  33463 |  565 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  33463 |  566 | `	return PH7_OK;` |
|  16734 |  567 | `}` |
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
|      2 |  580 | `{` |
|      - |  581 | `	const char *zPath;` |
|      - |  582 | `	ph7_vfs *pVfs;` |
|      - |  583 | `	int iMode;` |
|      - |  584 | `	int rc;` |
|    146 |  585 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  586 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  587 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  588 | `		return PH7_OK;` |
|      - |  589 | `	}` |
|      - |  590 | `	/* Point to the underlying vfs */` |
|    146 |  591 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    146 |  592 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  593 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  594 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  595 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  596 | `			ph7_function_name(pCtx)` |
|      - |  597 | `			);` |
|    ! 0 |  598 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  599 | `		return PH7_OK;` |
|      - |  600 | `	}` |
|      - |  601 | `	/* Point to the desired directory */` |
|    146 |  602 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  603 | `	/* Extract the mode */` |
|    146 |  604 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  605 | `	/* Perform the requested operation */` |
|    146 |  606 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  607 | `	/* IO return value */` |
|    146 |  608 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    146 |  609 | `	return PH7_OK;` |
|     74 |  610 | `}` |
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
|      2 |  807 | `{` |
|      - |  808 | `	const char *zPath;` |
|      - |  809 | `	ph7_vfs *pVfs;` |
|      - |  810 | `	int rc;` |
|    180 |  811 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  812 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  813 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  814 | `		return PH7_OK;` |
|      - |  815 | `	}` |
|      - |  816 | `	/* Point to the underlying vfs */` |
|    180 |  817 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    180 |  818 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  819 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  820 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  821 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  822 | `			ph7_function_name(pCtx)` |
|      - |  823 | `			);` |
|    ! 0 |  824 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  825 | `		return PH7_OK;` |
|      - |  826 | `	}` |
|      - |  827 | `	/* Point to the desired directory */` |
|    180 |  828 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  829 | `	/* Perform the requested operation */` |
|    180 |  830 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  831 | `	/* IO return value */` |
|    180 |  832 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    180 |  833 | `	return PH7_OK;` |
|     91 |  834 | `}` |
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
|   6716 | 1004 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1005 | `{` |
|      - | 1006 | `	const char *zPath;` |
|      - | 1007 | `	ph7_vfs *pVfs;` |
|      - | 1008 | `	int rc;` |
|   6721 | 1009 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1010 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1011 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1012 | `		return PH7_OK;` |
|      - | 1013 | `	}` |
|      - | 1014 | `	/* Point to the underlying vfs */` |
|   6721 | 1015 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6721 | 1016 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1017 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1018 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1019 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1020 | `			ph7_function_name(pCtx)` |
|      - | 1021 | `			);` |
|    ! 0 | 1022 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1023 | `		return PH7_OK;` |
|      - | 1024 | `	}` |
|      - | 1025 | `	/* Point to the desired directory */` |
|   6721 | 1026 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1027 | `	/* Perform the requested operation */` |
|   6721 | 1028 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1029 | `	/* IO return value */` |
|   6721 | 1030 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6721 | 1031 | `	return PH7_OK;` |
|   3363 | 1032 | `}` |
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
|    976 | 1614 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    916 | 1615 | `		zEnd--;` |
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
|  13310 | 1657 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1658 | `{` |
|  13315 | 1659 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1660 | `	SyString *pCur;` |
|      - | 1661 | `	int c,d;` |
|  13315 | 1662 | `	c = d = '/';` |
|      - | 1663 | `#ifdef __WINNT__` |
|      5 | 1664 | `	d = '\\';` |
|      - | 1665 | `#endif` |
|      - | 1666 | `	/* Zero the structure */` |
|  13315 | 1667 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1668 | `	/* Handle special case */` |
|  13315 | 1669 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1670 | `#ifdef __WINNT__` |
|    ! 0 | 1671 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1672 | `#else` |
|    ! 0 | 1673 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1674 | `#endif` |
|    ! 0 | 1675 | `		return SXRET_OK;` |
|      - | 1676 | `	}` |
|      - | 1677 | `	/* Extract the basename */` |
| 359158 | 1678 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 339193 | 1679 | `		zEnd--;` |
|      5 | 1680 | `	}` |
|  13315 | 1681 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13315 | 1682 | `	zEnd = &zPath[nByte];` |
|      - | 1683 | `	/* dirname */` |
|  13315 | 1684 | `	pCur = &pOut->sDir;` |
|  13315 | 1685 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13315 | 1686 | `	if( pCur->nByte > 1 ){` |
|  26625 | 1687 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1688 | `#ifdef __WINNT__` |
|      5 | 1689 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1690 | `#endif` |
|   6660 | 1691 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1692 | `#ifdef __WINNT__` |
|    ! 0 | 1693 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1694 | `#else` |
|    ! 0 | 1695 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1696 | `#endif` |
|    ! 0 | 1697 | `	}` |
|      - | 1698 | `	/* basename/filename */` |
|  13315 | 1699 | `	pCur = &pOut->sBasename;` |
|  13315 | 1700 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13315 | 1701 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1702 | `#ifdef __WINNT__` |
|      5 | 1703 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1704 | `#endif` |
|  13315 | 1705 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13315 | 1706 | `	if( pCur->nByte > 0 ){` |
|      - | 1707 | `		/* extension */` |
|  13315 | 1708 | `		zEnd--;` |
|  66549 | 1709 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  53239 | 1710 | `			zEnd--;` |
|      5 | 1711 | `		}` |
|  13315 | 1712 | `		if( zEnd > pCur->zString ){` |
|  13313 | 1713 | `			zEnd++; /* Jump leading dot */` |
|  13313 | 1714 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1715 | `			/* Fix filename */` |
|  13313 | 1716 | `			pCur = &pOut->sFilename;` |
|  13313 | 1717 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13313 | 1718 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6654 | 1719 | `			}` |
|   6654 | 1720 | `		}` |
|   6655 | 1721 | `	}` |
|  13315 | 1722 | `	return SXRET_OK;` |
|   6660 | 1723 | `}` |
|      - | 1724 | `/*` |
|      - | 1725 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1726 | ` *  See block comment above.` |
|      - | 1727 | ` */` |
|  13310 | 1728 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1729 | `{` |
|      - | 1730 | `	const char *zPath;` |
|      - | 1731 | `	path_info sInfo;` |
|      - | 1732 | `	SyString *pComp;` |
|      - | 1733 | `	int iLen;` |
|  13315 | 1734 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1735 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1736 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1737 | `		return PH7_OK;` |
|      - | 1738 | `	}` |
|      - | 1739 | `	/* Point to the target path */` |
|  13315 | 1740 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13315 | 1741 | `	if( iLen < 1 ){` |
|      - | 1742 | `		/* Empty string */` |
|    ! 0 | 1743 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1744 | `		return PH7_OK;` |
|      - | 1745 | `	}` |
|      - | 1746 | `	/* Extract path info */` |
|  13315 | 1747 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  19969 | 1748 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1749 | `		/* Return path component */` |
|  13313 | 1750 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13313 | 1751 | `		switch(nComp){` |
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
|   3328 | 1770 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6661 | 1771 | `			pComp = &sInfo.sExtension;` |
|   6661 | 1772 | `			if( pComp->nByte > 0 ){` |
|   6659 | 1773 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3332 | 1774 | `			}else{` |
|      - | 1775 | `				/* Expand the empty string */` |
|      3 | 1776 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1777 | `			}` |
|   6661 | 1778 | `			break;` |
|   3324 | 1779 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6653 | 1780 | `			pComp = &sInfo.sFilename;` |
|   6653 | 1781 | `			if( pComp->nByte > 0 ){` |
|   6653 | 1782 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3329 | 1783 | `			}else{` |
|      - | 1784 | `				/* Expand the empty string */` |
|    ! 0 | 1785 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1786 | `			}` |
|   6653 | 1787 | `			break;` |
|    ! 0 | 1788 | `		default:` |
|      - | 1789 | `			/* Expand the empty string */` |
|    ! 0 | 1790 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1791 | `			break;` |
|      - | 1792 | `		}` |
|   6659 | 1793 | `	}else{` |
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
|  13315 | 1843 | `	return PH7_OK;` |
|   6660 | 1844 | `}` |
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
|     20 | 1917 | `static int patternCompare(` |
|      - | 1918 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1919 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1920 | `  const int esc,                    /* The escape character */` |
|      - | 1921 | `  int noCase` |
|      1 | 1922 | `){` |
|      - | 1923 | `  int c, c2;` |
|      - | 1924 | `  int invert;` |
|      - | 1925 | `  int seen;` |
|     21 | 1926 | `  u8 matchOne = '?';` |
|     21 | 1927 | `  u8 matchAll = '*';` |
|     21 | 1928 | `  u8 matchSet = '[';` |
|     21 | 1929 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1930 |  |
|     21 | 1931 | `  if( !zPattern \|\| !zString ) return 0;` |
|     51 | 1932 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     43 | 1933 | `    if( !prevEscape && c==matchAll ){` |
|     16 | 1934 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|      9 | 1935 | `               \|\| c == matchOne ){` |
|    ! 0 | 1936 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1937 | `          return 0;` |
|      - | 1938 | `        }` |
|    ! 0 | 1939 | `      }` |
|      9 | 1940 | `      if( c==0 ){` |
|    ! 0 | 1941 | `        return 1;` |
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
|     35 | 1971 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 1972 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1973 | `        return 0;` |
|    ! 0 | 1974 | `      }` |
|     35 | 1975 | `    }else if( c==matchSet ){` |
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
|     35 | 2007 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 2008 | `      prevEscape = 1;` |
|    ! 0 | 2009 | `    }else{` |
|     35 | 2010 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     35 | 2011 | `      if( noCase ){` |
|      7 | 2012 | `        GlogUpperToLower(c);` |
|      7 | 2013 | `        GlogUpperToLower(c2);` |
|      3 | 2014 | `      }` |
|     35 | 2015 | `      if( c!=c2 ){` |
|      5 | 2016 | `        return 0;` |
|      - | 2017 | `      }` |
|     31 | 2018 | `      prevEscape = 0;` |
|      - | 2019 | `    }` |
|      1 | 2020 | `  }` |
|      9 | 2021 | `  return *zString==0;` |
|     11 | 2022 | `}` |
|      - | 2023 | `/* SPDX-SnippetEnd */` |
|      - | 2024 | `/*` |
|      - | 2025 | ` * Wrapper around patternCompare() defined above.` |
|      - | 2026 | ` * See block comment above for more information.` |
|      - | 2027 | ` */` |
|     12 | 2028 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 2029 | `{` |
|      - | 2030 | `	int rc;` |
|     13 | 2031 | `	if( iEsc < 0 ){` |
|    ! 0 | 2032 | `		iEsc = '\\';` |
|    ! 0 | 2033 | `	}` |
|     13 | 2034 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     13 | 2035 | `	return rc;` |
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
|      4 | 2096 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2097 | `{` |
|      - | 2098 | `	const char *zString,*zPattern;` |
|      5 | 2099 | `	int iEsc = '\\';` |
|      - | 2100 | `	int rc;` |
|      5 | 2101 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2102 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2103 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2104 | `		return PH7_OK;` |
|      - | 2105 | `	}` |
|      - | 2106 | `	/* Extract the pattern and the string */` |
|      5 | 2107 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      5 | 2108 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2109 | `	/* Go globbing */` |
|      5 | 2110 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2111 | `	/* Globbing result */` |
|      5 | 2112 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 2113 | `	return PH7_OK;` |
|      3 | 2114 | `}` |
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
|    230 | 2242 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2243 | `{` |
|      - | 2244 | `	ph7_vfs *pVfs;` |
|      - | 2245 | `	/* Set the empty string as the default return value */` |
|    233 | 2246 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2247 | `	/* Point to the underlying vfs */` |
|    233 | 2248 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    233 | 2249 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
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
|    233 | 2260 | `	pVfs->xTempDir(pCtx);` |
|    233 | 2261 | `	return PH7_OK;` |
|    118 | 2262 | `}` |
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
|     76 | 2300 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2301 | `{` |
|      - | 2302 | `	ph7_int64 nProcessId;` |
|      - | 2303 | `	ph7_vfs *pVfs;` |
|      - | 2304 | `	/* Point to the underlying vfs */` |
|     78 | 2305 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     78 | 2306 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
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
|     78 | 2318 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2319 | `	/* Set the result */` |
|     78 | 2320 | `	ph7_result_int64(pCtx,nProcessId);` |
|     78 | 2321 | `	return PH7_OK;` |
|     40 | 2322 | `}` |
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
|      - | 2537 | `/* Stream-device predicates (devices defined later in this file) */` |
|      - | 2538 | `static int is_php_stream(const ph7_io_stream *pStream);` |
|      - | 2539 | `static int is_data_stream(const ph7_io_stream *pStream);` |
|      - | 2540 | `/* Make sure we are dealing with a valid io_private instance */` |
|      - | 2541 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|      - | 2542 | `/* Forward declaration */` |
|      - | 2543 | `static void ResetIOPrivate(io_private *pDev);` |
|      - | 2544 | `/*` |
|      - | 2545 | ` * Return the PHP resource-type name for a raw resource handle.` |
|      - | 2546 | ` * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the` |
|      - | 2547 | ` * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as` |
|      - | 2548 | ` * "stream"; anything else is "Unknown". The magic probe mirrors the` |
|      - | 2549 | ` * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.` |
|      - | 2550 | ` */` |
|      4 | 2551 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      1 | 2552 | `{` |
|      5 | 2553 | `	io_private *pDev = (io_private *)pResource;` |
|      5 | 2554 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|      5 | 2555 | `		return "stream";` |
|      - | 2556 | `	}` |
|    ! 0 | 2557 | `	return "Unknown";` |
|      3 | 2558 | `}` |
|      - | 2559 | `/*` |
|      - | 2560 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - | 2561 | ` *  Truncates a file to a given length.` |
|      - | 2562 | ` * Parameters` |
|      - | 2563 | ` *  $handle` |
|      - | 2564 | ` *   The file pointer.` |
|      - | 2565 | ` *   Note:` |
|      - | 2566 | ` *    The handle must be open for writing.` |
|      - | 2567 | ` * $size` |
|      - | 2568 | ` *   The size to truncate to.` |
|      - | 2569 | ` * Return` |
|      - | 2570 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2571 | ` */` |
|      6 | 2572 | `static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2573 | `{` |
|      - | 2574 | `	const ph7_io_stream *pStream;` |
|      - | 2575 | `	io_private *pDev;` |
|      - | 2576 | `	int rc;` |
|      7 | 2577 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2578 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2579 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2580 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2581 | `		return PH7_OK;` |
|      - | 2582 | `	}` |
|      - | 2583 | `	/* Extract our private data */` |
|      7 | 2584 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2585 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2586 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2587 | `		/*Expecting an IO handle */` |
|    ! 0 | 2588 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2590 | `		return PH7_OK;` |
|      - | 2591 | `	}` |
|      - | 2592 | `	/* Point to the target IO stream device */` |
|      7 | 2593 | `	pStream = pDev->pStream;` |
|      7 | 2594 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 | 2595 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2596 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2597 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2598 | `			);` |
|    ! 0 | 2599 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2600 | `		return PH7_OK;` |
|      - | 2601 | `	}` |
|      - | 2602 | `	/* Perform the requested operation */` |
|      7 | 2603 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      7 | 2604 | `	if( rc == PH7_OK ){` |
|      - | 2605 | `		/* Discard buffered data */` |
|      7 | 2606 | `		ResetIOPrivate(pDev);` |
|      3 | 2607 | `	}` |
|      - | 2608 | `	/* IO result */` |
|      7 | 2609 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 | 2610 | `	return PH7_OK;` |
|      4 | 2611 | `}` |
|      - | 2612 | `/*` |
|      - | 2613 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - | 2614 | ` *  Seeks on a file pointer.` |
|      - | 2615 | ` * Parameters` |
|      - | 2616 | ` *  $handle` |
|      - | 2617 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - | 2618 | ` * $offset` |
|      - | 2619 | ` *   The offset.` |
|      - | 2620 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - | 2621 | ` *   value in offset and set whence to SEEK_END.` |
|      - | 2622 | ` *   whence` |
|      - | 2623 | ` *   whence values are:` |
|      - | 2624 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - | 2625 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - | 2626 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - | 2627 | ` * Return` |
|      - | 2628 | ` *  0 on success,-1 on failure` |
|      - | 2629 | ` */` |
|     10 | 2630 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2631 | `{` |
|      - | 2632 | `	const ph7_io_stream *pStream;` |
|      - | 2633 | `	io_private *pDev;` |
|      - | 2634 | `	ph7_int64 iOfft;` |
|      - | 2635 | `	int whence;` |
|      - | 2636 | `	int rc;` |
|     12 | 2637 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2638 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2639 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2640 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2641 | `		return PH7_OK;` |
|      - | 2642 | `	}` |
|      - | 2643 | `	/* Extract our private data */` |
|     12 | 2644 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2645 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     12 | 2646 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2647 | `		/*Expecting an IO handle */` |
|    ! 0 | 2648 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2649 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2650 | `		return PH7_OK;` |
|      - | 2651 | `	}` |
|      - | 2652 | `	/* Point to the target IO stream device */` |
|     12 | 2653 | `	pStream = pDev->pStream;` |
|     12 | 2654 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2655 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2656 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2657 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2658 | `			);` |
|    ! 0 | 2659 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2660 | `		return PH7_OK;` |
|      - | 2661 | `	}` |
|      - | 2662 | `	/* Extract the offset */` |
|     12 | 2663 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     12 | 2664 | `	whence = 0;/* SEEK_SET */` |
|     12 | 2665 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      3 | 2666 | `		whence = ph7_value_to_int(apArg[2]);` |
|      1 | 2667 | `	}` |
|      - | 2668 | `	/* Perform the requested operation */` |
|     12 | 2669 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     12 | 2670 | `	if( rc == PH7_OK ){` |
|      - | 2671 | `		/* Ignore buffered data */` |
|     12 | 2672 | `		ResetIOPrivate(pDev);` |
|      5 | 2673 | `	}` |
|      - | 2674 | `	/* IO result */` |
|     12 | 2675 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     12 | 2676 | `	return PH7_OK;` |
|      7 | 2677 | `}` |
|      - | 2678 | `/*` |
|      - | 2679 | ` * int64 ftell(resource $handle)` |
|      - | 2680 | ` *  Returns the current position of the file read/write pointer.` |
|      - | 2681 | ` * Parameters` |
|      - | 2682 | ` *  $handle` |
|      - | 2683 | ` *   The file pointer.` |
|      - | 2684 | ` * Return` |
|      - | 2685 | ` *  Returns the position of the file pointer referenced by handle` |
|      - | 2686 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - | 2687 | ` *  FALSE is returned on failure.` |
|      - | 2688 | ` */` |
|     12 | 2689 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2690 | `{` |
|      - | 2691 | `	const ph7_io_stream *pStream;` |
|      - | 2692 | `	io_private *pDev;` |
|      - | 2693 | `	ph7_int64 iOfft;` |
|     14 | 2694 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2695 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2696 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2697 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2698 | `		return PH7_OK;` |
|      - | 2699 | `	}` |
|      - | 2700 | `	/* Extract our private data */` |
|     14 | 2701 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2702 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     14 | 2703 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2704 | `		/*Expecting an IO handle */` |
|    ! 0 | 2705 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2706 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2707 | `		return PH7_OK;` |
|      - | 2708 | `	}` |
|      - | 2709 | `	/* Point to the target IO stream device */` |
|     14 | 2710 | `	pStream = pDev->pStream;` |
|     14 | 2711 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2712 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2713 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2714 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2715 | `			);` |
|    ! 0 | 2716 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2717 | `		return PH7_OK;` |
|      - | 2718 | `	}` |
|      - | 2719 | `	/* Perform the requested operation */` |
|     14 | 2720 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2721 | `	/* IO result */` |
|     14 | 2722 | `	ph7_result_int64(pCtx,iOfft);` |
|     14 | 2723 | `	return PH7_OK;` |
|      8 | 2724 | `}` |
|      - | 2725 | `/*` |
|      - | 2726 | ` * bool rewind(resource $handle)` |
|      - | 2727 | ` *  Rewind the position of a file pointer.` |
|      - | 2728 | ` * Parameters` |
|      - | 2729 | ` *  $handle` |
|      - | 2730 | ` *   The file pointer.` |
|      - | 2731 | ` * Return` |
|      - | 2732 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2733 | ` */` |
|     14 | 2734 | `static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2735 | `{` |
|      - | 2736 | `	const ph7_io_stream *pStream;` |
|      - | 2737 | `	io_private *pDev;` |
|      - | 2738 | `	int rc;` |
|     15 | 2739 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2740 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2741 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2742 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2743 | `		return PH7_OK;` |
|      - | 2744 | `	}` |
|      - | 2745 | `	/* Extract our private data */` |
|     15 | 2746 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2747 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     15 | 2748 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2749 | `		/*Expecting an IO handle */` |
|    ! 0 | 2750 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2751 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2752 | `		return PH7_OK;` |
|      - | 2753 | `	}` |
|      - | 2754 | `	/* Point to the target IO stream device */` |
|     15 | 2755 | `	pStream = pDev->pStream;` |
|     15 | 2756 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2757 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2758 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2759 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2760 | `			);` |
|    ! 0 | 2761 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2762 | `		return PH7_OK;` |
|      - | 2763 | `	}` |
|      - | 2764 | `	/* Perform the requested operation */` |
|     15 | 2765 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|     15 | 2766 | `	if( rc == PH7_OK ){` |
|      - | 2767 | `		/* Ignore buffered data */` |
|     15 | 2768 | `		ResetIOPrivate(pDev);` |
|      7 | 2769 | `	}` |
|      - | 2770 | `	/* IO result */` |
|     15 | 2771 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     15 | 2772 | `	return PH7_OK;` |
|      8 | 2773 | `}` |
|      - | 2774 | `/*` |
|      - | 2775 | ` * bool fflush(resource $handle)` |
|      - | 2776 | ` *  Flushes the output to a file.` |
|      - | 2777 | ` * Parameters` |
|      - | 2778 | ` *  $handle` |
|      - | 2779 | ` *   The file pointer.` |
|      - | 2780 | ` * Return` |
|      - | 2781 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2782 | ` */` |
|      2 | 2783 | `static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2784 | `{` |
|      - | 2785 | `	const ph7_io_stream *pStream;` |
|      - | 2786 | `	io_private *pDev;` |
|      - | 2787 | `	int rc;` |
|      3 | 2788 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2789 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2790 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2791 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2792 | `		return PH7_OK;` |
|      - | 2793 | `	}` |
|      - | 2794 | `	/* Extract our private data */` |
|      3 | 2795 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2796 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2797 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2798 | `		/*Expecting an IO handle */` |
|    ! 0 | 2799 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2800 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2801 | `		return PH7_OK;` |
|      - | 2802 | `	}` |
|      - | 2803 | `	/* Point to the target IO stream device */` |
|      3 | 2804 | `	pStream = pDev->pStream;` |
|      3 | 2805 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 | 2806 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2807 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2808 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2809 | `			);` |
|    ! 0 | 2810 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2811 | `		return PH7_OK;` |
|      - | 2812 | `	}` |
|      - | 2813 | `	/* Perform the requested operation */` |
|      3 | 2814 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - | 2815 | `	/* IO result */` |
|      3 | 2816 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2817 | `	return PH7_OK;` |
|      2 | 2818 | `}` |
|      - | 2819 | `/*` |
|      - | 2820 | ` * bool feof(resource $handle)` |
|      - | 2821 | ` *  Tests for end-of-file on a file pointer.` |
|      - | 2822 | ` * Parameters` |
|      - | 2823 | ` *  $handle` |
|      - | 2824 | ` *   The file pointer.` |
|      - | 2825 | ` * Return` |
|      - | 2826 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - | 2827 | ` */` |
|  10498 | 2828 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2829 | `{` |
|      - | 2830 | `	const ph7_io_stream *pStream;` |
|      - | 2831 | `	io_private *pDev;` |
|      - | 2832 | `	int rc;` |
|  10503 | 2833 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2834 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2835 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2836 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2837 | `		return PH7_OK;` |
|      - | 2838 | `	}` |
|      - | 2839 | `	/* Extract our private data */` |
|  10503 | 2840 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2841 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10503 | 2842 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2843 | `		/*Expecting an IO handle */` |
|    ! 0 | 2844 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2845 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2846 | `		return PH7_OK;` |
|      - | 2847 | `	}` |
|      - | 2848 | `	/* Point to the target IO stream device */` |
|  10503 | 2849 | `	pStream = pDev->pStream;` |
|  10503 | 2850 | `	if( pStream == 0 ){` |
|    ! 0 | 2851 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2852 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2853 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2854 | `			);` |
|    ! 0 | 2855 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2856 | `		return PH7_OK;` |
|      - | 2857 | `	}` |
|  10503 | 2858 | `	rc = SXERR_EOF;` |
|      - | 2859 | `	/* Perform the requested operation */` |
|  10503 | 2860 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2861 | `		/* Data is available */` |
|   4807 | 2862 | `		rc = PH7_OK;` |
|   2406 | 2863 | `	}else{` |
|      - | 2864 | `		char zBuf[4096];` |
|      - | 2865 | `		ph7_int64 n;` |
|      - | 2866 | `		/* Perform a buffered read */` |
|   5701 | 2867 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5701 | 2868 | `		if( n > 0 ){` |
|      - | 2869 | `			/* Copy buffered data */` |
|   1859 | 2870 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1859 | 2871 | `			rc = PH7_OK;` |
|    927 | 2872 | `		}` |
|      - | 2873 | `	}` |
|      - | 2874 | `	/* EOF or not */` |
|  10503 | 2875 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10503 | 2876 | `	return PH7_OK;` |
|   5254 | 2877 | `}` |
|      - | 2878 | `/*` |
|      - | 2879 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2880 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2881 | ` * [i.e: IO error ] or EOF.` |
|      - | 2882 | ` */` |
|     36 | 2883 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 | 2884 | `{` |
|     38 | 2885 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     38 | 2886 | `	char *zBuf = (char *)pBuf;` |
|      - | 2887 | `	ph7_int64 n,nRead;` |
|     38 | 2888 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     38 | 2889 | `	if( n > 0 ){` |
|      2 | 2890 | `		if( n > nLen ){` |
|    ! 0 | 2891 | `			n = nLen;` |
|    ! 0 | 2892 | `		}` |
|      - | 2893 | `		/* Copy the buffered data */` |
|      2 | 2894 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2895 | `		/* Update the read offset */` |
|      2 | 2896 | `		pDev->nOfft += (sxu32)n;` |
|      2 | 2897 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2898 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      2 | 2899 | `			SyBlobReset(&pDev->sBuffer);` |
|      2 | 2900 | `			pDev->nOfft = 0;` |
|      1 | 2901 | `		}` |
|      2 | 2902 | `		nLen -= n;` |
|      2 | 2903 | `		if( nLen < 1 ){` |
|      - | 2904 | `			/* All done */` |
|    ! 0 | 2905 | `			return n;` |
|      - | 2906 | `		}` |
|      - | 2907 | `		/* Advance the cursor */` |
|      2 | 2908 | `		zBuf += n;` |
|      1 | 2909 | `	}` |
|      - | 2910 | `	/* Read without buffering */` |
|     38 | 2911 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     38 | 2912 | `	if( nRead > 0 ){` |
|     34 | 2913 | `		n += nRead;` |
|     21 | 2914 | `	}else if( n < 1 ){` |
|      - | 2915 | `		/* EOF or IO error */` |
|      3 | 2916 | `		return nRead;` |
|      - | 2917 | `	}` |
|     36 | 2918 | `	return n;` |
|     20 | 2919 | `}` |
|      - | 2920 | `/*` |
|      - | 2921 | ` * Extract a single line from the buffered input.` |
|      - | 2922 | ` */` |
|   6722 | 2923 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 | 2924 | `{` |
|      - | 2925 | `	const char *zIn,*zEnd,*zPtr;` |
|   6727 | 2926 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6727 | 2927 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6727 | 2928 | `	zPtr = zIn;` |
| 391644 | 2929 | `	while( zIn < zEnd ){` |
| 391540 | 2930 | `		if( zIn[0] == '\n' ){` |
|      - | 2931 | `			/* Line found */` |
|   6623 | 2932 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6623 | 2933 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6623 | 2934 | `			*pzLine = zPtr;` |
|   6623 | 2935 | `			return SXRET_OK;` |
|      - | 2936 | `		}` |
| 384922 | 2937 | `		zIn++;` |
|      5 | 2938 | `	}` |
|      - | 2939 | `	/* No line were found */` |
|    109 | 2940 | `	return SXERR_NOTFOUND;` |
|   3366 | 2941 | `}` |
|      - | 2942 | `/*` |
|      - | 2943 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2944 | ` */` |
|   6726 | 2945 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 | 2946 | `{` |
|   6731 | 2947 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2948 | `	char zBuf[8192];` |
|      - | 2949 | `	ph7_int64 n;` |
|      - | 2950 | `	sxi32 rc;` |
|   6731 | 2951 | `	n = 0;` |
|   6731 | 2952 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2953 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     73 | 2954 | `		SyBlobReset(&pDev->sBuffer);` |
|     73 | 2955 | `		pDev->nOfft = 0;` |
|     34 | 2956 | `	}` |
|   6731 | 2957 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2958 | `		/* Check if there is a line */` |
|   6663 | 2959 | `		rc = GetLine(pDev,&n,pzData);` |
|   6663 | 2960 | `		if( rc == SXRET_OK ){` |
|      - | 2961 | `			/* Got line,update the cursor  */` |
|   6563 | 2962 | `			pDev->nOfft += (sxu32)n;` |
|   6563 | 2963 | `			return n;` |
|      - | 2964 | `		}` |
|     50 | 2965 | `	}` |
|      - | 2966 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2967 | `	 * limit is reached.` |
|      - | 2968 | `	 */` |
|     86 | 2969 | `	for(;;){` |
|    177 | 2970 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    177 | 2971 | `		if( n < 1 ){` |
|      - | 2972 | `			/* EOF or IO error */` |
|    113 | 2973 | `			break;` |
|      - | 2974 | `		}` |
|      - | 2975 | `		/* Append the data just read */` |
|     67 | 2976 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2977 | `		/* Try to extract a line */` |
|     67 | 2978 | `		rc = GetLine(pDev,&n,pzData);` |
|     67 | 2979 | `		if( rc == SXRET_OK ){` |
|      - | 2980 | `			/* Got one,return immediately */` |
|     63 | 2981 | `			pDev->nOfft += (sxu32)n;` |
|     63 | 2982 | `			return n;` |
|      - | 2983 | `		}` |
|      5 | 2984 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - | 2985 | `			/* Read limit reached,return the available data */` |
|    ! 0 | 2986 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 | 2987 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2988 | `			/* Reset the working buffer */` |
|    ! 0 | 2989 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2990 | `			pDev->nOfft = 0;` |
|    ! 0 | 2991 | `			return n;` |
|      - | 2992 | `		}` |
|      1 | 2993 | `	}` |
|    113 | 2994 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2995 | `		/* Read limit reached,return the available data */` |
|    109 | 2996 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    109 | 2997 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2998 | `		/* Reset the working buffer */` |
|    109 | 2999 | `		SyBlobReset(&pDev->sBuffer);` |
|    109 | 3000 | `		pDev->nOfft = 0;` |
|     52 | 3001 | `	}` |
|    113 | 3002 | `	return n;` |
|   3368 | 3003 | `}` |
|      - | 3004 | `/*` |
|      - | 3005 | ` * Open an IO stream handle.` |
|      - | 3006 | ` * Notes on stream:` |
|      - | 3007 | ` * According to the PHP reference manual.` |
|      - | 3008 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - | 3009 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - | 3010 | ` * to an arbitrary locations within the stream.` |
|      - | 3011 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - | 3012 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - | 3013 | ` * on a remote server.` |
|      - | 3014 | ` * A stream is referenced as: scheme://target` |
|      - | 3015 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - | 3016 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - | 3017 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - | 3018 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - | 3019 | ` *  with a path appended.` |
|      - | 3020 | ` *` |
|      - | 3021 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - | 3022 | ` * Please refer to the official documentation for a full discussion.` |
|      - | 3023 | ` * This function return a handle on success. Otherwise null.` |
|      - | 3024 | ` */` |
|  30218 | 3025 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 3026 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 | 3027 | `{` |
|  30223 | 3028 | `	void *pHandle = 0; /* cc warning */` |
|      - | 3029 | `	SyString sFile;` |
|      - | 3030 | `	ph7_value sDummy;` |
|      - | 3031 | `	int rc;` |
|  30223 | 3032 | `	if( pStream == 0 ){` |
|      - | 3033 | `		/* No such stream device */` |
|    ! 0 | 3034 | `		return 0;` |
|      - | 3035 | `	}` |
|  30223 | 3036 | `	if( pResource == 0 ){` |
|      - | 3037 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - | 3038 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - | 3039 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - | 3040 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - | 3041 | `		 * reads it during the call, and file:// ignores it. */` |
|  30203 | 3042 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  30203 | 3043 | `		pResource = &sDummy;` |
|  15099 | 3044 | `	}` |
|  30223 | 3045 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  30223 | 3046 | `	if( use_include ){` |
|   9656 | 3047 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 3048 | `#ifdef __WINNT__` |
|      - | 3049 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 3050 | `#endif` |
|   9634 | 3051 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9630 | 3052 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 3053 | `				/*  Open the file directly */` |
|     27 | 3054 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     14 | 3055 | `		}else{` |
|      - | 3056 | `			SyString *pPath;` |
|      - | 3057 | `			SyBlob sWorker;` |
|      - | 3058 | `#ifdef __WINNT__` |
|      - | 3059 | `			static const int c = '\\';` |
|      - | 3060 | `#else` |
|      - | 3061 | `			static const int c = '/';` |
|      - | 3062 | `#endif` |
|      - | 3063 | `			/* Init the path builder working buffer */` |
|   9634 | 3064 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 3065 | `			/* Build a path from the set of include path */` |
|   9634 | 3066 | `			SySetResetCursor(&pVm->aPaths);` |
|   9634 | 3067 | `			rc = SXERR_IO;` |
|   9640 | 3068 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 3069 | `				/* Build full path */` |
|   9634 | 3070 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 3071 | `				/* Append null terminator */` |
|   9634 | 3072 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 3073 | `					continue;` |
|      - | 3074 | `				}` |
|      - | 3075 | `				/* Try to open the file */` |
|   9634 | 3076 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9634 | 3077 | `				if( rc == PH7_OK ){` |
|   9627 | 3078 | `					if( bPushInclude ){` |
|      - | 3079 | `						/* Mark as included */` |
|   9627 | 3080 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4812 | 3081 | `					}` |
|   9627 | 3082 | `					break;` |
|      - | 3083 | `				}` |
|      - | 3084 | `				/* Reset the working buffer */` |
|      8 | 3085 | `				SyBlobReset(&sWorker);` |
|      - | 3086 | `				/* Check the next path */` |
|      2 | 3087 | `			}` |
|   9634 | 3088 | `			SyBlobRelease(&sWorker);` |
|      - | 3089 | `		}` |
|   9660 | 3090 | `		if( rc == PH7_OK ){` |
|   9653 | 3091 | `			if( bPushInclude ){` |
|      - | 3092 | `				/* Mark as included */` |
|   9653 | 3093 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   4825 | 3094 | `			}` |
|   4825 | 3095 | `		}` |
|   4832 | 3096 | `	}else{` |
|      - | 3097 | `		/* Open the URI direcly */` |
|  20567 | 3098 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 3099 | `	}` |
|  30223 | 3100 | `	if( rc != PH7_OK ){` |
|      - | 3101 | `		/* IO error */` |
|     16 | 3102 | `		return 0;` |
|      - | 3103 | `	}` |
|      - | 3104 | `	/* Return the file handle */` |
|  30211 | 3105 | `	return pHandle;` |
|  15114 | 3106 | `}` |
|      - | 3107 | `/*` |
|      - | 3108 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 3109 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 3110 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 3111 | ` */` |
|   9644 | 3112 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      3 | 3113 | `{` |
|      - | 3114 | `	ph7_int64 nRead;` |
|      - | 3115 | `	char zBuf[8192]; /* 8K */` |
|      - | 3116 | `	int rc;` |
|      - | 3117 | `	/* Perform the requested operation */` |
|   9644 | 3118 | `	for(;;){` |
|  19291 | 3119 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  19291 | 3120 | `		if( nRead < 1 ){` |
|      - | 3121 | `			/* EOF or IO error */` |
|   9647 | 3122 | `			break;` |
|      - | 3123 | `		}` |
|      - | 3124 | `		/* Append contents */` |
|   9647 | 3125 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9647 | 3126 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3127 | `			break;` |
|      - | 3128 | `		}` |
|      3 | 3129 | `	}` |
|   9647 | 3130 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      3 | 3131 | `}` |
|      - | 3132 | `/*` |
|      - | 3133 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 3134 | ` */` |
|  30304 | 3135 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 3136 | `{` |
|  30309 | 3137 | `	if( pStream->xClose ){` |
|  30309 | 3138 | `		pStream->xClose(pHandle);` |
|  15152 | 3139 | `	}` |
|  30309 | 3140 | `}` |
|      - | 3141 | `/*` |
|      - | 3142 | ` * string fgetc(resource $handle)` |
|      - | 3143 | ` *  Gets a character from the given file pointer.` |
|      - | 3144 | ` * Parameters` |
|      - | 3145 | ` *  $handle` |
|      - | 3146 | ` *   The file pointer.` |
|      - | 3147 | ` * Return` |
|      - | 3148 | ` *  Returns a string containing a single character read from the file` |
|      - | 3149 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3150 | ` * WARNING` |
|      - | 3151 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3152 | ` */` |
|      4 | 3153 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3154 | `{` |
|      - | 3155 | `	const ph7_io_stream *pStream;` |
|      - | 3156 | `	io_private *pDev;` |
|      - | 3157 | `	int c,n;` |
|      5 | 3158 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3159 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3160 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3161 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3162 | `		return PH7_OK;` |
|      - | 3163 | `	}` |
|      - | 3164 | `	/* Extract our private data */` |
|      5 | 3165 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3166 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3167 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3168 | `		/*Expecting an IO handle */` |
|    ! 0 | 3169 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3170 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3171 | `		return PH7_OK;` |
|      - | 3172 | `	}` |
|      - | 3173 | `	/* Point to the target IO stream device */` |
|      5 | 3174 | `	pStream = pDev->pStream;` |
|      5 | 3175 | `	if( pStream == 0  ){` |
|    ! 0 | 3176 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3177 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3178 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3179 | `			);` |
|    ! 0 | 3180 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3181 | `		return PH7_OK;` |
|      - | 3182 | `	}` |
|      - | 3183 | `	/* Perform the requested operation */` |
|      5 | 3184 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3185 | `	/* IO result */` |
|      5 | 3186 | `	if( n < 1 ){` |
|      - | 3187 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3189 | `	}else{` |
|      - | 3190 | `		/* Return the string holding the character */` |
|      5 | 3191 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3192 | `	}` |
|      5 | 3193 | `	return PH7_OK;` |
|      3 | 3194 | `}` |
|      - | 3195 | `/*` |
|      - | 3196 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3197 | ` *  Gets line from file pointer.` |
|      - | 3198 | ` * Parameters` |
|      - | 3199 | ` *  $handle` |
|      - | 3200 | ` *   The file pointer.` |
|      - | 3201 | ` * $length` |
|      - | 3202 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3203 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3204 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3205 | ` *  the end of the line.` |
|      - | 3206 | ` * Return` |
|      - | 3207 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3208 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3209 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3210 | ` */` |
|   6716 | 3211 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3212 | `{` |
|      - | 3213 | `	const ph7_io_stream *pStream;` |
|      - | 3214 | `	const char *zLine;` |
|      - | 3215 | `	io_private *pDev;` |
|      - | 3216 | `	ph7_int64 n,nLen;` |
|   6721 | 3217 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3218 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3219 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3220 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3221 | `		return PH7_OK;` |
|      - | 3222 | `	}` |
|      - | 3223 | `	/* Extract our private data */` |
|   6721 | 3224 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3225 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6721 | 3226 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3227 | `		/*Expecting an IO handle */` |
|    ! 0 | 3228 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3229 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3230 | `		return PH7_OK;` |
|      - | 3231 | `	}` |
|      - | 3232 | `	/* Point to the target IO stream device */` |
|   6721 | 3233 | `	pStream = pDev->pStream;` |
|   6721 | 3234 | `	if( pStream == 0  ){` |
|    ! 0 | 3235 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3236 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3237 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3238 | `			);` |
|    ! 0 | 3239 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3240 | `		return PH7_OK;` |
|      - | 3241 | `	}` |
|   6721 | 3242 | `	nLen = -1;` |
|   6721 | 3243 | `	if( nArg > 1 ){` |
|      - | 3244 | `		/* Maximum data to read */` |
|    ! 0 | 3245 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3246 | `	}` |
|      - | 3247 | `	/* Perform the requested operation */` |
|   6721 | 3248 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6721 | 3249 | `	if( n < 1 ){` |
|      - | 3250 | `		/* EOF or IO error,return FALSE */` |
|      7 | 3251 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3252 | `	}else{` |
|      - | 3253 | `		/* Return the freshly extracted line */` |
|   6719 | 3254 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3255 | `	}` |
|   6721 | 3256 | `	return PH7_OK;` |
|   3363 | 3257 | `}` |
|      - | 3258 | `/*` |
|      - | 3259 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3260 | ` *  Binary-safe file read.` |
|      - | 3261 | ` * Parameters` |
|      - | 3262 | ` *  $handle` |
|      - | 3263 | ` *   The file pointer.` |
|      - | 3264 | ` * $length` |
|      - | 3265 | ` *  Up to length number of bytes read.` |
|      - | 3266 | ` * Return` |
|      - | 3267 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3268 | ` */` |
|     28 | 3269 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3270 | `{` |
|      - | 3271 | `	const ph7_io_stream *pStream;` |
|      - | 3272 | `	io_private *pDev;` |
|      - | 3273 | `	ph7_int64 nRead;` |
|      - | 3274 | `	void *pBuf;` |
|      - | 3275 | `	int nLen;` |
|     30 | 3276 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3277 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3278 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3279 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3280 | `		return PH7_OK;` |
|      - | 3281 | `	}` |
|      - | 3282 | `	/* Extract our private data */` |
|     30 | 3283 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3284 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     30 | 3285 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3286 | `		/*Expecting an IO handle */` |
|    ! 0 | 3287 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3288 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3289 | `		return PH7_OK;` |
|      - | 3290 | `	}` |
|      - | 3291 | `	/* Point to the target IO stream device */` |
|     30 | 3292 | `	pStream = pDev->pStream;` |
|     30 | 3293 | `	if( pStream == 0  ){` |
|    ! 0 | 3294 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3295 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3296 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3297 | `			);` |
|    ! 0 | 3298 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3299 | `		return PH7_OK;` |
|      - | 3300 | `	}` |
|     30 | 3301 | `        nLen = 4096;` |
|     30 | 3302 | `	if( nArg > 1 ){` |
|     30 | 3303 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     30 | 3304 | `	  if( nLen < 1 ){` |
|      - | 3305 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3306 | `		nLen = 4096;` |
|    ! 0 | 3307 | `	  }` |
|     14 | 3308 | `        }` |
|      - | 3309 | `	/* Allocate enough buffer */` |
|     30 | 3310 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     30 | 3311 | `	if( pBuf == 0 ){` |
|    ! 0 | 3312 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3313 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3314 | `		return PH7_OK;` |
|      - | 3315 | `	}` |
|      - | 3316 | `	/* Perform the requested operation */` |
|     30 | 3317 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     30 | 3318 | `	if( nRead < 1 ){` |
|      - | 3319 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3320 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3321 | `	}else{` |
|      - | 3322 | `		/* Make a copy of the data just read */` |
|     30 | 3323 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3324 | `	}` |
|      - | 3325 | `	/* Release the buffer */` |
|     30 | 3326 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     30 | 3327 | `	return PH7_OK;` |
|     16 | 3328 | `}` |
|      - | 3329 | `/*` |
|      - | 3330 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3331 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3332 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3333 | ` * Parameters` |
|      - | 3334 | ` * $handle` |
|      - | 3335 | ` *   The file pointer.` |
|      - | 3336 | ` * $length` |
|      - | 3337 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3338 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3339 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3340 | ` *  the end of the line.` |
|      - | 3341 | ` * $delimiter` |
|      - | 3342 | ` *   Set the field delimiter (one character only).` |
|      - | 3343 | ` * $enclosure` |
|      - | 3344 | ` *   Set the field enclosure character (one character only).` |
|      - | 3345 | ` * $escape` |
|      - | 3346 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3347 | ` * Return` |
|      - | 3348 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3349 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3350 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3351 | ` */` |
|      2 | 3352 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3353 | `{` |
|      - | 3354 | `	const ph7_io_stream *pStream;` |
|      - | 3355 | `	const char *zLine;` |
|      - | 3356 | `	io_private *pDev;` |
|      - | 3357 | `	ph7_int64 n,nLen;` |
|      3 | 3358 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3359 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3360 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3361 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3362 | `		return PH7_OK;` |
|      - | 3363 | `	}` |
|      - | 3364 | `	/* Extract our private data */` |
|      3 | 3365 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3366 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3367 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3368 | `		/*Expecting an IO handle */` |
|    ! 0 | 3369 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3370 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3371 | `		return PH7_OK;` |
|      - | 3372 | `	}` |
|      - | 3373 | `	/* Point to the target IO stream device */` |
|      3 | 3374 | `	pStream = pDev->pStream;` |
|      3 | 3375 | `	if( pStream == 0  ){` |
|    ! 0 | 3376 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3377 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3378 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3379 | `			);` |
|    ! 0 | 3380 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3381 | `		return PH7_OK;` |
|      - | 3382 | `	}` |
|      3 | 3383 | `	nLen = -1;` |
|      3 | 3384 | `	if( nArg > 1 ){` |
|      - | 3385 | `		/* Maximum data to read */` |
|      3 | 3386 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3387 | `	}` |
|      - | 3388 | `	/* Perform the requested operation */` |
|      3 | 3389 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3390 | `	if( n < 1 ){` |
|      - | 3391 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3393 | `	}else{` |
|      - | 3394 | `		ph7_value *pArray;` |
|      3 | 3395 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3396 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3397 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3398 | `		if( nArg > 2 ){` |
|      - | 3399 | `			const char *zPtr;` |
|      - | 3400 | `			int i;` |
|      3 | 3401 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3402 | `				/* Extract the delimiter */` |
|      3 | 3403 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3404 | `				if( i > 0 ){` |
|      3 | 3405 | `					delim = zPtr[0];` |
|      1 | 3406 | `				}` |
|      1 | 3407 | `			}` |
|      3 | 3408 | `			if( nArg > 3 ){` |
|      3 | 3409 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3410 | `					/* Extract the enclosure */` |
|      3 | 3411 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3412 | `					if( i > 0 ){` |
|      3 | 3413 | `						encl = zPtr[0];` |
|      1 | 3414 | `					}` |
|      1 | 3415 | `				}` |
|      3 | 3416 | `				if( nArg > 4 ){` |
|      3 | 3417 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3418 | `						/* Extract the escape character */` |
|      3 | 3419 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3420 | `						if( i > 0 ){` |
|      3 | 3421 | `							escape = zPtr[0];` |
|      1 | 3422 | `						}` |
|      1 | 3423 | `					}` |
|      1 | 3424 | `				}` |
|      1 | 3425 | `			}` |
|      1 | 3426 | `		}` |
|      - | 3427 | `		/* Create our array */` |
|      3 | 3428 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3429 | `		if( pArray == 0 ){` |
|    ! 0 | 3430 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3431 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3432 | `			return PH7_OK;` |
|      - | 3433 | `		}` |
|      - | 3434 | `		/* Parse the raw input */` |
|      3 | 3435 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3436 | `		/* Return the freshly created array  */` |
|      3 | 3437 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3438 | `	}` |
|      3 | 3439 | `	return PH7_OK;` |
|      2 | 3440 | `}` |
|      - | 3441 | `/*` |
|      - | 3442 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3443 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3444 | ` * Parameters` |
|      - | 3445 | ` * $handle` |
|      - | 3446 | ` *   The file pointer.` |
|      - | 3447 | ` * $length` |
|      - | 3448 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3449 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3450 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3451 | ` *  the end of the line.` |
|      - | 3452 | ` * $allowable_tags` |
|      - | 3453 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3454 | ` * Return` |
|      - | 3455 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3456 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3457 | ` */` |
|      2 | 3458 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3459 | `{` |
|      - | 3460 | `	const ph7_io_stream *pStream;` |
|      - | 3461 | `	const char *zLine;` |
|      - | 3462 | `	io_private *pDev;` |
|      - | 3463 | `	ph7_int64 n,nLen;` |
|      3 | 3464 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3465 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3466 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3467 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3468 | `		return PH7_OK;` |
|      - | 3469 | `	}` |
|      - | 3470 | `	/* Extract our private data */` |
|      3 | 3471 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3472 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3473 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3474 | `		/*Expecting an IO handle */` |
|    ! 0 | 3475 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3476 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3477 | `		return PH7_OK;` |
|      - | 3478 | `	}` |
|      - | 3479 | `	/* Point to the target IO stream device */` |
|      3 | 3480 | `	pStream = pDev->pStream;` |
|      3 | 3481 | `	if( pStream == 0  ){` |
|    ! 0 | 3482 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3483 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3484 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3485 | `			);` |
|    ! 0 | 3486 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3487 | `		return PH7_OK;` |
|      - | 3488 | `	}` |
|      3 | 3489 | `	nLen = -1;` |
|      3 | 3490 | `	if( nArg > 1 ){` |
|      - | 3491 | `		/* Maximum data to read */` |
|    ! 0 | 3492 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3493 | `	}` |
|      - | 3494 | `	/* Perform the requested operation */` |
|      3 | 3495 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3496 | `	if( n < 1 ){` |
|      - | 3497 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3498 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3499 | `	}else{` |
|      3 | 3500 | `		const char *zTaglist = 0;` |
|      3 | 3501 | `		int nTaglen = 0;` |
|      3 | 3502 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3503 | `			/* Allowed tag */` |
|    ! 0 | 3504 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3505 | `		}` |
|      - | 3506 | `		/* Process data just read */` |
|      3 | 3507 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3508 | `	}` |
|      3 | 3509 | `	return PH7_OK;` |
|      2 | 3510 | `}` |
|      - | 3511 | `/*` |
|      - | 3512 | ` * string readdir(resource $dir_handle)` |
|      - | 3513 | ` *   Read entry from directory handle.` |
|      - | 3514 | ` * Parameter` |
|      - | 3515 | ` *  $dir_handle` |
|      - | 3516 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3517 | ` * Return` |
|      - | 3518 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3519 | ` */` |
|   8756 | 3520 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3521 | `{` |
|      - | 3522 | `	const ph7_io_stream *pStream;` |
|      - | 3523 | `	io_private *pDev;` |
|      - | 3524 | `	int rc;` |
|   8761 | 3525 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3526 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3527 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3528 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3529 | `		return PH7_OK;` |
|      - | 3530 | `	}` |
|      - | 3531 | `	/* Extract our private data */` |
|   8761 | 3532 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3533 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   8761 | 3534 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3535 | `		/*Expecting an IO handle */` |
|    ! 0 | 3536 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3537 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3538 | `		return PH7_OK;` |
|      - | 3539 | `	}` |
|      - | 3540 | `	/* Point to the target IO stream device */` |
|   8761 | 3541 | `	pStream = pDev->pStream;` |
|   8761 | 3542 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3543 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3544 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3545 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3546 | `			);` |
|    ! 0 | 3547 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3548 | `		return PH7_OK;` |
|      - | 3549 | `	}` |
|   8761 | 3550 | `	ph7_result_bool(pCtx,0);` |
|      - | 3551 | `	/* Perform the requested operation */` |
|   8761 | 3552 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   8761 | 3553 | `	if( rc != PH7_OK ){` |
|      - | 3554 | `		/* Return FALSE */` |
|   1055 | 3555 | `		ph7_result_bool(pCtx,0);` |
|    525 | 3556 | `	}` |
|   8761 | 3557 | `	return PH7_OK;` |
|   4383 | 3558 | `}` |
|      - | 3559 | `/*` |
|      - | 3560 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3561 | ` *   Rewind directory handle.` |
|      - | 3562 | ` * Parameter` |
|      - | 3563 | ` *  $dir_handle` |
|      - | 3564 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3565 | ` * Return` |
|      - | 3566 | ` *  FALSE on failure.` |
|      - | 3567 | ` */` |
|      2 | 3568 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3569 | `{` |
|      - | 3570 | `	const ph7_io_stream *pStream;` |
|      - | 3571 | `	io_private *pDev;` |
|      3 | 3572 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3573 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3574 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3575 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3576 | `		return PH7_OK;` |
|      - | 3577 | `	}` |
|      - | 3578 | `	/* Extract our private data */` |
|      3 | 3579 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3580 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3581 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3582 | `		/*Expecting an IO handle */` |
|    ! 0 | 3583 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3584 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3585 | `		return PH7_OK;` |
|      - | 3586 | `	}` |
|      - | 3587 | `	/* Point to the target IO stream device */` |
|      3 | 3588 | `	pStream = pDev->pStream;` |
|      3 | 3589 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3590 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3591 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3592 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3593 | `			);` |
|    ! 0 | 3594 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3595 | `		return PH7_OK;` |
|      - | 3596 | `	}` |
|      - | 3597 | `	/* Perform the requested operation */` |
|      3 | 3598 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3599 | `	return PH7_OK;` |
|      2 | 3600 | ` }` |
|      - | 3601 | `/* Forward declaration */` |
|      - | 3602 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3603 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3604 | `/*` |
|      - | 3605 | ` * void closedir(resource $dir_handle)` |
|      - | 3606 | ` *   Close directory handle.` |
|      - | 3607 | ` * Parameter` |
|      - | 3608 | ` *  $dir_handle` |
|      - | 3609 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3610 | ` * Return` |
|      - | 3611 | ` *  FALSE on failure.` |
|      - | 3612 | ` */` |
|   1054 | 3613 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3614 | `{` |
|      - | 3615 | `	const ph7_io_stream *pStream;` |
|      - | 3616 | `	io_private *pDev;` |
|   1059 | 3617 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3618 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3619 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3620 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3621 | `		return PH7_OK;` |
|      - | 3622 | `	}` |
|      - | 3623 | `	/* Extract our private data */` |
|   1059 | 3624 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3625 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1059 | 3626 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3627 | `		/*Expecting an IO handle */` |
|    ! 0 | 3628 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3629 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3630 | `		return PH7_OK;` |
|      - | 3631 | `	}` |
|      - | 3632 | `	/* Point to the target IO stream device */` |
|   1059 | 3633 | `	pStream = pDev->pStream;` |
|   1059 | 3634 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3635 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3636 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3637 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3638 | `			);` |
|    ! 0 | 3639 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3640 | `		return PH7_OK;` |
|      - | 3641 | `	}` |
|      - | 3642 | `	/* Perform the requested operation */` |
|   1059 | 3643 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3644 | `	/* Release the private stucture */` |
|   1059 | 3645 | `	ReleaseIOPrivate(pCtx,pDev);` |
|   1059 | 3646 | `	PH7_MemObjRelease(apArg[0]);` |
|   1059 | 3647 | `	return PH7_OK;` |
|    532 | 3648 | ` }` |
|      - | 3649 | `/*` |
|      - | 3650 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3651 | ` *  Open directory handle.` |
|      - | 3652 | ` * Parameters` |
|      - | 3653 | ` * $path` |
|      - | 3654 | ` *   The directory path that is to be opened.` |
|      - | 3655 | ` * $context` |
|      - | 3656 | ` *   A context stream resource.` |
|      - | 3657 | ` * Return` |
|      - | 3658 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3659 | ` */` |
|   1054 | 3660 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3661 | `{` |
|      - | 3662 | `	const ph7_io_stream *pStream;` |
|      - | 3663 | `	const char *zPath;` |
|      - | 3664 | `	io_private *pDev;` |
|      - | 3665 | `	int iLen,rc;` |
|   1059 | 3666 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3667 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3668 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3669 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3670 | `		return PH7_OK;` |
|      - | 3671 | `	}` |
|      - | 3672 | `	/* Extract the target path */` |
|   1059 | 3673 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3674 | `	/* Try to extract a stream */` |
|   1059 | 3675 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1059 | 3676 | `	if( pStream == 0 ){` |
|    ! 0 | 3677 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3678 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3679 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3680 | `		return PH7_OK;` |
|      - | 3681 | `	}` |
|   1059 | 3682 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3683 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3684 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3685 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3686 | `			);` |
|    ! 0 | 3687 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3688 | `		return PH7_OK;` |
|      - | 3689 | `	}` |
|      - | 3690 | `	/* Allocate a new IO private instance */` |
|   1059 | 3691 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1059 | 3692 | `	if( pDev == 0 ){` |
|    ! 0 | 3693 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3694 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3695 | `		return PH7_OK;` |
|      - | 3696 | `	}` |
|      - | 3697 | `	/* Initialize the structure */` |
|   1059 | 3698 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3699 | `	/* Open the target directory */` |
|   1059 | 3700 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1059 | 3701 | `	if( rc != PH7_OK ){` |
|      - | 3702 | `		/* IO error,return FALSE */` |
|    ! 0 | 3703 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3704 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3705 | `	}else{` |
|      - | 3706 | `		/* Return the handle as a resource */` |
|   1059 | 3707 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3708 | `	}` |
|   1059 | 3709 | `	return PH7_OK;` |
|    532 | 3710 | `}` |
|      - | 3711 | `/*` |
|      - | 3712 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3713 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3714 | ` * Parameters` |
|      - | 3715 | ` *  $filename` |
|      - | 3716 | ` *   The filename being read.` |
|      - | 3717 | ` *  $use_include_path` |
|      - | 3718 | ` *   You can use the optional second parameter and set it to` |
|      - | 3719 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3720 | ` *  $context` |
|      - | 3721 | ` *   A context stream resource.` |
|      - | 3722 | ` * Return` |
|      - | 3723 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3724 | ` */` |
|      - | 3725 | `/*` |
|      - | 3726 | ` * php's IO failures are E_WARNINGs naming the function, the path and the system reason:` |
|      - | 3727 | ` *   file_get_contents(/nope): Failed to open stream: No such file or directory` |
|      - | 3728 | ` * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of` |
|      - | 3729 | ` * them -- wrong severity, wrong text, and no reason. errno still holds the failing` |
|      - | 3730 | ` * syscall's code at this point (a successful call never clears it), which is where the` |
|      - | 3731 | ` * trailing reason comes from.` |
|      - | 3732 | ` */` |
|      2 | 3733 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3734 | `{` |
|      3 | 3735 | `	int use_include  = FALSE;` |
|      - | 3736 | `	const ph7_io_stream *pStream;` |
|      - | 3737 | `	ph7_int64 n,nRead;` |
|      - | 3738 | `	const char *zFile;` |
|      - | 3739 | `	char zBuf[8192];` |
|      - | 3740 | `	void *pHandle;` |
|      - | 3741 | `	int rc,nLen;` |
|      3 | 3742 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3743 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3744 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3745 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3746 | `		return PH7_OK;` |
|      - | 3747 | `	}` |
|      - | 3748 | `	/* Extract the file path */` |
|      3 | 3749 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3750 | `	/* Point to the target IO stream device */` |
|      3 | 3751 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3752 | `	if( pStream == 0 ){` |
|    ! 0 | 3753 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3754 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3755 | `		return PH7_OK;` |
|      - | 3756 | `	}` |
|      3 | 3757 | `	if( nArg > 1 ){` |
|    ! 0 | 3758 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3759 | `	}` |
|      - | 3760 | `	/* Try to open the file in read-only mode */` |
|      4 | 3761 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3762 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3763 | `	if( pHandle == 0 ){` |
|    ! 0 | 3764 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3765 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3766 | `		return PH7_OK;` |
|      - | 3767 | `	}` |
|      - | 3768 | `	/* Perform the requested operation */` |
|      3 | 3769 | `	nRead = 0;` |
|      2 | 3770 | `	for(;;){` |
|      5 | 3771 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3772 | `		if( n < 1 ){` |
|      - | 3773 | `			/* EOF or IO error,break immediately */` |
|      3 | 3774 | `			break;` |
|      - | 3775 | `		}` |
|      - | 3776 | `		/* Output data */` |
|      3 | 3777 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3778 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3779 | `			break;` |
|      - | 3780 | `		}` |
|      - | 3781 | `		/* Increment counter */` |
|      3 | 3782 | `		nRead += n;` |
|      1 | 3783 | `	}` |
|      - | 3784 | `	/* Close the stream */` |
|      3 | 3785 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3786 | `	/* Total number of bytes readen */` |
|      3 | 3787 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3788 | `	return PH7_OK;` |
|      2 | 3789 | `}` |
|      - | 3790 | `/*` |
|      - | 3791 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3792 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3793 | ` *  Reads entire file into a string.` |
|      - | 3794 | ` * Parameters` |
|      - | 3795 | ` *  $filename` |
|      - | 3796 | ` *   The filename being read.` |
|      - | 3797 | ` *  $use_include_path` |
|      - | 3798 | ` *   You can use the optional second parameter and set it to` |
|      - | 3799 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3800 | ` *  $context` |
|      - | 3801 | ` *   A context stream resource.` |
|      - | 3802 | ` *  $offset` |
|      - | 3803 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3804 | ` *  $maxlen` |
|      - | 3805 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3806 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3807 | ` * Return` |
|      - | 3808 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3809 | ` */` |
|   6700 | 3810 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3811 | `{` |
|      - | 3812 | `	const ph7_io_stream *pStream;` |
|      - | 3813 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6705 | 3814 | `	int use_include  = FALSE;` |
|      - | 3815 | `	const char *zFile;` |
|      - | 3816 | `	char zBuf[8192];` |
|      - | 3817 | `	void *pHandle;` |
|      - | 3818 | `	int nLen;` |
|      - | 3819 |  |
|   6705 | 3820 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3821 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3822 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3823 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3824 | `		return PH7_OK;` |
|      - | 3825 | `	}` |
|      - | 3826 | `	/* Extract the file path */` |
|   6705 | 3827 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3828 | `	/* Point to the target IO stream device */` |
|   6705 | 3829 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6705 | 3830 | `	if( pStream == 0 ){` |
|    ! 0 | 3831 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3832 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3833 | `		return PH7_OK;` |
|      - | 3834 | `	}` |
|   6705 | 3835 | `	nMaxlen = -1;` |
|   6705 | 3836 | `	if( nArg > 1 ){` |
|      5 | 3837 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3838 | `	}` |
|      - | 3839 | `	/* Try to open the file in read-only mode */` |
|   6705 | 3840 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6705 | 3841 | `	if( pHandle == 0 ){` |
|    ! 0 | 3842 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3843 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3844 | `		return PH7_OK;` |
|      - | 3845 | `	}` |
|   6705 | 3846 | `	if( nArg > 3 ){` |
|      - | 3847 | `		/* Extract the offset */` |
|      5 | 3848 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3849 | `		if( n > 0 ){` |
|    ! 0 | 3850 | `			if( pStream->xSeek ){` |
|      - | 3851 | `				/* Seek to the desired offset */` |
|    ! 0 | 3852 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3853 | `			}` |
|    ! 0 | 3854 | `		}` |
|      5 | 3855 | `		if( nArg > 4 ){` |
|      - | 3856 | `			/* Maximum data to read */` |
|      5 | 3857 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3858 | `		}` |
|      2 | 3859 | `	}` |
|      - | 3860 | `	/* Perform the requested operation */` |
|   6705 | 3861 | `	nRead = 0;` |
|   6698 | 3862 | `	for(;;){` |
|  20102 | 3863 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6701 | 3864 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  13401 | 3865 | `		if( n < 1 ){` |
|      - | 3866 | `			/* EOF or IO error,break immediately */` |
|   6703 | 3867 | `			break;` |
|      - | 3868 | `		}` |
|      - | 3869 | `		/* Append data */` |
|   6703 | 3870 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3871 | `		/* Increment read counter */` |
|   6703 | 3872 | `		nRead += n;` |
|   6703 | 3873 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3874 | `			/* Read limit reached */` |
|      3 | 3875 | `			break;` |
|      - | 3876 | `		}` |
|      5 | 3877 | `	}` |
|      - | 3878 | `	/* Close the stream */` |
|   6705 | 3879 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3880 | `	/* Check if we have read something */` |
|   6705 | 3881 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3882 | `		/* Nothing read,return FALSE */` |
|      3 | 3883 | `		ph7_result_bool(pCtx,0);` |
|      1 | 3884 | `	}` |
|   6705 | 3885 | `	return PH7_OK;` |
|   3355 | 3886 | `}` |
|      - | 3887 | `/*` |
|      - | 3888 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3889 | ` *  Write a string to a file.` |
|      - | 3890 | ` * Parameters` |
|      - | 3891 | ` *  $filename` |
|      - | 3892 | ` *  Path to the file where to write the data.` |
|      - | 3893 | ` * $data` |
|      - | 3894 | ` *  The data to write(Must be a string).` |
|      - | 3895 | ` * $flags` |
|      - | 3896 | ` *  The value of flags can be any combination of the following` |
|      - | 3897 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3898 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3899 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3900 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3901 | ` * context` |
|      - | 3902 | ` *  A context stream resource.` |
|      - | 3903 | ` * Return` |
|      - | 3904 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3905 | ` */` |
|  13628 | 3906 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3907 | `{` |
|  13633 | 3908 | `	int use_include  = FALSE;` |
|      - | 3909 | `	const ph7_io_stream *pStream;` |
|      - | 3910 | `	const char *zFile;` |
|      - | 3911 | `	const char *zData;` |
|      - | 3912 | `	int iOpenFlags;` |
|      - | 3913 | `	void *pHandle;` |
|      - | 3914 | `	int iFlags;` |
|      - | 3915 | `	int nLen;` |
|      - | 3916 |  |
|  13633 | 3917 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3918 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3919 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3920 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3921 | `		return PH7_OK;` |
|      - | 3922 | `	}` |
|      - | 3923 | `	/* Extract the file path */` |
|  13633 | 3924 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3925 | `	/* Point to the target IO stream device */` |
|  13633 | 3926 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13633 | 3927 | `	if( pStream == 0 ){` |
|    ! 0 | 3928 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3929 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3930 | `		return PH7_OK;` |
|      - | 3931 | `	}` |
|      - | 3932 | `	/* Data to write */` |
|  13633 | 3933 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3934 | `	/* Try to open the file in read-write mode */` |
|  13633 | 3935 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3936 | `	/* Extract the flags */` |
|  13633 | 3937 | `	iFlags = 0;` |
|  13633 | 3938 | `	if( nArg > 2 ){` |
|    ! 0 | 3939 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3940 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3941 | `			use_include = TRUE;` |
|    ! 0 | 3942 | `		}` |
|    ! 0 | 3943 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3944 | `			/* If the file already exists, append the data to the file` |
|      - | 3945 | `			 * instead of overwriting it.` |
|      - | 3946 | `			 */` |
|    ! 0 | 3947 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3948 | `			/* Append mode */` |
|    ! 0 | 3949 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3950 | `		}` |
|    ! 0 | 3951 | `	}` |
|  20447 | 3952 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6814 | 3953 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13633 | 3954 | `	if( pHandle == 0 ){` |
|    ! 0 | 3955 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 3956 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3957 | `		return PH7_OK;` |
|      - | 3958 | `	}` |
|  13633 | 3959 | `	if( nLen < 1 ){` |
|      - | 3960 | `		/* Empty data, file is created/truncated */` |
|      7 | 3961 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3962 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3963 | `		return PH7_OK;` |
|      - | 3964 | `	}` |
|  13627 | 3965 | `	if( pStream->xWrite ){` |
|      - | 3966 | `		ph7_int64 n;` |
|  13627 | 3967 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 3968 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3969 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3970 | `		}` |
|      - | 3971 | `		/* Perform the write operation */` |
|  13627 | 3972 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13627 | 3973 | `		if( n < 0 ){` |
|      - | 3974 | `			/* IO error,return FALSE */` |
|    ! 0 | 3975 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3976 | `		}else{` |
|      - | 3977 | `			/* Total number of bytes written */` |
|  13627 | 3978 | `			ph7_result_int64(pCtx,n);` |
|      - | 3979 | `		}` |
|   6816 | 3980 | `	}else{` |
|      - | 3981 | `		/* Read-only stream */` |
|    ! 0 | 3982 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 3983 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 3984 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 3985 | `			);` |
|    ! 0 | 3986 | `		ph7_result_bool(pCtx,0);` |
|      - | 3987 | `	}` |
|      - | 3988 | `	/* Close the handle */` |
|  13627 | 3989 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13627 | 3990 | `	return PH7_OK;` |
|   6819 | 3991 | `}` |
|      - | 3992 | `/*` |
|      - | 3993 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 3994 | ` *  Reads entire file into an array.` |
|      - | 3995 | ` * Parameters` |
|      - | 3996 | ` *  $filename` |
|      - | 3997 | ` *   The filename being read.` |
|      - | 3998 | ` *  $flags` |
|      - | 3999 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 4000 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 4001 | ` *       Search for the file in the include_path.` |
|      - | 4002 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 4003 | ` *       Do not add newline at the end of each array element` |
|      - | 4004 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 4005 | ` *       Skip empty lines` |
|      - | 4006 | ` *  $context` |
|      - | 4007 | ` *   A context stream resource.` |
|      - | 4008 | ` * Return` |
|      - | 4009 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 4010 | ` */` |
|      4 | 4011 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4012 | `{` |
|      - | 4013 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 4014 | `	ph7_value *pArray,*pLine;` |
|      - | 4015 | `	const ph7_io_stream *pStream;` |
|      6 | 4016 | `	int use_include = 0;` |
|      - | 4017 | `	io_private *pDev;` |
|      - | 4018 | `	ph7_int64 n;` |
|      - | 4019 | `	int iFlags;` |
|      - | 4020 | `	int nLen;` |
|      - | 4021 |  |
|      6 | 4022 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4023 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4024 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4025 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4026 | `		return PH7_OK;` |
|      - | 4027 | `	}` |
|      - | 4028 | `	/* Extract the file path */` |
|      6 | 4029 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4030 | `	/* Point to the target IO stream device */` |
|      6 | 4031 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4032 | `	if( pStream == 0 ){` |
|    ! 0 | 4033 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4034 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4035 | `		return PH7_OK;` |
|      - | 4036 | `	}` |
|      - | 4037 | `	/* Allocate a new IO private instance */` |
|      6 | 4038 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      6 | 4039 | `	if( pDev == 0 ){` |
|    ! 0 | 4040 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4041 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4042 | `		return PH7_OK;` |
|      - | 4043 | `	}` |
|      - | 4044 | `	/* Initialize the structure */` |
|      6 | 4045 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      6 | 4046 | `	iFlags = 0;` |
|      6 | 4047 | `	if( nArg > 1 ){` |
|    ! 0 | 4048 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 4049 | `	}` |
|      6 | 4050 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 4051 | `		use_include = TRUE;` |
|    ! 0 | 4052 | `	}` |
|      - | 4053 | `	/* Create the array and the working value */` |
|      6 | 4054 | `	pArray = ph7_context_new_array(pCtx);` |
|      6 | 4055 | `	pLine = ph7_context_new_scalar(pCtx);` |
|      6 | 4056 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 4057 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4058 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4059 | `		return PH7_OK;` |
|      - | 4060 | `	}` |
|      - | 4061 | `	/* Try to open the file in read-only mode */` |
|      6 | 4062 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4063 | `	if( pDev->pHandle == 0 ){` |
|      3 | 4064 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 4065 | `		ph7_result_bool(pCtx,0);` |
|      - | 4066 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 4067 | `		 * as soon we return from this function.` |
|      - | 4068 | `		 */` |
|      3 | 4069 | `		return PH7_OK;` |
|      - | 4070 | `	}` |
|      - | 4071 | `	/* Perform the requested operation */` |
|      3 | 4072 | `	for(;;){` |
|      - | 4073 | `		/* Try to extract a line */` |
|      7 | 4074 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 4075 | `		if( n < 1 ){` |
|      - | 4076 | `			/* EOF or IO error */` |
|      3 | 4077 | `			break;` |
|      - | 4078 | `		}` |
|      - | 4079 | `		/* Reset the cursor */` |
|      5 | 4080 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 4081 | `		/* Remove line ending if requested by the caller */` |
|      5 | 4082 | `		zPtr = zBuf;` |
|      5 | 4083 | `		zEnd = &zBuf[n];` |
|      5 | 4084 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 4085 | `			/* Ignore trailig lines */` |
|    ! 0 | 4086 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 4087 | `#ifdef __WINNT__` |
|      - | 4088 | `				\|\| zEnd[-1] == '\r'` |
|      - | 4089 | `#endif` |
|      - | 4090 | `				)){` |
|    ! 0 | 4091 | `					n--;` |
|    ! 0 | 4092 | `					zEnd--;` |
|    ! 0 | 4093 | `			}` |
|    ! 0 | 4094 | `		}` |
|      5 | 4095 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 4096 | `			/* Ignore empty lines */` |
|    ! 0 | 4097 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 4098 | `				zPtr++;` |
|    ! 0 | 4099 | `			}` |
|    ! 0 | 4100 | `			if( zPtr >= zEnd ){` |
|      - | 4101 | `				/* Empty line */` |
|    ! 0 | 4102 | `				continue;` |
|      - | 4103 | `			}` |
|    ! 0 | 4104 | `		}` |
|      5 | 4105 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 4106 | `		/* Insert line */` |
|      5 | 4107 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 4108 | `	}` |
|      - | 4109 | `	/* Close the stream */` |
|      3 | 4110 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4111 | `	/* Release the io_private instance */` |
|      3 | 4112 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4113 | `	/* Return the created array */` |
|      3 | 4114 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4115 | `	return PH7_OK;` |
|      4 | 4116 | `}` |
|      - | 4117 | `/*` |
|      - | 4118 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 4119 | ` *  Makes a copy of the file source to dest.` |
|      - | 4120 | ` * Parameters` |
|      - | 4121 | ` *  $source` |
|      - | 4122 | ` *   Path to the source file.` |
|      - | 4123 | ` *  $dest` |
|      - | 4124 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 4125 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 4126 | ` *  $context` |
|      - | 4127 | ` *   A context stream resource.` |
|      - | 4128 | ` * Return` |
|      - | 4129 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4130 | ` */` |
|      4 | 4131 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4132 | `{` |
|      - | 4133 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 4134 | `	const char *zFile;` |
|      - | 4135 | `	char zBuf[8192];` |
|      - | 4136 | `	void *pIn,*pOut;` |
|      - | 4137 | `	ph7_int64 n;` |
|      - | 4138 | `	int nLen;` |
|      6 | 4139 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 4140 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4141 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 4142 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4143 | `		return PH7_OK;` |
|      - | 4144 | `	}` |
|      - | 4145 | `	/* Extract the source name */` |
|      6 | 4146 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4147 | `	/* Point to the target IO stream device */` |
|      6 | 4148 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4149 | `	if( pSin == 0 ){` |
|    ! 0 | 4150 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4151 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4152 | `		return PH7_OK;` |
|      - | 4153 | `	}` |
|      - | 4154 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4155 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4156 | `	if( pIn == 0 ){` |
|      3 | 4157 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 4158 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4159 | `		return PH7_OK;` |
|      - | 4160 | `	}` |
|      - | 4161 | `	/* Extract the destination name */` |
|      3 | 4162 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4163 | `	/* Point to the target IO stream device */` |
|      3 | 4164 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4165 | `	if( pSout == 0 ){` |
|    ! 0 | 4166 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4167 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4168 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4169 | `		return PH7_OK;` |
|      - | 4170 | `	}` |
|      3 | 4171 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4172 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4173 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4174 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4175 | `			);` |
|    ! 0 | 4176 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4177 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4178 | `		return PH7_OK;` |
|      - | 4179 | `	}` |
|      - | 4180 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4181 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4182 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4183 | `	if( pOut == 0 ){` |
|    ! 0 | 4184 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 4185 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4186 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4187 | `		return PH7_OK;` |
|      - | 4188 | `	}` |
|      - | 4189 | `	/* Perform the requested operation */` |
|      2 | 4190 | `	for(;;){` |
|      - | 4191 | `		/* Read from source */` |
|      5 | 4192 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4193 | `		if( n < 1 ){` |
|      - | 4194 | `			/* EOF or IO error,break immediately */` |
|      3 | 4195 | `			break;` |
|      - | 4196 | `		}` |
|      - | 4197 | `		/* Write to dest */` |
|      3 | 4198 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4199 | `		if( n < 1 ){` |
|      - | 4200 | `			/* IO error,break immediately */` |
|    ! 0 | 4201 | `			break;` |
|      - | 4202 | `		}` |
|      1 | 4203 | `	}` |
|      - | 4204 | `	/* Close the streams */` |
|      3 | 4205 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4206 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4207 | `	/* Return TRUE */` |
|      3 | 4208 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4209 | `	return PH7_OK;` |
|      4 | 4210 | `}` |
|      - | 4211 | `/*` |
|      - | 4212 | ` * array fstat(resource $handle)` |
|      - | 4213 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4214 | ` * Parameters` |
|      - | 4215 | ` *  $handle` |
|      - | 4216 | ` *   The file pointer.` |
|      - | 4217 | ` * Return` |
|      - | 4218 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4219 | ` */` |
|      2 | 4220 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4221 | `{` |
|      - | 4222 | `	ph7_value *pArray,*pValue;` |
|      - | 4223 | `	const ph7_io_stream *pStream;` |
|      - | 4224 | `	io_private *pDev;` |
|      3 | 4225 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4226 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4227 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4228 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4229 | `		return PH7_OK;` |
|      - | 4230 | `	}` |
|      - | 4231 | `	/* Extract our private data */` |
|      3 | 4232 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4233 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4234 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4235 | `		/* Expecting an IO handle */` |
|    ! 0 | 4236 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4237 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4238 | `		return PH7_OK;` |
|      - | 4239 | `	}` |
|      - | 4240 | `	/* Point to the target IO stream device */` |
|      3 | 4241 | `	pStream = pDev->pStream;` |
|      3 | 4242 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4243 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4244 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4245 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4246 | `			);` |
|    ! 0 | 4247 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4248 | `		return PH7_OK;` |
|      - | 4249 | `	}` |
|      - | 4250 | `	/* Create the array and the working value */` |
|      3 | 4251 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4252 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4253 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4254 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4255 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4256 | `		return PH7_OK;` |
|      - | 4257 | `	}` |
|      - | 4258 | `	/* Perform the requested operation */` |
|      3 | 4259 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4260 | `	/* Return the freshly created array */` |
|      3 | 4261 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4262 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4263 | `	 * released automatically as soon we return from this function.` |
|      - | 4264 | `	 */` |
|      3 | 4265 | `	return PH7_OK;` |
|      2 | 4266 | `}` |
|      - | 4267 | `/*` |
|      - | 4268 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4269 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4270 | ` * Parameters` |
|      - | 4271 | ` *  $handle` |
|      - | 4272 | ` *   The file pointer.` |
|      - | 4273 | ` *  $string` |
|      - | 4274 | ` *   The string that is to be written.` |
|      - | 4275 | ` *  $length` |
|      - | 4276 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4277 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4278 | ` * Return` |
|      - | 4279 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4280 | ` */` |
|     22 | 4281 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4282 | `{` |
|      - | 4283 | `	const ph7_io_stream *pStream;` |
|      - | 4284 | `	const char *zString;` |
|      - | 4285 | `	io_private *pDev;` |
|      - | 4286 | `	int nLen,n;` |
|     24 | 4287 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4288 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4289 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4290 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4291 | `		return PH7_OK;` |
|      - | 4292 | `	}` |
|      - | 4293 | `	/* Extract our private data */` |
|     24 | 4294 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4295 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     24 | 4296 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4297 | `		/* Expecting an IO handle */` |
|    ! 0 | 4298 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4299 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4300 | `		return PH7_OK;` |
|      - | 4301 | `	}` |
|      - | 4302 | `	/* Point to the target IO stream device */` |
|     24 | 4303 | `	pStream = pDev->pStream;` |
|     24 | 4304 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4305 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4306 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4307 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4308 | `			);` |
|    ! 0 | 4309 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4310 | `		return PH7_OK;` |
|      - | 4311 | `	}` |
|      - | 4312 | `	/* Extract the data to write */` |
|     24 | 4313 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     24 | 4314 | `	if( nArg > 2 ){` |
|      - | 4315 | `		/* Maximum data length to write */` |
|    ! 0 | 4316 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4317 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4318 | `			nLen = n;` |
|    ! 0 | 4319 | `		}` |
|    ! 0 | 4320 | `	}` |
|     24 | 4321 | `	if( nLen < 1 ){` |
|      - | 4322 | `		/* Nothing to write */` |
|    ! 0 | 4323 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4324 | `		return PH7_OK;` |
|      - | 4325 | `	}` |
|      - | 4326 | `	/* Perform the requested operation */` |
|     24 | 4327 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|     24 | 4328 | `	if( n <  0 ){` |
|      - | 4329 | `		/* IO error,return FALSE */` |
|    ! 0 | 4330 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4331 | `	}else{` |
|      - | 4332 | `		/* #Bytes written */` |
|     24 | 4333 | `		ph7_result_int(pCtx,n);` |
|      - | 4334 | `	}` |
|     24 | 4335 | `	return PH7_OK;` |
|     13 | 4336 | `}` |
|      - | 4337 | `/*` |
|      - | 4338 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4339 | ` *  Portable advisory file locking.` |
|      - | 4340 | ` * Parameters` |
|      - | 4341 | ` *  $handle` |
|      - | 4342 | ` *   The file pointer.` |
|      - | 4343 | ` *  $operation` |
|      - | 4344 | ` *   operation is one of the following:` |
|      - | 4345 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4346 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4347 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4348 | ` * Return` |
|      - | 4349 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4350 | ` */` |
|      4 | 4351 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 4352 | `{` |
|      - | 4353 | `	const ph7_io_stream *pStream;` |
|      - | 4354 | `	io_private *pDev;` |
|      - | 4355 | `	int nLock;` |
|      - | 4356 | `	int rc;` |
|      4 | 4357 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4358 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4359 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4360 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4361 | `		return PH7_OK;` |
|      - | 4362 | `	}` |
|      - | 4363 | `	/* Extract our private data */` |
|      4 | 4364 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4365 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      4 | 4366 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4367 | `		/*Expecting an IO handle */` |
|    ! 0 | 4368 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4369 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4370 | `		return PH7_OK;` |
|      - | 4371 | `	}` |
|      - | 4372 | `	/* Point to the target IO stream device */` |
|      4 | 4373 | `	pStream = pDev->pStream;` |
|      4 | 4374 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4375 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4376 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4377 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4378 | `			);` |
|    ! 0 | 4379 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4380 | `		return PH7_OK;` |
|      - | 4381 | `	}` |
|      - | 4382 | `	/* Requested lock operation */` |
|      4 | 4383 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4384 | `	/*` |
|      - | 4385 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally \|LOCK_NB=4)` |
|      - | 4386 | `	 * into the xLock() vtable contract, which is a PUBLIC C API and stays as it is:` |
|      - | 4387 | `	 * negative = unlock, 1 = exclusive, anything else = shared.` |
|      - | 4388 | `	 */` |
|      - | 4389 | `	{` |
|      4 | 4390 | `		int iOp = nLock & ~4 /* strip LOCK_NB */;` |
|      4 | 4391 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      2 | 4392 | `			nLock = -1;` |
|      3 | 4393 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|      2 | 4394 | `			nLock = 1;` |
|      1 | 4395 | `		}else{` |
|    ! 0 | 4396 | `			nLock = 0; /* LOCK_SH */` |
|      - | 4397 | `		}` |
|      - | 4398 | `	}` |
|      - | 4399 | `	/* Lock operation */` |
|      4 | 4400 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4401 | `	/* IO result */` |
|      4 | 4402 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 4403 | `	return PH7_OK;` |
|      2 | 4404 | `}` |
|      - | 4405 | `/*` |
|      - | 4406 | ` * int fpassthru(resource $handle)` |
|      - | 4407 | ` *  Output all remaining data on a file pointer.` |
|      - | 4408 | ` * Parameters` |
|      - | 4409 | ` *  $handle` |
|      - | 4410 | ` *   The file pointer.` |
|      - | 4411 | ` * Return` |
|      - | 4412 | ` *  Total number of characters read from handle and passed through` |
|      - | 4413 | ` *  to the output on success or FALSE on failure.` |
|      - | 4414 | ` */` |
|      2 | 4415 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4416 | `{` |
|      - | 4417 | `	const ph7_io_stream *pStream;` |
|      - | 4418 | `	io_private *pDev;` |
|      - | 4419 | `	ph7_int64 n,nRead;` |
|      - | 4420 | `	char zBuf[8192];` |
|      - | 4421 | `	int rc;` |
|      3 | 4422 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4423 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4424 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4425 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4426 | `		return PH7_OK;` |
|      - | 4427 | `	}` |
|      - | 4428 | `	/* Extract our private data */` |
|      3 | 4429 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4430 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4431 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4432 | `		/*Expecting an IO handle */` |
|    ! 0 | 4433 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4434 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4435 | `		return PH7_OK;` |
|      - | 4436 | `	}` |
|      - | 4437 | `	/* Point to the target IO stream device */` |
|      3 | 4438 | `	pStream = pDev->pStream;` |
|      3 | 4439 | `	if( pStream == 0  ){` |
|    ! 0 | 4440 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4441 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4442 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4443 | `			);` |
|    ! 0 | 4444 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4445 | `		return PH7_OK;` |
|      - | 4446 | `	}` |
|      - | 4447 | `	/* Perform the requested operation */` |
|      3 | 4448 | `	nRead = 0;` |
|      2 | 4449 | `	for(;;){` |
|      5 | 4450 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4451 | `		if( n < 1 ){` |
|      - | 4452 | `			/* Error or EOF */` |
|      3 | 4453 | `			break;` |
|      - | 4454 | `		}` |
|      - | 4455 | `		/* Increment the read counter */` |
|      3 | 4456 | `		nRead += n;` |
|      - | 4457 | `		/* Output data */` |
|      3 | 4458 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4459 | `		if( rc == PH7_ABORT ){` |
|      - | 4460 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4461 | `			break;` |
|      - | 4462 | `		}` |
|      1 | 4463 | `	}` |
|      - | 4464 | `	/* Total number of bytes readen */` |
|      3 | 4465 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4466 | `	return PH7_OK;` |
|      2 | 4467 | `}` |
|      - | 4468 | `/* CSV reader/writer private data */` |
|      - | 4469 | `struct csv_data` |
|      - | 4470 | `{` |
|      - | 4471 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4472 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4473 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4474 | `	int iCount;       /* Counter */` |
|      - | 4475 | `};` |
|      - | 4476 | `/*` |
|      - | 4477 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4478 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4479 | ` * associated data.` |
|      - | 4480 | ` */` |
|      6 | 4481 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4482 | `{` |
|      7 | 4483 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4484 | `	const char *zData;` |
|      - | 4485 | `	int nLen,c2;` |
|      - | 4486 | `	sxu32 n;` |
|      - | 4487 | `	/* Point to the raw data */` |
|      7 | 4488 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4489 | `	if( nLen < 1 ){` |
|      - | 4490 | `		/* Nothing to write */` |
|    ! 0 | 4491 | `		return PH7_OK;` |
|      - | 4492 | `	}` |
|      7 | 4493 | `	if( pData->iCount > 0 ){` |
|      - | 4494 | `		/* Write the delimiter */` |
|      5 | 4495 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4496 | `	}` |
|      7 | 4497 | `	n = 1;` |
|      7 | 4498 | `	c2 = 0;` |
|     10 | 4499 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4500 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4501 | `			c2 = 1;` |
|    ! 0 | 4502 | `			if( n == 0 ){` |
|    ! 0 | 4503 | `				c2 = 2;` |
|    ! 0 | 4504 | `			}` |
|      - | 4505 | `			/* Write the enclosure */` |
|    ! 0 | 4506 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4507 | `			if( c2 > 1 ){` |
|    ! 0 | 4508 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4509 | `			}` |
|    ! 0 | 4510 | `	}` |
|      - | 4511 | `	/* Write the data */` |
|      7 | 4512 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4513 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4514 | `		return PH7_ABORT;` |
|      - | 4515 | `	}` |
|      7 | 4516 | `	if( c2 > 0 ){` |
|      - | 4517 | `		/* Write the enclosure */` |
|    ! 0 | 4518 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4519 | `		if( c2 > 1 ){` |
|    ! 0 | 4520 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4521 | `		}` |
|    ! 0 | 4522 | `	}` |
|      7 | 4523 | `	pData->iCount++;` |
|      7 | 4524 | `	return PH7_OK;` |
|      4 | 4525 | `}` |
|      - | 4526 | `/*` |
|      - | 4527 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4528 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4529 | ` * Parameters` |
|      - | 4530 | ` *  $handle` |
|      - | 4531 | ` *   Open file handle.` |
|      - | 4532 | ` * $fields` |
|      - | 4533 | ` *   An array of values.` |
|      - | 4534 | ` * $delimiter` |
|      - | 4535 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4536 | ` * $enclosure` |
|      - | 4537 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4538 | ` */` |
|      2 | 4539 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4540 | `{` |
|      - | 4541 | `	const ph7_io_stream *pStream;` |
|      - | 4542 | `	struct csv_data sCsv;` |
|      - | 4543 | `	io_private *pDev;` |
|      - | 4544 | `	char *zEol;` |
|      - | 4545 | `	int eolen;` |
|      3 | 4546 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4547 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4548 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4549 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4550 | `		return PH7_OK;` |
|      - | 4551 | `	}` |
|      - | 4552 | `	/* Extract our private data */` |
|      3 | 4553 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4554 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4555 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4556 | `		/*Expecting an IO handle */` |
|    ! 0 | 4557 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4558 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4559 | `		return PH7_OK;` |
|      - | 4560 | `	}` |
|      - | 4561 | `	/* Point to the target IO stream device */` |
|      3 | 4562 | `	pStream = pDev->pStream;` |
|      3 | 4563 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4564 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4565 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4566 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4567 | `			);` |
|    ! 0 | 4568 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4569 | `		return PH7_OK;` |
|      - | 4570 | `	}` |
|      - | 4571 | `	/* Set default csv separator */` |
|      3 | 4572 | `	sCsv.delimiter = ',';` |
|      3 | 4573 | `	sCsv.enclosure = '"';` |
|      3 | 4574 | `	sCsv.pDev = pDev;` |
|      3 | 4575 | `	sCsv.iCount = 0;` |
|      3 | 4576 | `	if( nArg > 2 ){` |
|      - | 4577 | `		/* User delimiter */` |
|      - | 4578 | `		const char *z;` |
|      - | 4579 | `		int n;` |
|      3 | 4580 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4581 | `		if( n > 0 ){` |
|      3 | 4582 | `			sCsv.delimiter = z[0];` |
|      1 | 4583 | `		}` |
|      3 | 4584 | `		if( nArg > 3 ){` |
|      3 | 4585 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4586 | `			if( n > 0 ){` |
|      3 | 4587 | `				sCsv.enclosure = z[0];` |
|      1 | 4588 | `			}` |
|      1 | 4589 | `		}` |
|      1 | 4590 | `	}` |
|      - | 4591 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4592 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4593 | `	/* Write a line ending */` |
|      - | 4594 | `#ifdef __WINNT__` |
|      1 | 4595 | `	zEol = "\r\n";` |
|      1 | 4596 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4597 | `#else` |
|      - | 4598 | `	/* Assume UNIX LF */` |
|      2 | 4599 | `	zEol = "\n";` |
|      2 | 4600 | `	eolen = (int)sizeof(char);` |
|      - | 4601 | `#endif` |
|      3 | 4602 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4603 | `	return PH7_OK;` |
|      2 | 4604 | `}` |
|      - | 4605 | `/*` |
|      - | 4606 | ` * fprintf,vfprintf private data.` |
|      - | 4607 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4608 | ` * input consumer callback defined below.` |
|      - | 4609 | ` */` |
|      - | 4610 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4611 | `struct fprintf_data` |
|      - | 4612 | `{` |
|      - | 4613 | `	io_private *pIO;        /* IO stream */` |
|      - | 4614 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4615 | `};` |
|      - | 4616 | `/*` |
|      - | 4617 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4618 | ` */` |
|     30 | 4619 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4620 | `{` |
|     31 | 4621 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4622 | `	ph7_int64 n;` |
|      - | 4623 | `	/* Write the formatted data */` |
|     31 | 4624 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     31 | 4625 | `	if( n < 1 ){` |
|    ! 0 | 4626 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4627 | `		/* IO error,abort immediately */` |
|    ! 0 | 4628 | `		return SXERR_ABORT;` |
|      - | 4629 | `	}` |
|      - | 4630 | `	/* Increment counter */` |
|     31 | 4631 | `	pFdata->nCount += n;` |
|     31 | 4632 | `	return PH7_OK;` |
|     16 | 4633 | `}` |
|      - | 4634 | `/*` |
|      - | 4635 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4636 | ` *  Write a formatted string to a stream.` |
|      - | 4637 | ` * Parameters` |
|      - | 4638 | ` *  $handle` |
|      - | 4639 | ` *   The file pointer.` |
|      - | 4640 | ` *  $format` |
|      - | 4641 | ` *   String format (see sprintf()).` |
|      - | 4642 | ` * Return` |
|      - | 4643 | ` *  The length of the written string.` |
|      - | 4644 | ` */` |
|     18 | 4645 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4646 | `{` |
|      - | 4647 | `	fprintf_data sFdata;` |
|      - | 4648 | `	const char *zFormat;` |
|      - | 4649 | `	io_private *pDev;` |
|      - | 4650 | `	int nLen;` |
|     19 | 4651 | `	if( nArg < 2 ){` |
|    ! 0 | 4652 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4653 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4654 | `		return PH7_OK;` |
|      - | 4655 | `	}` |
|      - | 4656 | `	{` |
|      - | 4657 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     19 | 4658 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     19 | 4659 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 4660 | `			return rcs;` |
|      - | 4661 | `		}` |
|      - | 4662 | `	}` |
|      - | 4663 | `	/* Extract our private data */` |
|     19 | 4664 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4665 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     19 | 4666 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4667 | `		/*Expecting an IO handle */` |
|    ! 0 | 4668 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4669 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4670 | `		return PH7_OK;` |
|      - | 4671 | `	}` |
|      - | 4672 | `	/* Point to the target IO stream device */` |
|     19 | 4673 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4674 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4675 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4676 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4677 | `			);` |
|    ! 0 | 4678 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4679 | `		return PH7_OK;` |
|      - | 4680 | `	}` |
|      - | 4681 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 4682 | `	{` |
|     19 | 4683 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     19 | 4684 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4685 | `			return rcf;` |
|      - | 4686 | `		}` |
|      - | 4687 | `	}` |
|      - | 4688 | `	/* Extract the string format (scalars/null coerce). */` |
|     19 | 4689 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     19 | 4690 | `	if( nLen < 1 ){` |
|      - | 4691 | `		/* Empty string,return zero */` |
|    ! 0 | 4692 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4693 | `		return PH7_OK;` |
|      - | 4694 | `	}` |
|      - | 4695 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4696 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4697 | `	{` |
|     19 | 4698 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     19 | 4699 | `		if( rcv != PH7_OK ){` |
|      3 | 4700 | `			return rcv;` |
|      - | 4701 | `		}` |
|      - | 4702 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output.` |
|      - | 4703 | `		 * fprintf's values start at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     17 | 4704 | `		rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     17 | 4705 | `		if( rcv != PH7_OK ){` |
|      3 | 4706 | `			return rcv;` |
|      - | 4707 | `		}` |
|      - | 4708 | `	}` |
|      - | 4709 | `	/* Prepare our private data */` |
|     15 | 4710 | `	sFdata.nCount = 0;` |
|     15 | 4711 | `	sFdata.pIO = pDev;` |
|      - | 4712 | `	/* Format the string */` |
|     15 | 4713 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4714 | `	/* Return total number of bytes written */` |
|     15 | 4715 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     15 | 4716 | `	return PH7_OK;` |
|     10 | 4717 | `}` |
|      - | 4718 | `/*` |
|      - | 4719 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4720 | ` *  Write a formatted string to a stream.` |
|      - | 4721 | ` * Parameters` |
|      - | 4722 | ` *  $handle` |
|      - | 4723 | ` *   The file pointer.` |
|      - | 4724 | ` *  $format` |
|      - | 4725 | ` *   String format (see sprintf()).` |
|      - | 4726 | ` * $args` |
|      - | 4727 | ` *   User arguments.` |
|      - | 4728 | ` * Return` |
|      - | 4729 | ` *  The length of the written string.` |
|      - | 4730 | ` */` |
|      6 | 4731 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4732 | `{` |
|      - | 4733 | `	fprintf_data sFdata;` |
|      - | 4734 | `	const char *zFormat;` |
|      - | 4735 | `	ph7_hashmap *pMap;` |
|      - | 4736 | `	io_private *pDev;` |
|      - | 4737 | `	SySet sArg;` |
|      - | 4738 | `	int n,nLen;` |
|      7 | 4739 | `	if( nArg < 3 ){` |
|    ! 0 | 4740 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4741 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4742 | `		return PH7_OK;` |
|      - | 4743 | `	}` |
|      - | 4744 | `	{` |
|      - | 4745 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|      7 | 4746 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|      7 | 4747 | `		if( rcs != PH7_OK ){` |
|      3 | 4748 | `			return rcs;` |
|      - | 4749 | `		}` |
|      - | 4750 | `	}` |
|      - | 4751 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 4752 | `	{` |
|      5 | 4753 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      5 | 4754 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4755 | `			return rcf;` |
|      - | 4756 | `		}` |
|      - | 4757 | `	}` |
|      5 | 4758 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 4759 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4760 | `		char zBuf[64];` |
|    ! 0 | 4761 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4762 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 4763 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 4764 | `	}` |
|      - | 4765 | `	/* Extract our private data */` |
|      5 | 4766 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4767 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4768 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4769 | `		/*Expecting an IO handle */` |
|    ! 0 | 4770 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4771 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4772 | `		return PH7_OK;` |
|      - | 4773 | `	}` |
|      - | 4774 | `	/* Point to the target IO stream device */` |
|      5 | 4775 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4776 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4777 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4778 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4779 | `			);` |
|    ! 0 | 4780 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4781 | `		return PH7_OK;` |
|      - | 4782 | `	}` |
|      - | 4783 | `	/* Extract the string format */` |
|      5 | 4784 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      5 | 4785 | `	if( nLen < 1 ){` |
|      - | 4786 | `		/* Empty string,return zero */` |
|    ! 0 | 4787 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4788 | `		return PH7_OK;` |
|      - | 4789 | `	}` |
|      - | 4790 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4791 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4792 | `	{` |
|      5 | 4793 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      5 | 4794 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4795 | `			return rcv;` |
|      - | 4796 | `		}` |
|      - | 4797 | `	}` |
|      - | 4798 | `	/* Point to hashmap */` |
|      5 | 4799 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4800 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output. */` |
|      - | 4801 | `	{` |
|      5 | 4802 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      5 | 4803 | `		if( rcc != PH7_OK ){` |
|      3 | 4804 | `			return rcc;` |
|      - | 4805 | `		}` |
|      - | 4806 | `	}` |
|      - | 4807 | `	/* Extract arguments from the hashmap */` |
|      3 | 4808 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4809 | `	/* Prepare our private data */` |
|      3 | 4810 | `	sFdata.nCount = 0;` |
|      3 | 4811 | `	sFdata.pIO = pDev;` |
|      - | 4812 | `	/* Format the string */` |
|      3 | 4813 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4814 | `	/* Return total number of bytes written*/` |
|      3 | 4815 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4816 | `	SySetRelease(&sArg);` |
|      3 | 4817 | `	return PH7_OK;` |
|      4 | 4818 | `}` |
|      - | 4819 | `/*` |
|      - | 4820 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4821 | ` * According to the PHP reference manual:` |
|      - | 4822 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4823 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4824 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4825 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4826 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4827 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4828 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4829 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4830 | ` *         exist, attempt to create it.` |
|      - | 4831 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4832 | ` *          not exist, attempt to create it.` |
|      - | 4833 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4834 | ` *         already exists,` |
|      - | 4835 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4836 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4837 | ` *         the underlying open(2) system call.` |
|      - | 4838 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4839 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4840 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4841 | ` *          is positioned on the beginning of the file.` |
|      - | 4842 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4843 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4844 | ` *          be used after the lock is requested).` |
|      - | 4845 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4846 | ` */` |
|    214 | 4847 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      3 | 4848 | `{` |
|    217 | 4849 | `	const char *zEnd = &zMode[nLen];` |
|    217 | 4850 | `	int iFlag = 0;` |
|      - | 4851 | `	int c;` |
|    217 | 4852 | `	if( nLen < 1 ){` |
|      - | 4853 | `		/* Open in a read-only mode */` |
|    ! 0 | 4854 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4855 | `	}` |
|    217 | 4856 | `	c = zMode[0];` |
|    217 | 4857 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4858 | `		/* Read-only access */` |
|     55 | 4859 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     55 | 4860 | `		zMode++; /* Advance */` |
|     55 | 4861 | `		if( zMode < zEnd ){` |
|     15 | 4862 | `			c = zMode[0];` |
|     15 | 4863 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4864 | `				/* Read+Write access */` |
|     15 | 4865 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      7 | 4866 | `			}` |
|     10 | 4867 | `		}` |
|    191 | 4868 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4869 | `		/* Overwrite mode.` |
|      - | 4870 | `		 * If the file does not exists,try to create it` |
|      - | 4871 | `		 */` |
|     34 | 4872 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     34 | 4873 | `		zMode++; /* Advance */` |
|     34 | 4874 | `		if( zMode < zEnd ){` |
|      5 | 4875 | `			c = zMode[0];` |
|      5 | 4876 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4877 | `				/* Read+Write access */` |
|      5 | 4878 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      5 | 4879 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      2 | 4880 | `			}` |
|      4 | 4881 | `		}` |
|    148 | 4882 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4883 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4884 | `		 * Create the file if it does not exists.` |
|      - | 4885 | `		 */` |
|    ! 0 | 4886 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4887 | `		zMode++; /* Advance */` |
|    ! 0 | 4888 | `		if( zMode < zEnd ){` |
|    ! 0 | 4889 | `			c = zMode[0];` |
|    ! 0 | 4890 | `			if( c == '+' ){` |
|      - | 4891 | `				/* Read-Write access */` |
|    ! 0 | 4892 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4893 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4894 | `			}` |
|    ! 0 | 4895 | `		}` |
|    132 | 4896 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4897 | `		/* Exclusive access.` |
|      - | 4898 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4899 | `		 * Otherwise create a new file.` |
|      - | 4900 | `		 */` |
|    132 | 4901 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    132 | 4902 | `		zMode++; /* Advance */` |
|    132 | 4903 | `		if( zMode < zEnd ){` |
|    ! 0 | 4904 | `			c = zMode[0];` |
|    ! 0 | 4905 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4906 | `				/* Read-Write access */` |
|    ! 0 | 4907 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4908 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4909 | `			}` |
|      2 | 4910 | `		}` |
|     65 | 4911 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4912 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4913 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4914 | `		zMode++; /* Advance */` |
|    ! 0 | 4915 | `		if( zMode < zEnd ){` |
|    ! 0 | 4916 | `			c = zMode[0];` |
|    ! 0 | 4917 | `			if( c == '+' ){` |
|      - | 4918 | `				/* Read-Write access */` |
|    ! 0 | 4919 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4920 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4921 | `			}` |
|    ! 0 | 4922 | `		}` |
|    ! 0 | 4923 | `	}else{` |
|      - | 4924 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4925 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4926 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4927 | `	}` |
|    235 | 4928 | `	while( zMode < zEnd ){` |
|     19 | 4929 | `		c = zMode[0];` |
|     19 | 4930 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4931 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4932 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     19 | 4933 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4934 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4935 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4936 | `		}` |
|     19 | 4937 | `		zMode++;` |
|      1 | 4938 | `	}` |
|    217 | 4939 | `	return iFlag;` |
|    110 | 4940 | `}` |
|      - | 4941 | `/*` |
|      - | 4942 | ` * Initialize the IO private structure.` |
|      - | 4943 | ` */` |
|   5206 | 4944 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 4945 | `{` |
|   5211 | 4946 | `	pOut->pStream = pStream;` |
|   5211 | 4947 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   5211 | 4948 | `	pOut->nOfft = 0;` |
|      - | 4949 | `	/* Set the magic number */` |
|   5211 | 4950 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   5211 | 4951 | `}` |
|      - | 4952 | `/*` |
|      - | 4953 | ` * Release the IO private structure.` |
|      - | 4954 | ` */` |
|   5166 | 4955 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      5 | 4956 | `{` |
|   5171 | 4957 | `	SyBlobRelease(&pDev->sBuffer);` |
|   5171 | 4958 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4959 | `	/* Release the whole structure */` |
|   5171 | 4960 | `	ph7_context_free_chunk(pCtx,pDev);` |
|   5171 | 4961 | `}` |
|      - | 4962 | `/*` |
|      - | 4963 | ` * Reset the IO private structure.` |
|      - | 4964 | ` */` |
|     30 | 4965 | `static void ResetIOPrivate(io_private *pDev)` |
|      2 | 4966 | `{` |
|     32 | 4967 | `	SyBlobReset(&pDev->sBuffer);` |
|     32 | 4968 | `	pDev->nOfft = 0;` |
|     32 | 4969 | `}` |
|      - | 4970 | `/* Forward declaration */` |
|      - | 4971 |  |
|      - | 4972 | `/*` |
|      - | 4973 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 4974 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 4975 | ` * Parameters` |
|      - | 4976 | ` *  $filename` |
|      - | 4977 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 4978 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 4979 | ` *   then a regular file is assumed.` |
|      - | 4980 | ` *  $mode` |
|      - | 4981 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 4982 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 4983 | ` *   modes.` |
|      - | 4984 | ` *  $use_include_path` |
|      - | 4985 | ` *   You can use the optional second parameter and set it to` |
|      - | 4986 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 4987 | ` *  $context` |
|      - | 4988 | ` *   A context stream resource.` |
|      - | 4989 | ` * Return` |
|      - | 4990 | ` *  File handle on success or FALSE on failure.` |
|      - | 4991 | ` */` |
|      - | 4992 | `/*` |
|      - | 4993 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 4994 | ` *                                  int $offset = -1)` |
|      - | 4995 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 4996 | ` */` |
|     10 | 4997 | `static int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4998 | `{` |
|      - | 4999 | `	const ph7_io_stream *pStream;` |
|      - | 5000 | `	io_private *pDev;` |
|     11 | 5001 | `	ph7_int64 nMax = -1;` |
|      - | 5002 | `	char zBuf[4096];` |
|      - | 5003 | `	ph7_int64 nRead;` |
|     11 | 5004 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 5005 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5006 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5007 | `		return PH7_OK;` |
|      - | 5008 | `	}` |
|     11 | 5009 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     11 | 5010 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5011 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5012 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5013 | `		return PH7_OK;` |
|      - | 5014 | `	}` |
|     11 | 5015 | `	pStream = pDev->pStream;` |
|     11 | 5016 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 5017 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5018 | `		return PH7_OK;` |
|      - | 5019 | `	}` |
|     11 | 5020 | `	if( nArg > 1 ){` |
|      5 | 5021 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      2 | 5022 | `	}` |
|     11 | 5023 | `	if( nArg > 2 ){` |
|      5 | 5024 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 5025 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 5026 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 5027 | `		}` |
|      2 | 5028 | `	}` |
|     11 | 5029 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     21 | 5030 | `	while( nMax != 0 ){` |
|     19 | 5031 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     19 | 5032 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 5033 | `			nAsk = nMax;` |
|      1 | 5034 | `		}` |
|     19 | 5035 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     19 | 5036 | `		if( nRead < 1 ){` |
|      9 | 5037 | `			break;` |
|      - | 5038 | `		}` |
|     11 | 5039 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     11 | 5040 | `		if( nMax > 0 ){` |
|      3 | 5041 | `			nMax -= nRead;` |
|      1 | 5042 | `		}` |
|      1 | 5043 | `	}` |
|     11 | 5044 | `	return PH7_OK;` |
|      6 | 5045 | `}` |
|      - | 5046 | `/*` |
|      - | 5047 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 5048 | ` */` |
|      4 | 5049 | `static int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5050 | `{` |
|      - | 5051 | `	ph7_value *pArr,*pV;` |
|      - | 5052 | `	ph7_io_stream **apDev;` |
|      - | 5053 | `	sxu32 n;` |
|      2 | 5054 | `	SXUNUSED(nArg);` |
|      2 | 5055 | `	SXUNUSED(apArg);` |
|      6 | 5056 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 5057 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 5058 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 5059 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5060 | `		return PH7_OK;` |
|      - | 5061 | `	}` |
|      6 | 5062 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 5063 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 5064 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 5065 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 5066 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 5067 | `	}` |
|      6 | 5068 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 5069 | `	return PH7_OK;` |
|      4 | 5070 | `}` |
|      - | 5071 | `/*` |
|      - | 5072 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 5073 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 5074 | ` */` |
|      2 | 5075 | `static int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5076 | `{` |
|      - | 5077 | `	io_private *pDev;` |
|      - | 5078 | `	ph7_value *pArr,*pV;` |
|      3 | 5079 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 5080 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5081 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5082 | `		return PH7_OK;` |
|      - | 5083 | `	}` |
|      3 | 5084 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5085 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5086 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5087 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5088 | `		return PH7_OK;` |
|      - | 5089 | `	}` |
|      3 | 5090 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 5091 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 5092 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 5093 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5094 | `		return PH7_OK;` |
|      - | 5095 | `	}` |
|      3 | 5096 | `	ph7_value_bool(pV,0);` |
|      3 | 5097 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 5098 | `	ph7_value_bool(pV,1);` |
|      3 | 5099 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 5100 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 5101 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 5102 | `	ph7_value_bool(pV,0);` |
|      3 | 5103 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 5104 | `	ph7_value_int(pV,0);` |
|      3 | 5105 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 5106 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 5107 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 5108 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 5109 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 5110 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 5111 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 5112 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 5113 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 5114 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 5115 | `	return PH7_OK;` |
|      2 | 5116 | `}` |
|      - | 5117 | `/*` |
|      - | 5118 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 5119 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 5120 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 5121 | ` * resource, options unconsumed).` |
|      - | 5122 | ` */` |
|      2 | 5123 | `static int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5124 | `{` |
|      3 | 5125 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 5126 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 5127 | `	}else{` |
|    ! 0 | 5128 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 5129 | `		if( pArr == 0 ){` |
|    ! 0 | 5130 | `			ph7_result_null(pCtx);` |
|    ! 0 | 5131 | `			return PH7_OK;` |
|      - | 5132 | `		}` |
|    ! 0 | 5133 | `		ph7_result_value(pCtx,pArr);` |
|      - | 5134 | `	}` |
|      3 | 5135 | `	return PH7_OK;` |
|      2 | 5136 | `}` |
|      - | 5137 | `/*` |
|      - | 5138 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 5139 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 5140 | ` */` |
|      - | 5141 | `#ifdef PH7_ENABLE_NET` |
|      - | 5142 | `typedef struct sock_private sock_private;` |
|      - | 5143 | `struct sock_private` |
|      - | 5144 | `{` |
|      - | 5145 | `	ph7_vm *pVm;` |
|      - | 5146 | `	ph7_socket sock;` |
|      - | 5147 | `	int bEof;` |
|      - | 5148 | `};` |
|     10 | 5149 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 5150 | `{` |
|     10 | 5151 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5152 | `	int n;` |
|     10 | 5153 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      2 | 5154 | `		return 0;` |
|      - | 5155 | `	}` |
|      8 | 5156 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|      8 | 5157 | `	if( n <= 0 ){` |
|      4 | 5158 | `		pSock->bEof = 1;` |
|      4 | 5159 | `		return 0;` |
|      - | 5160 | `	}` |
|      4 | 5161 | `	return (ph7_int64)n;` |
|      5 | 5162 | `}` |
|      4 | 5163 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 5164 | `{` |
|      4 | 5165 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5166 | `	int n;` |
|      4 | 5167 | `	if( pSock == 0 ){` |
|    ! 0 | 5168 | `		return -1;` |
|      - | 5169 | `	}` |
|      4 | 5170 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 5171 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 5172 | `}` |
|      4 | 5173 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 5174 | `{` |
|      4 | 5175 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 5176 | `	if( pSock == 0 ){` |
|    ! 0 | 5177 | `		return;` |
|      - | 5178 | `	}` |
|      4 | 5179 | `	PH7_NetClose(pSock->sock);` |
|      4 | 5180 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 5181 | `}` |
|      - | 5182 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 5183 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 5184 | `{` |
|      - | 5185 | `	sock_private *pSock;` |
|      - | 5186 | `	ph7_socket sock;` |
|      - | 5187 | `	char zHost[256];` |
|      - | 5188 | `	const char *zColon;` |
|    ! 0 | 5189 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 5190 | `	const char *zErr = "";` |
|    ! 0 | 5191 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 5192 | `	SXUNUSED(iMode);` |
|    ! 0 | 5193 | `	if( pVm == 0 ){` |
|    ! 0 | 5194 | `		return -1;` |
|      - | 5195 | `	}` |
|    ! 0 | 5196 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 5197 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 5198 | `		zColon--;` |
|    ! 0 | 5199 | `	}` |
|    ! 0 | 5200 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 5201 | `		return -1;` |
|      - | 5202 | `	}` |
|      - | 5203 | `	{` |
|    ! 0 | 5204 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 5205 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 5206 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 5207 | `		}` |
|    ! 0 | 5208 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 5209 | `		zHost[n] = 0;` |
|      - | 5210 | `	}` |
|      - | 5211 | `	{` |
|    ! 0 | 5212 | `		sxi32 iTmp = 0;` |
|    ! 0 | 5213 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 5214 | `		iPort = (int)iTmp;` |
|      - | 5215 | `	}` |
|    ! 0 | 5216 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 5217 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 5218 | `		return -1;` |
|      - | 5219 | `	}` |
|    ! 0 | 5220 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 5221 | `	if( pSock == 0 ){` |
|    ! 0 | 5222 | `		PH7_NetClose(sock);` |
|    ! 0 | 5223 | `		return -1;` |
|      - | 5224 | `	}` |
|    ! 0 | 5225 | `	pSock->pVm = pVm;` |
|    ! 0 | 5226 | `	pSock->sock = sock;` |
|    ! 0 | 5227 | `	pSock->bEof = 0;` |
|    ! 0 | 5228 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 5229 | `	return PH7_OK;` |
|    ! 0 | 5230 | `}` |
|      - | 5231 | `static const ph7_io_stream sTCP_Stream = {` |
|      - | 5232 | `	"tcp",` |
|      - | 5233 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5234 | `	SockStreamData_Open, /* xOpen */` |
|      - | 5235 | `	0,   /* xOpenDir */` |
|      - | 5236 | `	SockStreamData_Close,/* xClose */` |
|      - | 5237 | `	0,  /* xCloseDir */` |
|      - | 5238 | `	SockStreamData_Read, /* xRead */` |
|      - | 5239 | `	0,  /* xReadDir */` |
|      - | 5240 | `	SockStreamData_Write,/* xWrite */` |
|      - | 5241 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 5242 | `	0,  /* xLock */` |
|      - | 5243 | `	0,  /* xRewindDir */` |
|      - | 5244 | `	0,  /* xTell */` |
|      - | 5245 | `	0,  /* xTrunc */` |
|      - | 5246 | `	0,  /* xSync */` |
|      - | 5247 | `	0   /* xStat */` |
|      - | 5248 | `};` |
|      - | 5249 | `#endif /* PH7_ENABLE_NET */` |
|      - | 5250 | `/*` |
|      - | 5251 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 5252 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 5253 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 5254 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 5255 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 5256 | ` * php streamWrapper protocol method on it.` |
|      - | 5257 | ` */` |
|      - | 5258 | `#define PHL_UWRAP_MAX 8` |
|      - | 5259 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 5260 | `struct uwrap_slot` |
|      - | 5261 | `{` |
|      - | 5262 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 5263 | `	char zScheme[32];         /* protocol name */` |
|      - | 5264 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 5265 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 5266 | `};` |
|      - | 5267 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 5268 | `struct uwrap_handle` |
|      - | 5269 | `{` |
|      - | 5270 | `	ph7_vm *pVm;` |
|      - | 5271 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 5272 | `	int iSlot;` |
|      - | 5273 | `	int bEof;` |
|      - | 5274 | `};` |
|      - | 5275 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 5276 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 5277 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 5278 | `	ph7_value *pResult)` |
|      1 | 5279 | `{` |
|      - | 5280 | `	ph7_class_method *pMeth;` |
|     27 | 5281 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 5282 | `		return -1;` |
|      - | 5283 | `	}` |
|     27 | 5284 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 5285 | `	if( pMeth == 0 ){` |
|    ! 0 | 5286 | `		return -1;` |
|      - | 5287 | `	}` |
|     27 | 5288 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 5289 | `		return -1;` |
|      - | 5290 | `	}` |
|     27 | 5291 | `	return 0;` |
|     14 | 5292 | `}` |
|      8 | 5293 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 5294 | `{` |
|      9 | 5295 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5296 | `	ph7_value sArg,sRet;` |
|      - | 5297 | `	const char *zData;` |
|      9 | 5298 | `	int nData = 0;` |
|      9 | 5299 | `	ph7_int64 n = 0;` |
|      9 | 5300 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 5301 | `		return 0;` |
|      - | 5302 | `	}` |
|      9 | 5303 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 5304 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 5305 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 5306 | `	{` |
|      - | 5307 | `		ph7_value *apArg[1];` |
|      9 | 5308 | `		apArg[0] = &sArg;` |
|      9 | 5309 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5310 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5311 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5312 | `			return -1;` |
|      - | 5313 | `		}` |
|      - | 5314 | `	}` |
|      9 | 5315 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 5316 | `	if( nData > 0 ){` |
|      7 | 5317 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 5318 | `			nData = (int)nRead;` |
|    ! 0 | 5319 | `		}` |
|      7 | 5320 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 5321 | `		n = nData;` |
|      4 | 5322 | `	}else{` |
|      3 | 5323 | `		pH->bEof = 1;` |
|      - | 5324 | `	}` |
|      9 | 5325 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 5326 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 5327 | `	return n;` |
|      5 | 5328 | `}` |
|      2 | 5329 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5330 | `{` |
|      3 | 5331 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5332 | `	ph7_value sArg,sRet;` |
|      - | 5333 | `	ph7_int64 n;` |
|      3 | 5334 | `	if( pH == 0 ){` |
|    ! 0 | 5335 | `		return -1;` |
|      - | 5336 | `	}` |
|      3 | 5337 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 5338 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5339 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 5340 | `	{` |
|      - | 5341 | `		ph7_value *apArg[1];` |
|      3 | 5342 | `		apArg[0] = &sArg;` |
|      3 | 5343 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5344 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5345 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5346 | `			return -1;` |
|      - | 5347 | `		}` |
|      - | 5348 | `	}` |
|      3 | 5349 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5350 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 5351 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5352 | `	return n;` |
|      2 | 5353 | `}` |
|      2 | 5354 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5355 | `{` |
|      3 | 5356 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5357 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 5358 | `	ph7_value *apArg[2];` |
|      - | 5359 | `	int rc;` |
|      3 | 5360 | `	if( pH == 0 ){` |
|    ! 0 | 5361 | `		return -1;` |
|      - | 5362 | `	}` |
|      3 | 5363 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 5364 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 5365 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5366 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 5367 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 5368 | `	apArg[0] = &sOfft;` |
|      3 | 5369 | `	apArg[1] = &sWhence;` |
|      3 | 5370 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 5371 | `	if( rc == 0 ){` |
|      3 | 5372 | `		pH->bEof = 0;` |
|      3 | 5373 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 5374 | `	}` |
|      3 | 5375 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 5376 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 5377 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5378 | `	return rc;` |
|      2 | 5379 | `}` |
|      2 | 5380 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 5381 | `{` |
|      3 | 5382 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5383 | `	ph7_value sRet;` |
|      - | 5384 | `	ph7_int64 n;` |
|      3 | 5385 | `	if( pH == 0 ){` |
|    ! 0 | 5386 | `		return -1;` |
|      - | 5387 | `	}` |
|      3 | 5388 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5389 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 5390 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5391 | `		return -1;` |
|      - | 5392 | `	}` |
|      3 | 5393 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5394 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5395 | `	return n;` |
|      2 | 5396 | `}` |
|      6 | 5397 | `static void UwrapClose(void *pHandle)` |
|      1 | 5398 | `{` |
|      7 | 5399 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 5400 | `	if( pH == 0 ){` |
|    ! 0 | 5401 | `		return;` |
|      - | 5402 | `	}` |
|      7 | 5403 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 5404 | `	if( pH->pObj ){` |
|      7 | 5405 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 5406 | `	}` |
|      7 | 5407 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 5408 | `}` |
|      - | 5409 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 5410 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 5411 | `{` |
|      7 | 5412 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 5413 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 5414 | `	ph7_class *pClass;` |
|      - | 5415 | `	uwrap_handle *pH;` |
|      - | 5416 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 5417 | `	ph7_value *apArg[4];` |
|      - | 5418 | `	int rc;` |
|      7 | 5419 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 5420 | `		return -1;` |
|      - | 5421 | `	}` |
|      7 | 5422 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 5423 | `	if( pClass == 0 ){` |
|    ! 0 | 5424 | `		return -1;` |
|      - | 5425 | `	}` |
|      7 | 5426 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 5427 | `	if( pH == 0 ){` |
|    ! 0 | 5428 | `		return -1;` |
|      - | 5429 | `	}` |
|      7 | 5430 | `	pH->pVm = pVm;` |
|      7 | 5431 | `	pH->iSlot = iSlot;` |
|      7 | 5432 | `	pH->bEof = 0;` |
|      7 | 5433 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 5434 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 5435 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5436 | `		return -1;` |
|      - | 5437 | `	}` |
|      - | 5438 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 5439 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 5440 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 5441 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 5442 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 5443 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 5444 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 5445 | `	 * it as "could not be passed by reference"). */` |
|      - | 5446 | `	{` |
|      7 | 5447 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 5448 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 5449 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5450 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5451 | `			return -1;` |
|      - | 5452 | `		}` |
|      7 | 5453 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 5454 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 5455 | `	}` |
|      - | 5456 | `	{` |
|      - | 5457 | `		SyBlob sUrl;` |
|      7 | 5458 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 5459 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 5460 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 5461 | `		SyBlobRelease(&sUrl);` |
|      - | 5462 | `	}` |
|      9 | 5463 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 5464 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 5465 | `	ph7_value_int(&sOpts,0);` |
|      7 | 5466 | `	apArg[0] = &sPath;` |
|      7 | 5467 | `	apArg[1] = &sMode;` |
|      7 | 5468 | `	apArg[2] = &sOpts;` |
|      7 | 5469 | `	apArg[3] = &sOpened;` |
|      7 | 5470 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 5471 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 5472 | `		rc = -1;` |
|    ! 0 | 5473 | `	}` |
|      7 | 5474 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 5475 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 5476 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 5477 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 5478 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 5479 | `	if( rc != 0 ){` |
|    ! 0 | 5480 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5481 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5482 | `		return -1;` |
|      - | 5483 | `	}` |
|      7 | 5484 | `	*ppHandle = (void *)pH;` |
|      7 | 5485 | `	return PH7_OK;` |
|      4 | 5486 | `}` |
|      - | 5487 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 5488 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 5489 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 5490 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 5491 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 5492 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 5493 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 5494 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 5495 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 5496 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 5497 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 5498 | `PHL_UWRAP_THUNK(7)` |
|      - | 5499 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 5500 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 5501 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 5502 | `};` |
|      - | 5503 | `/*` |
|      - | 5504 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 5505 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5506 | ` */` |
|      2 | 5507 | `static int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5508 | `{` |
|      - | 5509 | `	const char *zScheme,*zClass;` |
|      3 | 5510 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 5511 | `	if( nArg < 2 ){` |
|    ! 0 | 5512 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5513 | `		return PH7_OK;` |
|      - | 5514 | `	}` |
|      3 | 5515 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5516 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 5517 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 5518 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 5519 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5520 | `		return PH7_OK;` |
|      - | 5521 | `	}` |
|      - | 5522 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 5523 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 5524 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 5525 | `	{` |
|      3 | 5526 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 5527 | `		sxu32 n;` |
|     11 | 5528 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 5529 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 5530 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 5531 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5532 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 5533 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5534 | `				return PH7_OK;` |
|      - | 5535 | `			}` |
|      5 | 5536 | `		}` |
|      - | 5537 | `	}` |
|      3 | 5538 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 5539 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 5540 | `			iFree = i;` |
|      3 | 5541 | `			break;` |
|      - | 5542 | `		}` |
|    ! 0 | 5543 | `	}` |
|      3 | 5544 | `	if( iFree < 0 ){` |
|    ! 0 | 5545 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5546 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 5547 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5548 | `		return PH7_OK;` |
|      - | 5549 | `	}` |
|      - | 5550 | `	{` |
|      3 | 5551 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 5552 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 5553 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 5554 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 5555 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 5556 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 5557 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 5558 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 5559 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 5560 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 5561 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 5562 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 5563 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 5564 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 5565 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 5566 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 5567 | `	}` |
|      3 | 5568 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5569 | `	return PH7_OK;` |
|      2 | 5570 | `}` |
|      2 | 5571 | `static int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5572 | `{` |
|      - | 5573 | `	const char *zScheme;` |
|      - | 5574 | `	int nScheme,i;` |
|      3 | 5575 | `	if( nArg < 1 ){` |
|    ! 0 | 5576 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5577 | `		return PH7_OK;` |
|      - | 5578 | `	}` |
|      3 | 5579 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5580 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 5581 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 5582 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 5583 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 5584 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 5585 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 5586 | `			 * what unregister means to a script — recorded. */` |
|      3 | 5587 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 5588 | `			ph7_result_bool(pCtx,1);` |
|      3 | 5589 | `			return PH7_OK;` |
|      - | 5590 | `		}` |
|    ! 0 | 5591 | `	}` |
|    ! 0 | 5592 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 5593 | `	return PH7_OK;` |
|      2 | 5594 | `}` |
|      - | 5595 | `#ifdef PH7_ENABLE_NET` |
|      - | 5596 | `/*` |
|      - | 5597 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 5598 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 5599 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 5600 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 5601 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 5602 | ` */` |
|      6 | 5603 | `static int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 5604 | `{` |
|      6 | 5605 | `	const char *zFunc = ph7_function_name(pCtx);` |
|      6 | 5606 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|      6 | 5607 | `	const char *zTarget,*zErr = "";` |
|      - | 5608 | `	char zHost[256];` |
|      6 | 5609 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 5610 | `	ph7_socket sock;` |
|      - | 5611 | `	io_private *pDev;` |
|      - | 5612 | `	sock_private *pSock;` |
|      6 | 5613 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|      6 | 5614 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|      6 | 5615 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|      6 | 5616 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 5617 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5618 | `		return PH7_OK;` |
|      - | 5619 | `	}` |
|      6 | 5620 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 5621 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 5622 | `	{` |
|      6 | 5623 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|      6 | 5624 | `		const char *zSep = 0;` |
|     32 | 5625 | `		while( z < zEnd - 2 ){` |
|     30 | 5626 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 5627 | `				zSep = z;` |
|      4 | 5628 | `				break;` |
|      - | 5629 | `			}` |
|     26 | 5630 | `			z++;` |
|    ! 0 | 5631 | `		}` |
|      6 | 5632 | `		if( zSep ){` |
|      4 | 5633 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 5634 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5635 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 5636 | `					nTarget,zTarget);` |
|    ! 0 | 5637 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5638 | `				return PH7_OK;` |
|      - | 5639 | `			}` |
|      4 | 5640 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 5641 | `			zTarget = zSep + 3;` |
|      2 | 5642 | `		}` |
|      - | 5643 | `	}` |
|      - | 5644 | `	/* host[:port] */` |
|      - | 5645 | `	{` |
|      6 | 5646 | `		int i = nTarget - 1;` |
|      6 | 5647 | `		int nHost = nTarget;` |
|     48 | 5648 | `		while( i > 0 && zTarget[i] != ':' ){` |
|     42 | 5649 | `			i--;` |
|    ! 0 | 5650 | `		}` |
|      6 | 5651 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 5652 | `			sxi32 iTmp = 0;` |
|      2 | 5653 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 5654 | `			iPort = (int)iTmp;` |
|      2 | 5655 | `			nHost = i;` |
|      1 | 5656 | `		}` |
|      6 | 5657 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 5658 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 5659 | `		}` |
|      6 | 5660 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|      6 | 5661 | `		zHost[nHost] = 0;` |
|      - | 5662 | `	}` |
|      6 | 5663 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      4 | 5664 | `		iPort = ph7_value_to_int(apArg[1]);` |
|      2 | 5665 | `	}` |
|      6 | 5666 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|      6 | 5667 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|      6 | 5668 | `		if( rTimeout > 0 ){` |
|      6 | 5669 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|      3 | 5670 | `		}` |
|      3 | 5671 | `	}` |
|      6 | 5672 | `	if( iPort < 0 ){` |
|    ! 0 | 5673 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5674 | `		return PH7_OK;` |
|      - | 5675 | `	}` |
|      6 | 5676 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|      6 | 5677 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5678 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 5679 | `		{` |
|      2 | 5680 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      2 | 5681 | `			if( pTmp ){` |
|      2 | 5682 | `				if( nArg > iArgErrno ){` |
|      2 | 5683 | `					ph7_value_int(pTmp,iErrno);` |
|      2 | 5684 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      1 | 5685 | `				}` |
|      2 | 5686 | `				if( nArg > iArgErrstr ){` |
|      2 | 5687 | `					ph7_value_string(pTmp,zErr,-1);` |
|      2 | 5688 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      1 | 5689 | `				}` |
|      1 | 5690 | `			}` |
|      - | 5691 | `		}` |
|      - | 5692 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|      3 | 5693 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 5694 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|      2 | 5695 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5696 | `		return PH7_OK;` |
|      - | 5697 | `	}` |
|      - | 5698 | `	{` |
|      4 | 5699 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 5700 | `		if( pTmp ){` |
|      4 | 5701 | `			if( nArg > iArgErrno ){` |
|      4 | 5702 | `				ph7_value_int(pTmp,0);` |
|      4 | 5703 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 5704 | `			}` |
|      4 | 5705 | `			if( nArg > iArgErrstr ){` |
|      4 | 5706 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 5707 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 5708 | `			}` |
|      2 | 5709 | `		}` |
|      - | 5710 | `	}` |
|      - | 5711 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 5712 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 5713 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 5714 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 5715 | `		PH7_NetClose(sock);` |
|    ! 0 | 5716 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5717 | `		return PH7_OK;` |
|      - | 5718 | `	}` |
|      4 | 5719 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 5720 | `	pSock->sock = sock;` |
|      4 | 5721 | `	pSock->bEof = 0;` |
|      4 | 5722 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 5723 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 5724 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 5725 | `	return PH7_OK;` |
|      3 | 5726 | `}` |
|      - | 5727 | `#endif /* PH7_ENABLE_NET */` |
|    214 | 5728 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5729 | `{` |
|      - | 5730 | `	const ph7_io_stream *pStream;` |
|      - | 5731 | `	const char *zUri,*zMode;` |
|      - | 5732 | `	ph7_value *pResource;` |
|      - | 5733 | `	io_private *pDev;` |
|      - | 5734 | `	int iLen,imLen;` |
|      - | 5735 | `	int iOpenFlags;` |
|    217 | 5736 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5737 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5738 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 5739 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5740 | `		return PH7_OK;` |
|      - | 5741 | `	}` |
|      - | 5742 | `	/* Extract the URI and the desired access mode */` |
|    217 | 5743 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    217 | 5744 | `	if( nArg > 1 ){` |
|    217 | 5745 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    110 | 5746 | `	}else{` |
|      - | 5747 | `		/* Set a default read-only mode */` |
|    ! 0 | 5748 | `		zMode = "r";` |
|    ! 0 | 5749 | `		imLen = (int)sizeof(char);` |
|      - | 5750 | `	}` |
|      - | 5751 | `	/* Try to extract a stream */` |
|    217 | 5752 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    217 | 5753 | `	if( pStream == 0 ){` |
|    ! 0 | 5754 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5755 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 5756 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5757 | `		return PH7_OK;` |
|      - | 5758 | `	}` |
|      - | 5759 | `	/* Allocate a new IO private instance */` |
|    217 | 5760 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    217 | 5761 | `	if( pDev == 0 ){` |
|    ! 0 | 5762 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5763 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5764 | `		return PH7_OK;` |
|      - | 5765 | `	}` |
|    217 | 5766 | `	pResource = 0;` |
|    217 | 5767 | `	if( nArg > 3 ){` |
|    ! 0 | 5768 | `		pResource = apArg[3];` |
|    217 | 5769 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 5770 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 5771 | `		 * access the underlying virtual machine.` |
|      - | 5772 | `		 */` |
|     17 | 5773 | `		pResource = apArg[0];` |
|      8 | 5774 | `	}` |
|      - | 5775 | `	/* Initialize the structure */` |
|    217 | 5776 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 5777 | `	/* Convert open mode to PH7 flags */` |
|    217 | 5778 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 5779 | `	/* Try to get a handle */` |
|    324 | 5780 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    107 | 5781 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|    217 | 5782 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 5783 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|    ! 0 | 5784 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5785 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 5786 | `		return PH7_OK;` |
|      - | 5787 | `	}` |
|      - | 5788 | `	/* All done,return the io_private instance as a resource */` |
|    217 | 5789 | `	ph7_result_resource(pCtx,pDev);` |
|    217 | 5790 | `	return PH7_OK;` |
|    110 | 5791 | `}` |
|      - | 5792 | `/*` |
|      - | 5793 | ` * bool fclose(resource $handle)` |
|      - | 5794 | ` *  Closes an open file pointer` |
|      - | 5795 | ` * Parameters` |
|      - | 5796 | ` *  $handle` |
|      - | 5797 | ` *   The file pointer.` |
|      - | 5798 | ` * Return` |
|      - | 5799 | ` *  TRUE on success or FALSE on failure.` |
|      - | 5800 | ` */` |
|    312 | 5801 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5802 | `{` |
|      - | 5803 | `	const ph7_io_stream *pStream;` |
|      - | 5804 | `	io_private *pDev;` |
|      - | 5805 | `	ph7_vm *pVm;` |
|    317 | 5806 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5807 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5808 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5809 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5810 | `		return PH7_OK;` |
|      - | 5811 | `	}` |
|      - | 5812 | `	/* Extract our private data */` |
|    317 | 5813 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5814 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    317 | 5815 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 5816 | `		/*Expecting an IO handle */` |
|    ! 0 | 5817 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5818 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5819 | `		return PH7_OK;` |
|      - | 5820 | `	}` |
|      - | 5821 | `	/* Point to the target IO stream device */` |
|    317 | 5822 | `	pStream = pDev->pStream;` |
|    317 | 5823 | `	if( pStream == 0 ){` |
|    ! 0 | 5824 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5825 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 5826 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 5827 | `			);` |
|    ! 0 | 5828 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5829 | `		return PH7_OK;` |
|      - | 5830 | `	}` |
|      - | 5831 | `	/* Point to the VM that own this context */` |
|    317 | 5832 | `	pVm = pCtx->pVm;` |
|      - | 5833 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    317 | 5834 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 5835 | `		/* Perform the requested operation */` |
|    317 | 5836 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 5837 | `		/* Release the IO private structure */` |
|    317 | 5838 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      - | 5839 | `		/* Invalidate the resource handle */` |
|    317 | 5840 | `		ph7_value_release(apArg[0]);` |
|    156 | 5841 | `	}` |
|      - | 5842 | `	/* Return TRUE */` |
|    317 | 5843 | `	ph7_result_bool(pCtx,1);` |
|    317 | 5844 | `	return PH7_OK;` |
|    161 | 5845 | `}` |
|      - | 5846 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5847 | `/*` |
|      - | 5848 | ` * MD5/SHA1 digest consumer.` |
|      - | 5849 | ` */` |
|     72 | 5850 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 5851 | `{` |
|      - | 5852 | `	/* Append hex chunk verbatim */` |
|     73 | 5853 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 5854 | `	return SXRET_OK;` |
|      1 | 5855 | `}` |
|      - | 5856 | `/*` |
|      - | 5857 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 5858 | ` *  Calculates the md5 hash of a given file.` |
|      - | 5859 | ` * Parameters` |
|      - | 5860 | ` *  $uri` |
|      - | 5861 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5862 | ` *  $raw_output` |
|      - | 5863 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 5864 | ` * Return` |
|      - | 5865 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 5866 | ` */` |
|      2 | 5867 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5868 | `{` |
|      - | 5869 | `	const ph7_io_stream *pStream;` |
|      - | 5870 | `	unsigned char zDigest[16];` |
|      3 | 5871 | `	int raw_output  = FALSE;` |
|      - | 5872 | `	const char *zFile;` |
|      - | 5873 | `	MD5Context sCtx;` |
|      - | 5874 | `	char zBuf[8192];` |
|      - | 5875 | `	void *pHandle;` |
|      - | 5876 | `	ph7_int64 n;` |
|      - | 5877 | `	int nLen;` |
|      3 | 5878 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5879 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5880 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5881 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5882 | `		return PH7_OK;` |
|      - | 5883 | `	}` |
|      - | 5884 | `	/* Extract the file path */` |
|      3 | 5885 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5886 | `	/* Point to the target IO stream device */` |
|      3 | 5887 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5888 | `	if( pStream == 0 ){` |
|    ! 0 | 5889 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5890 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5891 | `		return PH7_OK;` |
|      - | 5892 | `	}` |
|      3 | 5893 | `	if( nArg > 1 ){` |
|    ! 0 | 5894 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5895 | `	}` |
|      - | 5896 | `	/* Try to open the file in read-only mode */` |
|      3 | 5897 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5898 | `	if( pHandle == 0 ){` |
|    ! 0 | 5899 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 5900 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5901 | `		return PH7_OK;` |
|      - | 5902 | `	}` |
|      - | 5903 | `	/* Init the MD5 context */` |
|      3 | 5904 | `	MD5Init(&sCtx);` |
|      - | 5905 | `	/* Perform the requested operation */` |
|      2 | 5906 | `	for(;;){` |
|      5 | 5907 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5908 | `		if( n < 1 ){` |
|      - | 5909 | `			/* EOF or IO error,break immediately */` |
|      3 | 5910 | `			break;` |
|      - | 5911 | `		}` |
|      3 | 5912 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5913 | `	}` |
|      - | 5914 | `	/* Close the stream */` |
|      3 | 5915 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5916 | `	/* Extract the digest */` |
|      3 | 5917 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 5918 | `	if( raw_output ){` |
|      - | 5919 | `		/* Output raw digest */` |
|    ! 0 | 5920 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5921 | `	}else{` |
|      - | 5922 | `		/* Perform a binary to hex conversion */` |
|      3 | 5923 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5924 | `	}` |
|      3 | 5925 | `	return PH7_OK;` |
|      2 | 5926 | `}` |
|      - | 5927 | `/*` |
|      - | 5928 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 5929 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 5930 | ` * Parameters` |
|      - | 5931 | ` *  $uri` |
|      - | 5932 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5933 | ` *  $raw_output` |
|      - | 5934 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 5935 | ` * Return` |
|      - | 5936 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 5937 | ` */` |
|      2 | 5938 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5939 | `{` |
|      - | 5940 | `	const ph7_io_stream *pStream;` |
|      - | 5941 | `	unsigned char zDigest[20];` |
|      3 | 5942 | `	int raw_output  = FALSE;` |
|      - | 5943 | `	const char *zFile;` |
|      - | 5944 | `	SHA1Context sCtx;` |
|      - | 5945 | `	char zBuf[8192];` |
|      - | 5946 | `	void *pHandle;` |
|      - | 5947 | `	ph7_int64 n;` |
|      - | 5948 | `	int nLen;` |
|      3 | 5949 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5950 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5951 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5952 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5953 | `		return PH7_OK;` |
|      - | 5954 | `	}` |
|      - | 5955 | `	/* Extract the file path */` |
|      3 | 5956 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5957 | `	/* Point to the target IO stream device */` |
|      3 | 5958 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5959 | `	if( pStream == 0 ){` |
|    ! 0 | 5960 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5961 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5962 | `		return PH7_OK;` |
|      - | 5963 | `	}` |
|      3 | 5964 | `	if( nArg > 1 ){` |
|    ! 0 | 5965 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5966 | `	}` |
|      - | 5967 | `	/* Try to open the file in read-only mode */` |
|      3 | 5968 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5969 | `	if( pHandle == 0 ){` |
|    ! 0 | 5970 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 5971 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5972 | `		return PH7_OK;` |
|      - | 5973 | `	}` |
|      - | 5974 | `	/* Init the SHA1 context */` |
|      3 | 5975 | `	SHA1Init(&sCtx);` |
|      - | 5976 | `	/* Perform the requested operation */` |
|      2 | 5977 | `	for(;;){` |
|      5 | 5978 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5979 | `		if( n < 1 ){` |
|      - | 5980 | `			/* EOF or IO error,break immediately */` |
|      3 | 5981 | `			break;` |
|      - | 5982 | `		}` |
|      3 | 5983 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5984 | `	}` |
|      - | 5985 | `	/* Close the stream */` |
|      3 | 5986 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5987 | `	/* Extract the digest */` |
|      3 | 5988 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 5989 | `	if( raw_output ){` |
|      - | 5990 | `		/* Output raw digest */` |
|    ! 0 | 5991 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5992 | `	}else{` |
|      - | 5993 | `		/* Perform a binary to hex conversion */` |
|      3 | 5994 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5995 | `	}` |
|      3 | 5996 | `	return PH7_OK;` |
|      2 | 5997 | `}` |
|      - | 5998 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5999 | `/*` |
|      - | 6000 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 6001 | ` *  Parse a configuration file.` |
|      - | 6002 | ` * Parameters` |
|      - | 6003 | ` * $filename` |
|      - | 6004 | ` *  The filename of the ini file being parsed.` |
|      - | 6005 | ` * $process_sections` |
|      - | 6006 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 6007 | ` *  with the section names and settings included.` |
|      - | 6008 | ` *  The default for process_sections is FALSE.` |
|      - | 6009 | ` * $scanner_mode` |
|      - | 6010 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 6011 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 6012 | ` * Return` |
|      - | 6013 | ` *  The settings are returned as an associative array on success.` |
|      - | 6014 | ` *  Otherwise is returned.` |
|      - | 6015 | ` */` |
|      2 | 6016 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6017 | `{` |
|      - | 6018 | `	const ph7_io_stream *pStream;` |
|      - | 6019 | `	const char *zFile;` |
|      - | 6020 | `	SyBlob sContents;` |
|      - | 6021 | `	void *pHandle;` |
|      - | 6022 | `	int nLen;` |
|      3 | 6023 | `	sxi32 rc = PH7_OK;` |
|      3 | 6024 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6025 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 6026 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 6027 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6028 | `		return PH7_OK;` |
|      - | 6029 | `	}` |
|      - | 6030 | `	/* Extract the file path */` |
|      3 | 6031 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 6032 | `	/* Point to the target IO stream device */` |
|      3 | 6033 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 6034 | `	if( pStream == 0 ){` |
|    ! 0 | 6035 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 6036 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6037 | `		return PH7_OK;` |
|      - | 6038 | `	}` |
|      - | 6039 | `	/* Try to open the file in read-only mode */` |
|      3 | 6040 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 6041 | `	if( pHandle == 0 ){` |
|    ! 0 | 6042 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 6043 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6044 | `		return PH7_OK;` |
|      - | 6045 | `	}` |
|      3 | 6046 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 6047 | `	/* Read the whole file */` |
|      3 | 6048 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 6049 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 6050 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 6051 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6052 | `	}else{` |
|      - | 6053 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 6054 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 6055 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 6056 | `	}` |
|      - | 6057 | `	/* Close the stream */` |
|      3 | 6058 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 6059 | `	/* Release the working buffer */` |
|      3 | 6060 | `	SyBlobRelease(&sContents);` |
|      - | 6061 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 6062 | `	return rc;` |
|      2 | 6063 | `}` |
|      - | 6064 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 6065 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 6066 | `/*` |
|      - | 6067 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 6068 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 6069 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 6070 | ` * to any non-VFS resource).` |
|      - | 6071 | ` */` |
|      - | 6072 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 6073 | `{` |
|      - | 6074 | `	SXUNUSED(pResource);` |
|      - | 6075 | `	return "Unknown";` |
|      - | 6076 | `}` |
|      - | 6077 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 6078 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 6079 | `#if defined(_MSC_VER)` |
|      - | 6080 | `static const ph7_vfs null_vfs = {` |
|      - | 6081 | `#else` |
|      - | 6082 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 6083 | `#endif` |
|      - | 6084 | `	"null_vfs",` |
|      - | 6085 | `	PH7_VFS_VERSION,` |
|      - | 6086 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 6087 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 6088 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 6089 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 6090 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 6091 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 6092 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 6093 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 6094 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 6095 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 6096 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 6097 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 6098 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 6099 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 6100 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 6101 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 6102 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 6103 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 6104 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 6105 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 6106 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6107 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6108 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 6109 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 6110 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 6111 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 6112 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 6113 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 6114 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 6115 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 6116 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 6117 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 6118 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 6119 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 6120 | `	0, /* int (*xUmask)(int) */` |
|      - | 6121 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 6122 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 6123 | `	0, /* int (*xUid)(void) */` |
|      - | 6124 | `	0, /* int (*xGid)(void) */` |
|      - | 6125 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 6126 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 6127 | `};` |
|      - | 6128 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 6129 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 6130 | `/*` |
|      - | 6131 | ` * Export the builtin vfs.` |
|      - | 6132 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 6133 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 6134 | ` * Note:` |
|      - | 6135 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 6136 | ` * Note:` |
|      - | 6137 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6138 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 6139 | ` */` |
|   3856 | 6140 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 6141 | `{` |
|      - | 6142 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6143 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 6144 | `	return &null_vfs;` |
|      - | 6145 | `#else` |
|      - | 6146 | `#ifdef __WINNT__` |
|      5 | 6147 | `	return &sWinVfs;` |
|      - | 6148 | `#elif defined(__UNIXES__)` |
|   3856 | 6149 | `	return &sUnixVfs;` |
|      - | 6150 | `#else` |
|      - | 6151 | `	return &null_vfs;` |
|      - | 6152 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 6153 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 6154 | `#else` |
|      - | 6155 | `	return &null_vfs;` |
|      - | 6156 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 6157 | `}` |
|      - | 6158 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6159 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6160 | `/*` |
|      - | 6161 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 6162 | ` * no particular meaning on windows.` |
|      - | 6163 | ` */` |
|      - | 6164 | `#ifndef STDIN_FILENO` |
|      - | 6165 | `#define STDIN_FILENO	0` |
|      - | 6166 | `#endif` |
|      - | 6167 | `#ifndef STDOUT_FILENO` |
|      - | 6168 | `#define STDOUT_FILENO	1` |
|      - | 6169 | `#endif` |
|      - | 6170 | `#ifndef STDERR_FILENO` |
|      - | 6171 | `#define STDERR_FILENO	2` |
|      - | 6172 | `#endif` |
|      - | 6173 | `/*` |
|      - | 6174 | ` * php:// Accessing various I/O streams` |
|      - | 6175 | ` * According to the PHP langage reference manual` |
|      - | 6176 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 6177 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 6178 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 6179 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 6180 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 6181 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 6182 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 6183 | ` * php://output` |
|      - | 6184 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 6185 | ` *  mechanism in the same way as print and echo.` |
|      - | 6186 | ` */` |
|      - | 6187 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 6188 | `/* Supported IO streams */` |
|      - | 6189 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 6190 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 6191 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 6192 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 6193 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|      - | 6194 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 6195 | `struct ph7_stream_data` |
|      - | 6196 | `{` |
|      - | 6197 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 6198 | `	int iType;   /* Stream type */` |
|      - | 6199 | `	union{` |
|      - | 6200 | `		void *pHandle; /* Stream handle */` |
|      - | 6201 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 6202 | `	}x;` |
|      - | 6203 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|      - | 6204 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|      - | 6205 | `	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */` |
|      - | 6206 | `};` |
|      - | 6207 | `/*` |
|      - | 6208 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 6209 | ` */` |
|     28 | 6210 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 6211 | `{` |
|      - | 6212 | `	ph7_stream_data *pData;` |
|     29 | 6213 | `	if( pVm == 0 ){` |
|    ! 0 | 6214 | `		return 0;` |
|      - | 6215 | `	}` |
|      - | 6216 | `	/* Allocate a new instance */` |
|     29 | 6217 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|     29 | 6218 | `	if( pData == 0 ){` |
|    ! 0 | 6219 | `		return 0;` |
|      - | 6220 | `	}` |
|      - | 6221 | `	/* Zero the structure */` |
|     29 | 6222 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 6223 | `	/* Initialize fields */` |
|     29 | 6224 | `	pData->iType = iType;` |
|     29 | 6225 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|     29 | 6226 | `	pData->nCur = 0;` |
|     29 | 6227 | `	pData->bReadOnly = 0;` |
|     29 | 6228 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6229 | `		/* Nothing else to set up: the buffer is the stream */` |
|     19 | 6230 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 6231 | `		/* Point to the default VM consumer routine. */` |
|      3 | 6232 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 6233 | `	}else{` |
|      - | 6234 | `#ifdef __WINNT__` |
|      - | 6235 | `		DWORD nChannel;` |
|      1 | 6236 | `		switch(iType){` |
|      1 | 6237 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 6238 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 6239 | `		default:` |
|      1 | 6240 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 6241 | `			break;` |
|      - | 6242 | `		}` |
|      1 | 6243 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 6244 | `#else` |
|      - | 6245 | `		/* Assume an UNIX system */` |
|      6 | 6246 | `		int ifd = STDIN_FILENO;` |
|      6 | 6247 | `		switch(iType){` |
|      2 | 6248 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 6249 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 6250 | `		default:` |
|      2 | 6251 | `			break;` |
|      - | 6252 | `		}` |
|      6 | 6253 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 6254 | `#endif` |
|      - | 6255 | `	}` |
|     29 | 6256 | `	pData->pVm = pVm;` |
|     29 | 6257 | `	return pData;` |
|     15 | 6258 | `}` |
|      - | 6259 | `/*` |
|      - | 6260 | ` * Implementation of the php:// IO streams routines` |
|      - | 6261 | ` * Status:` |
|      - | 6262 | ` *   Stable.` |
|      - | 6263 | ` */` |
|      - | 6264 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|     12 | 6265 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6266 | `{` |
|      - | 6267 | `	ph7_stream_data *pData;` |
|      - | 6268 | `	SyString sStream;` |
|     13 | 6269 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 6270 | `	/* Trim leading and trailing white spaces */` |
|     13 | 6271 | `	SyStringFullTrim(&sStream);` |
|      - | 6272 | `	/* Stream to open */` |
|     13 | 6273 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 6274 | `		iMode = PH7_IO_STREAM_STDIN;` |
|     13 | 6275 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 6276 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|     12 | 6277 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 6278 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|     11 | 6279 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 6280 | `		iMode = PH7_IO_STREAM_STDERR;` |
|     10 | 6281 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|      7 | 6282 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|      - | 6283 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|      - | 6284 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|     11 | 6285 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|      6 | 6286 | `	}else{` |
|      - | 6287 | `		/* unknown stream name */` |
|    ! 0 | 6288 | `		return -1;` |
|      - | 6289 | `	}` |
|      - | 6290 | `	/* Create our handle */` |
|     13 | 6291 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|     13 | 6292 | `	if( pData == 0 ){` |
|    ! 0 | 6293 | `		return -1;` |
|      - | 6294 | `	}` |
|      - | 6295 | `	/* Make the handle public */` |
|     13 | 6296 | `	*ppHandle = (void *)pData;` |
|     13 | 6297 | `	return PH7_OK;` |
|      7 | 6298 | `}` |
|      - | 6299 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|     42 | 6300 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|      1 | 6301 | `{` |
|     43 | 6302 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     43 | 6303 | `	if( pData == 0 ){` |
|    ! 0 | 6304 | `		return -1;` |
|      - | 6305 | `	}` |
|     43 | 6306 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     43 | 6307 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|      - | 6308 | `		sxu32 nRead;` |
|     43 | 6309 | `		if( pData->nCur >= nAvail ){` |
|     15 | 6310 | `			return 0; /* EOF */` |
|      - | 6311 | `		}` |
|     29 | 6312 | `		nRead = nAvail - pData->nCur;` |
|     29 | 6313 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|      7 | 6314 | `			nRead = (sxu32)nDatatoRead;` |
|      3 | 6315 | `		}` |
|     29 | 6316 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|     29 | 6317 | `		pData->nCur += nRead;` |
|     29 | 6318 | `		return (ph7_int64)nRead;` |
|      - | 6319 | `	}` |
|    ! 0 | 6320 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 6321 | `		/* Forbidden */` |
|    ! 0 | 6322 | `		return -1;` |
|      - | 6323 | `	}` |
|      - | 6324 | `#ifdef __WINNT__` |
|      - | 6325 | `	{` |
|      - | 6326 | `		DWORD nRd;` |
|      - | 6327 | `		BOOL rc;` |
|    ! 0 | 6328 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 6329 | `		if( !rc ){` |
|      - | 6330 | `			/* IO error */` |
|    ! 0 | 6331 | `			return -1;` |
|      - | 6332 | `		}` |
|    ! 0 | 6333 | `		return (ph7_int64)nRd;` |
|      - | 6334 | `	}` |
|      - | 6335 | `#elif defined(__UNIXES__)` |
|      - | 6336 | `	{` |
|      - | 6337 | `		ssize_t nRd;` |
|      - | 6338 | `		int fd;` |
|    ! 0 | 6339 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6340 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 6341 | `		if( nRd < 1 ){` |
|    ! 0 | 6342 | `			return -1;` |
|      - | 6343 | `		}` |
|    ! 0 | 6344 | `		return (ph7_int64)nRd;` |
|      - | 6345 | `	}` |
|      - | 6346 | `#else` |
|      - | 6347 | `	return -1;` |
|      - | 6348 | `#endif` |
|     22 | 6349 | `}` |
|      - | 6350 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|     12 | 6351 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 6352 | `{` |
|     13 | 6353 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     13 | 6354 | `	if( pData == 0 ){` |
|    ! 0 | 6355 | `		return -1;` |
|      - | 6356 | `	}` |
|     13 | 6357 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 6358 | `		/* Forbidden */` |
|    ! 0 | 6359 | `		return -1;` |
|     13 | 6360 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6361 | `		sxu32 nLen,nEnd;` |
|     11 | 6362 | `		if( pData->bReadOnly ){` |
|    ! 0 | 6363 | `			return -1;` |
|      - | 6364 | `		}` |
|     11 | 6365 | `		nLen = SyBlobLength(&pData->sMem);` |
|     11 | 6366 | `		if( pData->nCur > nLen ){` |
|      - | 6367 | `			/* seek past end: php zero-fills the gap */` |
|      - | 6368 | `			static const char zZero[64] = {0};` |
|    ! 0 | 6369 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|    ! 0 | 6370 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|    ! 0 | 6371 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6372 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6373 | `					return -1;` |
|      - | 6374 | `				}` |
|    ! 0 | 6375 | `			}` |
|    ! 0 | 6376 | `			nLen = SyBlobLength(&pData->sMem);` |
|    ! 0 | 6377 | `		}` |
|     11 | 6378 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|     11 | 6379 | `		if( pData->nCur < nLen ){` |
|      - | 6380 | `			/* overwrite in place up to the current end */` |
|      3 | 6381 | `			sxu32 nOver = nLen - pData->nCur;` |
|      3 | 6382 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|      3 | 6383 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|      3 | 6384 | `			if( nEnd > nLen ){` |
|    ! 0 | 6385 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|    ! 0 | 6386 | `					return -1;` |
|      - | 6387 | `				}` |
|    ! 0 | 6388 | `			}` |
|      2 | 6389 | `		}else{` |
|      9 | 6390 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|    ! 0 | 6391 | `				return -1;` |
|      - | 6392 | `			}` |
|      - | 6393 | `		}` |
|     11 | 6394 | `		pData->nCur = nEnd;` |
|     11 | 6395 | `		return nWrite;` |
|      3 | 6396 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 6397 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 6398 | `		int rc;` |
|      - | 6399 | `		/* Call the vm output consumer */` |
|      3 | 6400 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 6401 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 6402 | `			return -1;` |
|      - | 6403 | `		}` |
|      3 | 6404 | `		return nWrite;` |
|      - | 6405 | `	}` |
|      - | 6406 | `#ifdef __WINNT__` |
|      - | 6407 | `	{` |
|      - | 6408 | `		DWORD nWr;` |
|      - | 6409 | `		BOOL rc;` |
|    ! 0 | 6410 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 6411 | `		if( !rc ){` |
|      - | 6412 | `			/* IO error */` |
|    ! 0 | 6413 | `			return -1;` |
|      - | 6414 | `		}` |
|    ! 0 | 6415 | `		return (ph7_int64)nWr;` |
|      - | 6416 | `	}` |
|      - | 6417 | `#elif defined(__UNIXES__)` |
|      - | 6418 | `	{` |
|      - | 6419 | `		ssize_t nWr;` |
|      - | 6420 | `		int fd;` |
|    ! 0 | 6421 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6422 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 6423 | `		if( nWr < 1 ){` |
|    ! 0 | 6424 | `			return -1;` |
|      - | 6425 | `		}` |
|    ! 0 | 6426 | `		return (ph7_int64)nWr;` |
|      - | 6427 | `	}` |
|      - | 6428 | `#else` |
|      - | 6429 | `	return -1;` |
|      - | 6430 | `#endif` |
|      7 | 6431 | `}` |
|      - | 6432 | `/* void (*xClose)(void *) */` |
|     18 | 6433 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 6434 | `{` |
|     19 | 6435 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6436 | `	ph7_vm *pVm;` |
|     19 | 6437 | `	if( pData == 0 ){` |
|    ! 0 | 6438 | `		return;` |
|      - | 6439 | `	}` |
|     19 | 6440 | `	pVm = pData->pVm;` |
|     19 | 6441 | `	SyBlobRelease(&pData->sMem);` |
|      - | 6442 | `	/* Free the instance */` |
|     19 | 6443 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|     10 | 6444 | `}` |
|      - | 6445 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|     20 | 6446 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 6447 | `{` |
|     21 | 6448 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6449 | `	ph7_int64 iNew;` |
|     21 | 6450 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6451 | `		return -1;` |
|      - | 6452 | `	}` |
|     21 | 6453 | `	switch(whence){` |
|    ! 0 | 6454 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|      3 | 6455 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|     19 | 6456 | `	default:            iNew = iOfft; break;` |
|      - | 6457 | `	}` |
|     21 | 6458 | `	if( iNew < 0 ){` |
|    ! 0 | 6459 | `		return -1;` |
|      - | 6460 | `	}` |
|     21 | 6461 | `	pData->nCur = (sxu32)iNew;` |
|     21 | 6462 | `	return PH7_OK;` |
|     11 | 6463 | `}` |
|      - | 6464 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|      4 | 6465 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|      1 | 6466 | `{` |
|      5 | 6467 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      5 | 6468 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6469 | `		return -1;` |
|      - | 6470 | `	}` |
|      5 | 6471 | `	return (ph7_int64)pData->nCur;` |
|      3 | 6472 | `}` |
|      - | 6473 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|    ! 0 | 6474 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|    ! 0 | 6475 | `{` |
|    ! 0 | 6476 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 6477 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|    ! 0 | 6478 | `		return -1;` |
|      - | 6479 | `	}` |
|    ! 0 | 6480 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|      - | 6481 | `		/* shrink in place: the blob keeps its allocation */` |
|    ! 0 | 6482 | `		pData->sMem.nByte = (sxu32)nLen;` |
|    ! 0 | 6483 | `	}else{` |
|      - | 6484 | `		static const char zZero[64] = {0};` |
|    ! 0 | 6485 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|    ! 0 | 6486 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|    ! 0 | 6487 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6488 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6489 | `				return -1;` |
|      - | 6490 | `			}` |
|    ! 0 | 6491 | `		}` |
|      - | 6492 | `	}` |
|    ! 0 | 6493 | `	return PH7_OK;` |
|    ! 0 | 6494 | `}` |
|      - | 6495 | `/*` |
|      - | 6496 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|      - | 6497 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|      - | 6498 | ` * base64). Shares the MEMORY machinery above.` |
|      - | 6499 | ` */` |
|      8 | 6500 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 6501 | `{` |
|      9 | 6502 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|      1 | 6503 | `}` |
|     10 | 6504 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6505 | `{` |
|      - | 6506 | `	ph7_stream_data *pData;` |
|     11 | 6507 | `	const char *zIn = zName;` |
|     11 | 6508 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|     11 | 6509 | `	const char *zComma = 0;` |
|     11 | 6510 | `	int bBase64 = 0;` |
|      5 | 6511 | `	SXUNUSED(iMode);` |
|      - | 6512 | `	/* Find the comma separating the mediatype from the payload */` |
|    105 | 6513 | `	while( zIn < zEnd ){` |
|    105 | 6514 | `		if( zIn[0] == ',' ){` |
|     11 | 6515 | `			zComma = zIn;` |
|     11 | 6516 | `			break;` |
|      - | 6517 | `		}` |
|     95 | 6518 | `		zIn++;` |
|      1 | 6519 | `	}` |
|     11 | 6520 | `	if( zComma == 0 ){` |
|    ! 0 | 6521 | `		return -1;` |
|      - | 6522 | `	}` |
|     10 | 6523 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|     10 | 6524 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|      3 | 6525 | `		bBase64 = 1;` |
|      1 | 6526 | `	}` |
|     11 | 6527 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|     11 | 6528 | `	if( pData == 0 ){` |
|    ! 0 | 6529 | `		return -1;` |
|      - | 6530 | `	}` |
|     11 | 6531 | `	pData->bReadOnly = 1;` |
|     11 | 6532 | `	zIn = &zComma[1];` |
|     11 | 6533 | `	if( bBase64 ){` |
|      3 | 6534 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|    ! 0 | 6535 | `			SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6536 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6537 | `			return -1;` |
|      - | 6538 | `		}` |
|      2 | 6539 | `	}else{` |
|      - | 6540 | `		/* percent-decode the payload */` |
|     71 | 6541 | `		while( zIn < zEnd ){` |
|     63 | 6542 | `			char c = zIn[0];` |
|     63 | 6543 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|      3 | 6544 | `				int hi = SyHexToint(zIn[1]);` |
|      3 | 6545 | `				int lo = SyHexToint(zIn[2]);` |
|      3 | 6546 | `				c = (char)((hi << 4) \| lo);` |
|      3 | 6547 | `				zIn += 3;` |
|      2 | 6548 | `			}else{` |
|     61 | 6549 | `				zIn++;` |
|      - | 6550 | `			}` |
|     63 | 6551 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|    ! 0 | 6552 | `				SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6553 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6554 | `				return -1;` |
|      - | 6555 | `			}` |
|      1 | 6556 | `		}` |
|      - | 6557 | `	}` |
|     11 | 6558 | `	*ppHandle = (void *)pData;` |
|     11 | 6559 | `	return PH7_OK;` |
|      6 | 6560 | `}` |
|      - | 6561 | `/* data:// rejects writes outright */` |
|    ! 0 | 6562 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 6563 | `{` |
|    ! 0 | 6564 | `	SXUNUSED(pHandle);` |
|    ! 0 | 6565 | `	SXUNUSED(pBuf);` |
|    ! 0 | 6566 | `	SXUNUSED(nWrite);` |
|    ! 0 | 6567 | `	return -1;` |
|    ! 0 | 6568 | `}` |
|      - | 6569 | `static const ph7_io_stream sDATA_Stream = {` |
|      - | 6570 | `	"data",` |
|      - | 6571 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6572 | `	DataStreamData_Open,  /* xOpen */` |
|      - | 6573 | `	0,   /* xOpenDir */` |
|      - | 6574 | `	PHPStreamData_Close, /* xClose */` |
|      - | 6575 | `	0,  /* xCloseDir */` |
|      - | 6576 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 6577 | `	0,  /* xReadDir */` |
|      - | 6578 | `	DataStreamData_Write, /* xWrite */` |
|      - | 6579 | `	PHPStreamData_Seek,  /* xSeek */` |
|      - | 6580 | `	0,  /* xLock */` |
|      - | 6581 | `	0,  /* xRewindDir */` |
|      - | 6582 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 6583 | `	0,  /* xTrunc */` |
|      - | 6584 | `	0,  /* xSync */` |
|      - | 6585 | `	0   /* xStat */` |
|      - | 6586 | `};` |
|      - | 6587 | `/*` |
|      - | 6588 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 6589 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 6590 | ` * PHP-compatible process I/O functionality.` |
|      - | 6591 | ` */` |
|      - | 6592 | `typedef struct pipe_private pipe_private;` |
|      - | 6593 | `struct pipe_private` |
|      - | 6594 | `{` |
|      - | 6595 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 6596 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 6597 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 6598 | `#ifdef __WINNT__` |
|      - | 6599 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 6600 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 6601 | `#endif` |
|      - | 6602 | `};` |
|      - | 6603 |  |
|      - | 6604 | `#ifdef __WINNT__` |
|      - | 6605 | `#include <Windows.h>` |
|      - | 6606 | `#include <stdio.h>` |
|      - | 6607 | `#include <io.h>` |
|      - | 6608 | `#include <fcntl.h>` |
|      - | 6609 | `/*` |
|      - | 6610 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 6611 | ` * This allows us to properly wait for process completion.` |
|      - | 6612 | ` */` |
|      - | 6613 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      5 | 6614 | `{` |
|      5 | 6615 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      5 | 6616 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      5 | 6617 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 6618 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 6619 | `	STARTUPINFOW si;` |
|      - | 6620 | `	PROCESS_INFORMATION pi;` |
|      5 | 6621 | `	WCHAR *zWideCmd = NULL;` |
|      5 | 6622 | `	FILE *pFile = NULL;` |
|      - | 6623 | `	int fd;` |
|      5 | 6624 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 6625 |  |
|      - | 6626 | `	/* Set up security attributes for pipe inheritance */` |
|      5 | 6627 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      5 | 6628 | `	sa.bInheritHandle = TRUE;` |
|      5 | 6629 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 6630 |  |
|      - | 6631 | `	/* Create pipes for child process I/O */` |
|      5 | 6632 | `	if( bRead ){` |
|      - | 6633 | `		/* Reading from child's stdout */` |
|      5 | 6634 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 6635 | `			return NULL;` |
|      - | 6636 | `		}` |
|      - | 6637 | `		/* Ensure read handle is not inherited */` |
|      5 | 6638 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      5 | 6639 | `		hReadPipe = hChildStdoutRd;` |
|      5 | 6640 | `		*phPipe = hChildStdoutRd;` |
|      5 | 6641 | `	}else{` |
|      - | 6642 | `		/* Writing to child's stdin */` |
|    ! 0 | 6643 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 6644 | `			return NULL;` |
|      - | 6645 | `		}` |
|      - | 6646 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 6647 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 6648 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 6649 | `		*phPipe = hChildStdinWr;` |
|      - | 6650 | `	}` |
|      - | 6651 |  |
|      - | 6652 | `	/* Convert command to wide string */` |
|      - | 6653 | `	{` |
|      5 | 6654 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      5 | 6655 | `		if( nLen <= 0 ){` |
|    ! 0 | 6656 | `			goto cleanup_pipes;` |
|      - | 6657 | `		}` |
|      5 | 6658 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      5 | 6659 | `		if( !zWideCmd ){` |
|    ! 0 | 6660 | `			goto cleanup_pipes;` |
|      - | 6661 | `		}` |
|      5 | 6662 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 6663 | `	}` |
|      - | 6664 |  |
|      - | 6665 | `	/* Set up process startup info */` |
|      5 | 6666 | `	ZeroMemory(&si, sizeof(si));` |
|      5 | 6667 | `	si.cb = sizeof(si);` |
|      5 | 6668 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      5 | 6669 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      5 | 6670 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      5 | 6671 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      5 | 6672 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 6673 |  |
|      5 | 6674 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 6675 |  |
|      - | 6676 | `	/* Create the child process */` |
|      5 | 6677 | `	if( !CreateProcessW(` |
|      - | 6678 | `		NULL,           /* Application name */` |
|      - | 6679 | `		zWideCmd,       /* Command line */` |
|      - | 6680 | `		NULL,           /* Process security attributes */` |
|      - | 6681 | `		NULL,           /* Thread security attributes */` |
|      - | 6682 | `		TRUE,           /* Inherit handles */` |
|      - | 6683 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 6684 | `		NULL,           /* Environment */` |
|      - | 6685 | `		NULL,           /* Current directory */` |
|      - | 6686 | `		&si,            /* Startup info */` |
|      - | 6687 | `		&pi             /* Process info */` |
|      - | 6688 | `	)){` |
|    ! 0 | 6689 | `		goto cleanup_all;` |
|      - | 6690 | `	}` |
|      - | 6691 |  |
|      - | 6692 | `	/* Close handles we don't need in parent */` |
|      5 | 6693 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      5 | 6694 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 6695 |  |
|      - | 6696 | `	/* Close thread handle (we only need process handle) */` |
|      5 | 6697 | `	CloseHandle(pi.hThread);` |
|      - | 6698 |  |
|      - | 6699 | `	/* Store process handle for later waiting */` |
|      5 | 6700 | `	*phProcess = pi.hProcess;` |
|      - | 6701 |  |
|      - | 6702 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      5 | 6703 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 6704 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      5 | 6705 | `	if( fd == -1 ){` |
|    ! 0 | 6706 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6707 | `		*phProcess = NULL;` |
|    ! 0 | 6708 | `		goto cleanup_all;` |
|      - | 6709 | `	}` |
|      - | 6710 |  |
|      5 | 6711 | `	pFile = _fdopen(fd, zMode);` |
|      5 | 6712 | `	if( !pFile ){` |
|    ! 0 | 6713 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 6714 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6715 | `		*phProcess = NULL;` |
|    ! 0 | 6716 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 6717 | `		return NULL;` |
|      - | 6718 | `	}` |
|      - | 6719 |  |
|      5 | 6720 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      5 | 6721 | `	return pFile;` |
|      - | 6722 |  |
|      - | 6723 | `cleanup_all:` |
|    ! 0 | 6724 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 6725 | `cleanup_pipes:` |
|    ! 0 | 6726 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 6727 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 6728 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 6729 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 6730 | `	return NULL;` |
|      5 | 6731 | `}` |
|      - | 6732 |  |
|      - | 6733 | `/*` |
|      - | 6734 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 6735 | ` */` |
|      - | 6736 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      5 | 6737 | `{` |
|      5 | 6738 | `	DWORD dwExitCode = 0;` |
|      - | 6739 | `	int status;` |
|      - | 6740 |  |
|      - | 6741 | `	/* Close the FILE* (this closes the pipe) */` |
|      5 | 6742 | `	fclose(pFile);` |
|      - | 6743 |  |
|      5 | 6744 | `	if( hProcess ){` |
|      - | 6745 | `		/* Wait for the process to complete */` |
|      5 | 6746 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 6747 |  |
|      5 | 6748 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      5 | 6749 | `			status = (int)dwExitCode;` |
|      5 | 6750 | `		}else{` |
|    ! 0 | 6751 | `			status = -1;` |
|      - | 6752 | `		}` |
|      - | 6753 |  |
|      - | 6754 | `		/* Close process handle */` |
|      5 | 6755 | `		CloseHandle(hProcess);` |
|      5 | 6756 | `	}else{` |
|    ! 0 | 6757 | `		status = -1;` |
|      - | 6758 | `	}` |
|      - | 6759 |  |
|      5 | 6760 | `	return status;` |
|      5 | 6761 | `}` |
|      - | 6762 | `#endif /* __WINNT__ */` |
|      - | 6763 | `/*` |
|      - | 6764 | ` * Open a pipe to a process.` |
|      - | 6765 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 6766 | ` */` |
|   3928 | 6767 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      5 | 6768 | `{` |
|      - | 6769 | `	pipe_private *pPipe;` |
|      - | 6770 | `	FILE *pFile;` |
|   3933 | 6771 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 6772 | `		return 0;` |
|      - | 6773 | `	}` |
|      - | 6774 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   3933 | 6775 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 6776 | `		return 0;` |
|      - | 6777 | `	}` |
|      - | 6778 | `	/* Open the pipe using system popen */` |
|      - | 6779 | `#ifdef __WINNT__` |
|      - | 6780 | `	{` |
|      - | 6781 | `		/* Build cmd.exe command wrapper */` |
|      5 | 6782 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      5 | 6783 | `		const char *zShellSuffix = "\"";` |
|      5 | 6784 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      5 | 6785 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      5 | 6786 | `		size_t nCmd = strlen(zCommand);` |
|      5 | 6787 | `		size_t nQuotes = 0;` |
|      5 | 6788 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6789 | `			if (zCommand[i] == '"') nQuotes++;` |
|      5 | 6790 | `		}` |
|      5 | 6791 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      5 | 6792 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      5 | 6793 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 6794 | `			return 0;` |
|      - | 6795 | `		}` |
|      - | 6796 | `		/* Escape quotes in command */` |
|      5 | 6797 | `		size_t j = 0;` |
|      5 | 6798 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6799 | `			char ch = zCommand[i];` |
|      5 | 6800 | `			if (ch == '"') {` |
|      4 | 6801 | `				zCmdEsc[j++] = '^';` |
|      4 | 6802 | `				zCmdEsc[j++] = '"';` |
|      4 | 6803 | `			} else {` |
|      5 | 6804 | `				zCmdEsc[j++] = ch;` |
|      - | 6805 | `			}` |
|      5 | 6806 | `		}` |
|      5 | 6807 | `		zCmdEsc[j] = '\0';` |
|      5 | 6808 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      5 | 6809 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      5 | 6810 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 6811 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6812 | `			return 0;` |
|      - | 6813 | `		}` |
|      5 | 6814 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      5 | 6815 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      5 | 6816 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      5 | 6817 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 6818 | `		/* Allocate pipe structure early so we can store handles */` |
|      5 | 6819 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      5 | 6820 | `		if( pPipe == 0 ){` |
|    ! 0 | 6821 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6822 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 6823 | `			return 0;` |
|      - | 6824 | `		}` |
|      - | 6825 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      5 | 6826 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      5 | 6827 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      5 | 6828 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      5 | 6829 | `		if( pFile == 0 ){` |
|    ! 0 | 6830 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 6831 | `			return 0;` |
|      - | 6832 | `		}` |
|      - | 6833 | `		/* Initialize remaining fields */` |
|      5 | 6834 | `		pPipe->pFile = pFile;` |
|      5 | 6835 | `		pPipe->pVm = pVm;` |
|      5 | 6836 | `		pPipe->iMode = zMode[0];` |
|      - | 6837 | `	}` |
|      - | 6838 | `#elif defined(__UNIXES__) /* Unix */` |
|   3928 | 6839 | `	pFile = popen(zCommand, zMode);` |
|   3928 | 6840 | `	if( pFile == 0 ){` |
|    ! 0 | 6841 | `		return 0;` |
|      - | 6842 | `	}` |
|      - | 6843 | `	/* Allocate pipe private structure */` |
|   3928 | 6844 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   3928 | 6845 | `	if( pPipe == 0 ){` |
|      - | 6846 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 6847 | `		pclose(pFile);` |
|    ! 0 | 6848 | `		return 0;` |
|      - | 6849 | `	}` |
|      - | 6850 | `	/* Initialize the structure */` |
|   3928 | 6851 | `	pPipe->pFile = pFile;` |
|   3928 | 6852 | `	pPipe->pVm = pVm;` |
|   3928 | 6853 | `	pPipe->iMode = zMode[0];` |
|      - | 6854 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6855 | `	(void)pFile;` |
|      - | 6856 | `	return 0;` |
|      - | 6857 | `#endif` |
|   3933 | 6858 | `	return pPipe;` |
|   1969 | 6859 | `}` |
|      - | 6860 | `/*` |
|      - | 6861 | ` * Close a pipe and return the exit status of the process.` |
|      - | 6862 | ` * Returns the exit status, or -1 on error.` |
|      - | 6863 | ` */` |
|   3902 | 6864 | `static int PipeClose(pipe_private *pPipe)` |
|      5 | 6865 | `{` |
|      - | 6866 | `	int status;` |
|      - | 6867 | `	ph7_vm *pVm;` |
|   3907 | 6868 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6869 | `		return -1;` |
|      - | 6870 | `	}` |
|   3907 | 6871 | `	pVm = pPipe->pVm;` |
|      - | 6872 | `	/* Close the pipe and get exit status */` |
|      - | 6873 | `#ifdef __WINNT__` |
|      - | 6874 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      5 | 6875 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 6876 | `#elif defined(__UNIXES__)` |
|   3902 | 6877 | `	status = pclose(pPipe->pFile);` |
|      - | 6878 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   3902 | 6879 | `	if( status != -1 ){` |
|   3902 | 6880 | `		if( WIFEXITED(status) ){` |
|   3902 | 6881 | `			status = WEXITSTATUS(status);` |
|   1951 | 6882 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 6883 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 6884 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 6885 | `		}else{` |
|      - | 6886 | `			/* Unknown termination reason */` |
|    ! 0 | 6887 | `			status = -1;` |
|      - | 6888 | `		}` |
|   1951 | 6889 | `	}` |
|      - | 6890 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6891 | `	status = -1;` |
|      - | 6892 | `#endif` |
|      - | 6893 | `	/* Free the structure */` |
|   3907 | 6894 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   3907 | 6895 | `	return status;` |
|   1956 | 6896 | `}` |
|      - | 6897 | `/*` |
|      - | 6898 | ` * Pipe stream xClose implementation.` |
|      - | 6899 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 6900 | ` * It closes the pipe but does not return the exit status.` |
|      - | 6901 | ` */` |
|    100 | 6902 | `static void PipeStream_Close(void *pHandle)` |
|      4 | 6903 | `{` |
|    104 | 6904 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    104 | 6905 | `	if( pPipe ){` |
|    104 | 6906 | `		PipeClose(pPipe);` |
|     50 | 6907 | `	}` |
|    104 | 6908 | `}` |
|      - | 6909 | `/*` |
|      - | 6910 | ` * Pipe stream xRead implementation.` |
|      - | 6911 | ` */` |
|   5838 | 6912 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      4 | 6913 | `{` |
|   5842 | 6914 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6915 | `	size_t nRead;` |
|   5842 | 6916 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6917 | `		return -1;` |
|      - | 6918 | `	}` |
|   5842 | 6919 | `	if( pPipe->iMode != 'r' ){` |
|      - | 6920 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 6921 | `		return -1;` |
|      - | 6922 | `	}` |
|   5842 | 6923 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   5842 | 6924 | `	if( nRead == 0 ){` |
|   3936 | 6925 | `		if( feof(pPipe->pFile) ){` |
|   3936 | 6926 | `			return 0; /* EOF */` |
|      - | 6927 | `		}` |
|    ! 0 | 6928 | `		return -1; /* Error */` |
|      - | 6929 | `	}` |
|   1910 | 6930 | `	return (ph7_int64)nRead;` |
|   2923 | 6931 | `}` |
|      - | 6932 | `/*` |
|      - | 6933 | ` * Pipe stream xWrite implementation.` |
|      - | 6934 | ` */` |
|      2 | 6935 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 6936 | `{` |
|      2 | 6937 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6938 | `	size_t nWritten;` |
|      2 | 6939 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6940 | `		return -1;` |
|      - | 6941 | `	}` |
|      2 | 6942 | `	if( pPipe->iMode != 'w' ){` |
|      - | 6943 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 6944 | `		return -1;` |
|      - | 6945 | `	}` |
|      2 | 6946 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 6947 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 6948 | `		return -1; /* Error */` |
|      - | 6949 | `	}` |
|      2 | 6950 | `	return (ph7_int64)nWritten;` |
|      1 | 6951 | `}` |
|      - | 6952 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 6953 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 6954 | `	"pipe",` |
|      - | 6955 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6956 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|      - | 6957 | `	0,  /* xOpenDir */` |
|      - | 6958 | `	PipeStream_Close,  /* xClose */` |
|      - | 6959 | `	0,  /* xCloseDir */` |
|      - | 6960 | `	PipeStream_Read,   /* xRead */` |
|      - | 6961 | `	0,  /* xReadDir */` |
|      - | 6962 | `	PipeStream_Write,  /* xWrite */` |
|      - | 6963 | `	0,  /* xSeek */` |
|      - | 6964 | `	0,  /* xLock */` |
|      - | 6965 | `	0,  /* xRewindDir */` |
|      - | 6966 | `	0,  /* xTell */` |
|      - | 6967 | `	0,  /* xTrunc */` |
|      - | 6968 | `	0,  /* xSync */` |
|      - | 6969 | `	0   /* xStat */` |
|      - | 6970 | `};` |
|      - | 6971 | `/*` |
|      - | 6972 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 6973 | ` * FALSE otherwise.` |
|      - | 6974 | ` */` |
|   3798 | 6975 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      5 | 6976 | `{` |
|   3803 | 6977 | `	return pStream == &sPipe_Stream;` |
|      5 | 6978 | `}` |
|      - | 6979 | `/*` |
|      - | 6980 | ` * resource popen(string $command, string $mode)` |
|      - | 6981 | ` *  Opens process file pointer.` |
|      - | 6982 | ` * Parameters` |
|      - | 6983 | ` *  $command` |
|      - | 6984 | ` *   The command to execute. Passed to the system shell.` |
|      - | 6985 | ` *  $mode` |
|      - | 6986 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 6987 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 6988 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 6989 | ` * Return` |
|      - | 6990 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 6991 | ` */` |
|      - | 6992 | `/*` |
|      - | 6993 | ` * string\|false\|null shell_exec(string $command)` |
|      - | 6994 | ` *  Execute a command via the shell and return the complete output as a string.` |
|      - | 6995 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|      - | 6996 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|      - | 6997 | ` */` |
|      4 | 6998 | `static int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6999 | `{` |
|      - | 7000 | `	const char *zCommand;` |
|      - | 7001 | `	pipe_private *pPipe;` |
|      - | 7002 | `	SyBlob sOut;` |
|      - | 7003 | `	char zBuf[4096];` |
|      - | 7004 | `	size_t nRead;` |
|      - | 7005 | `	int nCmdLen;` |
|      6 | 7006 | `	if( nArg < 1 ){` |
|    ! 0 | 7007 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7008 | `		return PH7_OK;` |
|      - | 7009 | `	}` |
|      6 | 7010 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|      6 | 7011 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7012 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7013 | `		return PH7_OK;` |
|      - | 7014 | `	}` |
|      6 | 7015 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|      6 | 7016 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 7017 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7018 | `		return PH7_OK;` |
|      - | 7019 | `	}` |
|      6 | 7020 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|      4 | 7021 | `	for(;;){` |
|     10 | 7022 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|     10 | 7023 | `		if( nRead < 1 ){` |
|      6 | 7024 | `			break;` |
|      - | 7025 | `		}` |
|      6 | 7026 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|      2 | 7027 | `	}` |
|      6 | 7028 | `	PipeClose(pPipe);` |
|      6 | 7029 | `	if( SyBlobLength(&sOut) < 1 ){` |
|      - | 7030 | `		/* php answers NULL, not "", when the command printed nothing */` |
|    ! 0 | 7031 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7032 | `	}else{` |
|      6 | 7033 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|      - | 7034 | `	}` |
|      6 | 7035 | `	SyBlobRelease(&sOut);` |
|      6 | 7036 | `	return PH7_OK;` |
|      4 | 7037 | `}` |
|   3924 | 7038 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7039 | `{` |
|      - | 7040 | `	const char *zCommand, *zMode;` |
|      - | 7041 | `	pipe_private *pPipe;` |
|      - | 7042 | `	io_private *pDev;` |
|      - | 7043 | `	int nCmdLen, nModeLen;` |
|   3929 | 7044 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 7045 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 7046 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 7047 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7048 | `		return PH7_OK;` |
|      - | 7049 | `	}` |
|      - | 7050 | `	/* Extract the command and mode */` |
|   3929 | 7051 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   3929 | 7052 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   3929 | 7053 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7054 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 7055 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7056 | `		return PH7_OK;` |
|      - | 7057 | `	}` |
|   3929 | 7058 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 7059 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 7060 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7061 | `		return PH7_OK;` |
|      - | 7062 | `	}` |
|      - | 7063 | `	/* Open the pipe */` |
|   3929 | 7064 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   3929 | 7065 | `	if( pPipe == 0 ){` |
|      - | 7066 | `		/* Failed to open pipe */` |
|    ! 0 | 7067 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7068 | `		return PH7_OK;` |
|      - | 7069 | `	}` |
|      - | 7070 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   3929 | 7071 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   3929 | 7072 | `	if( pDev == 0 ){` |
|    ! 0 | 7073 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 7074 | `		PipeClose(pPipe);` |
|    ! 0 | 7075 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7076 | `		return PH7_OK;` |
|      - | 7077 | `	}` |
|      - | 7078 | `	/* Initialize the io_private structure */` |
|   3929 | 7079 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   3929 | 7080 | `	pDev->pHandle = pPipe;` |
|      - | 7081 | `	/* Return the io_private instance as a resource */` |
|   3929 | 7082 | `	ph7_result_resource(pCtx, pDev);` |
|   3929 | 7083 | `	return PH7_OK;` |
|   1967 | 7084 | `}` |
|      - | 7085 | `/*` |
|      - | 7086 | ` * int pclose(resource $handle)` |
|      - | 7087 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 7088 | ` * Parameters` |
|      - | 7089 | ` *  $handle` |
|      - | 7090 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 7091 | ` * Return` |
|      - | 7092 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 7093 | ` */` |
|   3798 | 7094 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7095 | `{` |
|      - | 7096 | `	const ph7_io_stream *pStream;` |
|      - | 7097 | `	pipe_private *pPipe;` |
|      - | 7098 | `	io_private *pDev;` |
|      - | 7099 | `	int status;` |
|   3803 | 7100 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 7101 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 7102 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7103 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7104 | `		return PH7_OK;` |
|      - | 7105 | `	}` |
|      - | 7106 | `	/* Extract our private data */` |
|   3803 | 7107 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 7108 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3803 | 7109 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 7110 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7111 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7112 | `		return PH7_OK;` |
|      - | 7113 | `	}` |
|      - | 7114 | `	/* Point to the target IO stream device */` |
|   3803 | 7115 | `	pStream = pDev->pStream;` |
|   3803 | 7116 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 7117 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 7118 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7119 | `		return PH7_OK;` |
|      - | 7120 | `	}` |
|      - | 7121 | `	/* Get the pipe handle */` |
|   3803 | 7122 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 7123 | `	/* Close the pipe and get exit status */` |
|   3803 | 7124 | `	status = PipeClose(pPipe);` |
|      - | 7125 | `	/* Release the IO private structure */` |
|   3803 | 7126 | `	ReleaseIOPrivate(pCtx, pDev);` |
|      - | 7127 | `	/* Invalidate the resource handle */` |
|   3803 | 7128 | `	ph7_value_release(apArg[0]);` |
|      - | 7129 | `	/* Return the exit status */` |
|   3803 | 7130 | `	ph7_result_int(pCtx, status);` |
|   3803 | 7131 | `	return PH7_OK;` |
|   1904 | 7132 | `}` |
|      - | 7133 | `/* Export the php:// stream */` |
|      - | 7134 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 7135 | `	"php",` |
|      - | 7136 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7137 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 7138 | `	0,   /* xOpenDir */` |
|      - | 7139 | `	PHPStreamData_Close, /* xClose */` |
|      - | 7140 | `	0,  /* xCloseDir */` |
|      - | 7141 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 7142 | `	0,  /* xReadDir */` |
|      - | 7143 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 7144 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|      - | 7145 | `	0,  /* xLock */` |
|      - | 7146 | `	0,  /* xRewindDir */` |
|      - | 7147 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 7148 | `	PHPStreamData_Trunc, /* xTrunc */` |
|      - | 7149 | `	0,  /* xSync */` |
|      - | 7150 | `	0   /* xStat */` |
|      - | 7151 | `};` |
|      - | 7152 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7153 | `/*` |
|      - | 7154 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 7155 | ` * FALSE otherwise.` |
|      - | 7156 | ` */` |
|    220 | 7157 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      3 | 7158 | `{` |
|      - | 7159 | `#ifndef PH7_DISABLE_DISK_IO` |
|    223 | 7160 | `	return pStream == &sPHP_Stream;` |
|      - | 7161 | `#else` |
|      - | 7162 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7163 | `	return 0;` |
|      - | 7164 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      3 | 7165 | `}` |
|      - | 7166 | `/*` |
|      - | 7167 | ` * Return TRUE if we are dealing with the data:// stream.` |
|      - | 7168 | ` */` |
|    202 | 7169 | `static int is_data_stream(const ph7_io_stream *pStream)` |
|      3 | 7170 | `{` |
|      - | 7171 | `#ifndef PH7_DISABLE_DISK_IO` |
|    205 | 7172 | `	return pStream == &sDATA_Stream;` |
|      - | 7173 | `#else` |
|      - | 7174 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7175 | `	return 0;` |
|      - | 7176 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      3 | 7177 | `}` |
|      - | 7178 | `/*` |
|      - | 7179 | ` * bool stream_isatty(resource $stream)` |
|      - | 7180 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|      - | 7181 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|      - | 7182 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|      - | 7183 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|      - | 7184 | ` */` |
|      6 | 7185 | `static int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7186 | `{` |
|      7 | 7187 | `	int bTty = 0;` |
|      7 | 7188 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7189 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7190 | `		return PH7_OK;` |
|      - | 7191 | `	}` |
|      - | 7192 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7193 | `	{` |
|      7 | 7194 | `		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      7 | 7195 | `		if( !IO_PRIVATE_INVALID(pDev) && is_php_stream(pDev->pStream) ){` |
|      5 | 7196 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|      6 | 7197 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|      4 | 7198 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|      3 | 7199 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|      - | 7200 | `#ifdef __WINNT__` |
|      1 | 7201 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|      - | 7202 | `#else` |
|      4 | 7203 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|      - | 7204 | `#endif` |
|      2 | 7205 | `			}` |
|      2 | 7206 | `		}` |
|      - | 7207 | `	}` |
|      - | 7208 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      7 | 7209 | `	ph7_result_bool(pCtx,bTty);` |
|      7 | 7210 | `	return PH7_OK;` |
|      4 | 7211 | `}` |
|      - | 7212 |  |
|      - | 7213 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7214 | `/*` |
|      - | 7215 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 7216 | ` * [i.e: file://,php://].` |
|      - | 7217 | ` * Note:` |
|      - | 7218 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 7219 | ` *  defined then this function is a no-op.` |
|      - | 7220 | ` */` |
|   3368 | 7221 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 7222 | `{` |
|      - | 7223 | `	/*` |
|      - | 7224 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 7225 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 7226 | `	 */` |
|      - | 7227 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7228 | `	/* VFS: disk I/O related functions */` |
|      - | 7229 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 7230 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 7231 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 7232 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 7233 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 7234 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 7235 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 7236 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 7237 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 7238 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 7239 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 7240 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 7241 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 7242 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 7243 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 7244 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 7245 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 7246 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 7247 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 7248 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 7249 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 7250 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 7251 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 7252 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 7253 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 7254 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 7255 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 7256 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 7257 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 7258 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 7259 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 7260 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 7261 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 7262 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 7263 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 7264 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 7265 | `		{"link",        PH7_vfs_link     },` |
|      - | 7266 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 7267 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 7268 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 7269 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 7270 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 7271 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 7272 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 7273 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 7274 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 7275 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 7276 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 7277 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 7278 | `	};` |
|      - | 7279 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 7280 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 7281 | `	 */` |
|      - | 7282 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 7283 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 7284 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 7285 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 7286 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 7287 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 7288 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 7289 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 7290 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 7291 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 7292 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 7293 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 7294 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 7295 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 7296 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 7297 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 7298 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 7299 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 7300 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 7301 | `		{"file",      PH7_builtin_file   },` |
|      - | 7302 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 7303 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 7304 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 7305 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 7306 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 7307 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 7308 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 7309 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 7310 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 7311 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 7312 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 7313 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 7314 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 7315 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 7316 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 7317 | `#ifdef PH7_ENABLE_NET` |
|      - | 7318 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 7319 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 7320 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 7321 | `#endif` |
|      - | 7322 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 7323 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 7324 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 7325 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 7326 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 7327 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 7328 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 7329 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 7330 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 7331 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7332 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 7333 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 7334 | `	};` |
|   3373 | 7335 | `	const ph7_io_stream *pFileStream = 0;` |
|   3373 | 7336 | `	sxu32 n = 0;` |
|      - | 7337 | `	/* Register disk-related functions */` |
| 165037 | 7338 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 161669 | 7339 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  80837 | 7340 | `	}` |
| 161669 | 7341 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 158301 | 7342 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  79153 | 7343 | `	}` |
|      - | 7344 | `#else` |
|      - | 7345 | `	SXUNUSED(pVm);` |
|      - | 7346 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7347 |  |
|      - | 7348 | `	/*` |
|      - | 7349 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7350 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 7351 | `	 */` |
|      - | 7352 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7353 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 7354 | `		/* Path processing */` |
|      - | 7355 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 7356 | `		{"basename",    PH7_builtin_basename },` |
|      - | 7357 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 7358 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 7359 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 7360 | `		/* ZIP processing */` |
|      - | 7361 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 7362 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 7363 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 7364 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 7365 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 7366 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 7367 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 7368 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 7369 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 7370 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 7371 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 7372 | `	};` |
|  57261 | 7373 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  53893 | 7374 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  26949 | 7375 | `	}` |
|      - | 7376 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7377 |  |
|      - | 7378 | `	/* Install streams if disk I/O is enabled */` |
|      - | 7379 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7380 | `#ifdef __WINNT__` |
|      5 | 7381 | `	pFileStream = &sWinFileStream;` |
|      - | 7382 | `#elif defined(__UNIXES__)` |
|   3368 | 7383 | `	pFileStream = &sUnixFileStream;` |
|      - | 7384 | `#endif` |
|      - | 7385 | `	/* Install the php:// stream */` |
|   3373 | 7386 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3373 | 7387 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 7388 | `#ifdef PH7_ENABLE_NET` |
|   3373 | 7389 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 7390 | `#endif` |
|   3373 | 7391 | `	if( pFileStream ){` |
|      - | 7392 | `		/* Install the file:// stream */` |
|   3373 | 7393 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1684 | 7394 | `	}` |
|      - | 7395 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7396 |  |
|   3373 | 7397 | `	return SXRET_OK;` |
|      5 | 7398 | `}` |
|      - | 7399 | `/*` |
|      - | 7400 | ` * Export the STDIN handle.` |
|      - | 7401 | ` */` |
|      2 | 7402 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 7403 | `{` |
|      - | 7404 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7405 | `	if( pVm->pStdin == 0  ){` |
|      - | 7406 | `		io_private *pIn;` |
|      - | 7407 | `		/* Allocate an IO private instance */` |
|      3 | 7408 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7409 | `		if( pIn == 0 ){` |
|    ! 0 | 7410 | `			return 0;` |
|      - | 7411 | `		}` |
|      3 | 7412 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 7413 | `		/* Initialize the handle */` |
|      3 | 7414 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 7415 | `		/* Install the STDIN stream */` |
|      3 | 7416 | `		pVm->pStdin = pIn;` |
|      3 | 7417 | `		return pIn;` |
|    ! 0 | 7418 | `	}else{` |
|      - | 7419 | `		/* NULL or STDIN */` |
|    ! 0 | 7420 | `		return pVm->pStdin;` |
|      - | 7421 | `	}` |
|      - | 7422 | `#else` |
|      - | 7423 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7424 | `	return 0;` |
|      - | 7425 | `#endif` |
|      2 | 7426 | `}` |
|      - | 7427 | `/*` |
|      - | 7428 | ` * Export the STDOUT handle.` |
|      - | 7429 | ` */` |
|      4 | 7430 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 7431 | `{` |
|      - | 7432 | `#ifndef PH7_DISABLE_DISK_IO` |
|      5 | 7433 | `	if( pVm->pStdout == 0  ){` |
|      - | 7434 | `		io_private *pOut;` |
|      - | 7435 | `		/* Allocate an IO private instance */` |
|      3 | 7436 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7437 | `		if( pOut == 0 ){` |
|    ! 0 | 7438 | `			return 0;` |
|      - | 7439 | `		}` |
|      3 | 7440 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 7441 | `		/* Initialize the handle */` |
|      3 | 7442 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 7443 | `		/* Install the STDOUT stream */` |
|      3 | 7444 | `		pVm->pStdout = pOut;` |
|      3 | 7445 | `		return pOut;` |
|    ! 0 | 7446 | `	}else{` |
|      - | 7447 | `		/* NULL or STDOUT */` |
|      3 | 7448 | `		return pVm->pStdout;` |
|      - | 7449 | `	}` |
|      - | 7450 | `#else` |
|      - | 7451 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7452 | `	return 0;` |
|      - | 7453 | `#endif` |
|      3 | 7454 | `}` |
|      - | 7455 | `/*` |
|      - | 7456 | ` * Export the STDERR handle.` |
|      - | 7457 | ` */` |
|      4 | 7458 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 7459 | `{` |
|      - | 7460 | `#ifndef PH7_DISABLE_DISK_IO` |
|      5 | 7461 | `	if( pVm->pStderr == 0  ){` |
|      - | 7462 | `		io_private *pErr;` |
|      - | 7463 | `		/* Allocate an IO private instance */` |
|      3 | 7464 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7465 | `		if( pErr == 0 ){` |
|    ! 0 | 7466 | `			return 0;` |
|      - | 7467 | `		}` |
|      3 | 7468 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 7469 | `		/* Initialize the handle */` |
|      3 | 7470 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 7471 | `		/* Install the STDERR stream */` |
|      3 | 7472 | `		pVm->pStderr = pErr;` |
|      3 | 7473 | `		return pErr;` |
|    ! 0 | 7474 | `	}else{` |
|      - | 7475 | `		/* NULL or STDERR */` |
|      3 | 7476 | `		return pVm->pStderr;` |
|      - | 7477 | `	}` |
|      - | 7478 | `#else` |
|      - | 7479 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7480 | `	return 0;` |
|      - | 7481 | `#endif` |
|      3 | 7482 | `}` |
|      - | 7483 |  |
