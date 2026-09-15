# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 811/1187 lines (68.32%)

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
|     88 |   24 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      5 |   25 | `{` |
|      - |   26 | `	/* php_dirname: strip any trailing separators, cut at the last remaining one,` |
|      - |   27 | `	 * then strip trailing separators off the parent too. The previous version` |
|      - |   28 | `	 * scanned back to the first separator and stopped, so it never coped with a` |
|      - |   29 | `	 * trailing separator or a run of them: dirname("/a/") answered "/a" instead` |
|      - |   30 | `	 * of "/", dirname("a//b") answered "a/", and dirname("///") answered "//".` |
|      - |   31 | `	 * It also answered "." for the empty string, where php answers "". */` |
|      - |   32 | `	int c,d,iEnd,i;` |
|      - |   33 | `#ifdef __WINNT__` |
|      5 |   34 | `	const char *zRoot = "\\";` |
|      - |   35 | `#else` |
|     88 |   36 | `	const char *zRoot = "/";` |
|      - |   37 | `#endif` |
|     93 |   38 | `	c = d = '/';` |
|      - |   39 | `#ifdef __WINNT__` |
|      5 |   40 | `	d = '\\';` |
|      - |   41 | `#endif` |
|      - |   42 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|     93 |   43 | `	if( nByte < 1 ){` |
|      - |   44 | `		/* php returns the empty string for the empty path */` |
|    ! 0 |   45 | `		*pLen = 0;` |
|    ! 0 |   46 | `		return "";` |
|      - |   47 | `	}` |
|     93 |   48 | `	iEnd = nByte;` |
|    151 |   49 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     15 |   50 | `		iEnd--;` |
|      1 |   51 | `	}` |
|     93 |   52 | `	if( iEnd == 0 ){` |
|      - |   53 | `		/* The path is nothing but separators: the root is its own parent */` |
|      7 |   54 | `		*pLen = (int)sizeof(char);` |
|      7 |   55 | `		return zRoot;` |
|      - |   56 | `	}` |
|      - |   57 | `	/* Walk back to the separator that ends the parent directory */` |
|     87 |   58 | `	i = iEnd;` |
|   1731 |   59 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
|   1649 |   60 | `		i--;` |
|      5 |   61 | `	}` |
|     87 |   62 | `	if( i == 0 ){` |
|      - |   63 | `		/* No separator at all,return "." as the current directory */` |
|     10 |   64 | `		*pLen = (int)sizeof(char);` |
|     10 |   65 | `		return ".";` |
|      - |   66 | `	}` |
|      - |   67 | `	/* Drop the separator, plus any that repeat before it */` |
|    186 |   68 | `	while( i > 1 && DIR_IS_SEP(zPath[i - 1]) ){` |
|     75 |   69 | `		i--;` |
|      5 |   70 | `	}` |
|     79 |   71 | `	if( i == 1 && DIR_IS_SEP(zPath[0]) ){` |
|      7 |   72 | `		*pLen = (int)sizeof(char);` |
|      7 |   73 | `		return zRoot;` |
|      - |   74 | `	}` |
|     73 |   75 | `	*pLen = i;` |
|     73 |   76 | `	return zPath;` |
|      - |   77 | `#undef DIR_IS_SEP` |
|     49 |   78 | `}` |
|      - |   79 | `/*` |
|      - |   80 | ` * Compile the VFS implementations when builtins are enabled OR when disk I/O` |
|      - |   81 | ` * is explicitly enabled (i.e. PH7_DISABLE_DISK_IO is NOT defined).` |
|      - |   82 | ` */` |
|      - |   83 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |   84 | `/*` |
|      - |   85 | ` * strerror() trips MSVC's C4996 "may be unsafe" deprecation under /WX. It is a` |
|      - |   86 | ` * standard C function we use deliberately to mirror php's IO error text; wrap it` |
|      - |   87 | ` * once with the deprecation suppressed. The pragma is _MSC_VER-guarded so the` |
|      - |   88 | ` * GCC/-Werror Linux build never sees an unknown-pragma warning.` |
|      - |   89 | ` */` |
|  19704 |   90 | `PH7_PRIVATE const char * VfsStrerror(int iErr)` |
|      5 |   91 | `{` |
|      - |   92 | `#if defined(_MSC_VER)` |
|      - |   93 | `#pragma warning(push)` |
|      - |   94 | `#pragma warning(disable:4996)` |
|      - |   95 | `#endif` |
|  19709 |   96 | `	return strerror(iErr);` |
|      - |   97 | `#if defined(_MSC_VER)` |
|      - |   98 | `#pragma warning(pop)` |
|      - |   99 | `#endif` |
|      5 |  100 | `}` |
|      - |  101 | `/*` |
|      - |  102 | ` * php's non-open IO failures: "unlink(/nope): No such file or directory".` |
|      - |  103 | ` * PH7 returned FALSE in SILENCE for unlink/rmdir/mkdir/rename/chdir/opendir/scandir and` |
|      - |  104 | ` * filesize, so a script could not tell a failed operation from a successful one without` |
|      - |  105 | ` * checking the return value it never got told to check.` |
|      - |  106 | ` */` |
|  19682 |  107 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |  108 | `{` |
|  29528 |  109 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  19682 |  110 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  19687 |  111 | `}` |
|     12 |  112 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      3 |  113 | `{` |
|     21 |  114 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open stream: %s",` |
|     12 |  115 | `		ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));` |
|     15 |  116 | `}` |
|      - |  117 | `/*` |
|      - |  118 | ` * bool chdir(string $directory)` |
|      - |  119 | ` *  Change the current directory.` |
|      - |  120 | ` * Parameters` |
|      - |  121 | ` *  $directory` |
|      - |  122 | ` *   The new current directory` |
|      - |  123 | ` * Return` |
|      - |  124 | ` *  TRUE on success or FALSE on failure.` |
|      - |  125 | ` */` |
|  12888 |  126 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  127 | `{` |
|      - |  128 | `	const char *zPath;` |
|      - |  129 | `	ph7_vfs *pVfs;` |
|      - |  130 | `	int rc;` |
|  12893 |  131 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  132 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |  133 | `		ph7_result_bool(pCtx,0);` |
|      6 |  134 | `		return PH7_OK;` |
|      - |  135 | `	}` |
|      - |  136 | `	/* Point to the underlying vfs */` |
|  12889 |  137 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  12889 |  138 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  139 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  140 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  141 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  142 | `			ph7_function_name(pCtx)` |
|      - |  143 | `			);` |
|    ! 0 |  144 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  145 | `		return PH7_OK;` |
|      - |  146 | `	}` |
|      - |  147 | `	/* Point to the desired directory */` |
|  12889 |  148 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  149 | `	/* Perform the requested operation */` |
|  12889 |  150 | `	errno = 0;` |
|  12889 |  151 | `	rc = pVfs->xChdir(zPath);` |
|  12889 |  152 | `	if( rc != PH7_OK ){` |
|      - |  153 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      4 |  154 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      2 |  155 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      1 |  156 | `	}` |
|      - |  157 | `	/* IO return value */` |
|  12889 |  158 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  12889 |  159 | `	return PH7_OK;` |
|   6449 |  160 | `}` |
|      - |  161 | `/*` |
|      - |  162 | ` * bool chroot(string $directory)` |
|      - |  163 | ` *  Change the root directory.` |
|      - |  164 | ` * Parameters` |
|      - |  165 | ` *  $directory` |
|      - |  166 | ` *   The path to change the root directory to` |
|      - |  167 | ` * Return` |
|      - |  168 | ` *  TRUE on success or FALSE on failure.` |
|      - |  169 | ` */` |
|      6 |  170 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  171 | `{` |
|      - |  172 | `	const char *zPath;` |
|      - |  173 | `	ph7_vfs *pVfs;` |
|      - |  174 | `	int rc;` |
|      7 |  175 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  176 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 |  177 | `		ph7_result_bool(pCtx,0);` |
|      5 |  178 | `		return PH7_OK;` |
|      - |  179 | `	}` |
|      - |  180 | `	/* Point to the underlying vfs */` |
|      3 |  181 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  182 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  183 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  184 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  185 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  186 | `			ph7_function_name(pCtx)` |
|      - |  187 | `			);` |
|    ! 0 |  188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  189 | `		return PH7_OK;` |
|      - |  190 | `	}` |
|      - |  191 | `	/* Point to the desired directory */` |
|      3 |  192 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  193 | `	/* Perform the requested operation */` |
|      3 |  194 | `	rc = pVfs->xChroot(zPath);` |
|      - |  195 | `	/* IO return value */` |
|      3 |  196 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 |  197 | `	return PH7_OK;` |
|      4 |  198 | `}` |
|      - |  199 | `/*` |
|      - |  200 | ` * string getcwd(void)` |
|      - |  201 | ` *  Gets the current working directory.` |
|      - |  202 | ` * Parameters` |
|      - |  203 | ` *  None` |
|      - |  204 | ` * Return` |
|      - |  205 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  206 | ` */` |
|     18 |  207 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  208 | `{` |
|      - |  209 | `	ph7_vfs *pVfs;` |
|      - |  210 | `	int rc;` |
|      - |  211 | `	/* Point to the underlying vfs */` |
|     23 |  212 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     23 |  213 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  214 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  215 | `		SXUNUSED(apArg);` |
|      - |  216 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  217 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  218 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  219 | `			ph7_function_name(pCtx)` |
|      - |  220 | `			);` |
|    ! 0 |  221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  222 | `		return PH7_OK;` |
|      - |  223 | `	}` |
|     23 |  224 | `	ph7_result_string(pCtx,"",0);` |
|      - |  225 | `	/* Perform the requested operation */` |
|     23 |  226 | `	rc = pVfs->xGetcwd(pCtx);` |
|     23 |  227 | `	if( rc != PH7_OK ){` |
|      - |  228 | `		/* Error,return FALSE */` |
|    ! 0 |  229 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  230 | `	}` |
|     23 |  231 | `	return PH7_OK;` |
|     14 |  232 | `}` |
|      - |  233 | `/*` |
|      - |  234 | ` * bool rmdir(string $directory)` |
|      - |  235 | ` *  Removes directory.` |
|      - |  236 | ` * Parameters` |
|      - |  237 | ` *  $directory` |
|      - |  238 | ` *   The path to the directory` |
|      - |  239 | ` * Return` |
|      - |  240 | ` *  TRUE on success or FALSE on failure.` |
|      - |  241 | ` */` |
|     46 |  242 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  243 | `{` |
|      - |  244 | `	const char *zPath;` |
|      - |  245 | `	ph7_vfs *pVfs;` |
|      - |  246 | `	int rc;` |
|     49 |  247 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  248 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  249 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  250 | `		return PH7_OK;` |
|      - |  251 | `	}` |
|      - |  252 | `	/* Point to the underlying vfs */` |
|     49 |  253 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     49 |  254 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  255 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  256 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  257 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  258 | `			ph7_function_name(pCtx)` |
|      - |  259 | `			);` |
|    ! 0 |  260 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  261 | `		return PH7_OK;` |
|      - |  262 | `	}` |
|      - |  263 | `	/* Point to the desired directory */` |
|     49 |  264 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  265 | `	/* Perform the requested operation */` |
|     49 |  266 | `	errno = 0;` |
|     49 |  267 | `	rc = pVfs->xRmdir(zPath);` |
|     49 |  268 | `	if( rc != PH7_OK ){` |
|      8 |  269 | `		VfsThrowSysWarning(pCtx,zPath);` |
|      3 |  270 | `	}` |
|      - |  271 | `	/* IO return value */` |
|     49 |  272 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     49 |  273 | `	return PH7_OK;` |
|     26 |  274 | `}` |
|      - |  275 | `/*` |
|      - |  276 | ` * bool is_dir(string $filename)` |
|      - |  277 | ` *  Tells whether the given filename is a directory.` |
|      - |  278 | ` * Parameters` |
|      - |  279 | ` *  $filename` |
|      - |  280 | ` *   Path to the file.` |
|      - |  281 | ` * Return` |
|      - |  282 | ` *  TRUE on success or FALSE on failure.` |
|      - |  283 | ` */` |
|   8656 |  284 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  285 | `{` |
|      - |  286 | `	const char *zPath;` |
|      - |  287 | `	ph7_vfs *pVfs;` |
|      - |  288 | `	int rc;` |
|   8661 |  289 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  290 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  291 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  292 | `		return PH7_OK;` |
|      - |  293 | `	}` |
|      - |  294 | `	/* Point to the underlying vfs */` |
|   8661 |  295 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8661 |  296 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  297 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  298 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  299 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  300 | `			ph7_function_name(pCtx)` |
|      - |  301 | `			);` |
|    ! 0 |  302 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  303 | `		return PH7_OK;` |
|      - |  304 | `	}` |
|      - |  305 | `	/* Point to the desired directory */` |
|   8661 |  306 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  307 | `	/* Perform the requested operation */` |
|   8661 |  308 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  309 | `	/* IO return value */` |
|   8661 |  310 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8661 |  311 | `	return PH7_OK;` |
|   4333 |  312 | `}` |
|      - |  313 | `/*` |
|      - |  314 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  315 | ` *  Make a directory.` |
|      - |  316 | ` * Parameters` |
|      - |  317 | ` *  $pathname` |
|      - |  318 | ` *   The directory path.` |
|      - |  319 | ` * $mode` |
|      - |  320 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  321 | ` *  Note:` |
|      - |  322 | ` *   mode is ignored on Windows.` |
|      - |  323 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  324 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  325 | ` *   which you can change using umask().` |
|      - |  326 | ` * $recursive` |
|      - |  327 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  328 | ` *  Defaults to FALSE. (Not used)` |
|      - |  329 | ` * Return` |
|      - |  330 | ` *  TRUE on success or FALSE on failure.` |
|      - |  331 | ` */` |
|     46 |  332 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  333 | `{` |
|     49 |  334 | `	int iRecursive = 0;` |
|      - |  335 | `	const char *zPath;` |
|      - |  336 | `	ph7_vfs *pVfs;` |
|      - |  337 | `	int iMode,rc;` |
|     49 |  338 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  339 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  340 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  341 | `		return PH7_OK;` |
|      - |  342 | `	}` |
|      - |  343 | `	/* Point to the underlying vfs */` |
|     49 |  344 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     49 |  345 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  346 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  347 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  348 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  349 | `			ph7_function_name(pCtx)` |
|      - |  350 | `			);` |
|    ! 0 |  351 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  352 | `		return PH7_OK;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Point to the desired directory */` |
|     49 |  355 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  356 | `#ifdef __WINNT__` |
|      3 |  357 | `	iMode = 0;` |
|      - |  358 | `#else` |
|      - |  359 | `	/* Assume UNIX */` |
|     46 |  360 | `	iMode = 0777;` |
|      - |  361 | `#endif` |
|     49 |  362 | `	if( nArg > 1 ){` |
|    ! 0 |  363 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  364 | `		if( nArg > 2 ){` |
|    ! 0 |  365 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  366 | `		}` |
|    ! 0 |  367 | `	}` |
|      - |  368 | `	/* Perform the requested operation */` |
|     49 |  369 | `	errno = 0;` |
|     49 |  370 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|     49 |  371 | `	if( rc != PH7_OK ){` |
|      - |  372 | `		/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|    ! 0 |  373 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|    ! 0 |  374 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|    ! 0 |  375 | `	}` |
|      - |  376 | `	/* IO return value */` |
|     49 |  377 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     49 |  378 | `	return PH7_OK;` |
|     26 |  379 | `}` |
|      - |  380 | `/*` |
|      - |  381 | ` * bool rename(string $oldname,string $newname)` |
|      - |  382 | ` *  Attempts to rename oldname to newname.` |
|      - |  383 | ` * Parameters` |
|      - |  384 | ` *  $oldname` |
|      - |  385 | ` *   Old name.` |
|      - |  386 | ` *  $newname` |
|      - |  387 | ` *   New name.` |
|      - |  388 | ` * Return` |
|      - |  389 | ` *  TRUE on success or FALSE on failure.` |
|      - |  390 | ` */` |
|      2 |  391 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  392 | `{` |
|      - |  393 | `	const char *zOld,*zNew;` |
|      - |  394 | `	ph7_vfs *pVfs;` |
|      - |  395 | `	int rc;` |
|      3 |  396 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  397 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  398 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  399 | `		return PH7_OK;` |
|      - |  400 | `	}` |
|      - |  401 | `	/* Point to the underlying vfs */` |
|      3 |  402 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  403 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  404 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  405 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  406 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  407 | `			ph7_function_name(pCtx)` |
|      - |  408 | `			);` |
|    ! 0 |  409 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  410 | `		return PH7_OK;` |
|      - |  411 | `	}` |
|      - |  412 | `	/* Perform the requested operation */` |
|      3 |  413 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  414 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  415 | `	errno = 0;` |
|      3 |  416 | `	rc = pVfs->xRename(zOld,zNew);` |
|      3 |  417 | `	if( rc != PH7_OK ){` |
|      - |  418 | `		/* php names BOTH paths here */` |
|    ! 0 |  419 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    ! 0 |  420 | `			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));` |
|    ! 0 |  421 | `	}` |
|      - |  422 | `	/* IO result */` |
|      3 |  423 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  424 | `	return PH7_OK;` |
|      2 |  425 | `}` |
|      - |  426 | `/*` |
|      - |  427 | ` * string realpath(string $path)` |
|      - |  428 | ` *  Returns canonicalized absolute pathname.` |
|      - |  429 | ` * Parameters` |
|      - |  430 | ` *  $path` |
|      - |  431 | ` *   Target path.` |
|      - |  432 | ` * Return` |
|      - |  433 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  434 | ` */` |
|      6 |  435 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  436 | `{` |
|      - |  437 | `	const char *zPath;` |
|      - |  438 | `	ph7_vfs *pVfs;` |
|      - |  439 | `        int rc;` |
|      8 |  440 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  441 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  442 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  443 | `		return PH7_OK;` |
|      - |  444 | `	}` |
|      - |  445 | `	/* Point to the underlying vfs */` |
|      8 |  446 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 |  447 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  448 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  449 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  450 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  451 | `			ph7_function_name(pCtx)` |
|      - |  452 | `			);` |
|    ! 0 |  453 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  454 | `		return PH7_OK;` |
|      - |  455 | `	}` |
|      - |  456 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|      8 |  457 | `	ph7_result_string(pCtx,"",0);` |
|      - |  458 | `	/* Perform the requested operation */` |
|      8 |  459 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      8 |  460 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|      8 |  461 | `	if( rc != PH7_OK ){` |
|      2 |  462 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  463 | `	}` |
|      8 |  464 | `	return PH7_OK;` |
|      5 |  465 | `}` |
|      - |  466 | `/*` |
|      - |  467 | ` * int sleep(int $seconds)` |
|      - |  468 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  469 | ` * Parameters` |
|      - |  470 | ` *  $seconds` |
|      - |  471 | ` *   Halt time in seconds.` |
|      - |  472 | ` * Return` |
|      - |  473 | ` *  Zero on success or FALSE on failure.` |
|      - |  474 | ` */` |
|     10 |  475 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  476 | `{` |
|      - |  477 | `	ph7_vfs *pVfs;` |
|      - |  478 | `	int rc,nSleep;` |
|     11 |  479 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  480 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  481 | `		ph7_result_bool(pCtx,0);` |
|      3 |  482 | `		return PH7_OK;` |
|      - |  483 | `	}` |
|      - |  484 | `	/* Point to the underlying vfs */` |
|      9 |  485 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 |  486 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  487 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  488 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  489 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  490 | `			ph7_function_name(pCtx)` |
|      - |  491 | `			);` |
|    ! 0 |  492 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  493 | `		return PH7_OK;` |
|      - |  494 | `	}` |
|      - |  495 | `	/* Amount to sleep */` |
|      9 |  496 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|      9 |  497 | `	if( nSleep < 0 ){` |
|      - |  498 | `		/* Invalid value,return FALSE */` |
|      3 |  499 | `		ph7_result_bool(pCtx,0);` |
|      3 |  500 | `		return PH7_OK;` |
|      - |  501 | `	}` |
|      - |  502 | `	/* Perform the requested operation (Microseconds) */` |
|      7 |  503 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      7 |  504 | `	if( rc != PH7_OK ){` |
|      - |  505 | `		/* Return FALSE */` |
|    ! 0 |  506 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  507 | `	}else{` |
|      - |  508 | `		/* Return zero */` |
|      7 |  509 | `		ph7_result_int(pCtx,0);` |
|      - |  510 | `	}` |
|      7 |  511 | `	return PH7_OK;` |
|      6 |  512 | `}` |
|      - |  513 | `/*` |
|      - |  514 | ` * void usleep(int $micro_seconds)` |
|      - |  515 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  516 | ` * Parameters` |
|      - |  517 | ` *  $micro_seconds` |
|      - |  518 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  519 | ` * Return` |
|      - |  520 | ` *  None.` |
|      - |  521 | ` */` |
|     56 |  522 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  523 | `{` |
|      - |  524 | `	ph7_vfs *pVfs;` |
|      - |  525 | `	int nSleep;` |
|     57 |  526 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  527 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  528 | `		return PH7_OK;` |
|      - |  529 | `	}` |
|      - |  530 | `	/* Point to the underlying vfs */` |
|     57 |  531 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     57 |  532 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  533 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  534 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  535 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  536 | `			ph7_function_name(pCtx)` |
|      - |  537 | `			);` |
|    ! 0 |  538 | `		return PH7_OK;` |
|      - |  539 | `	}` |
|      - |  540 | `	/* Amount to sleep */` |
|     57 |  541 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     57 |  542 | `	if( nSleep < 0 ){` |
|      - |  543 | `		/* Invalid value,return immediately */` |
|      3 |  544 | `		return PH7_OK;` |
|      - |  545 | `	}` |
|      - |  546 | `	/* Perform the requested operation (Microseconds) */` |
|     55 |  547 | `	pVfs->xSleep((unsigned int)nSleep);` |
|     55 |  548 | `	return PH7_OK;` |
|     29 |  549 | `}` |
|      - |  550 | `/*` |
|      - |  551 | ` * bool unlink (string $filename)` |
|      - |  552 | ` *  Delete a file.` |
|      - |  553 | ` * Parameters` |
|      - |  554 | ` *  $filename` |
|      - |  555 | ` *   Path to the file.` |
|      - |  556 | ` * Return` |
|      - |  557 | ` *  TRUE on success or FALSE on failure.` |
|      - |  558 | ` */` |
|  32818 |  559 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  560 | `{` |
|      - |  561 | `	const char *zPath;` |
|      - |  562 | `	ph7_vfs *pVfs;` |
|      - |  563 | `	int rc;` |
|  32823 |  564 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  565 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  566 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  567 | `		return PH7_OK;` |
|      - |  568 | `	}` |
|      - |  569 | `	/* Point to the underlying vfs */` |
|  32823 |  570 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  32823 |  571 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  572 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  573 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  574 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  575 | `			ph7_function_name(pCtx)` |
|      - |  576 | `			);` |
|    ! 0 |  577 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  578 | `		return PH7_OK;` |
|      - |  579 | `	}` |
|      - |  580 | `	/* Point to the desired directory */` |
|  32823 |  581 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  582 | `	/* Perform the requested operation */` |
|  32823 |  583 | `	errno = 0;` |
|  32823 |  584 | `	rc = pVfs->xUnlink(zPath);` |
|  32823 |  585 | `	if( rc != PH7_OK ){` |
|  19681 |  586 | `		VfsThrowSysWarning(pCtx,zPath);` |
|   9838 |  587 | `	}` |
|      - |  588 | `	/* IO return value */` |
|  32823 |  589 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  32823 |  590 | `	return PH7_OK;` |
|  16414 |  591 | `}` |
|      - |  592 | `/*` |
|      - |  593 | ` * bool chmod(string $filename,int $mode)` |
|      - |  594 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - |  595 | ` * Parameters` |
|      - |  596 | ` *  $filename` |
|      - |  597 | ` *   Path to the file.` |
|      - |  598 | ` * $mode` |
|      - |  599 | ` *   Mode (Must be an integer)` |
|      - |  600 | ` * Return` |
|      - |  601 | ` *  TRUE on success or FALSE on failure.` |
|      - |  602 | ` */` |
|    140 |  603 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  604 | `{` |
|      - |  605 | `	const char *zPath;` |
|      - |  606 | `	ph7_vfs *pVfs;` |
|      - |  607 | `	int iMode;` |
|      - |  608 | `	int rc;` |
|    142 |  609 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  610 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  611 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  612 | `		return PH7_OK;` |
|      - |  613 | `	}` |
|      - |  614 | `	/* Point to the underlying vfs */` |
|    142 |  615 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    142 |  616 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  617 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  618 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  619 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  620 | `			ph7_function_name(pCtx)` |
|      - |  621 | `			);` |
|    ! 0 |  622 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  623 | `		return PH7_OK;` |
|      - |  624 | `	}` |
|      - |  625 | `	/* Point to the desired directory */` |
|    142 |  626 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  627 | `	/* Extract the mode */` |
|    142 |  628 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  629 | `	/* Perform the requested operation */` |
|    142 |  630 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  631 | `	/* IO return value */` |
|    142 |  632 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    142 |  633 | `	return PH7_OK;` |
|     72 |  634 | `}` |
|      - |  635 | `/*` |
|      - |  636 | ` * bool chown(string $filename,string $user)` |
|      - |  637 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - |  638 | ` * Parameters` |
|      - |  639 | ` *  $filename` |
|      - |  640 | ` *   Path to the file.` |
|      - |  641 | ` * $user` |
|      - |  642 | ` *   Username.` |
|      - |  643 | ` * Return` |
|      - |  644 | ` *  TRUE on success or FALSE on failure.` |
|      - |  645 | ` */` |
|      6 |  646 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  647 | `{` |
|      - |  648 | `	const char *zPath,*zUser;` |
|      - |  649 | `	ph7_vfs *pVfs;` |
|      - |  650 | `	int rc;` |
|      7 |  651 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  652 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  653 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  654 | `		return PH7_OK;` |
|      - |  655 | `	}` |
|      - |  656 | `	/* Point to the underlying vfs */` |
|      7 |  657 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  658 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - |  659 | `		/* IO routine not implemented,return NULL */` |
|      1 |  660 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  661 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  662 | `			ph7_function_name(pCtx)` |
|      - |  663 | `			);` |
|      1 |  664 | `		ph7_result_bool(pCtx,0);` |
|      1 |  665 | `		return PH7_OK;` |
|      - |  666 | `	}` |
|      - |  667 | `	/* Point to the desired directory */` |
|      6 |  668 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  669 | `	/* Extract the user */` |
|      6 |  670 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - |  671 | `	/* Perform the requested operation */` |
|      6 |  672 | `	errno = 0;` |
|      6 |  673 | `	rc = pVfs->xChown(zPath,zUser);` |
|      6 |  674 | `	if( rc != PH7_OK ){` |
|      - |  675 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  676 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  677 | `		 * "chown(): Operation not permitted". */` |
|      6 |  678 | `		if( rc == -2 ){` |
|      3 |  679 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",` |
|      1 |  680 | `				ph7_function_name(pCtx),zUser);` |
|      1 |  681 | `		}else{` |
|      6 |  682 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  683 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  684 | `		}` |
|      3 |  685 | `	}` |
|      - |  686 | `	/* IO return value */` |
|      6 |  687 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  688 | `	return PH7_OK;` |
|      4 |  689 | `}` |
|      - |  690 | `/*` |
|      - |  691 | ` * bool chgrp(string $filename,string $group)` |
|      - |  692 | ` *  Attempts to change the group of the file filename to group.` |
|      - |  693 | ` * Parameters` |
|      - |  694 | ` *  $filename` |
|      - |  695 | ` *   Path to the file.` |
|      - |  696 | ` * $group` |
|      - |  697 | ` *   groupname.` |
|      - |  698 | ` * Return` |
|      - |  699 | ` *  TRUE on success or FALSE on failure.` |
|      - |  700 | ` */` |
|      6 |  701 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  702 | `{` |
|      - |  703 | `	const char *zPath,*zGroup;` |
|      - |  704 | `	ph7_vfs *pVfs;` |
|      - |  705 | `	int rc;` |
|      7 |  706 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  707 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  708 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  709 | `		return PH7_OK;` |
|      - |  710 | `	}` |
|      - |  711 | `	/* Point to the underlying vfs */` |
|      7 |  712 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  713 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - |  714 | `		/* IO routine not implemented,return NULL */` |
|      1 |  715 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  716 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  717 | `			ph7_function_name(pCtx)` |
|      - |  718 | `			);` |
|      1 |  719 | `		ph7_result_bool(pCtx,0);` |
|      1 |  720 | `		return PH7_OK;` |
|      - |  721 | `	}` |
|      - |  722 | `	/* Point to the desired directory */` |
|      6 |  723 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  724 | `	/* Extract the user */` |
|      6 |  725 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - |  726 | `	/* Perform the requested operation */` |
|      6 |  727 | `	errno = 0;` |
|      6 |  728 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      6 |  729 | `	if( rc != PH7_OK ){` |
|      - |  730 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  731 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  732 | `		 * "chown(): Operation not permitted". */` |
|      6 |  733 | `		if( rc == -2 ){` |
|      3 |  734 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",` |
|      1 |  735 | `				ph7_function_name(pCtx),zGroup);` |
|      1 |  736 | `		}else{` |
|      6 |  737 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  738 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  739 | `		}` |
|      3 |  740 | `	}` |
|      - |  741 | `	/* IO return value */` |
|      6 |  742 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  743 | `	return PH7_OK;` |
|      4 |  744 | `}` |
|      - |  745 | `/*` |
|      - |  746 | ` * int64 disk_free_space(string $directory)` |
|      - |  747 | ` *  Returns available space on filesystem or disk partition.` |
|      - |  748 | ` * Parameters` |
|      - |  749 | ` *  $directory` |
|      - |  750 | ` *   A directory of the filesystem or disk partition.` |
|      - |  751 | ` * Return` |
|      - |  752 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  753 | ` */` |
|      4 |  754 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  755 | `{` |
|      - |  756 | `	const char *zPath;` |
|      - |  757 | `	ph7_int64 iSize;` |
|      - |  758 | `	ph7_vfs *pVfs;` |
|      5 |  759 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  760 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  761 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  762 | `		return PH7_OK;` |
|      - |  763 | `	}` |
|      - |  764 | `	/* Point to the underlying vfs */` |
|      5 |  765 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  766 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - |  767 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  768 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  769 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  770 | `			ph7_function_name(pCtx)` |
|      - |  771 | `			);` |
|    ! 0 |  772 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  773 | `		return PH7_OK;` |
|      - |  774 | `	}` |
|      - |  775 | `	/* Point to the desired directory */` |
|      5 |  776 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  777 | `	/* Perform the requested operation */` |
|      5 |  778 | `	iSize = pVfs->xFreeSpace(zPath);` |
|      - |  779 | `	/* IO return value */` |
|      5 |  780 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  781 | `	return PH7_OK;` |
|      3 |  782 | `}` |
|      - |  783 | `/*` |
|      - |  784 | ` * int64 disk_total_space(string $directory)` |
|      - |  785 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - |  786 | ` * Parameters` |
|      - |  787 | ` *  $directory` |
|      - |  788 | ` *   A directory of the filesystem or disk partition.` |
|      - |  789 | ` * Return` |
|      - |  790 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  791 | ` */` |
|      4 |  792 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  793 | `{` |
|      - |  794 | `	const char *zPath;` |
|      - |  795 | `	ph7_int64 iSize;` |
|      - |  796 | `	ph7_vfs *pVfs;` |
|      5 |  797 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  798 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  799 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  800 | `		return PH7_OK;` |
|      - |  801 | `	}` |
|      - |  802 | `	/* Point to the underlying vfs */` |
|      5 |  803 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  804 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - |  805 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  806 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  807 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  808 | `			ph7_function_name(pCtx)` |
|      - |  809 | `			);` |
|    ! 0 |  810 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  811 | `		return PH7_OK;` |
|      - |  812 | `	}` |
|      - |  813 | `	/* Point to the desired directory */` |
|      5 |  814 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  815 | `	/* Perform the requested operation */` |
|      5 |  816 | `	iSize = pVfs->xTotalSpace(zPath);` |
|      - |  817 | `	/* IO return value */` |
|      5 |  818 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  819 | `	return PH7_OK;` |
|      3 |  820 | `}` |
|      - |  821 | `/*` |
|      - |  822 | ` * bool file_exists(string $filename)` |
|      - |  823 | ` *  Checks whether a file or directory exists.` |
|      - |  824 | ` * Parameters` |
|      - |  825 | ` *  $filename` |
|      - |  826 | ` *   Path to the file.` |
|      - |  827 | ` * Return` |
|      - |  828 | ` *  TRUE on success or FALSE on failure.` |
|      - |  829 | ` */` |
|    160 |  830 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  831 | `{` |
|      - |  832 | `	const char *zPath;` |
|      - |  833 | `	ph7_vfs *pVfs;` |
|      - |  834 | `	int rc;` |
|    162 |  835 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  836 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  837 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  838 | `		return PH7_OK;` |
|      - |  839 | `	}` |
|      - |  840 | `	/* Point to the underlying vfs */` |
|    162 |  841 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    162 |  842 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  843 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  844 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  845 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  846 | `			ph7_function_name(pCtx)` |
|      - |  847 | `			);` |
|    ! 0 |  848 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  849 | `		return PH7_OK;` |
|      - |  850 | `	}` |
|      - |  851 | `	/* Point to the desired directory */` |
|    162 |  852 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  853 | `	/* Perform the requested operation */` |
|    162 |  854 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  855 | `	/* IO return value */` |
|    162 |  856 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    162 |  857 | `	return PH7_OK;` |
|     82 |  858 | `}` |
|      - |  859 | `/*` |
|      - |  860 | ` * int64 file_size(string $filename)` |
|      - |  861 | ` *  Gets the size for the given file.` |
|      - |  862 | ` * Parameters` |
|      - |  863 | ` *  $filename` |
|      - |  864 | ` *   Path to the file.` |
|      - |  865 | ` * Return` |
|      - |  866 | ` *  File size on success or FALSE on failure.` |
|      - |  867 | ` */` |
|     10 |  868 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  869 | `{` |
|      - |  870 | `	const char *zPath;` |
|      - |  871 | `	ph7_int64 iSize;` |
|      - |  872 | `	ph7_vfs *pVfs;` |
|     11 |  873 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  874 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  875 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  876 | `		return PH7_OK;` |
|      - |  877 | `	}` |
|      - |  878 | `	/* Point to the underlying vfs */` |
|     11 |  879 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  880 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - |  881 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  882 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  883 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  884 | `			ph7_function_name(pCtx)` |
|      - |  885 | `			);` |
|    ! 0 |  886 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  887 | `		return PH7_OK;` |
|      - |  888 | `	}` |
|      - |  889 | `	/* Point to the desired directory */` |
|     11 |  890 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  891 | `	/* Perform the requested operation */` |
|     11 |  892 | `	iSize = pVfs->xFileSize(zPath);` |
|     11 |  893 | `	if( iSize < 0 ){` |
|      - |  894 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - |  895 | `		 * truthy and compares equal to nothing a caller would test for. */` |
|    ! 0 |  896 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): stat failed for %s",` |
|    ! 0 |  897 | `			ph7_function_name(pCtx),zPath);` |
|    ! 0 |  898 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  899 | `		return PH7_OK;` |
|      - |  900 | `	}` |
|      - |  901 | `	/* IO return value */` |
|     11 |  902 | `	ph7_result_int64(pCtx,iSize);` |
|     11 |  903 | `	return PH7_OK;` |
|      6 |  904 | `}` |
|      - |  905 | `/*` |
|      - |  906 | ` * int64 fileatime(string $filename)` |
|      - |  907 | ` *  Gets the last access time of the given file.` |
|      - |  908 | ` * Parameters` |
|      - |  909 | ` *  $filename` |
|      - |  910 | ` *   Path to the file.` |
|      - |  911 | ` * Return` |
|      - |  912 | ` *  File atime on success or FALSE on failure.` |
|      - |  913 | ` */` |
|      2 |  914 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  915 | `{` |
|      - |  916 | `	const char *zPath;` |
|      - |  917 | `	ph7_int64 iTime;` |
|      - |  918 | `	ph7_vfs *pVfs;` |
|      3 |  919 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  920 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  921 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  922 | `		return PH7_OK;` |
|      - |  923 | `	}` |
|      - |  924 | `	/* Point to the underlying vfs */` |
|      3 |  925 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  926 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - |  927 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  928 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  929 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  930 | `			ph7_function_name(pCtx)` |
|      - |  931 | `			);` |
|    ! 0 |  932 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  933 | `		return PH7_OK;` |
|      - |  934 | `	}` |
|      - |  935 | `	/* Point to the desired directory */` |
|      3 |  936 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  937 | `	/* Perform the requested operation */` |
|      3 |  938 | `	iTime = pVfs->xFileAtime(zPath);` |
|      - |  939 | `	/* IO return value */` |
|      3 |  940 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  941 | `	return PH7_OK;` |
|      2 |  942 | `}` |
|      - |  943 | `/*` |
|      - |  944 | ` * int64 filemtime(string $filename)` |
|      - |  945 | ` *  Gets file modification time.` |
|      - |  946 | ` * Parameters` |
|      - |  947 | ` *  $filename` |
|      - |  948 | ` *   Path to the file.` |
|      - |  949 | ` * Return` |
|      - |  950 | ` *  File mtime on success or FALSE on failure.` |
|      - |  951 | ` */` |
|      4 |  952 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  953 | `{` |
|      - |  954 | `	const char *zPath;` |
|      - |  955 | `	ph7_int64 iTime;` |
|      - |  956 | `	ph7_vfs *pVfs;` |
|      5 |  957 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  958 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  959 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  960 | `		return PH7_OK;` |
|      - |  961 | `	}` |
|      - |  962 | `	/* Point to the underlying vfs */` |
|      5 |  963 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  964 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - |  965 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  966 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  967 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  968 | `			ph7_function_name(pCtx)` |
|      - |  969 | `			);` |
|    ! 0 |  970 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  971 | `		return PH7_OK;` |
|      - |  972 | `	}` |
|      - |  973 | `	/* Point to the desired directory */` |
|      5 |  974 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  975 | `	/* Perform the requested operation */` |
|      5 |  976 | `	iTime = pVfs->xFileMtime(zPath);` |
|      - |  977 | `	/* IO return value */` |
|      5 |  978 | `	ph7_result_int64(pCtx,iTime);` |
|      5 |  979 | `	return PH7_OK;` |
|      3 |  980 | `}` |
|      - |  981 | `/*` |
|      - |  982 | ` * int64 filectime(string $filename)` |
|      - |  983 | ` *  Gets inode change time of file.` |
|      - |  984 | ` * Parameters` |
|      - |  985 | ` *  $filename` |
|      - |  986 | ` *   Path to the file.` |
|      - |  987 | ` * Return` |
|      - |  988 | ` *  File ctime on success or FALSE on failure.` |
|      - |  989 | ` */` |
|      2 |  990 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  991 | `{` |
|      - |  992 | `	const char *zPath;` |
|      - |  993 | `	ph7_int64 iTime;` |
|      - |  994 | `	ph7_vfs *pVfs;` |
|      3 |  995 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  996 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  997 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  998 | `		return PH7_OK;` |
|      - |  999 | `	}` |
|      - | 1000 | `	/* Point to the underlying vfs */` |
|      3 | 1001 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1002 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - | 1003 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1004 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1005 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1006 | `			ph7_function_name(pCtx)` |
|      - | 1007 | `			);` |
|    ! 0 | 1008 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1009 | `		return PH7_OK;` |
|      - | 1010 | `	}` |
|      - | 1011 | `	/* Point to the desired directory */` |
|      3 | 1012 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1013 | `	/* Perform the requested operation */` |
|      3 | 1014 | `	iTime = pVfs->xFileCtime(zPath);` |
|      - | 1015 | `	/* IO return value */` |
|      3 | 1016 | `	ph7_result_int64(pCtx,iTime);` |
|      3 | 1017 | `	return PH7_OK;` |
|      2 | 1018 | `}` |
|      - | 1019 | `/*` |
|      - | 1020 | ` * bool is_file(string $filename)` |
|      - | 1021 | ` *  Tells whether the filename is a regular file.` |
|      - | 1022 | ` * Parameters` |
|      - | 1023 | ` *  $filename` |
|      - | 1024 | ` *   Path to the file.` |
|      - | 1025 | ` * Return` |
|      - | 1026 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1027 | ` */` |
|   6594 | 1028 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1029 | `{` |
|      - | 1030 | `	const char *zPath;` |
|      - | 1031 | `	ph7_vfs *pVfs;` |
|      - | 1032 | `	int rc;` |
|   6599 | 1033 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1034 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1035 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1036 | `		return PH7_OK;` |
|      - | 1037 | `	}` |
|      - | 1038 | `	/* Point to the underlying vfs */` |
|   6599 | 1039 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6599 | 1040 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1041 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1042 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1043 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1044 | `			ph7_function_name(pCtx)` |
|      - | 1045 | `			);` |
|    ! 0 | 1046 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1047 | `		return PH7_OK;` |
|      - | 1048 | `	}` |
|      - | 1049 | `	/* Point to the desired directory */` |
|   6599 | 1050 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1051 | `	/* Perform the requested operation */` |
|   6599 | 1052 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1053 | `	/* IO return value */` |
|   6599 | 1054 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6599 | 1055 | `	return PH7_OK;` |
|   3302 | 1056 | `}` |
|      - | 1057 | `/*` |
|      - | 1058 | ` * bool is_link(string $filename)` |
|      - | 1059 | ` *  Tells whether the filename is a symbolic link.` |
|      - | 1060 | ` * Parameters` |
|      - | 1061 | ` *  $filename` |
|      - | 1062 | ` *   Path to the file.` |
|      - | 1063 | ` * Return` |
|      - | 1064 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1065 | ` */` |
|      4 | 1066 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1067 | `{` |
|      - | 1068 | `	const char *zPath;` |
|      - | 1069 | `	ph7_vfs *pVfs;` |
|      - | 1070 | `	int rc;` |
|      4 | 1071 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1072 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1073 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1074 | `		return PH7_OK;` |
|      - | 1075 | `	}` |
|      - | 1076 | `	/* Point to the underlying vfs */` |
|      4 | 1077 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 | 1078 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - | 1079 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1080 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1081 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1082 | `			ph7_function_name(pCtx)` |
|      - | 1083 | `			);` |
|    ! 0 | 1084 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1085 | `		return PH7_OK;` |
|      - | 1086 | `	}` |
|      - | 1087 | `	/* Point to the desired directory */` |
|      4 | 1088 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1089 | `	/* Perform the requested operation */` |
|      4 | 1090 | `	rc = pVfs->xIslink(zPath);` |
|      - | 1091 | `	/* IO return value */` |
|      4 | 1092 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 1093 | `	return PH7_OK;` |
|      2 | 1094 | `}` |
|      - | 1095 | `/*` |
|      - | 1096 | ` * bool is_readable(string $filename)` |
|      - | 1097 | ` *  Tells whether a file exists and is readable.` |
|      - | 1098 | ` * Parameters` |
|      - | 1099 | ` *  $filename` |
|      - | 1100 | ` *   Path to the file.` |
|      - | 1101 | ` * Return` |
|      - | 1102 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1103 | ` */` |
|      2 | 1104 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1105 | `{` |
|      - | 1106 | `	const char *zPath;` |
|      - | 1107 | `	ph7_vfs *pVfs;` |
|      - | 1108 | `	int rc;` |
|      3 | 1109 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1110 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1111 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1112 | `		return PH7_OK;` |
|      - | 1113 | `	}` |
|      - | 1114 | `	/* Point to the underlying vfs */` |
|      3 | 1115 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1116 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - | 1117 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1118 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1119 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1120 | `			ph7_function_name(pCtx)` |
|      - | 1121 | `			);` |
|    ! 0 | 1122 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1123 | `		return PH7_OK;` |
|      - | 1124 | `	}` |
|      - | 1125 | `	/* Point to the desired directory */` |
|      3 | 1126 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1127 | `	/* Perform the requested operation */` |
|      3 | 1128 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1129 | `	/* IO return value */` |
|      3 | 1130 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1131 | `	return PH7_OK;` |
|      2 | 1132 | `}` |
|      - | 1133 | `/*` |
|      - | 1134 | ` * bool is_writable(string $filename)` |
|      - | 1135 | ` *  Tells whether the filename is writable.` |
|      - | 1136 | ` * Parameters` |
|      - | 1137 | ` *  $filename` |
|      - | 1138 | ` *   Path to the file.` |
|      - | 1139 | ` * Return` |
|      - | 1140 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1141 | ` */` |
|      4 | 1142 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1143 | `{` |
|      - | 1144 | `	const char *zPath;` |
|      - | 1145 | `	ph7_vfs *pVfs;` |
|      - | 1146 | `	int rc;` |
|      5 | 1147 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1148 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1149 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1150 | `		return PH7_OK;` |
|      - | 1151 | `	}` |
|      - | 1152 | `	/* Point to the underlying vfs */` |
|      5 | 1153 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1154 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1155 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1156 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1157 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1158 | `			ph7_function_name(pCtx)` |
|      - | 1159 | `			);` |
|    ! 0 | 1160 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1161 | `		return PH7_OK;` |
|      - | 1162 | `	}` |
|      - | 1163 | `	/* Point to the desired directory */` |
|      5 | 1164 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1165 | `	/* Perform the requested operation */` |
|      5 | 1166 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1167 | `	/* IO return value */` |
|      5 | 1168 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1169 | `	return PH7_OK;` |
|      3 | 1170 | `}` |
|      - | 1171 | `/*` |
|      - | 1172 | ` * bool is_executable(string $filename)` |
|      - | 1173 | ` *  Tells whether the filename is executable.` |
|      - | 1174 | ` * Parameters` |
|      - | 1175 | ` *  $filename` |
|      - | 1176 | ` *   Path to the file.` |
|      - | 1177 | ` * Return` |
|      - | 1178 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1179 | ` */` |
|      2 | 1180 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1181 | `{` |
|      - | 1182 | `	const char *zPath;` |
|      - | 1183 | `	ph7_vfs *pVfs;` |
|      - | 1184 | `	int rc;` |
|      3 | 1185 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1186 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1187 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1188 | `		return PH7_OK;` |
|      - | 1189 | `	}` |
|      - | 1190 | `	/* Point to the underlying vfs */` |
|      3 | 1191 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1192 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1193 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1194 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1195 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1196 | `			ph7_function_name(pCtx)` |
|      - | 1197 | `			);` |
|    ! 0 | 1198 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1199 | `		return PH7_OK;` |
|      - | 1200 | `	}` |
|      - | 1201 | `	/* Point to the desired directory */` |
|      3 | 1202 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1203 | `	/* Perform the requested operation */` |
|      3 | 1204 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1205 | `	/* IO return value */` |
|      3 | 1206 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1207 | `	return PH7_OK;` |
|      2 | 1208 | `}` |
|      - | 1209 | `/*` |
|      - | 1210 | ` * string filetype(string $filename)` |
|      - | 1211 | ` *  Gets file type.` |
|      - | 1212 | ` * Parameters` |
|      - | 1213 | ` *  $filename` |
|      - | 1214 | ` *   Path to the file.` |
|      - | 1215 | ` * Return` |
|      - | 1216 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1217 | ` *  file, socket and unknown.` |
|      - | 1218 | ` */` |
|      4 | 1219 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1220 | `{` |
|      - | 1221 | `	const char *zPath;` |
|      - | 1222 | `	ph7_vfs *pVfs;` |
|      5 | 1223 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1224 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1225 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1226 | `		return PH7_OK;` |
|      - | 1227 | `	}` |
|      - | 1228 | `	/* Point to the underlying vfs */` |
|      5 | 1229 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1230 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1231 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1232 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1233 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1234 | `			ph7_function_name(pCtx)` |
|      - | 1235 | `			);` |
|    ! 0 | 1236 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1237 | `		return PH7_OK;` |
|      - | 1238 | `	}` |
|      - | 1239 | `	/* Point to the desired directory */` |
|      5 | 1240 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1241 | `	/* Set the empty string as the default return value */` |
|      5 | 1242 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1243 | `	/* Perform the requested operation */` |
|      5 | 1244 | `	pVfs->xFiletype(zPath,pCtx);` |
|      5 | 1245 | `	return PH7_OK;` |
|      3 | 1246 | `}` |
|      - | 1247 | `/*` |
|      - | 1248 | ` * array stat(string $filename)` |
|      - | 1249 | ` *  Gives information about a file.` |
|      - | 1250 | ` * Parameters` |
|      - | 1251 | ` *  $filename` |
|      - | 1252 | ` *   Path to the file.` |
|      - | 1253 | ` * Return` |
|      - | 1254 | ` *  An associative array on success holding the following entries on success` |
|      - | 1255 | ` *  0   dev     device number` |
|      - | 1256 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1257 | ` * 2    mode    inode protection mode` |
|      - | 1258 | ` * 3    nlink   number of links` |
|      - | 1259 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1260 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1261 | ` * 6    rdev    device type, if inode device` |
|      - | 1262 | ` * 7    size    size in bytes` |
|      - | 1263 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1264 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1265 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1266 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1267 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1268 | ` * Note:` |
|      - | 1269 | ` *  FALSE is returned on failure.` |
|      - | 1270 | ` */` |
|     10 | 1271 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1272 | `{` |
|      - | 1273 | `	ph7_value *pArray,*pValue;` |
|      - | 1274 | `	const char *zPath;` |
|      - | 1275 | `	ph7_vfs *pVfs;` |
|      - | 1276 | `	int rc;` |
|     11 | 1277 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1278 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1279 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1280 | `		return PH7_OK;` |
|      - | 1281 | `	}` |
|      - | 1282 | `	/* Point to the underlying vfs */` |
|     11 | 1283 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 1284 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1285 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1286 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1287 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1288 | `			ph7_function_name(pCtx)` |
|      - | 1289 | `			);` |
|    ! 0 | 1290 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1291 | `		return PH7_OK;` |
|      - | 1292 | `	}` |
|      - | 1293 | `	/* Create the array and the working value */` |
|     11 | 1294 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 1295 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 1296 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1297 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1298 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1299 | `		return PH7_OK;` |
|      - | 1300 | `	}` |
|      - | 1301 | `	/* Extract the file path */` |
|     11 | 1302 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1303 | `	/* Perform the requested operation */` |
|     11 | 1304 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     11 | 1305 | `	if( rc != PH7_OK ){` |
|      - | 1306 | `		/* IO error,return FALSE */` |
|    ! 0 | 1307 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1308 | `	}else{` |
|      - | 1309 | `		/* Return the associative array */` |
|     11 | 1310 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1311 | `	}` |
|      - | 1312 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1313 | `	 * automatically as soon we return from this function. */` |
|     11 | 1314 | `	return PH7_OK;` |
|      6 | 1315 | `}` |
|      - | 1316 | `/*` |
|      - | 1317 | ` * array lstat(string $filename)` |
|      - | 1318 | ` *  Gives information about a file or symbolic link.` |
|      - | 1319 | ` * Parameters` |
|      - | 1320 | ` *  $filename` |
|      - | 1321 | ` *   Path to the file.` |
|      - | 1322 | ` * Return` |
|      - | 1323 | ` *  An associative array on success holding the following entries on success` |
|      - | 1324 | ` *  0   dev     device number` |
|      - | 1325 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1326 | ` * 2    mode    inode protection mode` |
|      - | 1327 | ` * 3    nlink   number of links` |
|      - | 1328 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1329 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1330 | ` * 6    rdev    device type, if inode device` |
|      - | 1331 | ` * 7    size    size in bytes` |
|      - | 1332 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1333 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1334 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1335 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1336 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1337 | ` * Note:` |
|      - | 1338 | ` *  FALSE is returned on failure.` |
|      - | 1339 | ` */` |
|      2 | 1340 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1341 | `{` |
|      - | 1342 | `	ph7_value *pArray,*pValue;` |
|      - | 1343 | `	const char *zPath;` |
|      - | 1344 | `	ph7_vfs *pVfs;` |
|      - | 1345 | `	int rc;` |
|      3 | 1346 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1347 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1348 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1349 | `		return PH7_OK;` |
|      - | 1350 | `	}` |
|      - | 1351 | `	/* Point to the underlying vfs */` |
|      3 | 1352 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1353 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1354 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1355 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1356 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1357 | `			ph7_function_name(pCtx)` |
|      - | 1358 | `			);` |
|    ! 0 | 1359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1360 | `		return PH7_OK;` |
|      - | 1361 | `	}` |
|      - | 1362 | `	/* Create the array and the working value */` |
|      3 | 1363 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 1364 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1365 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1366 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1367 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1368 | `		return PH7_OK;` |
|      - | 1369 | `	}` |
|      - | 1370 | `	/* Extract the file path */` |
|      3 | 1371 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1372 | `	/* Perform the requested operation */` |
|      3 | 1373 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      3 | 1374 | `	if( rc != PH7_OK ){` |
|      - | 1375 | `		/* IO error,return FALSE */` |
|    ! 0 | 1376 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1377 | `	}else{` |
|      - | 1378 | `		/* Return the associative array */` |
|      3 | 1379 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1380 | `	}` |
|      - | 1381 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1382 | `	 * automatically as soon we return from this function. */` |
|      3 | 1383 | `	return PH7_OK;` |
|      2 | 1384 | `}` |
|      - | 1385 | `/*` |
|      - | 1386 | ` * string getenv(string $varname)` |
|      - | 1387 | ` *  Gets the value of an environment variable.` |
|      - | 1388 | ` * Parameters` |
|      - | 1389 | ` *  $varname` |
|      - | 1390 | ` *   The variable name.` |
|      - | 1391 | ` * Return` |
|      - | 1392 | ` *  Returns the value of the environment variable varname, or FALSE if the environment` |
|      - | 1393 | ` * variable varname does not exist.` |
|      - | 1394 | ` */` |
|     56 | 1395 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1396 | `{` |
|      - | 1397 | `	const char *zEnv;` |
|      - | 1398 | `	ph7_vfs *pVfs;` |
|      - | 1399 | `	int iLen;` |
|     60 | 1400 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1401 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1402 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1403 | `		return PH7_OK;` |
|      - | 1404 | `	}` |
|      - | 1405 | `	/* Point to the underlying vfs */` |
|     60 | 1406 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     60 | 1407 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1408 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1409 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1410 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1411 | `			ph7_function_name(pCtx)` |
|      - | 1412 | `			);` |
|    ! 0 | 1413 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1414 | `		return PH7_OK;` |
|      - | 1415 | `	}` |
|      - | 1416 | `	/* Extract the environment variable */` |
|     60 | 1417 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1418 | `	/* Set a boolean FALSE as the default return value */` |
|     60 | 1419 | `	ph7_result_bool(pCtx,0);` |
|     60 | 1420 | `	if( iLen < 1 ){` |
|      - | 1421 | `		/* Empty string */` |
|    ! 0 | 1422 | `		return PH7_OK;` |
|      - | 1423 | `	}` |
|      - | 1424 | `	/* Perform the requested operation */` |
|     60 | 1425 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     60 | 1426 | `	return PH7_OK;` |
|     32 | 1427 | `}` |
|      - | 1428 | `/*` |
|      - | 1429 | ` * bool putenv(string $settings)` |
|      - | 1430 | ` *  Set the value of an environment variable.` |
|      - | 1431 | ` * Parameters` |
|      - | 1432 | ` *  $setting` |
|      - | 1433 | ` *   The setting, like "FOO=BAR"` |
|      - | 1434 | ` * Return` |
|      - | 1435 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1436 | ` */` |
|      6 | 1437 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1438 | `{` |
|      - | 1439 | `	const char *zName,*zValue;` |
|      - | 1440 | `	char *zSettings,*zEnd;` |
|      - | 1441 | `	ph7_vfs *pVfs;` |
|      - | 1442 | `	int iLen,rc;` |
|      7 | 1443 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1444 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1445 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1446 | `		return PH7_OK;` |
|      - | 1447 | `	}` |
|      - | 1448 | `	/* Extract the setting variable */` |
|      7 | 1449 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|      7 | 1450 | `	if( iLen < 1 ){` |
|      - | 1451 | `		/* Empty string,return FALSE */` |
|    ! 0 | 1452 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1453 | `		return PH7_OK;` |
|      - | 1454 | `	}` |
|      - | 1455 | `	/* Parse the setting */` |
|      7 | 1456 | `	zEnd = &zSettings[iLen];` |
|      7 | 1457 | `	zValue = 0;` |
|      7 | 1458 | `	zName = zSettings;` |
|    127 | 1459 | `	while( zSettings < zEnd ){` |
|    127 | 1460 | `		if( zSettings[0] == '=' ){` |
|      - | 1461 | `			/* Null terminate the name */` |
|      7 | 1462 | `			zSettings[0] = 0;` |
|      7 | 1463 | `			zValue = &zSettings[1];` |
|      7 | 1464 | `			break;` |
|      - | 1465 | `		}` |
|    121 | 1466 | `		zSettings++;` |
|      1 | 1467 | `	}` |
|      - | 1468 | `	/* Install the environment variable in the $_Env array */` |
|      7 | 1469 | `	if( zValue == 0 \|\| zName[0] == 0 \|\| zValue >= zEnd \|\| zName >= zValue ){` |
|      - | 1470 | `		/* Invalid settings,retun FALSE */` |
|      5 | 1471 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1472 | `		if( zSettings  < zEnd ){` |
|      5 | 1473 | `			zSettings[0] = '=';` |
|      2 | 1474 | `		}` |
|      5 | 1475 | `		return PH7_OK;` |
|      - | 1476 | `	}` |
|      3 | 1477 | `	ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_ENV_ATTR,zName,zValue,(int)(zEnd-zValue));` |
|      - | 1478 | `	/* Point to the underlying vfs */` |
|      3 | 1479 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1480 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 1481 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1482 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1483 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1484 | `			ph7_function_name(pCtx)` |
|      - | 1485 | `			);` |
|    ! 0 | 1486 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1487 | `		zSettings[0] = '=';` |
|    ! 0 | 1488 | `		return PH7_OK;` |
|      - | 1489 | `	}` |
|      - | 1490 | `	/* Perform the requested operation */` |
|      3 | 1491 | `	rc = pVfs->xSetenv(zName,zValue);` |
|      3 | 1492 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 1493 | `	zSettings[0] = '=';` |
|      3 | 1494 | `	return PH7_OK;` |
|      4 | 1495 | `}` |
|      - | 1496 | `/*` |
|      - | 1497 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 1498 | ` *  Sets access and modification time of file.` |
|      - | 1499 | ` * Note: On windows` |
|      - | 1500 | ` *   If the file does not exists,it will not be created.` |
|      - | 1501 | ` * Parameters` |
|      - | 1502 | ` *  $filename` |
|      - | 1503 | ` *   The name of the file being touched.` |
|      - | 1504 | ` *  $time` |
|      - | 1505 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 1506 | ` * $atime` |
|      - | 1507 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 1508 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 1509 | ` *   present, the current system time is used.` |
|      - | 1510 | ` * Return` |
|      - | 1511 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1512 | `*/` |
|      4 | 1513 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1514 | `{` |
|      - | 1515 | `	ph7_int64 nTime,nAccess;` |
|      - | 1516 | `	const char *zFile;` |
|      - | 1517 | `	ph7_vfs *pVfs;` |
|      - | 1518 | `	int rc;` |
|      5 | 1519 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1520 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1521 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1522 | `		return PH7_OK;` |
|      - | 1523 | `	}` |
|      - | 1524 | `	/* Point to the underlying vfs */` |
|      5 | 1525 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1526 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 1527 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1528 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1529 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1530 | `			ph7_function_name(pCtx)` |
|      - | 1531 | `			);` |
|    ! 0 | 1532 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1533 | `		return PH7_OK;` |
|      - | 1534 | `	}` |
|      - | 1535 | `	/* Perform the requested operation */` |
|      5 | 1536 | `	nTime = nAccess = -1;` |
|      5 | 1537 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|      5 | 1538 | `	if( nArg > 1 ){` |
|      2 | 1539 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      2 | 1540 | `		if( nArg > 2 ){` |
|      2 | 1541 | `			nAccess = ph7_value_to_int64(apArg[1]);` |
|      1 | 1542 | `		}else{` |
|    ! 0 | 1543 | `			nAccess = nTime;` |
|      - | 1544 | `		}` |
|      1 | 1545 | `	}` |
|      5 | 1546 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 1547 | `	/* IO result */` |
|      5 | 1548 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1549 | `	return PH7_OK;` |
|      3 | 1550 | `}` |
|      - | 1551 | `/*` |
|      - | 1552 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 1553 | ` * Status:` |
|      - | 1554 | ` *    Stable.` |
|      - | 1555 | ` */` |
|      - | 1556 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1557 | `/*` |
|      - | 1558 | ` * string dirname(string $path)` |
|      - | 1559 |  |
|      - | 1560 | ` *  Returns parent directory's path.` |
|      - | 1561 | ` * Parameters` |
|      - | 1562 | ` * $path` |
|      - | 1563 | ` *  Target path.` |
|      - | 1564 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1565 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1566 | ` * Return` |
|      - | 1567 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 1568 | ` *  is returned, indicating the current directory.` |
|      - | 1569 | ` */` |
|     38 | 1570 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1571 | `{` |
|      - | 1572 | `	const char *zPath,*zDir;` |
|      - | 1573 | `	int iLen,iDirlen;` |
|     43 | 1574 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1575 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 1576 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1577 | `		return PH7_OK;` |
|      - | 1578 | `	}` |
|      - | 1579 | `	/* Point to the target path */` |
|     43 | 1580 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     43 | 1581 | `	if( iLen < 1 ){` |
|      - | 1582 | `		/* php answers "" for the empty path, not "." */` |
|      3 | 1583 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1584 | `		return PH7_OK;` |
|      - | 1585 | `	}` |
|      - | 1586 | `	/* Perform the requested operation */` |
|     41 | 1587 | `	zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1588 | `	/* Return directory name */` |
|     41 | 1589 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     41 | 1590 | `	return PH7_OK;` |
|     24 | 1591 | `}` |
|      - | 1592 | `/*` |
|      - | 1593 | ` * string basename(string $path[, string $suffix ])` |
|      - | 1594 | ` *  Returns trailing name component of path.` |
|      - | 1595 | ` * Parameters` |
|      - | 1596 | ` * $path` |
|      - | 1597 | ` *  Target path.` |
|      - | 1598 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1599 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1600 | ` * $suffix` |
|      - | 1601 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 1602 | ` * Return` |
|      - | 1603 | ` *  The base name of the given path.` |
|      - | 1604 | ` */` |
|     46 | 1605 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1606 | `{` |
|      - | 1607 | `	const char *zPath,*zBase,*zEnd;` |
|      - | 1608 | `	int c,d,iLen;` |
|     47 | 1609 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1610 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1611 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1612 | `		return PH7_OK;` |
|      - | 1613 | `	}` |
|     47 | 1614 | `	c = d = '/';` |
|      - | 1615 | `#ifdef __WINNT__` |
|      1 | 1616 | `	d = '\\';` |
|      - | 1617 | `#endif` |
|      - | 1618 | `	/* Point to the target path */` |
|     47 | 1619 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     47 | 1620 | `	if( iLen < 1 ){` |
|      - | 1621 | `		/* Empty string */` |
|      3 | 1622 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1623 | `		return PH7_OK;` |
|      - | 1624 | `	}` |
|      - | 1625 | `	/* Perform the requested operation */` |
|     45 | 1626 | `	zEnd = &zPath[iLen - 1];` |
|      - | 1627 | `	/* Ignore trailing '/' */` |
|     71 | 1628 | `	while( zEnd > zPath && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      5 | 1629 | `		zEnd--;` |
|      1 | 1630 | `	}` |
|     45 | 1631 | `	if( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ){` |
|      - | 1632 | `		/* Nothing but separators ("/", "///"): php answers the EMPTY string, where the` |
|      - | 1633 | `		 * strip loop above stops one short and left PH7 returning "/". */` |
|      5 | 1634 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1635 | `		return PH7_OK;` |
|      - | 1636 | `	}` |
|     41 | 1637 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    975 | 1638 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    915 | 1639 | `		zEnd--;` |
|      1 | 1640 | `	}` |
|     41 | 1641 | `	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     41 | 1642 | `	zEnd = &zPath[iLen];` |
|     41 | 1643 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1644 | `		const char *zSuffix;` |
|      - | 1645 | `		int nSuffix;` |
|      - | 1646 | `		/* Strip suffix */` |
|      5 | 1647 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      5 | 1648 | `		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1649 | `			zEnd -= nSuffix;` |
|      2 | 1650 | `		}` |
|      2 | 1651 | `	}` |
|      - | 1652 | `	/* Store the basename */` |
|     41 | 1653 | `	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));` |
|     41 | 1654 | `	return PH7_OK;` |
|     24 | 1655 | `}` |
|      - | 1656 | `/*` |
|      - | 1657 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1658 | ` *  Returns information about a file path.` |
|      - | 1659 | ` * Parameter` |
|      - | 1660 | ` *  $path` |
|      - | 1661 | ` *   The path to be parsed.` |
|      - | 1662 | ` *  $options` |
|      - | 1663 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 1664 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 1665 | ` * Return` |
|      - | 1666 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 1667 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 1668 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 1669 | ` */` |
|      - | 1670 | `typedef struct path_info path_info;` |
|      - | 1671 | `struct path_info` |
|      - | 1672 | `{` |
|      - | 1673 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1674 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1675 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1676 | `	SyString sFilename;  /* Filename */` |
|      - | 1677 | `};` |
|      - | 1678 | `/*` |
|      - | 1679 | ` * Extract path fields.` |
|      - | 1680 | ` */` |
|  13058 | 1681 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1682 | `{` |
|  13063 | 1683 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1684 | `	SyString *pCur;` |
|      - | 1685 | `	int c,d;` |
|  13063 | 1686 | `	c = d = '/';` |
|      - | 1687 | `#ifdef __WINNT__` |
|      5 | 1688 | `	d = '\\';` |
|      - | 1689 | `#endif` |
|      - | 1690 | `	/* Zero the structure */` |
|  13063 | 1691 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1692 | `	/* Handle special case */` |
|  13063 | 1693 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1694 | `#ifdef __WINNT__` |
|    ! 0 | 1695 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1696 | `#else` |
|    ! 0 | 1697 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1698 | `#endif` |
|    ! 0 | 1699 | `		return SXRET_OK;` |
|      - | 1700 | `	}` |
|      - | 1701 | `	/* Extract the basename */` |
| 351988 | 1702 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 332401 | 1703 | `		zEnd--;` |
|      5 | 1704 | `	}` |
|  13063 | 1705 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13063 | 1706 | `	zEnd = &zPath[nByte];` |
|      - | 1707 | `	/* dirname */` |
|  13063 | 1708 | `	pCur = &pOut->sDir;` |
|  13063 | 1709 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13063 | 1710 | `	if( pCur->nByte > 1 ){` |
|  26121 | 1711 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1712 | `#ifdef __WINNT__` |
|      5 | 1713 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1714 | `#endif` |
|   6534 | 1715 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1716 | `#ifdef __WINNT__` |
|    ! 0 | 1717 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1718 | `#else` |
|    ! 0 | 1719 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1720 | `#endif` |
|    ! 0 | 1721 | `	}` |
|      - | 1722 | `	/* basename/filename */` |
|  13063 | 1723 | `	pCur = &pOut->sBasename;` |
|  13063 | 1724 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13063 | 1725 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1726 | `#ifdef __WINNT__` |
|      5 | 1727 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1728 | `#endif` |
|  13063 | 1729 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13063 | 1730 | `	if( pCur->nByte > 0 ){` |
|      - | 1731 | `		/* extension */` |
|  13063 | 1732 | `		zEnd--;` |
|  65289 | 1733 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  52231 | 1734 | `			zEnd--;` |
|      5 | 1735 | `		}` |
|  13063 | 1736 | `		if( zEnd > pCur->zString ){` |
|  13061 | 1737 | `			zEnd++; /* Jump leading dot */` |
|  13061 | 1738 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1739 | `			/* Fix filename */` |
|  13061 | 1740 | `			pCur = &pOut->sFilename;` |
|  13061 | 1741 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13061 | 1742 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6528 | 1743 | `			}` |
|   6528 | 1744 | `		}` |
|   6529 | 1745 | `	}` |
|  13063 | 1746 | `	return SXRET_OK;` |
|   6534 | 1747 | `}` |
|      - | 1748 | `/*` |
|      - | 1749 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1750 | ` *  See block comment above.` |
|      - | 1751 | ` */` |
|  13058 | 1752 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1753 | `{` |
|      - | 1754 | `	const char *zPath;` |
|      - | 1755 | `	path_info sInfo;` |
|      - | 1756 | `	SyString *pComp;` |
|      - | 1757 | `	int iLen;` |
|  13063 | 1758 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1759 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1760 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1761 | `		return PH7_OK;` |
|      - | 1762 | `	}` |
|      - | 1763 | `	/* Point to the target path */` |
|  13063 | 1764 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13063 | 1765 | `	if( iLen < 1 ){` |
|      - | 1766 | `		/* Empty string */` |
|    ! 0 | 1767 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1768 | `		return PH7_OK;` |
|      - | 1769 | `	}` |
|      - | 1770 | `	/* Extract path info */` |
|  13063 | 1771 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  19591 | 1772 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1773 | `		/* Return path component */` |
|  13061 | 1774 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13061 | 1775 | `		switch(nComp){` |
|      1 | 1776 | `		case 1: /* PATHINFO_DIRNAME */` |
|      3 | 1777 | `			pComp = &sInfo.sDir;` |
|      3 | 1778 | `			if( pComp->nByte > 0 ){` |
|      3 | 1779 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1780 | `			}else{` |
|      - | 1781 | `				/* Expand the empty string */` |
|    ! 0 | 1782 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1783 | `			}` |
|      3 | 1784 | `			break;` |
|      1 | 1785 | `		case 2: /*PATHINFO_BASENAME*/` |
|      3 | 1786 | `			pComp = &sInfo.sBasename;` |
|      3 | 1787 | `			if( pComp->nByte > 0 ){` |
|      3 | 1788 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1789 | `			}else{` |
|      - | 1790 | `				/* Expand the empty string */` |
|    ! 0 | 1791 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1792 | `			}` |
|      3 | 1793 | `			break;` |
|   3265 | 1794 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6535 | 1795 | `			pComp = &sInfo.sExtension;` |
|   6535 | 1796 | `			if( pComp->nByte > 0 ){` |
|   6533 | 1797 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3269 | 1798 | `			}else{` |
|      - | 1799 | `				/* Expand the empty string */` |
|      3 | 1800 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1801 | `			}` |
|   6535 | 1802 | `			break;` |
|   3261 | 1803 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6527 | 1804 | `			pComp = &sInfo.sFilename;` |
|   6527 | 1805 | `			if( pComp->nByte > 0 ){` |
|   6527 | 1806 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3266 | 1807 | `			}else{` |
|      - | 1808 | `				/* Expand the empty string */` |
|    ! 0 | 1809 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1810 | `			}` |
|   6527 | 1811 | `			break;` |
|    ! 0 | 1812 | `		default:` |
|      - | 1813 | `			/* Expand the empty string */` |
|    ! 0 | 1814 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1815 | `			break;` |
|      - | 1816 | `		}` |
|   6533 | 1817 | `	}else{` |
|      - | 1818 | `		/* Return an associative array */` |
|      - | 1819 | `		ph7_value *pArray,*pValue;` |
|      3 | 1820 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 1821 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1822 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1823 | `			/* Out of mem,return NULL */` |
|    ! 0 | 1824 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1825 | `			return PH7_OK;` |
|      - | 1826 | `		}` |
|      - | 1827 | `		/* dirname */` |
|      3 | 1828 | `		pComp = &sInfo.sDir;` |
|      3 | 1829 | `		if( pComp->nByte > 0 ){` |
|      3 | 1830 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1831 | `			/* Perform the insertion */` |
|      3 | 1832 | `			ph7_array_add_strkey_elem(pArray,"dirname",pValue); /* Will make it's own copy */` |
|      1 | 1833 | `		}` |
|      - | 1834 | `		/* Reset the string cursor */` |
|      3 | 1835 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1836 | `		/* basername */` |
|      3 | 1837 | `		pComp = &sInfo.sBasename;` |
|      3 | 1838 | `		if( pComp->nByte > 0 ){` |
|      3 | 1839 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1840 | `			/* Perform the insertion */` |
|      3 | 1841 | `			ph7_array_add_strkey_elem(pArray,"basename",pValue); /* Will make it's own copy */` |
|      1 | 1842 | `		}` |
|      - | 1843 | `		/* Reset the string cursor */` |
|      3 | 1844 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1845 | `		/* extension */` |
|      3 | 1846 | `		pComp = &sInfo.sExtension;` |
|      3 | 1847 | `		if( pComp->nByte > 0 ){` |
|      3 | 1848 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1849 | `			/* Perform the insertion */` |
|      3 | 1850 | `			ph7_array_add_strkey_elem(pArray,"extension",pValue); /* Will make it's own copy */` |
|      1 | 1851 | `		}` |
|      - | 1852 | `		/* Reset the string cursor */` |
|      3 | 1853 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1854 | `		/* filename */` |
|      3 | 1855 | `		pComp = &sInfo.sFilename;` |
|      3 | 1856 | `		if( pComp->nByte > 0 ){` |
|      3 | 1857 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1858 | `			/* Perform the insertion */` |
|      3 | 1859 | `			ph7_array_add_strkey_elem(pArray,"filename",pValue); /* Will make it's own copy */` |
|      1 | 1860 | `		}` |
|      - | 1861 | `		/* Return the created array */` |
|      3 | 1862 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1863 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 1864 | `		 * automatically as soon we return from this foreign function.` |
|      - | 1865 | `		 */` |
|      - | 1866 | `	}` |
|  13063 | 1867 | `	return PH7_OK;` |
|   6534 | 1868 | `}` |
|      - | 1869 | `/* SPDX-SnippetBegin */` |
|      - | 1870 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 1871 | `/* SPDX-License-Identifier: blessing */` |
|      - | 1872 | `/*` |
|      - | 1873 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 1874 |  |
|      - | 1875 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 1876 | ` * Status: Public Domain` |
|      - | 1877 | ` */` |
|      - | 1878 | `typedef unsigned char u8;` |
|      - | 1879 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 1880 | `** lower-case character.` |
|      - | 1881 | `**` |
|      - | 1882 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 1883 | `** handle case conversions for the UTF character set since the tables` |
|      - | 1884 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 1885 | `*/` |
|      - | 1886 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 1887 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 1888 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 1889 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 1890 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 1891 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 1892 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 1893 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 1894 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 1895 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 1896 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 1897 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 1898 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 1899 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 1900 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 1901 | `    252,253,254,255` |
|      - | 1902 | `};` |
|      - | 1903 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 1904 | `/*` |
|      - | 1905 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 1906 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 1907 | `*/` |
|      - | 1908 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 1909 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 1910 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 1911 | `  }                                                    \` |
|      - | 1912 | `}` |
|      - | 1913 | `/*` |
|      - | 1914 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 1915 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 1916 | `** are the same and false (0) if they are different.` |
|      - | 1917 | `**` |
|      - | 1918 | `** Globbing rules:` |
|      - | 1919 | `**` |
|      - | 1920 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 1921 | `**` |
|      - | 1922 | `**      '?'       Matches exactly one character.` |
|      - | 1923 | `**` |
|      - | 1924 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 1925 | `**                characters.` |
|      - | 1926 | `**` |
|      - | 1927 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 1928 | `**` |
|      - | 1929 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 1930 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 1931 | `** range of characters can be specified using '-'.  Example:` |
|      - | 1932 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 1933 | `** it the last character in the list.` |
|      - | 1934 | `**` |
|      - | 1935 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 1936 | `**` |
|      - | 1937 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 1938 | `**` |
|      - | 1939 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 1940 | `*/` |
|     44 | 1941 | `static int patternCompare(` |
|      - | 1942 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1943 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1944 | `  const int esc,                    /* The escape character */` |
|      - | 1945 | `  int noCase` |
|      1 | 1946 | `){` |
|      - | 1947 | `  int c, c2;` |
|      - | 1948 | `  int invert;` |
|      - | 1949 | `  int seen;` |
|     45 | 1950 | `  u8 matchOne = '?';` |
|     45 | 1951 | `  u8 matchAll = '*';` |
|     45 | 1952 | `  u8 matchSet = '[';` |
|     45 | 1953 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1954 |  |
|     45 | 1955 | `  if( !zPattern \|\| !zString ) return 0;` |
|     81 | 1956 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     73 | 1957 | `    if( !prevEscape && c==matchAll ){` |
|     52 | 1958 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|     27 | 1959 | `               \|\| c == matchOne ){` |
|    ! 0 | 1960 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1961 | `          return 0;` |
|      - | 1962 | `        }` |
|    ! 0 | 1963 | `      }` |
|     27 | 1964 | `      if( c==0 ){` |
|     19 | 1965 | `        return 1;` |
|      9 | 1966 | `      }else if( c==esc ){` |
|    ! 0 | 1967 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1968 | `        if( c==0 ){` |
|    ! 0 | 1969 | `          return 0;` |
|    ! 0 | 1970 | `        }` |
|      9 | 1971 | `      }else if( c==matchSet ){` |
|    ! 0 | 1972 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 1973 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 1974 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 1975 | `        }` |
|    ! 0 | 1976 | `        return *zString!=0;` |
|      - | 1977 | `      }` |
|     11 | 1978 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     11 | 1979 | `        if( noCase ){` |
|      3 | 1980 | `          GlogUpperToLower(c2);` |
|      3 | 1981 | `          GlogUpperToLower(c);` |
|     11 | 1982 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 1983 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 1984 | `            GlogUpperToLower(c2);` |
|      1 | 1985 | `          }` |
|      2 | 1986 | `        }else{` |
|     47 | 1987 | `          while( c2 != 0 && c2 != c ){` |
|     39 | 1988 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 1989 | `          }` |
|      - | 1990 | `        }` |
|     11 | 1991 | `        if( c2==0 ) return 0;` |
|      9 | 1992 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 1993 | `      }` |
|    ! 0 | 1994 | `      return 0;` |
|     47 | 1995 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 1996 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1997 | `        return 0;` |
|    ! 0 | 1998 | `      }` |
|     47 | 1999 | `    }else if( c==matchSet ){` |
|    ! 0 | 2000 | `      int prior_c = 0;` |
|    ! 0 | 2001 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 2002 | `      seen = 0;` |
|    ! 0 | 2003 | `      invert = 0;` |
|    ! 0 | 2004 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 2005 | `      if( c==0 ) return 0;` |
|    ! 0 | 2006 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2007 | `      if( c2=='^' ){` |
|    ! 0 | 2008 | `        invert = 1;` |
|    ! 0 | 2009 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2010 | `      }` |
|    ! 0 | 2011 | `      if( c2==']' ){` |
|    ! 0 | 2012 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 2013 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2014 | `      }` |
|    ! 0 | 2015 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 2016 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 2017 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2018 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 2019 | `          prior_c = 0;` |
|    ! 0 | 2020 | `        }else{` |
|    ! 0 | 2021 | `          if( c==c2 ){` |
|    ! 0 | 2022 | `            seen = 1;` |
|    ! 0 | 2023 | `          }` |
|    ! 0 | 2024 | `          prior_c = c2;` |
|      - | 2025 | `        }` |
|    ! 0 | 2026 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2027 | `      }` |
|    ! 0 | 2028 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 2029 | `        return 0;` |
|    ! 0 | 2030 | `      }` |
|     47 | 2031 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 2032 | `      prevEscape = 1;` |
|    ! 0 | 2033 | `    }else{` |
|     47 | 2034 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     47 | 2035 | `      if( noCase ){` |
|      7 | 2036 | `        GlogUpperToLower(c);` |
|      7 | 2037 | `        GlogUpperToLower(c2);` |
|      3 | 2038 | `      }` |
|     47 | 2039 | `      if( c!=c2 ){` |
|     11 | 2040 | `        return 0;` |
|      - | 2041 | `      }` |
|     37 | 2042 | `      prevEscape = 0;` |
|      - | 2043 | `    }` |
|      1 | 2044 | `  }` |
|      9 | 2045 | `  return *zString==0;` |
|     23 | 2046 | `}` |
|      - | 2047 | `/* SPDX-SnippetEnd */` |
|      - | 2048 | `/*` |
|      - | 2049 | ` * Wrapper around patternCompare() defined above.` |
|      - | 2050 | ` * See block comment above for more information.` |
|      - | 2051 | ` */` |
|     36 | 2052 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 2053 | `{` |
|      - | 2054 | `	int rc;` |
|     37 | 2055 | `	if( iEsc < 0 ){` |
|    ! 0 | 2056 | `		iEsc = '\\';` |
|    ! 0 | 2057 | `	}` |
|     37 | 2058 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     37 | 2059 | `	return rc;` |
|      1 | 2060 | `}` |
|      - | 2061 | `/*` |
|      - | 2062 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 2063 | ` *  Match filename against a pattern.` |
|      - | 2064 | ` * Parameters` |
|      - | 2065 | ` *  $pattern` |
|      - | 2066 | ` *   The shell wildcard pattern.` |
|      - | 2067 | ` * $string` |
|      - | 2068 | ` *  The tested string.` |
|      - | 2069 | ` * $flags` |
|      - | 2070 | ` *   A list of possible flags:` |
|      - | 2071 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 2072 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 2073 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 2074 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 2075 | ` * Return` |
|      - | 2076 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2077 | ` */` |
|      8 | 2078 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2079 | `{` |
|      - | 2080 | `	const char *zString,*zPattern;` |
|      9 | 2081 | `	int iEsc = '\\';` |
|      9 | 2082 | `	int noCase = 0;` |
|      - | 2083 | `	int rc;` |
|      9 | 2084 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2085 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2086 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2087 | `		return PH7_OK;` |
|      - | 2088 | `	}` |
|      - | 2089 | `	/* Extract the pattern and the string */` |
|      9 | 2090 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 2091 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2092 | `	/* Extract the flags if avaialble */` |
|      9 | 2093 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 2094 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 2095 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|    ! 0 | 2096 | `			iEsc = 0;` |
|    ! 0 | 2097 | `		}` |
|      7 | 2098 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
|      3 | 2099 | `			noCase = 1;` |
|      1 | 2100 | `		}` |
|      3 | 2101 | `	}` |
|      - | 2102 | `	/* Go globbing */` |
|      9 | 2103 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 2104 | `	/* Globbing result */` |
|      9 | 2105 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 2106 | `	return PH7_OK;` |
|      5 | 2107 | `}` |
|      - | 2108 | `/*` |
|      - | 2109 | ` * bool strglob(string $pattern,string $string)` |
|      - | 2110 | ` *  Match string against a pattern.` |
|      - | 2111 | ` * Parameters` |
|      - | 2112 | ` *  $pattern` |
|      - | 2113 | ` *   The shell wildcard pattern.` |
|      - | 2114 | ` * $string` |
|      - | 2115 | ` *  The tested string.` |
|      - | 2116 | ` * Return` |
|      - | 2117 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2118 | ` * Note that this a symisc eXtension.` |
|      - | 2119 | ` */` |
|     28 | 2120 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2121 | `{` |
|      - | 2122 | `	const char *zString,*zPattern;` |
|     29 | 2123 | `	int iEsc = '\\';` |
|      - | 2124 | `	int rc;` |
|     29 | 2125 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2126 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2127 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2128 | `		return PH7_OK;` |
|      - | 2129 | `	}` |
|      - | 2130 | `	/* Extract the pattern and the string */` |
|     29 | 2131 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|     29 | 2132 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2133 | `	/* Go globbing */` |
|     29 | 2134 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2135 | `	/* Globbing result */` |
|     29 | 2136 | `	ph7_result_bool(pCtx,rc);` |
|     29 | 2137 | `	return PH7_OK;` |
|     15 | 2138 | `}` |
|      - | 2139 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2140 | `/*` |
|      - | 2141 | ` * bool link(string $target,string $link)` |
|      - | 2142 |  |
|      - | 2143 | ` *  Create a hard link.` |
|      - | 2144 | ` * Parameters` |
|      - | 2145 | ` *  $target` |
|      - | 2146 | ` *   Target of the link.` |
|      - | 2147 | ` *  $link` |
|      - | 2148 | ` *   The link name.` |
|      - | 2149 | ` * Return` |
|      - | 2150 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2151 | ` */` |
|      2 | 2152 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2153 | `{` |
|      - | 2154 | `	const char *zTarget,*zLink;` |
|      - | 2155 | `	ph7_vfs *pVfs;` |
|      - | 2156 | `	int rc;` |
|      3 | 2157 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2158 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2159 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2160 | `		return PH7_OK;` |
|      - | 2161 | `	}` |
|      - | 2162 | `	/* Point to the underlying vfs */` |
|      3 | 2163 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2164 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2165 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2166 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2167 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2168 | `			ph7_function_name(pCtx)` |
|      - | 2169 | `			);` |
|    ! 0 | 2170 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2171 | `		return PH7_OK;` |
|      - | 2172 | `	}` |
|      - | 2173 | `	/* Extract the given arguments */` |
|      3 | 2174 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      3 | 2175 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2176 | `	/* Perform the requested operation */` |
|      3 | 2177 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2178 | `	/* IO result */` |
|      3 | 2179 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 2180 | `	return PH7_OK;` |
|      2 | 2181 | `}` |
|      - | 2182 | `/*` |
|      - | 2183 | ` * bool symlink(string $target,string $link)` |
|      - | 2184 | ` *  Creates a symbolic link.` |
|      - | 2185 | ` * Parameters` |
|      - | 2186 | ` *  $target` |
|      - | 2187 | ` *   Target of the link.` |
|      - | 2188 | ` *  $link` |
|      - | 2189 | ` *   The link name.` |
|      - | 2190 | ` * Return` |
|      - | 2191 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2192 | ` */` |
|      6 | 2193 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2194 | `{` |
|      - | 2195 | `	const char *zTarget,*zLink;` |
|      - | 2196 | `	ph7_vfs *pVfs;` |
|      - | 2197 | `	int rc;` |
|      7 | 2198 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2199 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2200 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2201 | `		return PH7_OK;` |
|      - | 2202 | `	}` |
|      - | 2203 | `	/* Point to the underlying vfs */` |
|      7 | 2204 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 2205 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2206 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2207 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2208 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2209 | `			ph7_function_name(pCtx)` |
|      - | 2210 | `			);` |
|    ! 0 | 2211 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2212 | `		return PH7_OK;` |
|      - | 2213 | `	}` |
|      - | 2214 | `	/* Extract the given arguments */` |
|      7 | 2215 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      7 | 2216 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2217 | `	/* Perform the requested operation */` |
|      7 | 2218 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2219 | `	/* IO result */` |
|      7 | 2220 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      7 | 2221 | `	return PH7_OK;` |
|      4 | 2222 | `}` |
|      - | 2223 | `/*` |
|      - | 2224 | ` * int umask([ int $mask ])` |
|      - | 2225 | ` *  Changes the current umask.` |
|      - | 2226 | ` * Parameters` |
|      - | 2227 | ` *  $mask` |
|      - | 2228 | ` *   The new umask.` |
|      - | 2229 | ` * Return` |
|      - | 2230 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2231 | ` *  Otherwise the old umask is returned.` |
|      - | 2232 | ` */` |
|      8 | 2233 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2234 | `{` |
|      - | 2235 | `	int iOld,iNew;` |
|      - | 2236 | `	ph7_vfs *pVfs;` |
|      - | 2237 | `	/* Point to the underlying vfs */` |
|      9 | 2238 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 2239 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2240 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2241 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2242 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2243 | `			ph7_function_name(pCtx)` |
|      - | 2244 | `			);` |
|    ! 0 | 2245 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2246 | `		return PH7_OK;` |
|      - | 2247 | `	}` |
|      9 | 2248 | `	iNew = 0;` |
|      9 | 2249 | `	if( nArg > 0 ){` |
|      5 | 2250 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2251 | `	}` |
|      - | 2252 | `	/* Perform the requested operation */` |
|      9 | 2253 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2254 | `	/* Old mask */` |
|      9 | 2255 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 2256 | `	return PH7_OK;` |
|      5 | 2257 | `}` |
|      - | 2258 | `/*` |
|      - | 2259 | ` * string sys_get_temp_dir()` |
|      - | 2260 | ` *  Returns directory path used for temporary files.` |
|      - | 2261 | ` * Parameters` |
|      - | 2262 | ` *  None` |
|      - | 2263 | ` * Return` |
|      - | 2264 | ` *  Returns the path of the temporary directory.` |
|      - | 2265 | ` */` |
|    214 | 2266 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2267 | `{` |
|      - | 2268 | `	ph7_vfs *pVfs;` |
|      - | 2269 | `	/* Set the empty string as the default return value */` |
|    217 | 2270 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2271 | `	/* Point to the underlying vfs */` |
|    217 | 2272 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    217 | 2273 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2274 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2275 | `		SXUNUSED(apArg);` |
|      - | 2276 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2277 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2278 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2279 | `			ph7_function_name(pCtx)` |
|      - | 2280 | `			);` |
|    ! 0 | 2281 | `		return PH7_OK;` |
|      - | 2282 | `	}` |
|      - | 2283 | `	/* Perform the requested operation */` |
|    217 | 2284 | `	pVfs->xTempDir(pCtx);` |
|    217 | 2285 | `	return PH7_OK;` |
|    110 | 2286 | `}` |
|      - | 2287 | `/*` |
|      - | 2288 | ` * string get_current_user()` |
|      - | 2289 | ` *  Returns the name of the current working user.` |
|      - | 2290 | ` * Parameters` |
|      - | 2291 | ` *  None` |
|      - | 2292 | ` * Return` |
|      - | 2293 | ` *  Returns the name of the current working user.` |
|      - | 2294 | ` */` |
|      2 | 2295 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2296 | `{` |
|      - | 2297 | `	ph7_vfs *pVfs;` |
|      - | 2298 | `	/* Point to the underlying vfs */` |
|      3 | 2299 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2300 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2301 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2302 | `		SXUNUSED(apArg);` |
|      - | 2303 | `		/* IO routine not implemented */` |
|    ! 0 | 2304 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2305 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2306 | `			ph7_function_name(pCtx)` |
|      - | 2307 | `			);` |
|      - | 2308 | `		/* Set a dummy username */` |
|    ! 0 | 2309 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2310 | `		return PH7_OK;` |
|      - | 2311 | `	}` |
|      - | 2312 | `	/* Perform the requested operation */` |
|      3 | 2313 | `	pVfs->xUsername(pCtx);` |
|      3 | 2314 | `	return PH7_OK;` |
|      2 | 2315 | `}` |
|      - | 2316 | `/*` |
|      - | 2317 | ` * int64 getmypid()` |
|      - | 2318 | ` *  Gets process ID.` |
|      - | 2319 | ` * Parameters` |
|      - | 2320 | ` *  None` |
|      - | 2321 | ` * Return` |
|      - | 2322 | ` *  Returns the process ID.` |
|      - | 2323 | ` */` |
|     84 | 2324 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2325 | `{` |
|      - | 2326 | `	ph7_int64 nProcessId;` |
|      - | 2327 | `	ph7_vfs *pVfs;` |
|      - | 2328 | `	/* Point to the underlying vfs */` |
|     87 | 2329 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     87 | 2330 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2331 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2332 | `		SXUNUSED(apArg);` |
|      - | 2333 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2334 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2335 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2336 | `			ph7_function_name(pCtx)` |
|      - | 2337 | `			);` |
|    ! 0 | 2338 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2339 | `		return PH7_OK;` |
|      - | 2340 | `	}` |
|      - | 2341 | `	/* Perform the requested operation */` |
|     87 | 2342 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2343 | `	/* Set the result */` |
|     87 | 2344 | `	ph7_result_int64(pCtx,nProcessId);` |
|     87 | 2345 | `	return PH7_OK;` |
|     45 | 2346 | `}` |
|      - | 2347 | `/*` |
|      - | 2348 | ` * int getmyuid()` |
|      - | 2349 | ` *  Get user ID.` |
|      - | 2350 | ` * Parameters` |
|      - | 2351 | ` *  None` |
|      - | 2352 | ` * Return` |
|      - | 2353 | ` *  Returns the user ID.` |
|      - | 2354 | ` */` |
|      2 | 2355 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2356 | `{` |
|      - | 2357 | `	ph7_vfs *pVfs;` |
|      - | 2358 | `	int nUid;` |
|      - | 2359 | `	/* Point to the underlying vfs */` |
|      3 | 2360 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2361 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2362 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2363 | `		SXUNUSED(apArg);` |
|      - | 2364 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2365 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2366 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2367 | `			ph7_function_name(pCtx)` |
|      - | 2368 | `			);` |
|    ! 0 | 2369 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2370 | `		return PH7_OK;` |
|      - | 2371 | `	}` |
|      - | 2372 | `	/* Perform the requested operation */` |
|      3 | 2373 | `	nUid = pVfs->xUid();` |
|      - | 2374 | `	/* Set the result */` |
|      3 | 2375 | `	ph7_result_int(pCtx,nUid);` |
|      3 | 2376 | `	return PH7_OK;` |
|      2 | 2377 | `}` |
|      - | 2378 | `/*` |
|      - | 2379 | ` * int getmygid()` |
|      - | 2380 | ` *  Get group ID.` |
|      - | 2381 | ` * Parameters` |
|      - | 2382 | ` *  None` |
|      - | 2383 | ` * Return` |
|      - | 2384 | ` *  Returns the group ID.` |
|      - | 2385 | ` */` |
|      2 | 2386 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2387 | `{` |
|      - | 2388 | `	ph7_vfs *pVfs;` |
|      - | 2389 | `	int nGid;` |
|      - | 2390 | `	/* Point to the underlying vfs */` |
|      3 | 2391 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2392 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 2393 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2394 | `		SXUNUSED(apArg);` |
|      - | 2395 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2396 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2397 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2398 | `			ph7_function_name(pCtx)` |
|      - | 2399 | `			);` |
|    ! 0 | 2400 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2401 | `		return PH7_OK;` |
|      - | 2402 | `	}` |
|      - | 2403 | `	/* Perform the requested operation */` |
|      3 | 2404 | `	nGid = pVfs->xGid();` |
|      - | 2405 | `	/* Set the result */` |
|      3 | 2406 | `	ph7_result_int(pCtx,nGid);` |
|      3 | 2407 | `	return PH7_OK;` |
|      2 | 2408 | `}` |
|      - | 2409 | `#ifdef __WINNT__` |
|      - | 2410 | `#include <Windows.h>` |
|      - | 2411 | `#elif defined(__UNIXES__)` |
|      - | 2412 | `#include <sys/utsname.h>` |
|      - | 2413 | `#endif` |
|      - | 2414 | `/*` |
|      - | 2415 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 2416 | ` *  Returns information about the host operating system.` |
|      - | 2417 | ` * Parameters` |
|      - | 2418 | ` *  $mode` |
|      - | 2419 | ` *   mode is a single character that defines what information is returned:` |
|      - | 2420 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 2421 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 2422 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 2423 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 2424 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 2425 | ` *    'm': Machine type. eg. i386.` |
|      - | 2426 | ` * Return` |
|      - | 2427 | ` *  OS description as a string.` |
|      - | 2428 | ` */` |
|      4 | 2429 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2430 | `{` |
|      - | 2431 | `#if defined(__WINNT__)` |
|      1 | 2432 | `	const char *zName = "Microsoft Windows";` |
|      - | 2433 | `	OSVERSIONINFOW sVer;` |
|      - | 2434 | `#elif defined(__UNIXES__)` |
|      - | 2435 | `	struct utsname sName;` |
|      - | 2436 | `#endif` |
|      5 | 2437 | `	const char *zMode = "a";` |
|      5 | 2438 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 2439 | `		/* Extract the desired mode */` |
|    ! 0 | 2440 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 2441 | `	}` |
|      - | 2442 | `#if defined(__WINNT__)` |
|      1 | 2443 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 2444 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 2445 | `#if defined(_MSC_VER)` |
|      - | 2446 | `#pragma warning(push)` |
|      - | 2447 | `#pragma warning(disable:4996)` |
|      - | 2448 | `#endif` |
|      1 | 2449 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 2450 | `#if defined(_MSC_VER)` |
|      - | 2451 | `#pragma warning(pop)` |
|      - | 2452 | `#endif` |
|    ! 0 | 2453 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 2454 | `		return PH7_OK;` |
|      - | 2455 | `	}` |
|      1 | 2456 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 2457 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 2458 | `			zName = "Microsoft Windows NT";` |
|      1 | 2459 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 2460 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 2461 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 2462 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 2463 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 2464 | `			}` |
|    ! 0 | 2465 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 2466 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 2467 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 2468 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 2469 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 2470 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 2471 | `					default: break;` |
|      - | 2472 | `				}` |
|      - | 2473 | `		}` |
|      - | 2474 | `	}` |
|      1 | 2475 | `	switch(zMode[0]){` |
|      - | 2476 | `	case 's':` |
|      - | 2477 | `		/* Operating system name */` |
|    ! 0 | 2478 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 2479 | `		break;` |
|      - | 2480 | `	case 'n':` |
|      - | 2481 | `		/* Host name */` |
|    ! 0 | 2482 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 2483 | `		break;` |
|      - | 2484 | `	case 'r':` |
|      - | 2485 | `	case 'v':` |
|      - | 2486 | `		/* Version information. */` |
|    ! 0 | 2487 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 2488 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2489 | `			);` |
|    ! 0 | 2490 | `		break;` |
|      - | 2491 | `	case 'm':` |
|      - | 2492 | `		/* Machine name */` |
|    ! 0 | 2493 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 2494 | `		break;` |
|      - | 2495 | `	default:` |
|      1 | 2496 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 2497 | `			zName,` |
|      - | 2498 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2499 | `			);` |
|      - | 2500 | `		break;` |
|      - | 2501 | `	}` |
|      - | 2502 | `#elif defined(__UNIXES__)` |
|      4 | 2503 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 2504 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 2505 | `		return PH7_OK;` |
|      - | 2506 | `	}` |
|      4 | 2507 | `	switch(zMode[0]){` |
|    ! 0 | 2508 | `	case 's':` |
|      - | 2509 | `		/* Operating system name */` |
|    ! 0 | 2510 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 2511 | `		break;` |
|    ! 0 | 2512 | `	case 'n':` |
|      - | 2513 | `		/* Host name */` |
|    ! 0 | 2514 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 2515 | `		break;` |
|    ! 0 | 2516 | `	case 'r':` |
|      - | 2517 | `		/* Release information */` |
|    ! 0 | 2518 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 2519 | `		break;` |
|    ! 0 | 2520 | `	case 'v':` |
|      - | 2521 | `		/* Version information. */` |
|    ! 0 | 2522 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 2523 | `		break;` |
|    ! 0 | 2524 | `	case 'm':` |
|      - | 2525 | `		/* Machine name */` |
|    ! 0 | 2526 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 2527 | `		break;` |
|      2 | 2528 | `	default:` |
|      6 | 2529 | `		ph7_result_string_format(pCtx,` |
|      - | 2530 | `			"%s %s %s %s %s",` |
|      2 | 2531 | `			sName.sysname,` |
|      2 | 2532 | `			sName.release,` |
|      2 | 2533 | `			sName.version,` |
|      2 | 2534 | `			sName.nodename,` |
|      2 | 2535 | `			sName.machine` |
|      - | 2536 | `			);` |
|      4 | 2537 | `		break;` |
|      - | 2538 | `	}` |
|      - | 2539 | `#else` |
|      - | 2540 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 2541 | `#endif` |
|      5 | 2542 | `	return PH7_OK;` |
|      3 | 2543 | `}` |
|      - | 2544 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 2545 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 2546 | `#if defined(_MSC_VER)` |
|      - | 2547 | `static const ph7_vfs null_vfs = {` |
|      - | 2548 | `#else` |
|      - | 2549 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 2550 | `#endif` |
|      - | 2551 | `	"null_vfs",` |
|      - | 2552 | `	PH7_VFS_VERSION,` |
|      - | 2553 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 2554 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 2555 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 2556 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 2557 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 2558 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 2559 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 2560 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 2561 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 2562 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 2563 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 2564 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 2565 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 2566 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 2567 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 2568 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 2569 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 2570 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 2571 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 2572 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 2573 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 2574 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 2575 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 2576 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 2577 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 2578 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 2579 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 2580 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 2581 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 2582 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 2583 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 2584 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 2585 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 2586 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 2587 | `	0, /* int (*xUmask)(int) */` |
|      - | 2588 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 2589 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 2590 | `	0, /* int (*xUid)(void) */` |
|      - | 2591 | `	0, /* int (*xGid)(void) */` |
|      - | 2592 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 2593 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 2594 | `};` |
|      - | 2595 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 2596 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 2597 | `/*` |
|      - | 2598 | ` * Export the builtin vfs.` |
|      - | 2599 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 2600 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 2601 | ` * Note:` |
|      - | 2602 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 2603 | ` * Note:` |
|      - | 2604 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2605 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 2606 | ` */` |
|   3878 | 2607 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 2608 | `{` |
|      - | 2609 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 2610 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 2611 | `	return &null_vfs;` |
|      - | 2612 | `#else` |
|      - | 2613 | `#ifdef __WINNT__` |
|      5 | 2614 | `	return &sWinVfs;` |
|      - | 2615 | `#elif defined(__UNIXES__)` |
|   3878 | 2616 | `	return &sUnixVfs;` |
|      - | 2617 | `#else` |
|      - | 2618 | `	return &null_vfs;` |
|      - | 2619 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 2620 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 2621 | `#else` |
|      - | 2622 | `	return &null_vfs;` |
|      - | 2623 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 2624 | `}` |
|      - | 2625 | `/*` |
|      - | 2626 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 2627 | ` * [i.e: file://,php://].` |
|      - | 2628 | ` * Note:` |
|      - | 2629 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 2630 | ` *  defined then this function is a no-op.` |
|      - | 2631 | ` */` |
|   3408 | 2632 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 2633 | `{` |
|      - | 2634 | `	/*` |
|      - | 2635 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 2636 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 2637 | `	 */` |
|      - | 2638 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 2639 | `	/* VFS: disk I/O related functions */` |
|      - | 2640 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 2641 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 2642 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 2643 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 2644 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 2645 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 2646 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 2647 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 2648 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 2649 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 2650 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 2651 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 2652 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 2653 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 2654 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 2655 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 2656 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 2657 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 2658 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 2659 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 2660 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 2661 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 2662 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 2663 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 2664 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 2665 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 2666 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 2667 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 2668 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 2669 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 2670 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 2671 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 2672 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 2673 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 2674 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 2675 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 2676 | `		{"link",        PH7_vfs_link     },` |
|      - | 2677 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 2678 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 2679 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 2680 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 2681 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 2682 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 2683 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 2684 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 2685 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 2686 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 2687 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 2688 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 2689 | `	};` |
|      - | 2690 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 2691 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 2692 | `	 */` |
|      - | 2693 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 2694 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 2695 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 2696 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 2697 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 2698 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 2699 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 2700 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 2701 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 2702 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 2703 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 2704 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 2705 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 2706 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 2707 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 2708 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 2709 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 2710 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 2711 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 2712 | `		{"file",      PH7_builtin_file   },` |
|      - | 2713 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 2714 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 2715 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 2716 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 2717 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 2718 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 2719 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 2720 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 2721 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 2722 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 2723 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 2724 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 2725 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 2726 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 2727 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 2728 | `#ifdef PH7_ENABLE_NET` |
|      - | 2729 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 2730 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 2731 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 2732 | `#endif` |
|      - | 2733 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 2734 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 2735 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 2736 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 2737 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 2738 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 2739 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 2740 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 2741 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 2742 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 2743 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 2744 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 2745 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 2746 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 2747 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 2748 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 2749 | `	};` |
|   3413 | 2750 | `	const ph7_io_stream *pFileStream = 0;` |
|   3413 | 2751 | `	sxu32 n = 0;` |
|      - | 2752 | `	/* Register disk-related functions */` |
| 166997 | 2753 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 163589 | 2754 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  81797 | 2755 | `	}` |
| 177221 | 2756 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 173813 | 2757 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  86909 | 2758 | `	}` |
|      - | 2759 | `#else` |
|      - | 2760 | `	SXUNUSED(pVm);` |
|      - | 2761 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 2762 |  |
|      - | 2763 | `	/*` |
|      - | 2764 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2765 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 2766 | `	 */` |
|      - | 2767 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2768 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 2769 | `		/* Path processing */` |
|      - | 2770 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 2771 | `		{"basename",    PH7_builtin_basename },` |
|      - | 2772 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 2773 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 2774 | `		{"fnmatch",     PH7_builtin_fnmatch  }` |
|      - | 2775 | `	};` |
|  20453 | 2776 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  17045 | 2777 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|   8525 | 2778 | `	}` |
|      - | 2779 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2780 |  |
|      - | 2781 | `	/* Install streams if disk I/O is enabled */` |
|      - | 2782 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 2783 | `#ifdef __WINNT__` |
|      5 | 2784 | `	pFileStream = &sWinFileStream;` |
|      - | 2785 | `#elif defined(__UNIXES__)` |
|   3408 | 2786 | `	pFileStream = &sUnixFileStream;` |
|      - | 2787 | `#endif` |
|      - | 2788 | `	/* Install the php:// stream */` |
|   3413 | 2789 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3413 | 2790 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 2791 | `#ifdef PH7_ENABLE_NET` |
|   3413 | 2792 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 2793 | `#endif` |
|   3413 | 2794 | `	if( pFileStream ){` |
|      - | 2795 | `		/* Install the file:// stream */` |
|   3413 | 2796 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1704 | 2797 | `	}` |
|      - | 2798 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 2799 |  |
|   3413 | 2800 | `	return SXRET_OK;` |
|      5 | 2801 | `}` |
|      - | 2802 |  |
