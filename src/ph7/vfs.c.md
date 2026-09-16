# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 806/1186 lines (67.96%)

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
|  19818 |   90 | `PH7_PRIVATE const char * VfsStrerror(int iErr)` |
|      5 |   91 | `{` |
|      - |   92 | `#if defined(_MSC_VER)` |
|      - |   93 | `#pragma warning(push)` |
|      - |   94 | `#pragma warning(disable:4996)` |
|      - |   95 | `#endif` |
|  19823 |   96 | `	return strerror(iErr);` |
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
|  19794 |  107 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |  108 | `{` |
|  29696 |  109 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  19794 |  110 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  19799 |  111 | `}` |
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
|  12860 |  126 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  127 | `{` |
|      - |  128 | `	const char *zPath;` |
|      - |  129 | `	ph7_vfs *pVfs;` |
|      - |  130 | `	int rc;` |
|      - |  131 | `	/* Only the ARITY is checked here: php coerces a scalar $directory to string,` |
|      - |  132 | `	 * so chdir(123) attempts "123" and warns that it does not exist. Requiring a` |
|      - |  133 | `	 * string outright made that call return FALSE silently, with no diagnostic. */` |
|  12865 |  134 | `	if( nArg < 1 ){` |
|      - |  135 | `		/* Missing argument,return FALSE */` |
|    ! 0 |  136 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  137 | `		return PH7_OK;` |
|      - |  138 | `	}` |
|      - |  139 | `	/* Point to the underlying vfs */` |
|  12865 |  140 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  12865 |  141 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  142 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  143 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  144 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  145 | `			ph7_function_name(pCtx)` |
|      - |  146 | `			);` |
|    ! 0 |  147 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  148 | `		return PH7_OK;` |
|      - |  149 | `	}` |
|      - |  150 | `	/* Point to the desired directory */` |
|  12865 |  151 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  152 | `	/* Perform the requested operation */` |
|  12865 |  153 | `	errno = 0;` |
|  12865 |  154 | `	rc = pVfs->xChdir(zPath);` |
|  12865 |  155 | `	if( rc != PH7_OK ){` |
|      - |  156 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      8 |  157 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      4 |  158 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      2 |  159 | `	}` |
|      - |  160 | `	/* IO return value */` |
|  12865 |  161 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  12865 |  162 | `	return PH7_OK;` |
|   6435 |  163 | `}` |
|      - |  164 | `/*` |
|      - |  165 | ` * bool chroot(string $directory)` |
|      - |  166 | ` *  Change the root directory.` |
|      - |  167 | ` * Parameters` |
|      - |  168 | ` *  $directory` |
|      - |  169 | ` *   The path to change the root directory to` |
|      - |  170 | ` * Return` |
|      - |  171 | ` *  TRUE on success or FALSE on failure.` |
|      - |  172 | ` */` |
|      6 |  173 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  174 | `{` |
|      - |  175 | `	const char *zPath;` |
|      - |  176 | `	ph7_vfs *pVfs;` |
|      - |  177 | `	int rc;` |
|      7 |  178 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  179 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 |  180 | `		ph7_result_bool(pCtx,0);` |
|      5 |  181 | `		return PH7_OK;` |
|      - |  182 | `	}` |
|      - |  183 | `	/* Point to the underlying vfs */` |
|      3 |  184 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  185 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  186 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  187 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  188 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  189 | `			ph7_function_name(pCtx)` |
|      - |  190 | `			);` |
|    ! 0 |  191 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  192 | `		return PH7_OK;` |
|      - |  193 | `	}` |
|      - |  194 | `	/* Point to the desired directory */` |
|      3 |  195 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  196 | `	/* Perform the requested operation */` |
|      3 |  197 | `	rc = pVfs->xChroot(zPath);` |
|      - |  198 | `	/* IO return value */` |
|      3 |  199 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 |  200 | `	return PH7_OK;` |
|      4 |  201 | `}` |
|      - |  202 | `/*` |
|      - |  203 | ` * string getcwd(void)` |
|      - |  204 | ` *  Gets the current working directory.` |
|      - |  205 | ` * Parameters` |
|      - |  206 | ` *  None` |
|      - |  207 | ` * Return` |
|      - |  208 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  209 | ` */` |
|     18 |  210 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  211 | `{` |
|      - |  212 | `	ph7_vfs *pVfs;` |
|      - |  213 | `	int rc;` |
|      - |  214 | `	/* Point to the underlying vfs */` |
|     23 |  215 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     23 |  216 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  217 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  218 | `		SXUNUSED(apArg);` |
|      - |  219 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  220 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  221 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  222 | `			ph7_function_name(pCtx)` |
|      - |  223 | `			);` |
|    ! 0 |  224 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  225 | `		return PH7_OK;` |
|      - |  226 | `	}` |
|     23 |  227 | `	ph7_result_string(pCtx,"",0);` |
|      - |  228 | `	/* Perform the requested operation */` |
|     23 |  229 | `	rc = pVfs->xGetcwd(pCtx);` |
|     23 |  230 | `	if( rc != PH7_OK ){` |
|      - |  231 | `		/* Error,return FALSE */` |
|    ! 0 |  232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  233 | `	}` |
|     23 |  234 | `	return PH7_OK;` |
|     14 |  235 | `}` |
|      - |  236 | `/*` |
|      - |  237 | ` * bool rmdir(string $directory)` |
|      - |  238 | ` *  Removes directory.` |
|      - |  239 | ` * Parameters` |
|      - |  240 | ` *  $directory` |
|      - |  241 | ` *   The path to the directory` |
|      - |  242 | ` * Return` |
|      - |  243 | ` *  TRUE on success or FALSE on failure.` |
|      - |  244 | ` */` |
|     46 |  245 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  246 | `{` |
|      - |  247 | `	const char *zPath;` |
|      - |  248 | `	ph7_vfs *pVfs;` |
|      - |  249 | `	int rc;` |
|     49 |  250 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  251 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  252 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  253 | `		return PH7_OK;` |
|      - |  254 | `	}` |
|      - |  255 | `	/* Point to the underlying vfs */` |
|     49 |  256 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     49 |  257 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  258 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  259 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  260 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  261 | `			ph7_function_name(pCtx)` |
|      - |  262 | `			);` |
|    ! 0 |  263 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  264 | `		return PH7_OK;` |
|      - |  265 | `	}` |
|      - |  266 | `	/* Point to the desired directory */` |
|     49 |  267 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  268 | `	/* Perform the requested operation */` |
|     49 |  269 | `	errno = 0;` |
|     49 |  270 | `	rc = pVfs->xRmdir(zPath);` |
|     49 |  271 | `	if( rc != PH7_OK ){` |
|      8 |  272 | `		VfsThrowSysWarning(pCtx,zPath);` |
|      3 |  273 | `	}` |
|      - |  274 | `	/* IO return value */` |
|     49 |  275 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     49 |  276 | `	return PH7_OK;` |
|     26 |  277 | `}` |
|      - |  278 | `/*` |
|      - |  279 | ` * bool is_dir(string $filename)` |
|      - |  280 | ` *  Tells whether the given filename is a directory.` |
|      - |  281 | ` * Parameters` |
|      - |  282 | ` *  $filename` |
|      - |  283 | ` *   Path to the file.` |
|      - |  284 | ` * Return` |
|      - |  285 | ` *  TRUE on success or FALSE on failure.` |
|      - |  286 | ` */` |
|   8670 |  287 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  288 | `{` |
|      - |  289 | `	const char *zPath;` |
|      - |  290 | `	ph7_vfs *pVfs;` |
|      - |  291 | `	int rc;` |
|   8675 |  292 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  293 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  294 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  295 | `		return PH7_OK;` |
|      - |  296 | `	}` |
|      - |  297 | `	/* Point to the underlying vfs */` |
|   8675 |  298 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8675 |  299 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  300 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  301 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  302 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  303 | `			ph7_function_name(pCtx)` |
|      - |  304 | `			);` |
|    ! 0 |  305 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  306 | `		return PH7_OK;` |
|      - |  307 | `	}` |
|      - |  308 | `	/* Point to the desired directory */` |
|   8675 |  309 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  310 | `	/* Perform the requested operation */` |
|   8675 |  311 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  312 | `	/* IO return value */` |
|   8675 |  313 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8675 |  314 | `	return PH7_OK;` |
|   4340 |  315 | `}` |
|      - |  316 | `/*` |
|      - |  317 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  318 | ` *  Make a directory.` |
|      - |  319 | ` * Parameters` |
|      - |  320 | ` *  $pathname` |
|      - |  321 | ` *   The directory path.` |
|      - |  322 | ` * $mode` |
|      - |  323 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  324 | ` *  Note:` |
|      - |  325 | ` *   mode is ignored on Windows.` |
|      - |  326 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  327 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  328 | ` *   which you can change using umask().` |
|      - |  329 | ` * $recursive` |
|      - |  330 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  331 | ` *  Defaults to FALSE. (Not used)` |
|      - |  332 | ` * Return` |
|      - |  333 | ` *  TRUE on success or FALSE on failure.` |
|      - |  334 | ` */` |
|     46 |  335 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  336 | `{` |
|     49 |  337 | `	int iRecursive = 0;` |
|      - |  338 | `	const char *zPath;` |
|      - |  339 | `	ph7_vfs *pVfs;` |
|      - |  340 | `	int iMode,rc;` |
|     49 |  341 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  342 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  343 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  344 | `		return PH7_OK;` |
|      - |  345 | `	}` |
|      - |  346 | `	/* Point to the underlying vfs */` |
|     49 |  347 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     49 |  348 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  349 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  350 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  351 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  352 | `			ph7_function_name(pCtx)` |
|      - |  353 | `			);` |
|    ! 0 |  354 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  355 | `		return PH7_OK;` |
|      - |  356 | `	}` |
|      - |  357 | `	/* Point to the desired directory */` |
|     49 |  358 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  359 | `#ifdef __WINNT__` |
|      3 |  360 | `	iMode = 0;` |
|      - |  361 | `#else` |
|      - |  362 | `	/* Assume UNIX */` |
|     46 |  363 | `	iMode = 0777;` |
|      - |  364 | `#endif` |
|     49 |  365 | `	if( nArg > 1 ){` |
|    ! 0 |  366 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  367 | `		if( nArg > 2 ){` |
|    ! 0 |  368 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  369 | `		}` |
|    ! 0 |  370 | `	}` |
|      - |  371 | `	/* Perform the requested operation */` |
|     49 |  372 | `	errno = 0;` |
|     49 |  373 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|     49 |  374 | `	if( rc != PH7_OK ){` |
|      - |  375 | `		/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|    ! 0 |  376 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|    ! 0 |  377 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|    ! 0 |  378 | `	}` |
|      - |  379 | `	/* IO return value */` |
|     49 |  380 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     49 |  381 | `	return PH7_OK;` |
|     26 |  382 | `}` |
|      - |  383 | `/*` |
|      - |  384 | ` * bool rename(string $oldname,string $newname)` |
|      - |  385 | ` *  Attempts to rename oldname to newname.` |
|      - |  386 | ` * Parameters` |
|      - |  387 | ` *  $oldname` |
|      - |  388 | ` *   Old name.` |
|      - |  389 | ` *  $newname` |
|      - |  390 | ` *   New name.` |
|      - |  391 | ` * Return` |
|      - |  392 | ` *  TRUE on success or FALSE on failure.` |
|      - |  393 | ` */` |
|      2 |  394 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  395 | `{` |
|      - |  396 | `	const char *zOld,*zNew;` |
|      - |  397 | `	ph7_vfs *pVfs;` |
|      - |  398 | `	int rc;` |
|      3 |  399 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  400 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  401 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  402 | `		return PH7_OK;` |
|      - |  403 | `	}` |
|      - |  404 | `	/* Point to the underlying vfs */` |
|      3 |  405 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  406 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  407 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  408 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  409 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  410 | `			ph7_function_name(pCtx)` |
|      - |  411 | `			);` |
|    ! 0 |  412 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  413 | `		return PH7_OK;` |
|      - |  414 | `	}` |
|      - |  415 | `	/* Perform the requested operation */` |
|      3 |  416 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  417 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  418 | `	errno = 0;` |
|      3 |  419 | `	rc = pVfs->xRename(zOld,zNew);` |
|      3 |  420 | `	if( rc != PH7_OK ){` |
|      - |  421 | `		/* php names BOTH paths here */` |
|    ! 0 |  422 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    ! 0 |  423 | `			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));` |
|    ! 0 |  424 | `	}` |
|      - |  425 | `	/* IO result */` |
|      3 |  426 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  427 | `	return PH7_OK;` |
|      2 |  428 | `}` |
|      - |  429 | `/*` |
|      - |  430 | ` * string realpath(string $path)` |
|      - |  431 | ` *  Returns canonicalized absolute pathname.` |
|      - |  432 | ` * Parameters` |
|      - |  433 | ` *  $path` |
|      - |  434 | ` *   Target path.` |
|      - |  435 | ` * Return` |
|      - |  436 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  437 | ` */` |
|      6 |  438 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  439 | `{` |
|      - |  440 | `	const char *zPath;` |
|      - |  441 | `	ph7_vfs *pVfs;` |
|      - |  442 | `        int rc;` |
|      8 |  443 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  444 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  445 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  446 | `		return PH7_OK;` |
|      - |  447 | `	}` |
|      - |  448 | `	/* Point to the underlying vfs */` |
|      8 |  449 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 |  450 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  451 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  452 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  453 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  454 | `			ph7_function_name(pCtx)` |
|      - |  455 | `			);` |
|    ! 0 |  456 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  457 | `		return PH7_OK;` |
|      - |  458 | `	}` |
|      - |  459 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|      8 |  460 | `	ph7_result_string(pCtx,"",0);` |
|      - |  461 | `	/* Perform the requested operation */` |
|      8 |  462 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      8 |  463 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|      8 |  464 | `	if( rc != PH7_OK ){` |
|      2 |  465 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  466 | `	}` |
|      8 |  467 | `	return PH7_OK;` |
|      5 |  468 | `}` |
|      - |  469 | `/*` |
|      - |  470 | ` * int sleep(int $seconds)` |
|      - |  471 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  472 | ` * Parameters` |
|      - |  473 | ` *  $seconds` |
|      - |  474 | ` *   Halt time in seconds.` |
|      - |  475 | ` * Return` |
|      - |  476 | ` *  Zero on success or FALSE on failure.` |
|      - |  477 | ` */` |
|     10 |  478 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  479 | `{` |
|      - |  480 | `	ph7_vfs *pVfs;` |
|      - |  481 | `	int rc,nSleep;` |
|     11 |  482 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  483 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  485 | `		return PH7_OK;` |
|      - |  486 | `	}` |
|      - |  487 | `	/* Point to the underlying vfs */` |
|     11 |  488 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  489 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  490 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  491 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  492 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  493 | `			ph7_function_name(pCtx)` |
|      - |  494 | `			);` |
|    ! 0 |  495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Amount to sleep */` |
|     11 |  499 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     11 |  500 | `	if( nSleep < 0 ){` |
|      - |  501 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 |  502 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  503 | `			"sleep(): Argument #1 ($seconds) must be greater than or equal to 0");` |
|      - |  504 | `	}` |
|      - |  505 | `	/* Perform the requested operation (Microseconds) */` |
|      9 |  506 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      9 |  507 | `	if( rc != PH7_OK ){` |
|      - |  508 | `		/* Return FALSE */` |
|    ! 0 |  509 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  510 | `	}else{` |
|      - |  511 | `		/* Return zero */` |
|      9 |  512 | `		ph7_result_int(pCtx,0);` |
|      - |  513 | `	}` |
|      9 |  514 | `	return PH7_OK;` |
|      6 |  515 | `}` |
|      - |  516 | `/*` |
|      - |  517 | ` * void usleep(int $micro_seconds)` |
|      - |  518 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  519 | ` * Parameters` |
|      - |  520 | ` *  $micro_seconds` |
|      - |  521 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  522 | ` * Return` |
|      - |  523 | ` *  None.` |
|      - |  524 | ` */` |
|     58 |  525 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  526 | `{` |
|      - |  527 | `	ph7_vfs *pVfs;` |
|      - |  528 | `	int nSleep;` |
|     59 |  529 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  530 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  531 | `		return PH7_OK;` |
|      - |  532 | `	}` |
|      - |  533 | `	/* Point to the underlying vfs */` |
|     59 |  534 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     59 |  535 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  536 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  537 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  538 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  539 | `			ph7_function_name(pCtx)` |
|      - |  540 | `			);` |
|    ! 0 |  541 | `		return PH7_OK;` |
|      - |  542 | `	}` |
|      - |  543 | `	/* Amount to sleep */` |
|     59 |  544 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     59 |  545 | `	if( nSleep < 0 ){` |
|      - |  546 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 |  547 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  548 | `			"usleep(): Argument #1 ($microseconds) must be greater than or equal to 0");` |
|      - |  549 | `	}` |
|      - |  550 | `	/* Perform the requested operation (Microseconds) */` |
|     57 |  551 | `	pVfs->xSleep((unsigned int)nSleep);` |
|     57 |  552 | `	return PH7_OK;` |
|     30 |  553 | `}` |
|      - |  554 | `/*` |
|      - |  555 | ` * bool unlink (string $filename)` |
|      - |  556 | ` *  Delete a file.` |
|      - |  557 | ` * Parameters` |
|      - |  558 | ` *  $filename` |
|      - |  559 | ` *   Path to the file.` |
|      - |  560 | ` * Return` |
|      - |  561 | ` *  TRUE on success or FALSE on failure.` |
|      - |  562 | ` */` |
|  32908 |  563 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  564 | `{` |
|      - |  565 | `	const char *zPath;` |
|      - |  566 | `	ph7_vfs *pVfs;` |
|      - |  567 | `	int rc;` |
|  32913 |  568 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  569 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  570 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  571 | `		return PH7_OK;` |
|      - |  572 | `	}` |
|      - |  573 | `	/* Point to the underlying vfs */` |
|  32913 |  574 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  32913 |  575 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  576 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  577 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  578 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  579 | `			ph7_function_name(pCtx)` |
|      - |  580 | `			);` |
|    ! 0 |  581 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  582 | `		return PH7_OK;` |
|      - |  583 | `	}` |
|      - |  584 | `	/* Point to the desired directory */` |
|  32913 |  585 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  586 | `	/* Perform the requested operation */` |
|  32913 |  587 | `	errno = 0;` |
|  32913 |  588 | `	rc = pVfs->xUnlink(zPath);` |
|  32913 |  589 | `	if( rc != PH7_OK ){` |
|  19793 |  590 | `		VfsThrowSysWarning(pCtx,zPath);` |
|   9894 |  591 | `	}` |
|      - |  592 | `	/* IO return value */` |
|  32913 |  593 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  32913 |  594 | `	return PH7_OK;` |
|  16459 |  595 | `}` |
|      - |  596 | `/*` |
|      - |  597 | ` * bool chmod(string $filename,int $mode)` |
|      - |  598 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - |  599 | ` * Parameters` |
|      - |  600 | ` *  $filename` |
|      - |  601 | ` *   Path to the file.` |
|      - |  602 | ` * $mode` |
|      - |  603 | ` *   Mode (Must be an integer)` |
|      - |  604 | ` * Return` |
|      - |  605 | ` *  TRUE on success or FALSE on failure.` |
|      - |  606 | ` */` |
|    140 |  607 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  608 | `{` |
|      - |  609 | `	const char *zPath;` |
|      - |  610 | `	ph7_vfs *pVfs;` |
|      - |  611 | `	int iMode;` |
|      - |  612 | `	int rc;` |
|    142 |  613 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  614 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  615 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  616 | `		return PH7_OK;` |
|      - |  617 | `	}` |
|      - |  618 | `	/* Point to the underlying vfs */` |
|    142 |  619 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    142 |  620 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  621 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  622 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  623 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  624 | `			ph7_function_name(pCtx)` |
|      - |  625 | `			);` |
|    ! 0 |  626 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  627 | `		return PH7_OK;` |
|      - |  628 | `	}` |
|      - |  629 | `	/* Point to the desired directory */` |
|    142 |  630 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  631 | `	/* Extract the mode */` |
|    142 |  632 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  633 | `	/* Perform the requested operation */` |
|    142 |  634 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  635 | `	/* IO return value */` |
|    142 |  636 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    142 |  637 | `	return PH7_OK;` |
|     72 |  638 | `}` |
|      - |  639 | `/*` |
|      - |  640 | ` * bool chown(string $filename,string $user)` |
|      - |  641 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - |  642 | ` * Parameters` |
|      - |  643 | ` *  $filename` |
|      - |  644 | ` *   Path to the file.` |
|      - |  645 | ` * $user` |
|      - |  646 | ` *   Username.` |
|      - |  647 | ` * Return` |
|      - |  648 | ` *  TRUE on success or FALSE on failure.` |
|      - |  649 | ` */` |
|      6 |  650 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  651 | `{` |
|      - |  652 | `	const char *zPath,*zUser;` |
|      - |  653 | `	ph7_vfs *pVfs;` |
|      - |  654 | `	int rc;` |
|      7 |  655 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  656 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  657 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  658 | `		return PH7_OK;` |
|      - |  659 | `	}` |
|      - |  660 | `	/* Point to the underlying vfs */` |
|      7 |  661 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  662 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - |  663 | `		/* IO routine not implemented,return NULL */` |
|      1 |  664 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  665 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  666 | `			ph7_function_name(pCtx)` |
|      - |  667 | `			);` |
|      1 |  668 | `		ph7_result_bool(pCtx,0);` |
|      1 |  669 | `		return PH7_OK;` |
|      - |  670 | `	}` |
|      - |  671 | `	/* Point to the desired directory */` |
|      6 |  672 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  673 | `	/* Extract the user */` |
|      6 |  674 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - |  675 | `	/* Perform the requested operation */` |
|      6 |  676 | `	errno = 0;` |
|      6 |  677 | `	rc = pVfs->xChown(zPath,zUser);` |
|      6 |  678 | `	if( rc != PH7_OK ){` |
|      - |  679 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  680 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  681 | `		 * "chown(): Operation not permitted". */` |
|      6 |  682 | `		if( rc == -2 ){` |
|      3 |  683 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",` |
|      1 |  684 | `				ph7_function_name(pCtx),zUser);` |
|      1 |  685 | `		}else{` |
|      6 |  686 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  687 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  688 | `		}` |
|      3 |  689 | `	}` |
|      - |  690 | `	/* IO return value */` |
|      6 |  691 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  692 | `	return PH7_OK;` |
|      4 |  693 | `}` |
|      - |  694 | `/*` |
|      - |  695 | ` * bool chgrp(string $filename,string $group)` |
|      - |  696 | ` *  Attempts to change the group of the file filename to group.` |
|      - |  697 | ` * Parameters` |
|      - |  698 | ` *  $filename` |
|      - |  699 | ` *   Path to the file.` |
|      - |  700 | ` * $group` |
|      - |  701 | ` *   groupname.` |
|      - |  702 | ` * Return` |
|      - |  703 | ` *  TRUE on success or FALSE on failure.` |
|      - |  704 | ` */` |
|      6 |  705 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  706 | `{` |
|      - |  707 | `	const char *zPath,*zGroup;` |
|      - |  708 | `	ph7_vfs *pVfs;` |
|      - |  709 | `	int rc;` |
|      7 |  710 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  711 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  712 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  713 | `		return PH7_OK;` |
|      - |  714 | `	}` |
|      - |  715 | `	/* Point to the underlying vfs */` |
|      7 |  716 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  717 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - |  718 | `		/* IO routine not implemented,return NULL */` |
|      1 |  719 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  720 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  721 | `			ph7_function_name(pCtx)` |
|      - |  722 | `			);` |
|      1 |  723 | `		ph7_result_bool(pCtx,0);` |
|      1 |  724 | `		return PH7_OK;` |
|      - |  725 | `	}` |
|      - |  726 | `	/* Point to the desired directory */` |
|      6 |  727 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  728 | `	/* Extract the user */` |
|      6 |  729 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - |  730 | `	/* Perform the requested operation */` |
|      6 |  731 | `	errno = 0;` |
|      6 |  732 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      6 |  733 | `	if( rc != PH7_OK ){` |
|      - |  734 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  735 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  736 | `		 * "chown(): Operation not permitted". */` |
|      6 |  737 | `		if( rc == -2 ){` |
|      3 |  738 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",` |
|      1 |  739 | `				ph7_function_name(pCtx),zGroup);` |
|      1 |  740 | `		}else{` |
|      6 |  741 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  742 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  743 | `		}` |
|      3 |  744 | `	}` |
|      - |  745 | `	/* IO return value */` |
|      6 |  746 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  747 | `	return PH7_OK;` |
|      4 |  748 | `}` |
|      - |  749 | `/*` |
|      - |  750 | ` * int64 disk_free_space(string $directory)` |
|      - |  751 | ` *  Returns available space on filesystem or disk partition.` |
|      - |  752 | ` * Parameters` |
|      - |  753 | ` *  $directory` |
|      - |  754 | ` *   A directory of the filesystem or disk partition.` |
|      - |  755 | ` * Return` |
|      - |  756 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  757 | ` */` |
|      4 |  758 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  759 | `{` |
|      - |  760 | `	const char *zPath;` |
|      - |  761 | `	ph7_int64 iSize;` |
|      - |  762 | `	ph7_vfs *pVfs;` |
|      5 |  763 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  764 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  765 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  766 | `		return PH7_OK;` |
|      - |  767 | `	}` |
|      - |  768 | `	/* Point to the underlying vfs */` |
|      5 |  769 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  770 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - |  771 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  772 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  773 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  774 | `			ph7_function_name(pCtx)` |
|      - |  775 | `			);` |
|    ! 0 |  776 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  777 | `		return PH7_OK;` |
|      - |  778 | `	}` |
|      - |  779 | `	/* Point to the desired directory */` |
|      5 |  780 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  781 | `	/* Perform the requested operation */` |
|      5 |  782 | `	iSize = pVfs->xFreeSpace(zPath);` |
|      - |  783 | `	/* IO return value */` |
|      5 |  784 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  785 | `	return PH7_OK;` |
|      3 |  786 | `}` |
|      - |  787 | `/*` |
|      - |  788 | ` * int64 disk_total_space(string $directory)` |
|      - |  789 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - |  790 | ` * Parameters` |
|      - |  791 | ` *  $directory` |
|      - |  792 | ` *   A directory of the filesystem or disk partition.` |
|      - |  793 | ` * Return` |
|      - |  794 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  795 | ` */` |
|      4 |  796 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  797 | `{` |
|      - |  798 | `	const char *zPath;` |
|      - |  799 | `	ph7_int64 iSize;` |
|      - |  800 | `	ph7_vfs *pVfs;` |
|      5 |  801 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  802 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  803 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  804 | `		return PH7_OK;` |
|      - |  805 | `	}` |
|      - |  806 | `	/* Point to the underlying vfs */` |
|      5 |  807 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  808 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - |  809 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  810 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  811 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  812 | `			ph7_function_name(pCtx)` |
|      - |  813 | `			);` |
|    ! 0 |  814 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  815 | `		return PH7_OK;` |
|      - |  816 | `	}` |
|      - |  817 | `	/* Point to the desired directory */` |
|      5 |  818 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  819 | `	/* Perform the requested operation */` |
|      5 |  820 | `	iSize = pVfs->xTotalSpace(zPath);` |
|      - |  821 | `	/* IO return value */` |
|      5 |  822 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  823 | `	return PH7_OK;` |
|      3 |  824 | `}` |
|      - |  825 | `/*` |
|      - |  826 | ` * bool file_exists(string $filename)` |
|      - |  827 | ` *  Checks whether a file or directory exists.` |
|      - |  828 | ` * Parameters` |
|      - |  829 | ` *  $filename` |
|      - |  830 | ` *   Path to the file.` |
|      - |  831 | ` * Return` |
|      - |  832 | ` *  TRUE on success or FALSE on failure.` |
|      - |  833 | ` */` |
|    160 |  834 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  835 | `{` |
|      - |  836 | `	const char *zPath;` |
|      - |  837 | `	ph7_vfs *pVfs;` |
|      - |  838 | `	int rc;` |
|    162 |  839 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  840 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  841 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  842 | `		return PH7_OK;` |
|      - |  843 | `	}` |
|      - |  844 | `	/* Point to the underlying vfs */` |
|    162 |  845 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    162 |  846 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  847 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  848 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  849 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  850 | `			ph7_function_name(pCtx)` |
|      - |  851 | `			);` |
|    ! 0 |  852 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  853 | `		return PH7_OK;` |
|      - |  854 | `	}` |
|      - |  855 | `	/* Point to the desired directory */` |
|    162 |  856 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  857 | `	/* Perform the requested operation */` |
|    162 |  858 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  859 | `	/* IO return value */` |
|    162 |  860 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    162 |  861 | `	return PH7_OK;` |
|     82 |  862 | `}` |
|      - |  863 | `/*` |
|      - |  864 | ` * int64 file_size(string $filename)` |
|      - |  865 | ` *  Gets the size for the given file.` |
|      - |  866 | ` * Parameters` |
|      - |  867 | ` *  $filename` |
|      - |  868 | ` *   Path to the file.` |
|      - |  869 | ` * Return` |
|      - |  870 | ` *  File size on success or FALSE on failure.` |
|      - |  871 | ` */` |
|     10 |  872 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  873 | `{` |
|      - |  874 | `	const char *zPath;` |
|      - |  875 | `	ph7_int64 iSize;` |
|      - |  876 | `	ph7_vfs *pVfs;` |
|     11 |  877 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  878 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  879 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  880 | `		return PH7_OK;` |
|      - |  881 | `	}` |
|      - |  882 | `	/* Point to the underlying vfs */` |
|     11 |  883 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  884 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - |  885 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  886 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  887 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  888 | `			ph7_function_name(pCtx)` |
|      - |  889 | `			);` |
|    ! 0 |  890 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  891 | `		return PH7_OK;` |
|      - |  892 | `	}` |
|      - |  893 | `	/* Point to the desired directory */` |
|     11 |  894 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  895 | `	/* Perform the requested operation */` |
|     11 |  896 | `	iSize = pVfs->xFileSize(zPath);` |
|     11 |  897 | `	if( iSize < 0 ){` |
|      - |  898 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - |  899 | `		 * truthy and compares equal to nothing a caller would test for. */` |
|    ! 0 |  900 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): stat failed for %s",` |
|    ! 0 |  901 | `			ph7_function_name(pCtx),zPath);` |
|    ! 0 |  902 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  903 | `		return PH7_OK;` |
|      - |  904 | `	}` |
|      - |  905 | `	/* IO return value */` |
|     11 |  906 | `	ph7_result_int64(pCtx,iSize);` |
|     11 |  907 | `	return PH7_OK;` |
|      6 |  908 | `}` |
|      - |  909 | `/*` |
|      - |  910 | ` * int64 fileatime(string $filename)` |
|      - |  911 | ` *  Gets the last access time of the given file.` |
|      - |  912 | ` * Parameters` |
|      - |  913 | ` *  $filename` |
|      - |  914 | ` *   Path to the file.` |
|      - |  915 | ` * Return` |
|      - |  916 | ` *  File atime on success or FALSE on failure.` |
|      - |  917 | ` */` |
|      2 |  918 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  919 | `{` |
|      - |  920 | `	const char *zPath;` |
|      - |  921 | `	ph7_int64 iTime;` |
|      - |  922 | `	ph7_vfs *pVfs;` |
|      3 |  923 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  924 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  925 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  926 | `		return PH7_OK;` |
|      - |  927 | `	}` |
|      - |  928 | `	/* Point to the underlying vfs */` |
|      3 |  929 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  930 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - |  931 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  932 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  933 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  934 | `			ph7_function_name(pCtx)` |
|      - |  935 | `			);` |
|    ! 0 |  936 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  937 | `		return PH7_OK;` |
|      - |  938 | `	}` |
|      - |  939 | `	/* Point to the desired directory */` |
|      3 |  940 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  941 | `	/* Perform the requested operation */` |
|      3 |  942 | `	iTime = pVfs->xFileAtime(zPath);` |
|      - |  943 | `	/* IO return value */` |
|      3 |  944 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  945 | `	return PH7_OK;` |
|      2 |  946 | `}` |
|      - |  947 | `/*` |
|      - |  948 | ` * int64 filemtime(string $filename)` |
|      - |  949 | ` *  Gets file modification time.` |
|      - |  950 | ` * Parameters` |
|      - |  951 | ` *  $filename` |
|      - |  952 | ` *   Path to the file.` |
|      - |  953 | ` * Return` |
|      - |  954 | ` *  File mtime on success or FALSE on failure.` |
|      - |  955 | ` */` |
|      4 |  956 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  957 | `{` |
|      - |  958 | `	const char *zPath;` |
|      - |  959 | `	ph7_int64 iTime;` |
|      - |  960 | `	ph7_vfs *pVfs;` |
|      5 |  961 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  962 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  963 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  964 | `		return PH7_OK;` |
|      - |  965 | `	}` |
|      - |  966 | `	/* Point to the underlying vfs */` |
|      5 |  967 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  968 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - |  969 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  970 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  971 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  972 | `			ph7_function_name(pCtx)` |
|      - |  973 | `			);` |
|    ! 0 |  974 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  975 | `		return PH7_OK;` |
|      - |  976 | `	}` |
|      - |  977 | `	/* Point to the desired directory */` |
|      5 |  978 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  979 | `	/* Perform the requested operation */` |
|      5 |  980 | `	iTime = pVfs->xFileMtime(zPath);` |
|      - |  981 | `	/* IO return value */` |
|      5 |  982 | `	ph7_result_int64(pCtx,iTime);` |
|      5 |  983 | `	return PH7_OK;` |
|      3 |  984 | `}` |
|      - |  985 | `/*` |
|      - |  986 | ` * int64 filectime(string $filename)` |
|      - |  987 | ` *  Gets inode change time of file.` |
|      - |  988 | ` * Parameters` |
|      - |  989 | ` *  $filename` |
|      - |  990 | ` *   Path to the file.` |
|      - |  991 | ` * Return` |
|      - |  992 | ` *  File ctime on success or FALSE on failure.` |
|      - |  993 | ` */` |
|      2 |  994 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  995 | `{` |
|      - |  996 | `	const char *zPath;` |
|      - |  997 | `	ph7_int64 iTime;` |
|      - |  998 | `	ph7_vfs *pVfs;` |
|      3 |  999 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1000 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1001 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1002 | `		return PH7_OK;` |
|      - | 1003 | `	}` |
|      - | 1004 | `	/* Point to the underlying vfs */` |
|      3 | 1005 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1006 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - | 1007 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1008 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1009 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1010 | `			ph7_function_name(pCtx)` |
|      - | 1011 | `			);` |
|    ! 0 | 1012 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1013 | `		return PH7_OK;` |
|      - | 1014 | `	}` |
|      - | 1015 | `	/* Point to the desired directory */` |
|      3 | 1016 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1017 | `	/* Perform the requested operation */` |
|      3 | 1018 | `	iTime = pVfs->xFileCtime(zPath);` |
|      - | 1019 | `	/* IO return value */` |
|      3 | 1020 | `	ph7_result_int64(pCtx,iTime);` |
|      3 | 1021 | `	return PH7_OK;` |
|      2 | 1022 | `}` |
|      - | 1023 | `/*` |
|      - | 1024 | ` * bool is_file(string $filename)` |
|      - | 1025 | ` *  Tells whether the filename is a regular file.` |
|      - | 1026 | ` * Parameters` |
|      - | 1027 | ` *  $filename` |
|      - | 1028 | ` *   Path to the file.` |
|      - | 1029 | ` * Return` |
|      - | 1030 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1031 | ` */` |
|   6612 | 1032 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1033 | `{` |
|      - | 1034 | `	const char *zPath;` |
|      - | 1035 | `	ph7_vfs *pVfs;` |
|      - | 1036 | `	int rc;` |
|   6617 | 1037 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1038 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1039 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1040 | `		return PH7_OK;` |
|      - | 1041 | `	}` |
|      - | 1042 | `	/* Point to the underlying vfs */` |
|   6617 | 1043 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6617 | 1044 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1045 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1046 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1047 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1048 | `			ph7_function_name(pCtx)` |
|      - | 1049 | `			);` |
|    ! 0 | 1050 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1051 | `		return PH7_OK;` |
|      - | 1052 | `	}` |
|      - | 1053 | `	/* Point to the desired directory */` |
|   6617 | 1054 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1055 | `	/* Perform the requested operation */` |
|   6617 | 1056 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1057 | `	/* IO return value */` |
|   6617 | 1058 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6617 | 1059 | `	return PH7_OK;` |
|   3311 | 1060 | `}` |
|      - | 1061 | `/*` |
|      - | 1062 | ` * bool is_link(string $filename)` |
|      - | 1063 | ` *  Tells whether the filename is a symbolic link.` |
|      - | 1064 | ` * Parameters` |
|      - | 1065 | ` *  $filename` |
|      - | 1066 | ` *   Path to the file.` |
|      - | 1067 | ` * Return` |
|      - | 1068 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1069 | ` */` |
|      4 | 1070 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1071 | `{` |
|      - | 1072 | `	const char *zPath;` |
|      - | 1073 | `	ph7_vfs *pVfs;` |
|      - | 1074 | `	int rc;` |
|      4 | 1075 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1076 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1077 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1078 | `		return PH7_OK;` |
|      - | 1079 | `	}` |
|      - | 1080 | `	/* Point to the underlying vfs */` |
|      4 | 1081 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 | 1082 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - | 1083 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1084 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1085 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1086 | `			ph7_function_name(pCtx)` |
|      - | 1087 | `			);` |
|    ! 0 | 1088 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1089 | `		return PH7_OK;` |
|      - | 1090 | `	}` |
|      - | 1091 | `	/* Point to the desired directory */` |
|      4 | 1092 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1093 | `	/* Perform the requested operation */` |
|      4 | 1094 | `	rc = pVfs->xIslink(zPath);` |
|      - | 1095 | `	/* IO return value */` |
|      4 | 1096 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 1097 | `	return PH7_OK;` |
|      2 | 1098 | `}` |
|      - | 1099 | `/*` |
|      - | 1100 | ` * bool is_readable(string $filename)` |
|      - | 1101 | ` *  Tells whether a file exists and is readable.` |
|      - | 1102 | ` * Parameters` |
|      - | 1103 | ` *  $filename` |
|      - | 1104 | ` *   Path to the file.` |
|      - | 1105 | ` * Return` |
|      - | 1106 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1107 | ` */` |
|      2 | 1108 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1109 | `{` |
|      - | 1110 | `	const char *zPath;` |
|      - | 1111 | `	ph7_vfs *pVfs;` |
|      - | 1112 | `	int rc;` |
|      3 | 1113 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1114 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1115 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1116 | `		return PH7_OK;` |
|      - | 1117 | `	}` |
|      - | 1118 | `	/* Point to the underlying vfs */` |
|      3 | 1119 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1120 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - | 1121 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1122 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1123 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1124 | `			ph7_function_name(pCtx)` |
|      - | 1125 | `			);` |
|    ! 0 | 1126 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1127 | `		return PH7_OK;` |
|      - | 1128 | `	}` |
|      - | 1129 | `	/* Point to the desired directory */` |
|      3 | 1130 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1131 | `	/* Perform the requested operation */` |
|      3 | 1132 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1133 | `	/* IO return value */` |
|      3 | 1134 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1135 | `	return PH7_OK;` |
|      2 | 1136 | `}` |
|      - | 1137 | `/*` |
|      - | 1138 | ` * bool is_writable(string $filename)` |
|      - | 1139 | ` *  Tells whether the filename is writable.` |
|      - | 1140 | ` * Parameters` |
|      - | 1141 | ` *  $filename` |
|      - | 1142 | ` *   Path to the file.` |
|      - | 1143 | ` * Return` |
|      - | 1144 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1145 | ` */` |
|      4 | 1146 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1147 | `{` |
|      - | 1148 | `	const char *zPath;` |
|      - | 1149 | `	ph7_vfs *pVfs;` |
|      - | 1150 | `	int rc;` |
|      5 | 1151 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1152 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1153 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1154 | `		return PH7_OK;` |
|      - | 1155 | `	}` |
|      - | 1156 | `	/* Point to the underlying vfs */` |
|      5 | 1157 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1158 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1159 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1160 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1161 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1162 | `			ph7_function_name(pCtx)` |
|      - | 1163 | `			);` |
|    ! 0 | 1164 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1165 | `		return PH7_OK;` |
|      - | 1166 | `	}` |
|      - | 1167 | `	/* Point to the desired directory */` |
|      5 | 1168 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1169 | `	/* Perform the requested operation */` |
|      5 | 1170 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1171 | `	/* IO return value */` |
|      5 | 1172 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1173 | `	return PH7_OK;` |
|      3 | 1174 | `}` |
|      - | 1175 | `/*` |
|      - | 1176 | ` * bool is_executable(string $filename)` |
|      - | 1177 | ` *  Tells whether the filename is executable.` |
|      - | 1178 | ` * Parameters` |
|      - | 1179 | ` *  $filename` |
|      - | 1180 | ` *   Path to the file.` |
|      - | 1181 | ` * Return` |
|      - | 1182 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1183 | ` */` |
|      2 | 1184 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1185 | `{` |
|      - | 1186 | `	const char *zPath;` |
|      - | 1187 | `	ph7_vfs *pVfs;` |
|      - | 1188 | `	int rc;` |
|      3 | 1189 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1190 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1191 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1192 | `		return PH7_OK;` |
|      - | 1193 | `	}` |
|      - | 1194 | `	/* Point to the underlying vfs */` |
|      3 | 1195 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1196 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1197 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1198 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1199 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1200 | `			ph7_function_name(pCtx)` |
|      - | 1201 | `			);` |
|    ! 0 | 1202 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1203 | `		return PH7_OK;` |
|      - | 1204 | `	}` |
|      - | 1205 | `	/* Point to the desired directory */` |
|      3 | 1206 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1207 | `	/* Perform the requested operation */` |
|      3 | 1208 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1209 | `	/* IO return value */` |
|      3 | 1210 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1211 | `	return PH7_OK;` |
|      2 | 1212 | `}` |
|      - | 1213 | `/*` |
|      - | 1214 | ` * string filetype(string $filename)` |
|      - | 1215 | ` *  Gets file type.` |
|      - | 1216 | ` * Parameters` |
|      - | 1217 | ` *  $filename` |
|      - | 1218 | ` *   Path to the file.` |
|      - | 1219 | ` * Return` |
|      - | 1220 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1221 | ` *  file, socket and unknown.` |
|      - | 1222 | ` */` |
|      4 | 1223 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1224 | `{` |
|      - | 1225 | `	const char *zPath;` |
|      - | 1226 | `	ph7_vfs *pVfs;` |
|      5 | 1227 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1228 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1229 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1230 | `		return PH7_OK;` |
|      - | 1231 | `	}` |
|      - | 1232 | `	/* Point to the underlying vfs */` |
|      5 | 1233 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1234 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1235 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1236 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1237 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1238 | `			ph7_function_name(pCtx)` |
|      - | 1239 | `			);` |
|    ! 0 | 1240 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1241 | `		return PH7_OK;` |
|      - | 1242 | `	}` |
|      - | 1243 | `	/* Point to the desired directory */` |
|      5 | 1244 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1245 | `	/* Set the empty string as the default return value */` |
|      5 | 1246 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1247 | `	/* Perform the requested operation */` |
|      5 | 1248 | `	pVfs->xFiletype(zPath,pCtx);` |
|      5 | 1249 | `	return PH7_OK;` |
|      3 | 1250 | `}` |
|      - | 1251 | `/*` |
|      - | 1252 | ` * array stat(string $filename)` |
|      - | 1253 | ` *  Gives information about a file.` |
|      - | 1254 | ` * Parameters` |
|      - | 1255 | ` *  $filename` |
|      - | 1256 | ` *   Path to the file.` |
|      - | 1257 | ` * Return` |
|      - | 1258 | ` *  An associative array on success holding the following entries on success` |
|      - | 1259 | ` *  0   dev     device number` |
|      - | 1260 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1261 | ` * 2    mode    inode protection mode` |
|      - | 1262 | ` * 3    nlink   number of links` |
|      - | 1263 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1264 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1265 | ` * 6    rdev    device type, if inode device` |
|      - | 1266 | ` * 7    size    size in bytes` |
|      - | 1267 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1268 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1269 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1270 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1271 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1272 | ` * Note:` |
|      - | 1273 | ` *  FALSE is returned on failure.` |
|      - | 1274 | ` */` |
|     10 | 1275 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1276 | `{` |
|      - | 1277 | `	ph7_value *pArray,*pValue;` |
|      - | 1278 | `	const char *zPath;` |
|      - | 1279 | `	ph7_vfs *pVfs;` |
|      - | 1280 | `	int rc;` |
|     11 | 1281 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1282 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1283 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1284 | `		return PH7_OK;` |
|      - | 1285 | `	}` |
|      - | 1286 | `	/* Point to the underlying vfs */` |
|     11 | 1287 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 1288 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1289 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1290 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1291 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1292 | `			ph7_function_name(pCtx)` |
|      - | 1293 | `			);` |
|    ! 0 | 1294 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1295 | `		return PH7_OK;` |
|      - | 1296 | `	}` |
|      - | 1297 | `	/* Create the array and the working value */` |
|     11 | 1298 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 1299 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 1300 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1301 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1302 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1303 | `		return PH7_OK;` |
|      - | 1304 | `	}` |
|      - | 1305 | `	/* Extract the file path */` |
|     11 | 1306 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1307 | `	/* Perform the requested operation */` |
|     11 | 1308 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     11 | 1309 | `	if( rc != PH7_OK ){` |
|      - | 1310 | `		/* IO error,return FALSE */` |
|    ! 0 | 1311 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1312 | `	}else{` |
|      - | 1313 | `		/* Return the associative array */` |
|     11 | 1314 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1315 | `	}` |
|      - | 1316 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1317 | `	 * automatically as soon we return from this function. */` |
|     11 | 1318 | `	return PH7_OK;` |
|      6 | 1319 | `}` |
|      - | 1320 | `/*` |
|      - | 1321 | ` * array lstat(string $filename)` |
|      - | 1322 | ` *  Gives information about a file or symbolic link.` |
|      - | 1323 | ` * Parameters` |
|      - | 1324 | ` *  $filename` |
|      - | 1325 | ` *   Path to the file.` |
|      - | 1326 | ` * Return` |
|      - | 1327 | ` *  An associative array on success holding the following entries on success` |
|      - | 1328 | ` *  0   dev     device number` |
|      - | 1329 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1330 | ` * 2    mode    inode protection mode` |
|      - | 1331 | ` * 3    nlink   number of links` |
|      - | 1332 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1333 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1334 | ` * 6    rdev    device type, if inode device` |
|      - | 1335 | ` * 7    size    size in bytes` |
|      - | 1336 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1337 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1338 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1339 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1340 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1341 | ` * Note:` |
|      - | 1342 | ` *  FALSE is returned on failure.` |
|      - | 1343 | ` */` |
|      2 | 1344 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1345 | `{` |
|      - | 1346 | `	ph7_value *pArray,*pValue;` |
|      - | 1347 | `	const char *zPath;` |
|      - | 1348 | `	ph7_vfs *pVfs;` |
|      - | 1349 | `	int rc;` |
|      3 | 1350 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1351 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1352 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1353 | `		return PH7_OK;` |
|      - | 1354 | `	}` |
|      - | 1355 | `	/* Point to the underlying vfs */` |
|      3 | 1356 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1357 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1358 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1359 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1360 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1361 | `			ph7_function_name(pCtx)` |
|      - | 1362 | `			);` |
|    ! 0 | 1363 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1364 | `		return PH7_OK;` |
|      - | 1365 | `	}` |
|      - | 1366 | `	/* Create the array and the working value */` |
|      3 | 1367 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 1368 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1369 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1370 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1371 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1372 | `		return PH7_OK;` |
|      - | 1373 | `	}` |
|      - | 1374 | `	/* Extract the file path */` |
|      3 | 1375 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1376 | `	/* Perform the requested operation */` |
|      3 | 1377 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      3 | 1378 | `	if( rc != PH7_OK ){` |
|      - | 1379 | `		/* IO error,return FALSE */` |
|    ! 0 | 1380 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1381 | `	}else{` |
|      - | 1382 | `		/* Return the associative array */` |
|      3 | 1383 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1384 | `	}` |
|      - | 1385 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1386 | `	 * automatically as soon we return from this function. */` |
|      3 | 1387 | `	return PH7_OK;` |
|      2 | 1388 | `}` |
|      - | 1389 | `/*` |
|      - | 1390 | ` * string getenv(string $varname)` |
|      - | 1391 | ` *  Gets the value of an environment variable.` |
|      - | 1392 | ` * Parameters` |
|      - | 1393 | ` *  $varname` |
|      - | 1394 | ` *   The variable name.` |
|      - | 1395 | ` * Return` |
|      - | 1396 | ` *  Returns the value of the environment variable varname, or FALSE if the environment` |
|      - | 1397 | ` * variable varname does not exist.` |
|      - | 1398 | ` */` |
|     56 | 1399 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1400 | `{` |
|      - | 1401 | `	const char *zEnv;` |
|      - | 1402 | `	ph7_vfs *pVfs;` |
|      - | 1403 | `	int iLen;` |
|     60 | 1404 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1405 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1406 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1407 | `		return PH7_OK;` |
|      - | 1408 | `	}` |
|      - | 1409 | `	/* Point to the underlying vfs */` |
|     60 | 1410 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     60 | 1411 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1412 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1413 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1414 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1415 | `			ph7_function_name(pCtx)` |
|      - | 1416 | `			);` |
|    ! 0 | 1417 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1418 | `		return PH7_OK;` |
|      - | 1419 | `	}` |
|      - | 1420 | `	/* Extract the environment variable */` |
|     60 | 1421 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1422 | `	/* Set a boolean FALSE as the default return value */` |
|     60 | 1423 | `	ph7_result_bool(pCtx,0);` |
|     60 | 1424 | `	if( iLen < 1 ){` |
|      - | 1425 | `		/* Empty string */` |
|    ! 0 | 1426 | `		return PH7_OK;` |
|      - | 1427 | `	}` |
|      - | 1428 | `	/* Perform the requested operation */` |
|     60 | 1429 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     60 | 1430 | `	return PH7_OK;` |
|     32 | 1431 | `}` |
|      - | 1432 | `/*` |
|      - | 1433 | ` * bool putenv(string $settings)` |
|      - | 1434 | ` *  Set the value of an environment variable.` |
|      - | 1435 | ` * Parameters` |
|      - | 1436 | ` *  $setting` |
|      - | 1437 | ` *   The setting, like "FOO=BAR"` |
|      - | 1438 | ` * Return` |
|      - | 1439 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1440 | ` */` |
|      6 | 1441 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1442 | `{` |
|      - | 1443 | `	const char *zName,*zValue;` |
|      - | 1444 | `	char *zSettings,*zEnd;` |
|      - | 1445 | `	ph7_vfs *pVfs;` |
|      - | 1446 | `	int iLen,rc;` |
|      7 | 1447 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1448 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1449 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1450 | `		return PH7_OK;` |
|      - | 1451 | `	}` |
|      - | 1452 | `	/* Extract the setting variable */` |
|      7 | 1453 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|      7 | 1454 | `	if( iLen < 1 ){` |
|      - | 1455 | `		/* Empty string,return FALSE */` |
|    ! 0 | 1456 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1457 | `		return PH7_OK;` |
|      - | 1458 | `	}` |
|      - | 1459 | `	/* Parse the setting */` |
|      7 | 1460 | `	zEnd = &zSettings[iLen];` |
|      7 | 1461 | `	zValue = 0;` |
|      7 | 1462 | `	zName = zSettings;` |
|    127 | 1463 | `	while( zSettings < zEnd ){` |
|    127 | 1464 | `		if( zSettings[0] == '=' ){` |
|      - | 1465 | `			/* Null terminate the name */` |
|      7 | 1466 | `			zSettings[0] = 0;` |
|      7 | 1467 | `			zValue = &zSettings[1];` |
|      7 | 1468 | `			break;` |
|      - | 1469 | `		}` |
|    121 | 1470 | `		zSettings++;` |
|      1 | 1471 | `	}` |
|      - | 1472 | `	/* Install the environment variable in the $_Env array */` |
|      7 | 1473 | `	if( zValue == 0 \|\| zName[0] == 0 \|\| zValue >= zEnd \|\| zName >= zValue ){` |
|      - | 1474 | `		/* Invalid settings,retun FALSE */` |
|      5 | 1475 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1476 | `		if( zSettings  < zEnd ){` |
|      5 | 1477 | `			zSettings[0] = '=';` |
|      2 | 1478 | `		}` |
|      5 | 1479 | `		return PH7_OK;` |
|      - | 1480 | `	}` |
|      3 | 1481 | `	ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_ENV_ATTR,zName,zValue,(int)(zEnd-zValue));` |
|      - | 1482 | `	/* Point to the underlying vfs */` |
|      3 | 1483 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1484 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 1485 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1486 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1487 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1488 | `			ph7_function_name(pCtx)` |
|      - | 1489 | `			);` |
|    ! 0 | 1490 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1491 | `		zSettings[0] = '=';` |
|    ! 0 | 1492 | `		return PH7_OK;` |
|      - | 1493 | `	}` |
|      - | 1494 | `	/* Perform the requested operation */` |
|      3 | 1495 | `	rc = pVfs->xSetenv(zName,zValue);` |
|      3 | 1496 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 1497 | `	zSettings[0] = '=';` |
|      3 | 1498 | `	return PH7_OK;` |
|      4 | 1499 | `}` |
|      - | 1500 | `/*` |
|      - | 1501 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 1502 | ` *  Sets access and modification time of file.` |
|      - | 1503 | ` * Note: On windows` |
|      - | 1504 | ` *   If the file does not exists,it will not be created.` |
|      - | 1505 | ` * Parameters` |
|      - | 1506 | ` *  $filename` |
|      - | 1507 | ` *   The name of the file being touched.` |
|      - | 1508 | ` *  $time` |
|      - | 1509 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 1510 | ` * $atime` |
|      - | 1511 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 1512 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 1513 | ` *   present, the current system time is used.` |
|      - | 1514 | ` * Return` |
|      - | 1515 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1516 | `*/` |
|      4 | 1517 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1518 | `{` |
|      - | 1519 | `	ph7_int64 nTime,nAccess;` |
|      - | 1520 | `	const char *zFile;` |
|      - | 1521 | `	ph7_vfs *pVfs;` |
|      - | 1522 | `	int rc;` |
|      5 | 1523 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1524 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1525 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1526 | `		return PH7_OK;` |
|      - | 1527 | `	}` |
|      - | 1528 | `	/* Point to the underlying vfs */` |
|      5 | 1529 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1530 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 1531 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1532 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1533 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1534 | `			ph7_function_name(pCtx)` |
|      - | 1535 | `			);` |
|    ! 0 | 1536 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1537 | `		return PH7_OK;` |
|      - | 1538 | `	}` |
|      - | 1539 | `	/* Perform the requested operation */` |
|      5 | 1540 | `	nTime = nAccess = -1;` |
|      5 | 1541 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|      5 | 1542 | `	if( nArg > 1 ){` |
|      2 | 1543 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      2 | 1544 | `		if( nArg > 2 ){` |
|      2 | 1545 | `			nAccess = ph7_value_to_int64(apArg[1]);` |
|      1 | 1546 | `		}else{` |
|    ! 0 | 1547 | `			nAccess = nTime;` |
|      - | 1548 | `		}` |
|      1 | 1549 | `	}` |
|      5 | 1550 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 1551 | `	/* IO result */` |
|      5 | 1552 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1553 | `	return PH7_OK;` |
|      3 | 1554 | `}` |
|      - | 1555 | `/*` |
|      - | 1556 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 1557 | ` * Status:` |
|      - | 1558 | ` *    Stable.` |
|      - | 1559 | ` */` |
|      - | 1560 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1561 | `/*` |
|      - | 1562 | ` * string dirname(string $path)` |
|      - | 1563 |  |
|      - | 1564 | ` *  Returns parent directory's path.` |
|      - | 1565 | ` * Parameters` |
|      - | 1566 | ` * $path` |
|      - | 1567 | ` *  Target path.` |
|      - | 1568 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1569 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1570 | ` * Return` |
|      - | 1571 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 1572 | ` *  is returned, indicating the current directory.` |
|      - | 1573 | ` */` |
|     38 | 1574 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1575 | `{` |
|      - | 1576 | `	const char *zPath,*zDir;` |
|      - | 1577 | `	int iLen,iDirlen;` |
|     43 | 1578 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1579 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 1580 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1581 | `		return PH7_OK;` |
|      - | 1582 | `	}` |
|      - | 1583 | `	/* Point to the target path */` |
|     43 | 1584 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     43 | 1585 | `	if( iLen < 1 ){` |
|      - | 1586 | `		/* php answers "" for the empty path, not "." */` |
|      3 | 1587 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1588 | `		return PH7_OK;` |
|      - | 1589 | `	}` |
|      - | 1590 | `	/* Perform the requested operation */` |
|     41 | 1591 | `	zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1592 | `	/* Return directory name */` |
|     41 | 1593 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     41 | 1594 | `	return PH7_OK;` |
|     24 | 1595 | `}` |
|      - | 1596 | `/*` |
|      - | 1597 | ` * string basename(string $path[, string $suffix ])` |
|      - | 1598 | ` *  Returns trailing name component of path.` |
|      - | 1599 | ` * Parameters` |
|      - | 1600 | ` * $path` |
|      - | 1601 | ` *  Target path.` |
|      - | 1602 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1603 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1604 | ` * $suffix` |
|      - | 1605 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 1606 | ` * Return` |
|      - | 1607 | ` *  The base name of the given path.` |
|      - | 1608 | ` */` |
|     46 | 1609 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1610 | `{` |
|      - | 1611 | `	const char *zPath,*zBase,*zEnd;` |
|      - | 1612 | `	int c,d,iLen;` |
|     47 | 1613 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1614 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1615 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1616 | `		return PH7_OK;` |
|      - | 1617 | `	}` |
|     47 | 1618 | `	c = d = '/';` |
|      - | 1619 | `#ifdef __WINNT__` |
|      1 | 1620 | `	d = '\\';` |
|      - | 1621 | `#endif` |
|      - | 1622 | `	/* Point to the target path */` |
|     47 | 1623 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     47 | 1624 | `	if( iLen < 1 ){` |
|      - | 1625 | `		/* Empty string */` |
|      3 | 1626 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1627 | `		return PH7_OK;` |
|      - | 1628 | `	}` |
|      - | 1629 | `	/* Perform the requested operation */` |
|     45 | 1630 | `	zEnd = &zPath[iLen - 1];` |
|      - | 1631 | `	/* Ignore trailing '/' */` |
|     71 | 1632 | `	while( zEnd > zPath && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      5 | 1633 | `		zEnd--;` |
|      1 | 1634 | `	}` |
|     45 | 1635 | `	if( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ){` |
|      - | 1636 | `		/* Nothing but separators ("/", "///"): php answers the EMPTY string, where the` |
|      - | 1637 | `		 * strip loop above stops one short and left PH7 returning "/". */` |
|      5 | 1638 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1639 | `		return PH7_OK;` |
|      - | 1640 | `	}` |
|     41 | 1641 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    976 | 1642 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    916 | 1643 | `		zEnd--;` |
|      1 | 1644 | `	}` |
|     41 | 1645 | `	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     41 | 1646 | `	zEnd = &zPath[iLen];` |
|     41 | 1647 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1648 | `		const char *zSuffix;` |
|      - | 1649 | `		int nSuffix;` |
|      - | 1650 | `		/* Strip suffix */` |
|      5 | 1651 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      5 | 1652 | `		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1653 | `			zEnd -= nSuffix;` |
|      2 | 1654 | `		}` |
|      2 | 1655 | `	}` |
|      - | 1656 | `	/* Store the basename */` |
|     41 | 1657 | `	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));` |
|     41 | 1658 | `	return PH7_OK;` |
|     24 | 1659 | `}` |
|      - | 1660 | `/*` |
|      - | 1661 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1662 | ` *  Returns information about a file path.` |
|      - | 1663 | ` * Parameter` |
|      - | 1664 | ` *  $path` |
|      - | 1665 | ` *   The path to be parsed.` |
|      - | 1666 | ` *  $options` |
|      - | 1667 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 1668 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 1669 | ` * Return` |
|      - | 1670 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 1671 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 1672 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 1673 | ` */` |
|      - | 1674 | `typedef struct path_info path_info;` |
|      - | 1675 | `struct path_info` |
|      - | 1676 | `{` |
|      - | 1677 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1678 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1679 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1680 | `	SyString sFilename;  /* Filename */` |
|      - | 1681 | `};` |
|      - | 1682 | `/*` |
|      - | 1683 | ` * Extract path fields.` |
|      - | 1684 | ` */` |
|  13094 | 1685 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1686 | `{` |
|  13099 | 1687 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1688 | `	SyString *pCur;` |
|      - | 1689 | `	int c,d;` |
|  13099 | 1690 | `	c = d = '/';` |
|      - | 1691 | `#ifdef __WINNT__` |
|      5 | 1692 | `	d = '\\';` |
|      - | 1693 | `#endif` |
|      - | 1694 | `	/* Zero the structure */` |
|  13099 | 1695 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1696 | `	/* Handle special case */` |
|  13099 | 1697 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1698 | `#ifdef __WINNT__` |
|    ! 0 | 1699 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1700 | `#else` |
|    ! 0 | 1701 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1702 | `#endif` |
|    ! 0 | 1703 | `		return SXRET_OK;` |
|      - | 1704 | `	}` |
|      - | 1705 | `	/* Extract the basename */` |
| 352994 | 1706 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 333353 | 1707 | `		zEnd--;` |
|      5 | 1708 | `	}` |
|  13099 | 1709 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13099 | 1710 | `	zEnd = &zPath[nByte];` |
|      - | 1711 | `	/* dirname */` |
|  13099 | 1712 | `	pCur = &pOut->sDir;` |
|  13099 | 1713 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13099 | 1714 | `	if( pCur->nByte > 1 ){` |
|  26193 | 1715 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1716 | `#ifdef __WINNT__` |
|      5 | 1717 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1718 | `#endif` |
|   6552 | 1719 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1720 | `#ifdef __WINNT__` |
|    ! 0 | 1721 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1722 | `#else` |
|    ! 0 | 1723 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1724 | `#endif` |
|    ! 0 | 1725 | `	}` |
|      - | 1726 | `	/* basename/filename */` |
|  13099 | 1727 | `	pCur = &pOut->sBasename;` |
|  13099 | 1728 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13099 | 1729 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1730 | `#ifdef __WINNT__` |
|      5 | 1731 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1732 | `#endif` |
|  13099 | 1733 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13099 | 1734 | `	if( pCur->nByte > 0 ){` |
|      - | 1735 | `		/* extension */` |
|  13099 | 1736 | `		zEnd--;` |
|  65469 | 1737 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  52375 | 1738 | `			zEnd--;` |
|      5 | 1739 | `		}` |
|  13099 | 1740 | `		if( zEnd > pCur->zString ){` |
|  13097 | 1741 | `			zEnd++; /* Jump leading dot */` |
|  13097 | 1742 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1743 | `			/* Fix filename */` |
|  13097 | 1744 | `			pCur = &pOut->sFilename;` |
|  13097 | 1745 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13097 | 1746 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6546 | 1747 | `			}` |
|   6546 | 1748 | `		}` |
|   6547 | 1749 | `	}` |
|  13099 | 1750 | `	return SXRET_OK;` |
|   6552 | 1751 | `}` |
|      - | 1752 | `/*` |
|      - | 1753 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1754 | ` *  See block comment above.` |
|      - | 1755 | ` */` |
|  13094 | 1756 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1757 | `{` |
|      - | 1758 | `	const char *zPath;` |
|      - | 1759 | `	path_info sInfo;` |
|      - | 1760 | `	SyString *pComp;` |
|      - | 1761 | `	int iLen;` |
|  13099 | 1762 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1763 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1764 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1765 | `		return PH7_OK;` |
|      - | 1766 | `	}` |
|      - | 1767 | `	/* Point to the target path */` |
|  13099 | 1768 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13099 | 1769 | `	if( iLen < 1 ){` |
|      - | 1770 | `		/* Empty string */` |
|    ! 0 | 1771 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1772 | `		return PH7_OK;` |
|      - | 1773 | `	}` |
|      - | 1774 | `	/* Extract path info */` |
|  13099 | 1775 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  19645 | 1776 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1777 | `		/* Return path component */` |
|  13097 | 1778 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13097 | 1779 | `		switch(nComp){` |
|      1 | 1780 | `		case 1: /* PATHINFO_DIRNAME */` |
|      3 | 1781 | `			pComp = &sInfo.sDir;` |
|      3 | 1782 | `			if( pComp->nByte > 0 ){` |
|      3 | 1783 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1784 | `			}else{` |
|      - | 1785 | `				/* Expand the empty string */` |
|    ! 0 | 1786 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1787 | `			}` |
|      3 | 1788 | `			break;` |
|      1 | 1789 | `		case 2: /*PATHINFO_BASENAME*/` |
|      3 | 1790 | `			pComp = &sInfo.sBasename;` |
|      3 | 1791 | `			if( pComp->nByte > 0 ){` |
|      3 | 1792 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1793 | `			}else{` |
|      - | 1794 | `				/* Expand the empty string */` |
|    ! 0 | 1795 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1796 | `			}` |
|      3 | 1797 | `			break;` |
|   3274 | 1798 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6553 | 1799 | `			pComp = &sInfo.sExtension;` |
|   6553 | 1800 | `			if( pComp->nByte > 0 ){` |
|   6551 | 1801 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3278 | 1802 | `			}else{` |
|      - | 1803 | `				/* Expand the empty string */` |
|      3 | 1804 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1805 | `			}` |
|   6553 | 1806 | `			break;` |
|   3270 | 1807 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6545 | 1808 | `			pComp = &sInfo.sFilename;` |
|   6545 | 1809 | `			if( pComp->nByte > 0 ){` |
|   6545 | 1810 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3275 | 1811 | `			}else{` |
|      - | 1812 | `				/* Expand the empty string */` |
|    ! 0 | 1813 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1814 | `			}` |
|   6545 | 1815 | `			break;` |
|    ! 0 | 1816 | `		default:` |
|      - | 1817 | `			/* Expand the empty string */` |
|    ! 0 | 1818 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1819 | `			break;` |
|      - | 1820 | `		}` |
|   6551 | 1821 | `	}else{` |
|      - | 1822 | `		/* Return an associative array */` |
|      - | 1823 | `		ph7_value *pArray,*pValue;` |
|      3 | 1824 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 1825 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1826 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1827 | `			/* Out of mem,return NULL */` |
|    ! 0 | 1828 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1829 | `			return PH7_OK;` |
|      - | 1830 | `		}` |
|      - | 1831 | `		/* dirname */` |
|      3 | 1832 | `		pComp = &sInfo.sDir;` |
|      3 | 1833 | `		if( pComp->nByte > 0 ){` |
|      3 | 1834 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1835 | `			/* Perform the insertion */` |
|      3 | 1836 | `			ph7_array_add_strkey_elem(pArray,"dirname",pValue); /* Will make it's own copy */` |
|      1 | 1837 | `		}` |
|      - | 1838 | `		/* Reset the string cursor */` |
|      3 | 1839 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1840 | `		/* basername */` |
|      3 | 1841 | `		pComp = &sInfo.sBasename;` |
|      3 | 1842 | `		if( pComp->nByte > 0 ){` |
|      3 | 1843 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1844 | `			/* Perform the insertion */` |
|      3 | 1845 | `			ph7_array_add_strkey_elem(pArray,"basename",pValue); /* Will make it's own copy */` |
|      1 | 1846 | `		}` |
|      - | 1847 | `		/* Reset the string cursor */` |
|      3 | 1848 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1849 | `		/* extension */` |
|      3 | 1850 | `		pComp = &sInfo.sExtension;` |
|      3 | 1851 | `		if( pComp->nByte > 0 ){` |
|      3 | 1852 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1853 | `			/* Perform the insertion */` |
|      3 | 1854 | `			ph7_array_add_strkey_elem(pArray,"extension",pValue); /* Will make it's own copy */` |
|      1 | 1855 | `		}` |
|      - | 1856 | `		/* Reset the string cursor */` |
|      3 | 1857 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1858 | `		/* filename */` |
|      3 | 1859 | `		pComp = &sInfo.sFilename;` |
|      3 | 1860 | `		if( pComp->nByte > 0 ){` |
|      3 | 1861 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1862 | `			/* Perform the insertion */` |
|      3 | 1863 | `			ph7_array_add_strkey_elem(pArray,"filename",pValue); /* Will make it's own copy */` |
|      1 | 1864 | `		}` |
|      - | 1865 | `		/* Return the created array */` |
|      3 | 1866 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1867 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 1868 | `		 * automatically as soon we return from this foreign function.` |
|      - | 1869 | `		 */` |
|      - | 1870 | `	}` |
|  13099 | 1871 | `	return PH7_OK;` |
|   6552 | 1872 | `}` |
|      - | 1873 | `/* SPDX-SnippetBegin */` |
|      - | 1874 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 1875 | `/* SPDX-License-Identifier: blessing */` |
|      - | 1876 | `/*` |
|      - | 1877 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 1878 |  |
|      - | 1879 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 1880 | ` * Status: Public Domain` |
|      - | 1881 | ` */` |
|      - | 1882 | `typedef unsigned char u8;` |
|      - | 1883 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 1884 | `** lower-case character.` |
|      - | 1885 | `**` |
|      - | 1886 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 1887 | `** handle case conversions for the UTF character set since the tables` |
|      - | 1888 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 1889 | `*/` |
|      - | 1890 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 1891 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 1892 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 1893 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 1894 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 1895 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 1896 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 1897 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 1898 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 1899 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 1900 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 1901 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 1902 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 1903 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 1904 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 1905 | `    252,253,254,255` |
|      - | 1906 | `};` |
|      - | 1907 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 1908 | `/*` |
|      - | 1909 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 1910 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 1911 | `*/` |
|      - | 1912 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 1913 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 1914 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 1915 | `  }                                                    \` |
|      - | 1916 | `}` |
|      - | 1917 | `/*` |
|      - | 1918 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 1919 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 1920 | `** are the same and false (0) if they are different.` |
|      - | 1921 | `**` |
|      - | 1922 | `** Globbing rules:` |
|      - | 1923 | `**` |
|      - | 1924 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 1925 | `**` |
|      - | 1926 | `**      '?'       Matches exactly one character.` |
|      - | 1927 | `**` |
|      - | 1928 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 1929 | `**                characters.` |
|      - | 1930 | `**` |
|      - | 1931 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 1932 | `**` |
|      - | 1933 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 1934 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 1935 | `** range of characters can be specified using '-'.  Example:` |
|      - | 1936 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 1937 | `** it the last character in the list.` |
|      - | 1938 | `**` |
|      - | 1939 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 1940 | `**` |
|      - | 1941 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 1942 | `**` |
|      - | 1943 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 1944 | `*/` |
|     44 | 1945 | `static int patternCompare(` |
|      - | 1946 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1947 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1948 | `  const int esc,                    /* The escape character */` |
|      - | 1949 | `  int noCase` |
|      1 | 1950 | `){` |
|      - | 1951 | `  int c, c2;` |
|      - | 1952 | `  int invert;` |
|      - | 1953 | `  int seen;` |
|     45 | 1954 | `  u8 matchOne = '?';` |
|     45 | 1955 | `  u8 matchAll = '*';` |
|     45 | 1956 | `  u8 matchSet = '[';` |
|     45 | 1957 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1958 |  |
|     45 | 1959 | `  if( !zPattern \|\| !zString ) return 0;` |
|     81 | 1960 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     73 | 1961 | `    if( !prevEscape && c==matchAll ){` |
|     52 | 1962 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|     27 | 1963 | `               \|\| c == matchOne ){` |
|    ! 0 | 1964 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1965 | `          return 0;` |
|      - | 1966 | `        }` |
|    ! 0 | 1967 | `      }` |
|     27 | 1968 | `      if( c==0 ){` |
|     19 | 1969 | `        return 1;` |
|      9 | 1970 | `      }else if( c==esc ){` |
|    ! 0 | 1971 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1972 | `        if( c==0 ){` |
|    ! 0 | 1973 | `          return 0;` |
|    ! 0 | 1974 | `        }` |
|      9 | 1975 | `      }else if( c==matchSet ){` |
|    ! 0 | 1976 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 1977 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 1978 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 1979 | `        }` |
|    ! 0 | 1980 | `        return *zString!=0;` |
|      - | 1981 | `      }` |
|     11 | 1982 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     11 | 1983 | `        if( noCase ){` |
|      3 | 1984 | `          GlogUpperToLower(c2);` |
|      3 | 1985 | `          GlogUpperToLower(c);` |
|     11 | 1986 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 1987 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 1988 | `            GlogUpperToLower(c2);` |
|      1 | 1989 | `          }` |
|      2 | 1990 | `        }else{` |
|     47 | 1991 | `          while( c2 != 0 && c2 != c ){` |
|     39 | 1992 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 1993 | `          }` |
|      - | 1994 | `        }` |
|     11 | 1995 | `        if( c2==0 ) return 0;` |
|      9 | 1996 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 1997 | `      }` |
|    ! 0 | 1998 | `      return 0;` |
|     47 | 1999 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 2000 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 2001 | `        return 0;` |
|    ! 0 | 2002 | `      }` |
|     47 | 2003 | `    }else if( c==matchSet ){` |
|    ! 0 | 2004 | `      int prior_c = 0;` |
|    ! 0 | 2005 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 2006 | `      seen = 0;` |
|    ! 0 | 2007 | `      invert = 0;` |
|    ! 0 | 2008 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 2009 | `      if( c==0 ) return 0;` |
|    ! 0 | 2010 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2011 | `      if( c2=='^' ){` |
|    ! 0 | 2012 | `        invert = 1;` |
|    ! 0 | 2013 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2014 | `      }` |
|    ! 0 | 2015 | `      if( c2==']' ){` |
|    ! 0 | 2016 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 2017 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2018 | `      }` |
|    ! 0 | 2019 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 2020 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 2021 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2022 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 2023 | `          prior_c = 0;` |
|    ! 0 | 2024 | `        }else{` |
|    ! 0 | 2025 | `          if( c==c2 ){` |
|    ! 0 | 2026 | `            seen = 1;` |
|    ! 0 | 2027 | `          }` |
|    ! 0 | 2028 | `          prior_c = c2;` |
|      - | 2029 | `        }` |
|    ! 0 | 2030 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2031 | `      }` |
|    ! 0 | 2032 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 2033 | `        return 0;` |
|    ! 0 | 2034 | `      }` |
|     47 | 2035 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 2036 | `      prevEscape = 1;` |
|    ! 0 | 2037 | `    }else{` |
|     47 | 2038 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     47 | 2039 | `      if( noCase ){` |
|      7 | 2040 | `        GlogUpperToLower(c);` |
|      7 | 2041 | `        GlogUpperToLower(c2);` |
|      3 | 2042 | `      }` |
|     47 | 2043 | `      if( c!=c2 ){` |
|     11 | 2044 | `        return 0;` |
|      - | 2045 | `      }` |
|     37 | 2046 | `      prevEscape = 0;` |
|      - | 2047 | `    }` |
|      1 | 2048 | `  }` |
|      9 | 2049 | `  return *zString==0;` |
|     23 | 2050 | `}` |
|      - | 2051 | `/* SPDX-SnippetEnd */` |
|      - | 2052 | `/*` |
|      - | 2053 | ` * Wrapper around patternCompare() defined above.` |
|      - | 2054 | ` * See block comment above for more information.` |
|      - | 2055 | ` */` |
|     36 | 2056 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 2057 | `{` |
|      - | 2058 | `	int rc;` |
|     37 | 2059 | `	if( iEsc < 0 ){` |
|    ! 0 | 2060 | `		iEsc = '\\';` |
|    ! 0 | 2061 | `	}` |
|     37 | 2062 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     37 | 2063 | `	return rc;` |
|      1 | 2064 | `}` |
|      - | 2065 | `/*` |
|      - | 2066 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 2067 | ` *  Match filename against a pattern.` |
|      - | 2068 | ` * Parameters` |
|      - | 2069 | ` *  $pattern` |
|      - | 2070 | ` *   The shell wildcard pattern.` |
|      - | 2071 | ` * $string` |
|      - | 2072 | ` *  The tested string.` |
|      - | 2073 | ` * $flags` |
|      - | 2074 | ` *   A list of possible flags:` |
|      - | 2075 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 2076 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 2077 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 2078 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 2079 | ` * Return` |
|      - | 2080 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2081 | ` */` |
|      8 | 2082 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2083 | `{` |
|      - | 2084 | `	const char *zString,*zPattern;` |
|      9 | 2085 | `	int iEsc = '\\';` |
|      9 | 2086 | `	int noCase = 0;` |
|      - | 2087 | `	int rc;` |
|      9 | 2088 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2089 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2090 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2091 | `		return PH7_OK;` |
|      - | 2092 | `	}` |
|      - | 2093 | `	/* Extract the pattern and the string */` |
|      9 | 2094 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 2095 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2096 | `	/* Extract the flags if avaialble */` |
|      9 | 2097 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 2098 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 2099 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|    ! 0 | 2100 | `			iEsc = 0;` |
|    ! 0 | 2101 | `		}` |
|      7 | 2102 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
|      3 | 2103 | `			noCase = 1;` |
|      1 | 2104 | `		}` |
|      3 | 2105 | `	}` |
|      - | 2106 | `	/* Go globbing */` |
|      9 | 2107 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 2108 | `	/* Globbing result */` |
|      9 | 2109 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 2110 | `	return PH7_OK;` |
|      5 | 2111 | `}` |
|      - | 2112 | `/*` |
|      - | 2113 | ` * bool strglob(string $pattern,string $string)` |
|      - | 2114 | ` *  Match string against a pattern.` |
|      - | 2115 | ` * Parameters` |
|      - | 2116 | ` *  $pattern` |
|      - | 2117 | ` *   The shell wildcard pattern.` |
|      - | 2118 | ` * $string` |
|      - | 2119 | ` *  The tested string.` |
|      - | 2120 | ` * Return` |
|      - | 2121 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2122 | ` * Note that this a symisc eXtension.` |
|      - | 2123 | ` */` |
|     28 | 2124 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2125 | `{` |
|      - | 2126 | `	const char *zString,*zPattern;` |
|     29 | 2127 | `	int iEsc = '\\';` |
|      - | 2128 | `	int rc;` |
|     29 | 2129 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2130 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2131 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2132 | `		return PH7_OK;` |
|      - | 2133 | `	}` |
|      - | 2134 | `	/* Extract the pattern and the string */` |
|     29 | 2135 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|     29 | 2136 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2137 | `	/* Go globbing */` |
|     29 | 2138 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2139 | `	/* Globbing result */` |
|     29 | 2140 | `	ph7_result_bool(pCtx,rc);` |
|     29 | 2141 | `	return PH7_OK;` |
|     15 | 2142 | `}` |
|      - | 2143 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2144 | `/*` |
|      - | 2145 | ` * bool link(string $target,string $link)` |
|      - | 2146 |  |
|      - | 2147 | ` *  Create a hard link.` |
|      - | 2148 | ` * Parameters` |
|      - | 2149 | ` *  $target` |
|      - | 2150 | ` *   Target of the link.` |
|      - | 2151 | ` *  $link` |
|      - | 2152 | ` *   The link name.` |
|      - | 2153 | ` * Return` |
|      - | 2154 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2155 | ` */` |
|      2 | 2156 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2157 | `{` |
|      - | 2158 | `	const char *zTarget,*zLink;` |
|      - | 2159 | `	ph7_vfs *pVfs;` |
|      - | 2160 | `	int rc;` |
|      3 | 2161 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2162 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2163 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2164 | `		return PH7_OK;` |
|      - | 2165 | `	}` |
|      - | 2166 | `	/* Point to the underlying vfs */` |
|      3 | 2167 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2168 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2169 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2170 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2171 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2172 | `			ph7_function_name(pCtx)` |
|      - | 2173 | `			);` |
|    ! 0 | 2174 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2175 | `		return PH7_OK;` |
|      - | 2176 | `	}` |
|      - | 2177 | `	/* Extract the given arguments */` |
|      3 | 2178 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      3 | 2179 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2180 | `	/* Perform the requested operation */` |
|      3 | 2181 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2182 | `	/* IO result */` |
|      3 | 2183 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 2184 | `	return PH7_OK;` |
|      2 | 2185 | `}` |
|      - | 2186 | `/*` |
|      - | 2187 | ` * bool symlink(string $target,string $link)` |
|      - | 2188 | ` *  Creates a symbolic link.` |
|      - | 2189 | ` * Parameters` |
|      - | 2190 | ` *  $target` |
|      - | 2191 | ` *   Target of the link.` |
|      - | 2192 | ` *  $link` |
|      - | 2193 | ` *   The link name.` |
|      - | 2194 | ` * Return` |
|      - | 2195 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2196 | ` */` |
|      6 | 2197 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2198 | `{` |
|      - | 2199 | `	const char *zTarget,*zLink;` |
|      - | 2200 | `	ph7_vfs *pVfs;` |
|      - | 2201 | `	int rc;` |
|      7 | 2202 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2203 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2204 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2205 | `		return PH7_OK;` |
|      - | 2206 | `	}` |
|      - | 2207 | `	/* Point to the underlying vfs */` |
|      7 | 2208 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 2209 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2210 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2211 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2212 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2213 | `			ph7_function_name(pCtx)` |
|      - | 2214 | `			);` |
|    ! 0 | 2215 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2216 | `		return PH7_OK;` |
|      - | 2217 | `	}` |
|      - | 2218 | `	/* Extract the given arguments */` |
|      7 | 2219 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      7 | 2220 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2221 | `	/* Perform the requested operation */` |
|      7 | 2222 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2223 | `	/* IO result */` |
|      7 | 2224 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      7 | 2225 | `	return PH7_OK;` |
|      4 | 2226 | `}` |
|      - | 2227 | `/*` |
|      - | 2228 | ` * int umask([ int $mask ])` |
|      - | 2229 | ` *  Changes the current umask.` |
|      - | 2230 | ` * Parameters` |
|      - | 2231 | ` *  $mask` |
|      - | 2232 | ` *   The new umask.` |
|      - | 2233 | ` * Return` |
|      - | 2234 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2235 | ` *  Otherwise the old umask is returned.` |
|      - | 2236 | ` */` |
|      8 | 2237 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2238 | `{` |
|      - | 2239 | `	int iOld,iNew;` |
|      - | 2240 | `	ph7_vfs *pVfs;` |
|      - | 2241 | `	/* Point to the underlying vfs */` |
|      9 | 2242 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 2243 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2244 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2245 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2246 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2247 | `			ph7_function_name(pCtx)` |
|      - | 2248 | `			);` |
|    ! 0 | 2249 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2250 | `		return PH7_OK;` |
|      - | 2251 | `	}` |
|      9 | 2252 | `	iNew = 0;` |
|      9 | 2253 | `	if( nArg > 0 ){` |
|      5 | 2254 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2255 | `	}` |
|      - | 2256 | `	/* Perform the requested operation */` |
|      9 | 2257 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2258 | `	/* Old mask */` |
|      9 | 2259 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 2260 | `	return PH7_OK;` |
|      5 | 2261 | `}` |
|      - | 2262 | `/*` |
|      - | 2263 | ` * string sys_get_temp_dir()` |
|      - | 2264 | ` *  Returns directory path used for temporary files.` |
|      - | 2265 | ` * Parameters` |
|      - | 2266 | ` *  None` |
|      - | 2267 | ` * Return` |
|      - | 2268 | ` *  Returns the path of the temporary directory.` |
|      - | 2269 | ` */` |
|    214 | 2270 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2271 | `{` |
|      - | 2272 | `	ph7_vfs *pVfs;` |
|      - | 2273 | `	/* Set the empty string as the default return value */` |
|    217 | 2274 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2275 | `	/* Point to the underlying vfs */` |
|    217 | 2276 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    217 | 2277 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2278 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2279 | `		SXUNUSED(apArg);` |
|      - | 2280 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2281 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2282 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2283 | `			ph7_function_name(pCtx)` |
|      - | 2284 | `			);` |
|    ! 0 | 2285 | `		return PH7_OK;` |
|      - | 2286 | `	}` |
|      - | 2287 | `	/* Perform the requested operation */` |
|    217 | 2288 | `	pVfs->xTempDir(pCtx);` |
|    217 | 2289 | `	return PH7_OK;` |
|    110 | 2290 | `}` |
|      - | 2291 | `/*` |
|      - | 2292 | ` * string get_current_user()` |
|      - | 2293 | ` *  Returns the name of the current working user.` |
|      - | 2294 | ` * Parameters` |
|      - | 2295 | ` *  None` |
|      - | 2296 | ` * Return` |
|      - | 2297 | ` *  Returns the name of the current working user.` |
|      - | 2298 | ` */` |
|      2 | 2299 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2300 | `{` |
|      - | 2301 | `	ph7_vfs *pVfs;` |
|      - | 2302 | `	/* Point to the underlying vfs */` |
|      3 | 2303 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2304 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2305 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2306 | `		SXUNUSED(apArg);` |
|      - | 2307 | `		/* IO routine not implemented */` |
|    ! 0 | 2308 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2309 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2310 | `			ph7_function_name(pCtx)` |
|      - | 2311 | `			);` |
|      - | 2312 | `		/* Set a dummy username */` |
|    ! 0 | 2313 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2314 | `		return PH7_OK;` |
|      - | 2315 | `	}` |
|      - | 2316 | `	/* Perform the requested operation */` |
|      3 | 2317 | `	pVfs->xUsername(pCtx);` |
|      3 | 2318 | `	return PH7_OK;` |
|      2 | 2319 | `}` |
|      - | 2320 | `/*` |
|      - | 2321 | ` * int64 getmypid()` |
|      - | 2322 | ` *  Gets process ID.` |
|      - | 2323 | ` * Parameters` |
|      - | 2324 | ` *  None` |
|      - | 2325 | ` * Return` |
|      - | 2326 | ` *  Returns the process ID.` |
|      - | 2327 | ` */` |
|     84 | 2328 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2329 | `{` |
|      - | 2330 | `	ph7_int64 nProcessId;` |
|      - | 2331 | `	ph7_vfs *pVfs;` |
|      - | 2332 | `	/* Point to the underlying vfs */` |
|     87 | 2333 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     87 | 2334 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2335 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2336 | `		SXUNUSED(apArg);` |
|      - | 2337 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2338 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2339 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2340 | `			ph7_function_name(pCtx)` |
|      - | 2341 | `			);` |
|    ! 0 | 2342 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2343 | `		return PH7_OK;` |
|      - | 2344 | `	}` |
|      - | 2345 | `	/* Perform the requested operation */` |
|     87 | 2346 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2347 | `	/* Set the result */` |
|     87 | 2348 | `	ph7_result_int64(pCtx,nProcessId);` |
|     87 | 2349 | `	return PH7_OK;` |
|     45 | 2350 | `}` |
|      - | 2351 | `/*` |
|      - | 2352 | ` * int getmyuid()` |
|      - | 2353 | ` *  Get user ID.` |
|      - | 2354 | ` * Parameters` |
|      - | 2355 | ` *  None` |
|      - | 2356 | ` * Return` |
|      - | 2357 | ` *  Returns the user ID.` |
|      - | 2358 | ` */` |
|      2 | 2359 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2360 | `{` |
|      - | 2361 | `	ph7_vfs *pVfs;` |
|      - | 2362 | `	int nUid;` |
|      - | 2363 | `	/* Point to the underlying vfs */` |
|      3 | 2364 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2365 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2366 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2367 | `		SXUNUSED(apArg);` |
|      - | 2368 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2369 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2370 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2371 | `			ph7_function_name(pCtx)` |
|      - | 2372 | `			);` |
|    ! 0 | 2373 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2374 | `		return PH7_OK;` |
|      - | 2375 | `	}` |
|      - | 2376 | `	/* Perform the requested operation */` |
|      3 | 2377 | `	nUid = pVfs->xUid();` |
|      - | 2378 | `	/* Set the result */` |
|      3 | 2379 | `	ph7_result_int(pCtx,nUid);` |
|      3 | 2380 | `	return PH7_OK;` |
|      2 | 2381 | `}` |
|      - | 2382 | `/*` |
|      - | 2383 | ` * int getmygid()` |
|      - | 2384 | ` *  Get group ID.` |
|      - | 2385 | ` * Parameters` |
|      - | 2386 | ` *  None` |
|      - | 2387 | ` * Return` |
|      - | 2388 | ` *  Returns the group ID.` |
|      - | 2389 | ` */` |
|      2 | 2390 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2391 | `{` |
|      - | 2392 | `	ph7_vfs *pVfs;` |
|      - | 2393 | `	int nGid;` |
|      - | 2394 | `	/* Point to the underlying vfs */` |
|      3 | 2395 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2396 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 2397 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2398 | `		SXUNUSED(apArg);` |
|      - | 2399 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2400 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2401 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2402 | `			ph7_function_name(pCtx)` |
|      - | 2403 | `			);` |
|    ! 0 | 2404 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2405 | `		return PH7_OK;` |
|      - | 2406 | `	}` |
|      - | 2407 | `	/* Perform the requested operation */` |
|      3 | 2408 | `	nGid = pVfs->xGid();` |
|      - | 2409 | `	/* Set the result */` |
|      3 | 2410 | `	ph7_result_int(pCtx,nGid);` |
|      3 | 2411 | `	return PH7_OK;` |
|      2 | 2412 | `}` |
|      - | 2413 | `#ifdef __WINNT__` |
|      - | 2414 | `#include <Windows.h>` |
|      - | 2415 | `#elif defined(__UNIXES__)` |
|      - | 2416 | `#include <sys/utsname.h>` |
|      - | 2417 | `#endif` |
|      - | 2418 | `/*` |
|      - | 2419 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 2420 | ` *  Returns information about the host operating system.` |
|      - | 2421 | ` * Parameters` |
|      - | 2422 | ` *  $mode` |
|      - | 2423 | ` *   mode is a single character that defines what information is returned:` |
|      - | 2424 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 2425 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 2426 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 2427 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 2428 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 2429 | ` *    'm': Machine type. eg. i386.` |
|      - | 2430 | ` * Return` |
|      - | 2431 | ` *  OS description as a string.` |
|      - | 2432 | ` */` |
|      4 | 2433 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2434 | `{` |
|      - | 2435 | `#if defined(__WINNT__)` |
|      1 | 2436 | `	const char *zName = "Microsoft Windows";` |
|      - | 2437 | `	OSVERSIONINFOW sVer;` |
|      - | 2438 | `#elif defined(__UNIXES__)` |
|      - | 2439 | `	struct utsname sName;` |
|      - | 2440 | `#endif` |
|      5 | 2441 | `	const char *zMode = "a";` |
|      5 | 2442 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 2443 | `		/* Extract the desired mode */` |
|    ! 0 | 2444 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 2445 | `	}` |
|      - | 2446 | `#if defined(__WINNT__)` |
|      1 | 2447 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 2448 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 2449 | `#if defined(_MSC_VER)` |
|      - | 2450 | `#pragma warning(push)` |
|      - | 2451 | `#pragma warning(disable:4996)` |
|      - | 2452 | `#endif` |
|      1 | 2453 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 2454 | `#if defined(_MSC_VER)` |
|      - | 2455 | `#pragma warning(pop)` |
|      - | 2456 | `#endif` |
|    ! 0 | 2457 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 2458 | `		return PH7_OK;` |
|      - | 2459 | `	}` |
|      1 | 2460 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 2461 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 2462 | `			zName = "Microsoft Windows NT";` |
|      1 | 2463 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 2464 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 2465 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 2466 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 2467 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 2468 | `			}` |
|    ! 0 | 2469 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 2470 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 2471 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 2472 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 2473 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 2474 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 2475 | `					default: break;` |
|      - | 2476 | `				}` |
|      - | 2477 | `		}` |
|      - | 2478 | `	}` |
|      1 | 2479 | `	switch(zMode[0]){` |
|      - | 2480 | `	case 's':` |
|      - | 2481 | `		/* Operating system name */` |
|    ! 0 | 2482 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 2483 | `		break;` |
|      - | 2484 | `	case 'n':` |
|      - | 2485 | `		/* Host name */` |
|    ! 0 | 2486 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 2487 | `		break;` |
|      - | 2488 | `	case 'r':` |
|      - | 2489 | `	case 'v':` |
|      - | 2490 | `		/* Version information. */` |
|    ! 0 | 2491 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 2492 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2493 | `			);` |
|    ! 0 | 2494 | `		break;` |
|      - | 2495 | `	case 'm':` |
|      - | 2496 | `		/* Machine name */` |
|    ! 0 | 2497 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 2498 | `		break;` |
|      - | 2499 | `	default:` |
|      1 | 2500 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 2501 | `			zName,` |
|      - | 2502 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2503 | `			);` |
|      - | 2504 | `		break;` |
|      - | 2505 | `	}` |
|      - | 2506 | `#elif defined(__UNIXES__)` |
|      4 | 2507 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 2508 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 2509 | `		return PH7_OK;` |
|      - | 2510 | `	}` |
|      4 | 2511 | `	switch(zMode[0]){` |
|    ! 0 | 2512 | `	case 's':` |
|      - | 2513 | `		/* Operating system name */` |
|    ! 0 | 2514 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 2515 | `		break;` |
|    ! 0 | 2516 | `	case 'n':` |
|      - | 2517 | `		/* Host name */` |
|    ! 0 | 2518 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 2519 | `		break;` |
|    ! 0 | 2520 | `	case 'r':` |
|      - | 2521 | `		/* Release information */` |
|    ! 0 | 2522 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 2523 | `		break;` |
|    ! 0 | 2524 | `	case 'v':` |
|      - | 2525 | `		/* Version information. */` |
|    ! 0 | 2526 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 2527 | `		break;` |
|    ! 0 | 2528 | `	case 'm':` |
|      - | 2529 | `		/* Machine name */` |
|    ! 0 | 2530 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 2531 | `		break;` |
|      2 | 2532 | `	default:` |
|      6 | 2533 | `		ph7_result_string_format(pCtx,` |
|      - | 2534 | `			"%s %s %s %s %s",` |
|      2 | 2535 | `			sName.sysname,` |
|      2 | 2536 | `			sName.release,` |
|      2 | 2537 | `			sName.version,` |
|      2 | 2538 | `			sName.nodename,` |
|      2 | 2539 | `			sName.machine` |
|      - | 2540 | `			);` |
|      4 | 2541 | `		break;` |
|      - | 2542 | `	}` |
|      - | 2543 | `#else` |
|      - | 2544 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 2545 | `#endif` |
|      5 | 2546 | `	return PH7_OK;` |
|      3 | 2547 | `}` |
|      - | 2548 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 2549 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 2550 | `#if defined(_MSC_VER)` |
|      - | 2551 | `static const ph7_vfs null_vfs = {` |
|      - | 2552 | `#else` |
|      - | 2553 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 2554 | `#endif` |
|      - | 2555 | `	"null_vfs",` |
|      - | 2556 | `	PH7_VFS_VERSION,` |
|      - | 2557 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 2558 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 2559 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 2560 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 2561 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 2562 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 2563 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 2564 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 2565 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 2566 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 2567 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 2568 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 2569 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 2570 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 2571 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 2572 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 2573 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 2574 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 2575 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 2576 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 2577 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 2578 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 2579 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 2580 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 2581 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 2582 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 2583 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 2584 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 2585 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 2586 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 2587 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 2588 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 2589 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 2590 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 2591 | `	0, /* int (*xUmask)(int) */` |
|      - | 2592 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 2593 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 2594 | `	0, /* int (*xUid)(void) */` |
|      - | 2595 | `	0, /* int (*xGid)(void) */` |
|      - | 2596 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 2597 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 2598 | `};` |
|      - | 2599 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 2600 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 2601 | `/*` |
|      - | 2602 | ` * Export the builtin vfs.` |
|      - | 2603 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 2604 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 2605 | ` * Note:` |
|      - | 2606 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 2607 | ` * Note:` |
|      - | 2608 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2609 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 2610 | ` */` |
|   3896 | 2611 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 2612 | `{` |
|      - | 2613 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 2614 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 2615 | `	return &null_vfs;` |
|      - | 2616 | `#else` |
|      - | 2617 | `#ifdef __WINNT__` |
|      5 | 2618 | `	return &sWinVfs;` |
|      - | 2619 | `#elif defined(__UNIXES__)` |
|   3896 | 2620 | `	return &sUnixVfs;` |
|      - | 2621 | `#else` |
|      - | 2622 | `	return &null_vfs;` |
|      - | 2623 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 2624 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 2625 | `#else` |
|      - | 2626 | `	return &null_vfs;` |
|      - | 2627 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 2628 | `}` |
|      - | 2629 | `/*` |
|      - | 2630 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 2631 | ` * [i.e: file://,php://].` |
|      - | 2632 | ` * Note:` |
|      - | 2633 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 2634 | ` *  defined then this function is a no-op.` |
|      - | 2635 | ` */` |
|   3416 | 2636 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 2637 | `{` |
|      - | 2638 | `	/*` |
|      - | 2639 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 2640 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 2641 | `	 */` |
|      - | 2642 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 2643 | `	/* VFS: disk I/O related functions */` |
|      - | 2644 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 2645 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 2646 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 2647 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 2648 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 2649 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 2650 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 2651 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 2652 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 2653 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 2654 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 2655 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 2656 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 2657 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 2658 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 2659 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 2660 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 2661 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 2662 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 2663 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 2664 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 2665 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 2666 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 2667 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 2668 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 2669 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 2670 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 2671 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 2672 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 2673 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 2674 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 2675 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 2676 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 2677 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 2678 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 2679 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 2680 | `		{"link",        PH7_vfs_link     },` |
|      - | 2681 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 2682 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 2683 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 2684 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 2685 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 2686 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 2687 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 2688 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 2689 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 2690 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 2691 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 2692 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 2693 | `	};` |
|      - | 2694 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 2695 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 2696 | `	 */` |
|      - | 2697 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 2698 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 2699 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 2700 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 2701 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 2702 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 2703 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 2704 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 2705 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 2706 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 2707 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 2708 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 2709 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 2710 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 2711 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 2712 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 2713 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 2714 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 2715 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 2716 | `		{"file",      PH7_builtin_file   },` |
|      - | 2717 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 2718 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 2719 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 2720 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 2721 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 2722 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 2723 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 2724 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 2725 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 2726 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 2727 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 2728 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 2729 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 2730 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 2731 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 2732 | `#ifdef PH7_ENABLE_NET` |
|      - | 2733 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 2734 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 2735 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 2736 | `#endif` |
|      - | 2737 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 2738 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 2739 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 2740 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 2741 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 2742 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 2743 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 2744 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 2745 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 2746 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 2747 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 2748 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 2749 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 2750 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 2751 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 2752 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 2753 | `	};` |
|   3421 | 2754 | `	const ph7_io_stream *pFileStream = 0;` |
|   3421 | 2755 | `	sxu32 n = 0;` |
|      - | 2756 | `	/* Register disk-related functions */` |
| 167389 | 2757 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 163973 | 2758 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  81989 | 2759 | `	}` |
| 177637 | 2760 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 174221 | 2761 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  87113 | 2762 | `	}` |
|      - | 2763 | `#else` |
|      - | 2764 | `	SXUNUSED(pVm);` |
|      - | 2765 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 2766 |  |
|      - | 2767 | `	/*` |
|      - | 2768 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2769 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 2770 | `	 */` |
|      - | 2771 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2772 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 2773 | `		/* Path processing */` |
|      - | 2774 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 2775 | `		{"basename",    PH7_builtin_basename },` |
|      - | 2776 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 2777 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 2778 | `		{"fnmatch",     PH7_builtin_fnmatch  }` |
|      - | 2779 | `	};` |
|  20501 | 2780 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  17085 | 2781 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|   8545 | 2782 | `	}` |
|      - | 2783 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2784 |  |
|      - | 2785 | `	/* Install streams if disk I/O is enabled */` |
|      - | 2786 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 2787 | `#ifdef __WINNT__` |
|      5 | 2788 | `	pFileStream = &sWinFileStream;` |
|      - | 2789 | `#elif defined(__UNIXES__)` |
|   3416 | 2790 | `	pFileStream = &sUnixFileStream;` |
|      - | 2791 | `#endif` |
|      - | 2792 | `	/* Install the php:// stream */` |
|   3421 | 2793 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3421 | 2794 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 2795 | `#ifdef PH7_ENABLE_NET` |
|   3421 | 2796 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 2797 | `#endif` |
|   3421 | 2798 | `	if( pFileStream ){` |
|      - | 2799 | `		/* Install the file:// stream */` |
|   3421 | 2800 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1708 | 2801 | `	}` |
|      - | 2802 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 2803 |  |
|   3421 | 2804 | `	return SXRET_OK;` |
|      5 | 2805 | `}` |
|      - | 2806 |  |
